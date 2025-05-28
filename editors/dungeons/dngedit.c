/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "strings.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../scripts.h"
#include "../../wilds.h"

extern bool dungeons_changed;
extern long top_dungeon_vnum;
extern void list_dungeons(CHAR_DATA *ch, char *argument);



DNGEDIT( dngedit_list )
{
	list_dungeons(ch, argument);
	return false;
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

	if( dng->repop > 0)
		sprintf(buf, "Repop:       %d minutes\n\r", dng->repop);
	else
		sprintf(buf, "Repop:       {Dnever{X\n\r");
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

	add_buf(buffer, "ZoneOut:     ");
	add_buf(buffer, dng->zone_out);
	add_buf(buffer, "{x\n\r");

	add_buf(buffer, "PortalOut:     ");
	add_buf(buffer, dng->zone_out_portal);
	add_buf(buffer, "{x\n\r");

	add_buf(buffer, "MountOut:     ");
	add_buf(buffer, dng->zone_out_mount);
	add_buf(buffer, "{x\n\r");

	add_buf(buffer, "Description:\n\r");
	add_buf(buffer, dng->description);
	add_buf(buffer, "\n\r");

	dngedit_buffer_floors(buffer, dng);

	add_buf(buffer, "Special Rooms:\n\r");
	if( list_size(dng->special_rooms) > 0 )
	{
		BUFFER *buffer = new_buf();
		DUNGEON_INDEX_SPECIAL_ROOM *special;

		char buf[MSL];
		int line = 0;

		ITERATOR sit;

		add_buf(buffer, "     [             Name             ] [ Floor ] [             Room             ]\n\r");
		add_buf(buffer, "---------------------------------------------------------------------------------\n\r");

		iterator_start(&sit, dng->special_rooms);
		while( (special = (DUNGEON_INDEX_SPECIAL_ROOM *)iterator_nextdata(&sit)) )
		{
			BLUEPRINT *blueprint = (BLUEPRINT *)list_nthdata(dng->floors, special->floor);

			if( !IS_VALID(blueprint) || blueprint->mode != BLUEPRINT_MODE_STATIC )
			{
				snprintf(buf, MSL-1, "{W%4d  %-30.30s   {G%7d{x   {D-{Winvalid{D-{x\n\r", ++line, special->name, special->floor);
			}
			else
			{
				BLUEPRINT_SECTION *section = list_nthdata(blueprint->sections, special->section);
				ROOM_INDEX_DATA *room = get_room_index(special->vnum);

				if( !IS_VALID(section) || !room || room->vnum < section->lower_vnum || room->vnum > section->upper_vnum)
				{
					snprintf(buf, MSL-1, "{W%4d  %-30.30s   {G%7d{x   {D-{Winvalid{D-{x\n\r", ++line, special->name, special->floor);
				}
				else
				{
					snprintf(buf, MSL-1, "{W%4d  %-30.30s   {G%7d{x   (%ld) {Y%s{x in (%ld) {Y%s{x\n\r", ++line, special->name, special->floor, room->vnum, room->name, section->vnum, section->name);
				}
			}

			buf[MSL-1] = '\0';
			add_buf(buffer, buf);
		}

		iterator_stop(&sit);
		add_buf(buffer, "---------------------------------------------------------------------------------\n\r");
	}
	else
	{
		add_buf(buffer, "   None\n\r");
	}
	add_buf(buffer, "\n\r");

	add_buf(buffer, "\n\r-----\n\r{WBuilders' Comments:{X\n\r");
	add_buf(buffer, dng->comments);
	add_buf(buffer, "\n\r-----\n\r");


	if (dng->progs) {
		int cnt, slot;

		for (cnt = 0, slot = 0; slot < TRIGSLOT_MAX; slot++)
			if(list_size(dng->progs[slot]) > 0) ++cnt;

		if (cnt > 0) {
			sprintf(buf, "{R%-6s %-20s %-10s %-10s\n\r{x", "Number", "Prog Vnum", "Trigger", "Phrase");
			add_buf(buffer, buf);

			sprintf(buf, "{R%-6s %-20s %-10s %-10s\n\r{x", "------", "-------------", "-------", "------");
			add_buf(buffer, buf);

			ITERATOR it;
			PROG_LIST *trigger;
			for (cnt = 0, slot = 0; slot < TRIGSLOT_MAX; slot++) {
				iterator_start(&it, dng->progs[slot]);
				while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
					sprintf(buf, "{C[{W%4d{C]{x %-20ld %-10s %-6s\n\r", cnt,
						trigger->vnum,trigger_name(trigger->trig_type),
						trigger_phrase_olcshow(trigger->trig_type,trigger->trig_phrase, false, false));
					add_buf(buffer, buf);
					cnt++;
				}
				iterator_stop(&it);
			}
		}
	}

	if (dng->index_vars) {
		pVARIABLE var;
		int cnt;

		for (cnt = 0, var = dng->index_vars; var; var = var->next) ++cnt;

		if (cnt > 0) {
			sprintf(buf, "{R%-20s %-8s %-5s %-10s\n\r{x", "Name", "Type", "Saved", "Value");
			add_buf(buffer, buf);

			sprintf(buf, "{R%-20s %-8s %-5s %-10s\n\r{x", "----", "----", "-----", "-----");
			add_buf(buffer, buf);

			for (var = dng->index_vars; var; var = var->next) {
				switch(var->type) {
				case VAR_INTEGER:
					sprintf(buf, "{x%-20.20s {GNUMBER     {Y%c   {W%d{x\n\r", var->name,var->save?'Y':'N',var->_.i);
					break;
				case VAR_STRING:
				case VAR_STRING_S:
					sprintf(buf, "{x%-20.20s {GSTRING     {Y%c   {W%s{x\n\r", var->name,var->save?'Y':'N',var->_.s?var->_.s:"(empty)");
					break;
				case VAR_ROOM:
					if(var->_.r && var->_.r->vnum > 0)
						sprintf(buf, "{x%-20.20s {GROOM       {Y%c   {W%s {R({W%d{R){x\n\r", var->name,var->save?'Y':'N',var->_.r->name,(int)var->_.r->vnum);
					else
						sprintf(buf, "{x%-20.20s {GROOM       {Y%c   {W-no-where-{x\n\r",var->name,var->save?'Y':'N');
					break;
				default:
					continue;
				}
				add_buf(buffer, buf);
			}
		}
	}

	if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH)
	{
		send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
	}
	else
	{
		page_to_char(buffer->string, ch);
	}

	free_buf(buffer);
	return false;
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
	else if( get_dungeon_index(value) )
	{
		send_to_char("That vnum already exists.\n\r", ch);
		return false;
	}

	dng = new_dungeon_index();
	dng->vnum = value;

	iHash							= dng->vnum % MAX_KEY_HASH;
	dng->next						= dungeon_index_hash[iHash];
	dungeon_index_hash[iHash]	= dng;
	ch->desc->pEdit					= (void *)dng;

	if( dng->vnum > top_dungeon_vnum)
		top_dungeon_vnum = dng->vnum;

    return true;
}

