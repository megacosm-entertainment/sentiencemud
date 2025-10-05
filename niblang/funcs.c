#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <ctype.h>

#include "../merc.h"
#include "niblang.h"
#include "script.h"
#include "interpret.h"
#include "tables.h"

extern char * const dir_name[];
#define IS__TYPE(n,t)	(argv[(n)].type == NST_##t)
#define IS_LV__TYPE(n,t)	(argv[(n)]._.lvalue.type == NST_##t)
#define IS_NUM32(n)		(IS__TYPE(n,NUMBER32))
#define IS_NUM(n)		(IS__TYPE(n,NUMBER))
#define IS_FLOAT(n)		(IS__TYPE(n,FLOAT))
#define IS_BOOL(n)		(IS__TYPE(n,BOOLEAN))
#define IS_CHAR(n)		(IS__TYPE(n,CHAR))
#define IS_STR(n)		(IS__TYPE(n,STRING) || IS__TYPE(n,STRING_S))
#define IS_WNUM(n)		(IS__TYPE(n,WIDEVNUM))
#define IS_LIST(n)		(IS__TYPE(n,LIST) || IS__TYPE(n,LIST_S))
#define IS_ARRAY(n)		(IS__TYPE(n,ARRAY) || IS__TYPE(n,ARRAY_S))
#define IS_AFFECT(n)	(IS__TYPE(n,AFFECT))
#define IS_AREA(n)		(IS__TYPE(n,AREA))
#define IS_EXIT(n)		(IS__TYPE(n,EXIT))
#define IS_MOB(n)		(IS__TYPE(n,MOBILE))
#define IS_OBJ(n)		(IS__TYPE(n,OBJECT))
#define IS_ROOM(n)		(IS__TYPE(n,ROOM))
#define IS_SECTOR(n)	(IS__TYPE(n,SECTOR))
#define IS_TOKEN(n)		(IS__TYPE(n,TOKEN))
#define IS_LVALUE(n)	(IS__TYPE(n,LVALUE))
#define IS_LV_NUM32(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,NUMBER32)))
#define IS_LV_NUM(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,NUMBER)))
#define IS_LV_FLOAT(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,FLOAT)))
#define IS_LV_BOOL(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,BOOLEAN)))
#define IS_LV_BIT(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,FLAG_BIT)))
#define IS_LV_CHAR(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,CHAR)))
#define IS_LV_STR(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,STRING)))
#define IS_LV_MAP(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,MAP)))
#define IS_LV_WNUM(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,WIDEVNUM)))
#define IS_LV_LIST(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,LIST)))
#define IS_LV_ARRAY(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,ARRAY)))
#define IS_LV_AFFECT(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,AFFECT)))
#define IS_LV_AREA(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,AREA)))
#define IS_LV_EXIT(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,EXIT)))
#define IS_LV_MOB(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,MOBILE)))
#define IS_LV_OBJ(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,OBJECT)))
#define IS_LV_ROOM(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,ROOM)))
#define IS_LV_SECTOR(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,SECTOR)))
#define IS_LV_TOKEN(n)	(IS_LVALUE(n) && (IS_LV__TYPE(n,TOKEN)))

#define ARG__TYPE(n,f)	(argv[(n)]._.f)
#define ARG_NUM(n)		(ARG__TYPE(n,i))
#define ARG_FLT(n)		(ARG__TYPE(n,d))
#define ARG_BLN(n)		(ARG__TYPE(n,b))
#define ARG_CHR(n)		(ARG__TYPE(n,ch))
#define ARG_STR(n)		(ARG__TYPE(n,str))
#define ARG_WNUM(n)		(ARG__TYPE(n,wnum))
#define ARG__LIST(n)	(ARG__TYPE(n,list))
#define ARG_LIST(n)		(ARG__TYPE(n,list.list))
#define ARG__ARRAY(n)	(ARG__TYPE(n,array))
#define ARG_ARRAY(n)	(ARG__TYPE(n,array.ptr))
#define ARG_AFFECT(n)	(ARG__TYPE(n,affect))
#define ARG_AREA(n)		(ARG__TYPE(n,area))
#define ARG_EXIT(n)		(ARG__TYPE(n,ex))
#define ARG_MOB(n)		(ARG__TYPE(n,mobile))
#define ARG_OBJ(n)		(ARG__TYPE(n,object))
#define ARG_ROOM(n)		(ARG__TYPE(n,room))
#define ARG_SECTOR(n)	(ARG__TYPE(n,sector))
#define ARG_TOK(n)		(ARG__TYPE(n,token))

