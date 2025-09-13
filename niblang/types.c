#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>


#include "niblang.h"
#include "interpret.h"

extern LLIST *nib_flag_created_tables;
extern LLIST *nib_stat_created_tables;

static NIB_TYPE __nibtype_null		= { true, NTC_ANY, {0}, "null"};
static NIB_TYPE __nibtype_void		= { true, NTC_VOID, {0}, "void"};
static NIB_TYPE __nibtype_any		= { true, NTC_ANY, {0}, "any"};
static NIB_TYPE __nibtype_bool		= { true, NTC_PRIMARY, {NT_BOOLEAN}, "boolean"};
static NIB_TYPE __nibtype_char		= { true, NTC_PRIMARY, {NT_CHAR}, "char"};
static NIB_TYPE __nibtype_int		= { true, NTC_PRIMARY, {NT_NUMBER}, "int"};
static NIB_TYPE __nibtype_float		= { true, NTC_PRIMARY, {NT_FLOAT}, "float"};
static NIB_TYPE __nibtype_string	= { true, NTC_PRIMARY, {NT_STRING}, "string"};
static NIB_TYPE __nibtype_map		= { true, NTC_PRIMARY, {NT_MAP}, "map"};
static NIB_TYPE __nibtype_flag		= { true, NTC_FLAG, {.flag = {0, NULL, NULL}}, "flag"};
static NIB_TYPE __nibtype_list		= { true, NTC_LIST, {.type = NULL}, "list"};
static NIB_TYPE __nibtype_stat		= { true, NTC_STAT, {.stat = {NULL, NULL}}, "stat"};
static NIB_TYPE __nibtype_varargs	= { true, NTC_VARARGS, {0}, "..."};
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

/* Types to add: TODO

Basic Types:
coord

Game Types:
exit
player
account
church/org
channel
objective
wilds
world
note		?
mail		?
script		?
liquid
skill
class
race
material
mission
reputation

*/

NIB_TYPE *nibtype_null = &__nibtype_null;
NIB_TYPE *nibtype_void = &__nibtype_void;
NIB_TYPE *nibtype_any = &__nibtype_any;
NIB_TYPE *nibtype_bool = &__nibtype_bool;
NIB_TYPE *nibtype_char = &__nibtype_char;
NIB_TYPE *nibtype_int  = &__nibtype_int;
NIB_TYPE *nibtype_float = &__nibtype_float;
NIB_TYPE *nibtype_string = &__nibtype_string;
NIB_TYPE *nibtype_map = &__nibtype_map;
NIB_TYPE *nibtype_flag = &__nibtype_flag;
NIB_TYPE *nibtype_list = &__nibtype_list;
NIB_TYPE *nibtype_stat = &__nibtype_stat;
NIB_TYPE *nibtype_varargs = &__nibtype_varargs;
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

// These will only be flags, stats and lists
LLIST *nibtype_created_types = NULL;

LLIST *nib_create_type_list();
bool nibtype_init()
{
	nibtype_created_types = nib_create_type_list();
	if (!list_isvalid(nibtype_created_types)) return false;

	return true;
}

void nibtype_cleanup()
{
	list_destroy(nibtype_created_types);
}

void nibtype_add(NIB_TYPE *type, char *name)
{
	NIB_TYPE *_type = nib_type_copy(type);
	_type->name = nib_strdup(name);

	list_appendlink(nibtype_created_types, _type);
}

NIB_TYPE *nibtype_get(char *name)
{
	ITERATOR it;
	NIB_TYPE *type;

	iterator_start(&it, nibtype_created_types);
	while((type = (NIB_TYPE *)iterator_nextdata(&it)))
	{
		if (type->name && !str_cmp(type->name, name))
			break;
	}
	iterator_stop(&it);

	return type;
}

void nib_dump_created_types()
{
#if 0
	if (list_size(nibtype_created_types) > 0)
	{
		printf("Create Types:\n");
		printf("[     Name     ] Type\n");
		printf("==========================================\n");

		ITERATOR it;
		NIB_TYPE *type;

		iterator_start(&it, nibtype_created_types);
		while((type = (NIB_TYPE *)iterator_nextdata(&it)))
		{
			char *name = type->name;
			type->name = NULL;

			printf("%-16.16s %s\n", name, nib_get_typename(type));

			type->name = name;
		}
		iterator_stop(&it);
	}
#endif
}

LLIST *nib_create_string_list();
NIB_TYPE *new_nib_type_flag(int bits)
{
	NIB_TYPE *type = nib_calloc(1,sizeof(NIB_TYPE));

	type->_static = false;
	type->type_class = NTC_FLAG;
	type->_.flag.bits = bits;
	type->_.flag.names = NULL;
	type->name = NULL;

	return type;
}

