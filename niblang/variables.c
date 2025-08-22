#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "niblang.h"

NIB_VARIABLE *nib_new_variable(char *name, NIB_TYPE *type, int scope)
{
	NIB_VARIABLE *var = calloc(1,sizeof(NIB_VARIABLE));

	var->name = strdup(name);
	var->type = type;
	var->scope = scope;

	//printf("New Variable: %s, %s, %d\n", name, nib_get_typename(type), scope);

	return var;
}

NIB_VARIABLE *nib_copy_variable(NIB_VARIABLE *src)
{
	if (!src) return NULL;

	return nib_new_variable(src->name, src->type, src->scope);
}

void nib_free_variable(NIB_VARIABLE *var)
{
	if (var)
	{
		if (var->name) free(var->name);

		free(var);
	}
}

