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
#include "wilderness_state.h"
#include "class_data.h"
#include "song_data.h"
#include "traits.h"

//#define DEBUG_MODULE
#include "debug.h"
#include "skill_data.h"
#include "event_types.h"

void reset_reckoning();

char *mp_getlocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room);
char *op_getlocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room);
char *rp_getlocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room);
char *tp_getlocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room);

static void quest_part_set_wnum(WNUM_LOAD *load, WNUM *wnum, AREA_DATA *area, long vnum)
{
    if (!load || !wnum) return;

    load->auid = area ? area->uid : 0;
    load->vnum = vnum;
    wnum->pArea = area;
    wnum->vnum = vnum;
}

#define PARSE_ARG				(rest = expand_argument(info,rest,arg))
#define PARSE_ARGTYPE(x)		if (!PARSE_ARG || arg->type != ENT_##x) return
#define PARSE_STR(b)			(expand_string(info,rest,(b)))
#define SETRETURN(ret)			info->progs->lastreturn = (ret)
#define IS_TRIGGER(trg)			(info->trigger_type == (trg))
#define ARG_EQUALS(ss)			(!str_cmp(arg->d.str, (ss)))
#define ARG_PREFIX(ss)			(!str_prefix(arg->d.str, (ss)))

static bool scriptcmd_event_get_source_from_info(SCRIPT_VARINFO *info, long *event_uid, uint32_t *instance_id, int *bracket)
{
    long uid = 0;
    uint32_t instance = 0;
    int source_bracket = 0;

    if (event_uid)
        *event_uid = 0;
    if (instance_id)
        *instance_id = 0;
    if (bracket)
        *bracket = 0;

    if (!info)
        return false;

    if (info->mob && event_get_mobile_spawn_source(info->mob, &uid, &instance) && uid > 0) {
        event_get_mobile_spawn_bracket(info->mob, &source_bracket);
        if (event_uid)
            *event_uid = uid;
        if (instance_id)
            *instance_id = instance;
        if (bracket)
            *bracket = source_bracket;
        return true;
    }

    if (info->mob && !IS_NPC(info->mob)
        && event_get_character_active_bracket(info->mob, &uid, &instance, &source_bracket)
        && uid > 0) {
        if (event_uid)
            *event_uid = uid;
        if (instance_id)
            *instance_id = instance;
        if (bracket)
            *bracket = source_bracket;
        return true;
    }

    if (info->obj && event_get_object_spawn_source(info->obj, &uid, &instance) && uid > 0) {
        event_get_object_spawn_bracket(info->obj, &source_bracket);
        if (event_uid)
            *event_uid = uid;
        if (instance_id)
            *instance_id = instance;
        if (bracket)
            *bracket = source_bracket;
        return true;
    }

    if (info->token) {
        if (info->token->player && event_get_mobile_spawn_source(info->token->player, &uid, &instance) && uid > 0) {
            event_get_mobile_spawn_bracket(info->token->player, &source_bracket);
            if (event_uid)
                *event_uid = uid;
            if (instance_id)
                *instance_id = instance;
            if (bracket)
                *bracket = source_bracket;
            return true;
        }

        if (info->token->player && !IS_NPC(info->token->player)
            && event_get_character_active_bracket(info->token->player, &uid, &instance, &source_bracket)
            && uid > 0) {
            if (event_uid)
                *event_uid = uid;
            if (instance_id)
                *instance_id = instance;
            if (bracket)
                *bracket = source_bracket;
            return true;
        }

        if (info->token->object && event_get_object_spawn_source(info->token->object, &uid, &instance) && uid > 0) {
            event_get_object_spawn_bracket(info->token->object, &source_bracket);
            if (event_uid)
                *event_uid = uid;
            if (instance_id)
                *instance_id = instance;
            if (bracket)
                *bracket = source_bracket;
            return true;
        }
    }

    return false;
}

static bool scriptcmd_event_get_source_from_param(SCRIPT_PARAM *param, long *event_uid, uint32_t *instance_id, int *bracket)
{
    long uid = 0;
    uint32_t instance = 0;
    int source_bracket = 0;

    if (!param)
        return false;

    if (event_uid)
        *event_uid = 0;
    if (instance_id)
        *instance_id = 0;
    if (bracket)
        *bracket = 0;

    if (param->type == ENT_MOBILE && param->d.mob &&
        event_get_mobile_spawn_source(param->d.mob, &uid, &instance) && uid > 0) {
        event_get_mobile_spawn_bracket(param->d.mob, &source_bracket);
        if (event_uid)
            *event_uid = uid;
        if (instance_id)
            *instance_id = instance;
        if (bracket)
            *bracket = source_bracket;
        return true;
    }

    if (param->type == ENT_MOBILE && param->d.mob && !IS_NPC(param->d.mob)
        && event_get_character_active_bracket(param->d.mob, &uid, &instance, &source_bracket)
        && uid > 0) {
        if (event_uid)
            *event_uid = uid;
        if (instance_id)
            *instance_id = instance;
        if (bracket)
            *bracket = source_bracket;
        return true;
    }

    if (param->type == ENT_OBJECT && param->d.obj &&
        event_get_object_spawn_source(param->d.obj, &uid, &instance) && uid > 0) {
        event_get_object_spawn_bracket(param->d.obj, &source_bracket);
        if (event_uid)
            *event_uid = uid;
        if (instance_id)
            *instance_id = instance;
        if (bracket)
            *bracket = source_bracket;
        return true;
    }

    return false;
}

static bool scriptcmd_parse_wilds_coord(SCRIPT_VARINFO *info, SCRIPT_PARAM *arg, WILDS_DATA **wilds, int *x, int *y, char **rest)
{
    char *next;

    if (!info || !arg || !wilds || !x || !y || !rest)
        return false;

    next = expand_argument(info, *rest, arg);
    if (!next)
        return false;

    switch (arg->type)
    {
    case ENT_WILDS_ROOM:
        *wilds = get_wilds_from_uid(NULL, arg->d.wroom.wuid);
        *x = arg->d.wroom.x;
        *y = arg->d.wroom.y;
        *rest = next;
        return *wilds != NULL;

    case ENT_ROOM:
        if (!arg->d.room || !arg->d.room->wilds)
            return false;
        *wilds = arg->d.room->wilds;
        *x = arg->d.room->x;
        *y = arg->d.room->y;
        *rest = next;
        return true;

    case ENT_NUMBER:
        *wilds = get_wilds_from_uid(NULL, arg->d.num);
        if (!*wilds)
            return false;

        next = expand_argument(info, next, arg);
        if (!next || arg->type != ENT_NUMBER)
            return false;
        *x = arg->d.num;

        next = expand_argument(info, next, arg);
        if (!next || arg->type != ENT_NUMBER)
            return false;
        *y = arg->d.num;

        *rest = next;
        return true;

    default:
        return false;
    }
}

static int scriptcmd_parse_vlink_linkage(const char *text)
{
    if (IS_NULLSTR(text))
        return VLINK_FROM_WILDS;

    if (!str_prefix(text, "to_wilds"))
        return VLINK_TO_WILDS;

    if (!str_prefix(text, "from_wilds"))
        return VLINK_FROM_WILDS;

    if (!str_prefix(text, "two_way") || !str_prefix(text, "twoway"))
        return VLINK_TO_WILDS | VLINK_FROM_WILDS;

    return -1;
}

static bool scriptcmd_parse_dest_wnum_from_arg(SCRIPT_VARINFO *info, SCRIPT_PARAM *arg, WNUM *dest_wnum, char **rest)
{
    char *next;
    AREA_DATA *context_area;

    if (!info || !arg || !dest_wnum || !rest)
        return false;

    next = expand_argument(info, *rest, arg);
    if (!next)
        return false;

    context_area = get_area_from_scriptinfo(info);
    dest_wnum->pArea = NULL;
    dest_wnum->vnum = 0;

    if (arg->type == ENT_WIDEVNUM)
    {
        *dest_wnum = arg->d.wnum;
        *rest = next;
        return dest_wnum->pArea != NULL && dest_wnum->vnum > 0;
    }

    if (arg->type == ENT_NUMBER)
    {
        char vnum_str[32];
        snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
        if (!parse_widevnum(vnum_str, context_area, dest_wnum))
            return false;
        *rest = next;
        return dest_wnum->pArea != NULL && dest_wnum->vnum > 0;
    }

    if (arg->type == ENT_STRING)
    {
        if (!parse_widevnum(arg->d.str, context_area, dest_wnum))
            return false;
        *rest = next;
        return dest_wnum->pArea != NULL && dest_wnum->vnum > 0;
    }

    return false;
}

static bool scriptcmd_event_parse_uid(SCRIPT_PARAM *arg, long *value)
{
    if (!arg || !value)
        return false;

    switch (arg->type) {
    case ENT_NUMBER:
        *value = arg->d.num;
        return true;
    case ENT_STRING:
        if (!arg->d.str || !is_number(arg->d.str))
            return false;
        *value = atol(arg->d.str);
        return true;
    default:
        return false;
    }
}

static bool scriptcmd_event_parse_instance(SCRIPT_PARAM *arg, uint32_t *value)
{
    long parsed = 0;

    if (!value)
        return false;

    if (!scriptcmd_event_parse_uid(arg, &parsed) || parsed < 0)
        return false;

    *value = (uint32_t)parsed;
    return true;
}

static bool scriptcmd_event_param_to_token(SCRIPT_PARAM *arg, char *buf, size_t size)
{
    if (!arg || !buf || size == 0)
        return false;

    buf[0] = '\0';

    switch (arg->type) {
    case ENT_NUMBER:
        snprintf(buf, size, "%d", arg->d.num);
        return true;
    case ENT_WIDEVNUM:
        if (!arg->d.wnum.pArea || arg->d.wnum.vnum < 1)
            return false;
        snprintf(buf, size, "%ld#%ld", arg->d.wnum.pArea->uid, arg->d.wnum.vnum);
        return true;
    case ENT_STRING:
        if (IS_NULLSTR(arg->d.str))
            return false;
        snprintf(buf, size, "%s", arg->d.str);
        return true;
    default:
        return false;
    }
}

static CHAR_DATA *scriptcmd_event_default_starter(SCRIPT_VARINFO *info)
{
    if (!info)
        return NULL;

    if (info->mob)
        return info->mob;

    if (info->token && info->token->player)
        return info->token->player;

    if (info->obj && info->obj->carried_by)
        return info->obj->carried_by;

    return NULL;
}

static VARIABLE **scriptcmd_event_runtime_vars_from_param(SCRIPT_PARAM *arg)
{
    VARIABLE **vars = NULL;

    if (!arg)
        return NULL;

    switch (arg->type) {
    case ENT_EVENT:
        if (event_runtime_get_vars_by_ref(arg->d.event.uid, arg->d.event.instance_id, &vars))
            return vars;
        break;

    case ENT_STRING:
        if (event_runtime_get_vars(arg->d.str, &vars))
            return vars;
        break;

    case ENT_NUMBER:
        if (event_runtime_get_vars_by_ref(arg->d.num, 0, &vars))
            return vars;
        break;

    default:
        break;
    }

    return NULL;
}

static VARIABLE **scriptcmd_event_index_vars_from_param(SCRIPT_PARAM *arg)
{
    VARIABLE **vars = NULL;
    EVENT_INDEX_DATA *event_index;

    if (!arg)
        return NULL;

    switch (arg->type) {
    case ENT_EVENT:
        if (event_index_get_vars_by_uid(arg->d.event.uid, &vars))
            return vars;
        break;

    case ENT_STRING:
        if (event_index_get_vars(arg->d.str, &vars))
            return vars;
        break;

    case ENT_NUMBER:
        if (event_index_get_vars_by_uid(arg->d.num, &vars))
            return vars;
        break;

    case ENT_WIDEVNUM:
        event_index = get_event_index_for_area(arg->d.wnum.pArea, arg->d.wnum.vnum);
        if (event_index)
            return &event_index->index_vars;
        break;

    default:
        break;
    }

    return NULL;
}

const struct script_cmd_type area_cmd_table[] = {
    { "alterroom",			scriptcmd_alterroom,		true,	true	},
    { "call",				scriptcmd_call,				false,	true	},
    { "churchannouncetheft",	scriptcmd_churchannouncetheft,	true, true },
    { "dungeoncomplete",	scriptcmd_dungeoncomplete,	true,	true	},
    { "dungeoncommence",	scriptcmd_dungeoncommence,	true,	true	},
    { "dungeonfailure",	scriptcmd_dungeonfailure,	true,	true	},
    { "event",             scriptcmd_event,            false,  true    },
    { "phaseevent",        scriptcmd_phaseevent,       false,  true    },
    { "stageevent",        scriptcmd_phaseevent,       false,  true    },
    { "startevent",        scriptcmd_startevent,       false,  true    },
    { "stopevent",         scriptcmd_stopevent,        false,  true    },
    { "echoat",				scriptcmd_echoat,			false,	true	},
    { "questechoat",       scriptcmd_questechoat,      false,  true    },
    { "quest",             scriptcmd_quest,            false,  true    },
    { "instancecomplete",	scriptcmd_instancecomplete,	true,	true	},
    { "instancefailure",	scriptcmd_instancefailure,	true,	true	},
    { "mail",				scriptcmd_mail,				true,	true	},
    { "mload",				scriptcmd_mload,			false,	true	},
    { "mute",				scriptcmd_mute,				false,	true	},
    { "oload",				scriptcmd_oload,			false,	true	},
    { "reckoning",			scriptcmd_reckoning,		true,	true	},
    { "resetroom",			scriptcmd_resetroom,		true,	true	},
    { "sendfloor",			scriptcmd_sendfloor,		false,	true	},
    { "specialkey",			scriptcmd_specialkey,		false,	true	},
    { "startreckoning",		scriptcmd_startreckoning,	true,	true	},
    { "stopreckoning",		scriptcmd_stopreckoning,	true,	true	},
    { "treasuremap",		scriptcmd_treasuremap,		false,	true	},
    { "unlockarea",			scriptcmd_unlockarea,		true,	true	},
    { "unlockdungeon",		scriptcmd_unlockdungeon,	true,	true	},
    { "unmute",				scriptcmd_unmute,			false,	true	},
    { "varclear",			scriptcmd_varclear,			false,	true	},
    { "varclearon",			scriptcmd_varclearon,		false,	true	},
    { "varcopy",			scriptcmd_varcopy,			false,	true	},
    { "varsave",			scriptcmd_varsave,			false,	true	},
    { "varsaveon",			scriptcmd_varsaveon,		false,	true	},
    { "varset",				scriptcmd_varset,			false,	true	},
    { "varseton",			scriptcmd_varseton,			false,	true	},
    { "wildsoverlay",		scriptcmd_wildsoverlay,		false,	true	},
    { "wildsanchor",		scriptcmd_wildsanchor,		false,	true	},
    { "wildstile",			scriptcmd_wildstile,		false,	true	},
    { "wildsvlink",		scriptcmd_wildsvlink,		false,	true	},
    { "wiznet",				scriptcmd_wiznet,			false,	true    },
    { "wildernessmap",		scriptcmd_wildernessmap,	false,	true	},
    { "xcall",				scriptcmd_xcall,			false,	true	},
    { NULL,					NULL,						false,	false	}
};

const struct script_cmd_type instance_cmd_table[] = {
    { "alterroom",			scriptcmd_alterroom,		true,	true	},
    { "call",				scriptcmd_call,				false,	true	},
    { "churchannouncetheft",	scriptcmd_churchannouncetheft,	true, true },
    { "dungeoncomplete",	scriptcmd_dungeoncomplete,	true,	true	},
    { "dungeoncommence",	scriptcmd_dungeoncommence,	true,	true	},
    { "dungeonfailure",	scriptcmd_dungeonfailure,	true,	true	},
    { "event",             scriptcmd_event,            false,  true    },
    { "phaseevent",        scriptcmd_phaseevent,       false,  true    },
    { "stageevent",        scriptcmd_phaseevent,       false,  true    },
    { "startevent",        scriptcmd_startevent,       false,  true    },
    { "stopevent",         scriptcmd_stopevent,        false,  true    },
    { "echoat",				scriptcmd_echoat,			false,	true	},
    { "questechoat",       scriptcmd_questechoat,      false,  true    },
    { "quest",             scriptcmd_quest,            false,  true    },
    { "instancecomplete",	scriptcmd_instancecomplete,	true,	true	},
    { "instancefailure",	scriptcmd_instancefailure,	true,	true	},
    { "loadinstanced",		scriptcmd_loadinstanced,	true,	true	},
    { "mail",				scriptcmd_mail,				true,	true	},
    { "makeinstanced",		scriptcmd_makeinstanced,	true,	true	},
    { "mload",				scriptcmd_mload,			false,	true	},
    { "mute",				scriptcmd_mute,				false,	true	},
    { "oload",				scriptcmd_oload,			false,	true	},
    { "reckoning",			scriptcmd_reckoning,		true,	true	},
    { "resetroom",			scriptcmd_resetroom,		true,	true	},
    { "sendfloor",			scriptcmd_sendfloor,		false,	true	},
    { "specialkey",			scriptcmd_specialkey,		false,	true	},
    { "startreckoning",		scriptcmd_startreckoning,	true,	true	},
    { "stopreckoning",		scriptcmd_stopreckoning,	true,	true	},
    { "stringmob",          scriptcmd_stringmob,        true,   true    },
    { "stringobj",          scriptcmd_stringobj,        true,   true    },
    { "treasuremap",		scriptcmd_treasuremap,		false,	true	},
    { "unlockarea",			scriptcmd_unlockarea,		true,	true	},
    { "unlockdungeon",		scriptcmd_unlockdungeon,	true,	true	},
    { "unmute",				scriptcmd_unmute,			false,	true	},
    { "varclear",			scriptcmd_varclear,			false,	true	},
    { "varclearon",			scriptcmd_varclearon,		false,	true	},
    { "varcopy",			scriptcmd_varcopy,			false,	true	},
    { "varsave",			scriptcmd_varsave,			false,	true	},
    { "varsaveon",			scriptcmd_varsaveon,		false,	true	},
    { "varset",				scriptcmd_varset,			false,	true	},
    { "varseton",			scriptcmd_varseton,			false,	true	},
    { "wildsoverlay",		scriptcmd_wildsoverlay,		false,	true	},
    { "wildsanchor",		scriptcmd_wildsanchor,		false,	true	},
    { "wildstile",			scriptcmd_wildstile,		false,	true	},
    { "wildsvlink",		scriptcmd_wildsvlink,		false,	true	},
    { "wildernessmap",		scriptcmd_wildernessmap,	false,	true	},
    { "wiznet",				scriptcmd_wiznet,			false,	true    },
    { "xcall",				scriptcmd_xcall,			false,	true	},
    { NULL,					NULL,						false,	false	}
};

const struct script_cmd_type dungeon_cmd_table[] = {
    { "alterroom",			scriptcmd_alterroom,		true,	true	},
    { "call",				scriptcmd_call,				false,	true	},
    { "churchannouncetheft",	scriptcmd_churchannouncetheft,	true, true },
    { "dungeoncomplete",	scriptcmd_dungeoncomplete,	true,	true	},
    { "dungeoncommence",	scriptcmd_dungeoncommence,	true,	true	},
    { "dungeonfailure",	scriptcmd_dungeonfailure,	true,	true	},
    { "event",             scriptcmd_event,            false,  true    },
    { "phaseevent",        scriptcmd_phaseevent,       false,  true    },
    { "stageevent",        scriptcmd_phaseevent,       false,  true    },
    { "startevent",        scriptcmd_startevent,       false,  true    },
    { "stopevent",         scriptcmd_stopevent,        false,  true    },
    { "echoat",				scriptcmd_echoat,			false,	true	},
    { "questechoat",       scriptcmd_questechoat,      false,  true    },
    { "quest",             scriptcmd_quest,            false,  true    },
    { "instancecomplete",	scriptcmd_instancecomplete,	true,	true	},
    { "instancefailure",	scriptcmd_instancefailure,	true,	true	},
    { "loadinstanced",		scriptcmd_loadinstanced,	true,	true	},
    { "mail",				scriptcmd_mail,				true,	true	},
    { "makeinstanced",		scriptcmd_makeinstanced,	true,	true	},
    { "mload",				scriptcmd_mload,			false,	true	},
    { "mute",				scriptcmd_mute,				false,	true	},
    { "oload",				scriptcmd_oload,			false,	true	},
    { "reckoning",			scriptcmd_reckoning,		true,	true	},
    { "resetroom",			scriptcmd_resetroom,		true,	true	},
    { "sendfloor",			scriptcmd_sendfloor,		false,	true	},
    { "specialkey",			scriptcmd_specialkey,		false,	true	},
    { "startreckoning",		scriptcmd_startreckoning,	true,	true	},
    { "stopreckoning",		scriptcmd_stopreckoning,	true,	true	},
    { "stringmob",          scriptcmd_stringmob,        true,   true    },
    { "stringobj",          scriptcmd_stringobj,        true,   true    },
    { "treasuremap",		scriptcmd_treasuremap,		false,	true	},
    { "unlockarea",			scriptcmd_unlockarea,		true,	true	},
    { "unlockdungeon",		scriptcmd_unlockdungeon,	true,	true	},
    { "unmute",				scriptcmd_unmute,			false,	true	},
    { "varclear",			scriptcmd_varclear,			false,	true	},
    { "varclearon",			scriptcmd_varclearon,		false,	true	},
    { "varcopy",			scriptcmd_varcopy,			false,	true	},
    { "varsave",			scriptcmd_varsave,			false,	true	},
    { "varsaveon",			scriptcmd_varsaveon,		false,	true	},
    { "varset",				scriptcmd_varset,			false,	true	},
    { "varseton",			scriptcmd_varseton,			false,	true	},
    { "wildsoverlay",		scriptcmd_wildsoverlay,		false,	true	},
    { "wildsanchor",		scriptcmd_wildsanchor,		false,	true	},
    { "wildstile",			scriptcmd_wildstile,		false,	true	},
    { "wildsvlink",		scriptcmd_wildsvlink,		false,	true	},
    { "wildernessmap",		scriptcmd_wildernessmap,	false,	true	},
    { "wiznet",				scriptcmd_wiznet,			false,	true    },
    { "xcall",				scriptcmd_xcall,			false,	true	},
    { NULL,					NULL,						false,	false	}
};

const struct script_cmd_type evt_cmd_table[] = {
    { "alterroom",            scriptcmd_alterroom,        true,   true    },
    { "call",                 scriptcmd_call,             false,  true    },
    { "churchannouncetheft",  scriptcmd_churchannouncetheft, true, true },
    { "dungeoncomplete",      scriptcmd_dungeoncomplete,  true,   true    },
    { "dungeoncommence",      scriptcmd_dungeoncommence,  true,   true    },
    { "dungeonfailure",       scriptcmd_dungeonfailure,   true,   true    },
    { "event",                scriptcmd_event,            false,  true    },
    { "phaseevent",           scriptcmd_phaseevent,       false,  true    },
    { "stageevent",           scriptcmd_phaseevent,       false,  true    },
    { "startevent",           scriptcmd_startevent,       false,  true    },
    { "stopevent",            scriptcmd_stopevent,        false,  true    },
    { "echoat",               scriptcmd_echoat,           false,  true    },
    { "questechoat",          scriptcmd_questechoat,      false,  true    },
    { "quest",                scriptcmd_quest,            false,  true    },
    { "instancecomplete",     scriptcmd_instancecomplete, true,   true    },
    { "instancefailure",      scriptcmd_instancefailure,  true,   true    },
    { "mail",                 scriptcmd_mail,             true,   true    },
    { "mload",                scriptcmd_mload,            false,  true    },
    { "mute",                 scriptcmd_mute,             false,  true    },
    { "oload",                scriptcmd_oload,            false,  true    },
    { "reckoning",            scriptcmd_reckoning,        true,   true    },
    { "resetroom",            scriptcmd_resetroom,        true,   true    },
    { "sendfloor",            scriptcmd_sendfloor,        false,  true    },
    { "specialkey",           scriptcmd_specialkey,       false,  true    },
    { "startreckoning",       scriptcmd_startreckoning,   true,   true    },
    { "stopreckoning",        scriptcmd_stopreckoning,    true,   true    },
    { "treasuremap",          scriptcmd_treasuremap,      false,  true    },
    { "unlockarea",           scriptcmd_unlockarea,       true,   true    },
    { "unlockdungeon",        scriptcmd_unlockdungeon,    true,   true    },
    { "unmute",               scriptcmd_unmute,           false,  true    },
    { "varclear",             scriptcmd_varclear,         false,  true    },
    { "varclearon",           scriptcmd_varclearon,       false,  true    },
    { "varcopy",              scriptcmd_varcopy,          false,  true    },
    { "varsave",              scriptcmd_varsave,          false,  true    },
    { "varsaveon",            scriptcmd_varsaveon,        false,  true    },
    { "varset",               scriptcmd_varset,           false,  true    },
    { "varseton",             scriptcmd_varseton,         false,  true    },
    { "wildsoverlay",         scriptcmd_wildsoverlay,     false,  true    },
    { "wildsanchor",          scriptcmd_wildsanchor,      false,  true    },
    { "wildstile",            scriptcmd_wildstile,        false,  true    },
    { "wildsvlink",           scriptcmd_wildsvlink,       false,  true    },
    { "wildernessmap",        scriptcmd_wildernessmap,    false,  true    },
    { "wiznet",               scriptcmd_wiznet,           false,  true    },
    { "xcall",                scriptcmd_xcall,            false,  true    },
    { NULL,                     NULL,                       false,  false   }
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

int evtcmd_lookup(char *command)
{
    int cmd;

    for (cmd = 0; evt_cmd_table[cmd].name; cmd++)
        if (command[0] == evt_cmd_table[cmd].name[0] &&
            !str_prefix(command, evt_cmd_table[cmd].name))
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
    WNUM wnum = { NULL, 0 };

    one_argument(argument, buf);
    if (!parse_widevnum(buf, ch->in_room ? ch->in_room->area : NULL, &wnum)) {
        send_to_char("Invalid vnum format.\n\r", ch);
        return;
    }

    aprg = get_script_index(wnum.pArea, wnum.vnum, PRG_APROG);

    if (!aprg) {
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
    WNUM wnum = { NULL, 0 };

    one_argument(argument, buf);
    if (!parse_widevnum(buf, ch->in_room ? ch->in_room->area : NULL, &wnum)) {
        send_to_char("Invalid vnum format.\n\r", ch);
        return;
    }

    iprg = get_script_index(wnum.pArea, wnum.vnum, PRG_IPROG);

    if (!iprg) {
        send_to_char("No such INSTANCEprogram.\n\r", ch);
        return;
    }

    if (!can_edit_blueprints(ch)) {
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
    WNUM wnum = { NULL, 0 };

    one_argument(argument, buf);
    if (!parse_widevnum(buf, ch->in_room ? ch->in_room->area : NULL, &wnum)) {
        send_to_char("Invalid vnum format.\n\r", ch);
        return;
    }

    dprg = get_script_index(wnum.pArea, wnum.vnum, PRG_DPROG);

    if (!dprg) {
        send_to_char("No such DUNGEONprogram.\n\r", ch);
        return;
    }

    if (!can_edit_dungeons(ch)) {
        send_to_char("You do not have permission to view that script.\n\r", ch);
        return;
    }

    page_to_char(dprg->edit_src, ch);
}

///////////////////////////////////////////
//
// Function: do_qpdump
//
// Section: Script/QPROG
//
// Purpose: Displays the current edit source code of a QPROG.
//
// Syntax: qpdump <vnum>
//
// Restrictions: Viewer must have implementor staff rank.
//
void do_qpdump(CHAR_DATA *ch, char *argument)
{
    char buf[ MAX_INPUT_LENGTH ];
    SCRIPT_DATA *qprg;
    WNUM wnum = { NULL, 0 };

    one_argument(argument, buf);
    if (!parse_widevnum(buf, ch->in_room ? ch->in_room->area : NULL, &wnum)) {
        send_to_char("Invalid vnum format.\n\r", ch);
        return;
    }

    qprg = get_script_index(wnum.pArea, wnum.vnum, PRG_QPROG);

    if (!qprg) {
        send_to_char("No such QUESTprogram.\n\r", ch);
        return;
    }

    if (!IS_STAFF(ch, STAFF_IMPLEMENTOR)) {
        send_to_char("You do not have permission to view that script.\n\r", ch);
        return;
    }

    page_to_char(qprg->edit_src, ch);
}

///////////////////////////////////////////
//
// Function: do_epdump
//
// Section: Script/EPROG
//
// Purpose: Displays the current edit source code of an EPROG.
//
// Syntax: epdump <vnum>
//
// Restrictions: Viewer must have read access to the owning area.
//
void do_epdump(CHAR_DATA *ch, char *argument)
{
    char buf[ MAX_INPUT_LENGTH ];
    SCRIPT_DATA *eprg;
    WNUM wnum = { NULL, 0 };

    one_argument(argument, buf);
    if (!parse_widevnum(buf, ch->in_room ? ch->in_room->area : NULL, &wnum)) {
        send_to_char("Invalid vnum format.\n\r", ch);
        return;
    }

    eprg = get_script_index(wnum.pArea, wnum.vnum, PRG_EPROG);

    if (!eprg) {
        send_to_char("No such EVENTprogram.\n\r", ch);
        return;
    }

    if (!area_has_read_access(ch, eprg->area)) {
        send_to_char("You do not have permission to view that script.\n\r", ch);
        return;
    }

    page_to_char(eprg->edit_src, ch);
}




//////////////////////////////////////
// A

// ADDAFFECT mobile|object apply-type(string) affect-group(string) skill(string) level(number) location(string) modifier(number) duration(number) bitvector(string) bitvector2(string)[ wear-location(object)]
SCRIPT_CMD(scriptcmd_addaffect)
{
    char *rest;
    int where, group, skill, level, loc, mod, hours;
    long bv, bv2;
    CHAR_DATA *mob = NULL;
    OBJ_DATA *obj = NULL;
    int wear_loc = WEAR_NONE;

    AFFECT_DATA af = {0};

    info->progs->lastreturn = 0;


    //
    // Get mobile or object TARGET
    if(!(rest = expand_argument(info,argument,arg))) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "AddAffect - Error in parsing.");
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        if (!(mob = script_get_char_room(info, arg->d.str, false)))
            obj = script_get_obj_here(info, arg->d.str);
        break;
    case ENT_MOBILE: mob = arg->d.mob; break;
    case ENT_OBJECT: obj = arg->d.obj; break;
    default: break;
    }

    if(!mob && !obj) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - NULL target.");
        return;
    }


    //
    // Get APPLY TYPE
    if(!(rest = expand_argument(info,rest,arg))) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
        return;
    }

    switch(arg->type) {
    case ENT_STRING: skill = skill_lookup(arg->d.str); break;
    default: return;
    }


    //
    // Get LEVEL
    if(!(rest = expand_argument(info,rest,arg))) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: mod = arg->d.num; break;
    default: return;
    }


    //
    // Get DURATION
    if(!(rest = expand_argument(info,rest,arg))) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
                return;
            }

            switch(arg->type) {
            case ENT_STRING: bv = flag_value(weapon_type2,arg->d.str); break;
            default: return;
            }

            if(bv2 == NO_FLAG) bv2 = 0;
            break;
    }

    //
    // Get WEAR-LOCATION of object
    if(rest && *rest) {
        if(!(rest = expand_argument(info,rest,arg))) {
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
    af.token = NULL;
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

    AFFECT_DATA af = {0};

    info->progs->lastreturn = 0;


    //
    // Get mobile or object TARGET
    if(!(rest = expand_argument(info,argument,arg))) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "AddAffectName - Error in parsing.");
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        if (!(mob = script_get_char_room(info, arg->d.str, false)))
            obj = script_get_obj_here(info, arg->d.str);
        break;
    case ENT_MOBILE: mob = arg->d.mob; break;
    case ENT_OBJECT: obj = arg->d.obj; break;
    default: break;
    }

    if(!mob && !obj) {
        pbugf(LOG_SCRIPTS, "AddAffectName - NULL target.", 0);
        return;
    }


    //
    // Get APPLY TYPE
    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "AddAffectName - Error in parsing.");
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
        pbugf(LOG_SCRIPTS, "AddAffectName - Error in parsing.");
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
    // Get NAME
    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "AddAffectName - Error in parsing.");
        return;
    }

    switch(arg->type) {
    case ENT_STRING: name = create_affect_cname(arg->d.str); break;
    default: return;
    }

    if(!name) {
        pbugf(LOG_SCRIPTS, "AddAffectName - Error allocating affect name.");
        return;
    }


    //
    // Get LEVEL
    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "AddAffectName - Error in parsing.");
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
        pbugf(LOG_SCRIPTS, "AddAffectName - Error in parsing.");
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
        pbugf(LOG_SCRIPTS, "AddAffectName - Error in parsing.");
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: mod = arg->d.num; break;
    default: return;
    }


    //
    // Get DURATION
    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "AddAffectName - Error in parsing.");
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
                pbugf(LOG_SCRIPTS, "Addaffect - Error in parsing.");
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
                pbugf(LOG_SCRIPTS, "Addaffect - Error in parsing.");
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
                pbugf(LOG_SCRIPTS, "Addaffect - Error in parsing.");
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
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Addaffect - Error in parsing.");
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
            pbugf(LOG_SCRIPTS, "AddAffectName - Error in parsing.");
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
    af.token = NULL;
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
    case ENT_STRING: victim = script_get_char_room(info, arg->d.str, false); break;
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

    if (!IS_SET(victim->affected_by[1], AFF2_TOXIN)) {
        AFFECT_DATA af;
        af.where = TO_AFFECTS;
        af.group     = AFFGROUP_BIOLOGICAL;
        SKILL_DATA *sk_toxins = skill_find("toxins");
        af.type  = skill_sn(sk_toxins);
    af.skill = sk_toxins;
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
// * REPLY: MOBILE
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

    bool show = true;

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
    strlcpy(field, arg->d.str, sizeof(field));

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
                SET_BIT(entity_mob->act[0], ACT_PET);
                SET_BIT(entity_mob->affected_by[0], AFF_CHARM);
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
            else if(!str_prefix(field, "reply"))
            {
                if (IS_NPC(entity_mob))
                    return;

                entity_mob->reply = target_mob;
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


// AWARD mobile string(type)[ subtype] number(amount)
// Types: silver, gold, pneuma, deity/dp, practice, train, quest/qp, experience/xp, reputation, paragon
// Subtype is only used by reputation or paragon and should be a widevnum.
//
// AWARD church string(type) number(amount)
// Types: gold, pneuma, deity/dp
//
SCRIPT_CMD(scriptcmd_award)
{
    char buf[MSL], *rest;
    char rep_name[3 * MIL];
    char field[MIL];
    char *field_name;
    CHAR_DATA *victim = NULL;
    CHURCH_DATA *church = NULL;
    REPUTATION_INDEX_DATA *repIndex = NULL;
    bool paragon = false;
    int amount = 0;


    info->progs->lastreturn = 0;

    if (!(rest = expand_argument(info,argument,arg)))
        return;

    switch(arg->type) {
    case ENT_CHURCH: church = arg->d.church; break;
    case ENT_STRING: victim = script_get_char_room(info, arg->d.str, true); break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: victim = NULL; break;
    }

    if (!victim && !church) return;

    if (!(rest = expand_argument(info,rest,arg)))
        return;

    if( arg->type != ENT_STRING ) return;
    strncpy(field,arg->d.str,MIL-1);

    if (!church)
    {
        if(!str_prefix(field, "reputation") || !str_prefix(field, "paragon"))
        {
            if (!str_prefix(field, "paragon"))
            {
                if (script_security < 7)
                    return;
                paragon = true;
            }

            if (!(rest = expand_argument(info,rest,arg)))
                return;

            switch(arg->type)
            {
            case ENT_WIDEVNUM:
                repIndex = get_reputation_index_wnum(arg->d.wnum);
                break;

            case ENT_STRING:
            {
                WNUM wnum;
                AREA_DATA *context = info->mob ? info->mob->pIndexData->area : NULL;
                if (parse_widevnum(arg->d.str, context, &wnum))
                    repIndex = get_reputation_index_wnum(wnum);
                break;
            }

            case ENT_NUMBER:
            {
                AREA_DATA *context = info->mob ? info->mob->pIndexData->area : NULL;
                if (context)
                    repIndex = get_reputation_index(context, arg->d.num);
                break;
            }

            default:
                break;
            }

            if (!IS_VALID(repIndex))
                return;
        }
    }

    if (!(rest = expand_argument(info,rest,arg)))
        return;

    switch(arg->type) {
    case ENT_STRING: amount = atoi(arg->d.str); break;
    case ENT_NUMBER: amount = arg->d.num; break;
    default: amount = 0; break;
    }

    if(amount < 1) return;

    int ret = amount;

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

        if (IS_VALID(repIndex)) {
            if (paragon) {
                REPUTATION_DATA *rep = find_reputation_char(victim, repIndex);
                if (!IS_VALID(rep)) return;

                if (rep->current_rank < list_size(repIndex->ranks))
                    return;

                REPUTATION_INDEX_RANK_DATA *rank = (REPUTATION_INDEX_RANK_DATA *)list_nthdata(repIndex->ranks, rep->current_rank);
                if (!IS_VALID(rank) || !IS_SET(rank->flags, REPUTATION_RANK_PARAGON))
                    return;

                for (int i = 0; i < amount; i++)
                    paragon_reputation(victim, rep, false);

                sprintf(rep_name, "%s (%ld#%ld) paragon levels", repIndex->name, repIndex->area->uid, repIndex->vnum);
                field_name = rep_name;
                ret = amount;
            } else {
                long total_given = 0;
                if (!gain_reputation(victim, repIndex, amount, NULL, &total_given, false))
                    return;

                sprintf(rep_name, "%s (%ld#%ld) reputation points", repIndex->name, repIndex->area->uid, repIndex->vnum);
                field_name = rep_name;
                ret = total_given;
            }

        } else if( !str_prefix(field, "silver") ) {
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
            script_adjust_quest_points(victim, amount, NULL);
            field_name = "quest points";

        } else if( !str_prefix(field, "experience") || !str_cmp(field, "xp") ) {
            gain_exp(victim, NULL, amount, true);
            field_name = "experience";

        } else
            return;


        if(!IS_NPC(victim)) {
            sprintf(buf, "Award logged: %s was awarded %d %s", victim->name, ret, field_name);
            log_string(buf);
        }
    }

    info->progs->lastreturn = ret;
}

//////////////////////////////////////
// B

// BREATHE $VICTIM $TYPE[ $ATTACKER]
// Does the dragon breath of the given type: acid, fire, frost, gas, lightning
SCRIPT_CMD(scriptcmd_breathe)
{
    static char *breath_names[] = { "acid", "fire", "frost", "gas", "lightning", NULL };
    static const char *breath_skill_names[] = { "acid breath", "fire breath", "frost breath", "gas breath", "lightning breath" };
    static SPELL_FUN *breath_fun[] = { spell_acid_breath, spell_fire_breath, spell_frost_breath, spell_gas_breath, spell_lightning_breath };
    char *rest;
    CHAR_DATA *attacker = NULL;
    CHAR_DATA *victim = NULL;
    int i;

    info->progs->lastreturn = 0;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    switch(arg->type) {
    case ENT_STRING: victim = script_get_char_room(info, arg->d.str, false); break;
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
        case ENT_STRING: attacker = script_get_char_room(info, arg->d.str, false); break;
        case ENT_MOBILE: attacker = arg->d.mob; break;
        default: attacker = NULL; break;
        }
    }

    if(!attacker)
        return;

    (*breath_fun[i])(skill_find(breath_skill_names[i]), attacker->tot_level, attacker, victim, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);

    info->progs->lastreturn = 1;
}

