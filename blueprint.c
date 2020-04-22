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

bool blueprints_changed = FALSE;
long top_blueprint_section_vnum = 0;

void fix_blueprint_section(BLUEPRINT_SECTION *bs)
{
	for(BLUEPRINT_LINK *bl = bs->links; bl; bl = bl->next)
	{
		if( bl->vnum < bs->lower_vnum || bl->vnum > bs->upper_vnum )
			continue;

		if( bl->vnum > 0 && bl->door >= 0 && bl->door < MAX_DIR )
		{
			bl->room = get_room_index(bl->vnum);

			if( bl->room )
				bl->ex = bl->room->exit[bl->door];
		}
	}
}

BLUEPRINT_LINK *load_blueprint_link(FILE *fp)
{
	BLUEPRINT_LINK *link;
	char *word;
	bool fMatch;

	link = new_blueprint_link();

	while (str_cmp((word = fread_word(fp)), "#-LINK"))
	{
		fMatch = FALSE;
		switch(word[0])
		{
		case 'D':
			KEY("Door", link->door, fread_number(fp));
			break;

		case 'N':
			KEYS("Name", link->name, fread_string(fp));
			break;

		case 'R':
			KEY("Room", link->room, fread_number(fp));
			break;
		}

		if (!fMatch) {
			sprintf(buf, "load_blueprint_link: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return bl;
}

BLUEPRINT_SECTION *load_blueprint_section(FILE *fp)
{
	BLUEPRINT_SECTION *bs;
	char *word;
	bool fMatch;

	bs = new_blueprint_section();
	bs->vnum = fread_number(fp);

	if( bs->vnum > top_blueprint_section_vnum)
		top_blueprint_section_vnum = bs->vnum;

	while (str_cmp((word = fread_word(fp)), "#-SECTION"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
		case '#':
			if( !str_cmp(word, "#LINK") )
			{
				BLUEPRINT_LINK *link = load_blueprint_link(fp);

				link->next = bs->links;
				bs->links = link;
				fMatch = TRUE;
				break;
			}
			break;

		case 'C':
			KEYS("Comments", bs->comments, fread_string(fp));
			break;

		case 'D':
			KEYS("Description", bs->description, fread_string(fp));
			break;

		case 'F':
			KEY("Flags", bs->flags, fread_number(fp));
			break;

		case 'L':
			KEY("Lower", bs->lower_vnum, fread_number(fp));
			break;

		case 'N':
			KEYS("Name", bs->name, fread_string(fp));
			break;

		case 'R':
			KEY("Recall", bs->recall, fread_number(fp));
			break;

		case 'T':
			KEY("Type", bs->type, fread_number(fp));
			break;

		case 'U':
			KEY("Upper", bs->upper_vnum, fread_number(fp));
			break;
		}


		if (!fMatch) {
			sprintf(buf, "load_blueprint_section: no match for word %s", word);
			bug(buf, 0);
		}
	}

	fix_blueprint_section(bs);
	return bs;
}


// load blueprints
// CALLED AFTER ALL AREAS ARE LOADED
void load_blueprints()
{
	FILE *fp = fopen(BLUEPRINTS_FILE, "r");
	if (fp == NULL)
	{
		bug("Couldn't load blueprints.dat", 0);
		return;
	}
	char *word;
	bool fMatch;

	while (str_cmp((word = fread_word(fp)), "#END"))
	{
		fMatch = FALSE;

		if( !str_cmp(word, "#SECTION") )
		{
			BLUEPRINT_SECTION *bs = load_blueprint_section(fp);
			int iHash = bs->vnum % MAX_KEY_HASH;

			bs->next = blueprint_section_hash[iHash];
			blueprint_section_hash[iHash] = bs;

			fMatch = TRUE;
		}

		if (!fMatch) {
			sprintf(buf, "load_blueprints: no match for word %s", word);
			bug(buf, 0);
		}

	}

	fclose(fp);
}

void save_blueprint_section(FILE *fp, BLUEPRINT_SECTION *bs)
{
	fprintf(fp, "#SECTION %ld\n\r", bs->vnum);
	fprintf(fp, "Name %s~\n\r", fix_string(bs->name));
	fprintf(fp, "Description %s~\n\r", fix_string(bs->description));
	fprintf(fp, "Comments %s~\n\r", fix_string(bs->comments));

	fprintf(fp, "Type %d\n\r", bs->type);
	fprintf(fp, "Flags %d\n\r", bs->flags);

	fprintf(fp, "Recall %ld\n\r", bs->recall);
	fprintf(fp, "Lower %ld\n\r", bs->lower_vnum);
	fprintf(fp, "Upper %ld\n\r", bs->upper_vnum);

	for(BLUEPRINT_LINK *bl = bs->links; bl; bl = bl->next)
	{
		if( valid_section_link(bl) )
		{
			fprintf(fp, "#LINK\n\r");
			fprintf(fp, "Name %s~\n\r", fix_string(bl->name));
			fprintf(fp, "Room %ld\n\r", bl->vnum);
			fprintf(fp, "Door %ld\n\r", bl->door);
			fprintf(fp, "#-LINK\n\r");
		}
	}

	fprintf(fp, "#-SECTION\n\r\n\r");
}

/*
void save_static_blueprint(FILE *fp, STATIC_BLUEPRINT *bp)
{

	fprintf(fp, "#STATIC %ld\n\r", bp->vnum);
	fprintf(fp, "Name %s~\n\r", fix_string(bp->name));
	fprintf(fp, "Description %s~\n\r", fix_string(bp->description));
	fprintf(fp, "Comments %s~\n\r", fix_string(bp->comments));
	fprintf(fp, "Recall %ld\n\r", bp->recall);

	ITERATOR sit;
	BLUEPRINT_SECTION *bs;
	iterator_start(&sit, bp->sections);
	while( (bs = (BLUEPRINT_SECTION *)iterator_nextdata(&sit)) )
	{
		fprintf(fp, "Section %ld\n\r", bs->vnum);
	}
	iterator_stop(&sit);


	for(STATIC_BLUEPRINT_LINK *sbl = bp->links; sbl; sbl = sbl->next)
	{
		if( valid_static_link(sbl) )
		{
			fprintf(fp, "Link %ld %d %ld %d\n\r",
				sbl->section1->vnum, sbl->link1,
				sbl->section2->vnum, sbl->link2);
		}
	}

	fprintf(fp, "#-STATIC\n\r\n\r");
}
*/

// save blueprints
bool save_blueprints()
{
	FILE *fp = fopen(BLUEPRINTS_FILE, "w");
	if (fp == NULL)
	{
		bug("Couldn't save blueprints.dat", 0);
		return FALSE;
	}

	int iHash;
	for(int iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(BLUEPRINT_SECTIONS *bs = blueprint_section_hash[iHash]; bs; bs = bs->next)
		{
			save_blueprint_section(fp, bs);
		}
	}

	fprintf(fp, "#END\n\r");

	fclose(fp);

	blueprints_changed = FALSE;
	return TRUE;
}

bool valid_section_link(BLUEPRINT_LINK *bl)
{
	if( !IS_VALID(bl) ) return FALSE;

	if( bl->vnum <= 0 ) return FALSE;

	if( bl->door < 0 || bl->door >= MAX_DIR ) return FALSE;

	if( !bl->room ) return FALSE;

	EXIT_DATA *ex = bl->room->exit[bl->door];
	if( !IS_VALID(ex) ) return FALSE;

	// Only environment exits can be used as links
	if( !IS_SET(ex->exit_info, EX_ENVIRONMENT) ) return FALSE;

	return TRUE;
}

BLUEPRINT_LINK *get_section_link(BLUEPRINT_SECTION *bs, int link)
{
	if( !IS_VALID(bs) ) return NULL;

	if( link < 0 ) return NULL;

	for(BLUEPRINT_LINK *bl = bs->links; bl; bl = bl->next)
	{
		if( !link-- )
			return bl;
	}

	return NULL;
}

bool valid_static_link(STATIC_BLUEPRINK_LINK *sbl)
{
	if( !IS_VALID(sbl) ) return FALSE;

	BLUEPRINT_LINK *link1 = get_section_link(sbl->section1, sbl->link1);
	if( !valid_section_link(link1) ) return FALSE;

	BLUEPRINT_LINK *link2 = get_section_link(sbl->section2, sbl->link2);
	if( !valid_section_link(link2) ) return FALSE;

	// Only allow links that are reverse directions to link
	if( rev_dir[link1->door] != link2->door ) return FALSE;

	return TRUE;
}

BLUEPRINT_SECTION *get_blueprint_section(long vnum)
{
	int iHash = vnum % MAX_KEY_HASH;

	for(BLUEPRINT_SECTION *bs = blueprint_section_hash[iHash]; bs; bs = bs->next)
	{
		if( bs->vnum == vnum )
			return bs;
	}

	return NULL;
}

// create instance

// extract instance


///////////////////////////////
//
// OLC Editor
//

const struct olc_cmd_type bsedit_table[] =
{
	{ "?",				show_help			},
	{ "list",			bsedit_list			},
	{ "show",			bsedit_show			},
	{ "create",			bsedit_create		},
	{ "name",			bsedit_name			},
	{ "description",	bsedit_description	},
	{ "comments",		bsedit_comments		},
	{ "type",			bsedit_type			},
	{ "flags",			bsedit_flags		},
	{ "recall",			bsedit_recall		},
	{ "rooms",			bsedit_rooms		},
	{ "link",			bsedit_link			},
	{ NULL,				NULL				}

};

bool can_edit_blueprints(CHAR_DATA *ch)
{
	return ch->pcdata->security >= 9;
}

void list_blueprint_sections(CHAR_DATA *ch, char *argument)
{
	if( !can_edit_blueprints(ch) )
	{
		send_to_char("You do not have access to blueprints.\n\r", ch);
		return;
	}

    if(!ch->lines)
		send_to_char("{RWARNING:{W Having scrolling off may limit how many sections you can see.{x\n\r", ch);

	int lines = 0;
	bool error = FALSE;
	BUFFER *buffer = new_buf();
	char buf[MSL];

	for(long vnum = 1; vnum < top_blueprint_section_vnum; vnum++)
	{
		BLUEPRINT_SECTION *section = get_blueprint_section(vnum);

		if( section )
		{
			sprintf(buf, "{Y[{W%5ld{Y] {x%-30.30s {G%-16.16s{x %11ld %11ld %11ld\n\r",
				vnum,
				section->name,
				flag_string(blueprint_section_types, section->type),
				section->recall,
				section->lower_vnum,
				section->upper_vnum);
			add_buf(buffer, buf);
		}
	}

	if( error )
	{
		send_to_char("Too many blueprints to list.  Please shorten!\n\r", ch);
	}
	else
	{
		if( !lines )
		{
			add_buf( buffer, "No blueprint sections to display.\n\r" );
		}
		else
		{
			// Header
			send_to_char("{Y Vnum   [            Name            ] [     Type     ] [ Recall  ] [   Room Vnum Range   ]{x\n\r", ch);
			send_to_char("{Y==========================================================================================={x\n\r", ch);
		}

		page_to_char(buffer->string, ch);
    }
    free_buf(buffer);
}

void do_bslist(CHAR_DATA *ch, char *argument)
{
	list_blueprint_sections(ch, argument);
}

void do_bsedit(CHAR_DATA *ch, char *argument)
{
	BLUEPRINT_SECTION *bs;
	long value;
	char arg1[MAX_STRING_LENGTH];

	argument = one_argument(argument, arg1);

	if (IS_NPC(ch))
		return;

	if (is_number(arg1))
	{
		value = atol(arg1);
		if (!(bs = get_blueprint_section(value)))
		{
			send_to_char("BSEdit:  That vnum does not exist.\n\r", ch);
			return;
		}

		if (!can_edit_blueprints(ch))
		{
			send_to_char("BSEdit:  Insufficient security to edit blueprint sections.\n\r", ch);
			return;
		}

		ch->pcdata->immortal->last_olc_command = current_time;
		ch->desc->pEdit = (void *)pMob;
		ch->desc->editor = ED_MOBILE;
		return;
	}
	else
	{
		if (!str_cmp(arg1, "create"))
		{
			if (bsedit_create(ch, argument))
			{
				blueprints_changed = TRUE;
				ch->desc->editor = ED_BPSECT;
			}
		}

		return;
	}

	send_to_char("BSEdit:  There is no default blueprint section to edit.\n\r", ch);
}


void bsedit(CHAR_DATA *ch, char *argument)
{
	BLUEPRINT_SECTION *bs;
	char command[MAX_INPUT_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	int  cmd;

	EDIT_BPSECT(ch, bs);
	smash_tilde(argument);
	strcpy(arg, argument);
	argument = one_argument(argument, command);

	if (!can_edit_blueprints(ch))
	{
		send_to_char("BSEdit:  Insufficient security to edit blueprint sections.\n\r", ch);
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
		bsedit_show(ch, argument);
		return;
	}

	for (cmd = 0; bsedit_table[cmd].name != NULL; cmd++)
	{
		if (!str_prefix(command, bsedit_table[cmd].name))
		{
			if ((*bsedit_table[cmd].olc_fun) (ch, argument))
			{
				blueprints_changed = TRUE;
				return;
			}
			else
				return;
		}
	}

    interpret(ch, arg);
}

BSEDIT( bsedit_list )
{
	list_blueprint_sections(ch, argument);
}

BSEDIT( bsedit_show )
{
	BLUEPRINT_SECTION *bs;
	BUFFER *buffer;
	char buf[MSL];

	EDIT_BPSECT(ch, bs);

	sprintf(buf, "Name:        [%5ld] %s\n\r", bs->vnum, bs->name);
	add_buf(buffer, buf);

	sprintf(buf, "Type:        %s\n\r", flag_string(blueprint_section_types, bs->type));
	add_buf(buffer, buf);

	sprintf(buf, "Flags:       %s\n\r", flag_string(blueprint_section_flags, bs->flags));
	add_buf(buffer, buf);

	if( bs->recall > 0 )
	{
		ROOM_INDEX_DATA *recall_room = get_room_index(bs->recall);
		sprintf(buf, "Recall:      [%5ld] %s\n\r", bs->recall, room ? room->name : "-invalid-");
	}
	else
	{
		sprintf(buf, "Recall:      no recall defined\n\r");
	}
	add_buf(buffer, buf);

	sprintf(buf, "Lower Vnum:  %ld\n\r", bs->lower_vnum);
	add_buf(buffer, buf);

	sprintf(buf, "Upper Vnum:  %ld\n\r", bs->upper_vnum);
	add_buf(buffer, buf);

	add_buf(buffer, "Description:\n\r");
	add_buf(buffer, bs->description);
	add_buf(buffer, "\n\r");

	add_buf(buffer, "\n\r-----\n\r{WBuilders' Comments:{X\n\r");
	add_buf(buffer, bs->comments);
	add_buf(buffer, "\n\r-----\n\r");

	if( bs->links )
	{
		int bli = 0;
		// List links
		add_buf(buffer, "{YSections Links:{x\n\r");
		for(BLUEPRINT_LINK *bl = bs->links; bl; bl = bl->next)
		{
			++bli;
			ROOM_INDEX_DATA *room = bl->room;

			char *door = (bl->door >= 0 && bl->door < MAX_DIR) ? dir_name[bl->door] : "none";
			char excolor = bl->ex ? 'W' : 'D';

			sprintf(buf, " {Y[{W%3d{Y] %c%-9s{x in {Y[{W%5ld{Y]{x %s\n\r", bli, door, bl->vnum, room ? room->name : "nowhere");
			add_buf(buffer, buf);
		}
	}

	page_to_char(buffer->string, ch);
	return FALSE;
}

BSEDIT( bsedit_create )
{
	BLUEPRINT_SECTION *bs;
	long  value;
	int  iHash;
	long auto_vnum = 0;

	value = atol(argument);
	if (argument[0] == '\0' || value == 0)
	{
		long last_vnum = 0;
		for(bs = blueprint_sections; bs; bs->next)
		{
			if( (bs->vnum - last_vnum) > 1 )
			{
				break;
			}

			last_vnum = bs->vnum;
		}
		value = last_vnum + 1;
	}

	bs = new_blueprint_section();
	bs->vnum = value;

	iHash							= bs->vnum % MAX_KEY_HASH;
	bs->next						= blueprint_section_hash[iHash];
	blueprint_section_hash[iHash]	= bs;
	ch->desc->pEdit					= (void *)bs;

	if( bs->vnum > top_blueprint_section_vnum)
		top_blueprint_section_vnum = bs->vnum;

    return TRUE;
}

BSEDIT( bsedit_name )
{
	BLUEPRINT_SECTION *bs;

	EDIT_BPSECT(ch, bs);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  name [string]\n\r", ch);
		return FALSE;
	}

	free_string(bs->name);
	bs->name = str_dup(argument);

	return TRUE;
}

BSEDIT( bsedit_description )
{
	BLUEPRINT_SECTION *bs;

	EDIT_BPSECT(ch, bs);

	if (argument[0] == '\0')
	{
		string_append(ch, &bs->description);
		return TRUE;
	}

	send_to_char("Syntax:  description - line edit\n\r", ch);
	return FALSE;
}

BSEDIT( bsedit_comments )
{
	BLUEPRINT_SECTION *bs;

	EDIT_BPSECT(ch, bs);

	if (argument[0] == '\0')
	{
		string_append(ch, &bs->comments);
		return TRUE;
	}

	send_to_char("Syntax:  comments - line edit\n\r", ch);
	return FALSE;
}

BSEDIT( bsedit_type )
{
	BLUEPRINT_SECTION *bs;
	int value;

	EDIT_BPSECT(ch, bs);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  type <type>\n\r", ch);
		send_to_char("'? section_types' for list of types.\n\r", ch);
		return FALSE;
	}

	if( (value = flag_value(blueprint_section_types, argument)) == NO_FLAG )
	{
		send_to_char("That is not a valid type.\n\r", ch);
		send_to_char("'? section_types' for list of types.\n\r", ch);
		return FALSE;
	}

	bs->type = value;
	send_to_char("Section type changed.\n\r", ch);
	return TRUE;
}

BSEDIT( bsedit_flags )
{
	BLUEPRINT_SECTION *bs;
	int value;

	EDIT_BPSECT(ch, bs);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  flags <flags>\n\r", ch);
		send_to_char("'? section_flags' for list of flags.\n\r", ch);
		return FALSE;
	}

	if( (value = flag_value(blueprint_section_flags, argument)) == NO_FLAG )
	{
		send_to_char("That is not a valid flag.\n\r", ch);
		send_to_char("'? section_flags' for list of flags.\n\r", ch);
		return FALSE;
	}

	bs->flags ^= flags;
	send_to_char("Section flags changed.\n\r", ch);
	return TRUE;
}


BSEDIT( bsedit_recall )
{
	BLUEPRINT_SECTION *bs;
	ROOM_INDEX_DATA *room;
	long vnum;
	char buf[MSL];

	EDIT_BPSECT(ch, bs);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  recall [vnum]\n\r", ch);
		send_to_char("         recall none\n\r", ch);
		return FALSE;
	}

	if( !str_cmp(argument, "none") )
	{
		if( bs->recall < 1 )
		{
			send_to_char("Recall was not defined.\n\r", ch);
			return FALSE;
		}

		bs->recall = 0;

		send_to_char("Recall cleared.\n\r", ch);
		return TRUE;
	}

	if( bs->lower_vnum < 1 || bs->upper_vnum < 1 )
	{
		send_to_char("Vnum range must be set first.\n\r", ch);
		return FALSE;
	}

	if (!is_number(argument))
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	vnum = atol(argument);
	if( vnum <= 0 )
	{
		send_to_char("That room does not exist.\n\r", ch);
		return FALSE;
	}

	if( vnum < bs->lower_vnum || vnum > bs->upper_vnum )
	{
		sprintf(buf, "Value must be a value from %ld to %ld.\n\r", bs->lower_vnum, bs->upper_vnum);
		send_to_char(buf, ch);
		return FALSE;
	}

	room = get_room_index(vnum);
	if( room == NULL )
	{
		send_to_char("That room does not exist.\n\r", ch);
		return FALSE;
	}

	bs->recall = vnum;
	sprintf(buf, "Recall set to %.30s (%ld)\n\r", room->name, vnum);
	send_to_char(buf, ch);
	return TRUE;
}

