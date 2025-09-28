#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "../merc.h"
#include "niblang.h"
#include "interpret.h"

extern LLIST *nib_flag_created_tables;
extern LLIST *nib_stat_created_tables;
extern LLIST *nib_used_tables;
extern LLIST *nib_used_banks;

struct flag_type_lookup *flag_list_head = NULL;
struct flag_type_lookup *flag_list_tail = NULL;

struct flagbank_type_lookup *flagbank_list_head = NULL;
struct flagbank_type_lookup *flagbank_list_tail = NULL;

struct flag_type_lookup *stat_list_head = NULL;
struct flag_type_lookup *stat_list_tail = NULL;

static const struct flag_type_lookup *get_internal_flag_table(const char *name)
{
	for(struct flag_type_lookup *cur = flag_list_head; cur; cur = cur->next)
	{
		if (!str_cmp(name, cur->name))
			return cur;
	}

	return NULL;
}

static const struct flagbank_type_lookup *get_internal_flagbank(const char *name)
{
	for(struct flagbank_type_lookup *cur = flagbank_list_head; cur; cur = cur->next)
	{
		if (!str_cmp(name, cur->name))
			return cur;
	}

	return NULL;
}

bool nib_register_flag_table(const char *name, const struct flag_type *table)
{
	if (get_internal_flag_table(name))
		return false;
	
	struct flag_type_lookup *lookup = nib_malloc(sizeof(struct flag_type_lookup));
	if (!lookup) return false;

	if (flag_list_head)
		flag_list_tail->next = lookup;
	else
		flag_list_head = lookup;
	flag_list_tail = lookup;
	
	lookup->next = NULL;
	lookup->name = nib_strdup(name);
	lookup->table = (struct flag_type *)table;
	lookup->internal = true;	// Table is internal, just free the name

	return true;
}


bool nib_register_flag_bank(const char *name, const struct flag_type **bank)
{
	if (get_internal_flagbank(name))
		return false;
	
	struct flagbank_type_lookup *lookup = nib_malloc(sizeof(struct flagbank_type_lookup));
	if (!lookup) return false;

	if (flagbank_list_head)
		flagbank_list_tail->next = lookup;
	else
		flagbank_list_head = lookup;
	flagbank_list_tail = lookup;
	
	lookup->next = NULL;
	lookup->name = nib_strdup(name);
	lookup->bank = bank;
	for(lookup->banks = 0; bank[lookup->banks]; lookup->banks++);
	return true;
}

static const struct flag_type_lookup *get_internal_stat_table(const char *name)
{
	for(struct flag_type_lookup *cur = stat_list_head; cur; cur = cur->next)
	{
		if (!str_cmp(name, cur->name))
			return cur;
	}

	return NULL;
}

bool nib_register_stat_table(const char *name, const struct flag_type *table)
{
	if (get_internal_stat_table(name))
		return false;
	
	struct flag_type_lookup *lookup = nib_malloc(sizeof(struct flag_type_lookup));
	if (!lookup) return false;

	if (stat_list_head)
		stat_list_tail->next = lookup;
	else
		stat_list_head = lookup;
	stat_list_tail = lookup;
	
	lookup->next = NULL;
	lookup->name = nib_strdup(name);
	lookup->table = (struct flag_type *)table;
	lookup->internal = true;	// Table is internal, just free the name

	return true;
}

const struct flag_type *nib_lookup_flag_table(LLIST *created, const char *name)
{
	for(struct flag_type_lookup *cur = flag_list_head; cur; cur = cur->next)
	{
		if (!str_cmp(name, cur->name))
			return cur->table;
	}

	ITERATOR it;
	struct flag_type_lookup *lookup;

	iterator_start(&it, created);
	while((lookup = (struct flag_type_lookup *)iterator_nextdata(&it)))
	{
		if(lookup->name && !str_cmp(name, lookup->name))
			break;
	}
	iterator_stop(&it);

	return lookup ? lookup->table : NULL;
}


const struct flag_type **nib_lookup_flag_bank(const char *name)
{
	for(struct flagbank_type_lookup *cur = flagbank_list_head; cur; cur = cur->next)
	{
		if (!str_cmp(name, cur->name))
			return cur->bank;
	}

	return NULL;
}

