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

bool dungeons_changed = FALSE;
long top_dungeon_vnum = 0;
LLIST *loaded_dungeons;

DUNGEON_INDEX_DATA *load_dungeon_index(FILE *fp)
{
	DUNGEON_INDEX_DATA *dng;
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
			KEY("Entry", dng->entry_room, fread_number(fp));
			KEY("Exit", dng->exit_room, fread_number(fp));
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

		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "load_dungeon_index: no match for word %.50s", word);
			bug(buf, 0);
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
			DUNGEON_INDEX_DATA *dng = load_dungeon_index(fp);
			int iHash = dng->vnum % MAX_KEY_HASH;

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


DUNGEON_INDEX_DATA *get_dungeon_index(long vnum)
{
	for(int iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(DUNGEON_INDEX_DATA *dng = dungeon_index_hash[iHash]; dng; dng = dng->next)
		{
			if( dng->vnum == vnum )
				return dng;
		}
	}

	return NULL;
}

DUNGEON *create_dungeon(long vnum)
{
	ITERATOR it;

	DUNGEON_INDEX_DATA *index = get_dungeon_index(vnum);

	if( !IS_VALID(index) ) return NULL;

	DUNGEON *dng = new_dungeon();
	dng->index = index;

	dng->entry_room = get_room_index(index->entry_room);
	if( !dng->entry_room )
	{
		free_dungeon(dng);
		return NULL;
	}

	dng->exit_room = get_room_index(index->exit_room);
	if( !dng->exit_room )
	{
		free_dungeon(dng);
		return NULL;
	}

	dng->flags = index->flags;

	int floor = 1;
	bool error = FALSE;
	BLUEPRINT *bp;
	INSTANCE *instance;
	iterator_start(&it, index->floors);
	while( (bp = (BLUEPRINT *)iterator_nextdata(&it)) )
	{
		instance = create_instance(bp);

		if( !instance )
		{
			error = TRUE;
			break;
		}

		instance->floor = floor++;
		instance->dungeon = dng;
		list_appendlink(dng->floors, instance);
	}
	iterator_stop(&it);

	if( error )
	{
		free_dungeon(dng);
		return NULL;
	}

	list_appendlink(loaded_dungeons, dng);
	return dng;
}


void extract_dungeon(DUNGEON *dungeon)
{
	ITERATOR it;
	CHAR_DATA *ch;
	OBJ_DATA *obj;
	ROOM_INDEX_DATA *room;

	room = dungeon->entry_room;
	if( !room )
		room = get_room_index(11001);

	// Dump all mobiles
	iterator_start(&it, dungeon->mobiles);
	while( (ch = (CHAR_DATA *)iterator_nextdata(&it)) )
	{
		char_from_room(ch);
		char_to_room(ch, room);
	}
	iterator_stop(&it);

	// Dump objects
	room = dungeon->entry_room;
	if( !room )
		room = get_room_index(ROOM_VNUM_DONATION);

	iterator_start(&it, dungeon->objects);
	while( (obj = (OBJ_DATA *)iterator_nextdata(&it)) )
	{
		if( obj->in_obj )
			obj_from_obj (obj);
		else if( obj->carried_by )
			obj_from_char(obj);
		else if( obj->in_room)
			obj_from_room(obj);

		obj_to_room(obj, room);
	}
	iterator_stop(&it);


	list_remlink(loaded_dungeons, dungeon);
	free_dungeon(dungeon);
}


DUNGEON *find_dungeon_byplayer(CHAR_DATA *ch, long vnum)
{
	ITERATOR dit;
	DUNGEON *dng;

	iterator_start(&dit, loaded_dungeons);
	while( (dng = (DUNGEON *)iterator_nextdata(&dit)) )
	{
		if( dng->index->vnum == vnum && dng->player == ch )
			break;
	}
	iterator_stop(&dit);

	return dng;
}


