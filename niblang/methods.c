#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "../merc.h"
#include "../wilds.h"
#include "niblang.h"

#define YYSTYPE NIBMETHODSTYPE
#define YYLTYPE NIBMETHODLTYPE
#include "yacc/method_parser.h"
#include "yacc/method_lexer.h"

NIB_SCRIPT_STACK_TYPE convert_to_stype(NIB_TYPE *type, bool constant);

// Create function pointers for method names
LLIST *nib_functions = NULL;			// Context-less methods (aka functions)
LLIST *nib_methods_int = NULL;
LLIST *nib_methods_float = NULL;
LLIST *nib_methods_boolean = NULL;
LLIST *nib_methods_char = NULL;
LLIST *nib_methods_string = NULL;
LLIST *nib_methods_map = NULL;
LLIST *nib_methods_widevnum = NULL;
LLIST *nib_methods_list = NULL;
LLIST *nib_methods_array = NULL;
LLIST *nib_methods_flag = NULL;
LLIST *nib_methods_stat = NULL;
LLIST *nib_methods_account = NULL;
LLIST *nib_methods_affect = NULL;
LLIST *nib_methods_area = NULL;
LLIST *nib_methods_channel = NULL;
LLIST *nib_methods_class = NULL;
LLIST *nib_methods_dungeon = NULL;
LLIST *nib_methods_exit = NULL;
LLIST *nib_methods_instance = NULL;
LLIST *nib_methods_liquid = NULL;
LLIST *nib_methods_mail = NULL;
LLIST *nib_methods_material = NULL;
LLIST *nib_methods_mission = NULL;
LLIST *nib_methods_mobile = NULL;
LLIST *nib_methods_note = NULL;
LLIST *nib_methods_object = NULL;
LLIST *nib_methods_org = NULL;
LLIST *nib_methods_quest = NULL;
LLIST *nib_methods_race = NULL;
LLIST *nib_methods_rank = NULL;
LLIST *nib_methods_reputation = NULL;
LLIST *nib_methods_room = NULL;
LLIST *nib_methods_ship = NULL;
LLIST *nib_methods_skill = NULL;
LLIST *nib_methods_token = NULL;
LLIST *nib_methods_wilds = NULL;
LLIST *nib_methods_world = NULL;

LLIST *nib_fields_int = NULL;
LLIST *nib_fields_float = NULL;
LLIST *nib_fields_boolean = NULL;
LLIST *nib_fields_char = NULL;
LLIST *nib_fields_string = NULL;
LLIST *nib_fields_map = NULL;
LLIST *nib_fields_widevnum = NULL;
LLIST *nib_fields_account = NULL;
LLIST *nib_fields_affect = NULL;
LLIST *nib_fields_area = NULL;
LLIST *nib_fields_channel = NULL;
LLIST *nib_fields_class = NULL;
LLIST *nib_fields_dungeon = NULL;
LLIST *nib_fields_exit = NULL;
LLIST *nib_fields_instance = NULL;
LLIST *nib_fields_liquid = NULL;
LLIST *nib_fields_mail = NULL;
LLIST *nib_fields_material = NULL;
LLIST *nib_fields_mission = NULL;
LLIST *nib_fields_mobile = NULL;
LLIST *nib_fields_note = NULL;
LLIST *nib_fields_object = NULL;
LLIST *nib_fields_org = NULL;
LLIST *nib_fields_quest = NULL;
LLIST *nib_fields_race = NULL;
LLIST *nib_fields_rank = NULL;
LLIST *nib_fields_reputation = NULL;
LLIST *nib_fields_room = NULL;
LLIST *nib_fields_ship = NULL;
LLIST *nib_fields_skill = NULL;
LLIST *nib_fields_token = NULL;
LLIST *nib_fields_wilds = NULL;
LLIST *nib_fields_world = NULL;
LLIST *nib_fields_list = NULL;
LLIST *nib_fields_array = NULL;
LLIST *nib_fields_flag = NULL;
LLIST *nib_fields_stat = NULL;

static WNUM __static_wnum;
static ACCOUNT_DATA __static_account;
static AFFECT_DATA __static_affect;
static AREA_DATA __static_area;
static CLASS_DATA __static_class;
// static CHANNEL_DATA __static_channel;		// Add when CHANNEL update is done
static DUNGEON __static_dungeon;
static EXIT_DATA __static_exit;
static INSTANCE __static_instance;
static LIQUID __static_liquid;
static MAIL_DATA __static_mail;
static MATERIAL __static_material;
static MISSION_DATA __static_mission;
static CHAR_DATA __static_mobile;
static NOTE_DATA __static_note;
static OBJ_DATA __static_object;
static RACE_DATA __static_race;
static ROOM_INDEX_DATA __static_room;
static SHIP_DATA __static_ship;
static SKILL_DATA __static_skill;
static TOKEN_DATA __static_token;
static WILDS_DATA __static_wilds;
// static WORLD_DATA __static_world;		// Add when WORLDS are done

