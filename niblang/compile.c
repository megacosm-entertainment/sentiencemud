#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <inttypes.h>
#include <stdint.h>


#include "../merc.h"
#include "niblang.h"
#include "script.h"

// HACK to fix the YYSTYPE for flex / bison interaction with regards to adding a name prefix
#define YYSTYPE NIBSTYPE
#define YYLTYPE NIBLTYPE

#include "yacc/parser.h"
#include "yacc/lexer.h"

int nibparse();



NIB_BUFFER *nib_program_storage = NULL;
// Variable Storage
LLIST *nib_global_variables = NULL;
LLIST *nib_local_variables = NULL;

// String Storage
LLIST *nib_string_storage = NULL;

// Comment Storage
LLIST *nib_comment_storage = NULL;

LLIST *nib_flag_created_tables = NULL;
LLIST *nib_stat_created_tables = NULL;

LLIST *nib_used_tables = NULL;

LLIST *nib_used_banks = NULL;

LLIST *nib_switch_blocks = NULL;

struct nib_break_s *nib_break_address = NULL;
struct nib_continue_s *nib_continue_address = NULL;
struct nib_compile_switch_s *nib_current_switch = NULL;

// Compilation Flags

void free_nib_compile_switch_case(struct nib_compile_switch_case_s *cs)
{
	if (cs)
	{
		if (cs->a.str) nib_free(cs->a.str);
		nib_free(cs);
	}
}

static void __free_switch_case(void *data)
{
	free_nib_compile_switch_case((struct nib_compile_switch_case_s *)data);
}

void free_nib_bc_statement(struct nib_bc_statment_s *stmt)
{
	if (stmt)
	{
		free_nib_bc_statement(stmt->next);
		nib_free(stmt);
	}
}

static int __compare_numbers(struct nib_compile_switch_case_s *a, struct nib_compile_switch_case_s *b)
{
	// Compare versus ranges
	// a = element in the list
	// b = new element

	switch(a->type)
	{
	case NCASE_VALUE:		// Single value
		switch(b->type)
		{
		case NCASE_VALUE:		// Single value
			return b->a.number - a->a.number;
		case NCASE_VX:			// Minimum
			if (a->a.number < b->a.number)
				return 1;
			else
				return 0;
		case NCASE_XV:			// Maximum
			if (a->a.number > b->b.number)
				return -1;
			else
				return 0;
		case NCASE_VV:
			if (a->a.number < b->a.number)
				return 1;
			else if (a->a.number > b->b.number)
				return -1;
			else
				return 0;	// Should never get to this point as it would mean they overlap
		}
		break;
	
	case NCASE_VX:			// Minimum
		switch(b->type)
		{
		case NCASE_VALUE:		// Single value
			if (a->a.number > b->a.number)
				return -1;
			else
				return 0;
		case NCASE_VX:			// Minimum
			return 0;		// These always overlap, so order is irrelevant
		case NCASE_XV:			// Maximum
		case NCASE_VV:			// Range
			if (a->a.number > b->b.number)
				return -1;
			else
				return 0;	// Overlap
		}
		break;
		
	case NCASE_XV:			// Maximum
		switch(b->type)
		{
		case NCASE_VALUE:		// Single value
			if (a->b.number < b->a.number)
				return 1;
			else
				return 0;
		case NCASE_XV:			// Maximum
			return 0;		// Overlap
		case NCASE_VX:			// Minimum
		case NCASE_VV:
			if (a->b.number < b->a.number)
				return 1;
			else
				return 0;	// overlap
		}
		break;
	case NCASE_VV:
		switch(b->type)
		{
		case NCASE_VALUE:		// Single value
			if (a->b.number < b->a.number)
				return 1;
			else if (a->a.number > b->a.number)
				return -1;
			else
				return 0;
		case NCASE_VX:			// Minimum
			if (a->b.number < b->a.number)
				return 1;
			else
				return 0;
		case NCASE_XV:			// Maximum
			if (a->a.number > b->b.number)
				return -1;
			else
				return 0;
		case NCASE_VV:
			if (a->b.number < b->a.number)
				return 1;
			if (a->a.number > b->b.number)
				return -1;
			return 0;	// overlap
		}
		break;
	}

	return 0;
}

