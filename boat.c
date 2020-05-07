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

INSTANCE *instance_load(FILE *fp);
void update_instance(INSTANCE *instance);
void reset_instance(INSTANCE *instance);
void save_script_new(FILE *fp, AREA_DATA *area,SCRIPT_DATA *scr,char *type);
SCRIPT_DATA *read_script_new( FILE *fp, AREA_DATA *area, int type);

extern LLIST *loaded_instances;

bool ships_changed = false;
long top_ship_index_vnum = 0;

LLIST *loaded_ships;

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
		case 'A':
			KEY("Armor", ship->armor, fread_number(fp));
			break;

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
			KEY("Capacity", ship->capacity, fread_number(fp));
			KEY("Class", ship->ship_class, fread_number(fp));
			if( !str_cmp(word, "Crew") )
			{
				ship->min_crew = fread_number(fp);
				ship->max_crew = fread_number(fp);

				fMatch = TRUE;
				break;
			}
			break;

		case 'D':
			KEYS("Description", ship->description, fread_string(fp));
			break;

		case 'F':
			KEY("Flags", ship->flags, fread_number(fp));
			break;

		case 'G':
			KEY("Guns", ship->guns, fread_number(fp));
			break;

		case 'H':
			KEY("Hit", ship->hit, fread_number(fp));
			break;

		case 'K':
			if( !str_cmp(word, "Key") )
			{
				long key_vnum = fread_number(fp);

				OBJ_INDEX_DATA *key = get_obj_index(key_vnum);
				if( key )
				{
					list_appendlink(ship->special_keys, key);
				}

				fMatch = TRUE;
				break;
			}
			break;

		case 'M':
			KEY("MoveDelay", ship->move_delay, fread_number(fp));
			break;

		case 'N':
			KEYS("Name", ship->name, fread_string(fp));
			break;

		case 'O':
			KEY("Object", ship->ship_object, fread_number(fp));
			break;

		case 'W':
			KEY("Weight", ship->weight, fread_number(fp));
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
	ITERATOR it;
	OBJ_INDEX_DATA *obj;

	fprintf(fp, "#SHIP %ld\n", ship->vnum);

	fprintf(fp, "Name %s~\n", fix_string(ship->name));
	fprintf(fp, "Description %s~\n", fix_string(ship->description));
	fprintf(fp, "Class %d\n", ship->ship_class);
	fprintf(fp, "Flags %d\n", ship->flags);

	if( IS_VALID(ship->blueprint) )
		fprintf(fp, "Blueprint %ld\n", ship->blueprint->vnum);

	if( ship->ship_object > 0 )
		fprintf(fp, "Object %ld\n", ship->ship_object);

	fprintf(fp, "Hit %d\n", ship->hit);
	fprintf(fp, "Guns %d\n", ship->guns);
	fprintf(fp, "Crew %d %d\n", ship->min_crew, ship->max_crew);
	fprintf(fp, "MoveDelay %d\n", ship->move_delay);
	fprintf(fp, "Weight %d\n", ship->weight);
	fprintf(fp, "Capacity %d\n", ship->capacity);
	fprintf(fp, "Armor %d\n", ship->armor);

	iterator_start(&it, ship->special_keys);
	while( (obj = (OBJ_INDEX_DATA *)iterator_nextdata(&it)) )
	{
		if( obj->item_type == ITEM_KEY )
			fprintf(fp, "Key %ld\n", obj->vnum);
	}
	iterator_stop(&it);

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