#define ADDR(x)				((void *)&(x))
#define GET_OFFSET(v,f)		(size_t)(ADDR((v).f) - ADDR(v))

struct nib_field_offset_type
{
	NIB_TYPE_CLASS type_class;
	NIB_PRIMARY_TYPE primary;		// NT_UNKNOWN for non-primary
	char *field;
	size_t offset;
	size_t size;
	bool lvalue;			// Whether this can be an lvalue
};

#define STRIFY(v)	#v

#define NFO(c,p,f,v,s) \
	{ NTC_##c, NT_##p, STRIFY(p ## _ ## f), GET_OFFSET(v,f), sizeof(s), true }

#define NFOS(c,p,f,v,s) \
	{ NTC_##c, NT_##p, STRIFY(p ## _ ## f), GET_OFFSET(v,f), (s), true }

#define NFOR(c,p,f,v,s) \
	{ NTC_##c, NT_##p, STRIFY(p ## _ ## f), GET_OFFSET(v,f), sizeof(s), false }

#define NFORS(c,p,f,v,s) \
	{ NTC_##c, NT_##p, STRIFY(p ## _ ## f), GET_OFFSET(v,f), (s), false }

#define NFOEND		{ NTC_VOID, NT_UNKNOWN, NULL, 0 }

static struct nib_field_offset_type __field_offsets[] =
{
	NFO(PRIMARY,AREA,uid,__static_area,long),
	NFO(PRIMARY,AREA,name,__static_area,char *),
	NFO(PRIMARY,AREA,description,__static_area,char *),
	NFO(PRIMARY,AREA,area_flags,__static_area,long),
	NFO(PRIMARY,AREA,room_list,__static_area,LLIST *),
	NFO(PRIMARY,EXIT,u1.to_room,__static_exit,ROOM_INDEX_DATA *),
	NFO(PRIMARY,EXIT,from_room,__static_exit,ROOM_INDEX_DATA *),
	NFO(PRIMARY,EXIT,exit_info,__static_exit,long),
	NFO(PRIMARY,MOBILE,name,__static_mobile,char *),
	NFO(PRIMARY,MOBILE,short_descr,__static_mobile,char *),
	NFO(PRIMARY,MOBILE,long_descr,__static_mobile,char *),
	NFO(PRIMARY,MOBILE,description,__static_mobile,char *),
	NFOR(PRIMARY,MOBILE,lcarrying,__static_mobile,LLIST *),
	NFOR(PRIMARY,MOBILE,lworn,__static_mobile,LLIST *),
	NFO(PRIMARY,OBJECT,name,__static_object,char *),
	NFO(PRIMARY,OBJECT,short_descr,__static_object,char *),
	NFO(PRIMARY,OBJECT,description,__static_object,char *),
	NFO(PRIMARY,OBJECT,full_description,__static_object,char *),
	NFOR(PRIMARY,OBJECT,carried_by,__static_object,CHAR_DATA *),
	NFOR(PRIMARY,OBJECT,pulled_by,__static_object,CHAR_DATA *),
	NFO(PRIMARY,OBJECT,num_enchanted,__static_object,int),
	NFOR(PRIMARY,OBJECT,item_type,__static_object,int),
	NFOS(PRIMARY,OBJECT,extra,__static_object,sizeof(long) * 4),
	NFO(PRIMARY,OBJECT,level,__static_object,int),
	NFO(PRIMARY,OBJECT,condition,__static_object,int),
	NFOR(PRIMARY,ROOM,vnum,__static_room,long),
	NFO(PRIMARY,ROOM,name,__static_room,char *),
	NFO(PRIMARY,ROOM,description,__static_room,char *),
	NFORS(PRIMARY,ROOM,exit,__static_room,sizeof(EXIT_DATA *) * 10),
	NFO(PRIMARY,ROOM,exit[DIR_NORTH],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_NORTHEAST],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_EAST],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_SOUTHEAST],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_SOUTH],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_SOUTHWEST],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_WEST],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_NORTHWEST],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_UP],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_DOWN],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,lpeople,__static_room,LLIST *),
	NFO(PRIMARY,WIDEVNUM,pArea,__static_wnum,AREA_DATA *),
	NFO(PRIMARY,WIDEVNUM,vnum,__static_wnum,long),
	NFOEND
};

bool nib_field_offset_lookup(NIB_TYPE *context, char *name, size_t *offset, size_t *size, bool *lvalue)
{
	if (!context) return NULL;	// Indicates error or unknown

	NIB_TYPE_CLASS c = context->type_class;
	NIB_PRIMARY_TYPE p = (c == NTC_PRIMARY) ? context->_.primary : NT_UNKNOWN;

	for(int i = 0; __field_offsets[i].field; i++)
	{
		if (__field_offsets[i].type_class == c &&
			__field_offsets[i].primary == p &&
			!str_cmp(__field_offsets[i].field, name))
		{
			*offset = __field_offsets[i].offset;
			*size = __field_offsets[i].size;
			*lvalue = __field_offsets[i].lvalue;
			return true;
		}
	}

	return false;
}

