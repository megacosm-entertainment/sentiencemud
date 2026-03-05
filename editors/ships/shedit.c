/***************************************************************************
 *  shedit.c — OLC Ship Template Editor                                   *
 *                                                                         *
 *  In-game editor for SHIP_INDEX_DATA definitions. Allows immortals to   *
 *  create, modify, and manage ship template properties including class,  *
 *  capacity, weapons, crew, and blueprints.                               *
 *                                                                         *
 *  Also contains non-editor functions: do_shshow (show without editing). *
 *                                                                         *
 *  Migrated to the unified OLC Editor Framework.                          *
 ***************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include "../strings.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../scripts.h"
#include "../../wilds.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

extern bool ships_changed;
extern long top_ship_index_vnum;
extern void list_ship_indexes(CHAR_DATA *ch, char *argument);
extern bool can_edit_ships(CHAR_DATA *ch);

/***************************************************************************
 * Permission & Change Tracking                                            *
 ***************************************************************************/

/**
 * shedit_check_perm - Custom permission check wrapper for can_edit_ships
 *
 * @param ch     Character to check
 * @param pEdit  Ship being edited (unused)
 * @return       true if character can edit ships
 */
static bool shedit_check_perm(CHAR_DATA *ch, void *pEdit)
{
    (void)pEdit;
    return can_edit_ships(ch);
}

/**
 * shedit_mark_changed - Mark ship data as needing save
 *
 * Sets both the ship's area AREA_CHANGED flag and the global
 * ships_changed flag for the deferred save cycle.
 *
 * @param ch     Character who made the change
 * @param pEdit  SHIP_INDEX_DATA being edited
 */
static void shedit_mark_changed(CHAR_DATA *ch, void *pEdit)
{
    (void)ch;
    SHIP_INDEX_DATA *ship = (SHIP_INDEX_DATA *)pEdit;
    if (ship && ship->area)
        SET_BIT(ship->area->area_flags, AREA_CHANGED);
    ships_changed = true;
}

/***************************************************************************
 * Command Table                                                           *
 ***************************************************************************/

const struct olc_cmd_type shedit_table[] =
{
    { "?",          show_help       },
    { "armor",      shedit_armor    },
    { "blueprint",  shedit_blueprint},
    { "capacity",   shedit_capacity },
    { "class",      shedit_class    },
    { "commands",   show_commands   },
    { "create",     shedit_create   },
    { "crew",       shedit_crew     },
    { "desc",       shedit_desc     },
    { "flags",      shedit_flags    },
    { "guns",       shedit_guns     },
    { "hit",        shedit_hit      },
    { "keys",       shedit_keys     },
    { "list",       shedit_list     },
    { "move",       shedit_move     },
    { "name",       shedit_name     },
    { "oars",       shedit_oars     },
    { "object",     shedit_object   },
    { "show",       shedit_show     },
    { "turning",    shedit_turning  },
    { "weight",     shedit_weight   },
    { NULL,         0               }
};

/***************************************************************************
 * Editor Definition                                                       *
 ***************************************************************************/

static const OLC_EDITOR_DEF shedit_def = {
    .name           = "SHEdit",
    .editor_type    = ED_SHIP,
    .cmd_table      = shedit_table,
    .show_fn        = shedit_show,
    .tabs           = { .count = 0 },
    .theme          = &olc_theme_building,
    .perm           = {
        .flags          = OLC_PERM_CUSTOM,
        .check_fn       = shedit_check_perm
    },
    .change_mode    = OLC_CHANGE_CUSTOM,
    .mark_changed_fn = shedit_mark_changed,
    .audit_changes  = false,
    .get_history_fn = NULL,
};

/***************************************************************************
 * Entry Point                                                             *
 ***************************************************************************/

/**
 * do_shedit - Enter ship template OLC editor
 *
 * Opens a ship template for editing by VNUM. Creates new template
 * if 'create' is specified.
 *
 * Syntax: shedit <vnum>
 *         shedit create <vnum>
 *
 * @param ch        Staff member
 * @param argument  Ship VNUM or 'create'
 */
