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
#include "debug.h"
#include "skill_data.h"

static AREA_DATA *script_relative_widevnum_context(AREA_DATA *context_area, const char *argument)
{
    if (!context_area || IS_NULLSTR(argument) || argument[0] != '#')
        return NULL;

    return context_area;
}

// Commands used by token scripts
const struct script_cmd_type token_cmd_table[] = {
    { "addaffect",            scriptcmd_addaffect,        true,   true    },
    { "addaffectname",        scriptcmd_addaffectname,    true,   true    },
    { "addaura",             scriptcmd_addaura,          true,   true    },
    { "addspell",             scriptcmd_addspell,         true,   true    },
    { "addstache",            scriptcmd_addstache,        true,   true    },
    { "adjust",               do_tpadjust,                false,  true    },
    { "alteraffect",          scriptcmd_alteraffect,      true,   true    },
    { "alterexit",            do_tpalterexit,             false,  true    },
    { "altermob",             do_tpaltermob,              true,   true    },
    { "alterobj",             scriptcmd_alterobj,         true,   true    },
    { "alterroom",            scriptcmd_alterroom,        true,   true    },
    { "applytoxin",           scriptcmd_applytoxin,       false,  true    },
    { "asound",               scriptcmd_asound,           false,  true    },
    { "attach",               scriptcmd_attach,           true,   true    },
    { "award",                scriptcmd_award,            true,   true    },
    { "breathe",              scriptcmd_breathe,          false,  true    },
    { "call",                 scriptcmd_call,             false,  true    },
    { "castfailure",          do_tpcastfailure,           false,  true    },
    { "castrecover",          do_tpcastrecover,           false,  true    },
    { "chargebank",           scriptcmd_chargebank,       false,  true    },
    { "checkpoint",           scriptcmd_checkpoint,       false,  true    },
    { "churchannouncetheft",  scriptcmd_churchannouncetheft, true, true },
    { "cloneroom",            do_tpcloneroom,             true,   true    },
    { "condition",            scriptcmd_condition,         false,  true    },
    { "crier",                scriptcmd_crier,            false,  true    },
    { "damage",               scriptcmd_damage,           false,  true    },
    { "deduct",               scriptcmd_deduct,           true,   true    },
    { "dequeue",              scriptcmd_dequeue,          false,  false   },
    { "destroyroom",          do_tpdestroyroom,           true,   true    },
    { "detach",               scriptcmd_detach,           true,   true    },
    { "dungeoncomplete",      scriptcmd_dungeoncomplete,  true,   true    },
    { "dungeoncommence",      scriptcmd_dungeoncommence,  true,   true    },
    { "dungeonfailure",       scriptcmd_dungeonfailure,   true,   true    },
    { "echo",                 scriptcmd_echo,             false,  true    },
    { "event",                scriptcmd_event,            false,  true    },
    { "phaseevent",           scriptcmd_phaseevent,       false,  true    },
    { "startevent",           scriptcmd_startevent,       false,  true    },
    { "stopevent",            scriptcmd_stopevent,        false,  true    },
    { "echoaround",           scriptcmd_echoaround,       false,  true    },
    { "echoat",               scriptcmd_echoat,           false,  true    },
    { "echobattlespam",       scriptcmd_echobattlespam,   false,  true    },
    { "echochurch",           scriptcmd_echochurch,       false,  true    },
    { "echogrouparound",      scriptcmd_echogrouparound,  false,  true    },
    { "echogroupat",          scriptcmd_echogroupat,      false,  true    },
    { "echoleadaround",       scriptcmd_echoleadaround,   false,  true    },
    { "echoleadat",           scriptcmd_echoleadat,       false,  true    },
    { "echonotvict",          scriptcmd_echonotvict,      false,  true    },
    { "echoroom",             scriptcmd_echoroom,         false,  true    },
    { "ed",                   scriptcmd_ed,               false,  true    },
    { "entercombat",          scriptcmd_entercombat,      false,  true    },
    { "fade",                 scriptcmd_fade,             true,   true    },
    { "fixaffects",           scriptcmd_fixaffects,       false,  true    },
    { "flee",                 scriptcmd_flee,             false,  true    },
    { "force",                scriptcmd_force,            false,  true    },
    { "forget",               scriptcmd_forget,           false,  false   },
    { "gdamage",              scriptcmd_gdamage,          false,  true    },
    { "gecho",                scriptcmd_gecho,            false,  true    },
    { "gforce",               scriptcmd_gforce,           false,  true    },
    { "give",                 do_tpgive,                  false,  true    },
    { "goto",                 scriptcmd_goto,             false,  true    },
    { "grantclass",           scriptcmd_grantclass,       false,  true    },
    { "grantskill",           scriptcmd_grantskill,       false,  true    },
    { "grantsong",            scriptcmd_grantsong,        false,  true    },
    { "group",                do_tpgroup,                 false,  true    },
    { "gtransfer",            scriptcmd_gtransfer,        false,  true    },
    { "input",                scriptcmd_input,            false,  true    },
    { "inputstring",          scriptcmd_inputstring,      false,  true    },
    { "instancecomplete",     scriptcmd_instancecomplete, true,   true    },
    { "instancefailure",      scriptcmd_instancefailure,  true,   true    },
    { "interrupt",            scriptcmd_interrupt,        false,  true    },
    { "junk",                 do_tpjunk,                  false,  true    },
    { "link",                 do_tplink,                  false,  true    },
    { "loadinstanced",        scriptcmd_loadinstanced,    true,   true    },
    { "lockadd",              scriptcmd_lockadd,          false,  true    },
    { "lockremove",           scriptcmd_lockremove,       false,  true    },
    { "mail",                 scriptcmd_mail,             true,   true    },
    { "mload",                scriptcmd_mload,            false,  true    },
    { "mute",                 scriptcmd_mute,             false,  true    },
    { "oload",                scriptcmd_oload,            false,  true    },
    { "otransfer",            do_tpotransfer,             false,  true    },
    { "pageat",               scriptcmd_pageat,           false,  true    },
    { "peace",                scriptcmd_peace,            false,  false   },
    { "persist",              scriptcmd_persist,          false,  true    },
    { "prompt",               scriptcmd_prompt,           false,  true    },
    { "purge",                scriptcmd_purge,            false,  false   },
    { "questaccept",          scriptcmd_questaccept,      false,  true    },
    { "questcancel",          scriptcmd_questcancel,      false,  true    },
    { "questcomplete",        scriptcmd_questcomplete,    false,  true    },
    { "questgenerate",        scriptcmd_questgenerate,    false,  true    },
    { "questsetindex",        scriptcmd_questsetindex,    false,  true    },
    { "questpartcustom",      scriptcmd_questpartcustom,  true,   true    },
    { "questpartgetitem",     scriptcmd_questpartgetitem, true,   true    },
    { "questpartgoto",        scriptcmd_questpartgoto,    true,   true    },
    { "questpartrescue",      scriptcmd_questpartrescue,  true,   true    },
    { "questpartslay",        scriptcmd_questpartslay,    true,   true    },
    { "questscroll",          scriptcmd_questscroll,      false,  true    },
    { "queue",                scriptcmd_queue,            false,  true    },
    { "raisedead",            scriptcmd_raisedead,        true,   true    },
    { "rawkill",              do_tprawkill,               false,  true    },
    { "reckoning",            scriptcmd_reckoning,        true,   true    },
    { "remember",             scriptcmd_remember,         false,  true    },
    { "remort",               scriptcmd_remort,           true,   true    },
    { "remove",               do_tpremove,                false,  true    },
    { "remspell",             scriptcmd_remspell,         true,   true    },
    { "remstache",            scriptcmd_remstache,        true,   true    },
    { "remaura",             scriptcmd_remaura,          true,   true    },
    { "resetdice",            scriptcmd_resetdice,        true,   true    },
    { "resetroom",            scriptcmd_resetroom,        true,   true    },
    { "restore",              scriptcmd_restore,          true,   true    },
    { "revokeclass",          scriptcmd_revokeclass,      false,  true    },
    { "revokeskill",          scriptcmd_revokeskill,      false,  true    },
    { "revokesong",           scriptcmd_revokesong,       false,  true    },
    { "saveplayer",           scriptcmd_saveplayer,       false,  true    },
    { "scriptwait",           scriptcmd_scriptwait,       true,   true    },
    { "sendfloor",            scriptcmd_sendfloor,        false,  true    },
    { "setclass",             scriptcmd_setclass,         false,  true    },
    { "setclasslevel",        scriptcmd_setclasslevel,    false,  true    },
    { "setposition",          scriptcmd_setposition,      true,   true    },
    { "setrace",              scriptcmd_setrace,          false,  true    },
    { "setrecall",            scriptcmd_setrecall,        false,  true    },
    { "settimer",             scriptcmd_settimer,         false,  true    },
    { "settrait",             scriptcmd_settrait,         false,  true    },
    { "addtrait",             scriptcmd_addtrait,         false,  true    },
    { "adjusttrait",          scriptcmd_adjusttrait,      false,  true    },
    { "removetrait",          scriptcmd_removetrait,      false,  true    },
    { "showcommand",          scriptcmd_showcommand,      false,  true    },
    { "showroom",             scriptcmd_showroom,         true,   true    },
    { "shop",                 scriptcmd_shop,             true,   true    },
    { "skimprove",            scriptcmd_skimprove,        true,   true    },
    { "spawndungeon",         scriptcmd_spawndungeon,     true,   true    },
    { "specialkey",           scriptcmd_specialkey,       false,  true    },
    { "startcombat",          scriptcmd_startcombat,      false,  true    },
    { "startreckoning",       scriptcmd_startreckoning,   true,   true    },
    { "stopcombat",           scriptcmd_stopcombat,       false,  true    },
    { "stopreckoning",        scriptcmd_stopreckoning,    true,   true    },
    { "stringmob",            scriptcmd_stringmob,        true,   true    },
    { "stringobj",            scriptcmd_stringobj,        true,   true    },
    { "stripaffect",          scriptcmd_stripaffect,      true,   true    },
    { "stripaffectname",      scriptcmd_stripaffectname,  true,   true    },
    { "transfer",             scriptcmd_transfer,         false,  true    },
    { "treasuremap",          scriptcmd_treasuremap,      false,  true    },
    { "ungroup",              scriptcmd_ungroup,          false,  true    },
    { "unlockarea",           scriptcmd_unlockarea,       true,   true    },
    { "unlockdungeon",        scriptcmd_unlockdungeon,    true,   true    },
    { "unlockdungeon",        scriptcmd_unlockdungeon,    true,   true    },
    { "unmute",               scriptcmd_unmute,           false,  true    },
    { "usecatalyst",          do_tpusecatalyst,           false,  true    },
    { "varclear",             scriptcmd_varclear,         false,  true    },
    { "varclearon",           scriptcmd_varclearon,       false,  true    },
    { "varcopy",              scriptcmd_varcopy,          false,  true    },
    { "varsave",              scriptcmd_varsave,          false,  true    },
    { "varsaveon",            scriptcmd_varsaveon,        false,  true    },
    { "varset",               scriptcmd_varset,           false,  true    },
    { "varseton",             scriptcmd_varseton,         false,  true    },
    { "vforce",               scriptcmd_vforce,           false,  true    },
    { "wildsoverlay",         scriptcmd_wildsoverlay,     false,  true    },
    { "wildstile",            scriptcmd_wildstile,        false,  true    },
    { "wildernessmap",        scriptcmd_wildernessmap,    false,  true    },
    { "wiretransfer",         scriptcmd_wiretransfer,     false,  true    },
    { "wiznet",               scriptcmd_wiznet,           false,  true    },
    { "xcall",                scriptcmd_xcall,            false,  true    },
    { "zecho",                scriptcmd_zecho,            false,  true    },
    { "zot",                  scriptcmd_zot,              true,   true    },
    { NULL,                    NULL,                       false,  false   }
};