DNGEDIT( dngedit_name )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	smash_tilde(argument);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  name [string]\n\r", ch);
		return false;
	}

	free_string(dng->name);
	dng->name = str_dup(argument);
	send_to_char("Name changed.\n\r", ch);
	return true;
}

DNGEDIT( dngedit_repop )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	if( !is_number(argument) )
	{
		send_to_char("Syntax:  repop [age]\n\r", ch);
		return false;
	}

	int repop = atoi(argument);
	dng->repop = UMAX(0, repop);
	send_to_char("Repop changed.\n\r", ch);
	return true;
}


DNGEDIT( dngedit_description )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	if (argument[0] == '\0')
	{
		string_append(ch, &dng->description);
		return true;
	}

	send_to_char("Syntax:  description - line edit\n\r", ch);
	return false;
}

DNGEDIT( dngedit_comments )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	if (argument[0] == '\0')
	{
		string_append(ch, &dng->comments);
		return true;
	}

	send_to_char("Syntax:  comments - line edit\n\r", ch);
	return false;
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
			return true;
		}

		if ((value = flag_value(area_who_titles, argument)) != NO_FLAG)
		{
			if( value == AREA_INSTANCE || value == AREA_DUTY )
			{
				send_to_char("Area who title only allowed in blueprints.\n\r", ch);
				return false;
			}

			dng->area_who = value;

			send_to_char("Area who title set.\n\r", ch);
			return true;
		}
    }

    send_to_char("Syntax:  areawho [title]\n\r"
				"Type '? areawho' for a list of who titles.\n\r", ch);
    return false;

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
		return false;
	}

	argument = one_argument(argument, arg);

	if( !str_prefix(arg, "list") )
	{
		BUFFER *buffer = new_buf();

		dngedit_buffer_floors(buffer, dng);

		page_to_char(buffer->string, ch);
		free_buf(buffer);
		return false;
	}

	if( !str_prefix(arg, "add") )
	{
		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		long vnum = atol(argument);

		BLUEPRINT *bp = get_blueprint(vnum);

		if( !bp )
		{
			send_to_char("That blueprint does not exist.\n\r", ch);
			return false;
		}

		if( bp->mode == BLUEPRINT_MODE_STATIC )
		{
			if( bp->static_entry_section < 1 || bp->static_entry_link < 1 ||
				bp->static_exit_section < 1 || bp->static_exit_link < 1 )
			{
				send_to_char("Blueprint must have an entrance and exit specified.\n\r", ch);
				return false;
			}
		}
		else
		{
			send_to_char("Blueprint mode not supported yet.\n\r", ch);
			return false;
		}

		list_appendlink(dng->floors, bp);
		send_to_char("Floor added.\n\r", ch);
		return true;
	}

	if( !str_prefix(arg, "remove") || !str_prefix(arg, "delete") )
	{
		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		int index = atoi(argument);

		if( index < 1 || index > list_size(dng->floors) )
		{
			send_to_char("Index out of range.\n\r", ch);
			return false;
		}

		list_remnthlink(dng->floors, index, false);

		ITERATOR it;
		DUNGEON_INDEX_SPECIAL_ROOM *special;
		iterator_start(&it, dng->special_rooms);
		while( (special = (DUNGEON_INDEX_SPECIAL_ROOM *)iterator_nextdata(&it)) )
		{
			if( special->floor == index )
			{
				iterator_remcurrent(&it);
			}
			else if( special->floor > index )
			{
				special->floor--;
			}
		}
		iterator_stop(&it);

		send_to_char("Floor removed.\n\r", ch);
		return true;
	}

	dngedit_floors(ch, "");
	return false;
}

