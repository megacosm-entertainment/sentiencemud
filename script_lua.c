/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

// This module will be used to handle LUA aspects
#include "merc.h"
#include "db.h"
#include "scripts.h"

/////////////////////////////////////////
// Definitions

#define lua_c
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"

#define LUA_PATH	"lua/"
#define LUA_EXT		".lua"

struct script_lua_data {
	lua_State *L;		// LUA state for the script
	char *code;		// Compiled code...
	unsigned long len;	// Length of compiled code
};

/////////////////////////////////////////
// System

static int _lua_buildcode(lua_State* L, const void* p, size_t size, void* u)
{
	SCRIPT_LUA *lua = (SCRIPT_LUA *)u;

	((void)(L));

	if(size > 0) {
		if(!lua->code) {
			lua->code = alloc_mem(size);
			if(!lua->code) return 1;
			lua->len = 0;
		} else {
			char *code = alloc_mem(lua->len + size);
			if(!code) return 1;
			memcpy(code,lua->code,lua->len);
			lua->code = code;
		}

		memcpy(lua->code + lua->len,p,size);
		lua->len += size;
	}
	return 0;
}

bool script_readlua(SCRIPT_DATA *script, char *path)
{
	lua_State *L;
	const Proto *f;

	L = script->lua->L;

	// Load the source string into the LUA state, which will compile the code
	if(luaL_loadstring(L,script->edit_src)) return FALSE;

	// Grab the compiled information
	f = clvalue(L->top - 1)->l.p;

	// Grab the actual compiled data
	lua_lock(L);
	luaU_dump(L,f,_lua_buildcode,script->lua,0);
	lua_unlock(L);

	return TRUE;
}



bool script_loadlua(SCRIPT_DATA *script)
{
	// File name format: lua/<TYPE><VNUM>.lua
	char path[MSL];
	char *types = "mort";

	if(script->type == -1) return FALSE;

	sprintf(path,LUA_PATH "%c%ld" LUA_EXT, types[script->type], script->vnum);

	script->src = fread_filename(path);
	script->edit_src = script->src;
	script->lua = alloc_mem(sizeof(SCRIPT_LUA));
	ISSET_BIT(script->flags,SCRIPT_LUA);

	script->lua->L = lua_newstate();
	if(!script->lua->L) return FALSE;

	// The LUA state created here will be kept, why?
	//	Because I want to have the option down the road to create global scripting
	//	whereby the executed data and assigned values are kept, akin to how MUSHClient
	//	has its scripting environment.
	//

	return script_readlua(script,path);
}

void script_freelua(SCRIPT_DATA *script)
{
	if(script && script->lua) {
		// Free anything needed for this
		if(script->lua->L) {
			lua_close(script->lua->L);
			script->lua->L = NULL;
		}

		if(script->lua->code) {
			free_mem(script->lua->code);
			script->lua->code = NULL;
		}

		free_mem(script->lua);
		script->lua = NULL;
	}
}

static void config_lua_globals(SCRIPT_BLOCK *block)
{
}

void execute_lua_script(SCRIPT_BLOCK *block)
{
	block->next = script_call_stack;
	script_call_stack = block;


	block->ret_val = PRET_EXECUTED;
}


