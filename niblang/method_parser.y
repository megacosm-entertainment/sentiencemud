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

#include "../niblang.h"
#include "method_parser.h"
#include "method_lexer.h"

extern int nibmethodlineno;

int nibmethoderror(const char *msg) {
    printf("error(%d): %s\n", nibmethodlineno, msg);
    return 0;
}

void nibmethoderrorf(const char *msg, ...)
{
    va_list va;
    char buff[5120];

    va_start(va, msg);
    vsprintf(buff, msg, va);
    va_end(va);

    nibmethoderror(buff);
}

// Verify that VARARGS, if in the list, is the LAST arg
static bool check_argtype_list(LLIST *list)
{
	bool found = false;
	ITERATOR it;
	NIB_TYPE *type;
	iterator_start(&it, list);
	while((type = (NIB_TYPE *)iterator_nextdata(&it)))
	{
		if (type->type_class == NTC_VARARGS)
		{
			found = true;
		}
		else if(found)	// Found something else after a VARARGS type
			break;
	}
	iterator_stop(&it);

	return (type == NULL);
}

%}
%locations

%output  "yacc/method_parser.c"
%defines "yacc/method_parser.h"

%define api.pure
%define api.prefix {nibmethod}

%union {
	bool b;
	long number;
	NIB_TYPE *nibtype;
	char *identifier;
	LLIST *type_list;
	const struct flag_type *flag_table;
}

%destructor { free_nib_type($$); } <nibtype>
%destructor { nib_free($$); } <identifier>
%destructor { list_destroy($$); } <type_list>

%token T_ANY
%token T_AREA
%token T_ARROW
%token T_BOOLEAN
%token T_CHAR
%token T_COMMA
%token T_CP
%token T_DOT
%token T_DUNGEON
%token T_FIELD
%token T_FLAG
%token T_FLOAT
%token T_FUNCTION
%token T_GT
%token T_IDENTIFIER
%token T_INSTANCE
%token T_INT
%token T_LIST
%token T_LT
%token T_MAP
%token T_METHOD
%token T_MOBILE
%token T_NUMBER
%token T_OBJECT
%token T_OP
%token T_QUEST
%token T_READONLY
%token T_ROOM
%token T_SEMICOLON
%token T_SHIP
%token T_STAT
%token T_STRING
%token T_TOKEN
%token T_VARARGS
%token T_VOID
%token T_WIDEVNUM

%type <number> T_NUMBER
%type <identifier> T_IDENTIFIER field_name

%type <b> possible_readonly
%type <nibtype> type return_type arg_type listtype contexttype fieldtype
%type <type_list> argtype_list optional_argtype_list
%type <flag_table> flag_table stat_table

%%

definitions: definitions def
	|
	;

def:	method_def
	|	function_def
	|	field_def
	;

method_def:
		T_METHOD return_type[R] contexttype[C] T_DOT T_IDENTIFIER[M] optional_argtype_list[A] T_ARROW T_IDENTIFIER[F] T_SEMICOLON
		{
			// Look up the internal function pointer for the given function name
			METHOD_FUNC *func = nib_method_func_lookup($F);
			if (!func)
			{
				nibmethoderrorf("Undefined method function '%s'.", $F);
				YYERROR;
			}

			// Validate $C
			if(!nib_method_valid_context($C))
			{
				yyerror("Invalid context type.");
				YYERROR;
			}

			// Validate $A
			if (!check_argtype_list($A))
			{
				yyerror("Variable arg type '...' must be the final parameter.");
				YYERROR;
			}

			// Check for duplicate signatures
			if (nib_method_exists($C, $M, $A))
			{
				yyerror("Duplicate method signature.");
				YYERROR;
			}

			// Generate method info for the argument list with return type for the given context type
			if (!nib_method_add($C, $M, $R, $A, $F, func))
			{
				yyerror("Could not add function signature.");
				YYERROR;
			}

			nib_free($M);
			nib_free($F);
			free_nib_type($C);
			free_nib_type($R);
			list_destroy($A);

		}
	;

function_def:
		T_FUNCTION return_type[R] T_IDENTIFIER[M] optional_argtype_list[A] T_ARROW T_IDENTIFIER[F] T_SEMICOLON
		{
			// Look up the internal function pointer for the given function name
			METHOD_FUNC *func = nib_method_func_lookup($F);
			if (!func)
			{
				nibmethoderrorf("Undefined method function '%s'.", $F);
				YYERROR;
			}

			// Validate $A
			if (!check_argtype_list($A))
			{
				yyerror("Variable arg type '...' must be the final parameter.");
				YYERROR;
			}

			// Check for duplicate signatures
			if (nib_method_exists(NULL, $M, $A))
			{
				yyerror("Duplicate method signature.");
				YYERROR;
			}

			// Generate method info for the argument list with return type for the given context type
			if (!nib_method_add(NULL, $M, $R, $A, $F, func))
			{
				yyerror("Could not add method signature.");
				YYERROR;
			}

			nib_free($M);
			nib_free($F);
			free_nib_type($R);
			list_destroy($A);
		}
	;

field_def:	T_FIELD possible_readonly[P] type[R] fieldtype[C] T_DOT field_name[I] T_ARROW T_IDENTIFIER[F] T_SEMICOLON
	{
		size_t *offset = nib_field_offset_lookup($C, $F);

		if (!offset)
		{
			nibmethoderrorf("No such field offset '%s' defined for '%s' type.",
				$F, nib_get_typename($C));
			YYERROR;
		}

		if (nib_field_get($C, $I))
		{
			nibmethoderrorf("Field '%s' already defined for '%s' type.",
				$I, nib_get_typename($C));
			YYERROR;
		}

		if (!nib_field_add($C, $I, $R, $P, *offset))
		{
			yyerror("Could not add field definition.");
			YYERROR;
		}

		nib_free($I);
		nib_free($F);
		free_nib_type($C);
		free_nib_type($R);
	}