void do_shedit(CHAR_DATA *ch, char *argument)
{
    SHIP_INDEX_DATA *ship = NULL;
    char arg[MAX_STRING_LENGTH];
    WNUM wnum;

    if (IS_NPC(ch))
        return;

    if (!olc_editor_check_perm(ch, &shedit_def, NULL)) {
        send_to_char("SHEdit: Insufficient security to edit ships.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg);

    if (parse_widevnum(arg, ch->in_room->area, &wnum)) {
        if (!(ship = get_ship_index_for_area(wnum.pArea, wnum.vnum))) {
            send_to_char("SHEdit: That ship does not exist.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &shedit_def, ship, false);
        return;
    }

    if (!str_cmp(arg, "create")) {
        if (shedit_create(ch, argument)) {
            ships_changed = true;
            olc_editor_enter(ch, &shedit_def, ch->desc->pEdit, false);
        }
        return;
    }

    send_to_char("Syntax: shedit <#vnum|area_uid#vnum>\n\r"
                 "        shedit create <vnum>\n\r", ch);
}

/***************************************************************************
 * Interpreter                                                             *
 ***************************************************************************/

/**
 * shedit - Command interpreter for the ship template editor
 */
void shedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &shedit_def);
}

/**
 * do_shshow - Display ship template details without entering editor
 *
 * @param ch        Staff member
 * @param argument  Ship template VNUM
 */
void do_shshow(CHAR_DATA *ch, char *argument)
{
    SHIP_INDEX_DATA *ship;
    WNUM wnum;

    if (argument[0] == '\0') {
        send_to_char("Syntax:  shshow <vnum>\n\r", ch);
        return;
    }

    if (!parse_widevnum(argument, ch->in_room ? ch->in_room->area : NULL, &wnum)) {
        send_to_char("Invalid vnum format.\n\r", ch);
        return;
    }

    if (!(ship = get_ship_index_for_area(wnum.pArea, wnum.vnum))) {
        send_to_char("That ship does not exist.\n\r", ch);
        return;
    }

    olc_show_item(ch, (void *)ship, shedit_show, argument);
    return;
}

/***************************************************************************
 * Command Handlers                                                        *
 ***************************************************************************/

SHEDIT( shedit_list )
{
    list_ship_indexes(ch, argument);
    return false;
}

/**
 * shedit_show - Display current ship template data
 *
 * Uses the unified OLC display framework for consistent formatting.
 *
 * @param ch        Character viewing
 * @param argument  Unused
 * @return          false (no data changed)
 */
SHEDIT( shedit_show )
{
    SHIP_INDEX_DATA *ship;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&shedit_def);
    OLC_LAYOUT_CTX *ctx;

    EDIT_SHIP(ch, ship);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "SHEdit", ship->name,
        formatf("#%ld", ship->vnum), &shedit_def);

    olc_display_string(ctx, theme, "Name:", "name", ship->name);
    olc_display_type(ctx, theme, "Class:", "class",
        ship_class_types, ship->ship_class);
    olc_display_flags(ctx, theme, "Flags:", "flags",
        ship_flags, (long)ship->flags);

    olc_display_section(ctx, theme, "References");

    if (IS_VALID(ship->blueprint))
        olc_display_vnum(ctx, theme, "Blueprint:", "blueprint",
            ship->blueprint->vnum, ship->blueprint->name);
    else
        olc_display_string(ctx, theme, "Blueprint:", "blueprint", NULL);

    {
        OBJ_INDEX_DATA *obj = ship->ship_object;
        if (obj)
            olc_display_vnum(ctx, theme, "Ship Object:", "object",
                obj->vnum, obj->short_descr);
        else
            olc_display_string(ctx, theme, "Ship Object:", "object", NULL);
    }

    olc_display_section(ctx, theme, "Combat");
    olc_display_number(ctx, theme, "Hit Points:", "hit", ship->hit);
    olc_display_number(ctx, theme, "Max Guns:", "guns", ship->guns);
    olc_display_number(ctx, theme, "Base Armor:", "armor", ship->armor);

    olc_display_section(ctx, theme, "Crew & Movement");
    olc_display_pair(ctx, theme,
        "Min Crew:", "crew", formatf("%d", ship->min_crew),
        "Max Crew:", "crew", formatf("%d", ship->max_crew));
    olc_display_number(ctx, theme, "Oars:", "oars", ship->oars);
    olc_display_pair(ctx, theme,
        "Move Delay:", "move", formatf("%d", ship->move_delay),
        "Move Steps:", "move", formatf("%d", ship->move_steps));
    olc_display_number(ctx, theme, "Max Turning:", "turning", ship->turning);

    olc_display_section(ctx, theme, "Cargo");
    olc_display_number(ctx, theme, "Max Weight:", "weight", ship->weight);
    olc_display_number(ctx, theme, "Capacity:", "capacity", ship->capacity);

    olc_display_text(ctx, theme, "Description:", "desc", ship->description);

    /* Special Keys */
    olc_display_section(ctx, theme, "Special Keys");
    if (list_size(ship->special_keys) > 0) {
        ITERATOR it;
        OBJ_INDEX_DATA *key;
        int count = 0;

        iterator_start(&it, ship->special_keys);
        while ((key = (OBJ_INDEX_DATA *)iterator_nextdata(&it))) {
            olc_display_infof(ctx, theme, "{W%3d  {G%8ld  %s%s{x",
                ++count, key->vnum,
                key->item_type != ITEM_KEY ? "{R" : "{Y",
                key->short_descr);
        }
        iterator_stop(&it);
    } else {
        olc_display_infof(ctx, theme, "  None");
    }

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
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

/**
 * shedit_name - Set the ship template name
 */
SHEDIT( shedit_name )
{
    SHIP_INDEX_DATA *ship;
    EDIT_SHIP(ch, ship);
    return olc_cmd_string(ch, argument, "Name", NULL, &ship->name,
        OLC_STR_DEFAULT | OLC_STR_UTF8_RESTRICT, NULL, NULL);
}

/**
 * shedit_desc - Edit the ship description (opens string editor)
 */
SHEDIT( shedit_desc )
{
    SHIP_INDEX_DATA *ship;
    EDIT_SHIP(ch, ship);
    return olc_cmd_string_append(ch, argument, "Description", NULL,
        &ship->description, NULL, NULL);
}

/**
 * shedit_class - Set the ship class
 */
SHEDIT( shedit_class )
{
    SHIP_INDEX_DATA *ship;
    EDIT_SHIP(ch, ship);
    return olc_cmd_type_set(ch, argument, "Class",
        "Syntax: class <ship class>\n\rType '? shipclass' for a list.",
        &ship->ship_class, ship_class_types, NULL, NULL);
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
        send_to_char("Syntax:  blueprint <vnum|#vnum|area#vnum>\n\r", ch);
        return false;
    }

    WNUM bp_wnum;
    if (!parse_widevnum(argument, ch->in_room->area, &bp_wnum) || !bp_wnum.pArea) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    if( !(bp = get_blueprint_for_area(bp_wnum.pArea, bp_wnum.vnum)) )
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
                AREA_DATA *sect_area = section->rooms_area ? section->rooms_area : bp->area;
                for( long vnum = section->lower_vnum; vnum <= section->upper_vnum; vnum++)
                {
                    ROOM_INDEX_DATA *room = get_room_index(sect_area, vnum);

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
    AREA_DATA *context = olc_relative_widevnum_context(ship->area, argument);
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

/**
 * shedit_hit - Set the ship's maximum hit points
 */
SHEDIT( shedit_hit )
{
    SHIP_INDEX_DATA *ship;
    EDIT_SHIP(ch, ship);
    return olc_cmd_number(ch, argument, "Hit Points", NULL,
        &ship->hit, 1, SHIP_MAX_HIT, NULL, NULL);
}

/**
 * shedit_turning - Set the ship's maximum turning power in degrees
 */
SHEDIT( shedit_turning )
{
    SHIP_INDEX_DATA *ship;
    EDIT_SHIP(ch, ship);
    return olc_cmd_number(ch, argument, "Turning", NULL,
        &ship->turning, 1, 60, NULL, NULL);
}

/**
 * shedit_guns - Set the ship's maximum gun allowance
 */
SHEDIT( shedit_guns )
{
    SHIP_INDEX_DATA *ship;
    EDIT_SHIP(ch, ship);
    return olc_cmd_number(ch, argument, "Guns", NULL,
        &ship->guns, 0, SHIP_MAX_GUNS, NULL, NULL);
}


/**
 * shedit_oars - Set the number of oar positions
 */
SHEDIT( shedit_oars )
{
    SHIP_INDEX_DATA *ship;
    EDIT_SHIP(ch, ship);
    return olc_cmd_number(ch, argument, "Oars", NULL,
        &ship->oars, 0, INT_MAX, NULL, NULL);
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

/**
 * shedit_weight - Set the ship's maximum weight limit
 */
SHEDIT( shedit_weight )
{
    SHIP_INDEX_DATA *ship;
    EDIT_SHIP(ch, ship);
    return olc_cmd_number(ch, argument, "Weight", NULL,
        &ship->weight, 0, SHIP_MAX_WEIGHT, NULL, NULL);
}

/**
 * shedit_capacity - Set the ship's item capacity
 */
SHEDIT( shedit_capacity )
{
    SHIP_INDEX_DATA *ship;
    EDIT_SHIP(ch, ship);
    return olc_cmd_number(ch, argument, "Capacity", NULL,
        &ship->capacity, 0, SHIP_MAX_CAPACITY, NULL, NULL);
}

/**
 * shedit_armor - Set the ship's base armor rating
 */
SHEDIT( shedit_armor)
{
    SHIP_INDEX_DATA *ship;
    EDIT_SHIP(ch, ship);
    return olc_cmd_number(ch, argument, "Armor", NULL,
        &ship->armor, 0, SHIP_MAX_ARMOR, NULL, NULL);
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
            bool append_ok = true;
            char buf[MSL];
            int count = 0;

            if (!add_buf(buffer, "    [  Vnum  ]  Name\n\r")
                || !add_buf(buffer, "==============================================\n\r")) {
                send_to_char("Special key list output exceeded buffer limits.\n\r", ch);
                free_buf(buffer);
                return false;
            }

            iterator_start(&it, ship->special_keys);
            while( (key = (OBJ_INDEX_DATA *)iterator_nextdata(&it)) )
            {
                char key_color = 'Y';

                if( key->item_type != ITEM_KEY )
                {
                    key_color = 'R';
                }

                sprintf(buf, "{W%3d  {G%8ld  {%c%s{x\n\r", ++count, key->vnum, key_color, key->short_descr);
                if (!add_buf(buffer, buf))
                {
                    append_ok = false;
                    break;
                }
            }
            iterator_stop(&it);

            if (append_ok && (!add_buf(buffer, "==============================================\n\r")
                || !add_buf(buffer, "{RRED{x = not a key.\n\r")))
                append_ok = false;

            if (!append_ok)
            {
                send_to_char("Special key list output exceeded buffer limits.\n\r", ch);
                free_buf(buffer);
                return false;
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
        AREA_DATA *context = olc_relative_widevnum_context(ship->area, argument);
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