NIB_FIELD *new_nib_field(char *name, NIB_TYPE *type, bool readonly, bool lvalue, size_t offset, METHOD_FUNC *method)
{
	NIB_FIELD *field = calloc(1, sizeof(NIB_FIELD));

	field->name = strdup(name);
	field->type = nib_type_copy(type);
	// fprintf(stderr, "new_nib_field(%s,%s)\n", field->name, nib_get_typename(NULL,type));
	field->stype = convert_to_stype(type, false);
	if (type && type->type_class == NTC_LIST)
		field->stype2 = convert_to_stype(type->_.list.type, false);
	else if (type && type->type_class == NTC_ARRAY)
		field->stype2 = convert_to_stype(type->_.array.type, false);
	else
		field->stype2 = NST_UNKNOWN;
	field->readonly = readonly;
	field->lvalue = lvalue;
	field->offset = offset;
	field->method = method;

	return field;
}

void free_nib_field(NIB_FIELD *field)
{
	if (field)
	{
		if (field->name) free(field->name);
		free_nib_type(field->type);

		free(field);
	}
}

bool nib_field_valid_context(NIB_TYPE *context)
{
	if (context)
	{
		if (context->type_class == NTC_PRIMARY)
		{
			switch(context->_.primary)
			{
				case NT_WIDEVNUM:	return true;

				case NT_ACCOUNT:	return true;
				case NT_AFFECT:		return true;
				case NT_AREA:		return true;
				case NT_CHANNEL:	return true;
				case NT_CLASS:		return true;
				case NT_DUNGEON:	return true;
				case NT_EXIT:		return true;
				case NT_INSTANCE:	return true;
				case NT_LIQUID:		return true;
				case NT_MAIL:		return true;
				case NT_MATERIAL:	return true;
				case NT_MISSION:	return true;
				case NT_MOBILE:		return true;
				case NT_NOTE:		return true;
				case NT_OBJECT:		return true;
				case NT_ORG:		return true;
				case NT_QUEST:		return true;
				case NT_RACE:		return true;
				case NT_RANK:		return true;
				case NT_REPUTATION:	return true;
				case NT_ROOM:		return true;
				case NT_SHIP:		return true;
				case NT_SKILL:		return true;
				case NT_TOKEN:		return true;
				case NT_WILDS:		return true;
				case NT_WORLD:		return true;
			}
		}
	}

	return false;
}

static LLIST *__get_field_context_nst(NIB_SCRIPT_STACK_TYPE context)
{
	switch(context)
	{
		case NST_NUMBER:	return nib_fields_int;
		case NST_FLOAT:		return nib_fields_float;
		case NST_BOOLEAN:	return nib_fields_boolean;
		case NST_CHAR:		return nib_fields_char;
		case NST_STRING:	return nib_fields_string;
		case NST_MAP:		return nib_fields_map;
		case NST_WIDEVNUM:	return nib_fields_widevnum;
		case NST_ACCOUNT:	return nib_fields_account;
		case NST_AFFECT:	return nib_fields_affect;
		case NST_AREA:		return nib_fields_area;
		case NST_CHANNEL:	return nib_fields_channel;
		case NST_CLASS:		return nib_fields_class;
		case NST_DUNGEON:	return nib_fields_dungeon;
		case NST_EXIT:		return nib_fields_exit;
		case NST_INSTANCE:	return nib_fields_instance;
		case NST_LIQUID:	return nib_fields_liquid;
		case NST_MAIL:		return nib_fields_mail;
		case NST_MATERIAL:	return nib_fields_material;
		case NST_MISSION:	return nib_fields_mission;
		case NST_MOBILE:	return nib_fields_mobile;
		case NST_NOTE:		return nib_fields_note;
		case NST_OBJECT:	return nib_fields_object;
		case NST_ORG:		return nib_fields_org;
		case NST_QUEST:		return nib_fields_quest;
		case NST_RACE:		return nib_fields_race;
		case NST_RANK:		return nib_fields_rank;
		case NST_REPUTATION:return nib_fields_reputation;
		case NST_ROOM:		return nib_fields_room;
		case NST_SHIP:		return nib_fields_ship;
		case NST_SKILL:		return nib_fields_skill;
		case NST_TOKEN:		return nib_fields_token;
		case NST_WILDS:		return nib_fields_wilds;
		case NST_WORLD:		return nib_fields_world;
		case NST_FLAG:		return nib_fields_flag;
		case NST_STAT:		return nib_fields_stat;
		case NST_LIST:		return nib_fields_list;
		case NST_LIST_S:	return nib_fields_list;
		case NST_ARRAY:		return nib_fields_array;
		case NST_ARRAY_S:	return nib_fields_array;
	}

	return NULL;
}