static int __compare_floats(struct nib_compile_switch_case_s *a, struct nib_compile_switch_case_s *b)
{
	// Compare versus ranges
	// a = element in the list
	// b = new element

	switch(a->type)
	{
	case NCASE_VALUE:		// Single value
		switch(b->type)
		{
		case NCASE_VALUE:		// Single value
			return b->a.flt - a->a.flt;
		case NCASE_VX:			// Minimum
			if (a->a.flt < b->a.flt)
				return 1;
			else
				return 0;
		case NCASE_XV:			// Maximum
			if (a->a.flt > b->b.flt)
				return -1;
			else
				return 0;
		case NCASE_VV:
			if (a->a.flt < b->a.flt)
				return 1;
			else if (a->a.flt > b->b.flt)
				return -1;
			else
				return 0;	// Should never get to this point as it would mean they overlap
		}
		break;
	
	case NCASE_VX:			// Minimum
		switch(b->type)
		{
		case NCASE_VALUE:		// Single value
			if (a->a.flt > b->a.flt)
				return -1;
			else
				return 0;
		case NCASE_VX:			// Minimum
			return 0;		// These always overlap, so order is irrelevant
		case NCASE_XV:			// Maximum
		case NCASE_VV:			// Range
			if (a->a.flt > b->b.flt)
				return -1;
			else
				return 0;	// Overlap
		}
		break;
		
	case NCASE_XV:			// Maximum
		switch(b->type)
		{
		case NCASE_VALUE:		// Single value
			if (a->b.flt < b->a.flt)
				return 1;
			else
				return 0;
		case NCASE_XV:			// Maximum
			return 0;		// Overlap
		case NCASE_VX:			// Minimum
		case NCASE_VV:
			if (a->b.flt < b->a.flt)
				return 1;
			else
				return 0;	// overlap
		}
		break;
	case NCASE_VV:
		switch(b->type)
		{
		case NCASE_VALUE:		// Single value
			if (a->b.flt < b->a.flt)
				return 1;
			else if (a->a.flt > b->a.flt)
				return -1;
			else
				return 0;
		case NCASE_VX:			// Minimum
			if (a->b.flt < b->a.flt)
				return 1;
			else
				return 0;
		case NCASE_XV:			// Maximum
			if (a->a.flt > b->b.flt)
				return -1;
			else
				return 0;
		case NCASE_VV:
			if (a->b.flt < b->a.flt)
				return 1;
			if (a->a.flt > b->b.flt)
				return -1;
			return 0;	// overlap
		}
		break;
	}

	return 0;
}

static int __compare_chars(struct nib_compile_switch_case_s *a, struct nib_compile_switch_case_s *b)
{
	// Compare versus ranges
	// a = element in the list
	// b = new element

	switch(a->type)
	{
	case NCASE_VALUE:		// Single value
		switch(b->type)
		{
		case NCASE_VALUE:		// Single value
			return b->a.ch - a->a.ch;
		case NCASE_VX:			// Minimum
			if (a->a.ch < b->a.ch)
				return 1;
			else
				return 0;
		case NCASE_XV:			// Maximum
			if (a->a.ch > b->b.ch)
				return -1;
			else
				return 0;
		case NCASE_VV:
			if (a->a.ch < b->a.ch)
				return 1;
			else if (a->a.ch > b->b.ch)
				return -1;
			else
				return 0;	// Should never get to this point as it would mean they overlap
		}
		break;
	
	case NCASE_VX:			// Minimum
		switch(b->type)
		{
		case NCASE_VALUE:		// Single value
			if (a->a.ch > b->a.ch)
				return -1;
			else
				return 0;
		case NCASE_VX:			// Minimum
			return 0;		// These always overlap, so order is irrelevant
		case NCASE_XV:			// Maximum
		case NCASE_VV:			// Range
			if (a->a.ch > b->b.ch)
				return -1;
			else
				return 0;	// Overlap
		}
		break;
		
	case NCASE_XV:			// Maximum
		switch(b->type)
		{
		case NCASE_VALUE:		// Single value
			if (a->b.ch < b->a.ch)
				return 1;
			else
				return 0;
		case NCASE_XV:			// Maximum
			return 0;		// Overlap
		case NCASE_VX:			// Minimum
		case NCASE_VV:
			if (a->b.ch < b->a.ch)
				return 1;
			else
				return 0;	// overlap
		}
		break;
	case NCASE_VV:
		switch(b->type)
		{
		case NCASE_VALUE:		// Single value
			if (a->b.ch < b->a.ch)
				return 1;
			else if (a->a.ch > b->a.ch)
				return -1;
			else
				return 0;
		case NCASE_VX:			// Minimum
			if (a->b.ch < b->a.ch)
				return 1;
			else
				return 0;
		case NCASE_XV:			// Maximum
			if (a->a.ch > b->b.ch)
				return -1;
			else
				return 0;
		case NCASE_VV:
			if (a->b.ch < b->a.ch)
				return 1;
			if (a->a.ch > b->b.ch)
				return -1;
			return 0;	// overlap
		}
		break;
	}

	return 0;
}