ROOM_INDEX_DATA *spawn_dungeon(CHAR_DATA *ch, long vnum)
{
	CHAR_DATA *master = (ch->master != NULL) ? ch->master : ch;

	DUNGEON *dng = find_dungeon_byplayer(master, vnum);

	if( dng )
	{
		if( !IS_NPC(ch) && dng->player != ch )
		{
			// CH has gone into someone else's dungeon.  Purge
			DUNGEON *old_dng = find_dungeon_byplayer(ch, vnum);

			if( old_dng )
			{
				extract_dungeon(old_dng);
			}

			// Tell the player?
		}

	}
	else
	{
		dng = create_dungeon(vnum);

		if( !dng )
			return NULL;
	}

	INSTANCE *first_floor = (INSTANCE *)list_nthdata(dng->floors, 1);

	if( !IS_VALID(first_floor) )
		return NULL;

	return first_floor->entrance;
}

/////////////////////////////////////////
//
// DUNGEON EDITOR
//

const struct olc_cmd_type dngedit_table[] =
{
	{ "?",				show_help			},
	{ "commands",		show_commands		},
	{ "list",			dngedit_list		},
	{ "show",			dngedit_show		},
	{ "create",			dngedit_create		},
	{ "name",			dngedit_name		},
	{ "description",	dngedit_description	},
	{ "comments",		dngedit_comments	},
	{ "areawho",		dngedit_areawho		},
	{ "floors",			dngedit_floors		},
	{ "entry",			dngedit_entry		},
	{ "exit",			dngedit_exit		},
	{ "flags",			dngedit_flags		},
	{ NULL,				NULL				}

};

void list_dungeons(CHAR_DATA *ch, char *argument)
{
	if( !can_edit_dungeons(ch) )
	{
		send_to_char("You do not have access to dungeons.\n\r", ch);
		return;
	}

	if(!ch->lines)
		send_to_char("{RWARNING:{W Having scrolling off may limit how many dungeons you can see.{x\n\r", ch);

	int lines = 0;
	bool error = FALSE;
	BUFFER *buffer = new_buf();
	char buf[MSL];

	for(long vnum = 1; vnum <= top_dungeon_vnum; vnum++)
	{
		DUNGEON_INDEX_DATA *dng = get_dungeon_index(vnum);

		if( dng )
		{
			sprintf(buf, "{Y[{W%5ld{Y] {x%-30.30s\n\r",
				vnum,
				dng->name);

			++lines;
			if( !add_buf(buffer, buf) || (!ch->lines && strlen(buf_string(buffer)) > MAX_STRING_LENGTH) )
			{
				error = TRUE;
				break;
			}
		}
	}

	if( error )
	{
		send_to_char("Too many dungeons to list.  Please shorten!\n\r", ch);
	}
	else
	{
		if( !lines )
		{
			add_buf( buffer, "No dungeons to display.\n\r" );
		}
		else
		{
			// Header
			send_to_char("{Y Vnum   [            Name            ]{x\n\r", ch);
			send_to_char("{Y======================================={x\n\r", ch);
		}

		page_to_char(buffer->string, ch);
	}
	free_buf(buffer);
}

void do_dnglist(CHAR_DATA *ch, char *argument)
{
	list_dungeons(ch, argument);
}

void do_dngedit(CHAR_DATA *ch, char *argument)
{
	DUNGEON_INDEX_DATA *dng;
	long value;
	char arg1[MAX_STRING_LENGTH];

	argument = one_argument(argument, arg1);

	if (IS_NPC(ch))
		return;

	if (is_number(arg1))
	{
		value = atol(arg1);
		if (!(dng = get_dungeon_index(value)))
		{
			send_to_char("DNGEdit:  That vnum does not exist.\n\r", ch);
			return;
		}

		if (!can_edit_blueprints(ch))
		{
			send_to_char("DNGEdit:  Insufficient security to edit dungeons.\n\r", ch);
			return;
		}

		ch->pcdata->immortal->last_olc_command = current_time;
		ch->desc->pEdit = (void *)dng;
		ch->desc->editor = ED_DUNGEON;
		return;
	}
	else
	{
		if (!str_cmp(arg1, "create"))
		{
			if (dngedit_create(ch, argument))
			{
				dungeons_changed = TRUE;
				ch->desc->editor = ED_DUNGEON;
			}

			return;
		}

	}

	send_to_char("DNGEdit:  There is no default dungeon to edit.\n\r", ch);
}

