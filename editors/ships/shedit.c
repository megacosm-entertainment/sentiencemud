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

/* Forward declarations for commands defined below the command table */
SHEDIT( shedit_captain );
SHEDIT( shedit_crewmob );
SHEDIT( shedit_faction );
SHEDIT( shedit_factionrank );
SHEDIT( shedit_npctype );
SHEDIT( shedit_schedule );

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
    { "captain",    shedit_captain  },
    { "class",      shedit_class    },
    { "commands",   show_commands   },
    { "create",     shedit_create   },
    { "crew",       shedit_crew     },
    { "crewmob",    shedit_crewmob  },
    { "desc",       shedit_desc     },
    { "faction",    shedit_faction  },
    { "factionrank",shedit_factionrank },
    { "flags",      shedit_flags    },

    { "guns",       shedit_guns     },
    { "hardpoint",  shedit_hardpoint},
    { "hit",        shedit_hit      },
    { "keys",       shedit_keys     },
    { "list",       shedit_list     },
    { "moduleweight", shedit_moduleweight },
    { "move",       shedit_move     },
    { "name",       shedit_name     },
    { "npctype",    shedit_npctype  },
    { "oars",       shedit_oars     },
    { "object",     shedit_object   },
    { "schedule",   shedit_schedule },
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
    if (IS_SET(ship->flags, SHIP_AUTONOMOUS_NPC)) {
        olc_display_type(ctx, theme, "NPC Type:", "npctype",
            npc_ship_types, ship->npc_type);
        if (ship->faction)
            olc_display_infof(ctx, theme, "{xFaction:     {C%-6s {Y%s {x(rank {W%d{x)",
                "faction",
                ship->faction->name,
                ship->faction_rank);
        else
            olc_display_string(ctx, theme, "Faction:", "faction", NULL);
    }

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

    /* Hardpoints */
    olc_display_section(ctx, theme, "Hardpoints");
    olc_display_number(ctx, theme, "Module Weight Budget:", "moduleweight", ship->max_module_weight);
    if (ship->hardpoints && list_size(ship->hardpoints) > 0) {
        ITERATOR hp_it;
        SHIP_HARDPOINT_DEF *hp;
        int hp_num = 0;

        olc_display_infof(ctx, theme, "  {W%-4s %-20s %-10s %-8s %-20s %s{x",
            "Slot", "Name", "Type", "Size", "Domain", "Flags");

        iterator_start(&hp_it, ship->hardpoints);
        while ((hp = (SHIP_HARDPOINT_DEF *)iterator_nextdata(&hp_it))) {
            hp_num++;
            olc_display_infof(ctx, theme, "  {G%3d  {Y%-20s {C%-10s {M%-8s {W%-20s {x%s",
                hp->slot_id,
                hp->name ? hp->name : "(unnamed)",
                flag_string(hardpoint_types, hp->type),
                flag_string(hardpoint_sizes, hp->size),
                flag_string(domain_flags, hp->domain_flags),
                flag_string(hardpoint_flags, hp->flags));
        }
        iterator_stop(&hp_it);
    } else {
        olc_display_infof(ctx, theme, "  None");
    }

    /* Crew Mobs */
    olc_display_section(ctx, theme, "Crew Mobs");
    if (ship->captain) {
        olc_display_vnum(ctx, theme, "Captain:", "captain",
            ship->captain->vnum, ship->captain->short_descr);
    } else {
        olc_display_string(ctx, theme, "Captain:", "captain", NULL);
    }

    if (ship->crew_defs && list_size(ship->crew_defs) > 0) {
        ITERATOR cd_it;
        SHIP_CREW_DEF *cd;
        int cd_num = 0;

        olc_display_infof(ctx, theme, "  {W%-4s %-8s %-30s %s{x",
            "#", "Vnum", "Name", "Count");

        iterator_start(&cd_it, ship->crew_defs);
        while ((cd = (SHIP_CREW_DEF *)iterator_nextdata(&cd_it))) {
            cd_num++;
            olc_display_infof(ctx, theme, "  {G%3d  {W%-8ld {Y%-30s {Cx%d{x",
                cd_num,
                cd->mob ? cd->mob->vnum : cd->mob_ref.vnum,
                cd->mob ? cd->mob->short_descr : "(unresolved)",
                cd->count);
        }
        iterator_stop(&cd_it);
    } else {
        olc_display_infof(ctx, theme, "  None");
    }

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

    /* Transport Schedule */
    if (IS_SET(ship->flags, SHIP_TRANSPORT) && ship->schedule_stops) {
        extern char *const dir_name[];
        olc_display_section(ctx, theme, "Transport Schedule");
        olc_display_infof(ctx, theme, "  {xLoop: %s{x", ship->schedule_loop ? "{GYes" : "{RNo (ping-pong)");

        if (list_size(ship->schedule_stops) > 0) {
            ITERATOR sch_it;
            SHIP_SCHEDULE_STOP *stop;

            olc_display_infof(ctx, theme, "  {W%-3s %-20s %-12s %-16s %-6s %-6s %-8s %-5s %-8s{x",
                "ID", "Name", "Type", "Location", "Arr", "Dep", "Dwell", "Dir", "Dock");

            iterator_start(&sch_it, ship->schedule_stops);
            while ((stop = (SHIP_SCHEDULE_STOP *)iterator_nextdata(&sch_it))) {
                char loc_buf[64];
                if (stop->location_type == STOP_LOC_WILDERNESS) {
                    snprintf(loc_buf, sizeof(loc_buf), "W%ld:%d,%d",
                        stop->wilds_uid, stop->loc_x, stop->loc_y);
                } else {
                    if (stop->dock_room)
                        snprintf(loc_buf, sizeof(loc_buf), "[%ld] %.20s",
                            stop->dock_room->vnum, stop->dock_room->name);
                    else
                        snprintf(loc_buf, sizeof(loc_buf), "%ld#%ld",
                            stop->room_ref.load.auid, stop->room_ref.load.vnum);
                }

                char arr_buf[8], dep_buf[8], dwell_buf[8];
                if (stop->arrive_hour >= 0)
                    snprintf(arr_buf, sizeof(arr_buf), "%d", stop->arrive_hour);
                else
                    snprintf(arr_buf, sizeof(arr_buf), "-");
                if (stop->depart_hour >= 0)
                    snprintf(dep_buf, sizeof(dep_buf), "%d", stop->depart_hour);
                else
                    snprintf(dep_buf, sizeof(dep_buf), "-");
                snprintf(dwell_buf, sizeof(dwell_buf), "%d", stop->dwell_ticks);

                const char *dir_str = (stop->dock_exit_dir >= 0 && stop->dock_exit_dir < MAX_DIR)
                    ? dir_name[stop->dock_exit_dir] : "none";

                olc_display_infof(ctx, theme, "  {G%3d {Y%-20.20s {C%-12s {W%-16s {x%-6s %-6s %-8s {M%-5s {x%s",
                    stop->stop_id,
                    stop->name,
                    flag_string(schedule_loc_types, stop->location_type),
                    loc_buf,
                    arr_buf, dep_buf, dwell_buf,
                    dir_str,
                    flag_string(dock_exit_types, stop->dock_exit_type));
            }
            iterator_stop(&sch_it);
        } else {
            olc_display_infof(ctx, theme, "  No stops defined.");
        }
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
        OLC_STR_DEFAULT, NULL, NULL);
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