static int __compare_strings(struct nib_compile_switch_case_s *a, struct nib_compile_switch_case_s *b)
{
	return utf8_str_cmp(b->a.str,a->a.str);
}


bool push_nib_switch_block(SWITCH_TYPE type, const struct flag_type *table)
{
	struct nib_compile_switch_s *sw = nib_calloc(1, sizeof(struct nib_compile_switch_s));
	if (!sw) return false;

	sw->prev = nib_current_switch;
	nib_current_switch = sw;

	list_appendlink(nib_switch_blocks, sw);
	sw->id = list_size(nib_switch_blocks);

	sw->type = type;
	sw->table = table;
	sw->cases = list_createx(false,NULL,__free_switch_case);
	switch(type)
	{
	case NSWT_NUMBER:	sw->sorter = __compare_numbers; break;
	case NSWT_FLOAT:	sw->sorter = __compare_floats; break;
	case NSWT_CHAR:		sw->sorter = __compare_chars; break;
	case NSWT_STRING:	sw->sorter = NULL; break;
	case NSWT_STAT:		sw->sorter = __compare_numbers; break;
	}

	return true;
}

void pop_nib_switch_block()
{
	if (nib_current_switch)
	{
		nib_current_switch = nib_current_switch->prev;

		// Nothing to free here
	}
}

static bool nib_switch_overlaps_case_number(struct nib_compile_switch_case_s *c)
{
	bool overlaps = false;
	ITERATOR it;
	struct nib_compile_switch_case_s *cs;
	iterator_start(&it, nib_current_switch->cases);
	while(!overlaps && (cs = (struct nib_compile_switch_case_s *)iterator_nextdata(&it)))
	{
		switch(c->type)
		{
		case NCASE_VALUE:		// Single value
			switch(cs->type)
			{
			case NCASE_VALUE:		// Single value
				overlaps = c->a.number == cs->a.number;
				break;
			case NCASE_VX:			// Minimum
				overlaps = c->a.number >= cs->a.number;
				break;
			case NCASE_XV:			// Maximum
				overlaps = c->a.number <= cs->b.number;
				break;
			case NCASE_VV:			// Range
				overlaps = (c->a.number >= cs->a.number) &&
							(c->a.number <= cs->b.number);
				break;
			}
			break;

		case NCASE_VX:			// Minimum
			switch(cs->type)
			{
			case NCASE_VALUE:		// Single value
				// 1.. overlaps with 2
				overlaps = c->a.number <= cs->a.number;
				break;
			case NCASE_VX:			// Minimum
				// 1.. overlaps with 2..
				overlaps = true;	// Always overlaps
				break;
			case NCASE_XV:			// Maximum
				// 1.. overlaps with ..4
			case NCASE_VV:			// Range
				// 1.. overlaps with 2..4 and 0..4
				overlaps = c->a.number <= cs->b.number;
				break;
			}
			break;

		case NCASE_XV:			// Maximum
			switch(cs->type)
			{
			case NCASE_VALUE:		// Single value
				// ..2 overlaps with 1
				overlaps = c->b.number >= cs->a.number;
				break;
			case NCASE_XV:			// Maximum
				// ..1 overlaps with ..2
				overlaps = true;	// Always overlaps
				break;
			case NCASE_VX:			// Minimum
				// ..2 overlaps with 1..
			case NCASE_VV:			// Range
				// ..3 overlaps with 1..4 and 1..2
				overlaps = c->b.number >= cs->a.number;
				break;
			}
			break;
		case NCASE_VV:			// Range
			switch(cs->type)
			{
			case NCASE_VALUE:		// Single value
				// 1..2 overlaps with 1
				overlaps = (c->a.number <= cs->a.number) &&
							(c->b.number >= cs->a.number);
				break;
			case NCASE_VX:			// Minimum
				// 3..5 overlaps with 4.. and 1..
				overlaps = c->b.number >= cs->a.number;
				break;
			case NCASE_XV:			// Maximum
				// 3..5 overlaps with ..4 and ..6
				overlaps = c->a.number <= cs->b.number;
				break;
			case NCASE_VV:			// Range
				// 3..5 overlaps 4..6
				overlaps = (c->a.number <= cs->b.number) &&
							(c->b.number >= cs->a.number);
				break;
			}
			break;
		}
	}
	iterator_stop(&it);

	return overlaps;
}