void dngedit(CHAR_DATA *ch, char *argument)
{
	char command[MAX_INPUT_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	int  cmd;

	smash_tilde(argument);
	strcpy(arg, argument);
	argument = one_argument(argument, command);

	if (!can_edit_dungeons(ch))
	{
		send_to_char("DNGEdit:  Insufficient security to edit dungeons.\n\r", ch);
		edit_done(ch);
		return;
	}

	if (!str_cmp(command, "done"))
	{
		edit_done(ch);
		return;
	}

    ch->pcdata->immortal->last_olc_command = current_time;

	if (command[0] == '\0')
	{
		dngedit_show(ch, argument);
		return;
	}

	for (cmd = 0; dngedit_table[cmd].name != NULL; cmd++)
	{
		if (!str_prefix(command, dngedit_table[cmd].name))
		{
			if ((*dngedit_table[cmd].olc_fun) (ch, argument))
			{
				dungeons_changed = TRUE;
				return;
			}
			else
				return;
		}
	}

    interpret(ch, arg);
}

DNGEDIT( dngedit_list )
{
	list_dungeons(ch, argument);
	return FALSE;
}

void dngedit_buffer_floors(BUFFER *buffer, DUNGEON_INDEX_DATA *dng)
{
	char buf[MSL];

	if( list_size(dng->floors) > 0 )
	{
		ITERATOR fit;
		BLUEPRINT *bp;

		add_buf(buffer, "{gFloors:{x\n\r");
		add_buf(buffer, "{g     [  Vnum  ] [             Name             ]\n\r");
		add_buf(buffer, "{g=================================================\n\r");

		int floor = 0;
		iterator_start(&fit, dng->floors);
		while( (bp = (BLUEPRINT *)iterator_nextdata(&fit)) )
		{
			sprintf(buf, "{W%4d  {G%8ld   {x%-.30s{x\n\r", ++floor, bp->vnum, bp->name);
			add_buf(buffer, buf);
		}
		iterator_stop(&fit);
		add_buf(buffer, "=================================================\n\r");
	}
	else
	{
		add_buf(buffer, "{gFloors:{x\n\r");
		add_buf(buffer, "   None\n\r");
	}

}

DNGEDIT( dngedit_show )
{
	DUNGEON_INDEX_DATA *dng;
	ROOM_INDEX_DATA *room;
	BUFFER *buffer;
	char buf[MSL];

	EDIT_DUNGEON(ch, dng);

	buffer = new_buf();

	sprintf(buf, "Name:        [%5ld] %s\n\r", dng->vnum, dng->name);
	add_buf(buffer, buf);

	sprintf(buf, "Flags:       %s\n\r", flag_string(dungeon_flags, dng->flags));
	add_buf(buffer, buf);

	sprintf(buf, "AreaWho:     %s\n\r", flag_string(area_who_titles, dng->area_who));
	add_buf(buffer, buf);

	room = get_room_index(dng->entry_room);
	if( room )
	{
		sprintf(buf, "Entry:       [%ld] %-.30s\n\r", room->vnum, room->name);
		add_buf(buffer, buf);
	}
	else
		add_buf(buffer, "Entry:       {Dinvalid{x\n\r");

	room = get_room_index(dng->exit_room);
	if( room )
	{
		sprintf(buf, "Exit:        [%ld] %-.30s\n\r", room->vnum, room->name);
		add_buf(buffer, buf);
	}
	else
		add_buf(buffer, "Exit:        {Dinvalid{x\n\r");

	add_buf(buffer, "Description:\n\r");
	add_buf(buffer, dng->description);
	add_buf(buffer, "\n\r");

	add_buf(buffer, "\n\r-----\n\r{WBuilders' Comments:{X\n\r");
	add_buf(buffer, dng->comments);
	add_buf(buffer, "\n\r-----\n\r");

	dngedit_buffer_floors(buffer, dng);

	page_to_char(buffer->string, ch);

	free_buf(buffer);
	return FALSE;
}


DNGEDIT( dngedit_create )
{
	DUNGEON_INDEX_DATA *dng;
	long  value;
	int  iHash;

	value = atol(argument);
	if (argument[0] == '\0' || value == 0)
	{
		long last_vnum = 0;
		value = top_dungeon_vnum + 1;
		for(last_vnum = 1; last_vnum <= top_dungeon_vnum; last_vnum++)
		{
			if( !get_dungeon_index(last_vnum) )
			{
				value = last_vnum;
				break;
			}
		}
	}

	dng = new_dungeon_index();
	dng->vnum = value;

	iHash							= dng->vnum % MAX_KEY_HASH;
	dng->next						= dungeon_index_hash[iHash];
	dungeon_index_hash[iHash]	= dng;
	ch->desc->pEdit					= (void *)dng;

	if( dng->vnum > top_dungeon_vnum)
		top_dungeon_vnum = dng->vnum;

    return TRUE;
}

DNGEDIT( dngedit_name )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  name [string]\n\r", ch);
		return FALSE;
	}

	free_string(dng->name);
	dng->name = str_dup(argument);
	send_to_char("Name changed.\n\r", ch);
	return TRUE;
}

