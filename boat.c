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
#include "scripts.h"

bool ships_changed = false;
long top_ship_index_vnum = 0;

/////////////////////////////////////////////////////////////////
//
// Ship Types
//

SHIP_INDEX_DATA *load_ship_index(FILE *fp)
{
	SHIP_INDEX_DATA *ship;
	char *word;
	bool fMatch;

	ship = new_ship_index();
	ship->vnum = fread_number(fp);

	if( ship->vnum > top_ship_index_vnum)
		top_ship_index_vnum = ship->vnum;

	while (str_cmp((word = fread_word(fp)), "#-SHIP"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
		case 'B':
			if( !str_cmp(word, "Blueprint") )
			{
				long vnum = fread_number(fp);
				BLUEPRINT *bp = get_blueprint(vnum);

				if( bp )
				{
					ship->blueprint = bp;
				}

				fMatch = TRUE;
				break;
			}
			break;
		case 'C':
			KEY("Class", ship->ship_class, fread_number(fp));
			break;
		case 'D':
			KEYS("Description", ship->description, fread_string(fp));
			break;
		case 'N':
			KEYS("Name", ship->name, fread_string(fp));
			break;
		}

		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "load_ship: no match for word %.50s", word);
			bug(buf, 0);
		}

	}

	return ship;
}

void load_ships()
{
	FILE *fp = fopen(SHIPS_FILE, "r");
	if (fp == NULL)
	{
		bug("Couldn't read ships.dat", 0);
		return;
	}

	char *word;
	bool fMatch;

	top_ship_index_vnum = 0;

	while (str_cmp((word = fread_word(fp)), "#END"))
	{
		fMatch = FALSE;

		if( !str_cmp(word, "#SHIP") )
		{
			SHIP_INDEX_DATA *ship = load_ship_index(fp);
			int iHash = ship->vnum % MAX_KEY_HASH;

			ship->next = ship_index_hash[iHash];
			ship_index_hash[iHash] = ship;

			fMatch = TRUE;
			continue;
		}

		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "load_ships: no match for word %.50s", word);
			bug(buf, 0);
		}
	}

	fclose(fp);
}

void save_ship_index(FILE *fp, SHIP_INDEX_DATA *ship)
{
	fprintf(fp, "#SHIP %ld\n", ship->vnum);

	fprintf(fp, "Name %s~\n", fix_string(ship->name));
	fprintf(fp, "Description %s~\n", fix_string(ship->description));
	fprintf(fp, "Class %d\n", ship->ship_class);

	if( IS_VALID(ship->blueprint) )
		fprintf(fp, "Blueprint %ld\n", ship->blueprint->vnum);

	fprintf(fp, "#-SHIP\n\n");
}

bool save_ships()
{
	FILE *fp = fopen(SHIPS_FILE, "w");
	if (fp == NULL)
	{
		bug("Couldn't save ships.dat", 0);
		return FALSE;
	}

	int iHash;

	for(iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(SHIP_INDEX_DATA *ship = ship_index_hash[iHash]; ship; ship = ship->next)
		{
			save_ship_index(fp, ship);
		}
	}

	fprintf(fp, "#END\n");
	fclose(fp);

	ships_changed = false;
	return true;
}

SHIP_INDEX_DATA *get_ship_index(long vnum)
{
	for(int iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(SHIP_INDEX_DATA *ship = ship_index_hash[iHash]; ship; ship = ship->next)
		{
			if( ship->vnum == vnum )
				return ship;
		}
	}

	return NULL;
}


/////////////////////////////////////////////////////////////////
//
// Ships
//

/////////////////////////////////////////////////////////////////
//
// NPC Ships
//

/////////////////////////////////////////////////////////////////
//
// SHip Edit
//

const struct olc_cmd_type shedit_table[] =
{
	{ "?",					show_help			},
	{ "blueprint",			shedit_blueprint	},
	{ "class",				shedit_class		},
	{ "commands",			show_commands		},
	{ "create",				shedit_create		},
	{ "desc",				shedit_desc			},
	{ "list",				shedit_list			},
	{ "name",				shedit_name			},
	{ "show",				shedit_show			},
	{ NULL,					0,					}
};

bool can_edit_ships(CHAR_DATA *ch)
{
	return !IS_NPC(ch) && (ch->pcdata->security >= 9) && (ch->tot_level >= MAX_LEVEL);
}

