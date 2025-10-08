#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>


#include "../merc.h"
#include "niblang.h"
#include "interpret.h"

extern LLIST *nib_flag_created_tables;
extern LLIST *nib_stat_created_tables;

static NIB_TYPE __nibtype_null			= { true, false, NTC_ANY, {0}, "null"};
static NIB_TYPE __nibtype_void			= { true, false, NTC_VOID, {0}, "void"};
static NIB_TYPE __nibtype_any			= { true, false, NTC_ANY, {0}, "any"};
static NIB_TYPE __nibtype_bool			= { true, false, NTC_PRIMARY, {NT_BOOLEAN}, "boolean"};
static NIB_TYPE __nibtype_char			= { true, false, NTC_PRIMARY, {NT_CHAR}, "char"};
static NIB_TYPE __nibtype_int16			= { true, false, NTC_PRIMARY, {NT_NUMBER16}, "int32"};
static NIB_TYPE __nibtype_int32			= { true, false, NTC_PRIMARY, {NT_NUMBER32}, "int32"};
static NIB_TYPE __nibtype_int			= { true, false, NTC_PRIMARY, {NT_NUMBER}, "int"};
static NIB_TYPE __nibtype_float			= { true, false, NTC_PRIMARY, {NT_FLOAT}, "float"};
static NIB_TYPE __nibtype_string		= { true, false, NTC_PRIMARY, {NT_STRING}, "string"};
static NIB_TYPE __nibtype_map			= { true, false, NTC_PRIMARY, {NT_MAP}, "map"};
static NIB_TYPE __nibtype_flag			= { true, false, NTC_FLAG, {.flag = {0, NULL, NULL}}, "flag"};
static NIB_TYPE __nibtype_list			= { true, false, NTC_LIST, {.list = {NULL, false}}, "list"};
static NIB_TYPE __nibtype_array			= { true, false, NTC_ARRAY, {.array = {NULL, 0, false}}, "array"};
static NIB_TYPE __nibtype_stat			= { true, false, NTC_STAT, {.stat = {NULL, NULL}}, "stat"};
static NIB_TYPE __nibtype_varargs		= { true, false, NTC_VARARGS, {0}, "..."};
static NIB_TYPE __nibtype_widevnum		= { true, false, NTC_PRIMARY, {NT_WIDEVNUM}, "widevnum"};
static NIB_TYPE __nibtype_time			= { true, false, NTC_PRIMARY, {NT_TIME}, "time"};
static NIB_TYPE __nibtype_dice			= { true, false, NTC_PRIMARY, {NT_DICE}, "dice"};
static NIB_TYPE __nibtype_account		= { true, false, NTC_PRIMARY, {NT_ACCOUNT}, "account"};
static NIB_TYPE __nibtype_affect		= { true, false, NTC_PRIMARY, {NT_AFFECT}, "affect"};
static NIB_TYPE __nibtype_area			= { true, false, NTC_PRIMARY, {NT_AREA}, "area"};
static NIB_TYPE __nibtype_channel		= { true, false, NTC_PRIMARY, {NT_CHANNEL}, "channel"};
static NIB_TYPE __nibtype_class			= { true, false, NTC_PRIMARY, {NT_CLASS}, "class"};
static NIB_TYPE __nibtype_dungeon		= { true, false, NTC_PRIMARY, {NT_DUNGEON}, "dungeon"};
static NIB_TYPE __nibtype_exit			= { true, false, NTC_PRIMARY, {NT_EXIT}, "exit"};
static NIB_TYPE __nibtype_instance		= { true, false, NTC_PRIMARY, {NT_INSTANCE}, "instance"};
static NIB_TYPE __nibtype_liquid		= { true, false, NTC_PRIMARY, {NT_LIQUID}, "liquid"};
static NIB_TYPE __nibtype_mail			= { true, false, NTC_PRIMARY, {NT_MAIL}, "mail"};
static NIB_TYPE __nibtype_material		= { true, false, NTC_PRIMARY, {NT_MATERIAL}, "material"};
static NIB_TYPE __nibtype_mission		= { true, false, NTC_PRIMARY, {NT_MISSION}, "mission"};
static NIB_TYPE __nibtype_mobile		= { true, false, NTC_PRIMARY, {NT_MOBILE}, "mobile"};
static NIB_TYPE __nibtype_note			= { true, false, NTC_PRIMARY, {NT_NOTE}, "note"};
static NIB_TYPE __nibtype_object		= { true, false, NTC_PRIMARY, {NT_OBJECT}, "object"};
static NIB_TYPE __nibtype_org			= { true, false, NTC_PRIMARY, {NT_ORG}, "organization"};
static NIB_TYPE __nibtype_quest			= { true, false, NTC_PRIMARY, {NT_QUEST}, "quest"};
static NIB_TYPE __nibtype_race			= { true, false, NTC_PRIMARY, {NT_RACE}, "race"};
static NIB_TYPE __nibtype_rank			= { true, false, NTC_PRIMARY, {NT_RANK}, "rank"};
static NIB_TYPE __nibtype_reputation	= { true, false, NTC_PRIMARY, {NT_REPUTATION}, "reputation"};
static NIB_TYPE __nibtype_room			= { true, false, NTC_PRIMARY, {NT_ROOM}, "room"};
static NIB_TYPE __nibtype_sector		= { true, false, NTC_PRIMARY, {NT_SECTOR}, "sector"};
static NIB_TYPE __nibtype_ship			= { true, false, NTC_PRIMARY, {NT_SHIP}, "ship"};
static NIB_TYPE __nibtype_skill			= { true, false, NTC_PRIMARY, {NT_SKILL}, "skill"};
static NIB_TYPE __nibtype_token			= { true, false, NTC_PRIMARY, {NT_TOKEN}, "token"};
static NIB_TYPE __nibtype_wilds			= { true, false, NTC_PRIMARY, {NT_WILDS}, "wilds"};
static NIB_TYPE __nibtype_world			= { true, false, NTC_PRIMARY, {NT_WORLD}, "world"};