//////////////////////////////////////
// C

SCRIPT_CMD(scriptcmd_call)
{
    char *rest; //buf[MSL], *rest;
    CHAR_DATA *vch = NULL,*ch = NULL;
    OBJ_DATA *obj1 = NULL,*obj2 = NULL;
    SCRIPT_DATA *script;
    int depth, ret;
    long vnum;
    int space;

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

    if (info->mob) space = PRG_MPROG;
    else if(info->obj) space = PRG_OPROG;
    else if(info->room) space = PRG_RPROG;
    else if(info->token) space = PRG_TPROG;
    else if(info->area) {
        if (info->block && info->block->script && info->block->script->type == PRG_QPROG)
            space = PRG_QPROG;
        else
            space = PRG_APROG;
    }
    else if(info->instance) space = PRG_IPROG;
    else if(info->dungeon) space = PRG_DPROG;
    else return;

    script = get_script_from_arg(info, arg, space, &vnum);
    if (vnum < 1 || !script) {
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
        case ENT_STRING: ch = script_get_char_room(info, arg->d.str, false); break;
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
            // Restore the call depth to the previous value
            script_call_depth = depth;
            return;
        }

        switch(arg->type) {
        case ENT_STRING: vch = script_get_char_room(info, arg->d.str, false); break;
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
    ret = execute_script(script->vnum, script, info->mob, info->obj, info->room, info->token, info->area, info->instance, info->dungeon, ch, obj1, obj2, vch, NULL,NULL, NULL,info->phrase,info->trigger,info->trigger_type,0,0,0,0,0);
    if(info->progs) {
        info->progs->lastreturn = ret;
    } else
        info->block->ret_val = ret;

    // restore the call depth to the previous value
    script_call_depth = depth;
}

SCRIPT_CMD(scriptcmd_crier)
{
    BUFFER *buffer;

    if(!info)
        return;

    if(!(info->mob || info->obj || info->room || info->token))
        return;

    buffer = new_buf();
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

SCRIPT_CMD(scriptcmd_stringobj)
{
    char field[MIL], *rest, **str;
    int min_sec = MIN_SCRIPT_SECURITY;
    OBJ_DATA *obj = NULL;
    bool newlines = false;
    const char *scope_name = NULL;
    long scope_vnum = 0;

    if(!info) return;

    if(info->obj) {
        scope_name = "OpStringObj";
        scope_vnum = VNUM(info->obj);
    } else if(info->mob) {
        scope_name = "MpStringObj";
        scope_vnum = VNUM(info->mob);
    } else if(info->room) {
        scope_name = "RpStringObj";
        scope_vnum = info->room->vnum;
    } else if(info->token) {
        scope_name = "TpStringObj";
        scope_vnum = VNUM(info->token);
    } else if(info->instance) {
        scope_name = "IpStringObj";
        scope_vnum = (info->instance->blueprint ? info->instance->blueprint->vnum : 0);
    } else if(info->dungeon) {
        scope_name = "DpStringObj";
        scope_vnum = (info->dungeon->index ? info->dungeon->index->vnum : 0);
    } else
        return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        obj = script_get_obj_here(info, arg->d.str);
        break;
    case ENT_OBJECT:
        obj = arg->d.obj;
        break;
    default:
        break;
    }

    if(!obj) {
        pbugf(LOG_SCRIPTS, "%s - NULL object from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(PROG_FLAG(obj,PROG_AT)) {
        pbugf(LOG_SCRIPTS, "%s - blocked restring on PROG_AT object from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "%s - Missing field type from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    field[0] = '\0';

    if(arg->type == ENT_STRING)
        strncpy(field,arg->d.str,MIL-1);
    else
        return;

    if(!field[0]) return;

    BUFFER *buffer = new_buf();
    expand_string(info,rest,buffer);

    if(!buf_string(buffer)[0]) {
        pbugf(LOG_SCRIPTS, "%s - Empty string used from vnum %ld.", scope_name, scope_vnum);
        free_buf(buffer);
        return;
    }

    if(!str_cmp(field,"name")) {
        if(obj->old_short_descr)
        {
            free_buf(buffer);
            return;
        }
        str = (char**)&obj->name;
    } else if(!str_cmp(field,"owner")) {
        str = (char**)&obj->owner;
        min_sec = 5;
    } else if(!str_cmp(field,"short")) {
        if(obj->old_short_descr)
        {
            free_buf(buffer);
            return;
        }
        str = (char**)&obj->short_descr;
    } else if(!str_cmp(field,"long")) {
        if(obj->old_description)
        {
            free_buf(buffer);
            return;
        }
        str = (char**)&obj->description;
    } else if(!str_cmp(field,"full")) {
        if(obj->old_full_description)
        {
            free_buf(buffer);
            return;
        }
        str = (char**)&obj->full_description;
        newlines = true;
    } else if(!str_cmp(field,"material")) {
        int mat = material_lookup(buf_string(buffer));

        if(mat < 0) {
            pbugf(LOG_SCRIPTS, "%s - Invalid material from vnum %ld.", scope_name, scope_vnum);
            free_buf(buffer);
            return;
        }

        clear_buf(buffer);
        add_buf(buffer, material_name(mat));

        str = (char**)&obj->material;
    } else {
        free_buf(buffer);
        return;
    }

    if(script_security < min_sec) {
        pbugf(LOG_SCRIPTS, "%s - Attempting to restring '%s' with security %d from vnum %ld.", scope_name, field, script_security, scope_vnum);
        free_buf(buffer);
        return;
    }

    char *p = buf_string(buffer);
    strip_newline(p, newlines);

    free_string(*str);
    *str = str_dup(p);

    free_buf(buffer);
}

SCRIPT_CMD(scriptcmd_stringmob)
{
    char field[MIL], *rest, **str;
    int min_sec = MIN_SCRIPT_SECURITY;
    CHAR_DATA *mob = NULL;
    bool newlines = false;
    const char *scope_name = NULL;
    long scope_vnum = 0;

    if(!info) return;

    if(info->obj) {
        scope_name = "OpStringMob";
        scope_vnum = VNUM(info->obj);
    } else if(info->mob) {
        scope_name = "MpStringMob";
        scope_vnum = VNUM(info->mob);
    } else if(info->room) {
        scope_name = "RpStringMob";
        scope_vnum = info->room->vnum;
    } else if(info->token) {
        scope_name = "TpStringMob";
        scope_vnum = VNUM(info->token);
    } else if(info->instance) {
        scope_name = "IpStringMob";
        scope_vnum = (info->instance->blueprint ? info->instance->blueprint->vnum : 0);
    } else if(info->dungeon) {
        scope_name = "DpStringMob";
        scope_vnum = (info->dungeon->index ? info->dungeon->index->vnum : 0);
    } else
        return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        mob = script_get_char_room(info, arg->d.str, true);
        break;
    case ENT_MOBILE:
        mob = arg->d.mob;
        break;
    default:
        break;
    }

    if(!mob) {
        pbugf(LOG_SCRIPTS, "%s - NULL mobile from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(!IS_NPC(mob)) {
        pbugf(LOG_SCRIPTS, "%s - can't change strings on PCs from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "%s - Missing field type from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    field[0] = '\0';
    if(arg->type == ENT_STRING)
        strncpy(field,arg->d.str,MIL-1);
    else
        return;

    if(!field[0]) return;

    BUFFER *buffer = new_buf();
    expand_string(info,rest,buffer);

    if(!buf_string(buffer)[0]) {
        pbugf(LOG_SCRIPTS, "%s - Empty string used from vnum %ld.", scope_name, scope_vnum);
        free_buf(buffer);
        return;
    }

    if(!str_cmp(field,"name"))
        str = (char**)&mob->name;
    else if(!str_cmp(field,"owner")) {
        str = (char**)&mob->owner;
        min_sec = 5;
    } else if(!str_cmp(field,"short"))
        str = (char**)&mob->short_descr;
    else if(!str_cmp(field,"long")) {
        str = (char**)&mob->long_descr;
        newlines = true;
    } else if(!str_cmp(field,"full")) {
        str = (char**)&mob->description;
        newlines = true;
    } else if(!str_cmp(field,"tempstring"))
        str = (char**)&mob->tempstring;
    else {
        free_buf(buffer);
        return;
    }

    if(script_security < min_sec) {
        pbugf(LOG_SCRIPTS, "%s - Attempting to restring '%s' with security %d from vnum %ld.", scope_name, field, script_security, scope_vnum);
        free_buf(buffer);
        return;
    }

    char *p = buf_string(buffer);
    strip_newline(p, newlines);

    free_string(*str);
    *str = str_dup(p);

    free_buf(buffer);
}

SCRIPT_CMD(scriptcmd_interrupt)
{
    char *rest;
    CHAR_DATA *victim = NULL;
    ROOM_INDEX_DATA *here = NULL;
    const char *scope_name = NULL;
    long scope_vnum = 0;
    int stop, ret = 0;
    bool silent = false;

    if(!info) return;

    if(info->mob) {
        scope_name = "MpInterrupt";
        scope_vnum = VNUM(info->mob);
        here = info->mob->in_room;
    } else if(info->obj) {
        scope_name = "OpInterrupt";
        scope_vnum = VNUM(info->obj);
        here = obj_room(info->obj);
    } else if(info->room) {
        scope_name = "RpInterrupt";
        scope_vnum = info->room->vnum;
        here = info->room;
    } else if(info->token) {
        scope_name = "TpInterrupt";
        scope_vnum = VNUM(info->token);
        here = token_room(info->token);
    } else
        return;

    SETRETURN(0);

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        victim = get_char_world(NULL, arg->d.str);
        break;
    case ENT_MOBILE:
        victim = arg->d.mob;
        break;
    default:
        break;
    }

    if(!victim) {
        pbugf(LOG_SCRIPTS, "%s - NULL victim from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    BUFFER *buffer = new_buf();
    expand_string(info,rest,buffer);
    if(buffer->string[0] != '\0') {
        stop = flag_value(interrupt_action_types,buffer->string);
        if(stop == NO_FLAG) {
            pbugf(LOG_SCRIPTS, "%s - invalid interrupt type from vnum %ld.", scope_name, scope_vnum);
            free_buf(buffer);
            return;
        }
    } else
        stop = ~INTERRUPT_SILENT;

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
        if(here && victim->projectile_dir >= 0)
            variables_set_exit(info->var,"stoprangeexit",here->exit[victim->projectile_dir]);
        else
            variables_set_exit(info->var,"stoprangeexit",NULL);
        victim->ranged = 0;
        victim->projectile_weapon = NULL;
        free_string(victim->projectile_victim);
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
        if(here && victim->fade_dir >= 0)
            variables_set_exit(info->var,"stopfade",here->exit[victim->fade_dir]);
        else
            variables_set_exit(info->var,"stopfade",NULL);
        victim->fade = 0;
        victim->fade_dir = -1;
        SET_BIT(ret,INTERRUPT_FADE);
    }

    if (IS_SET(stop,INTERRUPT_SCRIPT)) {
        if(interrupt_script(victim, silent))
            SET_BIT(ret,INTERRUPT_SCRIPT);
    }

    SETRETURN(ret);
    free_buf(buffer);
}

SCRIPT_CMD(scriptcmd_showroom)
{
    CHAR_DATA *viewer = NULL, *next;
    ROOM_INDEX_DATA *room = NULL, *dest;
    WILDS_DATA *wilds = NULL;
    const char *scope_name = NULL;
    long scope_vnum = 0;
    long mapid;
    long x,y;
    long width, height;
    bool force;

    if(!info) return;

    if(info->mob) {
        scope_name = "MpShowMap";
        scope_vnum = VNUM(info->mob);
    } else if(info->obj) {
        scope_name = "OpShowMap";
        scope_vnum = VNUM(info->obj);
    } else if(info->room) {
        scope_name = "RpShowMap";
        scope_vnum = info->room->vnum;
    } else if(info->token) {
        scope_name = "TpShowMap";
        scope_vnum = VNUM(info->token);
    } else
        return;

    if(!(argument = expand_argument(info,argument,arg)))
        return;

    switch(arg->type) {
    case ENT_MOBILE:
        viewer = arg->d.mob;
        break;
    case ENT_ROOM:
        room = arg->d.room;
        break;
    }

    if(!viewer && !room) {
        pbugf(LOG_SCRIPTS,"%s - bad target for showing the map from vnum %ld.", scope_name, scope_vnum);
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

        if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
            return;

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

        if(width < 5) width = 5;
        if(height < 5) height = 5;

        if(room) {
            for(viewer = room->people; viewer; viewer = next) {
                next = viewer->next_in_room;
                if(!IS_NPC(viewer) && (force || (IS_AWAKE(viewer) && check_vision(viewer,dest,false,false))))
                    show_map_to_char_wyx(wilds,x,y, viewer,x,y, width + viewer->wildview_bonus_x, height + viewer->wildview_bonus_y, false);
            }
        } else if(!IS_NPC(viewer)) {
            show_map_to_char_wyx(wilds,x,y, viewer,x,y, width + viewer->wildview_bonus_x, height + viewer->wildview_bonus_y, false);
        }
        return;
    }

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
        show_room(viewer,dest,true,true,false);
    }
}

SCRIPT_CMD(scriptcmd_skimprove)
{
    char skill[MIL],*rest;
    int min_diff, diff, sn=-1;
    CHAR_DATA *mob = NULL;
    TOKEN_DATA *token = NULL;
    bool success = false;
    const char *scope_name = NULL;
    long scope_vnum = 0;

    if(!info) return;

    if(info->mob) {
        scope_name = "MpSkImprove";
        scope_vnum = VNUM(info->mob);
    } else if(info->obj) {
        scope_name = "OpSkImprove";
        scope_vnum = VNUM(info->obj);
    } else if(info->room) {
        scope_name = "RpSkImprove";
        scope_vnum = info->room->vnum;
    } else if(info->token) {
        scope_name = "TpSkImprove";
        scope_vnum = VNUM(info->token);
    } else
        return;

    if(script_security < MIN_SCRIPT_SECURITY) {
        pbugf(LOG_SCRIPTS, "%s - Insufficient security from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        mob = script_get_char_room(info, arg->d.str, true);
        break;
    case ENT_MOBILE:
        mob = arg->d.mob;
        break;
    case ENT_TOKEN:
        token = arg->d.token;
    default:
        break;
    }

    if(!mob && !token) {
        pbugf(LOG_SCRIPTS, "%s - NULL target from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(mob) {
        if(IS_NPC(mob)) {
            pbugf(LOG_SCRIPTS, "%s - NPC target from vnum %ld.", scope_name, scope_vnum);
            return;
        }

        if(!(rest = expand_argument(info,rest,arg))) {
            pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
            return;
        }

        skill[0] = '\0';
        switch(arg->type) {
        case ENT_STRING:
            strncpy(skill,arg->d.str,MIL-1);
            break;
        default:
            return;
        }

        if(!skill[0]) return;

        sn = skill_lookup(skill);
        if(sn < 1) return;
    } else {
        if(token->pIndexData->type != TOKEN_SKILL && token->pIndexData->type != TOKEN_SPELL) {
            pbugf(LOG_SCRIPTS, "%s - Token is not a skill/spell token from vnum %ld.", scope_name, scope_vnum);
            return;
        }
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING: diff = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
    case ENT_NUMBER: diff = arg->d.num; break;
    default: return;
    }

    min_diff = 10 - script_security;
    if(diff < min_diff) {
        pbugf(LOG_SCRIPTS, "%s - Difficulty lower than allowed from vnum %ld.", scope_name, scope_vnum);
        diff = min_diff;
    }

    switch(arg->type) {
    case ENT_NONE: success = true; break;
    case ENT_STRING:
        if(is_number(arg->d.str))
            success = (bool)(atoi(arg->d.str) != 0);
        else
            success = !str_cmp(arg->d.str,"yes") || !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str,"success") || !str_cmp(arg->d.str,"pass");
        break;
    case ENT_NUMBER:
        success = (bool)(arg->d.num != 0);
        break;
    default:
        success = false;
        break;
    }

    if(token)
        token_skill_improve(token->player,token,success,diff);
    else
        check_improve(mob, sn, success, diff);
}

SCRIPT_CMD(scriptcmd_input)
{
    char *rest, *p;
    long vnum;
    CHAR_DATA *mob = NULL;
    SCRIPT_DATA *script = NULL;
    const char *scope_name = NULL;
    long scope_vnum = 0;
    int space;

    if(!info) return;

    if(info->mob) {
        scope_name = "MpInput";
        scope_vnum = VNUM(info->mob);
        space = PRG_MPROG;
    } else if(info->obj) {
        scope_name = "OpInput";
        scope_vnum = VNUM(info->obj);
        space = PRG_OPROG;
    } else if(info->room) {
        scope_name = "RpInput";
        scope_vnum = info->room->vnum;
        space = PRG_RPROG;
    } else if(info->token) {
        scope_name = "TpInput";
        scope_vnum = VNUM(info->token);
        space = PRG_TPROG;
    } else
        return;

    SETRETURN(0);

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        mob = script_get_char_room(info,arg->d.str,true);
        break;
    case ENT_MOBILE:
        mob = arg->d.mob;
        break;
    default:
        break;
    }

    if(!mob) {
        pbugf(LOG_SCRIPTS, "%s - NULL mobile from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(IS_NPC(mob) || !mob->desc || is_char_busy(mob) || mob->desc->pString != NULL || mob->desc->input)
        return;

    if(mob->desc->showstr_head != NULL)
        return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    script = get_script_from_arg(info, arg, space, &vnum);
    if(vnum < 1 || !script) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_NONE: p = NULL; break;
    case ENT_STRING: p = arg->d.str; break;
    default: return;
    }

    BUFFER *buffer = new_buf();
    expand_string(info,rest,buffer);

    mob->desc->input = true;
    mob->desc->input_var = p ? str_dup(p) : NULL;
    mob->desc->input_prompt = str_dup(buffer->string[0] ? buffer->string : " >");
    mob->desc->input_script = vnum;
    mob->desc->input_mob = info->mob;
    mob->desc->input_obj = info->obj;
    mob->desc->input_room = info->room;
    mob->desc->input_tok = info->token;

    SETRETURN(1);
    free_buf(buffer);
}

SCRIPT_CMD(scriptcmd_prompt)
{
    char name[MIL], *rest;
    CHAR_DATA *mob = NULL;
    const char *scope_name = NULL;
    long scope_vnum = 0;

    if(!info) return;

    if(info->mob) {
        scope_name = "MpPrompt";
        scope_vnum = VNUM(info->mob);
    } else if(info->obj) {
        scope_name = "OpPrompt";
        scope_vnum = VNUM(info->obj);
    } else if(info->room) {
        scope_name = "RpPrompt";
        scope_vnum = info->room->vnum;
    } else if(info->token) {
        scope_name = "TpPrompt";
        scope_vnum = VNUM(info->token);
    } else
        return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        mob = script_get_char_room(info,arg->d.str,true);
        break;
    case ENT_MOBILE:
        mob = arg->d.mob;
        break;
    default:
        break;
    }

    if(!mob) {
        pbugf(LOG_SCRIPTS, "%s - NULL mobile from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(IS_NPC(mob)) {
        pbugf(LOG_SCRIPTS, "%s - cannot set prompt strings on NPCs from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "%s - Missing name type from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    name[0] = '\0';
    switch(arg->type) {
    case ENT_STRING:
        strncpy(name,arg->d.str,MIL-1);
        break;
    default:
        return;
    }

    if(!name[0]) return;

    BUFFER *buffer = new_buf();
    expand_string(info,rest,buffer);

    if(buffer->string[0] != '\0')
        string_vector_set(&mob->pcdata->script_prompts,name,buffer->string);

    free_buf(buffer);
}

SCRIPT_CMD(scriptcmd_churchannouncetheft)
{
    char *rest = argument;
    CHAR_DATA *thief;
    OBJ_DATA *stolen;

    if (!info) return;

    SETRETURN(0);

    PARSE_ARGTYPE(MOBILE);
    thief = arg->d.mob;

    stolen = NULL;
    if (rest && *rest)
    {
        PARSE_ARGTYPE(OBJECT);
        stolen = arg->d.obj;
    }

    church_announce_theft(thief, stolen);
    SETRETURN(1);
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
    bool fAll = false, fKill = false, fLevel = false, fRemort = false, fTwo = false;


    if(!(rest = expand_argument(info,argument,arg))) {
        //pbugf(LOG_SCRIPTS, "MpDamage - Error in parsing from vnum %ld.", VNUM(info->mob));
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        if(!str_cmp(arg->d.str,"all")) fAll = true;
        else victim = script_get_char_room(info, arg->d.str, false);
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
        if(!str_cmp(arg->d.str,"level")) { fLevel = true; break; }
        if(!str_cmp(arg->d.str,"remort")) { fLevel = fRemort = true; break; }
        if(!str_cmp(arg->d.str,"dual")) { fLevel = fTwo = true; break; }
        if(!str_cmp(arg->d.str,"dualremort")) { fLevel = fTwo = fRemort = true; break; }
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
//		pbugf(LOG_SCRIPTS, "MpDamage - invalid argument from vnum %ld.", VNUM(info->mob));
        return;
    }

    if( *rest ) {
        if(!(rest = expand_argument(info,rest,arg)))
            return;

        if( arg->type != ENT_STRING ) return;

        if (!str_cmp(arg->d.str,"kill") || !str_cmp(arg->d.str,"lethal")) fKill = true;
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
                damage(attacker?attacker:victim, victim, fKill ? value : UMIN(victim->hit,value), TYPE_UNDEFINED, dc, false);
            }
        }
    } else {
        value = fLevel ? dice(low,high) : number_range(low,high);
        damage(attacker?attacker:victim, victim, fKill ? value : UMIN(victim->hit,value), TYPE_UNDEFINED, dc, false);
    }
}

SCRIPT_CMD(scriptcmd_gdamage)
{
    char buf[MSL], *rest;
    CHAR_DATA *victim = NULL, *rch, *rch_next;
    int low, high, level, value, dc;
    bool fKill = false, fLevel = false, fRemort = false, fTwo = false;
    ROOM_INDEX_DATA *location = NULL;
    const char *scope_name = NULL;
    long scope_vnum = 0;


    if(!info)
        return;

    if(info->mob) {
        location = info->mob->in_room;
        scope_name = "MpGdamage";
        scope_vnum = VNUM(info->mob);
    } else if(info->obj) {
        location = obj_room(info->obj);
        scope_name = "OpGdamage";
        scope_vnum = VNUM(info->obj);
    } else if(info->room) {
        location = info->room;
        scope_name = "RpGdamage";
        scope_vnum = info->room->vnum;
    } else if(info->token) {
        location = token_room(info->token);
        scope_name = "TpGdamage";
        scope_vnum = VNUM(info->token);
    } else
        return;

    if(!location)
        return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING: victim = get_char_room(NULL, location, arg->d.str); break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: victim = NULL; break;
    }

    if (!victim) {
        pbugf(LOG_SCRIPTS, "%s - Null victim from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "%s - missing argument from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
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
        pbugf(LOG_SCRIPTS, "%s - invalid argument from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "%s - missing argument from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
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
            pbugf(LOG_SCRIPTS, "%s - invalid argument from vnum %ld.", scope_name, scope_vnum);
            return;
        }
        break;
    case ENT_MOBILE:
        if(fLevel) {
            if(arg->d.mob) level = arg->d.mob->tot_level;
            else {
                pbugf(LOG_SCRIPTS, "%s - Null reference mob from vnum %ld.", scope_name, scope_vnum);
                return;
            }
            break;
        } else {
            pbugf(LOG_SCRIPTS, "%s - invalid argument from vnum %ld.", scope_name, scope_vnum);
            return;
        }
        break;
    default:
        pbugf(LOG_SCRIPTS, "%s - invalid argument from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    argument = one_argument(rest, buf);
    if (!str_cmp(buf,"kill") || !str_cmp(buf,"lethal"))
        fKill = true;

    one_argument(argument, buf);
    dc = damage_class_lookup(buf);

    if(fLevel)
        get_level_damage(level,&low,&high,fRemort,fTwo);

    for(rch = location->people; rch; rch = rch_next) {
        rch_next = rch->next_in_room;
        if ((info->mob && rch == info->mob) || rch == victim)
            continue;

        if (is_same_group(victim,rch)) {
            value = fLevel ? dice(low,high) : number_range(low,high);
            damage(rch, rch, fKill ? value : UMIN(rch->hit,value), TYPE_UNDEFINED, dc, false);
        }
    }
}


// DEDUCT mobile string(type)[ subtype] number(amount)
// Types: silver, gold, pneuma, deity/dp, practice, train, quest/qp, reputation, paragon
// Subtype is only used by reputation or paragon and should be a widevnum.
// Returns actual amount deducted
//
// DEDUCT church string(type) number(amount)
// Types: gold, pneuma, deity/dp
// Returns actual amount deducted
//
SCRIPT_CMD(scriptcmd_deduct)
{
    char buf[MSL], *rest;
    char rep_name[3 * MIL];
    char field[MIL];
    char *field_name;
    CHAR_DATA *victim = NULL;
    CHURCH_DATA *church = NULL;
    REPUTATION_INDEX_DATA *repIndex = NULL;
    bool paragon = false;
    int amount = 0;


    info->progs->lastreturn = 0;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    switch(arg->type) {
    case ENT_CHURCH: church = arg->d.church; break;
    case ENT_STRING: victim = script_get_char_room(info, arg->d.str, true); break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: victim = NULL; break;
    }

    if (!victim && !church) return;

    if(!(rest = expand_argument(info,rest,arg)))
        return;

    if( arg->type != ENT_STRING ) return;
    strncpy(field,arg->d.str,MIL-1);

    if (!church)
    {
        if(!str_prefix(field, "reputation") || !str_prefix(field, "paragon"))
        {
            if (!str_prefix(field, "paragon"))
            {
                if (script_security < 7)
                    return;
                paragon = true;
            }

            if (!(rest = expand_argument(info,rest,arg)))
                return;

            switch(arg->type)
            {
            case ENT_WIDEVNUM:
                repIndex = get_reputation_index_wnum(arg->d.wnum);
                break;

            case ENT_STRING:
            {
                WNUM wnum;
                AREA_DATA *context = info->mob ? info->mob->pIndexData->area : NULL;
                if (parse_widevnum(arg->d.str, context, &wnum))
                    repIndex = get_reputation_index_wnum(wnum);
                break;
            }

            case ENT_NUMBER:
            {
                AREA_DATA *context = info->mob ? info->mob->pIndexData->area : NULL;
                if (context)
                    repIndex = get_reputation_index(context, arg->d.num);
                break;
            }

            default:
                break;
            }

            if (!IS_VALID(repIndex))
                return;
        }
    }

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
        if (IS_VALID(repIndex)) {
            if (paragon) {
                REPUTATION_DATA *rep = find_reputation_char(victim, repIndex);
                if (!IS_VALID(rep)) return;

                if (rep->current_rank < list_size(repIndex->ranks))
                    return;

                REPUTATION_INDEX_RANK_DATA *rank = (REPUTATION_INDEX_RANK_DATA *)list_nthdata(repIndex->ranks, rep->current_rank);
                if (!IS_VALID(rank) || !IS_SET(rank->flags, REPUTATION_RANK_PARAGON))
                    return;

                info->progs->lastreturn = UMIN(rep->paragon_level, amount);
                rep->paragon_level -= info->progs->lastreturn;

                sprintf(rep_name, "%s (%ld#%ld) paragon levels", repIndex->name, repIndex->area->uid, repIndex->vnum);
                field_name = rep_name;
            } else {
                long total_given = 0;
                if (!gain_reputation(victim, repIndex, -amount, NULL, &total_given, false))
                    return;

                sprintf(rep_name, "%s (%ld#%ld) reputation points", repIndex->name, repIndex->area->uid, repIndex->vnum);
                field_name = rep_name;
                info->progs->lastreturn = -total_given;
            }

        } else if( !str_prefix(field, "silver") ) {
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
            int deducted = 0;
            script_adjust_quest_points(victim, -amount, &deducted);
            info->progs->lastreturn = -deducted;
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

    bool show = true;

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
    strlcpy(field, arg->d.str, sizeof(field));

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

            if( !IS_SET(mob->pet->pIndexData->act[0], ACT_PET) )
                REMOVE_BIT(mob->pet->act[0], ACT_PET);

            if( !IS_SET(mob->pet->pIndexData->affected_by[0], AFF_CHARM) )
                REMOVE_BIT(mob->pet->affected_by[0], AFF_CHARM);

            // This will not ungroup/unfollow
            mob->pet->comm &= ~(COMM_NOTELL|COMM_NOCHANNELS);
            mob->pet = NULL;
        }
        else if(!str_prefix(field, "master") || !str_cmp(field, "follower"))
        {
            if( mob->master == NULL ) return;

            if( mob->master->pet == mob )
            {
                if( !IS_SET(mob->pet->pIndexData->act[0], ACT_PET) )
                    REMOVE_BIT(mob->pet->act[0], ACT_PET);

                if( !IS_SET(mob->pet->pIndexData->affected_by[0], AFF_CHARM) )
                    REMOVE_BIT(mob->pet->affected_by[0], AFF_CHARM);

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
        else if( !str_prefix(field, "reply") )
        {
            if (mob) mob->reply = NULL;
        }
    }

    info->progs->lastreturn = 1;
}

// DUNGEONCOMPLETE $DUNGEON
SCRIPT_CMD(scriptcmd_dungeoncomplete)
{
    if(!expand_argument(info,argument,arg))
        return;

    if( arg->type == ENT_DUNGEON ) {
        if( !IS_SET(arg->d.dungeon->flags, DUNGEON_COMPLETED) )
        {
            p_percent2_trigger(NULL, NULL, arg->d.dungeon, NULL, NULL, NULL, NULL, NULL, TRIG_COMPLETED,NULL);

            SET_BIT(arg->d.dungeon->flags, DUNGEON_COMPLETED);
        }
    }
}

// DUNGEONCOMMENCE $DUNGEON
SCRIPT_CMD(scriptcmd_dungeoncommence)
{
    if(!expand_argument(info,argument,arg))
        return;

    if( arg->type == ENT_DUNGEON ) {
        if( !IS_SET(arg->d.dungeon->flags, DUNGEON_COMMENCED) )
        {
            SET_BIT(arg->d.dungeon->flags, DUNGEON_COMMENCED);

            p_percent2_trigger(NULL, NULL, arg->d.dungeon, NULL, NULL, NULL, NULL, NULL, TRIG_DUNGEON_COMMENCED, NULL);
        }
    }
}

// DUNGEONFAILURE $DUNGEON
SCRIPT_CMD(scriptcmd_dungeonfailure)
{
    if(!expand_argument(info,argument,arg))
        return;

    if( arg->type == ENT_DUNGEON ) {
        if( !IS_SET(arg->d.dungeon->flags, DUNGEON_FAILED) )
        {
            SET_BIT(arg->d.dungeon->flags, DUNGEON_FAILED);

            p_percent2_trigger(NULL, NULL, arg->d.dungeon, NULL, NULL, NULL, NULL, NULL, TRIG_FAILED, NULL);
        }
    }
}


//////////////////////////////////////
// E

// ECHOAT $MOBILE|ROOM|$AREA|INSTANCE|DUNGEON|CHURCH string
SCRIPT_CMD(scriptcmd_echoat)
{
    char *rest;
    CHAR_DATA *victim = NULL;
    ROOM_INDEX_DATA *room = NULL;
    AREA_DATA *area = NULL;
    INSTANCE *instance = NULL;
    DUNGEON *dungeon = NULL;
    CHURCH_DATA *church = NULL;

    if(!info) return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;


    switch(arg->type) {
    case ENT_STRING: victim = script_get_char_room(info, arg->d.str, false); break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    case ENT_ROOM: room = arg->d.room; break;
    case ENT_AREA: area = arg->d.area; break;
    case ENT_INSTANCE: instance = arg->d.instance; break;
    case ENT_DUNGEON: dungeon = arg->d.dungeon; break;
    case ENT_CHURCH: church = arg->d.church; break;
    default: return;
    }

    if ((!victim || !victim->in_room) && !room && !area && !instance && !dungeon && !church)
        return;

    // Expand the message
    BUFFER *buffer = new_buf();
    expand_string(info,rest,buffer);

    if( !IS_NULLSTR(buffer->string) )
    {
        int i = 0;
        i = strlen(buffer->string);
        if (buffer->string[i-2] != '\n' && !victim)
            strcat(buffer->string,"\n\r");
        
        if( IS_VALID(instance) )
        {
            instance_echo(instance, buffer->string);
        }
        else if( IS_VALID(dungeon) )
        {
            dungeon_echo(dungeon, buffer->string);
        }
        else if ( church )
        {
            church_echo(church, buffer->string);
        }
        else if( area )
        {
            area_echo(area, buffer->string);
        }
        else if( room )
        {
            room_echo(room, buffer->string);
        }
        else
            act(buffer->string, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    }
    free_buf(buffer);
}

// QUEST <subcommand> [args]
// Initial namespace dispatcher for quest-context helper commands.
// Example: QUEST ECHOAT <message>
static QUEST_DATA *scriptcmd_quest_default_run(SCRIPT_VARINFO *info)
{
    if (!info)
        return NULL;

    if (info->quest)
        return info->quest;

    if (info->ch && !IS_NPC(info->ch))
        return quest_runtime_get_focused_run(info->ch);

    return NULL;
}

static QUEST_DATA *scriptcmd_quest_resolve_run(SCRIPT_VARINFO *info, SCRIPT_PARAM *arg, char **argument)
{
    QUEST_DATA *run;
    char *rest;

    run = scriptcmd_quest_default_run(info);
    if (!argument || IS_NULLSTR(*argument))
        return run;

    rest = expand_argument(info, *argument, arg);
    if (!rest)
        return run;

    if (arg->type == ENT_QUEST)
    {
        *argument = rest;
        return arg->d.quest;
    }

    if (arg->type == ENT_NUMBER && info && info->ch && !IS_NPC(info->ch))
    {
        QUEST_DATA *by_id = quest_runtime_get_run_by_id(info->ch, arg->d.num);
        if (by_id)
        {
            *argument = rest;
            return by_id;
        }
    }

    return run;
}

SCRIPT_CMD(scriptcmd_quest)
{
    char subcmd[MIL];
    QUEST_DATA *run;
    char *rest;

    if (!info || IS_NULLSTR(argument))
        return;

    if (info->progs)
        info->progs->lastreturn = 0;

    argument = one_argument(argument, subcmd);
    if (IS_NULLSTR(subcmd))
        return;

    if (!str_prefix(subcmd, "echoat"))
    {
        scriptcmd_questechoat(info, argument, arg);
        return;
    }

    if (!str_prefix(subcmd, "objectivecomplete") || !str_prefix(subcmd, "objective_completed") || !str_prefix(subcmd, "objcomplete"))
    {
        int objective_id;

        rest = argument;
        run = scriptcmd_quest_resolve_run(info, arg, &rest);
        if (!run)
            return;

        if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_NUMBER)
            return;

        objective_id = arg->d.num;
        if (objective_id < 1)
            return;

        if (quest_runtime_complete_objective(run, objective_id) && info->progs)
            info->progs->lastreturn = 1;
        return;
    }

    if (!str_prefix(subcmd, "objectivefail") || !str_prefix(subcmd, "objective_failed") || !str_prefix(subcmd, "objfail"))
    {
        int objective_id;

        rest = argument;
        run = scriptcmd_quest_resolve_run(info, arg, &rest);
        if (!run)
            return;

        if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_NUMBER)
            return;

        objective_id = arg->d.num;
        if (objective_id < 1)
            return;

        if (quest_runtime_fail_objective(run, objective_id, "objective_failed") && info->progs)
            info->progs->lastreturn = 1;
        return;
    }

    if (!str_prefix(subcmd, "objectiveadd") || !str_prefix(subcmd, "objective_add") || !str_prefix(subcmd, "objadd"))
    {
        int objective_id;
        int delta = 1;

        rest = argument;
        run = scriptcmd_quest_resolve_run(info, arg, &rest);
        if (!run)
            return;

        if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_NUMBER)
            return;

        objective_id = arg->d.num;
        if (objective_id < 1)
            return;

        if (*rest)
        {
            if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_NUMBER)
                return;
            delta = arg->d.num;
        }

        if (quest_runtime_update_objective_progress(run, objective_id, delta) && info->progs)
            info->progs->lastreturn = 1;
        return;
    }

    if (!str_prefix(subcmd, "stageset") || !str_prefix(subcmd, "stage_set"))
    {
        int stage_id;

        rest = argument;
        run = scriptcmd_quest_resolve_run(info, arg, &rest);
        if (!run)
            return;

        if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_NUMBER)
            return;

        stage_id = arg->d.num;
        if (stage_id < 1)
            return;

        if (quest_runtime_set_stage(run, stage_id) && info->progs)
            info->progs->lastreturn = 1;
        return;
    }

    if (!str_prefix(subcmd, "stageadvance") || !str_prefix(subcmd, "stage_advance"))
    {
        QUEST_STAGE_INDEX_V2_DATA *stage;
        bool advanced;

        rest = argument;
        run = scriptcmd_quest_resolve_run(info, arg, &rest);
        if (!run)
            return;

        stage = quest_runtime_get_current_stage(run);
        if (stage && stage->next_stage_id > 0)
            advanced = quest_runtime_set_stage(run, stage->next_stage_id);
        else
            advanced = quest_runtime_try_advance_stage(run);

        if (advanced && info->progs)
            info->progs->lastreturn = 1;
        return;
    }

    if (!str_prefix(subcmd, "complete"))
    {
        rest = argument;
        run = scriptcmd_quest_resolve_run(info, arg, &rest);
        if (!run)
            return;

        if (quest_runtime_complete_run(run, "forced") && info->progs)
            info->progs->lastreturn = 1;
        return;
    }

    if (!str_prefix(subcmd, "fail"))
    {
        char reason_buf[MIL];
        const char *reason = "failed";

        rest = argument;
        run = scriptcmd_quest_resolve_run(info, arg, &rest);
        if (!run)
            return;

        if (*rest)
        {
            if (!(rest = expand_argument(info, rest, arg)))
                return;

            if (arg->type == ENT_STRING)
                reason = arg->d.str;
            else if (arg->type == ENT_NUMBER)
            {
                sprintf(reason_buf, "%d", arg->d.num);
                reason = reason_buf;
            }
        }

        if (quest_runtime_fail_run(run, QUEST_RUN_STATUS_FAILED, reason) && info->progs)
            info->progs->lastreturn = 1;
        return;
    }
}

// QUESTECHOAT string
// Sends the expanded message to all currently-online recipients in the quest runtime scope.
SCRIPT_CMD(scriptcmd_questechoat)
{
    QUEST_DATA *run;
    BUFFER *buffer;
    CHAR_DATA *vch;
    ITERATOR it;
    int len;

    if (!info || !info->quest)
        return;

    run = info->quest;
    buffer = new_buf();
    expand_string(info, argument, buffer);

    if (IS_NULLSTR(buffer->string)) {
        free_buf(buffer);
        return;
    }

    len = strlen(buffer->string);
    if (len > 0 && buffer->string[len - 1] != '\n' && buffer->string[len - 1] != '\r')
        add_buf(buffer, "\n\r");

    iterator_start(&it, loaded_chars);
    while ((vch = (CHAR_DATA *)iterator_nextdata(&it)) != NULL)
    {
        if (IS_NPC(vch))
            continue;

        switch (run->target_scope)
        {
        case QUEST_TARGET_SCOPE_CHARACTER:
        case QUEST_TARGET_SCOPE_GROUP:
            if (!uid_match(run->scope_owner_id, vch->id))
                continue;
            break;

        case QUEST_TARGET_SCOPE_CHURCH:
            if (run->scope_owner_uid <= 0 || !vch->church || vch->church->uid != run->scope_owner_uid)
                continue;
            break;

        default:
            continue;
        }

        send_to_char(buffer->string, vch);
    }
    iterator_stop(&it);

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

    bool fSilent = false;


    info->progs->lastreturn = 0;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    switch(arg->type) {
    case ENT_STRING: victim = script_get_char_room(info, arg->d.str, false); break;
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
            fSilent = arg->d.boolean == true;
        } else {
            switch(arg->type) {
            case ENT_STRING: victim = script_get_char_room(info, arg->d.str, false); break;
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
                    fSilent = arg->d.boolean == true;
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
// $INTERRUPT - flag (default false) on whether to interrupt the player.
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
    case ENT_STRING: target = script_get_char_room(info, arg->d.str, true); break;
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
            if( arg->d.boolean ) busy = false;
        }
        else if( arg->type == ENT_NUMBER )
        {
            if( arg->d.num != 0 ) busy = false;
        }
        else if( arg->type == ENT_STRING )
        {
            if( !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "true") )
                busy = false;
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
    bool conceal = false, pursue = true;
    char fleedata[MIL];
    char *fleearg = str_empty;

    if(!info) return;
    info->progs->lastreturn = -1;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    target = info->mob;
    switch(arg->type) {
    case ENT_STRING: target = script_get_char_room(info, arg->d.str, true); break;
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

SCRIPT_CMD(scriptcmd_goto)
{
    ROOM_INDEX_DATA *dest = NULL;

    if(!info)
        return;

    if(info->mob) {
        if(!info->mob->in_room || PROG_FLAG(info->mob,PROG_AT))
            return;

        if(!argument[0]) {
            pbugf(LOG_SCRIPTS, "Mpgoto - No argument from vnum %d.", VNUM(info->mob));
            return;
        }

        mp_getlocation(info, argument, &dest);

        if(!dest) {
            pbugf(LOG_SCRIPTS, "Mpgoto - Bad location from vnum %d.", VNUM(info->mob));
            return;
        }

        if(info->mob->fighting)
            stop_fighting(info->mob, true);

        char_from_room(info->mob);
        if(dest->wilds)
            char_to_vroom(info->mob, dest->wilds, dest->x, dest->y);
        else
            char_to_room(info->mob, dest);
        return;
    }

    if(info->obj) {
        if(!obj_room(info->obj) || PROG_FLAG(info->obj,PROG_AT))
            return;

        if(!argument[0]) {
            pbugf(LOG_SCRIPTS, "Opgoto - No argument from vnum %d.", VNUM(info->obj));
            return;
        }

        op_getlocation(info, argument, &dest);

        if(!dest) {
            pbugf(LOG_SCRIPTS, "Opgoto - Bad location from vnum %d.", VNUM(info->obj));
            return;
        }

        if(info->obj->in_obj)
            obj_from_obj(info->obj);
        else if(info->obj->carried_by)
            obj_from_char(info->obj);
        else if(info->obj->in_room)
            obj_from_room(info->obj);

        obj_to_room(info->obj, dest);
        return;
    }

    if(info->token) {
        if(!info->token->player || !info->token->player->in_room)
            return;

        if(!argument[0]) {
            pbugf(LOG_SCRIPTS, "Tpgoto - No argument from vnum %d.", VNUM(info->token));
            return;
        }

        tp_getlocation(info, argument, &dest);

        if(!dest) {
            pbugf(LOG_SCRIPTS, "Tpgoto - Bad location from vnum %d.", VNUM(info->token));
            return;
        }

        if(info->token->player->fighting)
            stop_fighting(info->token->player, true);

        char_from_room(info->token->player);
        char_to_room(info->token->player, dest);
    }
}

SCRIPT_CMD(scriptcmd_force)
{
    char *rest;
    CHAR_DATA *victim = NULL, *next;
    bool fAll = false, forced;
    ROOM_INDEX_DATA *source_room = NULL;

    if(!info)
        return;

    if(info->mob) {
        if(!info->mob)
            return;

        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "MpForce - Error in parsing from vnum %ld.", VNUM(info->mob));
            return;
        }

        source_room = info->mob->in_room;
        switch(arg->type) {
        case ENT_STRING:
            if(!str_cmp(arg->d.str,"all")) fAll = true;
            else victim = get_char_room(info->mob, NULL, arg->d.str);
            break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: break;
        }

        if(!fAll && !victim) {
            pbugf(LOG_SCRIPTS, "MpForce - Null victim from vnum %ld.", VNUM(info->mob));
            return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buf_string(buffer)[0] == '\0') {
            pbugf(LOG_SCRIPTS,"MpForce - Error in parsing from vnum %ld.", VNUM(info->mob));
            free_buf(buffer);
            return;
        }

        forced = forced_command;
        if(fAll) {
            for(victim = source_room ? source_room->people : NULL; victim; victim = next) {
                next = victim->next_in_room;
                if(get_staff_rank(victim) < get_staff_rank(info->mob)
                && can_see(info->mob, victim)
                && (IS_NPC(victim) || !IS_IMMORTAL(victim))) {
                    forced_command = true;
                    interpret(victim, buf_string(buffer));
                }
            }
        } else {
            if(victim == info->mob) {
                free_buf(buffer);
                return;
            }
            if(!IS_NPC(victim) && IS_IMMORTAL(victim)) {
                free_buf(buffer);
                return;
            }

            forced_command = true;
            interpret(victim, buf_string(buffer));
        }

        forced_command = forced;
        free_buf(buffer);
        return;
    }

    if(info->obj) {
        if(!info->obj)
            return;

        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "OpForce - Error in parsing from vnum %ld.", VNUM(info->obj));
            return;
        }

        source_room = obj_room(info->obj);
        switch(arg->type) {
        case ENT_STRING:
            if(!str_cmp(arg->d.str,"all")) fAll = true;
            else victim = get_char_room(NULL, source_room, arg->d.str);
            break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: break;
        }

        if(!fAll && !victim) {
            pbugf(LOG_SCRIPTS, "OpForce - Null victim from vnum %ld.", VNUM(info->obj));
            return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);
        if(buffer->string[0] != '\0') {
            forced = forced_command;

            if(fAll) {
                for(victim = source_room ? source_room->people : NULL; victim; victim = next) {
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
        return;
    }

    if(info->room) {
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "RpForce - Error in parsing from vnum %ld.", info->room->vnum);
            return;
        }

        source_room = info->room;
        switch(arg->type) {
        case ENT_STRING:
            if(!str_cmp(arg->d.str,"all")) fAll = true;
            else victim = get_char_room(NULL, source_room, arg->d.str);
            break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: break;
        }

        if(!fAll && !victim) {
            pbugf(LOG_SCRIPTS, "RpForce - Null victim from vnum %ld.", info->room->vnum);
            return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0') {
            forced = forced_command;

            if(fAll) {
                for(victim = source_room ? source_room->people : NULL; victim; victim = next) {
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
        return;
    }

    if(info->token) {
        if(!info->token)
            return;

        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS,"TpForce - Error in parsing from vnum %ld.", VNUM(info->token));
            return;
        }

        source_room = token_room(info->token);
        switch(arg->type) {
        case ENT_STRING:
            if(!str_cmp(arg->d.str,"all")) fAll = true;
            else victim = get_char_room(NULL, source_room, arg->d.str);
            break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: break;
        }

        if(!fAll && !victim) {
            pbugf(LOG_SCRIPTS,"TpForce - Null victim from vnum %ld.", VNUM(info->token));
            return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0') {
            forced = forced_command;

            if(fAll) {
                for(victim = source_room ? source_room->people : NULL; victim; victim = next) {
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
}

SCRIPT_CMD(scriptcmd_gforce)
{
    char *rest;
    CHAR_DATA *victim = NULL, *vch, *next;

    if(!info)
        return;

    if(info->mob) {
        if(!info->mob)
            return;

        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "MpGforce - Error in parsing from vnum %ld.", VNUM(info->mob));
            return;
        }

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: break;
        }

        if (!victim) {
            pbugf(LOG_SCRIPTS, "MpGforce - Null victim from vnum %ld.", VNUM(info->mob));
            return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,argument,buffer);
        if(buf_string(buffer)[0] == '\0') {
            pbugf(LOG_SCRIPTS, "MpGforce - Error in parsing from vnum %ld.", VNUM(info->mob));
            free_buf(buffer);
            return;
        }

        for (vch = info->mob->in_room->people; vch; vch = next) {
            next = vch->next_in_room;
            if (is_same_group(victim,vch) &&
                get_staff_rank(vch) < get_staff_rank(info->mob) &&
                can_see(info->mob, vch) &&
                (IS_NPC(vch) || !IS_IMMORTAL(vch)))
                interpret(vch, buf_string(buffer));
        }

        free_buf(buffer);
        return;
    }

    if(info->obj) {
        if(!obj_room(info->obj))
            return;

        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "OpGforce - Error in parsing from vnum %ld.", VNUM(info->obj));
            return;
        }

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, obj_room(info->obj), arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: break;
        }

        if (!victim) {
            pbugf(LOG_SCRIPTS, "OpGforce - Null victim from vnum %ld.", VNUM(info->obj));
            return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);
        if(buffer->string[0] != '\0') {
            for (vch = obj_room(info->obj)->people; vch; vch = next) {
                next = vch->next_in_room;
                if (is_same_group(victim,vch))
                    interpret(vch, buffer->string);
            }
        }
        free_buf(buffer);
        return;
    }

    if(info->room) {
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "RpGforce - Error in parsing from vnum %ld.", info->room->vnum);
            return;
        }

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, info->room, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: break;
        }

        if (!victim) {
            pbugf(LOG_SCRIPTS, "RpGforce - Null victim from vnum %ld.", info->room->vnum);
            return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if( buffer->string[0] != '\0' ) {
            for (vch = info->room->people; vch; vch = next) {
                next = vch->next_in_room;
                if (is_same_group(victim,vch))
                    interpret(vch, buffer->string);
            }
        }

        free_buf(buffer);
        return;
    }

    if(info->token) {
        if(!token_room(info->token))
            return;

        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS,"TpGforce - Error in parsing from vnum %ld.", VNUM(info->token));
            return;
        }

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, token_room(info->token), arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: break;
        }

        if (!victim) {
            pbugf(LOG_SCRIPTS,"TpGforce - Null victim from vnum %ld.", VNUM(info->token));
            return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if( buffer->string[0] != '\0' ) {
            for (vch = token_room(info->token)->people; vch; vch = next) {
                next = vch->next_in_room;
                if (is_same_group(victim,vch))
                    interpret(vch, buffer->string);
            }
        }

        free_buf(buffer);
    }
}

