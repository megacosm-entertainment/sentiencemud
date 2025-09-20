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

NIB_SCRIPT_STACK_TYPE convert_to_stype(NIB_TYPE *type);

// Create function pointers for method names
LLIST *nib_functions = NULL;			// Context-less methods (aka functions)
LLIST *nib_methods_int = NULL;
LLIST *nib_methods_float = NULL;
LLIST *nib_methods_boolean = NULL;
LLIST *nib_methods_char = NULL;
LLIST *nib_methods_string = NULL;
LLIST *nib_methods_map = NULL;
LLIST *nib_methods_widevnum = NULL;
LLIST *nib_methods_area = NULL;
LLIST *nib_methods_dungeon = NULL;
LLIST *nib_methods_instance = NULL;
LLIST *nib_methods_mobile = NULL;
LLIST *nib_methods_object = NULL;
LLIST *nib_methods_quest = NULL;
LLIST *nib_methods_room = NULL;
LLIST *nib_methods_ship = NULL;
LLIST *nib_methods_token = NULL;
LLIST *nib_methods_list = NULL;
LLIST *nib_methods_flag = NULL;
LLIST *nib_methods_stat = NULL;

LLIST *nib_fields_int = NULL;
LLIST *nib_fields_float = NULL;
LLIST *nib_fields_boolean = NULL;
LLIST *nib_fields_char = NULL;
LLIST *nib_fields_string = NULL;
LLIST *nib_fields_map = NULL;
LLIST *nib_fields_widevnum = NULL;
LLIST *nib_fields_area = NULL;
LLIST *nib_fields_dungeon = NULL;
LLIST *nib_fields_instance = NULL;
LLIST *nib_fields_mobile = NULL;
LLIST *nib_fields_object = NULL;
LLIST *nib_fields_quest = NULL;
LLIST *nib_fields_room = NULL;
LLIST *nib_fields_ship = NULL;
LLIST *nib_fields_token = NULL;
LLIST *nib_fields_list = NULL;
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
};

#define STRIFY(v)	#v

#define NFO(c,p,f,v) \
	{ NTC_##c, NT_##p, STRIFY(p ## _ ## f), GET_OFFSET(v,f) }

#define NFOEND		{ NTC_VOID, NT_UNKNOWN, NULL, 0 }

static struct nib_field_offset_type __field_offsets[] =
{
	NFO(PRIMARY,AREA,uid,__static_area),
	NFO(PRIMARY,AREA,name,__static_area),
	NFO(PRIMARY,AREA,description,__static_area),
	NFO(PRIMARY,AREA,area_flags,__static_area),
	NFO(PRIMARY,AREA,room_list,__static_area),
	NFO(PRIMARY,MOBILE,name,__static_mobile),
	NFO(PRIMARY,MOBILE,short_descr,__static_mobile),
	NFO(PRIMARY,MOBILE,long_descr,__static_mobile),
	NFO(PRIMARY,MOBILE,description,__static_mobile),
	NFO(PRIMARY,ROOM,vnum,__static_room),
	NFO(PRIMARY,ROOM,name,__static_room),
	NFO(PRIMARY,ROOM,description,__static_room),
	NFO(PRIMARY,ROOM,lpeople,__static_room),
	NFO(PRIMARY,WIDEVNUM,pArea,__static_wnum),
	NFO(PRIMARY,WIDEVNUM,vnum,__static_wnum),
	NFOEND
};

#define MFE(f)	{ #f, nib_method_func_##f }
#define MFEND	{ NULL, NULL }

size_t *nib_field_offset_lookup(NIB_TYPE *context, char *name)
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
			return &(__field_offsets[i].offset);
		}
	}

	return NULL;
}