/* Types to add: TODO

Basic Types:
coord

Game Types:
exit
player	?
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
NIB_TYPE *nibtype_int16  = &__nibtype_int16;
NIB_TYPE *nibtype_int32  = &__nibtype_int32;
NIB_TYPE *nibtype_int  = &__nibtype_int;
NIB_TYPE *nibtype_float = &__nibtype_float;
NIB_TYPE *nibtype_string = &__nibtype_string;
NIB_TYPE *nibtype_map = &__nibtype_map;
NIB_TYPE *nibtype_flag = &__nibtype_flag;
NIB_TYPE *nibtype_list = &__nibtype_list;
NIB_TYPE *nibtype_array = &__nibtype_array;
NIB_TYPE *nibtype_stat = &__nibtype_stat;
NIB_TYPE *nibtype_varargs = &__nibtype_varargs;
NIB_TYPE *nibtype_widevnum = &__nibtype_widevnum;
NIB_TYPE *nibtype_time = &__nibtype_time;
NIB_TYPE *nibtype_dice = &__nibtype_dice;
NIB_TYPE *nibtype_account = &__nibtype_account;
NIB_TYPE *nibtype_affect = &__nibtype_affect;
NIB_TYPE *nibtype_area = &__nibtype_area;
NIB_TYPE *nibtype_channel = &__nibtype_channel;
NIB_TYPE *nibtype_class = &__nibtype_class;
NIB_TYPE *nibtype_dungeon = &__nibtype_dungeon;
NIB_TYPE *nibtype_exit = &__nibtype_exit;
NIB_TYPE *nibtype_instance = &__nibtype_instance;
NIB_TYPE *nibtype_liquid = &__nibtype_liquid;
NIB_TYPE *nibtype_mail = &__nibtype_mail;
NIB_TYPE *nibtype_material = &__nibtype_material;
NIB_TYPE *nibtype_mission = &__nibtype_mission;
NIB_TYPE *nibtype_mobile = &__nibtype_mobile;
NIB_TYPE *nibtype_note = &__nibtype_note;
NIB_TYPE *nibtype_object = &__nibtype_object;
NIB_TYPE *nibtype_org = &__nibtype_org;
NIB_TYPE *nibtype_quest = &__nibtype_quest;
NIB_TYPE *nibtype_race = &__nibtype_race;
NIB_TYPE *nibtype_rank = &__nibtype_rank;
NIB_TYPE *nibtype_reputation = &__nibtype_reputation;
NIB_TYPE *nibtype_room = &__nibtype_room;
NIB_TYPE *nibtype_sector = &__nibtype_sector;
NIB_TYPE *nibtype_ship = &__nibtype_ship;
NIB_TYPE *nibtype_skill = &__nibtype_skill;
NIB_TYPE *nibtype_token = &__nibtype_token;
NIB_TYPE *nibtype_wilds = &__nibtype_wilds;
NIB_TYPE *nibtype_world = &__nibtype_world;


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

NIB_TYPE *new_nib_type_flag_bank(const struct flag_type **bank)
{
	NIB_TYPE *type = nib_calloc(1,sizeof(NIB_TYPE));

	type->_static = false;
	type->type_class = NTC_FLAG_BANK;
	type->_.flagbank.bank = bank;
	for(type->_.flagbank.banks = 0; bank[type->_.flagbank.banks]; type->_.flagbank.banks++);
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

NIB_TYPE *new_nib_type_stat32_table(const struct flag_type *table)
{
	NIB_TYPE *type = nib_calloc(1,sizeof(NIB_TYPE));

	type->_static = false;
	type->type_class = NTC_STAT32;
	type->_.stat.names = NULL;
	type->_.stat.table = table;
	type->name = NULL;

	return type;
}

NIB_TYPE *new_nib_type_stat16_table(const struct flag_type *table)
{
	NIB_TYPE *type = nib_calloc(1,sizeof(NIB_TYPE));

	type->_static = false;
	type->type_class = NTC_STAT16;
	type->_.stat.names = NULL;
	type->_.stat.table = table;
	type->name = NULL;

	return type;
}

NIB_TYPE *new_nib_type_list(NIB_TYPE *elem, bool constant)
{
	NIB_TYPE *type = nib_calloc(1,sizeof(NIB_TYPE));

	type->_static = false;
	type->type_class = NTC_LIST;
	type->_.list.type = elem;
	type->_.list.constant = constant;
	type->name = NULL;

	// printf("new_nib_type_list:\n");
	// hex_dump(type,sizeof(NIB_TYPE));

	return type;
}

NIB_TYPE *new_nib_type_array(NIB_TYPE *elem, long length, bool constant)
{
	NIB_TYPE *type = nib_calloc(1,sizeof(NIB_TYPE));

	type->_static = false;
	type->type_class = NTC_ARRAY;
	type->_.array.type = elem;
	type->_.array.length = length;
	type->_.array.constant = constant;
	type->name = NULL;

	return type;
}

NIB_TYPE *new_nib_type_multi(LLIST *types)
{
	NIB_TYPE *type = nib_calloc(1,sizeof(NIB_TYPE));

	type->_static = false;
	type->type_class = NTC_MULTI;
	type->_.multi = list_copy(types);
	type->name = NULL;

	return type;
}

void free_nib_type(NIB_TYPE *type)
{
	// if (type && type->_reference)
	// {
	// 	fprintf(stderr, "Reference Type: %s (%p)\n", nib_get_typename(NULL, type), type);
	// }
	if (type && !type->_static)
	{
		if (type->type_class == NTC_LIST)
		{
			free_nib_type(type->_.list.type);
		}
		else if (type->type_class == NTC_FLAG)
		{
			list_destroy(type->_.flag.names);
		}
		else if (type->type_class == NTC_STAT)
		{
			list_destroy(type->_.stat.names);
		}
		else if (type->type_class == NTC_MULTI)
		{
			list_destroy(type->_.multi);
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
	dest->_reference = src->_reference;
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
			dest->_.list.constant = src->_.list.constant;
			dest->_.list.type = nib_type_copy(src->_.list.type);
			break;

		case NTC_ARRAY:
			dest->_.array.constant = src->_.array.constant;
			dest->_.array.length = src->_.array.length;
			dest->_.array.type = nib_type_copy(src->_.array.type);
			break;

		case NTC_FLAG:
			dest->_.flag.bits = src->_.flag.bits;
			dest->_.flag.names = list_copy(src->_.flag.names);
			dest->_.flag.table = src->_.flag.table;
			break;

		case NTC_FLAG_BANK:
			dest->_.flagbank.bank = src->_.flagbank.bank;
			dest->_.flagbank.banks = src->_.flagbank.banks;
			break;

		case NTC_STAT:
		case NTC_STAT32:
		case NTC_STAT16:
			dest->_.stat.names = list_copy(src->_.flag.names);
			dest->_.stat.table = src->_.stat.table;
			break;

		case NTC_MULTI:
			dest->_.multi = list_copy(src->_.multi);
			break;
	}

	return dest;
}

NIB_TYPE *nib_type_by_reference(NIB_TYPE *src, bool byref)
{
	if (!src) return NULL;

	if (!byref) return src;

	bool _static = src->_static;
	src->_static = false;
	NIB_TYPE *tp = nib_type_copy(src);
	src->_static = _static;

	if (tp)
		tp->_reference = true;
	return tp;
}

#define MTSL 10240
#define MTSN 20
char *nib_get_typename(NIB_SCRIPT *context, NIB_TYPE *type)
{
	static char buf[MTSN][MTSL+1];
	static int i = 0;

	i = (i + 1) % MTSN;
	char *p = buf[i];
	char *start = p;

	if (type->_static)
		*p++ = '$';

	// Already has a name
	if (type->name)
	{
		snprintf(p, MTSL-1, "%s%s", type->name, (type->_reference ? "&" : ""));
	}
	else if (type->type_class == NTC_LIST)
	{
		snprintf(p, MTSL-1, "list%s(%s%s)", (type->_reference ? "&" : ""), (type->_.list.constant?"constant ":""), nib_get_typename(context, type->_.list.type));
	}
	else if (type->type_class == NTC_ARRAY)
	{
		snprintf(p, MTSL-1, "array%s(%s%s[%ld])", (type->_reference ? "&" : ""), (type->_.array.constant?"constant ":""), nib_get_typename(context, type->_.array.type), type->_.array.length);
	}
	else if (type->type_class == NTC_MULTI)
	{
		strcpy(p, "multi(");
		ITERATOR it;
		NIB_TYPE *tp;
		bool first = true;
		iterator_start(&it, type->_.multi);
		while((tp = (NIB_TYPE *)iterator_nextdata(&it)))
		{
			if (first)
				first = false;
			else
				strcat(p, "|");

			strcat(p, nib_get_typename(context,tp));
		}
		iterator_stop(&it);
		strcat(p, ")");
	}
	else if (type->type_class == NTC_FLAG_BANK)
	{
		if (type->_.flagbank.bank)
			snprintf(p, MTSL-1, "flagbank%s(%s)", (type->_reference ? "&" : ""), nib_get_flag_bank_name(type->_.flagbank.bank));
		else
			snprintf(p, MTSL-1, "flagbank%s(???)", (type->_reference ? "&" : ""));
	}
	else if (type->type_class == NTC_FLAG)
	{
		if (type->_.flag.bits > 0)
		{
			snprintf(p, MTSL-1, "flag%s(%d)", (type->_reference ? "&" : ""), type->_.flag.bits);
		}
		else if (type->_.flag.table)
		{
			snprintf(p, MTSL-1, "flag%s(%s)", (type->_reference ? "&" : ""), nib_get_flag_table_name((context?context->flag_tables:nib_flag_created_tables),type->_.flag.table));
		}
		else
		{
			snprintf(p, MTSL-1, "flag%s(<", (type->_reference ? "&" : ""));
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
			snprintf(p, MTSL-1, "stat%s(%s)", (type->_reference ? "&" : ""), nib_get_stat_table_name((context?context->stat_tables:nib_stat_created_tables),type->_.stat.table));
		}
		else
		{
			snprintf(p, MTSL-1, "stat%s(<", (type->_reference ? "&" : ""));
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
	else if (type->type_class == NTC_STAT32)
	{
		if (type->_.stat.table)
		{
			snprintf(p, MTSL-1, "stat32%s(%s)", (type->_reference ? "&" : ""), nib_get_stat_table_name(NULL,type->_.stat.table));
		}
		else
		{
			snprintf(p, MTSL-1, "stat32%s(???)", (type->_reference ? "&" : ""));
		}
	}
	else if (type->type_class == NTC_STAT16)
	{
		if (type->_.stat.table)
		{
			snprintf(p, MTSL-1, "stat16%s(%s)", (type->_reference ? "&" : ""), nib_get_stat_table_name(NULL,type->_.stat.table));
		}
		else
		{
			snprintf(p, MTSL-1, "stat16%s(???)", (type->_reference ? "&" : ""));
		}
	}

	p[MTSL-1] = '\0';
	return start;
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
			return (a->_.primary == b->_.primary);	// They are the same

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

		case NTC_FLAG_BANK:
			return a->_.flagbank.bank == b->_.flagbank.bank;

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
			if (a->_.list.constant != b->_.list.constant) return false;
			return are_nib_types_equal(a->_.list.type, b->_.list.type);

		case NTC_ARRAY:
			if (a->_.array.constant != b->_.array.constant) return false;
			if (a->_.array.length != b->_.array.length) return false;
			return are_nib_types_equal(a->_.array.type, b->_.array.type);

		case NTC_MULTI:
			if (list_size(a->_.multi) != list_size(b->_.multi)) return false;

			// Check if a ⊆ b
			ITERATOR it;
			NIB_TYPE *ta;
			iterator_start(&it, a->_.multi);
			while((ta = (NIB_TYPE *)iterator_nextdata(&it)))
			{
				if (!is_nib_type_in_multi(b,ta))
					break;
			}
			iterator_stop(&it);

			if (ta == NULL)
			{
				// Check if b ⊆ a
				NIB_TYPE *tb;
				iterator_start(&it, b->_.multi);
				while((tb = (NIB_TYPE *)iterator_nextdata(&it)))
				{
					if (!is_nib_type_in_multi(a,tb))
						break;
				}
				iterator_stop(&it);

				return tb == NULL;
			}

			return false;
	}

	return false;
}

bool is_nib_type_in_multi(NIB_TYPE *multi, NIB_TYPE *type)
{
	if (multi->type_class != NTC_MULTI) return false;

	ITERATOR it;
	NIB_TYPE *tp;
	iterator_start(&it,multi->_.multi);
	while((tp = (NIB_TYPE *)iterator_nextdata(&it)))
	{
		if (are_nib_types_equal(tp, type))
			break;
	}
	iterator_stop(&it);

	return tp != NULL;
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
