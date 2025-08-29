#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <inttypes.h>
#include <stdint.h>


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

	if (!flag_tables_init()) return false;

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

	flag_tables_cleanup();

	nib_cleanup_scopetree();
}

bool nib_compile_script(const char *src)
{
	if(!nib_init_compile()) return false;

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
			nib_get_typename(var->type));
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
			nib_get_typename(var->type));
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

int nib_get_string_in_storage(const char *str)
{
	if(!str) return 0;	
	if(!list_isvalid(nib_string_storage)) return 0;

	ITERATOR it;
	char *name;
	int index = 0;
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

int nib_add_string_to_storage(const char *str)
{
	if(!str) return 0;	

	// Make sure it is not in the storage already
	int index = nib_get_string_in_storage(str);
	if (index > 0) return index;

	if (!list_isvalid(nib_string_storage))
		nib_string_storage = nib_create_string_list();

	list_appendlink(nib_string_storage, nib_strdup(str));
	return list_size(nib_string_storage);
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