void list_ship_indexes(CHAR_DATA *ch, char *argument)
{
	if( !can_edit_ships(ch) )
	{
		send_to_char("You do not have access to ships.\n\r", ch);
		return;
	}

	if(!ch->lines)
		send_to_char("{RWARNING:{W Having scrolling off may limit how many ships you can see.{x\n\r", ch);

	int lines = 0;
	bool error = FALSE;
	BUFFER *buffer = new_buf();
	char buf[MSL];

	for(long vnum = 1; vnum <= top_ship_index_vnum; vnum++)
	{
		SHIP_INDEX_DATA *ship = get_ship_index(vnum);

		if( ship )
		{
			sprintf(buf, "{Y[{W%5ld{Y] {x%-30.30s  {G%-16.16s{x \n\r",
				vnum,
				ship->name,
				flag_string(ship_class_types, ship->ship_class));

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
		send_to_char("Too many ships to list.  Please shorten!\n\r", ch);
	}
	else
	{
		if( !lines )
		{
			add_buf( buffer, "No ships to display.\n\r" );
		}
		else
		{
			// Header
			send_to_char("{Y Vnum   [            Name            ] [   Ship Class   ]{x\n\r", ch);
			send_to_char("{Y=========================================================={x\n\r", ch);
		}

		page_to_char(buffer->string, ch);
	}
	free_buf(buffer);
}

void do_shlist(CHAR_DATA *ch, char *argument)
{
	list_ship_indexes(ch, argument);
}

void shedit(CHAR_DATA *ch, char *argument)
{
	char command[MAX_INPUT_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	int  cmd;

	smash_tilde(argument);
	strcpy(arg, argument);
	argument = one_argument(argument, command);

	if (!can_edit_ships(ch))
	{
		send_to_char("SHEdit:  Insufficient security to edit ships - action logged.\n\r", ch);
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
		shedit_show(ch, argument);
		return;
	}

	for (cmd = 0; shedit_table[cmd].name != NULL; cmd++)
	{
		if (!str_prefix(command, shedit_table[cmd].name))
		{
			if ((*shedit_table[cmd].olc_fun) (ch, argument))
			{
				ships_changed = true;
			}

			return;
		}
	}

	interpret(ch, arg);
}

void do_shedit(CHAR_DATA *ch, char *argument)
{
	SHIP_INDEX_DATA *ship = NULL;
	int value;
	char arg[MAX_STRING_LENGTH];

	if (IS_NPC(ch))
		return;

	argument = one_argument(argument,arg);

	if (!can_edit_ships(ch))
	{
		send_to_char("SHEdit : Insufficient security to edit ships - action logged.\n\r", ch);
		return;
	}

	if (is_number(arg))
	{
		value = atoi(arg);
		if ( !(ship = get_ship_index(value)) )
		{
			send_to_char("That ship vnum does not exist.\n\r", ch);
			return;
		}

		ch->pcdata->immortal->last_olc_command = current_time;
		ch->desc->pEdit = (void *)ship;
		ch->desc->editor = ED_SHIP;
		return;
	}
	else if (!str_cmp(arg, "create"))
	{
		if( shedit_create(ch, argument) )
		{
			ch->pcdata->immortal->last_olc_command = current_time;
			ships_changed = true;
			ch->desc->editor = ED_SHIP;
		}
		return;
	}

	send_to_char("Syntax: shedit <vnum>\n\r"
				 "        shedit create <vnum>\n\r", ch);
}

void do_shshow(CHAR_DATA *ch, char *argument)
{
	SHIP_INDEX_DATA *ship;
	void *old_edit;
	long value;

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  shshow <vnum>\n\r", ch);
		return;
	}

	if (!is_number(argument))
	{
		send_to_char("Vnum must be a number.\n\r", ch);
		return;
	}

	value = atol(argument);
	if (!(ship = get_ship_index(value)))
	{
		send_to_char("That ship does not exist.\n\r", ch);
		return;
	}

	old_edit = ch->desc->pEdit;
	ch->desc->pEdit = (void *) ship;

	shedit_show(ch, argument);
	ch->desc->pEdit = old_edit;
	return;
}

SHEDIT( shedit_list )
{
	list_ship_indexes(ch, argument);
	return FALSE;
}

SHEDIT( shedit_show )
{
	SHIP_INDEX_DATA *ship;
	BUFFER *buffer;
	char buf[MSL];

	EDIT_SHIP(ch, ship);

	buffer = new_buf();

	add_buf(buffer, "{x");

	sprintf(buf, "Name:        [%5ld] %s{x\n\r", ship->vnum, ship->name);
	add_buf(buffer, buf);

	sprintf(buf, "Ship Class:  %s{x\n\r", flag_string(ship_class_types, ship->ship_class));
	add_buf(buffer, buf);

	if( IS_VALID(ship->blueprint) )
		sprintf(buf, "Blueprint:   [%5ld] %s{x\n\r", ship->blueprint->vnum, ship->blueprint->name);
	else
		sprintf(buf, "Blueprint:   {Dunassigned{x\n\r");
	add_buf(buffer, buf);

	add_buf(buffer, "Description:\n\r");
	add_buf(buffer, fix_string(ship->description));
	add_buf(buffer, "\n\r");

	if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
	{
		send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
	}
	else
	{
		page_to_char(buffer->string, ch);
	}

	free_buf(buffer);
	return FALSE;
}

SHEDIT( shedit_create )
{
	SHIP_INDEX_DATA *ship;
	long  value;
	int  iHash;

	value = atol(argument);
	if (argument[0] == '\0' || value == 0)
	{
		long last_vnum = 0;
		value = top_ship_index_vnum + 1;
		for(last_vnum = 1; last_vnum <= top_ship_index_vnum; last_vnum++)
		{
			if( !get_ship_index(last_vnum) )
			{
				value = last_vnum;
				break;
			}
		}
	}
	else if( get_ship_index(value) )
	{
		send_to_char("That vnum already exists.\n\r", ch);
		return FALSE;
	}

	ship = new_ship_index();
	ship->vnum = value;

	iHash							= ship->vnum % MAX_KEY_HASH;
	ship->next						= ship_index_hash[iHash];
	ship_index_hash[iHash]			= ship;
	ch->desc->pEdit					= (void *)ship;

	if( ship->vnum > top_ship_index_vnum)
		top_ship_index_vnum = ship->vnum;

    return TRUE;
}

SHEDIT( shedit_name )
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	smash_tilde(argument);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  name [name]\n\r", ch);
		return FALSE;
	}

	free_string(ship->name);
	ship->name = str_dup(argument);
	send_to_char("Name changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_desc )
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	if( argument[0] != '\0' )
	{
		send_to_char("Syntax:  desc\n\r", ch);
		return FALSE;
	}

	string_append(ch, &ship->description);
	return TRUE;
}

