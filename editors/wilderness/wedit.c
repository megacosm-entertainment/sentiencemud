/***************************************************************************
 *  File: olc_act.c                                                        *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 *                                                                         *
 *  This code was freely distributed with the The Isles 1.1 source code,   *
 *  and has been used here for OLC - OLC would not be what it is without   *
 *  all the previous coders who released their source code.                *
 *                                                                         *
 ***************************************************************************/

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

extern void correct_vrooms(WILDS_DATA *pWilds, WILDS_TERRAIN *pTerrain);

WEDIT ( wedit_create )
{
	LLIST_WILDS_DATA *data;
    WILDS_DATA *pWilds, *pLastWilds;
    WILDS_TERRAIN *pTerrain;
    AREA_DATA *pArea;
    char arg1[MIL];
    char arg2[MIL];
    char *pMap, *pStaticMap;
    long lScount, lMapsize;

    pArea = ch->in_room->area;

    if (!IS_BUILDER(ch, pArea))
    {
        send_to_char("Wedit: you don't have OLC access to that area.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg1);
    one_argument(argument, arg2);

    if (IS_NULLSTR(arg1) || IS_NULLSTR(arg2))
    {
        send_to_char("Wedit (create): Usage:\n\r", ch);
        send_to_char("              :        create <map_size_x> <map_size_y>\n\r", ch);
        return false;
    }

    if (!is_number(arg1) || !is_number(arg2))
    {
        send_to_char("Wedit (create): Wilds map dimensions must be integers.\n\r", ch);
        send_to_char("              : Usage:\n\r", ch);
        send_to_char("              :        create <map_size_x> <map_size_y>\n\r", ch);
        return false;
    }

    pWilds = new_wilds();
    pWilds->pArea = pArea;
    pWilds->name = str_dup("New Wilds");
    pWilds->map_size_x = atoi(arg1);
    pWilds->map_size_y = atoi(arg2);
    lMapsize = pWilds->map_size_x * pWilds->map_size_y;
    pWilds->staticmap = calloc(sizeof(char), lMapsize);
    pWilds->map = calloc(sizeof(char), lMapsize);
    pWilds->uid = ++gconfig.next_wilds_uid;
    gconfig_write();

    pMap = pWilds->map;
    pStaticMap = pWilds->staticmap;

    if((data = alloc_mem(sizeof(LLIST_WILDS_DATA)))) {
		data->wilds = pWilds;
		data->uid = pWilds->uid;

		list_appendlink(loaded_wilds, pWilds);
	}

    for(lScount = 0;lScount < lMapsize; lScount++)
    {
        *pMap++ = 'S';
        *pStaticMap++ = 'S';
    }

    if (pArea->wilds)
    {
        pLastWilds = pArea->wilds;

        while(pLastWilds->next)
            pLastWilds = pLastWilds->next;

        plogf(LOG_INFO, "olc_act.c, wedit_create(): Adding Wilds to existing linked-list.");
        pLastWilds->next = pWilds;
    }
    else
    {
        plogf(LOG_INFO, "olc_act.c, wedit_create(): Adding first Wilds to linked-list.");
        pArea->wilds = pWilds;
    }

    send_to_char("Wedit: New Wilds Region created.\n\r", ch);
    pTerrain = new_terrain(pWilds);
    pTerrain->mapchar = 'S';
    pTerrain->showchar = str_dup("{B~");
    pWilds->pTerrain = pTerrain;
    send_to_char("Wedit: Default wilds terrain mapping completed.\n\r", ch);
    return true;
}

WEDIT ( wedit_delete )
{
    send_to_char("{x[{W wedit delete{x ] Not implemented yet.\n\r\n\r", ch);
    return false;
}

WEDIT ( wedit_show )
{
    WILDS_DATA *pWilds;
    WILDS_TERRAIN *pTerrain;
    char buf[MSL];
    int col = 0;

    pWilds = (WILDS_DATA *)ch->desc->pEdit;
    send_to_char("{x[ {Wwedit show{x ]\n\r\n\r", ch);

    sprintf(buf, "Wilds defined in area {W%ld{x, '{W%s{x'\n\r", pWilds->pArea->anum, pWilds->pArea->name);
    send_to_char( buf, ch );

    sprintf(buf, "Map size: [ {W%d {xx {W%d{x ] ({W%ld{x vrooms)\n\r",
                  pWilds->map_size_x, pWilds->map_size_y,
                  (long)(pWilds->map_size_x * pWilds->map_size_y));
    send_to_char( buf, ch );

    show_map_to_char(ch, ch, 3, 3, true);

    send_to_char("\n\r{C*Terrain Key*{x\n\r", ch);

    for(pTerrain=pWilds->pTerrain;pTerrain;pTerrain=pTerrain->next)
    {
        if (pTerrain->mapchar == pWilds->cDefaultTerrain)
        {
            /* Vizz - Handle colour code char exception before send_to_char() */
            if (pTerrain->mapchar == '{')
                sprintf(buf, "Default terrain: '{W{%c{x' '%s{x' {W%-12s{x\n\r\n\r",
                             pTerrain->mapchar, pTerrain->showchar,
                             pTerrain->showname ? pTerrain->showname : "(Not Set)");
            else
                sprintf(buf, "Default terrain: '{W%c{x' '%s{x' {W%-12s{x\n\r\n\r",
                             pTerrain->mapchar, pTerrain->showchar,
                             pTerrain->showname ? pTerrain->showname : "(Not Set)");

            send_to_char(buf, ch);
        }
    }

    send_to_char("Tile Ansi Name        Tile Ansi Name        Tile Ansi Name\n\r", ch);
    for(pTerrain=pWilds->pTerrain;pTerrain;pTerrain=pTerrain->next)
    {
        /* Vizz - Handle colour code char exception before send_to_char() */
        if (pTerrain->mapchar == '{')
            sprintf(buf, " '{W{%c{x'  '%s{x' {W%-12s{x{x",
                         pTerrain->mapchar, pTerrain->showchar,
                         pTerrain->showname ? pTerrain->showname : "(Not Set)");
        else
            sprintf(buf, " '{W%c{x'  '%s{x' {W%-12s{x{x",
                         pTerrain->mapchar, pTerrain->showchar,
                         pTerrain->showname ? pTerrain->showname : "(Not Set)");

        send_to_char(buf, ch);

        if (col++ % 3 == 2)
            send_to_char("\n\r", ch);

    }

    send_to_char("\n\r\n\r{C*Current State*{x\n\r", ch);

    sprintf(buf, "Players: {W%d{x\n\r", pWilds->nplayer);
    send_to_char( buf, ch );

    sprintf(buf, "Age: {W%d{x\n\r", pWilds->age);
    send_to_char( buf, ch );

    if (!IS_SET(ch->comm, COMM_COMPACT))
        send_to_char("\n\r", ch);

    return false;
}

WEDIT (wedit_name)
{
    WILDS_DATA *pWilds;

    EDIT_WILDS (ch, pWilds);

    if (argument[0] == '\0')
    {
        send_to_char ("Syntax:  name [name]\n\r", ch);
        return false;
    }

    free_string (pWilds->name);
    pWilds->name = str_dup (argument);

    send_to_char ("Wilds name set.\n\r", ch);
    return true;
}

WEDIT ( wedit_terrain )
{
    WILDS_DATA *pWilds;
    WILDS_TERRAIN *pTerrain;
    char token;
    char arg[MIL];
    char arg2[MIL];
    int value;

    EDIT_WILDS (ch, pWilds);

    if (argument[0] == '\0')
    {
        send_to_char ("Syntax:  terrain <token> create\n\r"
                      "         terrain <token> delete\n\r"
                      "         terrain <token> ansi <string>\n\r"
                      "         terrain <token> showname <string>\n\r"
                      "         terrain <token> briefdesc <string>\n\r"
                      "         terrain <token> room_flag <flag>\n\r"
                      "         terrain <token> room2flag <flag>\n\r"
                      "         terrain <token> sector <sector>\n\r"
                      "         terrain list <flag>\n\r", ch);
        return false;
    }

    argument = one_caseful_argument (argument, arg);
    argument = one_caseful_argument (argument, arg2);

    if (!str_cmp(arg, "list"))
    {
        BUFFER *output;
        char buf[MSL];

        output = new_buf();
        add_buf(output, "[{WWedit{x] Full Terrain List:\n\r\n\r");
        add_buf(output, "Token  Ansi  Showname        Sector          Nonroom?  Flags\n\r");

        for(pTerrain=pWilds->pTerrain;pTerrain;pTerrain=pTerrain->next)
        {
            sprintf(buf, " '{W%c{x'   '%s{x'   {W%-15s{x  {W%-15s{x  {W%s%s{x\n\r",
                     pTerrain->mapchar, pTerrain->showchar,
                     pTerrain->showname ? pTerrain->showname : "(Not Set)",
                     flag_string(sector_flags, pTerrain->template->sector_type),
                     pTerrain->nonroom ? "  Yes    " : "  No     ",
                     bitmatrix_string(room_flagbank, pTerrain->template->room_flag));
                     //flag_string(room2_flags, pTerrain->template->room_flag[1]));
            add_buf(output, buf);
        }

        send_to_char(buf_string(output), ch);
        free_buf(output);

        return false;
    }

    token = arg[0];
    pTerrain = get_terrain_by_token(pWilds, token);

    if (!str_cmp(arg2, "create"))
    {

        if (token == '\0')
        {
            send_to_char ("Syntax:\n\r         terrain <token> create\n\r", ch);
            return false;
        }

        if (pTerrain)
        {
            send_to_char ("[Wedit] That token has already been assigned.\n\r", ch);
            return false;
        }

        pTerrain = new_terrain(pWilds);
        add_terrain (pWilds, pTerrain);

        pTerrain->mapchar = token;
        pTerrain->showchar = str_dup(" ");
        pTerrain->showchar[0] = token;
        send_to_char ("[Wedit] Terrain token added.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg2, "delete"))
    {
        if (token == '\0')
        {
            send_to_char ("Syntax:\n\r         terrain <token> delete\n\r", ch);
            return false;
        }

        if (pTerrain == NULL)
        {
            send_to_char ("[Wedit] That token does not exist.\n\r", ch);
            return false;
        }

        if (token == pTerrain->pWilds->cDefaultTerrain)
        {
            send_to_char ("[Wedit] You can't delete the default terrain.\n\r", ch);
            return false;
        }

        plogf (LOG_INFO, "Deleting terrain struct for token '%c'", token);
        del_terrain (pWilds, pTerrain);
        send_to_char ("[Wedit] Terrain token deleted.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg2, "ansi"))
    {
        if (pTerrain == NULL)
        {
            send_to_char ("[Wedit] That token does not exist.\n\r", ch);
            return false;
        }

        if (argument[0] == '\0')
        {
            send_to_char ("Syntax:  terrain <token> ansi <string>\n\r", ch);
            return false;
        }

        free_string (pTerrain->showchar);
        pTerrain->showchar = str_dup (argument);

        send_to_char ("[Wedit] Terrain ansi set.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg2, "showname"))
    {
        if (pTerrain == NULL)
        {
            send_to_char ("[Wedit] That token does not exist.\n\r", ch);
            return false;
        }

        if (argument[0] == '\0')
        {
            send_to_char ("Syntax:  terrain <token> showname <name>\n\r", ch);
            return false;
        }

        free_string (pTerrain->showname);
        pTerrain->showname = str_dup (argument);

        free_string (pTerrain->template->name);
        pTerrain->template->name = str_dup (argument);

	correct_vrooms(pWilds, pTerrain);

        send_to_char ("[Wedit] Terrain showname set.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg2, "briefdesc"))
    {
        if (pTerrain == NULL)
        {
            send_to_char ("[Wedit] That token does not exist.\n\r", ch);
            return false;
        }

        if (argument[0] == '\0')
        {
            send_to_char ("Syntax:  terrain <token> briefdesc <string>\n\r", ch);
            return false;
        }

        free_string (pTerrain->showname);
        pTerrain->showname = str_dup (argument);

        send_to_char ("[Wedit] Terrain briefdesc set.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg2, "room_flag"))
    {
	long room_flag[2];
	if (bitvector_lookup(argument, 2, room_flag, room_flags, room2_flags))
	{
		TOGGLE_BIT(pTerrain->template->room_flag[0], room_flag[0]);
		TOGGLE_BIT(pTerrain->template->room_flag[1], room_flag[1]);
		correct_vrooms(pWilds, pTerrain);
		send_to_char("Room flags toggled.\n\r", ch);
        return true;
	/*
	if ((value = flag_value(room_flags, argument)) == NO_FLAG)
	{
	    send_to_char("Syntax: terrain <token> room_flag <flag>\n\r", ch);
	    return false;
	}

	TOGGLE_BIT(pTerrain->template->room_flag[0], value);
	correct_vrooms(pWilds, pTerrain);
	send_to_char("Room flags toggled.\n\r", ch);
        return true;
	*/
    }
/*
    if (!str_cmp(arg2, "room2flag"))
    {
	if ((value = flag_value(room2_flags, argument)) == NO_FLAG)
	{
	    send_to_char("Syntax: terrain <token> room2flag <flag>\n\r", ch);
	    return false;
	}

	TOGGLE_BIT(pTerrain->template->room2_flags, value);
	correct_vrooms(pWilds, pTerrain);
	send_to_char("Room2 flags toggled.\n\r", ch);
        return true;
    }
*/
	}
    if (!str_cmp(arg2, "sector"))
    {
	if ((value = flag_value(sector_flags, argument)) == NO_FLAG)
	{
	    send_to_char("Syntax: terrain <token> sector <sector>\n\r", ch);
	    return false;
	}

	pTerrain->template->sector_type =  value;
	correct_vrooms(pWilds, pTerrain);
	send_to_char("Sector set.\n\r", ch);
        return true;
    }

    return false;
}

WEDIT ( wedit_vlink )
{
    WILDS_DATA *pWilds;
    WILDS_VLINK *pVLink;
    char arg[MIL],
         arg2[MIL],
         arg3[MIL],
         arg4[MIL],
         *argsave;

    EDIT_WILDS (ch, pWilds);

    argument = one_argument (argument, arg);
    argsave = argument = one_argument (argument, arg2);
    argument = one_argument (argument, arg3);
    argument = one_argument (argument, arg4);

    if (!arg[0])
    {
       send_to_char("Usage:\n\r", ch);
       send_to_char("       [wedit] vlink link <vlinknum>\n\r", ch);
       send_to_char("               - Applies a vlink into play.\n\r", ch);
       send_to_char("       [wedit] vlink unlink <vlinknum>\n\r", ch);
       send_to_char("               - Removes a vlink from play.\n\r", ch);
       send_to_char("       [wedit] vlink create [<x coor> <y coor> <direction>]\n\r", ch);
       send_to_char("               - Creates a new vlink (unlinked) at your wilds location.\n\r", ch);
       send_to_char("       [wedit] vlink linkage <vlinknum> <to_wilds|from_wilds|two_way>\n\r", ch);
       send_to_char("               - Sets both the current and default linkage of the unlinked vlink.\n\r", ch);
       send_to_char("       [wedit] vlink direction <vlinknum> <door>\n\r", ch);
       send_to_char("               - Change the direction of the unlinked vlink.\n\r", ch);
       send_to_char("       [wedit] vlink destination <vlinknum> <vnum>\n\r", ch);
       send_to_char("               - Sets the destination of the unlinked vlink.\n\r", ch);
       send_to_char("       [wedit] vlink location <vlinknum> <x> <y>\n\r", ch);
       send_to_char("               - Sets the location of the vlink.\n\r", ch);
       send_to_char("       [wedit] vlink maptile <vlinknum> <tile>\n\r", ch);
       send_to_char("               - Sets the maptile of the vlink.\n\r", ch);
       send_to_char("       [wedit] vlink delete <vlinknum>\n\r", ch);
       send_to_char("               - Deletes selected vlink.\n\r", ch);
       send_to_char("       [wedit] vlink list\n\r", ch);
       send_to_char("               - Lists the status of all vlinks in wilds you are in.\n\r", ch);
       return false;
    }

    if (!str_cmp(arg, "link"))
    {
        int vlnum = 0;

        if (!pWilds)
        {
            plogf(LOG_INFO, "wilds.c, wedit_vlink(): Failed to link vlink ch->in_wilds is NULL");
            return false;
        }

        if (!is_number(arg2) || (vlnum = atoi(arg2)) < 0)
        {
            send_to_char("Wedit vlink link: Invalid vlinknum. Type 'vlink list' for valid vlinknums", ch);
            return false;
        }

        pVLink = get_vlink_from_index(pWilds,vlnum);
        if(pVLink) {
		if (link_vlink(pVLink)) {
			send_to_char("Wedit vlink: Found matching vlnum.\n\r", ch);
			return true;
		} else
			printf_to_char(ch, "Wedit vlink: Found vlink %d, but could not link it.\n\r", vlnum);
	} else
		send_to_char("Wedit vlink unlink: Could not find vlink. Try 'vlink show' for a list.", ch);

        return false;
    }

    if (!str_cmp(arg, "unlink"))
    {
        int vlnum = 0;

        if (!is_number(arg2) || (vlnum = atoi(arg2)) < 0)
        {
            send_to_char("Wedit vlink link: Invalid vlinknum. For usage, type 'vlink'", ch);
            return (false);
        }

        pVLink = get_vlink_from_index(pWilds,vlnum);
        if(pVLink) {
		if (unlink_vlink(pVLink)) {
			send_to_char("Wedit vlink: Found matching vlnum.\n\r", ch);
			return true;
		} else
			printf_to_char(ch, "Wedit vlink: Found vlink %d, but could not unlink it.\n\r", vlnum);
	} else
		send_to_char("Wedit vlink unlink: Could not find vlink. Try 'vlink show' for a list.", ch);

        return false;
    }

// VIZZWILDS - CURRENT WORK IN PROGRESS - CREATE ROUTINE ADDITION
    if (!str_cmp(arg, "create"))
    {
	    int x=0, y=0, door=0;
        WILDS_VLINK *temp_pVLink;

        if (arg2[0]) {
		if((!is_number(arg2) || (x = atoi(arg2)) < 0 || x > (pWilds->map_size_x - 1)) ||
			(!is_number(arg3) || (y = atoi(arg3)) < 0 || y > (pWilds->map_size_y - 1)) ||
			((door = parse_direction(arg4)) < 0))
	        {
	            send_to_char("Syntax: vlink create [<x coord> <y coord> <direction>]", ch);
	            return false;
		}
	}

        temp_pVLink = new_vlink();
        temp_pVLink->uid = ++gconfig.next_vlink_uid;	// Give it a UID
        temp_pVLink->wildsorigin_x = x;
        temp_pVLink->wildsorigin_y = y;
        temp_pVLink->door = door;
        temp_pVLink->map_tile = str_dup("{YO");
        temp_pVLink->orig_description = str_dup("");
        temp_pVLink->orig_keyword = str_dup("");
        temp_pVLink->rev_description = str_dup("");
        temp_pVLink->rev_keyword = str_dup("");
        add_vlink(pWilds, temp_pVLink);
        gconfig_write();				// Save the UIDs

        send_to_char("Wedit vlink create: Link created.\n\r", ch);
        return false;
    }

    if (!str_cmp(arg, "delete"))
    {
        send_to_char("Wedit vlink delete not implemented yet.\n\r", ch);
        return false;
    }

    if (!str_cmp(arg, "linkage"))
    {
        int vlnum = 0;

        if (!pWilds)
        {
            plogf(LOG_INFO, "wilds.c, wedit_vlink(): Failed to link vlink ch->in_wilds is NULL");
            return false;
        }

        if (!is_number(arg2) || (vlnum = atoi(arg2)) < 0)
        {
            send_to_char("Wedit vlink: Invalid vlinknum. Type 'vlink list' for valid vlinknums", ch);
            return false;
        }

        pVLink = get_vlink_from_index(pWilds,vlnum);
        if(pVLink) {
		if(pVLink->current_linkage == VLINK_UNLINKED) {
			if(!str_prefix(arg3,"to_wilds")) pVLink->default_linkage = VLINK_TO_WILDS;
			else if(!str_prefix(arg3,"from_wilds")) pVLink->default_linkage = VLINK_FROM_WILDS;
			else if(!str_prefix(arg3,"two_way")) pVLink->default_linkage = VLINK_TO_WILDS|VLINK_FROM_WILDS;
			else {
				printf_to_char(ch, "Wedit vlink: Invalid linkage.  Valid values are {Wto_wilds{x, {Wfrom_wilds{x and {Wtwo_way{x.\n\r", vlnum);
				return false;
			}
			send_to_char("Wedit vlink: Linkage set.\n\r", ch);
			return true;
		} else
			printf_to_char(ch, "Wedit vlink: Found vlink %d, but it needs to be unlinked first.\n\r", vlnum);
	} else
		send_to_char("Wedit vlink: Could not find vlink. Try 'vlink show' for a list.", ch);

        return false;
    }

    if (!str_cmp(arg, "direction"))
    {
        int vlnum = 0;
        int value;

        if (!pWilds)
        {
            plogf(LOG_INFO, "wilds.c, wedit_vlink(): Failed to link vlink ch->in_wilds is NULL");
            return false;
        }

        if (!is_number(arg2) || (vlnum = atoi(arg2)) < 0)
        {
            send_to_char("Wedit vlink: Invalid vlinknum. Type 'vlink list' for valid vlinknums", ch);
            return false;
        }

        pVLink = get_vlink_from_index(pWilds,vlnum);
        if(pVLink) {
		if(pVLink->current_linkage == VLINK_UNLINKED) {
			value = parse_direction(arg3);
			if(value >= 0) {
				pVLink->door = value;
				send_to_char("Wedit vlink: Door set.\n\r", ch);
				return true;
			} else
				printf_to_char(ch, "Wedit vlink: Invalid direction.\n\r", vlnum);

		} else
			printf_to_char(ch, "Wedit vlink: Found vlink %d, but it needs to be unlinked first.\n\r", vlnum);
	} else
		send_to_char("Wedit vlink: Could not find vlink. Try 'vlink show' for a list.", ch);

        return false;
    }

    if (!str_cmp(arg, "destination"))
    {
        int vlnum = 0;
        int value;

        if (!pWilds)
        {
            plogf(LOG_INFO, "wilds.c, wedit_vlink(): Failed to link vlink ch->in_wilds is NULL");
            return false;
        }

        if (!is_number(arg2) || (vlnum = atoi(arg2)) < 0)
        {
            send_to_char("Wedit vlink: Invalid vlinknum. Type 'vlink list' for valid vlinknums", ch);
            return false;
        }

        pVLink = get_vlink_from_index(pWilds,vlnum);
        if(pVLink) {
		if(pVLink->current_linkage == VLINK_UNLINKED) {
			if (is_number(arg3) && (value = atoi(arg3)) > 0) {
				ROOM_INDEX_DATA *destRoom = get_room_index(value);

				if( !destRoom )
				{
					send_to_char("Wedit vlink: Invalid destination.\n\r", ch);
					return false;
				}

				if( IS_SET(destRoom->room_flag[1], ROOM_BLUEPRINT) ||
					IS_SET(destRoom->area->area_flags, AREA_BLUEPRINT) )
				{
					send_to_char("Wedit vlink: Invalid destination.\n\r", ch);
					return false;
				}

				pVLink->destvnum = value;
				send_to_char("Wedit vlink: Destination set.\n\r", ch);
				return true;
			} else
				send_to_char("Wedit vlink: Invalid destination", ch);
		} else
			printf_to_char(ch, "Wedit vlink: Found vlink %d, but it needs to be unlinked first.\n\r", vlnum);
	} else
		send_to_char("Wedit vlink: Could not find vlink. Try 'vlink show' for a list.", ch);

        return false;
    }


    if (!str_cmp(arg, "location"))
    {
        int vlnum = 0;
        int x, y;

        if (!pWilds)
        {
            plogf(LOG_INFO, "wilds.c, wedit_vlink(): Failed to link vlink ch->in_wilds is NULL");
            return false;
        }

        if (!is_number(arg2) || (vlnum = atoi(arg2)) < 0)
        {
            send_to_char("Wedit vlink: Invalid vlinknum. Type 'vlink list' for valid vlinknums", ch);
            return false;
        }

        pVLink = get_vlink_from_index(pWilds,vlnum);
        if(pVLink) {
		if(pVLink->current_linkage == VLINK_UNLINKED) {
			if (is_number(arg3) && (x = atoi(arg3)) >= 0 && x < pWilds->map_size_x &&
				is_number(arg4) && (y = atoi(arg4)) >= 0 && y < pWilds->map_size_y) {
				pVLink->wildsorigin_x = x;
				pVLink->wildsorigin_y = y;
				send_to_char("Wedit vlink: Location set.\n\r", ch);
				return true;
			} else
				send_to_char("Wedit vlink: Invalid location", ch);
		} else
			printf_to_char(ch, "Wedit vlink: Found vlink %d, but it needs to be unlinked first.\n\r", vlnum);
	} else
		send_to_char("Wedit vlink: Could not find vlink. Try 'vlink show' for a list.", ch);

        return false;
    }

    if (!str_cmp(arg, "maptile"))
    {
        int vlnum = 0;

        if (!pWilds)
        {
            plogf(LOG_INFO, "wilds.c, wedit_vlink(): Failed to link vlink ch->in_wilds is NULL");
            return false;
        }

        if (!is_number(arg2) || (vlnum = atoi(arg2)) < 0)
        {
            send_to_char("Wedit vlink: Invalid vlinknum. Type 'vlink list' for valid vlinknums", ch);
            return false;
        }

        pVLink = get_vlink_from_index(pWilds,vlnum);
        if(pVLink) {
		if (argsave[0]) {
			free_string(pVLink->map_tile);
			pVLink->map_tile = str_dup(argsave);
			send_to_char("Wedit vlink: Map tile set.\n\r", ch);
			return true;
		} else
			send_to_char("Wedit vlink: Invalid maptile", ch);
	} else
		send_to_char("Wedit vlink: Could not find vlink. Try 'vlink show' for a list.", ch);

        return false;
    }

    if (!str_cmp(arg, "list"))
    {
	    int vlnum;
        if (!ch->in_room)
        {
            plogf(LOG_INFO, "wilds.c, vlinks(): ch->in_room invalid.");
            return false;
        }
        else
            if (!ch->in_room->area)
            {
                plogf(LOG_INFO, "wilds.c, vlinks(): ch->in_room->area invalid.");
                return false;
            }
            else
                if (!ch->in_room->area->wilds)
                {
                    plogf(LOG_INFO, "wilds.c, vlinks(): ch->in_room->area->wilds invalid.");
                    return false;
                }

        send_to_char("{x[ {Wwedit vlink{x ]\n\r\n\r", ch);
        send_to_char("[num] [uid]   [x coor] [y coor] [direction] "
                     "[destvnum] [default] [current] [maptile]\n\r", ch);

        for(vlnum = 0,pVLink=pWilds?pWilds->pVLink:ch->in_room->area->wilds->pVLink;pVLink!=NULL;pVLink = pVLink->next)
        {
            printf_to_char(ch, "%-5d ({W%6ld{x)  {W%6d   %6d   %-9s   %-8ld   %10s%10s%s{x\n\r",
            		   vlnum++,
                           pVLink->uid,
                           pVLink->wildsorigin_x,
                           pVLink->wildsorigin_y,
                           dir_name[pVLink->door],
                           pVLink->destvnum,
                           vlinkage_bit_name(pVLink->default_linkage),
                           vlinkage_bit_name(pVLink->current_linkage),
                           pVLink->map_tile);
        }

        if (!IS_SET(ch->comm, COMM_COMPACT))
        {
            send_to_char("\n\r", ch);
        }
    }

    return false;
}