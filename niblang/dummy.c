/*

Dummy file that will contain method functions to be referenced by the pointer table

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#include "niblang.h"
#include "script.h"
#include "interpret.h"

static WNUM __wnum_zero;

AREA_DATA plith;

MOB_INDEX_DATA steiner_index;
CHAR_DATA steiner;

MOB_INDEX_DATA ravage_index;
CHAR_DATA ravage;

MOB_INDEX_DATA mayor_index;
CHAR_DATA mayor;

ROOM_INDEX_DATA beginning;

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

/* 
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
*/

void dummy_init()
{
	steiner_index.area = &plith;
	steiner_index.vnum = 1L;
	steiner.pIndexData = &steiner_index;
	steiner.name = strdup("steiner");
	steiner.short_descr = strdup("Steiner");
	steiner.long_descr = strdup("Steiner watches over the Town of Plith.");
	steiner.description = strdup("Steiner watches over the Town of Plith.");

	ravage_index.area = &plith;
	ravage_index.vnum = 2L;
	ravage.pIndexData = &ravage_index;
	ravage.name = strdup("ravage");
	ravage.short_descr = strdup("Ravage");
	ravage.long_descr = strdup("The sinister Ravage looms over the city.");
	ravage.description = strdup("The sinister Ravage looms over the city.");

	mayor_index.area = &plith;
	mayor_index.vnum = 3L;
	mayor.pIndexData = &mayor_index;
	mayor.name = strdup("mayor plith");
	mayor.short_descr = strdup("the Mayor of Plith");
	mayor.long_descr = strdup("The Mayor governs Plith with a firm hand.");
	mayor.description = strdup("The Mayor governs Plith with a firm hand.");

	beginning.area = &plith;
	beginning.vnum = 1L;
	beginning.name = strdup("The Beginning");
	beginning.description = strdup("The heart of the Town of Plith.");
	beginning.people = list_create(false);
	list_appendlink(beginning.people, &steiner);
	list_appendlink(beginning.people, &ravage);
	list_appendlink(beginning.people, &mayor);

	plith.uid = 1;
	plith.name = strdup("Plith");
	plith.description = strdup("The Town of Plith.");
	plith.flags = AREA_NEWBIE;
	plith.rooms = list_create(false);
	list_appendlink(plith.rooms, &beginning);

	nib_register_flag_table("affect", affect_flags);
	nib_register_flag_table("area", area_flags);
}

void dummy_cleanup()
{
	if (steiner.name) free(steiner.name);
	if (steiner.short_descr) free(steiner.short_descr);
	if (steiner.long_descr) free(steiner.long_descr);
	if (steiner.description) free(steiner.description);

	if (ravage.name) free(ravage.name);
	if (ravage.short_descr) free(ravage.short_descr);
	if (ravage.long_descr) free(ravage.long_descr);
	if (ravage.description) free(ravage.description);

	if (mayor.name) free(mayor.name);
	if (mayor.short_descr) free(mayor.short_descr);
	if (mayor.long_descr) free(mayor.long_descr);
	if (mayor.description) free(mayor.description);

	if (beginning.name) free(beginning.name);
	if (beginning.description) free(beginning.description);
	list_destroy(beginning.people);

	if (plith.name) free(plith.name);
	if (plith.description) free(plith.description);
	list_destroy(plith.rooms);



}

AREA_DATA *find_area(char *name)
{
	if (!name) return NULL;

	if (!str_cmp(plith.name, name))
		return &plith;

	return NULL;
}

AREA_DATA *get_area_from_uid (long uid)
{
	if (plith.uid == uid) return &plith;
	return NULL;
}


/////////////////////////////////////
// Functions
DECL_METHOD_FUNC(function_print_msg)
{
	// Print the message
	if (argv[0].type == NST_STRING)
		printf("%s\n", argv[0]._.str ? argv[0]._.str : "<null>");
	return SCPERR_SUCCESS;
}

DECL_METHOD_FUNC(function_reckoning)
{
	return SCPERR_SUCCESS;
}

#define ARG_NUM(n)		(argv[(n)]._.i)
#define ARG_STR(n)		(argv[(n)]._.str)
#define ARG_LST(n)		(argv[(n)]._.list.list)
#define ARG_MOB(n)		(argv[(n)]._.mobile)

#define SET_NUM(n)		(output->type = NST_NUMBER, output->_.i = (n))
#define SET_WNUM(w)		(output->type = NST_WIDEVNUM, output->_.wnum = (w))

// NUMBER methods
DECL_METHOD_FUNC(number_random_value)
{
	// Get a number from 0 to N-1
	long value = 0;

	fprintf(stderr,"random(%ld)\n", ARG_NUM(0));

	if (ARG_NUM(0) > 1)
		value = number_range(0,ARG_NUM(0) - 1);

	fprintf(stderr,"value = %ld\n", value);
	fflush(stderr);

	SET_NUM(value);
	return SCPERR_SUCCESS;
}

// FLOAT methods

// BOOLEAN methods

// STRING methods
DECL_METHOD_FUNC(string_length)
{
	// Get length of string
	long len;
	if (ARG_STR(0))
		len = strlen(ARG_STR(0));
	else
		len = 0;

	SET_NUM(len);
	return SCPERR_SUCCESS;
}

// FLAG methods

