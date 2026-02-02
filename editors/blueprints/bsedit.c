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

extern void list_blueprint_sections(CHAR_DATA *ch, char *argument);
extern bool validate_vnum_range(CHAR_DATA *ch, BLUEPRINT_SECTION *section, long lower, long upper);

extern long top_blueprint_section_vnum;


BSEDIT( bsedit_list )
{
    list_blueprint_sections(ch, argument);
    return false;
}

BSEDIT( bsedit_show )
{
    BLUEPRINT_SECTION *bs;
    BUFFER *buffer;
    char buf[MSL];

    EDIT_BPSECT(ch, bs);

    buffer = new_buf();

    sprintf(buf, "Name:        [%5ld] %s\n\r", bs->vnum, bs->name);
    add_buf(buffer, buf);

    sprintf(buf, "Type:        %s\n\r", flag_string(blueprint_section_types, bs->type));
    add_buf(buffer, buf);

    sprintf(buf, "Flags:       %s\n\r", flag_string(blueprint_section_flags, bs->flags));
    add_buf(buffer, buf);

    if( bs->recall_room )
    {
        sprintf(buf, "Recall:      [%5ld] %s\n\r", bs->recall_room->vnum, bs->recall_room->name);
    }
    else if( bs->recall_ref.load.vnum > 0 )
    {
        sprintf(buf, "Recall:      [%5ld] (unresolved)\n\r", bs->recall_ref.load.vnum);
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
    add_buf(buffer, bs->description ? bs->description : "(none)\n\r");
    add_buf(buffer, "\n\r");

    add_buf(buffer, "\n\r-----\n\r{WBuilders' Comments:{X\n\r");
    add_buf(buffer, bs->comments ? bs->comments : "(none)\n\r");
    add_buf(buffer, "\n\r-----\n\r");

    if( bs->links )
    {
        int bli = 0;
        // List links
        add_buf(buffer, "{YSection Links:{x\n\r");
        for(BLUEPRINT_LINK *bl = bs->links; bl; bl = bl->next)
        {
            ++bli;
            ROOM_INDEX_DATA *room = bl->room;

            char *door = (bl->door >= 0 && bl->door < MAX_DIR) ? dir_name[bl->door] : "none";
            char excolor = bl->ex ? 'W' : 'D';

            sprintf(buf, " {Y[{W%3d{Y] {G%-30.30s {%c%-9s{x in {Y[{W%5ld{Y]{x %s\n\r", bli, bl->name, excolor, door, bl->room ? bl->room->vnum : bl->room_ref.load.vnum, room ? room->name : "nowhere");
            add_buf(buffer, buf);
        }
    }

    page_to_char(buffer->string, ch);

    free_buf(buffer);
    return false;
}

BSEDIT( bsedit_create )
{
    BLUEPRINT_SECTION *bs;
    long  value;
    int  iHash;

    // Auto-vnum: empty or "0" finds next available
    if (argument[0] == '\0' || !str_cmp(argument, "0"))
    {
        long last_vnum = 0;
        value = top_blueprint_section_vnum + 1;
        for(last_vnum = 1; last_vnum <= top_blueprint_section_vnum; last_vnum++)
        {
            if( !get_blueprint_section(last_vnum) )
            {
                value = last_vnum;
                break;
            }
        }
    }
    else
    {
        // Parse widevnum - sections are global so no context needed
        WNUM bs_wnum;
        if (!parse_widevnum(argument, NULL, &bs_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        
        value = bs_wnum.vnum;
        
        if( get_blueprint_section(value) )
        {
            send_to_char("That vnum already exists.\n\r", ch);
            return false;
        }
    }

    bs = new_blueprint_section();
    bs->vnum = value;

    iHash							= bs->vnum % MAX_KEY_HASH;
    bs->next						= blueprint_section_hash[iHash];
    blueprint_section_hash[iHash]	= bs;
    ch->desc->pEdit					= (void *)bs;

    if( bs->vnum > top_blueprint_section_vnum)
        top_blueprint_section_vnum = bs->vnum;

    return true;
}

BSEDIT( bsedit_name )
{
    BLUEPRINT_SECTION *bs;

    EDIT_BPSECT(ch, bs);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  name [string]\n\r", ch);
        return false;
    }

    free_string(bs->name);
    bs->name = str_dup(argument);
    send_to_char("Name changed.\n\r", ch);
    return true;
}

BSEDIT( bsedit_description )
{
    BLUEPRINT_SECTION *bs;

    EDIT_BPSECT(ch, bs);

    if (argument[0] == '\0')
    {
        string_append(ch, &bs->description);
        return true;
    }

    send_to_char("Syntax:  description - line edit\n\r", ch);
    return false;
}

BSEDIT( bsedit_comments )
{
    BLUEPRINT_SECTION *bs;

    EDIT_BPSECT(ch, bs);

    if (argument[0] == '\0')
    {
        string_append(ch, &bs->comments);
        return true;
    }

    send_to_char("Syntax:  comments - line edit\n\r", ch);
    return false;
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
        return false;
    }

    if( (value = flag_value(blueprint_section_types, argument)) == NO_FLAG )
    {
        send_to_char("That is not a valid type.\n\r", ch);
        send_to_char("'? section_types' for list of types.\n\r", ch);
        return false;
    }

    bs->type = value;
    send_to_char("Section type changed.\n\r", ch);
    return true;
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
        return false;
    }

    if( (value = flag_value(blueprint_section_flags, argument)) == NO_FLAG )
    {
        send_to_char("That is not a valid flag.\n\r", ch);
        send_to_char("'? section_flags' for list of flags.\n\r", ch);
        return false;
    }

    bs->flags ^= value;
    send_to_char("Section flags changed.\n\r", ch);
    return true;
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
        return false;
    }

    if( !str_cmp(argument, "none") )
    {
        if( bs->recall_room ? bs->recall_room->vnum : bs->recall_ref.load.vnum < 1 )
        {
            send_to_char("Recall was not defined.\n\r", ch);
            return false;
        }

        bs->recall_ref.load.vnum = 0;
        bs->recall_ref.load.auid = 0;
        bs->recall_room = NULL;

        send_to_char("Recall cleared.\n\r", ch);
        return true;
    }

    if( bs->lower_vnum < 1 || bs->upper_vnum < 1 )
    {
        send_to_char("Vnum range must be set first.\n\r", ch);
        return false;
    }

    WNUM room_wnum;
    AREA_DATA *context = strchr(argument, '#') ? bs->area : NULL;
    if (!parse_widevnum(argument, context, &room_wnum)) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    vnum = room_wnum.vnum;
    if( vnum <= 0 )
    {
        send_to_char("That room does not exist.\n\r", ch);
        return false;
    }

    if( vnum < bs->lower_vnum || vnum > bs->upper_vnum )
    {
        sprintf(buf, "Value must be a value from %ld to %ld.\n\r", bs->lower_vnum, bs->upper_vnum);
        send_to_char(buf, ch);
        return false;
    }

    room = get_room_index(room_wnum.pArea, vnum);
    if( room == NULL )
    {
        send_to_char("That room does not exist.\n\r", ch);
        return false;
    }

    bs->recall_ref.load.vnum = vnum; bs->recall_ref.load.auid = bs->area ? bs->area->uid : 0; bs->recall_room = get_room_index(bs->area ? bs->area : get_system_area_fallback(), vnum);
    sprintf(buf, "Recall set to %.30s (%ld)\n\r", room->name, vnum);
    send_to_char(buf, ch);
    return true;
}