SHIP_DATA *create_ship(long vnum)
{
	OBJ_DATA *obj;						// Physical ship object
	OBJ_INDEX_DATA *obj_index;			// Ship object index to create
	SHIP_DATA *ship;					// Runtime ship data
	SHIP_INDEX_DATA *ship_index;		// Ship index to create
	INSTANCE *instance;
	ITERATOR it;

	// Verify the ship index exists
	if( !(ship_index = get_ship_index(vnum)) )
		return NULL;

	// Verify the object index exists and is a ship
	if( !(obj_index = get_obj_index(ship_index->ship_object)) )
		return NULL;

	if( obj_index->item_type != ITEM_SHIP )
	{
		char buf[MSL];
		sprintf(buf, "create_ship: attempting to use object (%ld) that is not a ship object for ship (%ld)", obj_index->vnum, ship_index->vnum);
		bug(buf, 0);
		return NULL;
	}

	ship = new_ship();
	if( !IS_VALID(ship) )
		return NULL;

	ship->index = ship_index;

	obj = create_object(obj_index, 0, FALSE);
	if( !IS_VALID(obj) )
	{
		free_ship(ship);
		return NULL;
	}
	ship->ship = obj;
	obj->ship = ship;

	instance = create_instance(ship_index->blueprint);
	if( !IS_VALID(instance) )
	{
		list_remlink(loaded_objects, obj);
		--obj->pIndexData->count;
		free_obj(obj);
		free_ship(ship);
		return NULL;
	}

	if( list_size(ship_index->special_keys) > 0 )
	{
		iterator_start(&it, ship_index->special_keys);
		OBJ_INDEX_DATA *key;
		while( (key = (OBJ_INDEX_DATA *)iterator_nextdata(&it)) )
		{
			SPECIAL_KEY_DATA *sk = new_special_key();

			sk->key_vnum = key->vnum;
			list_appendlink(ship->special_keys, sk);
		}
		iterator_stop(&it);
		instance_apply_specialkeys(instance, ship->special_keys);
	}

	ship->instance = instance;
	instance->ship = ship;

	list_appendlink(loaded_instances, instance);

	ship->ship_type = ship_index->ship_class;
	ship->hit = ship_index->hit;
	ship->ship_flags = ship_index->flags;
	ship->armor = ship_index->armor;

	// Build cannons

	list_appendlink(loaded_ships, ship);
	return ship;
}

void extract_ship(SHIP_DATA *ship)
{
	if( !IS_VALID(ship) ) return;

	list_remlink(loaded_ships, ship);

	extract_obj(ship->ship);
	extract_instance(ship->instance);

	free_ship(ship);
}

bool ship_isowner_player(SHIP_DATA *ship, CHAR_DATA *ch)
{
	if( !IS_VALID(ship) ) return false;

	if( !IS_VALID(ch) ) return false;

	return ( (ship->owner_uid[0] == ch->id[0]) && (ship->owner_uid[1] == ch->id[1]) );
}

void ships_update()
{

}


SHIP_DATA *ship_load(FILE *fp)
{
	return NULL;
}

bool ship_save(FILE *fp, SHIP_DATA *ship)
{
	return true;
}