SHEDIT( shedit_class )
{
	SHIP_INDEX_DATA *ship;
	int value;

	EDIT_SHIP(ch, ship);

	value = flag_value(ship_class_types, argument);
	if( value == NO_FLAG )
	{
		send_to_char("Syntax:  class [ship class]\n\r", ch);
		send_to_char("See '? shipclass' for list of classes.\n\r\n\r", ch);
		show_help(ch, "shipclass");
		return FALSE;
	}

	ship->ship_class = value;
	send_to_char("Ship Class changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_blueprint )
{
	SHIP_INDEX_DATA *ship;
	BLUEPRINT *bp;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  blueprint [vnum]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	if( !(bp = get_blueprint(atol(argument))) )
	{
		send_to_char("Blueprint does not exist.\n\r", ch);
		return FALSE;
	}

	if( bp->mode == BLUEPRINT_MODE_STATIC )
	{

		// Verify the blueprint has certain features
		// * Has an entry room
		if( bp->static_entry_section < 1 || bp->static_entry_link < 1)
		{
			send_to_char("Blueprint requires an entry point for boarding purposes.\n\r", ch);
			return FALSE;
		}

		// * Room with HELM
		// * Room with VIEWWILDS (optional)
		bool helm = false, viewwilds = false;
		ITERATOR sit;

		// Check special rooms
		BLUEPRINT_SPECIAL_ROOM *special_room;
		iterator_start(&sit, bp->special_rooms);
		while( (special_room = (BLUEPRINT_SPECIAL_ROOM *)iterator_nextdata(&sit)) )
		{
			ROOM_INDEX_DATA *room = get_room_index(special_room->vnum);

			if( room )
			{
				if( IS_SET(room->room_flags, ROOM_SHIP_HELM) )
				{
					helm = true;
				}

				if( IS_SET(room->room_flags, ROOM_VIEWWILDS) )
				{
					viewwilds = true;
				}
			}
		}
		iterator_stop(&sit);

		if( !helm || !viewwilds )
		{
			BLUEPRINT_SECTION *section;
			iterator_start(&sit, bp->sections);
			while( (section = (BLUEPRINT_SECTION *)iterator_nextdata(&sit)) )
			{
				for( long vnum = section->lower_vnum; vnum <= section->upper_vnum; vnum++)
				{
					ROOM_INDEX_DATA *room = get_room_index(vnum);

					if( room )
					{
						if( IS_SET(room->room_flags, ROOM_SHIP_HELM) )
						{
							helm = true;
						}

						if( IS_SET(room->room_flags, ROOM_VIEWWILDS) )
						{
							viewwilds = true;
						}
					}
				}
			}
			iterator_stop(&sit);
		}


		if( !helm )
		{
			send_to_char("Blueprint requires at least one room with the 'helm' flag set, for controlling the ship.\n\r", ch);
			return FALSE;
		}

		if( !viewwilds )
		{
			// Not a deal breaker, just warn about it being missing
			send_to_char("{YWARNING: {xBlueprint missing a room with 'viewwilds' to serve as a crow's nest. Might want to add one.\n\r", ch);
		}

	}
	else
	{
		send_to_char("Only static blueprints supported.\n\r", ch);
		return FALSE;
	}



	ship->blueprint = bp;
	send_to_char("Ship blueprint changed.\n\r", ch);
	return TRUE;
}


/////////////////////////////////////////////////////////////////
//
// NPC SHip Edit
//
