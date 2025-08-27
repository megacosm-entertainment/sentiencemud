/*

Dummy file that will contain method functions to be referenced by the pointer table

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "niblang.h"

AREA_DATA *new_area()
{
	AREA_DATA *area = calloc(1, sizeof(AREA_DATA));

	area->name = strdup("");
	area->description = strdup("");

	return area;
}

void free_area(AREA_DATA *area)
{
	if (area)
	{
		free(area->name);
		free(area->description);
		free(area);
	}
}

// NUMBER methods
DECL_METHOD_FUNC(number_random_value)
{
	// Get a number from 0 to N-1

	return 0;
}

// FLOAT methods

// BOOLEAN methods

// STRING methods
DECL_METHOD_FUNC(string_length)
{
	// Get length of string
	return 0;
}

// FLAG methods

// LIST methods

// STAT methods

// MAP methods

// WIDEVNUM methods

// AREA Methods
DECL_METHOD_FUNC(area_get_name)
{
	return 0;
}

// DUNGEON methods

// INSTANCE methods

// MOBILE methods

// OBJECT methods

// QUEST methods

// ROOM methods

// SHIP methods

// TOKEN methods