NIB_TYPE *new_nib_type_flag_named(LLIST *names)
{
	NIB_TYPE *type = nib_calloc(1,sizeof(NIB_TYPE));

	type->_static = false;
	type->type_class = NTC_FLAG;
	type->_.flag.bits = 0;			// Indicate that it is named flag
	type->_.flag.names = list_copy(names);
	type->_.flag.table = NULL;
	type->name = NULL;

	return type;
}

NIB_TYPE *new_nib_type_flag_table(const struct flag_type *table)
{
	NIB_TYPE *type = nib_calloc(1,sizeof(NIB_TYPE));

	type->_static = false;
	type->type_class = NTC_FLAG;
	type->_.flag.bits = 0;
	type->_.flag.names = NULL;
	type->_.flag.table = table;
	type->name = NULL;

	return type;
}

NIB_TYPE *new_nib_type_stat_named(LLIST *names)
{
	NIB_TYPE *type = nib_calloc(1,sizeof(NIB_TYPE));

	type->_static = false;
	type->type_class = NTC_STAT;
	type->_.stat.names = list_copy(names);
	type->name = NULL;

	return type;
}

NIB_TYPE *new_nib_type_stat_table(const struct flag_type *table)
{
	NIB_TYPE *type = nib_calloc(1,sizeof(NIB_TYPE));

	type->_static = false;
	type->type_class = NTC_STAT;
	type->_.stat.names = NULL;
	type->_.stat.table = table;
	type->name = NULL;

	return type;
}


NIB_TYPE *new_nib_type_list(NIB_TYPE *elem)
{
	NIB_TYPE *type = nib_calloc(1,sizeof(NIB_TYPE));

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
			// if (type->_.flag.table)
			// {
			// 	printf("free_nib_type(flag(%s))\n", get_flag_table_name(type->_.flag.table));
			// }
			list_destroy(type->_.flag.names);
		}
		else if (type->type_class == NTC_STAT)
		{
			list_destroy(type->_.stat.names);
		}

		if (type->name) nib_free(type->name);
		nib_free(type);
	}
}

NIB_TYPE *nib_type_copy(NIB_TYPE *src)
{
	if (!src) return nibtype_void;

	if (src->_static) return src;	// Straight keep the static reference

	NIB_TYPE *dest = nib_calloc(1,sizeof(NIB_TYPE));

	dest->_static = false;
	dest->type_class = src->type_class;
	if (src->name)
		dest->name = nib_strdup(src->name);
	else
		dest->name = NULL;

	switch(src->type_class)
	{
		case NTC_PRIMARY:
			dest->_.primary = src->_.primary;
			break;

		case NTC_LIST:
			dest->_.type = nib_type_copy(src->_.type);
			break;

		case NTC_FLAG:
			dest->_.flag.bits = src->_.flag.bits;
			dest->_.flag.names = list_copy(src->_.flag.names);
			dest->_.flag.table = src->_.flag.table;
			break;

		case NTC_STAT:
			dest->_.stat.names = list_copy(src->_.flag.names);
			dest->_.stat.table = src->_.stat.table;
			break;
	}

	return dest;
}

#define MSL 10240
#define MSN 20
char *nib_get_typename(NIB_SCRIPT *context, NIB_TYPE *type)
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
		snprintf(p, MSL-1, "list(%s)", nib_get_typename(context, type->_.type));
	}
	else if (type->type_class == NTC_FLAG)
	{
		if (type->_.flag.bits > 0)
		{
			snprintf(p, MSL-1, "flag(%d)", type->_.flag.bits);
		}
		else if (type->_.flag.table)
		{
			snprintf(p, MSL-1, "flag(%s)", nib_get_flag_table_name((context?context->flag_tables:nib_flag_created_tables),type->_.flag.table));
		}
		else
		{
			strcpy(p, "flag(<");
			ITERATOR it;
			char *name;
			bool first = true;
			iterator_start(&it, type->_.flag.names);
			while((name = (char *)iterator_nextdata(&it)))
			{
				if (first)
					first = false;
				else
					strcat(p, ",");

				strcat(p, name);
			}
			iterator_stop(&it);
			strcat(p, ">)");
		}
	}
	else if (type->type_class == NTC_STAT)
	{
		if (type->_.stat.table)
		{
			snprintf(p, MSL-1, "stat(%s)", nib_get_stat_table_name((context?context->stat_tables:nib_stat_created_tables),type->_.stat.table));
		}
		else
		{
			strcpy(p, "stat(<");
			ITERATOR it;
			char *name;
			bool first = true;
			iterator_start(&it, type->_.stat.names);
			while((name = (char *)iterator_nextdata(&it)))
			{
				if (first)
					first = false;
				else
					strcat(p, ",");

				strcat(p, name);
			}
			iterator_stop(&it);
			strcat(p, ">)");
		}
	}
	
	p[MSL-1] = '\0';
	return p;
}

