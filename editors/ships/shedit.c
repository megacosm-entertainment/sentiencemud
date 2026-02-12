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
#include "../strings.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../scripts.h"
#include "../../wilds.h"

extern bool ships_changed;
extern long top_ship_index_vnum;
extern void list_ship_indexes(CHAR_DATA *ch, char *argument);

SHEDIT( shedit_list )
{
    list_ship_indexes(ch, argument);
    return false;
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

    OBJ_INDEX_DATA *obj = ship->ship_object;
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

    sprintf(buf, "Oars:        [%5d]{x\n\r", ship->oars);
    add_buf(buffer, buf);

    sprintf(buf, "Move Delay:  [%5d]{x\n\r", ship->move_delay);
    add_buf(buffer, buf);

    sprintf(buf, "Move Steps:  [%5d]{x\n\r", ship->move_steps);
    add_buf(buffer, buf);

    sprintf(buf, "Max Turning: %d degrees{x\n\r", ship->turning);
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
    return false;
}

SHEDIT( shedit_create )
{
    SHIP_INDEX_DATA *ship;
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
            if (!get_ship_index_for_area(pArea, try_vnum))
            {
                value = try_vnum;
                break;
            }
        }

        if (value == 0)
        {
            send_to_char("SHEdit: Could not find an available vnum in this area.\n\r", ch);
            return false;
        }
    }
    else
    {
        WNUM ship_wnum;
        if (!parse_widevnum(argument, ch->in_room->area, &ship_wnum)) {
            send_to_char("SHEdit: Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        value = ship_wnum.vnum;
        pArea = ship_wnum.pArea;
    }

    if (!pArea)
    {
        send_to_char("SHEdit: That vnum is not assigned an area.\n\r", ch);
        return false;
    }

    if (!IS_BUILDER(ch, pArea))
    {
        send_to_char("SHEdit: Vnum in an area you cannot build in.\n\r", ch);
        return false;
    }

    if (get_ship_index_for_area(pArea, value))
    {
        send_to_char("SHEdit: That vnum already exists.\n\r", ch);
        return false;
    }

    ship = new_ship_index();
    ship->vnum = value;
    ship->area = pArea;

    iHash                               = ship->vnum % MAX_KEY_HASH;
    ship->next                          = pArea->ship_index_hash[iHash];
    pArea->ship_index_hash[iHash]       = ship;
    ch->desc->pEdit                     = (void *)ship;

    if (ship->vnum > top_ship_index_vnum)
        top_ship_index_vnum = ship->vnum;

    send_to_char("Ship Created.\n\r", ch);
    SET_BIT(pArea->area_flags, AREA_CHANGED);
    return true;
}

SHEDIT( shedit_name )
{
    SHIP_INDEX_DATA *ship;

    EDIT_SHIP(ch, ship);

    smash_tilde(argument);

    if( argument[0] == '\0' )
    {
        send_to_char("Syntax:  name [name]\n\r", ch);
        return false;
    }

    free_string(ship->name);
    ship->name = str_dup(argument);
    send_to_char("Name changed.\n\r", ch);
    return true;
}

SHEDIT( shedit_desc )
{
    SHIP_INDEX_DATA *ship;

    EDIT_SHIP(ch, ship);

    if( argument[0] != '\0' )
    {
        send_to_char("Syntax:  desc\n\r", ch);
        return false;
    }

    string_append(ch, &ship->description);
    return true;
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
        return false;
    }

    ship->ship_class = value;
    send_to_char("Ship Class changed.\n\r", ch);
    return true;
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
        return false;
    }

    ship->flags ^= value;
    send_to_char("Ship flags changed.\n\r", ch);
    return true;
}