DNGEDIT( dngedit_description )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	if (argument[0] == '\0')
	{
		string_append(ch, &dng->description);
		return TRUE;
	}

	send_to_char("Syntax:  description - line edit\n\r", ch);
	return FALSE;
}

DNGEDIT( dngedit_comments )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	if (argument[0] == '\0')
	{
		string_append(ch, &dng->comments);
		return TRUE;
	}

	send_to_char("Syntax:  comments - line edit\n\r", ch);
	return FALSE;
}

DNGEDIT( dngedit_areawho )
{
	DUNGEON_INDEX_DATA *dng;
	int value;

	EDIT_DUNGEON(ch, dng);

    if (argument[0] != '\0')
    {
		if ( !str_prefix(argument, "blank") )
		{
			dng->area_who = AREA_BLANK;

			send_to_char("Area who title cleared.\n\r", ch);
			return TRUE;
		}

		if ((value = flag_value(area_who_titles, argument)) != NO_FLAG)
		{
			if( value == AREA_INSTANCE || value == AREA_DUTY )
			{
				send_to_char("Area who title only allowed in blueprints.\n\r", ch);
				return FALSE;
			}

			dng->area_who = value;

			send_to_char("Area who title set.\n\r", ch);
			return TRUE;
		}
    }

    send_to_char("Syntax:  areawho [title]\n\r"
				"Type '? areawho' for a list of who titles.\n\r", ch);
    return FALSE;

}

DNGEDIT( dngedit_floors )
{
	DUNGEON_INDEX_DATA *dng;
	char arg[MIL];

	EDIT_DUNGEON(ch, dng);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  floors add <vnum>\n\r", ch);
		send_to_char("         floors remove #\n\r", ch);
		send_to_char("         floors list\n\r", ch);
		return FALSE;
	}

	argument = one_argument(argument, arg);

	if( !str_prefix(arg, "list") )
	{
		BUFFER *buffer = new_buf();

		dngedit_buffer_floors(buffer, dng);

		page_to_char(buffer->string, ch);
		free_buf(buffer);
		return FALSE;
	}

	if( !str_prefix(arg, "add") )
	{
		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return FALSE;
		}

		long vnum = atol(argument);

		BLUEPRINT *bp = get_blueprint(vnum);

		if( !bp )
		{
			send_to_char("That blueprint does not exist.\n\r", ch);
			return FALSE;
		}

		if( bp->mode == BLUEPRINT_MODE_STATIC )
		{
			if( bp->static_entry_section < 1 || bp->static_entry_link < 1 ||
				bp->static_exit_section < 1 || bp->static_exit_link < 1 )
			{
				send_to_char("Blueprint must have an entrance and exit specified.\n\r", ch);
				return FALSE;
			}
		}
		else
		{
			send_to_char("Blueprint mode not supported yet.\n\r", ch);
			return FALSE;
		}

		list_appendlink(dng->floors, bp);
		send_to_char("Floor added.\n\r", ch);
		return TRUE;
	}

	if( !str_prefix(arg, "remove") )
	{
		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return FALSE;
		}

		int index = atoi(argument);

		if( index < 1 || index > list_size(dng->floors) )
		{
			send_to_char("Index out of range.\n\r", ch);
			return FALSE;
		}

		list_remnthlink(dng->floors, index);
		send_to_char("Floor removed.\n\r", ch);
		return TRUE;
	}

	dngedit_floors(ch, "");
	return FALSE;
}

