%define lr.type ielr
%define parse.error verbose
%defines
%{

/*
 * Parser.y file
 * To generate the parser run: "bison Parser.y"
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdint.h>

#include "../../merc.h"
#include "../niblang.h"
#include "parser.h"
#include "lexer.h"

#define IS_READONLY		(A)
#define IS_LITERAL		(B)
#define IS_LVALUE		(C)

#define MAX_SHIFT		(bitsize(long) - 1)

extern int niblineno;
extern NIB_SCRIPT_CLASS nib_compile_script_class;
extern NIB_BUFFER *nib_program_storage;
bool is_valid_variable_type(pVARIABLE var, NIB_TYPE *type);
NIB_SCRIPT_STACK_TYPE convert_to_stype(NIB_TYPE *type, bool constant);
extern LLIST *nib_flag_created_tables;
extern LLIST *nib_stat_created_tables;
extern LLIST *nib_switch_blocks;
extern struct nib_compile_switch_s *nib_current_switch;
struct nib_compile_switch_case_s *new_nib_compile_switch_case();
void free_nib_compile_switch_case(struct nib_compile_switch_case_s *cs);
bool push_nib_switch_block(SWITCH_TYPE type, const struct flag_type *table);
void pop_nib_switch_block();
bool nib_switch_overlaps_case(struct nib_compile_switch_case_s *c);
void nib_switch_add_case(struct nib_compile_switch_case_s *c);

static long last_expression = -1;

#define CURRENT_PROGRAM_SIZE	(nib_program_storage->len)
#define EXTEND_PROGRAM(o,l)		(mem_buffer_extend(nib_program_storage, (o), (l))
#define PROGRAM_BUFFER			(nib_program_storage->buffer)
#define ins_byte(b)				(mem_buffer_append_byte(nib_program_storage, (b)))
#define ins_short(s)			(mem_buffer_append_short(nib_program_storage, (s)))
#define ins_int(i)				(mem_buffer_append_int(nib_program_storage, (i)))
#define ins_long(l)				(mem_buffer_append_long(nib_program_storage, (l)))
#define ins_float(f)			(mem_buffer_append_float(nib_program_storage, (f)))
#define ins_ptr(p)				(mem_buffer_append_pointer(nib_program_storage, (void *)(p)))
#define ins_bytes(d,l)			(mem_buffer_append(nib_program_storage, (d), (l)))
#define upd_byte(o,b)			(PROGRAM_BUFFER[(o)] = (b))
#define upd_short(o,s)			(mem_buffer_update_short(nib_program_storage, (o), (s)))
#define upd_int(o,i)			(mem_buffer_update_int(nib_program_storage, (o), (i)))
#define upd_long(o,l)			(mem_buffer_update_long(nib_program_storage, (o), (l)))
#define upd_float(o,f)			(mem_buffer_update_float(nib_program_storage, (o), (f)))
#define upd_ptr(o,p)			(mem_buffer_update_pointer(nib_program_storage, (o), (void *)(p)))

#define get_byte(o)				(PROGRAM_BUFFER[(o)])

#define PUT_TYPE(t) \
static inline void put_##t (nib_bytecode_p address, t data) \
{ \
	memcpy(address, &data, sizeof(data)); \
}

#define PUT_TYPE2(n, t) \
static inline void put_##n (nib_bytecode_p address, t data) \
{ \
	memcpy(address, &data, sizeof(data)); \
}

PUT_TYPE2(byte,nib_bytecode_t)
PUT_TYPE(short)
PUT_TYPE(int)
PUT_TYPE(long)
PUT_TYPE2(float,double)

static inline void ins_code(enum nib_instructions_e instr)
{
	ins_byte((nib_bytecode_t)instr);
}

static inline void ins_address(int data)
{
	mem_buffer_append(nib_program_storage, (nib_bytecode_p)&data, sizeof(data));
}

static inline void upd_code(int address, enum nib_instructions_e instr)
{
	upd_byte(address, (nib_bytecode_t)instr);
}

static inline void upd_address(int address, int data)
{
	if ((address + sizeof(data)) > nib_program_storage->len)
		return;

	memcpy(nib_program_storage->buffer + address, (nib_bytecode_p)&data, sizeof(data));
}

static inline nib_bytecode_p nib_alloc_bytecodes(size_t size)
{
	return nib_calloc(1, size);
}

static void nib_free_bytecodes(nib_bytecode_p data)
{
	if (data) nib_free(data);
}

#define set_current_address(o)		(upd_address((o), CURRENT_PROGRAM_SIZE))


void nib_free_lvalue_s(struct lvalue_s *lvalue)
{
	nib_free_bytecodes(lvalue->lhs);
	nib_free_bytecodes(lvalue->rhs);
}

void nib_free_lrvalue_s(struct lrvalue_s *lrvalue)
{
	nib_free_bytecodes(lrvalue->lhs);
	nib_free_bytecodes(lrvalue->rhs);
}

void nib_free_rvalue_s(struct rvalue_s *rvalue)
{
	nib_free_bytecodes(rvalue->rhs);
}

static void insert_pop_value()
{
#if 1
	if (last_expression < 0)
	{
		ins_code(NI_POP);
	}
	else if (last_expression == (CURRENT_PROGRAM_SIZE - sizeof(nib_bytecode_t)))
	{
		switch(get_byte(last_expression))
		{
			case NI_VOID_ASSIGN:
			case NI_VOID_ADD_EQ:
				return;

			case NI_ASSIGN:
				upd_byte(last_expression, NI_VOID_ASSIGN);
				break;

			case NI_ADD_EQ:
				upd_byte(last_expression, NI_VOID_ADD_EQ);
				break;

			case NI_PRE_INC:
			case NI_POST_INC:
				upd_byte(last_expression, NI_INC);
				break;

			case NI_PRE_DEC:
			case NI_POST_DEC:
				upd_byte(last_expression, NI_DEC);
				break;

			case NI_NULL:
			case NI_TRUE:
			case NI_FALSE:
			case NI_CONST0:
			case NI_CONST1:
			case NI_NCONST1:
			case NI_FCONST0:
				CURRENT_PROGRAM_SIZE = last_expression;
				break;

			default:
				ins_code(NI_POP);
				break;
		}
	}
	// Check for LOAD_STRING
	else if(last_expression == (CURRENT_PROGRAM_SIZE - sizeof(nib_bytecode_t) - sizeof(short)))
	{
		if (get_byte(last_expression) == NI_LOAD_STRING)
			CURRENT_PROGRAM_SIZE = last_expression;
		else
			ins_code(NI_POP);
	}
	// Check for LOAD_NUMBER
	else if(last_expression == (CURRENT_PROGRAM_SIZE - sizeof(nib_bytecode_t) - sizeof(long)))
	{
		if (get_byte(last_expression) == NI_LOAD_NUMBER)
			CURRENT_PROGRAM_SIZE = last_expression;
		else
			ins_code(NI_POP);
	}
	// Check for LOAD FLOAT
	else if(last_expression == (CURRENT_PROGRAM_SIZE - sizeof(nib_bytecode_t) - sizeof(double)))
	{
		if (get_byte(last_expression) == NI_LOAD_FLOAT)
			CURRENT_PROGRAM_SIZE = last_expression;
		else
			ins_code(NI_POP);
	}
	else
	{
		ins_code(NI_POP);
	}
#else
	ins_code(NI_POP);
#endif
}

static enum nib_switch_type_e check_valid_switch_type(NIB_TYPE *type, const struct flag_type **table)
{
	*table = NULL;
	if (type == NULL) return NSWT_UNKNOWN;
	if (type == nibtype_int32) return NSWT_NUMBER;
	if (type == nibtype_int) return NSWT_NUMBER;
	if (type == nibtype_float) return NSWT_FLOAT;
	if (type == nibtype_char) return NSWT_CHAR;
	if (type == nibtype_string) return NSWT_STRING;
	if (type->type_class == NTC_STAT)
	{
		*table = type->_.stat.table;
		return NSWT_STAT;
	}

	return NSWT_UNKNOWN;
}

// Checks assignment can be handled, adding any instructions if necessary.
static bool check_valid_assignment(NIB_TYPE *left, NIB_TYPE *right, enum nib_instructions_e op)
{
	NIB_SCRIPT_STACK_TYPE lhs = convert_to_stype(left, false);
	if (lhs == NST_NUMBER32)
		lhs = NST_NUMBER;

	NIB_SCRIPT_STACK_TYPE rhs;
	if (right->type_class == NTC_MULTI)
	{
		if (!is_nib_type_in_multi(right, left))
			return false;

		// Automatically morph to the left's type
		right = left;
		rhs = lhs;
	}
	else
	{
		rhs = convert_to_stype(right, false);

		if (rhs == NST_NUMBER32)
			rhs = NST_NUMBER;
	}

	switch(lhs)
	{
	case NST_NUMBER:
		if (rhs == NST_NUMBER)
			return true;	// All operations are valid between numbers
		else if (rhs == NST_FLOAT)
		{
			if (op == NI_ASSIGN ||
				op == NI_ADD_EQ ||
				op == NI_SUBT_EQ ||
				op == NI_MULT_EQ ||
				op == NI_DIV_EQ)
				return true;
		}
		else if (rhs == NST_BOOLEAN)
			return op == NI_ASSIGN;
		break;

	case NST_FLOAT:
		if (rhs == NST_NUMBER ||
			rhs == NST_FLOAT)
		{
			if (op == NI_ASSIGN ||
				op == NI_ADD_EQ ||
				op == NI_SUBT_EQ ||
				op == NI_MULT_EQ ||
				op == NI_DIV_EQ)
				return true;
		}
		else if (rhs == NST_BOOLEAN)
			return op == NI_ASSIGN;
		break;

	case NST_BOOLEAN:				// Can only assign to BOOLEAN lvalues
		if (rhs == NST_BOOLEAN)
			return op == NI_ASSIGN;
		break;

	case NST_CHAR:					// Can only assign to CHAR lvalues
		if (rhs == NST_NUMBER ||
			rhs == NST_CHAR ||
			rhs == NST_NULL)
			return op == NI_ASSIGN;
		break;
	
	case NST_STRING:
		if (rhs == NST_NUMBER)
		{
			if (op == NI_ASSIGN ||	// Stringify
				op == NI_ADD_EQ ||	// Concatenation
				op == NI_MULT_EQ)	// Cloning
				return true;
		}
		else if (rhs == NST_CHAR ||
				 rhs == NST_STRING)
		{
			if (op == NI_ASSIGN ||		// Assignment
				op == NI_ADD_EQ)		// Concatenation
				return true;
		}
		else if (rhs != NST_LIST && rhs != NST_ARRAY)
			return op == NI_ASSIGN ||	// This performs a "stringify" operation
				   op == NI_ADD_EQ;		// Concatenation
		break;

	case NST_WIDEVNUM:
		if (rhs == NST_WIDEVNUM ||
			rhs == NST_NULL)
			return op == NI_ASSIGN;	// Can only assign to WIDEVNUM lvalues.
		break;

	case NST_FLAG:
		if (rhs == NST_NUMBER ||
			rhs == NST_FLAG)
		{
			if (op == NI_ASSIGN ||		// Assignment
				op == NI_BAND_EQ ||		// Masking/Resetting
				op == NI_BOR_EQ ||		// Setting
				op == NI_BXOR_EQ)		// Toggling
				return true;
		}
		break;

	case NST_FLAG_BANK:
		if (rhs == NST_FLAG_BANK)
		{
			if (op == NI_ASSIGN ||		// Assignment
				op == NI_BAND_EQ ||		// Masking/Resetting
				op == NI_BOR_EQ ||		// Setting
				op == NI_BXOR_EQ)		// Toggling
				return true;
		}
		break;

	case NST_STAT:
		if (rhs == NST_STAT)
			return op == NI_ASSIGN;	// Can only assign to STAT lvalues.
		break;

	case NST_LIST:
		if (!left->_.list.constant && right == nibtype_list)	// Special case: LIST = ({}); -- NEW LIST generation
		{
			if (op == NI_ASSIGN)
			{
				ins_code(NI_NEW_LIST);
				ins_byte(convert_to_stype(left->_.list.type, false));
				return true;
			}
		}
		else if (rhs == lhs ||
				 rhs == NST_NULL)
			return op == NI_ASSIGN;
		break;

	case NST_ARRAY:
		if (!left->_.array.constant && right == nibtype_list)	// ARRAY = {[]};
		{
			if (op == NI_ASSIGN)
			{
				ins_code(NI_NEW_ARRAY);
				ins_byte(convert_to_stype(left->_.array.type, false));
				ins_long(left->_.array.length);
				return true;
			}
		}
		else if (rhs == lhs ||
				 rhs == NST_NULL)
			return op == NI_ASSIGN;
		break;
	
	case NST_AFFECT:
	// case NST_CHANNEL:
	case NST_DUNGEON:
	case NST_EXIT:
	case NST_INSTANCE:
	case NST_MAIL:
	case NST_MISSION:
	case NST_MOBILE:
	case NST_NOTE:
	case NST_OBJECT:
	// case NST_QUEST:
	case NST_RANK:
	case NST_REPUTATION:
	case NST_ROOM:
	case NST_SHIP:
	case NST_TOKEN:
	// case NST_WORLD:
		if (rhs == lhs ||
			rhs == NST_NULL)
			return op == NI_ASSIGN;
		break;

	case NST_ACCOUNT:
		if (rhs == NST_ACCOUNT ||		// Direct
			rhs == NST_MOBILE ||		// Pulls off players, NULL for NPCs
			rhs == NST_NULL)
			return op == NI_ASSIGN;
		break;

	case NST_AREA:
		if (rhs == NST_NUMBER ||
			rhs == NST_STRING)
		{
			if (op == NI_ASSIGN)
			{
				ins_code(NI_GET_AREA);
				return true;
			}
		}
		else if (rhs == NST_AREA ||
				 rhs == NST_NULL)
			return op == NI_ASSIGN;
		break;

	case NST_CLASS:
		if (rhs == NST_STRING)
		{
			if (op == NI_ASSIGN)
			{
				ins_code(NI_GET_CLASS);
				return true;
			}
		}
		else if (rhs == lhs ||
				 rhs == NST_NULL)
			return op == NI_ASSIGN;
		break;

	case NST_LIQUID:
		if (rhs == NST_STRING)
		{
			if (op == NI_ASSIGN)
			{
				ins_code(NI_GET_LIQUID);
				return true;
			}
		}
		else if (rhs == lhs ||
				 rhs == NST_NULL)
			return op == NI_ASSIGN;
		break;

	case NST_MATERIAL:
		if (rhs == NST_STRING)
		{
			if (op == NI_ASSIGN)
			{
				ins_code(NI_GET_MATERIAL);
				return true;
			}
		}
		else if (rhs == lhs ||
				 rhs == NST_NULL)
			return op == NI_ASSIGN;
		break;

	case NST_ORG:
		if (rhs == NST_STRING)
		{
			if (op == NI_ASSIGN)
			{
				ins_code(NI_GET_ORG);
				return true;
			}
		}
		else if (rhs == lhs ||
				 rhs == NST_NULL)
			return op == NI_ASSIGN;
		break;

	case NST_RACE:
		if (rhs == NST_STRING)
		{
			if (op == NI_ASSIGN)
			{
				ins_code(NI_GET_RACE);
				return true;
			}
		}
		else if (rhs == lhs ||
				 rhs == NST_NULL)
			return op == NI_ASSIGN;
		break;

	case NST_SKILL:
		if (rhs == NST_STRING)
		{
			if (op == NI_ASSIGN)
			{
				ins_code(NI_GET_SKILL);
				return true;
			}
		}
		else if (rhs == lhs ||
				 rhs == NST_NULL)
			return op == NI_ASSIGN;
		break;

	case NST_WILDS:
		if (rhs == NST_NUMBER)
		{
			if (op == NI_ASSIGN)
			{
				ins_code(NI_GET_WILDS);
				return true;
			}
		}
		else if (rhs == lhs ||
				 rhs == NST_NULL)
			return op == NI_ASSIGN;
		break;

	}

	return false;
}

#define __bool_ent(t) \
	case NST_##t: \
		switch(rhs) \
		{ \
		case NST_##t: \
			switch(op) \
			{ \
			case NI_EQ: \
			case NI_NEQ: \
			case NI_LAND: \
			case NI_LOR: \
			case NI_LXOR: \
				return nibtype_bool; \
			} \
			break; \
		case NST_NULL: \
			if (op == NI_EQ || op == NI_NEQ) \
				return nibtype_bool; \
			break; \
		} \
		break; \

#define __bool_ent_wnum(t) \
	case NST_##t: \
		switch(rhs) \
		{ \
		case NST_##t: \
			switch(op) \
			{ \
			case NI_EQ: \
			case NI_NEQ: \
			case NI_LAND: \
			case NI_LOR: \
			case NI_LXOR: \
				return nibtype_bool; \
			} \
			break; \
		case NST_WIDEVNUM: \
			if (op == NI_EQ || op == NI_NEQ) \
				return nibtype_bool; \
			break; \
		case NST_NULL: \
			if (op == NI_EQ || op == NI_NEQ) \
				return nibtype_bool; \
			break; \
		} \
		break; \


static NIB_TYPE *check_valid_operation(NIB_TYPE *left, NIB_TYPE *right, enum nib_instructions_e op)
{
	NIB_SCRIPT_STACK_TYPE lhs = convert_to_stype(left, false);
	NIB_SCRIPT_STACK_TYPE rhs = convert_to_stype(right, false);

	if (lhs == NST_NUMBER32) lhs = NST_NUMBER;
	else if (lhs == NST_STAT32) lhs = NST_STAT;

	if (rhs == NST_NUMBER32) rhs = NST_NUMBER;
	else if (rhs == NST_STAT32) rhs = NST_STAT;

	// String concatenation for anything
	switch(lhs)
	{
	case NST_NUMBER:	// NUMBER op ???
		switch(rhs)
		{
		case NST_NUMBER:	// NUMBER op NUMBER
			switch(op)
			{
			case NI_ADD:		// NUMBER + NUMBER (Addition)
			case NI_SUBT:		// NUMBER - NUMBER (Subtraction)
			case NI_MULT:		// NUMBER * NUMBER (Multiplication)
			case NI_MOD:		// NUMBER % NUMBER (Modulo Division)
			case NI_DIV:		// NUMBER / NUMBER (Division)
			case NI_BAND:		// NUMBER & NUMBER (Bitwise AND)
			case NI_BOR:		// NUMBER | NUMBER (Bitwise OR)
			case NI_BXOR:		// NUMBER ^ NUMBER (Bitwise XOR)
			case NI_LSH:		// NUMBER << NUMBER (Left Shift)
			case NI_RSH:		// NUMBER >> NUMBER (Right Shift)
			case NI_RSHL:		// NUMBER >>> NUMBER (Logical Right Shift)
				return nibtype_int;

			case NI_EQ:			// NUMBER == NUMBER (Equality)
			case NI_NEQ:		// NUMBER != NUMBER (Inequality)
			case NI_LT:			// NUMBER < NUMBER (Less Than)
			case NI_LE:			// NUMBER <= NUMBER (Less Than or Equal)
			case NI_GT:			// NUMBER > NUMBER (Greater Than)
			case NI_GE:			// NUMBER >= NUMBER (Greater Than or Equal)
			case NI_LAND:		// NUMBER && NUMBER (Both Numbers Non-Zero)
			case NI_LOR:		// NUMBER || NUMBER (One or Both Numbers Non-Zero)
			case NI_LXOR:		// NUMBER ^^ NUMBER (Only One Number Non-Zero)
				return nibtype_bool;
			}

			break;
		case NST_FLOAT:		// NUMBER op FLOAT
			switch(op)
			{
			case NI_ADD:		// NUMBER + FLOAT (Addition)
			case NI_SUBT:		// NUMBER - FLOAT (Subtraction)
			case NI_MULT:		// NUMBER * FLOAT (Multiplication)
			case NI_DIV:		// NUMBER / FLOAT (Division)
				return nibtype_float;

			case NI_EQ:			// NUMBER == FLOAT (Equality)
			case NI_NEQ:		// NUMBER != FLOAT (Inequality)
			case NI_LT:			// NUMBER < FLOAT (Less Than)
			case NI_LE:			// NUMBER <= FLOAT (Less Than or Equal)
			case NI_GT:			// NUMBER > FLOAT (Greater Than)
			case NI_GE:			// NUMBER >= FLOAT (Greater Than or Equal)
			case NI_LAND:		// NUMBER && FLOAT (Both Numbers Non-Zero)
			case NI_LOR:		// NUMBER || FLOAT (One or Both Numbers Non-Zero)
			case NI_LXOR:		// NUMBER ^^ FLOAT (Only One Number Non-Zero)
				return nibtype_bool;
			}
			break;
		case NST_CHAR:		// NUMBER op CHAR
			switch(op)
			{
			case NI_MULT:		// Cloning
				return nibtype_string;

			case NI_EQ:			// NUMBER == NUMBER (Equality)
			case NI_NEQ:		// NUMBER != NUMBER (Inequality)
			case NI_LT:			// NUMBER < NUMBER (Less Than)
			case NI_LE:			// NUMBER <= NUMBER (Less Than or Equal)
			case NI_GT:			// NUMBER > NUMBER (Greater Than)
			case NI_GE:			// NUMBER >= NUMBER (Greater Than or Equal)
			case NI_LAND:		// NUMBER && NUMBER (Both Numbers Non-Zero)
			case NI_LOR:		// NUMBER || NUMBER (One or Both Numbers Non-Zero)
			case NI_LXOR:		// NUMBER ^^ NUMBER (Only One Number Non-Zero)
				return nibtype_bool;
			}
			break;
		case NST_STRING:	// NUMBER op STRING
			if(op == NI_ADD || op == NI_MULT) return nibtype_string;
			break;

		case NST_BOOLEAN:	// NUMBER op BOOLEAN
			switch(op)
			{
			case NI_LAND:
			case NI_LOR:
			case NI_LXOR:
				return nibtype_bool;
			}
			break;

		case NST_FLAG:		// NUMBER op FLAG
			switch(op)
			{
			case NI_BAND:		// NUMBER & FLAG (Bitwise AND)
			case NI_BOR:		// NUMBER | FLAG (Bitwise OR)
			case NI_BXOR:		// NUMBER ^ FLAG (Bitwise XOR)
				return right;

			case NI_EQ:			// NUMBER == FLAG (Equality)
			case NI_NEQ:		// NUMBER != FLAG (Inequality)
			case NI_LAND:		// NUMBER && FLAG (Both Numbers Non-Zero)
			case NI_LOR:		// NUMBER || FLAG (One or Both Numbers Non-Zero)
			case NI_LXOR:		// NUMBER ^^ FLAG (Only One Number Non-Zero)
				return nibtype_bool;
			}
		}
		break;

	case NST_FLOAT:		// FLOAT op ???
		switch(rhs)
		{
		case NST_NUMBER:	// FLOAT op NUMBER
		case NST_FLOAT:		// FLOAT op FLOAT
			switch(op)
			{
			case NI_ADD:		// FLOAT + NUMBER (Addition)
			case NI_SUBT:		// FLOAT - NUMBER (Subtraction)
			case NI_MULT:		// FLOAT * NUMBER (Multiplication)
			case NI_DIV:		// FLOAT / NUMBER (Division)
				return nibtype_float;

			case NI_EQ:			// FLOAT == NUMBER (Equality)
			case NI_NEQ:		// FLOAT != NUMBER (Inequality)
			case NI_LT:			// FLOAT < NUMBER (Less Than)
			case NI_LE:			// FLOAT <= NUMBER (Less Than or Equal)
			case NI_GT:			// FLOAT > NUMBER (Greater Than)
			case NI_GE:			// FLOAT >= NUMBER (Greater Than or Equal)
			case NI_LAND:		// FLOAT && NUMBER (Both Non-Zero)
			case NI_LOR:		// FLOAT || NUMBER (One or Both Non-Zero)
			case NI_LXOR:		// FLOAT ^^ NUMBER (Only One Non-Zero)
				return nibtype_bool;
			}

			break;
		}
		break;

	case NST_CHAR:		// CHAR op ???
		switch(rhs)
		{
		case NST_NUMBER:	// CHAR op NUMBER
			switch(op)
			{
			case NI_MULT:		// CHAR * NUMBER (Cloning)
				return nibtype_string;

			case NI_EQ:			// CHAR == NUMBER (Equality)
			case NI_NEQ:		// CHAR != NUMBER (Inequality)
			case NI_LT:			// CHAR < NUMBER (Less Than)
			case NI_LE:			// CHAR <= NUMBER (Less Than or Equal)
			case NI_GT:			// CHAR > NUMBER (Greater Than)
			case NI_GE:			// CHAR >= NUMBER (Greater Than or Equal)
			case NI_LAND:		// CHAR && NUMBER (Both Numbers Non-Zero)
			case NI_LOR:		// CHAR || NUMBER (One or Both Numbers Non-Zero)
			case NI_LXOR:		// CHAR ^^ NUMBER (Only One Number Non-Zero)
				return nibtype_bool;
			}
			break;
		case NST_CHAR:		// CHAR op CHAR
			switch(op)
			{
			case NI_ADD:		// CHAR + CHAR (Concatenation)
				return nibtype_string;

			case NI_EQ:			// CHAR == CHAR (Equality)
			case NI_NEQ:		// CHAR != CHAR (Inequality)
			case NI_LT:			// CHAR < CHAR (Less Than)
			case NI_LE:			// CHAR <= CHAR (Less Than or Equal)
			case NI_GT:			// CHAR > CHAR (Greater Than)
			case NI_GE:			// CHAR >= CHAR (Greater Than or Equal)
			case NI_LAND:		// CHAR && CHAR (Both Numbers Non-Zero)
			case NI_LOR:		// CHAR || CHAR (One or Both Numbers Non-Zero)
			case NI_LXOR:		// CHAR ^^ CHAR (Only One Number Non-Zero)
				return nibtype_bool;
			}
			
			break;
		case NST_STRING:	// CHAR op STRING
			if (op == NI_ADD)		// CHAR + STRING (Concatenation)
				return nibtype_string;
			break;
		}
		break;

	case NST_STRING:	// STRING op ???
		switch(rhs)
		{
		case NST_NUMBER:	// STRING op NUMBER
			if (op == NI_ADD ||		// STRING + NUMBER (Concatenation)
				op == NI_MULT)		// STRING * NUMBER (Cloning)
					return nibtype_string;
			break;

		case NST_CHAR:		// STRING op CHAR
			if (op == NI_ADD)		// STRING + CHAR (Concatenation)
				return nibtype_string;
			break;

		case NST_STRING:	// STRING op STRING
			switch(op)
			{
			case NI_ADD:			// STRING + STRING (Concatenation)
				return nibtype_string;

			case NI_EQ:				// STRING == STRING (Equality, insensitive)
			case NI_NEQ:			// STRING != STRING (Inequality, insensitive)
			case NI_LT:				// STRING < STRING (case insensitive comparison)
			case NI_LE:				// STRING <= STRING (case insensitive comparison)
			case NI_GT:				// STRING > STRING (case insensitive comparison)
			case NI_GE:				// STRING >= STRING (case insensitive comparison)
			case NI_STR_PREFIX:		// STRING <~ STRING (String Prefix, insensitive)
			case NI_STR_INFIX:		// STRING ~~ STRING (String Infix/Contains, insensitive)
			case NI_STR_SUFFIX:		// STRING ~> STRING (String Suffix, insensitive)
			case NI_LAND:			// STRING && STRING (Both Strings Non-Empty)
			case NI_LOR:			// STRING || STRING (One or Both Strings Non-Empty)
			case NI_LXOR:			// STRING ^^ STRING (Only One String Non-Empty)
				return nibtype_bool;
			}
			break;

		case NST_NULL:		// STRING op NULL (== and != allowed)
			if (op == NI_EQ ||		// STRING == null (String is NULL)
				op == NI_NEQ)		// STRING != null (String is not NULL)
				return nibtype_bool;
			break;
		default:
			if(rhs != NST_LIST)
			{
				if (op == NI_ADD)
					return nibtype_string;
			}
		}
		break;

	case NST_FLAG:		// FLAG op ???
		switch(rhs)
		{
		case NST_FLAG:		// FLAG op FLAG
			// Flags *must* be compatible first
			if (!are_nib_types_equal(left, right))
				return NULL;

		case NST_NUMBER:	// FLAG op NUMBER
			switch(op)
			{
			case NI_BAND:
			case NI_BOR:
			case NI_BXOR:
				return left;

			case NI_EQ:
			case NI_NEQ:
				return nibtype_bool;
			}
			break;
		}
		break;

	// Everything below can only do boolean operations
	case NST_WIDEVNUM:
		switch(rhs)
		{
		case NST_WIDEVNUM:
		case NST_DUNGEON:
		case NST_INSTANCE:
		case NST_MOBILE:
		case NST_OBJECT:
		// case NST_QUEST:
		case NST_ROOM:
		case NST_SHIP:
		case NST_TOKEN:
		case NST_NULL:
			if (op == NI_EQ ||	// WIDEVNUM == null (WIDEVNUM is empty)
				op == NI_NEQ)	// WIDEVNUM != null (WIDEVNUM is not empty)
				return nibtype_bool;
			break;
		}
		break;
	case NST_BOOLEAN:	// BOOLEAN op ???
		switch(rhs)
		{
		case NST_NUMBER:		// BOOLEAN op NUMBER
			switch(op)
			{
			case NI_LAND:		// BOOLEAN && NUMBER (Both True)
			case NI_LOR:		// BOOLEAN || NUMBER (One or Both True)
			case NI_LXOR:		// BOOLEAN ^^ NUMBER (Only One True)
				return nibtype_bool;
			}
			break;
		case NST_BOOLEAN:		// BOOLEAN op BOOLEAN
			switch(op)
			{
			case NI_EQ:			// BOOLEAN == BOOLEAN (Equality)
			case NI_NEQ:		// BOOLEAN != BOOLEAN (Inequality)
			case NI_LAND:		// BOOLEAN && BOOLEAN (Both True)
			case NI_LOR:		// BOOLEAN || BOOLEAN (One or Both True)
			case NI_LXOR:		// BOOLEAN ^^ BOOLEAN (Only One True)
				return nibtype_bool;
			}
			break;
		}
		break;

	case NST_STAT:		// STAT op ???
		switch(rhs)
		{
		case NST_STAT:		// STAT op STAT
			// For stat comparisons to work, they must be the same stat type
			if (!are_nib_types_equal(left, right))
				return NULL;

		case NST_NUMBER:	// STAT op NUMBER
			switch(op)
			{
			case NI_EQ:
			case NI_NEQ:
			case NI_LT:
			case NI_LE:
			case NI_GT:
			case NI_GE:
				return nibtype_bool;
			}
			break;
		}
		break;

	__bool_ent(ACCOUNT)		// ACCOUNT op ???
	__bool_ent(AFFECT)		// AFFECT op ???
	__bool_ent(AREA)		// AREA op ???
	// __bool_ent(CHANNEL)
	__bool_ent(CLASS)
	__bool_ent_wnum(DUNGEON)
	__bool_ent(EXIT)
	__bool_ent_wnum(INSTANCE)
	__bool_ent(LIQUID)
	__bool_ent(MAIL)
	__bool_ent(MATERIAL)
	__bool_ent(MISSION)
	__bool_ent_wnum(MOBILE)
	__bool_ent(NOTE)
	__bool_ent_wnum(OBJECT)
	__bool_ent(ORG)
	// __bool_ent_wnum(QUEST)
	__bool_ent(RACE)
	__bool_ent(RANK)
	__bool_ent(REPUTATION)
	__bool_ent_wnum(ROOM)
	__bool_ent_wnum(SHIP)
	__bool_ent(SKILL)
	__bool_ent_wnum(TOKEN)
	__bool_ent(WILDS)
	// __bool_ent(WORLD)
	case NST_NULL:
		switch(rhs)
		{
		case NST_STRING:
		case NST_WIDEVNUM:		// will check if the whole thing is "empty"
		case NST_ACCOUNT:
		case NST_AFFECT:
		case NST_AREA:
		// case NST_CHANNEL:
		case NST_CLASS:
		case NST_DUNGEON:
		case NST_EXIT:
		case NST_INSTANCE:
		case NST_LIQUID:
		case NST_MAIL:
		case NST_MATERIAL:
		case NST_MISSION:
		case NST_MOBILE:
		case NST_NOTE:
		case NST_OBJECT:
		case NST_ORG:
		// case NST_QUEST:
		case NST_RACE:
		case NST_RANK:
		case NST_REPUTATION:
		case NST_ROOM:
		case NST_SHIP:
		case NST_SKILL:
		case NST_TOKEN:
		case NST_WILDS:
		// case NST_WORLD:
			if (op == NI_EQ ||	// entity == null (entity is null)
				op == NI_NEQ)	// entity != null (entity is not null)
				return nibtype_bool;
			break;
		}
		break;
	}

	return NULL;
}

// reference the implementation provided in Lexer.l
LLIST *nib_create_string_list();

int niberror(const char *msg) {
    printf("error(%d): %s\n", niblineno, msg);
    return 0;
}

void niberrorf(const char *msg, ...)
{
    va_list va;
    char buff[5120];

    va_start(va, msg);
    vsprintf(buff, msg, va);
    va_end(va);

    niberror(buff);
}


%}
%locations

/*
%code requires {
  typedef void* yyscan_t;
}
*/

%output  "yacc/parser.c"
%defines "yacc/parser.h"

%define api.pure
%define api.prefix {nib}

%token T_ACCOUNT
%token T_AFFECT
%token T_AREA
%token T_ARRAY
%token T_ARROW
%token T_ASSIGN
%token T_BAND
%token T_BNOT
%token T_BOOLEAN
%token T_BOR
%token T_BXOR
%token T_BREAK
%token T_CASE
%token T_CHANNEL
%token T_CHAR
%token T_CHAR_LITERAL
%token T_CLASS
%token T_CLOSE_BANK
%token T_CLOSE_BRACE
%token T_CLOSE_BRACKET
%token T_CLOSE_FLAG
%token T_CLOSE_LIST
%token T_CLOSE_MAP
%token T_CLOSE_PAREN
%token T_COALESCE
%token T_COLON
%token T_COLONS
%token T_COMMA
%token T_CONSTANT
%token T_CONTINUE
%token T_DECREMENT
%token T_DEFAULT
%token T_DIVIDE
%token T_DO
%token T_DOT
%token T_DUNGEON
%token T_ELSE
%token T_EQUAL
%token T_EXIT
%token T_EXPONENT
%token T_FALSE
%token T_FLAG
%token T_FLAGBANK
%token T_FLOAT
%token T_FLOAT_NUMBER
%token T_FOR
%token T_FOREACH
%token T_GAME
%token T_GLOBAL
%token T_GT
%token T_GT_EQUAL
%token T_IDENTIFIER
%token T_IF
%token T_IN
%token T_INCREMENT
%token T_INSTANCE
%token T_INT
%token T_LAND
%token T_LEFT_SHIFT
%token T_LIST
%token T_LIQUID
%token T_LNOT
%token T_LOR
%token T_LT
%token T_LT_EQUAL
%token T_LXOR
%token T_MAIL
%token T_MAP
%token T_MATERIAL
%token T_MINUS
%token T_MISSION
%token T_MOBILE
%token T_MOD
%token T_NOT_EQUAL
%token T_NOTE
%token T_NULL
%token T_NUMBER
%token T_OBJECT
%token T_OPEN_BANK
%token T_OPEN_BRACE
%token T_OPEN_BRACKET
%token T_OPEN_FLAG
%token T_OPEN_LIST
%token T_OPEN_MAP
%token T_OPEN_PAREN
%token T_ORG
%token T_PARSE_ERROR
%token T_PLUS
%token T_QMARK
%token T_QUEST
%token T_RACE
%token T_RANGE
%token T_RANK
%token T_REPUTATION
%token T_RETURN
%token T_RIGHT_SHIFT
%token T_RIGHTL_SHIFT
%token T_ROOM
%token T_SELF
%token T_SEMICOLON
%token T_SHIP
%token T_SIZEOF
%token T_SKILL
%token T_STAR
%token T_STAT
%token T_STR_PREFIX
%token T_STR_INFIX
%token T_STR_SUFFIX
%token T_STRING
%token T_STRING_LITERAL
%token T_SUBSETOF
%token T_SWITCH
%token T_TABLE
%token T_TOKEN
%token T_TRUE
%token T_TYPEOF
%token T_VARARGS
%token T_WHILE
%token T_WIDEVNUM
%token T_WIDEVNUM_DELIM
%token T_WILDS
%token T_WORLD

%union {
	bool b;
	utf8char_t ch;
    long number;
	uintptr_t address;
	nib_bytecode_t assign;	// Opcode for assignment
	flag_value_t flags;
	NIB_TYPE *nibtype;
	const struct flag_type *flag_table;
	const struct flag_type **flag_bank;

	struct for_intr_exp_s {
		nib_bytecode_p data;
		size_t len;
	} for_intr_exp;

	struct for_cond_expr {
		bool empty;
		long address;
	} for_cond_expr;

	struct foreach_s {
		NIB_VARIABLE *var;		// Loop variable
		long address;
		nib_bytecode_t stop;
	} foreach;

	struct {
		bool empty;
		uintptr_t address;
	} opt_else;

	struct {
		bool global;
		bool constant;
	} modifiers;

	enum nib_indexing_mode_e index_range;

	struct nib_compile_switch_case_s *case_label;

	struct decl_s 
	{
		NIB_TYPE *type;
		bool global;
		bool constant;
		nib_bytecode_t lvalue;
		short id;
	} decl;

	LLIST *string_list;
	LLIST *type_list;
	double float_number;
	char *literal;
	char *identifier;
	char *str;				// internal string literal

	struct lvalue_s lvalue;

    struct rvalue_s rvalue;

	struct lrvalue_s lrvalue;


    struct
    {
		NIB_TYPE *type;
		bool might_lvalue;
		bool needs_use;
		bool needs_pop;
		int flags;
    } function_call_result;

	NIB_STATEMENT statement;

    struct
    {
        NIB_STATEMENT statements;
        bool has_default;
    } switch_block;

}

%nonassoc LOWER_THAN_ELSE
%nonassoc T_ELSE

%destructor { free_nib_type($$); } <nibtype>
%destructor { nib_free($$); } <literal>
%destructor { nib_free($$); } <identifier>
%destructor { list_destroy($$); } <string_list>
%destructor { list_destroy($$); } <type_list>
%destructor { free_nib_compile_switch_case($$); } <case_label>

%type <number> T_NUMBER constant
%type <ch> T_CHAR_LITERAL
%type <float_number> T_FLOAT_NUMBER float_constant
%type <literal> T_STRING_LITERAL
%type <identifier> T_IDENTIFIER table_name
%type <nibtype> type listtype cast
%type <string_list> comma_name_list flag_name_list
%type <flags> flag_number_list comma_bit_list
%type <modifiers> possible_modifiers
%type <b> boolean_value possible_constant
%type <lrvalue> expr0 expr4 field_call widevnum_value for_cond_expr index_value
%type <lvalue> lvalue name_lvalue
%type <rvalue> comma_expr
%type <index_range> index_range
%type <function_call_result> function_call method_call
%type <type_list> argument_list optional_argument_list
%type <flag_table> flag_table stat_table
%type <flag_bank> flag_bank
// Contains the opcode used for the assignment
%type <assign> T_ASSIGN
%type <statement> statement_block statement cond for foreach do while switch
%type <case_label> case_label

%right T_ASSIGN
%right T_QMARK T_COALESCE
%left T_LOR
%left T_LXOR
%left T_LAND
%left T_BOR
%left T_BXOR
%left T_BAND
%left T_EQUAL T_NOT_EQUAL T_IDENTIFIER
%left T_LT T_LT_EQUAL T_GT T_GT_EQUAL
%left T_LEFT_SHIFT T_RIGHT_SHIFT T_RIGHTL_SHIFT T_STR_PREFIX T_STR_INFIX T_STR_SUFFIX
%left T_PLUS T_MINUS
%left T_STAR T_DIVIDE T_MOD
%right T_BNOT T_LNOT
%nonassoc T_INCREMENT T_DECREMENT
%left T_OPEN_PAREN
%left T_DOT T_ARROW T_OPEN_BRACKET

%right T_STRING_LITERAL

%%

all:
		{
			last_expression = -1;
		}
		program
	;

program: program statement
	| /* empty */
	;

def:	name_list T_SEMICOLON
		{
			free_nib_type($<decl>1.type);
		}
	|	table_def T_SEMICOLON
	;

table_def:
		T_TABLE T_FLAG T_OPEN_PAREN flag_name_list[L] T_CLOSE_PAREN T_IDENTIFIER[I]
		{
			const struct flag_type *table = nib_lookup_flag_table(nib_flag_created_tables,$I);
			if (table)
			{
				niberrorf("Duplicate flag table '%s' defined", $I);
				YYERROR;
			}

			if (list_size($L) < 1)
			{
				yyerror("Please specify at least one named flag.");
				YYERROR;
			}

			if (list_size($L) > MAX_FLAG_BITS)
			{
				niberrorf("Named flags only support up to %d names.", MAX_FLAG_BITS);
				YYERROR;
			}
			
			// Create flag table
			nib_flag_add_table($L, $I);
			nib_free($I);
			list_destroy($L);
		}
	|	T_TABLE T_STAT T_OPEN_PAREN flag_name_list[L] T_CLOSE_PAREN T_IDENTIFIER[I]
		{
			const struct flag_type *table = nib_lookup_stat_table(nib_stat_created_tables,$I);
			if (table)
			{
				niberrorf("Duplicate stat table '%s' defined", $I);
				YYERROR;
			}

			if (list_size($L) < 1)
			{
				yyerror("Please specify at least one named stat.");
				YYERROR;
			}

			// Create stat table
			nib_stat_add_table($L, $I);
			nib_free($I);
			list_destroy($L);
		}
	;

name_list:
		possible_modifiers[M] type[T] T_IDENTIFIER[I]
		{
			if ($M.global)
			{
				if (nib_get_max_scope() > 0)
				{
					yyerror("Global variables may only be declared at the beginning.");
					YYERROR;
				}

				// Global declaration.
				// There is no initialization involved within the script,
				//   that is done on the script itself.
				if (nib_get_global_variable($I))
				{
					niberrorf("Duplicate global variable '%s'", $I);
					YYERROR;
				}

				// Determine if the name exists *on* the script
				pVARIABLE _var = variable_get($I);
				if (!_var)
				{
					niberrorf("Global variable '%s' not registered on script.", $I);
					YYERROR;
				}

				// Make sure the types match!
				if (!is_valid_variable_type(_var, $T))
				{
					niberrorf("Type mismatch with global variable '%s'.", $I);
					YYERROR;
				}

				NIB_VARIABLE *var = nib_new_variable($I, $T, NIB_GLOBAL_SCOPE, _var->readonly);
				nib_add_global_variable(var);

				$<decl>$.type = $T;
			}
			else
			{
				// Local declaration (no initialization)

				if (nib_get_global_variable($I))
				{
					niberrorf("Attempting to shadow a global variable '%s'", $I);
					YYERROR;
				}

				NIB_VARIABLE *var;

				var = nib_get_local_variable($I);
				if (var && var->scope == nib_get_scope())
				{
					niberrorf("Redefinition of local variable '%s' within same scope.", $I);
					YYERROR;
				}
				else
				{
					var = nib_new_variable($I, $T, nib_get_scope(), $M.constant);
					nib_add_local_variable(var);

					$<decl>$.type = $T;
				}
			}

			$<decl>$.global = $M.global;
			$<decl>$.constant = $M.constant;
			nib_free($I);
			// printf("%d, %d: $<decl>$.type = %p\n", __LINE__, niblineno, $<decl>$.type);
		}
	|	possible_modifiers[M] type[T] T_IDENTIFIER[I] T_ASSIGN[A]
		{
			if ($M.global)
			{
				if (nib_get_max_scope() > 0)
				{
					yyerror("Global variables may only be declared at the beginning.");
					YYERROR;
				}

				// Global declaration.
				// There is no initialization involved within the script,
				//   that is done on the script itself.
				if (nib_get_global_variable($I))
				{
					niberrorf("Duplicate global variable '%s'", $I);
					YYERROR;
				}

				// Determine if the name exists *on* the script
				pVARIABLE _var = variable_get($I);
				if (!_var)
				{
					niberrorf("Global variable '%s' not registered on script.", $I);
					YYERROR;
				}

				// Make sure the types match!
				if (!is_valid_variable_type(_var, $T))
				{
					niberrorf("Type mismatch with global variable '%s'.", $I);
					YYERROR;
				}

				NIB_VARIABLE *var = nib_new_variable($I, $T, NIB_GLOBAL_SCOPE, _var->readonly);
				nib_add_global_variable(var);

				ins_code(NI_LVALUE_GLOBAL);
				ins_short(var->id);
				$<decl>$.type = $T;
			}
			else
			{
				// Local declaration (no initialization)

				if (nib_get_global_variable($I))
				{
					niberrorf("Attempting to shadow a global variable '%s'", $I);
					YYERROR;
				}

				NIB_VARIABLE *var;

				var = nib_get_local_variable($I);
				if (var && var->scope == nib_get_scope())
				{
					niberrorf("Redefinition of local variable '%s' within same scope.", $I);
					YYERROR;
				}
				else
				{
					var = nib_new_variable($I, $T, nib_get_scope(), $M.constant);
					nib_add_local_variable(var);

					ins_code(NI_LVALUE_LOCAL);
					ins_short(var->id);
					$<decl>$.type = $T;
				}
			}

			$<decl>$.global = $M.global;
			$<decl>$.constant = $M.constant;
		}
		expr0[E]
		{
			if ($A != NI_ASSIGN)
			{
				yyerror("Variable declarations only allow assignment (=).");
				YYERROR;
			}
			// Do some initialization

			if (!check_valid_assignment($T,$E.type,NI_ASSIGN))
			{
				yyerror("Right hand value not value for left hand lvalue.");
				YYERROR;
			}

			last_expression = CURRENT_PROGRAM_SIZE;
			ins_code(NI_VOID_ASSIGN);

			$<decl>$ = $<decl>5;
			nib_free($I);
			free_nib_type($E.type);

			// printf("%d, %d: $<decl>$.type = %p\n", __LINE__, niblineno, $<decl>$.type);
		}
	|	name_list[L] T_COMMA T_IDENTIFIER[I]
		{
			if ($<decl>L.global)
			{
				// Global declaration.
				// There is no initialization involved within the script,
				//   that is done on the script itself.

				if (nib_get_global_variable($I))
				{
					niberrorf("Duplicate global variable '%s'", $I);
					YYERROR;
				}

				// Determine if the name exists *on* the script
				pVARIABLE _var = variable_get($I);
				if (!_var)
				{
					niberrorf("Global variable '%s' not registered on script.", $I);
					YYERROR;
				}

				// Make sure the types match!
				if (!is_valid_variable_type(_var, $<decl>L.type))
				{
					niberrorf("Type mismatch with global variable '%s'.", $I);
					YYERROR;
				}

				NIB_VARIABLE *var = nib_new_variable($I, $<decl>L.type, NIB_GLOBAL_SCOPE, _var->readonly);
				nib_add_global_variable(var);
			}
			else
			{
				// Local declaration (no initialization)
				if (nib_get_global_variable($I))
				{
					niberrorf("Attempting to shadow a global variable '%s'", $I);
					YYERROR;
				}
				else
				{
					NIB_VARIABLE *var;

					var = nib_get_local_variable($I);
					if (var && var->scope == nib_get_scope())
					{
						niberrorf("Redefinition of local variable '%s' within same scope.", $I);
						YYERROR;
					}
					else
					{
						var = nib_new_variable($I, $<decl>L.type, nib_get_scope(), $<decl>L.constant);
						nib_add_local_variable(var);
					}
				}
			}

			$<decl>$ = $<decl>L;
			nib_free($I);
			// printf("%d, %d: $<decl>$.type = %p\n", __LINE__, niblineno, $<decl>$.type);
		}
	|	name_list[L] T_COMMA T_IDENTIFIER[I] T_ASSIGN[A]
		{
			$<decl>$ = $<decl>L;

			if ($<decl>L.global)
			{
				// Global declaration.
				// There is no initialization involved within the script,
				//   that is done on the script itself.

				if (nib_get_global_variable($I))
				{
					niberrorf("Duplicate global variable '%s'", $I);
					YYERROR;
				}

				// Determine if the name exists *on* the script
				pVARIABLE _var = variable_get($I);
				if (!_var)
				{
					niberrorf("Global variable '%s' not registered on script.", $I);
					YYERROR;
				}

				// Make sure the types match!
				if (!is_valid_variable_type(_var, $<decl>L.type))
				{
					niberrorf("Type mismatch with global variable '%s'.", $I);
					YYERROR;
				}

				NIB_VARIABLE *var = nib_new_variable($I, $<decl>L.type, NIB_GLOBAL_SCOPE, _var->readonly);
				nib_add_global_variable(var);

				ins_code(NI_LVALUE_GLOBAL);
				ins_short(var->id);
			}
			else
			{
				// Local declaration (no initialization)
				if (nib_get_global_variable($I))
				{
					niberrorf("Attempting to shadow a global variable '%s'", $I);
					YYERROR;
				}
				else
				{
					NIB_VARIABLE *var;

					var = nib_get_local_variable($I);
					if (var && var->scope == nib_get_scope())
					{
						niberrorf("Redefinition of local variable '%s' within same scope.", $I);
						YYERROR;
					}
					else
					{
						var = nib_new_variable($I, $<decl>L.type, nib_get_scope(), $<decl>L.constant);
						nib_add_local_variable(var);

						ins_code(NI_LVALUE_LOCAL);
						ins_short(var->id);
					}
				}
			}

		}
		expr0[E]
		{
			// Do some initialization
			if (!check_valid_assignment($<decl>L.type,$E.type,$A))
			{
				yyerror("Right hand value not value for left hand lvalue.");
				YYERROR;
			}

			last_expression = CURRENT_PROGRAM_SIZE;
			ins_code(NI_VOID_ASSIGN);

			$<decl>$ = $<decl>5;
			nib_free($I);
			free_nib_type($E.type);
			// printf("%d, %d: $<decl>$.type = %p\n", __LINE__, niblineno, $<decl>$.type);
		}
	;

possible_modifiers:
		T_GLOBAL
		{
			$$.constant = false;
			$$.global = true;
		}
	|	T_CONSTANT
		{
			$$.constant = true;
			$$.global = false;
		}
	|	/* empty */
		{
			$$.constant = false;
			$$.global = false;
		}
	;

block:	T_OPEN_BRACE statement_block T_CLOSE_BRACE
	;

statement_block:
	{
		nib_push_scope();
	}
	statements
	{
		nib_pop_scope();
	}
	;

statements:
		statements statement
	|	/* empty */
	;

statement:
		comma_expr T_SEMICOLON
		{
			// Do stuff?
			if ($1.needs_pop)
				insert_pop_value();

			free_nib_type($1.type);
		}
	|	def
		{

		}
	|	cond
	|	for
	|	foreach
	|	do
	|	while
	|	switch
	|	T_BREAK T_SEMICOLON
		{
			if (!nib_break_address)
			{
				yyerror("BREAK encountered outside of a for loop or switch statement.");
				YYERROR;
			}

			ins_code(NI_JUMP);
			push_nib_break_statement(CURRENT_PROGRAM_SIZE);
			ins_address(0);
		}
	|	T_CONTINUE T_SEMICOLON
		{
			if (!nib_continue_address)
			{
				yyerror("CONTINUE encountered outside of a for loop.");
				YYERROR;
			}

			ins_code(NI_JUMP);
			push_nib_continue_statement(CURRENT_PROGRAM_SIZE);
			ins_address(0);
		}
	|	T_RETURN T_SEMICOLON
		{
			ins_code(NI_CONST1);
			ins_code(NI_RETURN);
		}
	|	T_RETURN T_DOT T_IDENTIFIER[I] T_SEMICOLON
		{
			nib_bytecode_t ret;
			if (!str_cmp($I,"allow"))
				ret = 0;
			else if (!str_cmp($I,"deny"))
				ret = 1;
			else if (!str_cmp($I,"silent"))
				ret = 2;
			else
			{
				yyerror("RETURN.code only allows \"allow\" (0), \"deny\" (1) and \"silent\" (2).");
				YYERROR;
			}

			ins_code(NI_RETURN_BYTE);
			ins_byte(ret);

			nib_free($I);
		}
	|	T_RETURN expr0[E] T_SEMICOLON
		{
			if ($E.type != nibtype_int)
			{
				yyerror("RETURN only accepts integer values.");
				YYERROR;
			}

			ins_code(NI_RETURN);
		}
	|	block
		{

		}

	|	/* empty */	T_SEMICOLON
		{

		}
	;

cond_start:	T_IF T_OPEN_PAREN expr0[E] T_CLOSE_PAREN
	;

cond:
		cond_start
		{
			ins_code(NI_JUMP_ZERO);
			$<address>$ = CURRENT_PROGRAM_SIZE;
			ins_address(0);
		}
		statement
		{
			ins_code(NI_JUMP);
			$<address>$ = CURRENT_PROGRAM_SIZE;
			ins_address(0);
		}
		optional_else
		{
			if ($<opt_else>5.empty)
			{
				// Actually remove the JUMP from $4
				mem_buffer_prune(nib_program_storage,$<address>4 - 1, 1 + sizeof(int));
				upd_address($<address>2, $<opt_else>5.address - (1 + sizeof(int)));
			}
			else
			{
				upd_address($<address>2, $<opt_else>5.address);
				upd_address($<address>4, CURRENT_PROGRAM_SIZE);
			}
		}
	;

optional_else:
		/* empty */	%prec LOWER_THAN_ELSE
		{
			$<opt_else>$.empty = true;
			$<opt_else>$.address = CURRENT_PROGRAM_SIZE;
		}
	|	T_ELSE
		{
			$<address>$ = CURRENT_PROGRAM_SIZE;
		}
		statement
		{
			$<opt_else>$.empty = false;
			$<opt_else>$.address = $<address>2;
		}
	;

for:	T_FOR T_OPEN_PAREN
		{	// $3
			nib_push_scope();
			nib_script_comment_add(CURRENT_PROGRAM_SIZE, "for:");
		}
		for_init_expr[N] T_SEMICOLON
		{	// $6
			// Configure CONTINUE and BREAK information
			insert_pop_value();

			push_nib_continue_address();
			push_nib_break_address();

			$<address>$ = CURRENT_PROGRAM_SIZE;		// Continue address
			nib_script_comment_add(CURRENT_PROGRAM_SIZE, "for condition:");
		}
		for_cond_expr[C] T_SEMICOLON
		{	// $9
			if (!$C.needs_use)
			{
				yyerror("FOR condition does not have a value to test.");
				YYERROR;
			}

			// Need to detect when this is an empty condition
			if (CURRENT_PROGRAM_SIZE == $<address>6)
			{
				// An empty condition is treated as always true
				$<for_cond_expr>$.address = CURRENT_PROGRAM_SIZE;
				$<for_cond_expr>$.empty = true;
			}
			else
			{
				ins_code(NI_JUMP_ZERO);
				$<for_cond_expr>$.address = CURRENT_PROGRAM_SIZE;
				ins_address(0);

				$<for_cond_expr>$.empty = false;
			}
			last_expression = -1;
		}
		for_iter_expr[P] T_CLOSE_PAREN
		{	// $12
			insert_pop_value();
			last_expression = -1;

			// Pull everything off the program storage from $<for_cond_expr>9+long to end
			int address = $<for_cond_expr>9.address;
			if (!$<for_cond_expr>9.empty)
				address += sizeof(int);
			$<for_intr_exp>$.len = CURRENT_PROGRAM_SIZE - address;
			if ($<for_intr_exp>$.len > 0)
			{
				$<for_intr_exp>$.data = nib_malloc($<for_intr_exp>$.len);
				memcpy($<for_intr_exp>$.data, nib_program_storage->buffer + address, $<for_intr_exp>$.len);
				nib_program_storage->len -= $<for_intr_exp>$.len;
			}
			else
				$<for_intr_exp>$.data = NULL;
		}
		statement
		{
			nib_script_comment_add(CURRENT_PROGRAM_SIZE, "for iteration:");

			// Replace the FOR-iteration code
			ins_bytes($<for_intr_exp>12.data, $<for_intr_exp>12.len);

			ins_code(NI_JUMP);
			ins_address($<address>6);

			nib_script_comment_add(CURRENT_PROGRAM_SIZE, "end for loop:");

			if (!$<for_cond_expr>9.empty)
			{
				set_current_address($<for_cond_expr>9.address);
			}
			update_nib_break_statements(CURRENT_PROGRAM_SIZE);
			update_nib_continue_statements($<address>6);

			pop_nib_break_address();
			pop_nib_continue_address();
			nib_pop_scope();

			free_nib_type($C.type);
			nib_free($<for_intr_exp>12.data);
		}
	;

for_init_expr:
		/* empty */
		{
			last_expression = CURRENT_PROGRAM_SIZE;
			ins_int(1);
		}
	|	comma_expr_decl
	;

comma_expr_decl:
		expr_decl
	|	comma_expr_decl
		{
			insert_pop_value();
		}
		T_COMMA expr_decl
	;

expr_decl:
		expr0
		{

		}
	|	type[T] T_IDENTIFIER[I]
		{
			// Local declaration (no initialization)

			if (nib_get_global_variable($I))
			{
				niberrorf("Attempting to shadow a global variable '%s'", $I);
				YYERROR;
			}

			NIB_VARIABLE *var;

			var = nib_get_local_variable($I);
			if (var && var->scope == nib_get_scope())
			{
				niberrorf("Redefinition of local variable '%s' within same scope.", $I);
				YYERROR;
			}
			else
			{
				var = nib_new_variable($I, $T, nib_get_scope(), false);
				nib_add_local_variable(var);

				$<decl>$.type = $T;
			}

			$<decl>$.global = false;
			$<decl>$.constant = false;
			nib_free($I);
		}
	|	type[T] T_IDENTIFIER[I] T_ASSIGN[A]
		{
			// Local declaration (no initialization)

			if (nib_get_global_variable($I))
			{
				niberrorf("Attempting to shadow a global variable '%s'", $I);
				YYERROR;
			}

			NIB_VARIABLE *var;

			var = nib_get_local_variable($I);
			if (var && var->scope == nib_get_scope())
			{
				niberrorf("Redefinition of local variable '%s' within same scope.", $I);
				YYERROR;
			}
			else
			{
				var = nib_new_variable($I, $T, nib_get_scope(), false);
				nib_add_local_variable(var);

				ins_code(NI_LVALUE_LOCAL);
				ins_short(var->id);
				$<decl>$.type = $T;
			}

			$<decl>$.global = false;
			$<decl>$.constant = false;
		}
		expr0[E]
		{
			if ($A != NI_ASSIGN)
			{
				yyerror("Variable declarations only allow assignment (=).");
				YYERROR;
			}

			if (!check_valid_assignment($T,$E.type,NI_ASSIGN))
			{
				yyerror("Right hand value not value for left hand lvalue.");
				YYERROR;
			}

			last_expression = CURRENT_PROGRAM_SIZE;
			ins_code(NI_VOID_ASSIGN);

			$<decl>$ = $<decl>5;
			nib_free($I);
			free_nib_type($E.type);
		}
	;

for_cond_expr:
		/* empty */
		{
			last_expression = -1;
			// No instructions, will be treated as TRUE

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;

			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
		}
	|	comma_expr
		{
			$$.name = $1.name;
			$$.type = $1.type;
			$$.needs_use = $1.needs_use;
			$$.flags = $1.flags;
			$$.lhs = NULL;
			$$.lhs_len = 0;
			$$.rhs = $1.rhs;
			$$.rhs_len = $1.rhs_len;

			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
		}
	;

for_iter_expr:
		/* empty */
		{
			last_expression = CURRENT_PROGRAM_SIZE;
			ins_int(1);
		}
	|	comma_expr
	;

foreach:
		T_FOREACH
		{
			nib_push_scope();
			nib_script_comment_add(CURRENT_PROGRAM_SIZE, "foreach:");
		}
		T_OPEN_PAREN T_IDENTIFIER[I] T_COLON expr0[E] T_CLOSE_PAREN
		{
			// Configure CONTINUE and BREAK information
			push_nib_continue_address();
			push_nib_break_address();

			// Create a variable in this scope using the element type of the expression
			NIB_TYPE *type = NULL;
			long length = 0;
			nib_bytecode_t codes[3] = {
				NI_ILLEGAL,
				NI_ILLEGAL,
				NI_ILLEGAL
			};
			if ($E.type != NULL)
			{
				// printf("foreach: %p, %p\n", $E.type, $E.type->name);
				// hex_dump($E.type, sizeof(NIB_TYPE));
				// if ($E.type->name)
				// 	hex_dump($E.type->name, 1);
				// printf("foreach: name = %p\n", $E.type->name ? $E.type->name : "<null>");
				// printf("foreach: %s\n", nib_get_typename(NULL,$E.type));
				if ($E.type->type_class == NTC_PRIMARY)
				{
					if ($E.type->_.primary == NT_STRING)
					{
						type = nibtype_char;
						codes[0] = NI_STRINGER_START;
						codes[1] = NI_STRINGER_NEXT;
						codes[2] = NI_STRINGER_STOP;
					}
				}
				else if ($E.type->type_class == NTC_LIST)
				{
					type = $E.type->_.list.type;
					codes[0] = NI_ITER_START;
					codes[1] = NI_ITER_NEXT;
					codes[2] = NI_ITER_STOP;
				}
				else if ($E.type->type_class == NTC_ARRAY)
				{
					type = $E.type->_.array.type;
					length = $E.type->_.array.length;
					codes[0] = NI_INDEXER_START;
					codes[1] = NI_INDEXER_NEXT;
					codes[2] = NI_INDEXER_STOP;
				}
			}

			if (type == NULL || codes[0] == NI_ILLEGAL)
			{
				yyerror("Expecting an iterative type.");
				YYERROR;
			}

			NIB_VARIABLE *var = $<foreach>$.var = nib_new_variable($I, type, nib_get_scope(), true);
			nib_add_local_variable(var);

			// Initialize iterator
			ins_code(codes[0]);	// Messes with the list *on* the stack

			// Start loop
			ins_code(codes[1]);
			$<foreach>$.address = CURRENT_PROGRAM_SIZE;
			ins_address(0);
			ins_short(var->id);

			$<foreach>$.stop = codes[2];
		}
		statement
		{
			// Update CONTINUE and BREAK instructions

			ins_byte(NI_JUMP);
			ins_address($<foreach>8.address - sizeof(nib_bytecode_t));

			update_nib_break_statements(CURRENT_PROGRAM_SIZE);
			update_nib_continue_statements($<foreach>8.address);

			set_current_address($<foreach>8.address);

			ins_code($<foreach>8.stop);	// Close out the current list

			// Close scope
			nib_pop_scope();

			pop_nib_break_address();
			pop_nib_continue_address();

			nib_free($I);
			free_nib_type($E.type);
		}
	;

while:
		T_WHILE T_OPEN_PAREN
		{
			nib_script_comment_add(CURRENT_PROGRAM_SIZE, "while:");

			push_nib_continue_address();
			push_nib_break_address();

			$<address>$ = CURRENT_PROGRAM_SIZE;
		}
		expr0[C] T_CLOSE_PAREN
		{
			// Verify that the expression has a value to look at?
			if (!$C.needs_use)
			{
				yyerror("WHILE condition does not have a value to test.");
				YYERROR;
			}


			// Configure CONTINUE and BREAK information
			ins_code(NI_JUMP_ZERO);
			$<address>$ = CURRENT_PROGRAM_SIZE;
			ins_address(0);
		}
		statement[S]
		{
			// Update CONTINUE and BREAK instructions

			ins_code(NI_JUMP);
			ins_address($<address>3);

			set_current_address($<address>6);

			update_nib_break_statements(CURRENT_PROGRAM_SIZE);
			update_nib_continue_statements($<address>3);

			pop_nib_break_address();
			pop_nib_continue_address();
		}
	;

do:
		T_DO
		{
			// Configure CONTINUE and BREAK information
		}
		statement T_WHILE T_OPEN_PAREN expr0 T_CLOSE_PAREN
		{
			// Update CONTINUE and BREAK instructions
		}
	;

switch:
		T_SWITCH T_OPEN_PAREN expr0[E] T_CLOSE_PAREN
		{
			const struct flag_type *table;
			enum nib_switch_type_e type = check_valid_switch_type($E.type,&table);
			if(type == NSWT_UNKNOWN)
			{
				yyerror("Invalid expression type used in switch statement.");
				YYERROR;
			}

			if (list_size(nib_switch_blocks) >= 0x7FFF)
			{
				yyerror("Too many switch statements declared.");
				YYERROR;
			}

			if (!push_nib_switch_block(type, table))
			{
				yyerror("Error generating switch block.");
				YYERROR;
			}

			ins_code(NI_SWITCH);
			ins_short(nib_current_switch->id);
			// Add jump address in case there are no matching cases
			$<address>$ = CURRENT_PROGRAM_SIZE;
			ins_address(0);

			push_nib_break_address();
		}
		T_OPEN_BRACE switch_block T_CLOSE_BRACE
		{
			if (list_size(nib_current_switch->cases) < 1 && !nib_current_switch->has_default)
			{
				yyerror("Switch has no cases.");
				YYERROR;
			}

			upd_address($<address>5,CURRENT_PROGRAM_SIZE);

			update_nib_break_statements(CURRENT_PROGRAM_SIZE);

			pop_nib_break_address();
			pop_nib_switch_block();
		}
	;

switch_block:
		switch_block switch_statement
	|	switch_statement
	;

switch_statement:
		case
	|	default
	|	statement
	;

case:
		T_CASE case_label[C] T_COLON
		{
			if (nib_switch_overlaps_case($C))
			{
				free_nib_compile_switch_case($C);
				yyerror("Duplicate or overlapping case in switch statement.");
				YYERROR;
			}

			$C->address = CURRENT_PROGRAM_SIZE;
			nib_switch_add_case($C);
		}
	;

case_label:
		constant[A]
		{
			if (nib_current_switch->type != NSWT_NUMBER)
			{
				yyerror("NUMBER case label used in a non-NUMBER switch.");
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_VALUE;
			cs->a.number = $A;

			$$ = cs;
		}
	|	constant[A] T_RANGE
		{
			if (nib_current_switch->type != NSWT_NUMBER)
			{
				yyerror("NUMBER case label used in a non-NUMBER switch.");
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_VX;
			cs->a.number = $A;

			$$ = cs;
		}
	|	T_RANGE constant[B]
		{
			if (nib_current_switch->type != NSWT_NUMBER)
			{
				yyerror("NUMBER case label used in a non-NUMBER switch.");
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_XV;
			cs->b.number = $B;

			$$ = cs;
		}
	|	constant[A] T_RANGE constant[B]
		{
			if (nib_current_switch->type != NSWT_NUMBER)
			{
				yyerror("NUMBER case label used in a non-NUMBER switch.");
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_VV;
			cs->a.number = $A;
			cs->b.number = $B;

			$$ = cs;
		}
	|	float_constant[A]
		{
			if (nib_current_switch->type != NSWT_FLOAT)
			{
				yyerror("FLOAT case label used in a non-FLOAT switch.");
				YYERROR;
			}


			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_VALUE;
			cs->a.flt = $A;

			$$ = cs;
		}
	|	float_constant[A] T_RANGE
		{
			if (nib_current_switch->type != NSWT_FLOAT)
			{
				yyerror("FLOAT case label used in a non-FLOAT switch.");
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_VX;
			cs->a.flt = $A;

			$$ = cs;
		}
	|	T_RANGE float_constant[B]
		{
			if (nib_current_switch->type != NSWT_FLOAT)
			{
				yyerror("FLOAT case label used in a non-FLOAT switch.");
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_XV;
			cs->b.flt = $B;

			$$ = cs;
		}
	|	float_constant[A] T_RANGE float_constant[B]
		{
			if (nib_current_switch->type != NSWT_FLOAT)
			{
				yyerror("FLOAT case label used in a non-FLOAT switch.");
				YYERROR;
			}


			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_VV;
			cs->a.flt = $A;
			cs->b.flt = $B;

			$$ = cs;
		}
	|	T_CHAR_LITERAL[A]
		{
			if (nib_current_switch->type != NSWT_CHAR)
			{
				yyerror("CHAR case label used in a non-CHAR switch.");
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_VALUE;
			cs->a.ch = $A;

			$$ = cs;
		}
	|	T_CHAR_LITERAL[A] T_RANGE
		{
			if (nib_current_switch->type != NSWT_CHAR)
			{
				yyerror("CHAR case label used in a non-CHAR switch.");
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_VX;
			cs->a.ch = $A;

			$$ = cs;
		}
	|	T_RANGE T_CHAR_LITERAL[B]
		{
			if (nib_current_switch->type != NSWT_CHAR)
			{
				yyerror("CHAR case label used in a non-CHAR switch.");
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_XV;
			cs->b.ch = $B;

			$$ = cs;
		}
	|	T_CHAR_LITERAL[A] T_RANGE T_CHAR_LITERAL[B]
		{
			if (nib_current_switch->type != NSWT_CHAR)
			{
				yyerror("CHAR case label used in a non-CHAR switch.");
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_VV;
			cs->a.ch = $A;
			cs->b.ch = $B;

			$$ = cs;
		}
	|	T_STRING_LITERAL[A]
		{
			if (nib_current_switch->type != NSWT_STRING)
			{
				yyerror("STRING case label used in a non-STRING switch.");
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_VALUE;
			cs->a.str = $A;

			$$ = cs;
		}
	|	T_STRING_LITERAL[A] T_RANGE
		{
			if (nib_current_switch->type != NSWT_STRING)
			{
				yyerror("STRING case label used in a non-STRING switch.");
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_PREFIX;
			cs->a.str = $A;

			$$ = cs;
		}
	|	T_RANGE T_STRING_LITERAL[A]
		{
			if (nib_current_switch->type != NSWT_STRING)
			{
				yyerror("STRING case label used in a non-STRING switch.");
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_SUFFIX;
			cs->a.str = $A;

			$$ = cs;
		}
	|	T_RANGE T_STRING_LITERAL[A] T_RANGE
		{
			if (nib_current_switch->type != NSWT_STRING)
			{
				yyerror("STRING case label used in a non-STRING switch.");
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_INFIX;
			cs->a.str = $A;

			$$ = cs;
		}
	|	T_IDENTIFIER[A]
		{
			if (nib_current_switch->type != NSWT_STAT)
			{
				yyerror("STAT case label used in a non-STAT switch.");
				YYERROR;
			}

			flag_value_t value;
			if (!nib_find_flag_value(nib_current_switch->table, $A, NULL, &value))
			{
				niberrorf("Unknown stat value '%s' for case label.", $A);
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_VALUE;
			cs->a.number = value;

			$$ = cs;
			nib_free($A);
		}
	|	T_IDENTIFIER[A] T_RANGE
		{
			if (nib_current_switch->type != NSWT_STAT)
			{
				yyerror("STAT case label used in a non-STAT switch.");
				YYERROR;
			}

			flag_value_t value;
			if (!nib_find_flag_value(nib_current_switch->table, $A, NULL, &value))
			{
				niberrorf("Unknown stat value '%s' for case label.", $A);
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_VX;
			cs->a.number = value;

			$$ = cs;
			nib_free($A);
		}
	|	T_RANGE T_IDENTIFIER[A]
		{
			if (nib_current_switch->type != NSWT_STAT)
			{
				yyerror("STAT case label used in a non-STAT switch.");
				YYERROR;
			}

			flag_value_t value;
			if (!nib_find_flag_value(nib_current_switch->table, $A, NULL, &value))
			{
				niberrorf("Unknown stat value '%s' for case label.", $A);
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_XV;
			cs->b.number = value;

			$$ = cs;
			nib_free($A);
		}
	|	T_IDENTIFIER[A] T_RANGE T_IDENTIFIER[B]
		{
			if (nib_current_switch->type != NSWT_STAT)
			{
				yyerror("STAT case label used in a non-STAT switch.");
				YYERROR;
			}

			flag_value_t a;
			if (!nib_find_flag_value(nib_current_switch->table, $A, NULL, &a))
			{
				niberrorf("Unknown stat value '%s' for case label.", $A);
				YYERROR;
			}

			flag_value_t b;
			if (!nib_find_flag_value(nib_current_switch->table, $B, NULL, &b))
			{
				niberrorf("Unknown stat value '%s' for case label.", $B);
				YYERROR;
			}

			struct nib_compile_switch_case_s *cs = new_nib_compile_switch_case();
			if (!cs)
			{
				yyerror("Memory allocation failure.");
				YYERROR;
			}

			cs->type = NCASE_VV;
			cs->a.number = a;
			cs->b.number = b;

			$$ = cs;
			nib_free($A);
			nib_free($B);
		}
	;

default:
		T_DEFAULT T_COLON
		{
			if (nib_current_switch->has_default)
			{
				yyerror("Duplicate DEFAULT case found in switch.");
				YYERROR;
			}

			nib_current_switch->default_address = CURRENT_PROGRAM_SIZE;
			nib_current_switch->has_default = true;
		}
	;

constant:
		constant T_PLUS constant		{ $$ = $1 + $3; }
	|	constant T_MINUS constant		{ $$ = $1 - $3; }
	|	constant T_STAR constant		{ $$ = $1 * $3; }
	|	constant T_MOD constant
		{
			if (!$3)
			{
				yyerror("Attempting to divide by zero.");
				YYERROR;
			}

			$$ = $1 % $3;
		}
	|	constant T_DIVIDE constant
		{
			if (!$3)
			{
				yyerror("Attempting to divide by zero.");
				YYERROR;
			}

			$$ = $1 / $3;
		}
	|	constant T_BAND constant			{ $$ = $1 & $3; }
	|	constant T_BOR constant				{ $$ = $1 | $3; }
	|	constant T_BXOR constant			{ $$ = $1 ^ $3; }
	/*
	|	constant T_EQUAL constant			{ $$ = $1 == $3; }
	|	constant T_NOT_EQUAL constant		{ $$ = $1 != $3; }
	|	constant T_LT constant				{ $$ = $1 < $3; }
	|	constant T_LT_EQUAL constant		{ $$ = $1 <= $3; }
	|	constant T_GT constant				{ $$ = $1 > $3; }
	|	constant T_GT_EQUAL constant		{ $$ = $1 >= $3; }
	*/
	|	constant T_LEFT_SHIFT constant		{ $$ = ($3 > MAX_SHIFT) ? 0 : ($1 << $3); }
	|	constant T_RIGHT_SHIFT constant		{ $$ = ($3 > MAX_SHIFT) ? (($1 >= 0) ? 0 : -1) : ($1 >> $3); }
	|	constant T_RIGHTL_SHIFT constant	{ $$ = ($3 > MAX_SHIFT) ? 0 : (long)((unsigned long)$1 >> $3); }
	|	T_BNOT constant						{ $$ = ~$2; }
	|	T_MINUS constant %prec T_BNOT		{ $$ = -$2; }
	/*|	T_LNOT constant						{ $$ = !$2; }*/
	|	T_OPEN_PAREN constant T_CLOSE_PAREN	{ $$ = $2; }
	|	T_NUMBER							{ $$ = $1; }
	;

float_constant:
		float_constant T_PLUS float_constant	{ $$ = $1 + $3; }
	|	float_constant T_MINUS float_constant	{ $$ = $1 - $3; }
	|	float_constant T_STAR float_constant	{ $$ = $1 * $3; }
	|	float_constant T_DIVIDE float_constant
		{
			if ($3 == 0.0)
			{
				yyerror("Attempting to divide by zero.");
				YYERROR;
			}

			$$ = $1 / $3;
		}
	|	T_MINUS float_constant %prec T_BNOT		{ $$ = -$2; }
	|	T_OPEN_PAREN float_constant T_CLOSE_PAREN	{ $$ = $2; }
	|	T_FLOAT_NUMBER							{ $$ = $1; }
	;


comma_expr:
		expr0
		{
			$$.name = $1.name;
			$$.type = $1.type;
			$$.needs_use = $1.needs_use;
			$$.needs_pop = $1.needs_pop;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
		}
	|	comma_expr
		{
			// Complain if needed
			if ($1.needs_use)
			{
				yyerror("Expression result not used.  Discarded.");
				insert_pop_value();
			}
		}
		T_COMMA expr0
		{
			$$.name = $4.name;
			$$.type = $4.type;
			$$.needs_use = $4.needs_use;
			$$.needs_pop = $4.needs_pop;

			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($1.type);
		}
	;

expr0:
		lvalue[L] T_ASSIGN[A]
		{
			// printf("%d: lvalue: %s(%p)\n", __LINE__, nib_get_typename(NULL, $L.type), $L.type);

			if (IS_SET($L.flags, (IS_READONLY|IS_LITERAL)))
			{
				yyerror("left hand expression is readonly.");
				YYERROR;
			}

			ins_bytes($L.lhs, $L.lhs_len);
		}
		expr0[R]
		{
			if (!check_valid_assignment($L.type,$R.type,$A))
			{
				yyerror("Right hand value not value for left hand lvalue.");
				YYERROR;
			}

			last_expression = CURRENT_PROGRAM_SIZE;
			ins_code($2);

			$$.name = $L.name;
			$$.type = $L.type;
			$$.needs_use = false;	// Since it is an assignment
			$$.needs_pop = true;
			$$.flags = IS_READONLY;

			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			nib_free_lvalue_s(&($L));

			// printf("%d: rvalue: %s(%p)\n", __LINE__, nib_get_typename(NULL, $R.type), $R.type);

			free_nib_type($R.type);
		}
	|	field_call[L] T_ASSIGN[A]
		{
			// printf("%d: lvalue: %s(%p)\n", __LINE__, nib_get_typename(NULL, $L.type), $L.type);

			if (IS_SET($L.flags, IS_READONLY) || !IS_SET($L.flags, IS_LVALUE))
			{
				yyerror("Field is readonly.");
				YYERROR;
			}
		}
		expr0[R]
		{
			if (!check_valid_assignment($L.type,$R.type,$A))
			{
				yyerror("Right hand value not value for left hand lvalue.");
				YYERROR;
			}

			last_expression = CURRENT_PROGRAM_SIZE;
			ins_code($2);

			$$.name = $1.name;
			$$.type = $1.type;
			$$.needs_use = false;	// Since it is an assignment
			$$.needs_pop = true;
			$$.flags = IS_READONLY;

			// printf("%d: rvalue: %s(%p)\n", __LINE__, nib_get_typename(NULL, $R.type), $R.type);
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);

			free_nib_type($R.type);
		}
	|	expr0[C] T_QMARK
		{
			// Expression based if-else

			ins_code(NI_JUMP_ZERO);
			$<address>$ = CURRENT_PROGRAM_SIZE;		// Address of the jump offset
			ins_address(0);

		}
		expr0[T]
		{
			ins_code(NI_JUMP);
			$<address>$ = CURRENT_PROGRAM_SIZE;		// Address of the jump offset
			ins_address(0);

			set_current_address($<address>3);
		}
		T_COLON expr0[F] %prec T_QMARK
		{
			// Update the branch offset for the $T ($5) to after the expression
			set_current_address($<address>5);

			// Check the types of the two parts

			last_expression = -1;

			NIB_TYPE *t1 = $T.type;
			NIB_TYPE *t2 = $F.type;

			NIB_TYPE *t = nib_combine_types(t1, t2);
			if (!t)
			{
				yyerror("Incompatible types used in ?: expression.");
				YYERROR;
			}

			$$.name = NULL;
			$$.type = t;
			$$.needs_use = $T.needs_use && $F.needs_use;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;

			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);

			free_nib_type($T.type);
			free_nib_type($F.type);
		}
	|	expr0[L] T_COALESCE %prec T_COALESCE
		{
			ins_code(NI_DUP);	// Make a copy of $L on the stack
			ins_code(NI_JUMP_NOT_ZERO);		// Pops $L' off the stack
			$<address>$ = CURRENT_PROGRAM_SIZE;
			ins_address(0);
			ins_code(NI_POP);	// To remove the original $L off the stack
		}
		expr0[R]
		{
			NIB_TYPE *t = nib_combine_types($L.type, $R.type);
			if (!t)
			{
				yyerror("Incompatible types used in ?: expression.");
				YYERROR;
			}

			upd_address($<address>3,CURRENT_PROGRAM_SIZE);

			$$.name = NULL;
			$$.type = t;
			$$.needs_use = $L.needs_use && $L.needs_use;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
		}
	|	expr0[L] T_LOR %prec T_LOR
		{
		}
		expr0[R]
		{
			if (!check_valid_operation($L.type,$R.type,NI_LOR))
			{
				yyerror("Invalid arguments for || operation.");
				YYERROR;
			}

			ins_code(NI_LOR);

			last_expression = -1;

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_LXOR %prec T_LXOR
		{
			// check $1 type 

		}
		expr0[R]
		{
			if (!check_valid_operation($L.type,$R.type,NI_LXOR))
			{
				yyerror("Invalid arguments for ^^ operation.");
				YYERROR;
			}

			ins_code(NI_LXOR);

			last_expression = -1;

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_LAND %prec T_LAND
		{
		}
		expr0[R]
		{
			if (!check_valid_operation($L.type,$R.type,NI_LAND))
			{
				yyerror("Invalid arguments for && operation.");
				YYERROR;
			}

			ins_code(NI_LAND);

			last_expression = -1;

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_EQUAL expr0[R] %prec T_EQUAL
		{
			if (!check_valid_operation($L.type,$R.type,NI_EQ))
			{
				yyerror("Invalid arguments for == operation.");
				YYERROR;
			}

			ins_code(NI_EQ);

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_NOT_EQUAL expr0[R] %prec T_NOT_EQUAL
		{
			if (!check_valid_operation($L.type,$R.type,NI_NEQ))
			{
				yyerror("Invalid arguments for != operation.");
				YYERROR;
			}

			ins_code(NI_NEQ);

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_LT expr0[R] %prec T_LT
		{
			if (!check_valid_operation($L.type,$R.type,NI_LT))
			{
				yyerror("Invalid arguments for < operation.");
				YYERROR;
			}

			ins_code(NI_LT);

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_LT_EQUAL expr0[R] %prec T_LT_EQUAL
		{
			if (!check_valid_operation($L.type,$R.type,NI_LE))
			{
				yyerror("Invalid arguments for <= operation.");
				YYERROR;
			}

			ins_code(NI_LE);

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_GT expr0[R] %prec T_GT
		{
			if (!check_valid_operation($L.type,$R.type,NI_GT))
			{
				yyerror("Invalid arguments for > operation.");
				YYERROR;
			}

			ins_code(NI_GT);

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_GT_EQUAL expr0[R] %prec T_GT_EQUAL
		{
			if (!check_valid_operation($L.type,$R.type,NI_GE))
			{
				yyerror("Invalid arguments for >= operation.");
				YYERROR;
			}

			ins_code(NI_GE);

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_BOR expr0[R] %prec T_BOR
		{
			if (!check_valid_operation($L.type,$R.type,NI_BOR))
			{
				yyerror("Invalid arguments for | operation.");
				YYERROR;
			}

			ins_code(NI_BOR);

			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_BXOR expr0[R] %prec T_BXOR
		{
			if (!check_valid_operation($L.type,$R.type,NI_BXOR))
			{
				yyerror("Invalid arguments for ^ operation.");
				YYERROR;
			}

			ins_code(NI_BXOR);

			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_BAND expr0[R] %prec T_BAND
		{
			if (!check_valid_operation($L.type,$R.type,NI_BAND))
			{
				yyerror("Invalid arguments for & operation.");
				YYERROR;
			}

			ins_code(NI_BAND);

			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_STR_PREFIX expr0[R] %prec T_STR_PREFIX
		{
			if (!check_valid_operation($L.type,$R.type,NI_STR_PREFIX))
			{
				yyerror("Invalid arguments for <~ operation.");
				YYERROR;
			}

			ins_code(NI_STR_PREFIX);

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_STR_INFIX expr0[R] %prec T_STR_INFIX
		{
			if (!check_valid_operation($L.type,$R.type,NI_STR_INFIX))
			{
				yyerror("Invalid arguments for ~~ operation.");
				YYERROR;
			}

			ins_code(NI_STR_INFIX);

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_STR_SUFFIX expr0[R] %prec T_STR_SUFFIX
		{
			if (!check_valid_operation($L.type,$R.type,NI_STR_SUFFIX))
			{
				yyerror("Invalid arguments for ~> operation.");
				YYERROR;
			}

			ins_code(NI_STR_SUFFIX);

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_LEFT_SHIFT expr0[R] %prec T_LEFT_SHIFT
		{
			if (!check_valid_operation($L.type,$R.type,NI_LSH))
			{
				yyerror("Invalid arguments for << operation.");
				YYERROR;
			}

			ins_code(NI_LSH);

			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_RIGHT_SHIFT expr0[R] %prec T_RIGHT_SHIFT
		{
			if (!check_valid_operation($L.type,$R.type,NI_RSH))
			{
				yyerror("Invalid arguments for >> operation.");
				YYERROR;
			}

			ins_code(NI_RSH);

			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_RIGHTL_SHIFT expr0[R] %prec T_RIGHTL_SHIFT
		{
			if (!check_valid_operation($L.type,$R.type,NI_RSHL))
			{
				yyerror("Invalid arguments for >>> operation.");
				YYERROR;
			}

			ins_code(NI_RSHL);

			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_PLUS %prec T_PLUS
		{

		}
		expr0[R]
		{
			NIB_TYPE *type = check_valid_operation($L.type,$R.type,NI_ADD);
			if (!type)
			{
				niberrorf("Invalid arguments (%s + %s) for + operation.",
					nib_get_typename(NULL,$L.type),
					nib_get_typename(NULL,$R.type));
				YYERROR;
			}

			ins_code(NI_ADD);

			$$.name = NULL;
			$$.type = type;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_MINUS expr0[R] %prec T_MINUS
		{
			NIB_TYPE *type = check_valid_operation($L.type,$R.type,NI_SUBT);
			if (!type)
			{
				yyerror("Invalid arguments for - operation.");
				YYERROR;
			}

			ins_code(NI_SUBT);

			$$.name = NULL;
			$$.type = type;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_STAR expr0[R] %prec T_STAR
		{
			NIB_TYPE *type = check_valid_operation($L.type,$R.type,NI_ADD);
			if (!type)
			{
				yyerror("Invalid arguments for * operation.");
				YYERROR;
			}

			ins_code(NI_MULT);

			$$.name = NULL;
			$$.type = type;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_MOD expr0[R] %prec T_MOD
		{
			NIB_TYPE *type = check_valid_operation($L.type,$R.type,NI_MOD);
			if (!type)
			{
				yyerror("Invalid arguments for % operation.");
				YYERROR;
			}

			ins_code(NI_MOD);

			$$.name = NULL;
			$$.type = type;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	expr0[L] T_DIVIDE expr0[R] %prec T_DIVIDE
		{
			NIB_TYPE *type = check_valid_operation($L.type,$R.type,NI_ADD);
			if (!type)
			{
				yyerror("Invalid arguments for / operation.");
				YYERROR;
			}

			ins_code(NI_DIV);

			$$.name = NULL;
			$$.type = type;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
			free_nib_type($R.type);
		}
	|	cast[T] expr0[E] %prec T_BNOT
		{
			if ($E.type == NULL || ($E.type->type_class != NTC_MULTI && $E.type != nibtype_any))
			{
				yyerror("Unexpected typecast.");
				YYERROR;
			}

			if ($E.type->type_class == NTC_MULTI)
			{
				// Verify the typecast is in the restricted list
				ITERATOR it;
				NIB_TYPE *type;
				iterator_start(&it, $E.type->_.multi);
				while((type = (NIB_TYPE *)iterator_nextdata(&it)))
				{
					if (are_nib_types_equal(type, $T))
						break;
				}
				iterator_stop(&it);

				if (type == NULL)
				{
					yyerror("Invalid typecase for mixed type.");
					YYERROR;
				}
			}

			$$.name = $E.name;
			$$.type = $T;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;

			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($E.type);
		}
	|	T_INCREMENT lvalue[L] %prec T_INCREMENT
		{
			if (!are_nib_types_equal($L.type,nibtype_int))
			{
				yyerror("Only integers may be used with pre-increment operators.");
				YYERROR;
			}

			ins_bytes($L.lhs, $L.lhs_len);
			last_expression = CURRENT_PROGRAM_SIZE;
			ins_code(NI_PRE_INC);

			$$.name = NULL;
			$$.type = $L.type;
			$$.needs_use = false;
			$$.needs_pop = true;
			$$.flags = IS_READONLY;

			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			nib_free_lvalue_s(&($L));
		}
	|	T_DECREMENT lvalue[L] %prec T_DECREMENT
		{
			if (!are_nib_types_equal($L.type,nibtype_int))
			{
				yyerror("Only integers may be used with pre-decrement operators.");
				YYERROR;
			}

			ins_bytes($L.lhs, $L.lhs_len);
			last_expression = CURRENT_PROGRAM_SIZE;
			ins_code(NI_PRE_DEC);

			$$.name = NULL;
			$$.type = $L.type;
			$$.needs_use = false;
			$$.needs_pop = true;
			$$.flags = IS_READONLY;

			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			nib_free_lvalue_s(&($L));
		}
	|	T_LNOT expr0[L]
		{
			if ($L.type->type_class == NTC_LIST)
			{
				yyerror("Lists may not use ! operator.");
				YYERROR;
			}

			if ($L.type->type_class == NTC_ARRAY)
			{
				yyerror("Array may not use ! operator.");
				YYERROR;
			}

			if ($L.type->type_class == NTC_STAT)
			{
				yyerror("Stat values may not use ! operator.");
				YYERROR;
			}
			// Everything else can be !X'd

			last_expression = CURRENT_PROGRAM_SIZE;
			ins_code(NI_LNOT);

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;

			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			free_nib_type($L.type);
		}
	|	T_BNOT expr0[L]
		{
			if (!are_nib_types_equal($L.type,nibtype_int) && $L.type->type_class != NTC_FLAG)
			{
				yyerror("Only integers and flags may use bitwise operations.");
				YYERROR;
			}

			ins_code(NI_BNOT);

			$$.name = NULL;
			$$.type = $L.type;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;

			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
		}
	|	T_MINUS expr0[L] %prec T_BNOT
		{
			if (!are_nib_types_equal($L.type,nibtype_int) &&
				!are_nib_types_equal($L.type,nibtype_float))
			{
				yyerror("Only numerical values may be used with negation.");
				YYERROR;
			}

			ins_code(NI_NEG);

			$$.name = NULL;
			$$.type = $L.type;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;

			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
		}
	|	lvalue[L] T_INCREMENT %prec T_INCREMENT
		{
			if (!are_nib_types_equal($L.type,nibtype_int))
			{
				yyerror("Only integers may be used with post-increment operators.");
				YYERROR;
			}

			ins_bytes($L.lhs, $L.lhs_len);
			last_expression = CURRENT_PROGRAM_SIZE;
			ins_code(NI_POST_INC);

			$$.name = NULL;
			$$.type = $L.type;
			$$.needs_use = false;
			$$.needs_pop = true;
			$$.flags = IS_READONLY;

			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			nib_free_lvalue_s(&($L));
		}
	|	lvalue[L] T_DECREMENT %prec T_DECREMENT
		{
			if (!are_nib_types_equal($L.type,nibtype_int))
			{
				yyerror("Only integers may be used with post-decrement operators.");
				YYERROR;
			}

			ins_bytes($L.lhs, $L.lhs_len);
			last_expression = CURRENT_PROGRAM_SIZE;
			ins_code(NI_POST_DEC);

			$$.name = NULL;
			$$.type = $L.type;
			$$.needs_use = false;
			$$.needs_pop = true;
			$$.flags = IS_READONLY;

			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			nib_free_lvalue_s(&($L));
		}
	|	T_OPEN_PAREN expr0 T_CLOSE_PAREN
		{
			$$ = $2;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
		}
	|	expr4[E]
		{
			if ($E.type == NULL ||
				(!are_nib_types_equal($E.type,nibtype_string) &&
				 $E.type->type_class != NTC_LIST &&
				 $E.type->type_class != NTC_ARRAY))
			{
				yyerror("Only strings, lists and arrays may be indexed.");
				YYERROR;
			}
		}
		T_OPEN_BRACKET index_range[I] T_CLOSE_BRACKET %prec T_OPEN_BRACKET
		{
			NIB_TYPE *type = NULL;
			if ($E.type == nibtype_string)
			{
				if ($I == NIDX_SINGLE)
					type = nibtype_char;
				else
					type = nibtype_string;
			}
			else if ($E.type->type_class == NTC_LIST)
				type = $E.type->_.list.type;
			else if ($E.type->type_class == NTC_ARRAY)
				type = $E.type->_.array.type;
		}
	|	expr4
		{
			$$ = $1;
			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			// hex_dump($$.type, sizeof(NIB_TYPE));
		}
	;

index_range:
		index_value[A]
		{
			$$ = NIDX_SINGLE;
		}
	|	index_value[A] T_RANGE
		{
			if (!are_nib_types_equal($A.type,nibtype_int))
			{
				yyerror("Only integers may be used in ranged indexing.");
				YYERROR;
			}

			$$ = NIDX_MIN;
		}
	|	T_RANGE index_value[B]
		{
			if (!are_nib_types_equal($B.type,nibtype_int))
			{
				yyerror("Only integers may be used in ranged indexing.");
				YYERROR;
			}

			$$ = NIDX_MAX;
		}
	|	index_value[A] T_RANGE index_value[B]
		{
			if (!are_nib_types_equal($A.type,nibtype_int) ||
				!are_nib_types_equal($B.type,nibtype_int))
			{
				yyerror("Only integers may be used in ranged indexing.");
				YYERROR;
			}

			$$ = NIDX_RANGE;
		}
	;

index_value:
		expr0
		{
			if ($1.type == NULL ||
				(!are_nib_types_equal($1.type,nibtype_int) && $1.type->type_class != NTC_STAT))
			{
				yyerror("Only integer or stat values may be used to index an array or list.");
				YYERROR;
			}

			$$ = $1;
		}
	;

expr4:
		T_SELF
		{
			switch(nib_compile_script_class)
			{
			case NSC_AREA:		$$.type = nibtype_area; break;
			case NSC_DUNGEON:	$$.type = nibtype_dungeon; break;
			case NSC_INSTANCE:	$$.type = nibtype_instance;	break;
			case NSC_MOBILE:	$$.type = nibtype_mobile; break;
			case NSC_OBJECT:	$$.type = nibtype_object; break;
			case NSC_ROOM:		$$.type = nibtype_room; break;
			case NSC_TOKEN:		$$.type = nibtype_token; break;
			default:
				yyerror("SELF used with invalid script class.");
				YYERROR;
			}

			// Can only be used as an rvalue!
			$$.lhs = NULL;
			$$.lhs_len = 0;
			$$.rhs_len = 1;
			$$.rhs = nib_alloc_bytecodes($$.rhs_len);
			$$.rhs[0] = NI_LVALUE_SELF;

			$$.name = NULL;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_READONLY;
		}
	|	function_call
		{
			last_expression = CURRENT_PROGRAM_SIZE;

			// Get type
			$$.name = NULL;
			$$.type = $1.type;
			$$.needs_use = $1.needs_use;
			$$.needs_pop = $1.needs_pop;
			$$.flags = $1.flags;
		}
	|	method_call
		{
			last_expression = CURRENT_PROGRAM_SIZE;

			// Get type
			$$.name = NULL;
			$$.type = $1.type;
			$$.needs_use = $1.needs_use;
			$$.needs_pop = $1.needs_pop;
			$$.flags = $1.flags;
		}
	|	field_call
		{
			last_expression = CURRENT_PROGRAM_SIZE;

			// Get type
			$$.name = $1.name;
			$$.type = $1.type;
			$$.needs_use = $1.needs_use;
			$$.flags = $1.flags;

			// printf("field_call type: %p\n", $1.type);
			// hex_dump($$.type,sizeof(NIB_TYPE));
		}
	|	T_STRING_LITERAL
		{
			// Add string literal to the string literal list (if necessary)
			short index = nib_add_string_to_storage($1);
			if (index < 1)
			{
				yyerror("Error storing string literal into storage.");
				YYERROR;
			}

			last_expression = CURRENT_PROGRAM_SIZE;

			// Push OP code to pull string literal
			// Push index
			ins_code(NI_LOAD_STRING);
			ins_short(index);
			
			$$.name = NULL;
			$$.type = nibtype_string;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_LITERAL;

			nib_free($1);
		}
	|	T_CHAR_LITERAL
		{
			last_expression = CURRENT_PROGRAM_SIZE;

			ins_code(NI_LOAD_CHAR);
			ins_int($1);

			$$.name = NULL;
			$$.type = nibtype_char;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_LITERAL;
		}
	|	T_NUMBER
		{
			last_expression = CURRENT_PROGRAM_SIZE;
			if ($1 == 0L)
			{
				// Push opcode for constant 0
				ins_code(NI_CONST0);
			}
			else if ($1 == 1L)
			{
				// Push opcode for constant 1
				ins_code(NI_CONST1);
			}
			else if ($1 == -1L)
			{
				// Push opcode for constant -1
				ins_code(NI_NCONST1);
			}
			else
			{
				// Push opcode for reading number
				// Push integer

				ins_code(NI_LOAD_NUMBER);
				ins_long($1);
			}

			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_LITERAL;
		}
	|	T_FLOAT_NUMBER
		{
			last_expression = CURRENT_PROGRAM_SIZE;

			if ($1 == 0.0)
			{
				ins_code(NI_FCONST0);
			}
			else
			{
				// Push opcode for reading float
				// Push double
				ins_code(NI_LOAD_FLOAT);
				ins_float($1);
			}
			$$.name = NULL;
			$$.type = nibtype_float;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_LITERAL;
		}
	|	boolean_value
		{
			last_expression = CURRENT_PROGRAM_SIZE;

			if($1)
			{
				// Push opcode for reading TRUE
				ins_code(NI_TRUE);
			}
			else
			{
				// Push opcode for reading FALSE
				ins_code(NI_FALSE);
			}

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_LITERAL;
		}
	|	widevnum_value
		{
			// Not sure how to do this...

			$$ = $1;
		}
	|	T_OPEN_LIST T_CLOSE_LIST
		{
			$$.name = NULL;
			$$.type = nibtype_list;		// Generic empty list
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_LITERAL;
		}
	|	T_NULL
		{
			last_expression = CURRENT_PROGRAM_SIZE;

			ins_code(NI_NULL);

			$$.name = NULL;
			$$.type = nibtype_null;	// Think ((void *)0) from C
			$$.needs_use = true;
			$$.needs_pop = false;
			$$.flags = IS_LITERAL;
		}
	|	lvalue[L]
		{
			ins_bytes($L.rhs, $L.rhs_len);

			$$.name = $L.name;
			$$.type = $L.type;
			$$.flags = $L.flags;
			$$.needs_use = true;

			// printf("lvalue: %s\n", nib_get_typename(NULL, $L.type));

			nib_free_lvalue_s(&($L));
		}
	|	T_OPEN_BANK T_IDENTIFIER[I] T_COLON comma_name_list[L] T_CLOSE_BANK
		{
			const struct flag_type **bank = nib_lookup_flag_bank($I);
			if (!bank)
			{
				niberrorf("Undefined flag bank '%s'.", $I);
				YYERROR;
			}

			int banks;
			for(banks = 0; bank[banks]; banks++);

			ITERATOR it;
			char *name;

			long *bits = calloc(banks,sizeof(long));
			if (!bits)
			{
				niberror("Memory allocation error.");
				YYERROR;
			}

			iterator_start(&it, $L);
			while((name = (char *)iterator_nextdata(&it)))
			{
				flag_value_t bit;
				int b;

				if (nib_find_flagbank_value(bank, name, NULL, &b, &bit))
				{
					bits[b] |= bit;
				}
				else
					break;
			}
			iterator_stop(&it);

			if (name != NULL)
			{
				free(bits);
				niberrorf("Unknown name '%s' for table '%s'", name, $I);
				YYERROR;
			}

			ins_code(NI_LOAD_FLAG_BANK);
			ins_short((short)nib_add_used_bank(bank));
			for(int i = 0; i < banks; i++)
				ins_long(bits[i]);

			$$.name = NULL;
			$$.type = new_nib_type_flag_bank(bank);
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
			nib_free($I);
			list_destroy($L);
			free(bits);
		}
	|	T_OPEN_FLAG T_IDENTIFIER[I] T_COLON comma_name_list[L] T_CLOSE_FLAG
		{
			if (list_size($L) > MAX_FLAG_BITS)
			{
				niberrorf("Flag name lists only support %d names.  Encountered %d instead.",
					MAX_FLAG_BITS, list_size($L));
				YYERROR;
			}

			// Look up the flag table
			const struct flag_type *table = nib_lookup_flag_table(nib_flag_created_tables,$I);
			if (!table)
			{
				niberrorf("Undefined flag table '%s'.", $I);
				YYERROR;
			}

			flag_value_t value = 0;

			ITERATOR it;
			char *name;

			iterator_start(&it, $L);
			while((name = (char *)iterator_nextdata(&it)))
			{
				flag_value_t bit;

				if (nib_find_flag_value(table, name, NULL, &bit))
				{
					value |= bit;
				}
				else
					break;
			}
			iterator_stop(&it);

			if (name != NULL)
			{
				niberrorf("Unknown name '%s' for table '%s'", name, $I);
				YYERROR;
			}

			// Store it as a bitvector
			ins_code(NI_LOAD_FLAG_TABLE);
			ins_long(value);
			ins_short((short)nib_add_used_table(table));

			$$.name = NULL;
			$$.type = new_nib_type_flag_table(table);
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
			nib_free($I);
			list_destroy($L);
		}
	|	flag_number_list[L]
		{
			ins_code(NI_LOAD_FLAG);
			ins_long($L);

			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
		}
	|	T_OPEN_FLAG T_IDENTIFIER[T] T_DOT T_IDENTIFIER[I] T_CLOSE_FLAG
		{
			const struct flag_type *table = nib_lookup_stat_table(nib_stat_created_tables,$T);
			if (!table)
			{
				niberrorf("Undefined stat table '%s'.", $T);
				YYERROR;
			}

			flag_value_t bit;
			bool settable;
			if (!nib_find_flag_value(table, $I, &settable, &bit))
			{
				niberrorf("Stat table '%s' has no value '%s' defined.", $T, $I);
				YYERROR;
			}

			ins_code(NI_LOAD_STAT);
			ins_long(bit);
			ins_short((short)nib_add_used_table(table));

			$$.name = NULL;
			$$.type = new_nib_type_stat_table(table, false);
			$$.needs_use = true;
			$$.flags = IS_LITERAL;

			nib_free($I);
			nib_free($T);
		}
	;

// table_name:
// 		T_ACCOUNT									{ $$ = "account"; }
// 	|	T_AFFECT									{ $$ = "affect"; }
// 	|	T_AREA										{ $$ = "area"; }
// 	|	T_CLASS										{ $$ = "class"; }
// 	|	T_DUNGEON									{ $$ = "dungeon"; }
// 	|	T_EXIT										{ $$ = "exit"; }
// 	|	T_INSTANCE									{ $$ = "instance"; }
// 	|	T_LIQUID									{ $$ = "liquid"; }
// 	|	T_MAIL										{ $$ = "mail"; }
// 	|	T_MATERIAL									{ $$ = "material"; }
// 	|	T_MISSION									{ $$ = "mission"; }
// 	|	T_MOBILE									{ $$ = "mobile"; }
// 	|	T_NOTE										{ $$ = "note"; }
// 	|	T_OBJECT									{ $$ = "object"; }
// 	|	T_ORG										{ $$ = "org"; }
// 	|	T_QUEST										{ $$ = "quest"; }
// 	|	T_RACE										{ $$ = "race"; }
// 	|	T_RANK										{ $$ = "rank"; }
// 	|	T_REPUTATION								{ $$ = "reputation"; }
// 	|	T_ROOM										{ $$ = "room"; }
// 	|	T_SHIP										{ $$ = "ship"; }
// 	|	T_SKILL										{ $$ = "skill"; }
// 	|	T_TOKEN										{ $$ = "token"; }
// 	|	T_WILDS										{ $$ = "wilds"; }
// 	|	T_WORLD										{ $$ = "world"; }
// 	;

widevnum_value:
		T_NUMBER[A] T_WIDEVNUM_DELIM T_NUMBER[V]
		{
			// Store opcode for reading absolute widevnum
			// Store widevnum

			ins_code(NI_LOAD_NUMBER);
			ins_long($A);
			ins_code(NI_GET_AREA);
			ins_code(NI_LOAD_NUMBER);
			ins_long($V);
			ins_code(NI_LOAD_WIDEVNUM);

			$$.name = NULL;
			$$.type = nibtype_widevnum;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
		}
	|	T_STRING_LITERAL[A] T_WIDEVNUM_DELIM T_NUMBER[V]
		{
			// Add string literal to the string literal list (if necessary)
			short index = nib_add_string_to_storage($A);
			if (index < 1)
			{
				yyerror("Error storing string literal into storage.");
				YYERROR;
			}

			// Store opcodes for reading named widevnum
			ins_code(NI_LOAD_STRING);
			ins_int(index);
			ins_code(NI_GET_AREA);
			ins_code(NI_LOAD_NUMBER);
			ins_long($V);
			ins_code(NI_LOAD_WIDEVNUM);
			
			$$.name = NULL;
			$$.type = nibtype_widevnum;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
			nib_free($A);
		}
	|	lvalue[A] T_WIDEVNUM_DELIM T_NUMBER[V]
		{
			// Verify $A is an area
			if ($A.type == NULL ||
				$A.type->type_class != NTC_PRIMARY ||
				$A.type->_.primary != NT_AREA)
			{
				yyerror("Invalid area component for widevnum.");
				YYERROR;
			}

			ins_bytes($A.rhs, $A.rhs_len);		// lvalue loading
			ins_code(NI_LOAD_NUMBER);
			ins_long($V);
			ins_code(NI_LOAD_WIDEVNUM);

			$$.name = NULL;
			$$.type = nibtype_widevnum;
			$$.needs_use = true;
			$$.flags = IS_READONLY;

			nib_free_lvalue_s(&($A));
		}
	|	T_WIDEVNUM_DELIM T_NUMBER[V]
		{
			// Store opcode for reading relative widevnum
			ins_code(NI_CONST0);
			ins_code(NI_LOAD_NUMBER);
			ins_long($V);
			ins_code(NI_LOAD_WIDEVNUM);

			$$.name = NULL;
			$$.type = nibtype_widevnum;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
		}
	|	T_NUMBER[A] T_WIDEVNUM_DELIM lvalue[V]
		{
			// Verify $V is a number
			if ($V.type == NULL ||
				$V.type->type_class != NTC_PRIMARY ||
				$V.type->_.primary != NT_NUMBER)
			{
				yyerror("Invalid vnum component for widevnum.");
				YYERROR;
			}

			// Store opcode for reading absolute widevnum
			ins_code(NI_LOAD_NUMBER);
			ins_long($A);
			ins_code(NI_GET_AREA);
			ins_bytes($V.rhs,$V.rhs_len);
			ins_code(NI_LOAD_WIDEVNUM);

			$$.name = NULL;
			$$.type = nibtype_widevnum;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;

			nib_free_lvalue_s(&($V));
		}
	|	T_STRING_LITERAL[A] T_WIDEVNUM_DELIM lvalue[V]
		{
			// Verify $V is a number
			if ($V.type == NULL ||
				$V.type->type_class != NTC_PRIMARY ||
				$V.type->_.primary != NT_NUMBER)
			{
				yyerror("Invalid vnum component for widevnum.");
				YYERROR;
			}

			// Add string literal to the string literal list (if necessary)
			short index = nib_add_string_to_storage($A);
			if (index < 1)
			{
				yyerror("Error storing string literal into storage.");
				YYERROR;
			}

			// Store opcode for reading named widevnum
			ins_code(NI_LOAD_STRING);
			ins_int(index);
			ins_code(NI_GET_AREA);
			ins_bytes($V.rhs,$V.rhs_len);
			ins_code(NI_LOAD_WIDEVNUM);
			
			$$.name = NULL;
			$$.type = nibtype_widevnum;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
			nib_free($A);
			nib_free_lvalue_s(&($V));
		}
	|	lvalue[A] T_WIDEVNUM_DELIM lvalue[V]
		{
			// Verify $A is an area
			if ($A.type == NULL ||
				$A.type->type_class != NTC_PRIMARY ||
				$A.type->_.primary != NT_AREA)
			{
				yyerror("Invalid area component for widevnum.");
				YYERROR;
			}

			// Verify $V is a number
			if ($V.type == NULL ||
				$V.type->type_class != NTC_PRIMARY ||
				$V.type->_.primary != NT_NUMBER)
			{
				yyerror("Invalid vnum component for widevnum.");
				YYERROR;
			}

			ins_bytes($A.rhs, $A.rhs_len);
			ins_bytes($V.rhs, $V.rhs_len);
			ins_code(NI_LOAD_WIDEVNUM);

			$$.name = NULL;
			$$.type = nibtype_widevnum;
			$$.needs_use = true;
			$$.flags = IS_READONLY;

			nib_free_lvalue_s(&($A));
			nib_free_lvalue_s(&($V));
		}
	|	T_WIDEVNUM_DELIM lvalue[V]
		{
			// Verify $V is a number
			if ($V.type == NULL ||
				$V.type->type_class != NTC_PRIMARY ||
				$V.type->_.primary != NT_NUMBER)
			{
				yyerror("Invalid vnum component for widevnum.");
				YYERROR;
			}

			ins_code(NI_CONST0);
			ins_bytes($V.rhs, $V.rhs_len);
			ins_code(NI_LOAD_WIDEVNUM);

			$$.name = NULL;
			$$.type = nibtype_widevnum;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;

			nib_free_lvalue_s(&($V));
		}
	;

cast:	T_OPEN_PAREN type T_CLOSE_PAREN { $$ = $2; }
	;

name_lvalue:
		T_IDENTIFIER
		{
			NIB_VARIABLE *var;

			$$.lhs = NULL;
			$$.lhs_len = 0;
			$$.rhs = NULL;
			$$.rhs_len = 0;

			// Check global names first
			var = nib_get_global_variable($1);
			if (var)
			{
				// Store opcode for writing to global variable
				nib_bytecode_p q = $$.lhs = nib_alloc_bytecodes(3);
				$$.lhs_len = 3;

				q[0] = NI_LVALUE_GLOBAL;
				put_short(q+1, (short)var->id);

				q = $$.rhs = nib_alloc_bytecodes(3);
				$$.rhs_len = 3;

				q[0] = NI_LVALUE_GLOBAL;
				put_short(q+1, (short)var->id);
			}
			else
			{

				// Check for local name
				var = nib_get_local_variable($1);

				if (var)
				{
					// Store opcode for writing local variable
					// Store data for variable index
					nib_bytecode_p q = $$.lhs = nib_alloc_bytecodes(3);
					$$.lhs_len = 3;

					q[0] = NI_LVALUE_LOCAL;
					put_short(q+1, (short)var->id);

					q = $$.rhs = nib_alloc_bytecodes(3);
					$$.rhs_len = 3;

					q[0] = NI_LVALUE_LOCAL;
					put_short(q+1, (short)var->id);
				}
				else
				{
					niberrorf("Variable '%s' not declared within scope.", $1);
					YYERROR;
				}
			}

			$$.name = var->name;
			if (var->constant)
			{
				// Constant variables are never by reference
				$$.type = nib_type_copy(var->type);
				$$.flags = IS_READONLY | IS_LVALUE;
			}
			else
			{
				$$.type = nib_type_by_reference(var->type, true);
				$$.flags = IS_LVALUE;
			}

			nib_free($1);
		}
	;

lvalue:
		name_lvalue
		{
			$$ = $1;
		}
	;

function_call:
		T_IDENTIFIER[M]
		{
			$<address>$ = CURRENT_PROGRAM_SIZE;
		}
		optional_argument_list[A]
		{
			// Use the type of $C to find $M with $A type list
			NIB_METHOD *method = nib_method_get(NULL, $M, $A);

			if (!method)
			{
				niberrorf("No such function '%s' defined.", $M);
				YYERROR;
			}

			char comment[1000];
			nib_method_get_prototype(method, comment, sizeof(comment) - 1);
			nib_script_comment_add($<address>2, comment);

			ins_code(NI_CALL_FUNCTION);
			ins_short(method->id);
			ins_byte((unsigned char)list_size($A));
			
			NIB_TYPE *type = method->result;
			$$.type = nib_type_copy(type);
			$$.might_lvalue = false;	// TODO: FIX THIS
			$$.needs_use = (type && type->type_class != NTC_VOID);
			$$.needs_pop = (type && type->type_class != NTC_VOID);
			if (method->constant)
				$$.flags = IS_READONLY;
			else
				$$.flags = 0;

			if (type && type->_reference)
				$$.flags |= IS_LVALUE;

			nib_free($M);
			list_destroy($A);
		}
	;

field_call:
		expr0[L] T_DOT T_IDENTIFIER[F]
		{
			if ($L.type == NULL)
			{
				yyerror("Attempting to access an invalid type.");
				YYERROR;
			}

			if ($L.type->type_class == NTC_FLAG)
			{
				if ($L.type->_.flag.bits > 0)
				{
					yyerror("Invalid syntax to access a numerical flag.");
					YYERROR;
				}

				flag_value_t bit;

				// Verify the flag actually exists
				bool settable;
				if ($L.type->_.flag.table)
				{
					if (!nib_find_flag_value($L.type->_.flag.table, $F, &settable, &bit))
					{
						niberrorf("Flag '%s' is not defined.", $F);
						YYERROR;
					}
				}
				else
				{
					ITERATOR it;
					char *name;
					bit = (flag_value_t)1;
					iterator_start(&it, $L.type->_.flag.names);
					while((name = (char *)iterator_nextdata(&it)))
					{
						if (!str_cmp(name, $F))
							break;

						bit <<= 1;
					}
					iterator_stop(&it);

					if (!name)
					{
						niberrorf("Flag '%s' is not defined.", $F);
						YYERROR;
					}
				}

				// Instruction will pop the actual flag lvalue off the stack
				// This will create a flag lvalue focusing on the bit
				ins_code(NI_LVALUE_BIT);
				ins_long((long)bit);

				$$.name = NULL;
				$$.needs_use = true;
				if (IS_SET($L.flags, IS_READONLY) || !settable)
				{
					$$.type = nibtype_bool;
					$$.flags = IS_READONLY;
				}
				else
				{
					$$.type = nib_type_by_reference(nibtype_bool, true);
					$$.flags = 0;
				}

				if (IS_SET($L.flags, IS_LVALUE))
					$$.flags |= IS_LVALUE;
			}
			else if ($L.type->type_class == NTC_FLAG_BANK)
			{
				int bank;
				flag_value_t bit;
				bool settable;
				if (!nib_find_flagbank_value($L.type->_.flagbank.bank, $F, &settable, &bank, &bit))
				{
					niberrorf("Flag '%s' is not defined.", $F);
					YYERROR;
				}

				ins_code(NI_LVALUE_BIT_BANK);
				ins_byte(bank);
				ins_long((long)bit);

				$$.name = NULL;
				$$.needs_use = true;
				if (IS_SET($L.flags, IS_READONLY) || !settable)
				{
					$$.type = nibtype_bool;
					$$.flags = IS_READONLY;
				}
				else
				{
					$$.type = nib_type_by_reference(nibtype_bool, true);
					$$.flags = 0;
				}

				if (IS_SET($L.flags, IS_LVALUE))
					$$.flags |= IS_LVALUE;
			}
			else
			{
				// Search for $F on $L
				NIB_FIELD *field = nib_field_get($L.type, $F);
				if (!field)
				{
					niberrorf("No such field '%s' defined for type '%s'.",
						$F, nib_get_typename(NULL,$L.type));
					YYERROR;
				}

				// Deal with field methods that return any type
				NIB_TYPE *type = field->type;
				if (type == nibtype_any)
				{
					if ($L.type->type_class == NTC_LIST)
						type = $L.type->_.list.type;
					else if ($L.type->type_class == NTC_ARRAY)
						type = $L.type->_.array.type;
				}

				ins_code(NI_LVALUE_FIELD);
				ins_short(field->id);

				$$.name = $L.name;
				$$.needs_use = true;
				if (field->readonly || field->method)
				{
					$$.type = nib_type_copy(type);
					$$.flags = IS_READONLY;
				}
				else
				{
					$$.type = nib_type_by_reference(type, true);
					$$.flags = 0;
				}

				if (IS_SET($L.flags, IS_LVALUE) && field->lvalue)
					$$.flags |= IS_LVALUE;
			}

			free_nib_type($L.type);
			nib_free($F);

			// printf("%d, %d: $$.type = %p\n", __LINE__, niblineno, $$.type);
			// hex_dump($$.type,sizeof(NIB_TYPE));
		}
	|	expr0[L] T_DOT T_NUMBER[B]
		{
			if ($L.type == NULL || $L.type->type_class != NTC_FLAG)
			{
				yyerror("Attempting to access an invalid type.");
				YYERROR;
			}

			if ($L.type->_.flag.bits < 1)
			{
				yyerror("Invalid syntax to access a named flag.");
				YYERROR;
			}

			if ($B < 0 || $B >= $L.type->_.flag.bits)
			{
				niberrorf("Flag bit out of range for numerical flag: Range (0-%d)", $L.type->_.flag.bits - 1);
				YYERROR;
			}

			ins_code(NI_LVALUE_BIT);
			ins_long(1L << $B);

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			if (IS_SET($L.flags, IS_READONLY))
				$$.flags = IS_READONLY;
			else
				$$.flags = 0;

			if (IS_SET($L.flags, IS_LVALUE))
				$$.flags |= IS_LVALUE;

			free_nib_type($L.type);
		}
	;

method_call:
		expr0[C] T_DOT T_IDENTIFIER[M]
		{
			$<address>$ = CURRENT_PROGRAM_SIZE;
		}
		optional_argument_list[A]
		{
			if ($C.type == NULL || $C.type->type_class == NTC_VOID)
			{
				yyerror("Attempt to call a method on a void type.");
				YYERROR;
			}

			if ($C.type->type_class == NTC_ANY)
			{
				yyerror("Attempt to call a method on an unresolved type.");
				YYERROR;
			}

			// Use the type of $C to find $M with $A type list
			NIB_METHOD *method = nib_method_get($C.type, $M, $A);

			if (!method)
			{
				niberrorf("No such method '%s' found for '%s'.",
					$M, nib_get_typename(NULL,$C.type));
				YYERROR;
			}

			ins_code(NI_CALL_METHOD);
			ins_byte((unsigned char)convert_to_stype($C.type, false));
			ins_short(method->id);
			ins_byte((unsigned char)list_size($A));

			char comment[1000];
			nib_method_get_prototype(method, comment, sizeof(comment) - 1);
			nib_script_comment_add($<address>4, comment);

			NIB_TYPE *type = method->result;
			if (type != NULL && type->type_class == NTC_ANY)
			{
				if ($C.type->type_class == NTC_LIST)
					type = $C.type->_.list.type;	// Automatically assume it is the subtype of the list.
				else if ($C.type->type_class == NTC_ARRAY)
					type = $C.type->_.array.type;
			}

			$$.type = nib_type_copy(type);
			$$.might_lvalue = false;	// TODO: FIX THIS
			$$.needs_use = (type && type->type_class != NTC_VOID);
			$$.needs_pop = (type && type->type_class != NTC_VOID);
			if (method->constant)
				$$.flags = IS_READONLY;
			else
				$$.flags = 0;

			if (type && type->_reference)
				$$.flags |= IS_LVALUE;

			nib_free($M);
			list_destroy($A);
			free_nib_type($C.type);
		}
	;

optional_argument_list:
			T_OPEN_PAREN argument_list T_CLOSE_PAREN	{ $$ = $2; }
		|	T_OPEN_PAREN T_CLOSE_PAREN					{ $$ = nib_create_type_list(); }
	;

argument_list:
		expr0
		{
			$$ = nib_create_type_list();
			list_appendlink($$,nib_type_copy($1.type));
			free_nib_type($1.type);
		}
	|	argument_list T_COMMA expr0
		{
			list_appendlink($1,nib_type_copy($3.type));
			$$ = $1;
			free_nib_type($3.type);
		}
	;

boolean_value:
		T_TRUE				{ $$ = true; }
	|	T_FALSE				{ $$ = false; }
	;

type:	T_INT										{ $$ = nibtype_int; }
	| T_FLOAT										{ $$ = nibtype_float; }
	| T_BOOLEAN										{ $$ = nibtype_bool; }
	| T_CHAR										{ $$ = nibtype_char; }
	| T_STRING										{ $$ = nibtype_string; }
	| T_MAP											{ $$ = nibtype_map; }
	| T_FLAG T_OPEN_PAREN T_NUMBER[N] T_CLOSE_PAREN
		{
			if ($N < 1 || $N > MAX_FLAG_BITS)
			{
				niberrorf("Flag type only supports 1 to %d bits.", MAX_FLAG_BITS);
				YYERROR;
			}
			$$ = new_nib_type_flag($N);
		}
	| T_FLAG T_OPEN_PAREN flag_table[T] T_CLOSE_PAREN
		{
			$$ = new_nib_type_flag_table($T);
		}
	| T_FLAGBANK T_OPEN_PAREN flag_bank[B] T_CLOSE_PAREN
		{
			$$ = new_nib_type_flag_bank($B);
		}
	| T_STAT T_OPEN_PAREN stat_table[T] T_CLOSE_PAREN
		{
			$$ = new_nib_type_stat_table($T, false);
		}
	| T_LIST T_OPEN_PAREN possible_constant[C] listtype[T] T_CLOSE_PAREN	{ $$ = new_nib_type_list($T, $C); }
	| T_ARRAY T_OPEN_PAREN possible_constant[C] listtype[T] T_OPEN_BRACKET T_NUMBER[L] T_CLOSE_BRACKET T_CLOSE_PAREN
													{ $$ = new_nib_type_array($T,$L,$C); }
	| T_WIDEVNUM									{ $$ = nibtype_widevnum; }
	| T_ACCOUNT										{ $$ = nibtype_account; }
	| T_AFFECT										{ $$ = nibtype_affect; }
	| T_AREA										{ $$ = nibtype_area; }
	| T_CLASS										{ $$ = nibtype_class; }
	| T_DUNGEON										{ $$ = nibtype_dungeon; }
	| T_EXIT										{ $$ = nibtype_exit; }
	| T_INSTANCE									{ $$ = nibtype_instance; }
	| T_LIQUID										{ $$ = nibtype_liquid; }
	| T_MAIL										{ $$ = nibtype_mail; }
	| T_MATERIAL									{ $$ = nibtype_material; }
	| T_MISSION										{ $$ = nibtype_mission; }
	| T_MOBILE										{ $$ = nibtype_mobile; }
	| T_NOTE										{ $$ = nibtype_note; }
	| T_OBJECT										{ $$ = nibtype_object; }
	| T_ORG											{ $$ = nibtype_org; }
	| T_QUEST										{ $$ = nibtype_quest; }
	| T_RACE										{ $$ = nibtype_race; }
	| T_RANK										{ $$ = nibtype_rank; }
	| T_REPUTATION									{ $$ = nibtype_reputation; }
	| T_ROOM										{ $$ = nibtype_room; }
	| T_SHIP										{ $$ = nibtype_ship; }
	| T_SKILL										{ $$ = nibtype_skill; }
	| T_TOKEN										{ $$ = nibtype_token; }
	| T_WILDS										{ $$ = nibtype_wilds; }
	| T_WORLD										{ $$ = nibtype_world; }
	;

possible_constant:
		T_CONSTANT					{ $$ = true; }
	|	/* empty */					{ $$ = false; }
	;

flag_number_list:
		T_OPEN_FLAG comma_bit_list[F] T_CLOSE_FLAG		{ $$ = $F; }

comma_bit_list:
		T_NUMBER[B]
		{
			if ($B < 0 || $B >= MAX_FLAG_BITS)
			{
				niberrorf("Invalid bit number.  Range 0 to %d", MAX_FLAG_BITS - 1);
				YYERROR;
			}

			$$ = (flag_value_t)((flag_value_t)1 << $B);
		}
	|	comma_bit_list[F] T_COMMA T_NUMBER[B]
		{
			if ($B < 0 || $B >= MAX_FLAG_BITS)
			{
				niberrorf("Invalid bit number.  Range 0 to %d", MAX_FLAG_BITS - 1);
				YYERROR;
			}

			$$ = $F | (flag_value_t)((flag_value_t)1 << $B);
		}

flag_name_list: T_OPEN_FLAG comma_name_list T_CLOSE_FLAG	{
		if (list_size($2) > MAX_FLAG_BITS)
		{
			niberrorf("Flag name lists only support %d names.  Encountered %d instead.",
				MAX_FLAG_BITS, list_size($2));
			YYERROR;
		}

		$$ = $2;
	}

comma_name_list:
	T_IDENTIFIER[I]
	{
		$$ = nib_create_string_list();
		list_appendlink($$, $I);
	}
	| comma_name_list[L] T_COMMA T_IDENTIFIER[I]
	{
		list_appendlink($L, $I);
		$$ = $L;
	}
	

listtype:	T_INT										{ $$ = nibtype_int; }
	| T_FLOAT										{ $$ = nibtype_float; }
	| T_BOOLEAN										{ $$ = nibtype_bool; }
	| T_CHAR										{ $$ = nibtype_char; }
	| T_STRING										{ $$ = nibtype_string; }
	| T_MAP											{ $$ = nibtype_map; }
	| T_WIDEVNUM									{ $$ = nibtype_widevnum; }
	| T_ACCOUNT										{ $$ = nibtype_account; }
	| T_AFFECT										{ $$ = nibtype_affect; }
	| T_AREA										{ $$ = nibtype_area; }
	| T_CLASS										{ $$ = nibtype_class; }
	| T_DUNGEON										{ $$ = nibtype_dungeon; }
	| T_EXIT										{ $$ = nibtype_exit; }
	| T_INSTANCE									{ $$ = nibtype_instance; }
	| T_LIQUID										{ $$ = nibtype_liquid; }
	| T_MAIL										{ $$ = nibtype_mail; }
	| T_MATERIAL									{ $$ = nibtype_material; }
	| T_MISSION										{ $$ = nibtype_mission; }
	| T_MOBILE										{ $$ = nibtype_mobile; }
	| T_NOTE										{ $$ = nibtype_note; }
	| T_OBJECT										{ $$ = nibtype_object; }
	| T_ORG											{ $$ = nibtype_org; }
	| T_QUEST										{ $$ = nibtype_quest; }
	| T_RACE										{ $$ = nibtype_race; }
	| T_RANK										{ $$ = nibtype_rank; }
	| T_REPUTATION									{ $$ = nibtype_reputation; }
	| T_ROOM										{ $$ = nibtype_room; }
	| T_SHIP										{ $$ = nibtype_ship; }
	| T_SKILL										{ $$ = nibtype_skill; }
	| T_TOKEN										{ $$ = nibtype_token; }
	| T_WILDS										{ $$ = nibtype_wilds; }
	| T_WORLD										{ $$ = nibtype_world; }
	;

flag_table:
		table_name
			{
				const struct flag_type *table = nib_lookup_flag_table(nib_flag_created_tables,$1);
				if (!table)
				{
					niberrorf("Unknown flag table '%s'", $1);
					YYERROR;
				}

				$$ = table;
				nib_free($1);
			}
	;

flag_bank:
		table_name
			{
				const struct flag_type **bank = nib_lookup_flag_bank($1);
				if (!bank)
				{
					niberrorf("Unknown flag bank '%s'", $1);
					YYERROR;
				}

				$$ = bank;
				nib_free($1);
			}
	;

stat_table:
		table_name
			{
				const struct flag_type *table = nib_lookup_stat_table(nib_stat_created_tables,$1);
				if (!table)
				{
					niberrorf("Unknown stat table '%s'", $1);
					YYERROR;
				}

				$$ = table;
				nib_free($1);
			}
	;

table_name:
		T_IDENTIFIER
		{
			$$ = $1;
		}
	|	T_STRING_LITERAL
		{
			$$ = $1;
		}

%%
