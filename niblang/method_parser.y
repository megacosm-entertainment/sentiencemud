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

#include "../../merc.h"
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
static bool check_varargs_is_last(LLIST *list)
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

static bool check_anytype_on_list(NIB_TYPE *context, LLIST *list)
{
	// Don't care if it is known to be a list already
	if(context != NULL && context->type_class == NTC_LIST)
		return true;

	ITERATOR it;
	NIB_TYPE *type;
	iterator_start(&it, list);
	while((type = (NIB_TYPE *)iterator_nextdata(&it)))
	{
		if (type == nibtype_any)
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
	bool byref;
	long number;
	NIB_TYPE *nibtype;
	char *identifier;
	char *literal;
	char *str;
	LLIST *type_list;
	const struct flag_type *flag_table;
	const struct flag_type **flag_bank;
}

%destructor { free_nib_type($$); } <nibtype>
%destructor { nib_free($$); } <identifier>
%destructor { nib_free($$); } <literal>
%destructor { list_destroy($$); } <type_list>

%token T_ACCOUNT
%token T_AFFECT
%token T_ANY
%token T_AREA
%token T_ARRAY
%token T_ARROW
%token T_BAR
%token T_BOOLEAN
%token T_CB
%token T_CHANNEL
%token T_CHAR
%token T_CLASS
%token T_CMD
%token T_COLON
%token T_COMMA
%token T_CONSTANT
%token T_CP
%token T_DOT
%token T_DUNGEON
%token T_EXIT
%token T_FIELD
%token T_FLAG
%token T_FLAGBANK
%token T_FLOAT
%token T_FUNCTION
%token T_GT
%token T_IDENTIFIER
%token T_INSTANCE
%token T_INT
%token T_INT32
%token T_INT16
%token T_LIQUID
%token T_LIST
%token T_LT
%token T_MAIL
%token T_MAP
%token T_MATERIAL
%token T_MD_DESCRIPTION
%token T_MD_NAME
%token T_METHOD
%token T_MISSION
%token T_MOBILE
%token T_MULTI
%token T_NOTE
%token T_NUMBER
%token T_OB
%token T_OBJECT
%token T_OMD
%token T_OP
%token T_ORG
%token T_QUEST
%token T_RACE
%token T_RANK
%token T_REPUTATION
%token T_ROOM
%token T_SECTOR
%token T_SEMICOLON
%token T_SHIP
%token T_SKILL
%token T_STAT
%token T_STAT16
%token T_STAT32
%token T_STRING
%token T_STRING_LITERAL
%token T_TIME
%token T_TOKEN
%token T_VARARGS
%token T_VOID
%token T_WIDEVNUM
%token T_WILDS
%token T_WORLD

%type <number> T_NUMBER
%type <identifier> T_IDENTIFIER
%type <literal> T_STRING_LITERAL
%type <byref> T_ACCOUNT T_AFFECT T_AREA T_BOOLEAN T_CHANNEL T_CLASS T_CHAR T_DUNGEON T_EXIT T_FLAG T_FLAGBANK T_FLOAT T_INSTANCE T_INT T_INT32 T_INT16 T_LIQUID T_LIST T_MAIL T_MAP T_MATERIAL T_MISSION T_MOBILE T_NOTE T_OBJECT T_ORG T_QUEST T_RACE T_RANK T_REPUTATION T_ROOM T_SECTOR T_SHIP T_SKILL T_STAT T_STAT16 T_STAT32 T_STRING T_TIME T_TOKEN T_WIDEVNUM T_WILDS T_WORLD

%type <b> possible_constant
%type <nibtype> type return_type arg_type listtype contexttype fieldtype multitype
%type <type_list> argtype_list optional_argtype_list multi_list
%type <flag_table> flag_table stat_table
%type <flag_bank> flag_bank

%%

definitions: definitions def
	|
	;

def:	method_def
	|	function_def
	|	field_def
	;

method_def:
		T_METHOD possible_constant[P] return_type[R] contexttype[C] T_DOT T_IDENTIFIER[M] optional_argtype_list[A] T_ARROW T_IDENTIFIER[F] T_SEMICOLON
		{
			if ($C->_reference)
			{
				nibmethoderror("Context type may not be a by-reference type.");
				YYERROR;
			}

			// Validate $C
			if(!nib_method_valid_context($C))
			{
				yyerror("Invalid context type.");
				YYERROR;
			}

			// Look up the internal function pointer for the given function name
			METHOD_FUNC *func;
			bool lvalue;
			if (!nib_method_func_lookup($F, &func, &lvalue))
			{
				nibmethoderrorf("Undefined method function '%s'.", $F);
				YYERROR;
			}

			if (are_nib_types_equal($R,nibtype_any) &&
				$C->type_class != NTC_LIST &&
				$C->type_class != NTC_ARRAY)
			{
				yyerror("ANY return type may only be used with LIST/ARRAY contexts.");
				YYERROR;
			}

			if ($R->_reference && !lvalue)
			{
				yyerror("Return type may not be by-reference with a method that does not support it.");
				YYERROR;
			}

			// Validate $A
			if (!check_varargs_is_last($A))
			{
				yyerror("Variable arg type '...' must be the final parameter.");
				YYERROR;
			}

			if (!check_anytype_on_list($C,$A))
			{
				yyerror("ANY type may only be used with LIST contexts.");
				YYERROR;
			}

			// Check for duplicate signatures
			if (nib_method_exists($C, $M, $A))
			{
				yyerror("Duplicate method signature.");
				YYERROR;
			}

			// Generate method info for the argument list with return type for the given context type
			if (!nib_method_add($C, $M, $R, $P || !lvalue, lvalue, $A, $F, func))
			{
				yyerror("Could not add method signature.");
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
		T_FUNCTION possible_constant[P] return_type[R] T_IDENTIFIER[M] optional_argtype_list[A] T_ARROW T_IDENTIFIER[F] T_SEMICOLON
		{
			// Look up the internal function pointer for the given function name
			METHOD_FUNC *func;
			bool lvalue;
			if (!nib_method_func_lookup($F, &func, &lvalue))
			{
				nibmethoderrorf("Undefined function '%s'.", $F);
				YYERROR;
			}

			if (are_nib_types_equal($R,nibtype_any))
			{
				yyerror("ANY return type may not by used in function definitions.");
				YYERROR;
			}

			if ($R->_reference && !lvalue)
			{
				yyerror("Return type may not be by-reference with a function that does not support it.");
				YYERROR;
			}

			// Validate $A
			if (!check_varargs_is_last($A))
			{
				yyerror("Variable arg type '...' must be the final parameter.");
				YYERROR;
			}

			if (!check_anytype_on_list(NULL,$A))
			{
				yyerror("ANY type may not be used in function definitions.");
				YYERROR;
			}

			// Check for duplicate signatures
			if (nib_method_exists(NULL, $M, $A))
			{
				yyerror("Duplicate function signature.");
				YYERROR;
			}

			// Generate method info for the argument list with return type for the given context type
			if (!nib_method_add(NULL, $M, $R, $P || !lvalue, lvalue, $A, $F, func))
			{
				yyerror("Could not add function signature.");
				YYERROR;
			}

			nib_free($M);
			nib_free($F);
			free_nib_type($R);
			list_destroy($A);
		}
/* 	
	|	T_FUNCTION return_type[R] T_IDENTIFIER[M] optional_argtype_list[A]
		{
			// Create metadata structure
		}
		T_OMD function_meta_data[D] T_CMD
		{
			// Create function with the given metadata
		}
	;

function_meta_data:
		{
		}

function_meta_data_entry:
		T_MD_NAME T_COLON T_STRING_LITERAL
		{

		}
		*/
	;

field_def:
		T_FIELD possible_constant[P] type[R] fieldtype[C] T_DOT T_IDENTIFIER[I] T_ARROW T_IDENTIFIER[F] T_SEMICOLON
			{
				if ($C->_reference)
				{
					nibmethoderror("Context type may not be a by-reference type.");
					YYERROR;
				}

				size_t offset;
				size_t size;
				bool lvalue;
				
				if (!nib_field_offset_lookup($C, $F, &offset, &size, &lvalue))
				{
					nibmethoderrorf("No such field offset '%s' defined for '%s' type.",
						$F, nib_get_typename(NULL,$C));
					YYERROR;
				}

				if (are_nib_types_equal($R, nibtype_any))
				{
					yyerror("ANY return type may not by used in field definitions.");
					YYERROR;
				}

				if ($R->_reference && !lvalue)
				{
					yyerror("Return type may not be by-reference with a field that does not support it.");
					YYERROR;
				}

				if (nib_field_get($C, $I))
				{
					nibmethoderrorf("Field '%s' already defined for '%s' type.",
						$I, nib_get_typename(NULL,$C));
					YYERROR;
				}

				if (!nib_field_add($C, $I, $R, $P || !lvalue, lvalue, offset, size, NULL))
				{
					yyerror("Could not add field definition.");
					YYERROR;
				}

				nib_free($I);
				nib_free($F);
				free_nib_type($C);
				free_nib_type($R);
			}
	|	T_FIELD T_METHOD possible_constant[P] type[R] contexttype[C] T_DOT T_IDENTIFIER[I] T_ARROW T_IDENTIFIER[F] T_SEMICOLON
			{
				if ($C->_reference)
				{
					nibmethoderror("Context type may not be a by-reference type.");
					YYERROR;
				}

				METHOD_FUNC *func;
				bool lvalue;
				if (!nib_method_func_lookup($F, &func, &lvalue))
				{
					nibmethoderrorf("Undefined field-method function '%s'.", $F);
					YYERROR;
				}


				if (are_nib_types_equal($R,nibtype_any) &&
					$C->type_class != NTC_LIST &&
					$C->type_class != NTC_ARRAY)
				{
					yyerror("ANY return type may only be used with LIST/ARRAY contexts.");
					YYERROR;
				}

				if ($R->_reference && !lvalue)
				{
					yyerror("Return type may not be by-reference with a method that does not support it.");
					YYERROR;
				}

				if (nib_field_get($C, $I))
				{
					nibmethoderrorf("Field '%s' already defined for '%s' type.",
						$I, nib_get_typename(NULL,$C));
					YYERROR;
				}

				if (!nib_field_add($C, $I, $R, $P || !lvalue, lvalue, 0, 0, func))
				{
					yyerror("Could not add field definition.");
					YYERROR;
				}

				nib_free($I);
				nib_free($F);
				free_nib_type($C);
				free_nib_type($R);
			}
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
		type				{ $$ = $1; }
	|	T_VOID				{ $$ = nibtype_void; }		// No return value
	|	T_ANY				{ $$ = nibtype_any; }		// Any return, will need to be handled internally to get the appropriate type
	;

arg_type:
		type[T]				{ $$ = $T; }
	|	T_VARARGS			{ $$ = nibtype_varargs; }
	|	T_ANY				{ $$ = nibtype_any; }
	;

type:
		T_INT										{ $$ = nib_type_by_reference(nibtype_int, $1); }
	|	T_INT32										{ $$ = nib_type_by_reference(nibtype_int32, $1); }
	|	T_INT16										{ $$ = nib_type_by_reference(nibtype_int16, $1); }
	|	T_FLOAT										{ $$ = nib_type_by_reference(nibtype_float, $1); }
	|	T_BOOLEAN									{ $$ = nib_type_by_reference(nibtype_bool, $1); }
	|	T_CHAR										{ $$ = nib_type_by_reference(nibtype_char, $1); }
	|	T_STRING									{ $$ = nib_type_by_reference(nibtype_string, $1); }
	|	T_MAP										{ $$ = nib_type_by_reference(nibtype_map, $1); }
	|	T_FLAG T_OP flag_table[T] T_CP
		{
			$$ = new_nib_type_flag_table($T);
			$$->_reference = $1;
		}
	|	T_FLAGBANK T_OP flag_bank[B] T_CP
		{
			$$ = new_nib_type_flag_bank($B);
			$$->_reference = $1;
		}
	|	T_STAT T_OP stat_table[T] T_CP
		{
			$$ = new_nib_type_stat_table($T);
			$$->_reference = $1;
		}
	|	T_STAT32 T_OP stat_table[T] T_CP
		{
			// fprintf(stderr, "STAT32: %p (%s)\n", $T, nib_get_stat_table_name(NULL,$T));
			$$ = new_nib_type_stat32_table($T);
			$$->_reference = $1;
		}
	|	T_STAT16 T_OP stat_table[T] T_CP
		{
			$$ = new_nib_type_stat16_table($T);
			$$->_reference = $1;
		}
	|	T_LIST T_OP possible_constant[C] listtype[T] T_CP
		{
			if ($T->_reference)
			{
				nibmethoderror("Illegal use of reference type in LIST definition");
				YYERROR;
			}
			$$ = new_nib_type_list($T,$C);
			$$->_reference = $1;
		}
	|	T_ARRAY T_OP possible_constant[C] listtype[T] T_OB T_NUMBER[L] T_CB T_CP
		{
			if ($T->_reference)
			{
				nibmethoderror("Illegal use of reference type in ARRAY definition");
				YYERROR;
			}
			$$ = new_nib_type_array($T,$L,$C);
		}
	|	T_MULTI T_OP multi_list[L] T_CP
		{
			if (list_size($L) > 1)
				$$ = new_nib_type_multi($L);
			else
			{
				// Only one type in the MULTI, so set it to that type
				$$ = (NIB_TYPE *)list_nthdata($L,1);
				list_destroy($L);
			}
		}
	|	T_WIDEVNUM									{ $$ = nib_type_by_reference(nibtype_widevnum, $1); }
	|	T_ACCOUNT									{ $$ = nib_type_by_reference(nibtype_account, $1); }
	|	T_AFFECT									{ $$ = nib_type_by_reference(nibtype_affect, $1); }
	|	T_AREA										{ $$ = nib_type_by_reference(nibtype_area, $1); }
	|	T_CLASS										{ $$ = nib_type_by_reference(nibtype_class, $1); }
	|	T_DUNGEON									{ $$ = nib_type_by_reference(nibtype_dungeon, $1); }
	|	T_EXIT										{ $$ = nib_type_by_reference(nibtype_exit, $1); }
	|	T_INSTANCE									{ $$ = nib_type_by_reference(nibtype_instance, $1); }
	|	T_LIQUID									{ $$ = nib_type_by_reference(nibtype_liquid, $1); }
	|	T_MAIL										{ $$ = nib_type_by_reference(nibtype_mail, $1); }
	|	T_MATERIAL									{ $$ = nib_type_by_reference(nibtype_material, $1); }
	|	T_MISSION									{ $$ = nib_type_by_reference(nibtype_mission, $1); }
	|	T_MOBILE									{ $$ = nib_type_by_reference(nibtype_mobile, $1); }
	|	T_NOTE										{ $$ = nib_type_by_reference(nibtype_note, $1); }
	|	T_OBJECT									{ $$ = nib_type_by_reference(nibtype_object, $1); }
	|	T_ORG										{ $$ = nib_type_by_reference(nibtype_org, $1); }
	|	T_QUEST										{ $$ = nib_type_by_reference(nibtype_quest, $1); }
	|	T_RACE										{ $$ = nib_type_by_reference(nibtype_race, $1); }
	|	T_RANK										{ $$ = nib_type_by_reference(nibtype_rank, $1); }
	|	T_REPUTATION								{ $$ = nib_type_by_reference(nibtype_reputation, $1); }
	|	T_ROOM										{ $$ = nib_type_by_reference(nibtype_room, $1); }
	|	T_SECTOR									{ $$ = nib_type_by_reference(nibtype_sector, $1); }
	|	T_SHIP										{ $$ = nib_type_by_reference(nibtype_ship, $1); }
	|	T_SKILL										{ $$ = nib_type_by_reference(nibtype_skill, $1); }
	|	T_TIME										{ $$ = nib_type_by_reference(nibtype_time, $1); }
	|	T_TOKEN										{ $$ = nib_type_by_reference(nibtype_token, $1); }
	|	T_WILDS										{ $$ = nib_type_by_reference(nibtype_wilds, $1); }
	|	T_WORLD										{ $$ = nib_type_by_reference(nibtype_world, $1); }
	;

multi_list:
		multitype[T]
		{
			$$ = nib_create_type_list();
			list_appendlink($$,nib_type_copy($T));
		}
	|	multi_list[L] T_BAR multitype[T]
		{
			ITERATOR it;
			NIB_TYPE *type;
			iterator_start(&it,$L);
			while((type = (NIB_TYPE *)iterator_nextdata(&it)))
			{
				if (are_nib_types_equal(type, $T))
					break;
			}
			iterator_stop(&it);

			if (type != NULL)
			{
				nibmethoderrorf("Duplicate type '%s' found in multi-type.", nib_get_typename(NULL, type));
				YYERROR;
			}

			list_appendlink($L, nib_type_copy($T));
			$$ = $L;
		}
	;

listtype:
	 	T_INT										{ $$ = nib_type_by_reference(nibtype_int, $1); }
	|	T_FLOAT										{ $$ = nib_type_by_reference(nibtype_float, $1); }
	|	T_BOOLEAN									{ $$ = nib_type_by_reference(nibtype_bool, $1); }
	|	T_CHAR										{ $$ = nib_type_by_reference(nibtype_char, $1); }
	|	T_STRING									{ $$ = nib_type_by_reference(nibtype_string, $1); }
	|	T_MAP										{ $$ = nib_type_by_reference(nibtype_map, $1); }
	|	T_WIDEVNUM									{ $$ = nib_type_by_reference(nibtype_widevnum, $1); }
	|	T_ACCOUNT									{ $$ = nib_type_by_reference(nibtype_account, $1); }
	|	T_AFFECT									{ $$ = nib_type_by_reference(nibtype_affect, $1); }
	|	T_AREA										{ $$ = nib_type_by_reference(nibtype_area, $1); }
	|	T_CLASS										{ $$ = nib_type_by_reference(nibtype_class, $1); }
	|	T_DUNGEON									{ $$ = nib_type_by_reference(nibtype_dungeon, $1); }
	|	T_EXIT										{ $$ = nib_type_by_reference(nibtype_exit, $1); }
	|	T_INSTANCE									{ $$ = nib_type_by_reference(nibtype_instance, $1); }
	|	T_LIQUID									{ $$ = nib_type_by_reference(nibtype_liquid, $1); }
	|	T_MAIL										{ $$ = nib_type_by_reference(nibtype_mail, $1); }
	|	T_MATERIAL									{ $$ = nib_type_by_reference(nibtype_material, $1); }
	|	T_MISSION									{ $$ = nib_type_by_reference(nibtype_mission, $1); }
	|	T_MOBILE									{ $$ = nib_type_by_reference(nibtype_mobile, $1); }
	|	T_NOTE										{ $$ = nib_type_by_reference(nibtype_note, $1); }
	|	T_OBJECT									{ $$ = nib_type_by_reference(nibtype_object, $1); }
	|	T_ORG										{ $$ = nib_type_by_reference(nibtype_org, $1); }
	|	T_QUEST										{ $$ = nib_type_by_reference(nibtype_quest, $1); }
	|	T_RACE										{ $$ = nib_type_by_reference(nibtype_race, $1); }
	|	T_RANK										{ $$ = nib_type_by_reference(nibtype_rank, $1); }
	|	T_REPUTATION								{ $$ = nib_type_by_reference(nibtype_reputation, $1); }
	|	T_ROOM										{ $$ = nib_type_by_reference(nibtype_room, $1); }
	|	T_SECTOR									{ $$ = nib_type_by_reference(nibtype_sector, $1); }
	|	T_SHIP										{ $$ = nib_type_by_reference(nibtype_ship, $1); }
	|	T_SKILL										{ $$ = nib_type_by_reference(nibtype_skill, $1); }
	|	T_TIME										{ $$ = nib_type_by_reference(nibtype_time, $1); }
	|	T_TOKEN										{ $$ = nib_type_by_reference(nibtype_token, $1); }
	|	T_WILDS										{ $$ = nib_type_by_reference(nibtype_wilds, $1); }
	|	T_WORLD										{ $$ = nib_type_by_reference(nibtype_world, $1); }
	;

multitype:
	 	T_INT										{ $$ = nib_type_by_reference(nibtype_int, $1); }
	|	T_FLOAT										{ $$ = nib_type_by_reference(nibtype_float, $1); }
	|	T_BOOLEAN									{ $$ = nib_type_by_reference(nibtype_bool, $1); }
	|	T_CHAR										{ $$ = nib_type_by_reference(nibtype_char, $1); }
	|	T_STRING									{ $$ = nib_type_by_reference(nibtype_string, $1); }
	|	T_MAP										{ $$ = nib_type_by_reference(nibtype_map, $1); }
	|	T_WIDEVNUM									{ $$ = nib_type_by_reference(nibtype_widevnum, $1); }
	|	T_ACCOUNT									{ $$ = nib_type_by_reference(nibtype_account, $1); }
	|	T_AFFECT									{ $$ = nib_type_by_reference(nibtype_affect, $1); }
	|	T_AREA										{ $$ = nib_type_by_reference(nibtype_area, $1); }
	|	T_CLASS										{ $$ = nib_type_by_reference(nibtype_class, $1); }
	|	T_DUNGEON									{ $$ = nib_type_by_reference(nibtype_dungeon, $1); }
	|	T_EXIT										{ $$ = nib_type_by_reference(nibtype_exit, $1); }
	|	T_INSTANCE									{ $$ = nib_type_by_reference(nibtype_instance, $1); }
	|	T_LIQUID									{ $$ = nib_type_by_reference(nibtype_liquid, $1); }
	|	T_MAIL										{ $$ = nib_type_by_reference(nibtype_mail, $1); }
	|	T_MATERIAL									{ $$ = nib_type_by_reference(nibtype_material, $1); }
	|	T_MISSION									{ $$ = nib_type_by_reference(nibtype_mission, $1); }
	|	T_MOBILE									{ $$ = nib_type_by_reference(nibtype_mobile, $1); }
	|	T_NOTE										{ $$ = nib_type_by_reference(nibtype_note, $1); }
	|	T_OBJECT									{ $$ = nib_type_by_reference(nibtype_object, $1); }
	|	T_ORG										{ $$ = nib_type_by_reference(nibtype_org, $1); }
	|	T_QUEST										{ $$ = nib_type_by_reference(nibtype_quest, $1); }
	|	T_RACE										{ $$ = nib_type_by_reference(nibtype_race, $1); }
	|	T_RANK										{ $$ = nib_type_by_reference(nibtype_rank, $1); }
	|	T_REPUTATION								{ $$ = nib_type_by_reference(nibtype_reputation, $1); }
	|	T_ROOM										{ $$ = nib_type_by_reference(nibtype_room, $1); }
	|	T_SECTOR									{ $$ = nib_type_by_reference(nibtype_sector, $1); }
	|	T_SHIP										{ $$ = nib_type_by_reference(nibtype_ship, $1); }
	|	T_SKILL										{ $$ = nib_type_by_reference(nibtype_skill, $1); }
	|	T_TIME										{ $$ = nib_type_by_reference(nibtype_time, $1); }
	|	T_TOKEN										{ $$ = nib_type_by_reference(nibtype_token, $1); }
	|	T_WILDS										{ $$ = nib_type_by_reference(nibtype_wilds, $1); }
	|	T_WORLD										{ $$ = nib_type_by_reference(nibtype_world, $1); }
	;

contexttype:
	 	T_INT										{ $$ = nib_type_by_reference(nibtype_int, $1); }
	|	T_FLOAT										{ $$ = nib_type_by_reference(nibtype_float, $1); }
	|	T_BOOLEAN									{ $$ = nib_type_by_reference(nibtype_bool, $1); }
	|	T_CHAR										{ $$ = nib_type_by_reference(nibtype_char, $1); }
	|	T_STRING									{ $$ = nib_type_by_reference(nibtype_string, $1); }
	|	T_MAP										{ $$ = nib_type_by_reference(nibtype_map, $1); }
	|	T_FLAG										{ $$ = nib_type_by_reference(nibtype_flag, $1); }
	|	T_STAT										{ $$ = nib_type_by_reference(nibtype_stat, $1); }
	|	T_LIST										{ $$ = nib_type_by_reference(nibtype_list, $1); }
	|	T_ARRAY										{ $$ = nibtype_array; }
	|	T_WIDEVNUM									{ $$ = nib_type_by_reference(nibtype_widevnum, $1); }
	|	T_ACCOUNT									{ $$ = nib_type_by_reference(nibtype_account, $1); }
	|	T_AFFECT									{ $$ = nib_type_by_reference(nibtype_affect, $1); }
	|	T_AREA										{ $$ = nib_type_by_reference(nibtype_area, $1); }
	|	T_CLASS										{ $$ = nib_type_by_reference(nibtype_class, $1); }
	|	T_DUNGEON									{ $$ = nib_type_by_reference(nibtype_dungeon, $1); }
	|	T_EXIT										{ $$ = nib_type_by_reference(nibtype_exit, $1); }
	|	T_INSTANCE									{ $$ = nib_type_by_reference(nibtype_instance, $1); }
	|	T_LIQUID									{ $$ = nib_type_by_reference(nibtype_liquid, $1); }
	|	T_MAIL										{ $$ = nib_type_by_reference(nibtype_mail, $1); }
	|	T_MATERIAL									{ $$ = nib_type_by_reference(nibtype_material, $1); }
	|	T_MISSION									{ $$ = nib_type_by_reference(nibtype_mission, $1); }
	|	T_MOBILE									{ $$ = nib_type_by_reference(nibtype_mobile, $1); }
	|	T_NOTE										{ $$ = nib_type_by_reference(nibtype_note, $1); }
	|	T_OBJECT									{ $$ = nib_type_by_reference(nibtype_object, $1); }
	|	T_ORG										{ $$ = nib_type_by_reference(nibtype_org, $1); }
	|	T_QUEST										{ $$ = nib_type_by_reference(nibtype_quest, $1); }
	|	T_RACE										{ $$ = nib_type_by_reference(nibtype_race, $1); }
	|	T_RANK										{ $$ = nib_type_by_reference(nibtype_rank, $1); }
	|	T_REPUTATION								{ $$ = nib_type_by_reference(nibtype_reputation, $1); }
	|	T_ROOM										{ $$ = nib_type_by_reference(nibtype_room, $1); }
	|	T_SECTOR									{ $$ = nib_type_by_reference(nibtype_sector, $1); }
	|	T_SHIP										{ $$ = nib_type_by_reference(nibtype_ship, $1); }
	|	T_SKILL										{ $$ = nib_type_by_reference(nibtype_skill, $1); }
	|	T_TIME										{ $$ = nib_type_by_reference(nibtype_time, $1); }
	|	T_TOKEN										{ $$ = nib_type_by_reference(nibtype_token, $1); }
	|	T_WILDS										{ $$ = nib_type_by_reference(nibtype_wilds, $1); }
	|	T_WORLD										{ $$ = nib_type_by_reference(nibtype_world, $1); }
	;

fieldtype:
		T_WIDEVNUM									{ $$ = nib_type_by_reference(nibtype_widevnum, $1); }
	|	T_ACCOUNT									{ $$ = nib_type_by_reference(nibtype_account, $1); }
	|	T_AFFECT									{ $$ = nib_type_by_reference(nibtype_affect, $1); }
	|	T_AREA										{ $$ = nib_type_by_reference(nibtype_area, $1); }
	|	T_CLASS										{ $$ = nib_type_by_reference(nibtype_class, $1); }
	|	T_DUNGEON									{ $$ = nib_type_by_reference(nibtype_dungeon, $1); }
	|	T_EXIT										{ $$ = nib_type_by_reference(nibtype_exit, $1); }
	|	T_INSTANCE									{ $$ = nib_type_by_reference(nibtype_instance, $1); }
	|	T_LIQUID									{ $$ = nib_type_by_reference(nibtype_liquid, $1); }
	|	T_MAIL										{ $$ = nib_type_by_reference(nibtype_mail, $1); }
	|	T_MATERIAL									{ $$ = nib_type_by_reference(nibtype_material, $1); }
	|	T_MISSION									{ $$ = nib_type_by_reference(nibtype_mission, $1); }
	|	T_MOBILE									{ $$ = nib_type_by_reference(nibtype_mobile, $1); }
	|	T_NOTE										{ $$ = nib_type_by_reference(nibtype_note, $1); }
	|	T_OBJECT									{ $$ = nib_type_by_reference(nibtype_object, $1); }
	|	T_ORG										{ $$ = nib_type_by_reference(nibtype_org, $1); }
	|	T_QUEST										{ $$ = nib_type_by_reference(nibtype_quest, $1); }
	|	T_RACE										{ $$ = nib_type_by_reference(nibtype_race, $1); }
	|	T_RANK										{ $$ = nib_type_by_reference(nibtype_rank, $1); }
	|	T_REPUTATION								{ $$ = nib_type_by_reference(nibtype_reputation, $1); }
	|	T_ROOM										{ $$ = nib_type_by_reference(nibtype_room, $1); }
	|	T_SECTOR									{ $$ = nib_type_by_reference(nibtype_sector, $1); }
	|	T_SHIP										{ $$ = nib_type_by_reference(nibtype_ship, $1); }
	|	T_SKILL										{ $$ = nib_type_by_reference(nibtype_skill, $1); }
	|	T_TIME										{ $$ = nib_type_by_reference(nibtype_time, $1); }
	|	T_TOKEN										{ $$ = nib_type_by_reference(nibtype_token, $1); }
	|	T_WILDS										{ $$ = nib_type_by_reference(nibtype_wilds, $1); }
	|	T_WORLD										{ $$ = nib_type_by_reference(nibtype_world, $1); }
	;

flag_table:
		T_IDENTIFIER			
			{
				const struct flag_type *table = nib_lookup_flag_table(NULL, $1);
				if (!table)
				{
					nibmethoderrorf("Unknown flag table '%s'", $1);
					YYERROR;
				}

				$$ = table;
				nib_free($1);
			}
	|	T_STRING_LITERAL
			{
				const struct flag_type *table = nib_lookup_flag_table(NULL,$1);
				if (!table)
				{
					nibmethoderrorf("Unknown flag table '%s'", $1);
					YYERROR;
				}

				$$ = table;
				free($1);
			}
	;

flag_bank:
		T_IDENTIFIER			
			{
				const struct flag_type **bank = nib_lookup_flag_bank($1);
				if (!bank)
				{
					nibmethoderrorf("Unknown flag bank '%s'", $1);
					YYERROR;
				}

				$$ = bank;
				nib_free($1);
			}
	|	T_STRING_LITERAL
			{
				const struct flag_type **bank = nib_lookup_flag_bank($1);
				if (!bank)
				{
					nibmethoderrorf("Unknown flag bank '%s'", $1);
					YYERROR;
				}

				$$ = bank;
				free($1);
			}
	;

stat_table:
		T_IDENTIFIER			
			{
				const struct flag_type *table = nib_lookup_stat_table(NULL,$1);
				if (!table)
				{
					nibmethoderrorf("Unknown stat table '%s'", $1);
					YYERROR;
				}

				$$ = table;
				nib_free($1);
			}
	|	T_STRING_LITERAL
			{
				const struct flag_type *table = nib_lookup_stat_table(NULL,$1);
				if (!table)
				{
					nibmethoderrorf("Unknown stat table '%s'", $1);
					YYERROR;
				}

				$$ = table;
				free($1);
			}
	;


possible_constant:
		T_CONSTANT					{ $$ = true; }
	|								{ $$ = false; }
	;

%%