/////////////////////////////////////////
// Script API


	{ "addaffect",			do_mpaddaffect,		TRUE	},
	{ "addaffectname",		do_mpaddaffectname,		TRUE	},
	{ "airshipaddwaypoint", 	do_mpairshipaddwaypoint,	TRUE	},
	{ "airshipsetcrash", 		do_mpairshipsetcrash,		TRUE	},
	{ "alterexit",			do_mpalterexit,		FALSE	},
	{ "altermob",			do_mpaltermob,			TRUE	},
	{ "alterobj",			do_mpalterobj,			TRUE	},
	{ "appear",			do_mpvis,			FALSE	},
	{ "asound", 			do_mpasound,			FALSE	},
	{ "assist",			do_mpassist,			FALSE	},
	{ "at",				do_mpat,			FALSE	},
	{ "awardgold",			do_mpawardgold,			TRUE	},
	{ "awardpneuma",		do_mpawardpneuma,		TRUE	},
	{ "awardprac",			do_mpawardprac,			TRUE	},
	{ "awardqp",			do_mpawardqp,			TRUE	},
	{ "awardxp",			do_mpawardxp,			TRUE	},
	{ "call",			do_mpcall,			FALSE	},
	{ "cancel",			do_mpcancel,			FALSE	},
	{ "cast",			do_mpcast,			FALSE	},
	{ "changevesselname",		do_mpchangevesselname,		TRUE	},
	{ "chargemoney",		do_mpchargemoney,		FALSE	},
	{ "damage",			do_mpdamage,			FALSE	},
	{ "decdeity",			do_mpdecdeity,			TRUE	},
	{ "decpneuma",			do_mpdecpneuma,			TRUE	},
	{ "decprac",			do_mpdecprac,			TRUE	},
	{ "decquest",			do_mpdecquest,			TRUE	},
	{ "dectrain",			do_mpdectrain,			TRUE	},
	{ "delay",			do_mpdelay,			FALSE	},
	{ "dequeue",			do_mpdequeue,			FALSE	},
	{ "disappear",    		do_mpinvis,			FALSE	},
	{ "echo",			do_mpecho,			FALSE	},
	{ "echoaround",			do_mpechoaround,		FALSE	},
	{ "echoat",			do_mpechoat,			FALSE	},
	{ "echobattlespam",		do_mpechobattlespam,		FALSE	},
	{ "echochurch",			do_mpechochurch,		FALSE	},
	{ "echogrouparound",		do_mpechogrouparound,		FALSE	},
	{ "echogroupat",		do_mpechogroupat,		FALSE	},
	{ "echoleadaround",		do_mpecholeadaround,		FALSE	},
	{ "echoleadat",			do_mpecholeadat,		FALSE	},
	{ "echonotvict",		do_mpechonotvict,		FALSE	},
	{ "flee",			do_mpflee,			FALSE	},
	{ "force",			do_mpforce,			FALSE	},
	{ "forget",			do_mpforget,			FALSE	},
	{ "gdamage",			do_mpgdamage,			FALSE	},
	{ "gecho",			do_mpgecho,			FALSE	},
	{ "gforce",			do_mpgforce,			FALSE	},
	{ "goto",			do_mpgoto,			FALSE	},
	{ "gtransfer",			do_mpgtransfer,			FALSE	},
	{ "hunt",			do_mphunt,			FALSE	},
	{ "input",			do_mpinput,			FALSE	},
	{ "interrupt",			do_mpinterrupt,			FALSE	},
	{ "junk",			do_mpjunk,			FALSE	},
	{ "kill",			do_mpkill,			FALSE	},
	{ "link",			do_mplink,			FALSE	},
	{ "mload",			do_mpmload,			FALSE	},
	{ "oload",			scriptcmd_oload,			FALSE	},
	{ "otransfer",			do_mpotransfer,			FALSE	},
	{ "peace",			do_mppeace,			FALSE	},
	{ "prompt",			do_mpprompt,			FALSE	},
	{ "purge",			do_mppurge,			FALSE	},
	{ "queue",			do_mpqueue,			FALSE	},
	{ "raisedead",			do_mpraisedead,			TRUE	},
	{ "rawkill",			do_mprawkill,			FALSE	},
	{ "remember",			do_mpremember,			FALSE	},
	{ "remove",			do_mpremove,			FALSE	},
	{ "resetdice",			do_mpresetdice,			TRUE	},
	{ "selfdestruct",		do_mpselfdestruct,		FALSE	},
	{ "settimer",			do_mpsettimer,			FALSE	},
	{ "skimprove",			do_mpskimprove,			TRUE	},
	{ "stringobj",			do_mpstringobj,			TRUE	},
	{ "stringmob",			do_mpstringmob,			TRUE	},
	{ "stripaffect",		do_mpstripaffect,		TRUE	},
	{ "stripaffectname",		do_mpstripaffectname,		TRUE	},
	{ "take",			do_mptake,			FALSE	},
	{ "teleport", 			do_mpteleport,			FALSE	},
	{ "usecatalyst",		do_mpusecatalyst,		FALSE	},
	{ "varset",			do_mpvarset,			FALSE	},
	{ "varclear",			do_mpvarclear,			FALSE	},
	{ "varclearon",			do_mpvarclearon,		FALSE	},
	{ "varcopy",			do_mpvarcopy,			FALSE	},
	{ "varsave",			do_mpvarsave,			FALSE	},
	{ "varsaveon",			do_mpvarsaveon,			FALSE	},
	{ "varset",			do_mpvarset,			FALSE	},
	{ "varseton",			do_mpvarseton,			FALSE	},
	{ "vforce",			do_mpvforce,			FALSE	},
	{ "zot",			do_mpzot,			TRUE	},

