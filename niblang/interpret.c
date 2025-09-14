#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <stdint.h>
#include <ctype.h>

#include "niblang.h"
#include "script.h"
#include "interpret.h"

#define IS_NULLSTR(s)	(((s) == NULL) || ((s)[0] == '\0'))
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

				case NT_AREA:		return NST_AREA;
				case NT_DUNGEON:	return NST_DUNGEON;
				case NT_INSTANCE:	return NST_INSTANCE;
				case NT_MOBILE:		return NST_MOBILE;
				case NT_OBJECT:		return NST_OBJECT;
				case NT_QUEST:		return NST_QUEST;
				case NT_ROOM:		return NST_ROOM;
				case NT_SHIP:		return NST_SHIP;
				case NT_TOKEN:		return NST_TOKEN;
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
		case NST_AREA:		return "area";
		case NST_DUNGEON:	return "dungeon";
		case NST_INSTANCE:	return "instance";
		case NST_MOBILE:	return "mobile";
		case NST_OBJECT:	return "object";
		case NST_QUEST:		return "quest";
		case NST_ROOM:		return "room";
		case NST_SHIP:		return "ship";
		case NST_TOKEN:		return "token";
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
__push(char,CHAR,ch,char)
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
__push(AREA_DATA *,AREA,area,area)
__push(CHAR_DATA *,MOBILE,mobile,mobile)
__push(ROOM_INDEX_DATA *,ROOM,room,room)

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
	switch(lvalue->type)
	{
	case NST_NUMBER:
		return nib_push_stack_number(nsr, *(lvalue->_.number));

	case NST_FLOAT:
		return nib_push_stack_float(nsr, *(lvalue->_.d));

	case NST_BOOLEAN:
		return nib_push_stack_boolean(nsr, *(lvalue->_.b));

	case NST_CHAR:
		return nib_push_stack_char(nsr, *(lvalue->_.ch));

	case NST_STRING:
		return nib_push_stack_string(nsr, *(lvalue->_.str));

	case NST_WIDEVNUM:
		return nib_push_stack_widevnum(nsr, (lvalue->_.wnum));

	case NST_AREA:
		return nib_push_stack_area(nsr, *(lvalue->_.area));

	// case NST_DUNGEON:

	// case NST_INSTANCE:

	case NST_MOBILE:
		return nib_push_stack_mobile(nsr, *(lvalue->_.mobile));

	// case NST_OBJECT:

	// case NST_QUEST:

	case NST_ROOM:
		return nib_push_stack_room(nsr, *(lvalue->_.room));

	// case NST_SHIP:

	// case NST_TOKEN:

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
	switch(var->type)
	{
	case NST_NUMBER:
		return nib_push_stack_number(nsr, var->_.i);

	case NST_FLOAT:
		return nib_push_stack_float(nsr, var->_.f);

	case NST_BOOLEAN:
		return nib_push_stack_boolean(nsr, var->_.b);

	case NST_CHAR:
		return nib_push_stack_char(nsr, var->_.ch);

	case NST_STRING:
		return nib_push_stack_string_shared(nsr, var->_.str);

	case NST_WIDEVNUM:
		return nib_push_stack_widevnum(nsr, &var->_.wnum);

	case NST_AREA:
		return nib_push_stack_area(nsr, var->_.area);

	// case NST_DUNGEON:

	// case NST_INSTANCE:

	case NST_MOBILE:
		return nib_push_stack_mobile(nsr, var->_.mobile);

	// case NST_OBJECT:

	// case NST_QUEST:

	case NST_ROOM:
		return nib_push_stack_room(nsr, var->_.room);

	// case NST_SHIP:

	// case NST_TOKEN:

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
	if (var->constant) return false;

	NIB_SCRIPT_LVALUE lvalue;
	switch(var->type)
	{
	case NST_NUMBER:
		lvalue.type = NST_NUMBER;
		lvalue._.number = &(var->_.i);
		break;

	case NST_FLOAT:
		lvalue.type = NST_FLOAT;
		lvalue._.d = &(var->_.f);
		break;

	case NST_BOOLEAN:
		lvalue.type = NST_BOOLEAN;
		lvalue._.b = &(var->_.b);
		break;

	case NST_CHAR:
		lvalue.type = NST_CHAR;
		lvalue._.ch = &(var->_.ch);
		break;

	case NST_STRING:
		lvalue.type = NST_STRING;
		lvalue._.str = &(var->_.str);
		break;

	case NST_WIDEVNUM:
		lvalue.type = NST_WIDEVNUM;
		lvalue._.wnum = &(var->_.wnum);
		break;

	case NST_AREA:
		lvalue.type = NST_AREA;
		lvalue._.area = &(var->_.area);
		break;

	// case NST_DUNGEON:

	// case NST_INSTANCE:

	case NST_MOBILE:
		lvalue.type = NST_MOBILE;
		lvalue._.mobile = &(var->_.mobile);
		break;

	// case NST_OBJECT:

	// case NST_QUEST:

	case NST_ROOM:
		lvalue.type = NST_ROOM;
		lvalue._.room = &(var->_.room);
		break;

	// case NST_SHIP:

	// case NST_TOKEN:

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
	switch(var->type)
	{
	case VAR_NUMBER:
		return nib_push_stack_number(nsr, var->_.num);

	case VAR_FLOAT:
		return nib_push_stack_float(nsr, var->_.flt);

	case VAR_BOOLEAN:
		return nib_push_stack_boolean(nsr, var->_.b);

	case VAR_STRING:
		return nib_push_stack_string_shared(nsr, var->_.str);

	case VAR_WIDEVNUM:
		return nib_push_stack_widevnum(nsr, &var->_.wnum);

	case VAR_AREA:
		return nib_push_stack_area(nsr, var->_.area);

	case VAR_MOBILE:
		return nib_push_stack_mobile(nsr, var->_.mobile);

	case VAR_ROOM:
		return nib_push_stack_room(nsr, var->_.room);
	}

	return false;
}

bool nib_push_stack_global_var_lvalue(NIB_SCRIPT_RUNTIME *nsr, pVARIABLE var)
{
	NIB_SCRIPT_LVALUE lvalue;
	switch(var->type)
	{
	case VAR_NUMBER:
		lvalue.type = NST_NUMBER;
		lvalue._.number = &(var->_.num);
		break;

	case VAR_FLOAT:
		lvalue.type = NST_FLOAT;
		lvalue._.d = &(var->_.flt);
		break;

	case VAR_BOOLEAN:
		lvalue.type = NST_BOOLEAN;
		lvalue._.b = &(var->_.b);
		break;

	case VAR_STRING:
		lvalue.type = NST_STRING;
		lvalue._.str = &(var->_.str);
		break;

	case VAR_WIDEVNUM:
		lvalue.type = NST_WIDEVNUM;
		lvalue._.wnum = &(var->_.wnum);
		break;

	case VAR_AREA:
		lvalue.type = NST_AREA;
		lvalue._.area = &(var->_.area);
		break;

	case VAR_MOBILE:
		lvalue.type = NST_MOBILE;
		lvalue._.mobile = &(var->_.mobile);
		break;

	case VAR_ROOM:
		lvalue.type = NST_ROOM;
		lvalue._.room = &(var->_.room);
		break;

	default:
		return false;

	}

	return nib_push_stack_lvalue(nsr, &lvalue);
}

static void *__get_lvalue_field(NIB_SCRIPT_LVALUE *lvalue, NIB_FIELD *field)
{
	switch(lvalue->type)
	{
	case NST_AREA:		return (void*)*(lvalue->_.area) + field->offset;
	// case NST_DUNGEON:	return (void*)*(lvalue->_.dungeon) + field->offset;
	// case NST_INSTANCE:	return (void*)*(lvalue->_.instance) + field->offset;
	case NST_MOBILE:	return (void*)*(lvalue->_.mobile) + field->offset;
	// case NST_OBJECT:	return (void*)*(lvalue->_.object) + field->offset;
	// case NST_QUEST:		return (void*)*(lvalue->_.quest) + field->offset;
	case NST_ROOM:		return (void*)*(lvalue->_.room) + field->offset;
	// case NST_SHIP:		return (void*)*(lvalue->_.ship) + field->offset;
	// case NST_TOKEN:		return (void*)*(lvalue->_.token) + field->offset;
	case NST_WIDEVNUM:	return (void*)(lvalue->_.wnum) + field->offset;
	}

	return NULL;
}

bool nib_push_stack_field_lvalue(NIB_SCRIPT_RUNTIME *nsr, NIB_SCRIPT_LVALUE *lvalue, NIB_FIELD *field)
{
	void *ptr = __get_lvalue_field(lvalue, field);
	if (!ptr) return false;

	NIB_SCRIPT_LVALUE _lvalue;
	_lvalue.type = field->stype;
	switch(field->stype)
	{
	case NST_NUMBER:			_lvalue._.number = ptr;		break;
	case NST_FLOAT:				_lvalue._.d = ptr;			break;
	case NST_BOOLEAN:			_lvalue._.b = ptr;			break;
	case NST_CHAR:				_lvalue._.ch = ptr;			break;
	case NST_STRING:			_lvalue._.str = ptr;		break;
	case NST_WIDEVNUM:			_lvalue._.wnum = ptr;		break;

	case NST_AREA:				_lvalue._.area = ptr;		break;
	// case NST_DUNGEON:
	// case NST_INSTANCE:
	case NST_MOBILE:			_lvalue._.mobile = ptr;		break;
	// case NST_OBJECT:
	// case NST_QUEST:
	case NST_ROOM:				_lvalue._.room = ptr;		break;
	// case NST_SHIP:
	// case NST_TOKEN:

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
__peek(char,CHAR,ch,char)
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
__peek(AREA_DATA *,AREA,area,area)
__peek(CHAR_DATA *,MOBILE,mobile,mobile)
__peek(ROOM_INDEX_DATA *,ROOM,room,room)
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
__pop(char,CHAR,ch,char)
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
__pop(AREA_DATA *,AREA,area,area)
__pop(CHAR_DATA *,MOBILE,mobile,mobile)
__pop(ROOM_INDEX_DATA *,ROOM,room,room)
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

#if 0
#define __assign(t,d,f,n) \
static bool __assign_lvalue_##n (NIB_SCRIPT_RUNTIME *nsr, bool push_result) \
{ \
	/* Load VALUE */ \
	t value; \
	if (!nib_pop_stack_##n (nsr, &value)) \
		return false; \
\
	/* Load LVALUE */ \
	NIB_SCRIPT_LVALUE lvalue; \
	if (!nib_pop_stack_lvalue(nsr, &lvalue)) \
		return false; \
\
	/* Verify types actually match */ \
	if (lvalue.type != NST_##d) \
		return false; \
\
	(*(lvalue._.f)) = value; \
\
	if (push_result) \
		return nib_push_stack_##n (nsr, value); \
\
	return true; \
}

// Needs special processing to handle assigning to BOOLEANS using CONST0/1
static bool __assign_lvalue_number (NIB_SCRIPT_RUNTIME *nsr, bool push_result)
{
	/* Load VALUE */
	long value;
	if (!nib_pop_stack_number (nsr, &value))
		return false;

	/* Load LVALUE */
	NIB_SCRIPT_LVALUE lvalue;
	if (!nib_pop_stack_lvalue(nsr, &lvalue))
		return false;

	switch(lvalue.type)
	{
		case NST_NUMBER:		(*(lvalue._.number)) = value; break;
		case NST_BOOLEAN:		(*(lvalue._.number)) = (value != 0); break;

		default:
			return false;
	}

	if (push_result)
		return nib_push_stack_number (nsr, value);

	return true;
}

//__assign(long,NUMBER,number,number)
__assign(bool,BOOLEAN,b,boolean)
__assign(double,FLOAT,d,float)
__assign(char,CHAR,ch,char)
static bool __assign_lvalue_string (NIB_SCRIPT_RUNTIME *nsr, bool push_result)
{
	/* Load VALUE */
	char *value;
	if (!nib_pop_stack_string (nsr, &value))
		return false;

	/* Load LVALUE */
	NIB_SCRIPT_LVALUE lvalue;
	if (!nib_pop_stack_lvalue(nsr, &lvalue))
		return false;

	/* Verify types actually match */
	if (lvalue.type != NST_STRING)
		return false;

	// Replace the string
	if (*(lvalue._.str)) free(*(lvalue._.str));
	(*(lvalue._.str)) = strdup(value);

	if (push_result)
		return nib_push_stack_string (nsr, value);

	return true;
}
static bool __assign_lvalue_string_shared (NIB_SCRIPT_RUNTIME *nsr, bool push_result)
{
	/* Load VALUE */
	char *value;
	if (!nib_pop_stack_string_shared (nsr, &value))
		return false;

	/* Load LVALUE */
	NIB_SCRIPT_LVALUE lvalue;
	if (!nib_pop_stack_lvalue(nsr, &lvalue))
		return false;

	/* Verify types actually match */
	if (lvalue.type != NST_STRING)
		return false;

	// Replace the string
	if (*(lvalue._.str)) free(*(lvalue._.str));
	(*(lvalue._.str)) = strdup(value);

	if (push_result)
		return nib_push_stack_string_shared (nsr, value);


	return true;
}
static bool __assign_lvalue_widevnum (NIB_SCRIPT_RUNTIME *nsr, bool push_result)
{
	/* Load VALUE */
	WNUM value;
	if (!nib_pop_stack_widevnum (nsr, &value))
		return false;

	/* Load LVALUE */
	NIB_SCRIPT_LVALUE lvalue;
	if (!nib_pop_stack_lvalue(nsr, &lvalue))
		return false;

	/* Verify types actually match */
	if (lvalue.type != NST_WIDEVNUM)
		return false;

	(*(lvalue._.wnum)) = value;

	if (push_result)
		return nib_push_stack_widevnum (nsr, &value);

	return true;
}
__assign(AREA_DATA *,AREA,area,area)
__assign(CHAR_DATA *,MOBILE,mobile,mobile)
__assign(ROOM_INDEX_DATA *,ROOM,room,room)
// TODO: LIST
static bool __assign_lvalue_lvalue (NIB_SCRIPT_RUNTIME *nsr, bool push_result)
{
	/* Load RHS */
	NIB_SCRIPT_LVALUE rvalue;
	if (!nib_pop_stack_lvalue (nsr, &rvalue))
		return false;

	/* Load LHS */
	NIB_SCRIPT_LVALUE lvalue;
	if (!nib_pop_stack_lvalue(nsr, &lvalue))
		return false;

	/* Verify types actually match */
	if (lvalue.type != rvalue.type)
		return false;

	/* Verify list types actually match */
	if (lvalue.type == NTC_LIST && lvalue._.list.type != rvalue._.list.type)
		return false;

	switch(lvalue.type)
	{
	case NST_NUMBER:		*(lvalue._.number) = *(rvalue._.number); break;
	case NST_FLOAT:			*(lvalue._.d) = *(rvalue._.d); break;
	case NST_BOOLEAN:		*(lvalue._.b) = *(rvalue._.b); break;
	case NST_CHAR:			*(lvalue._.ch) = *(rvalue._.ch); break;
	case NST_STRING:
			if (*(lvalue._.str)) free(*(lvalue._.str));		// Free old string
			// Duplicate new one, or keep NULL
			*(lvalue._.str) = *(rvalue._.str) ? strdup(*(rvalue._.str)) : NULL;
			break;
	case NST_WIDEVNUM:		*(lvalue._.wnum) = *(rvalue._.wnum); break;
	case NST_AREA:			*(lvalue._.area) = *(rvalue._.area); break;
	// case NST_DUNGEON:		*(lvalue._.dungeon) = *(rvalue._.dungeon); break;
	// case NST_INSTANCE:		*(lvalue._.instance) = *(rvalue._.instance); break;
	case NST_MOBILE:		*(lvalue._.mobile) = *(rvalue._.mobile); break;
	// case NST_OBJECT:		*(lvalue._.object) = *(rvalue._.object); break;
	// case NST_QUEST:			*(lvalue._.quest) = *(rvalue._.quest); break;
	case NST_ROOM:			*(lvalue._.room) = *(rvalue._.room); break;
	// case NST_SHIP:			*(lvalue._.ship) = *(rvalue._.ship); break;
	// case NST_TOKEN:			*(lvalue._.token) = *(rvalue._.token); break;
	case NST_FLAG:			*(lvalue._.number) = *(rvalue._.number); break;
	case NST_STAT:			*(lvalue._.number) = *(rvalue._.number); break;
	case NST_LIST:
			// TODO: LIST management for script created (should be freed) and external lists (freed elsewhere)
			*(lvalue._.list.list) = *(rvalue._.list.list);
			break;
	}

	if (push_result)
		nib_push_stack_lvalue_value(nsr, &rvalue);

	return true;
}

static bool __assign_lvalue_null (NIB_SCRIPT_RUNTIME *nsr, bool push_result)
{
	/* Load LHS */
	NIB_SCRIPT_LVALUE lvalue;
	if (!nib_pop_stack_lvalue(nsr, &lvalue))
		return false;

	switch(lvalue.type)
	{
	case NST_STRING:
		if (*(lvalue._.str))	free(*(lvalue._.str));
		*(lvalue._.str) = NULL;
		break;

	case NST_AREA:			*(lvalue._.area) = NULL; break;
	// case NST_DUNGEON:		*(lvalue._.dungeon) = NULL; break;
	// case NST_INSTANCE:		*(lvalue._.instance) = NULL; break;
	case NST_MOBILE:		*(lvalue._.mobile) = NULL; break;
	// case NST_OBJECT:		*(lvalue._.object) = NULL; break;
	// case NST_QUEST:			*(lvalue._.quest) = NULL; break;
	case NST_ROOM:			*(lvalue._.room) = NULL; break;
	// case NST_SHIP:			*(lvalue._.ship) = NULL; break;
	// case NST_TOKEN:			*(lvalue._.token) = NULL; break;
	//case NST_LIST:			*(lvalue._.list.list) = NULL; break;
	}

	// If the result needs to stay, just keep the NST_NULL on the stack,
	//  Otherwise, pop it.
	if (!push_result)
		return nib_pop_stack (nsr);

	return true;
}
#endif

static bool __is_top_zero(NIB_SCRIPT_RUNTIME *nsr)
{
	switch(nib_peek_stack(nsr))
	{
		case NST_NUMBER:
		{
			long number;
			if (!nib_peek_stack_number(nsr, -1, &number))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return !number;
		}

		case NST_BOOLEAN:
		{
			bool value;
			if (!nib_peek_stack_boolean(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return !value;
		}

		case NST_FLOAT:
		{
			double value;
			if (!nib_peek_stack_float(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return value == 0.0;
		}

		case NST_CHAR:
		{
			char value;
			if (!nib_peek_stack_char(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return !value;
		}

		case NST_STRING:
		{
			char *value;
			if (!nib_peek_stack_string(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			bool ret = !value || !value[0];	// NULL or empty
			return ret;
		}

		case NST_STRING_S:
		{
			char *value;
			if (!nib_peek_stack_string_shared(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return !value || !value[0];	// NULL or empty
		}

		case NST_WIDEVNUM:
		{
			WNUM value;
			if (!nib_peek_stack_widevnum(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return !value.pArea && value.vnum < 1;
		}

		case NST_AREA:
		{
			AREA_DATA *value;
			if (!nib_peek_stack_area(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return !value;
		}

		// case NST_DUNGEON:
		// case NST_INSTANCE:

		case NST_MOBILE:
		{
			CHAR_DATA *value;
			if (!nib_peek_stack_mobile(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return !value;
		}

		// case NST_OBJECT:
		// case NST_QUEST:

		case NST_ROOM:
		{
			ROOM_INDEX_DATA *value;
			if (!nib_peek_stack_room(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return !value;
		}

		// case NST_SHIP:
		// case NST_TOKEN:

		case NST_FLAG:
		{
			long value;
			if (!nib_peek_stack_number(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
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
				nsr->last_return = SCPERR_FAILURE;
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
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return !list_isvalid(list) || list_size(list) < 1;	// invalid or empty
		}

		case NST_LVALUE:
		{
			NIB_SCRIPT_LVALUE lvalue;
			if (!nib_peek_stack_lvalue(nsr, -1, &lvalue))
			{
				nsr->last_return = SCPERR_FAILURE;
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
			case NST_WIDEVNUM:		return !lvalue._.wnum->pArea && lvalue._.wnum->vnum < 1;
			case NST_AREA:			return !*(lvalue._.area);
			// case NST_DUNGEON:
			// case NST_INSTANCE:
			case NST_MOBILE:		return !*(lvalue._.mobile);
			// case NST_OBJECT:
			// case NST_QUEST:
			case NST_ROOM:			return !*(lvalue._.room);
			// case NST_SHIP:
			// case NST_TOKEN:
			case NST_FLAG:			return !*(lvalue._.number);
			// cast NST_STAT - cannot be "zero/false"
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
		case NST_NUMBER:
		{
			long number;
			if (!nib_peek_stack_number(nsr, -1, &number))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return number != 0;
		}

		case NST_BOOLEAN:
		{
			bool value;
			if (!nib_peek_stack_boolean(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return value;
		}

		case NST_FLOAT:
		{
			double value;
			if (!nib_peek_stack_float(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return value != 0.0;
		}

		case NST_CHAR:
		{
			char value;
			if (!nib_peek_stack_char(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return value != '\0';
		}

		case NST_STRING:
		{
			char *value;
			if (!nib_peek_stack_string(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			bool ret = value && value[0];
			return ret;
		}

		case NST_STRING_S:
		{
			char *value;
			if (!nib_peek_stack_string_shared(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return value && value[0];
		}

		case NST_WIDEVNUM:
		{
			WNUM value;
			if (!nib_peek_stack_widevnum(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return value.vnum > 0;		// Area can be null
		}

		case NST_AREA:
		{
			AREA_DATA *value;
			if (!nib_peek_stack_area(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return value != NULL;
		}

		// case NST_DUNGEON:
		// case NST_INSTANCE:

		case NST_MOBILE:
		{
			CHAR_DATA *value;
			if (!nib_peek_stack_mobile(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return value != NULL;
		}

		// case NST_OBJECT:
		// case NST_QUEST:

		case NST_ROOM:
		{
			ROOM_INDEX_DATA *value;
			if (!nib_peek_stack_room(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return value != NULL;
		}

		// case NST_SHIP:
		// case NST_TOKEN:

		case NST_FLAG:
		{
			long value;
			if (!nib_peek_stack_number(nsr, -1, &value))
			{
				nsr->last_return = SCPERR_FAILURE;
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
				nsr->last_return = SCPERR_FAILURE;
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
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			return list_size(list) > 0;
		}

		case NST_LVALUE:
		{
			NIB_SCRIPT_LVALUE lvalue;
			if (!nib_peek_stack_lvalue(nsr, -1, &lvalue))
			{
				nsr->last_return = SCPERR_FAILURE;
				return true;
			}

			switch(lvalue.type)
			{
			case NST_NUMBER:		return *(lvalue._.number) != 0;
			case NST_FLOAT:			return *(lvalue._.d) != 0.0;
			case NST_BOOLEAN:		return *(lvalue._.b);
			case NST_FLAG_BIT:		return IS_SET(*(lvalue._.bit.value),lvalue._.bit.bit);
			case NST_CHAR:			return *(lvalue._.ch) != '\0';
			case NST_STRING:		return *(lvalue._.str) && (*(lvalue._.str))[0];
			case NST_WIDEVNUM:		return lvalue._.wnum->vnum > 0;
			case NST_AREA:			return *(lvalue._.area) != NULL;
			// case NST_DUNGEON:
			// case NST_INSTANCE:
			case NST_MOBILE:		return *(lvalue._.mobile) != NULL;
			// case NST_OBJECT:
			// case NST_QUEST:
			case NST_ROOM:			return *(lvalue._.room) != NULL;
			// case NST_SHIP:
			// case NST_TOKEN:
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
					int len = (lsp->_.i>0)?lsp->_.i:0;
					char *value = calloc(1,len+1);
					if (!value)
					{
						SETRET(nsr,MEMORY);
						return true;
					}
					for(int i = len; i-- > 0;)
						value[i] = rsp->_.ch;
					value[len] = '\0';

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
							int len = (lsp->_.i>0)?lsp->_.i:0;
							char *value = calloc(1,len+1);
							if (!value)
							{
								SETRET(nsr,MEMORY);
								return true;
							}
							for(int i = len; i-- > 0;)
								value[i] = *(rsp->_.lvalue._.ch);
							value[len] = '\0';

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
					int len = (rsp->_.i>0)?rsp->_.i:0;
					char *value = calloc(1,len+1);
					if (!value)
					{
						SETRET(nsr,MEMORY);
						return true;
					}
					for(int i = len; i-- > 0;)
						value[i] = lsp->_.ch;
					value[len] = '\0';

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
					char tmp[3];
					tmp[0] = lsp->_.ch;
					tmp[1] = rsp->_.ch;
					tmp[2] = 0;

					if (!nib_push_stack_string(nsr,tmp))
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
						if (rsp->_.str)
						{
							char *value = calloc(1,strlen(rsp->_.str)+2);

							value[0] = lsp->_.ch;
							strcpy(value+1,rsp->_.str);
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
							char tmp[2];
							tmp[0] = lsp->_.ch;
							tmp[1] = 0;

							if (!nib_push_stack_string(nsr,tmp))
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
						if (rsp->_.str)
						{
							char *value = calloc(1,strlen(rsp->_.str)+2);

							value[0] = lsp->_.ch;
							strcpy(value+1,rsp->_.str);

							if (!nib_push_stack_string_raw(nsr,value))
							{
								free(value);
								SETRET(nsr,STACK);
								return true;
							}
						}
						else
						{
							char tmp[2];
							tmp[0] = lsp->_.ch;
							tmp[1] = 0;

							if (!nib_push_stack_string(nsr,tmp))
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
							int cnt = *(rsp->_.lvalue._.number);
							int len = (cnt>0)?cnt:0;
							char *value = calloc(1,len+1);
							if (!value)
							{
								SETRET(nsr,MEMORY);
								return true;
							}
							for(int i = len; i-- > 0;)
								value[i] = lsp->_.ch;
							value[len] = '\0';

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

							char tmp[3];
							tmp[0] = lsp->_.ch;
							tmp[1] = *(rsp->_.lvalue._.ch);
							tmp[2] = 0;

							if (!nib_push_stack_string(nsr,tmp))
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
								if (*(rsp->_.lvalue._.str))
								{
									char *value = calloc(1,strlen(*(rsp->_.lvalue._.str))+2);

									value[0] = lsp->_.ch;
									strcpy(value+1,*(rsp->_.lvalue._.str));

									if (!nib_push_stack_string_raw(nsr,value))
									{
										free(value);
										SETRET(nsr,STACK);
										return true;
									}
								}
								else
								{
									char tmp[2];
									tmp[0] = lsp->_.ch;
									tmp[1] = 0;

									if (!nib_push_stack_string(nsr,tmp))
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
							int len = strlen(lsp->_.str);
							char *value = calloc(1,len+2);
							if (!value)
							{
								free(lsp->_.str);
								SETRET(nsr,MEMORY);
								return true;
							}
							strcpy(value,lsp->_.str);
							value[len] = rsp->_.ch;
							value[len+1] = 0;

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
						char tmp[2];
						tmp[0] = rsp->_.ch;
						tmp[1] = 0;

						if (!nib_push_stack_string(nsr, tmp))
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
			
			case NST_MOBILE:	// STRING op MOBILE => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							sprintf(stringify,"%s(%ld,%ld)",
								(rsp->_.mobile) ? (rsp->_.mobile)->name : "null",
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
			
			case NST_ROOM:		// STRING op ROOM => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
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
									int len = strlen(lsp->_.str);
									char *value = calloc(1,len+2);
									if (!value)
									{
										free(lsp->_.str);
										SETRET(nsr,MEMORY);
										return true;
									}
									strcpy(value,lsp->_.str);
									value[len] = *(rsp->_.lvalue._.ch);
									value[len+1] = 0;

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
								char tmp[2];
								tmp[0] = *(rsp->_.lvalue._.ch);
								tmp[1] = 0;

								if (!nib_push_stack_string(nsr, tmp))
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
							int len = strlen(lsp->_.str);
							char *value = calloc(1,len+2);
							if (!value)
							{
								SETRET(nsr,MEMORY);
								return true;
							}
							strcpy(value,lsp->_.str);
							value[len] = rsp->_.ch;
							value[len+1] = 0;

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
						char tmp[2];
						tmp[0] = rsp->_.ch;
						tmp[1] = 0;

						if (!nib_push_stack_string(nsr, tmp))
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
			
			case NST_MOBILE:	// STRING op MOBILE => STRING
				{
					switch(op)
					{
					case NI_ADD:		// Concatenation
						{
							char stringify[100];
							sprintf(stringify,"%s(%ld,%ld)",
								(rsp->_.mobile) ? (rsp->_.mobile)->name : "null",
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
								return true;
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
									int len = strlen(lsp->_.str);
									char *value = calloc(1,len+2);
									if (!value)
									{
										SETRET(nsr,MEMORY);
										return true;
									}
									strcpy(value,lsp->_.str);
									value[len] = *(rsp->_.lvalue._.ch);
									value[len+1] = 0;

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
								char tmp[2];
								tmp[0] = *(rsp->_.lvalue._.ch);
								tmp[1] = 0;

								if (!nib_push_stack_string(nsr, tmp))
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
							int len = (*(lsp->_.lvalue._.number)>0)?*(lsp->_.lvalue._.number):0;
							char *value = calloc(1,len+1);
							if (!value)
							{
								SETRET(nsr,MEMORY);
								return true;
							}
							for(int i = len; i-- > 0;)
								value[i] = rsp->_.ch;
							value[len] = '\0';

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
									int len = (*(lsp->_.lvalue._.number)>0)?*(lsp->_.lvalue._.number):0;
									char *value = calloc(1,len+1);
									if (!value)
									{
										SETRET(nsr,MEMORY);
										return true;
									}
									for(int i = len; i-- > 0;)
										value[i] = *(rsp->_.lvalue._.ch);
									value[len] = '\0';

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
							int len = (rsp->_.i>0)?rsp->_.i:0;
							char *value = calloc(1,len+1);
							if (!value)
							{
								SETRET(nsr,MEMORY);
								return true;
							}
							for(int i = len; i-- > 0;)
								value[i] = *(lsp->_.lvalue._.ch);
							value[len] = '\0';

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

							char tmp[3];
							tmp[0] = *(lsp->_.lvalue._.ch);
							tmp[1] = rsp->_.ch;
							tmp[2] = 0;

							if (!nib_push_stack_string(nsr,tmp))
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

							if (rsp->_.str)
							{
								char *value = calloc(1,strlen(rsp->_.str)+2);

								value[0] = *(lsp->_.lvalue._.ch);
								strcpy(value+1,rsp->_.str);
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
								char tmp[2];
								tmp[0] = *(lsp->_.lvalue._.ch);
								tmp[1] = 0;

								if (!nib_push_stack_string(nsr,tmp))
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

							if (rsp->_.str)
							{
								char *value = calloc(1,strlen(rsp->_.str)+2);

								value[0] = *(lsp->_.lvalue._.ch);
								strcpy(value+1,rsp->_.str);

								if (!nib_push_stack_string_raw(nsr,value))
								{
									free(value);
									SETRET(nsr,STACK);
									return true;
								}
							}
							else
							{
								char tmp[2];
								tmp[0] = *(lsp->_.lvalue._.ch);
								tmp[1] = 0;

								if (!nib_push_stack_string(nsr,tmp))
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

									// Cloning
									int cnt = *(rsp->_.lvalue._.number);
									int len = (cnt>0)?cnt:0;
									char *value = calloc(1,len+1);
									if (!value)
									{
										SETRET(nsr,MEMORY);
										return true;
									}
									for(int i = len; i-- > 0;)
										value[i] = *(lsp->_.lvalue._.ch);
									value[len] = '\0';

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

									char tmp[3];
									tmp[0] = *(lsp->_.lvalue._.ch);
									tmp[1] = *(rsp->_.lvalue._.ch);
									tmp[2] = 0;

									if (!nib_push_stack_string(nsr,tmp))
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

									if (*(rsp->_.lvalue._.str))
									{
										char *value = calloc(1,strlen(*(rsp->_.lvalue._.str))+2);

										value[0] = *(lsp->_.lvalue._.ch);
										strcpy(value+1,*(rsp->_.lvalue._.str));

										if (!nib_push_stack_string_raw(nsr,value))
										{
											free(value);
											SETRET(nsr,STACK);
											return true;
										}
									}
									else
									{
										char tmp[2];
										tmp[0] = *(lsp->_.lvalue._.ch);
										tmp[1] = 0;

										if (!nib_push_stack_string(nsr,tmp))
										{
											SETRET(nsr,STACK);
											return true;
										}
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
									int len = strlen(*(lsp->_.lvalue._.str));
									char *value = calloc(1,len+2);
									if (!value)
									{
										SETRET(nsr,MEMORY);
										return true;
									}
									strcpy(value,*(lsp->_.lvalue._.str));
									value[len] = rsp->_.ch;
									value[len+1] = 0;

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
								char tmp[2];
								tmp[0] = rsp->_.ch;
								tmp[1] = 0;

								if (!nib_push_stack_string(nsr, tmp))
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
					
					case NST_MOBILE:	// STRING op MOBILE => STRING
						{
							switch(op)
							{
							case NI_ADD:		// Concatenation
								{
									char stringify[100];
									sprintf(stringify,"%s(%ld,%ld)",
										(rsp->_.mobile) ? (rsp->_.mobile)->name : "null",
										(rsp->_.mobile && (rsp->_.mobile)->pIndexData) ? (rsp->_.mobile)->pIndexData->area->uid : 0,
										(rsp->_.mobile && (rsp->_.mobile)->pIndexData) ? (rsp->_.mobile)->pIndexData->vnum : 0);
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
										return true;
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
									sprintf(stringify,"%s(%ld,%ld)",
										(rsp->_.room) ? (rsp->_.room)->name : "null",
										(rsp->_.room) ? (rsp->_.room)->area->uid : 0,
										(rsp->_.room) ? (rsp->_.room)->vnum : 0);

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
											int len = strlen(*(lsp->_.lvalue._.str));
											char *value = calloc(1,len+2);
											if (!value)
											{
												SETRET(nsr,MEMORY);
												return true;
											}
											strcpy(value,*(lsp->_.lvalue._.str));
											value[len] = *(rsp->_.lvalue._.ch);
											value[len+1] = 0;

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
										char tmp[2];
										tmp[0] = *(rsp->_.lvalue._.ch);
										tmp[1] = 0;

										if (!nib_push_stack_string(nsr, tmp))
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
					case NI_EQ:			value = (str_cmp(lsp->_.str,rsp->_.str) == 0); break;
					case NI_NEQ:		value = (str_cmp(lsp->_.str,rsp->_.str) != 0); break;
					case NI_LT:			value = (str_cmp(lsp->_.str,rsp->_.str) < 0); break;
					case NI_LE:			value = (str_cmp(lsp->_.str,rsp->_.str) <= 0); break;
					case NI_GT:			value = (str_cmp(lsp->_.str,rsp->_.str) > 0); break;
					case NI_GE:			value = (str_cmp(lsp->_.str,rsp->_.str) >= 0); break;
					case NI_STR_PREFIX: value = !str_prefix(rsp->_.str,lsp->_.str); break;
					case NI_STR_INFIX:	value = !str_infix(rsp->_.str,lsp->_.str); break;
					case NI_STR_SUFFIX:	value = !str_suffix(rsp->_.str,lsp->_.str); break;
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
					case NI_EQ:			value = (str_cmp(lsp->_.str,rsp->_.str) == 0); break;
					case NI_NEQ:		value = (str_cmp(lsp->_.str,rsp->_.str) != 0); break;
					case NI_LT:			value = (str_cmp(lsp->_.str,rsp->_.str) < 0); break;
					case NI_LE:			value = (str_cmp(lsp->_.str,rsp->_.str) <= 0); break;
					case NI_GT:			value = (str_cmp(lsp->_.str,rsp->_.str) > 0); break;
					case NI_GE:			value = (str_cmp(lsp->_.str,rsp->_.str) >= 0); break;
					case NI_STR_PREFIX: value = !str_prefix(rsp->_.str,lsp->_.str); break;
					case NI_STR_INFIX:	value = !str_infix(rsp->_.str,lsp->_.str); break;
					case NI_STR_SUFFIX:	value = !str_suffix(rsp->_.str,lsp->_.str); break;
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
							case NI_EQ:			value = (str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) == 0); break;
							case NI_NEQ:		value = (str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) != 0); break;
							case NI_LT:			value = (str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) < 0); break;
							case NI_LE:			value = (str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) <= 0); break;
							case NI_GT:			value = (str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) > 0); break;
							case NI_GE:			value = (str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) >= 0); break;
							case NI_STR_PREFIX: value = !str_prefix(*(rsp->_.lvalue._.str),lsp->_.str); break;
							case NI_STR_INFIX:	value = !str_infix(*(rsp->_.lvalue._.str),lsp->_.str); break;
							case NI_STR_SUFFIX:	value = !str_suffix(*(rsp->_.lvalue._.str),lsp->_.str); break;
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
					case NI_EQ:			value = (str_cmp(lsp->_.str,rsp->_.str) == 0); break;
					case NI_NEQ:		value = (str_cmp(lsp->_.str,rsp->_.str) != 0); break;
					case NI_LT:			value = (str_cmp(lsp->_.str,rsp->_.str) < 0); break;
					case NI_LE:			value = (str_cmp(lsp->_.str,rsp->_.str) <= 0); break;
					case NI_GT:			value = (str_cmp(lsp->_.str,rsp->_.str) > 0); break;
					case NI_GE:			value = (str_cmp(lsp->_.str,rsp->_.str) >= 0); break;
					case NI_STR_PREFIX: value = !str_prefix(rsp->_.str,lsp->_.str); break;
					case NI_STR_INFIX:	value = !str_infix(rsp->_.str,lsp->_.str); break;
					case NI_STR_SUFFIX:	value = !str_suffix(rsp->_.str,lsp->_.str); break;
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
					case NI_EQ:			value = (str_cmp(lsp->_.str,rsp->_.str) == 0); break;
					case NI_NEQ:		value = (str_cmp(lsp->_.str,rsp->_.str) != 0); break;
					case NI_LT:			value = (str_cmp(lsp->_.str,rsp->_.str) < 0); break;
					case NI_LE:			value = (str_cmp(lsp->_.str,rsp->_.str) <= 0); break;
					case NI_GT:			value = (str_cmp(lsp->_.str,rsp->_.str) > 0); break;
					case NI_GE:			value = (str_cmp(lsp->_.str,rsp->_.str) >= 0); break;
					case NI_STR_PREFIX: value = !str_prefix(rsp->_.str,lsp->_.str); break;
					case NI_STR_INFIX:	value = !str_infix(rsp->_.str,lsp->_.str); break;
					case NI_STR_SUFFIX:	value = !str_suffix(rsp->_.str,lsp->_.str); break;
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
							case NI_EQ:			value = (str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) == 0); break;
							case NI_NEQ:		value = (str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) != 0); break;
							case NI_LT:			value = (str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) < 0); break;
							case NI_LE:			value = (str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) <= 0); break;
							case NI_GT:			value = (str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) > 0); break;
							case NI_GE:			value = (str_cmp(lsp->_.str,*(rsp->_.lvalue._.str)) >= 0); break;
							case NI_STR_PREFIX: value = !str_prefix(*(rsp->_.lvalue._.str),lsp->_.str); break;
							case NI_STR_INFIX:	value = !str_infix(*(rsp->_.lvalue._.str),lsp->_.str); break;
							case NI_STR_SUFFIX:	value = !str_suffix(*(rsp->_.lvalue._.str),lsp->_.str); break;
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

			// case NST_DUNGEON:
			// case NST_INSTANCE:
			case NST_MOBILE:		// WIDEVNUM op MOBILE => BOOLEAN
				{
					bool value;
					switch(op)
					{
					case NI_EQ:		value = (rsp->_.mobile->pIndexData != NULL) && (lsp->_.wnum.pArea == rsp->_.mobile->pIndexData->area) && (lsp->_.wnum.vnum == rsp->_.mobile->pIndexData->vnum); break;
					case NI_NEQ:	value = (rsp->_.mobile->pIndexData == NULL) || (lsp->_.wnum.pArea != rsp->_.mobile->pIndexData->area) || (lsp->_.wnum.vnum != rsp->_.mobile->pIndexData->vnum); break;
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
					case NI_EQ:		value = (lsp->_.wnum.pArea == rsp->_.room->area) && (lsp->_.wnum.vnum == rsp->_.room->vnum); break;
					case NI_NEQ:	value = (lsp->_.wnum.pArea != rsp->_.room->area) || (lsp->_.wnum.vnum != rsp->_.room->vnum); break;
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

					// case NST_DUNGEON:
					// case NST_INSTANCE:
					case NST_MOBILE:		// WIDEVNUM op MOBILE => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = ((*(rsp->_.lvalue._.mobile))->pIndexData != NULL) && (lsp->_.wnum.pArea == (*(rsp->_.lvalue._.mobile))->pIndexData->area) && (lsp->_.wnum.vnum == (*(rsp->_.lvalue._.mobile))->pIndexData->vnum); break;
							case NI_NEQ:	value = ((*(rsp->_.lvalue._.mobile))->pIndexData == NULL) || (lsp->_.wnum.pArea != (*(rsp->_.lvalue._.mobile))->pIndexData->area) || (lsp->_.wnum.vnum != (*(rsp->_.lvalue._.mobile))->pIndexData->vnum); break;
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
							case NI_EQ:		value = (lsp->_.wnum.pArea == (*(rsp->_.lvalue._.room))->area) && (lsp->_.wnum.vnum == (*(rsp->_.lvalue._.room))->vnum); break;
							case NI_NEQ:	value = (lsp->_.wnum.pArea != (*(rsp->_.lvalue._.room))->area) || (lsp->_.wnum.vnum != (*(rsp->_.lvalue._.room))->vnum); break;
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

	// case NST_DUNGEON:
	// case NST_INSTANCE:
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
					case NI_EQ:		value = (lsp->_.mobile->pIndexData != NULL) && (lsp->_.mobile->pIndexData->area == rsp->_.wnum.pArea) && (lsp->_.mobile->pIndexData->vnum == rsp->_.wnum.vnum); break;
					case NI_NEQ:	value = (lsp->_.mobile->pIndexData == NULL) || (lsp->_.mobile->pIndexData->area != rsp->_.wnum.pArea) || (lsp->_.mobile->pIndexData->vnum != rsp->_.wnum.vnum); break;
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
							case NI_EQ:		value = (lsp->_.mobile->pIndexData != NULL) && (lsp->_.mobile->pIndexData->area == rsp->_.lvalue._.wnum->pArea) && (lsp->_.mobile->pIndexData->vnum == rsp->_.lvalue._.wnum->vnum); break;
							case NI_NEQ:	value = (lsp->_.mobile->pIndexData == NULL) || (lsp->_.mobile->pIndexData->area != rsp->_.lvalue._.wnum->pArea) || (lsp->_.mobile->pIndexData->vnum != rsp->_.lvalue._.wnum->vnum); break;
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

	// case NST_OBJECT:
	// case NST_QUEST:
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
					case NI_EQ:		value = (lsp->_.room->area == rsp->_.wnum.pArea) && (lsp->_.room->vnum == rsp->_.wnum.vnum); break;
					case NI_NEQ:	value = (lsp->_.room->area != rsp->_.wnum.pArea) || (lsp->_.room->vnum != rsp->_.wnum.vnum); break;
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

	// case NST_SHIP:
	// case NST_TOKEN:
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

			case NST_AREA:
				{
					bool value = (rsp->_.area == NULL);
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

			// case NST_DUNGEON:
			// case NST_INSTANCE:
			case NST_MOBILE:
				{
					bool value = (rsp->_.mobile == NULL);
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

			// case NST_OBJECT:
			// case NST_QUEST:
			case NST_ROOM:
				{
					bool value = (rsp->_.room == NULL);
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

			// case NST_SHIP:
			// case NST_TOKEN:
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

					case NST_AREA:
						{
							bool value = (*(rsp->_.lvalue._.area) == NULL);
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

					// case NST_DUNGEON:
					// case NST_INSTANCE:
					case NST_MOBILE:
						{
							bool value = (*(rsp->_.lvalue._.mobile) == NULL);
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

					// case NST_OBJECT:
					// case NST_QUEST:
					case NST_ROOM:
						{
							bool value = (*(rsp->_.lvalue._.room) == NULL);
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

					// case NST_SHIP:
					// case NST_TOKEN:
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
							case NI_EQ:			value = (str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) == 0); break;
							case NI_NEQ:		value = (str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) != 0); break;
							case NI_LT:			value = (str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) < 0); break;
							case NI_LE:			value = (str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) <= 0); break;
							case NI_GT:			value = (str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) > 0); break;
							case NI_GE:			value = (str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) >= 0); break;
							case NI_STR_PREFIX: value = !str_prefix(rsp->_.str,*(lsp->_.lvalue._.str)); break;
							case NI_STR_INFIX:	value = !str_infix(rsp->_.str,*(lsp->_.lvalue._.str)); break;
							case NI_STR_SUFFIX:	value = !str_suffix(rsp->_.str,*(lsp->_.lvalue._.str)); break;
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
							case NI_EQ:			value = (str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) == 0); break;
							case NI_NEQ:		value = (str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) != 0); break;
							case NI_LT:			value = (str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) < 0); break;
							case NI_LE:			value = (str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) <= 0); break;
							case NI_GT:			value = (str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) > 0); break;
							case NI_GE:			value = (str_cmp(*(lsp->_.lvalue._.str),rsp->_.str) >= 0); break;
							case NI_STR_PREFIX:	value = !str_prefix(rsp->_.str,*(lsp->_.lvalue._.str)); break;
							case NI_STR_INFIX:	value = !str_infix(rsp->_.str,*(lsp->_.lvalue._.str)); break;
							case NI_STR_SUFFIX:	value = !str_suffix(rsp->_.str,*(lsp->_.lvalue._.str)); break;
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
									case NI_EQ:			value = (str_cmp(*(lsp->_.lvalue._.str),*(rsp->_.lvalue._.str)) == 0); break;
									case NI_NEQ:		value = (str_cmp(*(lsp->_.lvalue._.str),*(rsp->_.lvalue._.str)) != 0); break;
									case NI_LT:			value = (str_cmp(*(lsp->_.lvalue._.str),*(rsp->_.lvalue._.str)) < 0); break;
									case NI_LE:			value = (str_cmp(*(lsp->_.lvalue._.str),*(rsp->_.lvalue._.str)) <= 0); break;
									case NI_GT:			value = (str_cmp(*(lsp->_.lvalue._.str),*(rsp->_.lvalue._.str)) > 0); break;
									case NI_GE:			value = (str_cmp(*(lsp->_.lvalue._.str),*(rsp->_.lvalue._.str)) >= 0); break;
									case NI_STR_PREFIX: value = !str_prefix(*(rsp->_.lvalue._.str),*(lsp->_.lvalue._.str)); break;
									case NI_STR_INFIX:	value = !str_infix(*(rsp->_.lvalue._.str),*(lsp->_.lvalue._.str)); break;
									case NI_STR_SUFFIX:	value = !str_suffix(*(rsp->_.lvalue._.str),*(lsp->_.lvalue._.str)); break;
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

			case NST_AREA:			// AREA op ???
				{
					switch(rsp->type)
					{
					case NST_AREA:		// AREA op AREA => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = *(lsp->_.lvalue._.area) == (rsp->_.area); break;
							case NI_NEQ:	value = *(lsp->_.lvalue._.area) != (rsp->_.area); break;
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
							case NI_EQ:		value = *(lsp->_.lvalue._.area) == NULL; break;
							case NI_NEQ:	value = *(lsp->_.lvalue._.area) != NULL; break;
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
									case NI_EQ:		value = *(lsp->_.lvalue._.area) == *(rsp->_.lvalue._.area); break;
									case NI_NEQ:	value = *(lsp->_.lvalue._.area) != *(rsp->_.lvalue._.area); break;
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

			// case NST_DUNGEON:
			// case NST_INSTANCE:
			case NST_MOBILE:		// MOBILE op ???
				{
					switch(rsp->type)
					{
					case NST_MOBILE:		// MOBILE op MOBILE => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = *(lsp->_.lvalue._.mobile) == (rsp->_.mobile); break;
							case NI_NEQ:	value = *(lsp->_.lvalue._.mobile) != (rsp->_.mobile); break;
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
							case NI_EQ:		value = ((*(lsp->_.lvalue._.mobile))->pIndexData != NULL) && ((*(lsp->_.lvalue._.mobile))->pIndexData->area == rsp->_.wnum.pArea) && ((*(lsp->_.lvalue._.mobile))->pIndexData->vnum == rsp->_.wnum.vnum); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.mobile))->pIndexData == NULL) || ((*(lsp->_.lvalue._.mobile))->pIndexData->area != rsp->_.wnum.pArea) || ((*(lsp->_.lvalue._.mobile))->pIndexData->vnum != rsp->_.wnum.vnum); break;
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
							case NI_EQ:		value = *(lsp->_.lvalue._.mobile) == NULL; break;
							case NI_NEQ:	value = *(lsp->_.lvalue._.mobile) != NULL; break;
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
									case NI_EQ:		value = *(lsp->_.lvalue._.mobile) == *(rsp->_.lvalue._.mobile); break;
									case NI_NEQ:	value = *(lsp->_.lvalue._.mobile) != *(rsp->_.lvalue._.mobile); break;
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
									case NI_EQ:		value = ((*(lsp->_.lvalue._.mobile))->pIndexData != NULL) && ((*(lsp->_.lvalue._.mobile))->pIndexData->area == rsp->_.lvalue._.wnum->pArea) && ((*(lsp->_.lvalue._.mobile))->pIndexData->vnum == rsp->_.lvalue._.wnum->vnum); break;
									case NI_NEQ:	value = ((*(lsp->_.lvalue._.mobile))->pIndexData == NULL) || ((*(lsp->_.lvalue._.mobile))->pIndexData->area != rsp->_.lvalue._.wnum->pArea) || ((*(lsp->_.lvalue._.mobile))->pIndexData->vnum != rsp->_.lvalue._.wnum->vnum); break;
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

			// case NST_OBJECT:
			// case NST_QUEST:
			case NST_ROOM:			// ROOM op ???
				{
					switch(rsp->type)
					{
					case NST_ROOM:		// ROOM op ROOM => BOOLEAN
						{
							bool value;
							switch(op)
							{
							case NI_EQ:		value = *(lsp->_.lvalue._.room) == (rsp->_.room); break;
							case NI_NEQ:	value = *(lsp->_.lvalue._.room) != (rsp->_.room); break;
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
							case NI_EQ:		value = ((*(lsp->_.lvalue._.room))->area == rsp->_.wnum.pArea) && ((*(lsp->_.lvalue._.room))->vnum == rsp->_.wnum.vnum); break;
							case NI_NEQ:	value = ((*(lsp->_.lvalue._.room))->area != rsp->_.wnum.pArea) || ((*(lsp->_.lvalue._.room))->vnum != rsp->_.wnum.vnum); break;
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
							case NI_EQ:		value = *(lsp->_.lvalue._.room) == NULL; break;
							case NI_NEQ:	value = *(lsp->_.lvalue._.room) != NULL; break;
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
									case NI_EQ:		value = *(lsp->_.lvalue._.room) == *(rsp->_.lvalue._.room); break;
									case NI_NEQ:	value = *(lsp->_.lvalue._.room) != *(rsp->_.lvalue._.room); break;
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

	return false;
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
							if (rsp->_.i < 0 || rsp->_.i > 255)
							{
								SETRET(nsr,INVALID);
								return true;
							}

							*(lsp->_.lvalue._.ch) = (char)(unsigned char)rsp->_.i;

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
									if (*(rsp->_.lvalue._.number) < 0 || *(rsp->_.lvalue._.number) > 255)
									{
										SETRET(nsr,INVALID);
										return true;
									}

									*(lsp->_.lvalue._.ch) = (char)(unsigned char)*(rsp->_.lvalue._.number);

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
							char tmp[2];
							tmp[0] = rsp->_.ch;
							tmp[1] = '\0';

							if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
							*(lsp->_.lvalue._.str) = strdup(tmp);

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
									int len = strlen(*(lsp->_.lvalue._.str));
									char *value = calloc(1,len + 2);
									if (!value)
									{
										SETRET(nsr,MEMORY);
										return true;
									}

									strcpy(value,*(lsp->_.lvalue._.str));
									value[len] = rsp->_.ch;
									value[len+1] = '\0';

									free(*(lsp->_.lvalue._.str));
									*(lsp->_.lvalue._.str) = value;
								}
							}
							else
							{
								char tmp[2];
								tmp[0] = rsp->_.ch;
								tmp[1] = '\0';

								*(lsp->_.lvalue._.str) = strdup(tmp);
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
									char tmp[2];
									tmp[0] = *(rsp->_.lvalue._.ch);
									tmp[1] = '\0';

									if (*(lsp->_.lvalue._.str)) free(*(lsp->_.lvalue._.str));
									*(lsp->_.lvalue._.str) = strdup(tmp);

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
											int len = strlen(*(lsp->_.lvalue._.str));
											char *value = calloc(1,len + 2);
											if (!value)
											{
												SETRET(nsr,MEMORY);
												return true;
											}

											strcpy(value,*(lsp->_.lvalue._.str));
											value[len] = *(rsp->_.lvalue._.ch);
											value[len+1] = '\0';

											free(*(lsp->_.lvalue._.str));
											*(lsp->_.lvalue._.str) = value;
										}
									}
									else
									{
										char tmp[2];
										tmp[0] = *(rsp->_.lvalue._.ch);
										tmp[1] = '\0';

										*(lsp->_.lvalue._.str) = strdup(tmp);
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

	case NST_AREA:
		{
			switch(rsp->type)
			{
			case NST_AREA:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.area) = rsp->_.area;

							if (push_result && !nib_push_stack_area(nsr,*(lsp->_.lvalue._.area)))
							{
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
							*(lsp->_.lvalue._.area) = NULL;

							if (push_result && !nib_push_stack_area(nsr,*(lsp->_.lvalue._.area)))
							{
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
					case NST_AREA:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.area) = *(rsp->_.lvalue._.area);

									if (push_result && !nib_push_stack_area(nsr,*(lsp->_.lvalue._.area)))
									{
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

	case NST_MOBILE:
		{
			switch(rsp->type)
			{
			case NST_MOBILE:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.mobile) = rsp->_.mobile;

							if (push_result && !nib_push_stack_mobile(nsr,*(lsp->_.lvalue._.mobile)))
							{
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
							*(lsp->_.lvalue._.mobile) = NULL;

							if (push_result && !nib_push_stack_mobile(nsr,*(lsp->_.lvalue._.mobile)))
							{
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
					case NST_MOBILE:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.mobile) = *(rsp->_.lvalue._.mobile);

									if (push_result && !nib_push_stack_mobile(nsr,*(lsp->_.lvalue._.mobile)))
									{
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

	case NST_ROOM:
		{
			switch(rsp->type)
			{
			case NST_ROOM:
				{
					switch(op)
					{
					case NI_VOID_ASSIGN:
						push_result = false;
					case NI_ASSIGN:
						{
							*(lsp->_.lvalue._.room) = rsp->_.room;

							if (push_result && !nib_push_stack_room(nsr,*(lsp->_.lvalue._.room)))
							{
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
							*(lsp->_.lvalue._.room) = NULL;

							if (push_result && !nib_push_stack_room(nsr,*(lsp->_.lvalue._.room)))
							{
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
					case NST_ROOM:
						{
							switch(op)
							{
							case NI_VOID_ASSIGN:
								push_result = false;
							case NI_ASSIGN:
								{
									*(lsp->_.lvalue._.room) = *(rsp->_.lvalue._.room);

									if (push_result && !nib_push_stack_room(nsr,*(lsp->_.lvalue._.room)))
									{
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
			char value = __get_char(nsr);
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
		break;

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
				case NST_CHAR:		var->_.ch = *((char *)data); break;
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
		int address;
		long number;
		char ch;
		double floating;
		short string_index;
		short table_index;
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
			case NI_SWITCH:
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
					memcpy(&string_index, &pc[addr+1], sizeof(string_index));
					const char *str = nib_get_string(string_index);

					if (str)
						linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d \"%s\"", string_index, str);
					else
						linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d --invalid--", string_index);

					addr+=sizeof(string_index);
					break;
				}

			case NI_LOAD_CHAR:
				ch = pc[addr+1];
				linej += snprintf(line + linej, sizeof(line) - linej - 1, " %c (%02X)", (isprint(ch) ? ch : '.'), (unsigned char)ch);
				addr++;
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
				memcpy(&table_index, &pc[addr+1], sizeof(table_index)); addr+=sizeof(table_index);
				if (table_index > 0 && table_index <= script->n_tables)
					table = script->tables[table_index - 1];
				else
					table = NULL;

				linej += snprintf(line + linej, sizeof(line) - linej - 1, " <[%08X@%s]>", number, nib_get_flag_table_name(script->flag_tables,table));
				break;

			case NI_LOAD_STAT:
				memcpy(&number, &pc[addr+1], sizeof(number)); addr += sizeof(number);
				memcpy(&table_index, &pc[addr+1], sizeof(table_index)); addr+=sizeof(table_index);
				if (table_index > 0 && table_index <= script->n_tables)
					table = script->tables[table_index - 1];
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
	case NST_CHAR:		return snprintf(line, max_len, "CHR(%02X %c)", (unsigned char)stack->_.ch, (isprint(stack->_.ch)?stack->_.ch:'.'));
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
		case NST_CHAR:		return snprintf(line, max_len, "LVALUE(CHR(%02X %c))", (unsigned char)*(stack->_.lvalue._.ch), (isprint(*(stack->_.lvalue._.ch))?*(stack->_.lvalue._.ch):'.'));
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
	case NST_CHAR:		if (isprint(var->_.ch)) snprintf(value, max_vlen, "'%c'", var->_.ch); else snprintf(value, max_vlen, "0x%02X", (unsigned char)var->_.ch); break;
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