static LLIST *__get_field_context(NIB_TYPE *context)
{
	if (!context) return NULL;

	if (context->type_class == NTC_PRIMARY)
	{
		switch(context->_.primary)
		{
			case NT_NUMBER:		return nib_fields_int;
			case NT_FLOAT:		return nib_fields_float;
			case NT_BOOLEAN:	return nib_fields_boolean;
			case NT_CHAR:		return nib_fields_char;
			case NT_STRING:		return nib_fields_string;
			case NT_MAP:		return nib_fields_map;
			case NT_WIDEVNUM:	return nib_fields_widevnum;

			case NT_ACCOUNT:	return nib_fields_account;
			case NT_AFFECT:		return nib_fields_affect;
			case NT_AREA:		return nib_fields_area;
			case NT_CHANNEL:	return nib_fields_channel;
			case NT_CLASS:		return nib_fields_class;
			case NT_DUNGEON:	return nib_fields_dungeon;
			case NT_EXIT:		return nib_fields_exit;
			case NT_INSTANCE:	return nib_fields_instance;
			case NT_LIQUID:		return nib_fields_liquid;
			case NT_MAIL:		return nib_fields_mail;
			case NT_MATERIAL:	return nib_fields_material;
			case NT_MISSION:	return nib_fields_mission;
			case NT_MOBILE:		return nib_fields_mobile;
			case NT_NOTE:		return nib_fields_note;
			case NT_OBJECT:		return nib_fields_object;
			case NT_ORG:		return nib_fields_org;
			case NT_QUEST:		return nib_fields_quest;
			case NT_RACE:		return nib_fields_race;
			case NT_RANK:		return nib_fields_rank;
			case NT_REPUTATION:	return nib_fields_reputation;
			case NT_ROOM:		return nib_fields_room;
			case NT_SHIP:		return nib_fields_ship;
			case NT_SKILL:		return nib_fields_skill;
			case NT_TOKEN:		return nib_fields_token;
			case NT_WILDS:		return nib_fields_wilds;
			case NT_WORLD:		return nib_fields_world;
		}
	}
	else if (context->type_class == NTC_LIST)
		return nib_fields_list;
	else if (context->type_class == NTC_ARRAY)
		return nib_fields_array;
	else if (context->type_class == NTC_FLAG)
		return nib_fields_flag;
	else if (context->type_class == NTC_STAT)
		return nib_fields_stat;

	return NULL;
}

NIB_FIELD *nib_field_get(NIB_TYPE *context, char *name)
{
	// Determine the context
	LLIST *fields = __get_field_context(context);
	if (!fields) return NULL;

	ITERATOR it;
	NIB_FIELD *field;
	iterator_start(&it, fields);
	while((field = (NIB_FIELD *)iterator_nextdata(&it)))
	{
		if (!str_cmp(field->name, name))
			break;
	}
	iterator_stop(&it);

	return field;
}

NIB_FIELD *nib_field_get_byid(NIB_SCRIPT_STACK_TYPE context, int id)
{
	// Determine the context
	LLIST *fields = __get_field_context_nst(context);
	if (!fields) return NULL;

	ITERATOR it;
	NIB_FIELD *field;
	iterator_start(&it, fields);
	while((field = (NIB_FIELD *)iterator_nextdata(&it)))
	{
		if (field->id == id)
			break;
	}
	iterator_stop(&it);

	return field;
}

bool nib_field_add(NIB_TYPE *context, char *name, NIB_TYPE *ret, bool readonly, bool lvalue, size_t offset, size_t size, METHOD_FUNC *method)
{
	// Assume the field does not exist

	// Determine the context
	LLIST *fields = __get_field_context(context);
	if (!fields) return false;

	// Create field
	NIB_FIELD *field = new_nib_field(name, ret, readonly, lvalue, offset, method);
	if (!field) return false;

	list_appendlink(fields, field);
	field->id = list_size(fields);
	return true;
}

#include "funcs.h"

#define MFEL(f)	{ #f, nib_method_func_##f, true }
#define MFER(f)	{ #f, nib_method_func_##f, false }
#define MFEND	{ NULL, NULL, false }

const struct nib_method_func_type nib_method_funcs[] =
{
	MFER(area_get_room),
	MFER(array_length),
	MFER(exit_get_direction),
	MFER(exit_get_door),
	MFER(exit_get_mate),
	MFEL(exit_get_north),
	MFEL(exit_get_northeast),
	MFEL(exit_get_east),
	MFEL(exit_get_southeast),
	MFEL(exit_get_south),
	MFEL(exit_get_southwest),
	MFEL(exit_get_west),
	MFEL(exit_get_northwest),
	MFEL(exit_get_up),
	MFEL(exit_get_down),
	MFER(exit_is_oneway),
	MFER(exit_is_twoway),
	MFER(function_print_msg),
	MFER(function_random_percent),
	MFER(function_reckoning),
	MFER(list_add),
	MFER(list_insert),
	MFER(list_remove),
	MFER(list_size),
	MFER(mobile_get_widevnum),
	MFER(number_random_value),
	MFER(room_get_exits),
	MFER(string_length),
	MFEND
};