SHEDIT( shedit_blueprint )
{
    SHIP_INDEX_DATA *ship;
    BLUEPRINT *bp;

    EDIT_SHIP(ch, ship);

    if( argument[0] == '\0' )
    {
        send_to_char("Syntax:  blueprint [vnum]\n\r", ch);
        return false;
    }

    if( !is_number(argument) )
    {
        send_to_char("That is not a number.\n\r", ch);
        return false;
    }

    long bp_vnum = atol(argument);
    if( !(bp = get_blueprint(bp_vnum)) )
    {
        send_to_char("Blueprint does not exist.\n\r", ch);
        return false;
    }

    if( bp->mode == BLUEPRINT_MODE_STATIC )
    {

        // Verify the blueprint has certain features
        // * Has an entry room
        if( list_size(bp->_static.entries))
        {
            send_to_char("Blueprint requires an entry point for boarding purposes.\n\r", ch);
            return false;
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
            ROOM_INDEX_DATA *room = special_room->room ? special_room->room : (bp->area ? get_room_index(bp->area, special_room->room_ref.load.vnum) : NULL);

            if( room )
            {
                if( IS_SET(room->room_flag[0], ROOM_SHIP_HELM) )
                {
                    helm = true;
                }

                if( IS_SET(room->room_flag[0], ROOM_VIEWWILDS) )
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
                    ROOM_INDEX_DATA *room = get_room_index(bp->area, vnum);

                    if( room )
                    {
                        if( IS_SET(room->room_flag[0], ROOM_SHIP_HELM) )
                        {
                            helm = true;
                        }

                        if( IS_SET(room->room_flag[0], ROOM_VIEWWILDS) )
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
            return false;
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
        return false;
    }



    ship->blueprint = bp;
    send_to_char("Ship blueprint changed.\n\r", ch);
    return true;
}

SHEDIT( shedit_object )
{
    SHIP_INDEX_DATA *ship;
    OBJ_INDEX_DATA *obj;
    long vnum;

    EDIT_SHIP(ch, ship);

    if( argument[0] == '\0' )
    {
        send_to_char("Syntax:  object [widevnum]\n\r", ch);
        return false;
    }

    WNUM obj_wnum;
    AREA_DATA *context = strchr(argument, '#') ? ship->area : NULL;
    if (!parse_widevnum(argument, context, &obj_wnum)) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    vnum = obj_wnum.vnum;
    obj = get_obj_index(obj_wnum.pArea, obj_wnum.vnum);
    if( !obj )
    {
        send_to_char("That object does not exist.\n\r", ch);
        return false;
    }

    if( obj->item_type != ITEM_SHIP )
    {
        send_to_char("Object is not a ship.\n\r", ch);
        return false;
    }

    ship->ship_object_ref.vnum = vnum;
    ship->ship_object = obj;  // Set resolved pointer immediately
    send_to_char("Ship object set.\n\r", ch);
    return true;
}

SHEDIT( shedit_hit )
{
    SHIP_INDEX_DATA *ship;

    EDIT_SHIP(ch, ship);

    if( argument[0] == '\0' )
    {
        send_to_char("Syntax:  hit [points]\n\r", ch);
        return false;
    }

    if( !is_number(argument) )
    {
        send_to_char("That is not a number.\n\r", ch);
        return false;
    }

    int value = atoi(argument);
    if( value < 1 || value > SHIP_MAX_HIT )
    {
        send_to_char("Hit points must be in the range of 1 to " __STR(SHIP_MAX_HIT) ".\n\r", ch);
        return false;
    }

    ship->hit = value;
    send_to_char("Ship hit points changed.\n\r", ch);
    return true;
}

SHEDIT( shedit_turning )
{
    SHIP_INDEX_DATA *ship;

    EDIT_SHIP(ch, ship);

    if( argument[0] == '\0' )
    {
        send_to_char("Syntax:  turning [degrees]\n\r", ch);
        return false;
    }

    if( !is_number(argument) )
    {
        send_to_char("That is not a number.\n\r", ch);
        return false;
    }

    int value = atoi(argument);
    if( value < 1 || value > 60 )
    {
        send_to_char("Turning power must be in the range of 1 to 60 degrees.\n\r", ch);
        return false;
    }

    ship->turning = value;
    send_to_char("Turning power changed.\n\r", ch);
    return true;
}

SHEDIT( shedit_guns )
{
    SHIP_INDEX_DATA *ship;

    EDIT_SHIP(ch, ship);

    if( argument[0] == '\0' )
    {
        send_to_char("Syntax:  guns [count]\n\r", ch);
        return false;
    }

    if( !is_number(argument) )
    {
        send_to_char("That is not a number.\n\r", ch);
        return false;
    }

    int value = atoi(argument);
    if( value < 0 || value > SHIP_MAX_GUNS )
    {
        send_to_char("Gun allowance must be in the range of 0 to " __STR(SHIP_MAX_GUNS) ".\n\r", ch);
        return false;
    }

    ship->guns = value;
    send_to_char("Ship gun allowance changed.\n\r", ch);
    return true;
}


SHEDIT( shedit_oars )
{
    SHIP_INDEX_DATA *ship;

    EDIT_SHIP(ch, ship);

    if( argument[0] == '\0' )
    {
        send_to_char("Syntax:  oars [number]\n\r", ch);
        return false;
    }

    if( !is_number(argument) )
    {
        send_to_char("That is not a number.\n\r", ch);
        return false;
    }

    int value = atoi(argument);
    if( value < 0 )
    {
        send_to_char("Number of Oar positions must be non-negative.\n\r", ch);
        return false;
    }

    ship->oars = value;
    send_to_char("Oar positions changed.\n\r", ch);
    return true;
}


SHEDIT( shedit_crew )
{
    SHIP_INDEX_DATA *ship;
    char arg[MIL];

    EDIT_SHIP(ch, ship);

    if( argument[0] == '\0' )
    {
        send_to_char("Syntax:  crew [min] [max]\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if( !is_number(arg) ||  !is_number(argument) )
    {
        send_to_char("That is not a number.\n\r", ch);
        return false;
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
        return false;
    }

    if( max_crew < 0 || max_crew > SHIP_MAX_CREW )
    {
        send_to_char("Maximum crew allowance must be in the range of 0 to " __STR(SHIP_MAX_CREW) ".\n\r", ch);
        return false;
    }

    ship->min_crew = min_crew;
    ship->max_crew = max_crew;
    send_to_char("Ship crew allowance changed.\n\r", ch);
    return true;
}

SHEDIT( shedit_move )
{
    SHIP_INDEX_DATA *ship;
    char arg[MIL];

    EDIT_SHIP(ch, ship);

    if( argument[0] == '\0' )
    {
        send_to_char("Syntax:  move [delay] [steps]\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if( !is_number(arg) || !is_number(argument) )
    {
        send_to_char("That is not a number.\n\r", ch);
        return false;
    }

    int delay = atoi(arg);
    int steps = atoi(argument);

    if( delay < SHIP_MIN_DELAY )
    {
        send_to_char("Move delay must be at least " __STR(SHIP_MIN_DELAY) ".\n\r", ch);
        return false;
    }

    if( steps < SHIP_MIN_STEPS )
    {
        send_to_char("Move steps must be at least " __STR(SHIP_MIN_STEPS) ".\n\r", ch);
        return false;
    }

    ship->move_steps = steps;
    ship->move_delay = delay;
    send_to_char("Ship movement changed.\n\r", ch);
    return true;
}

SHEDIT( shedit_weight )
{
    SHIP_INDEX_DATA *ship;

    EDIT_SHIP(ch, ship);

    if( argument[0] == '\0' )
    {
        send_to_char("Syntax:  weight [weight]\n\r", ch);
        return false;
    }

    if( !is_number(argument) )
    {
        send_to_char("That is not a number.\n\r", ch);
        return false;
    }

    int value = atoi(argument);
    if( value < 0 || value > SHIP_MAX_WEIGHT )
    {
        send_to_char("Weight allowance must be in the range of 0 to " __STR(SHIP_MAX_WEIGHT) ".\n\r", ch);
        return false;
    }

    ship->weight = value;
    send_to_char("Ship weight allowance changed.\n\r", ch);
    return true;
}

SHEDIT( shedit_capacity )
{
    SHIP_INDEX_DATA *ship;

    EDIT_SHIP(ch, ship);

    if( argument[0] == '\0' )
    {
        send_to_char("Syntax:  capacity [count]\n\r", ch);
        return false;
    }

    if( !is_number(argument) )
    {
        send_to_char("That is not a number.\n\r", ch);
        return false;
    }

    int value = atoi(argument);
    if( value < 0 || value > SHIP_MAX_CAPACITY )
    {
        send_to_char("Ship capacity must be in the range of 0 to " __STR(SHIP_MAX_CAPACITY) ".\n\r", ch);
        return false;
    }

    ship->capacity = value;
    send_to_char("Ship capacity changed.\n\r", ch);
    return true;
}

SHEDIT( shedit_armor)
{
    SHIP_INDEX_DATA *ship;

    EDIT_SHIP(ch, ship);

    if( argument[0] == '\0' )
    {
        send_to_char("Syntax:  armor [rating]\n\r", ch);
        return false;
    }

    if( !is_number(argument) )
    {
        send_to_char("That is not a number.\n\r", ch);
        return false;
    }

    int value = atoi(argument);
    if( value < 0 || value > SHIP_MAX_ARMOR )
    {
        send_to_char("Ship base armor must be in the range of 0 to " __STR(SHIP_MAX_ARMOR) ".\n\r", ch);
        return false;
    }

    ship->armor = value;
    send_to_char("Ship base armor changed.\n\r", ch);
    return true;
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
        return false;
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

        return false;
    }

    if( !str_cmp(arg, "add") )
    {
        OBJ_INDEX_DATA *key;

        WNUM key_wnum;
        AREA_DATA *context = strchr(argument, '#') ? ship->area : NULL;
        if (!parse_widevnum(argument, context, &key_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        if( !(key = get_obj_index(key_wnum.pArea, key_wnum.vnum)) )
        {
            send_to_char("That object does not exist.\n\r", ch);
            return false;
        }

        if( key->item_type != ITEM_KEY )
        {
            send_to_char("That is not a key.\n\r", ch);
            return false;
        }

        if( list_hasdata(ship->special_keys, key) )
        {
            send_to_char("That key is already in the list.\n\r", ch);
            return false;
        }

        list_appendlink(ship->special_keys, key);
        send_to_char("Key added.\n\r", ch);
        return true;
    }

    if( !str_cmp(arg, "remove") )
    {
        if( !is_number(argument) )
        {
            send_to_char("That is not a number,\n\r", ch);
            return false;
        }

        int value = atoi(argument);
        if( value < 0 || value > list_size(ship->special_keys) )
        {
            send_to_char("Index out of range.\n\r", ch);
            return false;
        }

        list_remnthlink(ship->special_keys, value, true);
        send_to_char("Key removed.\n\r", ch);
        return true;
    }

    shedit_keys(ch, "");
    return false;

}