field_name:
		T_IDENTIFIER			{ $$ = $1; }
	|	T_AREA					{ $$ = nib_strdup("area"); }
	|	T_STRING				{ $$ = nib_strdup("string"); }
	;

optional_argtype_list:
		T_OP T_CP					{ $$ = nib_create_type_list(); /* empty list */ }
	|	T_OP argtype_list T_CP		{ $$ = $2; }
	;	

argtype_list:
		arg_type
		{
			$$ = nib_create_type_list();
			list_appendlink($$, nib_type_copy($1));
		}
	|	argtype_list T_COMMA arg_type
		{
			list_appendlink($1, nib_type_copy($3));
			$$ = $1;
		}
	;

return_type:
		type			{ $$ = $1; }
	|	T_VOID			{ $$ = nibtype_void; }		// No return value
	|	T_ANY			{ $$ = nibtype_any; }		// Any return, will need to be handled internally to get the appropriate type
	;

arg_type:
		type			{ $$ = $1; }
	|	T_VARARGS		{ $$ = nibtype_varargs; }
	;

type:
	  T_INT											{ $$ = nibtype_int; }
	| T_FLOAT										{ $$ = nibtype_float; }
	| T_BOOLEAN										{ $$ = nibtype_bool; }
	| T_CHAR										{ $$ = nibtype_char; }
	| T_STRING										{ $$ = nibtype_string; }
	| T_MAP											{ $$ = nibtype_map; }
	| T_FLAG T_OP flag_table[T] T_CP
		{
			$$ = new_nib_type_flag_table($T);
		}
	| T_STAT T_OP stat_table[T] T_CP
		{
			$$ = new_nib_type_stat_table($T);
		}
	| T_LIST T_OP listtype[T] T_CP					{ $$ = new_nib_type_list($T); }
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

listtype:
		T_INT										{ $$ = nibtype_int; }
	|	T_FLOAT										{ $$ = nibtype_float; }
	|	T_BOOLEAN									{ $$ = nibtype_bool; }
	|	T_STRING									{ $$ = nibtype_string; }
	|	T_MAP										{ $$ = nibtype_map; }
	|	T_AREA										{ $$ = nibtype_area; }
	|	T_DUNGEON									{ $$ = nibtype_dungeon; }
	|	T_INSTANCE									{ $$ = nibtype_instance; }
	|	T_MOBILE									{ $$ = nibtype_mobile; }
	|	T_OBJECT									{ $$ = nibtype_object; }
	|	T_QUEST										{ $$ = nibtype_quest; }
	|	T_ROOM										{ $$ = nibtype_room; }
	|	T_SHIP										{ $$ = nibtype_ship; }
	|	T_TOKEN										{ $$ = nibtype_token; }
	|	T_WIDEVNUM									{ $$ = nibtype_widevnum; }
	;

contexttype:
		T_INT										{ $$ = nibtype_int; }
	|	T_FLOAT										{ $$ = nibtype_float; }
	|	T_BOOLEAN									{ $$ = nibtype_bool; }
	|	T_STRING									{ $$ = nibtype_string; }
	|	T_MAP										{ $$ = nibtype_map; }
	|	T_FLAG										{ $$ = nibtype_flag; }
	|	T_STAT										{ $$ = nibtype_stat; }
	|	T_LIST										{ $$ = nibtype_list; }
	|	T_AREA										{ $$ = nibtype_area; }
	|	T_DUNGEON									{ $$ = nibtype_dungeon; }
	|	T_INSTANCE									{ $$ = nibtype_instance; }
	|	T_MOBILE									{ $$ = nibtype_mobile; }
	|	T_OBJECT									{ $$ = nibtype_object; }
	|	T_QUEST										{ $$ = nibtype_quest; }
	|	T_ROOM										{ $$ = nibtype_room; }
	|	T_SHIP										{ $$ = nibtype_ship; }
	|	T_TOKEN										{ $$ = nibtype_token; }
	|	T_WIDEVNUM									{ $$ = nibtype_widevnum; }
	;

fieldtype:
		T_AREA										{ $$ = nibtype_area; }
	|	T_DUNGEON									{ $$ = nibtype_dungeon; }
	|	T_INSTANCE									{ $$ = nibtype_instance; }
	|	T_MOBILE									{ $$ = nibtype_mobile; }
	|	T_OBJECT									{ $$ = nibtype_object; }
	|	T_QUEST										{ $$ = nibtype_quest; }
	|	T_ROOM										{ $$ = nibtype_room; }
	|	T_SHIP										{ $$ = nibtype_ship; }
	|	T_TOKEN										{ $$ = nibtype_token; }
	|	T_WIDEVNUM									{ $$ = nibtype_widevnum; }
	;


flag_table:
		T_IDENTIFIER			
			{
				const struct flag_type *table = lookup_flag_table($1);
				if (!table)
				{
					nibmethoderrorf("Unknown flag table '%s'", $1);
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
					nibmethoderrorf("Unknown stat table '%s'", $1);
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


possible_readonly:
		T_READONLY					{ $$ = true; }
	|								{ $$ = false; }
	;

%%