static bool nib_switch_overlaps_case_float(struct nib_compile_switch_case_s *c)
{
	bool overlaps = false;
	ITERATOR it;
	struct nib_compile_switch_case_s *cs;
	iterator_start(&it, nib_current_switch->cases);
	while(!overlaps && (cs = (struct nib_compile_switch_case_s *)iterator_nextdata(&it)))
	{
		switch(c->type)
		{
		case NCASE_VALUE:		// Single value
			switch(cs->type)
			{
			case NCASE_VALUE:		// Single value
				overlaps = c->a.flt == cs->a.flt;
				break;
			case NCASE_VX:			// Minimum
				overlaps = c->a.flt >= cs->a.flt;
				break;
			case NCASE_XV:			// Maximum
				overlaps = c->a.flt <= cs->b.flt;
				break;
			case NCASE_VV:			// Range
				overlaps = (c->a.flt >= cs->a.flt) &&
							(c->a.flt <= cs->b.flt);
				break;
			}
			break;

		case NCASE_VX:			// Minimum
			switch(cs->type)
			{
			case NCASE_VALUE:		// Single value
				// 1.. overlaps with 2
				overlaps = c->a.flt <= cs->a.flt;
				break;
			case NCASE_VX:			// Minimum
				// 1.. overlaps with 2..
				overlaps = true;	// Always overlaps
				break;
			case NCASE_XV:			// Maximum
				// 1.. overlaps with ..4
			case NCASE_VV:			// Range
				// 1.. overlaps with 2..4 and 0..4
				overlaps = c->a.flt <= cs->b.flt;
				break;
			}
			break;

		case NCASE_XV:			// Maximum
			switch(cs->type)
			{
			case NCASE_VALUE:		// Single value
				// ..2 overlaps with 1
				overlaps = c->b.flt >= cs->a.flt;
				break;
			case NCASE_XV:			// Maximum
				// ..1 overlaps with ..2
				overlaps = true;	// Always overlaps
				break;
			case NCASE_VX:			// Minimum
				// ..2 overlaps with 1..
			case NCASE_VV:			// Range
				// ..3 overlaps with 1..4 and 1..2
				overlaps = c->b.flt >= cs->a.flt;
				break;
			}
			break;
		case NCASE_VV:			// Range
			switch(cs->type)
			{
			case NCASE_VALUE:		// Single value
				// 1..2 overlaps with 1
				overlaps = (c->a.flt <= cs->a.flt) &&
							(c->b.flt >= cs->a.flt);
				break;
			case NCASE_VX:			// Minimum
				// 3..5 overlaps with 4.. and 1..
				overlaps = c->b.flt >= cs->a.flt;
				break;
			case NCASE_XV:			// Maximum
				// 3..5 overlaps with ..4 and ..6
				overlaps = c->a.flt <= cs->b.flt;
				break;
			case NCASE_VV:			// Range
				// 3..5 overlaps 4..6
				overlaps = (c->a.flt <= cs->b.flt) &&
							(c->b.flt >= cs->a.flt);
				break;
			}
			break;
		}
	}
	iterator_stop(&it);

	return overlaps;
}