bool nib_method_func_lookup(const char *name, METHOD_FUNC **func, bool *lvalue)
{
	for(int i = 0; nib_method_funcs[i].name; i++)
		if (!str_cmp(nib_method_funcs[i].name, name))
		{
			*func = nib_method_funcs[i].func;
			*lvalue = nib_method_funcs[i].lvalue;
			return true;
		}

	return false;
}


NIB_METHOD *new_nib_method(char *name, NIB_TYPE *ret, bool constant, bool lvalue, LLIST *params, char *method_name, METHOD_FUNC *method_func)
{
	NIB_METHOD *method = calloc(1, sizeof(NIB_METHOD));

	method->name = strdup(name);
	method->constant = constant;
	method->lvalue = lvalue;
	method->result = nib_type_copy(ret);
	method->sresult = convert_to_stype(ret, false);
	if (ret && ret->type_class == NTC_LIST)
		method->sresult2 = convert_to_stype(ret->_.list.type, false);
	else if (ret && ret->type_class == NTC_ARRAY)
		method->sresult2 = convert_to_stype(ret->_.array.type, false);
		// Length is on the actual result type
	else
		method->sresult2 = NST_UNKNOWN;
	method->nparams = list_size(params);
	if (method->nparams > 0)
	{
		method->params = (NIB_TYPE **)calloc(method->nparams, sizeof(NIB_TYPE *));
		ITERATOR it;
		NIB_TYPE *ptype;
		int index = 0;
		iterator_start(&it, params);
		while((ptype = (NIB_TYPE *)iterator_nextdata(&it)))
		{
			method->params[index++] = nib_type_copy(ptype);
		}
		iterator_stop(&it);
	}
	// else method->params = NULL; // already null by calloc

	if (method_name) method->method_name = strdup(method_name);
	method->method = method_func;

	return method;
}

void free_nib_method(NIB_METHOD *method)
{
	if (method)
	{
		free(method->name);
		free_nib_type(method->result);

		if (method->params && method->nparams > 0)
		{
			for(int i = method->nparams; i-- > 0;)
				free_nib_type(method->params[i]);

			free(method->params);
		}

		if (method->method_name)
			free(method->method_name);

		free(method);
	}
}

bool nib_method_valid_context(NIB_TYPE *context)
{
	if (context)
	{
		if (context->type_class == NTC_PRIMARY)
		{
			switch(context->_.primary)
			{
				case NT_NUMBER:		return true;
				case NT_FLOAT:		return true;
				case NT_BOOLEAN:	return true;
				case NT_CHAR:		return true;
				case NT_STRING:		return true;
				case NT_MAP:		return true;
				case NT_WIDEVNUM:	return true;

				case NT_ACCOUNT:	return true;
				case NT_AFFECT:		return true;
				case NT_AREA:		return true;
				case NT_CHANNEL:	return true;
				case NT_CLASS:		return true;
				case NT_DUNGEON:	return true;
				case NT_EXIT:		return true;
				case NT_INSTANCE:	return true;
				case NT_LIQUID:		return true;
				case NT_MAIL:		return true;
				case NT_MATERIAL:	return true;
				case NT_MISSION:	return true;
				case NT_MOBILE:		return true;
				case NT_NOTE:		return true;
				case NT_OBJECT:		return true;
				case NT_ORG:		return true;
				case NT_QUEST:		return true;
				case NT_RACE:		return true;
				case NT_RANK:		return true;
				case NT_REPUTATION:	return true;
				case NT_ROOM:		return true;
				case NT_SHIP:		return true;
				case NT_SKILL:		return true;
				case NT_TOKEN:		return true;
				case NT_WILDS:		return true;
				case NT_WORLD:		return true;
			}
		}
		else if (context->type_class == NTC_LIST)
			return true;
		else if (context->type_class == NTC_ARRAY)
			return true;
		else if (context->type_class == NTC_FLAG)
			return true;
		else if (context->type_class == NTC_STAT)
			return true;
	}

	return false;
}