// LIST methods
DECL_METHOD_FUNC(list_add)
{
	return SCPERR_SUCCESS;
}

DECL_METHOD_FUNC(list_insert)
{
	return SCPERR_SUCCESS;
}

DECL_METHOD_FUNC(list_remove)
{
	return SCPERR_SUCCESS;
}

DECL_METHOD_FUNC(list_size)
{
	long size = list_size(ARG_LST(0));

	SET_NUM(size);

	return SCPERR_SUCCESS;
}

// STAT methods

// MAP methods

// WIDEVNUM methods

// AREA Methods

// DUNGEON methods

// INSTANCE methods

// MOBILE methods
DECL_METHOD_FUNC(mobile_get_widevnum)
{
	CHAR_DATA *mob = ARG_MOB(0);

	WNUM wnum;
	if (mob && mob->pIndexData)
	{
		wnum.pArea = mob->pIndexData->area;
		wnum.vnum = mob->pIndexData->vnum;
	}
	else
	{
		wnum.pArea = NULL;
		wnum.vnum = 0;
	}

	SET_WNUM(wnum);
	return SCPERR_SUCCESS;
}

// OBJECT methods

// QUEST methods

// ROOM methods

// SHIP methods

// TOKEN methods






//////////////////////////

// SENTIENCE variables
pVARIABLE variable_new(const char *name)
{
	pVARIABLE var = calloc(1, sizeof(VARIABLE));

	if (var)
	{
		var->name = strdup(name);
	}

	return var;
}

void variable_free(pVARIABLE var)
{
	switch(var->type)
	{
		case VAR_STRING:
			free(var->_.str);
			break;
	}

	if (var->name) free(var->name);
	free(var);
}

static void __free_variable(void *data)
{
	variable_free((pVARIABLE)data);
}

LLIST *nib_create_var_list()
{
	return list_createx(false, NULL, __free_variable);
}

static LLIST *nib_variables = NULL;
bool variable_init()
{
	nib_variables = nib_create_var_list();
	if (!list_isvalid(nib_variables)) return false;

	pVARIABLE var;

	var = variable_new_area("plith", &plith);
	var->readonly = true;
	list_appendlink(nib_variables, var);

	var = variable_new_number("vnum", 2);
	list_appendlink(nib_variables, var);

	var = variable_new_float("bar", -INFINITY);
	var->readonly = true;
	list_appendlink(nib_variables, var);

	var = variable_new_widevnum("wnum", &plith, 100L);
	var->readonly = true;
	list_appendlink(nib_variables, var);

	return true;
}

void variable_cleanup()
{
	list_destroy(nib_variables);
}

bool is_valid_variable_type(pVARIABLE var, NIB_TYPE *type)
{
	switch(var->type)
	{
		case VAR_BOOLEAN:
			return (type != NULL && type->type_class == NTC_PRIMARY && type->_.primary == NT_BOOLEAN);

		case VAR_NUMBER:
			return (type != NULL && type->type_class == NTC_PRIMARY && type->_.primary == NT_NUMBER);

		case VAR_FLOAT:
			return (type != NULL && type->type_class == NTC_PRIMARY && type->_.primary == NT_FLOAT);

		case VAR_STRING:
		case VAR_STRING_S:
			return (type != NULL && type->type_class == NTC_PRIMARY && type->_.primary == NT_STRING);

		case VAR_WIDEVNUM:
			return (type != NULL && type->type_class == NTC_PRIMARY && type->_.primary == NT_WIDEVNUM);

		case VAR_AREA:
			return (type != NULL && type->type_class == NTC_PRIMARY && type->_.primary == NT_AREA);
	}

	return false;
}

pVARIABLE variable_get(const char *name)
{
	ITERATOR it;
	pVARIABLE var;

	iterator_start(&it, nib_variables);
	while((var = (pVARIABLE)iterator_nextdata(&it)))
	{
		if (!str_cmp(var->name, name))
			break;
	}
	iterator_stop(&it);

	return var;
}

pVARIABLE variable_new_number(const char *name, long value)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_NUMBER;
		var->_.num = value;
	}

	return var;
}

pVARIABLE variable_new_float(const char *name, double value)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_FLOAT;
		var->_.flt = value;
	}

	return var;
}

pVARIABLE variable_new_bool(const char *name, bool value)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_BOOLEAN;
		var->_.b = value;
	}

	return var;
}

pVARIABLE variable_new_string(const char *name, char *str)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_STRING;
		var->_.str = strdup(str);
	}

	return var;
}

pVARIABLE variable_new_shared_string(const char *name, char *str)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_STRING_S;
		var->_.str = str;
	}

	return var;
}

pVARIABLE variable_new_area(const char *name, AREA_DATA *area)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_AREA;
		var->_.area = area;
	}

	return var;
}

pVARIABLE variable_new_mobile(const char *name, CHAR_DATA *mobile)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_MOBILE;
		var->_.mobile = mobile;
	}

	return var;
}

pVARIABLE variable_new_room(const char *name, ROOM_INDEX_DATA *room)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_ROOM;
		var->_.room = room;
	}

	return var;
}

pVARIABLE variable_new_widevnum(const char *name, AREA_DATA *area, long vnum)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_WIDEVNUM;
		var->_.wnum.pArea = area;
		var->_.wnum.vnum = vnum;
	}

	return var;
}


