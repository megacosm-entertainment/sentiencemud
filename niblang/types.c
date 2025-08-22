#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>


#include "niblang.h"


static NIB_TYPE __nibtype_bool		= { true, NTC_PRIMARY, {NT_BOOLEAN}, "bool"};
static NIB_TYPE __nibtype_int		= { true, NTC_PRIMARY, {NT_NUMBER}, "int"};
static NIB_TYPE __nibtype_float		= { true, NTC_PRIMARY, {NT_FLOAT}, "float"};
static NIB_TYPE __nibtype_string	= { true, NTC_PRIMARY, {NT_STRING}, "string"};
static NIB_TYPE __nibtype_map		= { true, NTC_PRIMARY, {NT_MAP}, "map"};
static NIB_TYPE __nibtype_flag		= { true, NTC_FLAG, {32}, "flag"};
static NIB_TYPE __nibtype_widevnum	= { true, NTC_PRIMARY, {NT_WIDEVNUM}, "widevnum"};
static NIB_TYPE __nibtype_area		= { true, NTC_PRIMARY, {NT_AREA}, "area"};
static NIB_TYPE __nibtype_dungeon	= { true, NTC_PRIMARY, {NT_DUNGEON}, "dungeon"};
static NIB_TYPE __nibtype_instance	= { true, NTC_PRIMARY, {NT_INSTANCE}, "instance"};
static NIB_TYPE __nibtype_mobile	= { true, NTC_PRIMARY, {NT_MOBILE}, "mobile"};
static NIB_TYPE __nibtype_object	= { true, NTC_PRIMARY, {NT_OBJECT}, "object"};
static NIB_TYPE __nibtype_quest		= { true, NTC_PRIMARY, {NT_QUEST}, "quest"};
static NIB_TYPE __nibtype_room		= { true, NTC_PRIMARY, {NT_ROOM}, "room"};
static NIB_TYPE __nibtype_ship		= { true, NTC_PRIMARY, {NT_SHIP}, "ship"};
static NIB_TYPE __nibtype_token		= { true, NTC_PRIMARY, {NT_TOKEN}, "token"};


NIB_TYPE *nibtype_bool = &__nibtype_bool;
NIB_TYPE *nibtype_int  = &__nibtype_int;
NIB_TYPE *nibtype_float = &__nibtype_float;
NIB_TYPE *nibtype_string = &__nibtype_string;
NIB_TYPE *nibtype_map = &__nibtype_map;
NIB_TYPE *nibtype_flag = &__nibtype_flag;
NIB_TYPE *nibtype_widevnum = &__nibtype_widevnum;
NIB_TYPE *nibtype_area = &__nibtype_area;
NIB_TYPE *nibtype_dungeon = &__nibtype_dungeon;
NIB_TYPE *nibtype_instance = &__nibtype_instance;
NIB_TYPE *nibtype_mobile = &__nibtype_mobile;
NIB_TYPE *nibtype_object = &__nibtype_object;
NIB_TYPE *nibtype_quest = &__nibtype_quest;
NIB_TYPE *nibtype_room = &__nibtype_room;
NIB_TYPE *nibtype_ship = &__nibtype_ship;
NIB_TYPE *nibtype_token = &__nibtype_token;

LLIST *nib_create_string_list();
NIB_TYPE *new_nib_type_flag(int bits)
{
	NIB_TYPE *type = calloc(1,sizeof(NIB_TYPE));

	type->_static = false;
	type->type_class = NTC_FLAG;
	type->_.flag.bits = bits;
	type->_.flag.names = NULL;
	type->name = NULL;

	return type;
}

NIB_TYPE *new_nib_type_flag_named(LLIST *names)
{
	NIB_TYPE *type = calloc(1,sizeof(NIB_TYPE));

	type->_static = false;
	type->type_class = NTC_FLAG;
	type->_.flag.bits = 0;			// Indicate that it is named flag
	type->_.flag.names = list_copy(names);
	type->name = NULL;

	return type;
}

NIB_TYPE *new_nib_type_list(NIB_TYPE *elem)
{
	NIB_TYPE *type = calloc(1,sizeof(NIB_TYPE));

	type->_static = false;
	type->type_class = NTC_LIST;
	type->_.type = elem;
	type->name = NULL;

	return type;
}

void free_nib_type(NIB_TYPE *type)
{
	if (type && !type->_static)
	{
		if (type->type_class == NTC_LIST)
		{
			free_nib_type(type->_.type);
		}
		else if (type->type_class == NTC_FLAG)
		{
			list_destroy(type->_.flag.names);
		}

		free(type);
	}
}

#define MSL 10240
#define MSN 20
char *nib_get_typename(NIB_TYPE *type)
{
	static char buf[MSN][MSL];
	static int i = 0;

	// Already has a name
	if (type->name)
		return type->name;

	i = (i + 1) % MSN;
	char *p = buf[i];
	if (type->type_class == NTC_LIST)
	{
		snprintf(p, MSL-1, "list(%s)", nib_get_typename(type->_.type));
	}
	else if (type->type_class == NTC_FLAG)
	{
		if (type->_.flag.bits > 0)
		{
			snprintf(p, MSL-1, "flag(%d)", type->_.flag.bits);
		}
		else
		{
			strcpy(p, "flag(");
			ITERATOR it;
			char *name;
			bool first = false;
			iterator_start(&it, type->_.flag.names);
			while((name = (char *)iterator_nextdata(&it)))
			{
				if (!first) strcat(p, ",");

				strcat(p, name);
			}
			iterator_stop(&it);
			strcat(p, ")");
		}
	}
	
	p[MSL-1] = '\0';
	return p;
}

// When used for assignment or conversion:
//  a = destination
//  b = source
//  example:  int a;  a = true;
bool are_nib_types_compatible(NIB_TYPE *a, NIB_TYPE *b)
{
	if (!a || !b) return false;

	if (a == b) return true;	// Automatically compatible if they are the same

	// Need to compare based upon details of the type, ignore the static flag

	if (a->type_class != b->type_class)		// Not even the same class of type
		return false;

	switch(a->type_class)
	{
		case NTC_PRIMARY:
			if (a->_.primary == b->_.primary) return true;	// They are the same

			// Comparable numbers 
			switch (a->_.primary)
			{
				case NT_NUMBER:
					return b->_.primary == NT_BOOLEAN;
				
				case NT_FLOAT:
					return b->_.primary == NT_BOOLEAN ||
						b->_.primary == NT_NUMBER;
			}

			return false;

		case NTC_FLAG:
		{
			// Compare the bits
			if (a->_.flag.bits != b->_.flag.bits) return false;

			if (a->_.flag.bits > 0) return true;	// Numerical bits

			if (list_size(a->_.flag.names) != list_size(b->_.flag.names)) return false;

			ITERATOR ita, itb;
			char *n_a = NULL, *n_b = NULL;
			iterator_start(&ita, a->_.flag.names);
			iterator_start(&itb, b->_.flag.names);
			while((n_a = (char *)iterator_nextdata(&ita)) != NULL ||
				(n_b = (char *)iterator_nextdata(&itb)) != NULL)
				{
					if (str_cmp(n_a, n_b))
						break;
				}

			iterator_stop(&ita);
			iterator_stop(&itb);

			return !n_a && !n_b;
		}

		case NTC_LIST:
			return are_nib_types_compatible(a->_.type, b->_.type);

		default:
			return false;
	}
}