SCRIPT_CMD(scriptcmd_gtransfer)
{
    char buf[MIL], buf2[MIL], buf3[MIL], *rest;
    CHAR_DATA *victim, *vch,*next;
    ROOM_INDEX_DATA *dest;
    bool all = false, force = false, quiet = false;
    int mode;

    if(!info)
        return;

    if(info->mob) {
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "MpGtransfer - Bad syntax from vnum %ld.", VNUM(info->mob));
            return;
        }

        switch(arg->type) {
        case ENT_STRING: victim = get_char_world(info->mob, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }


        if (!victim) {
            pbugf(LOG_SCRIPTS, "MpGtransfer - Null victim from vnum %ld.", VNUM(info->mob));
            return;
        }

        if (!victim->in_room) return;

        if(!(argument = mp_getlocation(info, rest, &dest))) {
            pbugf(LOG_SCRIPTS, "MpGtransfer - Bad syntax from vnum %ld.", VNUM(info->mob));
            return;
        }

        if(!dest) {
            pbugf(LOG_SCRIPTS, "MpGtransfer - Bad location from vnum %d.", VNUM(info->mob));
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
        return;
    }

    if(info->obj) {
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "OpGtransfer - Bad syntax from vnum %ld.", VNUM(info->obj));
            return;
        }

        switch(arg->type) {
        case ENT_STRING: victim = get_char_world(NULL, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }


        if (!victim) {
            pbugf(LOG_SCRIPTS, "OpGtransfer - Null victim from vnum %ld.", VNUM(info->obj));
            return;
        }

        if (!victim->in_room) return;

        if(!(argument = op_getlocation(info, rest, &dest))) {
            pbugf(LOG_SCRIPTS, "OpGtransfer - Bad syntax from vnum %ld.", VNUM(info->obj));
            return;
        }

        if(!dest) {
            pbugf(LOG_SCRIPTS, "OpGtransfer - Bad location from vnum %d.", VNUM(info->obj));
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
        return;
    }

    if(info->room) {
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "RpGtransfer - Bad syntax from vnum %ld.", info->room->vnum);
            return;
        }

        switch(arg->type) {
        case ENT_STRING: victim = get_char_world(NULL, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }


        if (!victim) {
            pbugf(LOG_SCRIPTS, "RpGtransfer - Null victim from vnum %ld.", info->room->vnum);
            return;
        }

        if (!victim->in_room) return;

        if(!(argument = rp_getlocation(info, rest, &dest))) {
            pbugf(LOG_SCRIPTS, "RpGtransfer - Bad syntax from vnum %ld.", info->room->vnum);
            return;
        }

        if(!dest) {
            pbugf(LOG_SCRIPTS, "RpGtransfer - Bad location from vnum %d.", info->room->vnum);
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
            if (!IS_NPC(vch) && is_same_group(victim,vch)) {
                if (!all && vch->position != POS_STANDING) continue;
                if (!force && room_is_private(dest, info->mob)) break;
                do_mob_transfer(vch,dest,quiet,mode);
            }
        }
        return;
    }

    if(info->token) {
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS,"TpGtransfer - Bad syntax from vnum %ld.", VNUM(info->token));
            return;
        }

        switch(arg->type) {
        case ENT_STRING: victim = get_char_world(NULL, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }


        if (!victim) {
            pbugf(LOG_SCRIPTS,"TpGtransfer - Null victim from vnum %ld.", VNUM(info->token));
            return;
        }

        if (!victim->in_room) return;

        if(!(argument = tp_getlocation(info, rest, &dest))) {
            pbugf(LOG_SCRIPTS,"TpGtransfer - Bad syntax from vnum %ld.", VNUM(info->token));
            return;
        }

        if(!dest) {
            pbugf(LOG_SCRIPTS,"TpGtransfer - Bad location from vnum %d.", VNUM(info->token));
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

        for (vch = token_room(info->token)->people; vch; vch = next) {
            next = vch->next_in_room;
            if (!IS_NPC(vch) && is_same_group(victim,vch)) {
                if (!all && vch->position != POS_STANDING) continue;
                if (!force && room_is_private(dest, info->mob)) break;
                do_mob_transfer(vch,dest,quiet,mode);
            }
        }
    }
}

SCRIPT_CMD(scriptcmd_transfer)
{
    char buf[MIL], buf2[MIL], *rest;
    CHAR_DATA *victim = NULL, *vnext;
    ROOM_INDEX_DATA *dest = NULL;
    ROOM_INDEX_DATA *source_room = NULL;
    bool all = false;
    bool force = false;
    bool quiet = false;
    int mode;

    if(!info)
        return;

    if(info->mob) {
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "MpTransfer - Bad syntax from vnum %ld.", VNUM(info->mob));
            return;
        }

        quiet = true;
        source_room = info->mob->in_room;

        switch(arg->type) {
        case ENT_STRING:
            if(!str_cmp(arg->d.str,"all")) all = true;
            else victim = get_char_world(info->mob, arg->d.str);
            break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if(!victim && !all) {
            pbugf(LOG_SCRIPTS, "MpTransfer - Null victim from vnum %ld.", VNUM(info->mob));
            return;
        }

        argument = mp_getlocation(info, rest, &dest);

        if(!dest) {
            pbugf(LOG_SCRIPTS, "MpTransfer - Bad location from vnum %d.", VNUM(info->mob));
            return;
        }

        argument = one_argument(argument,buf);
        argument = one_argument(argument,buf2);
        force = !str_cmp(buf,"force") || !str_cmp(buf2,"force") || !str_cmp(argument,"force");
        quiet = !str_cmp(buf,"quiet") || !str_cmp(buf2,"quiet") || !str_cmp(argument,"quiet");
        mode = script_flag_value(transfer_modes, buf);
        if(mode == NO_FLAG) mode = script_flag_value(transfer_modes, buf2);
        if(mode == NO_FLAG) mode = script_flag_value(transfer_modes, argument);
        if(mode == NO_FLAG) mode = TRANSFER_MODE_SILENT;

        if(all) {
            for(victim = source_room ? source_room->people : NULL; victim; victim = vnext) {
                vnext = victim->next_in_room;
                if(PROG_FLAG(victim,PROG_AT)) continue;
                if(!IS_NPC(victim)) {
                    if(!force && room_is_private(dest, info->mob)) break;
                    do_mob_transfer(victim,dest,quiet,mode);
                }
            }
            return;
        }

        if(!force && room_is_private(dest,info->mob))
            return;

        if(PROG_FLAG(victim,PROG_AT))
            return;

        do_mob_transfer(victim,dest,quiet,mode);
        return;
    }

    if(info->obj) {
        if(!obj_room(info->obj))
            return;

        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "OpTransfer - Bad syntax from vnum %ld.", VNUM(info->obj));
            return;
        }

        source_room = obj_room(info->obj);

        switch(arg->type) {
        case ENT_STRING:
            if(!str_cmp(arg->d.str,"all")) all = true;
            else victim = get_char_world(NULL, arg->d.str);
            break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if(!victim && !all) {
            pbugf(LOG_SCRIPTS, "OpTransfer - Null victim from vnum %ld.", VNUM(info->obj));
            return;
        }

        argument = op_getlocation(info, rest, &dest);

        if(!dest) {
            pbugf(LOG_SCRIPTS, "OpTransfer - Bad location from vnum %d.", VNUM(info->obj));
            return;
        }

        argument = one_argument(argument,buf);
        argument = one_argument(argument,buf2);
        force = !str_cmp(buf,"force") || !str_cmp(buf2,"force") || !str_cmp(argument,"force");
        quiet = !str_cmp(buf,"quiet") || !str_cmp(buf2,"quiet") || !str_cmp(argument,"quiet");
        mode = script_flag_value(transfer_modes, buf);
        if(mode == NO_FLAG) mode = script_flag_value(transfer_modes, buf2);
        if(mode == NO_FLAG) mode = script_flag_value(transfer_modes, argument);
        if(mode == NO_FLAG) mode = TRANSFER_MODE_SILENT;

        if(all) {
            for(victim = source_room ? source_room->people : NULL; victim; victim = vnext) {
                vnext = victim->next_in_room;
                if(PROG_FLAG(victim,PROG_AT)) continue;
                if(!IS_NPC(victim)) {
                    if(!force && room_is_private(dest, NULL)) break;
                    do_mob_transfer(victim,dest,quiet,mode);
                }
            }
            return;
        }

        if(!force && room_is_private(dest, NULL))
            return;

        if(PROG_FLAG(victim,PROG_AT))
            return;

        do_mob_transfer(victim,dest,quiet,mode);
        return;
    }

    if(info->room) {
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "RpTransfer - Bad syntax from vnum %ld.", info->room->vnum);
            return;
        }

        source_room = info->room;

        switch(arg->type) {
        case ENT_STRING:
            if(!str_cmp(arg->d.str,"all")) all = true;
            else victim = get_char_world(NULL, arg->d.str);
            break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if(!victim && !all) {
            pbugf(LOG_SCRIPTS, "RpTransfer - Null victim from vnum %ld.", info->room->vnum);
            return;
        }

        argument = rp_getlocation(info, rest, &dest);

        if(!dest) {
            pbugf(LOG_SCRIPTS, "RpTransfer - Bad location from vnum %d.", info->room->vnum);
            return;
        }

        argument = one_argument(argument,buf);
        argument = one_argument(argument,buf2);
        force = !str_cmp(buf,"force") || !str_cmp(buf2,"force") || !str_cmp(argument,"force");
        quiet = !str_cmp(buf,"quiet") || !str_cmp(buf2,"quiet") || !str_cmp(argument,"quiet");
        mode = script_flag_value(transfer_modes, buf);
        if(mode == NO_FLAG) mode = script_flag_value(transfer_modes, buf2);
        if(mode == NO_FLAG) mode = script_flag_value(transfer_modes, argument);
        if(mode == NO_FLAG) mode = TRANSFER_MODE_SILENT;

        if(all) {
            for(victim = source_room ? source_room->people : NULL; victim; victim = vnext) {
                vnext = victim->next_in_room;
                if(PROG_FLAG(victim,PROG_AT)) continue;
                if(!IS_NPC(victim)) {
                    if(!force && room_is_private(dest, NULL)) break;
                    do_mob_transfer(victim,dest,quiet,mode);
                }
            }
            return;
        }

        if(!force && room_is_private(dest, NULL))
            return;

        if(PROG_FLAG(victim,PROG_AT))
            return;

        do_mob_transfer(victim,dest,quiet,mode);
        return;
    }

    if(info->token) {
        if(!token_room(info->token))
            return;

        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS,"TpTransfer - Bad syntax from vnum %ld.", VNUM(info->token));
            return;
        }

        source_room = token_room(info->token);

        switch(arg->type) {
        case ENT_STRING:
            if(!str_cmp(arg->d.str,"all")) all = true;
            else victim = get_char_world(NULL, arg->d.str);
            break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if(!victim && !all) {
            pbugf(LOG_SCRIPTS,"TpTransfer - Null victim from vnum %ld.", VNUM(info->token));
            return;
        }

        argument = tp_getlocation(info, rest, &dest);

        if(!dest) {
            pbugf(LOG_SCRIPTS,"TpTransfer - Bad location from vnum %d.", VNUM(info->token));
            return;
        }

        argument = one_argument(argument,buf);
        argument = one_argument(argument,buf2);
        force = !str_cmp(buf,"force") || !str_cmp(buf2,"force") || !str_cmp(argument,"force");
        quiet = !str_cmp(buf,"quiet") || !str_cmp(buf2,"quiet") || !str_cmp(argument,"quiet");
        mode = script_flag_value(transfer_modes, buf);
        if(mode == NO_FLAG) mode = script_flag_value(transfer_modes, buf2);
        if(mode == NO_FLAG) mode = script_flag_value(transfer_modes, argument);
        if(mode == NO_FLAG) mode = TRANSFER_MODE_SILENT;

        if(all) {
            for(victim = source_room ? source_room->people : NULL; victim; victim = vnext) {
                vnext = victim->next_in_room;
                if(PROG_FLAG(victim,PROG_AT)) continue;
                if(!IS_NPC(victim)) {
                    if(!force && room_is_private(dest, NULL)) break;
                    do_mob_transfer(victim,dest,quiet,mode);
                }
            }
            return;
        }

        if(!force && room_is_private(dest, NULL))
            return;

        if(PROG_FLAG(victim,PROG_AT))
            return;

        do_mob_transfer(victim,dest,quiet,mode);
    }
}

SCRIPT_CMD(scriptcmd_at)
{
    int sec;

    if(!info)
        return;

    if(info->mob) {
        ROOM_INDEX_DATA *orig, *location;
        char *command;
        OBJ_DATA *on;
        OBJ_DATA *pulled;
        bool remote;

        if(PROG_FLAG(info->mob,PROG_AT))
            return;

        if (!argument[0]) {
            pbugf(LOG_SCRIPTS, "Mpat - Bad argument from vnum %d.", VNUM(info->mob));
            return;
        }

        command = mp_getlocation(info, argument, &location);

        if (!location) {
            pbugf(LOG_SCRIPTS, "Mpat - No such location from vnum %d.", IS_NPC(info->mob) ? info->mob->pIndexData->vnum : 0);
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

        if(info->mob) {
            char_from_room(info->mob);
            char_to_room(info->mob, orig);
            info->mob->on = on;
            info->mob->pulled_cart = pulled;
        }
        return;
    }

    if(info->obj) {
        char *command;
        SCRIPT_VARINFO info2;
        OBJ_DATA *dummy_obj;
        ROOM_INDEX_DATA *location;

        if(!obj_room(info->obj))
            return;

        if(!(command = op_getlocation(info, argument, &location))) {
            pbugf(LOG_SCRIPTS, "OpAt: Bad syntax from vnum %ld.", VNUM(info->obj));
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

            dummy_obj->progs->target = NULL;
            dummy_obj->progs->vars = NULL;

            extract_obj(dummy_obj);
        }

        script_security = sec;
        return;
    }

    if(info->room) {
        SCRIPT_VARINFO info2;
        CHAR_DATA *target;
        int delay;
        pVARIABLE vars;
        ROOM_INDEX_DATA *dest;
        bool remote;

        if(!(argument = rp_getlocation(info, argument, &dest))) {
            pbugf(LOG_SCRIPTS, "Rpat - Bad argument from vnum %d.", info->room->vnum);
            return;
        }

        if (!dest) {
            pbugf(LOG_SCRIPTS, "Rpat - Null location from vnum %d.", info->room->vnum);
            return;
        }

        sec = script_security;
        script_security = NO_SCRIPT_SECURITY;

        remote = PROG_FLAG(dest,PROG_AT) ? true : false;

        SET_BIT(dest->progs->entity_flags,PROG_AT);

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
            REMOVE_BIT(dest->progs->entity_flags,PROG_AT);
    }
}

SCRIPT_CMD(scriptcmd_vforce)
{
    char *rest;
    int vnum = 0;
    AREA_DATA *target_area = NULL;
    AREA_DATA *context_area = NULL;
    WNUM target_wnum = wnum_zero;
    CHAR_DATA *vch, *next;

    if(!info)
        return;

    if(info->mob) {
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "MpVforce - Error in parsing from vnum %ld.", VNUM(info->mob));
            return;
        }

        context_area = get_area_from_scriptinfo(info);

        switch(arg->type) {
        case ENT_STRING:
            if (!IS_NULLSTR(arg->d.str)
            && parse_widevnum(arg->d.str, context_area, &target_wnum)
            && target_wnum.vnum > 0) {
                vnum = target_wnum.vnum;
                target_area = target_wnum.pArea;
            }
            break;
        case ENT_NUMBER: vnum = arg->d.num; break;
        default: break;
        }

        if (vnum < 1) {
            pbugf(LOG_SCRIPTS, "MpVforce - Invalid vnum from vnum %ld.", VNUM(info->mob));
            return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);
        if(buf_string(buffer)[0] == '\0') {
            pbugf(LOG_SCRIPTS, "MpGforce - Error in parsing from vnum %ld.", VNUM(info->mob));
            free_buf(buffer);
            return;
        }

        for (vch = info->mob->in_room->people; vch; vch = next) {
            next = vch->next_in_room;
            if (IS_NPC(vch) && vch->pIndexData->vnum == vnum &&
                (!target_area || vch->pIndexData->area == target_area) &&
                get_staff_rank(vch) < get_staff_rank(info->mob) &&
                can_see(info->mob, vch) &&
                (IS_NPC(vch) || !IS_IMMORTAL(vch)))
                interpret(vch, buf_string(buffer));
        }
        free_buf(buffer);
        return;
    }

    if(info->obj) {
        if(!obj_room(info->obj))
            return;

        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "OpVforce - Error in parsing from vnum %ld.", VNUM(info->obj));
            return;
        }

        context_area = get_area_from_scriptinfo(info);

        switch(arg->type) {
        case ENT_STRING:
            if (parse_widevnum(arg->d.str, context_area, &target_wnum) && target_wnum.pArea) {
                vnum = target_wnum.vnum;
                target_area = target_wnum.pArea;
            }
            break;
        case ENT_NUMBER: vnum = arg->d.num; break;
        default: break;
        }

        if (vnum < 1) {
            pbugf(LOG_SCRIPTS, "OpVforce - Invalid vnum from vnum %ld.", VNUM(info->obj));
            return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);
        if(buffer->string[0] != '\0') {
            for (vch = obj_room(info->obj)->people; vch; vch = next) {
                next = vch->next_in_room;
                if (IS_NPC(vch) &&  vch->pIndexData->vnum == vnum &&
                    (!target_area || vch->pIndexData->area == target_area) &&
                    !vch->fighting)
                    interpret(vch, buffer->string);
            }
        }
        free_buf(buffer);
        return;
    }

    if(info->room) {
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "RpVforce - Error in parsing from vnum %ld.", info->room->vnum);
            return;
        }

        context_area = get_area_from_scriptinfo(info);

        switch(arg->type) {
        case ENT_STRING:
            if (!IS_NULLSTR(arg->d.str)
            && parse_widevnum(arg->d.str, context_area, &target_wnum)
            && target_wnum.vnum > 0) {
                vnum = target_wnum.vnum;
                target_area = target_wnum.pArea;
            }
            break;
        case ENT_NUMBER: vnum = arg->d.num; break;
        default: break;
        }

        if (vnum < 1) {
            pbugf(LOG_SCRIPTS, "RpVforce - Invalid vnum from vnum %ld.", info->room->vnum);
            return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if( buffer->string[0] != '\0' ) {
            for (vch = info->room->people; vch; vch = next) {
                next = vch->next_in_room;
                if (IS_NPC(vch) &&  vch->pIndexData->vnum == vnum &&
                    (!target_area || vch->pIndexData->area == target_area) &&
                    !vch->fighting)
                    interpret(vch, buffer->string);
            }
        }

        free_buf(buffer);
        return;
    }

    if(info->token) {
        if(!token_room(info->token))
            return;

        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS,"TpVforce - Error in parsing from vnum %ld.", VNUM(info->token));
            return;
        }

        context_area = get_area_from_scriptinfo(info);

        switch(arg->type) {
        case ENT_STRING:
            if (parse_widevnum(arg->d.str, context_area, &target_wnum) && target_wnum.pArea) {
                vnum = target_wnum.vnum;
                target_area = target_wnum.pArea;
            }
            break;
        case ENT_NUMBER:
            vnum = arg->d.num;
            break;
        default: break;
        }

        if (vnum < 1) {
            pbugf(LOG_SCRIPTS,"TpVforce - Invalid vnum from vnum %ld.", VNUM(info->token));
            return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if( buffer->string[0] != '\0' ) {
            for (vch = token_room(info->token)->people; vch; vch = next) {
                next = vch->next_in_room;
                if (IS_NPC(vch)
                &&  vch->pIndexData->vnum == vnum
                &&  (!target_area || vch->pIndexData->area == target_area)
                &&  !vch->fighting)
                    interpret(vch, buffer->string);
            }
        }

        free_buf(buffer);
    }
}

SCRIPT_CMD(scriptcmd_forget)
{
    if(!info)
        return;

    if(info->mob) {
        info->mob->progs->target = NULL;
        return;
    }

    if(info->obj) {
        info->obj->progs->target = NULL;
        return;
    }

    if(info->room) {
        info->room->progs->target = NULL;
        return;
    }

    if(info->token)
        info->token->progs->target = NULL;
}