DNGEDIT( dngedit_entry )
{
	DUNGEON_INDEX_DATA *dng;
	long value;

	EDIT_DUNGEON(ch, dng);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  entry <vnum>\n\r", ch);
		return false;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return false;
	}

	value = atol(argument);

	if( !get_room_index(value) )
	{
		send_to_char("That room does not exist.\n\r", ch);
		return false;
	}

	dng->entry_room = value;
	send_to_char("Entry room changed.\n\r", ch);
	return true;
}

DNGEDIT( dngedit_exit )
{
	DUNGEON_INDEX_DATA *dng;
	long value;

	EDIT_DUNGEON(ch, dng);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  exit <vnum>\n\r", ch);
		return false;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return false;
	}

	value = atol(argument);

	if( !get_room_index(value) )
	{
		send_to_char("That room does not exist.\n\r", ch);
		return false;
	}

	dng->exit_room = value;
	send_to_char("Exit room changed.\n\r", ch);
	return true;
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
		return false;
	}

	if( (value = flag_value(dungeon_flags, argument)) != NO_FLAG )
	{
		dng->flags ^= value;
		send_to_char("Dungeon flags changed.\n\r", ch);
		return true;
	}

	dngedit_flags(ch, "");
	return false;

}

DNGEDIT( dngedit_zoneout )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	smash_tilde(argument);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  zoneout [string]\n\r", ch);
		return false;
	}

	free_string(dng->zone_out);
	dng->zone_out = str_dup(argument);
	send_to_char("ZoneOut changed.\n\r", ch);
	return true;
}

