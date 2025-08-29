/*

Dummy file that will contain method functions to be referenced by the pointer table

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#include "niblang.h"

AREA_DATA plith = {
	.name = "Plith",
	.description = "The Town of Plith.",
	.flags = AREA_NEWBIE
};

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