void do_ships(CHAR_DATA *ch, char *argument)
{
	char arg[MIL];
	SHIP_DATA *ship;

	if( IS_IMMORTAL(ch) )
	{
		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  ships list[ player]\n\r", ch);
			send_to_char("         ships load [vnum] [owner] [name]\n\r", ch);
			// TODO: NPC ship handling
			send_to_char("         ships unload [#]\n\r", ch);
			return;
		}

		argument = one_argument(argument, arg);

		if( !str_prefix(arg, "list") )
		{
			CHAR_DATA *owner = get_player(argument);

			if(!ch->lines)
				send_to_char("{RWARNING:{W Having scrolling off may limit how many ships you can see.{x\n\r", ch);

			int lines = 0;
			bool error = FALSE;
			BUFFER *buffer = new_buf();
			ITERATOR it;
			char buf[MSL];

			iterator_start(&it, loaded_ships);
			while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
			{
				if( owner && ship->owner != owner )
					continue;

				sprintf(buf, "{W%4d{x)  {G%8ld  {x%-30.30s   {x%s{x\n\r",
					++lines,
					ship->index->vnum,
					ship->ship_name,
					ship->owner ? ship->owner->name : "{DNone");

				if( !add_buf(buffer, buf) || (!ch->lines && strlen(buf_string(buffer)) > MAX_STRING_LENGTH) )
				{
					error = TRUE;
					break;
				}
			}
			iterator_stop(&it);


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
					send_to_char("{Y      [  Vnum  ] [             Name             ]  Owner{x\n\r", ch);
					send_to_char("{Y======================================================================={x\n\r", ch);
				}

				page_to_char(buffer->string, ch);
			}
			free_buf(buffer);
		}
		else if( !str_prefix(arg, "load") )
		{
			char buf[2*MSL];
			char arg2[MIL];
			char arg3[MIL];
			long vnum;

			argument = one_argument(argument, arg2);
			argument = one_argument(argument, arg3);

			if( !is_number(arg2) )
			{
				send_to_char("That is not a number.\n\r", ch);
				return;
			}

			vnum = atol(arg2);

			SHIP_INDEX_DATA *index = get_ship_index(vnum);

			if( !index )
			{
				send_to_char("That ship does not exist.\n\r", ch);
				return;
			}

			if( index->ship_class == SHIP_SAILING_BOAT )
			{
				if( !IS_WILDERNESS(ch->in_room) )
				{
					send_to_char("Must be in the wilderness.\n\r", ch);
					return;
				}

				if( ch->in_room->sector_type != SECT_WATER_SWIM &&
					ch->in_room->sector_type != SECT_WATER_NOSWIM )
				{
					send_to_char("Must be in the water.\n\r", ch);
					return;
				}
			}
			else if( index->ship_class == SHIP_AIR_SHIP )
			{
				if( !IS_OUTSIDE(ch) )
				{
					send_to_char("Must be outside.\n\r", ch);
					return;
				}
			}

			CHAR_DATA *owner = get_player(arg3);

			if( owner )
			{
				sprintf(buf, "Ship Owner: %s\n\r", owner->name);
			}
			else
			{
				sprintf(buf, "Ship Owner: '%s' not found\n\r", arg3);
			}
			send_to_char(buf, ch);

			ship = create_ship(vnum);

			if( !IS_VALID(ship) )
			{
				send_to_char("Failed to create ship.\n\r", ch);
				return;
			}

			ship->owner = owner;

			free_string(ship->ship_name);
			ship->ship_name = str_dup(argument);

			// Install ship_name
			free_string(ship->ship->name);
			sprintf(buf, ship->ship->pIndexData->name, ship->ship_name);
			ship->ship->name = str_dup(buf);

			free_string(ship->ship->short_descr);
			sprintf(buf, ship->ship->pIndexData->short_descr, ship->ship_name);
			ship->ship->short_descr = str_dup(buf);

			free_string(ship->ship->description);
			sprintf(buf, ship->ship->pIndexData->description, ship->ship_name);
			ship->ship->description = str_dup(buf);

			obj_to_room(ship->ship, ch->in_room);
			act("$p splashes down after being christened '$T'.",ch, NULL, NULL,ship->ship, NULL, NULL,ship->ship_name,TO_ALL);
		}
		else if( !str_prefix(arg, "unload") )
		{
			// Require that the ship being unloaded is not a special ship:
			//   Endeavour
			//   Goblin Airship
			//


		}

		return;
	}
	else
	{
		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  ships list\n\r", ch);
			return;
		}

		argument = one_argument(argument, arg);

		return;
	}


	do_ships(ch, "");
}





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
	{ "armor",				shedit_armor		},
	{ "blueprint",			shedit_blueprint	},
	{ "capacity",			shedit_capacity		},
	{ "class",				shedit_class		},
	{ "commands",			show_commands		},
	{ "create",				shedit_create		},
	{ "crew",				shedit_crew			},
	{ "desc",				shedit_desc			},
	{ "flags",				shedit_flags		},
	{ "guns",				shedit_guns			},
	{ "hit",				shedit_hit			},
	{ "keys",				shedit_keys			},
	{ "list",				shedit_list			},
	{ "movedelay",			shedit_move_delay	},
	{ "name",				shedit_name			},
	{ "object",				shedit_object		},
	{ "show",				shedit_show			},
	{ "weight",				shedit_weight		},
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

	sprintf(buf, "Flags:       [%s]\n\r", flag_string(ship_flags, ship->flags));
	add_buf(buffer, buf);

	if( IS_VALID(ship->blueprint) )
		sprintf(buf, "Blueprint:   [%5ld] %s{x\n\r", ship->blueprint->vnum, ship->blueprint->name);
	else
		sprintf(buf, "Blueprint:   {Dunassigned{x\n\r");
	add_buf(buffer, buf);

	OBJ_INDEX_DATA *obj = get_obj_index(ship->ship_object);
	if( obj )
		sprintf(buf, "Ship Object: [%5ld] %s{x\n\r", obj->vnum, obj->short_descr);
	else
		sprintf(buf, "Ship Object: {Dunassigned{x\n\r");
	add_buf(buffer, buf);

	sprintf(buf, "Hit Points:  [%5d]{x\n\r", ship->hit);
	add_buf(buffer, buf);

	sprintf(buf, "Max Guns:    [%5d]{x\n\r", ship->guns);
	add_buf(buffer, buf);

	sprintf(buf, "Min Crew:    [%5d]{x\n\r", ship->min_crew);
	add_buf(buffer, buf);

	sprintf(buf, "Max Crew:    [%5d]{x\n\r", ship->max_crew);
	add_buf(buffer, buf);

	sprintf(buf, "Move Delay:  [%5d]{x\n\r", ship->move_delay);
	add_buf(buffer, buf);

	sprintf(buf, "Max Weight:  [%5d]{x\n\r", ship->weight);
	add_buf(buffer, buf);

	sprintf(buf, "Capacity:    [%5d]{x\n\r", ship->capacity);
	add_buf(buffer, buf);

	sprintf(buf, "Base Armor:  [%5d]{x\n\r", ship->armor);
	add_buf(buffer, buf);

	add_buf(buffer, "Description:\n\r");
	add_buf(buffer, fix_string(ship->description));
	add_buf(buffer, "\n\r\n\r");

	add_buf(buffer, "Special Keys:\n\r");
	if( list_size(ship->special_keys) > 0 )
	{
		ITERATOR it;
		OBJ_INDEX_DATA *key;
		int count = 0;

		add_buf(buffer, "    [  Vnum  ]  Name\n\r");
		add_buf(buffer, "==============================================\n\r");

		iterator_start(&it, ship->special_keys);
		while( (key = (OBJ_INDEX_DATA *)iterator_nextdata(&it)) )
		{
			char key_color = 'Y';

			if( key->item_type != ITEM_KEY )
			{
				key_color = 'R';
			}


			sprintf(buf, "{W%3d  {G%8ld  {%c%s{x\n\r", ++count, key->vnum, key_color, key->short_descr);
			add_buf(buffer, buf);
		}
		iterator_stop(&it);

		add_buf(buffer, "==============================================\n\r");
		add_buf(buffer, "{RRED{x = not a key.\n\r");
	}
	else
	{
		add_buf(buffer, "  None\n\r");
	}

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

