#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "../merc.h"
#include "niblang.h"

NIB_SCRIPT_STACK_TYPE convert_to_stype(NIB_TYPE *type, bool constant);

NIB_VARIABLE *nib_new_variable(char *name, NIB_TYPE *type, int scope, bool constant)
{
	NIB_VARIABLE *var = nib_calloc(1,sizeof(NIB_VARIABLE));

	var->name = nib_strdup(name);
	var->type = type;
	var->stype = convert_to_stype(type,true);
	var->scope = scope;
	var->constant = constant;
	var->initialized = false;

	return var;
}

NIB_VARIABLE *nib_copy_variable(NIB_VARIABLE *src)
{
	if (!src) return NULL;

	return nib_new_variable(src->name, src->type, src->scope, src->constant);
}

void nib_free_variable(NIB_VARIABLE *var)
{
	if (var)
	{
		if (var->name) nib_free(var->name);
		if (var->type) free_nib_type(var->type);

		nib_free(var);
	}
}