static bool nib_switch_overlaps_case_char(struct nib_compile_switch_case_s *c)
{
	bool overlaps = false;
	ITERATOR it;
	struct nib_compile_switch_case_s *cs;
	iterator_start(&it, nib_current_switch->cases);
	while(!overlaps && (cs = (struct nib_compile_switch_case_s *)iterator_nextdata(&it)))
	{
		switch(c->type)
		{
		case NCASE_VALUE:		// Single value
			switch(cs->type)
			{
			case NCASE_VALUE:		// Single value
				overlaps = c->a.ch == cs->a.ch;
				break;
			case NCASE_VX:			// Minimum
				overlaps = c->a.ch >= cs->a.ch;
				break;
			case NCASE_XV:			// Maximum
				overlaps = c->a.ch <= cs->b.ch;
				break;
			case NCASE_VV:			// Range
				overlaps = (c->a.ch >= cs->a.ch) &&
							(c->a.ch <= cs->b.ch);
				break;
			}
			break;

		case NCASE_VX:			// Minimum
			switch(cs->type)
			{
			case NCASE_VALUE:		// Single value
				// 1.. overlaps with 2
				overlaps = c->a.ch <= cs->a.ch;
				break;
			case NCASE_VX:			// Minimum
				// 1.. overlaps with 2..
				overlaps = true;	// Always overlaps
				break;
			case NCASE_XV:			// Maximum
				// 1.. overlaps with ..4
			case NCASE_VV:			// Range
				// 1.. overlaps with 2..4 and 0..4
				overlaps = c->a.ch <= cs->b.ch;
				break;
			}
			break;

		case NCASE_XV:			// Maximum
			switch(cs->type)
			{
			case NCASE_VALUE:		// Single value
				// ..2 overlaps with 1
				overlaps = c->b.ch >= cs->a.ch;
				break;
			case NCASE_XV:			// Maximum
				// ..1 overlaps with ..2
				overlaps = true;	// Always overlaps
				break;
			case NCASE_VX:			// Minimum
				// ..2 overlaps with 1..
			case NCASE_VV:			// Range
				// ..3 overlaps with 1..4 and 1..2
				overlaps = c->b.ch >= cs->a.ch;
				break;
			}
			break;
		case NCASE_VV:			// Range
			switch(cs->type)
			{
			case NCASE_VALUE:		// Single value
				// 1..2 overlaps with 1
				overlaps = (c->a.ch <= cs->a.ch) &&
							(c->b.ch >= cs->a.ch);
				break;
			case NCASE_VX:			// Minimum
				// 3..5 overlaps with 4.. and 1..
				overlaps = c->b.ch >= cs->a.ch;
				break;
			case NCASE_XV:			// Maximum
				// 3..5 overlaps with ..4 and ..6
				overlaps = c->a.ch <= cs->b.ch;
				break;
			case NCASE_VV:			// Range
				// 3..5 overlaps 4..6
				overlaps = (c->a.ch <= cs->b.ch) &&
							(c->b.ch >= cs->a.ch);
				break;
			}
			break;
		}
	}
	iterator_stop(&it);

	return overlaps;
}

static bool nib_switch_overlaps_case_string(struct nib_compile_switch_case_s *c)
{
	if (c->type != NCASE_VALUE) return false;	// Only care about exact strings

	bool overlaps = false;
	ITERATOR it;
	struct nib_compile_switch_case_s *cs;
	iterator_start(&it, nib_current_switch->cases);
	while((cs = (struct nib_compile_switch_case_s *)iterator_nextdata(&it)))
	{
		if (cs->type == NCASE_VALUE && !utf8_str_cmp(c->a.str,cs->a.str))
			break;
	}
	iterator_stop(&it);

	return cs != NULL;
}

static bool nib_switch_overlaps_case_stat(struct nib_compile_switch_case_s *c)
{
	if (c->type != NCASE_VALUE) return false;	// Only care about exact strings

	bool overlaps = false;
	ITERATOR it;
	struct nib_compile_switch_case_s *cs;
	iterator_start(&it, nib_current_switch->cases);
	while((cs = (struct nib_compile_switch_case_s *)iterator_nextdata(&it)))
	{
		if (cs->type == NCASE_VALUE && c->a.number == cs->a.number)
			break;
	}
	iterator_stop(&it);

	return cs != NULL;
}

struct nib_compile_switch_case_s *new_nib_compile_switch_case()
{
	return nib_calloc(1,sizeof(struct nib_compile_switch_case_s));
}

bool nib_switch_overlaps_case(struct nib_compile_switch_case_s *c)
{
	switch(nib_current_switch->type)
	{
	case NSWT_NUMBER:	return nib_switch_overlaps_case_number(c);
	case NSWT_FLOAT:	return nib_switch_overlaps_case_float(c);
	case NSWT_CHAR:		return nib_switch_overlaps_case_char(c);
	case NSWT_STRING:	return nib_switch_overlaps_case_string(c);
	case NSWT_STAT:		return nib_switch_overlaps_case_stat(c);
	default:			return false;
	}
}

void nib_switch_add_case(struct nib_compile_switch_case_s *c)
{
	struct nib_compile_switch_case_s *cs = NULL;
	if (nib_current_switch->sorter)
	{
		ITERATOR it;
		iterator_start(&it,nib_current_switch->cases);
		while((cs = (struct nib_compile_switch_case_s *)iterator_nextdata(&it)))
		{
			int cmp = (*(nib_current_switch->sorter))(cs, c);

			if (cmp < 0)
			{
				iterator_insert_before(&it, c);
				break;
			}
		}
		iterator_stop(&it);
	}

	if (!cs)
		list_appendlink(nib_current_switch->cases, c);
}

