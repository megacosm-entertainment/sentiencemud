/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*	ROM 2.4 is copyright 1993-1998 Russ Taylor			   *
*	ROM has been brought to you by the ROM consortium		   *
*	    Russ Taylor (rtaylor@hypercube.org)				   *
*	    Gabrielle Taylor (gtaylor@hypercube.org)			   *
*	    Brian Moore (zump@rom.org)					   *
*	By using this code, you have agreed to follow the terms of the	   *
*	ROM license, in the file Rom24/doc/rom.license			   *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include "merc.h"
#include "recycle.h"
#include "tables.h"


int flag_lookup (const char *name, const struct flag_type *flag_table)
{
    int flag;

    for (flag = 0; flag_table[flag].name != NULL; flag++)
    {
	if (LOWER(name[0]) == LOWER(flag_table[flag].name[0]) &&
		!str_prefix(name,flag_table[flag].name) &&
		flag_table[flag].settable)
	    return flag_table[flag].bit;
    }

    return 0;
}

int flag_find (const char *name, const struct flag_type *flag_table)
{
    int flag;

    for (flag = 0; flag_table[flag].name != NULL; flag++)
    {
	if (LOWER(name[0]) == LOWER(flag_table[flag].name[0]) &&
		!str_prefix(name,flag_table[flag].name))
	    return flag_table[flag].bit;
    }

    return 0;
}

int stat_lookup (const char *name, const struct flag_type *flag_table, int invalid)
{
    int flag;

    for (flag = 0; flag_table[flag].name != NULL; flag++)
    {
	if (LOWER(name[0]) == LOWER(flag_table[flag].name[0]) &&
		!str_prefix(name,flag_table[flag].name) &&
		flag_table[flag].settable)
	    return flag_table[flag].bit;
    }

    return invalid;
}

int stat_find (const char *name, const struct flag_type *flag_table, int invalid)
{
    int flag;

    for (flag = 0; flag_table[flag].name != NULL; flag++)
    {
	if (LOWER(name[0]) == LOWER(flag_table[flag].name[0]) &&
		!str_prefix(name,flag_table[flag].name))
	    return flag_table[flag].bit;
    }

    return invalid;
}

int position_lookup (const char *name)
{
   int pos;

   for (pos = 0; position_table[pos].name != NULL; pos++)
   {
	if (LOWER(name[0]) == LOWER(position_table[pos].name[0])
	&&  !str_prefix(name,position_table[pos].name))
	    return pos;
   }

   return -1;
}


int sex_lookup (const char *name)
{
   int sex;

   for (sex = 0; sex_table[sex].name != NULL; sex++)
   {
	if (LOWER(name[0]) == LOWER(sex_table[sex].name[0])
	&&  !str_prefix(name,sex_table[sex].name))
	    return sex;
   }

   return -1;
}


int size_lookup (const char *name)
{
   int size;

   for ( size = 0; size_table[size].name != NULL; size++)
   {
        if (LOWER(name[0]) == LOWER(size_table[size].name[0])
        &&  !str_prefix( name,size_table[size].name))
            return size;
   }

   return -1;
}


/* returns race number */
int race_lookup (const char *name)
{
   int race;

   for ( race = 0; race_table[race].name != NULL; race++)
   {
	if (LOWER(name[0]) == LOWER(race_table[race].name[0])
	&&  !str_prefix( name,race_table[race].name))
	    return race;
   }

   return 0;
}


int item_lookup(const char *name)
{
    int type;

    for (type = 0; item_table[type].name != NULL; type++)
    {
        if (LOWER(name[0]) == LOWER(item_table[type].name[0])
        &&  !str_prefix(name,item_table[type].name))
            return item_table[type].type;
    }

    return -1;
}


int liq_lookup (const char *name)
{
    int liq;

    for ( liq = 0; liq_table[liq].liq_name != NULL; liq++)
    {
	if (LOWER(name[0]) == LOWER(liq_table[liq].liq_name[0])
	&& !str_prefix(name,liq_table[liq].liq_name))
	    return liq;
    }

    return -1;
}


int material_lookup (register const char *name)
{
	register int i;

	for(i = 0; material_table[i].name; i++)
		if (!str_prefix(name,material_table[i].name))
			return i;

	return -1;
}


char *get_weapon_class(OBJ_INDEX_DATA *obj)
{
	char *name = flag_name(weapon_class, obj->value[0]);

	return name ? name : "unknown";
}


char *flag_name(register const struct flag_type *flag_table, register int bit)
{
	register int i;

	for (i = 0; flag_table[i].name; i++)
		if (flag_table[i].bit == bit)
			return flag_table[i].name;

	return NULL;
}

// @@@NIB : 20070120
int damage_class_lookup(const char *name)
{
	int dc;

	if(*name) {
		dc = flag_lookup(name, damage_classes);
		if(dc == DAM_NONE) dc = DAM_BASH;
	} else
		dc = DAM_NONE;

	return dc;
}

// @@@NIB : 20070120
int toxin_lookup (const char *name)
{
	int tox;
	for (tox = 0; toxin_table[tox].name; tox++)
		if (!str_prefix (name, toxin_table[tox].name))
			return tox;

	return -1;
}