DNGEDIT( dngedit_portalout )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	smash_tilde(argument);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  portalout [string]\n\r", ch);
		return false;
	}

	free_string(dng->zone_out_portal);
	dng->zone_out_portal = str_dup(argument);
	send_to_char("PortalOut changed.\n\r", ch);
	return true;
}

DNGEDIT( dngedit_mountout )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	smash_tilde(argument);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  mountout [string]\n\r", ch);
		return false;
	}

	free_string(dng->zone_out_mount);
	dng->zone_out_mount = str_dup(argument);
	send_to_char("MountOut changed.\n\r", ch);
	return true;
}

DNGEDIT( dngedit_special )
{
	DUNGEON_INDEX_DATA *dng;
	char arg1[MIL];
	char arg2[MIL];
	char arg3[MIL];
	char arg4[MIL];

	EDIT_DUNGEON(ch, dng);

	argument = one_argument(argument, arg1);

	if (arg1[0] == '\0')
	{
		send_to_char("Syntax:  special list\n\r", ch);
		send_to_char("         special add [floor] [section] [room vnum] [name]\n\r", ch);
		send_to_char("         special # remove\n\r", ch);
		send_to_char("         special # name [name]\n\r", ch);
		send_to_char("         special # floor [floor]\n\r", ch);
		send_to_char("         special # room [section] [room vnum]\n\r", ch);
		return false;
	}

	if( !str_prefix(arg1, "list") )
	{
		if( list_size(dng->special_rooms) > 0 )
		{
			BUFFER *buffer = new_buf();
			DUNGEON_INDEX_SPECIAL_ROOM *special;

			char buf[MSL];
			int line = 0;

			ITERATOR sit;

			add_buf(buffer, "     [             Name             ] [ Floor ] [             Room             ]\n\r");
			add_buf(buffer, "---------------------------------------------------------------------------------\n\r");

			iterator_start(&sit, dng->special_rooms);
			while( (special = (DUNGEON_INDEX_SPECIAL_ROOM *)iterator_nextdata(&sit)) )
			{
				BLUEPRINT *blueprint = (BLUEPRINT *)list_nthdata(dng->floors, special->floor);

				if( !IS_VALID(blueprint) || blueprint->mode != BLUEPRINT_MODE_STATIC )
				{
					snprintf(buf, MSL-1, "{W%4d  %-30.30s   {G%7d{x   {D-{Winvalid{D-{x\n\r", ++line, special->name, special->floor);
				}
				else
				{
					BLUEPRINT_SECTION *section = list_nthdata(blueprint->sections, special->section);
					ROOM_INDEX_DATA *room = get_room_index(special->vnum);

					if( !IS_VALID(section) || !room || room->vnum < section->lower_vnum || room->vnum > section->upper_vnum)
					{
						snprintf(buf, MSL-1, "{W%4d  %-30.30s   {G%7d{x   {D-{Winvalid{D-{x\n\r", ++line, special->name, special->floor);
					}
					else
					{
						snprintf(buf, MSL-1, "{W%4d  %-30.30s   {G%7d{x   (%ld) {Y%s{x in (%ld) {Y%s{x\n\r", ++line, special->name, special->floor, room->vnum, room->name, section->vnum, section->name);
					}
				}
				buf[MSL-1] = '\0';
				add_buf(buffer, buf);
			}

			iterator_stop(&sit);
			add_buf(buffer, "---------------------------------------------------------------------------------\n\r");

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
			send_to_char("Dungeon has no special rooms defined.\n\r", ch);
		}

		return false;
	}

	if( is_number(arg1) )
	{
		int index = atoi(arg1);

		DUNGEON_INDEX_SPECIAL_ROOM *special = list_nthdata(dng->special_rooms, index);

		if( !IS_VALID(special) )
		{
			send_to_char("No such special room.\n\r", ch);
			return false;
		}

		if( arg2[0] == '\0' )
		{
			dngedit_special(ch, "");
			return false;
		}

		if( !str_prefix(arg2, "remove") || !str_prefix(arg2, "delete") )
		{
			list_remnthlink(dng->special_rooms, index, true);

			send_to_char("Special room deleted.\n\r", ch);
			return true;
		}

		if( !str_prefix(arg2, "floor") )
		{
			if( !is_number(arg3) )
			{
				send_to_char("That is not a number.\n\r", ch);
				return false;
			}

			int floor = atoi(arg3);

			if( floor < 1 || floor > list_size(dng->floors) )
			{
				send_to_char("Floor out of range.\n\r", ch);
				return false;
			}

			special->floor = floor;
			special->section = -1;
			special->vnum = -1;

			send_to_char("Floor changed.\n\r", ch);
			return true;
		}

		if( !str_prefix(arg2, "name") )
		{
			if( IS_NULLSTR(arg3) )
			{
				send_to_char("Syntax:  special # name [name]\n\r", ch);
				return false;
			}

			smash_tilde(arg3);
			free_string(special->name);
			special->name = str_dup(arg3);

			send_to_char("Special room name changed.\n\r", ch);
			return true;
		}

		if( !str_prefix(arg2, "room") )
		{
			if( !is_number(arg3) || !is_number(arg4) )
			{
				send_to_char("That is not a number.\n\r", ch);
				return false;
			}

			int section = atoi(arg3);
			long vnum = atol(arg4);

			BLUEPRINT *bp = list_nthdata(dng->floors, special->floor);

			if( section < 1 || section > list_size(bp->sections) )
			{
				send_to_char("Section number out of range.\n\r", ch);
				return false;
			}

			BLUEPRINT_SECTION *bs = list_nthdata(bp->sections, section);

			if( vnum < bs->lower_vnum || vnum > bs->upper_vnum )
			{
				send_to_char("Room vnum not in the section.\n\r", ch);
				return false;
			}

			if( !get_room_index(vnum) )
			{
				send_to_char("Room does not exist.\n\r", ch);
				return false;
			}

			special->section = section;
			special->vnum = vnum;

			send_to_char("Special room changed.\n\r", ch);
			return true;
		}
	}
	else if( !str_prefix(arg1, "add") )
	{
		argument = one_argument(argument, arg2);
		argument = one_argument(argument, arg3);
		argument = one_argument(argument, arg4);

		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  special add [floor] [section] [room vnum] [name]\n\r", ch);
			return false;
		}

		if( !is_number(arg2) || !is_number(arg3) || !is_number(arg4) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		int floor = atoi(arg2);
		int section = atoi(arg3);
		long vnum = atol(arg4);

		if( floor < 1 || floor > list_size(dng->floors) )
		{
			send_to_char("Floor out of range.\n\r", ch);
			return false;
		}

		BLUEPRINT *bp = list_nthdata(dng->floors, floor);

		if( bp->mode != BLUEPRINT_MODE_STATIC )
		{
			send_to_char("Only STATIC blueprints are supported.\n\r", ch);
			return false;
		}

		if( section < 1 || section > list_size(bp->sections) )
		{
			send_to_char("Section number out of range.\n\r", ch);
			return false;
		}

		BLUEPRINT_SECTION *bs = list_nthdata(bp->sections, section);

		if( vnum < bs->lower_vnum || vnum > bs->upper_vnum )
		{
			send_to_char("Room vnum not in the section.\n\r", ch);
			return false;
		}

		if( !get_room_index(vnum) )
		{
			send_to_char("Room does not exist.\n\r", ch);
			return false;
		}

		char name[MIL+1];
		strncpy(name, argument, MIL);
		name[MIL] = '\0';
		smash_tilde(name);

		DUNGEON_INDEX_SPECIAL_ROOM *special = new_dungeon_index_special_room();

		free_string(special->name);
		special->name = str_dup(name);
		special->floor = floor;
		special->section = section;
		special->vnum = vnum;

		list_appendlink(dng->special_rooms, special);

		send_to_char("Special Room added.\n\r", ch);
		return true;
	}


	dngedit_special(ch, "");
	return false;
}

