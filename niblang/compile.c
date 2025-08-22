#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include "niblang.h"


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

static void __free_string(void *data)
{
	if (data) free(data);
}

static void *__copy_string(void *data)
{
	if (!data) return NULL;

	return strdup((char *)data);
}

static void __free_variable(void *data)
{
	if (data) nib_free_variable((NIB_VARIABLE *)data);
}

static void *__copy_variable(void *data)
{
	return nib_copy_variable((NIB_VARIABLE *)data);	
}

LLIST *nib_create_string_list()
{
	return list_createx(false, __copy_string, __free_string);
}

LLIST *nib_create_variable_list()
{
	return list_createx(false, __copy_variable, __free_variable);
}

void nib_init_scopetree();
bool nib_init_compile()
{
	nib_init_scopetree();

	nib_program_storage = new_mem_buffer();
	if (!nib_program_storage)
	{
		return false;
	}

	nib_global_variables = nib_create_variable_list();
	if (!list_isvalid(nib_global_variables))
	{
		free_mem_buffer(nib_program_storage);
		nib_program_storage = NULL;
		return false;
	}

	nib_local_variables = nib_create_variable_list();
	if (!list_isvalid(nib_local_variables))
	{
		free_mem_buffer(nib_program_storage);
		list_destroy(nib_global_variables);

		nib_program_storage = NULL;
		nib_global_variables = NULL;
		return false;
	}

	nib_string_storage = nib_create_string_list();
	if (!list_isvalid(nib_string_storage))
	{
		free_mem_buffer(nib_program_storage);
		nib_program_storage = NULL;
		return false;
	}

	return true;
}

void nib_cleanup_scopetree();
void nib_cleanup_compile()
{
	free_mem_buffer(nib_program_storage);
	list_destroy(nib_global_variables);
	list_destroy(nib_local_variables);
	list_destroy(nib_string_storage);

	nib_program_storage = NULL;
	nib_global_variables = NULL;
	nib_local_variables = NULL;
	nib_string_storage = NULL;

	nib_cleanup_scopetree();
}

bool nib_compile_script(const char *src)
{
	nib_init_compile();

	YY_BUFFER_STATE state = nib_scan_string(src);

	if (nibparse()) {
		/* error parsing */
		return false;
	}

	nib_delete_buffer(state);
	return true;
}

// Used to process the string into a compiled form for handling escape sequences
char *compile_string_literal(const char *src)
{
	return strdup(src);
}

void nib_dump_global_variables()
{
	printf("Global Variables:\n");
	printf("Scope  Name              Type\n");
	printf("==================================\n");
	ITERATOR it;
	NIB_VARIABLE *var;
	iterator_start(&it, nib_global_variables);
	while((var = (NIB_VARIABLE *)iterator_nextdata(&it)))
	{
		printf("%-5d  %-16.16s  %s\n", var->scope, var->name, nib_get_typename(var->type));
	}

	iterator_stop(&it);
	printf("\n");
}

void nib_dump_local_variables()
{
	printf("Local Variables:\n");
	printf("Scope  Name              Type\n");
	printf("==================================\n");
	ITERATOR it;
	NIB_VARIABLE *var;
	iterator_start(&it, nib_local_variables);
	while((var = (NIB_VARIABLE *)iterator_nextdata(&it)))
	{
		printf("%-5d  %-16.16s  %s", var->scope, var->name, nib_get_typename(var->type));
	}

	iterator_stop(&it);
	printf("\n");
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
		list_appendlink(nib_global_variables, var);
}

void nib_add_local_variable(NIB_VARIABLE *var)
{
	if (var)
		list_appendlink(nib_local_variables, var);
}