NIB_FIELD *new_nib_field(char *name, NIB_TYPE *type, bool readonly, size_t offset, METHOD_FUNC *method)
{
	NIB_FIELD *field = calloc(1, sizeof(NIB_FIELD));

	field->name = strdup(name);
	field->type = nib_type_copy(type);
	field->stype = convert_to_stype(type);
	if (type && type->type_class == NTC_LIST)
		field->stype2 = convert_to_stype(type->_.type);
	else
		field->stype2 = NST_UNKNOWN;
	field->readonly = readonly;
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

				case NT_AREA:		return true;
				case NT_DUNGEON:	return true;
				case NT_INSTANCE:	return true;
				case NT_MOBILE:		return true;
				case NT_OBJECT:		return true;
				case NT_QUEST:		return true;
				case NT_ROOM:		return true;
				case NT_SHIP:		return true;
				case NT_TOKEN:		return true;
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
		case NST_AREA:		return nib_fields_area;
		case NST_DUNGEON:	return nib_fields_dungeon;
		case NST_INSTANCE:	return nib_fields_instance;
		case NST_MOBILE:	return nib_fields_mobile;
		case NST_OBJECT:	return nib_fields_object;
		case NST_QUEST:		return nib_fields_quest;
		case NST_ROOM:		return nib_fields_room;
		case NST_SHIP:		return nib_fields_ship;
		case NST_TOKEN:		return nib_fields_token;
		case NST_FLAG:		return nib_fields_flag;
		case NST_STAT:		return nib_fields_stat;
		case NST_LIST:		return nib_fields_list;
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

			case NT_AREA:		return nib_fields_area;
			case NT_DUNGEON:	return nib_fields_dungeon;
			case NT_INSTANCE:	return nib_fields_instance;
			case NT_MOBILE:		return nib_fields_mobile;
			case NT_OBJECT:		return nib_fields_object;
			case NT_QUEST:		return nib_fields_quest;
			case NT_ROOM:		return nib_fields_room;
			case NT_SHIP:		return nib_fields_ship;
			case NT_TOKEN:		return nib_fields_token;
		}
	}
	else if (context->type_class == NTC_LIST)
		return nib_fields_list;
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

bool nib_field_add(NIB_TYPE *context, char *name, NIB_TYPE *ret, bool readonly, size_t offset, METHOD_FUNC *method)
{
	// Assume the field does not exist

	// Determine the context
	LLIST *fields = __get_field_context(context);
	if (!fields) return false;

	// Create field
	NIB_FIELD *field = new_nib_field(name, ret, readonly, offset, method);
	if (!field) return false;

	list_appendlink(fields, field);
	field->id = list_size(fields);
	return true;
}

#include "funcs.h"

const struct nib_method_func_type nib_method_funcs[] =
{
	MFE(function_print_msg),
	MFE(function_random_percent),
	MFE(function_reckoning),
	MFE(list_add),
	MFE(list_insert),
	MFE(list_remove),
	MFE(list_size),
	MFE(mobile_get_widevnum),
	MFE(number_random_value),
	MFE(string_length),
	MFEND
};

METHOD_FUNC *nib_method_func_lookup(const char *name)
{
	for(int i = 0; nib_method_funcs[i].name; i++)
		if (!str_cmp(nib_method_funcs[i].name, name))
			return nib_method_funcs[i].func;

	return NULL;
}


