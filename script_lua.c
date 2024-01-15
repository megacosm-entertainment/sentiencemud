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
	if(luaL_loadstring(L,script->edit_src)) return false;

	// Grab the compiled information
	f = clvalue(L->top - 1)->l.p;

	// Grab the actual compiled data
	lua_lock(L);
	luaU_dump(L,f,_lua_buildcode,script->lua,0);
	lua_unlock(L);

	return true;
}



bool script_loadlua(SCRIPT_DATA *script)
{
	// File name format: lua/<TYPE><VNUM>.lua
	char path[MSL];
	char *types = "mort";

	if(script->type == -1) return false;

	sprintf(path,LUA_PATH "%c%ld" LUA_EXT, types[script->type], script->vnum);

	script->src = fread_filename(path);
	script->edit_src = script->src;
	script->lua = alloc_mem(sizeof(SCRIPT_LUA));
	ISSET_BIT(script->flags,SCRIPT_LUA);

	script->lua->L = lua_newstate();
	if(!script->lua->L) return false;

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


	{ "addaffect",			do_mpaddaffect,		true	},
	{ "addaffectname",		do_mpaddaffectname,		true	},
	{ "airshipaddwaypoint", 	do_mpairshipaddwaypoint,	true	},
	{ "airshipsetcrash", 		do_mpairshipsetcrash,		true	},
	{ "alterexit",			do_mpalterexit,		false	},
	{ "altermob",			do_mpaltermob,			true	},
	{ "alterobj",			scriptcmd_alterobjmt,			true	},
	{ "appear",			do_mpvis,			false	},
	{ "asound", 			do_mpasound,			false	},
	{ "assist",			do_mpassist,			false	},
	{ "at",				do_mpat,			false	},
	{ "awardgold",			do_mpawardgold,			true	},
	{ "awardpneuma",		do_mpawardpneuma,		true	},
	{ "awardprac",			do_mpawardprac,			true	},
	{ "awardqp",			do_mpawardqp,			true	},
	{ "awardxp",			do_mpawardxp,			true	},
	{ "call",			do_mpcall,			false	},
	{ "cancel",			do_mpcancel,			false	},
	{ "cast",			do_mpcast,			false	},
	{ "changevesselname",		do_mpchangevesselname,		true	},
	{ "chargemoney",		do_mpchargemoney,		false	},
	{ "damage",			do_mpdamage,			false	},
	{ "decdeity",			do_mpdecdeity,			true	},
	{ "decmission",			do_mpdecmission,			true	},
	{ "decpneuma",			do_mpdecpneuma,			true	},
	{ "decprac",			do_mpdecprac,			true	},
	{ "dectrain",			do_mpdectrain,			true	},
	{ "delay",			do_mpdelay,			false	},
	{ "dequeue",			do_mpdequeue,			false	},
	{ "disappear",    		do_mpinvis,			false	},
	{ "echo",			do_mpecho,			false	},
	{ "echoaround",			do_mpechoaround,		false	},
	{ "echoat",			do_mpechoat,			false	},
	{ "echobattlespam",		do_mpechobattlespam,		false	},
	{ "echochurch",			do_mpechochurch,		false	},
	{ "echogrouparound",		do_mpechogrouparound,		false	},
	{ "echogroupat",		do_mpechogroupat,		false	},
	{ "echoleadaround",		do_mpecholeadaround,		false	},
	{ "echoleadat",			do_mpecholeadat,		false	},
	{ "echonotvict",		do_mpechonotvict,		false	},
	{ "flee",			do_mpflee,			false	},
	{ "force",			do_mpforce,			false	},
	{ "forget",			do_mpforget,			false	},
	{ "gdamage",			do_mpgdamage,			false	},
	{ "gecho",			do_mpgecho,			false	},
	{ "gforce",			do_mpgforce,			false	},
	{ "goto",			do_mpgoto,			false	},
	{ "gtransfer",			do_mpgtransfer,			false	},
	{ "hunt",			do_mphunt,			false	},
	{ "input",			do_mpinput,			false	},
	{ "interrupt",			do_mpinterrupt,			false	},
	{ "junk",			do_mpjunk,			false	},
	{ "kill",			do_mpkill,			false	},
	{ "link",			do_mplink,			false	},
	{ "mload",			do_mpmload,			false	},
	{ "oload",			scriptcmd_oload,			false	},
	{ "otransfer",			do_mpotransfer,			false	},
	{ "peace",			do_mppeace,			false	},
	{ "prompt",			do_mpprompt,			false	},
	{ "purge",			do_mppurge,			false	},
	{ "queue",			do_mpqueue,			false	},
	{ "raisedead",			do_mpraisedead,			true	},
	{ "rawkill",			do_mprawkill,			false	},
	{ "remember",			do_mpremember,			false	},
	{ "remove",			do_mpremove,			false	},
	{ "resetdice",			do_mpresetdice,			true	},
	{ "selfdestruct",		do_mpselfdestruct,		false	},
	{ "settimer",			do_mpsettimer,			false	},
	{ "skimprove",			do_mpskimprove,			true	},
	{ "stringobj",			scriptcmd_stringobjmt,			true	},
	{ "stringmob",			do_mpstringmob,			true	},
	{ "stripaffect",		do_mpstripaffect,		true	},
	{ "stripaffectname",		do_mpstripaffectname,		true	},
	{ "take",			do_mptake,			false	},
	{ "teleport", 			do_mpteleport,			false	},
	{ "usecatalyst",		do_mpusecatalyst,		false	},
	{ "varset",			do_mpvarset,			false	},
	{ "varclear",			do_mpvarclear,			false	},
	{ "varclearon",			do_mpvarclearon,		false	},
	{ "varcopy",			do_mpvarcopy,			false	},
	{ "varsave",			do_mpvarsave,			false	},
	{ "varsaveon",			do_mpvarsaveon,			false	},
	{ "varset",			do_mpvarset,			false	},
	{ "varseton",			do_mpvarseton,			false	},
	{ "vforce",			do_mpvforce,			false	},
	{ "zot",			do_mpzot,			true	},

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