DNGEDIT (dngedit_adddprog)
{
    int tindex, slot;
	DUNGEON_INDEX_DATA *dungeon;
    PROG_LIST *list;
    SCRIPT_DATA *code;
    char trigger[MAX_STRING_LENGTH];
    char phrase[MAX_STRING_LENGTH];
    char num[MAX_STRING_LENGTH];

    EDIT_DUNGEON(ch, dungeon);
    argument = one_argument(argument, num);
    argument = one_argument(argument, trigger);
    argument = one_argument(argument, phrase);

    if (!is_number(num) || trigger[0] =='\0' || phrase[0] =='\0')
    {
	send_to_char("Syntax:   adddprog [vnum] [trigger] [phrase]\n\r",ch);
	return false;
    }

    if ((tindex = trigger_index(trigger, PRG_DPROG)) < 0) {
	send_to_char("Valid flags are:\n\r",ch);
	show_help(ch, "dprog");
	return false;
    }

    slot = trigger_table[tindex].slot;

    if ((code = get_script_index (atol(num), PRG_DPROG)) == NULL)
    {
	send_to_char("No such DUNGEONProgram.\n\r",ch);
	return false;
    }

    // Make sure this has a list of progs!
    if(!dungeon->progs) dungeon->progs = new_prog_bank();

    list                  = new_trigger();
    list->vnum            = atol(num);
    list->trig_type       = tindex;
    list->trig_phrase     = str_dup(phrase);
	list->trig_number		= atoi(list->trig_phrase);
    list->numeric		= is_number(list->trig_phrase);
    list->script          = code;

    list_appendlink(dungeon->progs[slot], list);

    send_to_char("Dprog Added.\n\r",ch);
    return true;
}