// Commands accessible by other scripts
const struct script_cmd_type tokenother_cmd_table[] = {
    { "adjust",     do_tpadjust, false, true  },
    { "give",       do_tpgive,   false, true  },
    { "junk",       do_tpjunk,   false, true  },
    { NULL,          NULL,         false, false }
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
    char buf[MAX_INPUT_LENGTH];
    SCRIPT_DATA *tprg;
    WNUM wnum;

    one_argument(argument, buf);

    if (!parse_widevnum(buf, script_relative_widevnum_context(ch->in_room ? ch->in_room->area : NULL, buf), &wnum))
    {
        send_to_char("Syntax:  tpdump <widevnum>\n\r", ch);
        return;
    }

    if (!(tprg = get_script_index(wnum.pArea, wnum.vnum, PRG_TPROG))) {
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
 * Syntax: tpstat [target] [vnum] [index]
 */
void do_tpstat(CHAR_DATA *ch, char *argument)
{
    char arg[MSL], arg2[MSL], arg3[MSL];
    TOKEN_DATA *token = NULL;
    CHAR_DATA *victim = NULL;
    OBJ_DATA *object = NULL;
    ROOM_INDEX_DATA *room = NULL;
    int count = 0;
    long vnum = 0;
    bool id_lookup = false;
    BUFFER *output = new_buf();

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0') {
        send_to_char("Syntax:  tpstat <mobile name|object name|room> [<count>.]<token vnum>\n\r", ch);
        return;
    }

    if (is_number(arg))
    {
        if (arg[0] != '\0' && is_number(arg) && is_number(arg2))
        {
            if ((token = idfind_token(atoi(arg), atoi(arg2))) == NULL)
            {
                send_to_char("No such token\n\r", ch);
                return;
            }
            else
            {
                id_lookup = true;
            }
        }
        else
        {
            send_to_char("Syntax:  tpstat <mobile name|object name|room|ida idb> [[<count>.]<token vnum>]",ch);
            return;
        }

    } else if (!str_cmp(arg,"mob")) {
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
        send_to_char("Syntax:  tpstat <mobile name|object name|room|ida idb> [[<count>.]<token vnum>]>\n\r", ch);
        return;
    }

    if (arg3[0] != '\0' && !id_lookup) {
        WNUM wnum = { NULL, 0 };
        AREA_DATA *token_area = NULL;
        if (!parse_widevnum(arg3, script_relative_widevnum_context(ch->in_room ? ch->in_room->area : NULL, arg3), &wnum)) {
            send_to_char("Invalid token vnum format.\n\r", ch);
            return;
        }
        vnum = wnum.vnum;

        if (wnum.pArea) {
            if (get_token_index(wnum.pArea, wnum.vnum) == NULL) {
                send_to_char("That token vnum does not exist.\n\r", ch);
                return;
            }
            token_area = wnum.pArea;
        } else if (get_token_index_global(wnum.vnum) == NULL) {
            send_to_char("That token vnum does not exist.\n\r", ch);
            return;
        }

        if (victim && !(token = get_token_char(victim, vnum, token_area, count))) {
            act("$N doesn't have that token.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            return;
        }

        if (object && !(token = get_token_obj(object, vnum, token_area, count))) {
            act("$p doesn't have that token.", ch, NULL, NULL, object, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            return;
        }

        if (room && !(token = get_token_room(room, vnum, token_area, count))) {
            send_to_char("The room doesn't have that token.", ch);
            return;
        }

    }

    if(!token) {
        send_to_char("Token not found.\n\r", ch);
        return;
    }
    sprintf(arg, "Token #%-6ld [%s] ID [%09d:%09d]\n\r", token->pIndexData->vnum, token->pIndexData->name, (int)token->id[0], (int)token->id[1]);
    add_buf(output, arg);

    if( !IS_NULLSTR(token->pIndexData->comments) )
    {
        sprintf(arg, "Comments:\n\r%s\n\r", token->pIndexData->comments);
        send_to_char(arg, ch);
    }

    sprintf(arg, "Delay   %-6d [%s]\n\r",
        token->progs->delay,
        token->progs->target ? token->progs->target->name : "No target");

    add_buf(output, arg);

    if (!token->pIndexData || !token->pIndexData->progs)
        add_buf(output, "[No programs set]\n\r");
    else
        olc_show_progs_grouped(output, token->pIndexData->progs, PRG_TPROG, NULL);

    if(token->progs->vars)
        pstat_variable_list(output, token->progs->vars);

    if( !ch->lines && strlen(output->string) > MAX_STRING_LENGTH )
    {
        send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
    }
    else
    {
        page_to_char(output->string, ch);
    }

}

char *tp_getlocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room)
{
    return script_getlocation(info, argument, room);
}

char *tp_getolocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room, OBJ_DATA **container, CHAR_DATA **carrier, int *wear_loc)
{
    char *rest, *rest2;
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
            } else {
                WNUM room_wnum;
                if (resolve_widevnum(x, NULL, &room_wnum))
                    *room = get_room_index(room_wnum.pArea, room_wnum.vnum);
            }
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
                        if(!(loc = get_area_recall_room(area))) {
                            // Find any room in this area by iterating hash buckets
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
    char command[MAX_INPUT_LENGTH];
    int cmd;

    if(!info->token) return;

    if (!str_prefix("token ", argument))
        argument = skip_whitespace(argument+5);

    argument = one_argument(argument, command);

    cmd = tpcmd_lookup(command,true);

    if(cmd < 0) {
        pbugf(LOG_SCRIPTS, "Token_interpret: invalid cmd from token %ld: '%s'", info->token->pIndexData->vnum, command);
        return;
    }

    SCRIPT_PARAM *arg = new_script_param();
    (*token_cmd_table[cmd].func) (info, argument, arg);
    free_script_param(arg);
    tail_chain();
}

void tokenother_interpret(SCRIPT_VARINFO *info, char *argument)
{
    char command[MAX_INPUT_LENGTH];
    int cmd;

    if(info->token) return;
    if(!info->mob && !info->obj && !info->room) return;

    if (!str_prefix("token ", argument))
        argument = skip_whitespace(argument+5);

    argument = one_argument(argument, command);

    cmd = tpcmd_lookup(command,false);

    if(cmd < 0) {
        pbugf(LOG_SCRIPTS, "Tokenother_interpret: invalid cmd: '%s'", command);
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
    AREA_DATA *token_area = NULL;
    AREA_DATA *context_area = NULL;
    WNUM token_wnum = wnum_zero;


    if(!info) return;

    context_area = get_area_from_scriptinfo(info);

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpAdjust - Error in parsing from vnum %ld.", info->room ? info->room->vnum : 0);
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
            pbugf(LOG_SCRIPTS,"TpAdjust - NULL victim from vnum %ld.", info->room ? info->room->vnum : 0);
            return;
        }

        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS,"TpAdjust - Error in parsing from vnum %ld.", info->room ? info->room->vnum : 0);
            return;
        }

        switch(arg->type) {
        case ENT_STRING:
            count = number_argument(arg->d.str, arg2);
            if (parse_widevnum(arg2, script_relative_widevnum_context(context_area, arg2), &token_wnum) && token_wnum.pArea) {
                vnum = token_wnum.vnum;
                token_area = token_wnum.pArea;
            } else {
                vnum = 0;
            }
            break;
        case ENT_WIDEVNUM:
            token_wnum = arg->d.wnum;
            count = 1;
            if (token_wnum.pArea && token_wnum.vnum > 0) {
                vnum = token_wnum.vnum;
                token_area = token_wnum.pArea;
            } else {
                vnum = 0;
                token_area = NULL;
            }
            break;
        case ENT_NUMBER:
            vnum = arg->d.num;
            count = 1;
            if (resolve_widevnum(vnum, NULL, &token_wnum) && token_wnum.pArea)
                token_area = token_wnum.pArea;
            break;
        default: break;
        }

        if (vnum < 1 || !token_area || !get_token_index(token_area, vnum)) {
            pbugf(LOG_SCRIPTS,"TpAdjust - invalid token vnum from vnum %ld.", info->room ? info->room->vnum : 0);
            return;
        }

        if(victim)
            token = get_token_char(victim, vnum, token_area, count);
        else if(object)
            token = get_token_obj(object, vnum, token_area, count);
        else if(room)
            token = get_token_room(room, vnum, token_area, count);

        if (!token) return;
    }

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpAdjust - Error in parsing from vnum %ld.", info->room ? info->room->vnum : 0);
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
        pbugf(LOG_SCRIPTS, "TpAdjust: bad v# from vnum %ld.", info->room ? info->room->vnum : 0);
        return;
    }


    argument = one_argument(rest,buf);

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpAdjust - Error in parsing from vnum %ld.", info->room ? info->room->vnum : 0);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        if(is_number(arg->d.str))
            value = atoi(arg->d.str);
        else {
            pbugf(LOG_SCRIPTS,"TpAdjust - Invalid value from vnum %ld.", info->room ? info->room->vnum : 0);
            return;
        }
        break;
    case ENT_NUMBER: value = arg->d.num; break;
    default:
        pbugf(LOG_SCRIPTS,"TpAdjust - Invalid value from vnum %ld.", info->room ? info->room->vnum : 0);
        return;
    }

    if(ptr) {
        switch (buf[0]) {
        case '+': *ptr += value; break;
        case '-': *ptr -= value; break;
        case '*': *ptr *= value; break;
        case '/':
            if (!value) {
                pbugf(LOG_SCRIPTS,"TpAdjust - adjust called with operator / and value 0 from vnum %ld.", info->room ? info->room->vnum : 0);
                return;
            }
            *ptr /= value;
            break;
        case '%':
            if (!value) {
                pbugf(LOG_SCRIPTS,"TpAdjust - adjust called with operator %% and value 0 from vnum %ld.", info->room ? info->room->vnum : 0);
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
                pbugf(LOG_SCRIPTS,"TpAdjust - adjust called with operator / and value 0 from vnum %ld.", info->room ? info->room->vnum : 0);
                return;
            }
            token->value[num] /= value;
            break;
        case '%':
            if (!value) {
                pbugf(LOG_SCRIPTS,"TpAdjust - adjust called with operator %% and value 0 from vnum %ld.", info->room ? info->room->vnum : 0);
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


SCRIPT_CMD(do_tpgive)
{
    char *rest;
    int vnum = 0;
    CHAR_DATA *victim = NULL;
    OBJ_DATA *object = NULL;
    ROOM_INDEX_DATA *room = NULL;
    TOKEN_INDEX_DATA *token_index;
    TOKEN_DATA *token;
    AREA_DATA *token_area = NULL;
    AREA_DATA *context_area = NULL;
    WNUM token_wnum = wnum_zero;


    if(!info) return;

    context_area = get_area_from_scriptinfo(info);

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpGive - Error in parsing from vnum %ld.", info->room ? info->room->vnum : 0);
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
        pbugf(LOG_SCRIPTS,"TpGive - NULL target from vnum %ld.", info->room ? info->room->vnum : 0);
        return;
    }

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpGive - Error in parsing from vnum %ld.", info->room ? info->room->vnum : 0);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        if (parse_widevnum(arg->d.str, script_relative_widevnum_context(context_area, arg->d.str), &token_wnum) && token_wnum.pArea) {
            vnum = token_wnum.vnum;
            token_area = token_wnum.pArea;
        }
        break;
    case ENT_WIDEVNUM:
        token_wnum = arg->d.wnum;
        if (token_wnum.pArea && token_wnum.vnum > 0) {
            vnum = token_wnum.vnum;
            token_area = token_wnum.pArea;
        }
        break;
    case ENT_NUMBER:
        vnum = arg->d.num;
        if (resolve_widevnum(vnum, NULL, &token_wnum) && token_wnum.pArea)
            token_area = token_wnum.pArea;
        break;
    default: break;
    }

    if (vnum < 1 || !token_area || !(token_index = get_token_index(token_area, vnum))) {
        pbugf(LOG_SCRIPTS,"TpGive - invalid token vnum from vnum %ld.", info->room ? info->room->vnum : 0);
        return;
    }

    if (is_singular_token(token_index)) {
        if (victim && get_token_char(victim, vnum, token_area, 1)) {
            pbugf(LOG_SCRIPTS, "TpGive - trying to give a second copy of token %s (%ld) to char %s",
                token_index->name, token_index->vnum, HANDLE(victim));
            return;
        } else if (object && get_token_obj(object, vnum, token_area, 1)) {
            pbugf(LOG_SCRIPTS, "TpGive - trying to give a second copy of token %s (%ld) to object %s",
                token_index->name, token_index->vnum, object->short_descr);
            return;
        } else if (room && get_token_room(room, vnum, token_area, 1)) {
            pbugf(LOG_SCRIPTS, "TpGive - trying to give a second copy of token %s (%ld) to room %s",
                token_index->name, token_index->vnum, room->name);
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
    AREA_DATA *token_area = NULL;
    AREA_DATA *context_area = NULL;
    WNUM token_wnum = wnum_zero;


    if(!info) return;

    context_area = get_area_from_scriptinfo(info);

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpJunk - Error in parsing from vnum %ld.", info->room ? info->room->vnum : 0);
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
            pbugf(LOG_SCRIPTS,"TpJunk - NULL target from vnum %ld.", info->room ? info->room->vnum : 0);
            return;
        }

        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS,"TpJunk - Error in parsing from vnum %ld.", info->room ? info->room->vnum : 0);
            return;
        }

        switch(arg->type) {
        case ENT_STRING:
            count = number_argument(arg->d.str, arg2);
            if (parse_widevnum(arg2, script_relative_widevnum_context(context_area, arg2), &token_wnum) && token_wnum.pArea) {
                vnum = token_wnum.vnum;
                token_area = token_wnum.pArea;
            } else {
                vnum = 0;
            }
            break;

        case ENT_WIDEVNUM:
            token_wnum = arg->d.wnum;
            count = 1;
            if (token_wnum.pArea && token_wnum.vnum > 0) {
                vnum = token_wnum.vnum;
                token_area = token_wnum.pArea;
            } else {
                vnum = 0;
                token_area = NULL;
            }
            break;

        case ENT_NUMBER:
            vnum = arg->d.num;
            count = 1;
            if (resolve_widevnum(vnum, NULL, &token_wnum) && token_wnum.pArea)
                token_area = token_wnum.pArea;
            break;
        default: break;
        }

        if(victim)
            token = get_token_char(victim, vnum, token_area, count);
        else if(object)
            token = get_token_obj(object, vnum, token_area, count);
        else if(room)
            token = get_token_room(room, vnum, token_area, count);

        if (!token) return;
    }

    if( token && IS_SET(token->flags, TOKEN_PERMANENT) && script_security < SYSTEM_SCRIPT_SECURITY) {
        pbugf(LOG_SCRIPTS,"TpJunk - Attempting to junk a permanent token with insufficient security from vnum %ld.", info->room ? info->room->vnum : 0);
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


/*
SCRIPT_CMD(do_tpalterobj)
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

    if(!info || !info->token) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpAlterObj - Error in parsing from vnum %ld.", info->room ? info->room->vnum : 0);
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
        pbugf(LOG_SCRIPTS,"TpAlterObj - NULL object from vnum %ld.", info->room ? info->room->vnum : 0);
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS,"TpAlterObj - Missing field type from vnum %ld.", info->room ? info->room->vnum : 0);
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAlterObj - Error in parsing from vnum %ld.", info->room ? info->room->vnum : 0);
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
        pbugf(LOG_SCRIPTS,"TpAlterObj - Error in parsing from vnum %ld.", info->room ? info->room->vnum : 0);
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
            pbugf(LOG_SCRIPTS,"TpAlterObj - Attempting to alter value%d with security %d from vnum %ld.", num, script_security, info->room ? info->room->vnum : 0);
            return;
        }

        switch (buf[0]) {
        case '+': obj->value[num] += value; break;
        case '-': obj->value[num] -= value; break;
        case '*': obj->value[num] *= value; break;
        case '/':
            if (!value) {
                pbugf(LOG_SCRIPTS,"TpAlterObj - adjust called with operator / and value 0 from vnum %ld.", info->room ? info->room->vnum : 0);
                return;
            }
            obj->value[num] /= value;
            break;
        case '%':
            if (!value) {
                pbugf(LOG_SCRIPTS,"TpAlterObj - adjust called with operator % and value 0 from vnum %ld.", info->room ? info->room->vnum : 0);
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
            pbugf(LOG_SCRIPTS,"TpAlterObj - Attempting to alter '%s' with security %d from vnum %ld.", field, script_security, info->room ? info->room->vnum : 0);
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
                pbugf(LOG_SCRIPTS,"TpAlterObj - alterobj called with arithmetic operator on a bitonly field from vnum %ld.", info->room ? info->room->vnum : 0);
                return;
            }

            *ptr += value;
            break;

        case '-':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS,"TpAlterObj - alterobj called with arithmetic operator on a bitonly field from vnum %ld.", info->room ? info->room->vnum : 0);
                return;
            }

            *ptr -= value;
            break;

        case '*':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS,"TpAlterObj - alterobj called with arithmetic operator on a bitonly field from vnum %ld.", info->room ? info->room->vnum : 0);
                return;
            }

            *ptr *= value;
            break;

        case '/':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS,"TpAlterObj - alterobj called with arithmetic operator on a bitonly field from vnum %ld.", info->room ? info->room->vnum : 0);
                return;
            }

            if (!value) {
                pbugf(LOG_SCRIPTS,"TpAlterObj - adjust called with operator / and value 0 from vnum %ld.", info->room ? info->room->vnum : 0);
                return;
            }
            *ptr /= value;
            break;
        case '%':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS,"TpAlterObj - alterobj called with arithmetic operator on a bitonly field from vnum %ld.", info->room ? info->room->vnum : 0);
                return;
            }

            if (!value) {
                pbugf(LOG_SCRIPTS,"TpAlterObj - adjust called with operator % and value 0 from vnum %ld.", info->room ? info->room->vnum : 0);
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






// do_tpdamage
SCRIPT_CMD(do_tpdamage)
{
    char buf[MSL],*rest;
    CHAR_DATA *victim = NULL, *victim_next;
    int low, high, level, value, dc;
    bool fAll = false, fKill = false, fLevel = false, fRemort = false, fTwo = false;


    if(!info || !info->token || !token_room(info->token)) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpDamage - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        if(!str_cmp(arg->d.str,"all")) fAll = true;
        else victim = get_char_room(NULL, token_room(info->token), arg->d.str);
        break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: victim = NULL; break;
    }

    if (!victim && !fAll) {
        pbugf(LOG_SCRIPTS,"TpDamage - Null victim from vnum %ld.", VNUM(info->token));
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS,"TpDamage - missing argument from vnum %ld.", VNUM(info->token));
        return;
    }

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpDamage - Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpDamage - invalid argument from vnum %ld.", VNUM(info->token));
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS,"TpDamage - missing argument from vnum %ld.", VNUM(info->token));
        return;
    }

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpDamage - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    if(fLevel && !victim) {
        pbugf(LOG_SCRIPTS,"TpDamage - Level aspect used with null victim from vnum %ld.", VNUM(info->token));
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
            pbugf(LOG_SCRIPTS,"TpDamage - invalid argument from vnum %ld.", VNUM(info->token));
            return;
        }
        break;
    case ENT_MOBILE:
        if(fLevel) {
            if(arg->d.mob) level = arg->d.mob->tot_level;
            else {
                pbugf(LOG_SCRIPTS,"TpDamage - Null reference mob from vnum %ld.", VNUM(info->token));
                return;
            }
            break;
        } else {
            pbugf(LOG_SCRIPTS,"TpDamage - invalid argument from vnum %ld.", VNUM(info->token));
            return;
        }
        break;
    default:
        pbugf(LOG_SCRIPTS,"TpDamage - invalid argument from vnum %ld.", VNUM(info->token));
        return;
    }

    argument = one_argument(rest, buf);
    if (!str_cmp(buf,"kill") || !str_cmp(buf,"lethal")) fKill = true;

    one_argument(argument, buf);
    dc = damage_class_lookup(buf);

    if(fLevel) get_level_damage(level,&low,&high,fRemort,fTwo);

    if (fAll) {
        for(victim = token_room(info->token)->people; victim; victim = victim_next) {
            victim_next = victim->next_in_room;
            value = fLevel ? dice(low,high) : number_range(low,high);
            damage(victim, victim, fKill ? value : UMIN(victim->hit,value), TYPE_UNDEFINED, dc, false);
        }
    } else {
        value = fLevel ? dice(low,high) : number_range(low,high);
        damage(victim, victim, fKill ? value : UMIN(victim->hit,value), TYPE_UNDEFINED, dc, false);
    }
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
        pbugf(LOG_SCRIPTS,"TpOtransfer - Bad syntax from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: obj = get_obj_here(NULL,token_room(info->token), arg->d.str); break;
    case ENT_OBJECT: obj = arg->d.obj; break;
    default: obj = NULL; break;
    }


    if (!obj) {
        pbugf(LOG_SCRIPTS,"TpOtransfer - Null object from vnum %ld.", VNUM(info->token));
        return;
    }

    if (PROG_FLAG(obj,PROG_AT)) return;

    if (IS_SET(obj->extra[2], ITEM_NO_TRANSFER) && script_security < MAX_SCRIPT_SECURITY) return;

    argument = tp_getolocation(info, rest, &dest, &container, &carrier, &wear_loc);

    if(!dest && !container && !carrier) {
        pbugf(LOG_SCRIPTS,"TpOTransfer - Bad location from vnum %d.", VNUM(info->token));
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

SCRIPT_CMD(do_tpremove)
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

    if(!info || !info->token) return;

    context_area = get_area_from_scriptinfo(info);

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpRemove: Bad syntax from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: victim = get_char_room(NULL, token_room(info->token), arg->d.str); break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: victim = NULL; break;
    }

    if (!victim) {
        pbugf(LOG_SCRIPTS,"TpRemove: Null victim from vnum %ld.", VNUM(info->token));
        return;
    }

    if(!*rest) return;

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpRemove: Bad syntax from vnum %ld.", VNUM(info->token));
        return;
    }

    name[0] = '\0';
    switch(arg->type) {
    case ENT_WIDEVNUM:
        if (arg->d.wnum.pArea && arg->d.wnum.vnum > 0) {
            vnum = arg->d.wnum.vnum;
            item_area = arg->d.wnum.pArea;
        }
        break;
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
        pbugf(LOG_SCRIPTS,"TpRemove: Invalid object from vnum %ld.", VNUM(info->token));
        return;
    }

    if(!fAll && !obj && *rest) {
        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS,"TpRemove: Bad syntax from vnum %ld.", VNUM(info->token));
            return;
        }

        switch(arg->type) {
        case ENT_NUMBER: count = arg->d.num; break;
        case ENT_STRING: count = atoi(arg->d.str); break;
        default: count = 0; break;
        }

        if(count < 0) {
            pbugf(LOG_SCRIPTS,"TpRemove: Invalid count from vnum %d.", VNUM(info->token));
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

SCRIPT_CMD(do_tplink)
{
    char *rest;
    ROOM_INDEX_DATA *room, *dest = NULL;
    int door, vnum;
    AREA_DATA *link_area = NULL;
    AREA_DATA *context_area = NULL;
    WNUM link_wnum = wnum_zero;
    unsigned long id1, id2;

    bool del = false;
    bool environ = false;

    if(!info || !info->token) return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    context_area = get_area_from_scriptinfo(info);

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
        pbugf(LOG_SCRIPTS,"TPlink used without an argument from room vnum %d.", room->vnum);
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
    case ENT_WIDEVNUM:
        if (arg->d.wnum.pArea && arg->d.wnum.vnum > 0) {
            vnum = arg->d.wnum.vnum;
            link_area = arg->d.wnum.pArea;
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
        // Only allow STATIC links
        vnum = (arg->d.door.r && arg->d.door.r->exit[arg->d.door.door] && arg->d.door.r->exit[arg->d.door.door]->u1.to_room) ? arg->d.door.r->exit[arg->d.door.door]->u1.to_room->vnum : -1;
        link_area = (arg->d.door.r && arg->d.door.r->exit[arg->d.door.door] && arg->d.door.r->exit[arg->d.door.door]->u1.to_room) ? arg->d.door.r->exit[arg->d.door.door]->u1.to_room->area : NULL;
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
        pbugf(LOG_SCRIPTS,"TPlink - invalid argument in room %d.", room->vnum);
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
        pbugf(LOG_SCRIPTS,"TPlink - invalid destination in room %d.", room->vnum);
        return;
    }

    script_change_exit(room, dest, door);
}

SCRIPT_CMD(do_tpoload)
{
    script_oload(info,argument,arg, false);
}

SCRIPT_CMD(do_tpforce)
{
    char *rest;
    CHAR_DATA *victim = NULL, *next;
    bool fAll = false, forced;


    if(!info || !info->token) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpForce - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        if(!str_cmp(arg->d.str,"all")) fAll = true;
        else victim = get_char_room(NULL,token_room(info->token), arg->d.str);
        break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: break;
    }

    if (!fAll && !victim) {
        pbugf(LOG_SCRIPTS,"TpForce - Null victim from vnum %ld.", VNUM(info->token));
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

SCRIPT_CMD(do_tpgoto)
{
    ROOM_INDEX_DATA *dest;

    if(!info || !info->token || !info->token->player || !info->token->player->in_room) return;

    if(!argument[0]) {
        pbugf(LOG_SCRIPTS,"Tpgoto - No argument from vnum %d.", VNUM(info->token));
        return;
    }

    tp_getlocation(info, argument, &dest);

    if(!dest) {
        pbugf(LOG_SCRIPTS,"Tpgoto - Bad location from vnum %d.", VNUM(info->token));
        return;
    }

    if (info->token->player->fighting) stop_fighting(info->token->player, true);

    char_from_room(info->token->player);
    char_to_room(info->token->player, dest);
}


SCRIPT_CMD(do_tpaltermob)
{
    char buf[MSL],field[MIL],*rest;
    long value = 0; 
    int min_sec = MIN_SCRIPT_SECURITY, min = 0, max = 0;
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
    int dirty_stat = -1;
    const struct flag_type *flags = NULL;
    const struct flag_type **bank = NULL;
    long temp_flags[4];

    if(!info || !info->token) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpAlterMob - Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpAlterMob - NULL mobile from vnum %ld.", VNUM(info->token));
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS,"TpAlterMob - Missing field type from vnum %ld.", VNUM(info->token));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAlterMob - Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpAlterMob - Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"AlterMob - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    // MINIMUM to alter ANYTHING not allowed on players on a player
    if(!allowpc && !IS_NPC(mob)) min_sec = 9;

    if(script_security < min_sec) {
        pbugf(LOG_SCRIPTS,"TpAlterMob - Attempting to alter '%s' with security %d.\n\r", field, script_security);
        return;
    }

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
            *lptr += value;
            if (!allowarith) {
                pbugf(LOG_SCRIPTS,"TpAlterMob - altermob called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->token));
                return;
            }
            break;

        case OPR_SUB:
            *lptr -= value;
            if (!allowarith) {
                pbugf(LOG_SCRIPTS,"TpAlterMob - altermob called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->token));
                return;
            }
            break;

        case OPR_MULT:
            *lptr *= value;
            if (!allowarith) {
                pbugf(LOG_SCRIPTS,"TpAlterMob - altermob called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->token));
                return;
            }
            break;

        case OPR_DIV:
            if (!value) {
                pbugf(LOG_SCRIPTS,"AlterMob - altermob called with operator / and value 0 from vnum %ld.", VNUM(info->token));
                return;
            }
            if (!allowarith) {
                pbugf(LOG_SCRIPTS,"TpAlterMob - altermob called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->token));
                return;
            }
            *lptr /= value;
            break;

        case OPR_MOD:
            if (!value) {
                pbugf(LOG_SCRIPTS,"AlterMob - altermob called with operator % and value 0 from vnum %ld.", VNUM(info->token));
                return;
            }
            if (!allowarith) {
                pbugf(LOG_SCRIPTS,"TpAlterMob - altermob called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->token));
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
                pbugf(LOG_SCRIPTS,"TpAlterMob - altermob called with bitwise operator on a non-bitvector field from vnum %ld.", VNUM(info->token));
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
            if (!allowbitwise) {
                pbugf(LOG_SCRIPTS,"TpAlterMob - altermob called with bitwise operator on a non-bitvector field from vnum %ld.", VNUM(info->token));
                return;
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
            if (!allowbitwise) {
                pbugf(LOG_SCRIPTS,"TpAlterMob - altermob called with bitwise operator on a non-bitvector field from vnum %ld.", VNUM(info->token));
                return;
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
            if (!allowbitwise) {
                pbugf(LOG_SCRIPTS,"TpAlterMob - altermob called with bitwise operator on a non-bitvector field from vnum %ld.", VNUM(info->token));
                return;
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
                pbugf(LOG_SCRIPTS,"AlterMob - altermob called with operator / and value 0 from vnum %ld.", VNUM(info->token));
                return;
            }
            *ptr /= value;
            break;

        case OPR_MOD:
            if (!value) {
                pbugf(LOG_SCRIPTS,"AlterMob - altermob called with operator % and value 0 from vnum %ld.", VNUM(info->token));
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



SCRIPT_CMD(do_tprawkill)
{
    char *rest;
    int type;
    bool has_head, show_msg;
    CHAR_DATA *mob = NULL;


    if(!info || !info->token) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpRawkill - Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpRawkill - NULL mobile from vnum %ld.", VNUM(info->token));
        return;
    }

    if(IS_IMMORTAL(mob)) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpRawkill - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: type = flag_lookup(arg->d.str,corpse_types); break;
    default: return;
    }

    if(type < 0 || type == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpRawkill - Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpRawkill - Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpAddAffect - Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpAddaffect - NULL target from vnum %ld.", VNUM(info->token));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: where = flag_lookup(arg->d.str,apply_types); break;
    default: return;
    }

    if(where == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: skill = skill_lookup(arg->d.str); break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: loc = flag_lookup(arg->d.str,apply_flags_full); break;
    default: return;
    }

    if(loc == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: mod = arg->d.num; break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: hours = arg->d.num; break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: bv = flag_value(affect_flags,arg->d.str); break;
    default: return;
    }

    if(bv == NO_FLAG) bv = 0;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: bv2 = flag_value(affect2_flags,arg->d.str); break;
    default: return;
    }

    if(bv2 == NO_FLAG) bv2 = 0;

    if(rest && *rest) {
        if(!(rest = expand_argument(info,rest,arg))) {
            pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
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
    af.skill = skill_find_uid(af.type);
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
        pbugf(LOG_SCRIPTS,"TpAddAffect - Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpAddaffect - NULL target from vnum %ld.", VNUM(info->token));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    // <where>
    switch(arg->type) {
    case ENT_STRING: where = flag_lookup(arg->d.str,apply_types); break;
    default: return;
    }

    if(where == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }


    // <name>
    switch(arg->type) {
    case ENT_STRING: name = create_affect_cname(arg->d.str); break;
    default: return;
    }

    if(!name) {
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error allocating affect name from vnum %ld.", VNUM(info->token));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    // <location>
    switch(arg->type) {
    case ENT_STRING: loc = flag_lookup(arg->d.str,apply_flags_full); break;
    default: return;
    }

    if(loc == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: mod = arg->d.num; break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: hours = arg->d.num; break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: bv = flag_value(affect_flags,arg->d.str); break;
    default: return;
    }

    if(bv == NO_FLAG) bv = 0;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: bv2 = flag_value(affect2_flags,arg->d.str); break;
    default: return;
    }

    if(bv2 == NO_FLAG) bv2 = 0;

    if(rest && *rest) {
        if(!(rest = expand_argument(info,rest,arg))) {
            pbugf(LOG_SCRIPTS,"TpAddaffect - Error in parsing from vnum %ld.", VNUM(info->token));
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
    af.skill     = NULL;
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





SCRIPT_CMD(do_tpusecatalyst)
{
    char *rest;
    int type, method, amount, min, max, show;
    CHAR_DATA *mob = NULL;
    ROOM_INDEX_DATA *room = NULL;


    if(!info || !info->token) return;

    info->token->progs->lastreturn = 0;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpUseCatalyst - NULL target from vnum %ld.", VNUM(info->token));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: type = flag_value(catalyst_types,arg->d.str); break;
    default: return;
    }

    if(type == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: method = flag_value(catalyst_method_types,arg->d.str); break;
    default: return;
    }

    if(method == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: amount = arg->d.num; break;
    case ENT_STRING: amount = atoi(arg->d.str); break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: min = arg->d.num; break;
    case ENT_STRING: min = atoi(arg->d.str); break;
    default: return;
    }

    if(min < 1 || min > CATALYST_MAXSTRENGTH) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: max = arg->d.num; break;
    case ENT_STRING: max = atoi(arg->d.str); break;
    default: return;
    }

    if(max < min || max > CATALYST_MAXSTRENGTH) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->token));
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
    int16_t *sptr = NULL;
    char **str;
    int min, max;
    bool hasmin = false, hasmax = false;
    bool allowarith = true;
    const struct flag_type *flags = NULL;

    if(!info || !info->token) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpAlterExit - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    room = token_room(info->token);

    switch(arg->type) {
    case ENT_ROOM:
        room = arg->d.room;
        if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING) {
            pbugf(LOG_SCRIPTS,"TpAlterExit - Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpAlterExit - Missing field type from vnum %ld.", VNUM(info->token));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpAlterExit - Error in parsing from vnum %ld.", VNUM(info->token));
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
            pbugf(LOG_SCRIPTS,"TpAlterExit - Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpAlterExit - Error in parsing from vnum %ld.", VNUM(info->token));
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
        sprintf(buf,"TpAlterExit - Attempting to alter '%s' with security %d.\n\r", field, script_security);
        log_event_t ev = {
            .severity = EVENT_SEV_WARN,
            .category = LOG_SCRIPTS,
            .plain_message = buf,
            .staff_message = buf,
            .wiznet_flag = WIZ_SCRIPTS,
            .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__,
        };
        log_emit_event(&ev, NULL);
        pbugf(LOG_SCRIPTS,"TpAlterExit - Attempting to alter '%s' with security %d from vnum %ld.", field, script_security, VNUM(info->token));
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
                pbugf(LOG_SCRIPTS,"TpAlterExit - alterexit called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->token));
                return;
            }
            *ptr += value;
            break;
        case '-':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS,"TpAlterExit - alterexit called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->token));
                return;
            }
            *ptr -= value;
            break;
        case '*':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS,"TpAlterExit - alterexit called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->token));
                return;
            }
            *ptr *= value;
            break;
        case '/':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS,"TpAlterExit - alterexit called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->token));
                return;
            }
            if (!value) {
                pbugf(LOG_SCRIPTS,"TpAlterExit - adjust called with operator / and value 0 from vnum %ld.", VNUM(info->token));
                return;
            }
            *ptr /= value;
            break;
        case '%':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS,"TpAlterExit - alterexit called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->token));
                return;
            }
            if (!value) {
                pbugf(LOG_SCRIPTS,"TpAlterExit - adjust called with operator % and value 0 from vnum %ld.", VNUM(info->token));
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
                pbugf(LOG_SCRIPTS,"TpAlterExit - adjust called with operator / and value 0 from vnum %ld.", VNUM(info->token));
                return;
            }
            *sptr /= value;
            break;
        case '%':
            if (!value) {
                pbugf(LOG_SCRIPTS,"TpAlterExit - adjust called with operator % and value 0 from vnum %ld.", VNUM(info->token));
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

// SYNTAX: token prompt <player> <name>[ <string>]


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
    WNUM source_wnum = wnum_zero;
    CHAR_DATA *mob;
    OBJ_DATA *obj;
    ROOM_INDEX_DATA *source, *room, *clone;
    TOKEN_DATA *tok;
    AREA_DATA *context_area;
    bool no_env = false;

    if(!info || !info->token) return;

    info->progs->lastreturn = 0;
    context_area = get_area_from_scriptinfo(info);

    // Get vnum
    if(!(argument = expand_argument(info,argument,arg)))
        return;

    switch (arg->type) {
    case ENT_NUMBER:
        if (!resolve_widevnum(arg->d.num, NULL, &source_wnum))
            return;
        break;
    case ENT_WIDEVNUM:
        source_wnum = arg->d.wnum;
        break;
    case ENT_STRING:
        if (!parse_widevnum(arg->d.str, script_relative_widevnum_context(context_area, arg->d.str), &source_wnum))
            return;
        break;
    default:
        return;
    }

    if (!source_wnum.pArea || source_wnum.vnum < 1)
        return;

    if (!(source = get_room_index(source_wnum.pArea, source_wnum.vnum)))
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

    strlcpy(name, arg->d.str, sizeof(name));

    clone = create_virtual_room(source,false,false);
    if(!clone) return;

    if(!no_env)
        room_to_environment(clone,mob,obj,room,tok);

    variables_set_room(info->var,name,clone);

    info->progs->lastreturn = 1;
}

// destroyroom <vnum> <id1> <id2>
// destroyroom <room>
SCRIPT_CMD(do_tpdestroyroom)
{
    unsigned long id1, id2;
    ROOM_INDEX_DATA *room;
    WNUM room_wnum = wnum_zero;
    AREA_DATA *context_area;


    if(!info || !info->token) return;

    info->token->progs->lastreturn = 0;
    context_area = get_area_from_scriptinfo(info);

    if(!(argument = expand_argument(info,argument,arg)))
        return;

    // It's a room, extract it directly
    if(arg->type == ENT_ROOM) {
        // Need to block this when done by room to itself
        if(extract_clone_room(arg->d.room->source,arg->d.room->id[0],arg->d.room->id[1],false))
            info->token->progs->lastreturn = 1;

        return;
    }

    switch (arg->type) {
    case ENT_NUMBER:
        if (!resolve_widevnum(arg->d.num, NULL, &room_wnum))
            return;
        break;
    case ENT_WIDEVNUM:
        room_wnum = arg->d.wnum;
        break;
    case ENT_STRING:
        if (!parse_widevnum(arg->d.str, script_relative_widevnum_context(context_area, arg->d.str), &room_wnum))
            return;
        break;
    default:
        return;
    }

    if (!room_wnum.pArea || room_wnum.vnum < 1)
        return;

    if (!(room = get_room_index(room_wnum.pArea, room_wnum.vnum)))
        return;

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
    int depth, space = PRG_MPROG;
    long vnum;

    int ret;

    if(!info || !info->token) return;

    if (!argument[0]) {
        pbugf(LOG_SCRIPTS,"TpCall: missing arguments from vnum %d.", VNUM(info->token));
        return;
    }

    if(script_security < 5) {
        pbugf(LOG_SCRIPTS,"TpCall: Minimum security needed is 5 from vnum %d.", VNUM(info->token));
        return;
    }

    // Call depth checking
    depth = script_call_depth;
    if(script_call_depth == 1) {
        pbugf(LOG_SCRIPTS,  "TpCall: maximum call depth exceeded for mob vnum %d.", VNUM(info->token));
        return;
    } else if(script_call_depth > 1)
        --script_call_depth;


    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpCall: No entity target from vnum %ld.", VNUM(info->token));
        // Restore the call depth to the previous value
        script_call_depth = depth;
        return;
    }

    if(mob && !IS_NPC(mob)) {
        pbugf(LOG_SCRIPTS,"TpCall: Invalid target for xcall.  Players cannot do scripts from vnum %ld.", VNUM(info->token));
        // Restore the call depth to the previous value
        script_call_depth = depth;
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS,"TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
        // Restore the call depth to the previous value
        script_call_depth = depth;
        return;
    }

    script = get_script_from_arg(info, arg, space, &vnum);
    if (vnum < 1 || !script) {
        pbugf(LOG_SCRIPTS,"TpCall: invalid prog from vnum %d.", VNUM(info->token));
        return;
    }

    ch = vch = NULL;
    obj1 = obj2 = NULL;

    if(*rest) {	// Enactor
        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS,"TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
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
            pbugf(LOG_SCRIPTS,"TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
            // Restore the call depth to the previous value
            script_call_depth = depth;
            return;
        }

        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS,"TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
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
            pbugf(LOG_SCRIPTS,"TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
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
            pbugf(LOG_SCRIPTS,"TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
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
    ret = execute_script(script->vnum, script, mob, obj, room, token, NULL, NULL, NULL, ch, obj1, obj2, vch, NULL,NULL, NULL,info->phrase,info->trigger,info->trigger_type,0,0,0,0,0);
    if(info->token)
        info->token->progs->lastreturn = ret;
    else
        info->block->ret_val = ret;

    // restore the call depth to the previous value
    script_call_depth = depth;
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
        pbugf(LOG_SCRIPTS,"TpClearRecall - Bad syntax from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpClearRecall - Null victim from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpHunt - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: prey = get_char_world(NULL, arg->d.str); break;
    case ENT_MOBILE: prey = arg->d.mob; break;
    default: prey = NULL; break;
    }

    if (!prey) {
        pbugf(LOG_SCRIPTS,"TpHunt - Null hunter/prey from vnum %ld.", VNUM(info->token));
        return;
    }

    if(*rest) {
        if(!expand_argument(info,rest,arg)) {
            pbugf(LOG_SCRIPTS,"TpHunt - Error in parsing from vnum %ld.", VNUM(info->token));
            return;
        }

        hunter = prey;

        switch(arg->type) {
        case ENT_STRING: prey = get_char_world(info->mob, arg->d.str); break;
        case ENT_MOBILE: prey = arg->d.mob; break;
        default: prey = NULL; break;
        }

        if (!prey) {
            pbugf(LOG_SCRIPTS,"TpHunt - Null prey from vnum %ld.", VNUM(info->token));
            return;
        }
    } else if(!info->token->player) {
        pbugf(LOG_SCRIPTS,"TpHunt - Null hunter from vnum %ld.", VNUM(info->token));
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
        pbugf(LOG_SCRIPTS,"TpStopHunt - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    stay = !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str,"yes") || !str_cmp(arg->d.str,"stay");

    if(!expand_argument(info,rest,arg)) {
        pbugf(LOG_SCRIPTS,"TpStopHunt - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

    switch(arg->type) {
    case ENT_NONE: hunter = info->token->player; break;
    case ENT_STRING: hunter = get_char_world(NULL, arg->d.str); break;
    case ENT_MOBILE: hunter = arg->d.mob; break;
    default: hunter = NULL; break;
    }

    if (!hunter) {
        pbugf(LOG_SCRIPTS,"TpStopHunt - Null hunter from vnum %ld.", VNUM(info->token));
        return;
    }

    stop_hunt(hunter, stay);
    return;
}

// token skill <player> <name> <op> <number>
// <op> =, +, -
SCRIPT_CMD(do_tpskill)
{
    char buf[MIL];

    char *rest;
    CHAR_DATA *mob = NULL;
    SKILL_ENTRY *entry = NULL;
    int sn, value;

    if(!info || !info->token || IS_NULLSTR(argument)) return;

    if ( script_security < 9 ) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpSkill - Error in parsing from vnum %ld.", VNUM(info->token));
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

            entry = skill_entry_findsn(mob->sorted_skills, sn);
            {
                SKILL_DATA *mod_sk = skill_find_uid(sn);
                if( value == 0 ) {
                    if( !mod_sk || mod_sk->spell_fun == spell_null )
                        skill_entry_removeskill(mob, sn, NULL);
                    else
                        skill_entry_removespell(mob, sn, NULL);
                } else {
                    if( !entry ) {
                        if( !mod_sk || mod_sk->spell_fun == spell_null )
                            skill_entry_addskill(mob, sn, NULL, SKILLSRC_SCRIPT, SKILL_AUTOMATIC);
                        else
                            skill_entry_addspell(mob, sn, NULL, SKILLSRC_SCRIPT, SKILL_AUTOMATIC);

                        entry = skill_entry_findsn(mob->sorted_skills, sn);
                    }

                    if( entry )
                        entry->rating = value;
                }
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


// token skillgroup <player> add|remove <group>
SCRIPT_CMD(do_tpskillgroup)
{
    char buf[MIL];

    char *rest;
    CHAR_DATA *mob = NULL;
    bool fAdd = false;

    if(!info || !info->token || IS_NULLSTR(argument)) return;

    if ( script_security < 9 ) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS,"TpSkill - Error in parsing from vnum %ld.", VNUM(info->token));
        return;
    }

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

    {
        SKILL_GROUP *sg = group_lookup(arg->d.str);
        if (sg) {
            if (fAdd) {
                if (!char_knows_group(mob, sg))
                    gn_add(mob, sg);
            } else {
                if (char_knows_group(mob, sg))
                    gn_remove(mob, sg);
            }
        }
    }

    return;
}

// token condition $PLAYER <condition> <value>
// Adjusts the specified condition by the given value
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
        mob->casting_recovered = true;

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
        bool recover = true;
        int chance;
        mob->casting_recovered = true;
        if(mob->cast_token) {
            if(!IS_SET(mob->cast_token->pIndexData->flags,TOKEN_NOSKILLTEST)) {
                if( mob->cast_successful == MAGICCAST_ROOMBLOCK) {
                    chance = 0;

                    if (IS_SET(mob->in_room->room_flag[1], ROOM_HARD_MAGIC) ||
                        room_sector_has_flag(mob->in_room, SECTOR_HARD_MAGIC)) chance += 2;
                    if (!IS_NPC(mob) && chance > 0 && number_range(1,chance) > 1)
                        recover = false;
                }

                if( recover ) {
                    if (mob->cast_token->pIndexData->value[TOKVAL_SPELL_RATING] > 0) {
                        if (number_range(0,mob->cast_token->pIndexData->value[TOKVAL_SPELL_RATING]) > mob->cast_token->value[TOKVAL_SPELL_RATING])
                            recover = false;
                    } else {
                        if (number_percent() > mob->cast_token->value[TOKVAL_SPELL_RATING])
                            recover = false;
                    }
                }
            }

        } else {
            // This is a skill spell
            if( mob->cast_successful == MAGICCAST_ROOMBLOCK) {
                chance = 0;

                if (IS_SET(mob->in_room->room_flag[1], ROOM_HARD_MAGIC) ||
                    room_sector_has_flag(mob->in_room, SECTOR_HARD_MAGIC)) chance += 2;
                if (!IS_NPC(mob) && chance > 0 && number_range(1,chance) > 1)
                    recover = false;
            }
            if (recover && number_percent() > get_skill(mob, mob->cast_sn))
                recover = false;
        }

        if(recover)
            mob->cast_successful = MAGICCAST_SUCCESS;
    }
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
    bool fShow = true;

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