#define LV__FLD(n,f)	(argv[(n)]._.lvalue._.f)
#define LV_NUM32(n)		(*(LV__FLD(n,number32)))
#define LV_NUM(n)		(*(LV__FLD(n,number)))
#define LV_FLT(n)		(*(LV__FLD(n,d)))
#define LV_BLN(n)		(*(LV__FLD(n,b)))
#define LV__BIT(n)		(LV__FLD(n,bit))
#define LV_CHR(n)		(*(LV__FLD(n,ch)))
#define LV_STR(n)		(*(LV__FLD(n,str)))
#define LV_WVUM(n)		(*(LV__FLD(n,wnum)))
#define LV__LIST(n)		(LV__FLD(n,list))
#define LV_LIST(n)		(*(LV__FLD(n,list.list)))
#define LV__ARRAY(n)	(LV__FLD(n,array))
#define LV_ARRAY(n)		(LV__FLD(n,array.ptr))
#define LV_AFFECT(n)	(*(LV__FLD(n,affect)))
#define LV_AREA(n)		(*(LV__FLD(n,area)))
#define LV_EXIT(n)		(*(LV__FLD(n,ex)))
#define LV_MOB(n)		(*(LV__FLD(n,mobile)))
#define LV_OBJ(n)		(*(LV__FLD(n,object)))
#define LV_ROOM(n)		(*(LV__FLD(n,room)))
#define LV_SECTOR(n)	(*(LV__FLD(n,sector)))
#define LV_TOKEN(n)		(*(LV__FLD(n,token)))

#define IS_ARG_NUM(n)	(IS_LV_NUM32((n)) || IS_LV_NUM((n)) || IS_NUM((n)))
#define IS_ARG_FLT(n)	(IS_LV_FLOAT((n)) || IS_FLOAT((n)))
#define IS_ARG_BLN(n)	(IS_LV_BOOL((n)) || IS_BOOL((n)))
#define IS_ARG_BIT(n)	(IS_LV_BIT((n)))
#define IS_ARG_CHR(n)	(IS_LV_CHAR((n)) || IS_CHAR((n)))
#define IS_ARG_STR(n)	(IS_LV_STR((n)) || IS_STR((n)))
#define IS_ARG_WVUM(n)	(IS_LV_WNUM((n)) || IS_WNUM((n)))
#define IS_ARG_LIST(n)	(IS_LV_LIST((n)) || IS_LIST((n)))
#define IS_ARG_ARRAY(n)	(IS_LV_ARRAY((n)) || IS_ARRAY((n)))
#define IS_ARG_AFFECT(n)	(IS_LV_AFFECT((n)) || IS_AFFECT((n)))
#define IS_ARG_AREA(n)	(IS_LV_AREA((n)) || IS_AREA((n)))
#define IS_ARG_EXIT(n)	(IS_LV_EXIT((n)) || IS_EXIT((n)))
#define IS_ARG_MOB(n)	(IS_LV_MOB((n)) || IS_MOB((n)))
#define IS_ARG_OBJ(n)	(IS_LV_OBJ((n)) || IS_OBJ((n)))
#define IS_ARG_ROOM(n)	(IS_LV_ROOM((n)) || IS_ROOM((n)))
#define IS_ARG_SECTOR(n)	(IS_LV_SECTOR((n)) || IS_SECTOR((n)))
#define IS_ARG_TOKEN(n)	(IS_LV_TOKEN((n)) || IS_TOKEN((n)))