BSEDIT( bsedit_rooms )
{
    BLUEPRINT_SECTION *bs;
    long lvnum, uvnum;
    char buf[MSL];
    char arg[MIL];

    EDIT_BPSECT(ch, bs);

    argument = one_argument(argument, arg);

    if( arg[0] == '\0' || argument[0] == '\0' )
    {
        send_to_char("Syntax:  rooms [lower vnum][upper vnum]\n\r", ch);
        send_to_char("{YVnums must be in the same area.{x\n\r", ch);
        return false;
    }

    if( !is_number(arg) || !is_number(argument) )
    {
        send_to_char("That is not a number.\n\r", ch);
        return false;
    }

    lvnum = atol(arg);
    uvnum = atol(argument);

    // Silently swap the bounds if necessary, don't be annoying
    if( uvnum < lvnum )
    {
        long vnum = lvnum;
        lvnum = uvnum;
        uvnum = vnum;
    }

    if( validate_vnum_range(ch, bs, lvnum, uvnum) )
    {
        bs->lower_vnum = lvnum;
        bs->upper_vnum = uvnum;

        send_to_char("Vnum range set.\n\r", ch);

        // Make sure recall point is still inside room range
        if( bs->recall_room ? bs->recall_room->vnum : bs->recall_ref.load.vnum > 0 )
        {
            if( bs->recall_room ? bs->recall_room->vnum : bs->recall_ref.load.vnum < lvnum || bs->recall_room ? bs->recall_room->vnum : bs->recall_ref.load.vnum > uvnum )
            {
                send_to_char("{YRecall room outside of new range.  Clearing.{x\n\r", ch);
                bs->recall_ref.load.vnum = 0; bs->recall_ref.load.auid = 0; bs->recall_room = NULL;
            }
        }

        // Make sure link are still inside room range
        if( bs->links )
        {
            BLUEPRINT_LINK *prev = NULL, *cur, *next;

            for(cur = bs->links; cur; cur = next)
            {
                next = cur->next;

                if( cur->room ? cur->room->vnum : cur->room_ref.load.vnum < lvnum || cur->room ? cur->room->vnum : cur->room_ref.load.vnum > uvnum )
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

        return true;
    }

    return false;
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
        return false;
    }

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if( !str_cmp(arg, "list" ) )
    {
        if( bs->links )
        {
            char buf[MSL];

            int bli = 0;
            // List links
            send_to_char("{YSection Links:{x\n\r", ch);
            for(BLUEPRINT_LINK *bl = bs->links; bl; bl = bl->next)
            {
                ++bli;
                ROOM_INDEX_DATA *room = bl->room;

                char *door = (bl->door >= 0 && bl->door < MAX_DIR) ? dir_name[bl->door] : "none";
                char excolor = bl->ex ? 'W' : 'D';

                sprintf(buf, " {Y[{W%3d{Y] {G%-30.30s {%c%-9s{x in {Y[{W%5ld{Y]{x %s\n\r", bli, bl->name, excolor, door, bl->room ? bl->room->vnum : bl->room_ref.load.vnum, room ? room->name : "nowhere");
                send_to_char(buf, ch);
            }
        }
        else
        {
            send_to_char("No links defined.\n\r", ch);
        }

        return false;
    }

    if( !str_cmp(arg, "add") )
    {
        if( bs->lower_vnum < 1 || bs->upper_vnum < 1 )
        {
            send_to_char("Vnum range must be set first.\n\r", ch);
            return false;
        }

        WNUM room_wnum;
        AREA_DATA *context = strchr(arg2, '#') ? bs->area : NULL;
        if (!parse_widevnum(arg2, context, &room_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        long vnum = room_wnum.vnum;
        if( vnum < bs->lower_vnum || vnum > bs->upper_vnum )
        {
            send_to_char("Vnum is out of range of blueprint section.\n\r", ch);
            return false;
        }

        ROOM_INDEX_DATA *room = get_room_index(room_wnum.pArea, vnum);
        if( !room )
        {
            send_to_char("That room does not exist.\n\r", ch);
            return false;
        }

        bool found = false;
        for( int i = 0; i < MAX_DIR; i++ )
        {
            if( room->exit[i] )
                found = true;
        }

        if( !found )
        {
            send_to_char("That room has no exits.\n\r,", ch);
            return false;
        }

        int door = parse_door(argument);
        if( door < 0 )
        {
            send_to_char("That is an invalid exit.\n\r", ch);
            return false;
        }

        EXIT_DATA *ex = room->exit[door];
        if( !ex )
        {
            send_to_char("That is an invalid exit.\n\r", ch);
            return false;
        }

        if( !IS_SET(ex->exit_info, EX_ENVIRONMENT) )
        {
            send_to_char("Exit links must be {YENVIRONMENT{x exits.\n\r", ch);
            return false;
        }

        link = new_blueprint_link();
        link->room_ref.load.vnum = vnum; link->room_ref.load.auid = bs->area ? bs->area->uid : 0; link->room = room;
        link->door = door;
        link->room = room;
        link->ex = ex;

        // Append to the end
        link->next = NULL;
        if( bs->links )
        {
            BLUEPRINT_LINK *cur;

            for(cur = bs->links;cur->next; cur = cur->next)
            {
                ;
            }

            cur->next = link;
        }
        else
        {
            bs->links = link;
        }

        send_to_char("Link added.\n\r", ch);
        return true;
    }

    if( !is_number(arg) )
    {
        send_to_char("That is not a number.\n\r", ch);
        return false;
    }

    int linkno = atoi(arg);
    if( linkno < 1 )
    {
        send_to_char("That is not a section link.\n\r", ch);
        return false;
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
            return false;
        }

        if( prev )
            prev->next = link->next;
        else
            bs->links = link->next;

        free_blueprint_link(link);
        send_to_char("Link removed.\n\r", ch);
        return true;
    }

    link = get_section_link(bs, linkno);
    if( !link )
    {
        send_to_char("That is not a section link.\n\r", ch);
        return false;
    }

    if( !str_cmp(arg2, "name") )
    {
        if( argument[0] == '\0' )
        {
            send_to_char("Syntax:  link # name <name>\n\r", ch);
            return false;
        }

        free_string(link->name);
        link->name = str_dup(argument);

        send_to_char("Name changed.\n\r", ch);
        return true;
    }

    if( !str_cmp(arg2, "room") )
    {
        if( argument[0] == '\0' )
        {
            send_to_char("Syntax:  link # room <widevnum>\n\r", ch);
            return false;
        }

        WNUM room_wnum;
        AREA_DATA *context = strchr(argument, '#') ? bs->area : NULL;
        if (!parse_widevnum(argument, context, &room_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        long vnum = room_wnum.vnum;
        if( vnum < bs->lower_vnum || vnum > bs->upper_vnum )
        {
            send_to_char("Vnum is out of range of blueprint section.\n\r", ch);
            return false;
        }

        ROOM_INDEX_DATA *room = get_room_index(room_wnum.pArea, vnum);
        if( !room )
        {
            send_to_char("That room does not exist.\n\r", ch);
            return false;
        }

        bool found = false;
        for( int i = 0; i < MAX_DIR; i++ )
        {
            if( room->exit[i] )
                found = true;
        }

        if( !found )
        {
            send_to_char("That room has no exits.\n\r,", ch);
            return false;
        }

        link->room_ref.load.vnum = vnum; link->room_ref.load.auid = bs->area ? bs->area->uid : 0; link->room = room;
        link->door = -1;
        link->room = room;
        link->ex = NULL;


        send_to_char("Room changed.\n\r", ch);
        return true;
    }

    if( !str_cmp(arg2, "exit") )
    {
        if( argument[0] == '\0' )
        {
            send_to_char("Syntax:  link # exit <door>\n\r", ch);
            return false;
        }

        int door = parse_door(argument);
        if( door < 0 )
        {
            send_to_char("That is an invalid exit.\n\r", ch);
            return false;
        }

        EXIT_DATA *ex = link->room->exit[door];
        if( !ex )
        {
            send_to_char("That is an invalid exit.\n\r", ch);
            return false;
        }

        if( !IS_SET(ex->exit_info, EX_ENVIRONMENT) )
        {
            send_to_char("Exit links must be {YENVIRONMENT{x exits.\n\r", ch);
            return false;
        }

        link->door = door;
        link->ex = ex;
        send_to_char("Exit changed.\n\r", ch);
        return true;
    }

    bsedit_link(ch, "");
    return false;
}