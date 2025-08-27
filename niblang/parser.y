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

#include "../niblang.h"
#include "parser.h"
#include "lexer.h"

#define IS_READONLY		(A)
#define IS_LITERAL		(B)

#define IS_SET(v,b)		(((v) & (b)) && true)

extern int niblineno;

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

%token T_AREA
%token T_ARROW
%token T_ASSIGN
%token T_BAND
%token T_BNOT
%token T_BOOLEAN
%token T_BOR
%token T_BXOR
%token T_BREAK
%token T_CASE
%token T_CHAR
%token T_CHAR_LITERAL
%token T_CLOSE_BRACE
%token T_CLOSE_BRACKET
%token T_CLOSE_FLAG
%token T_CLOSE_MAP
%token T_CLOSE_PAREN
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
%token T_EXPONENT
%token T_FALSE
%token T_FLAG
%token T_FLOAT
%token T_FLOAT_NUMBER
%token T_FOR
%token T_FOREACH
%token T_GLOBAL
%token T_GT
%token T_GT_EQUAL
%token T_IDENTIFIER
%token T_IF
%token T_INCREMENT
%token T_INSTANCE
%token T_INT
%token T_LAND
%token T_LEFT_SHIFT
%token T_LIST
%token T_LNOT
%token T_LOR
%token T_LT
%token T_LT_EQUAL
%token T_LXOR
%token T_MAP
%token T_MINUS
%token T_MOBILE
%token T_MOD
%token T_NOT_EQUAL
%token T_NULL
%token T_NUMBER
%token T_OBJECT
%token T_OPEN_BRACE
%token T_OPEN_BRACKET
%token T_OPEN_FLAG
%token T_OPEN_MAP
%token T_OPEN_PAREN
%token T_PLUS
%token T_QMARK
%token T_QUEST
%token T_RANGE
%token T_RETURN
%token T_RIGHT_SHIFT
%token T_ROOM
%token T_SELF
%token T_SEMICOLON
%token T_SHIP
%token T_SIZEOF
%token T_STAR
%token T_STAT
%token T_STRING
%token T_STRING_LITERAL
%token T_SWITCH
%token T_TABLE
%token T_TOKEN
%token T_TRUE
%token T_TYPEOF
%token T_VARARGS
%token T_WHILE
%token T_WIDEVNUM
%token T_WIDEVNUM_DELIM