void push_nib_break_address()
{
	struct nib_break_s *ba = nib_calloc(1, sizeof(struct nib_break_s));

	ba->prev = nib_break_address;
	nib_break_address = ba;
}

void push_nib_break_statement(int address)
{
	struct nib_bc_statment_s *stmt = nib_calloc(1, sizeof(struct nib_bc_statment_s));

	stmt->address = address;
	stmt->next = nib_break_address->stmts;
	nib_break_address->stmts = stmt;
}

void update_nib_break_statements(int address)
{
	if (nib_break_address)
	{
		struct nib_bc_statment_s *stmt = nib_break_address->stmts;

		while(stmt)
		{
			memcpy(nib_program_storage->buffer + stmt->address, &address, sizeof(address));

			stmt = stmt->next;
		}
	}
}


void push_nib_continue_address()
{
	struct nib_continue_s *ca = nib_calloc(1, sizeof(struct nib_continue_s));

	ca->prev = nib_continue_address;
	nib_continue_address = ca;
}

void push_nib_continue_statement(int address)
{
	struct nib_bc_statment_s *stmt = nib_calloc(1, sizeof(struct nib_bc_statment_s));

	stmt->address = address;
	stmt->next = nib_continue_address->stmts;
	nib_continue_address->stmts = stmt;
}

void update_nib_continue_statements(int address)
{
	if (nib_continue_address)
	{
		struct nib_bc_statment_s *stmt = nib_continue_address->stmts;

		while(stmt)
		{
			memcpy(nib_program_storage->buffer + stmt->address, &address, sizeof(address));

			stmt = stmt->next;
		}
	}
}


void pop_nib_break_address()
{
	if (nib_break_address)
	{
		struct nib_break_s *ba = nib_break_address->prev;
		free_nib_bc_statement(nib_break_address->stmts);
		nib_free(nib_break_address);
		nib_break_address = ba;
	}
}

void pop_nib_continue_address()
{
	if (nib_continue_address)
	{
		struct nib_continue_s *ca = nib_continue_address->prev;
		free_nib_bc_statement(nib_continue_address->stmts);
		nib_free(nib_continue_address);
		nib_continue_address = ca;
	}
}

NIB_SCRIPT_COMMENT *new_nib_script_comment(long address, char *comment)
{
	NIB_SCRIPT_COMMENT *data = nib_calloc(1, sizeof(NIB_SCRIPT_COMMENT));

	if (data)
	{
		data->address = address;
		data->comment = nib_strdup(comment);
	}

	return data;
}

NIB_SCRIPT_COMMENT *copy_nib_script_comment(NIB_SCRIPT_COMMENT *src)
{
	if (!src) return NULL;

	NIB_SCRIPT_COMMENT *data = nib_calloc(1, sizeof(NIB_SCRIPT_COMMENT));

	if (data)
	{
		data->address = src->address;
		data->comment = nib_strdup(src->comment);
	}

	return data;
}


void nib_script_comment_add(long address, char *comment)
{
	NIB_SCRIPT_COMMENT *data = new_nib_script_comment(address, comment);

	if (data)
	{
		list_appendlink(nib_comment_storage, data);
	}
}

static void __free_switch(void *data)
{
	struct nib_compile_switch_s *sw = (struct nib_compile_switch_s *)data;

	if (sw)
	{
		list_destroy(sw->cases);
		nib_free(sw);
	}
}

static void *__copy_comment(void *src)
{
	return copy_nib_script_comment((NIB_SCRIPT_COMMENT *)src);
}

static void __free_comment(void *data)
{
	if (!data) return;

	NIB_SCRIPT_COMMENT *comment = (NIB_SCRIPT_COMMENT *)data;

	nib_free(comment->comment);
	nib_free(comment);
}

static void __free_string(void *data)
{
	if (data) nib_free(data);
}

static void *__copy_string(void *data)
{
	if (!data) return NULL;

	return nib_strdup((char *)data);
}

static void __free_variable(void *data)
{
	if (data) nib_free_variable((NIB_VARIABLE *)data);
}

static void *__copy_variable(void *data)
{
	return nib_copy_variable((NIB_VARIABLE *)data);	
}

static void __free_nibtype(void *data)
{
	if (data) free_nib_type((NIB_TYPE *)data);
}

static void *__copy_nibtype(void *data)
{
	return nib_type_copy((NIB_TYPE *)data);
}