#define IS_THIS_NUM		(IS_ARG_NUM(0))
#define IS_THIS_FLT		(IS_ARG_FLT(0))
#define IS_THIS_BLN		(IS_ARG_BLN(0))
#define IS_THIS_BIT		(IS_ARG_BIT(0))
#define IS_THIS_CHR		(IS_ARG_CHR(0))
#define IS_THIS_STR		(IS_ARG_STR(0))
#define IS_THIS_WVUM	(IS_ARG_WVUM(0))
#define IS_THIS_LIST	(IS_ARG_LIST(0))
#define IS_THIS_ARRAY	(IS_ARG_ARRAY(0))
#define IS_THIS_AFFECT	(IS_ARG_AFFECT(0))
#define IS_THIS_AREA	(IS_ARG_AREA(0))
#define IS_THIS_EXIT	(IS_ARG_EXIT(0))
#define IS_THIS_MOB		(IS_ARG_MOB(0))
#define IS_THIS_OBJ		(IS_ARG_OBJ(0))
#define IS_THIS_ROOM	(IS_ARG_ROOM(0))
#define IS_THIS_SECTOR	(IS_ARG_SECTOR(0))
#define IS_THIS_TOKEN	(IS_ARG_TOKEN(0))

#define GET_NUM(n)		(IS_LV_NUM32((n)) ? LV_NUM32((n)) : (IS_LV_NUM((n)) ? LV_NUM((n)) : (IS_NUM((n)) ? ARG_NUM((n)) : 0)))
#define GET_FLT(n)		(IS_LV_FLOAT((n)) ? LV_FLT((n)) : ARG_FLT((n)))
#define GET_BLN(n)		(IS_LV_BOOL((n)) ? LV_BLN((n)) : ARG_BLN((n)))
#define GET__BIT(n)		(LV__BIT((n)))			// ALWAYS lvalue
#define GET_CHR(n)		(IS_LV_CHAR((n)) ? LV_CHR((n)) : ARG_CHR((n)))
#define GET_STR(n)		(IS_LV_STR((n)) ? LV_STR((n)) : ARG_STR((n)))
#define GET_WVUM(n)		(IS_LV_WNUM((n)) ? LV_WNUM((n)) : ARG_WNUM((n)))
#define GET_AFFECT(n)	(IS_LV_AFFECT((n)) ? LV_AFFECT((n)) : ARG_AFFECT((n)))
#define GET_AREA(n)		(IS_LV_AREA((n)) ? LV_AREA((n)) : ARG_AREA((n)))
#define GET_EXIT(n)		(IS_LV_EXIT((n)) ? LV_EXIT((n)) : ARG_EXIT((n)))
#define GET_MOB(n)		(IS_LV_MOB((n)) ? LV_MOB((n)) : ARG_MOB((n)))
#define GET_OBJ(n)		(IS_LV_OBJ((n)) ? LV_OBJ((n)) : ARG_OBJ((n)))
#define GET_ROOM(n)		(IS_LV_ROOM((n)) ? LV_ROOM((n)) : ARG_ROOM((n)))
#define GET_SECTOR(n)	(IS_LV_SECTOR((n)) ? LV_SECTOR((n)) : ARG_SECTOR((n)))
#define GET_TOKEN(n)	(IS_LV_TOKEN((n)) ? LV_TOKEN((n)) : ARG_TOK((n)))
//#define GET__LIST(n)	(IS_LV_LIST((n)) ? LV__LIST((n)) : ARG__LIST((n)))
#define GET_LIST(n)		(IS_LV_LIST((n)) ? LV_LIST((n)) : ARG_LIST((n)))
//#define GET__ARRAY(n)	(IS_LV_ARRAY((n)) ? LV__ARRAY((n)) : ARG__ARRAY((n)))
#define GET_ARRAY(n)	(IS_LV_ARRAY((n)) ? LV_ARRAY((n)) : ARG_ARRAY((n)))