bool validate_vnum_range(CHAR_DATA *ch, long lower, long upper)
{
	char buf[MSL];

	// Check the range spans just one area.
	if( !check_range(lower, upper) )
	{
		send_to_char("Vnums must be in the same area.\n\r", ch);
		return FALSE;
	}

	// Verify there are any rooms in the range
	bool found = FALSE;
	bool valid = TRUE;
	BUFFER *buffer = new_buf();		// This will buffer up ALL the problem rooms

	for(long vnum = lower; vnum <= upper; vnum++)
	{
		ROOM_INDEX_DATA *room = get_room_index(vnum);

		if( room )
		{
			found = TRUE;

			// Verify the room does not have non-environment exits pointing OUT of the range of vnums
			for( int i = 0; i < MAX_DIR; i++ )
			{
				EXIT_DATA *ex = room->exit[i];

				if( (ex != NULL) &&
					!IS_SET(ex->exit_info, EX_ENVIRONMENT) &&
					(ex->u1.to_room != NULL) )
				{
					if( ex->u1.to_room->vnum < lower || ex->u1.to_room->vnum > upper )
					{
						sprintf(buf, "{xRoom {W%ld{x has an exit ({W%s{x) leading outside of the vnum range.\n\r", ex->u1.to_room->vnum. dir_names[i]);
						add_buf(buffer, buf);
						valid = FALSE;
					}
				}
			}

		}
	}

	if( !found )
	{
		send_to_char("There are no rooms in that range.\n\r", ch);
	}
	else if( !valid )
	{
		page_to_char(buffer->string, ch);
	}

	free_buf(buffer);
	return found && valid;
}