DNGEDIT (dngedit_deldprog)
{
    DUNGEON_INDEX_DATA *dungeon;
    char dprog[MAX_STRING_LENGTH];
    int value;

    EDIT_DUNGEON(ch, dungeon);

    one_argument(argument, dprog);
    if (!is_number(dprog) || dprog[0] == '\0')
    {
       send_to_char("Syntax:  deldprog [#dprog]\n\r",ch);
       return false;
    }

    value = atol (dprog);

    if (value < 0)
    {
        send_to_char("Only non-negative dprog-numbers allowed.\n\r",ch);
        return false;
    }

    if(!edit_deltrigger(dungeon->progs,value)) {
	send_to_char("No such dprog.\n\r",ch);
	return false;
    }

    send_to_char("Dprog removed.\n\r", ch);
    return true;
}

DNGEDIT(dngedit_varset)
{
    DUNGEON_INDEX_DATA *dungeon;

	EDIT_DUNGEON(ch, dungeon);

	return olc_varset(&dungeon->index_vars, ch, argument, false);
}

DNGEDIT(dngedit_varclear)
{
    DUNGEON_INDEX_DATA *dungeon;

	EDIT_DUNGEON(ch, dungeon);

	return olc_varclear(&dungeon->index_vars, ch, argument, false);
}