#define THIS_NUM		(GET_NUM(0))
#define THIS_FLT		(GET_FLT(0))
#define THIS_BLN		(GET_BLN(0))
#define THIS__BIT		(GET__BIT(0))
#define THIS_CHR		(GET_CHR(0))
#define THIS_STR		(GET_STR(0))
#define THIS_WVUM		(GET_WVUM(0))
#define THIS_AFFECT		(GET_AFFECT(0))
#define THIS_AREA		(GET_AREA(0))
#define THIS_EXIT		(GET_EXIT(0))
#define THIS_MOB		(GET_MOB(0))
#define THIS_OBJ		(GET_OBJ(0))
#define THIS_ROOM		(GET_ROOM(0))
#define THIS_TOKEN		(GET_TOKEN(0))
//#define THIS__LIST		(GET__LIST(0))
#define THIS_LIST		(GET_LIST(0))
//#define THIS__ARRAY		(GET__ARRAY(0))
#define THIS_ARRAY		(GET_ARRAY(0))

#define SET_NULL		(output->type = NST_NULL)
#define SET_NUM(n)		(output->type = NST_NUMBER, output->_.i = (n))
#define SET_FLT(f)		(output->type = NST_FLOAT, output->_.d = (f))
#define SET_BOOL(v)		(output->type = NST_BOOLEAN, output->_.b = (v))
#define SET_STRS(s)		(output->type = NST_STRING_S, output->_.str = (s))
#define SET_WNUM(w)		(output->type = NST_WIDEVNUM, output->_.wnum = (w))
#define SET_MOB(m)		(output->type = NST_MOBILE, output->_.mobile = (m))
#define SET_OBJ(o)		(output->type = NST_OBJECT, output->_.object = (o))
#define SET_ROOM(r)		(output->type = NST_ROOM, output->_.room = (r))
#define SET_EXIT(x)		(output->type = NST_EXIT, output->_.ex = (x))
#define SET_LV_EXIT(x)	\
	do { \
		output->type = NST_LVALUE; \
		output->_.lvalue.type = NST_EXIT; \
		output->_.lvalue._.ex = x; \
	} while(false)


/////////////////////////////////////
// Functions

// print(string);
DECL_METHOD_FUNC(function_print_msg)
{
	if (!IS_ARG_STR(0)) return SCPERR_STACK;
	// Print the message
	char *str = GET_STR(0);
	printf("%s\n", str ? str : "<null>");

	return SCPERR_SUCCESS;
}

// random_percent()
//  - same as 100.random()
DECL_METHOD_FUNC(function_random_percent)
{
	long value = number_range(0,99);
	SET_NUM(value);
	return SCPERR_SUCCESS;
}

DECL_METHOD_FUNC(function_reckoning)
{
	return SCPERR_SUCCESS;
}

// NUMBER methods

// number.random()
DECL_METHOD_FUNC(number_random_value)
{
	if (!IS_THIS_NUM) return SCPERR_STACK;

	// Get a number from 0 to N-1
	long value = number_range(0, THIS_NUM - 1);

	SET_NUM(value);
	return SCPERR_SUCCESS;
}

// FLOAT methods

// BOOLEAN methods

// STRING methods

// field: int string.length
DECL_METHOD_FUNC(string_length)
{
	if (!IS_THIS_STR) return SCPERR_STACK;

	// Get length of string
	long len = utf8_strlen(THIS_STR);

	SET_NUM(len);
	return SCPERR_SUCCESS;
}

// FLAG methods

// LIST methods
DECL_METHOD_FUNC(list_add)
{
	return SCPERR_SUCCESS;
}

DECL_METHOD_FUNC(list_insert)
{
	return SCPERR_SUCCESS;
}

DECL_METHOD_FUNC(list_remove)
{
	return SCPERR_SUCCESS;
}

// field: int list.size
DECL_METHOD_FUNC(list_size)
{
	if (!IS_THIS_LIST) return SCPERR_STACK;

	long size = list_size(THIS_LIST);

	SET_NUM(size);

	return SCPERR_SUCCESS;
}

// ARRAY methods