const char *nib_get_flag_table_name(LLIST *created, const struct flag_type *table)
{
	for(struct flag_type_lookup *cur = flag_list_head; cur; cur = cur->next)
	{
		if (cur->table == table)
			return cur->name;
	}

	ITERATOR it;
	struct flag_type_lookup *lookup;

	iterator_start(&it, created);
	while((lookup = (struct flag_type_lookup *)iterator_nextdata(&it)))
	{
		if(lookup->name && lookup->table == table)
			break;
	}
	iterator_stop(&it);

	return lookup ? lookup->name : "NULL";
}

// All banks are internal only
const char *nib_get_flag_bank_name(const struct flag_type **bank)
{
	for(struct flagbank_type_lookup *cur = flagbank_list_head; cur; cur = cur->next)
	{
		if (cur->bank == bank)
			return cur->name;
	}

	return "NULL";
}

const struct flag_type *nib_lookup_stat_table(LLIST *created, const char *name)
{
	for(struct flag_type_lookup *cur = stat_list_head; cur; cur = cur->next)
	{
		if (!str_cmp(name, cur->name))
			return cur->table;
	}

	ITERATOR it;
	struct flag_type_lookup *lookup;

	iterator_start(&it, created);
	while((lookup = (struct flag_type_lookup *)iterator_nextdata(&it)))
	{
		if(lookup->name && !str_cmp(name, lookup->name))
			break;
	}
	iterator_stop(&it);

	return lookup ? lookup->table : NULL;
}

const char *nib_get_stat_table_name(LLIST *created, const struct flag_type *table)
{
	for(struct flag_type_lookup *cur = stat_list_head; cur; cur = cur->next)
	{
		if (cur->table == table)
			return cur->name;
	}

	ITERATOR it;
	struct flag_type_lookup *lookup;

	iterator_start(&it, created);
	while((lookup = (struct flag_type_lookup *)iterator_nextdata(&it)))
	{
		if(lookup->name && lookup->table == table)
			break;
	}
	iterator_stop(&it);

	return lookup ? lookup->name : "NULL";
}

bool nib_find_flag_value(const struct flag_type *table, const char *name, bool *settable, flag_value_t *output)
{
	for(int i = 0; table[i].name; i++)
	{
		if (!str_cmp(name, table[i].name))
		{
			if (settable) *settable = table[i].settable;
			if (output) *output = (flag_value_t)table[i].bit;
			return true;
		}
	}

	return false;
}


bool nib_find_flagbank_value(const struct flag_type **banks, const char *name, bool *settable, int *bank, flag_value_t *output)
{
	for(int b = 0; banks[b]; b++)
	{
		const struct flag_type *table = banks[b];
		for(int i = 0; table[i].name; i++)
		{
			if (!str_cmp(name, table[i].name))
			{
				if (settable) *settable = table[i].settable;
				if (bank) *bank = b;
				if (output) *output = (flag_value_t)table[i].bit;
				return true;
			}
		}
	}

	return false;
}

const char *nib_get_flag_string(const struct flag_type *table, long bits)
{
	static char buf[4][512];
	static int cnt = 0;
	int  flag;

	if (!table) return "none";

	if ( ++cnt > 3 )
		cnt = 0;

	buf[cnt][0] = '\0';
	for (flag = 0; table[flag].name != NULL; flag++)
	{
		if ( (bits & table[flag].bit) )
		{
			strcat( buf[cnt], " " );
			strcat( buf[cnt], table[flag].name );
		}
	}
	return (buf[cnt][0] != '\0') ? buf[cnt]+1 : "none";
}

const char *nib_get_flagbank_string(const struct flag_type **banks, long *bits)
{
	static char buf[4][512*10];
	static int cnt = 0;

	if (!banks) return "none";

	if ( ++cnt > 3 )
		cnt = 0;

	buf[cnt][0] = '\0';
	for(int b = 0; banks[b]; b++)
	{
		const struct flag_type *table = banks[b];
		for (int flag = 0; table[flag].name != NULL; flag++)
		{
			if ( (bits[b] & table[flag].bit) )
			{
				strcat( buf[cnt], " " );
				strcat( buf[cnt], table[flag].name );
			}
		}
	}
	return (buf[cnt][0] != '\0') ? buf[cnt]+1 : "none";
}

const char *nib_get_stat_string(const struct flag_type *table, long bits)
{
	// fprintf(stderr, "nib_get_stat_string(%p, %X)\n", table, bits);
	if (table)
	{
		for (int flag = 0; table[flag].name != NULL; flag++)
		{
			if ( table[flag].bit == bits )
				return table[flag].name;
		}
	}
	return "none";
}

