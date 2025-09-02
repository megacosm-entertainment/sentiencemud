#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <stdint.h>

#include "niblang.h"
#include "script.h"
#include "interpret.h"

NIB_SCRIPT_STACK_TYPE convert_to_stype(NIB_TYPE *type)
{
	if (!type) return NST_VOID;

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

static NIB_SCRIPT_RUNTIME *new_script_runtime(NIB_SCRIPT *script)
{
	NIB_SCRIPT_RUNTIME *nsr = calloc(1, sizeof(NIB_SCRIPT_RUNTIME));

	if (nsr)
	{
		nsr->script = script;

		nsr->n_locals = script->n_locals;
		nsr->locals = calloc(nsr->n_locals, sizeof(NIB_LOCAL_RUNTIME_VAR));

		for(int i = nsr->n_locals; i-- > 0;)
		{
			nsr->locals[i].name = script->locals[i].name;
			nsr->locals[i].type = script->locals[i].stype;
			nsr->locals[i].constant = script->locals[i].constant;

			if (nsr->locals[i].type == NST_LIST)
			{
				// Need to store the list's subtype
				nsr->locals[i]._.list.type = convert_to_stype(script->locals[i].type->_.type);
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

		free(nsr);
	}
}

#define PUSH_STACK	\
	if (++nsr->sp >= MAX_STACK) return true

#define __push(t,d,f,n) \
bool nib_push_stack_##n (NIB_SCRIPT_RUNTIME *nsr, t value) \
{ \
	PUSH_STACK; \
\
	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp]; \
\
	stack->type = NST_##d; \
	stack->_.f = value; \
\
	return false; \
}

__push(long,NUMBER,i,number)
__push(double,FLOAT,d,float)
__push(bool,BOOLEAN,b,boolean)
__push(char,CHAR,ch,char)
__push(char *,STRING_S,str,string_shared)
__push(WNUM,WIDEVNUM,wnum,widevnum)
__push(long,FLAG,i,flag)
__push(long,STAT,i,stat)
__push(AREA_DATA *,AREA,area,area)
__push(CHAR_DATA *,MOBILE,mobile,mobile)
__push(ROOM_INDEX_DATA *,ROOM,room,room)

bool nib_push_stack_list (NIB_SCRIPT_RUNTIME *nsr, LLIST *value, NIB_SCRIPT_STACK_TYPE type)
{
	PUSH_STACK;

	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp];

	stack->type = NST_LIST;
	stack->_.list.type = type;
	stack->_.list.list = value;

	return false;
}

bool nib_push_stack_list_shared (NIB_SCRIPT_RUNTIME *nsr, LLIST *value, NIB_SCRIPT_STACK_TYPE type)
{
	PUSH_STACK;

	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp];

	stack->type = NST_LIST_S;
	stack->_.list.type = type;
	stack->_.list.list = value;

	return false;
}

// This needs to be freed when popped
bool nib_push_stack_string(NIB_SCRIPT_RUNTIME *nsr, char *value)
{
	PUSH_STACK;

	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp];

	stack->type = NST_STRING;
	stack->_.str = strdup(value);

	return false;
}

bool nib_push_stack_lvalue(NIB_SCRIPT_RUNTIME *nsr, NIB_SCRIPT_LVALUE *value)
{
	PUSH_STACK;

	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp];

	stack->type = NST_LVALUE;
	stack->_.lvalue = *value;

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
		return nib_push_stack_string(nsr, var->_.str);

	case NST_WIDEVNUM:
		return nib_push_stack_widevnum(nsr, var->_.wnum);

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
		return nib_push_stack_flag(nsr, var->_.i);

	case NST_STAT:
		return nib_push_stack_stat(nsr, var->_.i);

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
		lvalue._.number = &(var->_.i);
		break;

	case NST_STAT:
		lvalue.type = NST_STAT;
		lvalue._.number = &(var->_.i);
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
		return nib_push_stack_string(nsr, var->_.str);

	case VAR_WIDEVNUM:
		return nib_push_stack_widevnum(nsr, var->_.wnum);

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
	if (var->readonly) return false;

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


NIB_SCRIPT_STACK_TYPE nib_peek_stack(NIB_SCRIPT_RUNTIME *nsr)
{
	return nsr->stack[nsr->sp].type;
}

#define __peek(t,d,f,n) \
bool nib_peek_stack_##n (NIB_SCRIPT_RUNTIME *nsr, int offset, t *output) \
{ \
	int sp = nsr->sp - offset - 1; \
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
__peek(long,FLAG,i,flag)
__peek(long,STAT,i,stat)
__peek(AREA_DATA *,AREA,area,area)
__peek(CHAR_DATA *,MOBILE,mobile,mobile)
__peek(ROOM_INDEX_DATA *,ROOM,room,room)
__peek(NIB_SCRIPT_LVALUE,LVALUE,lvalue,lvalue)

bool nib_peek_stack_list (NIB_SCRIPT_RUNTIME *nsr, int offset, LLIST **output, NIB_SCRIPT_STACK_TYPE *type)
{
	int sp = nsr->sp - offset - 1;

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
	int sp = nsr->sp - offset - 1;

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
	}

}

// Pops the stack without getting the value
bool pop_stack(NIB_SCRIPT_RUNTIME *nsr)
{
	if (nsr->sp < 1) return true;		// Popped one to many

	free_stack_item(&nsr->stack[nsr->sp]);

	--nsr->sp;
	return false;
}