int nib_type_get_flag_index(NIB_TYPE *type, const char *str)
{
	if (!type) return 0;
	if (type->type_class != NTC_FLAG) return 0;
	if (list_size(type->_.flag.names) < 1) return 0;
	if (list_size(type->_.flag.names) > MAX_FLAG_BITS) return 0;

	int index = 0;
	ITERATOR it;
	char *name;

	iterator_start(&it, type->_.flag.names);
	while((name = (char *)iterator_nextdata(&it)))
	{
		++index;

		if (!str_cmp(name, str))
			break;
	}
	iterator_stop(&it);

	return name ? index : 0;
}

bool are_nib_types_equal(NIB_TYPE *a, NIB_TYPE *b)
{
	if (!a || !b) return false;

	if (a == b) return true;	// Automatically compatible if they are the same pointer

	// Need to compare based upon details of the type, ignore the static flag

	if (a->type_class != b->type_class)		// Not even the same class of type
		return false;

	switch(a->type_class)
	{
		case NTC_PRIMARY:
			if (a->_.primary == b->_.primary) return true;	// They are the same

		case NTC_FLAG:
		{
			// Compare the bits
			if (a->_.flag.bits != b->_.flag.bits) return false;

			if (a->_.flag.bits > 0) return true;	// Numerical bits

			if (a->_.flag.table != b->_.flag.table) return true;	// Table flags

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

		case NTC_STAT:
		{
			if (a->_.stat.table != b->_.stat.table) return true;	// Table flags

			if (list_size(a->_.stat.names) != list_size(b->_.stat.names)) return false;

			ITERATOR ita, itb;
			char *n_a = NULL, *n_b = NULL;
			iterator_start(&ita, a->_.stat.names);
			iterator_start(&itb, b->_.stat.names);
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
			return are_nib_types_equal(a->_.type, b->_.type);

	}

	return false;
}

// When used for assignment or conversion:
//  a = destination
//  b = source
//  example:  int a;  a = true;
bool are_nib_types_compatible(NIB_TYPE *a, NIB_TYPE *b)
{
	if (!a || !b) return false;

	if (a == b) return true;	// Automatically compatible if they are the same pointer

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
					return b->_.primary == NT_BOOLEAN || b->_.primary == NT_CHAR;
				
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

			if (a->_.flag.table != b->_.flag.table) return false;	// Table flags

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

		case NTC_STAT:
		{
			if (a->_.stat.table != b->_.stat.table) return false;

			if (list_size(a->_.stat.names) != list_size(b->_.stat.names)) return false;

			ITERATOR ita, itb;
			char *n_a = NULL, *n_b = NULL;
			iterator_start(&ita, a->_.stat.names);
			iterator_start(&itb, b->_.stat.names);
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

NIB_TYPE *nib_combine_types(NIB_TYPE *a, NIB_TYPE *b)
{
	if (a->type_class == NTC_PRIMARY)
	{
		if (b->type_class == NTC_PRIMARY)
		{
			NIB_PRIMARY_TYPE pa = a->_.primary;
			NIB_PRIMARY_TYPE pb = b->_.primary;

			// Special rules for identical combining
			// char + char = string
			if (pa == NT_CHAR && pb == NT_CHAR) return nibtype_string;

			// Already the same
			if (pa == pb) return a;

			// Available Combining Rules:
			// bool + int = int
			// bool + float = float
			if (pa == NT_BOOLEAN && (pb == NT_NUMBER || pb == NT_FLOAT)) return b;

			// char + int = int
			// char + string = string
			if (pa == NT_CHAR && (pb == NT_NUMBER || pb == NT_STRING)) return b;

			// int + bool = int
			// int + char = int
			// int + float = float
			if (pa == NT_NUMBER && pb == NT_BOOLEAN) return a;
			if (pa == NT_NUMBER && pb == NT_CHAR) return a;
			if (pa == NT_NUMBER && pb == NT_FLOAT) return b;

			// float + bool = float
			// float + int = float
			if (pa == NT_FLOAT && (pb == NT_BOOLEAN || pb == NT_NUMBER)) return a;

			// string + char = string
			if (pa == NT_STRING && pb == NT_CHAR) return a;
		}
	}

	// TODO: fix the rest
	return NULL;	// Incompatible
}
