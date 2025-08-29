#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "niblang.h"

static LLIST *flag_created_tables = NULL;
static LLIST *stat_created_tables = NULL;

struct flag_type_lookup
{
	char *name;
	struct flag_type *table;
	bool _static;
};

extern const struct flag_type affect_flags[];
extern const struct flag_type area_flags[];


#define FTLK(n,f)	{ n, (struct flag_type *)f, true }
#define FTLKNULL	{ NULL, NULL, true }

struct flag_type_lookup flag_list[] = 
{
	FTLK("affects", affect_flags),
	FTLK("area", area_flags),
	FTLKNULL
};

struct flag_type_lookup stat_list[] = 
{
	FTLKNULL
};


const struct flag_type *lookup_flag_table(const char *name)
{
	for(int i = 0; flag_list[i].name; i++)
	{
		if (!str_cmp(name, flag_list[i].name))
			return flag_list[i].table;
	}

	ITERATOR it;
	struct flag_type_lookup *lookup;

	iterator_start(&it, flag_created_tables);
	while((lookup = (struct flag_type_lookup *)iterator_nextdata(&it)))
	{
		if(lookup->name && !str_cmp(name, lookup->name))
			break;
	}
	iterator_stop(&it);

	return lookup ? lookup->table : NULL;
}


const char *get_flag_table_name(const struct flag_type *table)
{
	for(int i = 0; flag_list[i].name; i++)
	{
		if (flag_list[i].table == table)
			return flag_list[i].name;
	}

	ITERATOR it;
	struct flag_type_lookup *lookup;

	iterator_start(&it, flag_created_tables);
	while((lookup = (struct flag_type_lookup *)iterator_nextdata(&it)))
	{
		if(lookup->name && lookup->table == table)
			break;
	}
	iterator_stop(&it);

	return lookup ? lookup->name : "NULL";
}

const struct flag_type *lookup_stat_table(const char *name)
{
	for(int i = 0; stat_list[i].name; i++)
	{
		if (!str_cmp(name, stat_list[i].name))
			return stat_list[i].table;
	}

	ITERATOR it;
	struct flag_type_lookup *lookup;

	iterator_start(&it, stat_created_tables);
	while((lookup = (struct flag_type_lookup *)iterator_nextdata(&it)))
	{
		if(lookup->name && !str_cmp(name, lookup->name))
			break;
	}
	iterator_stop(&it);

	return lookup ? lookup->table : NULL;
}

const char *get_stat_table_name(const struct flag_type *table)
{
	for(int i = 0; stat_list[i].name; i++)
	{
		if (stat_list[i].table == table)
			return stat_list[i].name;
	}

	ITERATOR it;
	struct flag_type_lookup *lookup;

	iterator_start(&it, stat_created_tables);
	while((lookup = (struct flag_type_lookup *)iterator_nextdata(&it)))
	{
		if(lookup->name && lookup->table == table)
			break;
	}
	iterator_stop(&it);

	return lookup ? lookup->name : "NULL";
}

bool find_flag_value(const struct flag_type *table, const char *name, bool settable, flag_value_t *output)
{
	for(int i = 0; table[i].name; i++)
	{
		if (!str_cmp(name, table[i].name) &&
			(!settable || table[i].settable))
		{
			if (output) *output = (flag_value_t)table[i].bit;
			return true;
		}
	}

	return false;
}

void flag_free_table(struct flag_type_lookup *lookup)
{
	if (lookup && !lookup->_static)
	{
		if (lookup->name) nib_free(lookup->name);
		if (lookup->table)
		{
			for(int i = 0; lookup->table[i].name; i++)
			{
				nib_free(lookup->table[i].name);
			}

			nib_free(lookup->table);
		}

		nib_free(lookup);
	}
}

// Only name and bit will be set
struct flag_type_lookup *flag_new_table(LLIST *names, char *table_name)
{
	// ASSUME names length is valid
	// ASSUME table_name is valid;

	struct flag_type_lookup *lookup = nib_calloc(1, sizeof(struct flag_type_lookup));
	if(lookup)
	{
		lookup->_static = false;
		lookup->name = strdup(table_name);

		// the +1 will fit the {NULL, ... } entry at the end
		struct flag_type *table = nib_calloc(list_size(names) + 1, sizeof(struct flag_type));
		if (!table)
		{
			flag_free_table(lookup);
			return NULL;
		}

		ITERATOR it;
		int index = 0;
		long bit = 1;
		char *name;

		iterator_start(&it, names);
		while((name = (char *)iterator_nextdata(&it)))
		{
			table[index].name = strdup(name);
			table[index].bit = bit;
			table[index].settable = true;

			++index;
			bit <<= 1;
		}
		iterator_stop(&it);

		table[index].name = NULL;

		lookup->table = table;
	}

	return lookup;
}


bool flag_add_table(LLIST *names, char *table_name)
{
	// ASSUME names length is valid
	struct flag_type_lookup *lookup = flag_new_table(names, table_name);
	if (!lookup) return false;

	list_appendlink(flag_created_tables, lookup);
}


struct flag_type_lookup *stat_new_table(LLIST *names, char *table_name)
{
	// ASSUME table_name is valid;

	struct flag_type_lookup *lookup = nib_calloc(1, sizeof(struct flag_type_lookup));
	if(lookup)
	{
		lookup->_static = false;
		lookup->name = strdup(table_name);

		// the +1 will fit the {NULL, ... } entry at the end
		struct flag_type *table = nib_calloc(list_size(names) + 1, sizeof(struct flag_type));
		if (!table)
		{
			flag_free_table(lookup);
			return NULL;
		}

		ITERATOR it;
		int index = 0;
		long bit = 1;
		char *name;

		iterator_start(&it, names);
		while((name = (char *)iterator_nextdata(&it)))
		{
			table[index].name = strdup(name);
			table[index].bit = bit;
			table[index].settable = true;

			++index;
			++bit;
		}
		iterator_stop(&it);

		table[index].name = NULL;

		lookup->table = table;
	}

	return lookup;
}

bool stat_add_table(LLIST *names, char *table_name)
{
	// ASSUME names length is valid
	struct flag_type_lookup *lookup = stat_new_table(names, table_name);
	if (!lookup) return false;

	list_appendlink(stat_created_tables, lookup);
}



static void __flag_free_table(void *data)
{
	flag_free_table((struct flag_type_lookup *)data);
}

bool flag_tables_init()
{
	flag_created_tables = list_createx(false, NULL, __flag_free_table);
	if (!list_isvalid(flag_created_tables)) return false;

	stat_created_tables = list_createx(false, NULL, __flag_free_table);
	if (!list_isvalid(stat_created_tables)) return false;

	return true;
}

void flag_tables_cleanup()
{
	list_destroy(flag_created_tables);
	list_destroy(stat_created_tables);
}