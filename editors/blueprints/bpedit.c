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

extern void list_blueprints(CHAR_DATA *ch, char *argument);

extern bool blueprints_changed;
extern long top_blueprint_vnum;


BPEDIT( bpedit_list )
{
	list_blueprints(ch, argument);
	return false;
}

BPEDIT( bpedit_show )
{
	BLUEPRINT *bp;
	BUFFER *buffer;
	char buf[MSL];

	EDIT_BLUEPRINT(ch, bp);

	buffer = new_buf();

	sprintf(buf, "{xName:        [%5ld] %s{x\n\r", bp->vnum, bp->name);
	add_buf(buffer, buf);

	if( bp->repop > 0)
		sprintf(buf, "Repop:       %d minutes\n\r", bp->repop);
	else
		sprintf(buf, "Repop:       {Dnever{x\n\r");
	add_buf(buffer, buf);

	sprintf(buf, "{xAreaWho:     [%s] [%s]{x\n\r", flag_string(area_who_titles, bp->area_who), flag_string(area_who_display, bp->area_who));
	add_buf(buffer, buf);

	sprintf(buf, "{xFlags:       %s{x\n\r", flag_string(instance_flags, bp->flags));
	add_buf(buffer, buf);

	add_buf(buffer, "Description:\n\r");
	add_buf(buffer, bp->description);
	add_buf(buffer, "\n\r");

	add_buf(buffer, "\n\r-----\n\r{WBuilders' Comments:{X\n\r");
	add_buf(buffer, bp->comments);
	add_buf(buffer, "\n\r-----\n\r");

	switch(bp->mode)
	{
	case BLUEPRINT_MODE_STATIC:
		add_buf(buffer, "{xMode:        [{WStatic{x]\n\r");
		break;

	default:
		add_buf(buffer, "{xMode:        [Unknown]\n\r");
		break;
	}

	if( list_size(bp->sections) > 0 )
	{
		int line = 0;
		BLUEPRINT_SECTION * bs;
		ITERATOR sit;

		add_buf(buffer, "{YSections:{x\n\r");
		add_buf(buffer, "     [  Vnum  ] [             Name             ]\n\r");
		add_buf(buffer, "------------------------------------------------\n\r");

		iterator_start(&sit, bp->sections);
		while( (bs = (BLUEPRINT_SECTION *)iterator_nextdata(&sit)) )
		{
			sprintf(buf, "{W%4d  {G%8ld{x   %-30.30s{x\n\r", ++line, bs->vnum, bs->name);
			add_buf(buffer, buf);
		}

		iterator_stop(&sit);
		add_buf(buffer, "------------------------------------------------\n\r\n\r");
	}
	else
	{
		add_buf(buffer, "{YSections:{x\n\r   None\n\r\n\r");
	}

	add_buf(buffer, "Special Rooms:\n\r");
	if( list_size(bp->special_rooms) > 0 )
	{
		BLUEPRINT_SPECIAL_ROOM *special;

		char buf[MSL];
		int line = 0;

		ITERATOR sit;

		add_buf(buffer, "     [             Name             ] [             Room             ]\n\r");
		add_buf(buffer, "---------------------------------------------------------------------------------\n\r");

		iterator_start(&sit, bp->special_rooms);
		while( (special = (BLUEPRINT_SPECIAL_ROOM *)iterator_nextdata(&sit)) )
		{
			BLUEPRINT_SECTION *section = list_nthdata(bp->sections, special->section);
			ROOM_INDEX_DATA *room = get_room_index(special->vnum);

			if( !IS_VALID(section) || !room || room->vnum < section->lower_vnum || room->vnum > section->upper_vnum)
			{
				snprintf(buf, MSL-1, "{W%4d  %-30.30s   {D-{Winvalid{D-{x\n\r", ++line, special->name);
			}
			else
			{
				snprintf(buf, MSL-1, "{W%4d  %-30.30s   (%ld) {Y%s{x in (%ld) {Y%s{x\n\r", ++line, special->name, room->vnum, room->name, section->vnum, section->name);
			}
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



	if( bp->mode == BLUEPRINT_MODE_STATIC )
	{
		if( bp->static_layout )
		{
			int linkno = 0;
			add_buf(buffer, "{CLinks:{x\n\r");

			add_buf(buffer, "     [ Section 1 ] [ Link 1 ] [ Section 2 ] [ Link 2 ]\n\r");
			add_buf(buffer, "-------------------------------------------------------\n\r");

			STATIC_BLUEPRINT_LINK *sbl;
			for(sbl = bp->static_layout; sbl; sbl = sbl->next)
			{
				sprintf(buf, "{W%4d   {G%9d     {Y%6d     {G%9d     {Y%6d{x\n\r",
					++linkno, sbl->section1, sbl->link1, sbl->section2, sbl->link2);
				add_buf(buffer, buf);
			}

			add_buf(buffer, "-------------------------------------------------------\n\r\n\r");
		}
		else
		{
			add_buf(buffer, "{CLinks:{x\n\r   None\n\r\n\r");
		}

		if( bp->static_recall > 0 )
		{
			BLUEPRINT_SECTION *bs = (BLUEPRINT_SECTION *)list_nthdata(bp->sections, bp->static_recall);

			if( bs )
			{
				sprintf(buf, "{xRecall:     %d [%ld] %-.30s\n\r", bp->static_recall, bs->vnum, bs->name);
			}
			else
			{
				sprintf(buf, "{xRecall:     %d [---] {Dinvalid{x\n\r", bp->static_recall);
			}

			add_buf(buffer, buf);
		}
		else
		{
			add_buf(buffer, "{xRecall:     None\n\r");
		}

		if( bp->static_entry_section > 0 && bp->static_entry_link > 0 )
		{
			BLUEPRINT_SECTION *bs = (BLUEPRINT_SECTION *)list_nthdata(bp->sections, bp->static_entry_section);

			if( bs )
			{
				BLUEPRINT_LINK *bl = get_section_link(bs, bp->static_entry_link);

				char section_name[31];

				strncpy(section_name, bs->name, 30);
				section_name[30] = '\0';

				if( bl && bl->vnum > 0 && bl->door >= 0 && bl->door < MAX_DIR )
				{
					sprintf(buf, "{xEntry:      %d [%ld] %s (%ld:%s)\n\r", bp->static_entry_section, bs->vnum, section_name, bl->vnum, dir_name[bl->door]);
				}
				else
				{
					sprintf(buf, "{xEntry:      %d [%ld] %s ({Dinvalid{x)\n\r", bp->static_entry_section, bs->vnum, section_name);
				}
			}
			else
			{
				sprintf(buf, "{xEntry:      %d [---] {Dinvalid{x\n\r", bp->static_entry_section);
			}

			add_buf(buffer, buf);
		}
		else
		{
			add_buf(buffer, "{xEntry:      None\n\r");
		}


		if( bp->static_exit_section > 0 && bp->static_exit_link > 0 )
		{
			BLUEPRINT_SECTION *bs = (BLUEPRINT_SECTION *)list_nthdata(bp->sections, bp->static_exit_section);

			if( bs )
			{
				BLUEPRINT_LINK *bl = get_section_link(bs, bp->static_exit_link);

				char section_name[31];

				strncpy(section_name, bs->name, 30);
				section_name[30] = '\0';

				if( bl && bl->vnum > 0 && bl->door >= 0 && bl->door < MAX_DIR )
				{
					sprintf(buf, "{xExit:       %d [%ld] %s (%ld:%s)\n\r", bp->static_exit_section, bs->vnum, section_name, bl->vnum, dir_name[bl->door]);
				}
				else
				{
					sprintf(buf, "{xExit:       %d [%ld] %s ({Dinvalid{x)\n\r", bp->static_exit_section, bs->vnum, section_name);
				}
			}
			else
			{
				sprintf(buf, "{xExit:       %d [---] {Dinvalid{x\n\r", bp->static_exit_section);
			}

			add_buf(buffer, buf);
		}
		else
		{
			add_buf(buffer, "{xExit:       None\n\r");
		}

	}

	if (bp->progs) {
		int cnt, slot;

		for (cnt = 0, slot = 0; slot < TRIGSLOT_MAX; slot++)
			if(list_size(bp->progs[slot]) > 0) ++cnt;

		if (cnt > 0) {
			sprintf(buf, "{R%-6s %-20s %-10s %-10s\n\r{x", "Number", "Prog Vnum", "Trigger", "Phrase");
			add_buf(buffer, buf);

			sprintf(buf, "{R%-6s %-20s %-10s %-10s\n\r{x", "------", "-------------", "-------", "------");
			add_buf(buffer, buf);

			ITERATOR it;
			PROG_LIST *trigger;
			for (cnt = 0, slot = 0; slot < TRIGSLOT_MAX; slot++) {
				iterator_start(&it, bp->progs[slot]);
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

	if (bp->index_vars) {
		pVARIABLE var;
		int cnt;

		for (cnt = 0, var = bp->index_vars; var; var = var->next) ++cnt;

		if (cnt > 0) {
			sprintf(buf, "{R%-20s %-8s %-5s %-10s\n\r{x", "Name", "Type", "Saved", "Value");
			add_buf(buffer, buf);

			sprintf(buf, "{R%-20s %-8s %-5s %-10s\n\r{x", "----", "----", "-----", "-----");
			add_buf(buffer, buf);

			for (var = bp->index_vars; var; var = var->next) {
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


	if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
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

BPEDIT( bpedit_create )
{
	BLUEPRINT *bp;

	long  value;
	int  iHash;

	value = atol(argument);
	if (argument[0] == '\0' || value == 0)
	{
		long last_vnum = 0;
		value = top_blueprint_vnum + 1;
		for(last_vnum = 1; last_vnum <= top_blueprint_vnum; last_vnum++)
		{
			if( !get_blueprint(last_vnum) )
			{
				value = last_vnum;
				break;
			}
		}
	}
	else if( get_blueprint(value) )
	{
		send_to_char("That vnum already exists.\n\r", ch);
		return false;
	}

	bp = new_blueprint();
	bp->vnum = value;

	iHash							= bp->vnum % MAX_KEY_HASH;
	bp->next						= blueprint_hash[iHash];
	blueprint_hash[iHash]			= bp;
	ch->desc->pEdit					= (void *)bp;

	if( bp->vnum > top_blueprint_vnum)
		top_blueprint_vnum = bp->vnum;

    return true;

}


BPEDIT( bpedit_name )
{
	BLUEPRINT *bp;

	EDIT_BLUEPRINT(ch, bp);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  name [string]\n\r", ch);
		return false;
	}

	free_string(bp->name);
	bp->name = str_dup(argument);
	send_to_char("Name changed.\n\r", ch);
	return true;
}

BPEDIT( bpedit_repop )
{
	BLUEPRINT *bp;

	EDIT_BLUEPRINT(ch, bp);

	if( !is_number(argument) )
	{
		send_to_char("Syntax:  repop [age]\n\r", ch);
		return false;
	}

	int repop = atoi(argument);
	bp->repop = UMAX(0, repop);
	send_to_char("Repop changed.\n\r", ch);
	return true;
}

BPEDIT( bpedit_flags )
{
	BLUEPRINT *bp;
	int value;

	EDIT_BLUEPRINT(ch, bp);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  flags <flags>\n\r", ch);
		send_to_char("'? instance' for list of flags.\n\r", ch);
		return false;
	}

	if( (value = flag_value(instance_flags, argument)) != NO_FLAG )
	{
		bp->flags ^= value;
		send_to_char("Instance flags changed.\n\r", ch);
		return true;
	}

	bpedit_flags(ch, "");
	return false;

}


BPEDIT( bpedit_description )
{
	BLUEPRINT *bp;

	EDIT_BLUEPRINT(ch, bp);

	if (argument[0] == '\0')
	{
		string_append(ch, &bp->description);
		return true;
	}

	send_to_char("Syntax:  description\n\r", ch);
	return false;
}

BPEDIT( bpedit_comments )
{
	BLUEPRINT *bp;

	EDIT_BLUEPRINT(ch, bp);

	if (argument[0] == '\0')
	{
		string_append(ch, &bp->comments);
		return true;
	}

	send_to_char("Syntax:  comments\n\r", ch);
	return false;
}

BPEDIT( bpedit_areawho )
{
	BLUEPRINT *bp;
	int value;

	EDIT_BLUEPRINT(ch, bp);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  areawho <value>\n\r", ch);
		send_to_char("See '? areawho' for list\n\r", ch);
		return false;
	}

	if ( !str_prefix(argument, "blank") )
	{
	    bp->area_who = AREA_BLANK;

	    send_to_char("Area who title cleared.\n\r", ch);
	    return true;
	}


	if ((value = flag_value(area_who_titles, argument)) != NO_FLAG)
	{
		bp->area_who = value;

		send_to_char("Area who title set.\n\r", ch);
		return true;
	}

	bpedit_areawho(ch, "");
	return false;
}

BPEDIT( bpedit_mode )
{
	BLUEPRINT *bp;

	EDIT_BLUEPRINT(ch, bp);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  mode static|procedural\n\r", ch);
		return false;
	}

	if( !str_prefix(argument, "static") )
	{
		if( bp->mode == BLUEPRINT_MODE_STATIC )
		{
			send_to_char("Blueprint is already in STATIC mode.\n\r", ch);
			return false;
		}


		bp->mode = BLUEPRINT_MODE_STATIC;
		// Remove non-static data

		// Initialize static data
		bp->static_layout = NULL;
		bp->static_recall = -1;
		bp->static_entry_section = -1;
		bp->static_entry_link = -1;
		bp->static_exit_section = -1;
		bp->static_exit_link = -1;

		send_to_char("Blueprint changed to STATIC mode.\n\r", ch);
		return true;
	}

	if( !str_prefix(argument, "procedural") )
	{
		send_to_char("Procedural mode is not implemented yet.\n\r", ch);
		return false;
	}

	bpedit_mode(ch, "");
	return false;
}

BPEDIT( bpedit_section )
{
	BLUEPRINT *bp;
	BLUEPRINT_SECTION *bs;
	char arg[MIL];

	EDIT_BLUEPRINT(ch, bp);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  section add <vnum>\n\r", ch);
		send_to_char("         section delete <#>\n\r", ch);
		send_to_char("         section list\n\r", ch);
		return false;
	}

	argument = one_argument(argument, arg);

	if( !str_prefix(arg, "add") )
	{
		if(!is_number(argument))
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		bs = get_blueprint_section(atol(argument));
		if( !bs )
		{
			send_to_char("That blueprint section does not exist.\n\r", ch);
			return false;
		}

		if( !list_appendlink(bp->sections, bs) )
		{
			send_to_char("{WError adding blueprint section to blueprint.{x\n\r", ch);
			return false;
		}


		send_to_char("Blueprint section added.\n\r", ch);
		return true;
	}

	if( !str_prefix(arg, "delete") )
	{
		if(!is_number(argument))
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		int index = atoi(argument);

		if( index < 1 || index > list_size(bp->sections) )
		{
			send_to_char("Index out of range.\n\r", ch);
			return false;
		}

		list_remnthlink(bp->sections, index, true);

		send_to_char("Blueprint section removed.\n\r", ch);
		if( bp->mode == BLUEPRINT_MODE_STATIC )
		{
			// Remove any invalid layout definitions since the section has been removed
			STATIC_BLUEPRINT_LINK *prev, *cur, *next;

			prev = NULL;
			for(cur = bp->static_layout; cur; cur = next)
			{
				next = cur->next;

				// Link references deleted section
				if( cur->section1 == index || cur->section2 == index )
				{
					if( !prev )
						bp->static_layout = next;
					else
						prev->next = next;
					free_static_blueprint_link(cur);
					continue;
				}

				// If link references a section AFTER the specified index, shift down by one
				if( cur->section1 > index )
					cur->section1--;

				if( cur->section2 > index )
					cur->section2--;

				prev = cur;
			}

			// Check RECALL
			if( bp->static_recall == index )
				bp->static_recall = -1;
			else if( bp->static_recall > index )
				bp->static_recall--;

			// Check ENTRY
			if( bp->static_entry_section == index )
			{
				bp->static_entry_section = -1;
				bp->static_entry_link = -1;
			}
			else if( bp->static_entry_section > index )
				bp->static_entry_section--;

			// Check EXIT
			if( bp->static_exit_section == index )
			{
				bp->static_exit_section = -1;
				bp->static_exit_link = -1;
			}
			else if( bp->static_exit_section > index )
				bp->static_exit_section--;
		}

		return true;
	}

	if( !str_prefix(arg, "list") )
	{
		if( list_size(bp->sections) > 0 )
		{
			BUFFER *buffer = new_buf();

			char buf[MSL];
			int line = 0;

			ITERATOR sit;

			add_buf(buffer, "     [  Vnum  ] [             Name             ]\n\r");
			add_buf(buffer, "------------------------------------------------\n\r");

			iterator_start(&sit, bp->sections);
			while( (bs = (BLUEPRINT_SECTION *)iterator_nextdata(&sit)) )
			{
				sprintf(buf, "{W%4d  {G%8ld{x   %-30.30s{x\n\r", ++line, bs->vnum, bs->name);
				add_buf(buffer, buf);
			}

			iterator_stop(&sit);
			add_buf(buffer, "------------------------------------------------\n\r");

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
			send_to_char("Blueprint has no blueprint sections assigned.\n\r", ch);
		}

		return false;
	}

	bpedit_section(ch, "");
	return false;
}

BPEDIT( bpedit_static )
{
	BLUEPRINT *bp;
	char arg[MIL];

	EDIT_BLUEPRINT(ch, bp);

	if( bp->mode != BLUEPRINT_MODE_STATIC )
	{
		send_to_char("Blueprint is not in STATIC mode.\n\r", ch);
		return false;
	}

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  static link add <section1#> <link1#> <section2#> <link2#>\n\r", ch);
		send_to_char("         static link remove #\n\r", ch);
		send_to_char("         static recall <section#>\n\r", ch);
		send_to_char("         static recall clear\n\r", ch);
		send_to_char("         static entry <section#> <link#>\n\r", ch);
		send_to_char("         static entry clear\n\r", ch);
		send_to_char("         static exit <section#> <link#>\n\r", ch);
		send_to_char("         static exit clear\n\r", ch);
		send_to_char("         static special add <section#> <room vnum> <name>\n\r", ch);
		send_to_char("         static special # remove\n\r", ch);
		send_to_char("         static special # name <name>\n\r", ch);
		send_to_char("         static special # room <section#> <room vnum>\n\r", ch);
		return false;
	}

	argument = one_argument(argument, arg);

	if( !str_prefix(arg, "special") )
	{
		char arg2[MIL];
		char arg3[MIL];
		char arg4[MIL];

		argument = one_argument(argument, arg2);

		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  static special add <section#> <room vnum> <name>\n\r", ch);
			send_to_char("         static special # remove\n\r", ch);
			send_to_char("         static special # name <name>\n\r", ch);
			send_to_char("         static special # room <section#> <room vnum>\n\r", ch);
			return false;
		}

		if( is_number(arg2) )
		{
			int index = atoi(arg2);

			BLUEPRINT_SPECIAL_ROOM *special = list_nthdata(bp->special_rooms, index);

			if( !IS_VALID(special) )
			{
				send_to_char("No such special room.\n\r", ch);
				return false;
			}

			if( argument[0] == '\0' )
			{
				send_to_char("Syntax:  static special # remove\n\r", ch);
				send_to_char("         static special # name <name>\n\r", ch);
				send_to_char("         static special # room <section#> <room vnum>\n\r", ch);
				return false;
			}

			if( !str_prefix(argument, "remove") || !str_prefix(argument, "delete") )
			{
				list_remnthlink(bp->special_rooms, index, true);
				send_to_char("Special Room removed.\n\r", ch);
				return true;
			}


			argument = one_argument(argument, arg3);

			if( !str_prefix(arg3, "name") )
			{
				if( argument[0] == '\0' )
				{
					send_to_char("Syntax:  static special # name <name>\n\r", ch);
					return false;
				}

				free_string(special->name);
				special->name = str_dup(argument);

				send_to_char("Name changed.\n\r", ch);
				return true;
			}

			argument = one_argument(argument, arg4);

			if( !str_prefix(arg3, "room") )
			{
				if( !is_number(arg4) || !is_number(argument) )
				{
					send_to_char("That is not a number.\n\r", ch);
					return false;
				}

				int section = atoi(arg4);
				long vnum = atol(argument);

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

			send_to_char("Syntax:  static special # remove\n\r", ch);
			send_to_char("         static special # name <name>\n\r", ch);
			send_to_char("         static special # room <section#> <room vnum>\n\r", ch);
			return false;
		}

		if( !str_prefix(arg2, "add") )
		{
			if( argument[0] == '\0' )
			{
				send_to_char("Syntax:  static special add <section#> <room vnum> <name>\n\r", ch);
				return false;
			}

			argument = one_argument(argument, arg3);
			argument = one_argument(argument, arg4);


			if( !is_number(arg3) || !is_number(arg4) )
			{
				send_to_char("That is not a number.\n\r", ch);
				return false;
			}

			int section = atoi(arg3);
			long vnum = atol(arg4);

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

			BLUEPRINT_SPECIAL_ROOM *special = new_blueprint_special_room();
			free_string(special->name);
			special->name = str_dup(name);
			special->section = section;
			special->vnum = vnum;

			list_appendlink(bp->special_rooms, special);

			send_to_char("Special Room added.\n\r", ch);
			return true;
		}

		send_to_char("Syntax:  static special add <section#> <room vnum> <name>\n\r", ch);
		send_to_char("         static special # remove\n\r", ch);
		send_to_char("         static special # name <name>\n\r", ch);
		send_to_char("         static special # room <section#> <room vnum>\n\r", ch);
		return false;
	}

	if( !str_prefix(arg, "link") )
	{
		char arg2[MIL];
		char arg3[MIL];
		char arg4[MIL];
		char arg5[MIL];

		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  static link add <section1#> <link1#> <section2#> <link2#>\n\r", ch);
			send_to_char("         static link remove #\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg2);

		if( !str_prefix(arg2, "add") )
		{
			BLUEPRINT_SECTION *bs;

			argument = one_argument(argument, arg3);
			argument = one_argument(argument, arg4);
			argument = one_argument(argument, arg5);

			if( !is_number(arg3) || !is_number(arg4) || !is_number(arg5) || !is_number(argument) )
			{
				send_to_char("That is not a number.\n\r", ch);
				return false;
			}

			int section1 = atoi(arg3);
			int link1 = atoi(arg4);
			int section2 = atoi(arg5);
			int link2 = atoi(argument);

			if( section1 < 1 || section1 > list_size(bp->sections) )
			{
				send_to_char("Index out of range.\n\r", ch);
				return false;
			}

			bs = (BLUEPRINT_SECTION *)list_nthdata(bp->sections, section1);
			if( !get_section_link(bs, link1) )
			{
				send_to_char("Link index out of range.\n\r", ch);
				return false;
			}

			if( section2 < 1 || section2 > list_size(bp->sections) )
			{
				send_to_char("Index out of range.\n\r", ch);
				return false;
			}

			bs = (BLUEPRINT_SECTION *)list_nthdata(bp->sections, section2);
			if( !get_section_link(bs, link2) )
			{
				send_to_char("Link index out of range.\n\r", ch);
				return false;
			}

			STATIC_BLUEPRINT_LINK *sbl = new_static_blueprint_link();

			sbl->blueprint = bp;
			sbl->section1 = section1;
			sbl->link1 = link1;
			sbl->section2 = section2;
			sbl->link2 = link2;

			sbl->next = bp->static_layout;
			bp->static_layout = sbl;

			send_to_char("Static link added.\n\r", ch);
			return true;
		}

		if( !str_prefix(arg2, "remove") )
		{
			STATIC_BLUEPRINT_LINK *prev, *cur;

			if( !is_number(arg2) )
			{
				send_to_char("That is not a number.\n\r", ch);
				return false;
			}

			int index = atoi(arg);
			if( index < 1 )
			{
				send_to_char("Link does not exist.\n\r", ch);
				return false;
			}

			prev = NULL;
			for(cur = bp->static_layout; cur && index > 0; prev = cur, cur = cur->next)
			{
				if( !--index )
				{
					if( prev )
						prev->next = cur->next;
					else
						bp->static_layout = cur->next;

					free_static_blueprint_link(cur);
					send_to_char("Link removed.\n\r", ch);
					return true;
				}
			}


			send_to_char("Link does not exist.\n\r", ch);
			return false;
		}

		bpedit_static(ch, "link");
		return false;
	}

	if( !str_prefix(arg, "recall") )
	{
		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  static recall <section#>\n\r", ch);
			send_to_char("         static recall clear\n\r", ch);
			return false;
		}

		if( is_number(argument) )
		{
			int index = atoi(argument);
			if( index < 1 || index > list_size(bp->sections) )
			{
				send_to_char("Index out of range.\n\r", ch);
				return false;
			}

			bp->static_recall = index;
			send_to_char("Blueprint recall section changed.\n\r", ch);
			return true;
		}

		if( !str_prefix(argument, "clear") )
		{
			bp->static_recall = -1;
			send_to_char("Blueprint recall section cleared.\n\r", ch);
			return true;
		}

		bpedit_static(ch, "recall");
		return false;
	}

	if( !str_prefix(arg, "entry") )
	{
		char arg2[MIL];

		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  static entry <section#> <link#>\n\r", ch);
			send_to_char("         static entry clear\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg2);

		if( is_number(arg2) && is_number(argument) )
		{
			int section = atoi(arg2);
			int link = atoi(argument);

			if( section < 1 || section > list_size(bp->sections) )
			{
				send_to_char("Section index out of range.\n\r", ch);
				return false;
			}

			BLUEPRINT_SECTION *bs = (BLUEPRINT_SECTION *)list_nthdata(bp->sections, section);

			if( !get_section_link(bs, link) )
			{
				send_to_char("Link index out of range.\n\r", ch);
				return false;
			}


			bp->static_entry_section = section;
			bp->static_entry_link = link;
			send_to_char("Blueprint entry point changed.\n\r", ch);
			return true;
		}

		if( !str_prefix(arg2, "clear") )
		{
			bp->static_entry_section = -1;
			bp->static_entry_link = -1;
			send_to_char("Blueprint entry point cleared.\n\r", ch);
			return true;
		}

		bpedit_static(ch, "entry");
		return false;
	}

	if( !str_prefix(arg, "exit") )
	{
		char arg2[MIL];

		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  static exit <section#> <link#>\n\r", ch);
			send_to_char("         static exit clear\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg2);

		if( is_number(arg2) && is_number(argument) )
		{
			int section = atoi(arg2);
			int link = atoi(argument);

			if( section < 1 || section > list_size(bp->sections) )
			{
				send_to_char("Section index out of range.\n\r", ch);
				return false;
			}

			BLUEPRINT_SECTION *bs = (BLUEPRINT_SECTION *)list_nthdata(bp->sections, section);

			if( !get_section_link(bs, link) )
			{
				send_to_char("Link index out of range.\n\r", ch);
				return false;
			}


			bp->static_exit_section = section;
			bp->static_exit_link = link;
			send_to_char("Blueprint exit point changed.\n\r", ch);
			return true;
		}

		if( !str_prefix(arg2, "clear") )
		{
			bp->static_exit_section = -1;
			bp->static_exit_link = -1;
			send_to_char("Blueprint exit point cleared.\n\r", ch);
			return true;
		}

		bpedit_static(ch, "exit");
		return false;
	}


	bpedit_static(ch, "");
	return false;
}


BPEDIT (bpedit_addiprog)
{
    int tindex, slot;
	BLUEPRINT *blueprint;
    PROG_LIST *list;
    SCRIPT_DATA *code;
    char trigger[MAX_STRING_LENGTH];
    char phrase[MAX_STRING_LENGTH];
    char num[MAX_STRING_LENGTH];

    EDIT_BLUEPRINT(ch, blueprint);
    argument = one_argument(argument, num);
    argument = one_argument(argument, trigger);
    argument = one_argument(argument, phrase);

    if (!is_number(num) || trigger[0] =='\0' || phrase[0] =='\0')
    {
	send_to_char("Syntax:   addiprog [vnum] [trigger] [phrase]\n\r",ch);
	return false;
    }

    if ((tindex = trigger_index(trigger, PRG_IPROG)) < 0) {
	send_to_char("Valid flags are:\n\r",ch);
	show_help(ch, "iprog");
	return false;
    }

    slot = trigger_table[tindex].slot;

    if ((code = get_script_index (atol(num), PRG_IPROG)) == NULL)
    {
	send_to_char("No such INSTANCEProgram.\n\r",ch);
	return false;
    }

    // Make sure this has a list of progs!
    if(!blueprint->progs) blueprint->progs = new_prog_bank();

    list                  = new_trigger();
    list->vnum            = atol(num);
    list->trig_type       = tindex;
    list->trig_phrase     = str_dup(phrase);
	list->trig_number		= atoi(list->trig_phrase);
    list->numeric		= is_number(list->trig_phrase);
    list->script          = code;

    list_appendlink(blueprint->progs[slot], list);

    send_to_char("Iprog Added.\n\r",ch);
    return true;
}

BPEDIT (bpedit_deliprog)
{
    BLUEPRINT *blueprint;
    char iprog[MAX_STRING_LENGTH];
    int value;

    EDIT_BLUEPRINT(ch, blueprint);

    one_argument(argument, iprog);
    if (!is_number(iprog) || iprog[0] == '\0')
    {
       send_to_char("Syntax:  deliprog [#iprog]\n\r",ch);
       return false;
    }

    value = atol (iprog);

    if (value < 0)
    {
        send_to_char("Only non-negative iprog-numbers allowed.\n\r",ch);
        return false;
    }

    if(!edit_deltrigger(blueprint->progs,value)) {
	send_to_char("No such iprog.\n\r",ch);
	return false;
    }

    send_to_char("Iprog removed.\n\r", ch);
    return true;
}

BPEDIT(bpedit_varset)
{
    BLUEPRINT *blueprint;

	EDIT_BLUEPRINT(ch, blueprint);

	return olc_varset(&blueprint->index_vars, ch, argument, false);
}

BPEDIT(bpedit_varclear)
{
    BLUEPRINT *blueprint;

	EDIT_BLUEPRINT(ch, blueprint);

	return olc_varclear(&blueprint->index_vars, ch, argument, false);
}