BSEDIT( bsedit_rooms )
{
	BLUEPRINT_SECTION *bs;
	ROOM_INDEX_DATA *room;
	long lvnum, uvnum;
	char buf[MSL];
	char arg[MIL];
	AREA_DATA *larea, *uarea;

	EDIT_BPSECT(ch, bs);

	argument = one_argument(argument, arg);

	if( arg[0] == '\0' || argument[0] == '\0' )
	{
		send_to_char("Syntax:  rooms [lower vnum] [upper vnum]\n\r", ch);
		send_to_char("{YVnums must be in the same area.{x\n\r", ch);
		return FALSE;
	}

	if( !is_number(arg) || !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	lvnum = atol(arg);
	uvnum = atol(argument);

	// Silently swap the bounds if necessary, don't be annoying
	if( uvum < lvnum )
	{
		long vnum = lvnum;
		lvnum = uvnum;
		uvnum = lvnum;
	}

	if( validate_vnum_range(lvnum, uvnum) )
	{
		bs->lower_vnum = lvnum;
		bs->upper_vnum = uvnum;

		send_to_char("Vnum range set.\n\r", ch);

		// Make sure recall point is still inside room range
		if( bs->recall > 0 )
		{
			if( bs->recall < lvnum || bs->recall > uvnum )
			{
				send_to_char("{YRecall room outside of new range.  Clearing.{x\n\r", ch);
				bs->recall = 0;
			}
		}

		// Make sure link are still inside room range
		if( bs->links )
		{
			BLUEPRINT_LINK *prev = NULL, *cur, *next;

			for(cur = bs->links; cur; cur = next)
			{
				next = cur->next;

				if( cur->vnum < lvnum || cur->vnum > uvnum )
				{
					sprintf(buf, "Link %.30s outside of new vnum range.  Removing.\n\r", cur->name);
					send_to_char(buf, ch);

					if( prev )
						prev->next = next;
					else
						bs->links = next;

					free_blueprint_link(cur);
				}
				else
				{
					prev = cur;
				}
			}

		}

		return TRUE;
	}

	return FALSE;
}

BSEDIT( bsedit_link )
{
	BLUEPRINT_SECTION *bs;
	BLUEPRINT_LINK *link;
	char arg[MIL];
	char arg2[MIL];

	EDIT_BPSECT(ch, bs);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  link list\n\r", ch);
		send_to_char("         link add <vnum> <door>\n\r", ch);
		send_to_char("         link # delete\n\r", ch);
		send_to_char("         link # name <name>\n\r", ch);
		send_to_char("         link # room <vnum>\n\r", ch);
		send_to_char("         link # exit <door>\n\r", ch);
		return FALSE;
	}

	argument = one_argument(argument, arg);
	argument = one_argument(argument, arg2);

	if( !str_cmp(arg, "list" ) )
	{
		if( bs->links )
		{
			int bli = 0;
			// List links
			send_to_char("{YSections Links:{x\n\r", ch);
			for(BLUEPRINT_LINK *bl = bs->links; bl; bl = bl->next)
			{
				++bli;
				ROOM_INDEX_DATA *room = bl->room;

				char *door = (bl->door >= 0 && bl->door < MAX_DIR) ? dir_name[bl->door] : "none";
				char excolor = bl->ex ? 'W' : 'D';

				sprintf(buf, " {Y[{W%3d{Y] {G%-30.30s %c%-9s{x in {Y[{W%5ld{Y]{x %s\n\r", bli, bl->name, door, bl->vnum, room ? room->name : "nowhere");
				send_to_char(buf, ch);
			}
		}
		else
		{
			send_to_char("No links defined.\n\r", ch);
		}

		return FALSE;
	}

	if( !str_cmp(arg, "add") )
	{
		if( bs->lower_vnum < 1 || bs->upper_vnum < 1 )
		{
			send_to_char("Vnum range must be set first.\n\r", ch);
			return FALSE;
		}

		if( !is_number(arg2) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return FALSE;
		}

		long vnum = atol(arg2);
		if( vnum < bs->lower_vnum || vnum > bs->upper_vnum )
		{
			send_to_char("Vnum is out of range of blueprint section.\n\r", ch);
			return FALSE;
		}

		ROOM_INDEX_DATA *room = get_room_index(vnum);
		if( !room )
		{
			send_to_char("That room does not exist.\n\r", ch);
			return FALSE;
		}

		bool found = FALSE;
		for( int i = 0; i < MAX_DIR; i++ )
		{
			if( room->exit[i] )
				found = TRUE;
		}

		if( !found )
		{
			send_to_char("That room has no exits.\n\r,", ch);
			return FALSE;
		}

		int door = parse_door(argument);
		if( door < 0 )
		{
			send_to_char("That is an invalid exit.\n\r", ch);
			return FALSE;
		}

		EXIT_DATA *ex = room->exit[door];
		if( !ex )
		{
			send_to_char("That is an invalid exit.\n\r", ch);
			return FALSE;
		}

		if( !IS_SET(ex->exit_flags, EX_ENVIRONMENT) )
		{
			send_to_char("Exit links must be {YENVIRONMENT{x exits.\n\r", ch);
			return FALSE;
		}

		link = new_blueprint_link();
		link->vnum = vnum;
		link->door = door;
		link->room = room;
		link->ex = ex;
		link->next = bs->links;
		bs->links = link;

		send_to_char("Link added.\n\r", ch);
		return TRUE;
	}

	if( !is_number(arg) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int linkno = atoi(arg);
	if( linkno < 1 )
	{
		send_to_char("That is not a section link.\n\r", ch);
		return FALSE;
	}


	if( !str_cmp(arg2, "delete") )
	{
		BLUEPRINT_LINK *prev = NULL;

		for( link = bs->links; link; link = link->next )
		{
			if(!--linkno)
				break;

			prev = link;
		}

		if(!link)
		{
			send_to_char("That is not a section link.\n\r", ch);
			return FALSE;
		}

		if( prev )
			prev->next = link->next;
		else
			bs->links = link->next;

		free_blueprint_link(link);
		send_to_char("Link removed.\n\r", ch);
		return TRUE;
	}

	link = get_section_link(bs, linkno);
	if( !link )
	{
		send_to_char("That is not a section link.\n\r", ch);
		return FALSE;
	}

	if( !str_cmp(arg2, "name") )
	{
		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  link # name <name>\n\r", ch);
			return FALSE;
		}

		free_string(link->name);
		link->name = str_dup(argument);

		send_to_char("Name changed.\n\r", ch);
		return TRUE;
	}

	if( !str_cmp(arg2, "room") )
	{
		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  link # room <vnum>\n\r", ch);
			return FALSE;
		}

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return FALSE;
		}

		long vnum = atol(argument);
		if( vnum < bs->lower_vnum || vnum > bs->upper_vnum )
		{
			send_to_char("Vnum is out of range of blueprint section.\n\r", ch);
			return FALSE;
		}

		ROOM_INDEX_DATA *room = get_room_index(vnum);
		if( !room )
		{
			send_to_char("That room does not exist.\n\r", ch);
			return FALSE;
		}

		bool found = FALSE;
		for( int i = 0; i < MAX_DIR; i++ )
		{
			if( room->exit[i] )
				found = TRUE;
		}

		if( !found )
		{
			send_to_char("That room has no exits.\n\r,", ch);
			return FALSE;
		}

		link->vnum = vnum;
		link->door = -1;
		link->room = room;
		link->ex = NULL;


		send_to_char("Room changed.\n\r", ch);
		return TRUE;
	}

	if( !str_cmp(arg2, "exit") )
	{
		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  link # exit <door>\n\r", ch);
			return FALSE;
		}

		int door = parse_door(argument);
		if( door < 0 )
		{
			send_to_char("That is an invalid exit.\n\r", ch);
			return FALSE;
		}

		EXIT_DATA *ex = link->room->exit[door];
		if( !ex )
		{
			send_to_char("That is an invalid exit.\n\r", ch);
			return FALSE;
		}

		if( !IS_SET(ex->exit_flags, EX_ENVIRONMENT) )
		{
			send_to_char("Exit links must be {YENVIRONMENT{x exits.\n\r", ch);
			return FALSE;
		}

		link->door = door;
		link->ex = ex;
		send_to_char("Exit changed.\n\r", ch);
		return TRUE;
	}

	bsedit_link(ch, "");
	return FALSE;
}