DNGEDIT( dngedit_entry )
{
	DUNGEON_INDEX_DATA *dng;
	long value;

	EDIT_DUNGEON(ch, dng);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  entry <vnum>\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	value = atol(argument);

	if( !get_room_index(value) )
	{
		send_to_char("That room does not exist.\n\r", ch);
		return FALSE;
	}

	dng->entry_room = value;
	send_to_char("Entry room changed.\n\r", ch);
	return TRUE;
}

DNGEDIT( dngedit_exit )
{
	DUNGEON_INDEX_DATA *dng;
	long value;

	EDIT_DUNGEON(ch, dng);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  exit <vnum>\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	value = atol(argument);

	if( !get_room_index(value) )
	{
		send_to_char("That room does not exist.\n\r", ch);
		return FALSE;
	}

	dng->exit_room = value;
	send_to_char("Exit room changed.\n\r", ch);
	return TRUE;
}

DNGEDIT( dngedit_flags )
{
	DUNGEON_INDEX_DATA *dng;
	int value;

	EDIT_DUNGEON(ch, dng);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  flags <flags>\n\r", ch);
		send_to_char("'? dungeon' for list of flags.\n\r", ch);
		return FALSE;
	}

	if( (value = flag_value(blueprint_section_flags, argument)) != NO_FLAG )
	{
		dng->flags ^= value;
		send_to_char("Dungeon flags changed.\n\r", ch);
		return TRUE;
	}

	dngedit_flags(ch, "");
	return FALSE;

}

//////////////////////////////////////////////////////////////
//
// Immortal Commands
//


void do_dungeon(CHAR_DATA *ch, char *argument)
{
	char arg1[MIL];

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  dungeon list\n\r", ch);
		send_to_char("         dungeon unload #\n\r", ch);
		return;
	}

	argument = one_argument(argument, arg1);

	if( !str_prefix(arg1, "list") )
	{
		if(!ch->lines)
			send_to_char("{RWARNING:{W Having scrolling off may limit how many dungeons you can see.{x\n\r", ch);

		int lines = 0;
		bool error = FALSE;
		BUFFER *buffer = new_buf();
		char buf[MSL];


		ITERATOR it;
		DUNGEON *dungeon;

		iterator_start(&it, loaded_dungeons);
		while((dungeon = (DUNGEON *)iterator_nextdata(&it)))
		{
			++lines;

			sprintf(buf, "%4d {Y[{W%5ld{Y] {x%-30.30s{x\n\r",
				lines,
				dungeon->index->vnum,
				dungeon->index->name);

			if( !add_buf(buffer, buf) || (!ch->lines && strlen(buf_string(buffer)) > MAX_STRING_LENGTH) )
			{
				error = TRUE;
				break;
			}
		}
		iterator_stop(&it);

		if( error )
		{
			send_to_char("Too many dungeons to list.  Please shorten!\n\r", ch);
		}
		else
		{
			if( !lines )
			{
				add_buf( buffer, "No dungeons to display.\n\r" );
			}
			else
			{
				// Header
				send_to_char("{Y      Vnum   [            Name            ] {x\n\r", ch);
				send_to_char("{Y============================================{x\n\r", ch);
			}

			page_to_char(buffer->string, ch);
		}
		free_buf(buffer);

		return;
	}

	if( !str_prefix(arg1, "unload") )
	{
		if( !can_edit_dungeons(ch) )
		{
			send_to_char("Insufficient access to unload dungeons.\n\r", ch);
			return;
		}

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		int index = atoi(argument);

		if( list_size(loaded_dungeons) < index )
		{
			send_to_char("No dungeon at that index.\n\r", ch);
			return;
		}

		DUNGEON *dungeon = (DUNGEON *)list_nthdata(loaded_dungeons, index);

		extract_dungeon(dungeon);
		send_to_char("Dungeon unloaded.\n\r", ch);
		return;
	}

	do_dungeon(ch, "");
	return;
}