static void __free_flag_table(void *data)
{
	if (data) nib_flag_free_table((struct flag_type_lookup *)data);
}

static void *__copy_flag_table(void *data)
{
	return nib_flag_copy_table((struct flag_type_lookup *)data);
}


LLIST *nib_create_comment_list()
{
	return list_createx(false, __copy_comment, __free_comment);
}

LLIST *nib_create_string_list()
{
	return list_createx(false, __copy_string, __free_string);
}

LLIST *nib_create_variable_list()
{
	return list_createx(false, __copy_variable, __free_variable);
}

LLIST *nib_create_type_list()
{
	return list_createx(false, __copy_nibtype, __free_nibtype);
}

LLIST *nib_create_flag_table_list()
{
	return list_createx(false, __copy_flag_table, __free_flag_table);
}

LLIST *nib_create_switch_list()
{
	return list_createx(false, NULL, __free_switch);
}

void nib_init_scopetree();
bool nib_init_compile()
{
	nib_init_scopetree();

	nib_program_storage = new_mem_buffer();
	if (!nib_program_storage) return false;

	nib_global_variables = nib_create_variable_list();
	if (!list_isvalid(nib_global_variables)) return false;

	nib_local_variables = nib_create_variable_list();
	if (!list_isvalid(nib_local_variables)) return false;

	nib_string_storage = nib_create_string_list();
	if (!list_isvalid(nib_string_storage)) return false;

	nib_comment_storage = nib_create_comment_list();
	if (!list_isvalid(nib_comment_storage)) return false;

	nib_flag_created_tables = nib_create_flag_table_list();
	if (!list_isvalid(nib_flag_created_tables)) return false;

	nib_stat_created_tables = nib_create_flag_table_list();
	if (!list_isvalid(nib_stat_created_tables)) return false;

	nib_used_tables = list_create(false);	// Will hold (struct flag_type *)
	if (!list_isvalid(nib_used_tables)) return false;

	nib_used_banks = list_create(false);	// Will hold (struct flag_type **)
	if (!list_isvalid(nib_used_banks)) return false;

	nib_switch_blocks = nib_create_switch_list();
	if (!list_isvalid(nib_switch_blocks)) return false;

	return true;
}

void nib_cleanup_scopetree();
void nib_cleanup_compile()
{
	free_mem_buffer(nib_program_storage);
	list_destroy(nib_global_variables);
	list_destroy(nib_local_variables);
	list_destroy(nib_string_storage);
	list_destroy(nib_comment_storage);
	list_destroy(nib_flag_created_tables);
	list_destroy(nib_stat_created_tables);
	list_destroy(nib_used_tables);
	list_destroy(nib_used_banks);
	list_destroy(nib_switch_blocks);

	nib_program_storage = NULL;
	nib_global_variables = NULL;
	nib_local_variables = NULL;
	nib_string_storage = NULL;
	nib_comment_storage = NULL;
	nib_flag_created_tables = NULL;
	nib_stat_created_tables = NULL;
	nib_used_tables = NULL;
	nib_used_banks = NULL;
	nib_switch_blocks = NULL;

	nib_cleanup_scopetree();
}

NIB_SCRIPT_CLASS nib_compile_script_class;

NIB_SCRIPT *nib_compile_script(const char *src, NIB_SCRIPT_CLASS sc)
{
	if(!nib_init_compile()) return NULL;

	YY_BUFFER_STATE state = nib_scan_string(src);

	nib_compile_script_class = sc;

	if (nibparse()) {
		/* error parsing */
		return NULL;
	}

	nib_delete_buffer(state);

	return new_nib_script(src, nib_compile_script_class);
}

// Used to process the string into a compiled form for handling escape sequences
char *compile_string_literal(const char *src)
{
	return nib_strdup(src);
}

void nib_dump_program()
{
	if (nib_program_storage)
	{
		printf("Program:\n");
		hex_dump(nib_program_storage->buffer, nib_program_storage->len);
		printf("\n");
	}
}

void nib_dump_global_variables()
{
	printf("Global Variables:\n");
	printf("Scope  ID     Name              C  Type\n");
	printf("============================================\n");
	ITERATOR it;
	NIB_VARIABLE *var;
	iterator_start(&it, nib_global_variables);
	while((var = (NIB_VARIABLE *)iterator_nextdata(&it)))
	{
		printf("%-5d  %-5d  %-16.16s  %c  %s\n", var->scope, var->id, var->name,
			(var->constant ? 'Y' : 'N'),
			nib_get_typename(NULL,var->type));
	}

	iterator_stop(&it);
	printf("\n");
}