struct flag_type_lookup *nib_flag_copy_table(struct flag_type_lookup *src)
{
	if (!src) return NULL;

	struct flag_type_lookup *lookup = nib_calloc(1,sizeof(struct flag_type_lookup));
	if (lookup)
	{
		lookup->internal = src->internal;
		lookup->name = nib_strdup(src->name);

		if (src->internal)
			lookup->table = src->table;
		else if (src->table)
		{
			int count = 0;
			for(; src->table[count].name; count++);

			lookup->table = nib_calloc(count + 1,sizeof(struct flag_type));
			if (!lookup->table)
			{
				nib_free(lookup->name);
				nib_free(lookup);
				return NULL;
			}

			for(int i = 0; i < count; i++)
			{
				lookup->table[i].name = strdup(src->table[i].name);
				lookup->table[i].bit = src->table[i].bit;
				lookup->table[i].settable = src->table[i].settable;
			}
			
		}
	}

	return lookup;
}

void nib_flag_free_table(struct flag_type_lookup *lookup)
{
	if (lookup)
	{
		if (lookup->name) nib_free(lookup->name);
		if (!lookup->internal && lookup->table)
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

void nib_flag_free_bank(struct flagbank_type_lookup *lookup)
{
	if (lookup)
	{
		if (lookup->name) nib_free(lookup->name);

		nib_free(lookup);
	}
}

// Only name and bit will be set
struct flag_type_lookup *nib_flag_new_table(LLIST *names, char *table_name)
{
	// ASSUME names length is valid
	// ASSUME table_name is valid;

	struct flag_type_lookup *lookup = nib_calloc(1, sizeof(struct flag_type_lookup));
	if(lookup)
	{
		lookup->internal = false;
		lookup->name = strdup(table_name);

		// the +1 will fit the {NULL, ... } entry at the end
		struct flag_type *table = nib_calloc(list_size(names) + 1, sizeof(struct flag_type));
		if (!table)
		{
			nib_flag_free_table(lookup);
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


bool nib_flag_add_table(LLIST *names, char *table_name)
{
	// ASSUME names length is valid
	struct flag_type_lookup *lookup = nib_flag_new_table(names, table_name);
	if (!lookup) return false;

	list_appendlink(nib_flag_created_tables, lookup);
}


struct flag_type_lookup *nib_stat_new_table(LLIST *names, char *table_name)
{
	// ASSUME table_name is valid;

	struct flag_type_lookup *lookup = nib_calloc(1, sizeof(struct flag_type_lookup));
	if(lookup)
	{
		lookup->internal = false;
		lookup->name = strdup(table_name);

		// the +1 will fit the {NULL, ... } entry at the end
		struct flag_type *table = nib_calloc(list_size(names) + 1, sizeof(struct flag_type));
		if (!table)
		{
			nib_flag_free_table(lookup);
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

bool nib_stat_add_table(LLIST *names, char *table_name)
{
	// ASSUME names length is valid
	struct flag_type_lookup *lookup = nib_stat_new_table(names, table_name);
	if (!lookup) return false;

	list_appendlink(nib_stat_created_tables, lookup);
}

int nib_add_used_table(const struct flag_type *table)
{
	int index = list_getindex(nib_used_tables, (void*)table);
	if (index > 0) return index;

	list_appendlink(nib_used_tables,(void *)table);
	return list_size(nib_used_tables);
}

int nib_add_used_bank(const struct flag_type **bank)
{
	int index = list_getindex(nib_used_banks, (void*)bank);
	if (index > 0) return index;

	list_appendlink(nib_used_banks,(void *)bank);
	return list_size(nib_used_banks);
}

void nib_flag_tables_cleanup()
{
	for(struct flag_type_lookup *cur = flag_list_head; cur;)
	{
		struct flag_type_lookup *next = cur->next;
		nib_flag_free_table(cur);
		cur = next;
	}

	flag_list_head = NULL;
	flag_list_tail = NULL;

	for(struct flagbank_type_lookup *cur = flagbank_list_head; cur;)
	{
		struct flagbank_type_lookup *next = cur->next;
		nib_flag_free_bank(cur);
		cur = next;
	}

	flagbank_list_head = NULL;
	flagbank_list_tail = NULL;

	for(struct flag_type_lookup *cur = stat_list_head; cur;)
	{
		struct flag_type_lookup *next = cur->next;
		nib_flag_free_table(cur);
		cur = next;
	}

	stat_list_head = NULL;
	stat_list_tail = NULL;
}
