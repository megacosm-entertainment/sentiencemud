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
*       ROM 2.4 is copyright 1993-1998 Russ Taylor                         *
*       ROM has been brought to you by the ROM consortium                  *
*           Russ Taylor (rtaylor@hypercube.org)                            *
*           Gabrielle Taylor (gtaylor@hypercube.org)                       *
*           Brian Moore (zump@rom.org)                                     *
*       By using this code, you have agreed to follow the terms of the     *
*       ROM license, in the file Rom24/doc/rom.license                     *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdarg.h>
#include "merc.h"
#include "recycle.h"

// load blueprints
void load_blueprints()
{
	FILE *fp = fopen(BLUEPRINTS_FILE, "r");
	if (fp == NULL)
	{
		bug("Couldn't load blueprints.dat", 0);
		return;
	}



	fclose(fp);
}

// save blueprints
bool save_blueprints()
{
	FILE *fp = fopen(BLUEPRINTS_FILE, "w");
	if (fp == NULL)
	{
		bug("Couldn't save blueprints.dat", 0);
		return FALSE;
	}

	BLUEPRINT *bp;
	for(bp = blueprints; bp; bp = bp->next)
	{
		fprintf(fp, "#BLUEPRINT %ld\n\r", bp->vnum);
		fprintf(fp, "Name %s~\n\r", bp->name);
		fprintf(fp, "Description %s~\n\r", bp->description);

		fprintf(fp, "Recall %ld\n\r", bp->recall);
		fprintf(fp, "Lower %ld\n\r", bp->lower_vnum);
		fprintf(fp, "Upper %ld\n\r", bp->upper_vnum);

		fprintf(fp, "End\n\r");
	}

	fprintf(fp, "#END\n\r");

	fclose(fp);
	return TRUE;
}

// clone blueprint