SHEDIT( shedit_flags)
{
	SHIP_INDEX_DATA *ship;
	int value;

	EDIT_SHIP(ch, ship);

	value = flag_value(ship_flags, argument);
	if( value == NO_FLAG )
	{
		send_to_char("Syntax:  flags [flags]\n\r", ch);
		send_to_char("See '? ship' for list of flags.\n\r\n\r", ch);
		show_help(ch, "ship");
		return FALSE;
	}

	ship->flags ^= value;
	send_to_char("Ship flags changed.\n\r", ch);
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

SHEDIT( shedit_object )
{
	SHIP_INDEX_DATA *ship;
	OBJ_INDEX_DATA *obj;
	long vnum;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  object [vnum]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	vnum = atol(argument);
	obj = get_obj_index(vnum);
	if( !obj )
	{
		send_to_char("That object does not exist.\n\r", ch);
		return FALSE;
	}

	if( obj->item_type != ITEM_SHIP )
	{
		send_to_char("Object is not a ship.\n\r", ch);
		return FALSE;
	}

	ship->ship_object = vnum;
	send_to_char("Ship object set.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_hit )
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  hit [points]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int value = atoi(argument);
	if( value < 1 || value > SHIP_MAX_HIT )
	{
		send_to_char("Hit points must be in the range of 1 to " __STR(SHIP_MAX_HIT) ".\n\r", ch);
		return FALSE;
	}

	ship->hit = value;
	send_to_char("Ship hit points changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_guns )
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  guns [count]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int value = atoi(argument);
	if( value < 0 || value > SHIP_MAX_GUNS )
	{
		send_to_char("Gun allowance must be in the range of 0 to " __STR(SHIP_MAX_GUNS) ".\n\r", ch);
		return FALSE;
	}

	ship->guns = value;
	send_to_char("Ship gun allowance changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_crew )
{
	SHIP_INDEX_DATA *ship;
	char arg[MIL];

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  crew [min] [max]\n\r", ch);
		return FALSE;
	}

	argument = one_argument(argument, arg);

	if( !is_number(arg) ||  !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int min_crew = atoi(arg);
	int max_crew = atoi(argument);

	if( max_crew < min_crew )
	{
		int value = min_crew;
		min_crew = max_crew;
		max_crew = value;
	}

	if( min_crew < 0 || min_crew > SHIP_MAX_CREW )
	{
		send_to_char("Minimum crew allowance must be in the range of 0 to " __STR(SHIP_MAX_CREW) ".\n\r", ch);
		return FALSE;
	}

	if( max_crew < 0 || max_crew > SHIP_MAX_CREW )
	{
		send_to_char("Maximum crew allowance must be in the range of 0 to " __STR(SHIP_MAX_CREW) ".\n\r", ch);
		return FALSE;
	}

	ship->min_crew = min_crew;
	ship->max_crew = max_crew;
	send_to_char("Ship crew allowance changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_move_delay )
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  movedelay [count]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int value = atoi(argument);
	if( value < SHIP_MIN_DELAY )
	{
		send_to_char("Move delay must be at least " __STR(SHIP_MIN_DELAY) ".\n\r", ch);
		return FALSE;
	}

	ship->move_delay = value;
	send_to_char("Ship move delay changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_weight )
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  weight [weight]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int value = atoi(argument);
	if( value < 0 || value > SHIP_MAX_WEIGHT )
	{
		send_to_char("Weight allowance must be in the range of 0 to " __STR(SHIP_MAX_WEIGHT) ".\n\r", ch);
		return FALSE;
	}

	ship->weight = value;
	send_to_char("Ship weight allowance changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_capacity )
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  capacity [count]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int value = atoi(argument);
	if( value < 0 || value > SHIP_MAX_CAPACITY )
	{
		send_to_char("Ship capacity must be in the range of 0 to " __STR(SHIP_MAX_CAPACITY) ".\n\r", ch);
		return FALSE;
	}

	ship->capacity = value;
	send_to_char("Ship capacity changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_armor)
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  armor [rating]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int value = atoi(argument);
	if( value < 0 || value > SHIP_MAX_ARMOR )
	{
		send_to_char("Ship base armor must be in the range of 0 to " __STR(SHIP_MAX_ARMOR) ".\n\r", ch);
		return FALSE;
	}

	ship->armor = value;
	send_to_char("Ship base armor changed.\n\r", ch);
	return TRUE;
}


SHEDIT( shedit_keys )
{
	SHIP_INDEX_DATA *ship;
	char arg[MIL];

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  keys list\n\r", ch);
		send_to_char("Syntax:  keys add <vnum>\n\r", ch);
		send_to_char("Syntax:  keys remove <#>\n\r", ch);
		return FALSE;
	}

	argument = one_argument(argument, arg);

	if( !str_cmp(arg, "list") )
	{
		if( list_size(ship->special_keys) > 0 )
		{
			ITERATOR it;
			OBJ_INDEX_DATA *key;
			BUFFER *buffer = new_buf();
			char buf[MSL];
			int count = 0;

			add_buf(buffer, "    [  Vnum  ]  Name\n\r");
			add_buf(buffer, "==============================================\n\r");

			iterator_start(&it, ship->special_keys);
			while( (key = (OBJ_INDEX_DATA *)iterator_nextdata(&it)) )
			{
				char key_color = 'Y';

				if( key->item_type != ITEM_KEY )
				{
					key_color = 'R';
				}

				sprintf(buf, "{W%3d  {G%8ld  {%c%s{x\n\r", ++count, key->vnum, key_color, key->short_descr);
				add_buf(buffer, buf);
			}
			iterator_stop(&it);

			add_buf(buffer, "==============================================\n\r");
			add_buf(buffer, "{RRED{x = not a key.\n\r");

			if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
			{
				send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
			}
			else
			{
				page_to_char(buffer->string, ch);
			}

			free_buf(buffer);
		}
		else
		{
			send_to_char("No special keys to display.\n\r", ch);
		}

		return FALSE;
	}

	if( !str_cmp(arg, "add") )
	{
		OBJ_INDEX_DATA *key;
		long vnum;

		if( !is_number(argument) )
		{
			send_to_char("That is not a number,\n\r", ch);
			return FALSE;
		}

		vnum = atol(argument);
		if( !(key = get_obj_index(vnum)) )
		{
			send_to_char("That object does not exist.\n\r", ch);
			return FALSE;
		}

		if( key->item_type != ITEM_KEY )
		{
			send_to_char("That is not a key.\n\r", ch);
			return FALSE;
		}

		if( list_hasdata(ship->special_keys, key) )
		{
			send_to_char("That key is already in the list.\n\r", ch);
			return FALSE;
		}

		list_appendlink(ship->special_keys, key);
		send_to_char("Key added.\n\r", ch);
		return TRUE;
	}

	if( !str_cmp(arg, "remove") )
	{
		if( !is_number(argument) )
		{
			send_to_char("That is not a number,\n\r", ch);
			return FALSE;
		}

		int value = atoi(argument);
		if( value < 0 || value > list_size(ship->special_keys) )
		{
			send_to_char("Index out of range.\n\r", ch);
			return FALSE;
		}

		list_remnthlink(ship->special_keys, value);
		send_to_char("Key removed.\n\r", ch);
		return TRUE;
	}

	shedit_keys(ch, "");
	return FALSE;

}


/////////////////////////////////////////////////////////////////
//
// NPC SHip Edit
//
