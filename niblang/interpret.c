#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <stdint.h>
#include <ctype.h>

#include "../merc.h"
#include "../wilds.h"
#include "niblang.h"
#include "script.h"
#include "interpret.h"

char *get_affect_name(AFFECT_DATA *paf);
bool affect_equal(AFFECT_DATA *a, AFFECT_DATA *b);
extern char * const dir_name[];

//#define IS_NULLSTR(s)	(((s) == NULL) || ((s)[0] == '\0'))
#define CLS				printf("\033[2J")
#define SETPOS(r,c)		printf("\033[%d;%dH", (r), (c))
#define INVCLR			printf("\033[7m")
#define RSTCLR			printf("\033[0m")
#define MAX_STACK_SHOW 10
#define SIDE_PANEL_WIDTH 50
#define SETRET(n,r)			(n)->last_return = SCPERR_##r
#define SETRETN(n,r)		(n)->last_return = (r)

NIB_SCRIPT_STACK_TYPE convert_to_stype(NIB_TYPE *type)
{
	if (!type) return NST_VOID;

	if (type == nibtype_null) return NST_NULL;

	switch(type->type_class)
	{
		case NTC_PRIMARY:
			switch(type->_.primary)
			{
				case NT_NUMBER:		return NST_NUMBER;
				case NT_FLOAT:		return NST_FLOAT;
				case NT_BOOLEAN:	return NST_BOOLEAN;
				case NT_CHAR:		return NST_CHAR;
				case NT_STRING:		return NST_STRING;
				case NT_MAP:		return NST_MAP;
				case NT_WIDEVNUM:	return NST_WIDEVNUM;

				case NT_ACCOUNT:	return NST_ACCOUNT;
				case NT_AFFECT:		return NST_AFFECT;
				case NT_AREA:		return NST_AREA;
				case NT_CHANNEL:	return NST_CHANNEL;
				case NT_CLASS:		return NST_CLASS;
				case NT_DUNGEON:	return NST_DUNGEON;
				case NT_EXIT:		return NST_EXIT;
				case NT_INSTANCE:	return NST_INSTANCE;
				case NT_LIQUID:		return NST_LIQUID;
				case NT_MAIL:		return NST_MAIL;
				case NT_MATERIAL:	return NST_MATERIAL;
				case NT_MISSION:	return NST_MISSION;
				case NT_MOBILE:		return NST_MOBILE;
				case NT_NOTE:		return NST_NOTE;
				case NT_OBJECT:		return NST_OBJECT;
				case NT_ORG:		return NST_ORG;
				case NT_QUEST:		return NST_QUEST;
				case NT_RACE:		return NST_RACE;
				case NT_RANK:		return NST_RANK;
				case NT_REPUTATION:	return NST_REPUTATION;
				case NT_ROOM:		return NST_ROOM;
				case NT_SHIP:		return NST_SHIP;
				case NT_SKILL:		return NST_SKILL;
				case NT_TOKEN:		return NST_TOKEN;
				case NT_WILDS:		return NST_WILDS;
				case NT_WORLD:		return NST_WORLD;
			}
			break;
		case NTC_FLAG:		return NST_FLAG;
		case NTC_STAT:		return NST_STAT;
		case NTC_LIST:		return NST_LIST;
		case NTC_VOID:		return NST_VOID;
	}
	return NST_UNKNOWN;
}

static const char *nst_to_type(NIB_SCRIPT_STACK_TYPE type)
{
	switch(type)
	{
		case NST_BOOLEAN:	return "boolean";
		case NST_NUMBER:	return "number";
		case NST_FLOAT:		return "float";
		case NST_STRING:	return "string";
		case NST_CHAR:		return "char";
		case NST_MAP:		return "map";
		case NST_WIDEVNUM:	return "widevnum";
		case NST_FLAG:		return "flag";
		case NST_STAT:		return "stat";
		case NST_LIST:		return "list";
		case NST_ACCOUNT:	return "account";
		case NST_AFFECT:	return "affect";
		case NST_AREA:		return "area";
		case NST_CHANNEL:	return "channel";
		case NST_CLASS:		return "class";
		case NST_DUNGEON:	return "dungeon";
		case NST_EXIT:		return "exit";
		case NST_INSTANCE:	return "instance";
		case NST_LIQUID:	return "liquid";
		case NST_MAIL:		return "mail";
		case NST_MATERIAL:	return "material";
		case NST_MISSION:	return "mission";
		case NST_MOBILE:	return "mobile";
		case NST_NOTE:		return "note";
		case NST_OBJECT:	return "object";
		case NST_ORG:		return "org";
		case NST_QUEST:		return "quest";
		case NST_RACE:		return "race";
		case NST_RANK:		return "rank";
		case NST_REPUTATION:return "reputation";
		case NST_ROOM:		return "room";
		case NST_SHIP:		return "ship";
		case NST_SKILL:		return "skill";
		case NST_TOKEN:		return "token";
		case NST_WILDS:		return "wilds";
		case NST_WORLD:		return "world";
	}

	return "invalid";
}

static void __delete_integer(void *data)
{
	if (data) nib_free(data);
}

static void *__copy_integer(void *src)
{
	if (!src) return NULL;
	long *data = nib_malloc(sizeof(long));

	*data = *((long *)src);

	return data;
}

static LLIST *new_integer_list()
{
	return list_createx(false,__copy_integer,__delete_integer);
}

static void __delete_float(void *data)
{
	if (data) nib_free(data);
}

static void *__copy_float(void *src)
{
	if (!src) return NULL;
	double *data = nib_malloc(sizeof(double));

	*data = *((double *)src);

	return data;
}

static LLIST *new_float_list()
{
	return list_createx(false,__copy_float,__delete_float);
}

static void __delete_char(void *data)
{
	if (data) nib_free(data);
}

static void *__copy_char(void *src)
{
	if (!src) return NULL;
	char *data = nib_malloc(sizeof(char));

	*data = *((char *)src);

	return data;
}

static LLIST *new_char_list()
{
	return list_createx(false,__copy_char,__delete_char);
}

static void __delete_boolean(void *data)
{
	if (data) nib_free(data);
}

static void *__copy_boolean(void *src)
{
	if (!src) return NULL;
	bool *data = nib_malloc(sizeof(bool));

	*data = *((bool *)src);

	return data;
}

static LLIST *new_boolean_list()
{
	return list_createx(false,__copy_boolean,__delete_boolean);
}

static void __delete_widevnum(void *data)
{
	if (data) nib_free(data);
}

static void *__copy_widevnum(void *src)
{
	if (!src) return NULL;
	WNUM *data = nib_malloc(sizeof(WNUM));

	*data = *((WNUM *)src);

	return data;
}

static LLIST *new_widevnum_list()
{
	return list_createx(false,__copy_widevnum,__delete_widevnum);
}

static void __delete_string(void *data)
{
	if (data) nib_free(data);
}

static void *__copy_string(void *data)
{
	if (!data) return NULL;

	return nib_strdup((char *)data);
}

static LLIST *new_string_list()
{
	return list_createx(false,__copy_string,__delete_string);
}


static void free_stack_item(NIB_SCRIPT_STACK *stack)
{
	switch(stack->type)
	{
		case NST_STRING:
			free(stack->_.str);
			break;

		case NST_LIST:
			list_destroy(stack->_.list.list);
			break;

		case NST_ITERATOR:
			iterator_stop(&stack->_.iter.it);	// Make sure it is stopped
			list_destroy(stack->_.iter.list);
			break;
	}

}

static NIB_SCRIPT_RUNTIME *new_script_runtime(NIB_SCRIPT *script)
{
	NIB_SCRIPT_RUNTIME *nsr = calloc(1, sizeof(NIB_SCRIPT_RUNTIME));

	if (nsr)
	{
		nsr->script = script;

		// Redundant, just want to be explicit
		nsr->pc = 0;
		nsr->sp = 0;
		nsr->last_return = SCPERR_SUCCESS;

		nsr->n_locals = script->n_locals;
		nsr->locals = calloc(nsr->n_locals, sizeof(NIB_LOCAL_RUNTIME_VAR));

		for(int i = nsr->n_locals; i-- > 0;)
		{
			nsr->locals[i].name = script->locals[i].name;
			nsr->locals[i].type = script->locals[i].stype;
			nsr->locals[i].constant = script->locals[i].constant;

			// printf("RT Local: %s => %d (%s)",
			// 	nsr->locals[i].name,
			// 	nsr->locals[i].type,
			// 	nst_to_type(nsr->locals[i].type));

			switch(nsr->locals[i].type)
			{
			case NST_FLAG:
				nsr->locals[i]._.stat.table = script->locals[i].type->_.flag.table;
				break;

			case NST_STAT:
				nsr->locals[i]._.stat.table = script->locals[i].type->_.stat.table;
				break;

			case NST_LIST:
				// Need to store the list's subtype
				nsr->locals[i]._.list.type = convert_to_stype(script->locals[i].type->_.type);
				break;
			}
		}

		nsr->last_return = 0;
	}

	return nsr;
}

static void free_local_value(NIB_LOCAL_RUNTIME_VAR *local)
{
	if (local->type == NST_STRING)
	{
		if (local->_.str) free(local->_.str);
	}
	else if (local->type == NST_LIST)
	{
		list_destroy(local->_.list.list);
	}
}

static void free_script_runtime(NIB_SCRIPT_RUNTIME *nsr)
{
	if (nsr)
	{
		if (nsr->locals)
		{
			for(int i = nsr->n_locals; i-- > 0;)
			{
				free_local_value(&nsr->locals[i]);
			}
		}

		// Clear up the stack, regardless
		for(int sp = 0; sp < nsr->sp; sp++)
			free_stack_item(&nsr->stack[sp]);

		list_destroy(nsr->disassembly);

		free(nsr);
	}
}

#define CHECK_STACK	\
	if (nsr->sp >= MAX_STACK) return false

bool nib_dup_stack(NIB_SCRIPT_RUNTIME *nsr)
{
	CHECK_STACK;

	NIB_SCRIPT_STACK *top = &nsr->stack[nsr->sp - 1];

	if (top->type == NST_ITERATOR) return false;	// No duplication of stack iterators

	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp++];

	*stack = *top;	// Copy everything
	switch(top->type)
	{
	case NST_STRING:
		stack->_.str = strdup(top->_.str);
		break;
	
	case NST_LIST:
		stack->_.list.list = list_copy(top->_.list.list);
		break;
	}

	return true;
}

#define __push(t,d,f,n) \
bool nib_push_stack_##n (NIB_SCRIPT_RUNTIME *nsr, t value) \
{ \
	CHECK_STACK; \
\
	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp++]; \
\
	stack->type = NST_##d; \
	stack->_.f = value; \
\
	return true; \
}

__push(long,NUMBER,i,number)
__push(double,FLOAT,d,float)
__push(bool,BOOLEAN,b,boolean)
__push(utf8char_t,CHAR,ch,char)
__push(char *,STRING_S,str,string_shared)

static WNUM __wnum_zero;
bool nib_push_stack_widevnum (NIB_SCRIPT_RUNTIME *nsr, WNUM *value)
{
	CHECK_STACK;

	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp++];

	stack->type = NST_WIDEVNUM;
	if (value)
		stack->_.wnum = *value;
	else
		stack->_.wnum = __wnum_zero;

	return true;
}
bool nib_push_stack_flag (NIB_SCRIPT_RUNTIME *nsr, long value, const struct flag_type *table)
{
	CHECK_STACK;

	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp++];

	stack->type = NST_FLAG;
	stack->_.stat.number = value;
	stack->_.stat.table = table;

	return true;
}
bool nib_push_stack_stat (NIB_SCRIPT_RUNTIME *nsr, long value, const struct flag_type *table)
{
	CHECK_STACK;

	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp++];

	stack->type = NST_STAT;
	stack->_.stat.number = value;
	stack->_.stat.table = table;

	return true;
}
__push(ACCOUNT_DATA *,ACCOUNT,account,account)
__push(AFFECT_DATA *,AFFECT,affect,affect)
__push(AREA_DATA *,AREA,area,area)
//__push(CHANNEL_DATA *,CHANNEL,channel,channel)
__push(CLASS_DATA *,CLASS,clazz,class)
__push(DUNGEON *,DUNGEON,dungeon,dungeon)
__push(EXIT_DATA *,EXIT,ex,exit)
__push(INSTANCE *,INSTANCE,instance,instance)
__push(LIQUID *,LIQUID,liquid,liquid)
__push(MAIL_DATA *,MAIL,mail,mail)
__push(MATERIAL *,MATERIAL,material,material)
__push(MISSION_DATA *,MISSION,mission,mission)
__push(CHAR_DATA *,MOBILE,mobile,mobile)
__push(NOTE_DATA *,NOTE,note,note)
__push(OBJ_DATA *,OBJECT,object,object)
__push(CHURCH_DATA *,ORG,org,org)
// __push(QUEST_DATA *,QUEST,quest,quest)
__push(RACE_DATA *,RACE,race,race)
__push(REPUTATION_INDEX_RANK_DATA *,RANK,rank,rank)
__push(REPUTATION_DATA *,REPUTATION,reputation,reputation)
__push(ROOM_INDEX_DATA *,ROOM,room,room)
__push(SHIP_DATA *,SHIP,ship,ship)
__push(SKILL_DATA *,SKILL,skill,skill)
__push(TOKEN_DATA *,TOKEN,token,token)
__push(WILDS_DATA *,WILDS,wilds,wilds)
//__push(WORLD_DATA *,WORLD,world,world)

bool nib_push_stack_list (NIB_SCRIPT_RUNTIME *nsr, LLIST *value, NIB_SCRIPT_STACK_TYPE type)
{
	CHECK_STACK;

	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp++];

	stack->type = NST_LIST;
	stack->_.list.type = type;
	stack->_.list.list = value;

	return true;
}

bool nib_push_stack_list_shared (NIB_SCRIPT_RUNTIME *nsr, LLIST *value, NIB_SCRIPT_STACK_TYPE type)
{
	CHECK_STACK;

	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp++];

	stack->type = NST_LIST_S;
	stack->_.list.type = type;
	stack->_.list.list = value;

	return true;
}

bool nib_push_stack_list_raw (NIB_SCRIPT_RUNTIME *nsr, LLIST *value, NIB_SCRIPT_STACK_TYPE type)
{
	CHECK_STACK;

	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp++];

	stack->type = NST_LIST;
	stack->_.list.type = type;
	stack->_.list.list = value;

	return true;
}

bool nib_push_stack_iterator (NIB_SCRIPT_RUNTIME *nsr, LLIST *list, NIB_SCRIPT_STACK_TYPE type)
{
	CHECK_STACK;

	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp++];

	stack->type = NST_ITERATOR;
	stack->_.iter.type = type;
	stack->_.iter.list = list;
	iterator_start(&stack->_.iter.it, stack->_.iter.list);

	return true;
}

bool nib_push_stack_null (NIB_SCRIPT_RUNTIME *nsr)
{
	CHECK_STACK;

	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp++];

	stack->type = NST_NULL;

	return true;
}


// This needs to be freed when popped
bool nib_push_stack_string(NIB_SCRIPT_RUNTIME *nsr, const char *value)
{
	CHECK_STACK;

	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp++];

	stack->type = NST_STRING;
	stack->_.str = strdup(value);

	return true;
}

// Used when the string is allocated prior to pushing onto the stack
bool nib_push_stack_string_raw(NIB_SCRIPT_RUNTIME *nsr, char *value)
{
	CHECK_STACK;

	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp++];

	stack->type = NST_STRING;
	stack->_.str = value;

	return true;
}


bool nib_push_stack_lvalue(NIB_SCRIPT_RUNTIME *nsr, NIB_SCRIPT_LVALUE *value)
{
	CHECK_STACK;

	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp++];

	stack->type = NST_LVALUE;
	stack->_.lvalue = *value;

	return true;
}

bool nib_push_stack_lvalue_value(NIB_SCRIPT_RUNTIME *nsr, NIB_SCRIPT_LVALUE *lvalue)
{
#define __lv(t,f,n) \
	case NST_##t:\
		return nib_push_stack_##n (nsr, *(lvalue->_.f) );

	switch(lvalue->type)
	{
	__lv(NUMBER,number,number)
	__lv(FLOAT,d,float)
	__lv(BOOLEAN,b,boolean)
	__lv(CHAR,ch,char)
	__lv(STRING,str,string_shared)
	// __lv(MAP,map,map)
	case NST_WIDEVNUM:
		return nib_push_stack_widevnum (nsr, lvalue->_.wnum );
	__lv(ACCOUNT,account,account)
	__lv(AFFECT,affect,affect)
	__lv(AREA,area,area)
	// __lv(CHANNEL,channel,channel)
	__lv(CLASS,clazz,class)
	__lv(DUNGEON,dungeon,dungeon)
	__lv(EXIT,ex,exit)
	__lv(INSTANCE,instance,instance)
	__lv(LIQUID,liquid,liquid)
	__lv(MAIL,mail,mail)
	__lv(MATERIAL,material,material)
	__lv(MISSION,mission,mission)
	__lv(MOBILE,mobile,mobile)
	__lv(NOTE,note,note)
	__lv(OBJECT,object,object)
	__lv(ORG,org,org)
	// __lv(QUEST,quest,quest)
	__lv(RACE,race,race)
	__lv(RANK,rank,rank)
	__lv(REPUTATION,reputation,reputation)
	__lv(ROOM,room,room)
	__lv(SHIP,ship,ship)
	__lv(SKILL,skill,skill)
	__lv(TOKEN,token,token)
	__lv(WILDS,wilds,wilds)
	// __lv(WORLD,world,world)

	case NST_FLAG:
		return nib_push_stack_flag(nsr, *(lvalue->_.stat.number),lvalue->_.stat.table);

	case NST_STAT:
		return nib_push_stack_stat(nsr, *(lvalue->_.stat.number),lvalue->_.stat.table);

	case NST_LIST:
		return nib_push_stack_list(nsr, *(lvalue->_.list.list), lvalue->_.list.type);
	}

	return false;
}


bool nib_push_stack_local_var(NIB_SCRIPT_RUNTIME *nsr, NIB_LOCAL_RUNTIME_VAR *var)
{
#define __lcl(t,f,n) \
	case NST_##t:\
		return nib_push_stack_##n (nsr, var->_.f );

	switch(var->type)
	{
	__lcl(NUMBER,i,number)
	__lcl(FLOAT,f,float)
	__lcl(BOOLEAN,b,boolean)
	__lcl(CHAR,ch,char)
	__lcl(STRING,str,string_shared)
	// __lcl(MAP,map,map)
	case NST_WIDEVNUM:
		return nib_push_stack_widevnum(nsr, &var->_.wnum);
	__lcl(ACCOUNT,account,account)
	__lcl(AFFECT,affect,affect)
	__lcl(AREA,area,area)
	// __lcl(CHANNEL,channel,channel)
	__lcl(CLASS,clazz,class)
	__lcl(DUNGEON,dungeon,dungeon)
	__lcl(EXIT,ex,exit)
	__lcl(INSTANCE,instance,instance)
	__lcl(LIQUID,liquid,liquid)
	__lcl(MAIL,mail,mail)
	__lcl(MATERIAL,material,material)
	__lcl(MISSION,mission,mission)
	__lcl(MOBILE,mobile,mobile)
	__lcl(NOTE,note,note)
	__lcl(OBJECT,object,object)
	__lcl(ORG,org,org)
	// __lcl(QUEST,quest,quest)
	__lcl(RACE,race,race)
	__lcl(RANK,rank,rank)
	__lcl(REPUTATION,reputation,reputation)
	__lcl(ROOM,room,room)
	__lcl(SHIP,ship,ship)
	__lcl(SKILL,skill,skill)
	__lcl(TOKEN,token,token)
	__lcl(WILDS,wilds,wilds)
	// __lcl(WORLD,world,world)

	case NST_FLAG:
		return nib_push_stack_flag(nsr, var->_.stat.number, var->_.stat.table);

	case NST_STAT:
		return nib_push_stack_stat(nsr, var->_.stat.number, var->_.stat.table);

	case NST_LIST:
		return nib_push_stack_list(nsr, var->_.list.list, var->_.list.type);
	}

	return false;
}

bool nib_push_stack_local_var_lvalue(NIB_SCRIPT_RUNTIME *nsr, NIB_LOCAL_RUNTIME_VAR *var)
{
#define __llv(t,f,l) \
	case NST_##t:\
		lvalue.type = NST_##t; \
		lvalue._.l = &(var->_.f); \
		break;

	NIB_SCRIPT_LVALUE lvalue;
	switch(var->type)
	{
	__llv(NUMBER,i,number)
	__llv(FLOAT,f,d)
	__llv(BOOLEAN,b,b)
	__llv(CHAR,ch,ch)
	__llv(STRING,str,str)
	// __llv(MAP,map,map)
	__llv(WIDEVNUM,wnum,wnum)
	__llv(ACCOUNT,account,account)
	__llv(AFFECT,affect,affect)
	__llv(AREA,area,area)
	// __llv(CHANNEL,channel,channel)
	__llv(CLASS,clazz,clazz)
	__llv(DUNGEON,dungeon,dungeon)
	__llv(EXIT,ex,ex)
	__llv(INSTANCE,instance,instance)
	__llv(LIQUID,liquid,liquid)
	__llv(MAIL,mail,mail)
	__llv(MATERIAL,material,material)
	__llv(MISSION,mission,mission)
	__llv(MOBILE,mobile,mobile)
	__llv(NOTE,note,note)
	__llv(OBJECT,object,object)
	__llv(ORG,org,org)
	// __llv(QUEST,quest,quest)
	__llv(RACE,race,race)
	__llv(RANK,rank,rank)
	__llv(REPUTATION,reputation,reputation)
	__llv(ROOM,room,room)
	__llv(SHIP,ship,ship)
	__llv(SKILL,skill,skill)
	__llv(TOKEN,token,token)
	__llv(WILDS,wilds,wilds)
	// __llv(WORLD,world,world)

	case NST_FLAG:
		lvalue.type = NST_FLAG;
		lvalue._.stat.number = &(var->_.stat.number);
		lvalue._.stat.table = var->_.stat.table;
		break;

	case NST_STAT:
		lvalue.type = NST_STAT;
		lvalue._.stat.number = &(var->_.stat.number);
		lvalue._.stat.table = var->_.stat.table;
		break;

	case NST_LIST:
		lvalue.type = NST_LIST;
		lvalue._.list.type = var->_.list.type;
		lvalue._.list.list = &(var->_.list.list);
		break;

	case NST_LIST_S:
		lvalue.type = NST_LIST_S;
		lvalue._.list.type = var->_.list.type;
		lvalue._.list.list = &(var->_.list.list);
		break;

	default:
		return false;
	}

	return nib_push_stack_lvalue(nsr, &lvalue);
}

bool nib_push_stack_global_var(NIB_SCRIPT_RUNTIME *nsr, pVARIABLE var)
{
#define __gbl(t,f,n) \
	case VAR_##t:\
		return nib_push_stack_##n (nsr, var->_.f);

	switch(var->type)
	{
	__gbl(NUMBER,num,number)
	__gbl(FLOAT,flt,float)
	__gbl(BOOLEAN,b,boolean)
	__gbl(CHAR,ch,char)
	case VAR_STRING_S:
	__gbl(STRING,str,string_shared)
	// __gbl(MAP,map,map)
	case VAR_WIDEVNUM:
		return nib_push_stack_widevnum(nsr, &var->_.wnum);
	__gbl(ACCOUNT,account,account)
	__gbl(AFFECT,affect,affect)
	__gbl(AREA,area,area)
	// __gbl(CHANNEL,channel,channel)
	__gbl(CLASS,clazz,class)
	__gbl(DUNGEON,dungeon,dungeon)
	__gbl(EXIT,ex,exit)
	__gbl(INSTANCE,instance,instance)
	__gbl(LIQUID,liquid,liquid)
	__gbl(MAIL,mail,mail)
	__gbl(MATERIAL,material,material)
	__gbl(MISSION,mission,mission)
	__gbl(MOBILE,mobile,mobile)
	__gbl(NOTE,note,note)
	__gbl(OBJECT,object,object)
	__gbl(ORG,org,org)
	// __gbl(QUEST,quest,quest)
	__gbl(RACE,race,race)
	__gbl(RANK,rank,rank)
	__gbl(REPUTATION,reputation,reputation)
	__gbl(ROOM,room,room)
	__gbl(SHIP,ship,ship)
	__gbl(SKILL,skill,skill)
	__gbl(TOKEN,token,token)
	__gbl(WILDS,wilds,wilds)
	}

	return false;
}

bool nib_push_stack_global_var_lvalue(NIB_SCRIPT_RUNTIME *nsr, pVARIABLE var)
{
#define __glv(t,f,l) \
	case VAR_##t: \
		lvalue.type = NST_##t; \
		lvalue._.l = &(var->_.f); \
		break;

	NIB_SCRIPT_LVALUE lvalue;
	switch(var->type)
	{
	__glv(NUMBER,num,number)
	__glv(FLOAT,flt,d)
	__glv(BOOLEAN,b,b)
	__glv(CHAR,ch,ch)
	case VAR_STRING_S:
	__glv(STRING,str,str)
	// __glv(MAP,map,map)
	case VAR_FLAG:
		lvalue.type = NST_FLAG;
		lvalue._.stat.number = &(var->_.stat.number);
		lvalue._.stat.table = var->_.stat.table;
		break;

	case VAR_STAT:
		lvalue.type = NST_STAT;
		lvalue._.stat.number = &(var->_.stat.number);
		lvalue._.stat.table = var->_.stat.table;
		break;

	__glv(WIDEVNUM,wnum,wnum)
	__glv(ACCOUNT,account,account)
	__glv(AFFECT,affect,affect)
	__glv(AREA,area,area)
	// __glv(CHANNEL,channel,channel)
	__glv(CLASS,clazz,clazz)
	__glv(DUNGEON,dungeon,dungeon)
	__glv(EXIT,ex,ex)
	__glv(INSTANCE,instance,instance)
	__glv(LIQUID,liquid,liquid)
	__glv(MAIL,mail,mail)
	__glv(MATERIAL,material,material)
	__glv(MISSION,mission,mission)
	__glv(MOBILE,mobile,mobile)
	__glv(NOTE,note,note)
	__glv(OBJECT,object,object)
	__glv(ORG,org,org)
	// __glv(QUEST,quest,quest)
	__glv(RACE,race,race)
	__glv(RANK,rank,rank)
	__glv(REPUTATION,reputation,reputation)
	__glv(ROOM,room,room)
	__glv(SHIP,ship,ship)
	__glv(SKILL,skill,skill)
	__glv(TOKEN,token,token)
	__glv(WILDS,wilds,wilds)

	default:
		return false;

	}

	return nib_push_stack_lvalue(nsr, &lvalue);
}

static void *__get_lvalue_field(NIB_SCRIPT_LVALUE *lvalue, NIB_FIELD *field)
{
#define __lfo(t,f) \
	case NST_##t:		return (void *)*(lvalue->_.f) + field->offset;

	switch(lvalue->type)
	{
	__lfo(ACCOUNT,account)
	__lfo(AFFECT,affect)
	__lfo(AREA,area)
	// __lfo(CHANNEL,channel)
	__lfo(CLASS,clazz)
	__lfo(DUNGEON,dungeon)
	__lfo(EXIT,ex)
	__lfo(INSTANCE,instance)
	__lfo(LIQUID,liquid)
	__lfo(MAIL,mail)
	__lfo(MATERIAL,material)
	__lfo(MISSION,mission)
	__lfo(MOBILE,mobile)
	__lfo(NOTE,note)
	__lfo(OBJECT,object)
	__lfo(ORG,org)
	// __lfo(QUEST,quest)
	__lfo(RACE,race)
	__lfo(RANK,rank)
	__lfo(REPUTATION,reputation)
	__lfo(ROOM,room)
	__lfo(SHIP,ship)
	__lfo(SKILL,skill)
	__lfo(TOKEN,token)
	__lfo(WILDS,wilds)
	case NST_WIDEVNUM:	return (void*)(lvalue->_.wnum) + field->offset;
	}

	return NULL;
}

bool nib_push_stack_field_lvalue(NIB_SCRIPT_RUNTIME *nsr, NIB_SCRIPT_LVALUE *lvalue, NIB_FIELD *field)
{
#define __flv(t,f)	case NST_##t:	_lvalue._.f = ptr; break;

	void *ptr = __get_lvalue_field(lvalue, field);
	if (!ptr) return false;

	NIB_SCRIPT_LVALUE _lvalue;
	_lvalue.type = field->stype;
	switch(field->stype)
	{
	__flv(NUMBER,number)
	__flv(FLOAT,d)
	__flv(BOOLEAN,b)
	__flv(CHAR,ch)
	__flv(STRING,str)
	// __flv(MAP,map)
	__flv(WIDEVNUM,wnum)
	__flv(ACCOUNT,account)
	__flv(AFFECT,affect)
	__flv(AREA,area)
	// __flv(CHANNEL,channel)
	__flv(CLASS,clazz)
	__flv(DUNGEON,dungeon)
	__flv(EXIT,ex)
	__flv(INSTANCE,instance)
	__flv(LIQUID,liquid)
	__flv(MAIL,mail)
	__flv(MATERIAL,material)
	__flv(MISSION,mission)
	__flv(MOBILE,mobile)
	__flv(NOTE,note)
	__flv(OBJECT,object)
	__flv(ORG,org)
	// __flv(QUEST,quest)
	__flv(RACE,race)
	__flv(RANK,rank)
	__flv(REPUTATION,reputation)
	__flv(ROOM,room)
	__flv(SHIP,ship)
	__flv(SKILL,skill)
	__flv(TOKEN,token)
	__flv(WILDS,wilds)
	case NST_FLAG:
		_lvalue._.stat.number = ptr;
		_lvalue._.stat.table = field->type->_.flag.table;
		break;

	case NST_STAT:
		_lvalue._.stat.number = ptr;
		_lvalue._.stat.table = field->type->_.stat.table;
		break;

	case NST_LIST:
	case NST_LIST_S:
		_lvalue.type = NST_LIST;	// LVALUE Lists are always "shared"
		_lvalue._.list.type = field->stype2;
		_lvalue._.list.list = ptr;
		break;

	default:
		return false;

	}

	return nib_push_stack_lvalue(nsr, &_lvalue);
}


NIB_SCRIPT_STACK_TYPE nib_peek_stack(NIB_SCRIPT_RUNTIME *nsr)
{
	if (nsr->sp < 1) return NST_UNKNOWN;
	return nsr->stack[nsr->sp-1].type;
}

NIB_SCRIPT_STACK_TYPE nib_peek_stack_lvalue_type(NIB_SCRIPT_RUNTIME *nsr)
{
	if (nsr->sp < 1) return NST_UNKNOWN;
	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp-1];
	if (stack->type != NST_LVALUE) return stack->type;
	return stack->_.lvalue.type;
}

NIB_SCRIPT_STACK_TYPE nib_peek_stack_offset(NIB_SCRIPT_RUNTIME *nsr, int offset)
{
	int sp = nsr->sp + offset;

	if (sp < 0 || sp >= nsr->sp) return NST_UNKNOWN;

	NIB_SCRIPT_STACK *stack = &nsr->stack[sp];
	if (stack->type != NST_LVALUE) return stack->type;
	return stack->_.lvalue.type;
}

NIB_SCRIPT_STACK_TYPE nib_peek_stack_offset_lvalue_type(NIB_SCRIPT_RUNTIME *nsr, int offset)
{
	int sp = nsr->sp + offset;

	if (sp < 0 || sp >= nsr->sp) return NST_UNKNOWN;

	return nsr->stack[sp].type;
}

#define __peek(t,d,f,n) \
bool nib_peek_stack_##n (NIB_SCRIPT_RUNTIME *nsr, int offset, t *output) \
{ \
	int sp = nsr->sp + offset; \
\
	if (sp < 0 || sp >= MAX_STACK) return false; \
\
	NIB_SCRIPT_STACK *stack = &nsr->stack[sp]; \
\
	if (stack->type == NST_##d) \
	{ \
		*output = stack->_.f; \
		return true; \
	} \
\
	return false; \
}

__peek(long,NUMBER,i,number)
__peek(double,FLOAT,d,float)
__peek(bool,BOOLEAN,b,boolean)
__peek(utf8char_t,CHAR,ch,char)
__peek(char *,STRING,str,string)
__peek(char *,STRING_S,str,string_shared)
__peek(WNUM,WIDEVNUM,wnum,widevnum)
bool nib_peek_stack_flag (NIB_SCRIPT_RUNTIME *nsr, int offset, long *output, const struct flag_type **table)
{
	int sp = nsr->sp + offset;

	if (sp < 0 || sp >= MAX_STACK) return false;

	NIB_SCRIPT_STACK *stack = &nsr->stack[sp];

	if (stack->type == NST_FLAG)
	{
		*output = stack->_.stat.number;
		*table = stack->_.stat.table;
		return true;
	}

	return false;
}
bool nib_peek_stack_stat (NIB_SCRIPT_RUNTIME *nsr, int offset, long *output, const struct flag_type **table)
{
	int sp = nsr->sp + offset;

	if (sp < 0 || sp >= MAX_STACK) return false;

	NIB_SCRIPT_STACK *stack = &nsr->stack[sp];

	if (stack->type == NST_STAT)
	{
		*output = stack->_.stat.number;
		*table = stack->_.stat.table;
		return true;
	}

	return false;
}
__peek(ACCOUNT_DATA *,ACCOUNT,account,account)
__peek(AFFECT_DATA *,AFFECT,affect,affect)
__peek(AREA_DATA *,AREA,area,area)
//__peek(CHANNEL_DATA *,CHANNEL,channel,channel)
__peek(CLASS_DATA *,CLASS,clazz,class)
__peek(DUNGEON *,DUNGEON,dungeon,dungeon)
__peek(EXIT_DATA *,EXIT,ex,exit)
__peek(INSTANCE *,INSTANCE,instance,instance)
__peek(LIQUID *,LIQUID,liquid,liquid)
__peek(MAIL_DATA *,MAIL,mail,mail)
__peek(MATERIAL *,MATERIAL,material,material)
__peek(MISSION_DATA *,MISSION,mission,mission)
__peek(CHAR_DATA *,MOBILE,mobile,mobile)
__peek(NOTE_DATA *,NOTE,note,note)
__peek(OBJ_DATA *,OBJECT,object,object)
__peek(CHURCH_DATA *,ORG,org,org)
// __peek(QUEST_DATA *,QUEST,quest,quest)
__peek(RACE_DATA *,RACE,race,race)
__peek(REPUTATION_INDEX_RANK_DATA *,RANK,rank,rank)
__peek(REPUTATION_DATA *,REPUTATION,reputation,reputation)
__peek(ROOM_INDEX_DATA *,ROOM,room,room)
__peek(SHIP_DATA *,SHIP,ship,ship)
__peek(SKILL_DATA *,SKILL,skill,skill)
__peek(TOKEN_DATA *,TOKEN,token,token)
__peek(WILDS_DATA *,WILDS,wilds,wilds)
//__peek(WORLD_DATA *,WORLD,world,world)
__peek(NIB_SCRIPT_LVALUE,LVALUE,lvalue,lvalue)

bool nib_peek_stack_iterator (NIB_SCRIPT_RUNTIME *nsr, int offset, ITERATOR **it, LLIST **list, NIB_SCRIPT_STACK_TYPE *type)
{
	int sp = nsr->sp + offset;

	if (sp < 0 || sp >= MAX_STACK) return false;

	NIB_SCRIPT_STACK *stack = &nsr->stack[sp];

	if (stack->type == NST_ITERATOR)
	{
		*type = stack->_.iter.type;
		*list = stack->_.iter.list;
		*it = &stack->_.iter.it;
		return true;
	}

	return false;
}

bool nib_peek_stack_list (NIB_SCRIPT_RUNTIME *nsr, int offset, LLIST **output, NIB_SCRIPT_STACK_TYPE *type)
{
	int sp = nsr->sp + offset;

	if (sp < 0 || sp >= MAX_STACK) return false;

	NIB_SCRIPT_STACK *stack = &nsr->stack[sp];

	if (stack->type == NST_LIST)
	{
		*type = stack->_.list.type;
		*output = stack->_.list.list;
		return true;
	}

	return false;
}

bool nib_peek_stack_list_shared (NIB_SCRIPT_RUNTIME *nsr, int offset, LLIST **output, NIB_SCRIPT_STACK_TYPE *type)
{
	int sp = nsr->sp + offset;

	if (sp < 0 || sp >= MAX_STACK) return false;

	NIB_SCRIPT_STACK *stack = &nsr->stack[sp];

	if (stack->type == NST_LIST_S)
	{
		*type = stack->_.list.type;
		*output = stack->_.list.list;
		return true;
	}

	return false;
}

bool stack_empty(NIB_SCRIPT_RUNTIME *nsr)
{
	return nsr->sp < 1;
}


// Pops the stack without getting the value
bool nib_pop_stack(NIB_SCRIPT_RUNTIME *nsr)
{
	if (nsr->sp < 1) return false;		// Popped one to many

	free_stack_item(&nsr->stack[--nsr->sp]);
	return true;
}

bool nib_popn_stack(NIB_SCRIPT_RUNTIME *nsr, int n)
{
	if (nsr->sp < n) return false;		// Popped one to many

	for(;n-- > 0;)
		free_stack_item(&nsr->stack[--nsr->sp]);
	return true;
}


NIB_SCRIPT_STACK *nib_pop_stack_raw(NIB_SCRIPT_RUNTIME *nsr)
{
	if (nsr->sp < 1) return NULL;	// Stack is empty

	return &nsr->stack[--nsr->sp];
}

// Pops value off stack, pulling value
#define __pop(t,d,f,n) \
bool nib_pop_stack_##n (NIB_SCRIPT_RUNTIME *nsr, t *output) \
{ \
	if (nsr->sp < 1) return false; \
\
	NIB_SCRIPT_STACK *stack = &nsr->stack[--nsr->sp]; \
\
	if (stack->type == NST_##d) \
	{ \
		*output = stack->_.f; \
		return true; \
	} \
\
	return false; \
}

__pop(long,NUMBER,i,number)
__pop(double,FLOAT,d,float)
__pop(bool,BOOLEAN,b,boolean)
__pop(utf8char_t,CHAR,ch,char)
bool nib_pop_stack_string (NIB_SCRIPT_RUNTIME *nsr, char **output) \
{
	*output = NULL;	// This needs to be initialized to NULL
	if (nsr->sp < 1) return false;

	NIB_SCRIPT_STACK *stack = &nsr->stack[--nsr->sp];

	if (stack->type == NST_STRING)
	{
		*output = stack->_.str;
		return true;
	}

	return false;
}
__pop(char *,STRING_S,str,string_shared)
__pop(WNUM,WIDEVNUM,wnum,widevnum)
bool nib_pop_stack_flag (NIB_SCRIPT_RUNTIME *nsr, long *output, const struct flag_type **table)
{
	if (nsr->sp < 1) return false;

	NIB_SCRIPT_STACK *stack = &nsr->stack[--nsr->sp];

	if (stack->type == NST_FLAG)
	{
		*output = stack->_.stat.number;
		*table = stack->_.stat.table;
		return true;
	}

	return false;
}
bool nib_pop_stack_stat (NIB_SCRIPT_RUNTIME *nsr, long *output, const struct flag_type **table)
{
	if (nsr->sp < 1) return false;

	NIB_SCRIPT_STACK *stack = &nsr->stack[--nsr->sp];

	if (stack->type == NST_STAT)
	{
		*output = stack->_.stat.number;
		*table = stack->_.stat.table;
		return true;
	}

	return false;
}
__pop(ACCOUNT_DATA *,ACCOUNT,account,account)
__pop(AFFECT_DATA *,AFFECT,affect,affect)
__pop(AREA_DATA *,AREA,area,area)
//__pop(CHANNEL_DATA *,CHANNEL,channel,channel)
__pop(CLASS_DATA *,CLASS,clazz,class)
__pop(DUNGEON *,DUNGEON,dungeon,dungeon)
__pop(EXIT_DATA *,EXIT,ex,exit)
__pop(INSTANCE *,INSTANCE,instance,instance)
__pop(LIQUID *,LIQUID,liquid,liquid)
__pop(MAIL_DATA *,MAIL,mail,mail)
__pop(MATERIAL *,MATERIAL,material,material)
__pop(MISSION_DATA *,MISSION,mission,mission)
__pop(CHAR_DATA *,MOBILE,mobile,mobile)
__pop(NOTE_DATA *,NOTE,note,note)
__pop(OBJ_DATA *,OBJECT,object,object)
__pop(CHURCH_DATA *,ORG,org,org)
// __pop(QUEST_DATA *,QUEST,quest,quest)
__pop(RACE_DATA *,RACE,race,race)
__pop(REPUTATION_INDEX_RANK_DATA *,RANK,rank,rank)
__pop(REPUTATION_DATA *,REPUTATION,reputation,reputation)
__pop(ROOM_INDEX_DATA *,ROOM,room,room)
__pop(SHIP_DATA *,SHIP,ship,ship)
__pop(SKILL_DATA *,SKILL,skill,skill)
__pop(TOKEN_DATA *,TOKEN,token,token)
__pop(WILDS_DATA *,WILDS,wilds,wilds)
//__pop(WORLD_DATA *,WORLD,world,world)

__pop(NIB_SCRIPT_LVALUE,LVALUE,lvalue,lvalue)

bool nib_pop_stack_iterator (NIB_SCRIPT_RUNTIME *nsr, ITERATOR *it, LLIST **list, NIB_SCRIPT_STACK_TYPE *type)
{
	if (nsr->sp < 1) return false;

	NIB_SCRIPT_STACK *stack = &nsr->stack[--nsr->sp];

	if (stack->type == NST_ITERATOR)
	{
		*type = stack->_.iter.type;
		*list = stack->_.iter.list;
		*it = stack->_.iter.it;
		return true;
	}

	return false;
}

bool nib_pop_stack_list (NIB_SCRIPT_RUNTIME *nsr, LLIST **output, NIB_SCRIPT_STACK_TYPE *type)
{
	if (nsr->sp < 1) return false;

	NIB_SCRIPT_STACK *stack = &nsr->stack[--nsr->sp];

	if (stack->type == NST_LIST)
	{
		*type = stack->_.list.type;
		*output = stack->_.list.list;
		return true;
	}

	return false;
}

bool nib_pop_stack_list_shared (NIB_SCRIPT_RUNTIME *nsr, LLIST **output, NIB_SCRIPT_STACK_TYPE *type)
{
	if (nsr->sp < 1) return false;

	NIB_SCRIPT_STACK *stack = &nsr->stack[--nsr->sp];

	if (stack->type == NST_LIST_S)
	{
		*type = stack->_.list.type;
		*output = stack->_.list.list;
		return true;
	}

	return false;
}

// Read instruction data
#define __get(t,n) \
static inline t __get_##n(NIB_SCRIPT_RUNTIME *nsr) \
{ \
	t value; \
	memcpy(&value, nsr->script->code + nsr->pc, sizeof(value)); \
\
	nsr->pc += sizeof(value); \
	return value; \
}

__get(utf8char_t,utf8char)
__get(short,short)
__get(int,int)
__get(long,long)
__get(char,char)
__get(nib_bytecode_t,bytecode)
__get(nib_address_t,address)
__get(long,number)
__get(bool,boolean)
__get(double,float)
__get(void *,pointer)


static NIB_LOCAL_RUNTIME_VAR *get_local_var(NIB_SCRIPT_RUNTIME *nsr, int id)
{
	if (id < 1 || id > nsr->n_locals) return NULL;

	return &nsr->locals[id - 1];
}

static pVARIABLE get_global_var(NIB_SCRIPT_RUNTIME *nsr, int id)
{
	if (id < 1 || id > nsr->script->n_globals) return NULL;

	return nsr->script->globals[id - 1].var;
}

static void __dump_stack_item(NIB_SCRIPT_STACK *stack, int sp)
{

}

void nib_dump_stack(NIB_SCRIPT_RUNTIME *nsr)
{
	printf("Program Stack:\n");
	if (nsr->sp > 0)
	{
		for(int sp = 0; sp < nsr->sp; sp++)
		{
			__dump_stack_item(&nsr->stack[sp], sp);
		}
	}
	else
		printf(" --empty--\n");
	printf("\n");
}

#define __zr(s,t,n,z) \
	case NST_##s: \
	{ \
		t value; \
		if (!nib_peek_stack_##n (nsr, -1, &value)) \
		{ \
			SETRET(nsr,STACK); \
			return true; \
		} \
\
		return (z); \
	}

static bool __is_top_zero(NIB_SCRIPT_RUNTIME *nsr)
{
	switch(nib_peek_stack(nsr))
	{
		__zr(NUMBER,long,number,!value)
		__zr(FLOAT,double,float,value == 0.0)
		__zr(BOOLEAN,bool,boolean,!value)
		__zr(CHAR,utf8char_t,char,value == '\0')
		__zr(STRING,char *,string,IS_NULLSTR(value))
		__zr(STRING_S,char *,string_shared,IS_NULLSTR(value))
//		__zr(MAP...)
		__zr(WIDEVNUM,WNUM,widevnum,(!value.pArea && value.vnum < 1))
		__zr(ACCOUNT,ACCOUNT_DATA *,account,!IS_VALID(value))
		__zr(AFFECT,AFFECT_DATA *,affect,!IS_VALID(value))
		__zr(AREA,AREA_DATA *,area,value)
		// __zr(CHANNEL,CHANNEL_DATA *,channel,!value)
		__zr(CLASS,CLASS_DATA *,class,!IS_VALID(value))
		__zr(DUNGEON,DUNGEON *,dungeon,!IS_VALID(value))
		__zr(EXIT,EXIT_DATA *,exit,!IS_VALID(value))
		__zr(INSTANCE,INSTANCE *,instance,!IS_VALID(value))
		__zr(LIQUID,LIQUID *,liquid,!IS_VALID(value))
		__zr(MAIL,MAIL_DATA *,mail,!value)
		__zr(MATERIAL,MATERIAL *,material,!value)
		__zr(MISSION,MISSION_DATA *,mission,!value)
		__zr(MOBILE,CHAR_DATA *,mobile,!IS_VALID(value))
		__zr(NOTE,NOTE_DATA *,note,!IS_VALID(value))
		__zr(OBJECT,OBJ_DATA *,object,!IS_VALID(value))
		__zr(ORG,CHURCH_DATA *,org,value)
		//__zr(QUEST,QUEST_DATA *,quest,!IS_VALID(value))
		__zr(RACE,RACE_DATA *,race,!IS_VALID(value))
		__zr(RANK,REPUTATION_INDEX_RANK_DATA *,rank,!value)
		__zr(REPUTATION,REPUTATION_DATA *,reputation,!IS_VALID(value))
		__zr(ROOM,ROOM_INDEX_DATA *,room,!value)
		__zr(SHIP,SHIP_DATA *,ship,!IS_VALID(value))
		__zr(SKILL,SKILL_DATA *,skill,!IS_VALID(value))
		__zr(TOKEN,TOKEN_DATA *,token,!IS_VALID(value))
		__zr(WILDS,WILDS_DATA *,wilds,value)
		// __zr(WORLD,WORLD_DATA *,world,!IS_VALID(value))

		case NST_FLAG:
		{
			long value;
			if (!nib_peek_stack_flag(nsr, -1, &value, NULL))
			{
				SETRET(nsr,STACK);
				return true;
			}

			return !value;
		}

		case NST_LIST:
		{
			LLIST *list;
			NIB_SCRIPT_STACK_TYPE type;
			if (!nib_peek_stack_list(nsr, -1, &list, &type))
			{
				SETRET(nsr,STACK);
				return true;
			}

			return !list_isvalid(list) || list_size(list) < 1;	// invalid or empty
		}
	
		case NST_LIST_S:
		{
			LLIST *list;
			NIB_SCRIPT_STACK_TYPE type;
			if (!nib_peek_stack_list_shared(nsr, -1, &list, &type))
			{
				SETRET(nsr,STACK);
				return true;
			}

			return !list_isvalid(list) || list_size(list) < 1;	// invalid or empty
		}

		case NST_LVALUE:
		{
			NIB_SCRIPT_LVALUE lvalue;
			if (!nib_peek_stack_lvalue(nsr, -1, &lvalue))
			{
				SETRET(nsr,STACK);
				return true;
			}

			switch(lvalue.type)
			{
			case NST_NUMBER:		return !*(lvalue._.number);
			case NST_FLOAT:			return *(lvalue._.d) == 0.0;
			case NST_BOOLEAN:		return !*(lvalue._.b);
			case NST_FLAG_BIT:		return !IS_SET(*(lvalue._.bit.value),lvalue._.bit.bit);
			case NST_CHAR:			return !*(lvalue._.ch);
			case NST_STRING:		return !*(lvalue._.str) || !(*(lvalue._.str))[0];
			// case NST_MAP:		return is map null or empty;
			case NST_WIDEVNUM:		return !lvalue._.wnum->pArea && lvalue._.wnum->vnum < 1;
			case NST_ACCOUNT:		return !IS_VALID((*(lvalue._.account)));
			case NST_AFFECT:		return !IS_VALID((*(lvalue._.affect)));
			case NST_AREA:			return !*(lvalue._.area);
			// case NST_CHANNEL:		return !IS_VALID((*(lvalue._.channel)));
			case NST_CLASS:			return !IS_VALID((*(lvalue._.clazz)));
			case NST_DUNGEON:		return !IS_VALID((*(lvalue._.dungeon)));
			case NST_EXIT:			return !IS_VALID((*(lvalue._.ex)));
			case NST_INSTANCE:		return !IS_VALID((*(lvalue._.instance)));
			case NST_LIQUID:		return !IS_VALID((*(lvalue._.liquid)));
			case NST_MAIL:			return !*(lvalue._.mail);
			case NST_MATERIAL:		return !IS_VALID((*(lvalue._.material)));
			case NST_MISSION:		return !*(lvalue._.mission);
			case NST_MOBILE:		return !IS_VALID((*(lvalue._.mobile)));
			case NST_NOTE:			return !IS_VALID((*(lvalue._.note)));
			case NST_OBJECT:		return !IS_VALID((*(lvalue._.object)));
			case NST_ORG:			return !*(lvalue._.org);
			// case NST_QUEST:			return !IS_VALID((*(lvalue._.quest)));
			case NST_RACE:			return !IS_VALID((*(lvalue._.race)));
			case NST_RANK:			return !*(lvalue._.rank);
			case NST_REPUTATION:	return !IS_VALID((*(lvalue._.reputation)));
			case NST_ROOM:			return !*(lvalue._.room);
			case NST_SHIP:			return !IS_VALID((*(lvalue._.ship)));
			case NST_SKILL:			return !IS_VALID((*(lvalue._.skill)));
			case NST_TOKEN:			return !IS_VALID((*(lvalue._.token)));
			case NST_WILDS:			return !*(lvalue._.wilds);
			// case NST_WORLD:			return !IS_VALID((*(lvalue._.world)));
			case NST_FLAG:			return !*(lvalue._.stat.number);
			case NST_LIST:			return !list_isvalid(*(lvalue._.list.list)) || list_size(*(lvalue._.list.list)) < 1;
			}
			break;
		}
	}
	return false;
}

static bool __is_top_not_zero(NIB_SCRIPT_RUNTIME *nsr)
{
	switch(nib_peek_stack(nsr))
	{
		__zr(NUMBER,long,number,value != 0)
		__zr(FLOAT,double,float,value != 0.0)
		__zr(BOOLEAN,bool,boolean,value)
		__zr(CHAR,utf8char_t,char,value != '\0')
		__zr(STRING,char *,string,!IS_NULLSTR(value))
		__zr(STRING_S,char *,string_shared,!IS_NULLSTR(value))
		// __zr(MAP...)
		__zr(WIDEVNUM,WNUM,widevnum,value.vnum > 0)
		__zr(ACCOUNT,ACCOUNT_DATA *,account,IS_VALID(value))
		__zr(AFFECT,AFFECT_DATA *,affect,IS_VALID(value))
		__zr(AREA,AREA_DATA *,area,value != NULL)
		// __zr(CHANNEL,CHANNEL_DATA *,channel,!value)
		__zr(CLASS,CLASS_DATA *,class,IS_VALID(value))
		__zr(DUNGEON,DUNGEON *,dungeon,IS_VALID(value))
		__zr(EXIT,EXIT_DATA *,exit,IS_VALID(value))
		__zr(INSTANCE,INSTANCE *,instance,IS_VALID(value))
		__zr(LIQUID,LIQUID *,liquid,IS_VALID(value))
		__zr(MAIL,MAIL_DATA *,mail,value != NULL)
		__zr(MATERIAL,MATERIAL *,material,value)
		__zr(MISSION,MISSION_DATA *,mission,value)
		__zr(MOBILE,CHAR_DATA *,mobile,IS_VALID(value))
		__zr(NOTE,NOTE_DATA *,note,IS_VALID(value))
		__zr(OBJECT,OBJ_DATA *,object,IS_VALID(value))
		__zr(ORG,CHURCH_DATA *,org,value != NULL)
		//__zr(QUEST,QUEST_DATA *,quest,IS_VALID(value))
		__zr(RACE,RACE_DATA *,race,IS_VALID(value))
		__zr(RANK,REPUTATION_INDEX_RANK_DATA *,rank,value)
		__zr(REPUTATION,REPUTATION_DATA *,reputation,IS_VALID(value))
		__zr(ROOM,ROOM_INDEX_DATA *,room,value != NULL)
		__zr(SHIP,SHIP_DATA *,ship,IS_VALID(value))
		__zr(SKILL,SKILL_DATA *,skill,IS_VALID(value))
		__zr(TOKEN,TOKEN_DATA *,token,IS_VALID(value))
		__zr(WILDS,WILDS_DATA *,wilds,value != NULL)
		// __zr(WORLD,WORLD_DATA *,world,!IS_VALID(value))

		case NST_FLAG:
		{
			long value;
			if (!nib_peek_stack_flag(nsr, -1, &value, NULL))
			{
				SETRET(nsr,STACK);
				return true;
			}

			return value != 0;
		}

		case NST_LIST:
		{
			LLIST *list;
			NIB_SCRIPT_STACK_TYPE type;
			if (!nib_peek_stack_list(nsr, -1, &list, &type))
			{
				SETRET(nsr,STACK);
				return true;
			}

			return list_size(list) > 0;
		}
	
		case NST_LIST_S:
		{
			LLIST *list;
			NIB_SCRIPT_STACK_TYPE type;
			if (!nib_peek_stack_list_shared(nsr, -1, &list, &type))
			{
				SETRET(nsr,STACK);
				return true;
			}

			return list_size(list) > 0;
		}

		case NST_LVALUE:
		{
			NIB_SCRIPT_LVALUE lvalue;
			if (!nib_peek_stack_lvalue(nsr, -1, &lvalue))
			{
				SETRET(nsr,STACK);
				return true;
			}

			switch(lvalue.type)
			{
			case NST_NUMBER:		return *(lvalue._.number) != 0;
			case NST_FLOAT:			return *(lvalue._.d) != 0.0;
			case NST_BOOLEAN:		return *(lvalue._.b);
			case NST_FLAG_BIT:		return IS_SET(*(lvalue._.bit.value),lvalue._.bit.bit);
			case NST_CHAR:			return *(lvalue._.ch) != '\0';
			case NST_STRING:		return !IS_NULLSTR((*(lvalue._.str)));
			// case NST_MAP:		return is map valid and with entries;
			case NST_WIDEVNUM:		return lvalue._.wnum->vnum > 0;
			case NST_ACCOUNT:		return IS_VALID((*(lvalue._.account)));
			case NST_AFFECT:		return IS_VALID((*(lvalue._.affect)));
			case NST_AREA:			return *(lvalue._.area) != NULL;
			// case NST_CHANNEL:		return IS_VALID((*(lvalue._.channel)));
			case NST_CLASS:			return IS_VALID((*(lvalue._.clazz)));
			case NST_DUNGEON:		return IS_VALID((*(lvalue._.dungeon)));
			case NST_EXIT:			return IS_VALID((*(lvalue._.ex)));
			case NST_INSTANCE:		return IS_VALID((*(lvalue._.instance)));
			case NST_LIQUID:		return IS_VALID((*(lvalue._.liquid)));
			case NST_MAIL:			return *(lvalue._.mail) != NULL;
			case NST_MATERIAL:		return IS_VALID((*(lvalue._.material)));
			case NST_MISSION:		return *(lvalue._.mission) != NULL;
			case NST_MOBILE:		return IS_VALID((*(lvalue._.mobile)));
			case NST_NOTE:			return IS_VALID((*(lvalue._.note)));
			case NST_OBJECT:		return IS_VALID((*(lvalue._.object)));
			case NST_ORG:			return *(lvalue._.org) != NULL;
			// case NST_QUEST:			return IS_VALID((*(lvalue._.quest)));
			case NST_RACE:			return IS_VALID((*(lvalue._.race)));
			case NST_RANK:			return *(lvalue._.rank) != NULL;
			case NST_REPUTATION:	return IS_VALID((*(lvalue._.reputation)));
			case NST_ROOM:			return *(lvalue._.room) != NULL;
			case NST_SHIP:			return IS_VALID((*(lvalue._.ship)));
			case NST_SKILL:			return IS_VALID((*(lvalue._.skill)));
			case NST_TOKEN:			return IS_VALID((*(lvalue._.token)));
			case NST_WILDS:			return *(lvalue._.wilds) != NULL;
			// case NST_WORLD:			return IS_VALID((*(lvalue._.world)));
			case NST_FLAG:			return *(lvalue._.number) != 0;
			case NST_LIST:			return list_size(*(lvalue._.list.list)) > 0;
			}
			break;
		}
	}
	return false;
}

static bool __increment_stack(NIB_SCRIPT_RUNTIME *nsr, bool post, bool push_result)
{
	NIB_SCRIPT_LVALUE lvalue;

	if (!nib_pop_stack_lvalue(nsr, &lvalue))
		return false;

	if (lvalue.type != NST_NUMBER)
		return false;

	long value;

	if (post)
	{
		value = (*(lvalue._.number))++;
	}
	else
	{
		value = ++(*(lvalue._.number));
	}

	if (push_result)
		return nib_push_stack_number(nsr, value);

	return true;
}

static bool __decrement_stack(NIB_SCRIPT_RUNTIME *nsr, bool post, bool push_result)
{
	NIB_SCRIPT_LVALUE lvalue;

	if (!nib_pop_stack_lvalue(nsr, &lvalue))
		return false;

	if (lvalue.type != NST_NUMBER)
		return false;

	long value;

	if (post)
	{
		value = (*(lvalue._.number))--;
	}
	else
	{
		value = --(*(lvalue._.number));
	}

	if (push_result)
		return nib_push_stack_number(nsr, value);

	return true;
}


// This only handles operations that allow for actual operations
// Only arithmetic and bitwise operations are done
static bool __binary_operation(NIB_SCRIPT_RUNTIME *nsr, enum nib_instructions_e op)
{
	// Stack Order
	// RHS
	NIB_SCRIPT_STACK *rsp = nib_pop_stack_raw(nsr);
	if (!rsp)
	{
		SETRET(nsr,STACK);
		return true;
	}
	// LHS
	NIB_SCRIPT_STACK *lsp = nib_pop_stack_raw(nsr);
	if (!lsp)
	{
		free_stack_item(rsp);
		SETRET(nsr,STACK);
		return true;
	}

	switch(lsp->type)
	{
	case NST_NUMBER:		// NUMBER op ???
		{
			switch(rsp->type)
			{
			case NST_NUMBER:	// NUMBER op NUMBER => NUMBER
				{
					long value;
					switch(op)
					{
					case NI_ADD:	value = lsp->_.i + rsp->_.i; break;
					case NI_SUBT:	value = lsp->_.i - rsp->_.i; break;
					case NI_MULT:	value = lsp->_.i * rsp->_.i; break;
					case NI_MOD:
						if (rsp->_.i == 0)
						{
							SETRET(nsr,MATH);
							return true;
						}

						value = lsp->_.i % rsp->_.i;
						break;
					
					case NI_DIV:
						if (rsp->_.i == 0)
						{
							SETRET(nsr,MATH);
							return true;
						}

						value = lsp->_.i / rsp->_.i;
						break;

					case NI_BAND:	value = (lsp->_.i & rsp->_.i); break;
					case NI_BOR:	value = (lsp->_.i | rsp->_.i); break;
					case NI_BXOR:	value = (lsp->_.i ^ rsp->_.i); break;
					case NI_LSH:	value = (lsp->_.i << rsp->_.i); break;
					case NI_RSH:	value = (lsp->_.i >> rsp->_.i); break;
					case NI_RSHL:	value = (long)(((unsigned long)lsp->_.i) >> rsp->_.i); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_number(nsr, value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_FLOAT:		// NUMBER op FLOAT => FLOAT
				{
					double value;
					switch(op)
					{
					case NI_ADD:	value = (double)lsp->_.i + rsp->_.d; break;
					case NI_SUBT:	value = (double)lsp->_.i - rsp->_.d; break;
					case NI_MULT:	value = (double)lsp->_.i * rsp->_.d; break;
					case NI_DIV:
						if (rsp->_.d == 0.0)
						{
							SETRET(nsr,MATH);
							return true;
						}

						value = (double)lsp->_.i / rsp->_.d;
						break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_float(nsr, value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_CHAR:		// NUMBER op CHAR => STRING
				{
					if (op != NI_MULT)
					{
						SETRET(nsr,INVALID);
						return true;
					}

					// Cloning
					char *bytes = utf8_getbytes(rsp->_.ch);
					int len = strlen(bytes);
					int cnt = ((lsp->_.i>0)?lsp->_.i:0);

					char *value = calloc(1,cnt*len+1);
					if (!value)
					{
						SETRET(nsr,MEMORY);
						return true;
					}
					char *s = value;
					for(int i = cnt; i-- > 0;s += len)
						strcpy(value,bytes);
					*s = '\0';

					if (!nib_push_stack_string_raw(nsr,value))
					{
						free(value);
						SETRET(nsr,STACK);
						return true;
					}

					break;
				}
			case NST_STRING:	// NUMBER op STRING => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char number[100];
							ltoa(lsp->_.i, number);
							if (rsp->_.str)
							{
								char *value = calloc(1,strlen(number)+strlen(rsp->_.str)+1);
								if (!value)
								{
									free(rsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,number);
								strcat(value,rsp->_.str);
								free(rsp->_.str);

								if(!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return false;
								}
							}
							else if(!nib_push_stack_string(nsr,number))
							{
								SETRET(nsr,STACK);
								return true;
							}
								
							break;
						}

					case NI_MULT:		// Cloning
						{
							if (rsp->_.str)
							{
								int len = strlen(rsp->_.str);
								int cnt = (lsp->_.i>0)?lsp->_.i:0;
								char *value = calloc(1,cnt * len + 1);
								if(!value)
								{
									free(rsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								char *str = value;
								for(int i = 0; i < cnt; i++, str += len)
									strcpy(str,rsp->_.str);
								*str = '\0';
								free(rsp->_.str);

								if(!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return false;
								}
							}
							else if(!nib_push_stack_string(nsr, ""))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						if (rsp->_.str) free(rsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_STRING_S:	// NUMBER op STRING(s) => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char number[100];
							ltoa(lsp->_.i, number);
							if (rsp->_.str)
							{
								char *value = calloc(1,strlen(number)+strlen(rsp->_.str)+1);
								if(!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,number);
								strcat(value,rsp->_.str);

								if(!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return false;
								}
							}
							else if(!nib_push_stack_string(nsr,number))
							{
								SETRET(nsr,STACK);
								return true;
							}
								
							break;
						}

					case NI_MULT:		// Cloning
						{
							if (rsp->_.str)
							{
								int len = strlen(rsp->_.str);
								int cnt = (lsp->_.i>0)?lsp->_.i:0;
								char *value = calloc(1,cnt * len + 1);
								if(!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								char *str = value;
								for(int i = 0; i < cnt; i++, str += len)
									strcpy(str,rsp->_.str);
								*str = '\0';

								if(!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return false;
								}
							}
							else if(!nib_push_stack_string(nsr, ""))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_FLAG:		// NUMBER op FLAG => FLAG
				{
					long value;
					switch(op)
					{
					case NI_BAND:	value = lsp->_.i & rsp->_.stat.number; break;
					case NI_BOR:	value = lsp->_.i | rsp->_.stat.number; break;
					case NI_BXOR:	value = lsp->_.i ^ rsp->_.stat.number; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_flag(nsr, value, rsp->_.stat.table))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}
			case NST_LVALUE:	// NUMBER op LVALUE
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:	// NUMBER op NUMBER
						{
							long value;
							switch(op)
							{
							case NI_ADD:	value = lsp->_.i + *(rsp->_.lvalue._.number); break;
							case NI_SUBT:	value = lsp->_.i - *(rsp->_.lvalue._.number); break;
							case NI_MULT:	value = lsp->_.i * *(rsp->_.lvalue._.number); break;
							case NI_MOD:
								if (*(rsp->_.lvalue._.number) == 0)
								{
									SETRET(nsr,MATH);
									return true;
								}

								value = lsp->_.i % *(rsp->_.lvalue._.number);
								break;
							
							case NI_DIV:
								if (*(rsp->_.lvalue._.number) == 0)
								{
									SETRET(nsr,MATH);
									return true;
								}

								value = lsp->_.i / *(rsp->_.lvalue._.number);
								break;

							case NI_BAND:		value = (lsp->_.i & *(rsp->_.lvalue._.number)); break;
							case NI_BOR:		value = (lsp->_.i | *(rsp->_.lvalue._.number)); break;
							case NI_BXOR:		value = (lsp->_.i ^ *(rsp->_.lvalue._.number)); break;

							case NI_LSH:		value = (lsp->_.i << *(rsp->_.lvalue._.number)); break;
							case NI_RSH:		value = (lsp->_.i >> *(rsp->_.lvalue._.number)); break;
							case NI_RSHL:		value = (long)(((unsigned long)lsp->_.i) >> *(rsp->_.lvalue._.number)); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							break;
						}
					case NST_FLOAT:		// NUMBER op FLOAT => FLOAT
						{
							double value;
							switch(op)
							{
							case NI_ADD:	value = (double)lsp->_.i + *(rsp->_.lvalue._.d); break;
							case NI_SUBT:	value = (double)lsp->_.i - *(rsp->_.lvalue._.d); break;
							case NI_MULT:	value = (double)lsp->_.i * *(rsp->_.lvalue._.d); break;
							case NI_DIV:
								if (*(rsp->_.lvalue._.d) == 0.0)
								{
									SETRET(nsr,MATH);
									return true;
								}

								value = (double)lsp->_.i / *(rsp->_.lvalue._.d);
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_float(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_CHAR:		// NUMBER op CHAR => STRING
						{
							if (op != NI_MULT)
							{
								SETRET(nsr,INVALID);
								return true;
							}

							// Cloning
							char *bytes = utf8_getbytes(*(rsp->_.lvalue._.ch));
							int len = strlen(bytes);
							int cnt = (lsp->_.i>0)?lsp->_.i:0;

							char *value = calloc(1,cnt*len+1);
							if (!value)
							{
								SETRET(nsr,MEMORY);
								return true;
							}
							char *s = value;
							for(int i = cnt; i-- > 0; s += len)
								strcpy(s,bytes);
							*s = '\0';

							if (!nib_push_stack_string_raw(nsr,value))
							{
								free(value);
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}
					case NST_STRING:	// NUMBER op STRING => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char number[100];
									ltoa(lsp->_.i, number);
									if (*(rsp->_.lvalue._.str))
									{
										char *value = calloc(1,strlen(number)+strlen(rsp->_.str)+1);
										if(!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,number);
										strcat(value,*(rsp->_.lvalue._.str));

										if(!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return false;
										}
									}
									else if(!nib_push_stack_string(nsr,number))
									{
										SETRET(nsr,STACK);
										return true;
									}
										
									break;
								}

							case NI_MULT:		// Cloning
								{
									if (*(rsp->_.lvalue._.str))
									{
										int len = strlen(*(rsp->_.lvalue._.str));
										int cnt = (lsp->_.i>0)?lsp->_.i:0;
										char *value = calloc(1,cnt * len + 1);
										if(!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										char *str = value;
										for(int i = 0; i < cnt; i++, str += len)
											strcpy(str,*(rsp->_.lvalue._.str));
										*str = '\0';

										if(!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return false;
										}
									}
									else if(!nib_push_stack_string(nsr, ""))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_FLAG:		// NUMBER op FLAG => FLAG
						{
							long value;
							switch(op)
							{
							case NI_BAND:	value = lsp->_.i & *(rsp->_.lvalue._.stat.number); break;
							case NI_BOR:	value = lsp->_.i | *(rsp->_.lvalue._.stat.number); break;
							case NI_BXOR:	value = lsp->_.i ^ *(rsp->_.lvalue._.stat.number); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_flag(nsr, value, rsp->_.lvalue._.stat.table))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}
					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_FLOAT:			// FLOAT op ???
		{
			switch(rsp->type)
			{
			case NST_NUMBER:	// FLOAT op NUMBER => FLOAT
				{
					double value;
					switch(op)
					{
					case NI_ADD:	value = lsp->_.d + (double)rsp->_.i; break;
					case NI_SUBT:	value = lsp->_.d - (double)rsp->_.i; break;
					case NI_MULT:	value = lsp->_.d * (double)rsp->_.i; break;
					case NI_DIV:
						if (rsp->_.i == 0)
						{
							SETRET(nsr,MATH);
							return true;
						}

						value = lsp->_.d / (double)rsp->_.i;
						break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_float(nsr, value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_FLOAT:		// FLOAT op FLOAT => FLOAT
				{
					double value;
					switch(op)
					{
					case NI_ADD:	value = lsp->_.d + rsp->_.d; break;
					case NI_SUBT:	value = lsp->_.d - rsp->_.d; break;
					case NI_MULT:	value = lsp->_.d * rsp->_.d; break;
					case NI_DIV:
						if (rsp->_.d == 0.0)
						{
							SETRET(nsr,MATH);
							return true;
						}

						value = lsp->_.d / rsp->_.d;
						break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_float(nsr, value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// FLOAT op LVALUE
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:	// FLOAT op NUMBER => FLOAT
						{
							double value;
							switch(op)
							{
							case NI_ADD:	value = lsp->_.d + (double)*(rsp->_.lvalue._.number); break;
							case NI_SUBT:	value = lsp->_.d - (double)*(rsp->_.lvalue._.number); break;
							case NI_MULT:	value = lsp->_.d * (double)*(rsp->_.lvalue._.number); break;
							case NI_DIV:
								if (*(rsp->_.lvalue._.number) == 0)
								{
									SETRET(nsr,MATH);
									return true;
								}

								value = lsp->_.d / (double)*(rsp->_.lvalue._.number);
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_float(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}


					case NST_FLOAT:		// FLOAT op FLOAT => FLOAT
						{
							double value;
							switch(op)
							{
							case NI_ADD:	value = lsp->_.d + *(rsp->_.lvalue._.d); break;
							case NI_SUBT:	value = lsp->_.d - *(rsp->_.lvalue._.d); break;
							case NI_MULT:	value = lsp->_.d * *(rsp->_.lvalue._.d); break;
							case NI_DIV:
								if (*(rsp->_.lvalue._.d) == 0.0)
								{
									SETRET(nsr,MATH);
									return true;
								}

								value = lsp->_.d / *(rsp->_.lvalue._.d);
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_float(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_CHAR:			// CHAR op ???
		{
			switch(rsp->type)
			{
			case NST_NUMBER:		// CHAR op NUMBER => STRING
				{
					if (op != NI_MULT)
					{
						SETRET(nsr,INVALID);
						return true;
					}

					// Cloning
					char *bytes = utf8_getbytes(rsp->_.ch);
					int len = strlen(bytes);
					int cnt = (rsp->_.i>0)?rsp->_.i:0;
					char *value = calloc(1,cnt*len+1);
					if (!value)
					{
						SETRET(nsr,MEMORY);
						return true;
					}
					char *s = value;
					for(int i = cnt; i-- > 0; s += len)
						strcpy(s,bytes);
					*s = '\0';

					if (!nib_push_stack_string_raw(nsr,value))
					{
						free(value);
						SETRET(nsr,STACK);
						return true;
					}

					break;
				}

			case NST_CHAR:			// CHAR op CHAR => STRING
				{
					if (op != NI_ADD)
					{
						SETRET(nsr,INVALID);
						return true;
					}

					// If LHS is '\0', it will end up being an empty string
					char *lbytes = utf8_getbytes(lsp->_.ch);
					char *rbytes = utf8_getbytes(rsp->_.ch);
					int llen = strlen(lbytes);
					int rlen = strlen(rbytes);

					char *value = calloc(1,llen+rlen+1);
					if (!value)
					{
						SETRET(nsr,MEMORY);
						return true;
					}
					strcpy(value,lbytes);
					strcpy(value+llen,rbytes);

					if (!nib_push_stack_string_raw(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_STRING:		// CHAR op STRING => STRING
				{
					if (op != NI_ADD)
					{
						if (rsp->_.str) free(rsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}

					if (lsp->_.ch > 0)
					{
						char *bytes = utf8_getbytes(lsp->_.ch);
						int len = strlen(bytes);

						if (rsp->_.str)
						{
							char *value = calloc(1,len+strlen(rsp->_.str)+1);
							if (!value)
							{
								if (rsp->_.str) free(rsp->_.str);
								SETRET(nsr,MEMORY);
								return true;
							}

							strcpy(value,bytes);
							strcpy(value+len,rsp->_.str);
							free(rsp->_.str);

							if (!nib_push_stack_string_raw(nsr,value))
							{
								free(value);
								SETRET(nsr,STACK);
								return true;
							}
						}
						else
						{
							if (!nib_push_stack_string(nsr,bytes))
							{
								SETRET(nsr,STACK);
								return true;
							}
						}
					}
					else
					{
						// If the LHS is '\0', it becomes an empty string
						free(rsp->_.str);
						if (!nib_push_stack_string(nsr,""))
						{
							SETRET(nsr,STACK);
							return true;
						}
					}
					break;
				}

			case NST_STRING_S:		// CHAR op STRING(s) => STRING
				{
					if (op != NI_ADD)
					{
						SETRET(nsr,INVALID);
						return true;
					}

					if (lsp->_.ch > 0)
					{
						char *bytes = utf8_getbytes(lsp->_.ch);
						int len = strlen(bytes);

						if (rsp->_.str)
						{
							char *value = calloc(1,len+strlen(rsp->_.str)+1);
							if (!value)
							{
								SETRET(nsr,MEMORY);
								return true;
							}

							strcpy(value,bytes);
							strcpy(value+len,rsp->_.str);

							if (!nib_push_stack_string_raw(nsr,value))
							{
								free(value);
								SETRET(nsr,STACK);
								return true;
							}
						}
						else
						{
							if (!nib_push_stack_string(nsr,bytes))
							{
								SETRET(nsr,STACK);
								return true;
							}
						}
					}
					else if (!nib_push_stack_string(nsr,""))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:		// CHAR op LVALUE
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:		// CHAR op NUMBER => STRING
						{
							if (op != NI_MULT)
							{
								SETRET(nsr,INVALID);
								return true;
							}

							// Cloning
							char *bytes = utf8_getbytes(*(rsp->_.lvalue._.ch));
							int len = strlen(bytes);
							int cnt = *(rsp->_.lvalue._.number);
							cnt = (cnt>0)?cnt:0;
							char *value = calloc(1,len*cnt+1);
							if (!value)
							{
								SETRET(nsr,MEMORY);
								return true;
							}
							char *s = value;
							for(int i = cnt; i-- > 0; s+=len)
								strcpy(s,bytes);
							*s = '\0';

							if (!nib_push_stack_string_raw(nsr,value))
							{
								free(value);
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					case NST_CHAR:			// CHAR op CHAR => STRING
						{
							if (op != NI_ADD)
							{
								SETRET(nsr,INVALID);
								return true;
							}

							char *lbytes = utf8_getbytes(lsp->_.ch);
							char *rbytes = utf8_getbytes(*(rsp->_.lvalue._.ch));
							int llen = strlen(lbytes);
							int rlen = strlen(rbytes);

							char *value = calloc(1,llen+rlen+1);
							if (!value)
							{
								SETRET(nsr,MEMORY);
								return true;
							}
							strcpy(value,lbytes);
							strcpy(value+llen,rbytes);

							if (!nib_push_stack_string_raw(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_STRING:		// CHAR op STRING => STRING
						{
							if (op != NI_ADD)
							{
								SETRET(nsr,INVALID);
								return true;
							}

							if (lsp->_.ch > 0)
							{
								char *bytes = utf8_getbytes(lsp->_.ch);
								int len = strlen(bytes);

								if (*(rsp->_.lvalue._.str))
								{
									char *value = calloc(1,len+strlen(*(rsp->_.lvalue._.str))+1);
									if (!value)
									{
										SETRET(nsr,MEMORY);
										return true;
									}

									strcpy(value,bytes);
									strcpy(value+len,*(rsp->_.lvalue._.str));

									if (!nib_push_stack_string_raw(nsr,value))
									{
										free(value);
										SETRET(nsr,STACK);
										return true;
									}
								}
								else
								{
									if (!nib_push_stack_string(nsr,bytes))
									{
										SETRET(nsr,STACK);
										return true;
									}
								}
							}

							// Concatenating a string onto a null character will end up being an empty string
							else if (!nib_push_stack_string(nsr,""))
							{
								SETRET(nsr,STACK);
								return true;
							}
							
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
				}
			
			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_STRING:		// STRING op ???
		{
			switch(rsp->type)
			{
			case NST_NUMBER:	// STRING op NUMBER => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							ltoa(rsp->_.i,stringify);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					case NI_MULT:		// Cloning
						{
							if (lsp->_.str)
							{
								int len = strlen(lsp->_.str);
								char *value = calloc(1,len * ((rsp->_.i > 0)?rsp->_.i:0) + 1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}

								char *str = value;
								for(int i = rsp->_.i; i-- > 0; str += len)
									strcpy(str, lsp->_.str);
								*str = 0;

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr, value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr, ""))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_FLOAT:		// STRING op FLOAT => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							sprintf(stringify,"%lf", rsp->_.d);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_BOOLEAN:	// STRING op BOOLEAN => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char *stringify = (rsp->_.b?"true":"false");
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_CHAR:		// STRING op CHAR => STRING
				{
					if (op != NI_ADD)
					{
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}

					if (lsp->_.str)
					{
						if (rsp->_.ch > 0)
						{
							char *bytes = utf8_getbytes(rsp->_.ch);
							int blen = strlen(bytes);
							int len = strlen(lsp->_.str);
							char *value = calloc(1,len+blen+1);
							if (!value)
							{
								free(lsp->_.str);
								SETRET(nsr,MEMORY);
								return true;
							}
							strcpy(value,lsp->_.str);
							strcpy(value+len,bytes);
							value[len+blen] = '\0';

							free(lsp->_.str);

							if (!nib_push_stack_string_raw(nsr,value))
							{
								free(value);
								SETRET(nsr,STACK);
								return true;
							}
						}
						else if (!nib_push_stack_string_raw(nsr,lsp->_.str))
						{
							free(lsp->_.str);
							SETRET(nsr,STACK);
							return true;
						}
					}
					else
					{
						char *bytes = utf8_getbytes(rsp->_.ch);

						if (!nib_push_stack_string(nsr, bytes))
						{
							SETRET(nsr,STACK);
							return true;
						}
					}
					break;
				}

			case NST_STRING:	// STRING op STRING => STRING
				{
					if (op != NI_ADD)
					{
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}

					if (lsp->_.str)
					{
						if (rsp->_.str)
						{
							char *value = calloc(1,strlen(lsp->_.str)+strlen(rsp->_.str)+1);
							if (!value)
							{
								free(lsp->_.str);
								free(rsp->_.str);
								SETRET(nsr,MEMORY);
								return true;
							}

							strcpy(value,lsp->_.str);
							strcat(value,rsp->_.str);

							free(lsp->_.str);
							free(rsp->_.str);

							if (!nib_push_stack_string_raw(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
						}
						else if (!nib_push_stack_string_raw(nsr, lsp->_.str))
						{
							free(lsp->_.str);
							SETRET(nsr,STACK);
							return true;
						}
					}
					else if (rsp->_.str)
					{
						if (!nib_push_stack_string_raw(nsr, rsp->_.str))
						{
							free(rsp->_.str);
							SETRET(nsr,STACK);
							return true;
						}
					}
					else if (!nib_push_stack_string(nsr, ""))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_STRING_S:	// STRING op STRING(s) => STRING
				{
					if (op != NI_ADD)
					{
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}

					if (lsp->_.str)
					{
						if (rsp->_.str)
						{
							char *value = calloc(1,strlen(lsp->_.str)+strlen(rsp->_.str)+1);
							if (!value)
							{
								free(lsp->_.str);
								SETRET(nsr,MEMORY);
								return true;
							}

							strcpy(value,lsp->_.str);
							strcat(value,rsp->_.str);

							free(lsp->_.str);

							if (!nib_push_stack_string_raw(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
						}
						else if (!nib_push_stack_string_raw(nsr, lsp->_.str))
						{
							free(lsp->_.str);
							SETRET(nsr,STACK);
							return true;
						}
					}
					else if (rsp->_.str)
					{
						if (!nib_push_stack_string(nsr, rsp->_.str))
						{
							SETRET(nsr,STACK);
							return true;
						}
					}
					else if (!nib_push_stack_string(nsr, ""))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_WIDEVNUM:	// STRING op WIDEVNUM => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							sprintf(stringify,"(%ld,%ld)",
								(rsp->_.wnum.pArea?rsp->_.wnum.pArea->uid:0),
								rsp->_.wnum.vnum);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_FLAG:		// STRING op FLAG => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							const char *stringify = nib_get_flag_string(rsp->_.stat.table,rsp->_.stat.number);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_STAT:		// STRING op STAT => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							const char *stringify = nib_get_stat_string(rsp->_.stat.table,rsp->_.stat.number);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_ACCOUNT:	// STRING op ACCOUNT => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							strncpy(stringify,IS_VALID(rsp->_.account) ? rsp->_.account->username : "null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_AFFECT:	// STRING op AFFECT => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.affect))
								strncpy(stringify,get_affect_name(rsp->_.affect),sizeof(stringify)-1);
							else
								strcpy(stringify,"null");
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_AREA:		// STRING op AREA => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							sprintf(stringify,"%s(%ld)",
								(rsp->_.area) ? (rsp->_.area)->name : "null",
								(rsp->_.area) ? (rsp->_.area)->uid : 0);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			// case NST_CHANNEL:	// STRING op CHANNEL => STRING
			case NST_CLASS:		// STRING op CLASS => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.clazz))
								strncpy(stringify,rsp->_.clazz->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_DUNGEON:	// STRING op DUNGEON => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.dungeon))
								strncpy(stringify,rsp->_.dungeon->index->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_EXIT:		// STRING op EXIT => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.ex))
							{
								if (rsp->_.ex->orig_door >= 0 && rsp->_.ex->orig_door < MAX_DIR)
									strncpy(stringify,dir_name[rsp->_.ex->orig_door],sizeof(stringify)-1);
								else
									strncpy(stringify,"???",sizeof(stringify)-1);
							}
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_INSTANCE:	// STRING op INSTANCE => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.instance))
								strncpy(stringify,rsp->_.instance->blueprint->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_LIQUID:	// STRING op LIQUID => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.liquid))
								strncpy(stringify,rsp->_.liquid->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_MAIL:		// STRING op MAIL => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (rsp->_.mail)
								snprintf(stringify,sizeof(stringify)-1,"<mailto:%s>",rsp->_.mail->recipient);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_MATERIAL:	// STRING op MATERIAL => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.material))
								strncpy(stringify,rsp->_.material->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_MISSION:	// STRING op MISSION => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (rsp->_.mission)
								snprintf(stringify,sizeof(stringify)-1,"<mission:%ld>",rsp->_.mission->timer);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_MOBILE:	// STRING op MOBILE => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							sprintf(stringify,"%s(%ld,%ld)",
								(rsp->_.mobile) ? (rsp->_.mobile)->short_descr : "null",
								(rsp->_.mobile && (rsp->_.mobile)->pIndexData) ? (rsp->_.mobile)->pIndexData->area->uid : 0,
								(rsp->_.mobile && (rsp->_.mobile)->pIndexData) ? (rsp->_.mobile)->pIndexData->vnum : 0);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_NOTE:		// STRING op NOTE => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.note))
								snprintf(stringify,sizeof(stringify)-1,"<note:%s>",rsp->_.note->to_list);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_OBJECT:	// STRING op OBJECT => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							sprintf(stringify,"%s(%ld,%ld)",
								(rsp->_.object) ? (rsp->_.object)->short_descr : "null",
								(rsp->_.object && (rsp->_.object)->pIndexData) ? (rsp->_.object)->pIndexData->area->uid : 0,
								(rsp->_.object && (rsp->_.object)->pIndexData) ? (rsp->_.object)->pIndexData->vnum : 0);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_ORG:		// STRING op ORG => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (rsp->_.org)
								strncpy(stringify,rsp->_.org->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			// case NST_QUEST:	// STRING op QUEST => STRING
			case NST_RACE:		// STRING op RACE => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.race))
								strncpy(stringify,rsp->_.race->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_RANK:		// STRING op RANK => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.rank))
								strncpy(stringify,rsp->_.rank->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_REPUTATION:// STRING op REPUTATION => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.reputation))
								strncpy(stringify,rsp->_.reputation->pIndexData->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_ROOM:		// STRING op ROOM => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (rsp->_.room)
							{
								if(rsp->_.room->source)
								{
									snprintf(stringify,sizeof(stringify)-1,"%s(%ld,%ld,%lu,%lu)",
										(rsp->_.room)->source->name,
										(rsp->_.room)->source->area->uid,
										(rsp->_.room)->source->vnum,
										(rsp->_.room)->id[0],
										(rsp->_.room)->id[1]);
								}
								else
								{
									snprintf(stringify,sizeof(stringify)-1,"%s(%ld,%ld,%ld,%ld)",
										(rsp->_.room)->name,
										(rsp->_.room)->area->uid,
										(rsp->_.room)->vnum);
								}
							}
							else
								strcpy(stringify, "null");
							sprintf(stringify,"%s(%ld,%ld)",
								(rsp->_.room) ? (rsp->_.room)->name : "null",
								(rsp->_.room) ? (rsp->_.room)->area->uid : 0,
								(rsp->_.room) ? (rsp->_.room)->vnum : 0);

							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_SHIP:		// STRING op SHIP => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.ship))
								strncpy(stringify,rsp->_.ship->index->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_SKILL:		// STRING op SKILL => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.skill))
								strncpy(stringify,rsp->_.skill->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_TOKEN:		// STRING op TOKEN => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							sprintf(stringify,"%s(%ld,%ld)",
								(rsp->_.token) ? (rsp->_.token)->name : "null",
								(rsp->_.token && (rsp->_.token)->pIndexData) ? (rsp->_.token)->pIndexData->area->uid : 0,
								(rsp->_.token && (rsp->_.token)->pIndexData) ? (rsp->_.token)->pIndexData->vnum : 0);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_WILDS:		// STRING op WILDS => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							sprintf(stringify,"%s(%ld,%ld)",
								(rsp->_.wilds) ? (rsp->_.wilds)->name : "null",
								(rsp->_.wilds) ? (rsp->_.wilds)->pArea->uid : 0,
								(rsp->_.wilds) ? (rsp->_.wilds)->uid : 0);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									free(lsp->_.str);
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								free(lsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			// case NST_WORLD:	// STRING op WORLD => STRING
			case NST_LVALUE:	// STRING op LVALUE => STRING
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:	// STRING op NUMBER => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char number[100];
									ltoa(*(rsp->_.lvalue._.number),number);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(number)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,number);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,number))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							case NI_MULT:		// Cloning
								{
									if (lsp->_.str)
									{
										register int cnt = *(rsp->_.lvalue._.number);
										register int len = strlen(lsp->_.str);
										char *value = calloc(1,len * ((cnt > 0)?cnt:0) + 1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}

										register char *str = value;
										for(int i = cnt; i-- > 0; str += len)
											strcpy(str, lsp->_.str);
										*str = 0;

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr, value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr, ""))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_FLOAT:		// STRING op FLOAT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%lf", *(rsp->_.lvalue._.d));
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if(lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_BOOLEAN:	// STRING op BOOLEAN => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char *stringify = ((*(rsp->_.lvalue._.b))?"true":"false");
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);
										
										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if(lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_FLAG_BIT:	// STRING op BIT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									bool set = IS_SET(*(rsp->_.lvalue._.bit.value),rsp->_.lvalue._.bit.bit);
									char *stringify = (set?"true":"false");
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_CHAR:		// STRING op CHAR => STRING
						{
							if (op != NI_ADD)
							{
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}

							if (lsp->_.str)
							{
								if (*(rsp->_.lvalue._.ch) > 0)
								{
									char *bytes = utf8_getbytes(*(rsp->_.lvalue._.ch));
									int blen = strlen(bytes);

									int len = strlen(lsp->_.str);
									char *value = calloc(1,len+blen+1);
									if (!value)
									{
										free(lsp->_.str);
										SETRET(nsr,MEMORY);
										return true;
									}
									strcpy(value,lsp->_.str);
									strcpy(value+len,bytes);
									value[len+blen] = '\0';

									free(lsp->_.str);

									if (!nib_push_stack_string_raw(nsr,value))
									{
										free(value);
										SETRET(nsr,STACK);
										return true;
									}
								}
								else if (!nib_push_stack_string_raw(nsr,lsp->_.str))
								{
									free(lsp->_.str);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else
							{
								char *bytes = utf8_getbytes(*(rsp->_.lvalue._.ch));

								if (!nib_push_stack_string(nsr, bytes))
								{
									SETRET(nsr,STACK);
									return true;
								}
							}
							break;
						}
					case NST_STRING:	// STRING op STRING => STRING
						{
							if (op != NI_ADD)
							{
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}

							if (lsp->_.str)
							{
								if (*(rsp->_.lvalue._.str))
								{
									char *value = calloc(1,strlen(lsp->_.str)+strlen(*(rsp->_.lvalue._.str))+1);
									if (!value)
									{
										free(lsp->_.str);
										SETRET(nsr,MEMORY);
										return true;
									}

									strcpy(value,lsp->_.str);
									strcat(value,*(rsp->_.lvalue._.str));

									free(lsp->_.str);

									if (!nib_push_stack_string_raw(nsr, value))
									{
										SETRET(nsr,STACK);
										return true;
									}
								}
								else if (!nib_push_stack_string_raw(nsr, lsp->_.str))
								{
									free(lsp->_.str);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (*(rsp->_.lvalue._.str))
							{
								if (!nib_push_stack_string(nsr, *(rsp->_.lvalue._.str)))
								{
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr, ""))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}
					case NST_WIDEVNUM:	// STRING op WIDEVNUM => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"(%ld,%ld)",
										(rsp->_.lvalue._.wnum->pArea?rsp->_.lvalue._.wnum->pArea->uid:0),
										rsp->_.lvalue._.wnum->vnum);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_FLAG:		// STRING op FLAG => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									const char *stringify = nib_get_flag_string(rsp->_.lvalue._.stat.table,*(rsp->_.lvalue._.stat.number));
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_STAT:		// STRING op STAT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									const char *stringify = nib_get_stat_string(rsp->_.lvalue._.stat.table,*(rsp->_.lvalue._.stat.number));
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_ACCOUNT:	// STRING op ACCOUNT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									strncpy(stringify,IS_VALID((*(rsp->_.lvalue._.account))) ? (*(rsp->_.lvalue._.account))->username : "null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_AFFECT:	// STRING op AFFECT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.affect))))
										strncpy(stringify,get_affect_name((*(rsp->_.lvalue._.affect))),sizeof(stringify)-1);
									else
										strcpy(stringify,"null");
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_AREA:		// STRING op AREA => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld)",
										(*(rsp->_.lvalue._.area)) ? (*(rsp->_.lvalue._.area))->name : "null",
										(*(rsp->_.lvalue._.area)) ? (*(rsp->_.lvalue._.area))->uid : 0);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					// case NST_CHANNEL:	// STRING op CHANNEL => STRING
					case NST_CLASS:		// STRING op CLASS => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.clazz))))
										strncpy(stringify,(*(rsp->_.lvalue._.clazz))->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_DUNGEON:	// STRING op DUNGEON => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.dungeon))))
										strncpy(stringify,(*(rsp->_.lvalue._.dungeon))->index->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_EXIT:		// STRING op EXIT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.ex))))
									{
										if ((*(rsp->_.lvalue._.ex))->orig_door >= 0 && (*(rsp->_.lvalue._.ex))->orig_door < MAX_DIR)
											strncpy(stringify,dir_name[(*(rsp->_.lvalue._.ex))->orig_door],sizeof(stringify)-1);
										else
											strncpy(stringify,"???",sizeof(stringify)-1);
									}
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_INSTANCE:	// STRING op INSTANCE => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.instance))))
										strncpy(stringify,(*(rsp->_.lvalue._.instance))->blueprint->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_LIQUID:	// STRING op LIQUID => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.liquid))))
										strncpy(stringify,(*(rsp->_.lvalue._.liquid))->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_MAIL:		// STRING op MAIL => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (*(rsp->_.lvalue._.mail))
										snprintf(stringify,sizeof(stringify)-1,"<mailto:%s>",(*(rsp->_.lvalue._.mail))->recipient);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_MATERIAL:	// STRING op MATERIAL => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.material))))
										strncpy(stringify,(*(rsp->_.lvalue._.material))->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_MISSION:	// STRING op MISSION => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (*(rsp->_.lvalue._.mission))
										snprintf(stringify,sizeof(stringify)-1,"<mission:%ld>",(*(rsp->_.lvalue._.mission))->timer);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_MOBILE:	// STRING op MOBILE => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									CHAR_DATA *mob = *(rsp->_.lvalue._.mobile);
									char stringify[100];
									sprintf(stringify,"%s(%ld,%ld)",
										(mob) ? (mob)->name : "null",
										(mob && (mob)->pIndexData) ? (mob)->pIndexData->area->uid : 0,
										(mob && (mob)->pIndexData) ? (mob)->pIndexData->vnum : 0);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_NOTE:		// STRING op NOTE => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.note))))
										snprintf(stringify,sizeof(stringify)-1,"<note:%s>",(*(rsp->_.lvalue._.note))->to_list);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_OBJECT:	// STRING op OBJECT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld,%ld)",
										((*(rsp->_.lvalue._.object))) ? ((*(rsp->_.lvalue._.object)))->short_descr : "null",
										((*(rsp->_.lvalue._.object)) && ((*(rsp->_.lvalue._.object)))->pIndexData) ? ((*(rsp->_.lvalue._.object)))->pIndexData->area->uid : 0,
										((*(rsp->_.lvalue._.object)) && ((*(rsp->_.lvalue._.object)))->pIndexData) ? ((*(rsp->_.lvalue._.object)))->pIndexData->vnum : 0);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_ORG:		// STRING op ORG => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (*(rsp->_.lvalue._.org))
										strncpy(stringify,(*(rsp->_.lvalue._.org))->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					// case NST_QUEST:	// STRING op QUEST => STRING
					case NST_RACE:		// STRING op RACE => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.race))))
										strncpy(stringify,(*(rsp->_.lvalue._.race))->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_RANK:		// STRING op RANK => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.rank))))
										strncpy(stringify,(*(rsp->_.lvalue._.rank))->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_REPUTATION:// STRING op REPUTATION => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.reputation))))
										strncpy(stringify,(*(rsp->_.lvalue._.reputation))->pIndexData->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_ROOM:		// STRING op ROOM => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									ROOM_INDEX_DATA *room = *(rsp->_.lvalue._.room);
									char stringify[100];
									sprintf(stringify,"%s(%ld,%ld)",
										(room) ? (room)->name : "null",
										(room) ? (room)->area->uid : 0,
										(room) ? (room)->vnum : 0);

									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_SHIP:		// STRING op SHIP => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.ship))))
										strncpy(stringify,(*(rsp->_.lvalue._.ship))->index->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_SKILL:		// STRING op SKILL => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.skill))))
										strncpy(stringify,(*(rsp->_.lvalue._.skill))->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_TOKEN:		// STRING op TOKEN => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld,%ld)",
										((*(rsp->_.lvalue._.token))) ? ((*(rsp->_.lvalue._.token)))->name : "null",
										((*(rsp->_.lvalue._.token)) && ((*(rsp->_.lvalue._.token)))->pIndexData) ? ((*(rsp->_.lvalue._.token)))->pIndexData->area->uid : 0,
										((*(rsp->_.lvalue._.token)) && ((*(rsp->_.lvalue._.token)))->pIndexData) ? ((*(rsp->_.lvalue._.token)))->pIndexData->vnum : 0);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_WILDS:		// STRING op WILDS => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld,%ld)",
										((*(rsp->_.lvalue._.wilds))) ? ((*(rsp->_.lvalue._.wilds)))->name : "null",
										((*(rsp->_.lvalue._.wilds))) ? ((*(rsp->_.lvalue._.wilds)))->pArea->uid : 0,
										((*(rsp->_.lvalue._.wilds))) ? ((*(rsp->_.lvalue._.wilds)))->uid : 0);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											free(lsp->_.str);
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										free(lsp->_.str);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					// case NST_WORLD:	// STRING op WORLD => STRING

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_STRING_S:		// STRING(s) op ???
		{
			switch(rsp->type)
			{
			case NST_NUMBER:	// STRING(s) op NUMBER => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char number[100];
							ltoa(rsp->_.i,number);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(number)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,number);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,number))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					case NI_MULT:		// Cloning
						{
							if (lsp->_.str)
							{
								int len = strlen(lsp->_.str);
								char *value = calloc(1,len * ((rsp->_.i > 0)?rsp->_.i:0) + 1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}

								char *str = value;
								for(int i = rsp->_.i; i-- > 0; str += len)
									strcpy(str, lsp->_.str);
								*str = 0;

								if (!nib_push_stack_string_raw(nsr, value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr, ""))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_FLOAT:		// STRING op FLOAT => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							sprintf(stringify,"%lf", rsp->_.d);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_BOOLEAN:	// STRING op BOOLEAN => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char *stringify = (rsp->_.b?"true":"false");
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_CHAR:		// STRING(s) op CHAR => STRING
				{
					if (op != NI_ADD)
					{
						SETRET(nsr,INVALID);
						return true;
					}

					if (lsp->_.str)
					{
						if (rsp->_.ch > 0)
						{
							char *bytes = utf8_getbytes(rsp->_.ch);
							int blen = strlen(bytes);

							int len = strlen(lsp->_.str);
							char *value = calloc(1,len+blen+1);
							if (!value)
							{
								SETRET(nsr,MEMORY);
								return true;
							}
							strcpy(value,lsp->_.str);
							strcpy(value+len,bytes);
							value[len+blen] = '\0';

							if (!nib_push_stack_string_raw(nsr,value))
							{
								free(value);
								SETRET(nsr,STACK);
								return true;
							}
						}
						else if (!nib_push_stack_string_raw(nsr,lsp->_.str))
						{
							SETRET(nsr,STACK);
							return true;
						}
					}
					else
					{
						char *bytes = utf8_getbytes(rsp->_.ch);

						if (!nib_push_stack_string(nsr, bytes))
						{
							SETRET(nsr,STACK);
							return true;
						}
					}
					break;
				}

			case NST_STRING:	// STRING(s) op STRING => STRING
				{
					if (op != NI_ADD)
					{
						SETRET(nsr,INVALID);
						return true;
					}

					if (lsp->_.str)
					{
						if (rsp->_.str)
						{
							char *value = calloc(1,strlen(lsp->_.str)+strlen(rsp->_.str)+1);
							if (!value)
							{
								free(rsp->_.str);
								SETRET(nsr,MEMORY);
								return true;
							}

							strcpy(value,lsp->_.str);
							strcat(value,rsp->_.str);

							free(rsp->_.str);

							if (!nib_push_stack_string_raw(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
						}
						else if (!nib_push_stack_string_raw(nsr, lsp->_.str))
						{
							SETRET(nsr,STACK);
							return true;
						}
					}
					else if (rsp->_.str)
					{
						if (!nib_push_stack_string_raw(nsr, rsp->_.str))
						{
							free(rsp->_.str);
							SETRET(nsr,STACK);
							return true;
						}
					}
					else if (!nib_push_stack_string(nsr, ""))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_STRING_S:	// STRING(s) op STRING(s) => STRING
				{
					if (op != NI_ADD)
					{
						SETRET(nsr,INVALID);
						return true;
					}

					if (lsp->_.str)
					{
						if (rsp->_.str)
						{
							char *value = calloc(1,strlen(lsp->_.str)+strlen(rsp->_.str)+1);
							if (!value)
							{
								SETRET(nsr,MEMORY);
								return true;
							}

							strcpy(value,lsp->_.str);
							strcat(value,rsp->_.str);

							if (!nib_push_stack_string_raw(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
						}
						else if (!nib_push_stack_string_raw(nsr, lsp->_.str))
						{
							SETRET(nsr,STACK);
							return true;
						}
					}
					else if (rsp->_.str)
					{
						if (!nib_push_stack_string(nsr, rsp->_.str))
						{
							SETRET(nsr,STACK);
							return true;
						}
					}
					else if (!nib_push_stack_string(nsr, ""))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_WIDEVNUM:	// STRING op WIDEVNUM => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							sprintf(stringify,"(%ld,%ld)",
								(rsp->_.wnum.pArea?rsp->_.wnum.pArea->uid:0),
								rsp->_.wnum.vnum);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_FLAG:		// STRING op FLAG => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							const char *stringify = nib_get_flag_string(rsp->_.stat.table,rsp->_.stat.number);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_STAT:		// STRING op STAT => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							const char *stringify = nib_get_stat_string(rsp->_.stat.table,rsp->_.stat.number);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_ACCOUNT:	// STRING op ACCOUNT => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							strncpy(stringify,IS_VALID(rsp->_.account) ? rsp->_.account->username : "null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_AFFECT:	// STRING op AFFECT => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.affect))
								strncpy(stringify,get_affect_name(rsp->_.affect),sizeof(stringify)-1);
							else
								strcpy(stringify,"null");
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_AREA:		// STRING op AREA => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							sprintf(stringify,"%s(%ld)",
								(rsp->_.area) ? (rsp->_.area)->name : "null",
								(rsp->_.area) ? (rsp->_.area)->uid : 0);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			// case NST_CHANNEL:	// STRING op CHANNEL => STRING
			case NST_CLASS:		// STRING op CLASS => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.clazz))
								strncpy(stringify,rsp->_.clazz->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_DUNGEON:	// STRING op DUNGEON => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.dungeon))
								strncpy(stringify,rsp->_.dungeon->index->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_EXIT:		// STRING op EXIT => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.ex))
							{
								if (rsp->_.ex->orig_door >= 0 && rsp->_.ex->orig_door < MAX_DIR)
									strncpy(stringify,dir_name[rsp->_.ex->orig_door],sizeof(stringify)-1);
								else
									strncpy(stringify,"???",sizeof(stringify)-1);
							}
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_INSTANCE:	// STRING op INSTANCE => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.instance))
								strncpy(stringify,rsp->_.instance->blueprint->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_LIQUID:	// STRING op LIQUID => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.liquid))
								strncpy(stringify,rsp->_.liquid->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_MAIL:		// STRING op MAIL => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (rsp->_.mail)
								snprintf(stringify,sizeof(stringify)-1,"<mailto:%s>",rsp->_.mail->recipient);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_MATERIAL:	// STRING op MATERIAL => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.material))
								strncpy(stringify,rsp->_.material->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_MISSION:	// STRING op MISSION => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (rsp->_.mission)
								snprintf(stringify,sizeof(stringify)-1,"<mission:%ld>",rsp->_.mission->timer);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_MOBILE:	// STRING op MOBILE => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							sprintf(stringify,"%s(%ld,%ld)",
								(rsp->_.mobile) ? (rsp->_.mobile)->short_descr : "null",
								(rsp->_.mobile && (rsp->_.mobile)->pIndexData) ? (rsp->_.mobile)->pIndexData->area->uid : 0,
								(rsp->_.mobile && (rsp->_.mobile)->pIndexData) ? (rsp->_.mobile)->pIndexData->vnum : 0);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_NOTE:		// STRING op NOTE => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.note))
								snprintf(stringify,sizeof(stringify)-1,"<note:%s>",rsp->_.note->to_list);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_OBJECT:	// STRING op OBJECT => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							sprintf(stringify,"%s(%ld,%ld)",
								(rsp->_.object) ? (rsp->_.object)->short_descr : "null",
								(rsp->_.object && (rsp->_.object)->pIndexData) ? (rsp->_.object)->pIndexData->area->uid : 0,
								(rsp->_.object && (rsp->_.object)->pIndexData) ? (rsp->_.object)->pIndexData->vnum : 0);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_ORG:		// STRING op ORG => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (rsp->_.org)
								strncpy(stringify,rsp->_.org->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			// case NST_QUEST:	// STRING op QUEST => STRING
			case NST_RACE:		// STRING op RACE => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.race))
								strncpy(stringify,rsp->_.race->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_RANK:		// STRING op RANK => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.rank))
								strncpy(stringify,rsp->_.rank->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_REPUTATION:// STRING op REPUTATION => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.reputation))
								strncpy(stringify,rsp->_.reputation->pIndexData->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_ROOM:		// STRING op ROOM => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (rsp->_.room)
							{
								if(rsp->_.room->source)
								{
									snprintf(stringify,sizeof(stringify)-1,"%s(%ld,%ld,%lu,%lu)",
										(rsp->_.room)->source->name,
										(rsp->_.room)->source->area->uid,
										(rsp->_.room)->source->vnum,
										(rsp->_.room)->id[0],
										(rsp->_.room)->id[1]);
								}
								else
								{
									snprintf(stringify,sizeof(stringify)-1,"%s(%ld,%ld,%ld,%ld)",
										(rsp->_.room)->name,
										(rsp->_.room)->area->uid,
										(rsp->_.room)->vnum);
								}
							}
							else
								strcpy(stringify, "null");
							sprintf(stringify,"%s(%ld,%ld)",
								(rsp->_.room) ? (rsp->_.room)->name : "null",
								(rsp->_.room) ? (rsp->_.room)->area->uid : 0,
								(rsp->_.room) ? (rsp->_.room)->vnum : 0);

							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_SHIP:		// STRING op SHIP => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.ship))
								strncpy(stringify,rsp->_.ship->index->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_SKILL:		// STRING op SKILL => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							if (IS_VALID(rsp->_.skill))
								strncpy(stringify,rsp->_.skill->name,sizeof(stringify)-1);
							else
								strncpy(stringify,"null",sizeof(stringify)-1);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_TOKEN:		// STRING op TOKEN => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							sprintf(stringify,"%s(%ld,%ld)",
								(rsp->_.token) ? (rsp->_.token)->name : "null",
								(rsp->_.token && (rsp->_.token)->pIndexData) ? (rsp->_.token)->pIndexData->area->uid : 0,
								(rsp->_.token && (rsp->_.token)->pIndexData) ? (rsp->_.token)->pIndexData->vnum : 0);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_WILDS:		// STRING op WILDS => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							sprintf(stringify,"%s(%ld,%ld)",
								(rsp->_.wilds) ? (rsp->_.wilds)->name : "null",
								(rsp->_.wilds) ? (rsp->_.wilds)->pArea->uid : 0,
								(rsp->_.wilds) ? (rsp->_.wilds)->uid : 0);
							if (lsp->_.str)
							{
								char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}
								strcpy(value,lsp->_.str);
								strcat(value,stringify);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr,stringify))
							{
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			// case NST_WORLD:	// STRING op WORLD => STRING
			case NST_LVALUE:	// STRING(s) op LVALUE => STRING
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:	// STRING(s) op NUMBER => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char number[100];
									ltoa(*(rsp->_.lvalue._.number),number);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(number)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,number);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,number))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							case NI_MULT:		// Cloning
								{
									if (lsp->_.str)
									{
										register int cnt = *(rsp->_.lvalue._.number);
										register int len = strlen(lsp->_.str);
										char *value = calloc(1,len * ((cnt > 0)?cnt:0) + 1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}

										register char *str = value;
										for(int i = cnt; i-- > 0; str += len)
											strcpy(str, lsp->_.str);
										*str = 0;

										if (!nib_push_stack_string_raw(nsr, value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr, ""))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_FLOAT:		// STRING op FLOAT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%lf", *(rsp->_.lvalue._.d));
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_BOOLEAN:	// STRING op BOOLEAN => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char *stringify = ((*(rsp->_.lvalue._.b))?"true":"false");
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_FLAG_BIT:	// STRING op BIT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									bool set = IS_SET(*(rsp->_.lvalue._.bit.value),rsp->_.lvalue._.bit.bit);
									char *stringify = (set?"true":"false");
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_CHAR:		// STRING(s) op CHAR => STRING
						{
							if (op != NI_ADD)
							{
								SETRET(nsr,INVALID);
								return true;
							}

							if (lsp->_.str)
							{
								if (*(rsp->_.lvalue._.ch) > 0)
								{
									char *bytes = utf8_getbytes(*(rsp->_.lvalue._.ch));
									int blen = strlen(bytes);

									int len = strlen(lsp->_.str);
									char *value = calloc(1,len+blen+1);
									if (!value)
									{
										SETRET(nsr,MEMORY);
										return true;
									}
									strcpy(value,lsp->_.str);
									strcpy(value+len,bytes);
									value[len+blen] = '\0';

									if (!nib_push_stack_string_raw(nsr,value))
									{
										free(value);
										SETRET(nsr,STACK);
										// SETRETN(nsr,__LINE__);
										return true;
									}
								}
								else if (!nib_push_stack_string_raw(nsr,lsp->_.str))
								{
									SETRET(nsr,STACK);
									// SETRETN(nsr,__LINE__);
									return true;
								}
							}
							else
							{
								char *bytes = utf8_getbytes(*(rsp->_.lvalue._.ch));

								if (!nib_push_stack_string(nsr, bytes))
								{
									SETRET(nsr,STACK);
									// SETRETN(nsr,__LINE__);
									return true;
								}
							}
							break;
						}
					case NST_STRING:	// STRING(s) op STRING => STRING
						{
							if (op != NI_ADD)
							{
								SETRET(nsr,INVALID);
								return true;
							}

							if (lsp->_.str)
							{
								if (*(rsp->_.lvalue._.str))
								{
									char *value = calloc(1,strlen(lsp->_.str)+strlen(*(rsp->_.lvalue._.str))+1);
									if (!value)
									{
										SETRET(nsr,MEMORY);
										return true;
									}

									strcpy(value,lsp->_.str);
									strcat(value,*(rsp->_.lvalue._.str));

									if (!nib_push_stack_string_raw(nsr, value))
									{
										SETRET(nsr,STACK);
										return true;
									}
								}
								else if (!nib_push_stack_string_raw(nsr, lsp->_.str))
								{
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (*(rsp->_.lvalue._.str))
							{
								if (!nib_push_stack_string(nsr, *(rsp->_.lvalue._.str)))
								{
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr, ""))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}
					case NST_WIDEVNUM:	// STRING op WIDEVNUM => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"(%ld,%ld)",
										(rsp->_.lvalue._.wnum->pArea?rsp->_.lvalue._.wnum->pArea->uid:0),
										rsp->_.lvalue._.wnum->vnum);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_FLAG:		// STRING op FLAG => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									const char *stringify = nib_get_flag_string(rsp->_.lvalue._.stat.table,*(rsp->_.lvalue._.stat.number));
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_STAT:		// STRING op STAT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									const char *stringify = nib_get_stat_string(rsp->_.lvalue._.stat.table,*(rsp->_.lvalue._.stat.number));
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_ACCOUNT:	// STRING op ACCOUNT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									strncpy(stringify,IS_VALID((*(rsp->_.lvalue._.account))) ? (*(rsp->_.lvalue._.account))->username : "null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_AFFECT:	// STRING op AFFECT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.affect))))
										strncpy(stringify,get_affect_name((*(rsp->_.lvalue._.affect))),sizeof(stringify)-1);
									else
										strcpy(stringify,"null");
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_AREA:		// STRING op AREA => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld)",
										((*(rsp->_.lvalue._.area))) ? ((*(rsp->_.lvalue._.area)))->name : "null",
										((*(rsp->_.lvalue._.area))) ? ((*(rsp->_.lvalue._.area)))->uid : 0);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					// case NST_CHANNEL:	// STRING op CHANNEL => STRING
					case NST_CLASS:		// STRING op CLASS => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.clazz))))
										strncpy(stringify,(*(rsp->_.lvalue._.clazz))->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_DUNGEON:	// STRING op DUNGEON => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.dungeon))))
										strncpy(stringify,(*(rsp->_.lvalue._.dungeon))->index->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_EXIT:		// STRING op EXIT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.ex))))
									{
										if ((*(rsp->_.lvalue._.ex))->orig_door >= 0 && (*(rsp->_.lvalue._.ex))->orig_door < MAX_DIR)
											strncpy(stringify,dir_name[(*(rsp->_.lvalue._.ex))->orig_door],sizeof(stringify)-1);
										else
											strncpy(stringify,"???",sizeof(stringify)-1);
									}
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_INSTANCE:	// STRING op INSTANCE => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.instance))))
										strncpy(stringify,(*(rsp->_.lvalue._.instance))->blueprint->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_LIQUID:	// STRING op LIQUID => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.liquid))))
										strncpy(stringify,(*(rsp->_.lvalue._.liquid))->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_MAIL:		// STRING op MAIL => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if ((*(rsp->_.lvalue._.mail)))
										snprintf(stringify,sizeof(stringify)-1,"<mailto:%s>",(*(rsp->_.lvalue._.mail))->recipient);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_MATERIAL:	// STRING op MATERIAL => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.material))))
										strncpy(stringify,(*(rsp->_.lvalue._.material))->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_MISSION:	// STRING op MISSION => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if ((*(rsp->_.lvalue._.mission)))
										snprintf(stringify,sizeof(stringify)-1,"<mission:%ld>",(*(rsp->_.lvalue._.mission))->timer);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_MOBILE:	// STRING op MOBILE => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld,%ld)",
										((*(rsp->_.lvalue._.mobile))) ? ((*(rsp->_.lvalue._.mobile)))->short_descr : "null",
										((*(rsp->_.lvalue._.mobile)) && ((*(rsp->_.lvalue._.mobile)))->pIndexData) ? ((*(rsp->_.lvalue._.mobile)))->pIndexData->area->uid : 0,
										((*(rsp->_.lvalue._.mobile)) && ((*(rsp->_.lvalue._.mobile)))->pIndexData) ? ((*(rsp->_.lvalue._.mobile)))->pIndexData->vnum : 0);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_NOTE:		// STRING op NOTE => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.note))))
										snprintf(stringify,sizeof(stringify)-1,"<note:%s>",(*(rsp->_.lvalue._.note))->to_list);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_OBJECT:	// STRING op OBJECT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld,%ld)",
										((*(rsp->_.lvalue._.object))) ? ((*(rsp->_.lvalue._.object)))->short_descr : "null",
										((*(rsp->_.lvalue._.object)) && ((*(rsp->_.lvalue._.object)))->pIndexData) ? ((*(rsp->_.lvalue._.object)))->pIndexData->area->uid : 0,
										((*(rsp->_.lvalue._.object)) && ((*(rsp->_.lvalue._.object)))->pIndexData) ? ((*(rsp->_.lvalue._.object)))->pIndexData->vnum : 0);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_ORG:		// STRING op ORG => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if ((*(rsp->_.lvalue._.org)))
										strncpy(stringify,(*(rsp->_.lvalue._.org))->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					// case NST_QUEST:	// STRING op QUEST => STRING
					case NST_RACE:		// STRING op RACE => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.race))))
										strncpy(stringify,(*(rsp->_.lvalue._.race))->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_RANK:		// STRING op RANK => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.rank))))
										strncpy(stringify,(*(rsp->_.lvalue._.rank))->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_REPUTATION:// STRING op REPUTATION => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.reputation))))
										strncpy(stringify,(*(rsp->_.lvalue._.reputation))->pIndexData->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_ROOM:		// STRING op ROOM => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if ((*(rsp->_.lvalue._.room)))
									{
										if((*(rsp->_.lvalue._.room))->source)
										{
											snprintf(stringify,sizeof(stringify)-1,"%s(%ld,%ld,%lu,%lu)",
												((*(rsp->_.lvalue._.room)))->source->name,
												((*(rsp->_.lvalue._.room)))->source->area->uid,
												((*(rsp->_.lvalue._.room)))->source->vnum,
												((*(rsp->_.lvalue._.room)))->id[0],
												((*(rsp->_.lvalue._.room)))->id[1]);
										}
										else
										{
											snprintf(stringify,sizeof(stringify)-1,"%s(%ld,%ld,%ld,%ld)",
												((*(rsp->_.lvalue._.room)))->name,
												((*(rsp->_.lvalue._.room)))->area->uid,
												((*(rsp->_.lvalue._.room)))->vnum);
										}
									}
									else
										strcpy(stringify, "null");
									sprintf(stringify,"%s(%ld,%ld)",
										((*(rsp->_.lvalue._.room))) ? ((*(rsp->_.lvalue._.room)))->name : "null",
										((*(rsp->_.lvalue._.room))) ? ((*(rsp->_.lvalue._.room)))->area->uid : 0,
										((*(rsp->_.lvalue._.room))) ? ((*(rsp->_.lvalue._.room)))->vnum : 0);

									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_SHIP:		// STRING op SHIP => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.ship))))
										strncpy(stringify,(*(rsp->_.lvalue._.ship))->index->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_SKILL:		// STRING op SKILL => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID((*(rsp->_.lvalue._.skill))))
										strncpy(stringify,(*(rsp->_.lvalue._.skill))->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_TOKEN:		// STRING op TOKEN => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld,%ld)",
										((*(rsp->_.lvalue._.token))) ? ((*(rsp->_.lvalue._.token)))->name : "null",
										((*(rsp->_.lvalue._.token)) && ((*(rsp->_.lvalue._.token)))->pIndexData) ? ((*(rsp->_.lvalue._.token)))->pIndexData->area->uid : 0,
										((*(rsp->_.lvalue._.token)) && ((*(rsp->_.lvalue._.token)))->pIndexData) ? ((*(rsp->_.lvalue._.token)))->pIndexData->vnum : 0);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_WILDS:		// STRING op WILDS => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld,%ld)",
										((*(rsp->_.lvalue._.wilds))) ? ((*(rsp->_.lvalue._.wilds)))->name : "null",
										((*(rsp->_.lvalue._.wilds))) ? ((*(rsp->_.lvalue._.wilds)))->pArea->uid : 0,
										((*(rsp->_.lvalue._.wilds))) ? ((*(rsp->_.lvalue._.wilds)))->uid : 0);
									if (lsp->_.str)
									{
										char *value = calloc(1,strlen(lsp->_.str)+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,lsp->_.str);
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					// case NST_WORLD:	// STRING op WORLD => STRING
					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_FLAG:			// FLAG op ???
		{
			switch(rsp->type)
			{
			case NST_NUMBER:
				{
					long value;
					switch(op)
					{
					case NI_BAND:	value = (lsp->_.stat.number) & (rsp->_.i); break;
					case NI_BOR:	value = (lsp->_.stat.number) | (rsp->_.i); break;
					case NI_BXOR:	value = (lsp->_.stat.number) ^ (rsp->_.i); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_flag(nsr,value,lsp->_.stat.table))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}
			
			case NST_FLAG:
				{
					if (lsp->_.stat.table != rsp->_.stat.table)
					{
						SETRET(nsr,INVALID);
						return true;
					}

					long value;
					switch(op)
					{
					case NI_BAND:	value = (lsp->_.stat.number) & (rsp->_.stat.number); break;
					case NI_BOR:	value = (lsp->_.stat.number) | (rsp->_.stat.number); break;
					case NI_BXOR:	value = (lsp->_.stat.number) ^ (rsp->_.stat.number); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_flag(nsr,value,lsp->_.stat.table))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}
			
			case NST_LVALUE:
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:
						{
							long value;
							switch(op)
							{
							case NI_BAND:	value = (lsp->_.stat.number) & *(rsp->_.lvalue._.number); break;
							case NI_BOR:	value = (lsp->_.stat.number) | *(rsp->_.lvalue._.number); break;
							case NI_BXOR:	value = (lsp->_.stat.number) ^ *(rsp->_.lvalue._.number); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_flag(nsr,value,lsp->_.stat.table))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_FLAG:
						{
							if (lsp->_.stat.table != rsp->_.lvalue._.stat.table)
							{
								SETRET(nsr,INVALID);
								return true;
							}
							long value;
							switch(op)
							{
							case NI_BAND:	value = (lsp->_.stat.number) & *(rsp->_.lvalue._.stat.number); break;
							case NI_BOR:	value = (lsp->_.stat.number) | *(rsp->_.lvalue._.stat.number); break;
							case NI_BXOR:	value = (lsp->_.stat.number) ^ *(rsp->_.lvalue._.stat.number); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_flag(nsr,value,lsp->_.stat.table))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_LVALUE:		// LVALUE op ???
		{
			switch(lsp->_.lvalue.type)
			{
			case NST_NUMBER:		// NUMBER op ???
				{
					switch(rsp->type)
					{
					case NST_NUMBER:	// NUMBER op NUMBER => NUMBER
						{
							long value;
							switch(op)
							{
							case NI_ADD:	value = *(lsp->_.lvalue._.number) + rsp->_.i; break;
							case NI_SUBT:	value = *(lsp->_.lvalue._.number) - rsp->_.i; break;
							case NI_MULT:	value = *(lsp->_.lvalue._.number) * rsp->_.i; break;
							case NI_MOD:
								if (rsp->_.i == 0)
								{
									SETRET(nsr,MATH);
									return true;
								}

								value = *(lsp->_.lvalue._.number) % rsp->_.i;
								break;
							
							case NI_DIV:
								if (rsp->_.i == 0)
								{
									SETRET(nsr,MATH);
									return true;
								}

								value = *(lsp->_.lvalue._.number) / rsp->_.i;
								break;

							case NI_BAND:	value = (*(lsp->_.lvalue._.number) & rsp->_.i); break;
							case NI_BOR:	value = (*(lsp->_.lvalue._.number) | rsp->_.i); break;
							case NI_BXOR:	value = (*(lsp->_.lvalue._.number) ^ rsp->_.i); break;
							case NI_LSH:	value = (*(lsp->_.lvalue._.number) << rsp->_.i); break;
							case NI_RSH:	value = (*(lsp->_.lvalue._.number) >> rsp->_.i); break;
							case NI_RSHL:	value = (long)(((unsigned long)*(lsp->_.lvalue._.number)) >> rsp->_.i); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_number(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_FLOAT:		// NUMBER op FLOAT => FLOAT
						{
							double value;
							switch(op)
							{
							case NI_ADD:	value = (double)*(lsp->_.lvalue._.number) + rsp->_.d; break;
							case NI_SUBT:	value = (double)*(lsp->_.lvalue._.number) - rsp->_.d; break;
							case NI_MULT:	value = (double)*(lsp->_.lvalue._.number) * rsp->_.d; break;
							case NI_DIV:
								if (rsp->_.d == 0.0)
								{
									SETRET(nsr,MATH);
									return true;
								}

								value = (double)*(lsp->_.lvalue._.number) / rsp->_.d;
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_float(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_CHAR:		// NUMBER op CHAR => STRING
						{
							if (op != NI_MULT)
							{
								SETRET(nsr,INVALID);
								return true;
							}

							// Cloning
							char *bytes = utf8_getbytes(*(rsp->_.lvalue._.ch));
							int len = strlen(bytes);
							int cnt = (*(lsp->_.lvalue._.number)>0)?*(lsp->_.lvalue._.number):0;
							char *value = calloc(1,len*cnt+1);
							if (!value)
							{
								SETRET(nsr,MEMORY);
								return true;
							}
							char *s = value;
							for(int i = cnt; i-- > 0; s+=len)
								strcpy(s,bytes);
							*s = '\0';

							if (!nib_push_stack_string_raw(nsr,value))
							{
								free(value);
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}
					case NST_STRING:	// NUMBER op STRING => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char number[100];
									ltoa(*(lsp->_.lvalue._.number), number);
									if (rsp->_.str)
									{
										char *value = calloc(1,strlen(number)+strlen(rsp->_.str)+1);
										strcpy(value,number);
										strcat(value,rsp->_.str);
										free(rsp->_.str);

										if(!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return false;
										}
									}
									else if(!nib_push_stack_string(nsr,number))
									{
										SETRET(nsr,STACK);
										return true;
									}
										
									break;
								}

							case NI_MULT:		// Cloning
								{
									if (rsp->_.str)
									{
										int len = strlen(rsp->_.str);
										int cnt = (*(lsp->_.lvalue._.number)>0)?*(lsp->_.lvalue._.number):0;
										char *value = calloc(1,cnt * len + 1);
										char *str = value;
										for(int i = 0; i < cnt; i++, str += len)
											strcpy(str,rsp->_.str);
										*str = '\0';
										free(rsp->_.str);

										if(!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return false;
										}
									}
									else if(!nib_push_stack_string(nsr, ""))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								if (rsp->_.str) free(rsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_STRING_S:	// NUMBER op STRING(s) => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char number[100];
									ltoa(*(lsp->_.lvalue._.number), number);
									if (rsp->_.str)
									{
										char *value = calloc(1,strlen(number)+strlen(rsp->_.str)+1);
										strcpy(value,number);
										strcat(value,rsp->_.str);

										if(!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return false;
										}
									}
									else if(!nib_push_stack_string(nsr,number))
									{
										SETRET(nsr,STACK);
										return true;
									}
										
									break;
								}

							case NI_MULT:		// Cloning
								{
									if (rsp->_.str)
									{
										int len = strlen(rsp->_.str);
										int cnt = (*(lsp->_.lvalue._.number)>0)?*(lsp->_.lvalue._.number):0;
										char *value = calloc(1,cnt * len + 1);
										char *str = value;
										for(int i = 0; i < cnt; i++, str += len)
											strcpy(str,rsp->_.str);
										*str = '\0';

										if(!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return false;
										}
									}
									else if(!nib_push_stack_string(nsr, ""))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_FLAG:		// NUMBER op FLAG => FLAG
						{
							long value;
							switch(op)
							{
							case NI_BAND:	value = *(lsp->_.lvalue._.number) & rsp->_.stat.number; break;
							case NI_BOR:	value = *(lsp->_.lvalue._.number) | rsp->_.stat.number; break;
							case NI_BXOR:	value = *(lsp->_.lvalue._.number) ^ rsp->_.stat.number; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_flag(nsr, value, rsp->_.stat.table))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}
					case NST_LVALUE:	// NUMBER op LVALUE
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_NUMBER:	// NUMBER op NUMBER
								{
									long value;
									switch(op)
									{
									case NI_ADD:	value = *(lsp->_.lvalue._.number) + *(rsp->_.lvalue._.number); break;
									case NI_SUBT:	value = *(lsp->_.lvalue._.number) - *(rsp->_.lvalue._.number); break;
									case NI_MULT:	value = *(lsp->_.lvalue._.number) * *(rsp->_.lvalue._.number); break;
									case NI_MOD:
										if (*(rsp->_.lvalue._.number) == 0)
										{
											SETRET(nsr,MATH);
											return true;
										}

										value = *(lsp->_.lvalue._.number) % *(rsp->_.lvalue._.number);
										break;
									
									case NI_DIV:
										if (*(rsp->_.lvalue._.number) == 0)
										{
											SETRET(nsr,MATH);
											return true;
										}

										value = *(lsp->_.lvalue._.number) / *(rsp->_.lvalue._.number);
										break;

									case NI_BAND:		value = (*(lsp->_.lvalue._.number) & *(rsp->_.lvalue._.number)); break;
									case NI_BOR:		value = (*(lsp->_.lvalue._.number) | *(rsp->_.lvalue._.number)); break;
									case NI_BXOR:		value = (*(lsp->_.lvalue._.number) ^ *(rsp->_.lvalue._.number)); break;

									case NI_LSH:		value = (*(lsp->_.lvalue._.number) << *(rsp->_.lvalue._.number)); break;
									case NI_RSH:		value = (*(lsp->_.lvalue._.number) >> *(rsp->_.lvalue._.number)); break;
									case NI_RSHL:		value = (long)(((unsigned long)*(lsp->_.lvalue._.number)) >> *(rsp->_.lvalue._.number)); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									break;
								}
							case NST_FLOAT:		// NUMBER op FLOAT => FLOAT
								{
									double value;
									switch(op)
									{
									case NI_ADD:	value = (double)*(lsp->_.lvalue._.number) + *(rsp->_.lvalue._.d); break;
									case NI_SUBT:	value = (double)*(lsp->_.lvalue._.number) - *(rsp->_.lvalue._.d); break;
									case NI_MULT:	value = (double)*(lsp->_.lvalue._.number) * *(rsp->_.lvalue._.d); break;
									case NI_DIV:
										if (*(rsp->_.lvalue._.d) == 0.0)
										{
											SETRET(nsr,MATH);
											return true;
										}

										value = (double)*(lsp->_.lvalue._.number) / *(rsp->_.lvalue._.d);
										break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_float(nsr, value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_CHAR:		// NUMBER op CHAR => STRING
								{
									if (op != NI_MULT)
									{
										SETRET(nsr,INVALID);
										return true;
									}

									// Cloning
									char *bytes = utf8_getbytes(*(rsp->_.lvalue._.ch));
									int len = strlen(bytes);
									int cnt = (*(lsp->_.lvalue._.number)>0)?*(lsp->_.lvalue._.number):0;
									char *value = calloc(1,len*cnt+1);
									if (!value)
									{
										SETRET(nsr,MEMORY);
										return true;
									}
									char *s = value;
									for(int i = cnt; i-- > 0;s+=len)
										strcpy(s,bytes);
									*s = '\0';

									if (!nib_push_stack_string_raw(nsr,value))
									{
										free(value);
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}
							case NST_STRING:	// NUMBER op STRING => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char number[100];
											ltoa(*(lsp->_.lvalue._.number), number);
											if (*(rsp->_.lvalue._.str))
											{
												char *value = calloc(1,strlen(number)+strlen(rsp->_.str)+1);
												strcpy(value,number);
												strcat(value,*(rsp->_.lvalue._.str));

												if(!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return false;
												}
											}
											else if(!nib_push_stack_string(nsr,number))
											{
												SETRET(nsr,STACK);
												return true;
											}
												
											break;
										}

									case NI_MULT:		// Cloning
										{
											if (*(rsp->_.lvalue._.str))
											{
												int len = strlen(*(rsp->_.lvalue._.str));
												int cnt = (*(lsp->_.lvalue._.number)>0)?*(lsp->_.lvalue._.number):0;
												char *value = calloc(1,cnt * len + 1);
												char *str = value;
												for(int i = 0; i < cnt; i++, str += len)
													strcpy(str,*(rsp->_.lvalue._.str));
												*str = '\0';

												if(!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return false;
												}
											}
											else if(!nib_push_stack_string(nsr, ""))
											{
												SETRET(nsr,STACK);
												return true;
											}
											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}

							case NST_FLAG:		// NUMBER op FLAG => FLAG
								{
									long value;
									switch(op)
									{
									case NI_BAND:	value = *(lsp->_.lvalue._.number) & *(rsp->_.lvalue._.stat.number); break;
									case NI_BOR:	value = *(lsp->_.lvalue._.number) | *(rsp->_.lvalue._.stat.number); break;
									case NI_BXOR:	value = *(lsp->_.lvalue._.number) ^ *(rsp->_.lvalue._.stat.number); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_flag(nsr, value, rsp->_.lvalue._.stat.table))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}
							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
				
			case NST_FLOAT:			// FLOAT op ???
				{
					switch(rsp->type)
					{
					case NST_NUMBER:	// FLOAT op NUMBER => FLOAT
						{
							double value;
							switch(op)
							{
							case NI_ADD:	value = *(lsp->_.lvalue._.d) + (double)rsp->_.i; break;
							case NI_SUBT:	value = *(lsp->_.lvalue._.d) - (double)rsp->_.i; break;
							case NI_MULT:	value = *(lsp->_.lvalue._.d) * (double)rsp->_.i; break;
							case NI_DIV:
								if (rsp->_.i == 0)
								{
									SETRET(nsr,MATH);
									return true;
								}

								value = *(lsp->_.lvalue._.d) / (double)rsp->_.i;
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_float(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_FLOAT:		// FLOAT op FLOAT => FLOAT
						{
							double value;
							switch(op)
							{
							case NI_ADD:	value = *(lsp->_.lvalue._.d) + rsp->_.d; break;
							case NI_SUBT:	value = *(lsp->_.lvalue._.d) - rsp->_.d; break;
							case NI_MULT:	value = *(lsp->_.lvalue._.d) * rsp->_.d; break;
							case NI_DIV:
								if (rsp->_.d == 0.0)
								{
									SETRET(nsr,MATH);
									return true;
								}

								value = *(lsp->_.lvalue._.d) / rsp->_.d;
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_float(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// FLOAT op LVALUE
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_NUMBER:	// FLOAT op NUMBER => FLOAT
								{
									double value;
									switch(op)
									{
									case NI_ADD:	value = *(lsp->_.lvalue._.d) + (double)*(rsp->_.lvalue._.number); break;
									case NI_SUBT:	value = *(lsp->_.lvalue._.d) - (double)*(rsp->_.lvalue._.number); break;
									case NI_MULT:	value = *(lsp->_.lvalue._.d) * (double)*(rsp->_.lvalue._.number); break;
									case NI_DIV:
										if (*(rsp->_.lvalue._.number) == 0)
										{
											SETRET(nsr,MATH);
											return true;
										}

										value = *(lsp->_.lvalue._.d) / (double)*(rsp->_.lvalue._.number);
										break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_float(nsr, value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}


							case NST_FLOAT:		// FLOAT op FLOAT => FLOAT
								{
									double value;
									switch(op)
									{
									case NI_ADD:	value = *(lsp->_.lvalue._.d) + *(rsp->_.lvalue._.d); break;
									case NI_SUBT:	value = *(lsp->_.lvalue._.d) - *(rsp->_.lvalue._.d); break;
									case NI_MULT:	value = *(lsp->_.lvalue._.d) * *(rsp->_.lvalue._.d); break;
									case NI_DIV:
										if (*(rsp->_.lvalue._.d) == 0.0)
										{
											SETRET(nsr,MATH);
											return true;
										}

										value = *(lsp->_.lvalue._.d) / *(rsp->_.lvalue._.d);
										break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_float(nsr, value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_CHAR:			// CHAR op ???
				{
					switch(rsp->type)
					{
					case NST_NUMBER:		// CHAR op NUMBER => STRING
						{
							if (op != NI_MULT)
							{
								SETRET(nsr,INVALID);
								return true;
							}

							// Cloning
							char *bytes = utf8_getbytes(*(lsp->_.lvalue._.ch));
							int len = strlen(bytes);
							int cnt = (rsp->_.i>0)?rsp->_.i:0;
							char *value = calloc(1,len*cnt+1);
							if (!value)
							{
								SETRET(nsr,MEMORY);
								return true;
							}
							char *s = value;
							for(int i = cnt; i-- > 0;s+=len)
								strcpy(s,bytes);
							*s = '\0';

							if (!nib_push_stack_string_raw(nsr,value))
							{
								free(value);
								SETRET(nsr,STACK);
								return true;
							}

							break;
						}

					case NST_CHAR:			// CHAR op CHAR => STRING
						{
							if (op != NI_ADD)
							{
								SETRET(nsr,INVALID);
								return true;
							}

							char *lbytes = utf8_getbytes(*(lsp->_.lvalue._.ch));
							char *rbytes = utf8_getbytes(rsp->_.ch);
							int llen = strlen(lbytes);
							int rlen = strlen(rbytes);
							
							char *value = calloc(1,llen+rlen+1);
							if (!value)
							{
								SETRET(nsr,MEMORY);
								return true;
							}

							if (!nib_push_stack_string_raw(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_STRING:		// CHAR op STRING => STRING
						{
							if (op != NI_ADD)
							{
								if (rsp->_.str) free(rsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}

							if (*(lsp->_.lvalue._.ch) > 0)
							{
								char *bytes = utf8_getbytes(*(lsp->_.lvalue._.ch));
								int len = strlen(bytes);

								if (rsp->_.str)
								{
									char *value = calloc(1,strlen(rsp->_.str)+len+1);
									if (!value)
									{
										free(rsp->_.str);
										SETRET(nsr,MEMORY);
										return true;
									}

									strcpy(value,bytes);
									strcpy(value+len,rsp->_.str);
									free(rsp->_.str);

									if (!nib_push_stack_string_raw(nsr,value))
									{
										free(value);
										SETRET(nsr,STACK);
										return true;
									}
								}
								else
								{
									if (!nib_push_stack_string(nsr,bytes))
									{
										SETRET(nsr,STACK);
										return true;
									}
								}
							}
							else
							{
								if (rsp->_.str) free(rsp->_.str);

								// '\0' + string => empty string?
								if (!nib_push_stack_string(nsr,""))
								{
									SETRET(nsr,STACK);
									return true;
								}
							}
							break;
						}

					case NST_STRING_S:		// CHAR op STRING(s) => STRING
						{
							if (op != NI_ADD)
							{
								SETRET(nsr,INVALID);
								return true;
							}

							if (*(lsp->_.lvalue._.ch) > 0)
							{
								char *bytes = utf8_getbytes(*(lsp->_.lvalue._.ch));
								int len = strlen(bytes);

								if (rsp->_.str)
								{
									char *value = calloc(1,strlen(rsp->_.str)+len+1);
									if (!value)
									{
										SETRET(nsr,MEMORY);
										return true;
									}

									strcpy(value,bytes);
									strcpy(value+len,rsp->_.str);

									if (!nib_push_stack_string_raw(nsr,value))
									{
										free(value);
										SETRET(nsr,STACK);
										return true;
									}
								}
								else
								{
									if (!nib_push_stack_string(nsr,bytes))
									{
										SETRET(nsr,STACK);
										return true;
									}
								}
							}
							else
							{
								// '\0' + string => empty string?
								if (!nib_push_stack_string(nsr,""))
								{
									SETRET(nsr,STACK);
									return true;
								}
							}
							break;
						}

					case NST_LVALUE:		// CHAR op LVALUE
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_NUMBER:		// CHAR op NUMBER => STRING
								{
									if (op != NI_MULT)
									{
										SETRET(nsr,INVALID);
										return true;
									}

									if (*(lsp->_.lvalue._.ch) > 0)
									{
										// Cloning
										char *bytes = utf8_getbytes(*(lsp->_.lvalue._.ch));
										int len = strlen(bytes);
										int cnt = *(rsp->_.lvalue._.number);
										cnt = (cnt>0)?cnt:0;
										char *value = calloc(1,len*cnt+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										char *s = value;
										for(int i = cnt; i-- > 0;s+=len)
											strcpy(s,bytes);
										*s = '\0';

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else
									{
										if (!nib_push_stack_string(nsr,""))
										{
											SETRET(nsr,STACK);
											return true;
										}
									}

									break;
								}

							case NST_CHAR:			// CHAR op CHAR => STRING
								{
									if (op != NI_ADD)
									{
										SETRET(nsr,INVALID);
										return true;
									}

									if (*(lsp->_.lvalue._.ch) > 0)
									{
										char *lbytes = utf8_getbytes(*(lsp->_.lvalue._.ch));
										int llen = strlen(lbytes);

										if (*(rsp->_.lvalue._.ch) > 0)
										{
											char *rbytes = utf8_getbytes(*(rsp->_.lvalue._.ch));
											int rlen = strlen(rbytes);

											char *value = calloc(1,llen+rlen+1);
											if (!value)
											{
												SETRET(nsr,MEMORY);
												return true;
											}

											if (!nib_push_stack_string_raw(nsr,value))
											{
												free(value);
												SETRET(nsr,STACK);
												return true;
											}
										}
										else if (!nib_push_stack_string(nsr,lbytes))
										{
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,""))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_STRING:		// CHAR op STRING => STRING
								{
									if (op != NI_ADD)
									{
										SETRET(nsr,INVALID);
										return true;
									}

									if (*(lsp->_.lvalue._.ch) > 0)
									{
										char *bytes = utf8_getbytes(*(lsp->_.lvalue._.ch));
										int len = strlen(bytes);

										if (*(rsp->_.lvalue._.str))
										{
											char *value = calloc(1,len+strlen(*(rsp->_.lvalue._.str))+1);
											if (!value)
											{
												SETRET(nsr,MEMORY);
												return true;
											}

											strcpy(value,bytes);
											strcpy(value+len,*(rsp->_.lvalue._.str));

											if (!nib_push_stack_string_raw(nsr,value))
											{
												free(value);
												SETRET(nsr,STACK);
												return true;
											}
										}
										else if (!nib_push_stack_string(nsr,bytes))
										{
											SETRET(nsr,STACK);
											return true;
										}
									}

									// '\0' + string = empty string?
									else if (!nib_push_stack_string(nsr,""))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
						}
					
					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_STRING:		// STRING op ???
				{
					switch(rsp->type)
					{
					case NST_NUMBER:	// STRING op NUMBER => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char number[100];
									ltoa(rsp->_.i,number);
									if (*(lsp->_.lvalue._.str))
									{
										char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+strlen(number)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,*(lsp->_.lvalue._.str));
										strcat(value,number);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,number))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							case NI_MULT:		// Cloning
								{
									if (*(lsp->_.lvalue._.str))
									{
										int len = strlen(*(lsp->_.lvalue._.str));
										char *value = calloc(1,len * ((rsp->_.i > 0)?rsp->_.i:0) + 1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}

										char *str = value;
										for(int i = rsp->_.i; i-- > 0; str += len)
											strcpy(str, *(lsp->_.lvalue._.str));
										*str = 0;

										if (!nib_push_stack_string_raw(nsr, value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr, ""))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_FLOAT:		// STRING op FLOAT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%lf", rsp->_.d);
									if (*(lsp->_.lvalue._.str))
									{
										char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,*(lsp->_.lvalue._.str));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_BOOLEAN:	// STRING op BOOLEAN => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char *stringify = (rsp->_.b?"true":"false");
									if (*(lsp->_.lvalue._.str))
									{
										char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,*(lsp->_.lvalue._.str));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_CHAR:		// STRING op CHAR => STRING
						{
							if (op != NI_ADD)
							{
								SETRET(nsr,INVALID);
								return true;
							}

							if (*(lsp->_.lvalue._.str))
							{
								if (rsp->_.ch > 0)
								{
									char *bytes = utf8_getbytes(rsp->_.ch);
									int blen = strlen(bytes);
									int len = strlen(*(lsp->_.lvalue._.str));
									char *value = calloc(1,len+blen+1);
									if (!value)
									{
										SETRET(nsr,MEMORY);
										return true;
									}
									strcpy(value,*(lsp->_.lvalue._.str));
									strcpy(value+len,bytes);

									if (!nib_push_stack_string_raw(nsr,value))
									{
										free(value);
										SETRET(nsr,STACK);
										return true;
									}
								}
								else if (!nib_push_stack_string_raw(nsr,*(lsp->_.lvalue._.str)))
								{
									SETRET(nsr,STACK);
									return true;
								}
							}
							else
							{
								char *bytes = utf8_getbytes(rsp->_.ch);

								if (!nib_push_stack_string(nsr, bytes))
								{
									SETRET(nsr,STACK);
									return true;
								}
							}
							break;
						}

					case NST_STRING:	// STRING op STRING => STRING
						{
							if (op != NI_ADD)
							{
								SETRET(nsr,INVALID);
								return true;
							}

							if (*(lsp->_.lvalue._.str))
							{
								if (rsp->_.str)
								{
									char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+strlen(rsp->_.str)+1);
									if (!value)
									{
										free(rsp->_.str);
										SETRET(nsr,MEMORY);
										return true;
									}

									strcpy(value,*(lsp->_.lvalue._.str));
									strcat(value,rsp->_.str);

									free(rsp->_.str);

									if (!nib_push_stack_string_raw(nsr, value))
									{
										SETRET(nsr,STACK);
										return true;
									}
								}
								else if (!nib_push_stack_string_raw(nsr, *(lsp->_.lvalue._.str)))
								{
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (rsp->_.str)
							{
								if (!nib_push_stack_string_raw(nsr, rsp->_.str))
								{
									free(rsp->_.str);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr, ""))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_STRING_S:	// STRING op STRING(s) => STRING
						{
							if (op != NI_ADD)
							{
								SETRET(nsr,INVALID);
								return true;
							}

							if (*(lsp->_.lvalue._.str))
							{
								if (rsp->_.str)
								{
									char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+strlen(rsp->_.str)+1);
									if (!value)
									{
										SETRET(nsr,MEMORY);
										return true;
									}

									strcpy(value,*(lsp->_.lvalue._.str));
									strcat(value,rsp->_.str);

									if (!nib_push_stack_string_raw(nsr, value))
									{
										SETRET(nsr,STACK);
										return true;
									}
								}
								else if (!nib_push_stack_string_raw(nsr, *(lsp->_.lvalue._.str)))
								{
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (rsp->_.str)
							{
								if (!nib_push_stack_string(nsr, rsp->_.str))
								{
									SETRET(nsr,STACK);
									return true;
								}
							}
							else if (!nib_push_stack_string(nsr, ""))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:	// STRING op WIDEVNUM => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"(%ld,%ld)",
										(rsp->_.wnum.pArea?rsp->_.wnum.pArea->uid:0),
										rsp->_.wnum.vnum);
									if (*(lsp->_.lvalue._.str))
									{
										char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,*(lsp->_.lvalue._.str));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_FLAG:		// STRING op FLAG => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									const char *stringify = nib_get_flag_string(rsp->_.stat.table,rsp->_.stat.number);
									if (*(lsp->_.lvalue._.str))
									{
										char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,*(lsp->_.lvalue._.str));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_STAT:		// STRING op STAT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									const char *stringify = nib_get_stat_string(rsp->_.stat.table,rsp->_.stat.number);
									if (*(lsp->_.lvalue._.str))
									{
										char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,*(lsp->_.lvalue._.str));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_ACCOUNT:	// STRING op ACCOUNT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									strncpy(stringify,IS_VALID(rsp->_.account) ? rsp->_.account->username : "null",sizeof(stringify)-1);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_AFFECT:	// STRING op AFFECT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID(rsp->_.affect))
										strncpy(stringify,get_affect_name(rsp->_.affect),sizeof(stringify)-1);
									else
										strcpy(stringify,"null");
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_AREA:		// STRING op AREA => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld)",
										(rsp->_.area) ? (rsp->_.area)->name : "null",
										(rsp->_.area) ? (rsp->_.area)->uid : 0);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					// case NST_CHANNEL:	// STRING op CHANNEL => STRING
					case NST_CLASS:		// STRING op CLASS => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID(rsp->_.clazz))
										strncpy(stringify,rsp->_.clazz->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_DUNGEON:	// STRING op DUNGEON => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID(rsp->_.dungeon))
										strncpy(stringify,rsp->_.dungeon->index->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_EXIT:		// STRING op EXIT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID(rsp->_.ex))
									{
										if (rsp->_.ex->orig_door >= 0 && rsp->_.ex->orig_door < MAX_DIR)
											strncpy(stringify,dir_name[rsp->_.ex->orig_door],sizeof(stringify)-1);
										else
											strncpy(stringify,"???",sizeof(stringify)-1);
									}
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_INSTANCE:	// STRING op INSTANCE => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID(rsp->_.instance))
										strncpy(stringify,rsp->_.instance->blueprint->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_LIQUID:	// STRING op LIQUID => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID(rsp->_.liquid))
										strncpy(stringify,rsp->_.liquid->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_MAIL:		// STRING op MAIL => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (rsp->_.mail)
										snprintf(stringify,sizeof(stringify)-1,"<mailto:%s>",rsp->_.mail->recipient);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_MATERIAL:	// STRING op MATERIAL => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID(rsp->_.material))
										strncpy(stringify,rsp->_.material->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_MISSION:	// STRING op MISSION => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (rsp->_.mission)
										snprintf(stringify,sizeof(stringify)-1,"<mission:%ld>",rsp->_.mission->timer);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_MOBILE:	// STRING op MOBILE => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld,%ld)",
										(rsp->_.mobile) ? (rsp->_.mobile)->short_descr : "null",
										(rsp->_.mobile && (rsp->_.mobile)->pIndexData) ? (rsp->_.mobile)->pIndexData->area->uid : 0,
										(rsp->_.mobile && (rsp->_.mobile)->pIndexData) ? (rsp->_.mobile)->pIndexData->vnum : 0);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_NOTE:		// STRING op NOTE => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID(rsp->_.note))
										snprintf(stringify,sizeof(stringify)-1,"<note:%s>",rsp->_.note->to_list);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_OBJECT:	// STRING op OBJECT => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld,%ld)",
										(rsp->_.object) ? (rsp->_.object)->short_descr : "null",
										(rsp->_.object && (rsp->_.object)->pIndexData) ? (rsp->_.object)->pIndexData->area->uid : 0,
										(rsp->_.object && (rsp->_.object)->pIndexData) ? (rsp->_.object)->pIndexData->vnum : 0);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_ORG:		// STRING op ORG => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (rsp->_.org)
										strncpy(stringify,rsp->_.org->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					// case NST_QUEST:	// STRING op QUEST => STRING
					case NST_RACE:		// STRING op RACE => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID(rsp->_.race))
										strncpy(stringify,rsp->_.race->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_RANK:		// STRING op RANK => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID(rsp->_.rank))
										strncpy(stringify,rsp->_.rank->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_REPUTATION:// STRING op REPUTATION => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID(rsp->_.reputation))
										strncpy(stringify,rsp->_.reputation->pIndexData->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_ROOM:		// STRING op ROOM => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (rsp->_.room)
									{
										if(rsp->_.room->source)
										{
											snprintf(stringify,sizeof(stringify)-1,"%s(%ld,%ld,%lu,%lu)",
												(rsp->_.room)->source->name,
												(rsp->_.room)->source->area->uid,
												(rsp->_.room)->source->vnum,
												(rsp->_.room)->id[0],
												(rsp->_.room)->id[1]);
										}
										else
										{
											snprintf(stringify,sizeof(stringify)-1,"%s(%ld,%ld,%ld,%ld)",
												(rsp->_.room)->name,
												(rsp->_.room)->area->uid,
												(rsp->_.room)->vnum);
										}
									}
									else
										strcpy(stringify, "null");
									sprintf(stringify,"%s(%ld,%ld)",
										(rsp->_.room) ? (rsp->_.room)->name : "null",
										(rsp->_.room) ? (rsp->_.room)->area->uid : 0,
										(rsp->_.room) ? (rsp->_.room)->vnum : 0);

									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_SHIP:		// STRING op SHIP => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID(rsp->_.ship))
										strncpy(stringify,rsp->_.ship->index->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_SKILL:		// STRING op SKILL => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									if (IS_VALID(rsp->_.skill))
										strncpy(stringify,rsp->_.skill->name,sizeof(stringify)-1);
									else
										strncpy(stringify,"null",sizeof(stringify)-1);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_TOKEN:		// STRING op TOKEN => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld,%ld)",
										(rsp->_.token) ? (rsp->_.token)->name : "null",
										(rsp->_.token && (rsp->_.token)->pIndexData) ? (rsp->_.token)->pIndexData->area->uid : 0,
										(rsp->_.token && (rsp->_.token)->pIndexData) ? (rsp->_.token)->pIndexData->vnum : 0);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_WILDS:		// STRING op WILDS => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld,%ld)",
										(rsp->_.wilds) ? (rsp->_.wilds)->name : "null",
										(rsp->_.wilds) ? (rsp->_.wilds)->pArea->uid : 0,
										(rsp->_.wilds) ? (rsp->_.wilds)->uid : 0);
									if ((*(lsp->_.lvalue._.str)))
									{
										char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}
										strcpy(value,(*(lsp->_.lvalue._.str)));
										strcat(value,stringify);

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr,stringify))
									{
										SETRET(nsr,STACK);
										return true;
									}

									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					// case NST_WORLD:	// STRING op WORLD => STRING
					case NST_LVALUE:	// STRING op LVALUE => STRING
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_NUMBER:	// STRING op NUMBER => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char number[100];
											ltoa(*(rsp->_.lvalue._.number),number);
											if (*(lsp->_.lvalue._.str))
											{
												char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+strlen(number)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,*(lsp->_.lvalue._.str));
												strcat(value,number);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,number))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									case NI_MULT:		// Cloning
										{
											if (*(lsp->_.lvalue._.str))
											{
												register int cnt = *(rsp->_.lvalue._.number);
												register int len = strlen(*(lsp->_.lvalue._.str));
												char *value = calloc(1,len * ((cnt > 0)?cnt:0) + 1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}

												register char *str = value;
												for(int i = cnt; i-- > 0; str += len)
													strcpy(str, *(lsp->_.lvalue._.str));
												*str = 0;

												if (!nib_push_stack_string_raw(nsr, value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr, ""))
											{
												SETRET(nsr,STACK);
												return true;
											}
											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_FLOAT:		// STRING op FLOAT => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											sprintf(stringify,"%lf", *(rsp->_.lvalue._.d));
											if (*(lsp->_.lvalue._.str))
											{
												char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,*(lsp->_.lvalue._.str));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_BOOLEAN:	// STRING op BOOLEAN => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char *stringify = ((*(rsp->_.lvalue._.b))?"true":"false");
											if (*(lsp->_.lvalue._.str))
											{
												char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,*(lsp->_.lvalue._.str));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_FLAG_BIT:	// STRING op BIT => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											bool set = IS_SET(*(rsp->_.lvalue._.bit.value),rsp->_.lvalue._.bit.bit);
											char *stringify = (set?"true":"false");
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}

							case NST_CHAR:		// STRING op CHAR => STRING
								{
									if (op != NI_ADD)
									{
										SETRET(nsr,INVALID);
										return true;
									}

									if (*(lsp->_.lvalue._.str))
									{
										if (*(rsp->_.lvalue._.ch) > 0)
										{
											char *bytes = utf8_getbytes(*(rsp->_.lvalue._.ch));
											int blen = strlen(bytes);
											int len = strlen(*(lsp->_.lvalue._.str));
											char *value = calloc(1,len+blen+1);
											if (!value)
											{
												SETRET(nsr,MEMORY);
												return true;
											}
											strcpy(value,*(lsp->_.lvalue._.str));
											strcpy(value+len,bytes);

											if (!nib_push_stack_string_raw(nsr,value))
											{
												free(value);
												SETRET(nsr,STACK);
												return true;
											}
										}
										else if (!nib_push_stack_string_raw(nsr,*(lsp->_.lvalue._.str)))
										{
											SETRET(nsr,STACK);
											return true;
										}
									}
									else
									{
										char *bytes = utf8_getbytes(*(rsp->_.lvalue._.ch));
										if (!nib_push_stack_string(nsr, bytes))
										{
											SETRET(nsr,STACK);
											return true;
										}
									}
									break;
								}
							case NST_STRING:	// STRING op STRING => STRING
								{
									if (op != NI_ADD)
									{
										SETRET(nsr,INVALID);
										return true;
									}

									if (*(lsp->_.lvalue._.str))
									{
										if (*(rsp->_.lvalue._.str))
										{
											char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+strlen(*(rsp->_.lvalue._.str))+1);
											if (!value)
											{
												SETRET(nsr,MEMORY);
												return true;
											}

											strcpy(value,*(lsp->_.lvalue._.str));
											strcat(value,*(rsp->_.lvalue._.str));

											if (!nib_push_stack_string_raw(nsr, value))
											{
												SETRET(nsr,STACK);
												return true;
											}
										}
										else if (!nib_push_stack_string_raw(nsr, *(lsp->_.lvalue._.str)))
										{
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (*(rsp->_.lvalue._.str))
									{
										if (!nib_push_stack_string(nsr, *(rsp->_.lvalue._.str)))
										{
											SETRET(nsr,STACK);
											return true;
										}
									}
									else if (!nib_push_stack_string(nsr, ""))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}
							case NST_WIDEVNUM:	// STRING op WIDEVNUM => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											sprintf(stringify,"(%ld,%ld)",
												(rsp->_.lvalue._.wnum->pArea?rsp->_.lvalue._.wnum->pArea->uid:0),
												rsp->_.lvalue._.wnum->vnum);
											if (*(lsp->_.lvalue._.str))
											{
												char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,*(lsp->_.lvalue._.str));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_FLAG:		// STRING op FLAG => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											const char *stringify = nib_get_flag_string(rsp->_.lvalue._.stat.table,*(rsp->_.lvalue._.stat.number));
											if (*(lsp->_.lvalue._.str))
											{
												char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,*(lsp->_.lvalue._.str));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_STAT:		// STRING op STAT => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											const char *stringify = nib_get_stat_string(rsp->_.lvalue._.stat.table,*(rsp->_.lvalue._.stat.number));
											if (*(lsp->_.lvalue._.str))
											{
												char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,*(lsp->_.lvalue._.str));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_ACCOUNT:	// STRING op ACCOUNT => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											strncpy(stringify,IS_VALID((*(rsp->_.lvalue._.account))) ? (*(rsp->_.lvalue._.account))->username : "null",sizeof(stringify)-1);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_AFFECT:	// STRING op AFFECT => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if (IS_VALID((*(rsp->_.lvalue._.affect))))
												strncpy(stringify,get_affect_name((*(rsp->_.lvalue._.affect))),sizeof(stringify)-1);
											else
												strcpy(stringify,"null");
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_AREA:		// STRING op AREA => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											sprintf(stringify,"%s(%ld)",
												((*(rsp->_.lvalue._.area))) ? ((*(rsp->_.lvalue._.area)))->name : "null",
												((*(rsp->_.lvalue._.area))) ? ((*(rsp->_.lvalue._.area)))->uid : 0);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							// case NST_CHANNEL:	// STRING op CHANNEL => STRING
							case NST_CLASS:		// STRING op CLASS => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if (IS_VALID((*(rsp->_.lvalue._.clazz))))
												strncpy(stringify,(*(rsp->_.lvalue._.clazz))->name,sizeof(stringify)-1);
											else
												strncpy(stringify,"null",sizeof(stringify)-1);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_DUNGEON:	// STRING op DUNGEON => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if (IS_VALID((*(rsp->_.lvalue._.dungeon))))
												strncpy(stringify,(*(rsp->_.lvalue._.dungeon))->index->name,sizeof(stringify)-1);
											else
												strncpy(stringify,"null",sizeof(stringify)-1);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_EXIT:		// STRING op EXIT => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if (IS_VALID((*(rsp->_.lvalue._.ex))))
											{
												if ((*(rsp->_.lvalue._.ex))->orig_door >= 0 && (*(rsp->_.lvalue._.ex))->orig_door < MAX_DIR)
													strncpy(stringify,dir_name[(*(rsp->_.lvalue._.ex))->orig_door],sizeof(stringify)-1);
												else
													strncpy(stringify,"???",sizeof(stringify)-1);
											}
											else
												strncpy(stringify,"null",sizeof(stringify)-1);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_INSTANCE:	// STRING op INSTANCE => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if (IS_VALID((*(rsp->_.lvalue._.instance))))
												strncpy(stringify,(*(rsp->_.lvalue._.instance))->blueprint->name,sizeof(stringify)-1);
											else
												strncpy(stringify,"null",sizeof(stringify)-1);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_LIQUID:	// STRING op LIQUID => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if (IS_VALID((*(rsp->_.lvalue._.liquid))))
												strncpy(stringify,(*(rsp->_.lvalue._.liquid))->name,sizeof(stringify)-1);
											else
												strncpy(stringify,"null",sizeof(stringify)-1);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_MAIL:		// STRING op MAIL => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if ((*(rsp->_.lvalue._.mail)))
												snprintf(stringify,sizeof(stringify)-1,"<mailto:%s>",(*(rsp->_.lvalue._.mail))->recipient);
											else
												strncpy(stringify,"null",sizeof(stringify)-1);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_MATERIAL:	// STRING op MATERIAL => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if (IS_VALID((*(rsp->_.lvalue._.material))))
												strncpy(stringify,(*(rsp->_.lvalue._.material))->name,sizeof(stringify)-1);
											else
												strncpy(stringify,"null",sizeof(stringify)-1);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_MISSION:	// STRING op MISSION => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if ((*(rsp->_.lvalue._.mission)))
												snprintf(stringify,sizeof(stringify)-1,"<mission:%ld>",(*(rsp->_.lvalue._.mission))->timer);
											else
												strncpy(stringify,"null",sizeof(stringify)-1);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_MOBILE:	// STRING op MOBILE => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											sprintf(stringify,"%s(%ld,%ld)",
												((*(rsp->_.lvalue._.mobile))) ? ((*(rsp->_.lvalue._.mobile)))->short_descr : "null",
												((*(rsp->_.lvalue._.mobile)) && ((*(rsp->_.lvalue._.mobile)))->pIndexData) ? ((*(rsp->_.lvalue._.mobile)))->pIndexData->area->uid : 0,
												((*(rsp->_.lvalue._.mobile)) && ((*(rsp->_.lvalue._.mobile)))->pIndexData) ? ((*(rsp->_.lvalue._.mobile)))->pIndexData->vnum : 0);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_NOTE:		// STRING op NOTE => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if (IS_VALID((*(rsp->_.lvalue._.note))))
												snprintf(stringify,sizeof(stringify)-1,"<note:%s>",(*(rsp->_.lvalue._.note))->to_list);
											else
												strncpy(stringify,"null",sizeof(stringify)-1);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_OBJECT:	// STRING op OBJECT => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											sprintf(stringify,"%s(%ld,%ld)",
												((*(rsp->_.lvalue._.object))) ? ((*(rsp->_.lvalue._.object)))->short_descr : "null",
												((*(rsp->_.lvalue._.object)) && ((*(rsp->_.lvalue._.object)))->pIndexData) ? ((*(rsp->_.lvalue._.object)))->pIndexData->area->uid : 0,
												((*(rsp->_.lvalue._.object)) && ((*(rsp->_.lvalue._.object)))->pIndexData) ? ((*(rsp->_.lvalue._.object)))->pIndexData->vnum : 0);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_ORG:		// STRING op ORG => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if ((*(rsp->_.lvalue._.org)))
												strncpy(stringify,(*(rsp->_.lvalue._.org))->name,sizeof(stringify)-1);
											else
												strncpy(stringify,"null",sizeof(stringify)-1);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							// case NST_QUEST:	// STRING op QUEST => STRING
							case NST_RACE:		// STRING op RACE => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if (IS_VALID((*(rsp->_.lvalue._.race))))
												strncpy(stringify,(*(rsp->_.lvalue._.race))->name,sizeof(stringify)-1);
											else
												strncpy(stringify,"null",sizeof(stringify)-1);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_RANK:		// STRING op RANK => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if (IS_VALID((*(rsp->_.lvalue._.rank))))
												strncpy(stringify,(*(rsp->_.lvalue._.rank))->name,sizeof(stringify)-1);
											else
												strncpy(stringify,"null",sizeof(stringify)-1);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_REPUTATION:// STRING op REPUTATION => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if (IS_VALID((*(rsp->_.lvalue._.reputation))))
												strncpy(stringify,(*(rsp->_.lvalue._.reputation))->pIndexData->name,sizeof(stringify)-1);
											else
												strncpy(stringify,"null",sizeof(stringify)-1);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_ROOM:		// STRING op ROOM => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if ((*(rsp->_.lvalue._.room)))
											{
												if((*(rsp->_.lvalue._.room))->source)
												{
													snprintf(stringify,sizeof(stringify)-1,"%s(%ld,%ld,%lu,%lu)",
														((*(rsp->_.lvalue._.room)))->source->name,
														((*(rsp->_.lvalue._.room)))->source->area->uid,
														((*(rsp->_.lvalue._.room)))->source->vnum,
														((*(rsp->_.lvalue._.room)))->id[0],
														((*(rsp->_.lvalue._.room)))->id[1]);
												}
												else
												{
													snprintf(stringify,sizeof(stringify)-1,"%s(%ld,%ld,%ld,%ld)",
														((*(rsp->_.lvalue._.room)))->name,
														((*(rsp->_.lvalue._.room)))->area->uid,
														((*(rsp->_.lvalue._.room)))->vnum);
												}
											}
											else
												strcpy(stringify, "null");
											sprintf(stringify,"%s(%ld,%ld)",
												((*(rsp->_.lvalue._.room))) ? ((*(rsp->_.lvalue._.room)))->name : "null",
												((*(rsp->_.lvalue._.room))) ? ((*(rsp->_.lvalue._.room)))->area->uid : 0,
												((*(rsp->_.lvalue._.room))) ? ((*(rsp->_.lvalue._.room)))->vnum : 0);

											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_SHIP:		// STRING op SHIP => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if (IS_VALID((*(rsp->_.lvalue._.ship))))
												strncpy(stringify,(*(rsp->_.lvalue._.ship))->index->name,sizeof(stringify)-1);
											else
												strncpy(stringify,"null",sizeof(stringify)-1);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_SKILL:		// STRING op SKILL => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											if (IS_VALID((*(rsp->_.lvalue._.skill))))
												strncpy(stringify,(*(rsp->_.lvalue._.skill))->name,sizeof(stringify)-1);
											else
												strncpy(stringify,"null",sizeof(stringify)-1);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_TOKEN:		// STRING op TOKEN => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											sprintf(stringify,"%s(%ld,%ld)",
												((*(rsp->_.lvalue._.token))) ? ((*(rsp->_.lvalue._.token)))->name : "null",
												((*(rsp->_.lvalue._.token)) && ((*(rsp->_.lvalue._.token)))->pIndexData) ? ((*(rsp->_.lvalue._.token)))->pIndexData->area->uid : 0,
												((*(rsp->_.lvalue._.token)) && ((*(rsp->_.lvalue._.token)))->pIndexData) ? ((*(rsp->_.lvalue._.token)))->pIndexData->vnum : 0);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							case NST_WILDS:		// STRING op WILDS => STRING
								{
									switch(op)
									{
									case NI_ADD:		// Concatenation
										{
											char stringify[100];
											sprintf(stringify,"%s(%ld,%ld)",
												((*(rsp->_.lvalue._.wilds))) ? ((*(rsp->_.lvalue._.wilds)))->name : "null",
												((*(rsp->_.lvalue._.wilds))) ? ((*(rsp->_.lvalue._.wilds)))->pArea->uid : 0,
												((*(rsp->_.lvalue._.wilds))) ? ((*(rsp->_.lvalue._.wilds)))->uid : 0);
											if ((*(lsp->_.lvalue._.str)))
											{
												char *value = calloc(1,strlen((*(lsp->_.lvalue._.str)))+strlen(stringify)+1);
												if (!value)
												{
													SETRET(nsr,MEMORY);
													return true;
												}
												strcpy(value,(*(lsp->_.lvalue._.str)));
												strcat(value,stringify);

												if (!nib_push_stack_string_raw(nsr,value))
												{
													free(value);
													SETRET(nsr,STACK);
													return true;
												}
											}
											else if (!nib_push_stack_string(nsr,stringify))
											{
												SETRET(nsr,STACK);
												return true;
											}

											break;
										}

									default:
										SETRET(nsr,INVALID);
										return true;
									}
									break;
								}
							
							// case NST_WORLD:	// STRING op WORLD => STRING
							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_FLAG:			// FLAG op ???
				{
					switch(rsp->type)
					{
					case NST_NUMBER:
						{
							long value;
							switch(op)
							{
							case NI_BAND:	value = *(lsp->_.lvalue._.stat.number) & (rsp->_.i); break;
							case NI_BOR:	value = *(lsp->_.lvalue._.stat.number) | (rsp->_.i); break;
							case NI_BXOR:	value = *(lsp->_.lvalue._.stat.number) ^ (rsp->_.i); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_flag(nsr,value,lsp->_.lvalue._.stat.table))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}
					
					case NST_FLAG:
						{
							if (lsp->_.lvalue._.stat.table != rsp->_.stat.table)
							{
								SETRET(nsr,INVALID);
								return true;
							}
							long value;
							switch(op)
							{
							case NI_BAND:	value = *(lsp->_.lvalue._.stat.number) & (rsp->_.stat.number); break;
							case NI_BOR:	value = *(lsp->_.lvalue._.stat.number) | (rsp->_.stat.number); break;
							case NI_BXOR:	value = *(lsp->_.lvalue._.stat.number) ^ (rsp->_.stat.number); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_flag(nsr,value, lsp->_.stat.table))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}
					
					case NST_LVALUE:
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_NUMBER:
								{
									long value;
									switch(op)
									{
									case NI_BAND:	value = *(lsp->_.lvalue._.stat.number) & *(rsp->_.lvalue._.number); break;
									case NI_BOR:	value = *(lsp->_.lvalue._.stat.number) | *(rsp->_.lvalue._.number); break;
									case NI_BXOR:	value = *(lsp->_.lvalue._.stat.number) ^ *(rsp->_.lvalue._.number); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_flag(nsr,value,lsp->_.lvalue._.stat.table))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_FLAG:
								{
									if (lsp->_.lvalue._.stat.table != rsp->_.lvalue._.stat.table)
									{
										SETRET(nsr,INVALID);
										return true;
									}
									long value;
									switch(op)
									{
									case NI_BAND:	value = *(lsp->_.lvalue._.stat.number) & *(rsp->_.lvalue._.stat.number); break;
									case NI_BOR:	value = *(lsp->_.lvalue._.stat.number) | *(rsp->_.lvalue._.stat.number); break;
									case NI_BXOR:	value = *(lsp->_.lvalue._.stat.number) ^ *(rsp->_.lvalue._.stat.number); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_flag(nsr,value,lsp->_.lvalue._.stat.table))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	default:
		SETRET(nsr,INVALID);
		return true;
	}

	return false;
}

#define __bool_null(t,v) \
			case NST_##t: \
				{ \
					bool value = (v); \
					switch(op) \
					{ \
					case NI_EQ:		/* Already tested */break; \
					case NI_NEQ:	value = !value; break; \
					default: \
						SETRET(nsr,INVALID); \
						return true; \
					} \
\
					if (!nib_push_stack_boolean(nsr,value)) \
					{ \
						SETRET(nsr,STACK); \
						return true; \
					} \
					break; \
				}

#define __bool_nullf(t,f) \
			case NST_##t: \
				{ \
					bool value = (rsp->_.f == NULL); \
					switch(op) \
					{ \
					case NI_EQ:		/* Already tested */break; \
					case NI_NEQ:	value = !value; break; \
					default: \
						SETRET(nsr,INVALID); \
						return true; \
					} \
\
					if (!nib_push_stack_boolean(nsr,value)) \
					{ \
						SETRET(nsr,STACK); \
						return true; \
					} \
					break; \
				}

#define __bool_nullflv(t,f) \
			case NST_##t: \
				{ \
					bool value = (*(rsp->_.lvalue._.f) == NULL); \
					switch(op) \
					{ \
					case NI_EQ:		/* Already tested */break; \
					case NI_NEQ:	value = !value; break; \
					default: \
						SETRET(nsr,INVALID); \
						return true; \
					} \
\
					if (!nib_push_stack_boolean(nsr,value)) \
					{ \
						SETRET(nsr,STACK); \
						return true; \
					} \
					break; \
				}

#define __bool_nullvf(t,f) \
			case NST_##t: \
				{ \
					bool value = IS_VALID(rsp->_.f); \
					switch(op) \
					{ \
					case NI_EQ:		value = !value; break; \
					case NI_NEQ:	/* Already tested */ break; \
					default: \
						SETRET(nsr,INVALID); \
						return true; \
					} \
\
					if (!nib_push_stack_boolean(nsr,value)) \
					{ \
						SETRET(nsr,STACK); \
						return true; \
					} \
					break; \
				}

#define __bool_nullvflv(t,f) \
			case NST_##t: \
				{ \
					bool value = IS_VALID(*(rsp->_.lvalue._.f)); \
					switch(op) \
					{ \
					case NI_EQ:		value = !value; break; \
					case NI_NEQ:	/* Already tested */ break; \
					default: \
						SETRET(nsr,INVALID); \
						return true; \
					} \
\
					if (!nib_push_stack_boolean(nsr,value)) \
					{ \
						SETRET(nsr,STACK); \
						return true; \
					} \
					break; \
				}


// Does boolean comparisions (AND, OR, equalities)
static bool __boolean_operation(NIB_SCRIPT_RUNTIME *nsr, enum nib_instructions_e op)
{
	// Stack Order
	// RHS
	NIB_SCRIPT_STACK *rsp = nib_pop_stack_raw(nsr);
	if (!rsp)
	{
		SETRET(nsr,STACK);
		return true;
	}
	// LHS
	NIB_SCRIPT_STACK *lsp = nib_pop_stack_raw(nsr);
	if (!lsp)
	{
		free_stack_item(rsp);
		SETRET(nsr,STACK);
		return true;
	}
	
	switch(lsp->type)
	{
	case NST_NUMBER:		// NUMBER op ???
		{
			switch(rsp->type)
			{
			case NST_NUMBER:	// NUMBER op NUMBER => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.i) == (rsp->_.i); break;
					case NI_NEQ:	value = (lsp->_.i) != (rsp->_.i); break;
					case NI_LT:		value = (lsp->_.i) < (rsp->_.i); break;
					case NI_LE:		value = (lsp->_.i) <= (rsp->_.i); break;
					case NI_GT:		value = (lsp->_.i) > (rsp->_.i); break;
					case NI_GE:		value = (lsp->_.i) >= (rsp->_.i); break;
					case NI_LAND:	value = (lsp->_.i != 0) && (rsp->_.i != 0); break;
					case NI_LOR:	value = (lsp->_.i != 0) || (rsp->_.i != 0); break;
					case NI_LXOR:	value = (lsp->_.i != 0) != (rsp->_.i != 0); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr, value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_FLOAT:		// NUMBER op FLOAT => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = ((double)lsp->_.i) == (rsp->_.d); break;
					case NI_NEQ:	value = ((double)lsp->_.i) != (rsp->_.d); break;
					case NI_LT:		value = ((double)lsp->_.i) < (rsp->_.d); break;
					case NI_LE:		value = ((double)lsp->_.i) <= (rsp->_.d); break;
					case NI_GT:		value = ((double)lsp->_.i) > (rsp->_.d); break;
					case NI_GE:		value = ((double)lsp->_.i) >= (rsp->_.d); break;
					case NI_LAND:	value = (lsp->_.i != 0) && (rsp->_.d != 0.0); break;
					case NI_LOR:	value = (lsp->_.i != 0) || (rsp->_.d != 0.0); break;
					case NI_LXOR:	value = (lsp->_.i != 0) != (rsp->_.d != 0.0); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr, value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_CHAR:		// NUMBER op CHAR => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.i) == (rsp->_.ch); break;
					case NI_NEQ:	value = (lsp->_.i) != (rsp->_.ch); break;
					case NI_LT:		value = (lsp->_.i) < (rsp->_.ch); break;
					case NI_LE:		value = (lsp->_.i) <= (rsp->_.ch); break;
					case NI_GT:		value = (lsp->_.i) > (rsp->_.ch); break;
					case NI_GE:		value = (lsp->_.i) >= (rsp->_.ch); break;
					case NI_LAND:	value = (lsp->_.i != 0) && (rsp->_.ch != '\0'); break;
					case NI_LOR:	value = (lsp->_.i != 0) || (rsp->_.ch != '\0'); break;
					case NI_LXOR:	value = (lsp->_.i != 0) != (rsp->_.ch != '\0'); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr, value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_BOOLEAN:	// NUMBER op BOOLEAN => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_LAND:	value = (lsp->_.i != 0) && (rsp->_.b); break;
					case NI_LOR:	value = (lsp->_.i != 0) || (rsp->_.b); break;
					case NI_LXOR:	value = (lsp->_.i != 0) != (rsp->_.b); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr, value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_FLAG:		// NUMBER op FLAG => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.i) == (rsp->_.stat.number); break;
					case NI_NEQ:	value = (lsp->_.i) != (rsp->_.stat.number); break;
					case NI_LAND:	value = (lsp->_.i != 0) && (rsp->_.stat.number != 0); break;
					case NI_LOR:	value = (lsp->_.i != 0) || (rsp->_.stat.number != 0); break;
					case NI_LXOR:	value = (lsp->_.i != 0) != (rsp->_.stat.number != 0); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr, value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:	// NUMBER op NUMBER => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.i) == (*(rsp->_.lvalue._.number)); break;
							case NI_NEQ:	value = (lsp->_.i) != (*(rsp->_.lvalue._.number)); break;
							case NI_LT:		value = (lsp->_.i) < (*(rsp->_.lvalue._.number)); break;
							case NI_LE:		value = (lsp->_.i) <= (*(rsp->_.lvalue._.number)); break;
							case NI_GT:		value = (lsp->_.i) > (*(rsp->_.lvalue._.number)); break;
							case NI_GE:		value = (lsp->_.i) >= (*(rsp->_.lvalue._.number)); break;
							case NI_LAND:	value = (lsp->_.i != 0) && (*(rsp->_.lvalue._.number) != 0); break;
							case NI_LOR:	value = (lsp->_.i != 0) || (*(rsp->_.lvalue._.number) != 0); break;
							case NI_LXOR:	value = (lsp->_.i != 0) != (*(rsp->_.lvalue._.number) != 0); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_FLOAT:		// NUMBER op FLOAT => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((double)lsp->_.i) == (*(rsp->_.lvalue._.d)); break;
							case NI_NEQ:	value = ((double)lsp->_.i) != (*(rsp->_.lvalue._.d)); break;
							case NI_LT:		value = ((double)lsp->_.i) < (*(rsp->_.lvalue._.d)); break;
							case NI_LE:		value = ((double)lsp->_.i) <= (*(rsp->_.lvalue._.d)); break;
							case NI_GT:		value = ((double)lsp->_.i) > (*(rsp->_.lvalue._.d)); break;
							case NI_GE:		value = ((double)lsp->_.i) >= (*(rsp->_.lvalue._.d)); break;
							case NI_LAND:	value = (lsp->_.i != 0) && (*(rsp->_.lvalue._.d) != 0.0); break;
							case NI_LOR:	value = (lsp->_.i != 0) || (*(rsp->_.lvalue._.d) != 0.0); break;
							case NI_LXOR:	value = (lsp->_.i != 0) != (*(rsp->_.lvalue._.d) != 0.0); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_CHAR:		// NUMBER op CHAR => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.i) == (*(rsp->_.lvalue._.ch)); break;
							case NI_NEQ:	value = (lsp->_.i) != (*(rsp->_.lvalue._.ch)); break;
							case NI_LT:		value = (lsp->_.i) < (*(rsp->_.lvalue._.ch)); break;
							case NI_LE:		value = (lsp->_.i) <= (*(rsp->_.lvalue._.ch)); break;
							case NI_GT:		value = (lsp->_.i) > (*(rsp->_.lvalue._.ch)); break;
							case NI_GE:		value = (lsp->_.i) >= (*(rsp->_.lvalue._.ch)); break;
							case NI_LAND:	value = (lsp->_.i != 0) && (*(rsp->_.lvalue._.ch) != '\0'); break;
							case NI_LOR:	value = (lsp->_.i != 0) || (*(rsp->_.lvalue._.ch) != '\0'); break;
							case NI_LXOR:	value = (lsp->_.i != 0) != (*(rsp->_.lvalue._.ch) != '\0'); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_BOOLEAN:	// NUMBER op BOOLEAN => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_LAND:	value = (lsp->_.i != 0) && (*(rsp->_.lvalue._.b)); break;
							case NI_LOR:	value = (lsp->_.i != 0) || (*(rsp->_.lvalue._.b)); break;
							case NI_LXOR:	value = (lsp->_.i != 0) != (*(rsp->_.lvalue._.b)); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_FLAG:		// NUMBER op FLAG => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.i) == (*(rsp->_.lvalue._.stat.number)); break;
							case NI_NEQ:	value = (lsp->_.i) != (*(rsp->_.lvalue._.stat.number)); break;
							case NI_LAND:	value = (lsp->_.i != 0) && (*(rsp->_.lvalue._.stat.number) != 0); break;
							case NI_LOR:	value = (lsp->_.i != 0) || (*(rsp->_.lvalue._.stat.number) != 0); break;
							case NI_LXOR:	value = (lsp->_.i != 0) != (*(rsp->_.lvalue._.stat.number) != 0); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_FLAG_BIT:	// NUMBER op BIT => BOOLEAN
						{
							bool set = IS_SET(*(rsp->_.lvalue._.bit.value),rsp->_.lvalue._.bit.bit);
							bool value;
							switch(op)
							{
							case NI_LAND:	value = (lsp->_.i != 0) && set; break;
							case NI_LOR:	value = (lsp->_.i != 0) || set; break;
							case NI_LXOR:	value = (lsp->_.i != 0) != set; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_FLOAT:			// FLOAT op ???
		{
			switch(rsp->type)
			{
			case NST_NUMBER:	// FLOAT op NUMBER => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.d) == ((double)rsp->_.i); break;
					case NI_NEQ:	value = (lsp->_.d) != ((double)rsp->_.i); break;
					case NI_LT:		value = (lsp->_.d) < ((double)rsp->_.i); break;
					case NI_LE:		value = (lsp->_.d) <= ((double)rsp->_.i); break;
					case NI_GT:		value = (lsp->_.d) > ((double)rsp->_.i); break;
					case NI_GE:		value = (lsp->_.d) >= ((double)rsp->_.i); break;
					case NI_LAND:	value = (lsp->_.d != 0.0) && (rsp->_.i != 0); break;
					case NI_LOR:	value = (lsp->_.d != 0.0) || (rsp->_.i != 0); break;
					case NI_LXOR:	value = (lsp->_.d != 0.0) != (rsp->_.i != 0); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr, value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_FLOAT:		// FLOAT op FLOAT => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.d) == (rsp->_.d); break;
					case NI_NEQ:	value = (lsp->_.d) != (rsp->_.d); break;
					case NI_LT:		value = (lsp->_.d) < (rsp->_.d); break;
					case NI_LE:		value = (lsp->_.d) <= (rsp->_.d); break;
					case NI_GT:		value = (lsp->_.d) > (rsp->_.d); break;
					case NI_GE:		value = (lsp->_.d) >= (rsp->_.d); break;
					case NI_LAND:	value = (lsp->_.d != 0.0) && (rsp->_.d != 0.0); break;
					case NI_LOR:	value = (lsp->_.d != 0.0) || (rsp->_.d != 0.0); break;
					case NI_LXOR:	value = (lsp->_.d != 0.0) != (rsp->_.d != 0.0); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr, value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// FLOAT op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:	// FLOAT op NUMBER => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.d) == ((double)*(rsp->_.lvalue._.number)); break;
							case NI_NEQ:	value = (lsp->_.d) != ((double)*(rsp->_.lvalue._.number)); break;
							case NI_LT:		value = (lsp->_.d) < ((double)*(rsp->_.lvalue._.number)); break;
							case NI_LE:		value = (lsp->_.d) <= ((double)*(rsp->_.lvalue._.number)); break;
							case NI_GT:		value = (lsp->_.d) > ((double)*(rsp->_.lvalue._.number)); break;
							case NI_GE:		value = (lsp->_.d) >= ((double)*(rsp->_.lvalue._.number)); break;
							case NI_LAND:	value = (lsp->_.d != 0.0) && (*(rsp->_.lvalue._.number) != 0); break;
							case NI_LOR:	value = (lsp->_.d != 0.0) || (*(rsp->_.lvalue._.number) != 0); break;
							case NI_LXOR:	value = (lsp->_.d != 0.0) != (*(rsp->_.lvalue._.number) != 0); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_FLOAT:		// FLOAT op FLOAT => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.d) == (*(rsp->_.lvalue._.d)); break;
							case NI_NEQ:	value = (lsp->_.d) != (*(rsp->_.lvalue._.d)); break;
							case NI_LT:		value = (lsp->_.d) < (*(rsp->_.lvalue._.d)); break;
							case NI_LE:		value = (lsp->_.d) <= (*(rsp->_.lvalue._.d)); break;
							case NI_GT:		value = (lsp->_.d) > (*(rsp->_.lvalue._.d)); break;
							case NI_GE:		value = (lsp->_.d) >= (*(rsp->_.lvalue._.d)); break;
							case NI_LAND:	value = (lsp->_.d != 0.0) && (*(rsp->_.lvalue._.d) != 0.0); break;
							case NI_LOR:	value = (lsp->_.d != 0.0) || (*(rsp->_.lvalue._.d) != 0.0); break;
							case NI_LXOR:	value = (lsp->_.d != 0.0) != (*(rsp->_.lvalue._.d) != 0.0); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_CHAR:			// CHAR op ???
		{
			switch(rsp->type)
			{
			case NST_NUMBER:	// CHAR op NUMBER => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.ch) == (rsp->_.i); break;
					case NI_NEQ:	value = (lsp->_.ch) != (rsp->_.i); break;
					case NI_LT:		value = (lsp->_.ch) < (rsp->_.i); break;
					case NI_LE:		value = (lsp->_.ch) <= (rsp->_.i); break;
					case NI_GT:		value = (lsp->_.ch) > (rsp->_.i); break;
					case NI_GE:		value = (lsp->_.ch) >= (rsp->_.i); break;
					case NI_LAND:	value = (lsp->_.ch != '\0') && (rsp->_.i != 0); break;
					case NI_LOR:	value = (lsp->_.ch != '\0') || (rsp->_.i != 0); break;
					case NI_LXOR:	value = (lsp->_.ch != '\0') != (rsp->_.i != 0); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr, value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_CHAR:		// CHAR op CHAR => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.ch) == (rsp->_.ch); break;
					case NI_NEQ:	value = (lsp->_.ch) != (rsp->_.ch); break;
					case NI_LT:		value = (lsp->_.ch) < (rsp->_.ch); break;
					case NI_LE:		value = (lsp->_.ch) <= (rsp->_.ch); break;
					case NI_GT:		value = (lsp->_.ch) > (rsp->_.ch); break;
					case NI_GE:		value = (lsp->_.ch) >= (rsp->_.ch); break;
					case NI_LAND:	value = (lsp->_.ch != '\0') && (rsp->_.ch != '\0'); break;
					case NI_LOR:	value = (lsp->_.ch != '\0') || (rsp->_.ch != '\0'); break;
					case NI_LXOR:	value = (lsp->_.ch != '\0') != (rsp->_.ch != '\0'); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr, value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// CHAR op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:	// CHAR op NUMBER => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.ch) == (*(rsp->_.lvalue._.number)); break;
							case NI_NEQ:	value = (lsp->_.ch) != (*(rsp->_.lvalue._.number)); break;
							case NI_LT:		value = (lsp->_.ch) < (*(rsp->_.lvalue._.number)); break;
							case NI_LE:		value = (lsp->_.ch) <= (*(rsp->_.lvalue._.number)); break;
							case NI_GT:		value = (lsp->_.ch) > (*(rsp->_.lvalue._.number)); break;
							case NI_GE:		value = (lsp->_.ch) >= (*(rsp->_.lvalue._.number)); break;
							case NI_LAND:	value = (lsp->_.ch != '\0') && (*(rsp->_.lvalue._.number) != 0); break;
							case NI_LOR:	value = (lsp->_.ch != '\0') || (*(rsp->_.lvalue._.number) != 0); break;
							case NI_LXOR:	value = (lsp->_.ch != '\0') != (*(rsp->_.lvalue._.number) != 0); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_CHAR:		// CHAR op CHAR => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.ch) == (*(rsp->_.lvalue._.ch)); break;
							case NI_NEQ:	value = (lsp->_.ch) != (*(rsp->_.lvalue._.ch)); break;
							case NI_LT:		value = (lsp->_.ch) < (*(rsp->_.lvalue._.ch)); break;
							case NI_LE:		value = (lsp->_.ch) <= (*(rsp->_.lvalue._.ch)); break;
							case NI_GT:		value = (lsp->_.ch) > (*(rsp->_.lvalue._.ch)); break;
							case NI_GE:		value = (lsp->_.ch) >= (*(rsp->_.lvalue._.ch)); break;
							case NI_LAND:	value = (lsp->_.ch != '\0') && (*(rsp->_.lvalue._.ch) != '\0'); break;
							case NI_LOR:	value = (lsp->_.ch != '\0') || (*(rsp->_.lvalue._.ch) != '\0'); break;
							case NI_LXOR:	value = (lsp->_.ch != '\0') != (*(rsp->_.lvalue._.ch) != '\0'); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}		

	case NST_STRING:		// STRING op ???
		{
			switch(rsp->type)
			{
			case NST_STRING:	// STRING op STRING
				{
					bool value;
					switch(op)
					{
					case NI_EQ:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) == 0); break;
					case NI_NEQ:		value = (utf8_str_cmp(lsp->_.str,rsp->_.str) != 0); break;
					case NI_LT:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) < 0); break;
					case NI_LE:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) <= 0); break;
					case NI_GT:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) > 0); break;
					case NI_GE:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) >= 0); break;
					case NI_STR_PREFIX: value = !utf8_str_prefix(rsp->_.str,lsp->_.str); break;
					case NI_STR_INFIX:	value = !utf8_str_infix(rsp->_.str,lsp->_.str); break;
					case NI_STR_SUFFIX:	value = !utf8_str_suffix(rsp->_.str,lsp->_.str); break;
					case NI_LAND:		value = (!IS_NULLSTR(lsp->_.str)) && (!IS_NULLSTR(rsp->_.str)); break;
					case NI_LOR:		value = (!IS_NULLSTR(lsp->_.str)) || (!IS_NULLSTR(rsp->_.str)); break;
					case NI_LXOR:		value = (!IS_NULLSTR(lsp->_.str)) != (!IS_NULLSTR(rsp->_.str)); break;
					default:
						if (lsp->_.str) free(lsp->_.str);
						if (rsp->_.str) free(rsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}

					if (lsp->_.str) free(lsp->_.str);
					if (rsp->_.str) free(rsp->_.str);

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}
			
			case NST_STRING_S:	// STRING op STRING(s)
				{
					bool value;
					switch(op)
					{
					case NI_EQ:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) == 0); break;
					case NI_NEQ:		value = (utf8_str_cmp(lsp->_.str,rsp->_.str) != 0); break;
					case NI_LT:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) < 0); break;
					case NI_LE:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) <= 0); break;
					case NI_GT:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) > 0); break;
					case NI_GE:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) >= 0); break;
					case NI_STR_PREFIX: value = !utf8_str_prefix(rsp->_.str,lsp->_.str); break;
					case NI_STR_INFIX:	value = !utf8_str_infix(rsp->_.str,lsp->_.str); break;
					case NI_STR_SUFFIX:	value = !utf8_str_suffix(rsp->_.str,lsp->_.str); break;
					case NI_LAND:		value = (!IS_NULLSTR(lsp->_.str)) && (!IS_NULLSTR(rsp->_.str)); break;
					case NI_LOR:		value = (!IS_NULLSTR(lsp->_.str)) || (!IS_NULLSTR(rsp->_.str)); break;
					case NI_LXOR:		value = (!IS_NULLSTR(lsp->_.str)) != (!IS_NULLSTR(rsp->_.str)); break;
					default:
						if (lsp->_.str) free(lsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}

					if (lsp->_.str) free(lsp->_.str);

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// STRING op null
				{
					bool value = (lsp->_.str == NULL);
					if (lsp->_.str) free(lsp->_.str);

					switch(op)
					{
					case NI_EQ:		break;	// Value already set
					case NI_NEQ:	value = !value; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// STRING op LVALUE
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_STRING:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:			value = (utf8_str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) == 0); break;
							case NI_NEQ:		value = (utf8_str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) != 0); break;
							case NI_LT:			value = (utf8_str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) < 0); break;
							case NI_LE:			value = (utf8_str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) <= 0); break;
							case NI_GT:			value = (utf8_str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) > 0); break;
							case NI_GE:			value = (utf8_str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) >= 0); break;
							case NI_STR_PREFIX: value = !utf8_str_prefix(*(rsp->_.lvalue._.str),lsp->_.str); break;
							case NI_STR_INFIX:	value = !utf8_str_infix(*(rsp->_.lvalue._.str),lsp->_.str); break;
							case NI_STR_SUFFIX:	value = !utf8_str_suffix(*(rsp->_.lvalue._.str),lsp->_.str); break;
							case NI_LAND:		value = (!IS_NULLSTR(lsp->_.str)) && (!IS_NULLSTR(*(rsp->_.lvalue._.str))); break;
							case NI_LOR:		value = (!IS_NULLSTR(lsp->_.str)) || (!IS_NULLSTR(*(rsp->_.lvalue._.str))); break;
							case NI_LXOR:		value = (!IS_NULLSTR(lsp->_.str)) != (!IS_NULLSTR(*(rsp->_.lvalue._.str))); break;
							default:
								if (lsp->_.str) free(lsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}

							if (lsp->_.str) free(lsp->_.str);

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}
					
					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_STRING_S:		// STRING op ???
		{
			switch(rsp->type)
			{
			case NST_STRING:	// STRING op STRING
				{
					bool value;
					switch(op)
					{
					case NI_EQ:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) == 0); break;
					case NI_NEQ:		value = (utf8_str_cmp(lsp->_.str,rsp->_.str) != 0); break;
					case NI_LT:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) < 0); break;
					case NI_LE:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) <= 0); break;
					case NI_GT:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) > 0); break;
					case NI_GE:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) >= 0); break;
					case NI_STR_PREFIX: value = !utf8_str_prefix(rsp->_.str,lsp->_.str); break;
					case NI_STR_INFIX:	value = !utf8_str_infix(rsp->_.str,lsp->_.str); break;
					case NI_STR_SUFFIX:	value = !utf8_str_suffix(rsp->_.str,lsp->_.str); break;
					case NI_LAND:		value = (!IS_NULLSTR(lsp->_.str)) && (!IS_NULLSTR(rsp->_.str)); break;
					case NI_LOR:		value = (!IS_NULLSTR(lsp->_.str)) || (!IS_NULLSTR(rsp->_.str)); break;
					case NI_LXOR:		value = (!IS_NULLSTR(lsp->_.str)) != (!IS_NULLSTR(rsp->_.str)); break;
					default:
						if (rsp->_.str) free(rsp->_.str);
						SETRET(nsr,INVALID);
						return true;
					}

					if (rsp->_.str) free(rsp->_.str);

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}
			
			case NST_STRING_S:	// STRING op STRING(s)
				{
					bool value;
					switch(op)
					{
					case NI_EQ:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) == 0); break;
					case NI_NEQ:		value = (utf8_str_cmp(lsp->_.str,rsp->_.str) != 0); break;
					case NI_LT:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) < 0); break;
					case NI_LE:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) <= 0); break;
					case NI_GT:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) > 0); break;
					case NI_GE:			value = (utf8_str_cmp(lsp->_.str,rsp->_.str) >= 0); break;
					case NI_STR_PREFIX: value = !utf8_str_prefix(rsp->_.str,lsp->_.str); break;
					case NI_STR_INFIX:	value = !utf8_str_infix(rsp->_.str,lsp->_.str); break;
					case NI_STR_SUFFIX:	value = !utf8_str_suffix(rsp->_.str,lsp->_.str); break;
					case NI_LAND:		value = (!IS_NULLSTR(lsp->_.str)) && (!IS_NULLSTR(rsp->_.str)); break;
					case NI_LOR:		value = (!IS_NULLSTR(lsp->_.str)) || (!IS_NULLSTR(rsp->_.str)); break;
					case NI_LXOR:		value = (!IS_NULLSTR(lsp->_.str)) != (!IS_NULLSTR(rsp->_.str)); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// STRING op null
				{
					bool value = (lsp->_.str == NULL);

					switch(op)
					{
					case NI_EQ:		break;	// Value already set
					case NI_NEQ:	value = !value; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// STRING op LVALUE
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_STRING:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:			value = (utf8_str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) == 0); break;
							case NI_NEQ:		value = (utf8_str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) != 0); break;
							case NI_LT:			value = (utf8_str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) < 0); break;
							case NI_LE:			value = (utf8_str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) <= 0); break;
							case NI_GT:			value = (utf8_str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) > 0); break;
							case NI_GE:			value = (utf8_str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) >= 0); break;
							case NI_STR_PREFIX: value = !utf8_str_prefix(*(rsp->_.lvalue._.str),lsp->_.str); break;
							case NI_STR_INFIX:	value = !utf8_str_infix(*(rsp->_.lvalue._.str),lsp->_.str); break;
							case NI_STR_SUFFIX:	value = !utf8_str_suffix(*(rsp->_.lvalue._.str),lsp->_.str); break;
							case NI_LAND:		value = (!IS_NULLSTR(lsp->_.str)) && (!IS_NULLSTR(*(rsp->_.lvalue._.str))); break;
							case NI_LOR:		value = (!IS_NULLSTR(lsp->_.str)) || (!IS_NULLSTR(*(rsp->_.lvalue._.str))); break;
							case NI_LXOR:		value = (!IS_NULLSTR(lsp->_.str)) != (!IS_NULLSTR(*(rsp->_.lvalue._.str))); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}
					
					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_FLAG:			// FLAG op ???
		{
			switch(rsp->type)
			{
			case NST_NUMBER:	// FLAG op NUMBER => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.stat.number) == (rsp->_.i); break;
					case NI_NEQ:	value = (lsp->_.stat.number) != (rsp->_.i); break;
					default:
						SETRET(nsr,INVALID);
						return true;		
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
				}

			case NST_FLAG:		// FLAG op FLAG => BOOLEAN
				{
					bool same = lsp->_.stat.table == rsp->_.stat.table;
					bool value;
					switch(op)
					{
					case NI_EQ:		value = same && ((lsp->_.stat.number) == (rsp->_.stat.number)); break;
					case NI_NEQ:	value = !same || ((lsp->_.stat.number) != (rsp->_.stat.number)); break;
					default:
						SETRET(nsr,INVALID);
						return true;		
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
				}

			case NST_LVALUE:
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:	// FLAG op NUMBER => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.stat.number) == (*(rsp->_.lvalue._.number)); break;
							case NI_NEQ:	value = (lsp->_.stat.number) != (*(rsp->_.lvalue._.number)); break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
						}

					case NST_FLAG:		// FLAG op FLAG => BOOLEAN
						{
							bool same = (lsp->_.stat.table == rsp->_.lvalue._.stat.table);
							bool value;
							switch(op)
							{
							case NI_EQ:		value = same && ((lsp->_.stat.number) == (*(rsp->_.lvalue._.stat.number))); break;
							case NI_NEQ:	value = !same || ((lsp->_.stat.number) != (*(rsp->_.lvalue._.stat.number))); break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
						}

					default:
						SETRET(nsr,INVALID);
						return true;		
					}
					break;
				}			

			default:
				SETRET(nsr,INVALID);
				return true;		
			}
			break;
		}

	case NST_WIDEVNUM:		// WIDEVNUM op ???
		{
			switch(rsp->type)
			{
			case NST_WIDEVNUM:		// WIDEVNUM op WIDEVNUM
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.wnum.pArea == rsp->_.wnum.pArea) && (lsp->_.wnum.vnum == rsp->_.wnum.vnum); break;
					case NI_NEQ:	value = (lsp->_.wnum.pArea != rsp->_.wnum.pArea) || (lsp->_.wnum.vnum != rsp->_.wnum.vnum); break;
					default:
						SETRET(nsr,INVALID);
						return true;		
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;		
					}

					break;
				}

			case NST_DUNGEON:		// WIDEVNUM op DUNGEON => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (rsp->_.dungeon) && (rsp->_.dungeon->index) && (lsp->_.wnum.pArea == rsp->_.dungeon->index->area) && (lsp->_.wnum.vnum == rsp->_.dungeon->index->vnum); break;
					case NI_NEQ:
						if (rsp->_.dungeon && rsp->_.dungeon->index)
							value = (lsp->_.wnum.pArea != rsp->_.dungeon->index->area) || (lsp->_.wnum.vnum != rsp->_.dungeon->index->vnum);
						else
							value = lsp->_.wnum.pArea && lsp->_.wnum.vnum > 0;
						break;
					default:
						SETRET(nsr,INVALID);
						return true;		
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;		
					}

					break;
				}
			
			case NST_INSTANCE:		// WIDEVNUM op INSTANCE
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = rsp->_.instance && (rsp->_.instance->blueprint != NULL) && (lsp->_.wnum.pArea == rsp->_.instance->blueprint->area) && (lsp->_.wnum.vnum == rsp->_.instance->blueprint->vnum); break;
					case NI_NEQ:
						if (rsp->_.instance && rsp->_.instance->blueprint)
							value = (lsp->_.wnum.pArea != rsp->_.instance->blueprint->area) || (lsp->_.wnum.vnum != rsp->_.instance->blueprint->vnum);
						else
							value = lsp->_.wnum.pArea && lsp->_.wnum.vnum > 0;
						break;
					default:
						SETRET(nsr,INVALID);
						return true;		
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;		
					}

					break;
				}
			
			case NST_MOBILE:		// WIDEVNUM op MOBILE => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = rsp->_.mobile && (rsp->_.mobile->pIndexData != NULL) && (lsp->_.wnum.pArea == rsp->_.mobile->pIndexData->area) && (lsp->_.wnum.vnum == rsp->_.mobile->pIndexData->vnum); break;
					case NI_NEQ:
						if (rsp->_.mobile && rsp->_.mobile->pIndexData)
							value = (lsp->_.wnum.pArea != rsp->_.mobile->pIndexData->area) || (lsp->_.wnum.vnum != rsp->_.mobile->pIndexData->vnum);
						else
							value = lsp->_.wnum.pArea && lsp->_.wnum.vnum > 0;
						break;
					default:
						SETRET(nsr,INVALID);
						return true;		
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;		
					}

					break;
				}
			
			case NST_OBJECT:		// WIDEVNUM op OBJECT => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = rsp->_.object && (rsp->_.object->pIndexData != NULL) && (lsp->_.wnum.pArea == rsp->_.object->pIndexData->area) && (lsp->_.wnum.vnum == rsp->_.object->pIndexData->vnum); break;
					case NI_NEQ:
						if (rsp->_.object && rsp->_.object->pIndexData)
							value = (lsp->_.wnum.pArea != rsp->_.object->pIndexData->area) || (lsp->_.wnum.vnum != rsp->_.object->pIndexData->vnum);
						else
							value = lsp->_.wnum.pArea && lsp->_.wnum.vnum > 0;
					default:
						SETRET(nsr,INVALID);
						return true;		
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;		
					}

					break;
				}
			
			// case NST_QUEST:
			case NST_ROOM:			// WIDEVNUM op ROOM
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = rsp->_.room && (lsp->_.wnum.pArea == rsp->_.room->area) && (lsp->_.wnum.vnum == rsp->_.room->vnum); break;
					case NI_NEQ:
						if (rsp->_.room)
							value = (lsp->_.wnum.pArea != rsp->_.room->area) || (lsp->_.wnum.vnum != rsp->_.room->vnum);
						else
							value = lsp->_.wnum.pArea && lsp->_.wnum.vnum > 0;
						break;
					default:
						SETRET(nsr,INVALID);
						return true;		
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;		
					}

					break;
				}

			case NST_SHIP:
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = rsp->_.ship && (rsp->_.ship->index != NULL) && (lsp->_.wnum.pArea == rsp->_.ship->index->area) && (lsp->_.wnum.vnum == rsp->_.ship->index->vnum); break;
					case NI_NEQ:
						if (rsp->_.ship && rsp->_.ship->index)
							value = (lsp->_.wnum.pArea != rsp->_.ship->index->area) || (lsp->_.wnum.vnum != rsp->_.ship->index->vnum);
						else
							value = lsp->_.wnum.pArea && lsp->_.wnum.vnum > 0;
						break;
					default:
						SETRET(nsr,INVALID);
						return true;		
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;		
					}

					break;
				}
			
			case NST_TOKEN:
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = rsp->_.token && (rsp->_.token->pIndexData != NULL) && (lsp->_.wnum.pArea == rsp->_.token->pIndexData->area) && (lsp->_.wnum.vnum == rsp->_.token->pIndexData->vnum); break;
					case NI_NEQ:
						if (rsp->_.token && rsp->_.token->pIndexData)
							value = (lsp->_.wnum.pArea != rsp->_.token->pIndexData->area) || (lsp->_.wnum.vnum != rsp->_.token->pIndexData->vnum);
						else
							value = lsp->_.wnum.pArea && lsp->_.wnum.vnum > 0;
						break;
					default:
						SETRET(nsr,INVALID);
						return true;		
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;		
					}

					break;
				}
			
			case NST_NULL:			// WIDEVNUM op null
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.wnum.pArea == NULL) || (lsp->_.wnum.vnum < 1); break;
					case NI_NEQ:	value = (lsp->_.wnum.pArea != NULL) && (lsp->_.wnum.vnum > 0); break;
					default:
						SETRET(nsr,INVALID);
						return true;		
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;		
					}

					break;
				}

			case NST_LVALUE:		// WIDEVNUM op LVALUE
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_WIDEVNUM:		// WIDEVNUM op WIDEVNUM
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.wnum.pArea == rsp->_.lvalue._.wnum->pArea) && (lsp->_.wnum.vnum == rsp->_.lvalue._.wnum->vnum); break;
							case NI_NEQ:	value = (lsp->_.wnum.pArea != rsp->_.lvalue._.wnum->pArea) || (lsp->_.wnum.vnum != rsp->_.lvalue._.wnum->vnum); break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;		
							}
							break;
						}

					case NST_DUNGEON:		// WIDEVNUM op DUNGEON => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = *(rsp->_.lvalue._.dungeon) && ((*(rsp->_.lvalue._.dungeon))->index != NULL) && (lsp->_.wnum.pArea == (*(rsp->_.lvalue._.dungeon))->index->area) && (lsp->_.wnum.vnum == (*(rsp->_.lvalue._.dungeon))->index->vnum); break;
							case NI_NEQ:
								if (*(rsp->_.lvalue._.dungeon) && (*(rsp->_.lvalue._.dungeon))->index)
									value = (lsp->_.wnum.pArea != (*(rsp->_.lvalue._.dungeon))->index->area) || (lsp->_.wnum.vnum != (*(rsp->_.lvalue._.dungeon))->index->vnum);
								else
									value = lsp->_.wnum.pArea && lsp->_.wnum.vnum > 0;
								break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;		
							}

							break;
						}
					
					case NST_INSTANCE:		// WIDEVNUM op INSTANCE
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = *(rsp->_.lvalue._.instance) && ((*(rsp->_.lvalue._.instance))->blueprint != NULL) && (lsp->_.wnum.pArea == (*(rsp->_.lvalue._.instance))->blueprint->area) && (lsp->_.wnum.vnum == (*(rsp->_.lvalue._.instance))->blueprint->vnum); break;
							case NI_NEQ:
								if (*(rsp->_.lvalue._.instance) && (*(rsp->_.lvalue._.instance))->blueprint)
									value = (lsp->_.wnum.pArea != (*(rsp->_.lvalue._.instance))->blueprint->area) || (lsp->_.wnum.vnum != (*(rsp->_.lvalue._.instance))->blueprint->vnum);
								else
									value = lsp->_.wnum.pArea && lsp->_.wnum.vnum > 0;
								break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;		
							}

							break;
						}
					
					case NST_MOBILE:		// WIDEVNUM op MOBILE => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = *(rsp->_.lvalue._.mobile) && ((*(rsp->_.lvalue._.mobile))->pIndexData != NULL) && (lsp->_.wnum.pArea == (*(rsp->_.lvalue._.mobile))->pIndexData->area) && (lsp->_.wnum.vnum == (*(rsp->_.lvalue._.mobile))->pIndexData->vnum); break;
							case NI_NEQ:
								if (*(rsp->_.lvalue._.mobile) && (*(rsp->_.lvalue._.mobile))->pIndexData)
									value = (lsp->_.wnum.pArea != (*(rsp->_.lvalue._.mobile))->pIndexData->area) || (lsp->_.wnum.vnum != (*(rsp->_.lvalue._.mobile))->pIndexData->vnum);
								else
									value = lsp->_.wnum.pArea && lsp->_.wnum.vnum > 0;
								break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;		
							}

							break;
						}
					
					case NST_OBJECT:		// WIDEVNUM op OBJECT => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = *(rsp->_.lvalue._.object) && ((*(rsp->_.lvalue._.object))->pIndexData != NULL) && (lsp->_.wnum.pArea == (*(rsp->_.lvalue._.object))->pIndexData->area) && (lsp->_.wnum.vnum == (*(rsp->_.lvalue._.object))->pIndexData->vnum); break;
							case NI_NEQ:
								if (*(rsp->_.lvalue._.object) && (*(rsp->_.lvalue._.object))->pIndexData)
									value = (lsp->_.wnum.pArea != (*(rsp->_.lvalue._.object))->pIndexData->area) || (lsp->_.wnum.vnum != (*(rsp->_.lvalue._.object))->pIndexData->vnum);
								else
									value = lsp->_.wnum.pArea && lsp->_.wnum.vnum > 0;
								break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;		
							}

							break;
						}
					
					// case NST_QUEST:
					case NST_ROOM:			// WIDEVNUM op ROOM
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = *(rsp->_.lvalue._.room) && (lsp->_.wnum.pArea == (*(rsp->_.lvalue._.room))->area) && (lsp->_.wnum.vnum == (*(rsp->_.lvalue._.room))->vnum); break;
							case NI_NEQ:
								if (*(rsp->_.lvalue._.room))
									value = (lsp->_.wnum.pArea != (*(rsp->_.lvalue._.room))->area) || (lsp->_.wnum.vnum != (*(rsp->_.lvalue._.room))->vnum);
								else
									value = lsp->_.wnum.pArea && lsp->_.wnum.vnum > 0;
								break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;		
							}

							break;
						}

					case NST_SHIP:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = *(rsp->_.lvalue._.ship) && ((*(rsp->_.lvalue._.ship))->index != NULL) && (lsp->_.wnum.pArea == (*(rsp->_.lvalue._.ship))->index->area) && (lsp->_.wnum.vnum == (*(rsp->_.lvalue._.ship))->index->vnum); break;
							case NI_NEQ:
								if (*(rsp->_.lvalue._.ship) && (*(rsp->_.lvalue._.ship))->index)
									value = (lsp->_.wnum.pArea != (*(rsp->_.lvalue._.ship))->index->area) || (lsp->_.wnum.vnum != (*(rsp->_.lvalue._.ship))->index->vnum);
								else
									value = lsp->_.wnum.pArea && lsp->_.wnum.vnum > 0;
								break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;		
							}

							break;
						}
					
					case NST_TOKEN:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(rsp->_.lvalue._.token))->pIndexData != NULL) && (lsp->_.wnum.pArea == (*(rsp->_.lvalue._.token))->pIndexData->area) && (lsp->_.wnum.vnum == (*(rsp->_.lvalue._.token))->pIndexData->vnum); break;
							case NI_NEQ:
								if (*(rsp->_.lvalue._.token) && (*(rsp->_.lvalue._.token))->pIndexData)
									value = (lsp->_.wnum.pArea != (*(rsp->_.lvalue._.token))->pIndexData->area) || (lsp->_.wnum.vnum != (*(rsp->_.lvalue._.token))->pIndexData->vnum);
								else
									value = lsp->_.wnum.pArea && lsp->_.wnum.vnum > 0;
								break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;		
							}

							break;
						}
					
					default:
						SETRET(nsr,INVALID);
						return true;		
					}
				}

			default:
				SETRET(nsr,INVALID);
				return true;		
			}
			break;
		}

	case NST_BOOLEAN:		// BOOLEAN op ???
		{
			switch(rsp->type)
			{
			case NST_NUMBER:		// BOOLEAN op NUMBER => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_LAND:	value = (lsp->_.b) && (rsp->_.i != 0); break;
					case NI_LOR:	value = (lsp->_.b) || (rsp->_.i != 0); break;
					case NI_LXOR:	value = (lsp->_.b) != (rsp->_.i != 0); break;
					default:
						SETRET(nsr,INVALID);
						return true;		
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_BOOLEAN:		// BOOLEAN op BOOLEAN => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.b) == (rsp->_.b); break;
					case NI_NEQ:	value = (lsp->_.b) != (rsp->_.b); break;
					case NI_LAND:	value = (lsp->_.b) && (rsp->_.b); break;
					case NI_LOR:	value = (lsp->_.b) || (rsp->_.b); break;
					case NI_LXOR:	value = (lsp->_.b) != (rsp->_.b); break;
					default:
						SETRET(nsr,INVALID);
						return true;		
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:		// BOOLEAN op LVALUE
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:		// BOOLEAN op NUMBER => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_LAND:	value = (lsp->_.b) && (*(rsp->_.lvalue._.number) != 0); break;
							case NI_LOR:	value = (lsp->_.b) || (*(rsp->_.lvalue._.number) != 0); break;
							case NI_LXOR:	value = (lsp->_.b) != (*(rsp->_.lvalue._.number) != 0); break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_BOOLEAN:		// BOOLEAN op BOOLEAN => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.b) == *(rsp->_.lvalue._.b); break;
							case NI_NEQ:	value = (lsp->_.b) != *(rsp->_.lvalue._.b); break;
							case NI_LAND:	value = (lsp->_.b) && *(rsp->_.lvalue._.b); break;
							case NI_LOR:	value = (lsp->_.b) || *(rsp->_.lvalue._.b); break;
							case NI_LXOR:	value = (lsp->_.b) != *(rsp->_.lvalue._.b); break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_FLAG_BIT:		// BOOLEAN op BIT => BOOLEAN
						{
							bool set = IS_SET(*(rsp->_.lvalue._.bit.value),rsp->_.lvalue._.bit.bit);
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.b) == set; break;
							case NI_NEQ:	value = (lsp->_.b) != set; break;
							case NI_LAND:	value = (lsp->_.b) && set; break;
							case NI_LOR:	value = (lsp->_.b) || set; break;
							case NI_LXOR:	value = (lsp->_.b) != set; break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;		
					}
				}
			default:
				SETRET(nsr,INVALID);
				return true;		
			}
			break;
		}

	case NST_STAT:			// STAT op ???
		{
			switch(rsp->type)
			{
			case NST_NUMBER:		// STAT op NUMBER => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.stat.number) == (rsp->_.i); break;
					case NI_NEQ:	value = (lsp->_.stat.number) != (rsp->_.i); break;
					case NI_LT:		value = (lsp->_.stat.number) < (rsp->_.i); break;
					case NI_LE:		value = (lsp->_.stat.number) <= (rsp->_.i); break;
					case NI_GT:		value = (lsp->_.stat.number) > (rsp->_.i); break;
					case NI_GE:		value = (lsp->_.stat.number) >= (rsp->_.i); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_STAT:			// STAT op STAT => BOOLEAN
				{
					bool same = (lsp->_.stat.table) == (rsp->_.stat.table);
					bool value;
					switch(op)
					{
					case NI_EQ:		value = same && ((lsp->_.stat.number) == (rsp->_.stat.number)); break;
					case NI_NEQ:	value = !same || ((lsp->_.stat.number) != (rsp->_.stat.number)); break;
					case NI_LT:		value = same && ((lsp->_.stat.number) < (rsp->_.stat.number)); break;
					case NI_LE:		value = same && ((lsp->_.stat.number) <= (rsp->_.stat.number)); break;
					case NI_GT:		value = same && ((lsp->_.stat.number) > (rsp->_.stat.number)); break;
					case NI_GE:		value = same && ((lsp->_.stat.number) >= (rsp->_.stat.number)); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:		// STAT op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:		// STAT op NUMBER => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.stat.number) == *(rsp->_.lvalue._.number); break;
							case NI_NEQ:	value = (lsp->_.stat.number) != *(rsp->_.lvalue._.number); break;
							case NI_LT:		value = (lsp->_.stat.number) < *(rsp->_.lvalue._.number); break;
							case NI_LE:		value = (lsp->_.stat.number) <= *(rsp->_.lvalue._.number); break;
							case NI_GT:		value = (lsp->_.stat.number) > *(rsp->_.lvalue._.number); break;
							case NI_GE:		value = (lsp->_.stat.number) >= *(rsp->_.lvalue._.number); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_STAT:			// STAT op STAT => BOOLEAN
						{
							bool same = lsp->_.stat.table == rsp->_.lvalue._.stat.table;
							bool value;
							switch(op)
							{
							case NI_EQ:		value = same && ((lsp->_.stat.number) == *(rsp->_.lvalue._.stat.number)); break;
							case NI_NEQ:	value = same && ((lsp->_.stat.number) != *(rsp->_.lvalue._.stat.number)); break;
							case NI_LT:		value = same && ((lsp->_.stat.number) < *(rsp->_.lvalue._.stat.number)); break;
							case NI_LE:		value = same && ((lsp->_.stat.number) <= *(rsp->_.lvalue._.stat.number)); break;
							case NI_GT:		value = same && ((lsp->_.stat.number) > *(rsp->_.lvalue._.stat.number)); break;
							case NI_GE:		value = same && ((lsp->_.stat.number) >= *(rsp->_.lvalue._.stat.number)); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_ACCOUNT:		// ACCOUNT op ???
		{
			switch(rsp->type)
			{
			case NST_ACCOUNT:		// ACCOUNT op ACCOUNT => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.account) == (rsp->_.account); break;
					case NI_NEQ:	value = (lsp->_.account) != (rsp->_.account); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// ACCOUNT op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.account) == NULL; break;
					case NI_NEQ:	value = (lsp->_.account) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// ACCOUNT op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_ACCOUNT:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.account) == *(rsp->_.lvalue._.account); break;
							case NI_NEQ:	value = (lsp->_.account) != *(rsp->_.lvalue._.account); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_AFFECT:		// AFFECT op ???
		{
			switch(rsp->type)
			{
			case NST_AFFECT:		// AFFECT op AFFECT => BOOLEAN
				{
					// TODO: Add an affect_cmp function
					bool value;
					switch(op)
					{
					case NI_EQ:		value = affect_equal(lsp->_.affect, rsp->_.affect); break;
					case NI_NEQ:	value = !affect_equal(lsp->_.affect, rsp->_.affect); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// AFFECT op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.affect) == NULL; break;
					case NI_NEQ:	value = (lsp->_.affect) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// AFFECT op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_AFFECT:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = affect_equal(lsp->_.affect, *(rsp->_.lvalue._.affect)); break;
							case NI_NEQ:	value = !affect_equal(lsp->_.affect, *(rsp->_.lvalue._.affect)); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_AREA:			// AREA op ???
		{
			switch(rsp->type)
			{
			case NST_AREA:		// AREA op AREA => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.area) == (rsp->_.area); break;
					case NI_NEQ:	value = (lsp->_.area) != (rsp->_.area); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// AREA op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.area) == NULL; break;
					case NI_NEQ:	value = (lsp->_.area) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// AREA op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_AREA:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.area) == *(rsp->_.lvalue._.area); break;
							case NI_NEQ:	value = (lsp->_.area) != *(rsp->_.lvalue._.area); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	// case NST_CHANNEL:
	case NST_CLASS:			// CLASS op ???
		{
			switch(rsp->type)
			{
			case NST_CLASS:		// CLASS op CLASS => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.clazz) == (rsp->_.clazz); break;
					case NI_NEQ:	value = (lsp->_.clazz) != (rsp->_.clazz); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// CLASS op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.clazz) == NULL; break;
					case NI_NEQ:	value = (lsp->_.clazz) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// CLASS op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_CLASS:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.clazz) == *(rsp->_.lvalue._.clazz); break;
							case NI_NEQ:	value = (lsp->_.clazz) != *(rsp->_.lvalue._.clazz); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_DUNGEON:		// DUNGEON op ???
		{
			switch(rsp->type)
			{
			case NST_DUNGEON:		// DUNGEON op DUNGEON => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.dungeon) == (rsp->_.dungeon); break;
					case NI_NEQ:	value = (lsp->_.dungeon) != (rsp->_.dungeon); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_WIDEVNUM:		// DUNGEON op WIDEVNUM => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = lsp->_.dungeon && (lsp->_.dungeon && lsp->_.dungeon->index) && (lsp->_.dungeon->index->area == rsp->_.wnum.pArea) && (lsp->_.dungeon->index->vnum == rsp->_.wnum.vnum); break;
					case NI_NEQ:
						if (lsp->_.dungeon && lsp->_.dungeon->index)
							value = (lsp->_.dungeon->index->area != rsp->_.wnum.pArea) || (lsp->_.dungeon->index->vnum != rsp->_.wnum.vnum);
						else
							value = (rsp->_.wnum.pArea && rsp->_.wnum.vnum > 0);
						break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// DUNGEON op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.dungeon) == NULL; break;
					case NI_NEQ:	value = (lsp->_.dungeon) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// DUNGEON op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_DUNGEON:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.dungeon) == *(rsp->_.lvalue._.dungeon); break;
							case NI_NEQ:	value = (lsp->_.dungeon) != *(rsp->_.lvalue._.dungeon); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:			// DUNGEON op WIDEVNUM => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = lsp->_.dungeon && (lsp->_.dungeon->index != NULL) && (lsp->_.dungeon->index->area == rsp->_.lvalue._.wnum->pArea) && (lsp->_.dungeon->index->vnum == rsp->_.lvalue._.wnum->vnum); break;
							case NI_NEQ:
								if (lsp->_.dungeon && lsp->_.dungeon->index)
									value = (lsp->_.dungeon->index->area != rsp->_.lvalue._.wnum->pArea) || (lsp->_.dungeon->index->vnum != rsp->_.lvalue._.wnum->vnum);
								else
									value = (rsp->_.lvalue._.wnum->pArea && rsp->_.lvalue._.wnum->vnum > 0);
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_EXIT:			// EXIT op ???
		{
			switch(rsp->type)
			{
			case NST_EXIT:		// EXIT op EXIT => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.ex) == (rsp->_.ex); break;
					case NI_NEQ:	value = (lsp->_.ex) != (rsp->_.ex); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// EXIT op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.ex) == NULL; break;
					case NI_NEQ:	value = (lsp->_.ex) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// EXIT op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_EXIT:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.ex) == *(rsp->_.lvalue._.ex); break;
							case NI_NEQ:	value = (lsp->_.ex) != *(rsp->_.lvalue._.ex); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_INSTANCE:		// INSTANCE op ???
		{
			switch(rsp->type)
			{
			case NST_INSTANCE:		// INSTANCE op INSTANCE => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.instance) == (rsp->_.instance); break;
					case NI_NEQ:	value = (lsp->_.instance) != (rsp->_.instance); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_WIDEVNUM:		// INSTANCE op WIDEVNUM => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = lsp->_.instance && (lsp->_.instance && lsp->_.instance->blueprint) && (lsp->_.instance->blueprint->area == rsp->_.wnum.pArea) && (lsp->_.instance->blueprint->vnum == rsp->_.wnum.vnum); break;
					case NI_NEQ:
						if (lsp->_.instance && lsp->_.instance->blueprint)
							value = (lsp->_.instance->blueprint->area != rsp->_.wnum.pArea) || (lsp->_.instance->blueprint->vnum != rsp->_.wnum.vnum);
						else
							value = (rsp->_.wnum.pArea && rsp->_.wnum.vnum > 0);
						break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// INSTANCE op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.instance) == NULL; break;
					case NI_NEQ:	value = (lsp->_.instance) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// INSTANCE op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_INSTANCE:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.instance) == *(rsp->_.lvalue._.instance); break;
							case NI_NEQ:	value = (lsp->_.instance) != *(rsp->_.lvalue._.instance); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:			// INSTANCE op WIDEVNUM => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = lsp->_.instance && (lsp->_.instance->blueprint != NULL) && (lsp->_.instance->blueprint->area == rsp->_.lvalue._.wnum->pArea) && (lsp->_.instance->blueprint->vnum == rsp->_.lvalue._.wnum->vnum); break;
							case NI_NEQ:
								if (lsp->_.instance && lsp->_.instance->blueprint)
									value = (lsp->_.instance->blueprint->area != rsp->_.lvalue._.wnum->pArea) || (lsp->_.instance->blueprint->vnum != rsp->_.lvalue._.wnum->vnum);
								else
									value = (rsp->_.lvalue._.wnum->pArea && rsp->_.lvalue._.wnum->vnum > 0);
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_LIQUID:		// LIQUID op ???
		{
			switch(rsp->type)
			{
			case NST_LIQUID:		// LIQUID op LIQUID => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.liquid) == (rsp->_.liquid); break;
					case NI_NEQ:	value = (lsp->_.liquid) != (rsp->_.liquid); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// LIQUID op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.liquid) == NULL; break;
					case NI_NEQ:	value = (lsp->_.liquid) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// LIQUID op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_LIQUID:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.liquid) == *(rsp->_.lvalue._.liquid); break;
							case NI_NEQ:	value = (lsp->_.liquid) != *(rsp->_.lvalue._.liquid); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_MAIL:			// MAIL op ???
		{
			switch(rsp->type)
			{
			case NST_MAIL:		// MAIL op MAIL => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.mail) == (rsp->_.mail); break;
					case NI_NEQ:	value = (lsp->_.mail) != (rsp->_.mail); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// MAIL op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.mail) == NULL; break;
					case NI_NEQ:	value = (lsp->_.mail) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// MAIL op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_MAIL:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.mail) == *(rsp->_.lvalue._.mail); break;
							case NI_NEQ:	value = (lsp->_.mail) != *(rsp->_.lvalue._.mail); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_MATERIAL:		// MATERIAL op ???
		{
			switch(rsp->type)
			{
			case NST_MATERIAL:		// MATERIAL op MATERIAL => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.material) == (rsp->_.material); break;
					case NI_NEQ:	value = (lsp->_.material) != (rsp->_.material); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// MATERIAL op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.material) == NULL; break;
					case NI_NEQ:	value = (lsp->_.material) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// MATERIAL op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_MATERIAL:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.material) == *(rsp->_.lvalue._.material); break;
							case NI_NEQ:	value = (lsp->_.material) != *(rsp->_.lvalue._.material); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_MISSION:		// MISSION op ???
		{
			switch(rsp->type)
			{
			case NST_MISSION:		// MISSION op MISSION => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.mission) == (rsp->_.mission); break;
					case NI_NEQ:	value = (lsp->_.mission) != (rsp->_.mission); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// MISSION op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.mission) == NULL; break;
					case NI_NEQ:	value = (lsp->_.mission) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// MISSION op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_MISSION:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.mission) == *(rsp->_.lvalue._.mission); break;
							case NI_NEQ:	value = (lsp->_.mission) != *(rsp->_.lvalue._.mission); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_MOBILE:		// MOBILE op ???
		{
			switch(rsp->type)
			{
			case NST_MOBILE:		// MOBILE op MOBILE => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.mobile) == (rsp->_.mobile); break;
					case NI_NEQ:	value = (lsp->_.mobile) != (rsp->_.mobile); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_WIDEVNUM:		// MOBILE op WIDEVNUM => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.mobile && lsp->_.mobile->pIndexData != NULL) && (lsp->_.mobile->pIndexData->area == rsp->_.wnum.pArea) && (lsp->_.mobile->pIndexData->vnum == rsp->_.wnum.vnum); break;
					case NI_NEQ:
						if (lsp->_.mobile && lsp->_.mobile->pIndexData)
							value = (lsp->_.mobile->pIndexData->area != rsp->_.wnum.pArea) || (lsp->_.mobile->pIndexData->vnum != rsp->_.wnum.vnum);
						else
							value = (rsp->_.wnum.pArea && rsp->_.wnum.vnum > 0);
						break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:			// MOBILE op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.mobile) == NULL; break;
					case NI_NEQ:	value = (lsp->_.mobile) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:		// MOBILE op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_MOBILE:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.mobile) == *(rsp->_.lvalue._.mobile); break;
							case NI_NEQ:	value = (lsp->_.mobile) != *(rsp->_.lvalue._.mobile); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:			// MOBILE op WIDEVNUM => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.mobile && lsp->_.mobile->pIndexData != NULL) && (lsp->_.mobile->pIndexData->area == rsp->_.lvalue._.wnum->pArea) && (lsp->_.mobile->pIndexData->vnum == rsp->_.lvalue._.wnum->vnum); break;
							case NI_NEQ:
								if (lsp->_.mobile && lsp->_.mobile->pIndexData)
								{
									value = (lsp->_.mobile->pIndexData->area != rsp->_.lvalue._.wnum->pArea) || (lsp->_.mobile->pIndexData->vnum != rsp->_.lvalue._.wnum->vnum);
								}
								else
									value = (rsp->_.lvalue._.wnum->pArea && rsp->_.lvalue._.wnum->vnum > 0);
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_OBJECT:		// OBJECT op ???
		{
			switch(rsp->type)
			{
			case NST_OBJECT:		// OBJECT op OBJECT => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.object) == (rsp->_.object); break;
					case NI_NEQ:	value = (lsp->_.object) != (rsp->_.object); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_WIDEVNUM:		// OBJECT op WIDEVNUM => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.object && lsp->_.object->pIndexData != NULL) && (lsp->_.object->pIndexData->area == rsp->_.wnum.pArea) && (lsp->_.object->pIndexData->vnum == rsp->_.wnum.vnum); break;
					case NI_NEQ:
						if (lsp->_.object && lsp->_.object->pIndexData)
							value = (lsp->_.object->pIndexData->area != rsp->_.wnum.pArea) || (lsp->_.object->pIndexData->vnum != rsp->_.wnum.vnum);
						else
							value = (rsp->_.wnum.pArea && rsp->_.wnum.vnum > 0);
						break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:			// OBJECT op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.object) == NULL; break;
					case NI_NEQ:	value = (lsp->_.object) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:		// OBJECT op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_OBJECT:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.object) == *(rsp->_.lvalue._.object); break;
							case NI_NEQ:	value = (lsp->_.object) != *(rsp->_.lvalue._.object); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:			// OBJECT op WIDEVNUM => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.object && lsp->_.object->pIndexData != NULL) && (lsp->_.object->pIndexData->area == rsp->_.lvalue._.wnum->pArea) && (lsp->_.object->pIndexData->vnum == rsp->_.lvalue._.wnum->vnum); break;
							case NI_NEQ:
								if (lsp->_.object && lsp->_.object->pIndexData)
								{
									value = (lsp->_.object->pIndexData->area != rsp->_.lvalue._.wnum->pArea) || (lsp->_.object->pIndexData->vnum != rsp->_.lvalue._.wnum->vnum);
								}
								else
									value = (rsp->_.lvalue._.wnum->pArea && rsp->_.lvalue._.wnum->vnum > 0);
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_ORG:			// ORG op ???
		{
			switch(rsp->type)
			{
			case NST_ORG:		// ORG op ORG => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.org) == (rsp->_.org); break;
					case NI_NEQ:	value = (lsp->_.org) != (rsp->_.org); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// ORG op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.org) == NULL; break;
					case NI_NEQ:	value = (lsp->_.org) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// ORG op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_ORG:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.org) == *(rsp->_.lvalue._.org); break;
							case NI_NEQ:	value = (lsp->_.org) != *(rsp->_.lvalue._.org); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	// case NST_QUEST:
	case NST_RACE:			// RACE op ???
		{
			switch(rsp->type)
			{
			case NST_RACE:		// RACE op RACE => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.race) == (rsp->_.race); break;
					case NI_NEQ:	value = (lsp->_.race) != (rsp->_.race); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// RACE op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.race) == NULL; break;
					case NI_NEQ:	value = (lsp->_.race) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// RACE op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_RACE:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.race) == *(rsp->_.lvalue._.race); break;
							case NI_NEQ:	value = (lsp->_.race) != *(rsp->_.lvalue._.race); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_RANK:			// RANK op ???
		{
			switch(rsp->type)
			{
			case NST_RANK:		// RANK op RANK => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.rank) == (rsp->_.rank); break;
					case NI_NEQ:	value = (lsp->_.rank) != (rsp->_.rank); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// RANK op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.rank) == NULL; break;
					case NI_NEQ:	value = (lsp->_.rank) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// RANK op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_RANK:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.rank) == *(rsp->_.lvalue._.rank); break;
							case NI_NEQ:	value = (lsp->_.rank) != *(rsp->_.lvalue._.rank); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_REPUTATION:	// REPUTATION op ???
		{
			switch(rsp->type)
			{
			case NST_REPUTATION:		// REPUTATION op REPUTATION => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.reputation) == (rsp->_.reputation); break;
					case NI_NEQ:	value = (lsp->_.reputation) != (rsp->_.reputation); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// REPUTATION op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.reputation) == NULL; break;
					case NI_NEQ:	value = (lsp->_.reputation) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// REPUTATION op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_REPUTATION:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.reputation) == *(rsp->_.lvalue._.reputation); break;
							case NI_NEQ:	value = (lsp->_.reputation) != *(rsp->_.lvalue._.reputation); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_ROOM:			// ROOM op ???
		{
			switch(rsp->type)
			{
			case NST_ROOM:		// ROOM op ROOM => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.room) == (rsp->_.room); break;
					case NI_NEQ:	value = (lsp->_.room) != (rsp->_.room); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_WIDEVNUM:		// ROOM op WIDEVNUM => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.room) && (lsp->_.room->area == rsp->_.wnum.pArea) && (lsp->_.room->vnum == rsp->_.wnum.vnum); break;
					case NI_NEQ:	value = !(lsp->_.room) && ((lsp->_.room->area != rsp->_.wnum.pArea) || (lsp->_.room->vnum != rsp->_.wnum.vnum)); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:			// ROOM op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.room) == NULL; break;
					case NI_NEQ:	value = (lsp->_.room) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:		// ROOM op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_ROOM:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.room) == *(rsp->_.lvalue._.room); break;
							case NI_NEQ:	value = (lsp->_.room) != *(rsp->_.lvalue._.room); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:			// ROOM op WIDEVNUM => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.room->area == rsp->_.lvalue._.wnum->pArea) && (lsp->_.room->vnum == rsp->_.lvalue._.wnum->vnum); break;
							case NI_NEQ:	value = (lsp->_.room->area != rsp->_.lvalue._.wnum->pArea) || (lsp->_.room->vnum != rsp->_.lvalue._.wnum->vnum); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_SHIP:			// SHIP op ???
		{
			switch(rsp->type)
			{
			case NST_SHIP:		// SHIP op SHIP => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.ship) == (rsp->_.ship); break;
					case NI_NEQ:	value = (lsp->_.ship) != (rsp->_.ship); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_WIDEVNUM:		// SHIP op WIDEVNUM => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = lsp->_.ship && (lsp->_.ship && lsp->_.ship->index) && (lsp->_.ship->index->area == rsp->_.wnum.pArea) && (lsp->_.ship->index->vnum == rsp->_.wnum.vnum); break;
					case NI_NEQ:
						if (lsp->_.ship && lsp->_.ship->index)
							value = (lsp->_.ship->index->area != rsp->_.wnum.pArea) || (lsp->_.ship->index->vnum != rsp->_.wnum.vnum);
						else
							value = (rsp->_.wnum.pArea && rsp->_.wnum.vnum > 0);
						break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// SHIP op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.ship) == NULL; break;
					case NI_NEQ:	value = (lsp->_.ship) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// SHIP op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_SHIP:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.ship) == *(rsp->_.lvalue._.ship); break;
							case NI_NEQ:	value = (lsp->_.ship) != *(rsp->_.lvalue._.ship); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:			// SHIP op WIDEVNUM => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = lsp->_.ship && (lsp->_.ship->index != NULL) && (lsp->_.ship->index->area == rsp->_.lvalue._.wnum->pArea) && (lsp->_.ship->index->vnum == rsp->_.lvalue._.wnum->vnum); break;
							case NI_NEQ:
								if (lsp->_.ship && lsp->_.ship->index)
									value = (lsp->_.ship->index->area != rsp->_.lvalue._.wnum->pArea) || (lsp->_.ship->index->vnum != rsp->_.lvalue._.wnum->vnum);
								else
									value = (rsp->_.lvalue._.wnum->pArea && rsp->_.lvalue._.wnum->vnum > 0);
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_SKILL:			// SKILL op ???
		{
			switch(rsp->type)
			{
			case NST_SKILL:		// SKILL op SKILL => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.skill) == (rsp->_.skill); break;
					case NI_NEQ:	value = (lsp->_.skill) != (rsp->_.skill); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// SKILL op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.skill) == NULL; break;
					case NI_NEQ:	value = (lsp->_.skill) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// SKILL op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_SKILL:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.skill) == *(rsp->_.lvalue._.skill); break;
							case NI_NEQ:	value = (lsp->_.skill) != *(rsp->_.lvalue._.skill); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_TOKEN:			// TOKEN op ???
		{
			switch(rsp->type)
			{
			case NST_TOKEN:		// TOKEN op TOKEN => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.token) == (rsp->_.token); break;
					case NI_NEQ:	value = (lsp->_.token) != (rsp->_.token); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_WIDEVNUM:		// TOKEN op WIDEVNUM => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.token && lsp->_.token->pIndexData != NULL) && (lsp->_.token->pIndexData->area == rsp->_.wnum.pArea) && (lsp->_.token->pIndexData->vnum == rsp->_.wnum.vnum); break;
					case NI_NEQ:
						if (lsp->_.token && lsp->_.token->pIndexData)
							value = (lsp->_.token->pIndexData->area != rsp->_.wnum.pArea) || (lsp->_.token->pIndexData->vnum != rsp->_.wnum.vnum);
						else
							value = (rsp->_.wnum.pArea && rsp->_.wnum.vnum > 0);
						break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:			// TOKEN op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.token) == NULL; break;
					case NI_NEQ:	value = (lsp->_.token) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:		// TOKEN op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_TOKEN:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.token) == *(rsp->_.lvalue._.token); break;
							case NI_NEQ:	value = (lsp->_.token) != *(rsp->_.lvalue._.token); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:			// TOKEN op WIDEVNUM => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.token && lsp->_.token->pIndexData != NULL) && (lsp->_.token->pIndexData->area == rsp->_.lvalue._.wnum->pArea) && (lsp->_.token->pIndexData->vnum == rsp->_.lvalue._.wnum->vnum); break;
							case NI_NEQ:
								if (lsp->_.token && lsp->_.token->pIndexData)
								{
									value = (lsp->_.token->pIndexData->area != rsp->_.lvalue._.wnum->pArea) || (lsp->_.token->pIndexData->vnum != rsp->_.lvalue._.wnum->vnum);
								}
								else
									value = (rsp->_.lvalue._.wnum->pArea && rsp->_.lvalue._.wnum->vnum > 0);
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_WILDS:			// WILDS op ???
		{
			switch(rsp->type)
			{
			case NST_WILDS:		// WILDS op WILDS => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.wilds) == (rsp->_.wilds); break;
					case NI_NEQ:	value = (lsp->_.wilds) != (rsp->_.wilds); break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_NULL:		// WILDS op null => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (lsp->_.wilds) == NULL; break;
					case NI_NEQ:	value = (lsp->_.wilds) != NULL; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:	// WILDS op LVALUE => BOOLEAN
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_WILDS:
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.wilds) == *(rsp->_.lvalue._.wilds); break;
							case NI_NEQ:	value = (lsp->_.wilds) != *(rsp->_.lvalue._.wilds); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	// case NST_WORLD:
	case NST_NULL:
		{
			switch(rsp->type)
			{
			case NST_STRING:
				{
					bool value = IS_NULLSTR(rsp->_.str);
					if (rsp->_.str) free(rsp->_.str);
					switch(op)
					{
					case NI_EQ:		/* Already tested */break;
					case NI_NEQ:	value = !value; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_STRING_S:
				{
					bool value = IS_NULLSTR(rsp->_.str);
					switch(op)
					{
					case NI_EQ:		/* Already tested */break;
					case NI_NEQ:	value = !value; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_WIDEVNUM:
				{
					bool value = (rsp->_.wnum.pArea == NULL) || (rsp->_.wnum.vnum < 1);
					switch(op)
					{
					case NI_EQ:		/* Already tested */break;
					case NI_NEQ:	value = !value; break;
					default:
						SETRET(nsr,INVALID);
						return true;
					}

					if (!nib_push_stack_boolean(nsr,value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}


			__bool_nullvf(ACCOUNT,account)
			__bool_nullvf(AFFECT,affect)
			__bool_nullf(AREA,area)
			// __bool_nullvf(CHANNEL,channel)
			__bool_nullvf(CLASS,clazz)
			__bool_nullvf(DUNGEON,dungeon)
			__bool_nullvf(EXIT,ex)
			__bool_nullvf(INSTANCE,instance)
			__bool_nullvf(LIQUID,liquid)
			__bool_nullf(MAIL,mail)
			__bool_nullvf(MATERIAL,material)
			__bool_nullf(MISSION,mission)
			__bool_nullvf(MOBILE,mobile)
			__bool_nullvf(NOTE,note)
			__bool_nullvf(OBJECT,object)
			__bool_nullf(ORG,org)
			// __bool_nullvf(QUEST,quest)
			__bool_nullvf(RACE,race)
			__bool_nullvf(RANK,rank)
			__bool_nullvf(REPUTATION,reputation)
			__bool_nullf(ROOM,room)
			__bool_nullvf(SHIP,ship)
			__bool_nullvf(SKILL,skill)
			__bool_nullvf(TOKEN,token)
			__bool_nullvf(WILDS,wilds)
			// __bool_nullvf(WORLD,world)
			case NST_LVALUE:
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_STRING:
						{
							bool value = IS_NULLSTR(*(rsp->_.lvalue._.str));
							switch(op)
							{
							case NI_EQ:		/* Already tested */break;
							case NI_NEQ:	value = !value; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:
						{
							bool value = (rsp->_.lvalue._.wnum->pArea == NULL) || (rsp->_.lvalue._.wnum->vnum < 1);
							switch(op)
							{
							case NI_EQ:		/* Already tested */break;
							case NI_NEQ:	value = !value; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					__bool_nullvflv(ACCOUNT,account)
					__bool_nullvflv(AFFECT,affect)
					__bool_nullflv(AREA,area)
					// __bool_nullvflv(CHANNEL,channel)
					__bool_nullvflv(CLASS,clazz)
					__bool_nullvflv(DUNGEON,dungeon)
					__bool_nullvflv(EXIT,ex)
					__bool_nullvflv(INSTANCE,instance)
					__bool_nullvflv(LIQUID,liquid)
					__bool_nullflv(MAIL,mail)
					__bool_nullvflv(MATERIAL,material)
					__bool_nullflv(MISSION,mission)
					__bool_nullvflv(MOBILE,mobile)
					__bool_nullvflv(NOTE,note)
					__bool_nullvflv(OBJECT,object)
					__bool_nullflv(ORG,org)
					// __bool_nullvflv(QUEST,quest)
					__bool_nullvflv(RACE,race)
					__bool_nullvflv(RANK,rank)
					__bool_nullvflv(REPUTATION,reputation)
					__bool_nullflv(ROOM,room)
					__bool_nullvflv(SHIP,ship)
					__bool_nullvflv(SKILL,skill)
					__bool_nullvflv(TOKEN,token)
					__bool_nullvflv(WILDS,wilds)
					// __bool_nullvflv(WORLD,world)
					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			}
			break;
		}

	case NST_LVALUE:		// LVALUE op ???
		{
			switch(lsp->_.lvalue.type)
			{
			case NST_NUMBER:		// NUMBER op ???
				{
					switch(rsp->type)
					{
					case NST_NUMBER:	// NUMBER op NUMBER => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (*(lsp->_.lvalue._.number)) == (rsp->_.i); break;
							case NI_NEQ:	value = (*(lsp->_.lvalue._.number)) != (rsp->_.i); break;
							case NI_LT:		value = (*(lsp->_.lvalue._.number)) < (rsp->_.i); break;
							case NI_LE:		value = (*(lsp->_.lvalue._.number)) <= (rsp->_.i); break;
							case NI_GT:		value = (*(lsp->_.lvalue._.number)) > (rsp->_.i); break;
							case NI_GE:		value = (*(lsp->_.lvalue._.number)) >= (rsp->_.i); break;
							case NI_LAND:	value = (*(lsp->_.lvalue._.number) != 0) && (rsp->_.i != 0); break;
							case NI_LOR:	value = (*(lsp->_.lvalue._.number) != 0) || (rsp->_.i != 0); break;
							case NI_LXOR:	value = (*(lsp->_.lvalue._.number) != 0) != (rsp->_.i != 0); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_FLOAT:		// NUMBER op FLOAT => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((double)*(lsp->_.lvalue._.number)) == (rsp->_.d); break;
							case NI_NEQ:	value = ((double)*(lsp->_.lvalue._.number)) != (rsp->_.d); break;
							case NI_LT:		value = ((double)*(lsp->_.lvalue._.number)) < (rsp->_.d); break;
							case NI_LE:		value = ((double)*(lsp->_.lvalue._.number)) <= (rsp->_.d); break;
							case NI_GT:		value = ((double)*(lsp->_.lvalue._.number)) > (rsp->_.d); break;
							case NI_GE:		value = ((double)*(lsp->_.lvalue._.number)) >= (rsp->_.d); break;
							case NI_LAND:	value = (*(lsp->_.lvalue._.number) != 0) && (rsp->_.d != 0.0); break;
							case NI_LOR:	value = (*(lsp->_.lvalue._.number) != 0) || (rsp->_.d != 0.0); break;
							case NI_LXOR:	value = (*(lsp->_.lvalue._.number) != 0) != (rsp->_.d != 0.0); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_CHAR:		// NUMBER op CHAR => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (*(lsp->_.lvalue._.number)) == (rsp->_.ch); break;
							case NI_NEQ:	value = (*(lsp->_.lvalue._.number)) != (rsp->_.ch); break;
							case NI_LT:		value = (*(lsp->_.lvalue._.number)) < (rsp->_.ch); break;
							case NI_LE:		value = (*(lsp->_.lvalue._.number)) <= (rsp->_.ch); break;
							case NI_GT:		value = (*(lsp->_.lvalue._.number)) > (rsp->_.ch); break;
							case NI_GE:		value = (*(lsp->_.lvalue._.number)) >= (rsp->_.ch); break;
							case NI_LAND:	value = (*(lsp->_.lvalue._.number) != 0) && (rsp->_.ch != '\0'); break;
							case NI_LOR:	value = (*(lsp->_.lvalue._.number) != 0) || (rsp->_.ch != '\0'); break;
							case NI_LXOR:	value = (*(lsp->_.lvalue._.number) != 0) != (rsp->_.ch != '\0'); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_BOOLEAN:	// NUMBER op BOOLEAN => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_LAND:	value = (*(lsp->_.lvalue._.number) != 0) && (rsp->_.b); break;
							case NI_LOR:	value = (*(lsp->_.lvalue._.number) != 0) || (rsp->_.b); break;
							case NI_LXOR:	value = (*(lsp->_.lvalue._.number) != 0) != (rsp->_.b); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_FLAG:		// NUMBER op FLAG => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (*(lsp->_.lvalue._.number)) == (rsp->_.i); break;
							case NI_NEQ:	value = (*(lsp->_.lvalue._.number)) != (rsp->_.i); break;
							case NI_LAND:	value = (*(lsp->_.lvalue._.number) != 0) && (rsp->_.i != 0); break;
							case NI_LOR:	value = (*(lsp->_.lvalue._.number) != 0) || (rsp->_.i != 0); break;
							case NI_LXOR:	value = (*(lsp->_.lvalue._.number) != 0) != (rsp->_.i != 0); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_NUMBER:	// NUMBER op NUMBER => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = (*(lsp->_.lvalue._.number)) == (*(rsp->_.lvalue._.number)); break;
									case NI_NEQ:	value = (*(lsp->_.lvalue._.number)) != (*(rsp->_.lvalue._.number)); break;
									case NI_LT:		value = (*(lsp->_.lvalue._.number)) < (*(rsp->_.lvalue._.number)); break;
									case NI_LE:		value = (*(lsp->_.lvalue._.number)) <= (*(rsp->_.lvalue._.number)); break;
									case NI_GT:		value = (*(lsp->_.lvalue._.number)) > (*(rsp->_.lvalue._.number)); break;
									case NI_GE:		value = (*(lsp->_.lvalue._.number)) >= (*(rsp->_.lvalue._.number)); break;
									case NI_LAND:	value = (*(lsp->_.lvalue._.number) != 0) && (*(rsp->_.lvalue._.number) != 0); break;
									case NI_LOR:	value = (*(lsp->_.lvalue._.number) != 0) || (*(rsp->_.lvalue._.number) != 0); break;
									case NI_LXOR:	value = (*(lsp->_.lvalue._.number) != 0) != (*(rsp->_.lvalue._.number) != 0); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr, value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_FLOAT:		// NUMBER op FLOAT => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((double)*(lsp->_.lvalue._.number)) == (*(rsp->_.lvalue._.d)); break;
									case NI_NEQ:	value = ((double)*(lsp->_.lvalue._.number)) != (*(rsp->_.lvalue._.d)); break;
									case NI_LT:		value = ((double)*(lsp->_.lvalue._.number)) < (*(rsp->_.lvalue._.d)); break;
									case NI_LE:		value = ((double)*(lsp->_.lvalue._.number)) <= (*(rsp->_.lvalue._.d)); break;
									case NI_GT:		value = ((double)*(lsp->_.lvalue._.number)) > (*(rsp->_.lvalue._.d)); break;
									case NI_GE:		value = ((double)*(lsp->_.lvalue._.number)) >= (*(rsp->_.lvalue._.d)); break;
									case NI_LAND:	value = (*(lsp->_.lvalue._.number) != 0) && (*(rsp->_.lvalue._.d) != 0.0); break;
									case NI_LOR:	value = (*(lsp->_.lvalue._.number) != 0) || (*(rsp->_.lvalue._.d) != 0.0); break;
									case NI_LXOR:	value = (*(lsp->_.lvalue._.number) != 0) != (*(rsp->_.lvalue._.d) != 0.0); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr, value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_CHAR:		// NUMBER op CHAR => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = (*(lsp->_.lvalue._.number)) == (*(rsp->_.lvalue._.ch)); break;
									case NI_NEQ:	value = (*(lsp->_.lvalue._.number)) != (*(rsp->_.lvalue._.ch)); break;
									case NI_LT:		value = (*(lsp->_.lvalue._.number)) < (*(rsp->_.lvalue._.ch)); break;
									case NI_LE:		value = (*(lsp->_.lvalue._.number)) <= (*(rsp->_.lvalue._.ch)); break;
									case NI_GT:		value = (*(lsp->_.lvalue._.number)) > (*(rsp->_.lvalue._.ch)); break;
									case NI_GE:		value = (*(lsp->_.lvalue._.number)) >= (*(rsp->_.lvalue._.ch)); break;
									case NI_LAND:	value = (*(lsp->_.lvalue._.number) != 0) && (*(rsp->_.lvalue._.ch) != '\0'); break;
									case NI_LOR:	value = (*(lsp->_.lvalue._.number) != 0) || (*(rsp->_.lvalue._.ch) != '\0'); break;
									case NI_LXOR:	value = (*(lsp->_.lvalue._.number) != 0) != (*(rsp->_.lvalue._.ch) != '\0'); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr, value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_BOOLEAN:	// NUMBER op BOOLEAN => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_LAND:	value = (*(lsp->_.lvalue._.number) != 0) && (*(rsp->_.lvalue._.b)); break;
									case NI_LOR:	value = (*(lsp->_.lvalue._.number) != 0) || (*(rsp->_.lvalue._.b)); break;
									case NI_LXOR:	value = (*(lsp->_.lvalue._.number) != 0) != (*(rsp->_.lvalue._.b)); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr, value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_FLAG:		// NUMBER op FLAG => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = (*(lsp->_.lvalue._.number)) == (*(rsp->_.lvalue._.number)); break;
									case NI_NEQ:	value = (*(lsp->_.lvalue._.number)) != (*(rsp->_.lvalue._.number)); break;
									case NI_LAND:	value = (*(lsp->_.lvalue._.number) != 0) && (*(rsp->_.lvalue._.number) != 0); break;
									case NI_LOR:	value = (*(lsp->_.lvalue._.number) != 0) || (*(rsp->_.lvalue._.number) != 0); break;
									case NI_LXOR:	value = (*(lsp->_.lvalue._.number) != 0) != (*(rsp->_.lvalue._.number) != 0); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr, value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_FLOAT:			// FLOAT op ???
				{
					switch(rsp->type)
					{
					case NST_NUMBER:	// FLOAT op NUMBER => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (*(lsp->_.lvalue._.d)) == ((double)rsp->_.i); break;
							case NI_NEQ:	value = (*(lsp->_.lvalue._.d)) != ((double)rsp->_.i); break;
							case NI_LT:		value = (*(lsp->_.lvalue._.d)) < ((double)rsp->_.i); break;
							case NI_LE:		value = (*(lsp->_.lvalue._.d)) <= ((double)rsp->_.i); break;
							case NI_GT:		value = (*(lsp->_.lvalue._.d)) > ((double)rsp->_.i); break;
							case NI_GE:		value = (*(lsp->_.lvalue._.d)) >= ((double)rsp->_.i); break;
							case NI_LAND:	value = (*(lsp->_.lvalue._.d) != 0.0) && (rsp->_.i != 0); break;
							case NI_LOR:	value = (*(lsp->_.lvalue._.d) != 0.0) || (rsp->_.i != 0); break;
							case NI_LXOR:	value = (*(lsp->_.lvalue._.d) != 0.0) != (rsp->_.i != 0); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_FLOAT:		// FLOAT op FLOAT => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (*(lsp->_.lvalue._.d)) == (rsp->_.d); break;
							case NI_NEQ:	value = (*(lsp->_.lvalue._.d)) != (rsp->_.d); break;
							case NI_LT:		value = (*(lsp->_.lvalue._.d)) < (rsp->_.d); break;
							case NI_LE:		value = (*(lsp->_.lvalue._.d)) <= (rsp->_.d); break;
							case NI_GT:		value = (*(lsp->_.lvalue._.d)) > (rsp->_.d); break;
							case NI_GE:		value = (*(lsp->_.lvalue._.d)) >= (rsp->_.d); break;
							case NI_LAND:	value = (*(lsp->_.lvalue._.d) != 0.0) && (rsp->_.d != 0.0); break;
							case NI_LOR:	value = (*(lsp->_.lvalue._.d) != 0.0) || (rsp->_.d != 0.0); break;
							case NI_LXOR:	value = (*(lsp->_.lvalue._.d) != 0.0) != (rsp->_.d != 0.0); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// FLOAT op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_NUMBER:	// FLOAT op NUMBER => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = (*(lsp->_.lvalue._.d)) == ((double)*(rsp->_.lvalue._.number)); break;
									case NI_NEQ:	value = (*(lsp->_.lvalue._.d)) != ((double)*(rsp->_.lvalue._.number)); break;
									case NI_LT:		value = (*(lsp->_.lvalue._.d)) < ((double)*(rsp->_.lvalue._.number)); break;
									case NI_LE:		value = (*(lsp->_.lvalue._.d)) <= ((double)*(rsp->_.lvalue._.number)); break;
									case NI_GT:		value = (*(lsp->_.lvalue._.d)) > ((double)*(rsp->_.lvalue._.number)); break;
									case NI_GE:		value = (*(lsp->_.lvalue._.d)) >= ((double)*(rsp->_.lvalue._.number)); break;
									case NI_LAND:	value = (*(lsp->_.lvalue._.d) != 0.0) && (*(rsp->_.lvalue._.number) != 0); break;
									case NI_LOR:	value = (*(lsp->_.lvalue._.d) != 0.0) || (*(rsp->_.lvalue._.number) != 0); break;
									case NI_LXOR:	value = (*(lsp->_.lvalue._.d) != 0.0) != (*(rsp->_.lvalue._.number) != 0); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr, value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_FLOAT:		// FLOAT op FLOAT => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = (*(lsp->_.lvalue._.d)) == (*(rsp->_.lvalue._.d)); break;
									case NI_NEQ:	value = (*(lsp->_.lvalue._.d)) != (*(rsp->_.lvalue._.d)); break;
									case NI_LT:		value = (*(lsp->_.lvalue._.d)) < (*(rsp->_.lvalue._.d)); break;
									case NI_LE:		value = (*(lsp->_.lvalue._.d)) <= (*(rsp->_.lvalue._.d)); break;
									case NI_GT:		value = (*(lsp->_.lvalue._.d)) > (*(rsp->_.lvalue._.d)); break;
									case NI_GE:		value = (*(lsp->_.lvalue._.d)) >= (*(rsp->_.lvalue._.d)); break;
									case NI_LAND:	value = (*(lsp->_.lvalue._.d) != 0.0) && (*(rsp->_.lvalue._.d) != 0.0); break;
									case NI_LOR:	value = (*(lsp->_.lvalue._.d) != 0.0) || (*(rsp->_.lvalue._.d) != 0.0); break;
									case NI_LXOR:	value = (*(lsp->_.lvalue._.d) != 0.0) != (*(rsp->_.lvalue._.d) != 0.0); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr, value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_CHAR:			// CHAR op ???
				{
					switch(rsp->type)
					{
					case NST_NUMBER:	// CHAR op NUMBER => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (*(lsp->_.lvalue._.ch)) == (rsp->_.i); break;
							case NI_NEQ:	value = (*(lsp->_.lvalue._.ch)) != (rsp->_.i); break;
							case NI_LT:		value = (*(lsp->_.lvalue._.ch)) < (rsp->_.i); break;
							case NI_LE:		value = (*(lsp->_.lvalue._.ch)) <= (rsp->_.i); break;
							case NI_GT:		value = (*(lsp->_.lvalue._.ch)) > (rsp->_.i); break;
							case NI_GE:		value = (*(lsp->_.lvalue._.ch)) >= (rsp->_.i); break;
							case NI_LAND:	value = (*(lsp->_.lvalue._.ch) != '\0') && (rsp->_.i != 0); break;
							case NI_LOR:	value = (*(lsp->_.lvalue._.ch) != '\0') || (rsp->_.i != 0); break;
							case NI_LXOR:	value = (*(lsp->_.lvalue._.ch) != '\0') != (rsp->_.i != 0); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_CHAR:		// CHAR op CHAR => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (*(lsp->_.lvalue._.ch)) == (rsp->_.ch); break;
							case NI_NEQ:	value = (*(lsp->_.lvalue._.ch)) != (rsp->_.ch); break;
							case NI_LT:		value = (*(lsp->_.lvalue._.ch)) < (rsp->_.ch); break;
							case NI_LE:		value = (*(lsp->_.lvalue._.ch)) <= (rsp->_.ch); break;
							case NI_GT:		value = (*(lsp->_.lvalue._.ch)) > (rsp->_.ch); break;
							case NI_GE:		value = (*(lsp->_.lvalue._.ch)) >= (rsp->_.ch); break;
							case NI_LAND:	value = (*(lsp->_.lvalue._.ch) != '\0') && (rsp->_.ch != '\0'); break;
							case NI_LOR:	value = (*(lsp->_.lvalue._.ch) != '\0') || (rsp->_.ch != '\0'); break;
							case NI_LXOR:	value = (*(lsp->_.lvalue._.ch) != '\0') != (rsp->_.ch != '\0'); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr, value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// CHAR op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_NUMBER:	// CHAR op NUMBER => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = (*(lsp->_.lvalue._.ch)) == (*(rsp->_.lvalue._.number)); break;
									case NI_NEQ:	value = (*(lsp->_.lvalue._.ch)) != (*(rsp->_.lvalue._.number)); break;
									case NI_LT:		value = (*(lsp->_.lvalue._.ch)) < (*(rsp->_.lvalue._.number)); break;
									case NI_LE:		value = (*(lsp->_.lvalue._.ch)) <= (*(rsp->_.lvalue._.number)); break;
									case NI_GT:		value = (*(lsp->_.lvalue._.ch)) > (*(rsp->_.lvalue._.number)); break;
									case NI_GE:		value = (*(lsp->_.lvalue._.ch)) >= (*(rsp->_.lvalue._.number)); break;
									case NI_LAND:	value = (*(lsp->_.lvalue._.ch) != '\0') && (*(rsp->_.lvalue._.number) != 0); break;
									case NI_LOR:	value = (*(lsp->_.lvalue._.ch) != '\0') || (*(rsp->_.lvalue._.number) != 0); break;
									case NI_LXOR:	value = (*(lsp->_.lvalue._.ch) != '\0') != (*(rsp->_.lvalue._.number) != 0); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr, value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_CHAR:		// CHAR op CHAR => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = (*(lsp->_.lvalue._.ch)) == (*(rsp->_.lvalue._.ch)); break;
									case NI_NEQ:	value = (*(lsp->_.lvalue._.ch)) != (*(rsp->_.lvalue._.ch)); break;
									case NI_LT:		value = (*(lsp->_.lvalue._.ch)) < (*(rsp->_.lvalue._.ch)); break;
									case NI_LE:		value = (*(lsp->_.lvalue._.ch)) <= (*(rsp->_.lvalue._.ch)); break;
									case NI_GT:		value = (*(lsp->_.lvalue._.ch)) > (*(rsp->_.lvalue._.ch)); break;
									case NI_GE:		value = (*(lsp->_.lvalue._.ch)) >= (*(rsp->_.lvalue._.ch)); break;
									case NI_LAND:	value = (*(lsp->_.lvalue._.ch) != '\0') && (*(rsp->_.lvalue._.ch) != '\0'); break;
									case NI_LOR:	value = (*(lsp->_.lvalue._.ch) != '\0') || (*(rsp->_.lvalue._.ch) != '\0'); break;
									case NI_LXOR:	value = (*(lsp->_.lvalue._.ch) != '\0') != (*(rsp->_.lvalue._.ch) != '\0'); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr, value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}		

			case NST_STRING:		// STRING op ???
				{
					switch(rsp->type)
					{
					case NST_STRING:	// STRING op STRING
						{
							bool value;
							switch(op)
							{
							case NI_EQ:			value = (utf8_str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) == 0); break;
							case NI_NEQ:		value = (utf8_str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) != 0); break;
							case NI_LT:			value = (utf8_str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) < 0); break;
							case NI_LE:			value = (utf8_str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) <= 0); break;
							case NI_GT:			value = (utf8_str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) > 0); break;
							case NI_GE:			value = (utf8_str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) >= 0); break;
							case NI_STR_PREFIX: value = !utf8_str_prefix(rsp->_.str,*(lsp->_.lvalue._.str)); break;
							case NI_STR_INFIX:	value = !utf8_str_infix(rsp->_.str,*(lsp->_.lvalue._.str)); break;
							case NI_STR_SUFFIX:	value = !utf8_str_suffix(rsp->_.str,*(lsp->_.lvalue._.str)); break;
							case NI_LAND:		value = (!IS_NULLSTR(*(lsp->_.lvalue._.str))) && (!IS_NULLSTR(rsp->_.str)); break;
							case NI_LOR:		value = (!IS_NULLSTR(*(lsp->_.lvalue._.str))) || (!IS_NULLSTR(rsp->_.str)); break;
							case NI_LXOR:		value = (!IS_NULLSTR(*(lsp->_.lvalue._.str))) != (!IS_NULLSTR(rsp->_.str)); break;
							default:
								if (rsp->_.str) free(rsp->_.str);
								SETRET(nsr,INVALID);
								return true;
							}

							if (rsp->_.str) free(rsp->_.str);

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}
					
					case NST_STRING_S:	// STRING op STRING(s)
						{
							bool value;
							switch(op)
							{
							case NI_EQ:			value = (utf8_str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) == 0); break;
							case NI_NEQ:		value = (utf8_str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) != 0); break;
							case NI_LT:			value = (utf8_str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) < 0); break;
							case NI_LE:			value = (utf8_str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) <= 0); break;
							case NI_GT:			value = (utf8_str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) > 0); break;
							case NI_GE:			value = (utf8_str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) >= 0); break;
							case NI_STR_PREFIX:	value = !utf8_str_prefix(rsp->_.str,*(lsp->_.lvalue._.str)); break;
							case NI_STR_INFIX:	value = !utf8_str_infix(rsp->_.str,*(lsp->_.lvalue._.str)); break;
							case NI_STR_SUFFIX:	value = !utf8_str_suffix(rsp->_.str,*(lsp->_.lvalue._.str)); break;
							case NI_LAND:		value = (!IS_NULLSTR(*(lsp->_.lvalue._.str))) && (!IS_NULLSTR(rsp->_.str)); break;
							case NI_LOR:		value = (!IS_NULLSTR(*(lsp->_.lvalue._.str))) || (!IS_NULLSTR(rsp->_.str)); break;
							case NI_LXOR:		value = (!IS_NULLSTR(*(lsp->_.lvalue._.str))) != (!IS_NULLSTR(rsp->_.str)); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// STRING op null
						{
							bool value = (*(lsp->_.lvalue._.str) == NULL);

							switch(op)
							{
							case NI_EQ:		break;	// Value already set
							case NI_NEQ:	value = !value; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// STRING op LVALUE
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_STRING:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:			value = (utf8_str_cmp(*(lsp->_.lvalue._.str),*(rsp->_.lvalue._.str)) == 0); break;
									case NI_NEQ:		value = (utf8_str_cmp(*(lsp->_.lvalue._.str),*(rsp->_.lvalue._.str)) != 0); break;
									case NI_LT:			value = (utf8_str_cmp(*(lsp->_.lvalue._.str),*(rsp->_.lvalue._.str)) < 0); break;
									case NI_LE:			value = (utf8_str_cmp(*(lsp->_.lvalue._.str),*(rsp->_.lvalue._.str)) <= 0); break;
									case NI_GT:			value = (utf8_str_cmp(*(lsp->_.lvalue._.str),*(rsp->_.lvalue._.str)) > 0); break;
									case NI_GE:			value = (utf8_str_cmp(*(lsp->_.lvalue._.str),*(rsp->_.lvalue._.str)) >= 0); break;
									case NI_STR_PREFIX: value = !utf8_str_prefix(*(rsp->_.lvalue._.str),*(lsp->_.lvalue._.str)); break;
									case NI_STR_INFIX:	value = !utf8_str_infix(*(rsp->_.lvalue._.str),*(lsp->_.lvalue._.str)); break;
									case NI_STR_SUFFIX:	value = !utf8_str_suffix(*(rsp->_.lvalue._.str),*(lsp->_.lvalue._.str)); break;
									case NI_LAND:		value = (!IS_NULLSTR(*(lsp->_.lvalue._.str))) && (!IS_NULLSTR(*(rsp->_.lvalue._.str))); break;
									case NI_LOR:		value = (!IS_NULLSTR(*(lsp->_.lvalue._.str))) || (!IS_NULLSTR(*(rsp->_.lvalue._.str))); break;
									case NI_LXOR:		value = (!IS_NULLSTR(*(lsp->_.lvalue._.str))) != (!IS_NULLSTR(*(rsp->_.lvalue._.str))); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}
							
							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_FLAG:			// FLAG op ???
				{
					switch(rsp->type)
					{
					case NST_NUMBER:	// FLAG op NUMBER => BOOLEAN
						{
							// Both are handled exactly the same way
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (*(lsp->_.lvalue._.stat.number)) == (rsp->_.i); break;
							case NI_NEQ:	value = (*(lsp->_.lvalue._.stat.number)) != (rsp->_.i); break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
						}

					case NST_FLAG:		// FLAG op FLAG => BOOLEAN
						{
							bool same = lsp->_.lvalue._.stat.table == rsp->_.stat.table;
							bool value;
							switch(op)
							{
							case NI_EQ:		value = same && ((*(lsp->_.lvalue._.stat.number)) == (rsp->_.stat.number)); break;
							case NI_NEQ:	value = !same || ((*(lsp->_.lvalue._.stat.number)) != (rsp->_.stat.number)); break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
						}

					case NST_LVALUE:
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_NUMBER:	// FLAG op NUMBER => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = (*(lsp->_.lvalue._.stat.number)) == (*(rsp->_.lvalue._.number)); break;
									case NI_NEQ:	value = (*(lsp->_.lvalue._.stat.number)) != (*(rsp->_.lvalue._.number)); break;
									default:
										SETRET(nsr,INVALID);
										return true;		
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
								}

							case NST_FLAG:		// FLAG op FLAG => BOOLEAN
								{
									bool same = lsp->_.lvalue._.stat.table == rsp->_.lvalue._.stat.table;
									bool value;
									switch(op)
									{
									case NI_EQ:		value = same && ((*(lsp->_.lvalue._.stat.number)) == (*(rsp->_.lvalue._.stat.number))); break;
									case NI_NEQ:	value = !same || ((*(lsp->_.lvalue._.stat.number)) != (*(rsp->_.lvalue._.stat.number))); break;
									default:
										SETRET(nsr,INVALID);
										return true;		
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
								}

							default:
								SETRET(nsr,INVALID);
								return true;		
							}
							break;
						}			

					default:
						SETRET(nsr,INVALID);
						return true;		
					}
					break;
				}

			case NST_WIDEVNUM:		// WIDEVNUM op ???
				{
					switch(rsp->type)
					{
					case NST_WIDEVNUM:		// WIDEVNUM op WIDEVNUM
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.lvalue._.wnum->pArea == rsp->_.wnum.pArea) && (lsp->_.lvalue._.wnum->vnum == rsp->_.wnum.vnum); break;
							case NI_NEQ:	value = (lsp->_.lvalue._.wnum->pArea != rsp->_.wnum.pArea) || (lsp->_.lvalue._.wnum->vnum != rsp->_.wnum.vnum); break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;		
							}

							break;
						}

					// case NST_DUNGEON:
					// case NST_INSTANCE:
					case NST_MOBILE:		// WIDEVNUM op MOBILE => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (rsp->_.mobile->pIndexData != NULL) && (lsp->_.lvalue._.wnum->pArea == rsp->_.mobile->pIndexData->area) && (lsp->_.lvalue._.wnum->vnum == rsp->_.mobile->pIndexData->vnum); break;
							case NI_NEQ:	value = (rsp->_.mobile->pIndexData == NULL) || (lsp->_.lvalue._.wnum->pArea != rsp->_.mobile->pIndexData->area) || (lsp->_.lvalue._.wnum->vnum != rsp->_.mobile->pIndexData->vnum); break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;		
							}

							break;
						}
					
					// case NST_OBJECT:
					// case NST_QUEST:
					case NST_ROOM:			// WIDEVNUM op ROOM
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.lvalue._.wnum->pArea == rsp->_.room->area) && (lsp->_.lvalue._.wnum->vnum == rsp->_.room->vnum); break;
							case NI_NEQ:	value = (lsp->_.lvalue._.wnum->pArea != rsp->_.room->area) || (lsp->_.lvalue._.wnum->vnum != rsp->_.room->vnum); break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;		
							}

							break;
						}

					// case NST_SHIP:
					// case NST_TOKEN:
					case NST_NULL:			// WIDEVNUM op null
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (lsp->_.lvalue._.wnum->pArea == NULL) || (lsp->_.lvalue._.wnum->vnum < 1); break;
							case NI_NEQ:	value = (lsp->_.lvalue._.wnum->pArea != NULL) && (lsp->_.lvalue._.wnum->vnum > 0); break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;		
							}

							break;
						}

					case NST_LVALUE:		// WIDEVNUM op LVALUE
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_WIDEVNUM:		// WIDEVNUM op WIDEVNUM
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = (lsp->_.lvalue._.wnum->pArea == rsp->_.lvalue._.wnum->pArea) && (lsp->_.lvalue._.wnum->vnum == rsp->_.lvalue._.wnum->vnum); break;
									case NI_NEQ:	value = (lsp->_.lvalue._.wnum->pArea != rsp->_.lvalue._.wnum->pArea) || (lsp->_.lvalue._.wnum->vnum != rsp->_.lvalue._.wnum->vnum); break;
									default:
										SETRET(nsr,INVALID);
										return true;		
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;		
									}
									break;
								}

							// case NST_DUNGEON:
							// case NST_INSTANCE:
							case NST_MOBILE:		// WIDEVNUM op MOBILE => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(rsp->_.lvalue._.mobile))->pIndexData != NULL) && (lsp->_.lvalue._.wnum->pArea == (*(rsp->_.lvalue._.mobile))->pIndexData->area) && (lsp->_.lvalue._.wnum->vnum == (*(rsp->_.lvalue._.mobile))->pIndexData->vnum); break;
									case NI_NEQ:	value = ((*(rsp->_.lvalue._.mobile))->pIndexData == NULL) || (lsp->_.lvalue._.wnum->pArea != (*(rsp->_.lvalue._.mobile))->pIndexData->area) || (lsp->_.lvalue._.wnum->vnum != (*(rsp->_.lvalue._.mobile))->pIndexData->vnum); break;
									default:
										SETRET(nsr,INVALID);
										return true;		
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;		
									}

									break;
								}
							
							// case NST_OBJECT:
							// case NST_QUEST:
							case NST_ROOM:			// WIDEVNUM op ROOM
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = (lsp->_.lvalue._.wnum->pArea == (*(rsp->_.lvalue._.room))->area) && (lsp->_.lvalue._.wnum->vnum == (*(rsp->_.lvalue._.room))->vnum); break;
									case NI_NEQ:	value = (lsp->_.lvalue._.wnum->pArea != (*(rsp->_.lvalue._.room))->area) || (lsp->_.lvalue._.wnum->vnum != (*(rsp->_.lvalue._.room))->vnum); break;
									default:
										SETRET(nsr,INVALID);
										return true;		
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;		
									}

									break;
								}

							// case NST_SHIP:
							// case NST_TOKEN:
							default:
								SETRET(nsr,INVALID);
								return true;		
							}
						}

					default:
						SETRET(nsr,INVALID);
						return true;		
					}
					break;
				}

			case NST_BOOLEAN:		// BOOLEAN op ???
				{
					switch(rsp->type)
					{
					case NST_NUMBER:		// BOOLEAN op NUMBER => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_LAND:	value = *(lsp->_.lvalue._.b) && (rsp->_.i != 0); break;
							case NI_LOR:	value = *(lsp->_.lvalue._.b) || (rsp->_.i != 0); break;
							case NI_LXOR:	value = *(lsp->_.lvalue._.b) != (rsp->_.i != 0); break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_BOOLEAN:		// BOOLEAN op BOOLEAN => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = *(lsp->_.lvalue._.b) == (rsp->_.b); break;
							case NI_NEQ:	value = *(lsp->_.lvalue._.b) != (rsp->_.b); break;
							case NI_LAND:	value = *(lsp->_.lvalue._.b) && (rsp->_.b); break;
							case NI_LOR:	value = *(lsp->_.lvalue._.b) || (rsp->_.b); break;
							case NI_LXOR:	value = *(lsp->_.lvalue._.b) != (rsp->_.b); break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:		// BOOLEAN op LVALUE
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_NUMBER:		// BOOLEAN op NUMBER => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_LAND:	value = *(lsp->_.lvalue._.b) && (*(rsp->_.lvalue._.number) != 0); break;
									case NI_LOR:	value = *(lsp->_.lvalue._.b) || (*(rsp->_.lvalue._.number) != 0); break;
									case NI_LXOR:	value = *(lsp->_.lvalue._.b) != (*(rsp->_.lvalue._.number) != 0); break;
									default:
										SETRET(nsr,INVALID);
										return true;		
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_BOOLEAN:		// BOOLEAN op BOOLEAN => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = *(lsp->_.lvalue._.b) == *(rsp->_.lvalue._.b); break;
									case NI_NEQ:	value = *(lsp->_.lvalue._.b) != *(rsp->_.lvalue._.b); break;
									case NI_LAND:	value = *(lsp->_.lvalue._.b) && *(rsp->_.lvalue._.b); break;
									case NI_LOR:	value = *(lsp->_.lvalue._.b) || *(rsp->_.lvalue._.b); break;
									case NI_LXOR:	value = *(lsp->_.lvalue._.b) != *(rsp->_.lvalue._.b); break;
									default:
										SETRET(nsr,INVALID);
										return true;		
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_FLAG_BIT:		// BOOLEAN op BIT => BOOLEAN
								{
									bool set = IS_SET(*(rsp->_.lvalue._.bit.value),rsp->_.lvalue._.bit.bit);
									bool value;
									switch(op)
									{
									case NI_EQ:		value = *(lsp->_.lvalue._.b) == set; break;
									case NI_NEQ:	value = *(lsp->_.lvalue._.b) != set; break;
									case NI_LAND:	value = *(lsp->_.lvalue._.b) && set; break;
									case NI_LOR:	value = *(lsp->_.lvalue._.b) || set; break;
									case NI_LXOR:	value = *(lsp->_.lvalue._.b) != set; break;
									default:
										SETRET(nsr,INVALID);
										return true;		
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;		
							}
						}
					default:
						SETRET(nsr,INVALID);
						return true;		
					}
					break;
				}

			case NST_FLAG_BIT:		// BIT op ???
				{
					bool lset = IS_SET(*(lsp->_.lvalue._.bit.value),lsp->_.lvalue._.bit.bit);
					switch(rsp->type)
					{
					case NST_NUMBER:		// BIT op NUMBER => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_LAND:	value = lset && (rsp->_.i != 0); break;
							case NI_LOR:	value = lset || (rsp->_.i != 0); break;
							case NI_LXOR:	value = lset != (rsp->_.i != 0); break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_BOOLEAN:		// BIT op BOOLEAN => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = lset == (rsp->_.b); break;
							case NI_NEQ:	value = lset != (rsp->_.b); break;
							case NI_LAND:	value = lset && (rsp->_.b); break;
							case NI_LOR:	value = lset || (rsp->_.b); break;
							case NI_LXOR:	value = lset != (rsp->_.b); break;
							default:
								SETRET(nsr,INVALID);
								return true;		
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:		// BOOLEAN op LVALUE
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_NUMBER:		// BIT op NUMBER => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_LAND:	value = lset && (*(rsp->_.lvalue._.number) != 0); break;
									case NI_LOR:	value = lset || (*(rsp->_.lvalue._.number) != 0); break;
									case NI_LXOR:	value = lset != (*(rsp->_.lvalue._.number) != 0); break;
									default:
										SETRET(nsr,INVALID);
										return true;		
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_BOOLEAN:		// BIT op BOOLEAN => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = lset == *(rsp->_.lvalue._.b); break;
									case NI_NEQ:	value = lset != *(rsp->_.lvalue._.b); break;
									case NI_LAND:	value = lset && *(rsp->_.lvalue._.b); break;
									case NI_LOR:	value = lset || *(rsp->_.lvalue._.b); break;
									case NI_LXOR:	value = lset != *(rsp->_.lvalue._.b); break;
									default:
										SETRET(nsr,INVALID);
										return true;		
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_FLAG_BIT:		// BIT op BIT => BOOLEAN
								{
									bool rset = IS_SET(*(rsp->_.lvalue._.bit.value),rsp->_.lvalue._.bit.bit);
									bool value;
									switch(op)
									{
									case NI_EQ:		value = lset == rset; break;
									case NI_NEQ:	value = lset != rset; break;
									case NI_LAND:	value = lset && rset; break;
									case NI_LOR:	value = lset || rset; break;
									case NI_LXOR:	value = lset != rset; break;
									default:
										SETRET(nsr,INVALID);
										return true;		
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;		
							}
						}
					default:
						SETRET(nsr,INVALID);
						return true;		
					}
					break;
				}

			case NST_STAT:			// STAT op ???
				{
					switch(rsp->type)
					{
					case NST_NUMBER:		// STAT op NUMBER => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = *(lsp->_.lvalue._.stat.number) == (rsp->_.i); break;
							case NI_NEQ:	value = *(lsp->_.lvalue._.stat.number) != (rsp->_.i); break;
							case NI_LT:		value = *(lsp->_.lvalue._.stat.number) < (rsp->_.i); break;
							case NI_LE:		value = *(lsp->_.lvalue._.stat.number) <= (rsp->_.i); break;
							case NI_GT:		value = *(lsp->_.lvalue._.stat.number) > (rsp->_.i); break;
							case NI_GE:		value = *(lsp->_.lvalue._.stat.number) >= (rsp->_.i); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_STAT:			// STAT op STAT => BOOLEAN
						{
							bool same = lsp->_.lvalue._.stat.table == rsp->_.stat.table;
							bool value;
							switch(op)
							{
							case NI_EQ:		value = same && (*(lsp->_.lvalue._.stat.number) == (rsp->_.stat.number)); break;
							case NI_NEQ:	value = !same || (*(lsp->_.lvalue._.stat.number) != (rsp->_.stat.number)); break;
							case NI_LT:		value = same && (*(lsp->_.lvalue._.stat.number) < (rsp->_.stat.number)); break;
							case NI_LE:		value = same && (*(lsp->_.lvalue._.stat.number) <= (rsp->_.stat.number)); break;
							case NI_GT:		value = same && (*(lsp->_.lvalue._.stat.number) > (rsp->_.stat.number)); break;
							case NI_GE:		value = same && (*(lsp->_.lvalue._.stat.number) >= (rsp->_.stat.number)); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:		// STAT op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_NUMBER:		// STAT op NUMBER => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = *(lsp->_.lvalue._.stat.number) == *(rsp->_.lvalue._.number); break;
									case NI_NEQ:	value = *(lsp->_.lvalue._.stat.number) != *(rsp->_.lvalue._.number); break;
									case NI_LT:		value = *(lsp->_.lvalue._.stat.number) < *(rsp->_.lvalue._.number); break;
									case NI_LE:		value = *(lsp->_.lvalue._.stat.number) <= *(rsp->_.lvalue._.number); break;
									case NI_GT:		value = *(lsp->_.lvalue._.stat.number) > *(rsp->_.lvalue._.number); break;
									case NI_GE:		value = *(lsp->_.lvalue._.stat.number) >= *(rsp->_.lvalue._.number); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_STAT:			// STAT op STAT => BOOLEAN
								{
									bool same = lsp->_.lvalue._.stat.table == rsp->_.lvalue._.stat.table;
									bool value;
									switch(op)
									{
									case NI_EQ:		value = same && (*(lsp->_.lvalue._.stat.number) == *(rsp->_.lvalue._.stat.number)); break;
									case NI_NEQ:	value = !same || (*(lsp->_.lvalue._.stat.number) != *(rsp->_.lvalue._.stat.number)); break;
									case NI_LT:		value = same && (*(lsp->_.lvalue._.stat.number) < *(rsp->_.lvalue._.stat.number)); break;
									case NI_LE:		value = same && (*(lsp->_.lvalue._.stat.number) <= *(rsp->_.lvalue._.stat.number)); break;
									case NI_GT:		value = same && (*(lsp->_.lvalue._.stat.number) > *(rsp->_.lvalue._.stat.number)); break;
									case NI_GE:		value = same && (*(lsp->_.lvalue._.stat.number) >= *(rsp->_.lvalue._.stat.number)); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_ACCOUNT:		// ACCOUNT op ???
				{
					switch(rsp->type)
					{
					case NST_ACCOUNT:		// ACCOUNT op ACCOUNT => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.account))) == (rsp->_.account); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.account))) != (rsp->_.account); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// ACCOUNT op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.account))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.account))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// ACCOUNT op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_ACCOUNT:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.account))) == *(rsp->_.lvalue._.account); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.account))) != *(rsp->_.lvalue._.account); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_AFFECT:		// AFFECT op ???
				{
					switch(rsp->type)
					{
					case NST_AFFECT:		// AFFECT op AFFECT => BOOLEAN
						{
							// TODO: Add an affect_cmp function
							bool value;
							switch(op)
							{
							case NI_EQ:		value = affect_equal((*(lsp->_.lvalue._.affect)), rsp->_.affect); break;
							case NI_NEQ:	value = !affect_equal((*(lsp->_.lvalue._.affect)), rsp->_.affect); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// AFFECT op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.affect))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.affect))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// AFFECT op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_AFFECT:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = affect_equal((*(lsp->_.lvalue._.affect)), *(rsp->_.lvalue._.affect)); break;
									case NI_NEQ:	value = !affect_equal((*(lsp->_.lvalue._.affect)), *(rsp->_.lvalue._.affect)); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_AREA:			// AREA op ???
				{
					switch(rsp->type)
					{
					case NST_AREA:		// AREA op AREA => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.area))) == (rsp->_.area); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.area))) != (rsp->_.area); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// AREA op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.area))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.area))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// AREA op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_AREA:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.area))) == *(rsp->_.lvalue._.area); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.area))) != *(rsp->_.lvalue._.area); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			// case NST_CHANNEL:
			case NST_CLASS:			// CLASS op ???
				{
					switch(rsp->type)
					{
					case NST_CLASS:		// CLASS op CLASS => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.clazz))) == (rsp->_.clazz); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.clazz))) != (rsp->_.clazz); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// CLASS op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.clazz))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.clazz))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// CLASS op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_CLASS:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.clazz))) == *(rsp->_.lvalue._.clazz); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.clazz))) != *(rsp->_.lvalue._.clazz); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_DUNGEON:		// DUNGEON op ???
				{
					switch(rsp->type)
					{
					case NST_DUNGEON:		// DUNGEON op DUNGEON => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.dungeon))) == (rsp->_.dungeon); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.dungeon))) != (rsp->_.dungeon); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:		// DUNGEON op WIDEVNUM => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (*(lsp->_.lvalue._.dungeon)) && ((*(lsp->_.lvalue._.dungeon)) && (*(lsp->_.lvalue._.dungeon))->index) && ((*(lsp->_.lvalue._.dungeon))->index->area == rsp->_.wnum.pArea) && ((*(lsp->_.lvalue._.dungeon))->index->vnum == rsp->_.wnum.vnum); break;
							case NI_NEQ:
								if ((*(lsp->_.lvalue._.dungeon)) && (*(lsp->_.lvalue._.dungeon))->index)
									value = ((*(lsp->_.lvalue._.dungeon))->index->area != rsp->_.wnum.pArea) || ((*(lsp->_.lvalue._.dungeon))->index->vnum != rsp->_.wnum.vnum);
								else
									value = (rsp->_.wnum.pArea && rsp->_.wnum.vnum > 0);
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// DUNGEON op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.dungeon))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.dungeon))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// DUNGEON op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_DUNGEON:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.dungeon))) == *(rsp->_.lvalue._.dungeon); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.dungeon))) != *(rsp->_.lvalue._.dungeon); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_WIDEVNUM:			// DUNGEON op WIDEVNUM => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = (*(lsp->_.lvalue._.dungeon)) && ((*(lsp->_.lvalue._.dungeon))->index != NULL) && ((*(lsp->_.lvalue._.dungeon))->index->area == rsp->_.lvalue._.wnum->pArea) && ((*(lsp->_.lvalue._.dungeon))->index->vnum == rsp->_.lvalue._.wnum->vnum); break;
									case NI_NEQ:
										if ((*(lsp->_.lvalue._.dungeon)) && (*(lsp->_.lvalue._.dungeon))->index)
											value = ((*(lsp->_.lvalue._.dungeon))->index->area != rsp->_.lvalue._.wnum->pArea) || ((*(lsp->_.lvalue._.dungeon))->index->vnum != rsp->_.lvalue._.wnum->vnum);
										else
											value = (rsp->_.lvalue._.wnum->pArea && rsp->_.lvalue._.wnum->vnum > 0);
										break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_EXIT:			// EXIT op ???
				{
					switch(rsp->type)
					{
					case NST_EXIT:		// EXIT op EXIT => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.ex))) == (rsp->_.ex); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.ex))) != (rsp->_.ex); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// EXIT op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.ex))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.ex))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// EXIT op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_EXIT:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.ex))) == *(rsp->_.lvalue._.ex); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.ex))) != *(rsp->_.lvalue._.ex); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_INSTANCE:		// INSTANCE op ???
				{
					switch(rsp->type)
					{
					case NST_INSTANCE:		// INSTANCE op INSTANCE => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.instance))) == (rsp->_.instance); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.instance))) != (rsp->_.instance); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:		// INSTANCE op WIDEVNUM => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (*(lsp->_.lvalue._.instance)) && ((*(lsp->_.lvalue._.instance)) && (*(lsp->_.lvalue._.instance))->blueprint) && ((*(lsp->_.lvalue._.instance))->blueprint->area == rsp->_.wnum.pArea) && ((*(lsp->_.lvalue._.instance))->blueprint->vnum == rsp->_.wnum.vnum); break;
							case NI_NEQ:
								if ((*(lsp->_.lvalue._.instance)) && (*(lsp->_.lvalue._.instance))->blueprint)
									value = ((*(lsp->_.lvalue._.instance))->blueprint->area != rsp->_.wnum.pArea) || ((*(lsp->_.lvalue._.instance))->blueprint->vnum != rsp->_.wnum.vnum);
								else
									value = (rsp->_.wnum.pArea && rsp->_.wnum.vnum > 0);
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// INSTANCE op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.instance))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.instance))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// INSTANCE op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_INSTANCE:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.instance))) == *(rsp->_.lvalue._.instance); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.instance))) != *(rsp->_.lvalue._.instance); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_WIDEVNUM:			// INSTANCE op WIDEVNUM => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = (*(lsp->_.lvalue._.instance)) && ((*(lsp->_.lvalue._.instance))->blueprint != NULL) && ((*(lsp->_.lvalue._.instance))->blueprint->area == rsp->_.lvalue._.wnum->pArea) && ((*(lsp->_.lvalue._.instance))->blueprint->vnum == rsp->_.lvalue._.wnum->vnum); break;
									case NI_NEQ:
										if ((*(lsp->_.lvalue._.instance)) && (*(lsp->_.lvalue._.instance))->blueprint)
											value = ((*(lsp->_.lvalue._.instance))->blueprint->area != rsp->_.lvalue._.wnum->pArea) || ((*(lsp->_.lvalue._.instance))->blueprint->vnum != rsp->_.lvalue._.wnum->vnum);
										else
											value = (rsp->_.lvalue._.wnum->pArea && rsp->_.lvalue._.wnum->vnum > 0);
										break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_LIQUID:		// LIQUID op ???
				{
					switch(rsp->type)
					{
					case NST_LIQUID:		// LIQUID op LIQUID => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.liquid))) == (rsp->_.liquid); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.liquid))) != (rsp->_.liquid); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// LIQUID op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.liquid))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.liquid))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// LIQUID op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_LIQUID:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.liquid))) == *(rsp->_.lvalue._.liquid); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.liquid))) != *(rsp->_.lvalue._.liquid); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_MAIL:			// MAIL op ???
				{
					switch(rsp->type)
					{
					case NST_MAIL:		// MAIL op MAIL => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.mail))) == (rsp->_.mail); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.mail))) != (rsp->_.mail); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// MAIL op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.mail))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.mail))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// MAIL op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_MAIL:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.mail))) == *(rsp->_.lvalue._.mail); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.mail))) != *(rsp->_.lvalue._.mail); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_MATERIAL:		// MATERIAL op ???
				{
					switch(rsp->type)
					{
					case NST_MATERIAL:		// MATERIAL op MATERIAL => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.material))) == (rsp->_.material); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.material))) != (rsp->_.material); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// MATERIAL op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.material))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.material))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// MATERIAL op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_MATERIAL:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.material))) == *(rsp->_.lvalue._.material); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.material))) != *(rsp->_.lvalue._.material); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_MISSION:		// MISSION op ???
				{
					switch(rsp->type)
					{
					case NST_MISSION:		// MISSION op MISSION => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.mission))) == (rsp->_.mission); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.mission))) != (rsp->_.mission); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// MISSION op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.mission))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.mission))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// MISSION op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_MISSION:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.mission))) == *(rsp->_.lvalue._.mission); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.mission))) != *(rsp->_.lvalue._.mission); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_MOBILE:		// MOBILE op ???
				{
					switch(rsp->type)
					{
					case NST_MOBILE:		// MOBILE op MOBILE => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.mobile))) == (rsp->_.mobile); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.mobile))) != (rsp->_.mobile); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:		// MOBILE op WIDEVNUM => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.mobile)) && (*(lsp->_.lvalue._.mobile))->pIndexData != NULL) && ((*(lsp->_.lvalue._.mobile))->pIndexData->area == rsp->_.wnum.pArea) && ((*(lsp->_.lvalue._.mobile))->pIndexData->vnum == rsp->_.wnum.vnum); break;
							case NI_NEQ:
								if ((*(lsp->_.lvalue._.mobile)) && (*(lsp->_.lvalue._.mobile))->pIndexData)
									value = ((*(lsp->_.lvalue._.mobile))->pIndexData->area != rsp->_.wnum.pArea) || ((*(lsp->_.lvalue._.mobile))->pIndexData->vnum != rsp->_.wnum.vnum);
								else
									value = (rsp->_.wnum.pArea && rsp->_.wnum.vnum > 0);
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:			// MOBILE op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.mobile))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.mobile))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:		// MOBILE op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_MOBILE:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.mobile))) == *(rsp->_.lvalue._.mobile); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.mobile))) != *(rsp->_.lvalue._.mobile); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_WIDEVNUM:			// MOBILE op WIDEVNUM => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.mobile)) && (*(lsp->_.lvalue._.mobile))->pIndexData != NULL) && ((*(lsp->_.lvalue._.mobile))->pIndexData->area == rsp->_.lvalue._.wnum->pArea) && ((*(lsp->_.lvalue._.mobile))->pIndexData->vnum == rsp->_.lvalue._.wnum->vnum); break;
									case NI_NEQ:
										if ((*(lsp->_.lvalue._.mobile)) && (*(lsp->_.lvalue._.mobile))->pIndexData)
										{
											value = ((*(lsp->_.lvalue._.mobile))->pIndexData->area != rsp->_.lvalue._.wnum->pArea) || ((*(lsp->_.lvalue._.mobile))->pIndexData->vnum != rsp->_.lvalue._.wnum->vnum);
										}
										else
											value = (rsp->_.lvalue._.wnum->pArea && rsp->_.lvalue._.wnum->vnum > 0);
										break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_OBJECT:		// OBJECT op ???
				{
					switch(rsp->type)
					{
					case NST_OBJECT:		// OBJECT op OBJECT => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.object))) == (rsp->_.object); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.object))) != (rsp->_.object); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:		// OBJECT op WIDEVNUM => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.object)) && (*(lsp->_.lvalue._.object))->pIndexData != NULL) && ((*(lsp->_.lvalue._.object))->pIndexData->area == rsp->_.wnum.pArea) && ((*(lsp->_.lvalue._.object))->pIndexData->vnum == rsp->_.wnum.vnum); break;
							case NI_NEQ:
								if ((*(lsp->_.lvalue._.object)) && (*(lsp->_.lvalue._.object))->pIndexData)
									value = ((*(lsp->_.lvalue._.object))->pIndexData->area != rsp->_.wnum.pArea) || ((*(lsp->_.lvalue._.object))->pIndexData->vnum != rsp->_.wnum.vnum);
								else
									value = (rsp->_.wnum.pArea && rsp->_.wnum.vnum > 0);
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:			// OBJECT op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.object))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.object))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:		// OBJECT op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_OBJECT:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.object))) == *(rsp->_.lvalue._.object); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.object))) != *(rsp->_.lvalue._.object); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_WIDEVNUM:			// OBJECT op WIDEVNUM => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.object)) && (*(lsp->_.lvalue._.object))->pIndexData != NULL) && ((*(lsp->_.lvalue._.object))->pIndexData->area == rsp->_.lvalue._.wnum->pArea) && ((*(lsp->_.lvalue._.object))->pIndexData->vnum == rsp->_.lvalue._.wnum->vnum); break;
									case NI_NEQ:
										if ((*(lsp->_.lvalue._.object)) && (*(lsp->_.lvalue._.object))->pIndexData)
										{
											value = ((*(lsp->_.lvalue._.object))->pIndexData->area != rsp->_.lvalue._.wnum->pArea) || ((*(lsp->_.lvalue._.object))->pIndexData->vnum != rsp->_.lvalue._.wnum->vnum);
										}
										else
											value = (rsp->_.lvalue._.wnum->pArea && rsp->_.lvalue._.wnum->vnum > 0);
										break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_ORG:			// ORG op ???
				{
					switch(rsp->type)
					{
					case NST_ORG:		// ORG op ORG => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.org))) == (rsp->_.org); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.org))) != (rsp->_.org); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// ORG op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.org))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.org))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// ORG op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_ORG:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.org))) == *(rsp->_.lvalue._.org); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.org))) != *(rsp->_.lvalue._.org); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			// case NST_QUEST:
			case NST_RACE:			// RACE op ???
				{
					switch(rsp->type)
					{
					case NST_RACE:		// RACE op RACE => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.race))) == (rsp->_.race); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.race))) != (rsp->_.race); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// RACE op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.race))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.race))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// RACE op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_RACE:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.race))) == *(rsp->_.lvalue._.race); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.race))) != *(rsp->_.lvalue._.race); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_RANK:			// RANK op ???
				{
					switch(rsp->type)
					{
					case NST_RANK:		// RANK op RANK => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.rank))) == (rsp->_.rank); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.rank))) != (rsp->_.rank); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// RANK op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.rank))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.rank))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// RANK op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_RANK:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.rank))) == *(rsp->_.lvalue._.rank); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.rank))) != *(rsp->_.lvalue._.rank); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_REPUTATION:	// REPUTATION op ???
				{
					switch(rsp->type)
					{
					case NST_REPUTATION:		// REPUTATION op REPUTATION => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.reputation))) == (rsp->_.reputation); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.reputation))) != (rsp->_.reputation); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// REPUTATION op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.reputation))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.reputation))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// REPUTATION op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_REPUTATION:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.reputation))) == *(rsp->_.lvalue._.reputation); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.reputation))) != *(rsp->_.lvalue._.reputation); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_ROOM:			// ROOM op ???
				{
					switch(rsp->type)
					{
					case NST_ROOM:		// ROOM op ROOM => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.room))) == (rsp->_.room); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.room))) != (rsp->_.room); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:		// ROOM op WIDEVNUM => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.room))) && ((*(lsp->_.lvalue._.room))->area == rsp->_.wnum.pArea) && ((*(lsp->_.lvalue._.room))->vnum == rsp->_.wnum.vnum); break;
							case NI_NEQ:	value = !((*(lsp->_.lvalue._.room))) && (((*(lsp->_.lvalue._.room))->area != rsp->_.wnum.pArea) || ((*(lsp->_.lvalue._.room))->vnum != rsp->_.wnum.vnum)); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:			// ROOM op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.room))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.room))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:		// ROOM op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_ROOM:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.room))) == *(rsp->_.lvalue._.room); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.room))) != *(rsp->_.lvalue._.room); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_WIDEVNUM:			// ROOM op WIDEVNUM => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.room))->area == rsp->_.lvalue._.wnum->pArea) && ((*(lsp->_.lvalue._.room))->vnum == rsp->_.lvalue._.wnum->vnum); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.room))->area != rsp->_.lvalue._.wnum->pArea) || ((*(lsp->_.lvalue._.room))->vnum != rsp->_.lvalue._.wnum->vnum); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_SHIP:			// SHIP op ???
				{
					switch(rsp->type)
					{
					case NST_SHIP:		// SHIP op SHIP => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.ship))) == (rsp->_.ship); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.ship))) != (rsp->_.ship); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:		// SHIP op WIDEVNUM => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = (*(lsp->_.lvalue._.ship)) && ((*(lsp->_.lvalue._.ship)) && (*(lsp->_.lvalue._.ship))->index) && ((*(lsp->_.lvalue._.ship))->index->area == rsp->_.wnum.pArea) && ((*(lsp->_.lvalue._.ship))->index->vnum == rsp->_.wnum.vnum); break;
							case NI_NEQ:
								if ((*(lsp->_.lvalue._.ship)) && (*(lsp->_.lvalue._.ship))->index)
									value = ((*(lsp->_.lvalue._.ship))->index->area != rsp->_.wnum.pArea) || ((*(lsp->_.lvalue._.ship))->index->vnum != rsp->_.wnum.vnum);
								else
									value = (rsp->_.wnum.pArea && rsp->_.wnum.vnum > 0);
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// SHIP op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.ship))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.ship))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// SHIP op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_SHIP:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.ship))) == *(rsp->_.lvalue._.ship); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.ship))) != *(rsp->_.lvalue._.ship); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_WIDEVNUM:			// SHIP op WIDEVNUM => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = (*(lsp->_.lvalue._.ship)) && ((*(lsp->_.lvalue._.ship))->index != NULL) && ((*(lsp->_.lvalue._.ship))->index->area == rsp->_.lvalue._.wnum->pArea) && ((*(lsp->_.lvalue._.ship))->index->vnum == rsp->_.lvalue._.wnum->vnum); break;
									case NI_NEQ:
										if ((*(lsp->_.lvalue._.ship)) && (*(lsp->_.lvalue._.ship))->index)
											value = ((*(lsp->_.lvalue._.ship))->index->area != rsp->_.lvalue._.wnum->pArea) || ((*(lsp->_.lvalue._.ship))->index->vnum != rsp->_.lvalue._.wnum->vnum);
										else
											value = (rsp->_.lvalue._.wnum->pArea && rsp->_.lvalue._.wnum->vnum > 0);
										break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_SKILL:			// SKILL op ???
				{
					switch(rsp->type)
					{
					case NST_SKILL:		// SKILL op SKILL => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.skill))) == (rsp->_.skill); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.skill))) != (rsp->_.skill); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// SKILL op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.skill))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.skill))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// SKILL op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_SKILL:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.skill))) == *(rsp->_.lvalue._.skill); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.skill))) != *(rsp->_.lvalue._.skill); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_TOKEN:			// TOKEN op ???
				{
					switch(rsp->type)
					{
					case NST_TOKEN:		// TOKEN op TOKEN => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.token))) == (rsp->_.token); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.token))) != (rsp->_.token); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:		// TOKEN op WIDEVNUM => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.token)) && (*(lsp->_.lvalue._.token))->pIndexData != NULL) && ((*(lsp->_.lvalue._.token))->pIndexData->area == rsp->_.wnum.pArea) && ((*(lsp->_.lvalue._.token))->pIndexData->vnum == rsp->_.wnum.vnum); break;
							case NI_NEQ:
								if ((*(lsp->_.lvalue._.token)) && (*(lsp->_.lvalue._.token))->pIndexData)
									value = ((*(lsp->_.lvalue._.token))->pIndexData->area != rsp->_.wnum.pArea) || ((*(lsp->_.lvalue._.token))->pIndexData->vnum != rsp->_.wnum.vnum);
								else
									value = (rsp->_.wnum.pArea && rsp->_.wnum.vnum > 0);
								break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:			// TOKEN op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.token))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.token))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:		// TOKEN op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_TOKEN:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.token))) == *(rsp->_.lvalue._.token); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.token))) != *(rsp->_.lvalue._.token); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NST_WIDEVNUM:			// TOKEN op WIDEVNUM => BOOLEAN
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.token)) && (*(lsp->_.lvalue._.token))->pIndexData != NULL) && ((*(lsp->_.lvalue._.token))->pIndexData->area == rsp->_.lvalue._.wnum->pArea) && ((*(lsp->_.lvalue._.token))->pIndexData->vnum == rsp->_.lvalue._.wnum->vnum); break;
									case NI_NEQ:
										if ((*(lsp->_.lvalue._.token)) && (*(lsp->_.lvalue._.token))->pIndexData)
										{
											value = ((*(lsp->_.lvalue._.token))->pIndexData->area != rsp->_.lvalue._.wnum->pArea) || ((*(lsp->_.lvalue._.token))->pIndexData->vnum != rsp->_.lvalue._.wnum->vnum);
										}
										else
											value = (rsp->_.lvalue._.wnum->pArea && rsp->_.lvalue._.wnum->vnum > 0);
										break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_WILDS:			// WILDS op ???
				{
					switch(rsp->type)
					{
					case NST_WILDS:		// WILDS op WILDS => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.wilds))) == (rsp->_.wilds); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.wilds))) != (rsp->_.wilds); break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_NULL:		// WILDS op null => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(lsp->_.lvalue._.wilds))) == NULL; break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.wilds))) != NULL; break;
							default:
								SETRET(nsr,INVALID);
								return true;
							}

							if (!nib_push_stack_boolean(nsr,value))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NST_LVALUE:	// WILDS op LVALUE => BOOLEAN
						{
							switch(rsp->_.lvalue.type)
							{
							case NST_WILDS:
								{
									bool value;
									switch(op)
									{
									case NI_EQ:		value = ((*(lsp->_.lvalue._.wilds))) == *(rsp->_.lvalue._.wilds); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.wilds))) != *(rsp->_.lvalue._.wilds); break;
									default:
										SETRET(nsr,INVALID);
										return true;
									}

									if (!nib_push_stack_boolean(nsr,value))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			// case NST_WORLD:
			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}
	
	default:
		SETRET(nsr,INVALID);
		return true;
	}

	return false;
}

#define __asn(t,l,f,n) \
	case NST_##t:\
		{ \
			switch(rsp->type) \
			{ \
			case NST_##t: \
				{ \
					switch(op) \
					{ \
					case NI_VOID_ASSIGN: \
						push_result = false; \
					case NI_ASSIGN: \
						{ \
							*(lsp->_.lvalue._.l) = rsp->_.f; \
\
							if (push_result && !nib_push_stack_##n (nsr,*(lsp->_.lvalue._.l))) \
							{ \
								SETRET(nsr,STACK); \
								return true; \
							} \
							break; \
						} \
\
					default: \
						SETRET(nsr,INVALID); \
						return true; \
					} \
					break; \
				} \
\
			case NST_NULL: \
				{ \
					switch(op) \
					{ \
					case NI_VOID_ASSIGN: \
						push_result = false; \
					case NI_ASSIGN: \
						{ \
							*(lsp->_.lvalue._.l) = NULL; \
\
							if (push_result && !nib_push_stack_##n (nsr,*(lsp->_.lvalue._.l))) \
							{ \
								SETRET(nsr,STACK); \
								return true; \
							} \
							break; \
						} \
\
					default: \
						SETRET(nsr,INVALID); \
						return true; \
					} \
					break; \
				} \
\
			case NST_LVALUE: \
				{ \
					switch(rsp->_.lvalue.type) \
					{ \
					case NST_##t: \
						{ \
							switch(op) \
							{ \
							case NI_VOID_ASSIGN: \
								push_result = false; \
							case NI_ASSIGN: \
								{ \
									*(lsp->_.lvalue._.l) = *(rsp->_.lvalue._.l); \
\
									if (push_result && !nib_push_stack_##n (nsr,*(lsp->_.lvalue._.l))) \
									{ \
										SETRET(nsr,STACK); \
										return true; \
									} \
									break; \
								} \
\
							default: \
								SETRET(nsr,INVALID); \
								return true; \
							} \
							break; \
						} \
 \
					default: \
						SETRET(nsr,INVALID); \
						return true; \
					} \
					break; \
				} \
\
			default: \
				SETRET(nsr,INVALID); \
				return true; \
			} \
\
			break; \
		}


static bool __assignment_operation(NIB_SCRIPT_RUNTIME *nsr, enum nib_instructions_e op)
{
	// Stack Order
	// RHS
	NIB_SCRIPT_STACK *rsp = nib_pop_stack_raw(nsr);
	if (!rsp)
	{
		SETRET(nsr,STACK);
		return true;
	}
	// LHS
	NIB_SCRIPT_STACK *lsp = nib_pop_stack_raw(nsr);
	if (!lsp || lsp->type != NST_LVALUE)
	{
		free_stack_item(rsp);
		SETRET(nsr,STACK);
		return true;
	}

	bool push_result = true;
	switch(lsp->_.lvalue.type)
	{
	case NST_NUMBER:		// NUMBER op= ???
		{
			switch(rsp->type)
			{
			case NST_NUMBER:		// NUMBER op= NUMBER
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.number) = rsp->_.i;

							if (push_result && !nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}
					
					case NI_VOID_ADD_EQ:
						push_result = false;
					case NI_ADD_EQ:
						{
							*(lsp->_.lvalue._.number) += rsp->_.i;

							if (push_result && !nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_SUBT_EQ:
						{
							*(lsp->_.lvalue._.number) -= rsp->_.i;

							if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_MULT_EQ:
						{
							*(lsp->_.lvalue._.number) *= rsp->_.i;

							if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_MOD_EQ:
						{
							if (rsp->_.i == 0)
							{
								SETRET(nsr,MATH);
								return true;
							}

							*(lsp->_.lvalue._.number) %= rsp->_.i;

							if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_DIV_EQ:
						{
							if (rsp->_.i == 0)
							{
								SETRET(nsr,MATH);
								return true;
							}

							*(lsp->_.lvalue._.number) /= rsp->_.i;

							if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_BAND_EQ:
						{
							*(lsp->_.lvalue._.number) |= rsp->_.i;

							if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_BOR_EQ:
						{
							*(lsp->_.lvalue._.number) |= rsp->_.i;

							if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_BXOR_EQ:
						{
							*(lsp->_.lvalue._.number) ^= rsp->_.i;

							if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_LSH_EQ:
						{
							if (rsp->_.i >= MAX_FLAG_BITS)
								*(lsp->_.lvalue._.number) = 0;
							else
								*(lsp->_.lvalue._.number) <<= rsp->_.i;

							if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_RSH_EQ:
						{
							if (rsp->_.i >= MAX_FLAG_BITS)
							{
								if (*(lsp->_.lvalue._.number) < 0)
									*(lsp->_.lvalue._.number) = -1;
								else
									*(lsp->_.lvalue._.number) = 0;
							}
							else
								*(lsp->_.lvalue._.number) >>= rsp->_.i;

							if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_RSHL_EQ:
						{
							if (rsp->_.i >= MAX_FLAG_BITS)
								*(lsp->_.lvalue._.number) = 0;
							else
								*(lsp->_.lvalue._.number) = (long)(((unsigned long)*(lsp->_.lvalue._.number)) >> rsp->_.i);

							if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_FLOAT:			// NUMBER op= FLOAT
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.number) = (long)rsp->_.d;

							if (push_result && !nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}
					
					case NI_VOID_ADD_EQ:
						push_result = false;
					case NI_ADD_EQ:
						{
							*(lsp->_.lvalue._.number) = (long)(*(lsp->_.lvalue._.number) + rsp->_.d);

							if (push_result && !nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_SUBT_EQ:
						{
							*(lsp->_.lvalue._.number) = (long)(*(lsp->_.lvalue._.number) - rsp->_.d);

							if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_MULT_EQ:
						{
							*(lsp->_.lvalue._.number) = (long)(*(lsp->_.lvalue._.number) * rsp->_.d);

							if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_DIV_EQ:
						{
							if (rsp->_.d == 0.0)
							{
								SETRET(nsr,MATH);
								return true;
							}

							*(lsp->_.lvalue._.number) = (long)(*(lsp->_.lvalue._.number) / rsp->_.d);

							if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_BOOLEAN:		// NUMBER op= BOOLEAN
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.number) = ((rsp->_.b)?1L:0L);

							if (push_result && !nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_LVALUE:
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:		// NUMBER op= NUMBER
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.number) = *(rsp->_.lvalue._.number);

									if (push_result && !nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}
							
							case NI_VOID_ADD_EQ:
								push_result = false;
							case NI_ADD_EQ:
								{
									*(lsp->_.lvalue._.number) += *(rsp->_.lvalue._.number);

									if (push_result && !nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_SUBT_EQ:
								{
									*(lsp->_.lvalue._.number) -= *(rsp->_.lvalue._.number);

									if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_MULT_EQ:
								{
									*(lsp->_.lvalue._.number) *= *(rsp->_.lvalue._.number);

									if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_MOD_EQ:
								{
									if (*(rsp->_.lvalue._.number) == 0)
									{
										SETRET(nsr,MATH);
										return true;
									}

									*(lsp->_.lvalue._.number) %= *(rsp->_.lvalue._.number);

									if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_DIV_EQ:
								{
									if (*(rsp->_.lvalue._.number) == 0)
									{
										SETRET(nsr,MATH);
										return true;
									}

									*(lsp->_.lvalue._.number) /= *(rsp->_.lvalue._.number);

									if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_BAND_EQ:
								{
									*(lsp->_.lvalue._.number) |= *(rsp->_.lvalue._.number);

									if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_BOR_EQ:
								{
									*(lsp->_.lvalue._.number) |= *(rsp->_.lvalue._.number);

									if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_BXOR_EQ:
								{
									*(lsp->_.lvalue._.number) ^= *(rsp->_.lvalue._.number);

									if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_LSH_EQ:
								{
									if (*(rsp->_.lvalue._.number) >= MAX_FLAG_BITS)
										*(lsp->_.lvalue._.number) = 0;
									else
										*(lsp->_.lvalue._.number) <<= *(rsp->_.lvalue._.number);

									if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_RSH_EQ:
								{
									if (*(rsp->_.lvalue._.number) >= MAX_FLAG_BITS)
									{
										if (*(lsp->_.lvalue._.number) < 0)
											*(lsp->_.lvalue._.number) = -1;
										else
											*(lsp->_.lvalue._.number) = 0;
									}
									else
										*(lsp->_.lvalue._.number) >>= *(rsp->_.lvalue._.number);

									if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_RSHL_EQ:
								{
									if (*(rsp->_.lvalue._.number) >= MAX_FLAG_BITS)
										*(lsp->_.lvalue._.number) = 0;
									else
										*(lsp->_.lvalue._.number) = (long)(((unsigned long)*(lsp->_.lvalue._.number)) >> *(rsp->_.lvalue._.number));

									if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_FLOAT:			// NUMBER op= FLOAT
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.number) = (long)*(rsp->_.lvalue._.d);

									if (push_result && !nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}
							
							case NI_VOID_ADD_EQ:
								push_result = false;
							case NI_ADD_EQ:
								{
									*(lsp->_.lvalue._.number) = (long)(*(lsp->_.lvalue._.number) + *(rsp->_.lvalue._.d));

									if (push_result && !nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_SUBT_EQ:
								{
									*(lsp->_.lvalue._.number) = (long)(*(lsp->_.lvalue._.number) - *(rsp->_.lvalue._.d));

									if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_MULT_EQ:
								{
									*(lsp->_.lvalue._.number) = (long)(*(lsp->_.lvalue._.number) * *(rsp->_.lvalue._.d));

									if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_DIV_EQ:
								{
									if (*(rsp->_.lvalue._.d) == 0.0)
									{
										SETRET(nsr,MATH);
										return true;
									}

									*(lsp->_.lvalue._.number) = (long)(*(lsp->_.lvalue._.number) / *(rsp->_.lvalue._.d));

									if (!nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_BOOLEAN:		// NUMBER op= BOOLEAN
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.number) = ((*(rsp->_.lvalue._.b))?1L:0L);

									if (push_result && !nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_FLAG_BIT:		// NUMBER op= BIT
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									
									*(lsp->_.lvalue._.number) = (IS_SET(*(rsp->_.lvalue._.bit.value),rsp->_.lvalue._.bit.bit)?1L:0L);

									if (push_result && !nib_push_stack_number(nsr,*(lsp->_.lvalue._.number)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_FLOAT:			// FLOAT op= ???
		{
			switch(rsp->type)
			{
			case NST_NUMBER:		// FLOAT op= NUMBER
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.d) = (double)rsp->_.i;

							if (push_result && !nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}
					
					case NI_VOID_ADD_EQ:
						push_result = false;
					case NI_ADD_EQ:
						{
							*(lsp->_.lvalue._.number) += (double)rsp->_.i;

							if (push_result && !nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_SUBT_EQ:
						{
							*(lsp->_.lvalue._.number) -= (double)rsp->_.i;

							if (!nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_MULT_EQ:
						{
							*(lsp->_.lvalue._.number) *= (double)rsp->_.i;

							if (!nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_DIV_EQ:
						{
							if (rsp->_.i == 0)
							{
								SETRET(nsr,MATH);
								return true;
							}

							*(lsp->_.lvalue._.number) /= (double)rsp->_.i;

							if (!nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_FLOAT:			// FLOAT op= FLOAT
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.d) = rsp->_.d;

							if (push_result && !nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}
					
					case NI_VOID_ADD_EQ:
						push_result = false;
					case NI_ADD_EQ:
						{
							*(lsp->_.lvalue._.d) += rsp->_.d;

							if (push_result && !nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_SUBT_EQ:
						{
							*(lsp->_.lvalue._.d) -= rsp->_.d;

							if (!nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_MULT_EQ:
						{
							*(lsp->_.lvalue._.d) *= rsp->_.d;

							if (!nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_DIV_EQ:
						{
							if (rsp->_.d == 0.0)
							{
								SETRET(nsr,MATH);
								return true;
							}

							*(lsp->_.lvalue._.d) /= rsp->_.d;

							if (!nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_BOOLEAN:		// FLOAT op= BOOLEAN
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.d) = ((rsp->_.b)?1.0:0.0);

							if (push_result && !nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_LVALUE:
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:		// NUMBER op= NUMBER
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.d) = *(rsp->_.lvalue._.d);

									if (push_result && !nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}
							
							case NI_VOID_ADD_EQ:
								push_result = false;
							case NI_ADD_EQ:
								{
									*(lsp->_.lvalue._.number) += *(rsp->_.lvalue._.number);

									if (push_result && !nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_SUBT_EQ:
								{
									*(lsp->_.lvalue._.d) -= *(rsp->_.lvalue._.number);

									if (!nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_MULT_EQ:
								{
									*(lsp->_.lvalue._.d) *= *(rsp->_.lvalue._.number);

									if (!nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_DIV_EQ:
								{
									if (*(rsp->_.lvalue._.number) == 0)
									{
										SETRET(nsr,MATH);
										return true;
									}

									*(lsp->_.lvalue._.d) /= *(rsp->_.lvalue._.number);

									if (!nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_FLOAT:			// FLOAT op= FLOAT
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.d) = *(rsp->_.lvalue._.d);

									if (push_result && !nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}
							
							case NI_VOID_ADD_EQ:
								push_result = false;
							case NI_ADD_EQ:
								{
									*(lsp->_.lvalue._.number) += *(rsp->_.lvalue._.d);

									if (push_result && !nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_SUBT_EQ:
								{
									*(lsp->_.lvalue._.d) -= *(rsp->_.lvalue._.d);

									if (!nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_MULT_EQ:
								{
									*(lsp->_.lvalue._.d) *= *(rsp->_.lvalue._.d);

									if (!nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_DIV_EQ:
								{
									if (*(rsp->_.lvalue._.d) == 0.0)
									{
										SETRET(nsr,MATH);
										return true;
									}

									*(lsp->_.lvalue._.d) /= *(rsp->_.lvalue._.d);

									if (!nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_BOOLEAN:		// FLOAT op= BOOLEAN
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.d) = ((*(rsp->_.lvalue._.b))?1.0:0.0);

									if (push_result && !nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_FLAG_BIT:		// FLOAT op= BIT
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.d) = (IS_SET(*(rsp->_.lvalue._.bit.value),rsp->_.lvalue._.bit.bit)?1.0:0.0);

									if (push_result && !nib_push_stack_float(nsr,*(lsp->_.lvalue._.d)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_BOOLEAN:		// BOOLEAN op= ???
		{
			switch(rsp->type)
			{
			case NST_BOOLEAN:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.b) = rsp->_.b;

							if (push_result && !nib_push_stack_boolean(nsr,*(lsp->_.lvalue._.b)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			case NST_LVALUE:
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_BOOLEAN:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.b) = *(rsp->_.lvalue._.b);

									if (push_result && !nib_push_stack_boolean(nsr,*(lsp->_.lvalue._.b)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					case NST_FLAG_BIT:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.b) = IS_SET(*(rsp->_.lvalue._.bit.value),rsp->_.lvalue._.bit.bit);

									if (push_result && !nib_push_stack_boolean(nsr,*(lsp->_.lvalue._.b)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_FLAG_BIT:		// BIT op= ???
		{
			switch(rsp->type)
			{
			case NST_BOOLEAN:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							if (rsp->_.b)
								*(lsp->_.lvalue._.bit.value) |= (lsp->_.lvalue._.bit.bit);
							else
								*(lsp->_.lvalue._.bit.value) &= ~(lsp->_.lvalue._.bit.bit);

							if (push_result && !nib_push_stack_boolean(nsr,rsp->_.b))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			case NST_LVALUE:
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_BOOLEAN:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									if (*(rsp->_.lvalue._.b))
										*(lsp->_.lvalue._.bit.value) |= (lsp->_.lvalue._.bit.bit);
									else
										*(lsp->_.lvalue._.bit.value) &= ~(lsp->_.lvalue._.bit.bit);

									if (push_result && !nib_push_stack_boolean(nsr,*(rsp->_.lvalue._.b)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					case NST_FLAG_BIT:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									bool set = IS_SET(*(rsp->_.lvalue._.bit.value),rsp->_.lvalue._.bit.bit);
									if (set)
										*(lsp->_.lvalue._.bit.value) |= (lsp->_.lvalue._.bit.bit);
									else
										*(lsp->_.lvalue._.bit.value) &= ~(lsp->_.lvalue._.bit.bit);

									if (push_result && !nib_push_stack_boolean(nsr, set))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_CHAR:			// CHAR op= ???
		{
			switch(rsp->type)
			{
			case NST_NUMBER:	// CHAR = NUMBER (provided NUMBER is in range)
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							if (rsp->_.i < 0 || rsp->_.i > 0xFFFFFFFF || !utf8_isvalid((utf8char_t)rsp->_.i))
							{
								SETRET(nsr,INVALID);
								return true;
							}

							*(lsp->_.lvalue._.ch) = (utf8char_t)rsp->_.i;

							if (push_result && !nib_push_stack_char(nsr,*(lsp->_.lvalue._.ch)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_CHAR:			// CHAR = CHAR				
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.ch) = rsp->_.ch;

							if (push_result && !nib_push_stack_char(nsr,*(lsp->_.lvalue._.ch)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_NULL:			// CHAR = null (stores the '\0' character)
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.ch) = '\0';

							if (push_result && !nib_push_stack_char(nsr,*(lsp->_.lvalue._.ch)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_LVALUE:
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:	// CHAR = NUMBER (provided NUMBER is in range)
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									if (*(rsp->_.lvalue._.number) < 0 ||
										*(rsp->_.lvalue._.number) > 0xFFFFFFFF ||
										!utf8_isvalid((utf8char_t)*(rsp->_.lvalue._.number)))
									{
										SETRET(nsr,INVALID);
										return true;
									}

									*(lsp->_.lvalue._.ch) = (utf8char_t)*(rsp->_.lvalue._.number);

									if (push_result && !nib_push_stack_char(nsr,*(lsp->_.lvalue._.ch)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_CHAR:			// CHAR = CHAR
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.ch) = *(rsp->_.lvalue._.ch);

									if (push_result && !nib_push_stack_char(nsr,*(lsp->_.lvalue._.ch)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_STRING:
		{
			switch(rsp->type)
			{
			case NST_NUMBER:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));

							char stringify[80];
							snprintf(stringify,sizeof(stringify)-1,"%ld",rsp->_.i);
							*(lsp->_.lvalue._.str) = strdup(stringify);

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_VOID_ADD_EQ:
						push_result = false;
					case NI_ADD_EQ:
						{
							char stringify[80];
							int len = snprintf(stringify,sizeof(stringify)-1,"%ld",rsp->_.i);

							if (*(lsp->_.lvalue._.str))
							{
								char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+len+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}

								strcpy(value,*(lsp->_.lvalue._.str));
								strcat(value,stringify);

								free(*(lsp->_.lvalue._.str));
								*(lsp->_.lvalue._.str) = value;
							}
							else
							{
								free(*(lsp->_.lvalue._.str));
								*(lsp->_.lvalue._.str) = strdup(stringify);
							}

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_MULT_EQ:
						{
							if (*(lsp->_.lvalue._.str))
							{
								if (rsp->_.i > 0)
								{
									int len = strlen(*(lsp->_.lvalue._.str));
									char *value = calloc(1,len * rsp->_.i + 1);
									if (!value)
									{
										SETRET(nsr,MEMORY);
										return true;
									}
									char *str = value;
									for(int i = rsp->_.i;i-- > 0; str+=len)
										strcpy(str,*(lsp->_.lvalue._.str));
									*str = '\0';

									free(*(lsp->_.lvalue._.str));
									*(lsp->_.lvalue._.str) = value;
								}
								else
								{
									free(*(lsp->_.lvalue._.str));
									*(lsp->_.lvalue._.str) = strdup("");
								}
							}
							else
							{
								*(lsp->_.lvalue._.str) = strdup("");
							}

							if (!nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_FLOAT:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));

							char stringify[100];
							snprintf(stringify,sizeof(stringify)-1,"%lf",rsp->_.d);
							*(lsp->_.lvalue._.str) = strdup(stringify);

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_VOID_ADD_EQ:
						push_result = false;
					case NI_ADD_EQ:
						{
							char stringify[100];
							int len = snprintf(stringify,sizeof(stringify)-1,"%lf",rsp->_.d);

							if (*(lsp->_.lvalue._.str))
							{
								char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+len+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}

								strcpy(value,*(lsp->_.lvalue._.str));
								strcat(value,stringify);

								free(*(lsp->_.lvalue._.str));
								*(lsp->_.lvalue._.str) = value;
							}
							else
							{
								free(*(lsp->_.lvalue._.str));
								*(lsp->_.lvalue._.str) = strdup(stringify);
							}

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_BOOLEAN:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));

							char *stringify = (rsp->_.b)?"true":"false";
							*(lsp->_.lvalue._.str) = strdup(stringify);

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_CHAR:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							char *bytes = utf8_getbytes(rsp->_.ch);

							if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
							*(lsp->_.lvalue._.str) = strdup(bytes);

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_VOID_ADD_EQ:
						push_result = false;
					case NI_ADD_EQ:
						{
							if (*(lsp->_.lvalue._.str))
							{
								if (rsp->_.ch > 0)
								{
									char *bytes = utf8_getbytes(rsp->_.ch);
									int blen = strlen(bytes);

									int len = strlen(*(lsp->_.lvalue._.str));
									char *value = calloc(1,len + blen + 1);
									if (!value)
									{
										SETRET(nsr,MEMORY);
										return true;
									}

									strcpy(value,*(lsp->_.lvalue._.str));
									strcpy(value+len,bytes);

									free(*(lsp->_.lvalue._.str));
									*(lsp->_.lvalue._.str) = value;
								}
							}
							else
							{
								char *bytes = utf8_getbytes(rsp->_.ch);

								*(lsp->_.lvalue._.str) = strdup(bytes);
							}

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_STRING:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));

							if (rsp->_.str)
							{
								// Don't bother freeing or copying the string, since it just changed ownership.
								*(lsp->_.lvalue._.str) = rsp->_.str;
							}
							else
							{
								*(lsp->_.lvalue._.str) = strdup("");
							}

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_VOID_ADD_EQ:
						push_result = false;
					case NI_ADD_EQ:
						{
							if (*(lsp->_.lvalue._.str))
							{
								if (rsp->_.str)
								{
									char *value = calloc(1,strlen(*(lsp->_.lvalue._.str)) + strlen(rsp->_.str) + 1);
									if (!value)
									{
										free(rsp->_.str);
										SETRET(nsr,MEMORY);
										return true;
									}

									strcpy(value, *(lsp->_.lvalue._.str));
									strcat(value, rsp->_.str);

									free(rsp->_.str);
									free(*(lsp->_.lvalue._.str));
									*(lsp->_.lvalue._.str) = value;
								}
								// else do nothing to lsp
							}
							else if(rsp->_.str)
							{
								// Don't bother freeing or copying the string, since it just changed ownership.
								*(lsp->_.lvalue._.str) = rsp->_.str;
							}
							else
							{
								*(lsp->_.lvalue._.str) = strdup("");
							}

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_STRING_S:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));

							if (rsp->_.str)
							{
								*(lsp->_.lvalue._.str) = strdup(rsp->_.str);
							}
							else
							{
								*(lsp->_.lvalue._.str) = strdup("");
							}

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_VOID_ADD_EQ:
						push_result = false;
					case NI_ADD_EQ:
						{
							if (*(lsp->_.lvalue._.str))
							{
								if (rsp->_.str)
								{
									char *value = calloc(1,strlen(*(lsp->_.lvalue._.str)) + strlen(rsp->_.str) + 1);
									if (!value)
									{
										SETRET(nsr,MEMORY);
										return true;
									}

									strcpy(value, *(lsp->_.lvalue._.str));
									strcat(value, rsp->_.str);

									free(*(lsp->_.lvalue._.str));
									*(lsp->_.lvalue._.str) = value;
								}
								// else do nothing to lsp
							}
							else if(rsp->_.str)
							{
								*(lsp->_.lvalue._.str) = strdup(rsp->_.str);
							}
							else
							{
								*(lsp->_.lvalue._.str) = strdup("");
							}

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_WIDEVNUM:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							char stringify[100];
							sprintf(stringify,"(%ld,%ld)",
								(rsp->_.wnum).pArea ? (rsp->_.wnum).pArea->uid : 0,
								(rsp->_.wnum).vnum);
							
							if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
							*(lsp->_.lvalue._.str) = strdup(stringify);

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_VOID_ADD_EQ:
						push_result = false;
					case NI_ADD_EQ:
						{
							char stringify[100];
							int len = sprintf(stringify,"(%ld,%ld)",
								(rsp->_.wnum).pArea ? (rsp->_.wnum).pArea->uid : 0,
								(rsp->_.wnum).vnum);

							if (*(lsp->_.lvalue._.str))
							{
								char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+len+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}

								strcpy(value,*(lsp->_.lvalue._.str));
								strcat(value,stringify);

								free(*(lsp->_.lvalue._.str));
								*(lsp->_.lvalue._.str) = value;
							}
							else
							{
								free(*(lsp->_.lvalue._.str));
								*(lsp->_.lvalue._.str) = strdup(stringify);
							}

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}

					break;
				}

			case NST_FLAG:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
							*(lsp->_.lvalue._.str) = strdup(nib_get_flag_string(rsp->_.stat.table,rsp->_.stat.number));

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_VOID_ADD_EQ:
						push_result = false;
					case NI_ADD_EQ:
						{
							const char *stringify = nib_get_flag_string(rsp->_.stat.table,rsp->_.stat.number);
							int len = strlen(stringify);
							if (*(lsp->_.lvalue._.str))
							{
								char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+len+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}

								strcpy(value,*(lsp->_.lvalue._.str));
								strcat(value,stringify);

								free(*(lsp->_.lvalue._.str));
								*(lsp->_.lvalue._.str) = value;
							}
							else
							{
								free(*(lsp->_.lvalue._.str));
								*(lsp->_.lvalue._.str) = strdup(stringify);
							}

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_STAT:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
							*(lsp->_.lvalue._.str) = strdup(nib_get_stat_string(rsp->_.stat.table,rsp->_.stat.number));

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_VOID_ADD_EQ:
						push_result = false;
					case NI_ADD_EQ:
						{
							const char *stringify = nib_get_stat_string(rsp->_.stat.table,rsp->_.stat.number);
							int len = strlen(stringify);
							if (*(lsp->_.lvalue._.str))
							{
								char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+len+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}

								strcpy(value,*(lsp->_.lvalue._.str));
								strcat(value,stringify);

								free(*(lsp->_.lvalue._.str));
								*(lsp->_.lvalue._.str) = value;
							}
							else
							{
								free(*(lsp->_.lvalue._.str));
								*(lsp->_.lvalue._.str) = strdup(stringify);
							}

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_AREA:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							char stringify[100];
							sprintf(stringify,"%s(%ld)",
								(rsp->_.area) ? (rsp->_.area)->name : "null",
								(rsp->_.area) ? (rsp->_.area)->uid : 0);
							
							if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
							*(lsp->_.lvalue._.str) = strdup(stringify);

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_VOID_ADD_EQ:
						push_result = false;
					case NI_ADD_EQ:
						{
							char stringify[100];
							int len = sprintf(stringify,"%s(%ld)",
								(rsp->_.area) ? (rsp->_.area)->name : "null",
								(rsp->_.area) ? (rsp->_.area)->uid : 0);

							if (*(lsp->_.lvalue._.str))
							{
								char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+len+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}

								strcpy(value,*(lsp->_.lvalue._.str));
								strcat(value,stringify);

								free(*(lsp->_.lvalue._.str));
								*(lsp->_.lvalue._.str) = value;
							}
							else
							{
								free(*(lsp->_.lvalue._.str));
								*(lsp->_.lvalue._.str) = strdup(stringify);
							}

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}

					break;
				}

			// case NST_DUNGEON:
			// case NST_INSTANCE:
			case NST_MOBILE:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							char stringify[100];
							sprintf(stringify,"%s(%ld,%ld)",
								(rsp->_.mobile) ? (rsp->_.mobile)->name : "null",
								(rsp->_.mobile && (rsp->_.mobile)->pIndexData) ? (rsp->_.mobile)->pIndexData->area->uid : 0,
								(rsp->_.mobile && (rsp->_.mobile)->pIndexData) ? (rsp->_.mobile)->pIndexData->vnum : 0);
							
							if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
							*(lsp->_.lvalue._.str) = strdup(stringify);

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_VOID_ADD_EQ:
						push_result = false;
					case NI_ADD_EQ:
						{
							char stringify[100];
							int len = sprintf(stringify,"%s(%ld,%ld)",
								(rsp->_.mobile) ? (rsp->_.mobile)->name : "null",
								(rsp->_.mobile && (rsp->_.mobile)->pIndexData) ? (rsp->_.mobile)->pIndexData->area->uid : 0,
								(rsp->_.mobile && (rsp->_.mobile)->pIndexData) ? (rsp->_.mobile)->pIndexData->vnum : 0);

							if (*(lsp->_.lvalue._.str))
							{
								char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+len+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}

								strcpy(value,*(lsp->_.lvalue._.str));
								strcat(value,stringify);

								free(*(lsp->_.lvalue._.str));
								*(lsp->_.lvalue._.str) = value;
							}
							else
							{
								free(*(lsp->_.lvalue._.str));
								*(lsp->_.lvalue._.str) = strdup(stringify);
							}

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}

					break;
				}

			// case NST_OBJECT:
			// case NST_QUEST:
			case NST_ROOM:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							char stringify[100];
							sprintf(stringify,"%s(%ld,%ld)",
								(rsp->_.room) ? (rsp->_.room)->name : "null",
								(rsp->_.room) ? (rsp->_.room)->area->uid : 0,
								(rsp->_.room) ? (rsp->_.room)->vnum : 0);
							
							if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
							*(lsp->_.lvalue._.str) = strdup(stringify);

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_VOID_ADD_EQ:
						push_result = false;
					case NI_ADD_EQ:
						{
							char stringify[100];
							int len = sprintf(stringify,"%s(%ld,%ld)",
								(rsp->_.room) ? (rsp->_.room)->name : "null",
								(rsp->_.room) ? (rsp->_.room)->area->uid : 0,
								(rsp->_.room) ? (rsp->_.room)->vnum : 0);

							if (*(lsp->_.lvalue._.str))
							{
								char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+len+1);
								if (!value)
								{
									SETRET(nsr,MEMORY);
									return true;
								}

								strcpy(value,*(lsp->_.lvalue._.str));
								strcat(value,stringify);

								free(*(lsp->_.lvalue._.str));
								*(lsp->_.lvalue._.str) = value;
							}
							else
							{
								free(*(lsp->_.lvalue._.str));
								*(lsp->_.lvalue._.str) = strdup(stringify);
							}

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}

					break;
				}

			// case NST_SHIP:
			// case NST_TOKEN:
			case NST_NULL:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
							*(lsp->_.lvalue._.str) = NULL;

							if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			case NST_LVALUE:
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));

									char stringify[80];
									snprintf(stringify,sizeof(stringify)-1,"%ld",*(rsp->_.lvalue._.number));
									*(lsp->_.lvalue._.str) = strdup(stringify);

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_VOID_ADD_EQ:
								push_result = false;
							case NI_ADD_EQ:
								{
									char stringify[80];
									int len = snprintf(stringify,sizeof(stringify)-1,"%ld",*(rsp->_.lvalue._.number));

									if (*(lsp->_.lvalue._.str))
									{
										char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+len+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}

										strcpy(value,*(lsp->_.lvalue._.str));
										strcat(value,stringify);

										free(*(lsp->_.lvalue._.str));
										*(lsp->_.lvalue._.str) = value;
									}
									else
									{
										free(*(lsp->_.lvalue._.str));
										*(lsp->_.lvalue._.str) = strdup(stringify);
									}

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_MULT_EQ:
								{
									if (*(lsp->_.lvalue._.str))
									{
										if (*(rsp->_.lvalue._.number) > 0)
										{
											int len = strlen(*(lsp->_.lvalue._.str));
											char *value = calloc(1,len * *(rsp->_.lvalue._.number) + 1);
											if (!value)
											{
												SETRET(nsr,MEMORY);
												return true;
											}
											char *str = value;
											for(int i = *(rsp->_.lvalue._.number);i-- > 0; str+=len)
												strcpy(str,*(lsp->_.lvalue._.str));
											*str = '\0';

											free(*(lsp->_.lvalue._.str));
											*(lsp->_.lvalue._.str) = value;
										}
										else
										{
											free(*(lsp->_.lvalue._.str));
											*(lsp->_.lvalue._.str) = strdup("");
										}
									}
									else
									{
										*(lsp->_.lvalue._.str) = strdup("");
									}

									if (!nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_FLOAT:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));

									char stringify[100];
									snprintf(stringify,sizeof(stringify)-1,"%lf",*(rsp->_.lvalue._.d));
									*(lsp->_.lvalue._.str) = strdup(stringify);

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_VOID_ADD_EQ:
								push_result = false;
							case NI_ADD_EQ:
								{
									char stringify[100];
									int len = snprintf(stringify,sizeof(stringify)-1,"%lf",*(rsp->_.lvalue._.d));

									if (*(lsp->_.lvalue._.str))
									{
										char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+len+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}

										strcpy(value,*(lsp->_.lvalue._.str));
										strcat(value,stringify);

										free(*(lsp->_.lvalue._.str));
										*(lsp->_.lvalue._.str) = value;
									}
									else
									{
										free(*(lsp->_.lvalue._.str));
										*(lsp->_.lvalue._.str) = strdup(stringify);
									}

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_BOOLEAN:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));

									char *stringify = (*(rsp->_.lvalue._.b))?"true":"false";
									*(lsp->_.lvalue._.str) = strdup(stringify);

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_CHAR:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									char *bytes = utf8_getbytes(*(rsp->_.lvalue._.ch));

									if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
									*(lsp->_.lvalue._.str) = strdup(bytes);

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_VOID_ADD_EQ:
								push_result = false;
							case NI_ADD_EQ:
								{
									if (*(lsp->_.lvalue._.str))
									{
										if (*(rsp->_.lvalue._.ch) > 0)
										{
											char *bytes = utf8_getbytes(*(rsp->_.lvalue._.ch));
											int blen = strlen(bytes);

											int len = strlen(*(lsp->_.lvalue._.str));
											char *value = calloc(1,len + blen + 1);
											if (!value)
											{
												SETRET(nsr,MEMORY);
												return true;
											}

											strcpy(value,*(lsp->_.lvalue._.str));
											strcpy(value+len,bytes);

											free(*(lsp->_.lvalue._.str));
											*(lsp->_.lvalue._.str) = value;
										}
									}
									else
									{
										char *bytes = utf8_getbytes(*(rsp->_.lvalue._.ch));

										*(lsp->_.lvalue._.str) = strdup(bytes);
									}

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_STRING:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));

									if (*(rsp->_.lvalue._.str))
									{
										*(lsp->_.lvalue._.str) = strdup(*(rsp->_.lvalue._.str));
									}
									else
									{
										*(lsp->_.lvalue._.str) = strdup("");
									}

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_VOID_ADD_EQ:
								push_result = false;
							case NI_ADD_EQ:
								{
									if (*(lsp->_.lvalue._.str))
									{
										if (*(rsp->_.lvalue._.str))
										{
											char *value = calloc(1,strlen(*(lsp->_.lvalue._.str)) + strlen(*(rsp->_.lvalue._.str)) + 1);
											if (!value)
											{
												SETRET(nsr,MEMORY);
												return true;
											}

											strcpy(value, *(lsp->_.lvalue._.str));
											strcat(value, *(rsp->_.lvalue._.str));

											free(*(lsp->_.lvalue._.str));
											*(lsp->_.lvalue._.str) = value;
										}
										// else do nothing to lsp
									}
									else if(*(rsp->_.lvalue._.str))
									{
										*(lsp->_.lvalue._.str) = strdup(*(rsp->_.lvalue._.str));
									}
									else
									{
										*(lsp->_.lvalue._.str) = strdup("");
									}

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_WIDEVNUM:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									char stringify[100];
									sprintf(stringify,"(%ld,%ld)",
										(rsp->_.lvalue._.wnum)->pArea ? (rsp->_.lvalue._.wnum)->pArea->uid : 0,
										(rsp->_.lvalue._.wnum)->vnum);
									
									if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
									*(lsp->_.lvalue._.str) = strdup(stringify);

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_VOID_ADD_EQ:
								push_result = false;
							case NI_ADD_EQ:
								{
									char stringify[100];
									int len = sprintf(stringify,"(%ld,%ld)",
										(rsp->_.lvalue._.wnum)->pArea ? (rsp->_.lvalue._.wnum)->pArea->uid : 0,
										(rsp->_.lvalue._.wnum)->vnum);

									if (*(lsp->_.lvalue._.str))
									{
										char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+len+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}

										strcpy(value,*(lsp->_.lvalue._.str));
										strcat(value,stringify);

										free(*(lsp->_.lvalue._.str));
										*(lsp->_.lvalue._.str) = value;
									}
									else
									{
										free(*(lsp->_.lvalue._.str));
										*(lsp->_.lvalue._.str) = strdup(stringify);
									}

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}

							break;
						}

					case NST_FLAG:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
									*(lsp->_.lvalue._.str) = strdup(nib_get_flag_string(rsp->_.lvalue._.stat.table,*(rsp->_.lvalue._.stat.number)));

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_VOID_ADD_EQ:
								push_result = false;
							case NI_ADD_EQ:
								{
									const char *stringify = nib_get_flag_string(rsp->_.lvalue._.stat.table,*(rsp->_.lvalue._.stat.number));
									int len = strlen(stringify);
									if (*(lsp->_.lvalue._.str))
									{
										char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+len+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}

										strcpy(value,*(lsp->_.lvalue._.str));
										strcat(value,stringify);

										free(*(lsp->_.lvalue._.str));
										*(lsp->_.lvalue._.str) = value;
									}
									else
									{
										free(*(lsp->_.lvalue._.str));
										*(lsp->_.lvalue._.str) = strdup(stringify);
									}

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_STAT:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
									*(lsp->_.lvalue._.str) = strdup(nib_get_stat_string(rsp->_.lvalue._.stat.table,*(rsp->_.lvalue._.stat.number)));

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_VOID_ADD_EQ:
								push_result = false;
							case NI_ADD_EQ:
								{
									const char *stringify = nib_get_stat_string(rsp->_.lvalue._.stat.table,*(rsp->_.lvalue._.stat.number));
									int len = strlen(stringify);
									if (*(lsp->_.lvalue._.str))
									{
										char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+len+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}

										strcpy(value,*(lsp->_.lvalue._.str));
										strcat(value,stringify);

										free(*(lsp->_.lvalue._.str));
										*(lsp->_.lvalue._.str) = value;
									}
									else
									{
										free(*(lsp->_.lvalue._.str));
										*(lsp->_.lvalue._.str) = strdup(stringify);
									}

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_AREA:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld)",
										(*(rsp->_.lvalue._.area)) ? (*(rsp->_.lvalue._.area))->name : "null",
										(*(rsp->_.lvalue._.area)) ? (*(rsp->_.lvalue._.area))->uid : 0);
									
									if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
									*(lsp->_.lvalue._.str) = strdup(stringify);

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_VOID_ADD_EQ:
								push_result = false;
							case NI_ADD_EQ:
								{
									char stringify[100];
									int len = sprintf(stringify,"%s(%ld)",
										(*(rsp->_.lvalue._.area)) ? (*(rsp->_.lvalue._.area))->name : "null",
										(*(rsp->_.lvalue._.area)) ? (*(rsp->_.lvalue._.area))->uid : 0);

									if (*(lsp->_.lvalue._.str))
									{
										char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+len+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}

										strcpy(value,*(lsp->_.lvalue._.str));
										strcat(value,stringify);

										free(*(lsp->_.lvalue._.str));
										*(lsp->_.lvalue._.str) = value;
									}
									else
									{
										free(*(lsp->_.lvalue._.str));
										*(lsp->_.lvalue._.str) = strdup(stringify);
									}

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}

							break;
						}

					// case NST_DUNGEON:
					// case NST_INSTANCE:
					case NST_MOBILE:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld,%ld)",
										(*(rsp->_.lvalue._.mobile)) ? (*(rsp->_.lvalue._.mobile))->name : "null",
										(*(rsp->_.lvalue._.mobile) && (*(rsp->_.lvalue._.mobile))->pIndexData) ? (*(rsp->_.lvalue._.mobile))->pIndexData->area->uid : 0,
										(*(rsp->_.lvalue._.mobile) && (*(rsp->_.lvalue._.mobile))->pIndexData) ? (*(rsp->_.lvalue._.mobile))->pIndexData->vnum : 0);
									
									if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
									*(lsp->_.lvalue._.str) = strdup(stringify);

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_VOID_ADD_EQ:
								push_result = false;
							case NI_ADD_EQ:
								{
									char stringify[100];
									int len = sprintf(stringify,"%s(%ld,%ld)",
										(*(rsp->_.lvalue._.mobile)) ? (*(rsp->_.lvalue._.mobile))->name : "null",
										(*(rsp->_.lvalue._.mobile) && (*(rsp->_.lvalue._.mobile))->pIndexData) ? (*(rsp->_.lvalue._.mobile))->pIndexData->area->uid : 0,
										(*(rsp->_.lvalue._.mobile) && (*(rsp->_.lvalue._.mobile))->pIndexData) ? (*(rsp->_.lvalue._.mobile))->pIndexData->vnum : 0);

									if (*(lsp->_.lvalue._.str))
									{
										char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+len+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}

										strcpy(value,*(lsp->_.lvalue._.str));
										strcat(value,stringify);

										free(*(lsp->_.lvalue._.str));
										*(lsp->_.lvalue._.str) = value;
									}
									else
									{
										free(*(lsp->_.lvalue._.str));
										*(lsp->_.lvalue._.str) = strdup(stringify);
									}

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}

							break;
						}

					// case NST_OBJECT:
					// case NST_QUEST:
					case NST_ROOM:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld,%ld)",
										(*(rsp->_.lvalue._.room)) ? (*(rsp->_.lvalue._.room))->name : "null",
										(*(rsp->_.lvalue._.room)) ? (*(rsp->_.lvalue._.room))->area->uid : 0,
										(*(rsp->_.lvalue._.room)) ? (*(rsp->_.lvalue._.room))->vnum : 0);
									
									if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
									*(lsp->_.lvalue._.str) = strdup(stringify);

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_VOID_ADD_EQ:
								push_result = false;
							case NI_ADD_EQ:
								{
									char stringify[100];
									int len = sprintf(stringify,"%s(%ld,%ld)",
										(*(rsp->_.lvalue._.room)) ? (*(rsp->_.lvalue._.room))->name : "null",
										(*(rsp->_.lvalue._.room)) ? (*(rsp->_.lvalue._.room))->area->uid : 0,
										(*(rsp->_.lvalue._.room)) ? (*(rsp->_.lvalue._.room))->vnum : 0);

									if (*(lsp->_.lvalue._.str))
									{
										char *value = calloc(1,strlen(*(lsp->_.lvalue._.str))+len+1);
										if (!value)
										{
											SETRET(nsr,MEMORY);
											return true;
										}

										strcpy(value,*(lsp->_.lvalue._.str));
										strcat(value,stringify);

										free(*(lsp->_.lvalue._.str));
										*(lsp->_.lvalue._.str) = value;
									}
									else
									{
										free(*(lsp->_.lvalue._.str));
										*(lsp->_.lvalue._.str) = strdup(stringify);
									}

									if (push_result && !nib_push_stack_string_shared(nsr,*(lsp->_.lvalue._.str)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}

							break;
						}

					// case NST_SHIP:
					// case NST_TOKEN:
					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_WIDEVNUM:
		{
			switch(rsp->type)
			{
			case NST_WIDEVNUM:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.wnum) = rsp->_.wnum;

							if (push_result && !nib_push_stack_widevnum(nsr,lsp->_.lvalue._.wnum))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_NULL:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							lsp->_.lvalue._.wnum->pArea = NULL;
							lsp->_.lvalue._.wnum->vnum = 0L;

							if (push_result && !nib_push_stack_widevnum(nsr,lsp->_.lvalue._.wnum))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_LVALUE:
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_WIDEVNUM:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.wnum) = *(rsp->_.lvalue._.wnum);

									if (push_result && !nib_push_stack_widevnum(nsr,lsp->_.lvalue._.wnum))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_FLAG:
		{
			switch(rsp->type)
			{
			case NST_NUMBER:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.stat.number) = rsp->_.i;

							if (!nib_push_stack_flag(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_BAND_EQ:
						{
							*(lsp->_.lvalue._.stat.number) &= rsp->_.i;

							if (!nib_push_stack_flag(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_BOR_EQ:
						{
							*(lsp->_.lvalue._.stat.number) |= rsp->_.i;

							if (!nib_push_stack_flag(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_BXOR_EQ:
						{
							*(lsp->_.lvalue._.stat.number) ^= rsp->_.i;

							if (!nib_push_stack_flag(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_FLAG:
				{
					if (lsp->_.lvalue._.stat.table != rsp->_.stat.table)
					{
						SETRET(nsr,INVALID);
						return true;
					}
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.stat.number) = rsp->_.stat.number;

							if (push_result && !nib_push_stack_flag(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_BAND_EQ:
						{
							*(lsp->_.lvalue._.stat.number) &= rsp->_.stat.number;

							if (push_result && !nib_push_stack_flag(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_BOR_EQ:
						{
							*(lsp->_.lvalue._.stat.number) |= rsp->_.stat.number;

							if (push_result && !nib_push_stack_flag(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					case NI_BXOR_EQ:
						{
							*(lsp->_.lvalue._.stat.number) ^= rsp->_.stat.number;

							if (push_result && !nib_push_stack_flag(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			
			case NST_LVALUE:
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_NUMBER:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.stat.number) = *(rsp->_.lvalue._.number);

									if (!nib_push_stack_flag(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_BAND_EQ:
								{
									*(lsp->_.lvalue._.stat.number) &= *(rsp->_.lvalue._.number);

									if (!nib_push_stack_flag(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_BOR_EQ:
								{
									*(lsp->_.lvalue._.stat.number) |= *(rsp->_.lvalue._.number);

									if (!nib_push_stack_flag(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_BXOR_EQ:
								{
									*(lsp->_.lvalue._.stat.number) ^= *(rsp->_.lvalue._.number);

									if (!nib_push_stack_flag(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					case NST_FLAG:
						{
							if (lsp->_.lvalue._.stat.table != rsp->_.lvalue._.stat.table)
							{
								SETRET(nsr,INVALID);
								return true;
							}
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.stat.number) = *(rsp->_.lvalue._.stat.number);

									if (push_result && !nib_push_stack_flag(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_BAND_EQ:
								{
									*(lsp->_.lvalue._.stat.number) &= *(rsp->_.lvalue._.stat.number);

									if (push_result && !nib_push_stack_flag(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_BOR_EQ:
								{
									*(lsp->_.lvalue._.stat.number) |= *(rsp->_.lvalue._.stat.number);

									if (push_result && !nib_push_stack_flag(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							case NI_BXOR_EQ:
								{
									*(lsp->_.lvalue._.stat.number) ^= *(rsp->_.lvalue._.stat.number);

									if (push_result && !nib_push_stack_flag(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}
					
					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_STAT:
		{
			switch(rsp->type)
			{
			case NST_STAT:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							if (lsp->_.lvalue._.stat.table != rsp->_.stat.table)
							{
								SETRET(nsr,INVALID);
								return true;
							}

							*(lsp->_.lvalue._.stat.number) = rsp->_.stat.number;

							if (push_result && !nib_push_stack_stat(nsr,*(lsp->_.lvalue._.number),rsp->_.stat.table))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_LVALUE:
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_STAT:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									if (lsp->_.lvalue._.stat.table != rsp->_.lvalue._.stat.table)
									{
										SETRET(nsr,INVALID);
										return true;
									}

									*(lsp->_.lvalue._.stat.number) = *(rsp->_.lvalue._.stat.number);

									if (push_result && !nib_push_stack_stat(nsr,*(lsp->_.lvalue._.stat.number),lsp->_.lvalue._.stat.table))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}
			default:
				SETRET(nsr,INVALID);
				return true;
			}
			break;
		}

	case NST_ACCOUNT:
		{
			switch(rsp->type)
			{
			case NST_ACCOUNT:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.account) = rsp->_.account;

							if (push_result && !nib_push_stack_account (nsr,*(lsp->_.lvalue._.account)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_MOBILE:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							// Grab the connected account
							if (IS_VALID(rsp->_.mobile) && rsp->_.mobile->desc)
								*(lsp->_.lvalue._.account) = rsp->_.mobile->desc->account;
							else
								*(lsp->_.lvalue._.account) = NULL;

							if (push_result && !nib_push_stack_account (nsr,*(lsp->_.lvalue._.account)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_NULL:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.account) = NULL;

							if (push_result && !nib_push_stack_account(nsr,*(lsp->_.lvalue._.account)))
							{
								SETRET(nsr,STACK);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			case NST_LVALUE:
				{
					switch(rsp->_.lvalue.type)
					{
					case NST_ACCOUNT:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.account) = *(rsp->_.lvalue._.account);

									if (push_result && !nib_push_stack_account (nsr,*(lsp->_.lvalue._.account)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					case NST_MOBILE:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									// Grab the connected account
									if (IS_VALID(*(rsp->_.lvalue._.mobile)) && (*(rsp->_.lvalue._.mobile))->desc)
										*(lsp->_.lvalue._.account) = (*(rsp->_.lvalue._.mobile))->desc->account;
									else
										*(lsp->_.lvalue._.account) = NULL;

									if (push_result && !nib_push_stack_account (nsr,*(lsp->_.lvalue._.account)))
									{
										SETRET(nsr,STACK);
										return true;
									}
									break;
								}

							default:
								SETRET(nsr,INVALID);
								return true;
							}
							break;
						}

					default:
						SETRET(nsr,INVALID);
						return true;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}

			break;
		}

	__asn(AFFECT,affect,affect,affect)
	__asn(AREA,area,area,area)
	// __asn(CHANNEL,channel,channel,channel)
	__asn(CLASS,clazz,clazz,class)
	__asn(DUNGEON,dungeon,dungeon,dungeon)
	__asn(EXIT,ex,ex,exit)
	__asn(INSTANCE,instance,instance,instance)
	__asn(LIQUID,liquid,liquid,liquid)
	__asn(MAIL,mail,mail,mail)
	__asn(MATERIAL,material,material,material)
	__asn(MISSION,mission,mission,mission)
	__asn(MOBILE,mobile,mobile,mobile)
	__asn(NOTE,note,note,note)
	__asn(OBJECT,object,object,object)
	__asn(ORG,org,org,org)
	// __asn(QUEST,quest,quest,quest)
	__asn(RACE,race,race,race)
	__asn(RANK,rank,rank,rank)
	__asn(REPUTATION,reputation,reputation,reputation)
	__asn(ROOM,room,room,room)
	__asn(SHIP,ship,ship,ship)
	__asn(SKILL,skill,skill,skill)
	__asn(TOKEN,token,token,token)
	__asn(WILDS,wilds,wilds,wilds)
	// __asn(WORLD,world,world,world)
	default:
		SETRET(nsr,INVALID);
		return true;
	}


	return false;
}

static bool __pop_method_arg(NIB_SCRIPT_RUNTIME *nsr, NIB_SCRIPT_ARG *arg)
{
	NIB_SCRIPT_STACK *sp = nib_pop_stack_raw(nsr);
	if (!sp)
		return false;

	arg->type = sp->type;
	switch(sp->type)
	{
	case NST_NUMBER:		arg->_.i = sp->_.i;	break;
	case NST_FLOAT:			arg->_.d = sp->_.d; break;
	case NST_BOOLEAN:		arg->_.b = sp->_.b; break;
	case NST_CHAR:			arg->_.ch = sp->_.ch; break;
	case NST_STRING:		arg->_.str = sp->_.str; break;
	case NST_STRING_S:		arg->_.str = sp->_.str; arg->type = NST_STRING; break;
	case NST_WIDEVNUM:		arg->_.wnum = sp->_.wnum; break;
	case NST_FLAG:
	case NST_STAT:
		arg->_.stat.number = sp->_.stat.number;
		arg->_.stat.table = sp->_.stat.table;
		break;
	case NST_LIST:
	case NST_LIST_S:
		arg->type = NST_LIST;
		arg->_.list.list = sp->_.list.list;
		arg->_.list.type = sp->_.list.type;
		break;
	case NST_AREA:			arg->_.area = sp->_.area; break;
	// case NST_DUNGEON:
	// case NST_INSTANCE:
	case NST_MOBILE:		arg->_.mobile = sp->_.mobile; break;
	// case NST_OBJECT:
	// case NST_QUEST:
	case NST_ROOM:			arg->_.room = sp->_.room; break;
	// case NST_SHIP:
	// case NST_TOKEN:
	case NST_LVALUE:
		{
			arg->type = sp->_.lvalue.type;
			switch(sp->_.lvalue.type)
			{
			case NST_NUMBER:		arg->_.i = *(sp->_.lvalue._.number); break;
			case NST_FLOAT:			arg->_.d = *(sp->_.lvalue._.d); break;
			case NST_BOOLEAN:		arg->_.b = *(sp->_.lvalue._.b); break;
			case NST_CHAR:			arg->_.ch = *(sp->_.lvalue._.ch); break;
			case NST_STRING:
			case NST_STRING_S:
				arg->type = NST_STRING;
				arg->_.str = *(sp->_.lvalue._.str);
				break;
			case NST_WIDEVNUM:		arg->_.wnum = *(sp->_.lvalue._.wnum); break;
			case NST_FLAG:
			case NST_STAT:
				arg->_.stat.number = *(sp->_.lvalue._.stat.number);
				arg->_.stat.table = sp->_.lvalue._.stat.table;
				break;
			case NST_LIST:
			case NST_LIST_S:
				arg->type = NST_LIST;
				arg->_.list.list = *(sp->_.lvalue._.list.list);
				arg->_.list.type = sp->_.lvalue._.list.type;
				break;
			case NST_AREA:			arg->_.area = *(sp->_.lvalue._.area); break;
			// case NST_DUNGEON:
			// case NST_INSTANCE:
			case NST_MOBILE:		arg->_.mobile = *(sp->_.lvalue._.mobile); break;
			// case NST_OBJECT:
			// case NST_QUEST:
			case NST_ROOM:			arg->_.room = *(sp->_.lvalue._.room); break;
			// case NST_SHIP:
			// case NST_TOKEN:
			default:
				return false;
			}
			break;
		}

	default:
		return false;
	}

	return true;
}

static bool __push_method_result(NIB_SCRIPT_RUNTIME *nsr, NIB_SCRIPT_ARG *result)
{
	switch(result->type)
	{
	case NST_NUMBER:		return nib_push_stack_number(nsr,result->_.i);
	case NST_FLOAT:			return nib_push_stack_float(nsr,result->_.d);
	case NST_BOOLEAN:		return nib_push_stack_boolean(nsr,result->_.b);
	case NST_CHAR:			return nib_push_stack_char(nsr,result->_.ch);
	case NST_STRING:		return nib_push_stack_string_raw(nsr,result->_.str);
	case NST_STRING_S:		return nib_push_stack_string_shared(nsr,result->_.str);
	case NST_WIDEVNUM:		return nib_push_stack_widevnum(nsr,&(result->_.wnum));
	case NST_FLAG:			return nib_push_stack_flag(nsr,result->_.stat.number,result->_.stat.table);
	case NST_STAT:			return nib_push_stack_stat(nsr,result->_.stat.number,result->_.stat.table);
	case NST_LIST:			return nib_push_stack_list_raw(nsr,result->_.list.list,result->_.list.type);
	case NST_LIST_S:		return nib_push_stack_list_shared(nsr,result->_.list.list,result->_.list.type);
	case NST_AREA:			return nib_push_stack_area(nsr,result->_.area);
	// case NST_DUNGEON:
	// case NST_INSTANCE:
	case NST_MOBILE:		return nib_push_stack_mobile(nsr,result->_.mobile);
	// case NST_OBJECT:
	// case NST_QUEST:
	case NST_ROOM:			return nib_push_stack_room(nsr,result->_.room);
	// case NST_SHIP:
	// case NST_TOKEN:
	default:
		return false;
	}
}

static bool __interpret_instruction(NIB_SCRIPT_RUNTIME *nsr)
{
	NIB_SCRIPT_STACK_TYPE type;

	nib_bytecode_t op = nsr->script->code[nsr->pc++];

	switch(op)
	{
	case NI_LVALUE_LOCAL:
		{
			short id = __get_short(nsr);
			NIB_LOCAL_RUNTIME_VAR *var = get_local_var(nsr, id);
			if (var)
			{
				if (!nib_push_stack_local_var_lvalue(nsr, var))
				{
					SETRET(nsr,STACK);
					return true;
				}
			}
			else
			{
				printf("Local variable %d not found", id);
				SETRET(nsr,FAILURE);
				return true;
			}
			break;
		}

	case NI_LVALUE_GLOBAL:
		{
			short id = __get_short(nsr);
			pVARIABLE var = get_global_var(nsr, id);

			if (var)
			{
				if (!nib_push_stack_global_var_lvalue(nsr, var))
				{
					SETRET(nsr,STACK);
					return true;
				}
			}
			else
			{
				printf("Global variable %d not found", id);
				SETRET(nsr,FAILURE);
				return true;
			}
			break;
		}

	case NI_LVALUE_SELF:
		break;

	case NI_LVALUE_BIT:
		{
			flag_value_t bit = __get_long(nsr);

			NIB_SCRIPT_LVALUE lvalue;
			if (!nib_pop_stack_lvalue(nsr,&lvalue))
			{
				SETRET(nsr,STACK);
				return true;
			}

			if (lvalue.type != NST_FLAG)
			{
				SETRET(nsr,INVALID);
				return true;
			}

			long *value = lvalue._.stat.number;
			const struct flag_type *table = lvalue._.stat.table;

			lvalue.type = NST_FLAG_BIT;
			lvalue._.bit.value = value;
			lvalue._.bit.table = table;
			lvalue._.bit.bit = bit;

			if (!nib_push_stack_lvalue(nsr,&lvalue))
			{
				SETRET(nsr,STACK);
				return true;
			}
			break;
		}

	case NI_LVALUE_FIELD:
		{
			short id = __get_short(nsr);

			NIB_SCRIPT_STACK_TYPE context = nib_peek_stack_lvalue_type(nsr);
			if (context == NST_UNKNOWN)
			{
				SETRET(nsr,STACK);
				return true;
			}

			NIB_FIELD *field = nib_field_get_byid(context, id);
			if (!field)
			{
				SETRET(nsr,FIELD);
				return true;
			}

			if (field->method)
			{
				NIB_SCRIPT_ARG this;

				if (!__pop_method_arg(nsr,&this))
				{
					SETRET(nsr,STACK);
					return true;
				}

				NIB_SCRIPT_ARG output;
				memset(&output,0,sizeof(output));
				output.type = field->stype;

				nsr->last_return = (*field->method)(nsr,1,&this,&output);

				if (!__push_method_result(nsr,&output))
				{
					SETRET(nsr,STACK);
					//SETRETN(nsr,__LINE__);
					return true;
				}

			}
			else
			{
				NIB_SCRIPT_LVALUE lvalue;
				if (!nib_pop_stack_lvalue(nsr, &lvalue))
				{
					SETRET(nsr,STACK);
					return true;
				}
				if (!nib_push_stack_field_lvalue(nsr, &lvalue, field))
				{
					SETRET(nsr,STACK);
					return true;
				}
			}


			break;
		}

	case NI_CALL_FUNCTION:
		{
			short id = __get_short(nsr);
			nib_bytecode_t argn = __get_bytecode(nsr);

			NIB_METHOD *method = nib_method_get_byid(NST_FUNCTION, id);
			if (!method)
			{
				SETRET(nsr,INVALID);
				return true;
			}

			NIB_SCRIPT_ARG *argv = nib_calloc(argn, sizeof(NIB_SCRIPT_ARG));
			if (!argv)
			{
				SETRET(nsr,MEMORY);
				return true;
			}

			for(int i = argn; i-- > 0;)
			{
				if (!__pop_method_arg(nsr,&argv[i]))
				{
					nib_free(argv);
					SETRET(nsr,STACK);
					// SETRETN(nsr,__LINE__);
					return true;
				}
			}
//typedef int METHOD_FUNC(NIB_SCRIPT_RUNTIME *nsr, int argc, NIB_SCRIPT_ARG *argv, NIB_SCRIPT_ARG *output);

			if (method->sresult == NST_VOID)
			{
				nsr->last_return = (*method->method)(nsr,argn,argv,NULL);
			}
			else
			{
				NIB_SCRIPT_ARG output;
				memset(&output,0,sizeof(output));
				output.type = method->sresult;

				nsr->last_return = (*method->method)(nsr,argn,argv,&output);

				if (!__push_method_result(nsr,&output))
				{
					nib_free(argv);
					SETRET(nsr,STACK);
					return true;
				}
			}

			nib_free(argv);

			// If something happened in the execution of the call that was considered fatal, fail the script
			if (nsr->last_return != SCPERR_SUCCESS)
				return true;
			break;
		}

	case NI_CALL_METHOD:
		{
			short id = __get_short(nsr);
			int argn = __get_bytecode(nsr) + 1;		// argv[0] == "this"

			NIB_SCRIPT_STACK_TYPE context = nib_peek_stack_lvalue_type(nsr);
			if (context == NST_UNKNOWN)
			{
				SETRET(nsr,STACK);
				return true;
			}

			NIB_METHOD *method = nib_method_get_byid(context, id);
			if (!method)
			{
				SETRET(nsr,INVALID);
				return true;
			}

			NIB_SCRIPT_ARG *argv = nib_calloc(argn, sizeof(NIB_SCRIPT_ARG));
			if (!argv)
			{
				SETRET(nsr,MEMORY);
				return true;
			}

			for(int i = argn; i-- > 0;)
			{
				if (!__pop_method_arg(nsr,&argv[i]))
				{
					nib_free(argv);
					SETRET(nsr,STACK);
					// SETRETN(nsr,(__LINE__ * 10 + i));
					return true;
				}
			}
//typedef int METHOD_FUNC(NIB_SCRIPT_RUNTIME *nsr, int argc, NIB_SCRIPT_ARG *argv, NIB_SCRIPT_ARG *output);

			if (method->sresult == NST_VOID)
			{
				nsr->last_return = (*method->method)(nsr,argn,argv,NULL);
			}
			else
			{
				NIB_SCRIPT_ARG output;
				memset(&output,0,sizeof(output));
				output.type = method->sresult;

				nsr->last_return = (*method->method)(nsr,argn,argv,&output);

				if (!__push_method_result(nsr,&output))
				{
					nib_free(argv);
					SETRET(nsr,STACK);
					// SETRETN(nsr,__LINE__);
					return true;
				}
			}

			nib_free(argv);

			// If something happened in the execution of the call that was considered fatal, fail the script
			if (nsr->last_return != SCPERR_SUCCESS)
				return true;
			break;
		}

	case NI_LOAD_NUMBER:
		{
			long number = __get_number(nsr);
			if (!nib_push_stack_number(nsr, number))
			{
				SETRET(nsr,STACK);
				return true;
			}
			break;
		}

	case NI_LOAD_FLOAT:
		{
			double value = __get_float(nsr);
			if (!nib_push_stack_float(nsr, value))
			{
				SETRET(nsr,STACK);
				return true;
			}
			break;
		}

	case NI_LOAD_CHAR:
		{
			utf8char_t value = __get_utf8char(nsr);
			if (!nib_push_stack_char(nsr, value))
			{
				SETRET(nsr,STACK);
				return true;
			}
		}
		break;

	case NI_LOAD_STRING:
		{
			short id = __get_short(nsr);
			if (id <= 0 || id > nsr->script->n_strings)
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			if (!nib_push_stack_string_shared(nsr, nsr->script->strings[id-1]))
			{
				SETRET(nsr,STACK);
				return true;
			}
			break;
		}

	case NI_LOAD_WIDEVNUM:
		{
			WNUM wnum;

			switch(nib_peek_stack(nsr))
			{
			case NST_NUMBER:
				if (!nib_pop_stack_number(nsr, &wnum.vnum))
				{
					nsr->last_return = SCPERR_FAILURE;
					return true;
				}
				break;

			case NST_LVALUE:
				{
					NIB_SCRIPT_LVALUE lvalue;
					if (!nib_pop_stack_lvalue(nsr, &lvalue) || lvalue.type != NST_NUMBER)
					{
						nsr->last_return = SCPERR_FAILURE;
						return true;
					}

					wnum.vnum = *(lvalue._.number);
					break;
				}
			
			default:
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			switch(nib_peek_stack(nsr))
			{
			case NST_AREA:
				if (!nib_pop_stack_area(nsr, &wnum.pArea))
				{
					nsr->last_return = SCPERR_FAILURE;
					return true;
				}
				break;

			case NST_LVALUE:
				{
					NIB_SCRIPT_LVALUE lvalue;
					if (!nib_pop_stack_lvalue(nsr, &lvalue) || lvalue.type != NST_AREA)
					{
						nsr->last_return = SCPERR_FAILURE;
						return true;
					}

					wnum.pArea = *(lvalue._.area);
					break;
				}
			
			default:
				SETRET(nsr,INVALID);
				return true;
			}

			if (!nib_push_stack_widevnum(nsr, &wnum))
			{
				SETRET(nsr,STACK);
				return true;
			}
			break;
		}

	case NI_LOAD_FLAG:
		{
			long number = __get_number(nsr);
			if (!nib_push_stack_flag(nsr, number, NULL))
			{
				SETRET(nsr,STACK);
				return true;
			}
			break;
		}

	case NI_LOAD_FLAG_TABLE:
		{
			long number = __get_number(nsr);
			short table_index = __get_short(nsr);

			if (table_index <= 0 || table_index > nsr->script->n_tables)
			{
				SETRET(nsr,INVALID);
				return true;
			}

			struct flag_type *table = nsr->script->tables[table_index - 1];

			if (!nib_push_stack_flag(nsr, number, table))
			{
				SETRET(nsr,STACK);
				return true;
			}
			break;
		}

	case NI_LOAD_STAT:
		{
			long number = __get_number(nsr);
			short table_index = __get_short(nsr);

			if (table_index <= 0 || table_index > nsr->script->n_tables)
			{
				SETRET(nsr,INVALID);
				return true;
			}

			struct flag_type *table = nsr->script->tables[table_index - 1];

			if (!nib_push_stack_stat(nsr, number, table))
			{
				SETRET(nsr,STACK);
				return true;
			}
			break;
		}

	case NI_NEW_LIST:
		{
			NIB_SCRIPT_STACK_TYPE type = (NIB_SCRIPT_STACK_TYPE)__get_bytecode(nsr);

			LLIST *list = NULL;
			switch(type)
			{
			case NST_NUMBER:	list = new_integer_list(); break;
			case NST_FLOAT:		list = new_float_list(); break;
			case NST_BOOLEAN:	list = new_boolean_list(); break;
			case NST_CHAR:		list = new_char_list(); break;
			case NST_STRING:	list = new_string_list(); break;
			case NST_WIDEVNUM:	list = new_widevnum_list(); break;
			case NST_AREA:		list = list_create(false); break;
			case NST_MOBILE:	list = list_create(false); break;
			case NST_ROOM:		list = list_create(false); break;
			default:
				SETRET(nsr,INVALID);
				return true;
			}

			if (!nib_push_stack_list_raw(nsr,list,type))
			{
				SETRET(nsr,STACK);
				return true;
			}
			break;
		}

	case NI_TRUE:
		if (!nib_push_stack_boolean(nsr, true))
		{
			SETRET(nsr,STACK);
			return true;
		}
		break;

	case NI_FALSE:
		if (!nib_push_stack_boolean(nsr, false))
		{
			SETRET(nsr,STACK);
			return true;
		}
		break;

	case NI_CONST0:
		if (!nib_push_stack_number(nsr, 0))
		{
			SETRET(nsr,STACK);
			return true;
		}
		break;

	case NI_CONST1:
		if (!nib_push_stack_number(nsr, 1))
		{
			SETRET(nsr,STACK);
			return true;
		}
		break;

	case NI_NCONST1:
		if (!nib_push_stack_number(nsr, -1))
		{
			SETRET(nsr,STACK);
			return true;
		}
		break;

	case NI_FCONST0:
		if (!nib_push_stack_float(nsr, 0.0))
		{
			SETRET(nsr,STACK);
			return true;
		}
		break;

	case NI_DUP:
		if (!nib_dup_stack(nsr))
		{
			SETRET(nsr,STACK);
			return true;
		}
		break;

	case NI_POP:
		if (!nib_pop_stack(nsr))
		{
			SETRET(nsr,STACK);
			return true;
		}
		break;

	case NI_POPN:
		{
			short n = __get_short(nsr);

			if (!nib_popn_stack(nsr, n))
			{
				SETRET(nsr,STACK);
				return true;
			}
			break;
		}

	case NI_RETURN:
		{
			NIB_SCRIPT_STACK *sp = nib_pop_stack_raw(nsr);

			if (!sp)
			{
				SETRET(nsr,STACK);
				return true;
			}

			long code = SCPERR_FAILURE;
			switch(sp->type)
			{
			case NST_NUMBER:	code = sp->_.i;	break;
			case NST_LVALUE:
				switch(sp->_.lvalue.type)
				{
				case NST_NUMBER:	code = *(sp->_.lvalue._.number); break;
				default:
					free_stack_item(sp);
					SETRET(nsr,INVALID);
					return true;
				}
				break;
			default:
				free_stack_item(sp);
				SETRET(nsr,INVALID);
				return true;
			}

			// Scripts can only really return non-negative return values
			nsr->last_return = (code < 0) ? SCPERR_FAILURE : code;
			return true;
		}

	case NI_RETURN_BYTE:
		{
			nsr->last_return = (long)(__get_bytecode(nsr));
			return true;
		}

	case NI_JUMP:
		{
			nib_address_t address = __get_address(nsr);
			nsr->pc = address;		
			break;
		}

	case NI_JUMP_ZERO:
		{
			nib_address_t address = __get_address(nsr);

			if(__is_top_zero(nsr))
				nsr->pc = address;

			nib_pop_stack(nsr);
			break;
		}

	case NI_JUMP_NOT_ZERO:
		{
			nib_address_t address = __get_address(nsr);

			if(__is_top_not_zero(nsr));
				nsr->pc = address;

			nib_pop_stack(nsr);
			break;
		}

	case NI_SWITCH:
		{
			short id = __get_short(nsr);
			nib_address_t no_match = __get_address(nsr);

			if (id <= 0 || id > nsr->script->n_switches)
			{
				SETRET(nsr,INVALID);
				// SETRETN(nsr,__LINE__);
				return true;
			}

			NIB_SWITCH *sw = &nsr->script->switches[id - 1];

			NIB_SCRIPT_STACK *sp = nib_pop_stack_raw(nsr);
			if (!sp)
			{
				SETRET(nsr,STACK);
				// SETRETN(nsr,__LINE__);
				return true;
			}

			nib_address_t address = NIB_INVALID_ADDRESS;
			switch(sw->type)
			{
			case NSWT_NUMBER:
				{
					if (sp->type != NST_NUMBER && (sp->type == NST_LVALUE && sp->_.lvalue.type != NST_NUMBER))
					{
						SETRET(nsr,INVALID);
						// SETRETN(nsr,__LINE__);
						return true;
					}

					long number;
					if (sp->type == NST_LVALUE)
						number = *(sp->_.lvalue._.number);
					else
						number = sp->_.i;
					
					for(int i = 0; address < 0 && i < sw->n_cases; i++)
					{
						NIB_SWITCH_CASE *cs = &sw->cases[i];

						switch(cs->type)
						{
						case NCASE_VALUE:
							if (number == cs->a.number) address = cs->address;
							break;
						case NCASE_VX:
							if (number >= cs->a.number) address = cs->address;
							break;
						case NCASE_XV:
							if (number <= cs->b.number) address = cs->address;
							break;
						case NCASE_VV:
							if ((number >= cs->a.number) &&
								(number <= cs->b.number)) address = cs->address;
							break;
						}
					}
					break;
				}

			case NSWT_FLOAT:
				{
					if (sp->type != NST_FLOAT && (sp->type == NST_LVALUE && sp->_.lvalue.type != NST_FLOAT))
					{
						SETRET(nsr,INVALID);
						return true;
					}

					double number;
					if (sp->type == NST_LVALUE)
						number = *(sp->_.lvalue._.d);
					else
						number = sp->_.d;
					
					for(int i = 0; address < 0 && i < sw->n_cases; i++)
					{
						NIB_SWITCH_CASE *cs = &sw->cases[i];

						switch(cs->type)
						{
						case NCASE_VALUE:
							if (number == cs->a.flt) address = cs->address;
							break;
						case NCASE_VX:
							if (number >= cs->a.flt) address = cs->address;
							break;
						case NCASE_XV:
							if (number <= cs->b.flt) address = cs->address;
							break;
						case NCASE_VV:
							if ((number >= cs->a.flt) &&
								(number <= cs->b.flt)) address = cs->address;
							break;
						}
					}
					break;
				}

			case NSWT_CHAR:
				{
					if (sp->type != NST_CHAR && (sp->type == NST_LVALUE && sp->_.lvalue.type != NST_CHAR))
					{
						SETRET(nsr,INVALID);
						return true;
					}

					utf8char_t ch;
					if (sp->type == NST_LVALUE)
						ch = *(sp->_.lvalue._.ch);
					else
						ch = sp->_.ch;
					
					for(int i = 0; address < 0 && i < sw->n_cases; i++)
					{
						NIB_SWITCH_CASE *cs = &sw->cases[i];

						switch(cs->type)
						{
						case NCASE_VALUE:
							if (ch == cs->a.ch) address = cs->address;
							break;
						case NCASE_VX:
							if (ch >= cs->a.ch) address = cs->address;
							break;
						case NCASE_XV:
							if (ch <= cs->b.ch) address = cs->address;
							break;
						case NCASE_VV:
							if ((ch >= cs->a.ch) &&
								(ch <= cs->b.ch)) address = cs->address;
							break;
						}
					}
					break;
				}

			case NSWT_STRING:
				{
					if (sp->type != NST_STRING &&
						sp->type != NST_STRING_S &&
						(sp->type == NST_LVALUE && sp->_.lvalue.type != NST_STRING))
					{
						free_stack_item(sp);
						SETRET(nsr,INVALID);
						return true;
					}

					char *str;
					if (sp->type == NST_LVALUE)
						str = *(sp->_.lvalue._.str);
					else
						str = sp->_.str;
					
					for(int i = 0; address < 0 && i < sw->n_cases; i++)
					{
						NIB_SWITCH_CASE *cs = &sw->cases[i];
						const char *cstr = nib_get_string(cs->a.str);

						switch(cs->type)
						{
						case NCASE_VALUE:
							if (!utf8_str_cmp(str, cstr)) address = cs->address;
							break;
						case NCASE_PREFIX:
							if (!utf8_str_prefix(str, cstr)) address = cs->address;
							break;
						case NCASE_INFIX:
							if (!utf8_str_infix(str, cstr)) address = cs->address;
							break;
						case NCASE_SUFFIX:
							if (!utf8_str_suffix(str, cstr)) address = cs->address;
							break;
						}
					}

					free_stack_item(sp);	// Done with the string data
					break;
				}

			case NSWT_STAT:
				{
					if (sp->type != NST_STAT && (sp->type == NST_LVALUE && sp->_.lvalue.type != NST_STAT))
					{
						SETRET(nsr,INVALID);
						return true;
					}

					long number;
					if (sp->type == NST_LVALUE)
						number = *(sp->_.lvalue._.stat.number);
					else
						number = sp->_.stat.number;
					
					for(int i = 0; address < 0 && i < sw->n_cases; i++)
					{
						NIB_SWITCH_CASE *cs = &sw->cases[i];

						switch(cs->type)
						{
						case NCASE_VALUE:
							if (number == cs->a.number) address = cs->address;
							break;
						case NCASE_VX:
							if (number >= cs->a.number) address = cs->address;
							break;
						case NCASE_XV:
							if (number <= cs->b.number) address = cs->address;
							break;
						case NCASE_VV:
							if ((number >= cs->a.number) &&
								(number <= cs->b.number)) address = cs->address;
							break;
						}
					}
					break;
				}

			default:
				free_stack_item(sp);
				SETRET(nsr,INVALID);
				return true;
			}

			if (address < 0)	// No matching case found, try default
				address = sw->default_address;

			if (address < 0)	// No matching case nor default found
				address = no_match;

			nsr->pc = address;
			break;
		}

	case NI_INC:
		if (!__increment_stack(nsr, false, false))
		{
			nsr->last_return = SCPERR_FAILURE;
			return true;
		}
		break;

	case NI_DEC:
		if (!__decrement_stack(nsr, false, false))
		{
			nsr->last_return = SCPERR_FAILURE;
			return true;
		}
		break;

	case NI_POST_INC:
		if (!__increment_stack(nsr, true, true))
		{
			nsr->last_return = SCPERR_FAILURE;
			return true;
		}
		break;

	case NI_POST_DEC:
		if (!__decrement_stack(nsr, true, true))
		{
			nsr->last_return = SCPERR_FAILURE;
			return true;
		}
		break;

	case NI_PRE_INC:
		if (!__increment_stack(nsr, false, true))
		{
			nsr->last_return = SCPERR_FAILURE;
			return true;
		}
		break;

	case NI_PRE_DEC:
		if (!__decrement_stack(nsr, false, true))
		{
			nsr->last_return = SCPERR_FAILURE;
			return true;
		}
		break;

	case NI_ITER_START:
		{
			LLIST *list;
			NIB_SCRIPT_STACK_TYPE type;

			NIB_SCRIPT_STACK *sp = nib_pop_stack_raw(nsr);

			if (!sp)
			{
				SETRET(nsr,STACK);
				return true;
			}

			switch(sp->type)
			{
			case NST_LIST:
			case NST_LIST_S:
				list = sp->_.list.list;
				type = sp->_.list.type;
				break;
			case NST_LVALUE:
				switch(sp->_.lvalue.type)
				{
				case NST_LIST:
				case NST_LIST_S:
					list = *(sp->_.lvalue._.list.list);
					type = sp->_.lvalue._.list.type;
					break;
				default:
					free_stack_item(sp);
					SETRET(nsr,INVALID);
					return true;
				}
				break;
			default:
				free_stack_item(sp);
				SETRET(nsr,INVALID);
				return true;
			}

			// Push iterator onto stack
			if (!nib_push_stack_iterator(nsr, list, type))
			{
				SETRET(nsr,STACK);
				return true;
			}
			break;
		}

	case NI_ITER_STOP:
		{
			ITERATOR it;
			LLIST *list;
			NIB_SCRIPT_STACK_TYPE type;
			if (!nib_pop_stack_iterator(nsr,&it,&list,&type))
			{
				SETRET(nsr,STACK);
				return true;
			}
			iterator_stop(&it);
			break;
		}

	case NI_ITER_NEXT:
		{
			nib_address_t address = __get_address(nsr);
			short id = __get_short(nsr);
			NIB_LOCAL_RUNTIME_VAR *var = get_local_var(nsr, id);
			if (!var)
			{
				SETRET(nsr,FAILURE);
				// SETRETN(nsr,__LINE__);
				return true;
			}

			ITERATOR *it;
			LLIST *list;
			NIB_SCRIPT_STACK_TYPE type;
			// Leave the iterator on the stack
			if (!nib_peek_stack_iterator (nsr,-1,&it,&list,&type))
			{
				SETRET(nsr,STACK);
				// SETRETN(nsr,__LINE__);
				return true;
			}

			if (type != var->type)
			{
				SETRET(nsr,INVALID);
				// SETRETN(nsr,__LINE__);
				return true;
			}

			void *data = iterator_nextdata(it);
			if (!data)
			{
				nsr->pc = address;
			}
			else
			{
				// Assign to the variable
				switch(type)
				{
				case NST_NUMBER:	var->_.i = *((long *)data); break;
				case NST_FLOAT:		var->_.f = *((double *)data); break;
				case NST_BOOLEAN:	var->_.b = *((bool *)data); break;
				case NST_CHAR:		var->_.ch = *((utf8char_t *)data); break;
				case NST_STRING:
					if (var->_.str) free(var->_.str);
					var->_.str = strdup((char *)data);
					break;
				case NST_WIDEVNUM:	var->_.wnum = *((WNUM *)data); break;
				case NST_AREA:		var->_.area = (AREA_DATA *)data; break;
				case NST_MOBILE:	var->_.mobile = (CHAR_DATA *)data; break;
				case NST_ROOM:		var->_.room = (ROOM_INDEX_DATA *)data; break;
				default:
					SETRET(nsr,INVALID);
					// SETRETN(nsr,__LINE__);
					return true;
				}
			}
		}
		break;

	case NI_LAND:
	case NI_LOR:
	case NI_LXOR:
	case NI_EQ:
	case NI_NEQ:
	case NI_LT:
	case NI_LE:
	case NI_GT:
	case NI_GE:
		if (__boolean_operation(nsr, op))
			return true;
		break;

	case NI_LNOT:
		{
			bool value = __is_top_zero(nsr);

			nib_pop_stack(nsr);
			nib_push_stack_boolean(nsr, value);
			break;
		}

	case NI_NEG:
		{
			switch(nib_peek_stack(nsr))
			{
			case NST_NUMBER:
				{
					long value;
					if (!nib_pop_stack_number(nsr, &value))
					{
						nsr->last_return = SCPERR_FAILURE;
						return true;
					}

					nib_push_stack_number(nsr, -value);
					break;
				}
			case NST_FLOAT:
				{
					double value;
					if (!nib_pop_stack_float(nsr, &value))
					{
						nsr->last_return = SCPERR_FAILURE;
						return true;
					}

					nib_push_stack_float(nsr, -value);
					break;
				}

			case NST_LVALUE:
				{
					NIB_SCRIPT_LVALUE lvalue;
					if (!nib_pop_stack_lvalue(nsr, &lvalue))
					{
						nsr->last_return = SCPERR_FAILURE;
						return true;
					}

					switch(lvalue.type)
					{
					case NST_NUMBER:	nib_push_stack_number(nsr, -(*(lvalue._.number))); break;
					case NST_FLOAT:		nib_push_stack_float(nsr, -(*(lvalue._.d))); break;
					}
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}
		}
		break;

	case NI_ADD:
	case NI_SUBT:
	case NI_MULT:
	case NI_MOD:
	case NI_DIV:
	case NI_BAND:
	case NI_BOR:
	case NI_BXOR:
	case NI_LSH:
	case NI_RSH:
	case NI_RSHL:
		if (__binary_operation(nsr, op))
			return true;
		break;

	case NI_BNOT:
		{
			switch(nib_peek_stack(nsr))
			{
			case NST_NUMBER:
				{
					long value;
					if (!nib_pop_stack_number(nsr, &value))
					{
						SETRET(nsr,STACK);
						return true;
					}

					if (!nib_push_stack_number(nsr, ~value))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_FLAG:
				{
					long value;
					const struct flag_type *table;
					if (!nib_pop_stack_flag(nsr, &value, &table))
					{
						SETRET(nsr,STACK);
						return true;
					}

					if (!nib_push_stack_flag(nsr, ~value, table))
					{
						SETRET(nsr,STACK);
						return true;
					}
					break;
				}

			case NST_LVALUE:
				{
					NIB_SCRIPT_LVALUE lvalue;
					if (!nib_pop_stack_lvalue(nsr, &lvalue))
					{
						SETRET(nsr,STACK);
						return true;
					}

					switch(lvalue.type)
					{
					case NST_NUMBER:
						if (!nib_push_stack_number(nsr, ~(*(lvalue._.number))))
						{
							SETRET(nsr,STACK);
							return true;
						}
						break;
					case NST_FLAG:
						if (!nib_push_stack_flag(nsr, ~(*(lvalue._.stat.number)), lvalue._.stat.table))
						{
							SETRET(nsr,STACK);
							return true;
						}
						break;
					}
					break;
				}

			default:
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}
		}
		break;

	case NI_ASSIGN:
	case NI_VOID_ASSIGN:
	case NI_ADD_EQ:
	case NI_VOID_ADD_EQ:
	case NI_SUBT_EQ:
	case NI_MULT_EQ:
	case NI_MOD_EQ:
	case NI_DIV_EQ:
	case NI_BAND_EQ:
	case NI_BOR_EQ:
	case NI_BXOR_EQ:
	case NI_LSH_EQ:
	case NI_RSH_EQ:
	case NI_RSHL_EQ:
		if (__assignment_operation(nsr,op))
			return true;
		break;

	case NI_STR_PREFIX:
		if (__boolean_operation(nsr, NI_STR_PREFIX))
			return true;
		break;

	case NI_STR_INFIX:
		if (__boolean_operation(nsr, NI_STR_INFIX))
			return true;
		break;

	case NI_STR_SUFFIX:
		if (__boolean_operation(nsr, NI_STR_SUFFIX))
			return true;
		break;

	case NI_GET_AREA:
		{
			AREA_DATA *area = NULL;
			switch(nib_peek_stack(nsr))
			{
			case NST_NUMBER:
				{
					long value;
					if (!nib_pop_stack_number(nsr, &value))
					{
						SETRET(nsr,STACK);
						return true;
					}

					area = get_area_from_uid(value);
					break;
				}

			case NST_STRING:
				{
					char *name;
					if (!nib_pop_stack_string(nsr, &name))
					{
						if (name) free(name);
						SETRET(nsr,STACK);
						return true;
					}

					area = find_area(name);
					if (name) free(name);
					break;
				}

			case NST_STRING_S:
				{
					char *name;
					if (!nib_pop_stack_string_shared(nsr, &name))
					{
						SETRET(nsr,STACK);
						return true;
					}

					area = find_area(name);
					break;
				}

			case NST_LVALUE:
				{
					NIB_SCRIPT_LVALUE lvalue;
					if (!nib_pop_stack_lvalue(nsr, &lvalue))
					{
						SETRET(nsr,STACK);
						return true;
					}

					switch(lvalue.type)
					{
					case NST_NUMBER:
						{
							area = get_area_from_uid(*(lvalue._.number));
							break;
						}

					case NST_STRING:
					case NST_STRING_S:
						{
							area = find_area(*(lvalue._.str));
							break;
						}
					
					default:
						SETRET(nsr,INVALID);
						return true;
					}
					
					break;
				}

			default:
				SETRET(nsr,INVALID);
				return true;
			}

			if (!nib_push_stack_area(nsr, area))
			{
				SETRET(nsr,STACK);
				return true;
			}
			break;
		}
	}

	return false;
}

int nib_interpret_script(NIB_SCRIPT *script /* add arguments */)
{
	// Generate the script runtime
	NIB_SCRIPT_RUNTIME *nsr = new_script_runtime(script);
	if (!nsr) return SCPERR_MEMORY;

	bool running = true;
	while(running && nsr->pc < script->code_len)
	{
		if (__interpret_instruction(nsr))
		{
			running = false;
			break;
		}
	}

	// Finished the code but still have something left on the stack
	//  Indicates not enough pops
	if (running && nsr->sp > 0)
		nsr->last_return = SCPERR_STACK;

	int ret = nsr->last_return;

	if (nsr->debug[0])
		printf("DEBUG: '%s'\n", nsr->debug);

	free_script_runtime(nsr);
	return ret;
}

// Have a STEP execution system

static const char *opcode_names[] = {
	"--ILLEGAL--",
	"LVALUE_LOCAL",
	"LVALUE_GLOBAL",
	"LVALUE_SELF",
	"LVALUE_BIT",
	"LVALUE_FIELD",
	"CALL_FUNCTION",
	"CALL_METHOD",
	"LOAD_NUMBER",
	"LOAD_FLOAT",
	"LOAD_CHAR",
	"LOAD_STRING",
	"LOAD_WIDEVNUM",
	"LOAD_FLAG",
	"LOAD_FLAG_TABLE",
	"LOAD_STAT",
	"NEW_LIST",
	"NULL",
	"TRUE",
	"FALSE",
	"CONST0",
	"CONST1",
	"NCONST1",
	"FCONST0",
	"DUP",
	"POP",
	"POPN",
	"RETURN",
	"RETURN_BYTE",
	"JUMP",
	"JUMP_ZERO",
	"JUMP_NOT_ZERO",
	"SWITCH",
	"INC",
	"DEC",
	"POST_INC",
	"POST_DEC",
	"PRE_INC",
	"PRE_DEC",
	"ITER_START",
	"ITER_STOP",
	"ITER_NEXT",
	"LAND",
	"LOR",
	"LXOR",
	"LNOT",
	"ASSIGN",
	"VOID_ASSIGN",
	"NEG",
	"ADD",
	"SUBT",
	"MULT",
	"MOD",
	"DIV",
	"EQ",
	"NEQ",
	"LT",
	"LE",
	"GT",
	"GE",
	"BAND",
	"BOR",
	"BXOR",
	"BNOT",
	"LSH",
	"RSH",
	"RSHL",
	"STRPREFIX",
	"STRINFIX",
	"STRSUFFIX",
	"ADD_EQ",
	"VOID_ADD_EQ",
	"SUBT_EQ",
	"MULT_EQ",
	"MOD_EQ",
	"DIV_EQ",
	"BAND_EQ",
	"BOR_EQ",
	"BXOR_EQ",
	"LSH_EQ",
	"RSH_EQ",
	"RSHL_EQ",
	"GET_AREA",
	"GET_CLASS",
	"GET_LIQUID",
	"GET_MATERIAL",
	"GET_ORG",
	"GET_RACE",
	"GET_SKILL",
	"GET_WILDS",
};

static void __add_dissassembled_line(LLIST *assembly, long address, char *str, bool is_comment)
{
	NIB_DISASSEMBLED_LINE *line = calloc(1, sizeof(NIB_DISASSEMBLED_LINE));

	if (line)
	{
		line->address = address;
		line->str = strdup(str);
		line->is_comment = is_comment;

		list_appendlink(assembly, line);
	}
}

static void __free_disassembled_line(void *ptr)
{
	NIB_DISASSEMBLED_LINE *line = (NIB_DISASSEMBLED_LINE *)ptr;

	if (line)
	{
		free(line->str);
		free(line);
	}
}

static void __add_comments(LLIST *assembly, NIB_SCRIPT *script, long address)
{
	ITERATOR it;
	NIB_SCRIPT_COMMENT *comment;

	bool first = true;
	iterator_start(&it, script->comments);
	while((comment = (NIB_SCRIPT_COMMENT *)iterator_nextdata(&it)))
	{
		if (comment->address == address)
		{
			if (first) {
				__add_dissassembled_line(assembly, comment->address, "", true);
				first = false;
			}

			__add_dissassembled_line(assembly, comment->address, comment->comment, true);
		}
	}
	iterator_stop(&it);
}


static LLIST *__generate_disassembly(NIB_SCRIPT *script)
{
	LLIST *assembly = list_createx(false, NULL, __free_disassembled_line);

	if (list_isvalid(assembly))
	{
		nib_bytecode_p pc = script->code;
		uintptr_t addr = 0;
		nib_address_t address;
		long number;
		char ch;
		double floating;
		short index;
		void *pointer;
		struct flag_type *table;
		NIB_SCRIPT_STACK_TYPE type = NST_UNKNOWN;

		__add_comments(assembly, script, addr);

		char line[1000];
		while(addr < script->code_len)
		{
			int linej = 0;

			uintptr_t start_addr = addr;

			if (pc[addr] == NI_ILLEGAL || pc[addr] >= NI__MAX)
			{
				snprintf(line, sizeof(line) - 1, "--ILLEGAL OPCODE--");
				__add_dissassembled_line(assembly, start_addr, line, false);				
				break;
			}
			else
			{
				linej = snprintf(line, sizeof(line) - 1, "%-16s", opcode_names[pc[addr]]);
			}

			switch(pc[addr])
			{
			case NI_LVALUE_SELF:
			case NI_LOAD_WIDEVNUM:
			case NI_CONST0:
			case NI_CONST1:
			case NI_NCONST1:
			case NI_FCONST0:
			case NI_DUP:
			case NI_POP:
			case NI_RETURN:
			case NI_INC:
			case NI_DEC:
			case NI_POST_INC:
			case NI_POST_DEC:
			case NI_PRE_INC:
			case NI_PRE_DEC:
			case NI_LAND:
			case NI_LOR:
			case NI_LXOR:
			case NI_LNOT:
			case NI_ASSIGN:
			case NI_VOID_ASSIGN:
			case NI_NEG:
			case NI_ADD:
			case NI_SUBT:
			case NI_MULT:
			case NI_MOD:
			case NI_DIV:
			case NI_EQ:
			case NI_NEQ:
			case NI_LT:
			case NI_LE:
			case NI_GT:
			case NI_GE:
			case NI_BAND:
			case NI_BOR:
			case NI_BXOR:
			case NI_BNOT:
			case NI_LSH:
			case NI_RSH:
			case NI_RSHL:
			case NI_STR_PREFIX:
			case NI_STR_INFIX:
			case NI_STR_SUFFIX:
			case NI_ADD_EQ:
			case NI_VOID_ADD_EQ:
			case NI_SUBT_EQ:
			case NI_MULT_EQ:
			case NI_MOD_EQ:
			case NI_DIV_EQ:
			case NI_BAND_EQ:
			case NI_BOR_EQ:
			case NI_BXOR_EQ:
			case NI_LSH_EQ:
			case NI_RSH_EQ:
			case NI_RSHL_EQ:
				break;

			case NI_SWITCH:
			{
				memcpy(&index, &pc[addr+1], sizeof(index)); addr+=sizeof(index);
				memcpy(&address, &pc[addr+1], sizeof(address)); addr+=sizeof(address);

				linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d <addr: %08X>", index, address);

				break;
			}

			case NI_GET_AREA:
				type = NST_AREA;
				break;

			case NI_RETURN_BYTE:
			{
				nib_bytecode_t ret = pc[addr+1];

				linej += snprintf(line + linej, sizeof(line) - linej - 1, " %02.2X", ret);

				addr++;
				break;
			}

			case NI_NEW_LIST:
			{
				NIB_SCRIPT_STACK_TYPE list_type = (NIB_SCRIPT_STACK_TYPE)pc[addr+1];

				linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d (new list(%s))", list_type, nst_to_type(list_type));

				addr++;
				break;
			}

			case NI_POPN:
				{
					short n;
					memcpy(&n, &pc[addr+1], sizeof(n));
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d", n);

					addr+=sizeof(n);
					break;
				}

			case NI_LVALUE_LOCAL:
				{
					short id;
					memcpy(&id, &pc[addr+1], sizeof(id));
					NIB_VARIABLE *var = nib_get_local_variable_byid(id);

					if (var)
					{
						type = var->stype;
						linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d (%s %s)", id, nib_get_typename(script,var->type), var->name);
					}
					else
					{
						type = NST_UNKNOWN;
						linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d --invalid--", id);
					}

					addr += sizeof(id);
					break;
				}

			case NI_LVALUE_GLOBAL:
				{
					short id;
					memcpy(&id, &pc[addr+1], sizeof(id));
					NIB_VARIABLE *var = nib_get_global_variable_byid(id);

					if (var)
					{
						type = var->stype;
						linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d (%s %s)", id, nib_get_typename(script,var->type), var->name);
					}
					else
					{
						type = NST_UNKNOWN;
						linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d --invalid--", id);
					}

					addr += sizeof(id);
					break;
				}

			case NI_LVALUE_FIELD:
				{
					short id;
					memcpy(&id, &pc[addr+1], sizeof(id));

					NIB_FIELD *field = nib_field_get_byid(type, id);
					if (field)
					{
						type = field->stype;
						linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d (%s .%s)", id, nib_get_typename(script,field->type), field->name);
					}
					else
					{
						type = NST_UNKNOWN;
						linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d --invalid--", id);
					}

					addr += sizeof(id);
					break;
				}

			// Allows for access a flag bit
			case NI_LVALUE_BIT:
				{
					flag_value_t bit;
					memcpy(&bit, &pc[addr+1], sizeof(flag_value_t));

					linej += snprintf(line + linej, sizeof(line) - linej - 1, " <[%08X]>", bit);

					addr += sizeof(flag_value_t);
					break;
				}

			case NI_CALL_FUNCTION:
				type = NST_FUNCTION;

			case NI_CALL_METHOD:
				{
					short id;
					unsigned char args;
					memcpy(&id, &pc[addr+1], sizeof(id));		addr += sizeof(id);
					args = (unsigned char)pc[addr+1];			addr++;

					NIB_METHOD *method = nib_method_get_byid(type, id);
					if (method)
					{
						type = method->sresult;
						linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d, %d (%s %s)", id, args, nib_get_typename(script,method->result), method->name);
					}
					else
					{
						type = NST_UNKNOWN;
						linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d, %d --invalid--", id, args);
					}

					break;
				}

			case NI_LOAD_STRING:
				{
					memcpy(&index, &pc[addr+1], sizeof(index));
					const char *str = nib_get_string(index);

					if (str)
						linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d \"%s\"", index, str);
					else
						linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d --invalid--", index);

					addr+=sizeof(index);
					break;
				}

			case NI_LOAD_CHAR:
				{
					utf8char_t ch;
					memcpy(&ch,&pc[addr+1],sizeof(ch)); addr+=sizeof(ch);
					if (utf8_isprint(ch))
						linej += snprintf(line + linej, sizeof(line) - linej - 1, " %s", utf8_getbytes(ch));
					else
						linej += snprintf(line + linej, sizeof(line) - linej - 1, " 0x%X", ch);
				}
				break;

			case NI_LOAD_NUMBER:
				memcpy(&number, &pc[addr+1], sizeof(number));
				linej += snprintf(line + linej, sizeof(line) - linej - 1, " %ld", number);

				addr+=sizeof(number);
				break;

			case NI_LOAD_FLOAT:
				memcpy(&floating, &pc[addr+1], sizeof(floating));
				linej += snprintf(line + linej, sizeof(line) - linej - 1, " %lf", floating);

				addr+=sizeof(floating);
				break;

			case NI_LOAD_FLAG:
				memcpy(&number, &pc[addr+1], sizeof(number));
				linej += snprintf(line + linej, sizeof(line) - linej - 1, " <[%08X]>", number);

				addr+=sizeof(number);
				break;

			case NI_LOAD_FLAG_TABLE:
				memcpy(&number, &pc[addr+1], sizeof(number)); addr += sizeof(number);
				memcpy(&index, &pc[addr+1], sizeof(index)); addr+=sizeof(index);
				if (index > 0 && index <= script->n_tables)
					table = script->tables[index - 1];
				else
					table = NULL;

				linej += snprintf(line + linej, sizeof(line) - linej - 1, " <[%08X@%s]>", number, nib_get_flag_table_name(script->flag_tables,table));
				break;

			case NI_LOAD_STAT:
				memcpy(&number, &pc[addr+1], sizeof(number)); addr += sizeof(number);
				memcpy(&index, &pc[addr+1], sizeof(index)); addr+=sizeof(index);
				if (index > 0 && index <= script->n_tables)
					table = script->tables[index - 1];
				else
					table = NULL;

				linej += snprintf(line + linej, sizeof(line) - linej - 1, " %ld@%s", number, nib_get_stat_table_name(script->stat_tables,table));
				break;

			case NI_JUMP:
			case NI_JUMP_ZERO:
			case NI_JUMP_NOT_ZERO:
				memcpy(&address, &pc[addr+1], sizeof(address));
				linej += snprintf(line + linej, sizeof(line) - linej - 1, " <addr: %08X>", address);

				addr+=sizeof(address);
				break;

			case NI_ITER_NEXT:
				{
					short id;
					memcpy(&address, &pc[addr+1], sizeof(address));	addr+=sizeof(address);
					memcpy(&id, &pc[addr+1], sizeof(id)); addr+= sizeof(id);

					NIB_VARIABLE *var = nib_get_local_variable_byid(id);

					if (var)
					{
						linej += snprintf(line + linej, sizeof(line) - linej - 1, " <addr: %08X> %d (%s %s)", address, id, nib_get_typename(script,var->type), var->name);
					}
					else
					{
						linej += snprintf(line + linej, sizeof(line) - linej - 1, " <addr: %08X> %d --invalid--", address, id);
					}
					break;
				}
			}
			addr++;


			// Pad and clip
			for(; linej < 60; linej++)
				line[linej] = ' ';
			linej = 60;
			line[linej++] = ' ';
			line[linej++] = ';';
			for(uintptr_t a = start_addr; a < addr; a++)
			{
				linej += snprintf(line + linej, sizeof(line) - linej - 1, " %02.2X", (unsigned char)pc[a]);
			}

			__add_dissassembled_line(assembly, start_addr, line, false);

			__add_comments(assembly, script, addr);
		}

		__add_dissassembled_line(assembly, addr, "--End of Code--", true);
	}

	return assembly;
}

NIB_SCRIPT_RUNTIME *nib_step_execute_init(NIB_SCRIPT *script)
{
	NIB_SCRIPT_RUNTIME *nsr = new_script_runtime(script);

	nsr->disassembly = __generate_disassembly(script);
	
	return nsr;
}

bool nib_is_execution_done(NIB_SCRIPT_RUNTIME *nsr)
{
	return nsr->pc >= nsr->script->code_len;
}


bool nib_step_execute(NIB_SCRIPT_RUNTIME *nsr)
{
	if (__interpret_instruction(nsr))
		return true;

	return nib_is_execution_done(nsr);
}

void nib_step_execute_cleanup(NIB_SCRIPT_RUNTIME *nsr)
{
	if (nsr)
		free_script_runtime(nsr);
}

// Find the first so that current PC is "centered"
static int __find_first_line(NIB_SCRIPT_RUNTIME *nsr, int span)
{
	ITERATOR it;
	int index = 0;

	NIB_DISASSEMBLED_LINE *ndl;
	iterator_start(&it, nsr->disassembly);
	while((ndl = (NIB_DISASSEMBLED_LINE *)iterator_nextdata(&it)))
	{
		++index;
		if (ndl->address == nsr->pc && !ndl->is_comment)
		{
			break;
		}
	}
	iterator_stop(&it);

	// index = 14
	// size = 16
	// span = 7
	// cspan = 4
	// nspan = 3

	// index = 14 - 4 + 1
	// 11
	// 12
	// 13
	// 14: inverted
	// 15
	// 16: last
	// 17: OOB

	// index = 16 - 7 + 1
	// 10
	// 11
	// 12
	// 13
	// 14: inverted
	// 15:
	// 16: last

	int cspan = (span + 1) / 2;
	int nspan = span - cspan;
	if (index > 0)
	{
		if ((index + nspan) > list_size(nsr->disassembly))
		{
			index = list_size(nsr->disassembly) - span + 1;
		}
		else
		{
			index -= cspan - 1;
		}
	}

	if (index < 1)
		index = 1;	// Clamp to the first one

	return index;
}

static int __display_stack_item(NIB_SCRIPT_STACK *stack, char *line, int max_len, NIB_SCRIPT *script)
{
	switch(stack->type)
	{
	case NST_NUMBER:	return snprintf(line, max_len, "INT(%ld)", stack->_.i);
	case NST_BOOLEAN:	return snprintf(line, max_len, "BLN(%s)", (stack->_.b) ? "true" : "false");
	case NST_FLOAT:		return snprintf(line, max_len, "FLT(%lf)", stack->_.d);
	case NST_CHAR:
			if (utf8_isprint(stack->_.ch))
				return snprintf(line, max_len, "CHR(%s)", utf8_getbytes(stack->_.ch));
			else
				return snprintf(line, max_len, "CHR(%X)", stack->_.ch);
	case NST_STRING:	return snprintf(line, max_len, "STR(%s)", (stack->_.str)?stack->_.str:"null");
	case NST_STRING_S:	return snprintf(line, max_len, "STRS(%s)", (stack->_.str)?stack->_.str:"null");
	case NST_MAP:		return snprintf(line, max_len, "MAP");
	case NST_WIDEVNUM:
		return snprintf(line, max_len, "WVN(%ld,%ld)", 
				(stack->_.wnum.pArea != NULL ) ? stack->_.wnum.pArea->uid : 0,
				stack->_.wnum.vnum
			);
	case NST_AREA:		return snprintf(line, max_len, "AREA(%ld)", (stack->_.area != NULL)?stack->_.area->uid:0);
	// case NST_INSTANCE:
	// case NST_DUNGEON:
	case NST_MOBILE:	return snprintf(line, max_len, "MOBILE");
	// case NST_OBJECT:
	// case NST_QUEST:
	case NST_ROOM:		return snprintf(line, max_len, "ROOM(%ld)", (stack->_.room != NULL)?stack->_.room->vnum:0);
	// case NST_SHIP:
	// case NST_TOKEN:
	case NST_FLAG:
		if (stack->_.stat.table)
			return snprintf(line, max_len, "FLG(%08X@%s)", stack->_.stat.number, nib_get_flag_table_name(script->flag_tables,stack->_.stat.table));
		else
			return snprintf(line, max_len, "FLG(%08X)", stack->_.stat.number);
	case NST_STAT:		return snprintf(line, max_len, "STA(%08X)", stack->_.stat.number);
	case NST_LIST:		return snprintf(line, max_len, "LST(%s)", nst_to_type(stack->_.list.type));
	case NST_LIST_S:	return snprintf(line, max_len, "LSTS(%s)", nst_to_type(stack->_.list.type));
	case NST_ITERATOR:
		return snprintf(line, max_len, "ITER(%s)", nst_to_type(stack->_.iter.type));
	case NST_LVALUE:
		switch(stack->_.lvalue.type)
		{
		case NST_NUMBER:	return snprintf(line, max_len, "LVALUE(INT(%ld))", *(stack->_.lvalue._.number));
		case NST_BOOLEAN:	return snprintf(line, max_len, "LVALUE(BLN(%s))", *(stack->_.lvalue._.b) ? "true" : "false");
		case NST_FLOAT:		return snprintf(line, max_len, "LVALUE(FLT(%lf))", *(stack->_.lvalue._.d));
		case NST_CHAR:
				if (utf8_isprint(*(stack->_.lvalue._.ch)))
					return snprintf(line, max_len, "CHR(%s)", utf8_getbytes(*(stack->_.lvalue._.ch)));
				else
					return snprintf(line, max_len, "CHR(%X)", *(stack->_.lvalue._.ch));
		case NST_STRING:	return snprintf(line, max_len, "LVALUE(STR(%s))", (*(stack->_.lvalue._.str))?*(stack->_.lvalue._.str):"null");
		case NST_MAP:		return snprintf(line, max_len, "LVALUE(MAP)");
		case NST_WIDEVNUM:
			return snprintf(line, max_len, "LVALUE(WVN(%ld,%ld))", 
					(stack->_.lvalue._.wnum->pArea != NULL ) ? stack->_.lvalue._.wnum->pArea->uid : 0,
					stack->_.lvalue._.wnum->vnum
				);
		case NST_AREA:		return snprintf(line, max_len, "LVALUE(AREA(%ld))", (*(stack->_.lvalue._.area) != NULL)?(*(stack->_.lvalue._.area))->uid:0);
		// case NST_INSTANCE:
		// case NST_DUNGEON:
		case NST_MOBILE:	return snprintf(line, max_len, "LVALUE(MOBILE)");
		// case NST_OBJECT:
		// case NST_QUEST:
		case NST_ROOM:		return snprintf(line, max_len, "LVALUE(ROOM(%ld))", (*(stack->_.lvalue._.room) != NULL)?(*(stack->_.lvalue._.room))->vnum:0);
		// case NST_SHIP:
		// case NST_TOKEN:
		case NST_FLAG:
			if (stack->_.lvalue._.stat.table)
				return snprintf(line, max_len, "LVALUE(FLG(%08X@%s))", *(stack->_.lvalue._.number),nib_get_flag_table_name(script->flag_tables,stack->_.lvalue._.stat.table));
			else
				return snprintf(line, max_len, "LVALUE(FLG(%08X))", *(stack->_.lvalue._.number));
		case NST_FLAG_BIT:	return snprintf(line, max_len, "LVALUE(BIT(%08X:%s))", stack->_.lvalue._.bit.bit, (IS_SET(*(stack->_.lvalue._.bit.value),stack->_.lvalue._.bit.bit)?"ON":"OFF"));
		case NST_STAT:		return snprintf(line, max_len, "LVALUE(STA(%08X))", *(stack->_.lvalue._.number));
		case NST_LIST:		return snprintf(line, max_len, "LVALUE(LST(%s))", nst_to_type(stack->_.lvalue._.list.type));
//		case NST_LIST_S:	return snprintf(line, max_len, "LSTS(%s)", nst_to_type(stack->_.list.type));

		default:			return snprintf(line, max_len, "LVALUE(???)");
		}
		break;
	case NST_NULL:		return snprintf(line, max_len, "<NULL>");
	}

	return snprintf(line, max_len, "type = %s(%d)", nst_to_type(stack->type), stack->type);
}

static void __display_local_var(NIB_LOCAL_RUNTIME_VAR *var, char *line, int max_len, NIB_SCRIPT *script)
{
	int linej = snprintf(line, max_len, "%s(%ld)", var->name, var->type);
	line[linej] = 0;

	char value[SIDE_PANEL_WIDTH+1];
	int max_vlen = max_len - linej - 1;
	switch(var->type)
	{
	case NST_NUMBER:	snprintf(value, max_vlen, "%ld", var->_.i);	break;
	case NST_BOOLEAN:	strncpy(value, (var->_.b?"true":"false"), max_vlen);	break;
	case NST_FLOAT:		snprintf(value, max_vlen, "%lf", var->_.f);	break;
	case NST_CHAR:
			if (utf8_isprint(var->_.ch))
				snprintf(value, max_vlen, "'%s'", utf8_getbytes(var->_.ch));
			else
				snprintf(value, max_vlen, "0x%X", var->_.ch);
			break;
	case NST_STRING:
	case NST_STRING_S:
		if (var->_.str)
		{
			if ((strlen(var->_.str)+2) > max_vlen)
				snprintf(value, max_vlen, "\"%*.*s...\"", max_vlen-5, max_vlen-5, var->_.str);
			else
				snprintf(value, max_vlen, "\"%s\"", var->_.str);
		}
		else
			strncpy(value, "null", max_vlen);
		break;
	case NST_MAP:		strncpy(value, "<map>", max_vlen); break;
	case NST_WIDEVNUM:	snprintf(value, max_vlen, "(%ld,%ld)", (var->_.wnum.pArea?var->_.wnum.pArea->uid:0),var->_.wnum.vnum); break;
	case NST_AREA:		snprintf(value, max_vlen, "%ld", (var->_.area?var->_.area->uid:0)); break;
	// case NST_INSTANCE:
	// case NST_DUNGEON:
	case NST_MOBILE:	snprintf(value, max_vlen, "%s", (var->_.mobile?var->_.mobile->name:"???")); break;
	// case NST_OBJECT:
	// case NST_QUEST:
	case NST_ROOM:		snprintf(value, max_vlen, "%ld", (var->_.room?var->_.room->vnum:0)); break;
	// case NST_SHIP:
	// case NST_TOKEN:
	case NST_FLAG:
		{
			if (var->_.stat.table)
				strncpy(value,nib_get_flag_string(var->_.stat.table,var->_.stat.number),max_vlen);
			else
				snprintf(value, max_vlen, "%ld", var->_.stat.number);

			break;
		}
	
	case NST_STAT:
		{
			if (var->_.stat.table)
				strncpy(value,nib_get_stat_string(var->_.stat.table,var->_.stat.number),max_vlen);
			else
				snprintf(value, max_vlen, "%ld", var->_.stat.number);

			break;
		}
	
	case NST_LIST:
	case NST_LIST_S:
		strncpy(value, "<list>", max_vlen);
		break;
	default:			strncpy(value, "<?""?""?>", max_vlen); break;
	}

	int val_len = strlen(value);
	while ((linej + val_len) < max_len)
	{
		line[linej++] = ' ';
	}

	linej = max_len - val_len - 1;
	line[linej++] = ' ';
	strncpy(line + linej, value, val_len);
	line[max_len] = '\0';
}

void nib_step_execute_show(NIB_SCRIPT_RUNTIME *nsr, int rows, int cols)
{
	char bar[SIDE_PANEL_WIDTH+1];

	for(int i = 0; i < SIDE_PANEL_WIDTH; i++)
		bar[i] = '-';
	bar[SIDE_PANEL_WIDTH] = 0;

	ITERATOR it;
	int span = rows - 6;
	int start = __find_first_line(nsr, span);

	CLS;	// Clear screen
	SETPOS(1,1); printf("[ PC: %08X  SP: %08X RETURN: %-5d ]", nsr->pc, nsr->sp, nsr->last_return);
	SETPOS(2,1); printf("%-50.50s", nsr->debug);

	SETPOS(3,1); printf( "[DISASSEMBLY]");
	SETPOS(4,1); printf("+----------------------------------------------------+");
	SETPOS(5+span,1); printf("+----------------------------------------------------+");

	// Show disassembly
	NIB_DISASSEMBLED_LINE *ndl;
	int row = 0;
	iterator_start_nth(&it, nsr->disassembly, start);
	while((row < span) && (ndl = (NIB_DISASSEMBLED_LINE *)iterator_nextdata(&it)))
	{
		SETPOS(row + 5, 1);
		printf("| ");
		if (ndl->address == nsr->pc && !ndl->is_comment)
		{
			INVCLR;
			printf("%08X: %-40.40s", ndl->address, ndl->str);
			RSTCLR;
		}
		else if (ndl->is_comment)
		{
			printf("%-50.50s", ndl->str);
		}
		else
		{
			printf("%08X: %-40.40s", ndl->address, ndl->str);
		}
		printf(" |");
		row++;
	}
	iterator_stop(&it);
	for(;row < span; row++)
	{
		SETPOS(row+5, 1);
		printf("| %-50.50s |", "");
	}

	// Show stack

	SETPOS(1,55); printf(" [STACK]");
	SETPOS(2,55); printf("+-%s-+", bar);
	SETPOS(3+MAX_STACK_SHOW,55); printf("+-%s-+", bar);
	row = MAX_STACK_SHOW + 2;

	if (nsr->sp > 0)
	{
		int start_sp = (nsr->sp > MAX_STACK_SHOW) ? nsr->sp - MAX_STACK_SHOW : 0;

		for(int i = 0; i < MAX_STACK_SHOW; i++)
		{
			int sp = start_sp + i;

			SETPOS(row - i, 55);
			if (sp < nsr->sp)
			{
				char line[SIDE_PANEL_WIDTH+1];
				int linej;

				linej = snprintf(line, sizeof(line) - 1, "[%2d] ", sp);

				// Show value on the stack
				linej += __display_stack_item(&nsr->stack[sp], line + linej, sizeof(line) - linej - 1, nsr->script);

				line[linej] = 0;

				printf("| %-*.*s |", SIDE_PANEL_WIDTH, SIDE_PANEL_WIDTH, line);
			}
			else
			{
				printf("| %-*.*s |", SIDE_PANEL_WIDTH, SIDE_PANEL_WIDTH, "");
			}
			
		}
	}
	else
	{
		for(int i = 0; i < MAX_STACK_SHOW; i++)
		{
			SETPOS(3 + i, 55); printf("| %-*.*s |", SIDE_PANEL_WIDTH, SIDE_PANEL_WIDTH, "");
		}
		SETPOS(2+(MAX_STACK_SHOW/2),57 + (SIDE_PANEL_WIDTH - 11)/2); printf("-- EMPTY --");
	}

	// Show locals
	const int localt = 4+MAX_STACK_SHOW;
	const int localb = rows - 1;
	const int localn = localb - (localt + 1);
	SETPOS(localt,55); printf(" [LOCALS]");
	SETPOS(localt+1,55); printf("+-%s-+", bar);
	for(int i = 0; i < localn; i++)
	{
		SETPOS(localt+2+i,55);
		char line[SIDE_PANEL_WIDTH+1];
		if (i < nsr->n_locals)
		{
			__display_local_var(&nsr->locals[i], line, sizeof(line) - 1, nsr->script);
		}
		else
		{
			line[0] = '\0';
		}
		printf("| %-*.*s |", SIDE_PANEL_WIDTH, SIDE_PANEL_WIDTH, line);
	}

	SETPOS(localb,55); printf("+-%s-+", bar);

	SETPOS(rows,1); printf("Press Q to quit or ENTER to step.");
}

int nib_get_last_return(NIB_SCRIPT_RUNTIME *nsr)
{
	return nsr->last_return;
}

const char *nib_get_debug(NIB_SCRIPT_RUNTIME *nsr)
{
	return nsr->debug;
}