static LLIST *__get_method_context(NIB_TYPE *context)
{
	if (!context) return nib_functions;

	if (context->type_class == NTC_PRIMARY)
	{
		switch(context->_.primary)
		{
			case NT_NUMBER:		return nib_methods_int;
			case NT_FLOAT:		return nib_methods_float;
			case NT_BOOLEAN:	return nib_methods_boolean;
			case NT_CHAR:		return nib_methods_char;
			case NT_STRING:		return nib_methods_string;
			case NT_MAP:		return nib_methods_map;
			case NT_WIDEVNUM:	return nib_methods_widevnum;

			case NT_ACCOUNT:	return nib_methods_account;
			case NT_AFFECT:		return nib_methods_affect;
			case NT_AREA:		return nib_methods_area;
			case NT_CHANNEL:	return nib_methods_channel;
			case NT_CLASS:		return nib_methods_class;
			case NT_DUNGEON:	return nib_methods_dungeon;
			case NT_EXIT:		return nib_methods_exit;
			case NT_INSTANCE:	return nib_methods_instance;
			case NT_LIQUID:		return nib_methods_liquid;
			case NT_MAIL:		return nib_methods_mail;
			case NT_MATERIAL:	return nib_methods_material;
			case NT_MISSION:	return nib_methods_mission;
			case NT_MOBILE:		return nib_methods_mobile;
			case NT_NOTE:		return nib_methods_note;
			case NT_OBJECT:		return nib_methods_object;
			case NT_ORG:		return nib_methods_org;
			case NT_QUEST:		return nib_methods_quest;
			case NT_RACE:		return nib_methods_race;
			case NT_RANK:		return nib_methods_rank;
			case NT_REPUTATION:	return nib_methods_reputation;
			case NT_ROOM:		return nib_methods_room;
			case NT_SHIP:		return nib_methods_ship;
			case NT_SKILL:		return nib_methods_skill;
			case NT_TOKEN:		return nib_methods_token;
			case NT_WILDS:		return nib_methods_wilds;
			case NT_WORLD:		return nib_methods_world;

		}
	}
	else if (context->type_class == NTC_LIST)
		return nib_methods_list;
	else if (context->type_class == NTC_ARRAY)
		return nib_methods_array;
	else if (context->type_class == NTC_FLAG)
		return nib_methods_flag;
	else if (context->type_class == NTC_STAT)
		return nib_methods_stat;

	return NULL;
}

static bool __method_matches_signature(NIB_METHOD *method, NIB_TYPE *anytype, char *name, LLIST *params)
{
	if (str_cmp(method->name, name)) return false;

	if (method->nparams < 1 && list_size(params) > 0) return false;

	ITERATOR it;
	NIB_TYPE *ptype;
	int index = 0;

	bool valid = true;
	iterator_start(&it, params);
	while((ptype = (NIB_TYPE *)iterator_nextdata(&it)))
	{
		if (index >= method->nparams)
		{
			// Ran out of params
			valid = false;
			break;
		}

		NIB_TYPE *atype = method->params[index];

		if (atype->type_class == NTC_VARARGS)
		{
			// Variable argument method; ignore the rest of the arguments
			break;
		}
		else if (atype->type_class == NTC_ANY)
		{
			// If it is ANY but the anytype is NULL, then skip it.
			//  Only really care if it is a list
			if (anytype == NULL)
				continue;

			atype = anytype;	// Substitute the ANY type replacement
		}
		else if (atype->type_class == NTC_MULTI)
		{
			if (!is_nib_type_in_multi(atype,ptype))
			{
				valid = false;
				break;
			}
		}
		else if (!are_nib_types_equal(atype, ptype))
		{
			valid = false;
			break;
		}
		else if (atype->_reference && !ptype->_reference)
		{
			valid = false;
			break;
		}

		++index;
	}
	iterator_stop(&it);

	return valid;
}

static LLIST *__get_method_context_nst(NIB_SCRIPT_STACK_TYPE context)
{
	switch(context)
	{
		case NST_FUNCTION:	return nib_functions;
		case NST_NUMBER:	return nib_methods_int;
		case NST_FLOAT:		return nib_methods_float;
		case NST_BOOLEAN:	return nib_methods_boolean;
		case NST_CHAR:		return nib_methods_char;
		case NST_STRING:	return nib_methods_string;
		case NST_STRING_S:	return nib_methods_string;
		case NST_MAP:		return nib_methods_map;
		case NST_WIDEVNUM:	return nib_methods_widevnum;
		case NST_ACCOUNT:	return nib_methods_account;
		case NST_AFFECT:	return nib_methods_affect;
		case NST_AREA:		return nib_methods_area;
		case NST_CHANNEL:	return nib_methods_channel;
		case NST_CLASS:		return nib_methods_class;
		case NST_DUNGEON:	return nib_methods_dungeon;
		case NST_EXIT:		return nib_methods_exit;
		case NST_INSTANCE:	return nib_methods_instance;
		case NST_LIQUID:	return nib_methods_liquid;
		case NST_MAIL:		return nib_methods_mail;
		case NST_MATERIAL:	return nib_methods_material;
		case NST_MISSION:	return nib_methods_mission;
		case NST_MOBILE:	return nib_methods_mobile;
		case NST_NOTE:		return nib_methods_note;
		case NST_OBJECT:	return nib_methods_object;
		case NST_ORG:		return nib_methods_org;
		case NST_QUEST:		return nib_methods_quest;
		case NST_RACE:		return nib_methods_race;
		case NST_RANK:		return nib_methods_rank;
		case NST_REPUTATION:return nib_methods_reputation;
		case NST_ROOM:		return nib_methods_room;
		case NST_SHIP:		return nib_methods_ship;
		case NST_SKILL:		return nib_methods_skill;
		case NST_TOKEN:		return nib_methods_token;
		case NST_WILDS:		return nib_methods_wilds;
		case NST_WORLD:		return nib_methods_world;
		case NST_FLAG:		return nib_methods_flag;
		case NST_STAT:		return nib_methods_stat;
		case NST_LIST:		return nib_methods_list;
		case NST_LIST_S:	return nib_methods_list;
		case NST_ARRAY:		return nib_methods_array;
		case NST_ARRAY_S:	return nib_methods_array;
	}

	return NULL;
}