void nib_dump_local_variables()
{
	printf("Local Variables:\n");
	printf("Scope  ID     Name              C  Type\n");
	printf("============================================\n");
	ITERATOR it;
	NIB_VARIABLE *var;
	iterator_start(&it, nib_local_variables);
	while((var = (NIB_VARIABLE *)iterator_nextdata(&it)))
	{
		printf("%-5d  %-5d  %-16.16s  %c  %s\n", var->scope, var->id, var->name,
			(var->constant ? 'Y' : 'N'),
			nib_get_typename(NULL,var->type));
	}

	iterator_stop(&it);
	printf("\n");
}

NIB_VARIABLE *nib_get_global_variable_byid(short id)
{
	ITERATOR it;
	NIB_VARIABLE *var;
	iterator_start(&it, nib_global_variables);
	while((var = (NIB_VARIABLE *)iterator_nextdata(&it)))
	{
		if (var->id == id)
			break;
	}

	iterator_stop(&it);

	return var;
}

NIB_VARIABLE *nib_get_global_variable(const char *name)
{
	ITERATOR it;
	NIB_VARIABLE *var;
	iterator_start(&it, nib_global_variables);
	while((var = (NIB_VARIABLE *)iterator_nextdata(&it)))
	{
		if (!str_cmp(var->name, name))
			break;
	}

	iterator_stop(&it);

	return var;
}

NIB_VARIABLE *nib_get_local_variable_byid(short id)
{
	ITERATOR it;
	NIB_VARIABLE *var;
	iterator_start(&it, nib_local_variables);
	while((var = (NIB_VARIABLE *)iterator_nextdata(&it)))
	{
		if (var->id == id)
			break;
	}

	iterator_stop(&it);

	return var;
}


// Gets the local variable closest to the current scope
NIB_VARIABLE *nib_get_local_variable(const char *name)
{
	NIB_VARIABLE *best_var = NULL;

	ITERATOR it;
	NIB_VARIABLE *var;
	iterator_start(&it, nib_local_variables);
	while((var = (NIB_VARIABLE *)iterator_nextdata(&it)))
	{
		if (!str_cmp(var->name, name) && nib_in_scope(var->scope))
		{
			if (!best_var || var->scope > best_var->scope)
				best_var = var;
		}
	}

	iterator_stop(&it);

	return best_var;
}

void nib_add_global_variable(NIB_VARIABLE *var)
{
	if (var)
	{
		list_appendlink(nib_global_variables, var);
		var->id = list_size(nib_global_variables);
	}
}

void nib_add_local_variable(NIB_VARIABLE *var)
{
	if (var)
	{
		list_appendlink(nib_local_variables, var);
		var->id = list_size(nib_local_variables);
	}
}

const char *nib_get_string(int index)
{
	if(!list_isvalid(nib_string_storage)) return NULL;

	if (index < 1 || index > list_size(nib_string_storage)) return NULL;

	return (const char *)list_nthdata(nib_string_storage, index);
}

short nib_get_string_in_storage(const char *str)
{
	if(!str) return 0;	
	if(!list_isvalid(nib_string_storage)) return 0;

	ITERATOR it;
	char *name;
	short index = 0;
	iterator_start(&it, nib_string_storage);
	while((name = (char *)iterator_nextdata(&it)))
	{
		++index;
		
		// Must be CASE SENSITIVE
		if(!strcmp(name, str))
			break;
	}
	iterator_stop(&it);

	return name ? index : 0;
}

short nib_add_string_to_storage(const char *str)
{
	if(!str) return 0;	

	// Make sure it is not in the storage already
	short index = nib_get_string_in_storage(str);
	if (index > 0) return index;

	if (!list_isvalid(nib_string_storage))
		nib_string_storage = nib_create_string_list();

	list_appendlink(nib_string_storage, nib_strdup(str));
	return (short)list_size(nib_string_storage);
}

void nib_dump_string_storage()
{
	if (list_isvalid(nib_string_storage))
	{
		printf("String Storage:\n");
		printf("==================================\n");
		ITERATOR it;
		char *str;
		int index = 0;
		iterator_start(&it, nib_string_storage);
		while((str = (char *)iterator_nextdata(&it)))
		{
			++index;
			printf("%-5d \"%s\"\n", index, str);
		}

		iterator_stop(&it);
		printf("\n");
	}
}