NIB_METHOD *new_nib_method(char *name, NIB_TYPE *ret, LLIST *params, char *method_name, METHOD_FUNC *method_func)
{
	NIB_METHOD *method = calloc(1, sizeof(NIB_METHOD));

	method->name = strdup(name);
	method->result = nib_type_copy(ret);
	method->sresult = convert_to_stype(ret);
	if (ret && ret->type_class == NTC_LIST)
		method->sresult2 = convert_to_stype(ret->_.type);
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

				case NT_AREA:		return true;
				case NT_DUNGEON:	return true;
				case NT_INSTANCE:	return true;
				case NT_MOBILE:		return true;
				case NT_OBJECT:		return true;
				case NT_QUEST:		return true;
				case NT_ROOM:		return true;
				case NT_SHIP:		return true;
				case NT_TOKEN:		return true;
			}
		}
		else if (context->type_class == NTC_LIST)
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

			case NT_AREA:		return nib_methods_area;
			case NT_DUNGEON:	return nib_methods_dungeon;
			case NT_INSTANCE:	return nib_methods_instance;
			case NT_MOBILE:		return nib_methods_mobile;
			case NT_OBJECT:		return nib_methods_object;
			case NT_QUEST:		return nib_methods_quest;
			case NT_ROOM:		return nib_methods_room;
			case NT_SHIP:		return nib_methods_ship;
			case NT_TOKEN:		return nib_methods_token;
		}
	}
	else if (context->type_class == NTC_LIST)
		return nib_methods_list;
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

		if (atype->type_class == NTC_ANY)
		{
			// If it is ANY but the anytype is NULL, then skip it.
			//  Only really care if it is a list
			if (anytype == NULL)
				continue;

			atype = anytype;	// Substitute the ANY type replacement
		}

		if (!are_nib_types_equal(atype, ptype))
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
		case NST_MAP:		return nib_methods_map;
		case NST_WIDEVNUM:	return nib_methods_widevnum;
		case NST_AREA:		return nib_methods_area;
		case NST_DUNGEON:	return nib_methods_dungeon;
		case NST_INSTANCE:	return nib_methods_instance;
		case NST_MOBILE:	return nib_methods_mobile;
		case NST_OBJECT:	return nib_methods_object;
		case NST_QUEST:		return nib_methods_quest;
		case NST_ROOM:		return nib_methods_room;
		case NST_SHIP:		return nib_methods_ship;
		case NST_TOKEN:		return nib_methods_token;
		case NST_FLAG:		return nib_methods_flag;
		case NST_STAT:		return nib_methods_stat;
		case NST_LIST:		return nib_methods_list;
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
	if (context && context->type_class == NTC_LIST)
		subtype = context->_.type;	// Get LIST element type

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