// Pops value off stack, pulling value
#define __pop(t,d,f,n) \
bool nib_pop_stack_##n (NIB_SCRIPT_RUNTIME *nsr, t *output) \
{ \
	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp]; \
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
__pop(char *,STRING,str,string)
__pop(char *,STRING_S,str,string_shared)
__pop(WNUM,WIDEVNUM,wnum,widevnum)
__pop(long,FLAG,i,flag)
__pop(long,STAT,i,stat)
__pop(AREA_DATA *,AREA,area,area)
__pop(CHAR_DATA *,MOBILE,mobile,mobile)
__pop(ROOM_INDEX_DATA *,ROOM,room,room)
__pop(NIB_SCRIPT_LVALUE,LVALUE,lvalue,lvalue)

bool nib_pop_stack_list (NIB_SCRIPT_RUNTIME *nsr, LLIST **output, NIB_SCRIPT_STACK_TYPE *type)
{
	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp];

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
	NIB_SCRIPT_STACK *stack = &nsr->stack[nsr->sp];

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
__get(long,address)
__get(long,number)
__get(bool,boolean)
__get(double,float)


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


int nib_interpret_script(NIB_SCRIPT *script /* add arguments */)
{
	// Generate the script runtime
	NIB_SCRIPT_RUNTIME *nsr = new_script_runtime(script);
	if (!nsr) return -1;

	nsr->pc = 0;
	
	bool running = true;
	while(running && nsr->pc < script->code_len)
	{
		switch(script->code[nsr->pc++])
		{
		/*
		case NI_LOAD_LOCAL:
			{
				int index = __get_int(nsr);
				NIB_LOCAL_RUNTIME_VAR *var = get_local_var(nsr, index);

				nib_push_stack_local_var(nsr, var);
				break;
			}

		case NI_LOAD_GLOBAL:
			{
				int index = __get_int(nsr);
				pVARIABLE var = get_global_var(nsr, index);

				nib_push_stack_global_var(nsr, var);
				break;
			}
			*/

		case NI_LVALUE_SELF:
			break;

		case NI_LVALUE_LOCAL:
			break;

		case NI_LVALUE_GLOBAL:
			break;

		case NI_LVALUE_FLAG:
			break;

		case NI_LVALUE_FIELD:
			break;

		case NI_CALL_FUNCTION:
			break;

		case NI_CALL_METHOD:
			break;

		case NI_LOAD_STRING:
			break;

		case NI_LOAD_NUMBER:
			{
				long number = __get_number(nsr);
				nib_push_stack_number(nsr, number);
				break;
			}

		case NI_LOAD_FLOAT:
			{
				double value = __get_float(nsr);
				nib_push_stack_float(nsr, value);
				break;
			}

		case NI_LOAD_WIDEVNUM:
			{
				
			}
			break;

		case NI_NEW_LIST:
			{
				// Get the type

				break;
			}

		case NI_CONST0:
			nib_push_stack_number(nsr, 0);
			break;

		case NI_CONST1:
			nib_push_stack_number(nsr, 1);
			break;

		case NI_NCONST1:
			nib_push_stack_number(nsr, -1);
			break;

		case NI_FCONST0:
			nib_push_stack_float(nsr, 0.0);
			break;

		case NI_DUP:
			// Copy the top of the stack
			break;

		case NI_POP:
			pop_stack(nsr);
			break;

		case NI_RETURN:
			{
				long code = -1;
				nib_pop_stack_number(nsr, &code);

				nsr->last_return = code;
				running = false;
				break;
			}

		case NI_JUMP:
			break;

		case NI_JUMP_ZERO:
			break;

		case NI_JUMP_NOT_ZERO:
			break;

		case NI_SWITCH:
			break;

		case NI_INC:
			break;

		case NI_DEC:
			break;

		case NI_POST_INC:
			break;

		case NI_POST_DEC:
			break;

		case NI_PRE_INC:
			break;

		case NI_PRE_DEC:
			break;

		case NI_ITER_START:
			break;

		case NI_ITER_STOP:
			break;

		case NI_ITER_NEXT:
			break;

		case NI_LAND:
			break;

		case NI_LOR:
			break;

		case NI_LXOR:
			break;

		case NI_LNOT:
			break;

		case NI_ASSIGN:
			break;

		case NI_VOID_ASSIGN:
			break;

		case NI_NEG:
			break;

		case NI_ADD:
			break;

		case NI_SUBT:
			break;

		case NI_MULT:
			break;

		case NI_MOD:
			break;

		case NI_DIV:
			break;

		case NI_EQ:
			break;

		case NI_NEQ:
			break;

		case NI_LT:
			break;

		case NI_LE:
			break;

		case NI_GT:
			break;

		case NI_GE:
			break;

		case NI_BAND:
			break;

		case NI_BOR:
			break;

		case NI_BXOR:
			break;

		case NI_BNOT:
			break;

		case NI_LSH:
			break;

		case NI_RSH:
			break;

		case NI_RSHL:
			break;

		case NI_ADD_EQ:
			break;

		case NI_SUBT_EQ:
			break;

		case NI_MULT_EQ:
			break;

		case NI_MOD_EQ:
			break;

		case NI_DIV_EQ:
			break;

		case NI_BAND_EQ:
			break;

		case NI_BOR_EQ:
			break;

		case NI_BXOR_EQ:
			break;

		case NI_LSH_EQ:
			break;

		case NI_RSH_EQ:
			break;

		case NI_RSHL_EQ:
			break;

		case NI_GET_AREA:
			break;

		}
	}

	if (running && nsr->sp > 0)
	{
		printf("Stack was not empty at end of program.\n");
	}

	// Clear up the stack, regardless
	for(int sp = 0; sp < nsr->sp; sp++)
		free_stack_item(&nsr->stack[sp]);

	return nsr->last_return;
}