NIB_METHOD *nib_method_get_byid(NIB_SCRIPT_STACK_TYPE context, int id)
{
	// Determine the context
	LLIST *methods = __get_method_context_nst(context);
	if (!methods) return NULL;

	ITERATOR it;
	NIB_METHOD *method;
	iterator_start(&it, methods);
	while((method = (NIB_METHOD *)iterator_nextdata(&it)))
	{
		if (method->id == id)
			break;
	}
	iterator_stop(&it);

	return method;
}

NIB_METHOD *nib_method_get(NIB_TYPE *context, char *name, LLIST *params)
{
	// Determine the context
	LLIST *methods = __get_method_context(context);
	if (!methods) return NULL;

	NIB_TYPE *subtype = NULL;
	if (context)
	{
		if (context->type_class == NTC_LIST)
			subtype = context->_.list.type;	// Get LIST element type
		else if (context->type_class == NTC_ARRAY)
			subtype = context->_.list.type;	// Get ARRAY element type
	}

	ITERATOR it;
	NIB_METHOD *method;
	iterator_start(&it, methods);
	while((method = (NIB_METHOD *)iterator_nextdata(&it)))
	{
		if (__method_matches_signature(method, subtype, name, params))
			break;
	}
	iterator_stop(&it);

	return method;
}

static bool __method_same_signature(NIB_METHOD *method, char *name, LLIST *params)
{
	if (str_cmp(method->name, name)) return false;

	if (list_size(params) != method->nparams) return false;

	ITERATOR it;
	NIB_TYPE *ptype;
	int index = 0;

	iterator_start(&it, params);
	while((ptype = (NIB_TYPE *)iterator_nextdata(&it)))
	{
		if (!are_nib_types_equal(method->params[index], ptype))
			break;

		if (method->params[index]->_reference && !ptype->_reference)
			break;

		++index;
	}
	iterator_stop(&it);

	return (ptype == NULL);
}

bool nib_method_exists(NIB_TYPE *context, char *name, LLIST *params)
{
	// Determine the context
	LLIST *methods = __get_method_context(context);
	if (!methods) return false;

	ITERATOR it;
	NIB_METHOD *method;
	iterator_start(&it, methods);
	while((method = (NIB_METHOD *)iterator_nextdata(&it)))
	{
		if (__method_same_signature(method, name, params))
			break;
	}
	iterator_stop(&it);

	return (method != NULL);
}

bool nib_method_add(NIB_TYPE *context, char *name, NIB_TYPE *ret, bool constant, bool lvalue, LLIST *params, char *method_name, METHOD_FUNC *method_func)
{
	// Assume the method signature does not exist

	// Determine the context
	LLIST *methods = __get_method_context(context);
	if (!methods) return false;

	// Create methods
	NIB_METHOD *method = new_nib_method(name, ret, constant, lvalue, params, method_name, method_func);
	if (!method) return false;

	list_appendlink(methods, method);
	method->id = list_size(methods);
	return true;
}


static bool nib_methods_load()
{
	bool valid = false;

	// Open the methods.dat file
	nibmethodin = fopen("./methods.dat", "r");

	if (nibmethodin != NULL)
	{
		// Parse file
		valid = !nibmethodparse();

		// Close file
		fclose(nibmethodin);
	}

	return valid;
}

static void __free_method(void *data)
{
	free_nib_method((NIB_METHOD *)data);
}

static void __free_field(void *data)
{
	free_nib_field((NIB_FIELD *)data);
}

static inline LLIST *__create_method_list()
{
	return list_createx(false, NULL, __free_method);
}

static inline LLIST *__create_field_list()
{
	return list_createx(false, NULL, __free_field);
}

