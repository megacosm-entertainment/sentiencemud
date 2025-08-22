%define lr.type ielr
%define parse.error verbose
%{

/*
 * Parser.y file
 * To generate the parser run: "bison Parser.y"
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "../niblang.h"
#include "parser.h"
#include "lexer.h"

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
%token T_CLOSE_BRACE
%token T_CLOSE_BRACKET
%token T_CLOSE_MAP
%token T_CLOSE_PAREN
%token T_COLON
%token T_COLONS
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
%token T_LS_ASSIGN
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
%token T_OPEN_MAP
%token T_OPEN_PAREN
%token T_PLUS
%token T_QMARK
%token T_QUEST
%token T_RANGE
%token T_RETURN
%token T_RIGHT_SHIFT
%token T_ROOM
%token T_RS_ASSIGN
%token T_SELF
%token T_SEMICOLON
%token T_SHIP
%token T_SIZEOF
%token T_STAR
%token T_STRING
%token T_STRING_LITERAL
%token T_SWITCH
%token T_TOKEN
%token T_TRUE
%token T_TYPEOF
%token T_WHILE
%token T_WIDEVNUM
%token T_WIDEVNUM_DELIM

%union {
	bool b;
    int number;
	NIB_TYPE *nibtype;
	LLIST *string_list;
	double float_number;
	char *literal;
	char *identifier;
}

%nonassoc LOWER_THAN_ELSE
%nonassoc T_ELSE

%destructor { free_nib_type($$); } <nibtype>
%destructor { free($$); } <literal>
%destructor { free($$); } <identifier>
%destructor { list_destroy($$); } <string_list>

%type <number> T_NUMBER
%type <float_number> T_FLOAT_NUMBER
%type <literal> T_STRING_LITERAL
%type <identifier> T_IDENTIFIER
%type <nibtype> type listtype
%type <string_list> comma_flag_name_list

%%


program: program def		;
	| /* empty */
	;

def:	T_GLOBAL type[T] T_IDENTIFIER[I] T_SEMICOLON
		{
			// Global declaration.
			// There is no initialization involved within the script,
			//   that is done on the script itself.

			if (nib_get_global_variable($I))
			{
				niberrorf("Duplicate global variable '%s'", $I);
				YYERROR;
			}
			else
			{
				NIB_VARIABLE *var = nib_new_variable($I, $T, NIB_GLOBAL_SCOPE);
				nib_add_global_variable(var);
			}
		}
	|	type[T] T_IDENTIFIER[I] T_SEMICOLON
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
					var = nib_new_variable($I, $T, nib_get_scope());
					nib_add_local_variable(var);
				}
			}
		}
	;

type:	T_INT										{ $$ = nibtype_int; }
	| T_FLOAT										{ $$ = nibtype_float; }
	| T_BOOLEAN										{ $$ = nibtype_bool; }
	| T_STRING										{ $$ = nibtype_string; }
	| T_MAP											{ $$ = nibtype_map; }
	| T_FLAG										{ $$ = nibtype_flag; }
	| T_FLAG T_OPEN_PAREN T_NUMBER[N] T_CLOSE_PAREN	{ $$ = new_nib_type_flag($N); }
	| T_FLAG T_OPEN_PAREN comma_flag_name_list[N] T_CLOSE_PAREN { $$ = new_nib_type_flag_named($N); }
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

comma_flag_name_list:
	T_IDENTIFIER
	{
		$$ = nib_create_string_list();
		list_appendlink($$, $1);
	}
	| comma_flag_name_list ',' T_IDENTIFIER
	{
		list_appendlink($1, $3);
		$$ = $1;
	}
	

listtype:	T_INT										{ $$ = nibtype_int; }
	| T_FLOAT										{ $$ = nibtype_float; }
	| T_BOOLEAN										{ $$ = nibtype_bool; }
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


%%
