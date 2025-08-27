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

#define AFF_BLIND (A)
#define AFF_INVISIBLE (B)
#define AFF_DETECT_EVIL (C)
#define AFF_DETECT_INVIS (D)
#define AFF_DETECT_MAGIC (E)
#define AFF_DETECT_HIDDEN (F)
#define AFF_DETECT_GOOD (G)
#define AFF_SANCTUARY (H)
#define AFF_FAERIE_FIRE (I)
#define AFF_INFRARED (J)
#define AFF_CURSE (K)
#define AFF_DEATH_GRIP (L)
#define AFF_POISON (M)
/*				(N) */
/*				(O) */
#define AFF_SNEAK (P)
#define AFF_HIDE (Q)
#define AFF_SLEEP (R)
#define AFF_CHARM (S)
#define AFF_FLYING (T)
#define AFF_PASS_DOOR (U)
#define AFF_HASTE (V)
#define AFF_CALM (W)
#define AFF_PLAGUE (X)
#define AFF_WEAKEN (Y)
#define AFF_FRENZY (Z)
#define AFF_BERSERK (aa)
#define AFF_SWIM (bb)
#define AFF_REGENERATION (cc)
#define AFF_SLOW (dd)
#define AFF_WEB (ee)

const struct flag_type affect_flags[] =
{
	{	"blind",			AFF_BLIND,			true	},
	{	"invisible",		AFF_INVISIBLE,		true	},
	{	"detect_evil",		AFF_DETECT_EVIL,	true	},
	{	"detect_invis",		AFF_DETECT_INVIS,	true	},
	{	"detect_magic",		AFF_DETECT_MAGIC,	true	},
	{	"detect_hidden",	AFF_DETECT_HIDDEN,	true	},
	{	"detect_good",		AFF_DETECT_GOOD,	true	},
	{	"sanctuary",		AFF_SANCTUARY,		true	},
	{	"faerie_fire",		AFF_FAERIE_FIRE,	true	},
	{	"infrared",			AFF_INFRARED,		true	},
	{	"curse",			AFF_CURSE,			true	},
	{   "death_grip",		AFF_DEATH_GRIP,		true    },
	{	"poison",			AFF_POISON,			true	},
	{	"sneak",			AFF_SNEAK,			true	},
	{	"hide",				AFF_HIDE,			true	},
	{	"sleep",			AFF_SLEEP,			true	},
	{	"charm",			AFF_CHARM,			true	},
	{	"flying",			AFF_FLYING,			true	},
	{	"pass_door",		AFF_PASS_DOOR,		true	},
	{	"haste",			AFF_HASTE,			true	},
	{	"calm",				AFF_CALM,			true	},
	{	"plague",			AFF_PLAGUE,			true	},
	{	"weaken",			AFF_WEAKEN,			true	},
	{	"frenzy",			AFF_FRENZY,			true	},
	{	"berserk",			AFF_BERSERK,		true	},
	{	"swim",				AFF_SWIM,			true	},
	{	"regeneration",		AFF_REGENERATION,	true	},
	{	"slow",				AFF_SLOW,			true	},
	{   "web",				AFF_WEB,			true	},
	{	NULL,				0,					false	}
};

#define AREA_NONE 0
#define AREA_CHANGED (A)
#define AREA_ADDED (B)
#define AREA_LOADING (C)
#define AREA_DARK (D)
#define AREA_NOMAP (E)
#define AREA_TESTPORT (F)
#define AREA_NO_RECALL (G)
#define AREA_NO_ROOMS (H)
#define AREA_NEWBIE (I)
#define AREA_NO_GET_RANDOM (J)
#define AREA_NO_FADING (K)
#define AREA_BLUEPRINT (L) // Area is used to hold rooms used for Blueprints.  Will block VLINKs
#define AREA_LOCKED (M)    // Area requires the player to unlock the area first
#define AREA_LOW_LEVEL (N) // Area is considered low level
#define AREA_IMMORTAL (O)  // Area is an immortal zone.
#define AREA_KEEP_LIVE (X) // Area's live data will not be overwritten when the area resets
#define AREA_PERSIST (Y)   // Area's live data will save to persist data
#define AREA_NO_SAVE (Z)
#define AREA_SOCIAL (aa)  // Area is meant for socializing.
#define AREA_HOUSING (bb) // Area is meant for housing.

const struct flag_type area_flags[] =
{
    {	"none",			AREA_NONE,		false	},
    {	"changed",		AREA_CHANGED,		true	},
    {	"added",		AREA_ADDED,		true    },
    {	"loading",		AREA_LOADING,		false	},
    {   "no_map",		AREA_NOMAP,		true    },
    {   "dark",			AREA_DARK,		true    },
    {	"testport",		AREA_TESTPORT,		true	},
    {	"no_recall",	AREA_NO_RECALL,		true	},
    {	"no_rooms",		AREA_NO_ROOMS,		true	},
    {	"newbie",		AREA_NEWBIE,		true	},
    {	"no_get_random",AREA_NO_GET_RANDOM,	true	},
    {	"no_fading",	AREA_NO_FADING,		true	},
    {	"blueprint",	AREA_BLUEPRINT,		true	},
    {	"locked",		AREA_LOCKED,		true	},
    {   "low_level",    AREA_LOW_LEVEL,     true    },
    {   "immortal",     AREA_IMMORTAL,      true    },
    {   "persist",      AREA_PERSIST,       true    },
    {   "keep_live",    AREA_KEEP_LIVE,     true    },
    {   "social",       AREA_SOCIAL,        true    },
    {   "housing",      AREA_HOUSING,       true    },
    {   "immortal",     AREA_IMMORTAL,      true    },
    {	NULL,			0,			0	}
};


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
		if (lookup->name) free(lookup->name);
		if (lookup->table)
		{
			for(int i = 0; lookup->table[i].name; i++)
			{
				free(lookup->table[i].name);
			}

			free(lookup->table);
		}

		free(lookup);
	}
}

// Only name and bit will be set
struct flag_type_lookup *flag_new_table(LLIST *names, char *table_name)
{
	// ASSUME names length is valid
	// ASSUME table_name is valid;

	struct flag_type_lookup *lookup = calloc(1, sizeof(struct flag_type_lookup));
	if(lookup)
	{
		lookup->_static = false;
		lookup->name = strdup(table_name);

		// the +1 will fit the {NULL, ... } entry at the end
		struct flag_type *table = calloc(list_size(names) + 1, sizeof(struct flag_type));
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

	struct flag_type_lookup *lookup = calloc(1, sizeof(struct flag_type_lookup));
	if(lookup)
	{
		lookup->_static = false;
		lookup->name = strdup(table_name);

		// the +1 will fit the {NULL, ... } entry at the end
		struct flag_type *table = calloc(list_size(names) + 1, sizeof(struct flag_type));
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