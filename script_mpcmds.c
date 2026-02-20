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
#include "editors/common.h"

//#define DEBUG_MODULE
#include "debug.h"
#include "skill_data.h"


/*
 * Command table.
 */
const struct script_cmd_type mob_cmd_table[] = {
    { "addaffect",			scriptcmd_addaffect,		true,	true	},
    { "addaffectname",		scriptcmd_addaffectname,	true,	true	},
    { "addspell",			do_mpaddspell,				true,	true	},
    { "alteraffect",		do_mpalteraffect,			true,	true	},
    { "alterexit",			do_mpalterexit,				false,	true	},
    { "altermob",			do_mpaltermob,				true,	true	},
    { "alterobj",			scriptcmd_alterobj,				true,	true	},
    { "alterroom",			scriptcmd_alterroom,				true,	true	},
    { "appear",				do_mpvis,					false,	false	},
    { "applytoxin",			scriptcmd_applytoxin,		false,	true	},
    { "asound", 			scriptcmd_asound,			false,	true	},
    { "assist",				do_mpassist,				false,	true	},
    { "at",					scriptcmd_at,			false,	true	},
    { "attach",				scriptcmd_attach,			true,	true	},
    { "award",				scriptcmd_award,			true,	true	},
    { "breathe",			scriptcmd_breathe,		false,	true	},
    { "call",				do_mpcall,					false,	true	},
    { "cancel",				scriptcmd_cancel,		false,	false	},
    { "cast",				do_mpcast,					false,	true	},
    { "chargebank",			scriptcmd_chargebank,		false,	true	},
    { "chargemoney",		do_mpchargemoney,			false,	true	},
    { "checkpoint",			scriptcmd_checkpoint,		false,	true	},
    { "churchannouncetheft",	scriptcmd_churchannouncetheft,	true, true },
    { "cloneroom",			do_mpcloneroom,				true,	true	},
    { "condition",			do_mpcondition,				false,	true	},
    { "crier",				do_mpcrier,					false,	true	},
    { "damage",				scriptcmd_damage,			false,	true	},
    { "deduct",				scriptcmd_deduct,			true,	true	},
    { "delay",				scriptcmd_delay,			false,	true	},
    { "dequeue",			scriptcmd_dequeue,		false,	false	},
    { "destroyroom",		do_mpdestroyroom,			true,	true	},
    { "detach",				scriptcmd_detach,			true,	true	},
    { "disappear",    		do_mpinvis,					false,	false	},
    { "dungeoncomplete",	scriptcmd_dungeoncomplete,	true,	true	},
    { "dungeoncommence",	scriptcmd_dungeoncommence,	true,	true	},
    { "dungeonfailure",	scriptcmd_dungeonfailure,	true,	true	},
    { "event",              scriptcmd_event,          false,  true    },
    { "echo",				scriptcmd_echo,				false,	true	},
    { "echoaround",			scriptcmd_echoaround,		false,	true	},
    { "echoat",				scriptcmd_echoat,			false,	true	},
    { "echobattlespam",		scriptcmd_echobattlespam,	false,	true	},
    { "echochurch",			scriptcmd_echochurch,		false,	true	},
    { "echogrouparound",	scriptcmd_echogrouparound,	false,	true	},
    { "echogroupat",		scriptcmd_echogroupat,		false,	true	},
    { "echoleadaround",		scriptcmd_echoleadaround,	false,	true	},
    { "echoleadat",			scriptcmd_echoleadat,		false,	true	},
    { "echonotvict",		scriptcmd_echonotvict,		false,	true	},
    { "echoroom",			scriptcmd_echoroom,			false,	true	},
    { "ed",					scriptcmd_ed,				false,	true	},
    { "entercombat",		scriptcmd_entercombat,		false,	true	},
    { "fade",				scriptcmd_fade,				true,	true	},
    { "fixaffects",			scriptcmd_fixaffects,		false,	true	},
    { "flee",				scriptcmd_flee,				false,	false	},
    { "force",				scriptcmd_force,			false,	true	},
    { "forget",				scriptcmd_forget,		false,	true	},
    { "gdamage",			scriptcmd_gdamage,		false,	true	},
    { "gecho",				scriptcmd_gecho,			false,	true	},
    { "gforce",				scriptcmd_gforce,			false,	true	},
    { "goto",				scriptcmd_goto,				false,	true	},
    { "grantclass",			scriptcmd_grantclass,		false,	true	},
    { "grantskill",			scriptcmd_grantskill,		false,	true	},
    { "grantsong",			scriptcmd_grantsong,		false,	true	},
    { "group",				do_mpgroup,					false,	true	},
    { "gtransfer",			scriptcmd_gtransfer,		false,	true	},
    { "hunt",				do_mphunt,					false,	true	},
    { "input",				do_mpinput,					false,	true	},
    { "inputstring",		scriptcmd_inputstring,		false,	true	},
    { "instancecomplete",	scriptcmd_instancecomplete,	true,	true	},
    { "instancefailure",	scriptcmd_instancefailure,	true,	true	},
    { "interrupt",			do_mpinterrupt,				false,	true	},
    { "junk",				do_mpjunk,					false,	true	},
    { "kill",				do_mpkill,					false,	true	},
    { "link",				do_mplink,					false,	true	},
    { "loadinstanced",		scriptcmd_loadinstanced,	true,	true	},
    { "lockadd",			scriptcmd_lockadd,			false,	true	},
    { "lockremove",			scriptcmd_lockremove,		false,	true	},
    { "mail",				scriptcmd_mail,				true,	true	},
    { "mload",				scriptcmd_mload,				false,	true	},
    { "mute",				scriptcmd_mute,				false,	true	},
    { "oload",				do_mpoload,					false,	true	},
    { "otransfer",			do_mpotransfer,				false,	true	},
    { "pageat",				scriptcmd_pageat,			false,	true	},
    { "peace",				scriptcmd_peace,				false,	false	},
    { "persist",			scriptcmd_persist,		false,	true	},
    { "prompt",				do_mpprompt,				false,	true	},
    { "purge",				do_mppurge,					false,	false	},
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
    { "raisedead",			do_mpraisedead,				true,	true	},
    { "rawkill",			do_mprawkill,				false,	true	},
    { "reckoning",			scriptcmd_reckoning,		true,	true	},
    { "remember",			scriptcmd_remember,	false,	true	},
    { "remort",				do_mpremort,				true,	true	},
    { "remove",				do_mpremove,				false,	true	},
    { "remspell",			do_mpremspell,				true,	true	},
    { "resetdice",			do_mpresetdice,				true,	true	},
    { "resetroom",			scriptcmd_resetroom,		true,	true	},
    { "restore",			scriptcmd_restore,			true,	true	},
    { "revokeclass",		scriptcmd_revokeclass,		false,	true	},
    { "revokeskill",		scriptcmd_revokeskill,		false,	true	},
    { "revokesong",			scriptcmd_revokesong,		false,	true	},
    { "saveplayer",			scriptcmd_saveplayer,		false,	true	},
    { "scriptwait",			scriptcmd_scriptwait,		false,	true	},
    { "selfdestruct",		do_mpselfdestruct,			false,	false	},
    { "sendfloor",			scriptcmd_sendfloor,		false,	true	},
    { "setclass",			scriptcmd_setclass,			false,	true	},
    { "setrace",			scriptcmd_setrace,			false,	true	},
    { "setrecall",			do_mpsetrecall,				false,	true,	},
    { "settimer",			do_mpsettimer,				false,	true	},
    { "settrait",			scriptcmd_settrait,			false,	true	},
    { "showcommand",		scriptcmd_showcommand,		false,	true	},
    { "showroom",			do_mpshowroom,				true,	true	},
    { "skimprove",			do_mpskimprove,				true,	true	},
    { "spawndungeon",		scriptcmd_spawndungeon,		true,	true	},
    { "specialkey",			scriptcmd_specialkey,		false,	true	},
    { "startcombat",		scriptcmd_startcombat,		false,	true	},
    { "startreckoning",		scriptcmd_startreckoning,	true,	true	},
    { "stopcombat",			scriptcmd_stopcombat,		false,	true	},
    { "stopreckoning",		scriptcmd_stopreckoning,	true,	true	},
    { "stringmob",			do_mpstringmob,				true,	true	},
    { "stringobj",			do_mpstringobj,				true,	true	},
    { "stripaffect",		do_mpstripaffect,			true,	true	},
    { "stripaffectname",	do_mpstripaffectname,		true,	true	},
    { "take",				do_mptake,					false,	true	},
    { "teleport", 			do_mpteleport,				false,	false	},
    { "transfer",			scriptcmd_transfer,			false,	true	},
    { "treasuremap",		scriptcmd_treasuremap,		false,	true	},
    { "ungroup",			do_mpungroup,				false,	true	},
    { "unlockarea",			scriptcmd_unlockarea,		true,	true	},
    { "unlockdungeon",		scriptcmd_unlockdungeon,	true,	true	},
    { "unmute",				scriptcmd_unmute,			false,	true	},
    { "usecatalyst",		do_mpusecatalyst,			false,	true	},
    { "varclear",			scriptcmd_varclear,			false,	true	},
    { "varclearon",			scriptcmd_varclearon,			false,	true	},
    { "varcopy",			scriptcmd_varcopy,			false,	true	},
    { "varsave",			scriptcmd_varsave,			false,	true	},
    { "varsaveon",			scriptcmd_varsaveon,			false,	true	},
    { "varset",				scriptcmd_varset,			false,	true	},
    { "varseton",			scriptcmd_varseton,			false,	true	},
    { "vforce",				scriptcmd_vforce,		false,	true	},
    { "wildernessmap",		scriptcmd_wildernessmap,	false,	true	},
    { "wiretransfer",		scriptcmd_wiretransfer,		false,	true	},
    { "wiznet",				scriptcmd_wiznet,			false,	true    },
    { "xcall",				do_mpxcall,					false,	true	},
    { "zecho",				scriptcmd_zecho,			false,	true	},
    { "zot",				scriptcmd_zot,				true,	true	},
    { NULL,					NULL,						false,	false	}
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
    SCRIPT_DATA *mprg;
    WNUM wnum;

    if (argument[0] == '\0')
    {
        send_to_char("Syntax: mpdump <vnum>\n\r", ch);
        return;
    }

    if (!parse_widevnum(argument, ch->in_room ? ch->in_room->area : NULL, &wnum))
    {
        send_to_char("Invalid vnum format.\n\r", ch);
        return;
    }

    if (!(mprg = get_script_index(wnum.pArea, wnum.vnum, PRG_MPROG))) {
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
// Syntax: mpstat [name]
//
void do_mpstat(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    CHAR_DATA *victim;
    BUFFER *output = new_buf();

    one_argument(argument, arg);

    if (!arg[0]) {
        send_to_char("Mpstat whom?\n\r", ch);
        return;
    }

    if (is_number(arg))
    {
        argument = one_argument(argument, arg);
        if (argument[0] != '\0' && is_number(arg) && is_number(argument))
        {
            if ((victim = idfind_mobile(atoi(arg), atoi(argument))) == NULL)
            {
                send_to_char("No such creature\n\r", ch);
                return;
            }
        }
        else
        {
            send_to_char("Syntax: mpstat <name|IDa IDb>",ch);
            return;
        }

    } else if (!(victim = get_char_world(ch, arg))) {
        send_to_char("No such creature.\n\r", ch);
        return;
    }

    if (!IS_NPC(victim)) {
        send_to_char("That is not a mobile.\n\r", ch);
        return;
    }

    sprintf(arg, "Mobile #%-6ld [%s] ID [%9d:%9d]\n\r", victim->pIndexData->vnum, victim->short_descr, (int)victim->id[0], (int)victim->id[1]);
    add_buf(output, arg);

    if( !IS_NULLSTR(victim->pIndexData->comments) )
    {
        sprintf(arg, "Comments:\n\r%s\n\r", victim->pIndexData->comments);
        add_buf(output, arg);
    }

    sprintf(arg, "Delay   %-6d [%s]\n\r",
    victim->progs->delay,
    victim->progs->target ? victim->progs->target->name : "No target");

    add_buf(output,arg);

    if (!victim->pIndexData->progs)
        add_buf(output, "[No programs set]\n\r");
    else
        olc_show_progs_grouped(output, victim->pIndexData->progs, PRG_MPROG, NULL);

    if(victim->progs->vars)
        pstat_variable_list(output, victim->progs->vars);

    if( !ch->lines && strlen(output->string) > MAX_STRING_LENGTH )
    {
        send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
    }
    else
    {
        page_to_char(output->string, ch);
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
    char command[MIL];
    int cmd;

    if(!info->mob) return;

    if (!str_prefix("mob ", argument))
        argument = skip_whitespace(argument+3);

    argument = one_argument(argument, command);

    cmd = mpcmd_lookup(command);
    if(cmd < 0) {
        pbugf(LOG_SCRIPTS, "Mob_interpret: invalid cmd from mob %ld: '%s'", VNUM(info->mob), command);
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
    return script_getlocation(info, argument, room);
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
                *room = get_room_index_from_info(info, x);
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
                            *room = get_clone_room(get_room_index_from_info(info, vnum),id1,id2);
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
                    *room = get_room_index_from_info(info, x);
            } else {
                // Named locations: <name>
                loc = NULL;
                // Try area names
                for (area = area_first; area; area = area->next) {
                    if (!str_infix(arg->d.str, area->name)) {
                        // Get the area's recall location
                        if(!(loc = location_to_room(&area->recall))) {
                            // Find any room in this area by iterating hash buckets
                            for (int iHash = 0; iHash < MAX_KEY_HASH && !loc; iHash++)
                                if ((loc = area->room_index_hash[iHash]) != NULL)
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
        pbugf(LOG_SCRIPTS, "Mpairshipaddwaypoint - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: pArea = find_area(arg->d.str); break;
    case ENT_AREA: pArea = arg->d.area; break;
    default: pArea = NULL; break;
    }

    if (!pArea) {
        pbugf(LOG_SCRIPTS, "MpAirshipAddWayPoint: no such area '%s'", arg->d.str);
        return;
    }

    if (!is_same_place_area(info->mob->in_room->area, pArea)) {
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
        pbugf(LOG_SCRIPTS, "Mpairshipsetcrash - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAssist - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: victim = NULL; break;
    }

    if (!victim) {
        pbugf(LOG_SCRIPTS, "MpAssist - Null victim from vnum %ld.", VNUM(info->mob));
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

// do_mpcall
SCRIPT_CMD(do_mpcall)
{
    char *rest; //buf[MSL], *rest;
    CHAR_DATA *vch = NULL,*ch = NULL;
    OBJ_DATA *obj1 = NULL,*obj2 = NULL;
    SCRIPT_DATA *script;
    int depth, ret;
    long vnum;


    DBG2ENTRY2(PTR,info,PTR,argument);
    if(info->mob) {
        DBG3MSG2("info->mob = %s(%d)\n",HANDLE(info->mob),VNUM(info->mob));
    }

    if(!info || !info->mob) return;

    if (!argument[0]) {
        pbugf(LOG_SCRIPTS, "MpCall: missing arguments from vnum %d.", VNUM(info->mob));
        return;
    }

    // Call depth checking
    depth = script_call_depth;
    if(script_call_depth == 1) {
        pbugf(LOG_SCRIPTS, "MpCall: maximum call depth exceeded for mob vnum %d.", VNUM(info->mob));
        return;
    } else if(script_call_depth > 1)
        --script_call_depth;


    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
        // Restore the call depth to the previous value
        script_call_depth = depth;
        return;
    }

    script = get_script_from_arg(info, arg, PRG_MPROG, &vnum);
    if (!script || vnum < 1) {
        pbugf(LOG_SCRIPTS, "MpCall: invalid prog from vnum %d.", VNUM(info->mob));
        return;
    }

    ch = vch = NULL;
    obj1 = obj2 = NULL;

    if(*rest) {	// Enactor
        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
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
            pbugf(LOG_SCRIPTS, "MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
            // Restore the call depth to the previous value
            script_call_depth = depth;
            return;
        }

        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
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
            pbugf(LOG_SCRIPTS, "MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
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
            pbugf(LOG_SCRIPTS, "MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
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
    ret = execute_script(script->vnum, script, info->mob, NULL, NULL, NULL, NULL, NULL, NULL, ch, obj1, obj2, vch, NULL,NULL, NULL,info->phrase,info->trigger,info->trigger_type,0,0,0,0,0);
    if(info->mob) {
        info->mob->progs->lastreturn = ret;
        DBG3MSG1("lastreturn = %d\n", info->mob->progs->lastreturn);
    } else
        info->block->ret_val = ret;

    // restore the call depth to the previous value
    script_call_depth = depth;
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
        pbugf(LOG_SCRIPTS, "MpCast - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpCast - No such spell from vnum %d.", VNUM(info->mob));
        return;
    }

    if(*rest) {
        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "MpCast - Error in parsing from vnum %ld.", VNUM(info->mob));
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
    (*skill_table[sn].spell_fun)(skill_find_uid(sn), info->mob->level, info->mob, to, skill_table[sn].target, WEAR_NONE, INVOC_INTERNAL);
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
        pbugf(LOG_SCRIPTS, "MpAwardXP - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpChargeMoney - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: victim = NULL; break;
    }

    if (!victim) {
        pbugf(LOG_SCRIPTS, "MpChargeMoney - Null victim from vnum %ld.", VNUM(info->mob));
        return;
    }

    if (!*rest) return;

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpChargeMoney - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpChargeMoney - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        int16_t sn_haggle = skill_resolve_gsn("haggle");
        roll = number_percent();
        if (roll < get_skill(victim, sn_haggle)) {
            amt -= amt * roll / 200;
            check_improve(victim, sn_haggle, true, 4);
        }
    }

    if ((victim->silver + 100 * victim->gold) < amt) {
        pbugf(LOG_SCRIPTS, "do_mpchargemoney: victim doesnt have enough cash, mob %s(%ld)", HANDLE(info->mob), VNUM(info->mob));
    }

    deduct_cost(victim, amt);
}


// do_mpdamage
SCRIPT_CMD(do_mpdamage)
{
    char buf[MSL],*rest;
    CHAR_DATA *victim = NULL, *victim_next;
    int low, high, level, value, dc;
    bool fAll = false, fKill = false, fLevel = false, fRemort = false, fTwo = false;


    if(!info || !info->mob) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpDamage - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        if(!str_cmp(arg->d.str,"all")) fAll = true;
        else victim = get_char_room(info->mob, NULL, arg->d.str);
        break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: victim = NULL; break;
    }

    if (!victim && !fAll) {
        pbugf(LOG_SCRIPTS, "MpDamage - Null victim from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "MpDamage - missing argument from vnum %ld.", VNUM(info->mob));
        return;
    }

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpDamage - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpDamage - invalid argument from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "MpDamage - missing argument from vnum %ld.", VNUM(info->mob));
        return;
    }

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpDamage - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(fLevel && !victim) {
        pbugf(LOG_SCRIPTS, "MpDamage - Level aspect used with null victim from vnum %ld.", VNUM(info->mob));
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
            pbugf(LOG_SCRIPTS, "MpDamage - invalid argument from vnum %ld.", VNUM(info->mob));
            return;
        }
        break;
    case ENT_MOBILE:
        if(fLevel) {
            if(arg->d.mob) level = arg->d.mob->tot_level;
            else {
                pbugf(LOG_SCRIPTS, "MpDamage - Null reference mob from vnum %ld.", VNUM(info->mob));
                return;
            }
            break;
        } else {
            pbugf(LOG_SCRIPTS, "MpDamage - invalid argument from vnum %ld.", VNUM(info->mob));
            return;
        }
        break;
    default:
        pbugf(LOG_SCRIPTS, "MpDamage - invalid argument from vnum %ld.", VNUM(info->mob));
        return;
    }

    // No expansion!
    argument = one_argument(rest, buf);
    if (!str_cmp(buf,"kill") || !str_cmp(buf,"lethal")) fKill = true;

    one_argument(argument, buf);
    dc = damage_class_lookup(buf);

    if(fLevel) get_level_damage(level,&low,&high,fRemort,fTwo);

    if (fAll) {
        for(victim = info->mob->in_room->people; victim; victim = victim_next) {
            victim_next = victim->next_in_room;
            if (victim != info->mob) {
                value = fLevel ? dice(low,high) : number_range(low,high);
                damage(victim, victim, fKill ? value : UMIN(victim->hit,value), TYPE_UNDEFINED, dc, false);
            }
        }
    } else {
        value = fLevel ? dice(low,high) : number_range(low,high);
        damage(victim, victim, fKill ? value : UMIN(victim->hit,value), TYPE_UNDEFINED, dc, false);
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
        pbugf(LOG_SCRIPTS, "MpDecDeity - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: victim = NULL; break;
    }

    if (!victim) {
        pbugf(LOG_SCRIPTS, "MpDecDeity - Null victim from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!expand_argument(info,rest,arg)) {
        pbugf(LOG_SCRIPTS, "MpDecDeity - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpDecPneuma - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: victim = NULL; break;
    }

    if (!victim) {
        pbugf(LOG_SCRIPTS, "MpDecPneuma - Null victim from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!expand_argument(info,rest,arg)) {
        pbugf(LOG_SCRIPTS, "MpDecPneuma - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpDecPrac - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: victim = NULL; break;
    }

    if (!victim) {
        pbugf(LOG_SCRIPTS, "MpDecPrac - Null victim from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!expand_argument(info,rest,arg)) {
        pbugf(LOG_SCRIPTS, "MpDecPrac - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpDecQuest - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: victim = NULL; break;
    }

    if (!victim) {
        pbugf(LOG_SCRIPTS, "MpDecQuest - Null victim from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!expand_argument(info,rest,arg)) {
        pbugf(LOG_SCRIPTS, "MpDecQuest - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpDecTrain - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: victim = NULL; break;
    }

    if (!victim) {
        pbugf(LOG_SCRIPTS, "MpDecTrain - Null victim from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!expand_argument(info,rest,arg)) {
        pbugf(LOG_SCRIPTS, "MpDecTrain - Error in parsing from vnum %ld.", VNUM(info->mob));
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
    bool conceal = false, pursue = true;
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
    bool fAll = false, forced;


    if(!info || !info->mob) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpForce - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        if(!str_cmp(arg->d.str,"all")) fAll = true;
        else victim = get_char_room(info->mob, NULL, arg->d.str);
        break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: break;
    }

    if (!fAll && !victim) {
        pbugf(LOG_SCRIPTS, "MpForce - Null victim from vnum %ld.", VNUM(info->mob));
        return;
    }

    BUFFER *buffer = new_buf();
    expand_string(info,rest,buffer);

    if( buf_string(buffer)[0] == '\0' ) {
        pbugf(LOG_SCRIPTS,"MpForce - Error in parsing from vnum %ld.", VNUM(info->mob));
        free_buf(buffer);
        return;
    }

    forced = forced_command;
    if (fAll) {
        for (victim = info->mob->in_room->people; victim; victim = next) {
            next = victim->next_in_room;
            if (get_staff_rank(victim) < get_staff_rank(info->mob)
                && can_see(info->mob, victim)
                && (IS_NPC(victim) || !IS_IMMORTAL(victim))) {
                forced_command = true;
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

        forced_command = true;
        interpret(victim, buf_string(buffer));
    }

    forced_command = forced;
    free_buf(buffer);
}

// do_mpgoto
// Syntax: mob goto <destination>
SCRIPT_CMD(do_mpgoto)
{
    ROOM_INDEX_DATA *dest;

    if(!info || !info->mob || !info->mob->in_room || PROG_FLAG(info->mob,PROG_AT)) return;

    if(!argument[0]) {
        pbugf(LOG_SCRIPTS, "Mpgoto - No argument from vnum %d.", VNUM(info->mob));
        return;
    }

    mp_getlocation(info, argument, &dest);

    if(!dest) {
        pbugf(LOG_SCRIPTS, "Mpgoto - Bad location from vnum %d.", VNUM(info->mob));
        return;
    }

    if (info->mob->fighting) stop_fighting(info->mob, true);

    char_from_room(info->mob);
    if(dest->wilds)
        char_to_vroom(info->mob, dest->wilds, dest->x, dest->y);
    else
        char_to_room(info->mob, dest);
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
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;
    ITERATOR it;

    if(!info || !info->mob || !argument[0]) return;

    if(expand_argument(info,argument,arg)) {
        switch(arg->type) {
        case ENT_STRING:
            if (str_cmp(arg->d.str, "all") && str_prefix("all.", arg->d.str)) {
                if (!(obj = get_obj_wear(info->mob, arg->d.str, true)))
                    obj = get_obj_carry(info->mob, arg->d.str, info->mob);
            } else {
                if (info->mob->lcarrying && IS_VALID(info->mob->lcarrying)) {
                    iterator_start(&it, info->mob->lcarrying);
                    while ((obj = iterator_nextdata(&it))) {
                        if (!arg->d.str[3] || is_name(&arg->d.str[4], obj->name)) {
                            if(!PROG_FLAG(obj,PROG_AT)) {
                                if (obj->wear_loc != WEAR_NONE)
                                    unequip_char(info->mob, obj, true);
                                extract_obj(obj);
                            }
                        }
                    }
                    iterator_stop(&it);
                }
                return;
            }
            break;
        case ENT_OBJECT:
            obj = (arg->d.obj && arg->d.obj->carried_by == info->mob) ? arg->d.obj : NULL;
            break;
        case ENT_OLLIST_OBJ:
            if (!arg->d.list.ptr.obj || !(*arg->d.list.ptr.obj))
                return;

            if (is_llist(*arg->d.list.ptr.obj)) {
                LLIST *list = (LLIST*)(*arg->d.list.ptr.obj);
                iterator_start(&it, list);
                while ((obj = iterator_nextdata(&it))) {
                    if (!PROG_FLAG(obj,PROG_AT)) {
                        if (obj->wear_loc != WEAR_NONE)
                            unequip_char(info->mob, obj, true);
                        extract_obj(obj);
                    }
                }
                iterator_stop(&it);
            } else {
                for (obj = *(arg->d.list.ptr.obj); obj; obj = obj_next) {
                    obj_next = obj->next_content;
                    if(!PROG_FLAG(obj,PROG_AT)) {
                        if (obj->wear_loc != WEAR_NONE)
                            unequip_char(info->mob, obj, true);
                        extract_obj(obj);
                    }
                }
            }
            return;
        default: obj = NULL; break;
        }

        if(obj && !PROG_FLAG(obj,PROG_AT)) {
            if(obj->wear_loc != WEAR_NONE)
                unequip_char(info->mob, obj, true);
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
        pbugf(LOG_SCRIPTS, "MpKill - Null victim from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(victim == info->mob || IS_NPC(victim) || victim->in_room != info->mob->in_room ||
        info->mob->position == POS_FIGHTING)
        return;

    if (IS_AFFECTED(info->mob, AFF_CHARM) && info->mob->master == victim) {
        pbugf(LOG_SCRIPTS, "MpKill - Charmed mob attacking master from vnum %d.", VNUM(info->mob));
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
    AREA_DATA *link_area = NULL;
    AREA_DATA *context_area = NULL;
    WNUM link_wnum = wnum_zero;
    unsigned long id1, id2;

    bool del = false;
    bool environ = false;

    if(!info || !info->mob) return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    context_area = get_area_from_scriptinfo(info);

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
        pbugf(LOG_SCRIPTS, "MPlink used without an argument from room vnum %d.", room->vnum);
        return;
    }

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg)))
        return;

    vnum = -1;
    id1 = id2 = 0;
    switch(arg->type) {
    case ENT_STRING:
        if (!IS_NULLSTR(arg->d.str) && strchr(arg->d.str, '#') != NULL) {
            if (parse_widevnum(arg->d.str, context_area, &link_wnum) && link_wnum.pArea) {
                vnum = link_wnum.vnum;
                link_area = link_wnum.pArea;
            }
        } else if(is_number(arg->d.str))
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
        link_area = arg->d.room ? arg->d.room->area : NULL;
        break;
    case ENT_EXIT:
        ex = arg->d.door.r ? arg->d.door.r->exit[arg->d.door.door] : NULL;
        vnum = ex && ex->u1.to_room ? ex->u1.to_room->vnum : -1;
        link_area = ex && ex->u1.to_room ? ex->u1.to_room->area : NULL;
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
        pbugf(LOG_SCRIPTS, "MPlink - invalid argument in room %d.", room->vnum);
        return;
    }

    if(id1 > 0 || id2 > 0) {
        if (link_area)
            dest = get_clone_room(get_room_index(link_area, vnum),id1,id2);
        else
            dest = get_clone_room(get_room_index_from_info(info, vnum),id1,id2);
    } else if(vnum > 0) {
        if (link_area)
            dest = get_room_index(link_area, vnum);
        else
            dest = get_room_index_from_info(info, vnum);
    }
    else if(environ)
        dest = &room_pointer_environment;
    else
        dest = NULL;

    if(!dest && !del) {
        pbugf(LOG_SCRIPTS, "MPlink - invalid destination in room %d.", room->vnum);
        return;
    }
    script_change_exit(room, dest, door);
}

// do_mpoload
// Syntax: mob oload <vnum> [<level>] [room|wear|$ENTITY]
SCRIPT_CMD(do_mpoload)
{
    /*
    char buf[MIL], *rest;
    long vnum, level;
    bool fToroom = false, fWear = false;
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
        pbugf(LOG_SCRIPTS, "Mpoload - Bad vnum arg from vnum %d.", VNUM(info->mob));
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

        if(level <= 0 || level > get_mob_level(info->mob))
            level = get_mob_level(info->mob);

        if(rest && *rest) {
            argument = rest;
            if(!(rest = expand_argument(info,argument,arg)))
                return;

            //
            // Added 3rd argument
            // omitted - load to mobile's inventory
            // 'none'  - load to mobile's inventory
            // 'room'  - load to room
            // 'wear'  - load to mobile and force wear
            // MOBILE  - load to target mobile
            //         - 'W' automatically wear
            // OBJECT  - load to target object
            // ROOM    - load to target room
             

            switch(arg->type) {
            case ENT_STRING:
                if(!str_cmp(arg->d.str, "room"))
                    fToroom = true;
                else if(!str_cmp(arg->d.str, "wear"))
                    fWear = true;
                break;

            case ENT_MOBILE:
                to_mob = arg->d.mob;
                if((rest = one_argument(rest,buf))) {
                    if(!str_cmp(buf, "wear"))
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
        level = get_mob_level(info->mob);

    obj = create_object(pObjIndex, level, true);
    if( to_room )
        obj_to_room(obj, to_room);
    else if( to_obj )
        obj_to_obj(obj, to_obj);
    else if( to_mob && (fWear || !fToroom) && CAN_WEAR(obj, ITEM_TAKE) &&
        (to_mob->carry_number < can_carry_n (to_mob)) &&
        (get_carry_weight (to_mob) + get_obj_weight (obj) <= can_carry_w (to_mob))) {
        obj_to_char(obj, to_mob);
        if (fWear)
            wear_obj(to_mob, obj, true);
    }
    else
        obj_to_room(obj, info->mob->in_room);

    if(rest && *rest) variables_set_object(info->var,rest,obj);
    p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
    */
    script_oload(info,argument,arg, false);
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
        pbugf(LOG_SCRIPTS, "MpOtransfer - Bad syntax from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: obj = get_obj_here(info->mob, NULL, arg->d.str); break;
    case ENT_OBJECT: obj = arg->d.obj; break;
    default: obj = NULL; break;
    }


    if (!obj) {
        pbugf(LOG_SCRIPTS, "MpOtransfer - Null object from vnum %ld.", VNUM(info->mob));
        return;
    }

    if (PROG_FLAG(obj,PROG_AT)) return;	// Can't transfer a remote looking object

    if (IS_SET(obj->extra[2], ITEM_NO_TRANSFER) && script_security < MAX_SCRIPT_SECURITY) return;

    argument = mp_getolocation(info, rest, &dest, &container, &carrier, &wear_loc);

    if(!dest && !container && !carrier) {
        pbugf(LOG_SCRIPTS, "MpOtransfer - Bad location from vnum %d.", VNUM(info->mob));
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
            pbugf(LOG_SCRIPTS, "Mppurge - Attempting to purge a PC from vnum %d.", VNUM(info->mob));
            return;
        }

        if(PROG_FLAG(victim,PROG_AT)) return;

        extract_char(victim, true);
    } else if(obj) {
        if(PROG_FLAG(obj,PROG_AT)) return;
        extract_obj(obj);
    } else if(here) {
        for (victim = here->people; victim; victim = vnext) {
            vnext = victim->next_in_room;
            if (IS_NPC(victim) && victim != info->mob && !IS_SET(victim->act[0], ACT_NOPURGE))
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
            if (IS_NPC(victim) && victim != info->mob && !IS_SET(victim->act[0], ACT_NOPURGE))
                extract_char(victim, true);
        }
    } else if(objs) {
        for (obj = *objs; obj; obj = obj_next) {
            obj_next = obj->next_content;
            if (!IS_SET(obj->extra[0], ITEM_NOPURGE))
                extract_obj(obj);
        }
    } else
        pbugf(LOG_SCRIPTS, "Mppurge - Bad argument from vnum %d.", VNUM(info->mob));

}

// do_mpraisedead
SCRIPT_CMD(do_mpraisedead)
{
    char *rest;
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
        pbugf(LOG_SCRIPTS, "do_mpraisedead: for mob %s(%ld), victim %s wasn't dead!",
            info->mob->pIndexData->short_descr, info->mob->pIndexData->vnum,
            victim->name);

        info->mob->progs->lastreturn = 0;
//		send_to_char("{WAn intense warmth washes over you momentarily.{x\n\r", victim);
        return;
    }

    resurrect_pc(victim);
    info->mob->progs->lastreturn = 1;
}

// do_mpremove
SCRIPT_CMD(do_mpremove)
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

    if(!info || !info->mob) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpRemove - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    context_area = get_area_from_scriptinfo(info);

    switch(arg->type) {
    case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: victim = NULL; break;
    }

    if (!victim) {
        pbugf(LOG_SCRIPTS, "MpRemove - Null victim from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!*rest) return;

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpRemove - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    name[0] = '\0';
    switch(arg->type) {
    case ENT_NUMBER: vnum = arg->d.num; break;
    case ENT_STRING:
        if (!IS_NULLSTR(arg->d.str) && strchr(arg->d.str, '#') != NULL) {
            if (parse_widevnum(arg->d.str, context_area, &item_wnum) && item_wnum.pArea) {
                vnum = item_wnum.vnum;
                item_area = item_wnum.pArea;
            }
        } else if(is_number(arg->d.str))
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
        pbugf(LOG_SCRIPTS, "MpRemove - Invalid object from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!fAll && !obj && *rest) {
        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "MpRemove - Error in parsing from vnum %ld.", VNUM(info->mob));
            return;
        }

        switch(arg->type) {
        case ENT_NUMBER: count = arg->d.num; break;
        case ENT_STRING: count = atoi(arg->d.str); break;
        default: count = 0; break;
        }

        if(count < 0) {
            pbugf(LOG_SCRIPTS, "MpRemove - Invalid count from vnum %d.", VNUM(info->mob));
            count = 0;
        }
    }

    if(obj) {
        if((obj->wear_loc != WEAR_NONE && obj->carried_by == victim) || 
           (obj->in_obj && obj->in_obj->carried_by == victim)) {
            // Unequip item if it's worn
            if (obj->wear_loc != WEAR_NONE)
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

// do_mpselfdestruct
SCRIPT_CMD(do_mpselfdestruct)
{
    char buf[MSL];

    if(!info || !info->mob || !IS_NPC(info->mob) || PROG_FLAG(info->mob,PROG_NODESTRUCT) || PROG_FLAG(info->mob,PROG_AT)) return;

    sprintf(buf, "do_mpselfdestruct: mob %s(%ld) self-destructed", info->mob->pIndexData->short_descr, info->mob->pIndexData->vnum);
    log_string(buf);

    extract_char(info->mob, true);
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
// do_mptake
SCRIPT_CMD(do_mptake)
{
    CHAR_DATA *victim;
    OBJ_DATA *obj = NULL;
    int vnum = 0, count = 0, taken = 0;
    bool fAll = false, force = false;
    AREA_DATA *item_area = NULL;
    AREA_DATA *context_area = NULL;
    WNUM item_wnum = wnum_zero;
    ITERATOR it;

    char name[MIL], *rest;

    if(!info || !info->mob) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpTake - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    context_area = get_area_from_scriptinfo(info);

    switch(arg->type) {
    case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: victim = NULL; break;
    }

    if (!victim) {
        pbugf(LOG_SCRIPTS, "MpTake - Null victim from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!*rest) return;

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpTake - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    name[0] = '\0';
    switch(arg->type) {
    case ENT_NUMBER: vnum = arg->d.num; break;
    case ENT_STRING:
        if (!IS_NULLSTR(arg->d.str) && strchr(arg->d.str, '#') != NULL) {
            if (parse_widevnum(arg->d.str, context_area, &item_wnum) && item_wnum.pArea) {
                vnum = item_wnum.vnum;
                item_area = item_wnum.pArea;
            }
        } else if(is_number(arg->d.str))
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
        bug ("MpTake - Invalid object from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(*rest) {
        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "MpTake - Error in parsing from vnum %ld.", VNUM(info->mob));
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
            pbugf(LOG_SCRIPTS, "MpTake - Error in parsing from vnum %ld.", VNUM(info->mob));
            return;
        }

        switch(arg->type) {
        case ENT_NUMBER: count = arg->d.num; break;
        case ENT_STRING: count = atoi(arg->d.str); break;
        case ENT_OBJECT: obj = arg->d.obj; break;
        default: count = 0; break;
        }

        if(count < 0) {
            bug ("MpTake - Invalid count from vnum %d.", VNUM(info->mob));
            count = 0;
        }
    }

    taken = 0;
    if(obj) {
        // Check if the object is directly carried or within a carried container
        if ((obj->carried_by == victim || (obj->in_obj && obj->in_obj->carried_by == victim)) && 
            victim->recite_scroll != obj && (force || can_drop_obj(victim,obj,true)) &&
            (info->mob->carry_number < can_carry_n(info->mob)) &&
            ((get_carry_weight(info->mob) + get_obj_weight(obj)) <= can_carry_w(info->mob))) {
            obj_from_char(obj);
            obj_to_char(obj, info->mob);
            taken++;
        }
    } else {
        // Check carried items (lcarrying)
        if (victim->lcarrying && IS_VALID(victim->lcarrying)) {
            iterator_start(&it, victim->lcarrying);
            while ((obj = iterator_nextdata(&it))) {
                if (fAll || (vnum > 0 && obj->pIndexData->vnum == vnum && (!item_area || obj->pIndexData->area == item_area)) ||
                    (*name && is_name(name, obj->name))) {
                    // Not even FORCE will allow this...
                    if(victim->recite_scroll == obj) continue;
                    // Can it be taken?
                    if(!force && !can_drop_obj(victim,obj,true)) continue;
                    // Can the mob carry anymore?
                    if(info->mob->carry_number >= can_carry_n(info->mob)) break;
                    if(get_carry_weight(info->mob) + get_obj_weight(obj) > can_carry_w(info->mob)) break;
                    
                    iterator_remcurrent(&it);
                    obj_to_char(obj, info->mob);
                    taken++;

                    if(count > 0 && !--count) break;
                }
            }
            iterator_stop(&it);
        }
        
        // Also check worn items (lworn)
        if (count != 0 && victim->lworn && IS_VALID(victim->lworn)) {
            iterator_start(&it, victim->lworn);
            while ((obj = iterator_nextdata(&it))) {
                if (fAll || (vnum > 0 && obj->pIndexData->vnum == vnum && (!item_area || obj->pIndexData->area == item_area)) ||
                    (*name && is_name(name, obj->name))) {
                    // Not even FORCE will allow this...
                    if(victim->recite_scroll == obj) continue;
                    // Can it be taken?
                    if(!force && !can_drop_obj(victim,obj,true)) continue;
                    // Can the mob carry anymore?
                    if(info->mob->carry_number >= can_carry_n(info->mob)) break;
                    if(get_carry_weight(info->mob) + get_obj_weight(obj) > can_carry_w(info->mob)) break;
                    
                    iterator_remcurrent(&it);
                    unequip_char(victim, obj, true);
                    obj_to_char(obj, info->mob);
                    taken++;

                    if(count > 0 && !--count) break;
                }
            }
            iterator_stop(&it);
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

    if ((room = get_random_room(info->mob, ANY_CONTINENT))) {
        char_from_room(info->mob);
        char_to_room(info->mob, room);
    }
}

// do_mpvis
SCRIPT_CMD(do_mpvis)
{
    if(!info || !info->mob) return;

    info->mob->invis_level = 0;
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
        pbugf(LOG_SCRIPTS, "MpSetTimer - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpSetTimer - NULL victim from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "MpSetTimer - Missing timer type from vnum %ld.", VNUM(info->mob));
        return;
    }

    buf[0] = 0;
    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpSetTimer - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        strncpy(buf,arg->d.str,MIL);
        break;
    default: break;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "MpSetTimer - Missing timer amount from vnum %ld.", VNUM(info->mob));
        return;
    }

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpSetTimer - Error in parsing from vnum %ld.", VNUM(info->mob));
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
    bool silent = false;

    if(!info || !info->mob) return;

    here = info->mob->in_room;

    info->mob->progs->lastreturn = 0;	// Nothing was interrupted

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpInterrupt - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpInterrupt - NULL victim from vnum %ld.", VNUM(info->mob));
        return;
    }

    BUFFER *buffer = new_buf();
    expand_string(info,rest,buffer);

    if(!buf_string(buffer)[0]) {
        stop = flag_value(interrupt_action_types,buf);
        if(stop == NO_FLAG) {
            pbugf(LOG_SCRIPTS, "MpInterrupt - invalid interrupt type from vnum %ld.", VNUM(info->mob));
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
    info->mob->progs->lastreturn = ret;

    free_buf(buffer);
}

/*
SCRIPT_CMD(do_mpalterobj)
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

    if(!info || !info->mob) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpAlterObj - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAlterObj - NULL object from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "MpAlterObj - Missing field type from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAlterObj - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAlterObj - Error in parsing from vnum %ld.", VNUM(info->mob));
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
            pbugf(LOG_SCRIPTS, "MpAlterObj - Attempting to alter value%d with security %d from vnum %ld.\n\r", num, script_security, VNUM(info->mob));
            return;
        }

        switch (buf[0]) {
        case '+': obj->value[num] += value; break;
        case '-': obj->value[num] -= value; break;
        case '*': obj->value[num] *= value; break;
        case '/':
            if (!value) {
                pbugf(LOG_SCRIPTS, "MpAlterObj - adjust called with operator / and value 0 from vnum %ld.", VNUM(info->mob));
                return;
            }
            obj->value[num] /= value;
            break;
        case '%':
            if (!value) {
                pbugf(LOG_SCRIPTS, "MpAlterObj - adjust called with operator % and value 0 from vnum %ld.", VNUM(info->mob));
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
            pbugf(LOG_SCRIPTS, "MpAlterObj - Attempting to alter '%s' with security %d from vnum %ld.\n\r", field, script_security, VNUM(info->mob));
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
                pbugf(LOG_SCRIPTS, "MpAlterObj - alterobj called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->mob));
                return;
            }

            *ptr += value;
            break;

        case '-':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "MpAlterObj - alterobj called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->mob));
                return;
            }

            *ptr -= value;
            break;

        case '*':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "MpAlterObj - alterobj called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->mob));
                return;
            }

            *ptr *= value;
            break;

        case '/':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "MpAlterObj - alterobj called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->mob));
                return;
            }

            if (!value) {
                pbugf(LOG_SCRIPTS, "MpAlterObj - adjust called with operator / and value 0 from vnum %ld.", VNUM(info->mob));
                return;
            }
            *ptr /= value;
            break;
        case '%':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "MpAlterObj - alterobj called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->mob));
                return;
            }

            if (!value) {
                pbugf(LOG_SCRIPTS, "MpAlterObj - adjust called with operator % and value 0 from vnum %ld.", VNUM(info->mob));
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



SCRIPT_CMD(do_mpresetdice)
{
    char *rest;
    OBJ_DATA *obj = NULL;


    if(!info || !info->mob) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpAlterObj - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAlterObj - NULL object from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(obj->item_type == ITEM_WEAPON)
        set_weapon_dice_obj(obj);
}




SCRIPT_CMD(do_mpstringobj)
{
    char field[MIL],*rest, **str;
    int min_sec = MIN_SCRIPT_SECURITY;
    OBJ_DATA *obj = NULL;

    bool newlines = false;

    if(!info || !info->mob) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpStringObj - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpStringObj - NULL object from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "MpStringObj - Missing field type from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpStringObj - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpStringObj - Empty string used from vnum %ld.", VNUM(info->mob));
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
        newlines = true;		// allow newlines
    } else if(!str_cmp(field,"material")) {
        int mat = material_lookup(buf_string(buffer));

        if(mat < 0) {
            pbugf(LOG_SCRIPTS, "MpStringObj - Invalid material from vnum %ld.\n\r", VNUM(info->mob));
            free_buf(buffer);
            return;
        }

        // Force material to the full name
        clear_buf(buffer);
        add_buf(buffer, material_name(mat));

        str = (char**)&obj->material;
    }
    else
    {
        free_buf(buffer);
        return;
    }

    if(script_security < min_sec) {
        pbugf(LOG_SCRIPTS,"MpStringObj - Attempting to restring '%s' with security %d from vnum %ld.\n\r", field, script_security, VNUM(info->mob));
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

    if(!info || !info->mob) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpAlterMob - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAlterMob - NULL mobile from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "MpAlterMob - Missing field type from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAlterMob - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAlterMob - Error in parsing from vnum %ld.", VNUM(info->mob));
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

    if(!ptr && !lptr) return;

        rest = one_argument(rest,buf);
    int op = cmd_operator_lookup(buf);
    if (op == OPR_UNKNOWN)
        return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "AlterMob - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }


    // MINIMUM to alter ANYTHING not allowed on players on a player
    if(!allowpc && !IS_NPC(mob)) min_sec = 9;

    if(script_security < min_sec) {
        pbugf(LOG_SCRIPTS, "MpAlterMob - Attempting to alter '%s' with security %d from vnum %ld.\n\r", field, script_security, VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAlterMob - altermob called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->mob));
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
                pbugf(LOG_SCRIPTS, "AlterMob - altermob called with operator / and value 0 from vnum %ld.", VNUM(info->mob));
                return;
            }
            *lptr /= value;
            break;

        case OPR_MOD:
            if (!value) {
                pbugf(LOG_SCRIPTS, "AlterMob - altermob called with operator % and value 0 from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAlterMob - altermob called with bitwise operator on a non-bitvector field from vnum %ld.", VNUM(info->mob));
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
                pbugf(LOG_SCRIPTS, "AlterMob - altermob called with operator / and value 0 from vnum %ld.", VNUM(info->mob));
                return;
            }
            *ptr /= value;
            break;

        case OPR_MOD:
            if (!value) {
                pbugf(LOG_SCRIPTS, "AlterMob - altermob called with operator % and value 0 from vnum %ld.", VNUM(info->mob));
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


SCRIPT_CMD(do_mpstringmob)
{
    char buf[MSL+2],field[MIL],*rest, **str;
    int min_sec = MIN_SCRIPT_SECURITY;
    CHAR_DATA *mob = NULL;

    bool newlines = false;

    if(!info || !info->mob) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpStringMob - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpStringMob - NULL mobile from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!IS_NPC(mob)) {
        pbugf(LOG_SCRIPTS, "MpStringMob - can't change strings on PCs from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "MpStringMob - Missing field type from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpStringMob - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpStringMob - Empty string used from vnum %ld.", VNUM(info->mob));
        free_buf(buffer);
        return;
    }

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
        pbugf(LOG_SCRIPTS, "MpStringMob - Attempting to restring '%s' with security %d from vnum %ld.\n\r", field, script_security, VNUM(info->mob));
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
    bool success = false;

    if(script_security < MIN_SCRIPT_SECURITY) {
        pbugf(LOG_SCRIPTS, "MpSkImprove - Insufficient security from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!info || !info->mob) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpSkImprove - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpSkImprove - NULL target from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(mob) {
        if(IS_NPC(mob)) {
            pbugf(LOG_SCRIPTS, "MpSkImprove - NPCs don't have skills to improve yet from vnum %ld.", VNUM(info->mob));
            return;
        }


        if(!(rest = expand_argument(info,rest,arg))) {
            pbugf(LOG_SCRIPTS, "MpSkImprove - Error in parsing from vnum %ld.", VNUM(info->mob));
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
            pbugf(LOG_SCRIPTS, "MpSkImprove - Token is not a spell token from vnum %ld.", VNUM(info->mob));
            return;
        }
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpSkImprove - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: diff = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
    case ENT_NUMBER: diff = arg->d.num; break;
    default: return;
    }

    min_diff = 10 - script_security;	// min=10, max=1

    if(diff < min_diff) {
        pbugf(LOG_SCRIPTS, "MpSkImprove - Attempting to use a difficulty multiplier lower than allowed from vnum %ld.", VNUM(info->mob));
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

SCRIPT_CMD(do_mpinput)
{
    char *rest, *p;
    long vnum;
    CHAR_DATA *mob = NULL;
    SCRIPT_DATA *script = NULL;


    if(!info || !info->mob) return;

    info->mob->progs->lastreturn = 0;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpInput - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpInput - NULL mobile from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(IS_NPC(mob) || !mob->desc || is_char_busy(mob) || mob->desc->pString != NULL || mob->desc->input) return;

    if( mob->desc->showstr_head != NULL ) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpInput - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    script = get_script_from_arg(info, arg, PRG_MPROG, &vnum);
    if(vnum < 1 || !script) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpInput - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpRawkill - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpRawkill - NULL mobile from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(IS_IMMORTAL(mob)) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpRawkill - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: type = flag_lookup(arg->d.str,corpse_types); break;
    default: return;
    }

    if(type < 0 || type == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpRawkill - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpRawkill - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAddAffect - NULL target from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: where = flag_lookup(arg->d.str,apply_types); break;
    default: return;
    }

    if(where == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: skill = skill_lookup(arg->d.str); break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: loc = flag_lookup(arg->d.str,apply_flags_full); break;
    default: return;
    }

    if(loc == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: mod = arg->d.num; break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: hours = arg->d.num; break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: bv = flag_value(affect_flags,arg->d.str); break;
    default: return;
    }

    if(bv == NO_FLAG) bv = 0;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: bv2 = flag_value(affect2_flags,arg->d.str); break;
    default: return;
    }

    if(bv2 == NO_FLAG) bv2 = 0;

    if(rest && *rest) {
        if(!(rest = expand_argument(info,rest,arg))) {
            pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAddAffect - NULL target from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: where = flag_lookup(arg->d.str,apply_types); break;
    default: return;
    }

    if(where == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }



    switch(arg->type) {
    case ENT_STRING: name = create_affect_cname(arg->d.str); break;
    default: return;
    }

    if(!name) {
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error allocating affect name from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: loc = flag_lookup(arg->d.str,apply_flags_full); break;
    default: return;
    }

    if(loc == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: mod = arg->d.num; break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: hours = arg->d.num; break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: bv = flag_value(affect_flags,arg->d.str); break;
    default: return;
    }

    if(bv == NO_FLAG) bv = 0;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: bv2 = flag_value(affect2_flags,arg->d.str); break;
    default: return;
    }

    if(bv2 == NO_FLAG) bv2 = 0;

    if(rest && *rest) {
        if(!(rest = expand_argument(info,rest,arg))) {
            pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpStripaffect - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpStripaffect - NULL target from vnum %ld.", VNUM(info->mob));
        return;
    }


    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpStripaffect - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpStripaffect - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpStripaffect - NULL target from vnum %ld.", VNUM(info->mob));
        return;
    }


    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpStripaffect - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpUseCatalyst - NULL target from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: type = flag_value(catalyst_types,arg->d.str); break;
    default: return;
    }

    if(type == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: method = flag_value(catalyst_method_types,arg->d.str); break;
    default: return;
    }

    if(method == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: amount = arg->d.num; break;
    case ENT_STRING: amount = atoi(arg->d.str); break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: min = arg->d.num; break;
    case ENT_STRING: min = atoi(arg->d.str); break;
    default: return;
    }

    if(min < 1 || min > CATALYST_MAXSTRENGTH) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: max = arg->d.num; break;
    case ENT_STRING: max = atoi(arg->d.str); break;
    default: return;
    }

    if(max < min || max > CATALYST_MAXSTRENGTH) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->mob));
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
    int16_t *sptr = NULL;
    char **str;
    int min, max;
    bool hasmin = false, hasmax = false;
    bool allowarith = true;
    const struct flag_type *flags = NULL;

    if(!info || !info->mob) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpAlterExit - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    room = info->mob->in_room;

    switch(arg->type) {
    case ENT_ROOM:
        room = arg->d.room;
        if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING) {
            pbugf(LOG_SCRIPTS, "MpAlterExit - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAlterExit - Missing field type from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAlterExit - Error in parsing from vnum %ld.", VNUM(info->mob));
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
            pbugf(LOG_SCRIPTS, "MpAlterExit - Error in parsing from vnum %ld.", VNUM(info->mob));
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

        if(!buf_string(buffer)[0]) {
            pbugf(LOG_SCRIPTS, "MpAlterExit - Empty string used from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpAlterExit - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        sprintf(buf,"MpAlterExit - Attempting to alter '%s' with security %d.\n\r", field, script_security);
        wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
        pbug(LOG_SCRIPTS, buf);
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
                pbugf(LOG_SCRIPTS, "MpAlterExit - alterexit called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->mob));
                return;
            }
            *ptr += value;
            break;
        case '-':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "MpAlterExit - alterexit called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->mob));
                return;
            }
            *ptr -= value;
            break;
        case '*':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "MpAlterExit - alterexit called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->mob));
                return;
            }
            *ptr *= value;
            break;
        case '/':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "MpAlterExit - alterexit called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->mob));
                return;
            }
            if (!value) {
                pbugf(LOG_SCRIPTS, "MpAlterExit - adjust called with operator / and value 0 from vnum %ld.", VNUM(info->mob));
                return;
            }
            *ptr /= value;
            break;
        case '%':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "MpAlterExit - alterexit called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->mob));
                return;
            }
            if (!value) {
                pbugf(LOG_SCRIPTS, "MpAlterExit - adjust called with operator % and value 0 from vnum %ld.", VNUM(info->mob));
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
                pbugf(LOG_SCRIPTS, "MpAlterExit - adjust called with operator / and value 0 from vnum %ld.", VNUM(info->mob));
                return;
            }
            *sptr /= value;
            break;
        case '%':
            if (!value) {
                pbugf(LOG_SCRIPTS, "MpAlterExit - adjust called with operator % and value 0 from vnum %ld.", VNUM(info->mob));
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


// SYNTAX: mob prompt <player> <name>[ <string>]
SCRIPT_CMD(do_mpprompt)
{
    char name[MIL],*rest;
    CHAR_DATA *mob = NULL;


    if(!info || !info->mob) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpPrompt - Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpPrompt - NULL mobile from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(IS_NPC(mob)) {
        pbugf(LOG_SCRIPTS, "MpPrompt - cannot set prompt strings on NPCs from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "MpPrompt - Missing name type from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpPrompt - Error in parsing from vnum %ld.", VNUM(info->mob));
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
    bool no_env = false;

    if(!info || !info->mob) return;

    info->progs->lastreturn = 0;

    // Get vnum
    if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
        return;

    vnum = arg->d.num;

    source = get_room_index_from_info(info, vnum);
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

    room = get_room_index_from_info(info, vnum);
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
        pbugf(LOG_SCRIPTS, "MpShowMap - bad target for showing the map from vnum %ld.", VNUM(info->mob));
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
    int depth, ret, space = PRG_MPROG;
    long vnum;


    DBG2ENTRY2(PTR,info,PTR,argument);
    if(info->mob) {
        DBG3MSG2("info->mob = %s(%d)\n",HANDLE(info->mob),VNUM(info->mob));
    }

    if(!info || !info->mob) return;

    if (!argument[0]) {
        pbugf(LOG_SCRIPTS, "MpCall: missing arguments from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(script_security < 5) {
        pbugf(LOG_SCRIPTS, "MpCall: Minimum security needed is 5 from vnum %ld.", VNUM(info->mob));
        return;
    }


    // Call depth checking
    depth = script_call_depth;
    if(script_call_depth == 1) {
        pbugf(LOG_SCRIPTS, "MpCall: maximum call depth exceeded for mob vnum %ld.", VNUM(info->mob));
        return;
    } else if(script_call_depth > 1)
        --script_call_depth;


    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpCall: No entity target from vnum %ld.", VNUM(info->mob));
        // Restore the call depth to the previous value
        script_call_depth = depth;
        return;
    }

    if(mob && !IS_NPC(mob)) {
        pbugf(LOG_SCRIPTS, "MpCall: Invalid target for xcall.  Players cannot do scripts from vnum %ld.", VNUM(info->mob));
        // Restore the call depth to the previous value
        script_call_depth = depth;
        return;
    }


    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
        // Restore the call depth to the previous value
        script_call_depth = depth;
        return;
    }

    script = get_script_from_arg(info, arg, space, &vnum);
    if (!script || vnum < 1) {
        pbugf(LOG_SCRIPTS, "MpCall: invalid prog from vnum %ld.", VNUM(info->mob));
        return;
    }

    ch = vch = NULL;
    obj1 = obj2 = NULL;

    if(*rest) {	// Enactor
        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
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
            pbugf(LOG_SCRIPTS, "MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
            // Restore the call depth to the previous value
            script_call_depth = depth;
            return;
        }

        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
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
            pbugf(LOG_SCRIPTS, "MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
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
            pbugf(LOG_SCRIPTS, "MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
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
    ret = execute_script(script->vnum, script, mob, obj, room, token, NULL, NULL, NULL, ch, obj1, obj2, vch, NULL,NULL, NULL,info->phrase,info->trigger,info->trigger_type,0,0,0,0,0);
    if(info->mob) {
        info->mob->progs->lastreturn = ret;
        DBG3MSG1("lastreturn = %d\n", info->mob->progs->lastreturn);
    } else
        info->block->ret_val = ret;

    // restore the call depth to the previous value
    script_call_depth = depth;
}


// do_mpsetrecall
// mob setrecall $MOBILE <location>
// Sets the recall point of the target mobile to the reference of the location
SCRIPT_CMD(do_mpsetrecall)
{
    char /*buf[MSL], - Unused???*/ *rest;
    CHAR_DATA *victim;
    ROOM_INDEX_DATA *room;
    ROOM_INDEX_DATA *location;
//	int amount = 0; - Unused???


    if(!info || !info->mob) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "MpSetRecall - Bad syntax from vnum %ld.", VNUM(info->mob));
        return;
    }

    victim = NULL;
    room = NULL;

    switch(arg->type) {
    case ENT_STRING:
        victim = get_char_world(info->mob, arg->d.str);
        break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    case ENT_ROOM: room = arg->d.room; break;
    default: victim = NULL; room = NULL; break;
    }


    if (!victim && !room) {
        pbugf(LOG_SCRIPTS, "MpSetRecall - Null victim from vnum %ld.", VNUM(info->mob));
        return;
    }

    argument = mp_getlocation(info, rest, &location);

    if(!location) {
        pbugf(LOG_SCRIPTS, "MpSetRecall - Bad location from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpClearRecall - Bad syntax from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpClearRecall - Null victim from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpHunt - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: prey = get_char_world(info->mob, arg->d.str); break;
    case ENT_MOBILE: prey = arg->d.mob; break;
    default: prey = NULL; break;
    }

    if (!prey) {
        pbugf(LOG_SCRIPTS, "MpStartCombat - Null victim from vnum %ld.", VNUM(info->mob));
        return;
    }

    if(*rest) {
        if(!expand_argument(info,rest,arg)) {
            pbugf(LOG_SCRIPTS, "MpHunt - Error in parsing from vnum %ld.", VNUM(info->mob));
            return;
        }

        hunter = prey;

        switch(arg->type) {
        case ENT_STRING: prey = get_char_world(info->mob, arg->d.str); break;
        case ENT_MOBILE: prey = arg->d.mob; break;
        default: prey = NULL; break;
        }

        if (!prey) {
            pbugf(LOG_SCRIPTS, "MpHunt - Null victim from vnum %ld.", VNUM(info->mob));
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
        pbugf(LOG_SCRIPTS, "MpStopHunt - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    stay = !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str,"yes") || !str_cmp(arg->d.str,"stay");

    if(!expand_argument(info,rest,arg)) {
        pbugf(LOG_SCRIPTS, "MpStopHunt - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_NONE: hunter = info->mob; break;
    case ENT_STRING: hunter = get_char_world(info->mob, arg->d.str); break;
    case ENT_MOBILE: hunter = arg->d.mob; break;
    default: hunter = NULL; break;
    }

    if (!hunter) {
        pbugf(LOG_SCRIPTS, "MpStopHunt - Null hunter from vnum %ld.", VNUM(info->mob));
        return;
    }

    stop_hunt(hunter, stay);
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
    bool fAdd = false;

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


// do_mpremspell
SCRIPT_CMD(do_mpremspell)
{
    char *rest;
    SPELL_DATA *spell, *spell_prev;
    OBJ_DATA *target;
    int level;
    int sn;
    bool found = false, show = true;
    AFFECT_DATA *paf;
    ITERATOR it;

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
                if (paf->type == sn && paf->slot == target->wear_loc)
                    break;
            }

            if( !paf ) {
                // This spell was not applied by this object
                return;
            }

            found = false;
            level = 0;

            // Check if character has lworn (linked list)
            if (target->carried_by->lworn && IS_VALID(target->carried_by->lworn)) {
                // Use iterator for linked list of worn items
                iterator_start(&it, target->carried_by->lworn);
                while ((obj_tmp = iterator_nextdata(&it))) {
                    if (obj_tmp != target) {
                        for (spell = obj_tmp->spells; spell != NULL; spell = spell->next) {
                            if (spell->sn == sn && spell->level > level) {
                                level = spell->level;    // Keep the maximum
                                found_loc = obj_tmp->wear_loc;
                                found = true;
                            }
                        }
                    }
                }
                iterator_stop(&it);
            }

            if(!found) {
                // No other worn object had this spell available

                if( show ) {
                    if (skill_table[sn].msg_off) {
                        send_to_char(skill_table[sn].msg_off, target->carried_by);
                        send_to_char("\n\r", target->carried_by);
                    }
                }

                affect_strip(target->carried_by, sn);
            } else if( level > spell_level ) {
                level -= spell_level;        // Get the difference

                // Update all affects to the current maximum and its slot
                for(; paf; paf = paf->next) {
                    if(paf->type == sn && paf->slot == target->wear_loc) {
                        paf->level += level;
                        paf->slot = found_loc;
                    }
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
        pbugf(LOG_SCRIPTS, "MpAlterAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    if( IS_NULLSTR(rest) ) return;

    if( arg->type != ENT_STRING || IS_NULLSTR(arg->d.str) ) return;

    strncpy(field,arg->d.str,MIL-1);


    if( !str_cmp(field, "level") ) {
        argument = one_argument(rest,buf);

        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "MpAlterAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
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
            pbugf(LOG_SCRIPTS, "MpAlterAffect - Error in parsing from vnum %ld.", VNUM(info->mob));
            return;
        }

        if( paf->slot != WEAR_NONE ) {
            pbugf(LOG_SCRIPTS, "MpAlterAffect - Attempting to modify duration of an object given affect from vnum %ld.", VNUM(info->mob));
            return;
        }

        if( paf->group == AFFGROUP_RACIAL ) {
            pbugf(LOG_SCRIPTS, "MpAlterAffect - Attempting to modify duration of a racial affect from vnum %ld.", VNUM(info->mob));
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

    mob->remort_question = true;
    send_to_char("Are you ready to be reborn? (yes/no)\n\r", mob);

    info->mob->progs->lastreturn = 1;
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
    bool fShow = true;

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

    bool fAll = false;

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