%union {
	bool b;
	char ch;
    long number;
	NIB_ASSIGN assign;
	flag_value_t flags;
	NIB_TYPE *nibtype;
	const struct flag_type *flag_table;

	struct {
		bool global;
		bool constant;
	} modifiers;

	struct {
		intptr_t key;		/* shared string ptr, or a number */
		bool numeric;		/* TRUE: .key is a number */
	} case_label;

	struct decl_s 
	{
		NIB_TYPE *type;
		bool global;
		bool constant;
	} decl;

	LLIST *string_list;
	LLIST *type_list;
	double float_number;
	char *literal;
	char *identifier;

    struct lvalue_s {
		char *name;				// Name of variable to be used
		NIB_TYPE *type;			// Resolved type of expression
		flag_value_t flags;
    } lvalue;

    struct rvalue_s {
		char *name;				// Name of variable to be used
		NIB_TYPE *type;			// Resolved type of expression
		bool needs_use;
		flag_value_t flags;

		// TODO: Add code stuff
    } rvalue;

	struct lrvalue_s {
		char *name;				// Name of variable to be used
		NIB_TYPE *type;			// Resolved type of expression
		bool needs_use;
		flag_value_t flags;

		// TODO: Add code stuff
	} lrvalue;

    struct
    {
		NIB_TYPE *type;
		bool might_lvalue;
		bool needs_use;
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
%destructor { free($$); } <literal>
%destructor { free($$); } <identifier>
%destructor { list_destroy($$); } <string_list>
%destructor { list_destroy($$); } <type_list>

%type <number> T_NUMBER constant
%type <ch> T_CHAR_LITERAL
%type <float_number> T_FLOAT_NUMBER
%type <literal> T_STRING_LITERAL
%type <identifier> T_IDENTIFIER field_name
%type <nibtype> type listtype cast
%type <string_list> comma_name_list flag_name_list
%type <flags> flag_number_list comma_bit_list
%type <modifiers> possible_modifiers
%type <b> boolean_value switch_label
%type <lrvalue> expr0 expr4 field_call widevnum_value
%type <lvalue> lvalue name_lvalue
%type <rvalue> comma_expr
%type <function_call_result> function_call method_call
%type <type_list> argument_list optional_argument_list
%type <flag_table> flag_table stat_table
// Contains the opcode used for the assignment
%type <assign> T_ASSIGN
%type <statement> statement_block statement cond for foreach do while switch
%type <switch_block> switch_block switch_statements
%type <case_label> case_label

%right T_ASSIGN
%right T_QMARK
%left T_LOR
%left T_LAND
%left T_BOR
%left T_BXOR
%left T_BAND
%left T_EQUAL T_NOT_EQUAL T_IDENTIFIER
%left T_LT T_LT_EQUAL T_GT T_GT_EQUAL
%left T_LEFT_SHIFT T_RIGHT_SHIFT
%left T_PLUS T_MINUS
%left T_STAR T_DIVIDE T_MOD
%right T_BNOT T_LNOT
%nonassoc T_INCREMENT T_DECREMENT
%left T_OPEN_PAREN
%left T_DOT T_ARROW T_OPEN_BRACKET

%right T_STRING_LITERAL

%%


program: program statement
	| /* empty */
	;

def:	name_list T_SEMICOLON
	|	table_def T_SEMICOLON
	;

table_def:
		T_TABLE T_FLAG T_OPEN_PAREN flag_name_list[L] T_CLOSE_PAREN T_IDENTIFIER[I]
		{
			const struct flag_type *table = lookup_flag_table($I);
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
			flag_add_table($L, $I);
		}
	|	T_TABLE T_STAT T_OPEN_PAREN flag_name_list[L] T_CLOSE_PAREN T_IDENTIFIER[I]
		{
			const struct flag_type *table = lookup_stat_table($I);
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
			stat_add_table($L, $I);
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
				// Determine if it has the correc type
				// Detemrine if it is readonly (constant)

				NIB_VARIABLE *var = nib_new_variable($I, $T, NIB_GLOBAL_SCOPE, false);
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
		}
	|	possible_modifiers[M] type[T] T_IDENTIFIER[I]
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

				NIB_VARIABLE *var = nib_new_variable($I, $T, NIB_GLOBAL_SCOPE, false);
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
		}
		T_ASSIGN[A] expr0[E]
		{
			// Do some initialization


			$<decl>$ = $<decl>4;
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
				NIB_VARIABLE *var = nib_new_variable($I, $<decl>L.type, NIB_GLOBAL_SCOPE, false);
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
				NIB_VARIABLE *var = nib_new_variable($I, $<decl>L.type, NIB_GLOBAL_SCOPE, false);
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
		}
		T_ASSIGN[A] expr0[E]
		{
			// Do some initialization


			$<decl>$ = $<decl>4;
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
			// Verify we are in a situation that allows for breaking
			// 1. Loops
			// 2. Switch
		}
	|	T_CONTINUE T_SEMICOLON
		{
			// Verify we are in a loop
		}
	|	block
		{

		}
	;

cond_start:	T_IF T_OPEN_PAREN expr0 T_CLOSE_PAREN
		{

		}
	;

cond:
		cond_start
		statement
		optional_else
		{

		}
	;

optional_else:
		/* empty */	%prec LOWER_THAN_ELSE
		{

		}
	|	T_ELSE
		{

		}
		statement
	;

for:	T_FOR T_OPEN_PAREN
		{
			nib_push_scope();
		}
		name_list[N] T_SEMICOLON expr0[C] T_SEMICOLON comma_expr[P] T_CLOSE_PAREN
		{
			// Configure CONTINUE and BREAK information
		}
		statement
		{
			// Update CONTINUE and BREAK instructions

			nib_pop_scope();
		}

foreach:
		T_FOREACH T_OPEN_PAREN T_IDENTIFIER[I] T_COLON expr0[E] T_CLOSE_PAREN
		{
			// Configure CONTINUE and BREAK information

			// Open a new scope
			nib_push_scope();

			// Create a variable in this scope using the element type of the expression
			NIB_TYPE *type = NULL;
			if ($E.type != NULL)
			{
				if ($E.type->type_class == NTC_PRIMARY)
				{
					if ($E.type->_.primary == NT_STRING)
					{
						type = nibtype_char;
					}
				}
				else if ($E.type->type_class == NTC_LIST)
				{
					type = $E.type->_.type;
				}
			}

			if (type == NULL)
			{
				yyerror("Expecting an iterative type.");
				YYERROR;
			}

			NIB_VARIABLE *var = nib_new_variable($I, type, nib_get_scope(), false);
			nib_add_local_variable(var);
		}
		statement
		{
			// Update CONTINUE and BREAK instructions

			// Close scope
			nib_pop_scope();
		}
	;

while:
		T_WHILE T_OPEN_PAREN expr0 T_CLOSE_PAREN
		{
			// Configure CONTINUE and BREAK information
		}
		statement
		{
			// Update CONTINUE and BREAK instructions
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
		T_SWITCH T_OPEN_PAREN expr0 T_CLOSE_PAREN
		{
			// Push current break information
			// Push current PC

			// Start code for dealing with SWITCH
			// Setup for new case label generation

			// Set up break information
		}
		T_OPEN_BRACE switch_block T_CLOSE_BRACE
		{


			// Pop old PC
			// Pop old break information
			// Revert to previous case information
		}
	;

switch_block:
		switch_block switch_statements
		{
			$$.has_default = $1.has_default || $2.has_default;
			$$.statements = (NIB_STATEMENT)
					{
						.may_return =		$1.statements.may_return		|| $2.statements.may_return,
						.may_break =		$1.statements.may_break			|| $2.statements.may_break,
						.may_continue =		$1.statements.may_continue		|| $2.statements.may_continue,
						.may_finish =										   $2.statements.may_finish,
						.is_empty =			false,
						.warned_dead_code =	$1.statements.warned_dead_code	|| $2.statements.warned_dead_code,
					};
		}
	|	switch_statements
	;

switch_statements:
		switch_label statement_block
		{
			$$.has_default = $1;
			$$.statements = $2;
		}
	;

switch_label:
		case		{ $$ = false; }
	|	default		{ $$ = true; }

case:
		T_CASE case_label T_COLON
		{
			// Make sure we are in a switch statement

			// Add case label

			// Mark the zero case? (why?)

			// Store info about case data
		}
	|	T_CASE case_label T_RANGE case_label T_COLON
		{
			// Make sure we are in a switch statement

			// Verify both labels are numeric
			if (!$2.numeric || !$4.numeric)
			{
				yyerror("String case labels not allowed as range bounds.");
				YYERROR;
			}

			// Verify range is valid
			if ($2.key >= $4.key)
			{
				if ($2.key < $4.key)
				{
					niberrorf("Illegal case range: lower limit %ld > upper limit %ld",
						(long)$2.key, (long)$4.key);
					YYERROR;
				}

				// Generate case entry
			}
			else
			{
				// Generate case entry pair
				// - Mark lower bound entry
				// - Mark upper bound entry
			}


			// Add case label

			// Mark the zero case? (why?)

			// Store info about case data
		}
	;

case_label:
		constant
		{
			// Check if the current switch is using numeric labels

			$$.key = $1;
			$$.numeric = true;
		}
	|	T_STRING_LITERAL
		{
			// Check if the current switch is using string labels

			int index = nib_add_string_to_storage($1);
			if (index < 1)
			{
				yyerror("Error storing string literal into storage.");
				YYERROR;
			}

			$$.key = index;
			$$.numeric = false;
		}
	;

default:
		T_DEFAULT T_COLON
		{
			// Detect this is done inside a switch statement
			// Which.. I don't know why because it will only ever
			//   be called in the switch parsing?

			// Detect duplicate default in current switch

			// Save default information in current switch
		}
	;

constant:

		T_NUMBER
		{

		}
	;

comma_expr:
		expr0
		{
			$$.name = $1.name;
			$$.type = $1.type;
			$$.needs_use = $1.needs_use;
		}
	|	comma_expr
		{
			// Complain if needed
			if ($1.needs_use)
			{
				// Warn
			}
			// Save value?
		}
		T_COMMA expr0
		{
			$$.name = $4.name;
			$$.type = $4.type;
			$$.needs_use = $4.needs_use;
		}
	;

expr0:
		lvalue T_ASSIGN
		{
			if (IS_SET($1.flags, IS_READONLY))
			{
				yyerror("left hand expression is readonly.");
				YYERROR;
			}
		}
		expr0
		{
			$$.name = $1.name;
			$$.type = $1.type;
			$$.needs_use = false;	// Since it is an assignment
			$$.flags = IS_READONLY;
		}
	|	field_call T_ASSIGN
		{
			if (IS_SET($1.flags, IS_READONLY))
			{
				yyerror("Field is readonly.");
				YYERROR;
			}
		}
		expr0
		{
			$$.name = $1.name;
			$$.type = $1.type;
			$$.needs_use = false;	// Since it is an assignment
			$$.flags = IS_READONLY;
		}
	|	expr0[C] T_QMARK
		{
			// Expression based if-else

		}
		expr0[T]
		{
			// 
		}
		T_COLON expr0[F] %prec T_QMARK
		{
			// Update $T's branching to deal with $F's expression code
			// Check the types of the two parts

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
			$$.flags = IS_READONLY;
		}
	|	expr0 T_LOR %prec T_LOR
		{
			
		}
		expr0
		{

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_LAND %prec T_LAND
		{
			
		}
		expr0
		{

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_EQUAL expr0 %prec T_EQUAL
		{
			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_NOT_EQUAL expr0 %prec T_NOT_EQUAL
		{
			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_LT expr0 %prec T_LT
		{
			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_LT_EQUAL expr0 %prec T_LT_EQUAL
		{
			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_GT expr0 %prec T_GT
		{
			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_GT_EQUAL expr0 %prec T_GT_EQUAL
		{
			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_BOR expr0 %prec T_BOR
		{
			// TODO: check valid types
			// TODO: get type for the binary operation
			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_BXOR expr0 %prec T_BXOR
		{
			// TODO: check valid types
			// TODO: get type for the binary operation
			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_BAND expr0 %prec T_BAND
		{
			// TODO: check valid types
			// TODO: get type for the binary operation
			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_LEFT_SHIFT expr0 %prec T_LEFT_SHIFT
		{
			// TODO: check valid types
			// TODO: get type for the binary operation
			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_RIGHT_SHIFT expr0 %prec T_RIGHT_SHIFT
		{
			// TODO: check valid types
			// TODO: get type for the binary operation
			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_PLUS %prec T_PLUS
		{

		}
		expr0
		{
			// TODO: get the correct types
			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_MINUS expr0 %prec T_MINUS
		{
			// TODO: get the correct types
			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_STAR expr0 %prec T_STAR
		{
			// TODO: get the correct types
			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_MOD expr0 %prec T_MOD
		{
			// TODO: get the correct types
			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	expr0 T_DIVIDE expr0 %prec T_DIVIDE
		{
			// TODO: get the correct types
			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	cast expr0 %prec T_BNOT
		{
			$$.name = $2.name;
			$$.type = $1;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	T_INCREMENT lvalue %prec T_INCREMENT
		{
			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = false;
			$$.flags = IS_READONLY;
		}
	|	T_DECREMENT lvalue %prec T_DECREMENT
		{
			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = false;
			$$.flags = IS_READONLY;
		}
	|	T_LNOT expr0
		{
			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	T_BNOT expr0
		{
			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	T_MINUS expr0 %prec T_BNOT
		{
			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	lvalue T_INCREMENT %prec T_INCREMENT
		{
			// TODO: get correct type
			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = false;
			$$.flags = IS_READONLY;
		}
	|	lvalue T_DECREMENT %prec T_DECREMENT
		{
			// TODO: get correct type
			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = false;
			$$.flags = IS_READONLY;
		}
	|	T_OPEN_PAREN expr0 T_CLOSE_PAREN
		{
			$$ = $2;
		}
	|	expr4
		{
			$$ = $1;
		}
	;

expr4:
		function_call
		{
			// Get type
			$$.name = NULL;
			$$.type = $1.type;
			$$.needs_use = $1.needs_use;
			$$.flags = IS_READONLY;
		}
	|	method_call
		{
			// Get type
			$$.name = NULL;
			$$.type = $1.type;
			$$.needs_use = $1.needs_use;
			$$.flags = IS_READONLY;
		}
	|	field_call
		{
			$$.name = NULL;
			$$.type = $1.type;
			$$.flags = $1.flags;
			$$.needs_use = true;
		}
	|	T_STRING_LITERAL
		{
			// Add string literal to the string literal list (if necessary)
			int index = nib_add_string_to_storage($1);
			if (index < 1)
			{
				yyerror("Error storing string literal into storage.");
				YYERROR;
			}

			// Push OP code to pull string literal
			// Push index
			
			$$.name = NULL;
			$$.type = nibtype_string;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
		}
	|	T_CHAR_LITERAL
		{
			$$.name = NULL;
			$$.type = nibtype_char;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
		}
	|	T_NUMBER
		{
			if ($1 == 0L)
			{
				// Push opcode for constant 0
			}
			else if ($1 == 1L)
			{
				// Push opcode for constant 1
			}
			else
			{
				// Push opcode for reading number
				// Push integer
			}

			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
		}
	|	T_FLOAT_NUMBER
		{
			// Push opcode for reading float
			// Push double
			$$.name = NULL;
			$$.type = nibtype_float;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
		}
	|	boolean_value
		{
			if($1)
			{
				// Push opcode for reading TRUE
			}
			else
			{
				// Push opcode for reading FALSE
			}

			$$.name = NULL;
			$$.type = nibtype_bool;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
		}
	|	widevnum_value
		{
			$$ = $1;
		}
	|	T_NULL
		{
			// Store opcode for constant 0
			$$.name = NULL;
			$$.type = nibtype_any;	// Think (void *) from C
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
		}
	|	lvalue
		{
			$$.name = $1.name;
			$$.type = $1.type;
			$$.flags = $1.flags;
			$$.needs_use = true;
		}
	|	T_IDENTIFIER[I] flag_name_list[L]
		{
			// Look up the flag table
			const struct flag_type *table = lookup_flag_table($I);
			if (!table)
			{
				niberrorf("Undefined flag table '%s'.", $1);
				YYERROR;
			}

			flag_value_t value = 0;

			ITERATOR it;
			char *name;

			iterator_start(&it, $L);
			while((name = (char *)iterator_nextdata(&it)))
			{
				flag_value_t bit;

				if (find_flag_value(table, name, false, &bit))
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

			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
		}
	|	flag_number_list[L]
		{
			// Store opcode for integer
			// Store $L

			$$.name = NULL;
			$$.type = nibtype_int;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
		}
	;

widevnum_value:
		T_NUMBER[A] T_WIDEVNUM_DELIM T_NUMBER[V]
		{
			// Store opcode for reading absolute widevnum
			// Store widevnum

			$$.name = NULL;
			$$.type = nibtype_widevnum;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
		}
	|	T_STRING_LITERAL[A] T_WIDEVNUM_DELIM T_NUMBER[V]
		{
			// Add string literal to the string literal list (if necessary)
			int index = nib_add_string_to_storage($A);
			if (index < 1)
			{
				yyerror("Error storing string literal into storage.");
				YYERROR;
			}

			// Store opcode for reading named widevnum
			// Store area name
			// Store vnum
			
			$$.name = NULL;
			$$.type = nibtype_widevnum;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
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

			$$.name = NULL;
			$$.type = nibtype_widevnum;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
		}
	|	T_WIDEVNUM_DELIM T_NUMBER[V]
		{
			// Store opcode for reading relative widevnum
			// Store vnum
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
			// Store widevnum

			$$.name = NULL;
			$$.type = nibtype_widevnum;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
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
			int index = nib_add_string_to_storage($A);
			if (index < 1)
			{
				yyerror("Error storing string literal into storage.");
				YYERROR;
			}

			// Store opcode for reading named widevnum
			// Store area name
			// Store vnum
			
			$$.name = NULL;
			$$.type = nibtype_widevnum;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
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

			$$.name = NULL;
			$$.type = nibtype_widevnum;
			$$.needs_use = true;
			$$.flags = IS_READONLY;
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

			// Store opcode for reading relative widevnum
			// Store vnum
			$$.name = NULL;
			$$.type = nibtype_widevnum;
			$$.needs_use = true;
			$$.flags = IS_LITERAL;
		}
	;

cast:	T_OPEN_PAREN type T_CLOSE_PAREN { $$ = $2; }
	;

name_lvalue:
		T_IDENTIFIER
		{
			NIB_VARIABLE *var;

			// Check global names first
			var = nib_get_global_variable($1);
			if (var)
			{
				// Store opcode for accessing global variable
				// Store data for the variable name
			}
			else
			{

				// Check for local name
				var = nib_get_local_variable($1);

				if (var)
				{
					// Store opcode for accessing local variable
					// Store data for variable index
				}
				else
				{
					niberrorf("Variable '%s' not declared within scope.", $1);
					YYERROR;
				}
			}

			$$.name = var->name;
			$$.type = var->type;
			$$.flags = 0;
		}
	;

lvalue:
		name_lvalue
	;

function_call:
		T_IDENTIFIER[M] optional_argument_list[A]
		{
			// Use the type of $C to find $M with $A type list
			NIB_METHOD *method = nib_method_get(NULL, $M, $A);

			if (!method)
			{
				niberrorf("No such function '%s' defined.", $M);
				YYERROR;
			}

			NIB_TYPE *type = method->result;
			$$.type = nib_type_copy(type);
			$$.might_lvalue = false;	// TODO: FIX THIS
			$$.needs_use = (type && type->type_class != NTC_VOID);

			printf("Function Call: %s %s.\n", nib_get_typename(type), method->name);
		}
	;

field_call:
		expr0[L] T_DOT field_name[F]
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

				// Verify the flag actually exists
				if ($L.type->_.flag.table)
				{
					if (!find_flag_value($L.type->_.flag.table, $F, false, NULL))
					{
						niberrorf("Flag '%s' is not defined.", $F);
						YYERROR;
					}
				}
				else
				{
					ITERATOR it;
					char *name;
					iterator_start(&it, $L.type->_.flag.names);
					while((name = (char *)iterator_nextdata(&it)))
					{
						if (!str_cmp(name, $F))
							break;
					}
					iterator_stop(&it);

					if (!name)
					{
						niberrorf("Flag '%s' is not defined.", $F);
						YYERROR;
					}
				}

				$$.name = NULL;
				$$.type = nibtype_bool;
				$$.needs_use = true;
				$$.flags = IS_READONLY;
			}
			else if ($L.type->type_class == NTC_STAT)
			{
				// Verify the stat actually exists
				if ($L.type->_.stat.table)
				{
					if (!find_flag_value($L.type->_.stat.table, $F, false, NULL))
					{
						niberrorf("Stat '%s' is not defined.", $F);
						YYERROR;
					}
				}
				else
				{
					ITERATOR it;
					char *name;
					iterator_start(&it, $L.type->_.stat.names);
					while((name = (char *)iterator_nextdata(&it)))
					{
						if (!str_cmp(name, $F))
							break;
					}
					iterator_stop(&it);

					if (!name)
					{
						niberrorf("Stat '%s' is not defined.", $F);
						YYERROR;
					}
				}

				$$.name = NULL;
				$$.type = nibtype_int;
				$$.needs_use = true;
				$$.flags = IS_READONLY;
			}
			else
			{
				// Search for $F on $L
				NIB_FIELD *field = nib_field_get($L.type, $F);
				if (!field)
				{
					niberrorf("No such field '%s' defined for type '%s'.",
						$F, nib_get_typename($L.type));
					YYERROR;
				}

				$$.name = $L.name;
				$$.type = field->type;
				$$.needs_use = true;
				$$.flags = field->readonly ? IS_READONLY : 0;
			}
		}
	;

field_name:
		T_IDENTIFIER			{ $$ = $1; }
	|	T_AREA					{ $$ = strdup("area"); }
	|	T_STRING				{ $$ = strdup("string"); }
	;

method_call:
		expr0[C] T_DOT T_IDENTIFIER[M] optional_argument_list[A]
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
					$M, nib_get_typename($C.type));
				YYERROR;
			}

			NIB_TYPE *type = method->result;
			if (type != NULL && type->type_class == NTC_ANY)
			{
				if ($C.type->type_class == NTC_LIST)
				{
					type = $C.type->_.type;	// Automatically assume it is the subtype of the list.
				}
			}

			$$.type = nib_type_copy(type);
			$$.might_lvalue = false;	// TODO: FIX THIS
			$$.needs_use = (type && type->type_class != NTC_VOID);

			printf("Method Call: %s %s for %s type.\n", nib_get_typename(type), method->name, nib_get_typename($C.type));
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
		}
	|	argument_list T_COMMA expr0
		{
			list_appendlink($1,nib_type_copy($3.type));
			$$ = $1;
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
	| T_FLAG										{ $$ = nibtype_flag; }
	| T_FLAG T_OPEN_PAREN T_NUMBER[N] T_CLOSE_PAREN
		{
			if ($N < 1 || $N > MAX_FLAG_BITS)
			{
				niberrorf("Flag type only supports 1 to %d bits.", MAX_FLAG_BITS);
				YYERROR;
			}
			$$ = new_nib_type_flag($N);
		}
	| T_FLAG T_OPEN_PAREN flag_name_list[L] T_CLOSE_PAREN
		{
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

			$$ = new_nib_type_flag_named($L);
		}
	| T_FLAG T_OPEN_PAREN flag_table[T] T_CLOSE_PAREN
		{
			$$ = new_nib_type_flag_table($T);
		}
	| T_STAT T_OPEN_PAREN flag_name_list[L] T_CLOSE_PAREN
		{
			if (list_size($L) < 1)
			{
				yyerror("Please specify at least one named stat.");
				YYERROR;
			}

			$$ = new_nib_type_stat_named($L);
		}
	| T_STAT T_OPEN_PAREN stat_table[T] T_CLOSE_PAREN
		{
			$$ = new_nib_type_stat_table($T);
		}
	| T_LIST T_OPEN_PAREN listtype[T] T_CLOSE_PAREN	{ $$ = new_nib_type_list($T); }
	| T_AREA										{ $$ = nibtype_area; }
	| T_DUNGEON										{ $$ = nibtype_dungeon; }
	| T_INSTANCE									{ $$ = nibtype_instance; }
	| T_MOBILE										{ $$ = nibtype_mobile; }
	| T_OBJECT										{ $$ = nibtype_object; }
	| T_QUEST										{ $$ = nibtype_quest; }
	| T_ROOM										{ $$ = nibtype_room; }
	| T_SHIP										{ $$ = nibtype_ship; }
	| T_TOKEN										{ $$ = nibtype_token; }
	| T_WIDEVNUM									{ $$ = nibtype_widevnum; }
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
	| T_AREA										{ $$ = nibtype_area; }
	| T_DUNGEON										{ $$ = nibtype_dungeon; }
	| T_INSTANCE									{ $$ = nibtype_instance; }
	| T_MOBILE										{ $$ = nibtype_mobile; }
	| T_OBJECT										{ $$ = nibtype_object; }
	| T_QUEST										{ $$ = nibtype_quest; }
	| T_ROOM										{ $$ = nibtype_room; }
	| T_SHIP										{ $$ = nibtype_ship; }
	| T_TOKEN										{ $$ = nibtype_token; }
	| T_WIDEVNUM									{ $$ = nibtype_widevnum; }
	;

flag_table:
		T_IDENTIFIER			
			{
				const struct flag_type *table = lookup_flag_table($1);
				if (!table)
				{
					niberrorf("Unknown flag table '%s'", $1);
					YYERROR;
				}

				$$ = table;
			}
	|	T_AREA
			{
				const struct flag_type *table = lookup_flag_table("area");
				if (!table)
				{
					yyerror("Unknown flag table 'area'");
					YYERROR;
				}

				$$ = table;
			}
	|	T_DUNGEON
			{
				const struct flag_type *table = lookup_flag_table("dungeon");
				if (!table)
				{
					yyerror("Unknown flag table 'dungeon'");
					YYERROR;
				}

				$$ = table;
			}
	|	T_INSTANCE
			{
				const struct flag_type *table = lookup_flag_table("instance");
				if (!table)
				{
					yyerror("Unknown flag table 'instance'");
					YYERROR;
				}

				$$ = table;
			}
	|	T_MOBILE
			{
				const struct flag_type *table = lookup_flag_table("mobile");
				if (!table)
				{
					yyerror("Unknown flag table 'mobile'");
					YYERROR;
				}

				$$ = table;
			}
	|	T_OBJECT
			{
				const struct flag_type *table = lookup_flag_table("object");
				if (!table)
				{
					yyerror("Unknown flag table 'object'");
					YYERROR;
				}

				$$ = table;
			}
	|	T_QUEST
			{
				const struct flag_type *table = lookup_flag_table("quest");
				if (!table)
				{
					yyerror("Unknown flag table 'quest'");
					YYERROR;
				}

				$$ = table;
			}
	|	T_ROOM
			{
				const struct flag_type *table = lookup_flag_table("room");
				if (!table)
				{
					yyerror("Unknown flag table 'room'");
					YYERROR;
				}

				$$ = table;
			}
	|	T_SHIP
			{
				const struct flag_type *table = lookup_flag_table("ship");
				if (!table)
				{
					yyerror("Unknown flag table 'ship'");
					YYERROR;
				}

				$$ = table;
			}
	|	T_TOKEN
			{
				const struct flag_type *table = lookup_flag_table("token");
				if (!table)
				{
					yyerror("Unknown flag table 'token'");
					YYERROR;
				}

				$$ = table;
			}
	;

stat_table:
		T_IDENTIFIER			
			{
				const struct flag_type *table = lookup_stat_table($1);
				if (!table)
				{
					niberrorf("Unknown stat table '%s'", $1);
					YYERROR;
				}

				$$ = table;
			}
	|	T_AREA
			{
				const struct flag_type *table = lookup_stat_table("area");
				if (!table)
				{
					yyerror("Unknown stat table 'area'");
					YYERROR;
				}

				$$ = table;
			}
	|	T_DUNGEON
			{
				const struct flag_type *table = lookup_stat_table("dungeon");
				if (!table)
				{
					yyerror("Unknown stat table 'dungeon'");
					YYERROR;
				}

				$$ = table;
			}
	|	T_INSTANCE
			{
				const struct flag_type *table = lookup_stat_table("instance");
				if (!table)
				{
					yyerror("Unknown stat table 'instance'");
					YYERROR;
				}

				$$ = table;
			}
	|	T_MOBILE
			{
				const struct flag_type *table = lookup_stat_table("mobile");
				if (!table)
				{
					yyerror("Unknown stat table 'mobile'");
					YYERROR;
				}

				$$ = table;
			}
	|	T_OBJECT
			{
				const struct flag_type *table = lookup_stat_table("object");
				if (!table)
				{
					yyerror("Unknown stat table 'object'");
					YYERROR;
				}

				$$ = table;
			}
	|	T_QUEST
			{
				const struct flag_type *table = lookup_stat_table("quest");
				if (!table)
				{
					yyerror("Unknown stat table 'quest'");
					YYERROR;
				}

				$$ = table;
			}
	|	T_ROOM
			{
				const struct flag_type *table = lookup_stat_table("room");
				if (!table)
				{
					yyerror("Unknown stat table 'room'");
					YYERROR;
				}

				$$ = table;
			}
	|	T_SHIP
			{
				const struct flag_type *table = lookup_stat_table("ship");
				if (!table)
				{
					yyerror("Unknown stat table 'ship'");
					YYERROR;
				}

				$$ = table;
			}
	|	T_TOKEN
			{
				const struct flag_type *table = lookup_stat_table("token");
				if (!table)
				{
					yyerror("Unknown stat table 'token'");
					YYERROR;
				}

				$$ = table;
			}
	;


%%