#define __met(t) \
	nib_methods_##t = __create_method_list(); \
	if (!list_isvalid(nib_methods_##t)) return false;

#define __fld(t) \
	nib_fields_##t = __create_field_list(); \
	if (!list_isvalid(nib_fields_##t)) return false;


bool nib_methods_init()
{
	nib_functions = __create_method_list();
	if(!list_isvalid(nib_functions)) return false;

	__met(int)
	__met(float)
	__met(boolean)
	__met(char)
	__met(string)
	__met(map)
	__met(widevnum)
	__met(list)
	__met(array)
	__met(flag)
	__met(stat)
	__met(account)
	__met(affect)
	__met(area)
	__met(channel)
	__met(class)
	__met(dungeon)
	__met(exit)
	__met(instance)
	__met(liquid)
	__met(mail)
	__met(material)
	__met(mission)
	__met(mobile)
	__met(note)
	__met(object)
	__met(org)
	__met(quest)
	__met(race)
	__met(rank)
	__met(reputation)
	__met(room)
	__met(ship)
	__met(skill)
	__met(token)
	__met(wilds)
	__met(world)

	__fld(int)
	__fld(float)
	__fld(boolean)
	__fld(char)
	__fld(string)
	__fld(map)
	__fld(widevnum)
	__fld(list)
	__fld(array)
	__fld(flag)
	__fld(stat)
	__fld(account)
	__fld(affect)
	__fld(area)
	__fld(channel)
	__fld(class)
	__fld(dungeon)
	__fld(exit)
	__fld(instance)
	__fld(liquid)
	__fld(mail)
	__fld(material)
	__fld(mission)
	__fld(mobile)
	__fld(note)
	__fld(object)
	__fld(org)
	__fld(quest)
	__fld(race)
	__fld(rank)
	__fld(reputation)
	__fld(room)
	__fld(ship)
	__fld(skill)
	__fld(token)
	__fld(wilds)
	__fld(world)

	return nib_methods_load();
}

void nib_methods_cleanup()
{
	list_destroy(nib_functions);
	list_destroy(nib_methods_int);
	list_destroy(nib_methods_float);
	list_destroy(nib_methods_boolean);
	list_destroy(nib_methods_char);
	list_destroy(nib_methods_string);
	list_destroy(nib_methods_map);
	list_destroy(nib_methods_widevnum);
	list_destroy(nib_methods_list);
	list_destroy(nib_methods_array);
	list_destroy(nib_methods_flag);
	list_destroy(nib_methods_stat);
	list_destroy(nib_methods_account);
	list_destroy(nib_methods_affect);
	list_destroy(nib_methods_area);
	list_destroy(nib_methods_channel);
	list_destroy(nib_methods_class);
	list_destroy(nib_methods_dungeon);
	list_destroy(nib_methods_exit);
	list_destroy(nib_methods_instance);
	list_destroy(nib_methods_liquid);
	list_destroy(nib_methods_mail);
	list_destroy(nib_methods_material);
	list_destroy(nib_methods_mission);
	list_destroy(nib_methods_mobile);
	list_destroy(nib_methods_note);
	list_destroy(nib_methods_object);
	list_destroy(nib_methods_org);
	list_destroy(nib_methods_quest);
	list_destroy(nib_methods_race);
	list_destroy(nib_methods_rank);
	list_destroy(nib_methods_reputation);
	list_destroy(nib_methods_room);
	list_destroy(nib_methods_ship);
	list_destroy(nib_methods_skill);
	list_destroy(nib_methods_token);
	list_destroy(nib_methods_wilds);
	list_destroy(nib_methods_world);

	list_destroy(nib_fields_int);
	list_destroy(nib_fields_float);
	list_destroy(nib_fields_boolean);
	list_destroy(nib_fields_char);
	list_destroy(nib_fields_string);
	list_destroy(nib_fields_map);
	list_destroy(nib_fields_widevnum);
	list_destroy(nib_fields_array);
	list_destroy(nib_fields_list);
	list_destroy(nib_fields_flag);
	list_destroy(nib_fields_stat);
	list_destroy(nib_fields_account);
	list_destroy(nib_fields_affect);
	list_destroy(nib_fields_area);
	list_destroy(nib_fields_channel);
	list_destroy(nib_fields_class);
	list_destroy(nib_fields_dungeon);
	list_destroy(nib_fields_exit);
	list_destroy(nib_fields_instance);
	list_destroy(nib_fields_liquid);
	list_destroy(nib_fields_mail);
	list_destroy(nib_fields_material);
	list_destroy(nib_fields_mission);
	list_destroy(nib_fields_mobile);
	list_destroy(nib_fields_note);
	list_destroy(nib_fields_object);
	list_destroy(nib_fields_org);
	list_destroy(nib_fields_quest);
	list_destroy(nib_fields_race);
	list_destroy(nib_fields_rank);
	list_destroy(nib_fields_reputation);
	list_destroy(nib_fields_room);
	list_destroy(nib_fields_ship);
	list_destroy(nib_fields_skill);
	list_destroy(nib_fields_token);
	list_destroy(nib_fields_wilds);
	list_destroy(nib_fields_world);
}

void nib_method_get_prototype(NIB_METHOD *method, char *buffer, size_t max_len)
{
	int len = 0;

	len = snprintf(buffer, max_len, "%s", nib_get_typename(NULL,method->result));
	len += snprintf(buffer + len, max_len - len, " %s(", method->name);

	for(int i = 0; i < method->nparams; i++)
	{
		if (i > 0)
			len += snprintf(buffer + len, max_len - len, ",%s", nib_get_typename(NULL,method->params[i]));
		else
			len += snprintf(buffer + len, max_len - len, "%s", nib_get_typename(NULL,method->params[i]));
	}

	len += snprintf(buffer + len, max_len - len, ")");
	buffer[len] = 0;
}