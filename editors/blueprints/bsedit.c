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

    // Show maze data if type is BSTYPE_MAZE
    if (bs->type == BSTYPE_MAZE) {
        sprintf(buf, "Maze Size:   %ld x %ld\n\r", bs->maze_x, bs->maze_y);
        add_buf(buffer, buf);

        if (bs->maze_templates && list_size(bs->maze_templates) > 0) {
            ITERATOR it;
            MAZE_WEIGHTED_ROOM *mwr;
            int idx = 0;
            add_buf(buffer, "{YMaze Templates:{x\n\r");
            iterator_start(&it, bs->maze_templates);
            while ((mwr = (MAZE_WEIGHTED_ROOM *)iterator_nextdata(&it))) {
                ++idx;
                sprintf(buf, "  {Y[{W%3d{Y]{x Weight: {W%3d{x  Exits: {W%s{x  Room: {W%ld{x %s\n\r",
                    idx, mwr->weight,
                    mwr->exit_count == 0 ? "Any" :
                    mwr->exit_count == 1 ? " 1 " :
                    mwr->exit_count == 2 ? " 2 " :
                    mwr->exit_count == 3 ? " 3 " : " 4 ",
                    mwr->room ? mwr->room->vnum : mwr->room_ref.load.vnum,
                    mwr->room ? mwr->room->name : "(unresolved)");
                add_buf(buffer, buf);
            }
            iterator_stop(&it);
            sprintf(buf, "  Total Weight: {W%d{x\n\r", bs->total_maze_weight);
            add_buf(buffer, buf);
        } else {
            add_buf(buffer, "{YMaze Templates:{x (none)\n\r");
        }

        if (bs->maze_fixed_rooms && list_size(bs->maze_fixed_rooms) > 0) {
            ITERATOR it;
            MAZE_FIXED_ROOM *mfr;
            int idx = 0;
            add_buf(buffer, "{YMaze Fixed Rooms:{x\n\r");
            iterator_start(&it, bs->maze_fixed_rooms);
            while ((mfr = (MAZE_FIXED_ROOM *)iterator_nextdata(&it))) {
                ++idx;
                sprintf(buf, "  {Y[{W%3d{Y]{x Pos: ({W%d{x,{W%d{x)  Room: {W%ld{x %s  Connected: %s\n\r",
                    idx, mfr->x, mfr->y,
                    mfr->room ? mfr->room->vnum : mfr->room_ref.load.vnum,
                    mfr->room ? mfr->room->name : "(unresolved)",
                    mfr->connected ? "{GYes{x" : "{RNo{x");
                add_buf(buffer, buf);
            }
            iterator_stop(&it);
        } else {
            add_buf(buffer, "{YMaze Fixed Rooms:{x (none)\n\r");
        }
        add_buf(buffer, "\n\r");
    }

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
    AREA_DATA *pArea;
    long  value;
    int  iHash;

    // Auto-vnum: empty or "0" finds next available in current area
    if (argument[0] == '\0' || !str_cmp(argument, "0"))
    {
        pArea = ch->in_room->area;
        value = 0;

        for (long try_vnum = 1; try_vnum < MAX_KEY_HASH * 100; try_vnum++)
        {
            if (!get_blueprint_section_for_area(pArea, try_vnum))
            {
                value = try_vnum;
                break;
            }
        }

        if (value == 0)
        {
            send_to_char("BSEdit: Could not find an available vnum in this area.\n\r", ch);
            return false;
        }
    }
    else
    {
        WNUM bs_wnum;
        if (!parse_widevnum(argument, ch->in_room->area, &bs_wnum)) {
            send_to_char("BSEdit: Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        value = bs_wnum.vnum;
        pArea = bs_wnum.pArea;
    }

    if (!pArea)
    {
        send_to_char("BSEdit: That vnum is not assigned an area.\n\r", ch);
        return false;
    }

    if (!IS_BUILDER(ch, pArea))
    {
        send_to_char("BSEdit: Vnum in an area you cannot build in.\n\r", ch);
        return false;
    }

    if (get_blueprint_section_for_area(pArea, value))
    {
        send_to_char("BSEdit: That vnum already exists.\n\r", ch);
        return false;
    }

    bs = new_blueprint_section();
    bs->vnum = value;
    bs->area = pArea;

    iHash                                   = bs->vnum % MAX_KEY_HASH;
    bs->next                                = pArea->blueprint_section_hash[iHash];
    pArea->blueprint_section_hash[iHash]    = bs;
    ch->desc->pEdit                         = (void *)bs;

    if (bs->vnum > top_blueprint_section_vnum)
        top_blueprint_section_vnum = bs->vnum;

    send_to_char("Blueprint Section Created.\n\r", ch);
    SET_BIT(pArea->area_flags, AREA_CHANGED);
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
    sprintf(buf, "Recall set to %.30s (%s)\n\r", room->name, widevnum_string_room(room, bs->area));
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
        send_to_char("Syntax:  rooms <lower vnum> <upper vnum>\n\r", ch);
        send_to_char("{YVnums must be in the same area. Use #vnum or area#vnum format.{x\n\r", ch);
        return false;
    }

    WNUM wnum_lower, wnum_upper;
    AREA_DATA *context = ch->in_room->area;

    if (!parse_widevnum(arg, context, &wnum_lower) || !wnum_lower.pArea) {
        send_to_char("Invalid lower vnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    if (!parse_widevnum(argument, context, &wnum_upper) || !wnum_upper.pArea) {
        send_to_char("Invalid upper vnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    if (wnum_lower.pArea != wnum_upper.pArea) {
        send_to_char("Lower and upper vnums must be in the same area.\n\r", ch);
        return false;
    }

    lvnum = wnum_lower.vnum;
    uvnum = wnum_upper.vnum;

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
        if( (bs->recall_room ? bs->recall_room->vnum : bs->recall_ref.load.vnum) > 0 )
        {
            if( (bs->recall_room ? bs->recall_room->vnum : bs->recall_ref.load.vnum) < lvnum || (bs->recall_room ? bs->recall_room->vnum : bs->recall_ref.load.vnum) > uvnum )
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

                if( (cur->room ? cur->room->vnum : cur->room_ref.load.vnum) < lvnum || (cur->room ? cur->room->vnum : cur->room_ref.load.vnum) > uvnum )
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

BSEDIT( bsedit_maze )
{
    BLUEPRINT_SECTION *bs;
    char arg[MIL];
    char arg2[MIL];

    EDIT_BPSECT(ch, bs);

    if (argument[0] == '\0') {
        send_to_char("Syntax:  maze size <width> <height>\n\r", ch);
        send_to_char("         maze templates list\n\r", ch);
        send_to_char("         maze templates add <weight> <room_vnum> [exit_count]\n\r", ch);
        send_to_char("         maze templates remove <#>\n\r", ch);
        send_to_char("         maze fixed list\n\r", ch);
        send_to_char("         maze fixed add <x> <y> <room_vnum> [connected]\n\r", ch);
        send_to_char("         maze fixed remove <#>\n\r", ch);
        send_to_char("\n\rExit Count: 0=any, 1=dead end, 2=tunnel, 3=fork, 4=crossroads\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (!str_cmp(arg, "size")) {
        if (arg2[0] == '\0' || argument[0] == '\0') {
            send_to_char("Syntax:  maze size <width> <height>\n\r", ch);
            return false;
        }

        int w = atoi(arg2);
        int h = atoi(argument);

        if (w < 1 || h < 1 || w > 100 || h > 100) {
            send_to_char("Maze dimensions must be between 1 and 100.\n\r", ch);
            return false;
        }

        bs->maze_x = w;
        bs->maze_y = h;

        // Clear fixed rooms since coordinates may be invalid
        if (bs->maze_fixed_rooms) {
            MAZE_FIXED_ROOM *mfr;
            while ((mfr = (MAZE_FIXED_ROOM *)list_nthdata(bs->maze_fixed_rooms, 1))) {
                list_remnthlink(bs->maze_fixed_rooms, 1, false);
                free_maze_fixed_room(mfr);
            }
        }

        char buf[MSL];
        sprintf(buf, "Maze size set to %d x %d. Fixed rooms cleared.\n\r", w, h);
        send_to_char(buf, ch);
        return true;
    }

    if (!str_cmp(arg, "templates")) {
        if (!str_cmp(arg2, "list")) {
            if (!bs->maze_templates || list_size(bs->maze_templates) < 1) {
                send_to_char("No maze templates defined.\n\r", ch);
                return false;
            }

            ITERATOR it;
            MAZE_WEIGHTED_ROOM *mwr;
            char buf[MSL];
            int idx = 0;

            send_to_char("{YMaze Templates:{x\n\r", ch);
            iterator_start(&it, bs->maze_templates);
            while ((mwr = (MAZE_WEIGHTED_ROOM *)iterator_nextdata(&it))) {
                ++idx;
                sprintf(buf, "  {Y[{W%3d{Y]{x Weight: {W%3d{x  Exits: {W%s{x  Room: {W%ld{x %s\n\r",
                    idx, mwr->weight,
                    mwr->exit_count == 0 ? "Any" :
                    mwr->exit_count == 1 ? " 1 " :
                    mwr->exit_count == 2 ? " 2 " :
                    mwr->exit_count == 3 ? " 3 " : " 4 ",
                    mwr->room ? mwr->room->vnum : mwr->room_ref.load.vnum,
                    mwr->room ? mwr->room->name : "(unresolved)");
                send_to_char(buf, ch);
            }
            iterator_stop(&it);

            sprintf(buf, "  Total Weight: {W%d{x\n\r", bs->total_maze_weight);
            send_to_char(buf, ch);
            return false;
        }

        if (!str_cmp(arg2, "add")) {
            char weight_arg[MIL];
            argument = one_argument(argument, weight_arg);

            char vnum_arg[MIL];
            argument = one_argument(argument, vnum_arg);

            if (weight_arg[0] == '\0' || vnum_arg[0] == '\0') {
                send_to_char("Syntax:  maze templates add <weight> <room_vnum> [exit_count]\n\r", ch);
                return false;
            }

            int weight = atoi(weight_arg);
            if (weight < 1) {
                send_to_char("Weight must be at least 1.\n\r", ch);
                return false;
            }

            int exit_count = 0;
            if (argument[0] != '\0') {
                exit_count = atoi(argument);
                if (exit_count < 0 || exit_count > 4) {
                    send_to_char("Exit count must be 0-4 (0=any, 1=dead end, 2=tunnel, 3=fork, 4=crossroads).\n\r", ch);
                    return false;
                }
            }

            WNUM room_wnum;
            AREA_DATA *context = strchr(vnum_arg, '#') ? NULL : bs->area;
            if (!parse_widevnum(vnum_arg, context, &room_wnum)) {
                send_to_char("Invalid widevnum format.\n\r", ch);
                return false;
            }

            ROOM_INDEX_DATA *room = get_room_index(room_wnum.pArea, room_wnum.vnum);
            if (!room) {
                send_to_char("That room does not exist.\n\r", ch);
                return false;
            }

            MAZE_WEIGHTED_ROOM *mwr = new_maze_weighted_room();
            mwr->weight = weight;
            mwr->exit_count = exit_count;
            mwr->room_ref.load.vnum = room_wnum.vnum;
            mwr->room_ref.load.auid = room_wnum.pArea ? room_wnum.pArea->uid : 0;
            mwr->room = room;

            if (!bs->maze_templates)
                bs->maze_templates = list_create(false);

            list_appendlink(bs->maze_templates, mwr);
            bs->total_maze_weight += weight;

            char buf[MSL];
            sprintf(buf, "Template added: Room %ld (%s) with weight %d, exits %s.\n\r",
                room->vnum, room->name, weight,
                exit_count == 0 ? "any" :
                exit_count == 1 ? "1 (dead end)" :
                exit_count == 2 ? "2 (tunnel)" :
                exit_count == 3 ? "3 (fork)" : "4 (crossroads)");
            send_to_char(buf, ch);
            return true;
        }

        if (!str_cmp(arg2, "remove")) {
            if (argument[0] == '\0') {
                send_to_char("Syntax:  maze templates remove <#>\n\r", ch);
                return false;
            }

            int idx = atoi(argument);
            MAZE_WEIGHTED_ROOM *mwr = (MAZE_WEIGHTED_ROOM *)list_nthdata(bs->maze_templates, idx);
            if (!mwr) {
                send_to_char("Invalid template number.\n\r", ch);
                return false;
            }

            bs->total_maze_weight -= mwr->weight;
            list_remnthlink(bs->maze_templates, idx, false);
            free_maze_weighted_room(mwr);

            send_to_char("Template removed.\n\r", ch);
            return true;
        }

        send_to_char("Syntax:  maze templates list|add|remove\n\r", ch);
        return false;
    }

    if (!str_cmp(arg, "fixed")) {
        if (!str_cmp(arg2, "list")) {
            if (!bs->maze_fixed_rooms || list_size(bs->maze_fixed_rooms) < 1) {
                send_to_char("No fixed rooms defined.\n\r", ch);
                return false;
            }

            ITERATOR it;
            MAZE_FIXED_ROOM *mfr;
            char buf[MSL];
            int idx = 0;

            send_to_char("{YMaze Fixed Rooms:{x\n\r", ch);
            iterator_start(&it, bs->maze_fixed_rooms);
            while ((mfr = (MAZE_FIXED_ROOM *)iterator_nextdata(&it))) {
                ++idx;
                sprintf(buf, "  {Y[{W%3d{Y]{x Pos: ({W%d{x,{W%d{x)  Room: {W%ld{x %s  Connected: %s\n\r",
                    idx, mfr->x, mfr->y,
                    mfr->room ? mfr->room->vnum : mfr->room_ref.load.vnum,
                    mfr->room ? mfr->room->name : "(unresolved)",
                    mfr->connected ? "{GYes{x" : "{RNo{x");
                send_to_char(buf, ch);
            }
            iterator_stop(&it);
            return false;
        }

        if (!str_cmp(arg2, "add")) {
            char x_arg[MIL], y_arg[MIL], vnum_arg[MIL];
            argument = one_argument(argument, x_arg);
            argument = one_argument(argument, y_arg);
            argument = one_argument(argument, vnum_arg);

            if (x_arg[0] == '\0' || y_arg[0] == '\0' || vnum_arg[0] == '\0') {
                send_to_char("Syntax:  maze fixed add <x> <y> <room_vnum> [connected]\n\r", ch);
                return false;
            }

            int x = atoi(x_arg);
            int y = atoi(y_arg);

            if (bs->maze_x < 1 || bs->maze_y < 1) {
                send_to_char("Set maze size first.\n\r", ch);
                return false;
            }

            if (x < 1 || x > bs->maze_x || y < 1 || y > bs->maze_y) {
                char buf[MSL];
                sprintf(buf, "Coordinates must be within 1..%ld, 1..%ld.\n\r", bs->maze_x, bs->maze_y);
                send_to_char(buf, ch);
                return false;
            }

            WNUM room_wnum;
            AREA_DATA *context = strchr(vnum_arg, '#') ? NULL : bs->area;
            if (!parse_widevnum(vnum_arg, context, &room_wnum)) {
                send_to_char("Invalid widevnum format.\n\r", ch);
                return false;
            }

            ROOM_INDEX_DATA *room = get_room_index(room_wnum.pArea, room_wnum.vnum);
            if (!room) {
                send_to_char("That room does not exist.\n\r", ch);
                return false;
            }

            bool connected = true;
            if (argument[0] != '\0') {
                if (!str_cmp(argument, "connected") || !str_cmp(argument, "true") || !str_cmp(argument, "yes"))
                    connected = true;
                else if (!str_cmp(argument, "disconnected") || !str_cmp(argument, "false") || !str_cmp(argument, "no"))
                    connected = false;
            }

            MAZE_FIXED_ROOM *mfr = new_maze_fixed_room();
            mfr->x = x;
            mfr->y = y;
            mfr->connected = connected;
            mfr->room_ref.load.vnum = room_wnum.vnum;
            mfr->room_ref.load.auid = room_wnum.pArea ? room_wnum.pArea->uid : 0;
            mfr->room = room;

            if (!bs->maze_fixed_rooms)
                bs->maze_fixed_rooms = list_create(false);

            list_appendlink(bs->maze_fixed_rooms, mfr);

            char buf[MSL];
            sprintf(buf, "Fixed room added at (%d,%d): Room %ld (%s), connected: %s.\n\r",
                x, y, room->vnum, room->name, connected ? "yes" : "no");
            send_to_char(buf, ch);
            return true;
        }

        if (!str_cmp(arg2, "remove")) {
            if (argument[0] == '\0') {
                send_to_char("Syntax:  maze fixed remove <#>\n\r", ch);
                return false;
            }

            int idx = atoi(argument);
            MAZE_FIXED_ROOM *mfr = (MAZE_FIXED_ROOM *)list_nthdata(bs->maze_fixed_rooms, idx);
            if (!mfr) {
                send_to_char("Invalid fixed room number.\n\r", ch);
                return false;
            }

            list_remnthlink(bs->maze_fixed_rooms, idx, false);
            free_maze_fixed_room(mfr);

            send_to_char("Fixed room removed.\n\r", ch);
            return true;
        }

        send_to_char("Syntax:  maze fixed list|add|remove\n\r", ch);
        return false;
    }

    bsedit_maze(ch, "");
    return false;
}