// echo.room		(<STRING>[,<LOCATION>])
// echo.at		(<STRING>,<MOBILE>)
// echo.around		(<STRING>,<MOBILE>)
// echo.zone		(<STRING>[,<LOCATION>])
// echo.global		(<STRING>)
// echo.battlespam	(<STRING>,<MOBILE>,<MOBILE>)
// echo.church		(<STRING>,<MOBILE|CHURCH|STRING>)
// echo.grouparound	(<STRING>,<MOBILE>)
// echo.groupat		(<STRING>,<MOBILE>)
// echo.leadat		(<STRING>,<MOBILE>)
// echo.leadaround	(<STRING>,<MOBILE>)
// echo.follow		(<STRING>,<MOBILE>)
// echo.notvict		(<STRING>,<MOBILE>,<MOBILE>)
// echo.list		(<STRING>,<MOBILE>...)
// echo.notlist		(<STRING>,<MOBILE>...)
// echo.func		(<STRING>,<FUNCTION(<MOBILE>,<DATA>)>[,<DATA>])
// echo.asound		(<STRING>[,<LOCATION>])

// transfer.single	(<TARGET>,<LOCATION>[,<FLAGS>])
// transfer.list	(<TABLE(TARGET)>,<LOCATION>[,<FLAGS>])
// transfer.group	(<TARGET>,<LOCATION>[,<ALL>])
// transfer.room	(<LOCATION>,<LOCATION>[,<FLAGS>])

// game.online		([<FILTER>]) -> TABLE(<PLAYER>)
// game.time		() -> <NUMBER>
// game.clock		() -> <NUMBER>
// game.hour		() -> <NUMBER>
// game.minute		() -> <NUMBER>
// game.day		() -> <NUMBER>
// game.month		() -> <NUMBER>
// game.year		() -> <NUMBER>

// cmd.execute		(<STRING>[,<LOCATION>...])
// cmd.prompt		(<PLAYER>,<NAME>[,<STRING>])
// cmd.at		(<LOCATION>,<FUNCTION>)
// cmd.call		(<VNUM>,<ENACTOR>,<MOBILE>,<OBJECT>,<OBJECT>) -> <NUMBER>

// var.set		(<STRING>,<VALUE>[,<TARGET>]) -> <BOOLEAN>
// var.get		(<STRING>[,<TARGET>]) -> <VALUE|nil>
// var.save		(<STRING>,<BOOLEAN>[,<TARGET>])
// var.clear		(<STRING>[,<TARGET>])

// memory.remember	(<TARGET>)
// memory.forget	(<TARGET>) = <TARGET> or nil

// affect.add		(<TARGET>,<WHERE>,<GROUP>,<NAME>,<LEVEL>,<LOCATION>,<MODIFIER>,<DURATION>,TABLE(<STRING>),<BOOLEAN>)
// affect.strip		(<TARGET>,<NAME>,<BOOLEAN>)

// -- these require security checks internal
// -- In fact, I may, if the script doesn't have the right securities
// --   not even load up the secured module
// secure.alter		(<MOBILE|OBJECT|EXIT|ROOM>,<FIELD>,<VALUE>)
// secure.string	(<MOBILE|OBJECT>,<FIELD>,<STRING>)
// secure.rawkill	(<MOBILE>,<TYPE>[,<HEADLESS>])
// secure.award		(<PLAYER>,<TYPE>,<NUMBER>)
// secure.deduct	(<PLAYER>,<TYPE>,<NUMBER>)
// secure.force		(<MOBILE>,<STRING>,<GROUP>)