// field: int array.length
DECL_METHOD_FUNC(array_length)
{
	if (!IS_THIS_ARRAY) return SCPERR_STACK;

	long size = 0;
	if (IS_ARRAY(0))
		size = ARG__ARRAY(0).length;
	else
		size = LV__ARRAY(0).length;

	SET_NUM(size);

	return SCPERR_SUCCESS;
}

// STAT methods

// MAP methods

// WIDEVNUM methods

// AFFECT Methods
DECL_METHOD_FUNC(affect_is_permanent)
{
	if (!IS_THIS_AFFECT) return SCPERR_STACK;

	AFFECT_DATA *aff = THIS_AFFECT;

	if (IS_VALID(aff) && aff->duration < 0)
	{
		SET_BOOL(true);
	}
	else
	{
		SET_BOOL(false);
	}

	return SCPERR_SUCCESS;
}

// AREA Methods

// room area.get_room(long vnum)
DECL_METHOD_FUNC(area_get_room)
{
	if (!IS_THIS_AREA) return SCPERR_STACK;
	if (!IS_ARG_NUM(1)) return SCPERR_STACK;

	AREA_DATA *area = THIS_AREA;
	long vnum = GET_NUM(1);

	if (area)
	{
		int h = vnum % MAX_KEY_HASH;
		ROOM_INDEX_DATA *room;

		for(room = area->room_index_hash[h]; room; room = room->next)
		{
			if (room->vnum == vnum)
				break;
		}

		SET_ROOM(room);
	}
	else
	{
		SET_ROOM(NULL);
	}

	return SCPERR_SUCCESS;
}

// DUNGEON methods

// EXIT methods

// field: int exit.door
DECL_METHOD_FUNC(exit_get_door)
{
	if (!IS_THIS_EXIT) return SCPERR_STACK;

	EXIT_DATA *ex = THIS_EXIT;
	long door = ex ? ex->orig_door : -1;

	SET_NUM(door);

	return SCPERR_SUCCESS;
}

// field: string exit.dir
// field: string exit.direction
DECL_METHOD_FUNC(exit_get_direction)
{
	if (!IS_THIS_EXIT) return SCPERR_STACK;

	EXIT_DATA *ex = THIS_EXIT;

	// fprintf(stderr, "exit_get_direction(%ld)\n", ex ? ex->orig_door : -1);

	if (ex && ex->orig_door >= 0 && ex->orig_door < MAX_DIR)
	{
		SET_STRS(dir_name[ex->orig_door]);
	}
	else
	{
		SET_STRS(NULL);
	}

	return SCPERR_SUCCESS;
}

// field: exit exit.mate
DECL_METHOD_FUNC(exit_get_mate)
{
	if (!IS_THIS_EXIT) return SCPERR_STACK;

	EXIT_DATA *ex = THIS_EXIT;

	if (ex && ex->u1.to_room)
	{
		int rev = rev_dir[ex->orig_door];
		
		SET_EXIT(ex->u1.to_room->exit[rev]);
	}
	else
		SET_EXIT(NULL);

	return SCPERR_SUCCESS;
}

// field: bool exit.is_oneway
DECL_METHOD_FUNC(exit_is_oneway)
{
	if (!IS_THIS_EXIT) return SCPERR_STACK;

	EXIT_DATA *ex = THIS_EXIT;

	bool oneway = false;
	if (ex && ex->u1.to_room)
	{
		int rev = rev_dir[ex->orig_door];

		EXIT_DATA *mate = ex->u1.to_room->exit[rev];
		oneway = !mate || !mate->u1.to_room || (ex->from_room != mate->u1.to_room);
	}

	SET_BOOL(oneway);
	return SCPERR_SUCCESS;
}

// field: bool exit.is_twoway
DECL_METHOD_FUNC(exit_is_twoway)
{
	if (!IS_THIS_EXIT) return SCPERR_STACK;

	EXIT_DATA *ex = THIS_EXIT;

	bool twoway = false;
	if (ex && ex->u1.to_room)
	{
		int rev = rev_dir[ex->orig_door];

		EXIT_DATA *mate = ex->u1.to_room->exit[rev];
		if (mate && mate->u1.to_room)
		{
			twoway = ex->from_room == mate->u1.to_room;
		}
	}

	SET_BOOL(twoway);
	return SCPERR_SUCCESS;
}