SCRIPT_CMD(scriptcmd_remember)
{
    CHAR_DATA *victim;

    if(!info)
        return;

    if(info->mob) {
        if(!expand_argument(info,argument,arg)) {
            pbugf(LOG_SCRIPTS, "MpRemember: Bad syntax from vnum %ld.", VNUM(info->mob));
            return;
        }

        switch(arg->type) {
        case ENT_STRING: victim = get_char_world(info->mob, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim) {
            pbugf(LOG_SCRIPTS, "MpRemember: Null victim from vnum %ld.", VNUM(info->mob));
            return;
        }

        info->mob->progs->target = victim;
        return;
    }

    if(info->obj) {
        if(!expand_argument(info,argument,arg)) {
            pbugf(LOG_SCRIPTS, "OpRemember: Bad syntax from vnum %ld.", VNUM(info->obj));
            return;
        }

        switch(arg->type) {
        case ENT_STRING: victim = get_char_world(NULL, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim) {
            pbugf(LOG_SCRIPTS, "OpRemember: Null victim from vnum %ld.", VNUM(info->obj));
            return;
        }

        info->obj->progs->target = victim;
        return;
    }

    if(info->room) {
        if(!expand_argument(info,argument,arg)) {
            pbugf(LOG_SCRIPTS, "RpRemember: Bad syntax from vnum %ld.", info->room->vnum);
            return;
        }

        switch(arg->type) {
        case ENT_STRING: victim = get_char_world(NULL, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim) {
            pbugf(LOG_SCRIPTS, "RpRemember: Null victim from vnum %ld.", info->room->vnum);
            return;
        }

        info->room->progs->target = victim;
        return;
    }

    if(info->token) {
        if(!expand_argument(info,argument,arg)) {
            pbugf(LOG_SCRIPTS,"TpRemember: Bad syntax from vnum %ld.", VNUM(info->token));
            return;
        }

        switch(arg->type) {
        case ENT_STRING: victim = get_char_world(NULL, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim) {
            pbugf(LOG_SCRIPTS,"TpRemember: Null victim from vnum %ld.", VNUM(info->token));
            return;
        }

        info->token->progs->target = victim;
    }
}

SCRIPT_CMD(scriptcmd_cancel)
{
    if(!info)
        return;

    if(info->mob) {
        info->mob->progs->delay = -1;
        return;
    }

    if(info->obj) {
        info->obj->progs->delay = -1;
        return;
    }

    if(info->room)
        info->room->progs->delay = -1;
}

SCRIPT_CMD(scriptcmd_delay)
{
    int delay = 0;

    if(!info)
        return;

    if(info->mob) {
        if(!expand_argument(info,argument,arg)) {
            pbugf(LOG_SCRIPTS, "MpDelay - Error in parsing from vnum %ld.", VNUM(info->mob));
            return;
        }

        switch(arg->type) {
        case ENT_STRING: delay = is_number(arg->d.str) ? atoi(arg->d.str) : -1; break;
        case ENT_NUMBER: delay = arg->d.num; break;
        default: delay = 0; break;
        }

        if (delay < 1) {
            pbugf(LOG_SCRIPTS, "MpDelay: invalid delay from vnum %d.", VNUM(info->mob));
            return;
        }
        info->mob->progs->delay = delay;
        return;
    }

    if(info->obj) {
        if(!expand_argument(info,argument,arg)) {
            pbugf(LOG_SCRIPTS, "OpDelay - Error in parsing from vnum %ld.", VNUM(info->obj));
            return;
        }

        switch(arg->type) {
        case ENT_STRING: delay = is_number(arg->d.str) ? atoi(arg->d.str) : -1; break;
        case ENT_NUMBER: delay = arg->d.num; break;
        default: delay = 0; break;
        }

        if (delay < 1) {
            pbugf(LOG_SCRIPTS, "OpDelay: invalid delay from vnum %d.", VNUM(info->obj));
            return;
        }
        info->obj->progs->delay = delay;
        return;
    }

    if(info->room) {
        if(!expand_argument(info,argument,arg)) {
            pbugf(LOG_SCRIPTS, "RpDelay - Error in parsing from vnum %ld.", info->room->vnum);
            return;
        }

        switch(arg->type) {
        case ENT_STRING: delay = is_number(arg->d.str) ? atoi(arg->d.str) : -1; break;
        case ENT_NUMBER: delay = arg->d.num; break;
        default: delay = 0; break;
        }

        if (delay < 1) {
            pbugf(LOG_SCRIPTS, "RpDelay: invalid delay from vnum %d.", info->room->vnum);
            return;
        }
        info->room->progs->delay = delay;
    }
}

SCRIPT_CMD(scriptcmd_dequeue)
{
    if(!info)
        return;

    if(info->mob) {
        if(!info->mob->events)
            return;

        wipe_owned_events(info->mob->events);
        return;
    }

    if(info->obj) {
        if(!info->obj->events)
            return;

        wipe_owned_events(info->obj->events);
        return;
    }

    if(info->room) {
        if(!info->room->events)
            return;

        wipe_owned_events(info->room->events);
        return;
    }

    if(info->token) {
        if(!info->token->events)
            return;

        wipe_owned_events(info->token->events);
    }
}

SCRIPT_CMD(scriptcmd_queue)
{
    char *rest;
    int delay;

    if(!info)
        return;

    if(info->mob) {
        if(!info->mob->in_room)
            return;

        if(!(rest = expand_argument(info,argument,arg)))
            return;

        switch(arg->type) {
        case ENT_NUMBER: delay = arg->d.num; break;
        case ENT_STRING: delay = atoi(arg->d.str); break;
        default:
            pbugf(LOG_SCRIPTS, "MpQueue:  missing arguments from mob vnum %d.", VNUM(info->mob));
            return;
        }

        if (delay < 0 || delay > 1000) {
            pbugf(LOG_SCRIPTS, "MpQueue:  unreasonable delay recieved from mob vnum %d.", VNUM(info->mob));
            return;
        }

        wait_function(info->mob, info, EVENT_MOBQUEUE, delay, script_interpret, rest);
        return;
    }

    if(info->obj) {
        if(PROG_FLAG(info->obj,PROG_AT))
            return;

        if(!(rest = expand_argument(info,argument,arg)))
            return;

        switch(arg->type) {
        case ENT_NUMBER: delay = arg->d.num; break;
        case ENT_STRING: delay = atoi(arg->d.str); break;
        default:
            pbugf(LOG_SCRIPTS, "OpQueue:  missing arguments from obj vnum %d.", VNUM(info->obj));
            return;
        }

        if (delay < 0 || delay > 1000) {
            pbugf(LOG_SCRIPTS, "OpQueue:  unreasonable delay recieved from obj vnum %d.", VNUM(info->obj));
            return;
        }

        wait_function(info->obj, info, EVENT_OBJQUEUE, delay, script_interpret, rest);
        return;
    }

    if(info->room) {
        if(!(rest = expand_argument(info,argument,arg)))
            return;

        switch(arg->type) {
        case ENT_NUMBER: delay = arg->d.num; break;
        case ENT_STRING: delay = atoi(arg->d.str); break;
        default:
            pbugf(LOG_SCRIPTS, "RpQueue:  missing arguments from vnum %d.", info->room->vnum);
            return;
        }

        if (delay < 0 || delay > 1000) {
            pbugf(LOG_SCRIPTS, "RpQueue:  unreasonable delay recieved from vnum %d.", info->room->vnum);
            return;
        }

        wait_function(info->room, info, EVENT_ROOMQUEUE, delay, script_interpret, rest);
        return;
    }

    if(info->token) {
        if(!(rest = expand_argument(info,argument,arg)))
            return;

        switch(arg->type) {
        case ENT_NUMBER: delay = arg->d.num; break;
        case ENT_STRING: delay = atoi(arg->d.str); break;
        default:
            pbugf(LOG_SCRIPTS,"TpQueue:  missing arguments from vnum %d.", VNUM(info->token));
            return;
        }

        if (delay < 0 || delay > 1000) {
            pbugf(LOG_SCRIPTS,"TpQueue:  unreasonable delay recieved from vnum %d.", VNUM(info->token));
            return;
        }

        wait_function(info->token, info, EVENT_TOKENQUEUE, delay, script_interpret, rest);
    }
}

// scriptwait $PLAYER NUMBER VNUM VNUM[ $ACTOR]
// - actor can be a $MOBILE, $OBJECT or $TOKEN
// - scripts must be available for the respective actor type
SCRIPT_CMD(scriptcmd_scriptwait)
{
    char *rest;
    CHAR_DATA *mob = NULL;
    int wait;
    long success, failure, pulse;
    TOKEN_DATA *actor_token = NULL;
    CHAR_DATA *actor_mob = NULL;
    OBJ_DATA *actor_obj = NULL;
    int prog_type;

    if(!info || IS_NULLSTR(argument))
        return;

    if(info->mob)
        info->mob->progs->lastreturn = 0;
    else if(info->obj)
        info->obj->progs->lastreturn = 0;
    else if(info->token)
        info->token->progs->lastreturn = 0;
    else
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(arg->type != ENT_MOBILE)
        return;

    mob = arg->d.mob;
    if(!mob)
        return;

    if(is_char_busy(mob))
        return;

    if(!*rest)
        return;

    if(!(rest = expand_argument(info,rest,arg)))
        return;

    switch(arg->type) {
    case ENT_STRING: wait = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
    case ENT_NUMBER: wait = arg->d.num; break;
    default: return;
    }

    if(!*rest)
        return;

    if(!(rest = expand_argument(info,rest,arg)))
        return;

    switch(arg->type) {
    case ENT_STRING: success = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
    case ENT_NUMBER: success = arg->d.num; break;
    default: return;
    }

    if(!*rest)
        return;

    if(!(rest = expand_argument(info,rest,arg)))
        return;

    switch(arg->type) {
    case ENT_STRING: failure = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
    case ENT_NUMBER: failure = arg->d.num; break;
    default: return;
    }

    if(!*rest)
        return;

    if(!(rest = expand_argument(info,rest,arg)))
        return;

    switch(arg->type) {
    case ENT_STRING: pulse = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
    case ENT_NUMBER: pulse = arg->d.num; break;
    default: return;
    }

    if(info->mob) {
        actor_mob = info->mob;
        prog_type = PRG_MPROG;
    } else if(info->obj) {
        actor_obj = info->obj;
        prog_type = PRG_OPROG;
    } else {
        actor_token = info->token;
        prog_type = PRG_TPROG;
    }

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

    if(!actor_mob && !actor_obj && !actor_token)
        return;

    if(success < 1 || !get_script_from_info(info, success, prog_type))
        return;
    if(failure < 1 || !get_script_from_info(info, failure, prog_type))
        return;
    if(pulse > 0 && !get_script_from_info(info, pulse, prog_type))
        return;

    wait = UMAX(wait, 1);

    mob->script_wait = wait;
    mob->script_wait_mob = actor_mob;
    mob->script_wait_obj = actor_obj;
    mob->script_wait_token = actor_token;
    if(actor_mob) {
        mob->script_wait_id[0] = actor_mob->id[0];
        mob->script_wait_id[1] = actor_mob->id[1];
    } else if(actor_obj) {
        mob->script_wait_id[0] = actor_obj->id[0];
        mob->script_wait_id[1] = actor_obj->id[1];
    } else if(actor_token) {
        mob->script_wait_id[0] = actor_token->id[0];
        mob->script_wait_id[1] = actor_token->id[1];
    }
    mob->script_wait_success = get_script_from_info(info, success, prog_type);
    mob->script_wait_failure = get_script_from_info(info, failure, prog_type);
    mob->script_wait_pulse = (pulse > 0) ? get_script_from_info(info, pulse, prog_type) : NULL;

    if(info->mob)
        info->mob->progs->lastreturn = wait;
    else if(info->obj)
        info->obj->progs->lastreturn = wait;
    else if(info->token)
        info->token->progs->lastreturn = wait;
}

// Format: PERSIST <MOBILE or OBJECT or ROOM> <STATE>
SCRIPT_CMD(scriptcmd_persist)
{
    char *rest;
    CHAR_DATA *mob = NULL;
    OBJ_DATA *obj = NULL;
    ROOM_INDEX_DATA *room = NULL;
    bool persist = false, current = false;
    const char *scope_name = NULL;
    long scope_vnum = 0;

    if(!info)
        return;

    if(info->mob) {
        scope_name = "MpPersist";
        scope_vnum = VNUM(info->mob);
    } else if(info->obj) {
        scope_name = "OpPersist";
        scope_vnum = VNUM(info->obj);
    } else if(info->room) {
        scope_name = "RpPersist";
        scope_vnum = info->room->vnum;
    } else if(info->token) {
        scope_name = "TpPersist";
        scope_vnum = VNUM(info->token);
    } else
        return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_MOBILE: mob = arg->d.mob; current = mob->persist; break;
    case ENT_OBJECT: obj = arg->d.obj; current = obj->persist; break;
    case ENT_ROOM: room = arg->d.room; current = room->persist; break;
    }

    if(!mob && !obj && !room) {
        pbugf(LOG_SCRIPTS, "%s - NULL target from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(mob) {
        if(!IS_NPC(mob))
            return;

        if(IS_SET(mob->act[1], ACT2_INSTANCE_MOB))
            return;
    }

    if(obj) {
        if(IS_SET(obj->extra[2], ITEM_INSTANCE_OBJ))
            return;
    }

    if(room) {
        if(get_blueprint_section_byroom(room->vnum))
            return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_NONE:   persist = !current; break;
    case ENT_STRING: persist = !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str,"yes") || !str_cmp(arg->d.str,"on"); break;
    default: return;
    }

    if(!current && persist && script_security < MAX_SCRIPT_SECURITY) {
        pbugf(LOG_SCRIPTS, "%s - Insufficient security to enable persistance from vnum %ld.", scope_name, scope_vnum);
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
}

SCRIPT_CMD(scriptcmd_peace)
{
    CHAR_DATA *rch;
    ROOM_INDEX_DATA *location = NULL;

    if(!info)
        return;

    if(info->mob)
        location = info->mob->in_room;
    else if(info->obj)
        location = obj_room(info->obj);
    else if(info->room)
        location = info->room;
    else if(info->token)
        location = token_room(info->token);

    if(!location)
        return;

    for (rch = location->people; rch; rch = rch->next_in_room) {
        if (rch->fighting)
            stop_fighting(rch, true);
        if (IS_NPC(rch) && IS_SET(rch->act[0],ACT_AGGRESSIVE))
            REMOVE_BIT(rch->act[0],ACT_AGGRESSIVE);
    }
}

// mob/obj/room/token chargebank <player> <gold>
SCRIPT_CMD(scriptcmd_chargebank)
{
    char *rest;
    CHAR_DATA *victim;
    ROOM_INDEX_DATA *location = NULL;
    const char *scope_name = NULL;
    long scope_vnum = 0;
    int amount = 0;

    if(!info)
        return;

    if(info->mob) {
        location = info->mob->in_room;
        scope_name = "MpChargeBank";
        scope_vnum = VNUM(info->mob);
    } else if(info->obj) {
        location = obj_room(info->obj);
        scope_name = "OpChargeBank";
        scope_vnum = VNUM(info->obj);
    } else if(info->room) {
        location = info->room;
        scope_name = "RpChargeBank";
        scope_vnum = info->room->vnum;
    } else if(info->token) {
        location = token_room(info->token);
        scope_name = "TpChargeBank";
        scope_vnum = VNUM(info->token);
    } else
        return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        if(info->mob)
            victim = get_char_room(info->mob, NULL, arg->d.str);
        else
            victim = get_char_room(NULL, location, arg->d.str);
        break;

    case ENT_MOBILE:
        victim = arg->d.mob;
        break;

    default:
        victim = NULL;
        break;
    }

    if (!victim || IS_NPC(victim)) {
        pbugf(LOG_SCRIPTS, "%s - Non-player victim from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(!expand_argument(info,rest,arg)) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        amount = atoi(arg->d.str);
        break;
    case ENT_NUMBER:
        amount = arg->d.num;
        break;
    default:
        amount = 0;
        break;
    }

    if(amount < 1 || amount > victim->pcdata->bankbalance)
        return;

    victim->pcdata->bankbalance -= amount;
}

// mob/obj/room/token wiretransfer <player> <gold>
// Limited to 1000 gold for security scopes less than 7.
SCRIPT_CMD(scriptcmd_wiretransfer)
{
    char buf[MSL], *rest;
    CHAR_DATA *victim;
    ROOM_INDEX_DATA *location = NULL;
    const char *scope_name = NULL;
    const char *scope_label = NULL;
    long scope_vnum = 0;
    long scope_room_vnum = 0;
    int amount = 0;

    if(!info)
        return;

    if(info->mob) {
        location = info->mob->in_room;
        scope_name = "MpWireTransfer";
        scope_label = "room";
        scope_vnum = VNUM(info->mob);
        scope_room_vnum = (info->mob->in_room ? info->mob->in_room->vnum : 0);
    } else if(info->obj) {
        location = obj_room(info->obj);
        scope_name = "OpWireTransfer";
        scope_label = "room";
        scope_vnum = VNUM(info->obj);
        scope_room_vnum = (info->obj->in_room ? info->obj->in_room->vnum : 0);
    } else if(info->room) {
        location = info->room;
        scope_name = "RpWireTransfer";
        scope_label = "room";
        scope_vnum = info->room->vnum;
        scope_room_vnum = info->room->vnum;
    } else if(info->token) {
        location = token_room(info->token);
        scope_name = "TpWireTransfer";
        scope_label = "token";
        scope_vnum = info->token->pIndexData ? info->token->pIndexData->vnum : VNUM(info->token);
    } else
        return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        if(info->mob)
            victim = get_char_room(info->mob, NULL, arg->d.str);
        else
            victim = get_char_room(NULL, location, arg->d.str);
        break;

    case ENT_MOBILE:
        victim = arg->d.mob;
        break;

    default:
        victim = NULL;
        break;
    }

    if (!victim || IS_NPC(victim)) {
        pbugf(LOG_SCRIPTS, "%s - Non-player victim from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(!expand_argument(info,rest,arg)) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        amount = atoi(arg->d.str);
        break;
    case ENT_NUMBER:
        amount = arg->d.num;
        break;
    default:
        amount = 0;
        break;
    }

    if(amount < 1)
        return;

    if(script_security < 7 && amount > 1000) {
        if(!str_cmp(scope_label, "room"))
            sprintf(buf, "%s logged: attempted to wire %d gold to %s in room %ld by %ld", scope_name, amount, victim->name, scope_room_vnum, scope_vnum);
        else
            sprintf(buf, "%s logged: attempted to wire %d gold to %s by token %ld", scope_name, amount, victim->name, scope_vnum);
        log_string(buf);
        amount = 1000;
    }

    victim->pcdata->bankbalance += amount;

    if(!str_cmp(scope_label, "room"))
        sprintf(buf, "%s logged: %s was wired %d gold in room %ld by %ld", scope_name, victim->name, amount, scope_room_vnum, scope_vnum);
    else
        sprintf(buf, "%s logged: %s was wired %d gold by token %ld", scope_name, victim->name, amount, scope_vnum);
    log_string(buf);
}

// Syntax: checkpoint $PLAYER $ROOM|VNUM|none|clear|reset
SCRIPT_CMD(scriptcmd_checkpoint)
{
    char *rest;
    CHAR_DATA *mob;

    if(!info || IS_NULLSTR(argument))
        return;

    if(!(info->mob || info->obj || info->room || info->token))
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(arg->type != ENT_MOBILE || !arg->d.mob)
        return;

    mob = arg->d.mob;
    if(IS_NPC(mob))
        return;

    if(!(rest = expand_argument(info,rest,arg)))
        return;

    switch(arg->type) {
    case ENT_STRING:
        if(!str_cmp(arg->d.str, "none") ||
           !str_cmp(arg->d.str, "clear") ||
           !str_cmp(arg->d.str, "reset"))
            mob->checkpoint = NULL;
        break;

    case ENT_NUMBER:
        if(arg->d.num > 0) {
            WNUM room_wnum;
            if(resolve_widevnum(arg->d.num, NULL, &room_wnum))
                mob->checkpoint = get_room_index(room_wnum.pArea, room_wnum.vnum);
            else
                mob->checkpoint = NULL;
        }
        break;

    case ENT_ROOM:
        if(arg->d.room != NULL)
            mob->checkpoint = arg->d.room;
        break;
    }
}

// Syntax: saveplayer $PLAYER
SCRIPT_CMD(scriptcmd_saveplayer)
{
    char *rest;
    CHAR_DATA *mob;

    if(!info || IS_NULLSTR(argument))
        return;

    if(!(info->mob || info->obj || info->room || info->token))
        return;

    info->progs->lastreturn = 0;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(arg->type != ENT_MOBILE || !arg->d.mob)
        return;

    mob = arg->d.mob;
    if(IS_NPC(mob))
        return;

    save_char_obj(mob);
    info->progs->lastreturn = 1;

    if(!(rest = expand_argument(info,rest,arg)))
        return;

    switch(arg->type) {
    case ENT_STRING:
        if(!str_cmp(arg->d.str, "none") ||
           !str_cmp(arg->d.str, "clear") ||
           !str_cmp(arg->d.str, "reset"))
            mob->checkpoint = NULL;
        break;

    case ENT_NUMBER:
        if(arg->d.num > 0) {
            WNUM room_wnum;
            if(resolve_widevnum(arg->d.num, NULL, &room_wnum))
                mob->checkpoint = get_room_index(room_wnum.pArea, room_wnum.vnum);
            else
                mob->checkpoint = NULL;
        }
        break;

    case ENT_ROOM:
        if(arg->d.room != NULL)
            mob->checkpoint = arg->d.room;
        break;
    }
}

// Syntax: <scope> zot <victim>
SCRIPT_CMD(scriptcmd_zot)
{
    CHAR_DATA *victim;
    ROOM_INDEX_DATA *location = NULL;
    const char *scope_name = NULL;
    long scope_vnum = 0;

    if(!info)
        return;

    if(info->mob) {
        location = info->mob->in_room;
        scope_name = "MpZot";
        scope_vnum = VNUM(info->mob);
    } else if(info->obj) {
        location = obj_room(info->obj);
        scope_name = "OpZot";
        scope_vnum = VNUM(info->obj);
    } else if(info->room) {
        location = info->room;
        scope_name = "RpZot";
        scope_vnum = info->room->vnum;
    } else if(info->token) {
        location = token_room(info->token);
        scope_name = "TpZot";
        scope_vnum = VNUM(info->token);
    } else
        return;

    if(!expand_argument(info,argument,arg))
        return;

    switch(arg->type) {
    case ENT_STRING:
        if(info->mob)
            victim = get_char_room(info->mob, NULL, arg->d.str);
        else
            victim = get_char_room(NULL, location, arg->d.str);
        break;

    case ENT_MOBILE:
        victim = arg->d.mob;
        break;

    default:
        victim = NULL;
        break;
    }

    if (!victim) {
        pbugf(LOG_SCRIPTS, "%s - Null victim from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    send_to_char("{Y***{R****** {WZOT {R******{Y***{x\n\r\n\r", victim);
    send_to_char("{YYou are struck by a bolt of lightning!\n\r{x", victim);
    act("{Y$n is struck by a bolt of lightning!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    send_to_char("{ROUCH! That really did hurt!{x\n\r", victim);

    victim->hit = 1;
    victim->mana = 1;
    victim->move = 1;
}

// Syntax: restore $MOBILE[ PERCENT]
// Syntax: remort $PLAYER
SCRIPT_CMD(scriptcmd_remort)
{
    char *rest;
    CHAR_DATA *mob;

    if(!info || IS_NULLSTR(argument))
        return;

    if(!(info->mob || info->obj || info->room || info->token))
        return;

    info->progs->lastreturn = 0;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(arg->type != ENT_MOBILE || !arg->d.mob)
        return;

    mob = arg->d.mob;
    if(IS_NPC(mob) || !mob->desc || is_char_busy(mob))
        return;

    if(mob->desc->input ||
        mob->pk_question ||
        mob->remove_question ||
        mob->personal_pk_question ||
        mob->cross_zone_question ||
        mob->pcdata->convert_church != -1 ||
        mob->challenged ||
        mob->remort_question)
        return;

    if(IS_REMORT(mob))
        return;

    if (mob->tot_level < LEVEL_HERO)
        return;

    mob->remort_question = true;
    send_to_char("Are you ready to be reborn? (yes/no)\n\r", mob);

    info->progs->lastreturn = 1;
}

// Syntax: restore $MOBILE[ PERCENT]
SCRIPT_CMD(scriptcmd_restore)
{
    char *rest;
    int amount = 100;

    if(!info || IS_NULLSTR(argument))
        return;

    if(!(info->mob || info->obj || info->room || info->token))
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(arg->type != ENT_MOBILE || !arg->d.mob)
        return;

    if(*rest) {
        if(!(rest = expand_argument(info,rest,arg)))
            return;

        if(arg->type != ENT_NUMBER)
            return;

        amount = URANGE(1,arg->d.num,100);
    }

    restore_char(arg->d.mob, NULL, amount);
}

// Syntax: FIXAFFECTS $MOBILE
SCRIPT_CMD(scriptcmd_fixaffects)
{
    if(!info || IS_NULLSTR(argument))
        return;

    if(!(info->mob || info->obj || info->room || info->token))
        return;

    if(!expand_argument(info,argument,arg))
        return;

    if(arg->type != ENT_MOBILE)
        return;

    if(arg->d.mob == NULL)
        return;

    affect_fix_char(arg->d.mob);
}

SCRIPT_CMD(scriptcmd_echo)
{
    if(!info)
        return;

    if(info->mob) {
        BUFFER *buffer;

        buffer = new_buf();
        expand_string(info,argument,buffer);

        if(buf_string(buffer)[0] != '\0')
            act(buf_string(buffer), info->mob, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        free_buf(buffer);
        return;
    }

    if(info->obj) {
        BUFFER *buffer;

        buffer = new_buf();
        expand_string(info,argument,buffer);

        if(!buf_string(buffer)[0]) {
            free_buf(buffer);
            return;
        }

        add_buf(buffer,"\n\r");
        room_echo(obj_room(info->obj), buf_string(buffer));
        free_buf(buffer);
        return;
    }

    if(info->room) {
        BUFFER *buffer;

        buffer = new_buf();
        expand_string(info,argument,buffer);

        if(buffer->string[0] != '\0') {
            add_buf(buffer,"\n\r");
            room_echo(info->room, buffer->string);
        }
        free_buf(buffer);
        return;
    }

    if(info->token) {
        BUFFER *buffer;

        buffer = new_buf();
        expand_string(info,argument,buffer);

        if(buffer->string[0] != '\0') {
            add_buf(buffer,"\n\r");
            room_echo(token_room(info->token), buffer->string);
        }
        free_buf(buffer);
    }
}

SCRIPT_CMD(scriptcmd_echoroom)
{
    char *rest;
    ROOM_INDEX_DATA *room;

    if(!info)
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    switch(arg->type) {
    case ENT_MOBILE: room = arg->d.mob->in_room; break;
    case ENT_OBJECT: room = obj_room(arg->d.obj); break;
    case ENT_ROOM: room = arg->d.room; break;
    case ENT_EXIT: room = (arg->d.door.r && arg->d.door.r->exit[arg->d.door.door]) ? exit_destination(arg->d.door.r->exit[arg->d.door.door]) : NULL; break;
    default: room = NULL; break;
    }

    if (!room || !room->people)
        return;

    if(info->mob) {
        BUFFER *buffer = new_buf();
        expand_string(info,argument,buffer);

        if(buf_string(buffer)[0] != '\0')
            act(buf_string(buffer), room->people, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL, NULL, NULL);
        free_buf(buffer);
        return;
    }

    if(info->obj) {
        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(!buf_string(buffer)[0]) {
            free_buf(buffer);
            return;
        }

        add_buf(buffer,"\n\r");
        room_echo(room, buf_string(buffer));
        free_buf(buffer);
        return;
    }

    if(info->room || info->token) {
        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0') {
            add_buf(buffer,"\n\r");
            room_echo(room, buffer->string);
        }

        free_buf(buffer);
    }
}

SCRIPT_CMD(scriptcmd_asound)
{
    ROOM_INDEX_DATA *here, *room;
    ROOM_INDEX_DATA *rooms[MAX_DIR];
    int door, i, j;
    EXIT_DATA *pexit;

    if(!info)
        return;
    if(!argument[0])
        return;

    if(info->mob) {
        if(!info->mob || !info->mob->in_room)
            return;

        here = info->mob->in_room;

        for(door = 0; door < MAX_DIR; door++)
            if((pexit = here->exit[door]) && (room = exit_destination(pexit)) && room != here)
                break;

        if(door < MAX_DIR) {
            BUFFER *buffer = new_buf();
            expand_string(info,argument,buffer);

            if(buffer->state == BUFFER_SAFE && buffer->string[0] != '\0') {
                for(i = 0; door < MAX_DIR; door++)
                    if((pexit = here->exit[door]) && (room = exit_destination(pexit)) && room != here) {
                        for(j = 0; j < i && rooms[j] != room; j++)
                            ;

                        if(i <= j) {
                            MOBtrigger = false;
                            act(buffer->string, room->people, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL, NULL, NULL);
                            MOBtrigger = true;
                            rooms[i++] = room;
                        }
                    }
            }
            free_buf(buffer);
        }
        return;
    }

    if(info->obj) {
        if(!info->obj || !obj_room(info->obj))
            return;

        here = obj_room(info->obj);

        for(door = 0; door < MAX_DIR; door++)
            if((pexit = here->exit[door]) && (room = exit_destination(pexit)) && room != here)
                break;

        if(door < MAX_DIR) {
            BUFFER *buffer = new_buf();
            expand_string(info,argument,buffer);
            if(!buf_string(buffer)[0]) {
                free_buf(buffer);
                return;
            }

            for(i = 0; door < MAX_DIR; door++) {
                if((pexit = here->exit[door]) && (room = exit_destination(pexit)) && room != here) {
                    for(j = 0; j < i && rooms[j] != room; j++)
                        ;

                    if(i <= j) {
                        MOBtrigger = false;
                        act(buf_string(buffer), room->people, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL, NULL, NULL);
                        MOBtrigger = true;
                        rooms[i++] = room;
                    }
                }
            }

            free_buf(buffer);
        }
        return;
    }

    if(info->room) {
        here = info->room;

        for(door = 0; door < MAX_DIR; door++)
            if((pexit = here->exit[door]) && (room = exit_destination(pexit)) && room != here)
                break;

        if(door < MAX_DIR) {
            BUFFER *buffer = new_buf();
            expand_string(info,argument,buffer);

            if(buffer->string[0] != '\0') {
                for(i = 0; door < MAX_DIR; door++)
                    if((pexit = here->exit[door]) && (room = exit_destination(pexit)) && room != here) {
                        for(j = 0; j < i && rooms[j] != room; j++)
                            ;

                        if(i <= j) {
                            MOBtrigger = false;
                            act(buffer->string, room->people, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL, NULL, NULL);
                            MOBtrigger = true;
                            rooms[i++] = room;
                        }
                    }
            }
            free_buf(buffer);
        }
        return;
    }

    if(info->token) {
        if(!token_room(info->token))
            return;

        here = token_room(info->token);

        for(door = 0; door < MAX_DIR; door++)
            if((pexit = here->exit[door]) && (room = exit_destination(pexit)) && room != here)
                break;

        if(door < MAX_DIR) {
            BUFFER *buffer = new_buf();
            expand_string(info,argument,buffer);

            if(buffer->string[0] != '\0') {
                for(i = 0; door < MAX_DIR; door++)
                    if((pexit = here->exit[door]) && (room = exit_destination(pexit)) && room != here) {
                        for(j = 0; j < i && rooms[j] != room; j++)
                            ;

                        if(i <= j) {
                            MOBtrigger = false;
                            act(buffer->string, room->people, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL, NULL, NULL);
                            MOBtrigger = true;
                            rooms[i++] = room;
                        }
                    }
            }

            free_buf(buffer);
        }
    }
}

SCRIPT_CMD(scriptcmd_echobattlespam)
{
    char *rest;
    CHAR_DATA *victim, *attacker, *ch;

    if(!info)
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(info->mob) {
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

        BUFFER *buffer = new_buf();
        expand_string(info,argument,buffer);

        if(buf_string(buffer)[0] != '\0') {
            for (ch = attacker->in_room->people; ch; ch = ch->next_in_room) {
                if (!IS_NPC(ch) && (ch != attacker && ch != victim) &&
                    (is_same_group(ch, attacker) || is_same_group(ch, victim) || !IS_SET(ch->comm, COMM_NOBATTLESPAM)))
                    act(buf_string(buffer), ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            }
        }

        free_buf(buffer);
        return;
    }

    if(info->obj) {
        ROOM_INDEX_DATA *room = obj_room(info->obj);

        switch(arg->type) {
        case ENT_STRING: attacker = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: attacker = arg->d.mob; break;
        default: attacker = NULL; break;
        }

        if (!attacker || attacker->in_room != room)
            return;

        if(!(rest = expand_argument(info,rest,arg)))
            return;

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || victim->in_room != room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0') {
            for (ch = attacker->in_room->people; ch; ch = ch->next_in_room) {
                if (!IS_NPC(ch) && (ch != attacker && ch != victim) &&
                    (is_same_group(ch, attacker) || is_same_group(ch, victim) || !IS_SET(ch->comm, COMM_NOBATTLESPAM)))
                    act(buffer->string, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            }
        }

        free_buf(buffer);
        return;
    }

    if(info->room) {
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

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0') {
            for (ch = attacker->in_room->people; ch; ch = ch->next_in_room) {
                if (!IS_NPC(ch) && (ch != attacker && ch != victim) &&
                    (is_same_group(ch, attacker) || is_same_group(ch, victim) || !IS_SET(ch->comm, COMM_NOBATTLESPAM)))
                    act(buffer->string, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            }
        }

        free_buf(buffer);
        return;
    }

    if(info->token) {
        ROOM_INDEX_DATA *room = token_room(info->token);

        switch(arg->type) {
        case ENT_STRING: attacker = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: attacker = arg->d.mob; break;
        default: attacker = NULL; break;
        }

        if (!attacker || !attacker->in_room)
            return;

        if(!(rest = expand_argument(info,rest,arg)))
            return;

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || victim->in_room != attacker->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0') {
            for (ch = attacker->in_room->people; ch; ch = ch->next_in_room) {
                if (!IS_NPC(ch) && (ch != attacker && ch != victim) &&
                    (is_same_group(ch, attacker) || is_same_group(ch, victim) || !IS_SET(ch->comm, COMM_NOBATTLESPAM)))
                    act(buffer->string, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            }
        }

        free_buf(buffer);
    }
}

SCRIPT_CMD(scriptcmd_echochurch)
{
    char *rest;
    CHAR_DATA *victim;

    if(!info)
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(info->mob) {
        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || IS_NPC(victim) || !victim->church)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buf_string(buffer)[0] != '\0')
            msg_church_members(victim->church, buf_string(buffer));

        free_buf(buffer);
        return;
    }

    if(info->obj) {
        ROOM_INDEX_DATA *room = obj_room(info->obj);

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || IS_NPC(victim) || !victim->church)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            msg_church_members(victim->church, buffer->string);

        free_buf(buffer);
        return;
    }

    if(info->room) {
        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, info->room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || IS_NPC(victim) || !victim->church)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            msg_church_members(victim->church, buffer->string);

        free_buf(buffer);
        return;
    }

    if(info->token) {
        ROOM_INDEX_DATA *room = token_room(info->token);

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || IS_NPC(victim) || !victim->church)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            msg_church_members(victim->church, buffer->string);

        free_buf(buffer);
    }
}

SCRIPT_CMD(scriptcmd_echogrouparound)
{
    char *rest;
    CHAR_DATA *victim;

    if(!info)
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(info->mob) {
        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || !victim->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buf_string(buffer)[0] != '\0')
            act_new(buf_string(buffer),victim,NULL,NULL, NULL, NULL,NULL,NULL,NULL,NULL,TO_NOTFUNC,POS_RESTING,rop_same_group);
        free_buf(buffer);
        return;
    }

    if(info->obj) {
        ROOM_INDEX_DATA *room = obj_room(info->obj);

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || victim->in_room != room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act_new(buffer->string,victim,NULL,NULL, NULL, NULL,NULL,NULL,NULL,NULL,TO_NOTFUNC,POS_RESTING,rop_same_group);
        free_buf(buffer);
        return;
    }

    if(info->room) {
        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, info->room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || !victim->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act_new(buffer->string,victim,NULL,NULL, NULL, NULL,NULL,NULL,NULL,NULL,TO_NOTFUNC,POS_RESTING,rop_same_group);
        free_buf(buffer);
        return;
    }

    if(info->token) {
        ROOM_INDEX_DATA *room = token_room(info->token);

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || !victim->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act_new(buffer->string,victim,NULL,NULL, NULL, NULL,NULL,NULL,NULL,NULL,TO_NOTFUNC,POS_RESTING,rop_same_group);
        free_buf(buffer);
    }
}

SCRIPT_CMD(scriptcmd_echogroupat)
{
    char *rest;
    CHAR_DATA *victim;

    if(!info)
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(info->mob) {
        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || !victim->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buf_string(buffer)[0] != '\0')
            act_new(buf_string(buffer),victim,NULL,NULL, NULL, NULL,NULL,NULL,NULL,NULL,TO_FUNC,POS_RESTING,rop_same_group);

        free_buf(buffer);
        return;
    }

    if(info->obj) {
        ROOM_INDEX_DATA *room = obj_room(info->obj);

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || victim->in_room != room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act_new(buffer->string,victim,NULL,NULL, NULL, NULL,NULL,NULL,NULL,NULL,TO_FUNC,POS_RESTING,rop_same_group);
        free_buf(buffer);
        return;
    }

    if(info->room) {
        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, info->room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || !victim->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act_new(buffer->string,victim,NULL,NULL, NULL, NULL,NULL,NULL,NULL,NULL,TO_FUNC,POS_RESTING,rop_same_group);
        free_buf(buffer);
        return;
    }

    if(info->token) {
        ROOM_INDEX_DATA *room = token_room(info->token);

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || !victim->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act_new(buffer->string,victim,NULL,NULL, NULL, NULL,NULL,NULL,NULL,NULL,TO_FUNC,POS_RESTING,rop_same_group);

        free_buf(buffer);
    }
}

SCRIPT_CMD(scriptcmd_echoleadaround)
{
    char *rest;
    CHAR_DATA *victim;

    if(!info)
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(info->mob) {
        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim)
            return;

        if(victim->leader)
            victim = victim->leader;

        if(!victim->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buf_string(buffer)[0] != '\0')
            act(buf_string(buffer), victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

        free_buf(buffer);
        return;
    }

    if(info->obj) {
        ROOM_INDEX_DATA *room = obj_room(info->obj);

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || !victim->leader || victim->leader->in_room != room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act(buffer->string, victim->leader, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        free_buf(buffer);
        return;
    }

    if(info->room) {
        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, info->room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || !victim->leader || !victim->leader->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act(buffer->string, victim->leader, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        free_buf(buffer);
        return;
    }

    if(info->token) {
        ROOM_INDEX_DATA *room = token_room(info->token);

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || !victim->leader || !victim->leader->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act(buffer->string, victim->leader, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        free_buf(buffer);
    }
}

SCRIPT_CMD(scriptcmd_echoleadat)
{
    char *rest;
    CHAR_DATA *victim;

    if(!info)
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(info->mob) {
        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim)
            return;

        if(victim->leader)
            victim = victim->leader;

        if(!victim->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buf_string(buffer)[0] != '\0')
            act(buf_string(buffer), victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

        free_buf(buffer);
        return;
    }

    if(info->obj) {
        ROOM_INDEX_DATA *room = obj_room(info->obj);

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || !victim->leader || victim->leader->in_room != room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act(buffer->string, victim->leader, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        free_buf(buffer);
        return;
    }

    if(info->room) {
        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, info->room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || !victim->leader || !victim->leader->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act(buffer->string, victim->leader, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        free_buf(buffer);
        return;
    }

    if(info->token) {
        ROOM_INDEX_DATA *room = token_room(info->token);

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if (!victim || !victim->leader || !victim->leader->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act(buffer->string, victim->leader, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

        free_buf(buffer);
    }
}

SCRIPT_CMD(scriptcmd_echoaround)
{
    char *rest;
    CHAR_DATA *victim;

    if(!info)
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(info->mob) {
        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if(!victim || !victim->in_room || !victim->in_room->people)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buf_string(buffer)[0] != '\0')
            act(buf_string(buffer), victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        free_buf(buffer);
        return;
    }

    if(info->obj) {
        ROOM_INDEX_DATA *room = obj_room(info->obj);

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if(!victim || victim->in_room != room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act(buffer->string, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        free_buf(buffer);
        return;
    }

    if(info->room) {
        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, info->room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if(!victim || !victim->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act(buffer->string, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        free_buf(buffer);
        return;
    }

    if(info->token) {
        ROOM_INDEX_DATA *room = token_room(info->token);

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if(!victim || !victim->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act(buffer->string, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        free_buf(buffer);
    }
}

SCRIPT_CMD(scriptcmd_echonotvict)
{
    char *rest;
    CHAR_DATA *victim, *attacker;

    if(!info)
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(info->mob) {
        switch(arg->type) {
        case ENT_STRING: attacker = get_char_room(info->mob, NULL, arg->d.str); break;
        case ENT_MOBILE: attacker = arg->d.mob; break;
        default: attacker = NULL; break;
        }

        if(!attacker || !attacker->in_room)
            return;

        if(!(rest = expand_argument(info,rest,arg)))
            return;

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if(!victim || victim->in_room != attacker->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,argument,buffer);

        if(buf_string(buffer)[0] != '\0')
            act(buf_string(buffer), victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
        free_buf(buffer);
        return;
    }

    if(info->obj) {
        ROOM_INDEX_DATA *room = obj_room(info->obj);

        switch(arg->type) {
        case ENT_STRING: attacker = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: attacker = arg->d.mob; break;
        default: attacker = NULL; break;
        }

        if(!attacker || attacker->in_room != room)
            return;

        if(!(rest = expand_argument(info,rest,arg)))
            return;

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if(!victim || victim->in_room != room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act(buffer->string, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
        free_buf(buffer);
        return;
    }

    if(info->room) {
        switch(arg->type) {
        case ENT_STRING: attacker = get_char_room(NULL, info->room,arg->d.str); break;
        case ENT_MOBILE: attacker = arg->d.mob; break;
        default: attacker = NULL; break;
        }

        if(!attacker || !attacker->in_room)
            return;

        if(!(rest = expand_argument(info,rest,arg)))
            return;

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, info->room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if(!victim || victim->in_room != attacker->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act(buffer->string, victim, attacker, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
        free_buf(buffer);
        return;
    }

    if(info->token) {
        ROOM_INDEX_DATA *room = token_room(info->token);

        switch(arg->type) {
        case ENT_STRING: attacker = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: attacker = arg->d.mob; break;
        default: attacker = NULL; break;
        }

        if(!attacker || !attacker->in_room)
            return;

        if(!(rest = expand_argument(info,rest,arg)))
            return;

        switch(arg->type) {
        case ENT_STRING: victim = get_char_room(NULL, room,arg->d.str); break;
        case ENT_MOBILE: victim = arg->d.mob; break;
        default: victim = NULL; break;
        }

        if(!victim || victim->in_room != attacker->in_room)
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(buffer->string[0] != '\0')
            act(buffer->string, victim, attacker, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
        free_buf(buffer);
    }
}

SCRIPT_CMD(scriptcmd_gecho)
{
    DESCRIPTOR_DATA *d;

    if(!info)
        return;

    if(info->mob) {
        if (!argument[0]) {
            pbugf(LOG_SCRIPTS, "MpGEcho: missing argument from vnum %d", VNUM(info->mob));
            return;
        }

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
        return;
    }

    if(info->obj) {
        if (!argument[0]) {
            pbugf(LOG_SCRIPTS, "OpGEcho: missing argument from vnum %d", VNUM(info->obj));
            return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,argument,buffer);

        if( buffer->string[0] != '\0' ) {
            for (d = descriptor_list; d; d = d->next)
                if (d->connected == CON_PLAYING) {
                    if (IS_IMMORTAL(d->character))
                        send_to_char("Obj echo> ", d->character);
                    send_to_char(buffer->string, d->character);
                    send_to_char("\n\r", d->character);
                }
        }

        free_buf(buffer);
        return;
    }

    if(info->room) {
        if (!argument[0]) {
            pbugf(LOG_SCRIPTS, "RpGEcho: missing argument from vnum %d", info->room->vnum);
            return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,argument,buffer);

        if( buffer->string[0] != '\0' ) {
            for (d = descriptor_list; d; d = d->next)
                if (d->connected == CON_PLAYING) {
                    if (IS_IMMORTAL(d->character))
                        send_to_char("Obj echo> ", d->character);
                    send_to_char(buffer->string, d->character);
                    send_to_char("\n\r", d->character);
                }
        }

        free_buf(buffer);
        return;
    }

    if(info->token) {
        if (!argument[0]) {
            pbugf(LOG_SCRIPTS,"TpZEcho: missing argument from vnum %d", VNUM(info->token));
            return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,argument,buffer);

        if( buffer->string[0] != '\0' ) {
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
}

SCRIPT_CMD(scriptcmd_zecho)
{
    DESCRIPTOR_DATA *d;

    if(!info)
        return;

    if(info->mob) {
        BUFFER *buffer = new_buf();
        expand_string(info,argument,buffer);

        if (!buf_string(buffer)[0]) {
            pbugf(LOG_SCRIPTS, "MpZEcho: missing argument from vnum %d", VNUM(info->mob));
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
        return;
    }

    if(info->obj) {
        AREA_DATA *area;

        if(!obj_room(info->obj))
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,argument,buffer);

        if( buffer->string[0] != '\0' ) {
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
        return;
    }

    if(info->room) {
        AREA_DATA *area;

        BUFFER *buffer = new_buf();
        expand_string(info,argument,buffer);

        if( buffer->string[0] != '\0' ) {
            area = info->room->area;

            for (d = descriptor_list; d; d = d->next)
                if (d->connected == CON_PLAYING &&
                    d->character->in_room &&
                    d->character->in_room->area == area) {
                    if (IS_IMMORTAL(d->character))
                        send_to_char("Room echo> ", d->character);
                    send_to_char(buffer->string, d->character);
                    send_to_char("\n\r", d->character);
                }
        }

        free_buf(buffer);
        return;
    }

    if(info->token) {
        AREA_DATA *area;

        if(!token_room(info->token))
            return;

        BUFFER *buffer = new_buf();
        expand_string(info,argument,buffer);

        if( buffer->string[0] != '\0' ) {
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
    } else if( arg->type == ENT_NUMBER ) {
        token_index = get_token_index_from_info(info, arg->d.num);

        if( !token_index ) return;
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
            bool fPerm = false;

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
        SKILL_ENTRY *entry;

        if( skill_entry_findsn(mob->sorted_skills, sn) )
            return;

        if( skill_table[sn].spell_fun == spell_null ) {
            skill_entry_addskill(mob, sn, NULL, source, flags);
        } else {
            skill_entry_addspell(mob, sn, NULL, source, flags);
        }

        entry = skill_entry_findsn(mob->sorted_skills, sn);
        if (entry)
            entry->rating = rating;

        mob->pcdata->learned[sn] = rating;
    }
    else
        return;

    info->progs->lastreturn = 1;
}

//////////////////////////////////////
// H

//////////////////////////////////////
// I

// INPUTSTRING $PLAYER script-vnum variable
//  Invokes the interal string editor, for use in getting multiline strings from players.
//
//  $PLAYER - player entity to get string from
//  script-vnum - script to call after the editor is closed
//  variable - name of variable to use to store the string (as well as supply the initial string)

SCRIPT_CMD(scriptcmd_inputstring)
{
    char *rest;
    long vnum;
    CHAR_DATA *mob = NULL;
    SCRIPT_DATA *script = NULL;

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
        pbugf(LOG_SCRIPTS, "MpInput - Error in parsing.");
        return;
    }

    script = get_script_from_arg(info, arg, type, &vnum);
    if(vnum < 1 || !script) return;
    BUFFER *buffer = new_buf();

    expand_string(info,rest,buffer);


    pVARIABLE var = variable_get(*(info->var),buf_string(buffer));

    mob->desc->input = true;
    if( var && var->type == VAR_STRING && !IS_NULLSTR(var->_.s) )
        mob->desc->inputString = str_dup(var->_.s);
    else
        mob->desc->inputString = &str_empty[0];
    mob->desc->input_var = str_dup(buf_string(buffer));
    mob->desc->input_prompt = NULL;
    mob->desc->input_script = vnum;
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
        if( !IS_SET(arg->d.instance->flags, INSTANCE_COMPLETED) )
        {
            p_percent2_trigger(NULL, arg->d.instance, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_COMPLETED,NULL);

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
        if( !IS_SET(arg->d.instance->flags, INSTANCE_FAILED) )
        {
            SET_BIT(arg->d.instance->flags, INSTANCE_FAILED);

            p_percent2_trigger(NULL, arg->d.instance, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_FAILED, NULL);
        }
    }
}


//////////////////////////////////////
// J

//////////////////////////////////////
// K

//////////////////////////////////////
// L

// LOADINSTANCED mobile $VNUM|$MOBILE $ROOM[ $VARIABLENAME]
// LOADINSTANCED object $VNUM|$OBJECT $LEVEL room|here|wear $ENTITY[ $VARIABLE]
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
    else if( info->mob && IS_SET(info->mob->act[1], ACT2_INSTANCE_MOB) ) valid = true;
    else if( info->obj && IS_SET(info->obj->extra[2], ITEM_INSTANCE_OBJ) ) valid = true;
    else if( info->token )
    {
        if( info->token->room && IS_VALID(info->token->room->instance_section) ) valid = true;
        else if( info->token->player && IS_SET(info->token->player->act[1], ACT2_INSTANCE_MOB) ) valid = true;
        else if( info->token->object && IS_SET(info->token->object->extra[2], ITEM_INSTANCE_OBJ) ) valid = true;
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

// LOCKADD $OBJECT
SCRIPT_CMD(scriptcmd_lockadd)
{
    info->progs->lastreturn = 0;

    if( !expand_argument(info,argument,arg) || arg->type != ENT_OBJECT || !IS_VALID(arg->d.obj) )
        return;

    if( arg->d.obj->lock )
        return;

    switch(arg->d.obj->item_type)
    {
    case ITEM_CONTAINER:
    case ITEM_BOOK:
    case ITEM_PORTAL:
//	case ITEM_WEAPON_CONTAINER:
//	case ITEM_DRINKCONTAINER:
        break;
    default:
        return;
    }

    LOCK_STATE *lock = new_lock_state();
    SET_BIT(lock->flags, LOCK_CREATED);

    arg->d.obj->lock = lock;

    info->progs->lastreturn = 1;
}

// LOCKREMOVE $OBJECT
SCRIPT_CMD(scriptcmd_lockremove)
{
    info->progs->lastreturn = 0;

    if( !expand_argument(info,argument,arg) || arg->type != ENT_OBJECT || !IS_VALID(arg->d.obj) )
        return;

    LOCK_STATE *lock = arg->d.obj->lock;

    if( !lock )
        return;

    // Only CREATED locks with noremove can be stripped off by this command.
    if( IS_SET(lock->flags, LOCK_NOREMOVE) && !IS_SET(lock->flags, LOCK_CREATED))
        return;

    free_lock_state(lock);

    arg->d.obj->lock = NULL;

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

        if( !IS_SET(arg->d.mob->act[1], ACT2_INSTANCE_MOB) )
        {
            SET_BIT(arg->d.mob->act[1], ACT2_INSTANCE_MOB);


            list_remlink(instance->mobiles, arg->d.mob, false);
            if( IS_VALID(instance->dungeon) )
                list_remlink(instance->dungeon->mobiles, arg->d.mob, false);
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

        if( !IS_SET(arg->d.obj->extra[2], ITEM_INSTANCE_OBJ) )
        {
            SET_BIT(arg->d.obj->extra[2], ITEM_INSTANCE_OBJ);

            list_remlink(instance->objects, arg->d.obj, false);
            if( IS_VALID(instance->dungeon) )
                list_remlink(instance->dungeon->objects, arg->d.obj, false);
        }
    }
    else
        return;



    info->progs->lastreturn = 1;
}

// MLOAD <vnum>[ <room>][ <variable>]
SCRIPT_CMD(scriptcmd_mload)
{
    script_mload(info,argument,arg, false);
}

// EVENT clear|inherit|set|copy <target_mob|target_obj> [args]
// EVENT phase|stage <event> next|set <phase_or_stage_name>
// EVENT objective <event> check|next|advance|complete
// clear   <target>
// inherit <target>
// set     <target> <event_uid> [instance_id]
// copy    <target> <source_mob|source_obj>
SCRIPT_CMD(scriptcmd_event)
{
    char command[MIL];
    char event_token[MIL];
    char operation[MIL];
    char *rest;
    CHAR_DATA *target_mob = NULL;
    OBJ_DATA *target_obj = NULL;
    long event_uid = 0;
    uint32_t instance_id = 0;
    int source_bracket = 0;

    if (!info)
        return;

    info->progs->lastreturn = 0;

    rest = one_argument(argument, command);
    if (IS_NULLSTR(command) || IS_NULLSTR(rest))
        return;

    if (!str_prefix(command, "progress")) {
        int delta = 0;

        if (!(rest = expand_argument(info, rest, arg)))
            return;
        if (!scriptcmd_event_param_to_token(arg, event_token, sizeof(event_token)))
            return;

        rest = one_argument(rest, operation);
        if (IS_NULLSTR(operation) || IS_NULLSTR(rest))
            return;

        if (!(rest = expand_argument(info, rest, arg)) || !scriptcmd_event_parse_uid(arg, &event_uid))
            return;

        delta = (int)event_uid;

        if (!str_prefix(operation, "addkills")) {
            if (event_runtime_adjust_progress(event_token, delta, 0))
                info->progs->lastreturn = 1;
            return;
        }

        if (!str_prefix(operation, "additems")) {
            if (event_runtime_adjust_progress(event_token, 0, delta))
                info->progs->lastreturn = 1;
            return;
        }

        if (!str_prefix(operation, "setgoal")) {
            if (event_runtime_set_goal(event_token, UMAX(0, delta)))
                info->progs->lastreturn = 1;
            return;
        }

        return;
    }

    if (!str_prefix(command, "phase") || !str_prefix(command, "stage")) {
        if (!(rest = expand_argument(info, rest, arg)))
            return;
        if (!scriptcmd_event_param_to_token(arg, event_token, sizeof(event_token)))
            return;

        rest = one_argument(rest, operation);
        if (!str_cmp(operation, "next")) {
            if (event_runtime_next_phase(event_token))
                info->progs->lastreturn = 1;
            return;
        }

        if (str_cmp(operation, "set") || IS_NULLSTR(rest))
            return;

        if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_STRING || IS_NULLSTR(arg->d.str))
            return;

        if (event_runtime_set_phase(event_token, arg->d.str))
            info->progs->lastreturn = 1;
        return;
    }

    if (!str_prefix(command, "objective")) {
        if (!(rest = expand_argument(info, rest, arg)))
            return;
        if (!scriptcmd_event_param_to_token(arg, event_token, sizeof(event_token)))
            return;

        rest = one_argument(rest, operation);
        if (IS_NULLSTR(operation))
            return;

        if (!str_cmp(operation, "check")) {
            if (event_runtime_check_objectives(event_token))
                info->progs->lastreturn = 1;
            return;
        }

        if (!str_cmp(operation, "next")
            || !str_cmp(operation, "advance")
            || !str_cmp(operation, "complete")) {
            if (event_runtime_next_phase(event_token))
                info->progs->lastreturn = 1;
            return;
        }

        return;
    }

    if (!str_prefix(command, "complete") || !str_prefix(command, "fail")) {
        bool success = !str_prefix(command, "complete");
        BUFFER *buffer;

        if (!(rest = expand_argument(info, rest, arg)))
            return;
        if (!scriptcmd_event_param_to_token(arg, event_token, sizeof(event_token)))
            return;

        rest = skip_whitespace(rest);
        if (IS_NULLSTR(rest)) {
            if (event_runtime_finish(event_token, success, NULL))
                info->progs->lastreturn = 1;
            return;
        }

        buffer = new_buf();
        if (expand_string(info, rest, buffer)
            && event_runtime_finish(event_token, success, buf_string(buffer)))
            info->progs->lastreturn = 1;
        free_buf(buffer);
        return;
    }

    if (!(rest = expand_argument(info, rest, arg)))
        return;

    if (arg->type == ENT_MOBILE)
        target_mob = arg->d.mob;
    else if (arg->type == ENT_OBJECT)
        target_obj = arg->d.obj;
    else
        return;

    if (!target_mob && !target_obj)
        return;

    if (!str_prefix(command, "clear")) {
        if (target_mob) {
            event_tag_mobile_spawn(target_mob, 0, 0);
            event_set_mobile_spawn_bracket(target_mob, 0);
        } else {
            event_tag_object_spawn(target_obj, 0, 0);
            event_set_object_spawn_bracket(target_obj, 0);
        }

        info->progs->lastreturn = 1;
        return;
    }

    if (!str_prefix(command, "inherit")) {
        if (!scriptcmd_event_get_source_from_info(info, &event_uid, &instance_id, &source_bracket) || event_uid <= 0)
            return;

        if (target_mob) {
            event_tag_mobile_spawn(target_mob, event_uid, instance_id);
            event_set_mobile_spawn_bracket(target_mob, source_bracket);
        } else {
            event_tag_object_spawn(target_obj, event_uid, instance_id);
            event_set_object_spawn_bracket(target_obj, source_bracket);
        }

        info->progs->lastreturn = 1;
        return;
    }

    if (!str_prefix(command, "set")) {
        if (IS_NULLSTR(rest) || !(rest = expand_argument(info, rest, arg)))
            return;

        if (!scriptcmd_event_parse_uid(arg, &event_uid) || event_uid <= 0)
            return;

        instance_id = 0;
        source_bracket = 0;
        if (!IS_NULLSTR(rest)) {
            if (!(rest = expand_argument(info, rest, arg)))
                return;

            if (!scriptcmd_event_parse_instance(arg, &instance_id))
                return;

            if (!IS_NULLSTR(rest)) {
                long bracket_value = 0;

                if (!(rest = expand_argument(info, rest, arg)))
                    return;

                if (!scriptcmd_event_parse_uid(arg, &bracket_value))
                    return;

                source_bracket = (int)UMAX(0, bracket_value);
                if (source_bracket > 32767)
                    source_bracket = 32767;
            }
        }

        if (target_mob) {
            event_tag_mobile_spawn(target_mob, event_uid, instance_id);
            event_set_mobile_spawn_bracket(target_mob, source_bracket);
        } else {
            event_tag_object_spawn(target_obj, event_uid, instance_id);
            event_set_object_spawn_bracket(target_obj, source_bracket);
        }

        info->progs->lastreturn = 1;
        return;
    }

    if (!str_prefix(command, "copy")) {
        if (IS_NULLSTR(rest) || !(rest = expand_argument(info, rest, arg)))
            return;

        if (!scriptcmd_event_get_source_from_param(arg, &event_uid, &instance_id, &source_bracket) || event_uid <= 0)
            return;

        if (target_mob) {
            event_tag_mobile_spawn(target_mob, event_uid, instance_id);
            event_set_mobile_spawn_bracket(target_mob, source_bracket);
        } else {
            event_tag_object_spawn(target_obj, event_uid, instance_id);
            event_set_object_spawn_bracket(target_obj, source_bracket);
        }

        info->progs->lastreturn = 1;
        return;
    }
}

// STARTEVENT <event_uid|name|widevnum> [starter_mob]
SCRIPT_CMD(scriptcmd_startevent)
{
    char event_token[MIL];
    char *rest;
    CHAR_DATA *starter;

    if (!info)
        return;

    info->progs->lastreturn = 0;

    if (!(rest = expand_argument(info, argument, arg)))
        return;
    if (!scriptcmd_event_param_to_token(arg, event_token, sizeof(event_token)))
        return;

    starter = scriptcmd_event_default_starter(info);

    if (!IS_NULLSTR(rest)) {
        if (!(rest = expand_argument(info, rest, arg)))
            return;

        if (arg->type != ENT_MOBILE || !arg->d.mob)
            return;

        starter = arg->d.mob;
    }

    if (event_runtime_start(event_token, starter, NULL, NULL))
        info->progs->lastreturn = 1;
}

// STOPEVENT <event_uid|name|widevnum>
SCRIPT_CMD(scriptcmd_stopevent)
{
    char event_token[MIL];

    if (!info)
        return;

    info->progs->lastreturn = 0;

    if (!expand_argument(info, argument, arg))
        return;
    if (!scriptcmd_event_param_to_token(arg, event_token, sizeof(event_token)))
        return;

    if (event_runtime_stop(event_token))
        info->progs->lastreturn = 1;
}

// PHASEEVENT|STAGEEVENT <event_uid|name|widevnum> next
// PHASEEVENT|STAGEEVENT <event_uid|name|widevnum> set <phase_or_stage_name>
SCRIPT_CMD(scriptcmd_phaseevent)
{
    char event_token[MIL];
    char operation[MIL];
    char *rest;

    if (!info)
        return;

    info->progs->lastreturn = 0;

    if (!(rest = expand_argument(info, argument, arg)))
        return;
    if (!scriptcmd_event_param_to_token(arg, event_token, sizeof(event_token)))
        return;

    rest = one_argument(rest, operation);
    if (IS_NULLSTR(operation))
        return;

    if (!str_cmp(operation, "next")) {
        if (event_runtime_next_phase(event_token))
            info->progs->lastreturn = 1;
        return;
    }

    if (str_cmp(operation, "set") || IS_NULLSTR(rest))
        return;

    if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_STRING || IS_NULLSTR(arg->d.str))
        return;

    if (event_runtime_set_phase(event_token, arg->d.str))
        info->progs->lastreturn = 1;
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

// OLOAD <vnum> [<level>] [room|wear|$ENTITY][ <variable>]
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

SCRIPT_CMD(scriptcmd_purge)
{
    char *rest;
    CHAR_DATA **mobs = NULL, *victim = NULL, *vnext;
    OBJ_DATA **objs = NULL, *obj = NULL, *obj_next;
    ROOM_INDEX_DATA *here = NULL;
    CHAR_DATA *exclude_mob = NULL;
    OBJ_DATA *exclude_obj = NULL;
    const char *scope_name = NULL;
    long scope_vnum = 0;
    bool check_prog_at = false;

    EXIT_DATA *ex;

    if(!info)
        return;

    if(info->mob) {
        if(!info->mob->in_room)
            return;
        here = info->mob->in_room;
        exclude_mob = info->mob;
        scope_name = "Mppurge";
        scope_vnum = VNUM(info->mob);
        check_prog_at = true;
    } else if(info->obj) {
        here = obj_room(info->obj);
        if(!here)
            return;
        exclude_obj = info->obj;
        scope_name = "Oppurge";
        scope_vnum = VNUM(info->obj);
    } else if(info->room) {
        here = info->room;
        scope_name = "Rppurge";
        scope_vnum = info->room->vnum;
    } else if(info->token) {
        here = token_room(info->token);
        if(!here)
            return;
        scope_name = "Oppurge";
        scope_vnum = VNUM(info->token);
    } else
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    switch(arg->type) {
    case ENT_NONE:
        break;

    case ENT_STRING:
        if (!(victim = get_char_room(NULL, here, arg->d.str)))
            obj = get_obj_here(NULL, here, arg->d.str);
        break;

    case ENT_MOBILE:
        victim = arg->d.mob;
        break;

    case ENT_OBJECT:
        obj = arg->d.obj;
        break;

    case ENT_ROOM:
        here = arg->d.room;
        break;

    case ENT_EXIT:
        ex = arg->d.door.r ? arg->d.door.r->exit[arg->d.door.door] : NULL;
        here = ex ? exit_destination(ex) : NULL;
        break;

    case ENT_OLLIST_MOB:
        mobs = arg->d.list.ptr.mob;
        break;

    case ENT_OLLIST_OBJ:
        objs = arg->d.list.ptr.obj;
        break;

    default:
        break;
    }

    if(victim) {
        if (!IS_NPC(victim)) {
            pbugf(LOG_SCRIPTS, "%s - Attempting to purge a PC from vnum %ld.", scope_name, scope_vnum);
            return;
        }

        if(check_prog_at && PROG_FLAG(victim,PROG_AT))
            return;

        extract_char(victim, true);
    } else if(obj) {
        if(check_prog_at && PROG_FLAG(obj,PROG_AT))
            return;

        extract_obj(obj);
    } else if(here) {
        for (victim = here->people; victim; victim = vnext) {
            vnext = victim->next_in_room;
            if (IS_NPC(victim)
                && victim != exclude_mob
                && !IS_SET(victim->act[0], ACT_NOPURGE))
                extract_char(victim, true);
        }

        for (obj = here->contents; obj; obj = obj_next) {
            obj_next = obj->next_content;
            if (obj != exclude_obj
                && !IS_SET(obj->extra[0], ITEM_NOPURGE))
                extract_obj(obj);
        }
    } else if(mobs) {
        for (victim = *mobs; victim; victim = vnext) {
            vnext = victim->next_in_room;
            if (IS_NPC(victim)
                && victim != exclude_mob
                && !IS_SET(victim->act[0], ACT_NOPURGE))
                extract_char(victim, true);
        }
    } else if(objs) {
        for (obj = *objs; obj; obj = obj_next) {
            obj_next = obj->next_content;
            if (obj != exclude_obj
                && !IS_SET(obj->extra[0], ITEM_NOPURGE))
                extract_obj(obj);
        }
    } else
        pbugf(LOG_SCRIPTS, "%s - Bad argument from vnum %ld.", scope_name, scope_vnum);
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

    mob->quest_runtime.expiry_modes |= QUEST_EXPIRY_COUNTDOWN;
    mob->quest_runtime.expiry_countdown_minutes = mob->countdown;

    mob->quest->generating = false;
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
    long vnum;
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
        if(!(rest = expand_argument(info,rest,arg)))
            return;

        script = get_script_from_arg(info, arg, type, &vnum);
        if (vnum < 1 || !script)
            return;

        // Don't care about response
        execute_script(script->vnum, script, info->mob, info->obj, info->room, info->token, NULL, NULL, NULL, mob, NULL, NULL, NULL, NULL,NULL, NULL,NULL,NULL,TRIG_NONE,0,0,0,0,0);
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


// QUESTGENERATE $PLAYER $QUESTRECEIVER $PARTCOUNT $PARTSCRIPT [ $QUESTINDEX ]

// QUESTRECEIVER cannot be a wilderness room (for now?)
SCRIPT_CMD(scriptcmd_questgenerate)
{
    char *rest;
    CHAR_DATA *mob;
    int qg_type;
    long qg_vnum;
    AREA_DATA *qg_area = NULL;
    CHAR_DATA *qr_mob = NULL;
    OBJ_DATA *qr_obj = NULL;
    ROOM_INDEX_DATA *qr_room = NULL;
    int *tempstores;
    int type, parts;
    WNUM quest_index_wnum = wnum_zero;
    long vnum;
    SCRIPT_DATA *script;

    info->progs->lastreturn = 0;

    if(info->mob)
    {
        type = PRG_MPROG;
        tempstores = info->mob->tempstore;

        if( !IS_NPC(info->mob) )
            return;

        qg_type = QUESTOR_MOB;
        qg_vnum = info->mob->pIndexData->vnum;
        qg_area = info->mob->pIndexData->area;
    }
    else if(info->obj)
    {
        type = PRG_OPROG;
        tempstores = info->obj->tempstore;

        qg_type = QUESTOR_OBJ;
        qg_vnum = info->obj->pIndexData->vnum;
        qg_area = info->obj->pIndexData->area;
    }
    else if(info->room)
    {
        type = PRG_RPROG;
        tempstores = info->room->tempstore;
        if( info->room->wilds || info->room->source )
            return;

        qg_type = QUESTOR_ROOM;
        qg_vnum = info->room->vnum;
        qg_area = info->room->area;
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
            qg_vnum = info->token->player->pIndexData->vnum;
            qg_area = info->token->player->pIndexData->area;
        }
        else if( info->token->object )
        {
            qg_type = QUESTOR_OBJ;
            qg_vnum = info->token->object->pIndexData->vnum;
            qg_area = info->token->object->pIndexData->area;
        }
        else if( info->token->room )
        {
            if( info->token->room->wilds || info->token->room->source )
                return;

            qg_type = QUESTOR_ROOM;
            qg_vnum = info->token->room->vnum;
            qg_area = info->token->room->area;
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
    if(!(rest = expand_argument(info,rest,arg)))
        return;

    script = get_script_from_arg(info, arg, type, &vnum);
    if (vnum < 1 || !script)
        return;

    // Optional authored/template quest index reference (widevnum).
    if (*rest && expand_argument(info, rest, arg)) {
        if (arg->type == ENT_WIDEVNUM) {
            if (arg->d.wnum.pArea && arg->d.wnum.vnum > 0)
                quest_index_wnum = arg->d.wnum;
        } else if (arg->type == ENT_STRING) {
            if (!parse_widevnum(arg->d.str, qg_area, &quest_index_wnum))
                return;
        } else {
            return;
        }

        if (quest_index_wnum.pArea && quest_index_wnum.vnum > 0) {
            if (!get_quest_index_wnum(quest_index_wnum))
                return;
        }
    }

    mob->quest = new_quest();
    quest_runtime_attach_active_quest(mob,
        quest_index_wnum.pArea ? quest_index_wnum.pArea->uid : 0,
        quest_index_wnum.vnum);
    mob->quest->generating = true;
    mob->quest->scripted = true;
    mob->quest->questgiver_type = qg_type;
    mob->quest->questgiver_load.auid = qg_area ? qg_area->uid : 0;
    mob->quest->questgiver_load.vnum = qg_vnum;
    mob->quest->questgiver_wnum.pArea = qg_area;
    mob->quest->questgiver_wnum.vnum = qg_vnum;
    if( qr_mob )
    {
        mob->quest->questreceiver_type = QUESTOR_MOB;
        mob->quest->questreceiver_load.auid = qr_mob->pIndexData->area->uid;
        mob->quest->questreceiver_load.vnum = qr_mob->pIndexData->vnum;
        mob->quest->questreceiver_wnum.pArea = qr_mob->pIndexData->area;
        mob->quest->questreceiver_wnum.vnum = qr_mob->pIndexData->vnum;
    }
    else if( qr_obj )
    {
        mob->quest->questreceiver_type = QUESTOR_OBJ;
        mob->quest->questreceiver_load.auid = qr_obj->pIndexData->area->uid;
        mob->quest->questreceiver_load.vnum = qr_obj->pIndexData->vnum;
        mob->quest->questreceiver_wnum.pArea = qr_obj->pIndexData->area;
        mob->quest->questreceiver_wnum.vnum = qr_obj->pIndexData->vnum;
    }
    else if( qr_room )
    {
        mob->quest->questreceiver_type = QUESTOR_ROOM;
        mob->quest->questreceiver_load.auid = qr_room->area->uid;
        mob->quest->questreceiver_load.vnum = qr_room->vnum;
        mob->quest->questreceiver_wnum.pArea = qr_room->area;
        mob->quest->questreceiver_wnum.vnum = qr_room->vnum;
    }

    bool success = true;
    for(int i = 0; i < parts; i++)
    {
        QUEST_PART_DATA *part = new_quest_part();

        part->next = mob->quest->parts;
        mob->quest->parts = part;
        part->index = parts - i;

        tempstores[0] = part->index;

        if( execute_script(script->vnum, script, info->mob, info->obj, info->room, info->token, NULL, NULL, NULL, mob, NULL, NULL, NULL, NULL,NULL, NULL,NULL,NULL,TRIG_NONE,0,0,0,0,0) <= 0 )
        {
            success = false;
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
    part->custom_task = true;
    part->minutes = minutes;

    info->progs->lastreturn = 1;
}

// QUESTSETINDEX $PLAYER $QUESTINDEX
// Sets or clears (<=0) the template quest index for the active quest run.
SCRIPT_CMD(scriptcmd_questsetindex)
{
    char *rest;
    CHAR_DATA *mob;
    WNUM quest_index_wnum;
    AREA_DATA *context_area;

    info->progs->lastreturn = 0;

    if (!(rest = expand_argument(info, argument, arg)))
        return;

    if (arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob) || !IS_QUESTING(arg->d.mob))
        return;

    mob = arg->d.mob;

    if (!(rest = expand_argument(info, rest, arg)))
        return;

    if (arg->type == ENT_STRING && !str_cmp(arg->d.str, "none")) {
        quest_runtime_attach_active_quest(mob, 0, 0);
        if (mob->quest) {
            mob->quest->quest_index_auid = 0;
            mob->quest->quest_index_vnum = 0;
        }
        info->progs->lastreturn = 1;
        return;
    }

    context_area = mob->in_room ? mob->in_room->area : NULL;
    quest_index_wnum = wnum_zero;

    if (arg->type == ENT_WIDEVNUM) {
        quest_index_wnum = arg->d.wnum;
    } else if (arg->type == ENT_STRING) {
        if (!parse_widevnum(arg->d.str, context_area, &quest_index_wnum))
            return;
    } else {
        return;
    }

    if (!quest_index_wnum.pArea || quest_index_wnum.vnum < 1)
        return;
    if (!get_quest_index_wnum(quest_index_wnum))
        return;

    quest_runtime_attach_active_quest(mob, quest_index_wnum.pArea->uid, quest_index_wnum.vnum);

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
    quest_part_set_wnum(&part->obj_load, &part->obj_wnum,
        obj->pIndexData->area, obj->pIndexData->vnum);
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
    quest_part_set_wnum(&part->room_load, &part->room_wnum,
        destination->area, destination->vnum);
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
    quest_part_set_wnum(&part->mob_rescue_load, &part->mob_rescue_wnum,
        target->pIndexData->area, target->pIndexData->vnum);
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
    quest_part_set_wnum(&part->mob_load, &part->mob_wnum,
        target->pIndexData->area, target->pIndexData->vnum);
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

// QUESTSCROLL $PLAYER $QUESTGIVER $VNUM $HEADER $FOOTER $WIDTH $PREFIX[ $SUFFIX] $VARIABLENAME
SCRIPT_CMD(scriptcmd_questscroll)
{
    QUEST_DATA *run;
    char *header, *footer, *prefix, *suffix;
    int width;
    char questgiver[MSL];
    char *rest;
    CHAR_DATA *ch;
    long vnum;

    info->progs->lastreturn = 0;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

    ch = arg->d.mob;
    run = quest_runtime_get_focused_run(ch);
    if (!run)
        run = ch->quest;

    // Must be in the generation phase
    if( run == NULL || !run->generating || !run->scripted ) return;

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
    if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
        return;

    AREA_DATA *area = find_area_by_vnum(arg->d.num, NULL);
    if (!area) area = get_system_area_fallback();
    if( arg->d.num < 1 || !get_obj_index(area, arg->d.num))
        return;

    vnum = arg->d.num;

    rest = __get_questscroll_args(info, rest, arg, &header, &footer, &width, &prefix, &suffix);
    if( rest && *rest )
    {
        rest = expand_argument(info,rest,arg);

        if( rest && arg->type == ENT_STRING )
        {
            OBJ_DATA *scroll = generate_quest_scroll(ch, run, questgiver, vnum, header, footer, prefix, suffix, width);
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

SCRIPT_CMD(scriptcmd_raisedead)
{
    char *rest;
    CHAR_DATA *victim;
    ROOM_INDEX_DATA *here = NULL;

    if(!info)
        return;

    if(info->mob) {
        if(!info->mob->in_room)
            return;
        here = info->mob->in_room;
        info->progs->lastreturn = -1;
    } else if(info->token) {
        here = token_room(info->token);
        if(!here)
            return;
    } else
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    switch(arg->type) {
    case ENT_STRING:
        victim = get_char_room(NULL, here, arg->d.str);
        break;
    case ENT_MOBILE:
        victim = arg->d.mob;
        break;
    default:
        victim = NULL;
        break;
    }

    if(!victim)
        return;

    if (!IS_DEAD(victim)) {
        if(info->mob) {
            pbugf(LOG_SCRIPTS, "do_mpraisedead: for mob %s(%ld), victim %s wasn't dead!",
                info->mob->pIndexData->short_descr, info->mob->pIndexData->vnum,
                victim->name);
        } else {
            pbugf(LOG_SCRIPTS, "TpRaisedead: for token %s(%ld), victim %s wasn't dead!",
                info->token->name, VNUM(info->token), victim->name);
        }

        info->progs->lastreturn = 0;
        return;
    }

    resurrect_pc(victim);
    info->progs->lastreturn = 1;
}

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
// REVOKESKILL player vnum
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

    } else if( arg->type == ENT_NUMBER ) {
        token_index = get_token_index_from_info(info, arg->d.num);

        if( !token_index ) return;
    } else if( arg->type == ENT_WIDEVNUM ) {
        token_index = get_token_index(arg->d.wnum.pArea, arg->d.wnum.vnum);

        if( !token_index ) return;
    }
    else
        return;

    if( token_index )
        entry = skill_entry_findtokenindex(mob->sorted_skills, token_index);

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
            if ((!IS_NPC(vch) || !IS_SET(vch->act[1],ACT2_INSTANCE_MOB)) &&
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
            if (!IS_NPC(vch) || !IS_SET(vch->act[1],ACT2_INSTANCE_MOB)) {
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

static bool scriptcmd_parse_number_or_none(SCRIPT_PARAM *arg, int *value)
{
    if (!arg || !value)
        return false;

    if (arg->type == ENT_NUMBER) {
        *value = arg->d.num;
        return true;
    }

    if (arg->type == ENT_STRING && arg->d.str) {
        if (!str_cmp(arg->d.str, "none")) {
            *value = 0;
            return true;
        }

        if (is_number(arg->d.str)) {
            *value = atoi(arg->d.str);
            return true;
        }
    }

    return false;
}

static void scriptcmd_shop_stock(SCRIPT_VARINFO *info, char *argument, SCRIPT_PARAM *arg, SHOP_DATA *shop, SHOP_STOCK_DATA *stock)
{
    char *rest = argument;
    int ret = 1;

    if (!info || !stock)
        return;

    if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_STRING)
        return;

    if (!str_prefix(arg->d.str, "level")) {
        PARSE_ARGTYPE(NUMBER);
        stock->level = arg->d.num;
    } else if (!str_prefix(arg->d.str, "silver")) {
        PARSE_ARGTYPE(NUMBER);
        stock->silver = UMAX(arg->d.num, 0);
    } else if (!str_prefix(arg->d.str, "qp")) {
        PARSE_ARGTYPE(NUMBER);
        stock->qp = UMAX(arg->d.num, 0);
    } else if (!str_prefix(arg->d.str, "dp")) {
        PARSE_ARGTYPE(NUMBER);
        stock->dp = UMAX(arg->d.num, 0);
    } else if (!str_prefix(arg->d.str, "pneuma")) {
        PARSE_ARGTYPE(NUMBER);
        stock->pneuma = UMAX(arg->d.num, 0);
    } else if (!str_prefix(arg->d.str, "price")) {
        int silver = 0, qp = 0, dp = 0, pneuma = 0;

        PARSE_ARGTYPE(NUMBER);
        silver = UMAX(arg->d.num, 0);

        if (rest && *rest) {
            PARSE_ARGTYPE(NUMBER);
            qp = UMAX(arg->d.num, 0);
        }

        if (rest && *rest) {
            PARSE_ARGTYPE(NUMBER);
            dp = UMAX(arg->d.num, 0);
        }

        if (rest && *rest) {
            PARSE_ARGTYPE(NUMBER);
            pneuma = UMAX(arg->d.num, 0);
        }

        stock->silver = silver;
        stock->qp = qp;
        stock->dp = dp;
        stock->pneuma = pneuma;
    } else if (!str_prefix(arg->d.str, "customprice") || !str_prefix(arg->d.str, "custom_price")) {
        if (!PARSE_ARG)
            return;

        if (arg->type == ENT_NULL) {
            free_string(stock->custom_price);
            stock->custom_price = NULL;
        } else if (arg->type == ENT_STRING) {
            free_string(stock->custom_price);
            stock->custom_price = str_dup(arg->d.str);
        } else
            return;
    } else if (!str_prefix(arg->d.str, "keyword") || !str_prefix(arg->d.str, "customkeyword") || !str_prefix(arg->d.str, "custom_keyword")) {
        PARSE_ARGTYPE(STRING);
        free_string(stock->custom_keyword);
        stock->custom_keyword = str_dup(arg->d.str);
    } else if (!str_prefix(arg->d.str, "description") || !str_prefix(arg->d.str, "customdescription") || !str_prefix(arg->d.str, "custom_description")) {
        BUFFER *buffer = new_buf();
        expand_string(info, rest, buffer);
        if (buffer->state == BUFFER_SAFE) {
            free_string(stock->custom_descr);
            stock->custom_descr = str_dup(buffer->string);
        }
        free_buf(buffer);
    } else if (!str_prefix(arg->d.str, "discount")) {
        PARSE_ARGTYPE(NUMBER);
        if (arg->d.num < 0 || arg->d.num > 100)
            return;
        stock->discount = arg->d.num;
    } else if (!str_prefix(arg->d.str, "quantity")) {
        PARSE_ARGTYPE(NUMBER);
        stock->quantity = UMAX(arg->d.num, 0);
    } else if (!str_prefix(arg->d.str, "maxquantity") || !str_prefix(arg->d.str, "max_quantity")) {
        PARSE_ARGTYPE(NUMBER);
        stock->max_quantity = UMAX(arg->d.num, 0);
        if (stock->quantity > stock->max_quantity)
            stock->quantity = stock->max_quantity;
    } else if (!str_prefix(arg->d.str, "restock") || !str_prefix(arg->d.str, "restockrate") || !str_prefix(arg->d.str, "restock_rate")) {
        PARSE_ARGTYPE(NUMBER);
        stock->restock_rate = UMAX(arg->d.num, 0);
    } else if (!str_prefix(arg->d.str, "duration")) {
        PARSE_ARGTYPE(NUMBER);
        stock->duration = UMAX(arg->d.num, 0);
    } else if (!str_prefix(arg->d.str, "remove")) {
        SHOP_STOCK_DATA *prev = NULL;

        if (!shop)
            return;

        for (SHOP_STOCK_DATA *s = shop->stock; s && s != stock; prev = s, s = s->next);

        if (prev == NULL)
            shop->stock = stock->next;
        else
            prev->next = stock->next;

        free_shop_stock(stock);
    } else if (!str_prefix(arg->d.str, "reputation")) {
        if (!PARSE_ARG)
            return;

        if (arg->type == ENT_NULL) {
            stock->reputation = NULL;
            stock->min_reputation_rank = 0;
            stock->max_reputation_rank = 0;
            stock->min_show_rank = 0;
            stock->max_show_rank = 0;
        } else if (arg->type == ENT_REPUTATION_INDEX) {
            REPUTATION_INDEX_DATA *rep = arg->d.repIndex;
            int min_rank = 0;
            int max_rank = 0;
            int min_show = 0;
            int max_show = 0;

            if (rest && *rest) {
                if (!PARSE_ARG || !scriptcmd_parse_number_or_none(arg, &min_rank))
                    return;

                if (rest && *rest) {
                    if (!PARSE_ARG || !scriptcmd_parse_number_or_none(arg, &max_rank))
                        return;

                    if (rest && *rest) {
                        if (!PARSE_ARG || !scriptcmd_parse_number_or_none(arg, &min_show))
                            return;

                        if (rest && *rest) {
                            if (!PARSE_ARG || !scriptcmd_parse_number_or_none(arg, &max_show))
                                return;
                        }
                    }
                }
            }

            stock->reputation = rep;
            stock->min_reputation_rank = UMAX(min_rank, 0);
            stock->max_reputation_rank = UMAX(max_rank, 0);
            stock->min_show_rank = UMAX(min_show, 0);
            stock->max_show_rank = UMAX(max_show, 0);
        } else
            return;
    } else if (!str_prefix(arg->d.str, "singular")) {
        bool state = !stock->singular;
        if (rest && *rest) {
            PARSE_ARGTYPE(BOOLEAN);
            state = arg->d.boolean;
        }
        stock->singular = state;
    } else
        return;

    SETRETURN(ret);
}

// SETCLASSLEVEL $PLAYER $CLASSNAME[ $LEVEL]
SCRIPT_CMD(scriptcmd_setclasslevel)
{
    char *rest = argument;

    SETRETURN(0);

    if (script_security < 5)
        return;

    PARSE_ARGTYPE(MOBILE);
    CHAR_DATA *victim = arg->d.mob;
    if (!IS_VALID(victim) || IS_NPC(victim))
        return;

    PARSE_ARGTYPE(STRING);

    if (!str_prefix(arg->d.str, "current")) {
        if (script_security < 9)
            return;

        CLASS_LEVEL *cl = get_class_level(victim, NULL);
        if (!cl)
            return;

        PARSE_ARGTYPE(NUMBER);
        int level = arg->d.num;

        if (level < cl->level || level > cl->clazz->max_level)
            return;

        add_class_level(victim, cl->clazz, level);
    } else {
        CLASS_DATA *clazz = class_find(arg->d.str);
        if (!IS_VALID(clazz))
            return;

        int level = 1;
        if (rest && *rest) {
            PARSE_ARGTYPE(NUMBER);
            level = arg->d.num;

            if (level < 1 || level > clazz->max_level)
                return;

            if (level > 1 && script_security < 9)
                return;

            CLASS_LEVEL *cl = get_class_level(victim, clazz);
            if (cl && level < cl->level)
                return;
        }

        add_class_level(victim, clazz, level);
    }

    save_char_obj(victim);
    SETRETURN(1);
}

// SETPOSITION $MOBILE $POSITION
SCRIPT_CMD(scriptcmd_setposition)
{
    char *rest = argument;
    CHAR_DATA *ch;
    int position;

    SETRETURN(-1);

    if (script_security < 5)
        return;

    PARSE_ARGTYPE(MOBILE);
    if (!IS_VALID(arg->d.mob))
        return;
    ch = arg->d.mob;

    PARSE_ARGTYPE(STRING);
    if (!str_prefix(arg->d.str, "feign"))
        position = POS_FEIGN;
    else if (!str_prefix(arg->d.str, "resting"))
        position = POS_RESTING;
    else if (!str_prefix(arg->d.str, "sitting"))
        position = POS_SITTING;
    else if (!str_prefix(arg->d.str, "sleeping"))
        position = POS_SLEEPING;
    else if (!str_prefix(arg->d.str, "standing"))
        position = POS_STANDING;
    else
        return;

    ch->position = position;
    SETRETURN(ch->position);
}

SCRIPT_CMD(scriptcmd_shop)
{
    char *rest = argument;
    CHAR_DATA *mob;

    if (!info)
        return;

    SETRETURN(0);

    if (!PARSE_ARG)
        return;

    if (arg->type == ENT_MOBILE) {
        mob = arg->d.mob;
        if (!IS_VALID(mob) || !IS_NPC(mob))
            return;

        PARSE_ARGTYPE(STRING);

        SHOP_DATA *shop = mob->shop;

        if (!str_prefix(arg->d.str, "add")) {
            if (shop)
                return;

            mob->shop = new_shop();
            mob->shop->keeper = VNUM(mob);
        } else if (!str_prefix(arg->d.str, "remove")) {
            if (!shop)
                return;

            free_shop(shop);
            mob->shop = NULL;
        } else if (!str_prefix(arg->d.str, "deplete")) {
            if (!shop)
                return;

            for (SHOP_STOCK_DATA *stock = shop->stock; stock; stock = stock->next)
                if (stock->max_quantity > 0)
                    stock->quantity = 0;
        } else if (!str_prefix(arg->d.str, "discount")) {
            bool reset_defaults = false;
            if (!shop)
                return;

            PARSE_ARGTYPE(NUMBER);
            int percent = arg->d.num;
            if (percent < 0 || percent > 100)
                return;

            if (rest && *rest) {
                if (!PARSE_ARG)
                    return;

                if (arg->type == ENT_STRING)
                    reset_defaults = !str_prefix(arg->d.str, "reset");
                else if (arg->type == ENT_BOOLEAN)
                    reset_defaults = arg->d.boolean;
                else
                    return;
            }

            shop->discount = percent;
            if (reset_defaults)
                for (SHOP_STOCK_DATA *stock = shop->stock; stock; stock = stock->next)
                    if (IS_NULLSTR(stock->custom_keyword))
                        stock->discount = shop->discount;
        } else if (!str_prefix(arg->d.str, "flags")) {
            if (!shop)
                return;

            char ops[MIL];
            rest = one_argument(rest, ops);
            int op = cmd_operator_lookup(ops);
            if (op == OPR_UNKNOWN)
                return;

            PARSE_ARGTYPE(STRING);
            long value = script_flag_value(shop_flags, arg->d.str);
            if (value == NO_FLAG)
                return;

            switch (op) {
                case OPR_ASSIGN: shop->flags = value; break;
                case OPR_AND:    shop->flags &= value; break;
                case OPR_OR:     shop->flags |= value; break;
                case OPR_NOT:    shop->flags &= ~value; break;
                case OPR_XOR:    shop->flags ^= value; break;
            }
        } else if (!str_prefix(arg->d.str, "hours")) {
            if (!shop)
                return;

            PARSE_ARGTYPE(NUMBER);
            int open = arg->d.num;
            if (open < 0 || open > 23)
                return;

            PARSE_ARGTYPE(NUMBER);
            int close = arg->d.num;
            if (close < 0 || close > 23)
                return;

            shop->open_hour = open;
            shop->close_hour = close;
        } else if (!str_prefix(arg->d.str, "profit")) {
            if (!shop)
                return;

            PARSE_ARGTYPE(NUMBER);
            int buy = arg->d.num;
            if (buy < 0 || buy > 200)
                return;

            PARSE_ARGTYPE(NUMBER);
            int sell = arg->d.num;
            if (sell < 0 || sell > 200)
                return;

            shop->profit_buy = buy;
            shop->profit_sell = sell;
        } else if (!str_prefix(arg->d.str, "reputation")) {
            if (!shop)
                return;

            if (!PARSE_ARG)
                return;

            if (arg->type == ENT_NULL) {
                shop->reputation = NULL;
                shop->min_reputation_rank = 0;
            } else if (arg->type == ENT_REPUTATION_INDEX) {
                int min_rank = 0;
                REPUTATION_INDEX_DATA *rep = arg->d.repIndex;

                if (rep && rest && *rest) {
                    PARSE_ARGTYPE(NUMBER);
                    min_rank = arg->d.num;
                    if (min_rank > list_size(rep->ranks))
                        return;
                }

                if (min_rank < 0)
                    return;

                shop->reputation = rep;
                shop->min_reputation_rank = min_rank;
            } else
                return;
        } else if (!str_prefix(arg->d.str, "restock")) {
            if (!shop)
                return;

            if (!PARSE_ARG)
                return;

            if (arg->type == ENT_NUMBER) {
                if (arg->d.num < 0)
                    return;

                shop->restock_interval = arg->d.num;
            } else if (arg->type == ENT_STRING) {
                if (str_prefix(arg->d.str, "force"))
                    return;

                bool full = false;
                bool restocked = false;

                if (rest && *rest) {
                    PARSE_ARGTYPE(BOOLEAN);
                    full = arg->d.boolean;
                }

                for (SHOP_STOCK_DATA *stock = shop->stock; stock; stock = stock->next) {
                    if (stock->max_quantity > 0 && stock->quantity < stock->max_quantity) {
                        if (full) {
                            stock->quantity = stock->max_quantity;
                            restocked = true;
                        } else if (stock->restock_rate > 0) {
                            stock->quantity += stock->restock_rate;
                            stock->quantity = UMIN(stock->quantity, stock->max_quantity);
                            restocked = true;
                        }
                    }
                }

                if (restocked)
                    p_percent_trigger(mob, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RESTOCKED, NULL);

                if (shop->restock_interval > 0)
                    shop->next_restock = current_time + shop->restock_interval * 60;
            } else
                return;
        } else if (!str_prefix(arg->d.str, "type")) {
            if (!shop)
                return;

            PARSE_ARGTYPE(NUMBER);
            int index = arg->d.num;
            if (index < 1 || index > MAX_TRADE)
                return;

            PARSE_ARGTYPE(STRING);
            int type = stat_lookup(arg->d.str, type_flags, NO_FLAG);
            if (type == NO_FLAG)
                return;

            shop->buy_type[index - 1] = type;
        } else if (!str_prefix(arg->d.str, "stock")) {
            if (!shop)
                return;

            if (!PARSE_ARG)
                return;

            if (arg->type == ENT_NUMBER) {
                int nth = arg->d.num;
                SHOP_STOCK_DATA *stock;

                if (nth < 1)
                    return;

                for (stock = shop->stock; (--nth) > 0 && stock; stock = stock->next);

                scriptcmd_shop_stock(info, rest, arg, shop, stock);
                return;
            }

            if (arg->type == ENT_STRING) {
                if (!str_prefix(arg->d.str, "clear")) {
                    SHOP_STOCK_DATA *stock, *next;
                    for (stock = shop->stock; stock; stock = next) {
                        next = stock->next;
                        free_shop_stock(stock);
                    }
                    shop->stock = NULL;
                } else if (!str_prefix(arg->d.str, "add")) {
                    SHOP_STOCK_DATA *stock = NULL;

                    PARSE_ARGTYPE(STRING);
                    int stock_type = stat_lookup(arg->d.str, stock_types, NO_FLAG);
                    if (stock_type == NO_FLAG)
                        return;

                    if (stock_type == STOCK_CUSTOM) {
                        BUFFER *buffer = new_buf();
                        expand_string(info, rest, buffer);

                        if (buffer->state == BUFFER_SAFE && !IS_NULLSTR(buffer->string)) {
                            SHOP_STOCK_DATA *existing;
                            for (existing = shop->stock; existing; existing = existing->next)
                                if (existing->type == STOCK_CUSTOM && !str_cmp(buffer->string, existing->custom_keyword))
                                    break;

                            if (!existing) {
                                stock = new_shop_stock();
                                if (stock) {
                                    stock->type = STOCK_CUSTOM;
                                    stock->custom_keyword = str_dup(buffer->string);
                                    stock->discount = shop->discount;
                                }
                            }
                        }

                        free_buf(buffer);
                    } else if (stock_type == STOCK_OBJECT || stock_type == STOCK_PET || stock_type == STOCK_MOUNT || stock_type == STOCK_GUARD || stock_type == STOCK_CREW || stock_type == STOCK_SHIP) {
                        if (!PARSE_ARG)
                            return;

                        stock = new_shop_stock();
                        if (!stock)
                            return;

                        stock->type = stock_type;
                        stock->discount = shop->discount;

                        if (stock_type == STOCK_OBJECT) {
                            OBJ_INDEX_DATA *obj = NULL;

                            if (arg->type == ENT_WIDEVNUM)
                                obj = get_obj_index(arg->d.wnum.pArea, arg->d.wnum.vnum);
                            else if (arg->type == ENT_OBJECT)
                                obj = IS_VALID(arg->d.obj) ? arg->d.obj->pIndexData : NULL;
                            else if (arg->type == ENT_OBJINDEX)
                                obj = arg->d.objindex;

                            if (!obj || IS_MONEY(obj)) {
                                free_shop_stock(stock);
                                return;
                            }

                            stock->obj = obj;
                            stock->entity.wnum.pArea = obj->area;
                            stock->entity.wnum.vnum = obj->vnum;
                            stock->silver = obj->cost;
                        } else if (stock_type == STOCK_SHIP) {
                            SHIP_INDEX_DATA *ship = NULL;

                            if (arg->type == ENT_WIDEVNUM)
                                ship = get_ship_index_for_area(arg->d.wnum.pArea, arg->d.wnum.vnum);
                            else if (arg->type == ENT_SHIP)
                                ship = IS_VALID(arg->d.ship) ? arg->d.ship->index : NULL;
                            else if (arg->type == ENT_SHIPINDEX)
                                ship = arg->d.ship_index;

                            if (!ship) {
                                free_shop_stock(stock);
                                return;
                            }

                            stock->ship = ship;
                            stock->entity.wnum.pArea = ship->area;
                            stock->entity.wnum.vnum = ship->vnum;
                            stock->silver = 100000;
                            stock->level = 1;
                        } else {
                            MOB_INDEX_DATA *mob_index = NULL;

                            if (arg->type == ENT_WIDEVNUM)
                                mob_index = get_mob_index(arg->d.wnum.pArea, arg->d.wnum.vnum);
                            else if (arg->type == ENT_MOBILE)
                                mob_index = (IS_VALID(arg->d.mob) && IS_NPC(arg->d.mob)) ? arg->d.mob->pIndexData : NULL;
                            else if (arg->type == ENT_MOBINDEX)
                                mob_index = arg->d.mobindex;

                            if (!mob_index) {
                                free_shop_stock(stock);
                                return;
                            }

                            stock->mob = mob_index;
                            stock->entity.wnum.pArea = mob_index->area;
                            stock->entity.wnum.vnum = mob_index->vnum;
                            if (stock_type == STOCK_PET)
                                stock->silver = 10 * mob_index->level * mob_index->level;
                            else if (stock_type == STOCK_MOUNT)
                                stock->silver = 25 * mob_index->level * mob_index->level;
                            else
                                stock->silver = 50 * mob_index->level * mob_index->level;
                        }
                    }

                    if (!stock)
                        return;

                    stock->next = shop->stock;
                    shop->stock = stock;
                } else
                    return;
            } else
                return;
        } else
            return;

        SETRETURN(1);
    } else if (arg->type == ENT_SHOP_STOCK) {
        scriptcmd_shop_stock(info, rest, arg, NULL, arg->d.stock);
    }
}

// SETCLASS $MOBILE $CLASSNAME
// Switches a player's active class (they must already have the class).
// Follows the same pattern as do_setclass in act_class.c.
SCRIPT_CMD(scriptcmd_setclass)
{
    char *rest;
    CHAR_DATA *mob;
    CLASS_DATA *clazz;
    CLASS_LEVEL *cl;
    OBJ_DATA *obj, *obj_next;

    info->progs->lastreturn = 0;

    if (!(rest = expand_argument(info, argument, arg)))
        return;

    if (arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob))
        return;

    mob = arg->d.mob;

    if (!(rest = expand_argument(info, rest, arg)))
        return;

    if (arg->type != ENT_STRING || !arg->d.str || !arg->d.str[0])
        return;

    clazz = class_find(arg->d.str);
    if (!clazz)
        return;

    cl = get_class_level(mob, clazz);
    if (!cl)
        return;

    /* Already in this class */
    if (mob->pcdata->current_class == cl)
        return;

    /* Revoke old class rewards */
    if (mob->pcdata->current_class && mob->pcdata->current_class->clazz) {
        CLASS_DATA *old_class = mob->pcdata->current_class->clazz;
        if (old_class->leave)
            (*old_class->leave)(mob);
        revoke_class_rewards(mob, old_class);
    }

    /* Switch */
    mob->pcdata->current_class = cl;

    /* Enter new class */
    if (cl->clazz->enter)
        (*cl->clazz->enter)(mob);

    apply_class_rewards(mob, cl->clazz, 1, cl->level, true);

    /* Unequip gear above new class level */
    for (obj = mob->carrying; obj != NULL; obj = obj_next) {
        obj_next = obj->next_content;
        if (obj->wear_loc != WEAR_NONE && cl->level < obj->level)
            unequip_char(mob, obj, true);
    }

    save_char_obj(mob);
    info->progs->lastreturn = 1;
}

// SETRACE $MOBILE $RACENAME
// Changes a player's race and updates racial attributes.
SCRIPT_CMD(scriptcmd_setrace)
{
    char *rest;
    CHAR_DATA *mob;
    RACE_DATA *race;

    info->progs->lastreturn = 0;

    if (!(rest = expand_argument(info, argument, arg)))
        return;

    if (arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob))
        return;

    mob = arg->d.mob;

    if (!(rest = expand_argument(info, rest, arg)))
        return;

    if (arg->type != ENT_STRING || !arg->d.str || !arg->d.str[0])
        return;

    race = race_lookup_name(arg->d.str);
    if (!race)
        race = race_lookup(arg->d.str);
    if (!race || !race->playable)
        return;

    char_set_race(mob, race, RACE_CHANGE_SILENT);

    save_char_obj(mob);
    info->progs->lastreturn = 1;
}

// GRANTCLASS $MOBILE $CLASSNAME[ $LEVEL]
// Grants a class to a player at the given level (default 1).
SCRIPT_CMD(scriptcmd_grantclass)
{
    char *rest;
    CHAR_DATA *mob;
    CLASS_DATA *clazz;
    int level = 1;

    info->progs->lastreturn = 0;

    if (!(rest = expand_argument(info, argument, arg)))
        return;

    if (arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob))
        return;

    mob = arg->d.mob;

    if (!(rest = expand_argument(info, rest, arg)))
        return;

    if (arg->type != ENT_STRING || !arg->d.str || !arg->d.str[0])
        return;

    clazz = class_find(arg->d.str);
    if (!clazz)
        return;

    /* Already has this class */
    if (has_class_level(mob, clazz))
        return;

    /* Optional level */
    if (*rest) {
        if (!(rest = expand_argument(info, rest, arg)))
            return;
        if (arg->type == ENT_NUMBER)
            level = URANGE(1, arg->d.num, clazz->max_level);
    }

    add_class_level(mob, clazz, level);

    save_char_obj(mob);
    info->progs->lastreturn = 1;
}

// REVOKECLASS $MOBILE $CLASSNAME
// Removes a class from a player, revoking its rewards.
SCRIPT_CMD(scriptcmd_revokeclass)
{
    char *rest;
    CHAR_DATA *mob;
    CLASS_DATA *clazz;

    info->progs->lastreturn = 0;

    if (!(rest = expand_argument(info, argument, arg)))
        return;

    if (arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob))
        return;

    mob = arg->d.mob;

    if (!(rest = expand_argument(info, rest, arg)))
        return;

    if (arg->type != ENT_STRING || !arg->d.str || !arg->d.str[0])
        return;

    clazz = class_find(arg->d.str);
    if (!clazz)
        return;

    if (!has_class_level(mob, clazz))
        return;

    /* If revoking the current class, switch away first */
    if (mob->pcdata->current_class
        && mob->pcdata->current_class->clazz == clazz) {
        if (clazz->leave)
            (*clazz->leave)(mob);
        revoke_class_rewards(mob, clazz);
        mob->pcdata->current_class = NULL;
    }

    remove_class_level(mob, clazz);

    save_char_obj(mob);
    info->progs->lastreturn = 1;
}

// GRANTSONG $MOBILE $SONGNAME
// Grants a song to a player.
SCRIPT_CMD(scriptcmd_grantsong)
{
    char *rest;
    CHAR_DATA *mob;
    SONG_DATA *song;

    info->progs->lastreturn = 0;

    if (!(rest = expand_argument(info, argument, arg)))
        return;

    if (arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob))
        return;

    mob = arg->d.mob;

    if (!(rest = expand_argument(info, rest, arg)))
        return;

    if (arg->type != ENT_STRING || !arg->d.str || !arg->d.str[0])
        return;

    song = song_lookup(arg->d.str);
    if (!song)
        return;

    /* Already has this song */
    if (skill_entry_findsong(mob->sorted_songs, song))
        return;

    skill_entry_addsong(mob, song, NULL, SKILLSRC_SCRIPT);

    save_char_obj(mob);
    info->progs->lastreturn = 1;
}

// REVOKESONG $MOBILE $SONGNAME
// Removes a song from a player.
SCRIPT_CMD(scriptcmd_revokesong)
{
    char *rest;
    CHAR_DATA *mob;
    SONG_DATA *song;
    SKILL_ENTRY *entry;

    info->progs->lastreturn = 0;

    if (!(rest = expand_argument(info, argument, arg)))
        return;

    if (arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob))
        return;

    mob = arg->d.mob;

    if (!(rest = expand_argument(info, rest, arg)))
        return;

    if (arg->type != ENT_STRING || !arg->d.str || !arg->d.str[0])
        return;

    song = song_lookup(arg->d.str);
    if (!song)
        return;

    entry = skill_entry_findsong(mob->sorted_songs, song);
    if (!entry)
        return;

    skill_entry_removeentry(&mob->sorted_songs, entry);

    save_char_obj(mob);
    info->progs->lastreturn = 1;
}

// SETTRAIT $MOBILE $TRAITID $VALUE
// Sets a personal trait override on a player.
// For bool traits: 0/false/no = false, anything else = true.
// For int traits: numeric value.
// For string traits: string value (empty string to clear).
SCRIPT_CMD(scriptcmd_settrait)
{
    char *rest;
    CHAR_DATA *mob;
    TRAIT_DEF *def;

    info->progs->lastreturn = 0;

    if (!(rest = expand_argument(info, argument, arg)))
        return;

    if (arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob))
        return;

    mob = arg->d.mob;

    if (!(rest = expand_argument(info, rest, arg)))
        return;

    if (arg->type != ENT_STRING || !arg->d.str || !arg->d.str[0])
        return;

    def = trait_def_lookup(arg->d.str);
    if (!def)
        return;

    if (!(rest = expand_argument(info, rest, arg)))
        return;

    switch (def->type) {
        case TRAIT_BOOLEAN: {
            bool val;
            if (arg->type == ENT_BOOLEAN)
                val = arg->d.boolean;
            else if (arg->type == ENT_NUMBER)
                val = (arg->d.num != 0);
            else if (arg->type == ENT_STRING)
                val = (!str_cmp(arg->d.str, "true")
                    || !str_cmp(arg->d.str, "yes")
                    || !str_cmp(arg->d.str, "1"));
            else
                return;
            if (!ch_set_trait_bool(mob, def->id, val))
                return;
            break;
        }

        case TRAIT_INTEGER: {
            int val;
            if (arg->type == ENT_NUMBER)
                val = arg->d.num;
            else if (arg->type == ENT_STRING)
                val = atoi(arg->d.str);
            else
                return;
            if (!ch_set_trait_int(mob, def->id, val))
                return;
            break;
        }

        case TRAIT_STRING:
            if (arg->type != ENT_STRING)
                return;
            if (!ch_set_trait_string(mob, def->id,
                    (arg->d.str[0] ? arg->d.str : NULL)))
                return;
            break;

        default:
            return;
    }

    save_char_obj(mob);
    info->progs->lastreturn = 1;
}

// ADDTRAIT $MOBILE $TRAITID [$VALUE]
// Adds or boosts a personal trait override on a player.
// Boolean: sets to true.
// Integer: adds VALUE (default 1).
// String: sets to VALUE (required).
SCRIPT_CMD(scriptcmd_addtrait)
{
    char *rest;
    CHAR_DATA *mob;
    TRAIT_DEF *def;

    info->progs->lastreturn = 0;

    if (!(rest = expand_argument(info, argument, arg)))
        return;

    if (arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob))
        return;

    mob = arg->d.mob;

    if (!(rest = expand_argument(info, rest, arg)))
        return;

    if (arg->type != ENT_STRING || !arg->d.str || !arg->d.str[0])
        return;

    def = trait_def_lookup_name(arg->d.str);
    if (!def)
        return;

    switch (def->type) {
        case TRAIT_BOOLEAN:
            if (!ch_set_trait_bool(mob, def->id, true))
                return;
            break;

        case TRAIT_INTEGER: {
            int delta = 1;
            int next_value;

            if (rest && *rest) {
                if (!(rest = expand_argument(info, rest, arg)))
                    return;

                if (arg->type == ENT_NUMBER)
                    delta = arg->d.num;
                else if (arg->type == ENT_STRING)
                    delta = atoi(arg->d.str);
                else
                    return;
            }

            next_value = ch_get_trait_int(mob, def->id) + delta;
            if (!ch_set_trait_int(mob, def->id, next_value))
                return;
            break;
        }

        case TRAIT_STRING:
            if (!(rest && *rest))
                return;
            if (!(rest = expand_argument(info, rest, arg)))
                return;
            if (arg->type != ENT_STRING)
                return;
            if (!ch_set_trait_string(mob, def->id, (arg->d.str[0] ? arg->d.str : NULL)))
                return;
            break;

        default:
            return;
    }

    save_char_obj(mob);
    info->progs->lastreturn = 1;
}

// ADJUSTTRAIT $MOBILE $TRAITID $DELTA
// Adjusts an integer personal trait override by DELTA.
SCRIPT_CMD(scriptcmd_adjusttrait)
{
    char *rest;
    CHAR_DATA *mob;
    TRAIT_DEF *def;
    int delta;
    int next_value;

    info->progs->lastreturn = 0;

    if (!(rest = expand_argument(info, argument, arg)))
        return;

    if (arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob))
        return;

    mob = arg->d.mob;

    if (!(rest = expand_argument(info, rest, arg)))
        return;

    if (arg->type != ENT_STRING || !arg->d.str || !arg->d.str[0])
        return;

    def = trait_def_lookup_name(arg->d.str);
    if (!def || def->type != TRAIT_INTEGER)
        return;

    if (!(rest = expand_argument(info, rest, arg)))
        return;

    if (arg->type == ENT_NUMBER)
        delta = arg->d.num;
    else if (arg->type == ENT_STRING)
        delta = atoi(arg->d.str);
    else
        return;

    next_value = ch_get_trait_int(mob, def->id) + delta;
    if (!ch_set_trait_int(mob, def->id, next_value))
        return;

    save_char_obj(mob);
    info->progs->lastreturn = 1;
}

// REMOVETRAIT $MOBILE $TRAITID
// Removes personal trait override for a player.
SCRIPT_CMD(scriptcmd_removetrait)
{
    char *rest;
    CHAR_DATA *mob;
    TRAIT_DEF *def;

    info->progs->lastreturn = 0;

    if (!(rest = expand_argument(info, argument, arg)))
        return;

    if (arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob))
        return;

    mob = arg->d.mob;

    if (!(rest = expand_argument(info, rest, arg)))
        return;

    if (arg->type != ENT_STRING || !arg->d.str || !arg->d.str[0])
        return;

    def = trait_def_lookup_name(arg->d.str);
    if (!def)
        return;

    if (!ch_clear_trait(mob, def->id))
        return;

    save_char_obj(mob);
    info->progs->lastreturn = 1;
}

SCRIPT_CMD(scriptcmd_setsubclass)
{
    /* Deprecated — subclass system removed. Kept as no-op for compatibility. */
}

SCRIPT_CMD(scriptcmd_settimer)
{
    char buf[MIL], *rest;
    int amt;
    CHAR_DATA *victim = NULL;
    const char *scope_name;
    long scope_vnum;
    bool use_mob_context = false;

    if(!info)
        return;

    if(info->mob) {
        scope_name = "MpSetTimer";
        scope_vnum = VNUM(info->mob);
        use_mob_context = true;
    } else if(info->obj) {
        scope_name = "OpSetTimer";
        scope_vnum = VNUM(info->obj);
    } else if(info->room) {
        scope_name = "RpSetTimer";
        scope_vnum = info->room->vnum;
    } else if(info->token) {
        scope_name = "TpSetTimer";
        scope_vnum = info->room ? info->room->vnum : 0;
    } else
        return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        if(use_mob_context && !str_cmp(arg->d.str, "self"))
            victim = info->mob;
        else
            victim = get_char_world(use_mob_context ? info->mob : NULL, arg->d.str);
        break;
    case ENT_MOBILE:
        victim = arg->d.mob;
        break;
    default:
        break;
    }

    if(!victim) {
        pbugf(LOG_SCRIPTS, "%s - NULL victim from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "%s - Missing timer type from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    buf[0] = 0;
    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        strlcpy(buf, arg->d.str, sizeof(buf));
        break;
    default:
        break;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "%s - Missing timer amount from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
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
                victim->quest_runtime.expiry_modes |= QUEST_EXPIRY_COUNTDOWN;
                victim->quest_runtime.expiry_countdown_minutes = amt;
                victim->countdown = amt;
            }
        }
        else if(!str_cmp(buf,"nextquest"))
        {
            if(!IS_NPC(victim))
            {
                if (amt > 0)
                {
                    time_t cooldown_until = current_time + (time_t)amt * 60;

                    if (victim->quest_runtime.mission_allowance > 0)
                        victim->quest_runtime.mission_allowance = 0;

                    if (victim->quest_runtime.allowance_last_update < cooldown_until)
                        victim->quest_runtime.allowance_last_update = cooldown_until;
                }
                else
                {
                    victim->quest_runtime.allowance_last_update = current_time;
                }

                victim->nextquest = 0;
            }
        }
    }
}

SCRIPT_CMD(scriptcmd_setrecall)
{
    char *rest;
    CHAR_DATA *victim;
    ROOM_INDEX_DATA *room;
    ROOM_INDEX_DATA *location;
    char *(*getlocation_func)(SCRIPT_VARINFO *, char *, ROOM_INDEX_DATA **) = NULL;
    const char *scope_name = NULL;
    long scope_vnum = 0;
    CHAR_DATA *searcher = NULL;

    if(!info)
        return;

    if(info->mob) {
        scope_name = "MpSetRecall";
        scope_vnum = VNUM(info->mob);
        getlocation_func = mp_getlocation;
        searcher = info->mob;
    } else if(info->obj) {
        scope_name = "OpSetRecall";
        scope_vnum = VNUM(info->obj);
        getlocation_func = op_getlocation;
    } else if(info->room) {
        scope_name = "RpSetRecall";
        scope_vnum = info->room->vnum;
        getlocation_func = rp_getlocation;
    } else if(info->token) {
        scope_name = "TpSetRecall";
        scope_vnum = VNUM(info->token);
        getlocation_func = tp_getlocation;
    } else
        return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Bad syntax from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    victim = NULL;
    room = NULL;

    switch(arg->type) {
    case ENT_STRING:
        victim = get_char_world(searcher, arg->d.str);
        break;
    case ENT_MOBILE:
        victim = arg->d.mob;
        break;
    case ENT_ROOM:
        room = arg->d.room;
        break;
    default:
        break;
    }

    if (!victim && !room) {
        pbugf(LOG_SCRIPTS, "%s - Null victim from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    argument = getlocation_func(info, rest, &location);

    if(!location) {
        pbugf(LOG_SCRIPTS, "%s - Bad location from vnum %ld.", scope_name, scope_vnum);
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

SCRIPT_CMD(scriptcmd_condition)
{
    char *rest;
    CHAR_DATA *mob = NULL;
    int cond, value;

    if(!info || IS_NULLSTR(argument))
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(arg->type != ENT_MOBILE)
        return;

    mob = arg->d.mob;

    if(!mob || IS_NPC(mob))
        return;

    if(!*rest)
        return;

    if(!(rest = expand_argument(info,rest,arg)))
        return;

    switch(arg->type) {
    case ENT_STRING:
        if(!str_cmp(arg->d.str,"drunk"))
            cond = COND_DRUNK;
        else if(!str_cmp(arg->d.str,"full"))
            cond = COND_FULL;
        else if(!str_cmp(arg->d.str,"thirst"))
            cond = COND_THIRST;
        else if(!str_cmp(arg->d.str,"hunger"))
            cond = COND_HUNGER;
        else if(!str_cmp(arg->d.str,"stoned"))
            cond = COND_STONED;
        else
            return;
        break;
    default:
        return;
    }

    if(!*rest)
        return;

    if(!(rest = expand_argument(info,rest,arg)))
        return;

    switch(arg->type) {
    case ENT_STRING:
        value = is_number(arg->d.str) ? atoi(arg->d.str) : 0;
        break;
    case ENT_NUMBER:
        value = arg->d.num;
        break;
    default:
        return;
    }

    if(script_security < 9)
    {
        if(value < -1)
            value = -1;
        else if(value > 48)
            value = 48;
    }

    gain_condition(mob, cond, value);
}

SCRIPT_CMD(scriptcmd_stripaffect)
{
    char *rest;
    int skill;
    CHAR_DATA *mob = NULL;
    OBJ_DATA *obj = NULL;
    const char *scope_name = NULL;
    long scope_vnum = 0;
    CHAR_DATA *searcher = NULL;
    ROOM_INDEX_DATA *search_room = NULL;

    if(!info)
        return;

    if(info->mob) {
        scope_name = "MpStripaffect";
        scope_vnum = VNUM(info->mob);
        searcher = info->mob;
    } else if(info->obj) {
        scope_name = "OpStripaffect";
        scope_vnum = VNUM(info->obj);
        search_room = obj_room(info->obj);
    } else if(info->room) {
        scope_name = "RpStripaffect";
        scope_vnum = info->room->vnum;
        search_room = info->room;
    } else if(info->token) {
        scope_name = "TpStripaffect";
        scope_vnum = VNUM(info->token);
        search_room = token_room(info->token);
    } else
        return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        if(search_room) {
            if(!(mob = get_char_room(NULL, search_room, arg->d.str)))
                obj = get_obj_here(NULL, search_room, arg->d.str);
        } else {
            if(!(mob = get_char_room(searcher, NULL, arg->d.str)))
                obj = get_obj_here(searcher, NULL, arg->d.str);
        }
        break;
    case ENT_MOBILE:
        mob = arg->d.mob;
        break;
    case ENT_OBJECT:
        obj = arg->d.obj;
        break;
    default:
        break;
    }

    if(!mob && !obj) {
        pbugf(LOG_SCRIPTS, "%s - NULL target from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        skill = skill_lookup(arg->d.str);
        break;
    default:
        return;
    }

    if(skill < 0)
        return;

    if(mob)
        affect_strip(mob, skill);
    else
        affect_strip_obj(obj, skill);
}

SCRIPT_CMD(scriptcmd_stripaffectname)
{
    char *rest, *name;
    CHAR_DATA *mob = NULL;
    OBJ_DATA *obj = NULL;
    const char *scope_name = NULL;
    long scope_vnum = 0;
    CHAR_DATA *searcher = NULL;
    ROOM_INDEX_DATA *search_room = NULL;

    if(!info)
        return;

    if(info->mob) {
        scope_name = "MpStripaffect";
        scope_vnum = VNUM(info->mob);
        searcher = info->mob;
    } else if(info->obj) {
        scope_name = "OpStripaffect";
        scope_vnum = VNUM(info->obj);
        search_room = obj_room(info->obj);
    } else if(info->room) {
        scope_name = "RpStripaffect";
        scope_vnum = info->room->vnum;
        search_room = info->room;
    } else if(info->token) {
        scope_name = "TpStripaffect";
        scope_vnum = VNUM(info->token);
        search_room = token_room(info->token);
    } else
        return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        if(search_room) {
            if(!(mob = get_char_room(NULL, search_room, arg->d.str)))
                obj = get_obj_here(NULL, search_room, arg->d.str);
        } else {
            if(!(mob = get_char_room(searcher, NULL, arg->d.str)))
                obj = get_obj_here(searcher, NULL, arg->d.str);
        }
        break;
    case ENT_MOBILE:
        mob = arg->d.mob;
        break;
    case ENT_OBJECT:
        obj = arg->d.obj;
        break;
    default:
        break;
    }

    if(!mob && !obj) {
        pbugf(LOG_SCRIPTS, "%s - NULL target from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        name = get_affect_cname(arg->d.str);
        break;
    default:
        return;
    }

    if(!name)
        return;

    if(mob)
        affect_strip_name(mob, name);
    else
        affect_strip_name_obj(obj, name);
}

// addspell $OBJECT STRING[ NUMBER]
SCRIPT_CMD(scriptcmd_addspell)
{
    char *rest;
    SPELL_DATA *spell, *spell_new;
    OBJ_DATA *target;
    int level;
    int sn;
    AFFECT_DATA *paf;

    if(!info || IS_NULLSTR(argument))
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(arg->type != ENT_OBJECT || !arg->d.obj)
        return;

    target = arg->d.obj;
    level = target->level;

    if(!(rest = expand_argument(info,rest,arg)))
        return;

    if(arg->type != ENT_STRING || IS_NULLSTR(arg->d.str))
        return;

    sn = skill_lookup(arg->d.str);
    if(sn <= 0)
        return;

    if(skill_table[sn].spell_fun == spell_null)
        return;

    if(rest && *rest) {
        if(!(rest = expand_argument(info,rest,arg)))
            return;

        if(arg->type != ENT_NUMBER || arg->d.num < 1 || arg->d.num > target->level)
            return;

        level = arg->d.num;
    }

    for(spell = target->spells; spell != NULL; spell = spell->next)
    {
        if(spell->sn == sn) {
            spell->level = level;

            if(target->carried_by != NULL && target->wear_loc != WEAR_NONE) {
                if(target->item_type != ITEM_WAND &&
                    target->item_type != ITEM_STAFF &&
                    target->item_type != ITEM_SCROLL &&
                    target->item_type != ITEM_POTION &&
                    target->item_type != ITEM_TATTOO &&
                    target->item_type != ITEM_PILL) {

                    for(paf = target->carried_by->affected; paf != NULL; paf = paf->next) {
                        if(paf->type == sn && paf->slot == target->wear_loc) {
                            if(paf->level > level)
                                paf->level = level;

                            break;
                        }
                    }
                }
            }
            return;
        }
    }

    spell_new = new_spell();
    spell_new->sn = sn;
    spell_new->level = level;

    spell_new->next = target->spells;
    target->spells = spell_new;

    if(target->carried_by != NULL && target->wear_loc != WEAR_NONE) {
        if(target->item_type != ITEM_WAND &&
            target->item_type != ITEM_STAFF &&
            target->item_type != ITEM_SCROLL &&
            target->item_type != ITEM_POTION &&
            target->item_type != ITEM_TATTOO &&
            target->item_type != ITEM_PILL) {

            for(paf = target->carried_by->affected; paf != NULL; paf = paf->next)
            {
                if(paf->type == sn)
                    break;
            }

            if(paf == NULL || paf->level < level) {
                affect_strip(target->carried_by, sn);
                obj_cast_spell(sn, level + MAGIC_WEAR_SPELL, target->carried_by, target->carried_by, target);
            }
        }
    }
}

SCRIPT_CMD(scriptcmd_alteraffect)
{
    char buf[MIL], field[MIL], *rest;
    AFFECT_DATA *paf;
    int value;
    const char *scope_name = NULL;
    long scope_vnum = 0;

    if(!info || IS_NULLSTR(argument))
        return;

    if(info->mob) {
        scope_name = "MpAlterAffect";
        scope_vnum = VNUM(info->mob);
    } else if(info->obj) {
        scope_name = "OpAlterAffect";
        scope_vnum = VNUM(info->obj);
    } else if(info->room) {
        scope_name = "RpAlterAffect";
        scope_vnum = info->room->vnum;
    } else if(info->token) {
        scope_name = "TpAlterAffect";
        scope_vnum = VNUM(info->token);
    } else
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(arg->type != ENT_AFFECT || !arg->d.aff)
        return;

    paf = arg->d.aff;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(IS_NULLSTR(rest))
        return;

    if(arg->type != ENT_STRING || IS_NULLSTR(arg->d.str))
        return;

    strncpy(field,arg->d.str,MIL-1);

    if(!str_cmp(field, "level")) {
        argument = one_argument(rest,buf);

        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
            return;
        }

        switch(arg->type) {
        case ENT_STRING:
            value = is_number(arg->d.str) ? atoi(arg->d.str) : 0;
            break;
        case ENT_NUMBER:
            value = arg->d.num;
            break;
        default:
            return;
        }

        switch(buf[0]) {
        case '=':
            if(value > 0 && value < paf->level)
                paf->level = value;
            break;

        case '+':
            if(value < 0) {
                paf->level += value;
                if(paf->level < 1)
                    paf->level = 1;
            }
            break;

        case '-':
            if(value > 0) {
                paf->level -= value;
                if(paf->level < 1)
                    paf->level = 1;
            }
            break;
        }

        return;
    }

    if(!str_cmp(field, "duration")) {
        argument = one_argument(rest,buf);

        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
            return;
        }

        if(paf->slot != WEAR_NONE) {
            pbugf(LOG_SCRIPTS, "%s - Attempting to modify duration of an object given affect from vnum %ld.", scope_name, scope_vnum);
            return;
        }

        if(paf->group == AFFGROUP_RACIAL) {
            pbugf(LOG_SCRIPTS, "%s - Attempting to modify duration of a racial affect from vnum %ld.", scope_name, scope_vnum);
            return;
        }

        if(!str_cmp(buf, "toggle")) {
            paf->duration = -paf->duration;
            return;
        }

        switch(arg->type) {
        case ENT_STRING:
            value = is_number(arg->d.str) ? atoi(arg->d.str) : 0;
            break;
        case ENT_NUMBER:
            value = arg->d.num;
            break;
        default:
            return;
        }

        switch(buf[0]) {
        case '=':
            if(value != 0)
                paf->duration = value;
            break;

        case '+':
            if(paf->duration < 0)
            {
                paf->duration += value;
                if(paf->duration >= 0)
                    paf->duration = -1;
            }
            else
            {
                paf->duration += value;
                if(paf->duration < 0)
                    paf->duration = 0;
            }
            break;

        case '-':
            if(paf->duration < 0)
            {
                paf->duration -= value;
                if(paf->duration >= 0)
                    paf->duration = -1;
            }
            else
            {
                paf->duration -= value;
                if(paf->duration < 0)
                    paf->duration = 0;
            }
            break;
        }
    }
}

// remspell $OBJECT STRING[ silent]
SCRIPT_CMD(scriptcmd_remspell)
{
    char *rest;
    SPELL_DATA *spell, *spell_prev;
    OBJ_DATA *target;
    int level;
    int sn;
    bool found = false, show = true;
    AFFECT_DATA *paf;
    ITERATOR it;

    if(!info || IS_NULLSTR(argument))
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(arg->type != ENT_OBJECT || !arg->d.obj)
        return;

    target = arg->d.obj;

    if(!(rest = expand_argument(info,rest,arg)))
        return;

    if(arg->type != ENT_STRING || IS_NULLSTR(arg->d.str))
        return;

    sn = skill_lookup(arg->d.str);
    if(sn <= 0)
        return;

    if(skill_table[sn].spell_fun == spell_null)
        return;

    if(rest && *rest) {
        if(!(rest = expand_argument(info,rest,arg)))
            return;

        if(arg->type != ENT_STRING || IS_NULLSTR(arg->d.str))
            return;

        if(!str_cmp(arg->d.str, "silent"))
            show = false;
    }

    found = false;
    spell_prev = NULL;
    for(spell = target->spells; spell; spell_prev = spell, spell = spell->next) {
        if(spell->sn == sn) {
            if(spell_prev != NULL)
                spell_prev->next = spell->next;
            else
                target->spells = spell->next;

            level = spell->level;

            free_spell(spell);

            found = true;
            break;
        }
    }

    if(found && target->carried_by != NULL && target->wear_loc != WEAR_NONE) {
        if(target->item_type != ITEM_WAND &&
            target->item_type != ITEM_STAFF &&
            target->item_type != ITEM_SCROLL &&
            target->item_type != ITEM_POTION &&
            target->item_type != ITEM_TATTOO &&
            target->item_type != ITEM_PILL) {

            OBJ_DATA *obj_tmp;
            int spell_level = level;
            int found_loc = WEAR_NONE;
            bool has_lworn = false;

            for(paf = target->carried_by->affected; paf != NULL; paf = paf->next)
            {
                if(paf->type == sn && paf->slot == target->wear_loc)
                    break;
            }

            if(!paf)
                return;

            found = false;
            level = 0;

            has_lworn = target->carried_by && target->carried_by->lworn && is_llist(target->carried_by->lworn);

            if(has_lworn) {
                iterator_start(&it, target->carried_by->lworn);
                while((obj_tmp = iterator_nextdata(&it))) {
                    if(obj_tmp != target) {
                        for(spell = obj_tmp->spells; spell != NULL; spell = spell->next) {
                            if(spell->sn == sn && spell->level > level) {
                                level = spell->level;
                                found_loc = obj_tmp->wear_loc;
                                found = true;
                            }
                        }
                    }
                }
                iterator_stop(&it);
            }

            if(!found) {
                if(show) {
                    if(skill_table[sn].msg_off) {
                        send_to_char(skill_table[sn].msg_off, target->carried_by);
                        send_to_char("\n\r", target->carried_by);
                    }
                }

                affect_strip(target->carried_by, sn);
            } else if(level > spell_level) {
                level -= spell_level;

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

static int cmd_cmp(void *a, void *b)
{
    return str_cmp((char *)a, (char *)b);
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

    if (!IS_VALID(ch) || IS_NPC(ch))
        return;

    if (!ch->pcdata || !ch->pcdata->extra_commands)
        return;

    BUFFER *buffer = new_buf();
    if (PARSE_STR(buffer) && !list_contains(ch->pcdata->extra_commands, buffer->string, cmd_cmp))
    {
        list_appendlink(ch->pcdata->extra_commands, str_dup(buffer->string));
        SETRETURN(1);
    }

    free_buf(buffer);
}


// SPAWNDUNGEON $PLAYER $DUNGEONID $FLOOR $VARIABLENAME
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
    case ENT_STRING: ch = script_get_char_room(info, arg->d.str, false); break;
    case ENT_MOBILE: ch = arg->d.mob; break;
    default: ch = NULL; break;
    }

    if (!ch || IS_NPC(ch))
        return;

    if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
        return;

    long vnum = arg->d.num;

    if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER || !*rest)
        return;

    int floor = arg->d.num;

    /* Script context doesn't have area info, use global lookup */
    WNUM wnum = { NULL, vnum };
    ROOM_INDEX_DATA *room = spawn_dungeon_player(ch, wnum, floor);

    if( !room )
        return;

    variables_set_room(info->var,rest,room);
    info->progs->lastreturn = 1;
}


// SPECIALKEY $SHIP|$DUNGEON $VNUM $VARIABLENAME
SCRIPT_CMD(scriptcmd_specialkey)
{
    char *rest;
    LLIST *keys;
    OBJ_INDEX_DATA *index = NULL;
    OBJ_DATA *obj;

    if(!info) return;

    info->progs->lastreturn = 0;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    keys = NULL;
    if( arg->type == ENT_SHIP )
        keys = IS_VALID(arg->d.ship) ? arg->d.ship->special_keys : NULL;
    else if( arg->type == ENT_DUNGEON )
        keys = NULL;//IS_VALID(arg->d.dungeon) ? arg->d.dungeon->special_keys : NULL; // NYI

    if( !IS_VALID(keys) )
        return;

    if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER || !*rest)
        return;

    WNUM sk_wnum = { .pArea = NULL, .vnum = (long)arg->d.num };
    SPECIAL_KEY_DATA *sk = get_special_key(keys, sk_wnum);

    if( !sk )
        return;

    if (sk->key_wnum.pArea) {
        index = get_obj_index(sk->key_wnum.pArea, sk->key_wnum.vnum);
    }

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
        info->progs->lastreturn = 1;
        return;
    }

    free_list_uid_data(luid);
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
    case ENT_STRING: victim = script_get_char_room(info, arg->d.str, false); break;
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
        case ENT_STRING: victim = script_get_char_room(info, arg->d.str, false); break;
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
    if(!IS_NPC(attacker) && victim->fighting != NULL && victim->fighting != attacker && !IS_SET(attacker->in_room->room_flag[1], ROOM_MULTIPLAY))
        return;

    // They are not in the same room
    if(attacker->in_room != victim->in_room)
        return;

    // The victim is safe
    if(is_safe(attacker, victim, false)) return;

    // Set them to fighting!
    if(set_fighting(attacker, victim))
        info->progs->lastreturn = 1;
}

// STARTRECKONING[ $DURATION=30[ $SKIP=false]]
SCRIPT_CMD(scriptcmd_startreckoning)
{
    char *rest = argument;
    int duration = 30;
    bool skip = false;

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
                skip = true;
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
    bool fBoth = false;

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

// STOPRECKONING
// STOPRECKONING $immediate(boolean)
// STOPRECKONING $duration(number)
SCRIPT_CMD(scriptcmd_stopreckoning)
{
    char *rest = argument;
    int duration = 5;		// Default, give 5 minutes
    bool immediate = false;

    info->progs->lastreturn = 0;

    if (script_security < 9)
        return;

    if (rest && *rest)
    {
        if (!(rest = expand_argument(info, rest, arg)))
            return;

        if (arg->type == ENT_BOOLEAN)
            immediate = arg->d.boolean;
        else if (arg->type == ENT_STRING)
            immediate = !str_prefix(arg->d.str, "yes") || !str_prefix(arg->d.str, "true") || !str_prefix(arg->d.str, "immediate");
        else if (arg->type == ENT_NUMBER)
            duration = UMAX(1, arg->d.num);
    }

    // Only work if the reckoning is active
    if (reckoning_timer > 0)
    {
        if (immediate)
        {
            // The global message will be under script control.
            reset_reckoning();
            info->progs->lastreturn = -1;
        }
        else
        {
            struct tm *reck_time = (struct tm *) localtime(&current_time);
            reck_time->tm_min += duration;
            time_t timer = (time_t) mktime(reck_time);

            // Check if the *new* timer is sooner than the current time left
            if (timer < reckoning_timer)
            {
                reckoning_duration = duration;
                reckoning_timer = timer;
                info->progs->lastreturn = duration;
            }
        }
    }
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

// UNGROUP mobile[ bool(ALL=false)]
SCRIPT_CMD(scriptcmd_ungroup)
{
    char *rest;
    bool fAll = false;

    if(!info || IS_NULLSTR(argument))
        return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if(arg->type != ENT_MOBILE || !arg->d.mob)
        return;

    if( *rest ) {
        if(!(rest = expand_argument(info,rest,arg)))
            return;

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
        LLIST *members;
        LLIST *snapshot;
        CHAR_DATA *leader = (arg->d.mob->leader != NULL) ? arg->d.mob->leader : arg->d.mob;
        CHAR_DATA *follower;

        members = (IS_VALID(leader->group) && leader->group->members)
            ? leader->group->members
            : leader->lgroup;

        if( !members || list_size(members) < 1 )
            return;

        snapshot = list_copy(members);
        if (!snapshot)
            return;

        iterator_start(&git, snapshot);
        while((follower = (CHAR_DATA *)iterator_nextdata(&git)))
            stop_grouped(follower);
        iterator_stop(&git);

        list_destroy(snapshot);
    }
    else
    {
        stop_grouped(arg->d.mob);
    }
}


// UNLOCKAREA $PLAYER $AREA|$AREANAME|$ANUM|$ROOM
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

// UNLOCKDUNGEON $PLAYER $DUNGEON|$ROOM|$WNUM|$VNUM
SCRIPT_CMD(scriptcmd_unlockdungeon)
{
    char *rest;
    CHAR_DATA *player;
    DUNGEON_INDEX_DATA *dungeon_index = NULL;

    if(!info) return;

    info->progs->lastreturn = 0;

    if(!(rest = expand_argument(info,argument,arg)) || arg->type != ENT_MOBILE || IS_NPC(arg->d.mob) )
        return;

    player = arg->d.mob;

    if(!expand_argument(info,rest,arg))
        return;

    if( arg->type == ENT_DUNGEON )
    {
        dungeon_index = arg->d.dungeon ? arg->d.dungeon->index : NULL;
    }
    else if( arg->type == ENT_ROOM )
    {
        DUNGEON *dng = arg->d.room ? get_room_dungeon(arg->d.room) : NULL;
        dungeon_index = dng ? dng->index : NULL;
    }
    else if( arg->type == ENT_NUMBER )
    {
        dungeon_index = get_dungeon_index(arg->d.num);
    }
    else if( arg->type == ENT_STRING )
    {
        WNUM wnum;
        if( parse_widevnum(arg->d.str, player->in_room ? player->in_room->area : NULL, &wnum) )
            dungeon_index = get_dungeon_index_for_area(wnum.pArea, wnum.vnum);
    }

    if( !dungeon_index )
        return;

    player_unlock_dungeon(player, dungeon_index);

    info->progs->lastreturn = 1;
}

//////////////////////////////////////
// V

SCRIPT_CMD(scriptcmd_varclear)
{
    char target[MIL];

    if(!info || !info->var) return;

    one_argument(argument, target);
    if (!str_cmp(target, "quest") || !str_cmp(target, "questindex")
        || !str_cmp(target, "event") || !str_cmp(target, "eventindex")) {
        scriptcmd_varclearon(info, argument, arg);
        return;
    }

    script_varclearon(info,info->var, argument, arg);
}

SCRIPT_CMD(scriptcmd_varclearon)
{
    VARIABLE **vars;
    char target[MIL];
    char *rest;
    QUEST_DATA *run;
    QUEST_INDEX_V2_DATA *index_v2;

    if(!info) return;

    rest = one_argument(argument, target);
    if (!str_cmp(target, "quest")) {
        run = info->quest;
        if (!run)
            run = (info->ch && !IS_NPC(info->ch)) ? quest_runtime_get_focused_run(info->ch) : NULL;
        vars = run ? &run->vars : NULL;
        script_varclearon(info, vars, rest, arg);
        return;
    }

    if (!str_cmp(target, "questindex")) {
        run = info->quest;
        if (!run)
            run = (info->ch && !IS_NPC(info->ch)) ? quest_runtime_get_focused_run(info->ch) : NULL;
        index_v2 = run ? quest_runtime_get_index_v2(run) : NULL;
        vars = index_v2 ? &index_v2->index_vars : NULL;
        script_varclearon(info, vars, rest, arg);
        return;
    }

    if (!str_cmp(target, "event")) {
        if (!(rest = expand_argument(info, rest, arg)))
            return;
        vars = scriptcmd_event_runtime_vars_from_param(arg);
        script_varclearon(info, vars, rest, arg);
        return;
    }

    if (!str_cmp(target, "eventindex")) {
        if (!(rest = expand_argument(info, rest, arg)))
            return;
        vars = scriptcmd_event_index_vars_from_param(arg);
        script_varclearon(info, vars, rest, arg);
        return;
    }

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
    char target[MIL];
    bool on;

    if(!info || !info->var) return;

    one_argument(argument, target);
    if (!str_cmp(target, "quest") || !str_cmp(target, "questindex")
        || !str_cmp(target, "event") || !str_cmp(target, "eventindex")) {
        scriptcmd_varsaveon(info, argument, arg);
        return;
    }

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
    VARIABLE **target_vars;
    char target[MIL];
    char *rest;
    QUEST_DATA *run;
    QUEST_INDEX_V2_DATA *index_v2;

    if(!info) return;

    rest = one_argument(argument, target);
    if (!str_cmp(target, "quest")) {
        run = info->quest;
        if (!run)
            run = (info->ch && !IS_NPC(info->ch)) ? quest_runtime_get_focused_run(info->ch) : NULL;
        vars = run ? run->vars : NULL;
        if(!vars) return;

        argument = one_argument(rest,name);
        if(!name[0]) return;
        argument = one_argument(argument,buf);
        if(!buf[0]) return;

        on = !str_cmp(buf,"on") || !str_cmp(buf,"true") || !str_cmp(buf,"yes");
        variable_setsave(vars,name,on);
        return;
    }

    if (!str_cmp(target, "questindex")) {
        run = info->quest;
        if (!run)
            run = (info->ch && !IS_NPC(info->ch)) ? quest_runtime_get_focused_run(info->ch) : NULL;
        index_v2 = run ? quest_runtime_get_index_v2(run) : NULL;
        vars = index_v2 ? index_v2->index_vars : NULL;
        if(!vars) return;

        argument = one_argument(rest,name);
        if(!name[0]) return;
        argument = one_argument(argument,buf);
        if(!buf[0]) return;

        on = !str_cmp(buf,"on") || !str_cmp(buf,"true") || !str_cmp(buf,"yes");
        variable_setsave(vars,name,on);
        return;
    }

    if (!str_cmp(target, "event")) {
        if (!(rest = expand_argument(info, rest, arg)))
            return;

        target_vars = scriptcmd_event_runtime_vars_from_param(arg);
        vars = target_vars ? *target_vars : NULL;
        if (!vars)
            return;

        argument = one_argument(rest, name);
        if (!name[0])
            return;
        argument = one_argument(argument, buf);
        if (!buf[0])
            return;

        on = !str_cmp(buf,"on") || !str_cmp(buf,"true") || !str_cmp(buf,"yes");
        variable_setsave(vars, name, on);
        return;
    }

    if (!str_cmp(target, "eventindex")) {
        if (!(rest = expand_argument(info, rest, arg)))
            return;

        target_vars = scriptcmd_event_index_vars_from_param(arg);
        vars = target_vars ? *target_vars : NULL;
        if (!vars)
            return;

        argument = one_argument(rest, name);
        if (!name[0])
            return;
        argument = one_argument(argument, buf);
        if (!buf[0])
            return;

        on = !str_cmp(buf,"on") || !str_cmp(buf,"true") || !str_cmp(buf,"yes");
        variable_setsave(vars, name, on);
        return;
    }

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

SCRIPT_CMD(scriptcmd_varset)
{
    char target[MIL];

    if(!info || !info->var) return;

    one_argument(argument, target);
    if (!str_cmp(target, "quest") || !str_cmp(target, "questindex")
        || !str_cmp(target, "event") || !str_cmp(target, "eventindex")) {
        scriptcmd_varseton(info, argument, arg);
        return;
    }

    script_varseton(info,info->var,argument, arg);
}

SCRIPT_CMD(scriptcmd_varseton)
{

    VARIABLE **vars;
    char target[MIL];
    char *rest;
    QUEST_DATA *run;
    QUEST_INDEX_V2_DATA *index_v2;

    if(!info) return;

    rest = one_argument(argument, target);
    if (!str_cmp(target, "quest")) {
        run = info->quest;
        if (!run)
            run = (info->ch && !IS_NPC(info->ch)) ? quest_runtime_get_focused_run(info->ch) : NULL;
        vars = run ? &run->vars : NULL;
        script_varseton(info, vars, rest, arg);
        return;
    }

    if (!str_cmp(target, "questindex")) {
        run = info->quest;
        if (!run)
            run = (info->ch && !IS_NPC(info->ch)) ? quest_runtime_get_focused_run(info->ch) : NULL;
        index_v2 = run ? quest_runtime_get_index_v2(run) : NULL;
        vars = index_v2 ? &index_v2->index_vars : NULL;
        script_varseton(info, vars, rest, arg);
        return;
    }

    if (!str_cmp(target, "event")) {
        if (!(rest = expand_argument(info, rest, arg)))
            return;
        vars = scriptcmd_event_runtime_vars_from_param(arg);
        script_varseton(info, vars, rest, arg);
        return;
    }

    if (!str_cmp(target, "eventindex")) {
        if (!(rest = expand_argument(info, rest, arg)))
            return;
        vars = scriptcmd_event_index_vars_from_param(arg);
        script_varseton(info, vars, rest, arg);
        return;
    }

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


//////////////////////////////////////
// W

// WILDSTILE $WUID $X $Y $TILE
// $WUID - wilderness uid
// $X/$Y - wilderness coordinates
// $TILE - terrain token (single character)
// Returns: lastreturn = 1 on success, 0 on failure
SCRIPT_CMD(scriptcmd_wildstile)
{
    char *rest;
    WILDS_DATA *wilds;
    WILDS_TERRAIN *terrain;
    int x;
    int y;
    int dx = 0;
    int dy = 0;
    char tile = '\0';

    if (!info)
        return;

    info->progs->lastreturn = 0;

    rest = argument;
    if (!scriptcmd_parse_wilds_coord(info, arg, &wilds, &x, &y, &rest))
        return;

    if (!(rest = expand_argument(info, rest, arg)))
        return;

    if (arg->type == ENT_NUMBER && rest && *rest)
    {
        dx = arg->d.num;
        if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_NUMBER)
            return;
        dy = arg->d.num;

        if (!(rest = expand_argument(info, rest, arg)))
            return;
    }

    if (arg->type == ENT_STRING)
    {
        if (IS_NULLSTR(arg->d.str))
            return;
        tile = arg->d.str[0];
        if (arg->d.str[1] != '\0')
            return;
    }
    else if (arg->type == ENT_NUMBER)
    {
        if (arg->d.num < 0 || arg->d.num > 255)
            return;
        tile = (char)arg->d.num;
    }
    else
    {
        return;
    }

    terrain = get_terrain_by_token(wilds, tile);
    if (!terrain)
        return;

    x += dx;
    y += dy;
    if (x < 0 || x >= wilds->map_size_x || y < 0 || y >= wilds->map_size_y)
        return;

    if (!set_wilds_runtime_tile(wilds, x, y, tile))
        return;

    info->progs->lastreturn = 1;
}

// WILDSOVERLAY ADD $WUID $X1 $Y1 $X2 $Y2 $TILE [ $REGION ] [ $DURATION ]
// WILDSOVERLAY REMOVE $WUID $ZONE_ID
// WILDSOVERLAY CLEARREGION $WUID $REGION
// WILDSOVERLAY CLEANUP $WUID
// Returns:
//   ADD -> zone id in lastreturn (0 on failure)
//   REMOVE/CLEARREGION/CLEANUP -> removed count in lastreturn
SCRIPT_CMD(scriptcmd_wildsoverlay)
{
    char cmd[MIL];
    char *rest;
    WILDS_DATA *wilds;

    if (!info)
        return;

    info->progs->lastreturn = 0;

    argument = one_argument(argument, cmd);
    if (IS_NULLSTR(cmd))
        return;

    if (!str_cmp(cmd, "add"))
    {
        int x1;
        int y1;
        int x2;
        int y2;
        int anchor_x = 0;
        int anchor_y = 0;
        bool anchored = false;
        int region = REGION_UNKNOWN;
        int duration = 0;
        char tile = '\0';
        long zone_id;

        if (!(rest = expand_argument(info, argument, arg)))
            return;

        if (arg->type == ENT_WILDS_ROOM)
        {
            wilds = get_wilds_from_uid(NULL, arg->d.wroom.wuid);
            anchor_x = arg->d.wroom.x;
            anchor_y = arg->d.wroom.y;
            anchored = true;
        }
        else if (arg->type == ENT_ROOM && arg->d.room && arg->d.room->wilds)
        {
            wilds = arg->d.room->wilds;
            anchor_x = arg->d.room->x;
            anchor_y = arg->d.room->y;
            anchored = true;
        }
        else if (arg->type == ENT_NUMBER)
        {
            wilds = get_wilds_from_uid(NULL, arg->d.num);
        }
        else
        {
            return;
        }

        if (!wilds)
            return;

        if (!(rest = expand_argument(info, rest, arg)) && arg->type != ENT_NUMBER)
            return;
        x1 = arg->d.num;

        if (!(rest = expand_argument(info, rest, arg)) && arg->type != ENT_NUMBER)
            return;
        y1 = arg->d.num;

        if (!(rest = expand_argument(info, rest, arg)) && arg->type != ENT_NUMBER)
            return;
        x2 = arg->d.num;

        if (!(rest = expand_argument(info, rest, arg)) && arg->type != ENT_NUMBER)
            return;
        y2 = arg->d.num;

        if (anchored)
        {
            x1 += anchor_x;
            y1 += anchor_y;
            x2 += anchor_x;
            y2 += anchor_y;
        }

        if (!(rest = expand_argument(info, rest, arg)))
            return;

        if (arg->type == ENT_STRING)
        {
            if (IS_NULLSTR(arg->d.str))
                return;
            tile = arg->d.str[0];
            if (arg->d.str[1] != '\0')
                return;
        }
        else if (arg->type == ENT_NUMBER)
        {
            if (arg->d.num < 0 || arg->d.num > 255)
                return;
            tile = (char)arg->d.num;
        }
        else
        {
            return;
        }

        if (!get_terrain_by_token(wilds, tile))
            return;

        if (rest && *rest)
        {
            if (!(rest = expand_argument(info, rest, arg)) && arg->type != ENT_NUMBER)
                return;
            region = arg->d.num;
        }

        if (rest && *rest)
        {
            if (!(rest = expand_argument(info, rest, arg)) && arg->type != ENT_NUMBER)
                return;
            duration = arg->d.num;
            if (duration < 0)
                return;
        }

        zone_id = wilds_add_temporary_zone(wilds, x1, y1, x2, y2, tile, region, duration);
        if (zone_id < 1)
            return;

        info->progs->lastreturn = zone_id;
        return;
    }

    if (!str_cmp(cmd, "remove"))
    {
        if (!(rest = expand_argument(info, argument, arg)) && arg->type != ENT_NUMBER)
            return;

        wilds = get_wilds_from_uid(NULL, arg->d.num);
        if (!wilds)
            return;

        if (!(rest = expand_argument(info, rest, arg)) && arg->type != ENT_NUMBER)
            return;

        info->progs->lastreturn = wilds_remove_temporary_zone(wilds, arg->d.num);
        return;
    }

    if (!str_cmp(cmd, "clearregion"))
    {
        if (!(rest = expand_argument(info, argument, arg)) && arg->type != ENT_NUMBER)
            return;

        wilds = get_wilds_from_uid(NULL, arg->d.num);
        if (!wilds)
            return;

        if (!(rest = expand_argument(info, rest, arg)) && arg->type != ENT_NUMBER)
            return;

        info->progs->lastreturn = wilds_remove_region_temporary_zones(wilds, arg->d.num);
        return;
    }

    if (!str_cmp(cmd, "cleanup"))
    {
        if (!(rest = expand_argument(info, argument, arg)) && arg->type != ENT_NUMBER)
            return;

        wilds = get_wilds_from_uid(NULL, arg->d.num);
        if (!wilds)
            return;

        info->progs->lastreturn = wilds_cleanup_expired_temporary_zones(wilds);
        return;
    }
}

// WILDSANCHOR $VARIABLENAME <WUID X Y|$ROOM|$WILDS_ROOM> [ $DX $DY ]
// Stores a coordinate anchor as a wilderness-room variable (wuid/x/y tuple).
SCRIPT_CMD(scriptcmd_wildsanchor)
{
    char *rest = argument;
    WILDS_DATA *wilds;
    int x;
    int y;
    int dx = 0;
    int dy = 0;
    char var_name[MIL];

    if (!info || !info->var)
        return;

    info->progs->lastreturn = 0;

    if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_STRING || IS_NULLSTR(arg->d.str))
        return;

    snprintf(var_name, sizeof(var_name), "%s", arg->d.str);

    if (!scriptcmd_parse_wilds_coord(info, arg, &wilds, &x, &y, &rest))
        return;

    if (rest && *rest)
    {
        if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_NUMBER)
            return;
        dx = arg->d.num;

        if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_NUMBER)
            return;
        dy = arg->d.num;
    }

    x += dx;
    y += dy;
    if (x < 0 || x >= wilds->map_size_x || y < 0 || y >= wilds->map_size_y)
        return;

    if (!variables_set_wilds_room(info->var, var_name, wilds->uid, x, y, false))
        return;

    info->progs->lastreturn = 1;
}

// WILDSVLINK ADD <WUID X Y|$ROOM|$WILDS_ROOM> <DOOR> <DEST_WNUM> [LINKAGE] [DURATION_SECONDS]
// WILDSVLINK REMOVE $WUID $RUNTIME_VLINK_UID
// WILDSVLINK CLEANUP $WUID
SCRIPT_CMD(scriptcmd_wildsvlink)
{
    char cmd[MIL];
    char *rest;
    WILDS_DATA *wilds = NULL;

    if (!info)
        return;

    info->progs->lastreturn = 0;

    argument = one_argument(argument, cmd);
    if (IS_NULLSTR(cmd))
        return;

    if (!str_cmp(cmd, "add"))
    {
        int x;
        int y;
        int door;
        int linkage = VLINK_FROM_WILDS;
        int duration = 0;
        WNUM dest_wnum = wnum_zero;
        long uid = 0;

        rest = argument;
        if (!scriptcmd_parse_wilds_coord(info, arg, &wilds, &x, &y, &rest))
            return;

        if (!(rest = expand_argument(info, rest, arg)))
            return;

        if (arg->type == ENT_NUMBER)
            door = arg->d.num;
        else if (arg->type == ENT_STRING)
            door = get_num_dir(arg->d.str);
        else
            return;

        if (door < 0 || door >= MAX_DIR)
            return;

        if (!scriptcmd_parse_dest_wnum_from_arg(info, arg, &dest_wnum, &rest))
            return;

        if (rest && *rest)
        {
            if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_STRING)
                return;

            linkage = scriptcmd_parse_vlink_linkage(arg->d.str);
            if (linkage < 0)
                return;
        }

        if (rest && *rest)
        {
            if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_NUMBER)
                return;
            duration = arg->d.num;
            if (duration < 0)
                return;
        }

        if (!wilderness_state_add_runtime_vlink(wilds,
            x,
            y,
            door,
            dest_wnum.pArea ? dest_wnum.pArea->uid : 0,
            dest_wnum.vnum,
            linkage,
            duration,
            &uid))
            return;

        info->progs->lastreturn = uid;
        return;
    }

    if (!str_cmp(cmd, "remove"))
    {
        long uid;

        if (!(rest = expand_argument(info, argument, arg)) && arg->type != ENT_NUMBER)
            return;

        wilds = get_wilds_from_uid(NULL, arg->d.num);
        if (!wilds)
            return;

        if (!(rest = expand_argument(info, rest, arg)) && arg->type != ENT_NUMBER)
            return;

        uid = arg->d.num;
        info->progs->lastreturn = wilderness_state_remove_runtime_vlink(wilds, uid);
        return;
    }

    if (!str_cmp(cmd, "cleanup"))
    {
        if (!(rest = expand_argument(info, argument, arg)) && arg->type != ENT_NUMBER)
            return;

        wilds = get_wilds_from_uid(NULL, arg->d.num);
        if (!wilds)
            return;

        info->progs->lastreturn = wilderness_state_cleanup_runtime_vlinks(wilds);
        return;
    }
}

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
    long vnum;


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

    if (space == PRG_APROG && info->block && info->block->script
        && info->block->script->type == PRG_QPROG)
        space = PRG_QPROG;

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


    if(!(rest = expand_argument(info,rest,arg))) {
        // Restore the call depth to the previous value
        script_call_depth = depth;
        return;
    }

    script = get_script_from_arg(info, arg, space, &vnum);
    if (vnum < 1 || !script) {
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
        case ENT_STRING: ch = script_get_char_room(info, arg->d.str, false); break;
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
        case ENT_STRING: vch = script_get_char_room(info, arg->d.str, false); break;
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
    ret = execute_script(script->vnum, script, mob, obj, room, token, area, instance, dungeon, ch, obj1, obj2, vch, NULL,NULL, NULL,info->phrase,info->trigger,info->trigger_type,0,0,0,0,0);
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



SCRIPT_CMD(scriptcmd_alterobj)
{
    char msg[MSL];
    char buf[2*MIL],field[MIL],*rest;
    int value = 0, num, min_sec = MIN_SCRIPT_SECURITY;
    OBJ_DATA *obj = NULL;
    int min = 0, max = 0;
    bool hasmin = false, hasmax = false;
    long assignmin = 0, assignmax = 0;
    bool hasassignmin = false, hasassignmax = false;
    bool allowarith = true;
    bool allowbitwise = true;
    const struct flag_type *flags = NULL;
    const struct flag_type **bank = NULL;
    long temp_flags[4];
    int sec_flags[4];
    int *ptr = NULL;
    long *lptr = NULL;

    if(!info) return;

    SETRETURN(0);

    for(int i = 0; i < 4; i++)
        sec_flags[i] = MIN_SCRIPT_SECURITY;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "AlterObj - Error in parsing.");
        return;
    }

    switch(arg->type) {
    case ENT_STRING: obj = script_get_obj_here(info, arg->d.str); break;
    case ENT_OBJECT: obj = arg->d.obj; break;
    default: break;
    }

    if(!obj) {
        pbugf(LOG_SCRIPTS, "AlterObj - NULL object.");
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "AlterObj - Missing field type.");
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "AlterObj - Error in parsing.");
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
        pbugf(LOG_SCRIPTS, "AlterObj - Error in parsing.");
        return;
    }

    if(num >= 0) {
        int current_value;
        int updated_value;

        switch(arg->type) {
        case ENT_STRING: value = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
        case ENT_NUMBER: value = arg->d.num; break;
        default: return;
        }

        if(script_security < min_sec) {
            pbugf(LOG_SCRIPTS, "AlterObj - Attempting to alter value%d with security %d.", num, script_security);
            return;
        }

        current_value = obj_get_legacy_value_slot(obj, num);
        updated_value = current_value;

        switch (buf[0]) {
        case '+': updated_value += value; break;
        case '-': updated_value -= value; break;
        case '*': updated_value *= value; break;
        case '/':
            if (!value) {
                pbugf(LOG_SCRIPTS, "AlterObj - adjust called with operator / and value 0");
                return;
            }
            updated_value /= value;
            break;
        case '%':
            if (!value) {
                pbugf(LOG_SCRIPTS, "AlterObj - adjust called with operator % and value 0");
                return;
            }
            updated_value %= value;
            break;

        case '=': updated_value = value; break;
        case '&': updated_value &= value; break;
        case '|': updated_value |= value; break;
        case '!': updated_value &= ~value; break;
        case '^': updated_value ^= value; break;
        default:
            return;
        }

        if (!obj_set_legacy_value_slot(obj, num, updated_value))
            return;

    } else {
        

        if(!str_cmp(field,"cond"))				{ ptr = (int*)&obj->condition; allowarith = true; allowbitwise = false; }
        else if(!str_cmp(field,"cost"))			{ ptr = (int*)&obj->cost; min_sec = 5; allowarith = true; allowbitwise = false; }
        else if(!str_cmp(field,"extra"))		{ lptr = obj->extra; bank = extra_flagbank; sec_flags[1] = sec_flags[2] = sec_flags[3] = 5; allowarith = false; allowbitwise = true; }
        else if(!str_cmp(field,"fixes"))		{ ptr = (int*)&obj->times_allowed_fixed; min_sec = 5; allowarith = true; allowbitwise = false; }
        else if(!str_cmp(field,"level"))		{ ptr = (int*)&obj->level; min_sec = 5; allowarith = true; allowbitwise = false; }
        else if(!str_cmp(field,"repairs"))		{ ptr = (int*)&obj->times_fixed; allowarith = true; allowbitwise = false; }
        else if(!str_cmp(field,"tempstore1"))	ptr = (int*)&obj->tempstore[0];
        else if(!str_cmp(field,"tempstore2"))	ptr = (int*)&obj->tempstore[1];
        else if(!str_cmp(field,"tempstore3"))	ptr = (int*)&obj->tempstore[2];
        else if(!str_cmp(field,"tempstore4"))	ptr = (int*)&obj->tempstore[3];
        else if(!str_cmp(field,"timer"))		{ ptr = (int*)&obj->timer; allowarith = true; allowbitwise = false; }
        else if(!str_cmp(field,"type"))			{ ptr = (int*)&obj->item_type; flags = type_flags; min_sec = 7; }
        else if(!str_cmp(field,"wear"))			{ ptr = (int*)&obj->wear_flags; flags = wear_flags; allowarith = false; allowbitwise = true; }
        else if(!str_cmp(field,"wearloc"))		{ ptr = (int*)&obj->wear_loc; flags = wear_loc_flags; allowarith = false; allowbitwise = false; }
        else if(!str_cmp(field,"weight"))		{ ptr = (int*)&obj->weight; allowarith = true; allowbitwise = false; }

        if(!ptr && !lptr) return;

        if(!(rest = expand_argument(info,argument,arg))) {
        scriptcmd_bug(info, "Alterobj - Error in parsing.");
        return;
        }

        if(script_security < min_sec) {
            pbugf(LOG_SCRIPTS, "AlterObj - Attempting to alter '%s' with security %d.", field, script_security);
            return;
        }

        if( bank != NULL )
        {
            if( arg->type != ENT_STRING ) return;

            allowarith = false;	// This is a bit vector, no arithmetic operators.
            if (!script_bitmatrix_lookup(arg->d.str, bank, temp_flags))
                return;

            // Make sure the script can change the particular flags
            if( buf[0] == '=' || buf[0] == '&' )
            {
                for(int i = 0; bank[i]; i++)
                {
                    if (script_security < sec_flags[i])
                    {
                        // Not enough security to change their values
                        temp_flags[i] = ptr[i];
                    }
                }
            }
            else
            {
                for(int i = 0; bank[i]; i++)
                {
                    if (script_security < sec_flags[i])
                    {
                        // Not enough security to change their values
                        temp_flags[i] = 0;
                    }
                }
            }

            if (bank == extra_flagbank)
            {
                REMOVE_BIT(temp_flags[2], ITEM_INSTANCE_OBJ);

                if( buf[0] == '=' || buf[0] == '&' )
                {
                    if( IS_SET(ptr[2], ITEM_INSTANCE_OBJ) ) SET_BIT(temp_flags[2], ITEM_INSTANCE_OBJ);
                }
            }		
        }
        else if( flags != NULL )
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
    

        if (lptr) {
            switch (buf[0]) {
            case '+':
                if( !allowarith ) {
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                *lptr += value;
                break;

            case '-':
                if( !allowarith ) {
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                *lptr -= value;
                break;

            case '*':
                if( !allowarith ) {
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                *lptr *= value;
                break;

            case '/':
                if( !allowarith ) {
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                if (!value) {
                    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Alterobj - adjust called with operator / and value 0");
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }
                *lptr /= value;
                break;
            case '%':
                if( !allowarith ) {
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                if (!value) {
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Alterobj - adjust called with operator % and value 0");
                    return;
                }
                *lptr %= value;
                break;

            case '>':
                if( !allowarith ) {
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                if (value > *lptr)
                    *lptr = value;
                break;

            case '<':
                if( !allowarith ) {
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                if (value < *lptr)
                    *lptr = value;
                break;

            case '=':
                if (bank != NULL)
                {
                    for(int i = 0; bank[i]; i++)
                        lptr[i] = temp_flags[i];
                }
                else
                    *lptr = value;

                // When explicitly assigning, allow different ranges
                if (hasassignmin)
                {
                    hasmin = true;
                    min = assignmin;
                }

                if (hasassignmax)
                {
                    hasmax = true;
                    max = assignmax;
                }
                break;

            case '&':
                if( !allowbitwise ) {
                    sprintf(msg, "Alterobj - called bitwise operator (%c) on a field (%s) that doesn't allow bitwise operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                if (bank != NULL)
                {
                    for(int i = 0; bank[i]; i++)
                        lptr[i] &= temp_flags[i];
                }
                else
                    *lptr &= value;
                break;
            case '|':
                if( !allowbitwise ) {
                    sprintf(msg, "Alterobj - called bitwise operator (%c) on a field (%s) that doesn't allow bitwise operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                if (bank != NULL)
                {
                    for(int i = 0; bank[i]; i++)
                        lptr[i] |= temp_flags[i];
                }
                else
                    *lptr |= value;
                break;
            case '!':
                if( !allowbitwise ) {
                    sprintf(msg, "Alterobj - called bitwise operator (%c) on a field (%s) that doesn't allow bitwise operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                if (bank != NULL)
                {
                    for(int i = 0; bank[i]; i++)
                        lptr[i] &= ~temp_flags[i];
                }
                else
                    *lptr &= ~value;
                break;
            case '^':
                if( !allowbitwise ) {
                    sprintf(msg, "Alterobj - called bitwise operator (%c) on a field (%s) that doesn't allow bitwise operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

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

            if( lptr )
            {
                if(hasmin && *lptr < min)
                    *lptr = min;

                if(hasmax && *lptr > max)
                    *lptr = max;
            }
        } else {
            switch (buf[0]) {
            case '+':
                if( !allowarith ) {
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                *ptr += value;
                break;

            case '-':
                if( !allowarith ) {
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                *ptr -= value;
                break;

            case '*':
                if( !allowarith ) {
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                *ptr *= value;
                break;

            case '/':
                if( !allowarith ) {
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                if (!value) {
                    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Alterobj - adjust called with operator / and value 0");
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }
                *ptr /= value;
                break;
            case '%':
                if( !allowarith ) {
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                if (!value) {
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Alterobj - adjust called with operator % and value 0");
                    return;
                }
                *ptr %= value;
                break;

            case '>':
                if( !allowarith ) {
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                if (value > *ptr)
                    *ptr = value;
                break;

            case '<':
                if( !allowarith ) {
                    sprintf(msg, "Alterobj - called arithmetic operator (%c) on a field (%s) that doesn't allow arithmetic operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
                    return;
                }

                if (value < *ptr)
                    *ptr = value;
                break;

            case '=':
                if (bank != NULL)
                {
                    for(int i = 0; bank[i]; i++)
                        ptr[i] = temp_flags[i];
                }
                else
                    *ptr = value;

                // When explicitly assigning, allow different ranges
                if (hasassignmin)
                {
                    hasmin = true;
                    min = assignmin;
                }

                if (hasassignmax)
                {
                    hasmax = true;
                    max = assignmax;
                }
                break;

            case '&':
                if( !allowbitwise ) {
                    sprintf(msg, "Alterobj - called bitwise operator (%c) on a field (%s) that doesn't allow bitwise operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
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
                    sprintf(msg, "Alterobj - called bitwise operator (%c) on a field (%s) that doesn't allow bitwise operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
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
                    sprintf(msg, "Alterobj - called bitwise operator (%c) on a field (%s) that doesn't allow bitwise operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
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
                    sprintf(msg, "Alterobj - called bitwise operator (%c) on a field (%s) that doesn't allow bitwise operations.",
                        buf[0], field);
                    scriptcmd_bug(info, msg);
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

            if( ptr )
            {
                if(hasmin && *ptr < min)
                    *ptr = (int)min;

                if(hasmax && *ptr > max)
                    *ptr = (int)max;
            }
        }
    }

    SETRETURN(1);
}

SCRIPT_CMD(scriptcmd_resetdice)
{
    char *rest;
    OBJ_DATA *obj = NULL;
    const char *scope_name = NULL;
    long scope_vnum = 0;

    if(!info)
        return;

    if(info->mob) {
        scope_name = "MpAlterObj";
        scope_vnum = VNUM(info->mob);
    } else if(info->obj) {
        scope_name = "OpAlterObj";
        scope_vnum = VNUM(info->obj);
    } else if(info->room) {
        scope_name = "RpAlterObj";
        scope_vnum = info->room->vnum;
    } else if(info->token) {
        scope_name = "TpAlterObj";
        scope_vnum = VNUM(info->token);
    } else
        return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "%s - Error in parsing from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        if(info->obj && !str_cmp(arg->d.str, "self"))
            obj = info->obj;
        else
            obj = script_get_obj_here(info, arg->d.str);
        break;

    case ENT_OBJECT:
        obj = arg->d.obj;
        break;

    default:
        break;
    }

    if(!obj) {
        pbugf(LOG_SCRIPTS, "%s - NULL object from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(PROG_FLAG(obj,PROG_AT)) {
        pbugf(LOG_SCRIPTS, "%s - blocked operation on PROG_AT object from vnum %ld.", scope_name, scope_vnum);
        return;
    }

    if(obj->item_type == ITEM_WEAPON)
        set_weapon_dice_obj(obj);
}



// alterroom <room> <field> <parameters>
SCRIPT_CMD(scriptcmd_alterroom)
{
    char buf[MSL+2],field[MIL],*rest;
    int value = 0, min_sec = MIN_SCRIPT_SECURITY;
    ROOM_INDEX_DATA *room;
    WILDS_DATA *wilds;
    AREA_DATA *area;

    long *lptr = NULL;
    int *ptr = NULL;
    int16_t *sptr = NULL;
    char **str;
    bool allow_empty = false;
    bool allowarith = true;
    bool allowbitwise = true;
    bool allow_static = true;
    bool sector_field = false;
    bool rs_sector_field = false;
    bool typed_sector_field = false;
    int sector_value = 0;
    int rs_sector_value = 0;
    bool hasmin = false, hasmax = false;
    int min = 0, max = 0;
    const struct flag_type *flags = NULL;
    const struct flag_type **bank = NULL;
    long temp_flags[4];

    if(!info) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "AlterRoom - Error in parsing.");
        return;
    }

    switch(arg->type) {
    case ENT_ROOM:
        room = arg->d.room;
        break;
    case ENT_NUMBER:
        area = find_area_by_vnum(arg->d.num, NULL);
        if (!area) area = get_system_area_fallback();
        room = get_room_index(area, arg->d.num);
        break;
    default: room = NULL; break;
    }

    if(!room) return;

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "AlterRoom - Missing field type.");
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "AlterRoom - Error in parsing.");
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
            pbugf(LOG_SCRIPTS, "AlterRoom - Error in parsing.");
            return;
        }
        switch(arg->type) {
        case ENT_STRING:
            if(!str_cmp(arg->d.str,"none")) { room->viewwilds = NULL; }
            break;
        case ENT_NUMBER:
            wilds = get_wilds_from_uid(NULL,arg->d.num);
            if(!wilds) {
                pbugf(LOG_SCRIPTS, "Not a valid wilds uid");
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
        
        // Must be a clone room
        if (!room_is_clone(room)) return;

        if(!(rest = expand_argument(info,rest,arg))) {
            pbugf(LOG_SCRIPTS, "AlterRoom - Error in parsing.");
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
            break;
        }

        return;
    }

    str = NULL;
    if(!str_cmp(field,"name"))			str = &room->name;
    else if(!str_cmp(field,"desc"))		{ str = &room->description; allow_empty = true; }
    else if(!str_cmp(field,"owner"))	{ str = &room->owner; allow_empty = true; min_sec = 9; }

    if(str) {
        // Can only change this on clone rooms
        if (!room_is_clone(room)) return;

        if(script_security < min_sec) {
            sprintf(buf,"AlterRoom - Attempting to alter '%s' with security %d.\n\r", field, script_security);
            wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
            pbug(LOG_SCRIPTS, buf);
            return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        if(!allow_empty && !buf_string(buffer)[0]) {
            pbugf(LOG_SCRIPTS, "AlterRoom - Empty string used.");
            free_buf(buffer);
            return;
        }

        free_string(*str);
        *str = str_dup(buf_string(buffer));
        free_buf(buffer);
        return;
    }

    rest = one_argument(rest,buf);
    int op = cmd_operator_lookup(buf);
    if (op == OPR_UNKNOWN)
        return;

    if (cmd_operator_info[op][OPR_NEEDS_VALUE])
    {
        if(!(rest = expand_argument(info,rest,arg))) {
            pbugf(LOG_SCRIPTS, "AlterRoom - Error in parsing.");
            return;
        }
    }

    if(!str_cmp(field,"flags"))				{ lptr = room->room_flag; bank = room_flagbank; }
    else if(!str_cmp(field,"light"))		{ ptr = (int*)&room->light; hasmin = true; min = 0; allowbitwise = false; }
    else if(!str_cmp(field,"sector"))		{ sector_value = room_sector_type(room); ptr = &sector_value; sector_field = true; typed_sector_field = true; }
    else if(!str_cmp(field,"heal"))			{ ptr = (int*)&room->heal_rate; min_sec = 1; }
    else if(!str_cmp(field,"mana"))			{ ptr = (int*)&room->mana_rate; min_sec = 1; }
    else if(!str_cmp(field,"move"))			{ ptr = (int*)&room->move_rate; min_sec = 1; }
    else if(!str_cmp(field,"mapx"))			{ ptr = (int*)&room->x; min_sec = 5; allow_static = false; }
    else if(!str_cmp(field,"mapy"))			{ ptr = (int*)&room->y; min_sec = 5; allow_static = false; }
    else if(!str_cmp(field,"rsflags"))		{ lptr = room->rs_room_flag; bank = room_flagbank; allow_static = false; }
    else if(!str_cmp(field,"rssector"))		{ rs_sector_value = room_rs_sector_type(room); ptr = &rs_sector_value; allow_static = false; rs_sector_field = true; typed_sector_field = true; }
    else if(!str_cmp(field,"rsheal"))		{ ptr = (int*)&room->rs_heal_rate; min_sec = 9; allow_static = false; }
    else if(!str_cmp(field,"rsmana"))		{ ptr = (int*)&room->rs_mana_rate; min_sec = 9; allow_static = false; }
    else if(!str_cmp(field,"rsmove"))		{ ptr = (int*)&room->rs_move_rate; min_sec = 1; allow_static = false; }
    else if(!str_cmp(field,"tempstore1"))	{ ptr = (int*)&room->tempstore[0]; }
    else if(!str_cmp(field,"tempstore2"))	{ ptr = (int*)&room->tempstore[1]; }
    else if(!str_cmp(field,"tempstore3"))	{ ptr = (int*)&room->tempstore[2]; }
    else if(!str_cmp(field,"tempstore4"))	{ ptr = (int*)&room->tempstore[3]; }

    if(!ptr && !sptr) return;

    if(script_security < min_sec) {
        sprintf(buf,"AlterRoom - Attempting to alter '%s' with security %d.\n\r", field, script_security);
        wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
        pbug(LOG_SCRIPTS, buf);
        return;
    }

    if (!allow_static && !room_is_clone(room))
        return;

    memset(temp_flags, 0, sizeof(temp_flags));

    if (typed_sector_field)
    {
        allowarith = false;

        switch (arg->type) {
        case ENT_STRING:
            value = sector_lookup(arg->d.str);
            if (value == NO_FLAG)
                return;
            break;

        case ENT_NUMBER:
            value = sector_type_sanitize(arg->d.num);
            break;

        default:
            return;
        }
    }
    else if( bank != NULL )
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


    if(lptr) {
        switch (op) {
        case OPR_ADD:
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "AlterRoom - alterroom called with arithmetic operator on a bitonly field.");
                return;
            }
            *lptr += value; break;

        case OPR_SUB:
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "AlterRoom - alterroom called with arithmetic operator on a bitonly field.");
                return;
            }
            *lptr -= value; break;

        case OPR_MULT:
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "AlterRoom - alterroom called with arithmetic operator on a bitonly field.");
                return;
            }
            *lptr *= value; break;

        case OPR_DIV:
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "AlterRoom - alterroom called with arithmetic operator on a bitonly field.");
                return;
            }
            if (!value) {
                pbugf(LOG_SCRIPTS, "AlterRoom - alterroom called with operator / and value 0");
                return;
            }
            *lptr /= value; break;

        case OPR_MOD:
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "AlterRoom - alterroom called with arithmetic operator on a bitonly field.");
                return;
            }
            if (!value) {
                pbugf(LOG_SCRIPTS, "AlterRoom - alterroom called with operator % and value 0");
                return;
            }
            *lptr %= value; break;

        case OPR_INC: (*lptr) += 1; break;
        case OPR_DEC: (*lptr) += 1; break;
        case OPR_MIN: *lptr = UMIN(*lptr, value); break;
        case OPR_MAX: *lptr = UMAX(*lptr, value); break;

        case OPR_ASSIGN:
            if( !allowbitwise ) {
                pbugf(LOG_SCRIPTS, "AlterRoom - alterroom called with bitwise operator on a non-bitvector field.");
                return;
            }
            if (bank != NULL)
            {
                for(int i = 0; bank[i]; i++)
                    lptr[i] = temp_flags[i];
            }
            else
                *lptr = value;
            break;

        case OPR_AND:
            if( !allowbitwise ) {
                pbugf(LOG_SCRIPTS, "AlterRoom - alterroom called with bitwise operator on a non-bitvector field.");
                return;
            }
            if (bank != NULL)
            {
                for(int i = 0; bank[i]; i++)
                    lptr[i] &= temp_flags[i];
            }
            else
                *lptr &= value;
            break;

        case OPR_OR:
            if( !allowbitwise ) {
                pbugf(LOG_SCRIPTS, "AlterRoom - alterroom called with bitwise operator on a non-bitvector field.");
                return;
            }
            if (bank != NULL)
            {
                for(int i = 0; bank[i]; i++)
                    lptr[i] |= temp_flags[i];
            }
            else
                *lptr |= value;
            break;

        case OPR_NOT:
            if( !allowbitwise ) {
                pbugf(LOG_SCRIPTS, "AlterRoom - alterroom called with bitwise operator on a non-bitvector field.");
                return;
            }
            if (bank != NULL)
            {
                for(int i = 0; bank[i]; i++)
                    lptr[i] &= ~temp_flags[i];
            }
            else
                *lptr &= ~value;
            break;

        case OPR_XOR:
            if( !allowbitwise ) {
                pbugf(LOG_SCRIPTS, "AlterRoom - alterroom called with bitwise operator on a non-bitvector field.");
                return;
            }
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

        if (hasmin && *lptr < min)
            *lptr = min;
        if(hasmax && *lptr > max)
            *lptr = max;

    } else if(ptr) {
        if (typed_sector_field && op != OPR_ASSIGN)
            return;

        switch (op) {
        case OPR_ADD:
            *ptr += value; break;

        case OPR_SUB:
            *ptr -= value; break;

        case OPR_MULT:
            *ptr *= value; break;

        case OPR_DIV:
            if (!value) {
                pbugf(LOG_SCRIPTS, "AlterRoom - alterroom called with operator / and value 0");
                return;
            }
            *ptr /= value; break;

        case OPR_MOD:
            if (!value) {
                pbugf(LOG_SCRIPTS, "AlterRoom - alterroom called with operator % and value 0");
                return;
            }

            *ptr %= value; break;

        case OPR_INC: (*ptr) += 1; break;
        case OPR_DEC: (*ptr) += 1; break;
        case OPR_MIN: *ptr = UMIN(*ptr, value); break;
        case OPR_MAX: *ptr = UMAX(*ptr, value); break;

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
                {
                    ptr[i] &= ~temp_flags[i];
                }
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

        if (hasmin && *ptr < min)
            *ptr = (int)min;
        if(hasmax && *ptr > max)
            *ptr = (int)max;

        if (sector_field)
            room_set_sector_type(room, *ptr);
        else if (rs_sector_field)
            room_set_rs_sector_type(room, *ptr);
    } else if (sptr) {
        switch (op) {
        case OPR_ADD: *sptr += value; break;
        case OPR_SUB: *sptr -= value; break;
        case OPR_MULT: *sptr *= value; break;
        case OPR_DIV:
            if (!value) {
                pbugf(LOG_SCRIPTS, "AlterRoom - adjust called with operator / and value 0");
                return;
            }
            *sptr /= value;
            break;
        case OPR_MOD:
            if (!value) {
                pbugf(LOG_SCRIPTS, "AlterRoom - adjust called with operator % and value 0");
                return;
            }
            *sptr %= value;
            break;

        case OPR_INC: (*sptr) += 1; break;
        case OPR_DEC: (*sptr) += 1; break;
        case OPR_MIN: *sptr = UMIN(*sptr, value); break;
        case OPR_MAX: *sptr = UMAX(*sptr, value); break;

        case OPR_ASSIGN: *sptr = value; break;
        case OPR_AND: *sptr &= value; break;
        case OPR_OR: *sptr |= value; break;
        case OPR_NOT: *sptr &= ~value; break;
        case OPR_XOR: *sptr ^= value; break;
        default:
            return;
        }

        if (hasmin && *sptr < min)
            *sptr = (int16_t)min;
        if(hasmax && *sptr > max)
            *sptr = (int16_t)max;
    }
}

// RESETROOM $ROOM
// Forces the room to reset
SCRIPT_CMD(scriptcmd_resetroom)
{
    char *rest = argument;

    if (!info) return;

    PARSE_ARGTYPE(ROOM);

    reset_room(arg->d.room, true);
}

// ADDSTACHE $MOBILE|$OBJECT $OBJECT
SCRIPT_CMD(scriptcmd_addstache)
{
    char *rest = argument;

    if (!info) return;

    SETRETURN(PRET_BADSYNTAX);
    if (!PARSE_ARG)
        return;

    if (!PARSE_ARG)
        return;

    CHAR_DATA *mob;
    OBJ_DATA *obj;
    if (arg->type == ENT_MOBILE)
    {
        mob = arg->d.mob;
        obj = NULL;
    }
    else if (arg->type == ENT_OBJECT)
    {
        mob = NULL;
        obj = arg->d.obj;
    }
    else
        return;

    if (!IS_VALID(mob) && !IS_VALID(obj))
        return;

    PARSE_ARGTYPE(OBJECT);
    OBJ_DATA *item = arg->d.obj;

    if (item->locker || item->stached)
        return;

    if (item->in_obj != NULL)
        obj_from_obj(item);
    else if (item->carried_by != NULL)
        obj_from_char(item);
    else
        return;

    LLIST *stache = NULL;
    if (IS_VALID(mob))
        stache = mob->lstache;
    else if (IS_VALID(obj))
        stache = obj->lstache;
    else
        return;

    item->next_content = NULL;
    item->stached = true;
    list_appendlink(stache, item);
    SETRETURN(1);
}

// REMSTACHE $MOBILE|$OBJECT $OBJECT[ $VARNAME]
// REMSTACHE $MOBILE|$OBJECT $NUMBER[ $VARNAME]
// REMSTACHE $MOBILE|$OBJECT $WIDEVNUM[ $VARNAME]
// REMSTACHE $MOBILE|$OBJECT $[#.]NAME[ $VARNAME]
SCRIPT_CMD(scriptcmd_remstache)
{
    char *rest = argument;

    if (!info) return;

    SETRETURN(0);
    if (!PARSE_ARG)
        return;

    LLIST *stache = NULL;
    CHAR_DATA *mob = NULL;
    OBJ_DATA *obj = NULL;
    if (arg->type == ENT_MOBILE)
    {
        stache = IS_VALID(arg->d.mob) ? arg->d.mob->lstache : NULL;
        mob = arg->d.mob;
    }
    else if (arg->type == ENT_OBJECT)
    {
        stache = IS_VALID(arg->d.obj) ? arg->d.obj->lstache : NULL;
        obj = arg->d.obj;
    }
    else
        return;

    if (!IS_VALID(stache))
        return;

    if (!PARSE_ARG)
        return;

    ITERATOR it;
    OBJ_DATA *item = NULL;
    if (arg->type == ENT_OBJECT)
    {
        item = arg->d.obj;
        list_remlink(stache, item, false);
    }
    else if (arg->type == ENT_NUMBER)
    {
        item = list_nthdata(stache, arg->d.num);
        list_remnthlink(stache, arg->d.num, false);
    }
    else if (arg->type == ENT_WIDEVNUM)
    {
        WNUM wnum = arg->d.wnum;

        iterator_start(&it, stache);
        while((item = (OBJ_DATA *)iterator_nextdata(&it)))
        {
            if (wnum_match_obj(wnum, item))
            {
                iterator_remcurrent(&it);
                break;
            }
        }
        iterator_stop(&it);
    }
    else if (arg->type == ENT_STRING)
    {
        int count;
        char name[MSL];

        count = number_argument(arg->d.str, name);
        if (count < 1)
            return;

        WNUM wnum;
        if (parse_widevnum(name, get_area_from_scriptinfo(info), &wnum))
        {
            iterator_start(&it, stache);
            while((item = (OBJ_DATA *)iterator_nextdata(&it)))
            {
                if (wnum_match_obj(wnum, item) && !--count)
                {
                    iterator_remcurrent(&it);
                    break;
                }
            }
            iterator_stop(&it);
        }
        else
        {
            iterator_start(&it, stache);
            while((item = (OBJ_DATA *)iterator_nextdata(&it)))
            {
                if (is_name(name, item->name) && !--count)
                {
                    iterator_remcurrent(&it);
                    break;
                }
            }
            iterator_stop(&it);
        }
    }
    else
        return;

    if (!item)
        return;

    char *var_name = NULL;
    if (rest && *rest)
    {
        PARSE_ARGTYPE(STRING);
        var_name = arg->d.str;
    }

    item->stached = false;
    if (mob != NULL)
        obj_to_char(item, mob);
    else if (obj != NULL)
        obj_to_obj(item, obj);

    if (var_name)
        variables_set_object(info->var, var_name, item);
    SETRETURN(1);
}

/*
        "mail to <person>  (start a mail package)\n\r"
        "mail show         (show what is currently in the package)\n\r"
        "mail put <object> (add an object to the package)\n\r"
        "mail get <object> (remove an object from the package)\n\r"
        "mail write        (write a message)\n\r"
        "mail cancel       (cancel the mail)\n\r"
        "mail send         (pay postage and send the package)\n\r"
        "mail check        (check your mail)\n\r"
        "mail info         (see the status of mail you've sent/received)\n\r", ch);
*/

// MAIL $SENDER(string) $PERSON(string) $MESSAGE(string)[ ...$OBJECT]
SCRIPT_CMD(scriptcmd_mail)
{
    char *rest = argument;
    char sender[MIL];
    char person[MIL];
    int ret = 1;
    bool valid = true;	
    MAIL_DATA *mail = NULL;
    ITERATOR it;
    OBJ_DATA *package;

    if (!info || script_security < 9) return;

    SETRETURN(0);

    PARSE_ARGTYPE(STRING);
    strncpy(sender, arg->d.str, MIL - 1);
    
    PARSE_ARGTYPE(STRING);
    strncpy(person, arg->d.str, MIL - 1);
    if (!player_exists(person)) return;

    if (!PARSE_ARG) return;
    

    BUFFER *message = new_buf();

    if (arg->type == ENT_STRING)
        add_buf(message, arg->d.str);
    else if (arg->type == ENT_NULL)
        clear_buf(message);
    else
        valid = false;

    if (message->state != BUFFER_SAFE)
        valid = false;

    LLIST *packages = list_create(false);

    int total_weight = 0;
    while(valid && rest && *rest)
    {
        if (!PARSE_ARG || arg->type != ENT_OBJECT)
            valid = false;
        else if(arg->d.obj->in_mail || arg->d.obj->locker || arg->d.obj->stached || arg->d.obj->pulled_by)
            valid = false;
        else if (!list_appendlink(packages, arg->d.obj))
            valid = false;
        else
        {
            total_weight += arg->d.obj->weight;
            if (total_weight > MAX_POSTAL_WEIGHT) valid = false;
            if (list_size(packages) > MAX_POSTAL_ITEMS) valid = false;
        }
    }

    if (valid)
    {
        // Send the mail
        mail = new_mail();
        mail->sender = str_dup(sender);
        mail->recipient = str_dup(person);
        mail->message = str_dup(message->string);
        mail->originating_script = info->block->script->vnum;
        mail->orig_script_type = info->block->script->type;
        
        
        
        iterator_start(&it, packages);
        while((package = (OBJ_DATA *)iterator_nextdata(&it)))
        {
            // This will check for whether the object is elegible to be put into mail, not whether it will fit
            if (!can_put_obj(NULL, package, NULL, mail, true))
            {
                valid = false;
                break;
            }
        }
        iterator_stop(&it);

        if (valid)
        {
            iterator_start(&it, packages);
            while((package = (OBJ_DATA *)iterator_nextdata(&it)))
            {
                if (package->carried_by)
                    obj_from_char(package);
                else if (package->in_obj)
                    obj_from_obj(package);
                else if (package->in_room)
                    obj_from_room(package);

                obj_to_mail(package, mail);
            }
            iterator_stop(&it);
        }
    }

    list_destroy(packages);
    free_buf(message);

    if (valid && mail)
    {
        // Send the actual mail
        MAIL_DATA *mail_tmp;
        for (mail_tmp = mail_list; mail_tmp && mail_tmp->next; mail_tmp = mail_tmp->next);

        /* hook it up to the list */
        mail->next = NULL;
        if (mail_list != NULL)
            mail_tmp->next = mail;
        else
            mail_list = mail;
        mail->scripted = true;
        mail->sent_date = current_time;
        mail->status = MAIL_BEING_DELIVERED;
    }
    else
    {
        if (mail) free_mail(mail);
        ret = 0;
    }

    SETRETURN(ret);
}

// Sends an expanded string to the given wiznet channel.
// Syntax: wiznet <channel> <string>
SCRIPT_CMD(scriptcmd_wiznet)
{
    char *rest = argument;
    char wiznet_name[MIL];
    int wiznet_flag = 0;
    int i;
    bool valid = false;

    if (!info) return;

    PARSE_ARGTYPE(STRING);
    strncpy(wiznet_name, arg->d.str, MIL - 1);

    for (i = 0; wiznet_table[i].name; i++)
    {
        if (!str_prefix(wiznet_name, wiznet_table[i].name))
        {
            wiznet_flag = wiznet_table[i].flag;
            valid = true;
            break;
        }
    }

    if (!str_cmp(wiznet_name, "on") || !str_cmp(wiznet_name, "prefix")) 
        valid = false;

    if (!valid) return;

    // Expand the message
    BUFFER *buffer = new_buf();
    expand_string(info,rest,buffer);

    // Broadcast the message
    if(buffer->string)
    {
        wiznet(buffer->string, NULL, NULL, wiznet_flag, 0, 0);
    }
    free_buf(buffer);
}