// mobile.hunt		(<TARGET>)
// mobile.clone		(<MOBILE|VNUM>[,<LOCATION>]) -> <MOBILE>
// mobile.load		(<VNUM>,[,<LOCATION>]) -> <MOBILE>
// mobile.assist	(<MOBILE>[,<GROUP>])
// mobile.damage	(<MOBILE>,<TYPE>,<MIN>,<MAX>,<LETHAL>,<SPLASH>)




// object.load		(<VNUM>,[,<LOCATION>]) -> <OBJECT>
// object.junk		(<OBJECT>) or (TABLE(<OBJECT>)) or (<CONTAINER>,<STRING>)

// room.random		([<FILTER>]) -> <ROOM>
// room.clone		(<ROOM|VNUM>) -> <ROOM>
// room.create		() -> <ROOM>
// room.exit		(<LOCATION>,<NUMBER|STRING>) = <EXIT>
// room.link		(<LOCATION>,<NUMBER|STRING>,<LOCATION>,<FLAGS>)

// exit.unlink		(<LOCATION>,<NUMBER|STRING>)
// exit.hide		(<LOCATION>,<NUMBER|STRING>)
// exit.show		(<LOCATION>,<NUMBER|STRING>)
// exit.hide		(<LOCATION>,<NUMBER|STRING>)
// exit.hide		(<LOCATION>,<NUMBER|STRING>)


// token.give		(<ENTITY>,<VNUM>) -> <TOKEN>
// token.junk		(<ENTITY>,<VNUM>) or (<TOKEN>)
// token.adjust		(<ENTITY>,<VNUM>,<FIELD>,<OPERATOR>,<VALUE>) or (<TOKEN>,<FIELD>,<OPERATOR>,<VALUE>)

// skill.find		(<STRING>[,<MOBILE>]) -> <SKILL|nil>
// skill.improve	(<MOBILE>,<STRING>,<SUCCESS>,<BIAS>,<SILENT>)

// magic.usecatalyst	(<TARGET>,<TYPE>,<METHOD>,<AMOUNT>,<MIN>,<MAX>,<SHOW>)
// magic.cast		(<STRING>,<TARGET>[,<LEVEL>])

// event.cancel		()
// event.delay		(<NUMBER>)
// event.queue		(<NUMBER>,<STRING|FUNCTION>)
// event.clear		()

// shop.charge		(<MOBILE>,<NUMBER>,<NUMBER>)

//---------------------------------------
// Variables

// info.enactor		ENTITY
// info.room		ROOM
// info.target		MOBILE
// info.victim		MOBILE
// info.object1		OBJECT
// info.object2		OBJECT
// info.rand.mob	MOBILE
// info.rand.obj	OBJECT
// info.phrase		STRING
// info.trigger		STRING


//---------------------------------------
// Mobile

// MOBILE:name()		STRING
// MOBILE:short()		STRING
// MOBILE:long()		STRING
// MOBILE:level()		NUMBER
// MOBILE:race()		STRING, NUMBER
// MOBILE:flee(<NUMBER>)	NUMBER or nil
// MOBILE:goto(<LOCATION>)	ROOM
// MOBILE:invis([<BOOLEAN>])	BOOLEAN
// MOBILE:kill(<MOBILE>)
// MOBILE:destroy()

// Object
// OBJECT:name()
// OBJECT:short()
// OBJECT:long()
// OBJECT:level()
// OBJECT:cond()
// OBJECT:frag()
// OBJECT:weight()
// OBJECT:extra()
// OBJECT:extra2()
// OBJECT:extra3()
// OBJECT:extra4()
// OBJECT:type()
// OBJECT:destroy()

// Exits
// EXIT:dir()			NUMBER
// EXIT:name()			STRING
// EXIT:destroy()
// EXIT:hide()
// EXIT:show()
// EXIT:isopen()		BOOLEAN
// EXIT:hasdoor()		BOOLEAN