static inline EXIT_DATA **__exit_get_exit(EXIT_DATA *ex, int door)
{
	if (ex && ex->u1.to_room)
		return &ex->u1.to_room->exit[door];
	else
		return NULL;
}

// field: exit& exit.north
DECL_METHOD_FUNC(exit_get_north)
{
	if (!IS_THIS_EXIT) return SCPERR_STACK;

	EXIT_DATA *ex = THIS_EXIT;

	SET_LV_EXIT(__exit_get_exit(ex,DIR_NORTH));

	return SCPERR_SUCCESS;
}

// field: exit& exit.northeast
DECL_METHOD_FUNC(exit_get_northeast)
{
	if (!IS_THIS_EXIT) return SCPERR_STACK;

	EXIT_DATA *ex = THIS_EXIT;

	SET_LV_EXIT(__exit_get_exit(ex,DIR_NORTHEAST));

	return SCPERR_SUCCESS;
}

// field: exit& exit.east
DECL_METHOD_FUNC(exit_get_east)
{
	if (!IS_THIS_EXIT) return SCPERR_STACK;

	EXIT_DATA *ex = THIS_EXIT;

	SET_LV_EXIT(__exit_get_exit(ex,DIR_EAST));

	return SCPERR_SUCCESS;
}

// field: exit& exit.southeast
DECL_METHOD_FUNC(exit_get_southeast)
{
	if (!IS_THIS_EXIT) return SCPERR_STACK;

	EXIT_DATA *ex = THIS_EXIT;

	SET_LV_EXIT(__exit_get_exit(ex,DIR_SOUTHEAST));

	return SCPERR_SUCCESS;
}

// field: exit& exit.south
DECL_METHOD_FUNC(exit_get_south)
{
	if (!IS_THIS_EXIT) return SCPERR_STACK;

	EXIT_DATA *ex = THIS_EXIT;

	SET_LV_EXIT(__exit_get_exit(ex,DIR_SOUTH));

	return SCPERR_SUCCESS;
}

// field: exit& exit.southwest
DECL_METHOD_FUNC(exit_get_southwest)
{
	if (!IS_THIS_EXIT) return SCPERR_STACK;

	EXIT_DATA *ex = THIS_EXIT;

	SET_LV_EXIT(__exit_get_exit(ex,DIR_SOUTHWEST));

	return SCPERR_SUCCESS;
}

// field: exit& exit.west
DECL_METHOD_FUNC(exit_get_west)
{
	if (!IS_THIS_EXIT) return SCPERR_STACK;

	EXIT_DATA *ex = THIS_EXIT;

	SET_LV_EXIT(__exit_get_exit(ex,DIR_WEST));

	return SCPERR_SUCCESS;
}

// field: exit& exit.northwest
DECL_METHOD_FUNC(exit_get_northwest)
{
	if (!IS_THIS_EXIT) return SCPERR_STACK;

	EXIT_DATA *ex = THIS_EXIT;

	SET_LV_EXIT(__exit_get_exit(ex,DIR_NORTHWEST));

	return SCPERR_SUCCESS;
}

// field: exit& exit.up
DECL_METHOD_FUNC(exit_get_up)
{
	if (!IS_THIS_EXIT) return SCPERR_STACK;

	EXIT_DATA *ex = THIS_EXIT;

	SET_LV_EXIT(__exit_get_exit(ex,DIR_UP));

	return SCPERR_SUCCESS;
}

// field: exit& exit.down
DECL_METHOD_FUNC(exit_get_down)
{
	if (!IS_THIS_EXIT) return SCPERR_STACK;

	EXIT_DATA *ex = THIS_EXIT;

	SET_LV_EXIT(__exit_get_exit(ex,DIR_DOWN));

	return SCPERR_SUCCESS;
}


// INSTANCE methods

// MOBILE methods