bool nib_method_add(NIB_TYPE *context, char *name, NIB_TYPE *ret, LLIST *params, char *method_name, METHOD_FUNC *method_func)
{
	// Assume the method signature does not exist

	// Determine the context
	LLIST *methods = __get_method_context(context);
	if (!methods) return false;

	// Create methods
	NIB_METHOD *method = new_nib_method(name, ret, params, method_name, method_func);
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


bool nib_methods_init()
{
	nib_functions = __create_method_list();
	if(!list_isvalid(nib_functions)) return false;

	nib_methods_int = __create_method_list();
	if(!list_isvalid(nib_methods_int)) return false;

	nib_methods_float = __create_method_list();
	if(!list_isvalid(nib_methods_float)) return false;

	nib_methods_boolean = __create_method_list();
	if(!list_isvalid(nib_methods_boolean)) return false;

	nib_methods_char = __create_method_list();
	if(!list_isvalid(nib_methods_char)) return false;

	nib_methods_string = __create_method_list();
	if(!list_isvalid(nib_methods_string)) return false;

	nib_methods_map = __create_method_list();
	if(!list_isvalid(nib_methods_map)) return false;

	nib_methods_widevnum = __create_method_list();
	if(!list_isvalid(nib_methods_widevnum)) return false;

	nib_methods_area = __create_method_list();
	if(!list_isvalid(nib_methods_area)) return false;

	nib_methods_dungeon = __create_method_list();
	if(!list_isvalid(nib_methods_dungeon)) return false;

	nib_methods_instance = __create_method_list();
	if(!list_isvalid(nib_methods_instance)) return false;

	nib_methods_mobile = __create_method_list();
	if(!list_isvalid(nib_methods_mobile)) return false;

	nib_methods_object = __create_method_list();
	if(!list_isvalid(nib_methods_object)) return false;

	nib_methods_quest = __create_method_list();
	if(!list_isvalid(nib_methods_quest)) return false;

	nib_methods_room = __create_method_list();
	if(!list_isvalid(nib_methods_room)) return false;

	nib_methods_ship = __create_method_list();
	if(!list_isvalid(nib_methods_ship)) return false;

	nib_methods_token = __create_method_list();
	if(!list_isvalid(nib_methods_token)) return false;

	nib_methods_list = __create_method_list();
	if(!list_isvalid(nib_methods_list)) return false;

	nib_methods_flag = __create_method_list();
	if(!list_isvalid(nib_methods_flag)) return false;

	nib_methods_stat = __create_method_list();
	if(!list_isvalid(nib_methods_stat)) return false;

	nib_fields_int = __create_field_list();
	if(!list_isvalid(nib_fields_int)) return false;

	nib_fields_float = __create_field_list();
	if(!list_isvalid(nib_fields_float)) return false;

	nib_fields_boolean = __create_field_list();
	if(!list_isvalid(nib_fields_boolean)) return false;

	nib_fields_char = __create_field_list();
	if(!list_isvalid(nib_fields_char)) return false;

	nib_fields_string = __create_field_list();
	if(!list_isvalid(nib_fields_string)) return false;

	nib_fields_map = __create_field_list();
	if(!list_isvalid(nib_fields_map)) return false;

	nib_fields_widevnum = __create_field_list();
	if(!list_isvalid(nib_fields_widevnum)) return false;

	nib_fields_area = __create_field_list();
	if(!list_isvalid(nib_fields_area)) return false;

	nib_fields_dungeon = __create_field_list();
	if(!list_isvalid(nib_fields_dungeon)) return false;

	nib_fields_instance = __create_field_list();
	if(!list_isvalid(nib_fields_instance)) return false;

	nib_fields_mobile = __create_field_list();
	if(!list_isvalid(nib_fields_mobile)) return false;

	nib_fields_object = __create_field_list();
	if(!list_isvalid(nib_fields_object)) return false;

	nib_fields_quest = __create_field_list();
	if(!list_isvalid(nib_fields_quest)) return false;

	nib_fields_room = __create_field_list();
	if(!list_isvalid(nib_fields_room)) return false;

	nib_fields_ship = __create_field_list();
	if(!list_isvalid(nib_fields_ship)) return false;

	nib_fields_token = __create_field_list();
	if(!list_isvalid(nib_fields_token)) return false;

	nib_fields_list = __create_field_list();
	if(!list_isvalid(nib_fields_list)) return false;

	nib_fields_flag = __create_field_list();
	if(!list_isvalid(nib_fields_flag)) return false;

	nib_fields_stat = __create_field_list();
	if(!list_isvalid(nib_fields_stat)) return false;

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
	list_destroy(nib_methods_area);
	list_destroy(nib_methods_dungeon);
	list_destroy(nib_methods_instance);
	list_destroy(nib_methods_mobile);
	list_destroy(nib_methods_object);
	list_destroy(nib_methods_quest);
	list_destroy(nib_methods_room);
	list_destroy(nib_methods_ship);
	list_destroy(nib_methods_token);
	list_destroy(nib_methods_list);
	list_destroy(nib_methods_flag);
	list_destroy(nib_methods_stat);

	list_destroy(nib_fields_int);
	list_destroy(nib_fields_float);
	list_destroy(nib_fields_boolean);
	list_destroy(nib_fields_char);
	list_destroy(nib_fields_string);
	list_destroy(nib_fields_map);
	list_destroy(nib_fields_widevnum);
	list_destroy(nib_fields_area);
	list_destroy(nib_fields_dungeon);
	list_destroy(nib_fields_instance);
	list_destroy(nib_fields_mobile);
	list_destroy(nib_fields_object);
	list_destroy(nib_fields_quest);
	list_destroy(nib_fields_room);
	list_destroy(nib_fields_ship);
	list_destroy(nib_fields_token);
	list_destroy(nib_fields_list);
	list_destroy(nib_fields_flag);
	list_destroy(nib_fields_stat);

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