/**
 * shedit_moduleweight - Set the ship's module weight budget
 *
 * The total weight of all installed modules cannot exceed this value.
 * Set to 0 to disable module installation.
 *
 * Syntax: moduleweight <value>
 */
SHEDIT( shedit_moduleweight )
{
    SHIP_INDEX_DATA *ship;
    EDIT_SHIP(ch, ship);
    return olc_cmd_number(ch, argument, "Module Weight Budget", NULL,
        &ship->max_module_weight, 0, SHIP_MAX_MODULE_WEIGHT, NULL, NULL);
}

/**
 * shedit_captain - Set or clear the default captain mob for this ship hull
 *
 * Syntax: captain <mob widevnum>  - Set the default captain
 *         captain none            - Clear the captain
 */
SHEDIT( shedit_captain )
{
    SHIP_INDEX_DATA *ship;

    EDIT_SHIP(ch, ship);

    if (argument[0] == '\0') {
        send_to_char("Syntax:  captain <mob widevnum>\n\r", ch);
        send_to_char("         captain none\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "none") || !str_cmp(argument, "clear")) {
        ship->captain = NULL;
        memset(&ship->captain_ref, 0, sizeof(ship->captain_ref));
        send_to_char("Captain cleared.\n\r", ch);
        return true;
    }

    WNUM mob_wnum;
    AREA_DATA *context = olc_relative_widevnum_context(ship->area, argument);
    if (!parse_widevnum(argument, context, &mob_wnum)) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    MOB_INDEX_DATA *mob = get_mob_index(mob_wnum.pArea, mob_wnum.vnum);
    if (!mob) {
        send_to_char("That mobile does not exist.\n\r", ch);
        return false;
    }

    ship->captain = mob;
    ship->captain_ref.vnum = mob_wnum.vnum;
    send_to_char(formatf("Captain set to [%ld] %s.\n\r", mob->vnum, mob->short_descr), ch);
    return true;
}

/**
 * shedit_crewmob - Manage crew mob definitions on a ship hull
 *
 * Associates mob templates with counts to define which NPCs populate
 * the ship as crew. Used for NPC ships and for permanent "ghost crew".
 *
 * Subcommands:
 *   crewmob add <mob widevnum> [count]  - Add a crew mob entry
 *   crewmob remove <#>                  - Remove a crew mob entry by index
 *   crewmob <#> count <value>           - Change count for an entry
 *   crewmob <#> mob <mob widevnum>      - Change mob for an entry
 */
SHEDIT( shedit_crewmob )
{
    SHIP_INDEX_DATA *ship;
    char arg[MIL];

    EDIT_SHIP(ch, ship);

    if (argument[0] == '\0') {
        send_to_char("Syntax:  crewmob add <mob widevnum> [count]\n\r", ch);
        send_to_char("         crewmob remove <#>\n\r", ch);
        send_to_char("         crewmob <#> count <value>\n\r", ch);
        send_to_char("         crewmob <#> mob <mob widevnum>\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    /* crewmob add <mob widevnum> [count] */
    if (!str_cmp(arg, "add")) {
        char mob_arg[MIL];
        argument = one_argument(argument, mob_arg);

        if (mob_arg[0] == '\0') {
            send_to_char("Syntax:  crewmob add <mob widevnum> [count]\n\r", ch);
            return false;
        }

        WNUM mob_wnum;
        AREA_DATA *context = olc_relative_widevnum_context(ship->area, mob_arg);
        if (!parse_widevnum(mob_arg, context, &mob_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        MOB_INDEX_DATA *mob = get_mob_index(mob_wnum.pArea, mob_wnum.vnum);
        if (!mob) {
            send_to_char("That mobile does not exist.\n\r", ch);
            return false;
        }

        int count = 1;
        if (argument[0] != '\0') {
            if (!is_number(argument)) {
                send_to_char("Count must be a number.\n\r", ch);
                return false;
            }
            count = atoi(argument);
            if (count < 1 || count > 100) {
                send_to_char("Count must be between 1 and 100.\n\r", ch);
                return false;
            }
        }

        SHIP_CREW_DEF *cd = new_ship_crew_def();
        cd->mob = mob;
        cd->mob_ref.vnum = mob_wnum.vnum;
        cd->count = count;
        list_appendlink(ship->crew_defs, cd);

        send_to_char(formatf("Crew mob added: [%ld] %s x%d.\n\r",
            mob->vnum, mob->short_descr, count), ch);
        return true;
    }

    /* crewmob remove <#> */
    if (!str_cmp(arg, "remove")) {
        if (!is_number(argument)) {
            send_to_char("Syntax:  crewmob remove <#>\n\r", ch);
            return false;
        }

        int index = atoi(argument);
        if (index < 1 || index > list_size(ship->crew_defs)) {
            send_to_char("Index out of range.\n\r", ch);
            return false;
        }

        list_remnthlink(ship->crew_defs, index, true);
        send_to_char("Crew mob entry removed.\n\r", ch);
        return true;
    }

    /* crewmob <#> <field> <value> */
    if (!is_number(arg)) {
        send_to_char("Expected 'add', 'remove', or an entry number.\n\r", ch);
        return false;
    }

    int index = atoi(arg);
    if (index < 1 || index > list_size(ship->crew_defs)) {
        send_to_char("Index out of range.\n\r", ch);
        return false;
    }

    SHIP_CREW_DEF *target = (SHIP_CREW_DEF *)list_nthdata(ship->crew_defs, index);
    if (!target) {
        send_to_char("Crew mob entry not found.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    /* crewmob <#> count <value> */
    if (!str_cmp(arg, "count")) {
        if (!is_number(argument)) {
            send_to_char("Syntax:  crewmob <#> count <value>\n\r", ch);
            return false;
        }
        int count = atoi(argument);
        if (count < 1 || count > 100) {
            send_to_char("Count must be between 1 and 100.\n\r", ch);
            return false;
        }
        target->count = count;
        send_to_char(formatf("Crew mob count set to %d.\n\r", count), ch);
        return true;
    }

    /* crewmob <#> mob <mob widevnum> */
    if (!str_cmp(arg, "mob")) {
        if (argument[0] == '\0') {
            send_to_char("Syntax:  crewmob <#> mob <mob widevnum>\n\r", ch);
            return false;
        }

        WNUM mob_wnum;
        AREA_DATA *context = olc_relative_widevnum_context(ship->area, argument);
        if (!parse_widevnum(argument, context, &mob_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        MOB_INDEX_DATA *mob = get_mob_index(mob_wnum.pArea, mob_wnum.vnum);
        if (!mob) {
            send_to_char("That mobile does not exist.\n\r", ch);
            return false;
        }

        target->mob = mob;
        target->mob_ref.vnum = mob_wnum.vnum;
        send_to_char(formatf("Crew mob changed to [%ld] %s.\n\r",
            mob->vnum, mob->short_descr), ch);
        return true;
    }

    send_to_char("Unknown crewmob field. Use: count, mob\n\r", ch);
    return false;
}

/**
 * shedit_hardpoint - Manage hardpoint slots on a ship hull
 *
 * Subcommands:
 *   hardpoint add <name>                    - Add a new hardpoint slot
 *   hardpoint remove <slot#>                - Remove a hardpoint by slot number
 *   hardpoint <slot#> name <new name>       - Rename a hardpoint
 *   hardpoint <slot#> type <type>           - Set type (weapon/defense/utility/propulsion)
 *   hardpoint <slot#> size <size>           - Set size (small/medium/large)
 *   hardpoint <slot#> domain <flags>        - Set domain flags (aquatic/aerial/terrestrial)
 *   hardpoint <slot#> flags <flags>         - Toggle hardpoint flags (required/locked)
 */
SHEDIT( shedit_hardpoint )
{
    SHIP_INDEX_DATA *ship;
    char arg[MIL];

    EDIT_SHIP(ch, ship);

    if (argument[0] == '\0') {
        send_to_char("Syntax:  hardpoint add <name>\n\r", ch);
        send_to_char("         hardpoint remove <slot#>\n\r", ch);
        send_to_char("         hardpoint <slot#> name <new name>\n\r", ch);
        send_to_char("         hardpoint <slot#> type <weapon|defense|utility|propulsion>\n\r", ch);
        send_to_char("         hardpoint <slot#> size <small|medium|large>\n\r", ch);
        send_to_char("         hardpoint <slot#> domain <aquatic|aerial|terrestrial>\n\r", ch);
        send_to_char("         hardpoint <slot#> flags <required|locked>\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    /* hardpoint add <name> */
    if (!str_cmp(arg, "add")) {
        if (argument[0] == '\0') {
            send_to_char("Syntax:  hardpoint add <name>\n\r", ch);
            return false;
        }

        /* Find next available slot ID */
        int16_t next_slot = 1;
        if (ship->hardpoints) {
            ITERATOR it;
            SHIP_HARDPOINT_DEF *hp;
            iterator_start(&it, ship->hardpoints);
            while ((hp = (SHIP_HARDPOINT_DEF *)iterator_nextdata(&it)) != NULL) {
                if (hp->slot_id >= next_slot)
                    next_slot = hp->slot_id + 1;
            }
            iterator_stop(&it);
        }

        SHIP_HARDPOINT_DEF *hp = new_ship_hardpoint_def();
        hp->slot_id = next_slot;
        free_string(hp->name);
        hp->name = str_dup(argument);
        list_appendlink(ship->hardpoints, hp);

        send_to_char(formatf("Hardpoint slot %d '%s' added.\n\r", hp->slot_id, hp->name), ch);
        return true;
    }

    /* hardpoint remove <slot#> */
    if (!str_cmp(arg, "remove")) {
        if (!is_number(argument)) {
            send_to_char("Syntax:  hardpoint remove <slot#>\n\r", ch);
            return false;
        }

        int slot = atoi(argument);
        ITERATOR it;
        SHIP_HARDPOINT_DEF *hp;
        bool found = false;

        iterator_start(&it, ship->hardpoints);
        while ((hp = (SHIP_HARDPOINT_DEF *)iterator_nextdata(&it)) != NULL) {
            if (hp->slot_id == slot) {
                iterator_remcurrent(&it);
                free_ship_hardpoint_def(hp);
                found = true;
                break;
            }
        }
        iterator_stop(&it);

        if (!found) {
            send_to_char("No hardpoint with that slot number.\n\r", ch);
            return false;
        }

        send_to_char(formatf("Hardpoint slot %d removed.\n\r", slot), ch);
        return true;
    }

    /* hardpoint <slot#> <field> <value> */
    if (!is_number(arg)) {
        send_to_char("Expected 'add', 'remove', or a slot number.\n\r", ch);
        return false;
    }

    int slot = atoi(arg);
    SHIP_HARDPOINT_DEF *target = NULL;

    {
        ITERATOR it;
        SHIP_HARDPOINT_DEF *hp;
        iterator_start(&it, ship->hardpoints);
        while ((hp = (SHIP_HARDPOINT_DEF *)iterator_nextdata(&it)) != NULL) {
            if (hp->slot_id == slot) {
                target = hp;
                break;
            }
        }
        iterator_stop(&it);
    }

    if (!target) {
        send_to_char("No hardpoint with that slot number.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    /* hardpoint <slot#> name <new name> */
    if (!str_cmp(arg, "name")) {
        if (argument[0] == '\0') {
            send_to_char("Syntax:  hardpoint <slot#> name <new name>\n\r", ch);
            return false;
        }
        free_string(target->name);
        target->name = str_dup(argument);
        send_to_char("Hardpoint name set.\n\r", ch);
        return true;
    }

    /* hardpoint <slot#> type <type> */
    if (!str_cmp(arg, "type")) {
        int value = flag_value(hardpoint_types, argument);
        if (value == NO_FLAG) {
            send_to_char("Invalid type. Use: weapon, defense, utility, propulsion\n\r", ch);
            return false;
        }
        target->type = value;
        send_to_char("Hardpoint type set.\n\r", ch);
        return true;
    }

    /* hardpoint <slot#> size <size> */
    if (!str_cmp(arg, "size")) {
        int value = flag_value(hardpoint_sizes, argument);
        if (value == NO_FLAG) {
            send_to_char("Invalid size. Use: small, medium, large\n\r", ch);
            return false;
        }
        target->size = value;
        send_to_char("Hardpoint size set.\n\r", ch);
        return true;
    }

    /* hardpoint <slot#> domain <flags> */
    if (!str_cmp(arg, "domain")) {
        int value = flag_value(domain_flags, argument);
        if (value == NO_FLAG) {
            send_to_char("Invalid domain. Use: aquatic, aerial, terrestrial\n\r", ch);
            return false;
        }
        target->domain_flags ^= value;
        send_to_char(formatf("Hardpoint domain flags: %s\n\r",
            flag_string(domain_flags, target->domain_flags)), ch);
        return true;
    }

    /* hardpoint <slot#> flags <flags> */
    if (!str_cmp(arg, "flags")) {
        int value = flag_value(hardpoint_flags, argument);
        if (value == NO_FLAG) {
            send_to_char("Invalid flag. Use: required, locked\n\r", ch);
            return false;
        }
        target->flags ^= value;
        send_to_char(formatf("Hardpoint flags: %s\n\r",
            flag_string(hardpoint_flags, target->flags)), ch);
        return true;
    }

    send_to_char("Unknown hardpoint field. Use: name, type, size, domain, flags\n\r", ch);
    return false;
}

/**
 * shedit_npctype - Set the NPC behavior type for this ship template
 *
 * Controls what AI behavior an NPC ship will exhibit when created from
 * this template (e.g., coast guard patrols, pirates attack, traders flee).
 * Only meaningful when the 'npc' ship flag is set.
 *
 * Syntax: npctype <type>
 */
SHEDIT( shedit_npctype )
{
    SHIP_INDEX_DATA *ship;
    EDIT_SHIP(ch, ship);
    return olc_cmd_type_set(ch, argument, "NPC Type",
        "Syntax: npctype <type>\n\rType '? npcshiptype' for a list.",
        &ship->npc_type, npc_ship_types, NULL, NULL);
}

/**
 * shedit_faction - Set or clear the faction allegiance for this NPC ship
 *
 * Links the ship to a REPUTATION_INDEX_DATA faction.  When players attack
 * or sink this ship, reputation changes are applied against this faction.
 * Only meaningful when the 'npc' ship flag is set.
 *
 * Syntax: faction <reputation widevnum>
 *         faction none
 */
SHEDIT( shedit_faction )
{
    SHIP_INDEX_DATA *ship;

    EDIT_SHIP(ch, ship);

    if (argument[0] == '\0') {
        send_to_char("Syntax:  faction <reputation widevnum>\n\r", ch);
        send_to_char("         faction none\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "none") || !str_cmp(argument, "clear")) {
        ship->faction = NULL;
        memset(&ship->faction_ref, 0, sizeof(ship->faction_ref));
        ship->faction_rank = 0;
        send_to_char("Faction cleared.\n\r", ch);
        return true;
    }

    WNUM rep_wnum;
    AREA_DATA *context = olc_relative_widevnum_context(ship->area, argument);
    if (!parse_widevnum(argument, context, &rep_wnum)) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    REPUTATION_INDEX_DATA *rep = get_reputation_index(rep_wnum.pArea, rep_wnum.vnum);
    if (!rep) {
        send_to_char("That reputation faction does not exist.\n\r", ch);
        return false;
    }

    ship->faction = rep;
    ship->faction_ref.vnum = rep_wnum.vnum;
    send_to_char(formatf("Faction set to [%ld] %s.\n\r", rep->vnum, rep->name), ch);
    return true;
}

/**
 * shedit_factionrank - Set the faction rank for this NPC ship
 *
 * Determines the ship's effective standing within its faction, used for
 * display and for scaling reputation changes on combat interactions.
 * Only meaningful when a faction is set.
 *
 * Syntax: factionrank <rank_number>
 */
SHEDIT( shedit_factionrank )
{
    SHIP_INDEX_DATA *ship;
    int value;

    EDIT_SHIP(ch, ship);

    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax: factionrank <rank_number>\n\r", ch);
        return false;
    }

    if (!ship->faction) {
        send_to_char("Set a faction first with 'faction <widevnum>'.\n\r", ch);
        return false;
    }

    value = atoi(argument);
    if (value < 0) {
        send_to_char("Faction rank must be non-negative.\n\r", ch);
        return false;
    }

    ship->faction_rank = (int16_t)value;
    send_to_char(formatf("Faction rank set to %d.\n\r", ship->faction_rank), ch);
    return true;
}

/**
 * shedit_schedule - Manage transport schedule stops
 *
 * Allows adding, removing, and editing stops on a transport ship's
 * schedule. The ship must have the SHIP_TRANSPORT flag set.
 *
 * Subcommands:
 *   schedule loop <yes|no>                     - Toggle loop vs ping-pong
 *   schedule add <name>                        - Add a new stop with a name
 *   schedule remove <#>                        - Remove a stop by ID
 *   schedule <#> name <new name>               - Rename a stop
 *   schedule <#> wilds <uid> <x> <y>           - Set wilderness location
 *   schedule <#> room <widevnum>               - Set zone room location
 *   schedule <#> arrive <hour|-1>              - Set arrival hour (-1 = none)
 *   schedule <#> depart <hour|-1>              - Set departure hour (-1 = none)
 *   schedule <#> dwell <ticks>                 - Set dwell time in ticks
 *   schedule <#> exitdir <direction|none>      - Set dock exit direction
 *   schedule <#> exittype <none|vlink|room|instance> - Set dock exit type
 */
SHEDIT( shedit_schedule )
{
    SHIP_INDEX_DATA *ship;
    char arg[MIL];
    extern char *const dir_name[];

    EDIT_SHIP(ch, ship);

    if (!IS_SET(ship->flags, SHIP_TRANSPORT)) {
        send_to_char("This ship does not have the 'transport' flag set.\n\r", ch);
        return false;
    }

    if (argument[0] == '\0') {
        send_to_char("Syntax:  schedule loop <yes|no>\n\r", ch);
        send_to_char("         schedule add <name>\n\r", ch);
        send_to_char("         schedule remove <#>\n\r", ch);
        send_to_char("         schedule <#> name <new name>\n\r", ch);
        send_to_char("         schedule <#> wilds <uid> <x> <y>\n\r", ch);
        send_to_char("         schedule <#> room <widevnum>\n\r", ch);
        send_to_char("         schedule <#> arrive <hour|-1>\n\r", ch);
        send_to_char("         schedule <#> depart <hour|-1>\n\r", ch);
        send_to_char("         schedule <#> dwell <ticks>\n\r", ch);
        send_to_char("         schedule <#> exitdir <direction|none>\n\r", ch);
        send_to_char("         schedule <#> exittype <none|vlink|room|instance>\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    /* schedule loop <yes|no> */
    if (!str_cmp(arg, "loop")) {
        if (!str_cmp(argument, "yes") || !str_cmp(argument, "true")) {
            ship->schedule_loop = true;
            send_to_char("Schedule will loop back to start.\n\r", ch);
            return true;
        } else if (!str_cmp(argument, "no") || !str_cmp(argument, "false")) {
            ship->schedule_loop = false;
            send_to_char("Schedule will ping-pong (reverse at endpoints).\n\r", ch);
            return true;
        }
        send_to_char("Syntax: schedule loop <yes|no>\n\r", ch);
        return false;
    }

    /* schedule add <name> */
    if (!str_cmp(arg, "add")) {
        if (argument[0] == '\0') {
            send_to_char("Syntax: schedule add <name>\n\r", ch);
            return false;
        }

        if (list_size(ship->schedule_stops) >= SHIP_SCHEDULE_MAX_STOPS) {
            send_to_char(formatf("Maximum of %d stops reached.\n\r",
                SHIP_SCHEDULE_MAX_STOPS), ch);
            return false;
        }

        /* Find next available stop_id */
        int16_t next_id = 1;
        {
            ITERATOR it;
            SHIP_SCHEDULE_STOP *s;
            iterator_start(&it, ship->schedule_stops);
            while ((s = (SHIP_SCHEDULE_STOP *)iterator_nextdata(&it))) {
                if (s->stop_id >= next_id)
                    next_id = s->stop_id + 1;
            }
            iterator_stop(&it);
        }

        SHIP_SCHEDULE_STOP *stop = new_ship_schedule_stop();
        stop->stop_id = next_id;
        free_string(stop->name);
        stop->name = str_dup(argument);
        list_appendlink(ship->schedule_stops, stop);

        send_to_char(formatf("Stop %d '%s' added. Set location with: schedule %d wilds/room ...\n\r",
            stop->stop_id, stop->name, stop->stop_id), ch);
        return true;
    }

    /* schedule remove <#> */
    if (!str_cmp(arg, "remove")) {
        if (!is_number(argument)) {
            send_to_char("Syntax: schedule remove <stop_id>\n\r", ch);
            return false;
        }

        int target_id = atoi(argument);
        ITERATOR it;
        SHIP_SCHEDULE_STOP *s;
        int idx = 0;
        bool found = false;
        iterator_start(&it, ship->schedule_stops);
        while ((s = (SHIP_SCHEDULE_STOP *)iterator_nextdata(&it))) {
            idx++;
            if (s->stop_id == target_id) {
                list_remnthlink(ship->schedule_stops, idx, true);
                send_to_char(formatf("Stop %d removed.\n\r", target_id), ch);
                found = true;
                break;
            }
        }
        iterator_stop(&it);

        if (!found) {
            send_to_char("No stop with that ID found.\n\r", ch);
            return false;
        }
        return true;
    }

    /* schedule <#> <field> <value> */
    if (!is_number(arg)) {
        send_to_char("Expected 'loop', 'add', 'remove', or a stop ID.\n\r", ch);
        return false;
    }

    int target_id = atoi(arg);
    SHIP_SCHEDULE_STOP *target = NULL;
    {
        ITERATOR it;
        SHIP_SCHEDULE_STOP *s;
        iterator_start(&it, ship->schedule_stops);
        while ((s = (SHIP_SCHEDULE_STOP *)iterator_nextdata(&it))) {
            if (s->stop_id == target_id) {
                target = s;
                break;
            }
        }
        iterator_stop(&it);
    }

    if (!target) {
        send_to_char("No stop with that ID found.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    /* schedule <#> name <new name> */
    if (!str_cmp(arg, "name")) {
        if (argument[0] == '\0') {
            send_to_char("Syntax: schedule <#> name <new name>\n\r", ch);
            return false;
        }
        free_string(target->name);
        target->name = str_dup(argument);
        send_to_char(formatf("Stop %d renamed to '%s'.\n\r", target_id, target->name), ch);
        return true;
    }

    /* schedule <#> wilds <uid> <x> <y> */
    if (!str_cmp(arg, "wilds")) {
        char uid_arg[MIL], x_arg[MIL], y_arg[MIL];
        argument = one_argument(argument, uid_arg);
        argument = one_argument(argument, x_arg);
        one_argument(argument, y_arg);

        if (uid_arg[0] == '\0' || x_arg[0] == '\0' || y_arg[0] == '\0'
            || !is_number(uid_arg) || !is_number(x_arg) || !is_number(y_arg)) {
            send_to_char("Syntax: schedule <#> wilds <uid> <x> <y>\n\r", ch);
            return false;
        }

        long uid = atol(uid_arg);
        int x = atoi(x_arg);
        int y = atoi(y_arg);

        /* Validate the wilderness exists */
        WILDS_DATA *wilds = get_wilds_from_uid(NULL, uid);
        if (!wilds) {
            send_to_char("That wilderness UID does not exist.\n\r", ch);
            return false;
        }

        target->location_type = STOP_LOC_WILDERNESS;
        target->wilds_uid = uid;
        target->loc_x = x;
        target->loc_y = y;
        target->dock_room = NULL;
        send_to_char(formatf("Stop %d set to wilderness %ld at (%d, %d).\n\r",
            target_id, uid, x, y), ch);
        return true;
    }

    /* schedule <#> room <widevnum> */
    if (!str_cmp(arg, "room")) {
        if (argument[0] == '\0') {
            send_to_char("Syntax: schedule <#> room <widevnum>\n\r", ch);
            return false;
        }

        WNUM room_wnum;
        AREA_DATA *context = olc_relative_widevnum_context(ship->area, argument);
        if (!parse_widevnum(argument, context, &room_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        ROOM_INDEX_DATA *room = get_room_index(room_wnum.pArea, room_wnum.vnum);
        if (!room) {
            send_to_char("That room does not exist.\n\r", ch);
            return false;
        }

        target->location_type = STOP_LOC_ROOM;
        target->room_ref.load.auid = room_wnum.pArea->uid;
        target->room_ref.load.vnum = room_wnum.vnum;
        target->dock_room = room;
        send_to_char(formatf("Stop %d set to room [%ld] %s.\n\r",
            target_id, room->vnum, room->name), ch);
        return true;
    }

    /* schedule <#> arrive <hour|-1> */
    if (!str_cmp(arg, "arrive")) {
        if (!is_number(argument) && str_cmp(argument, "-1")) {
            send_to_char("Syntax: schedule <#> arrive <0-23|-1>\n\r", ch);
            return false;
        }
        int hour = atoi(argument);
        if (hour < -1 || hour > 23) {
            send_to_char("Hour must be -1 (none) or 0-23.\n\r", ch);
            return false;
        }
        target->arrive_hour = hour;
        if (hour == -1)
            send_to_char(formatf("Stop %d: arrival hour cleared.\n\r", target_id), ch);
        else
            send_to_char(formatf("Stop %d: arrival hour set to %d.\n\r", target_id, hour), ch);
        return true;
    }

    /* schedule <#> depart <hour|-1> */
    if (!str_cmp(arg, "depart")) {
        if (!is_number(argument) && str_cmp(argument, "-1")) {
            send_to_char("Syntax: schedule <#> depart <0-23|-1>\n\r", ch);
            return false;
        }
        int hour = atoi(argument);
        if (hour < -1 || hour > 23) {
            send_to_char("Hour must be -1 (none) or 0-23.\n\r", ch);
            return false;
        }
        target->depart_hour = hour;
        if (hour == -1)
            send_to_char(formatf("Stop %d: departure hour cleared.\n\r", target_id), ch);
        else
            send_to_char(formatf("Stop %d: departure hour set to %d.\n\r", target_id, hour), ch);
        return true;
    }

    /* schedule <#> dwell <ticks> */
    if (!str_cmp(arg, "dwell")) {
        if (!is_number(argument)) {
            send_to_char("Syntax: schedule <#> dwell <ticks>\n\r", ch);
            return false;
        }
        int ticks = atoi(argument);
        if (ticks < 0 || ticks > 200) {
            send_to_char("Dwell ticks must be 0-200.\n\r", ch);
            return false;
        }
        target->dwell_ticks = ticks;
        send_to_char(formatf("Stop %d: dwell time set to %d ticks.\n\r", target_id, ticks), ch);
        return true;
    }

    /* schedule <#> exitdir <direction|none> */
    if (!str_cmp(arg, "exitdir")) {
        if (!str_cmp(argument, "none") || !str_cmp(argument, "-1")) {
            target->dock_exit_dir = -1;
            send_to_char(formatf("Stop %d: dock exit direction cleared.\n\r", target_id), ch);
            return true;
        }
        int dir = parse_direction(argument);
        if (dir < 0) {
            send_to_char("Invalid direction. Use: north south east west up down or none.\n\r", ch);
            return false;
        }
        target->dock_exit_dir = dir;
        send_to_char(formatf("Stop %d: dock exit direction set to %s.\n\r",
            target_id, dir_name[dir]), ch);
        return true;
    }

    /* schedule <#> exittype <none|vlink|room|instance> */
    if (!str_cmp(arg, "exittype")) {
        int value = flag_value(dock_exit_types, argument);
        if (value == NO_FLAG) {
            send_to_char("Valid types: none, vlink, room, instance.\n\r", ch);
            return false;
        }
        target->dock_exit_type = value;
        send_to_char(formatf("Stop %d: dock exit type set to '%s'.\n\r",
            target_id, flag_string(dock_exit_types, value)), ch);
        return true;
    }

    send_to_char("Unknown schedule field. Use: name, wilds, room, arrive, depart, dwell, exitdir, exittype\n\r", ch);
    return false;
}