// field: widevnum mobile.wnum
// field: widevnum mobile.widevnum
DECL_METHOD_FUNC(mobile_get_widevnum)
{
	if (!IS_THIS_MOB) return SCPERR_STACK;

	CHAR_DATA *mob = THIS_MOB;

	WNUM wnum;
	if (mob && mob->pIndexData)
	{
		wnum.pArea = mob->pIndexData->area;
		wnum.vnum = mob->pIndexData->vnum;
	}
	else
	{
		wnum.pArea = NULL;
		wnum.vnum = 0;
	}

	SET_WNUM(wnum);
	return SCPERR_SUCCESS;
}

// field: bool mobile.is_pc
DECL_METHOD_FUNC(mobile_is_pc)
{
	if (!IS_THIS_MOB) return SCPERR_STACK;

	CHAR_DATA *mob = THIS_MOB;

	if (IS_NPC(mob))
		SET_BOOL(false);
	else
		SET_BOOL(true);

	return SCPERR_SUCCESS;
}

// OBJECT methods

// QUEST methods

// ROOM methods

// array(exit) room.exits
DECL_METHOD_FUNC(room_get_exits)
{
	if (!IS_THIS_ROOM) return SCPERR_STACK;

	ROOM_INDEX_DATA *room = THIS_ROOM;

	// fprintf(stderr, "room_get_exits(%ld)\n", room ? room->vnum : 0);

	output->type = NST_ARRAY_S;
	output->_.array.type = NST_EXIT;
	output->_.array.size = sizeof(EXIT_DATA *);
	output->_.array.length = MAX_DIR;
	if (room)
		output->_.array.ptr = room->exit;
	else
		output->_.array.ptr = NULL;

	return SCPERR_SUCCESS;
}

DECL_METHOD_FUNC(room_set_sector)
{
	if (!IS_THIS_ROOM) return SCPERR_STACK;
	if (!IS_ARG_SECTOR(1)) return SCPERR_STACK;

	ROOM_INDEX_DATA *room = THIS_ROOM;
	SECTOR_DATA *sector = GET_SECTOR(1);

	if (!room || !sector)
	{
		SET_BOOL(false);
	}
	else
	{
		room->sector = sector;
		room->sector_flags = sector->flags;
		SET_BOOL(true);
	}

	return SCPERR_SUCCESS;
}

DECL_METHOD_FUNC(room_reset)
{
	if (!IS_THIS_ROOM) return SCPERR_STACK;

	ROOM_INDEX_DATA *room = THIS_ROOM;

	if (room)
	{
		room->sector = room->rs_sector;
		room->sector_flags = room->rs_sector ? room->rs_sector->flags : 0;

		SET_BOOL(true);
	}
	else
	{
		SET_BOOL(false);
	}

	return SCPERR_SUCCESS;
}

// SHIP methods

// TOKEN methods
DECL_METHOD_FUNC(token_owner_type)
{
	if (!IS_THIS_TOKEN) return SCPERR_STACK;

	TOKEN_DATA *token = THIS_TOKEN;

	output->type = NST_STAT;
	output->_.stat.table = token_owner_types;
	if (token->player)
		output->_.stat.number = TOKEN_OWNER_MOB;
	else if (token->object)
		output->_.stat.number = TOKEN_OWNER_OBJ;
	else if (token->room)
		output->_.stat.number = TOKEN_OWNER_ROOM;
	else
		output->_.stat.number = TOKEN_OWNER_NONE;

	return SCPERR_SUCCESS;
}

DECL_METHOD_FUNC(token_get_index_value)
{
	if (!IS_THIS_TOKEN) return SCPERR_STACK;

	TOKEN_DATA *token = THIS_TOKEN;

	output->type = NST_ARRAY_S;
	output->_.array.type = NST_NUMBER;
	output->_.array.size = sizeof(long);
	output->_.array.length = MAX_TOKEN_VALUES;
	if (token && token->pIndexData)
		output->_.array.ptr = token->pIndexData->value;
	else
		output->_.array.ptr = NULL;

	return SCPERR_SUCCESS;
}
