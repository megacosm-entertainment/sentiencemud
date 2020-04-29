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
#include "olc.h"
#include "tables.h"

bool blueprints_changed = FALSE;
long top_dungeon_vnum = 0;
LLIST *loaded_dungeons;

DUNGEON_INDEX *load_dungeon_index(FILE *fp)
{
	DUNGEON_INDEX *dng;
	char *word;
	bool fMatch;

	dng = new_dungeon_index();
	dng->vnum = fread_number(fp);

	if( dng->vnum > top_dungeon_vnum)
		top_dungeon_vnum = dng->vnum;

	while (str_cmp((word = fread_word(fp)), "#-DUNGEON"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
		case 'A':
			KEY("AreaWho", dng->area_who, fread_number(fp));
			break;

		case 'C':
			KEYS("Comments", dng->comments, fread_string(fp));
			break;

		case 'D':
			KEYS("Description", dng->description, fread_string(fp));
			break;

		case 'E':
			KEY("Entry", dng->entry_room, fread_string(fp));
			KEY("Exit", dng->exit_room, fread_string(fp));
			break;

		case 'F':
			KEY("Flags", dng->flags, fread_number(fp));
			if( !str_cmp(word, "Floor") )
			{
				long bp_vnum = fread_number(fp);

				BLUEPRINT *bp = get_blueprint(bp_vnum);

				if( bp )
				{
					list_appendlink(dng->floors, bp);
				}

				fMatch = TRUE;
				break;
			}
			break;

		case 'N':
			KEYS("Name", dng->name, fread_string(fp));
			break;

		}
	}

	return dng;

}

void load_dungeons()
{
	FILE *fp = fopen(DUNGEONS_FILE, "r");
	if (fp == NULL)
	{
		bug("Couldn't load dungeons.dat", 0);
		return;
	}
	char *word;
	bool fMatch;

	while (str_cmp((word = fread_word(fp)), "#END"))
	{
		fMatch = FALSE;

		if( !str_cmp(word, "#DUNGEON") )
		{
			DUNGEON_INDEX *dng = load_dungeon_index(fp);
			int iHash = bp->vnum % MAX_KEY_HASH;

			dng->next = dungeon_index_hash[iHash];
			dungeon_index_hash[iHash] = dng;

			fMatch = TRUE;
		}

		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "load_dungeons: no match for word %.50s", word);
			bug(buf, 0);
		}

	}

	fclose(fp);
}


void save_dungeon_index(FILE *fp, DUNGEON_INDEX_DATA *dng)
{
	fprintf(fp, "#DUNGEON %ld\n\r", dng->vnum);
	fprintf(fp, "Name %s~\n\r", fix_string(dng->name));
	fprintf(fp, "Description %s~\n\r", fix_string(dng->description));
	fprintf(fp, "Comments %s~\n\r", fix_string(dng->comments));
	fprintf(fp, "AreaWho %d\n\r", dng->area_who);

	fprintf(fp, "Flags %d\n\r", dng->flags);

	if( dng->entry_room > 0 )
		fprintf(fp, "Entry %ld\n\r", dng->entry_room);

	if( dng->exit_room > 0 )
		fprintf(fp, "Exit %ld\n\r", dng->exit_room);

	ITERATOR fit;
	BLUEPRINT *bp;
	iterator_start(&fit, dng->floors);
	while((bp = (BLUEPRINT *)iterator_nextdata(&fit)))
	{
		fprintf(fp, "Floor %ld\n\r", bp->vnum);
	}
	iterator_stop(&fit);

	fprintf(fp, "#-DUNGEON\n\r\n\r");
}

bool save_dungeons()
{
	FILE *fp = fopen(DUNGEONS_FILE, "w");
	if (fp == NULL)
	{
		bug("Couldn't save dungeons.dat", 0);
		return FALSE;
	}

	int iHash;
	for(iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(DUNGEON_INDEX_DATA *dng = dungeon_index_hash[iHash]; dng; dng = dng->next)
		{
			save_dungeon_index(fp, dng);
		}
	}

	fprintf(fp, "#END\n\r");

	fclose(fp);

	dungeons_changed = FALSE;
	return TRUE;
}

bool can_edit_dungeons(CHAR_DATA *ch)
{
	return !IS_NPC(ch) && (ch->pcdata->security >= 9) && (ch->tot_level >= MAX_LEVEL);
}
