/***************************************************************************
 *  bsedit.c - OLC Blueprint Section Editor                                *
 *                                                                         *
 *  Migrated to the unified OLC Editor Framework (Phase 4).                *
 ***************************************************************************/

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
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

extern void list_blueprint_sections(CHAR_DATA *ch, char *argument);
extern bool validate_vnum_range(CHAR_DATA *ch, BLUEPRINT_SECTION *section, AREA_DATA *rooms_area, long lower, long upper);
extern bool can_edit_blueprints(CHAR_DATA *ch);
extern long top_blueprint_section_vnum;

/***************************************************************************
 * Forward Declarations — Tab Show Functions                               *
 ***************************************************************************/

static void bsedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void bsedit_show_links_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void bsedit_show_maze_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void bsedit_show_notes_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);

/***************************************************************************
 * Framework Helpers                                                       *
 ***************************************************************************/

static AREA_DATA *bsedit_get_area(void *pEdit)
{
    return pEdit ? ((BLUEPRINT_SECTION *)pEdit)->area : NULL;
}

static bool bsedit_perm_check(CHAR_DATA *ch, void *pEdit)
{
    return can_edit_blueprints(ch);
}

static void bsedit_mark_changed(CHAR_DATA *ch, void *pEdit)
{
    BLUEPRINT_SECTION *bs = (BLUEPRINT_SECTION *)pEdit;
    if (bs && bs->area)
        SET_BIT(bs->area->area_flags, AREA_CHANGED);
}

/***************************************************************************
 * Blueprint Section Editor Command Table (moved from blueprint.c)         *
 ***************************************************************************/

const struct olc_cmd_type bsedit_table[] = {
    { "?",              show_help           },
    { "commands",       show_commands       },
    { "list",           bsedit_list         },
    { "show",           bsedit_show         },
    { "create",         bsedit_create       },
    { "name",           bsedit_name         },
    { "description",    bsedit_description  },
    { "comments",       bsedit_comments     },
    { "type",           bsedit_type         },
    { "flags",          bsedit_flags        },
    { "recall",         bsedit_recall       },
    { "rooms",          bsedit_rooms        },
    { "link",           bsedit_link         },
    { "maze",           bsedit_maze         },
    { NULL,             NULL                }
};

/***************************************************************************
 * Editor Definition (Unified Framework)                                   *
 ***************************************************************************/

static const OLC_EDITOR_DEF bsedit_def = {
    .name           = "BSEdit",
    .editor_type    = ED_BPSECT,
    .cmd_table      = bsedit_table,
    .show_fn        = bsedit_show,
    .tabs           = {
        .count      = 4,
        .tabs       = {
            { "General",  "Gen",  bsedit_show_general_tab },
            { "Links",    "Lnk",  bsedit_show_links_tab   },
            { "Maze",     "Mze",  bsedit_show_maze_tab    },
            { "Notes",    "Nts",  bsedit_show_notes_tab   },
        },
    },
    .theme          = &olc_theme_building,
    .perm           = {
        .flags      = OLC_PERM_CUSTOM,
        .check_fn   = bsedit_perm_check,
    },
    .change_mode    = OLC_CHANGE_CUSTOM,
    .mark_changed_fn = bsedit_mark_changed,
    .get_area_fn    = bsedit_get_area,
    .audit_changes  = true,
};

/***************************************************************************
 * Blueprint Section Editor Interpreter — delegates to framework.          *
 ***************************************************************************/

void bsedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &bsedit_def);
}

/***************************************************************************
 * Blueprint Section Editor Entry Point (moved from blueprint.c)           *
 ***************************************************************************/

/**
 * do_bsedit - Staff command to edit or create blueprint sections
 *
 * Syntax: bsedit <vnum> - Edit existing blueprint section
 *         bsedit create <vnum> - Create new blueprint section
 *
 * @param ch        Staff character
 * @param argument  Section vnum or "create <vnum>"
 */
void do_bsedit(CHAR_DATA *ch, char *argument)
{
    BLUEPRINT_SECTION *bs;
    char arg1[MAX_STRING_LENGTH];
    WNUM wnum;

    argument = one_argument(argument, arg1);

    if (IS_NPC(ch))
        return;

    if (!can_edit_blueprints(ch))
    {
        send_to_char("BSEdit:  Insufficient security to edit blueprints.\n\r", ch);
        return;
    }

    if (parse_widevnum(arg1, ch->in_room->area, &wnum))
    {
        if (!(bs = get_blueprint_section_for_area(wnum.pArea, wnum.vnum)))
        {
            send_to_char("BSEdit:  That blueprint section does not exist.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &bsedit_def, (void *)bs, true);
        return;
    }
    else
    {
        if (!str_cmp(arg1, "create"))
        {
            if (bsedit_create(ch, argument))
            {
                olc_editor_enter(ch, &bsedit_def, ch->desc->pEdit, true);
            }

            return;
        }
    }

    send_to_char("Syntax: bsedit <#vnum|area_uid#vnum>\n\r"
                 "        bsedit create <vnum>\n\r", ch);
}

/***************************************************************************
 * Commands                                                                *
 ***************************************************************************/

BSEDIT( bsedit_list )
{
    list_blueprint_sections(ch, argument);
    return false;
}

/***************************************************************************
 * Master Show — dispatches to active tab                                  *
 ***************************************************************************/

BSEDIT( bsedit_show )
{
    BLUEPRINT_SECTION *bs;
    EDIT_BPSECT(ch, bs);

    const OLC_EDITOR_THEME *theme = bsedit_def.theme;
    OLC_LAYOUT_CTX *ctx = olc_display_new(ch, theme);

    char id_buf[64];
    sprintf(id_buf, "%ld", bs->vnum);
    olc_display_header(ctx, "BSEdit", bs->name, id_buf, &bsedit_def);

    /* Dispatch to active tab's show function */
    int tab = ch->desc ? ch->desc->nEditTab : 0;
    if (tab >= 0 && tab < bsedit_def.tabs.count
        && bsedit_def.tabs.tabs[tab].show_fn) {
        bsedit_def.tabs.tabs[tab].show_fn(ch, ctx, (void *)bs);
    } else {
        bsedit_show_general_tab(ch, ctx, (void *)bs);
    }

    olc_display_footer(ctx, theme);
    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

/***************************************************************************
 * Tab 1: General                                                          *
 ***************************************************************************/

static void bsedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    BLUEPRINT_SECTION *bs = (BLUEPRINT_SECTION *)pEdit;
    const OLC_EDITOR_THEME *theme = bsedit_def.theme;
    char buf[MSL];

    sprintf(buf, "[%5ld] %s", bs->vnum, bs->name);
    olc_display_string(ctx, theme, "Name:", "name", buf);

    olc_display_string(ctx, theme, "Type:", "type", flag_string(blueprint_section_types, bs->type));
    olc_display_string(ctx, theme, "Flags:", "flags", flag_string(blueprint_section_flags, bs->flags));

    if (bs->recall_room)
    {
        sprintf(buf, "[%5ld] %s", bs->recall_room->vnum, bs->recall_room->name);
        olc_display_string(ctx, theme, "Recall:", "recall", buf);
    }
    else if (bs->recall_ref.load.vnum > 0)
    {
        sprintf(buf, "[%5ld] (unresolved)", bs->recall_ref.load.vnum);
        olc_display_string(ctx, theme, "Recall:", "recall", buf);
    }
    else
    {
        olc_display_string(ctx, theme, "Recall:", "recall", "no recall defined");
    }

    sprintf(buf, "%s - %s",
        bs->rooms_area ? widevnum_string(bs->rooms_area, bs->lower_vnum, bs->area) : "(not set)",
        bs->rooms_area ? widevnum_string(bs->rooms_area, bs->upper_vnum, bs->area) : "(not set)");
    olc_display_string(ctx, theme, "Room Range:", "rooms", buf);

    olc_display_section(ctx, theme, "Description");
    add_buf(ctx->buffer, bs->description ? bs->description : "(none)\n\r");
}

/***************************************************************************
 * Tab 2: Links                                                            *
 ***************************************************************************/

static void bsedit_show_links_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    BLUEPRINT_SECTION *bs = (BLUEPRINT_SECTION *)pEdit;
    const OLC_EDITOR_THEME *theme = bsedit_def.theme;
    char buf[MSL];

    if (bs->links)
    {
        int bli = 0;
        olc_display_section(ctx, theme, "Section Links");
        for (BLUEPRINT_LINK *bl = bs->links; bl; bl = bl->next)
        {
            ++bli;
            ROOM_INDEX_DATA *room = bl->room;
            char *door = (bl->door >= 0 && bl->door < MAX_DIR) ? dir_name[bl->door] : "none";
            char excolor = bl->ex ? 'W' : 'D';

            sprintf(buf, " {Y[{W%3d{Y] {G%-30.30s {%c%-9s{x in {Y[{W%5ld{Y]{x %s\n\r",
                bli, bl->name, excolor, door,
                bl->room ? bl->room->vnum : bl->room_ref.load.vnum,
                room ? room->name : "nowhere");
            add_buf(ctx->buffer, buf);
        }
    }
    else
    {
        olc_display_string(ctx, theme, "Links:", NULL, "None");
    }
}

/***************************************************************************
 * Tab 3: Maze (only meaningful when type == BSTYPE_MAZE)                  *
 ***************************************************************************/

static void bsedit_show_maze_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    BLUEPRINT_SECTION *bs = (BLUEPRINT_SECTION *)pEdit;
    const OLC_EDITOR_THEME *theme = bsedit_def.theme;
    char buf[MSL];

    if (bs->type != BSTYPE_MAZE)
    {
        olc_display_infof(ctx, theme, "Section type is {W%s{x, not MAZE. Set type first.",
            flag_string(blueprint_section_types, bs->type));
        return;
    }

    sprintf(buf, "%ld x %ld", bs->maze_x, bs->maze_y);
    olc_display_string(ctx, theme, "Maze Size:", "maze", buf);

    /* Maze Templates */
    if (bs->maze_templates && list_size(bs->maze_templates) > 0)
    {
        ITERATOR it;
        MAZE_WEIGHTED_ROOM *mwr;
        int idx = 0;
        olc_display_section(ctx, theme, "Maze Templates");
        iterator_start(&it, bs->maze_templates);
        while ((mwr = (MAZE_WEIGHTED_ROOM *)iterator_nextdata(&it)))
        {
            ++idx;
            sprintf(buf, "  {Y[{W%3d{Y]{x Weight: {W%3d{x  Exits: {W%s{x  Room: {W%ld{x %s\n\r",
                idx, mwr->weight,
                mwr->exit_count == 0 ? "Any" :
                mwr->exit_count == 1 ? " 1 " :
                mwr->exit_count == 2 ? " 2 " :
                mwr->exit_count == 3 ? " 3 " : " 4 ",
                mwr->room ? mwr->room->vnum : mwr->room_ref.load.vnum,
                mwr->room ? mwr->room->name : "(unresolved)");
            add_buf(ctx->buffer, buf);
            if (mwr->exit_template.flags & EX_ISDOOR)
            {
                sprintf(buf, "         {cExit Template:{x flags={W%s{x keyword={W%s{x",
                    flag_string(exit_flags, mwr->exit_template.flags),
                    mwr->exit_template.keyword ? mwr->exit_template.keyword : "door");
                add_buf(ctx->buffer, buf);
                if (mwr->exit_template.strength > 0) {
                    sprintf(buf, " str={W%d{x", mwr->exit_template.strength);
                    add_buf(ctx->buffer, buf);
                }
                if (mwr->exit_template.material) {
                    sprintf(buf, " mat={W%s{x", mwr->exit_template.material);
                    add_buf(ctx->buffer, buf);
                }
                if (mwr->exit_template.lock.key_wnum.vnum > 0) {
                    sprintf(buf, " key={W%ld{x", mwr->exit_template.lock.key_wnum.vnum);
                    add_buf(ctx->buffer, buf);
                }
                if (mwr->exit_template.lock.pick_chance > 0) {
                    sprintf(buf, " pick={W%d%%{x", mwr->exit_template.lock.pick_chance);
                    add_buf(ctx->buffer, buf);
                }
                if (mwr->exit_template.lock.flags) {
                    sprintf(buf, " lock={W%s{x", flag_string(lock_flags, mwr->exit_template.lock.flags));
                    add_buf(ctx->buffer, buf);
                }
                add_buf(ctx->buffer, "\n\r");
            }
        }
        iterator_stop(&it);
        sprintf(buf, "  Total Weight: {W%d{x\n\r", bs->total_maze_weight);
        add_buf(ctx->buffer, buf);
    }
    else
    {
        olc_display_string(ctx, theme, "Maze Templates:", NULL, "(none)");
    }

    /* Maze Fixed Rooms */
    if (bs->maze_fixed_rooms && list_size(bs->maze_fixed_rooms) > 0)
    {
        ITERATOR it;
        MAZE_FIXED_ROOM *mfr;
        int idx = 0;
        olc_display_section(ctx, theme, "Maze Fixed Rooms");
        iterator_start(&it, bs->maze_fixed_rooms);
        while ((mfr = (MAZE_FIXED_ROOM *)iterator_nextdata(&it)))
        {
            ++idx;
            sprintf(buf, "  {Y[{W%3d{Y]{x Pos: ({W%d{x,{W%d{x)  Room: {W%ld{x %s  Connected: %s\n\r",
                idx, mfr->x, mfr->y,
                mfr->room ? mfr->room->vnum : mfr->room_ref.load.vnum,
                mfr->room ? mfr->room->name : "(unresolved)",
                mfr->connected ? "{GYes{x" : "{RNo{x");
            add_buf(ctx->buffer, buf);
        }
        iterator_stop(&it);
    }
    else
    {
        olc_display_string(ctx, theme, "Maze Fixed Rooms:", NULL, "(none)");
    }

    /* Maze Map Data */
    if (bs->map_data)
    {
        MAZE_MAP_DATA *mmd = bs->map_data;
        olc_display_section(ctx, theme, "Maze Map");

        if (mmd->obj) {
            sprintf(buf, "{W[%ld]{x %s", mmd->obj->vnum, mmd->obj->short_descr);
        } else if (mmd->obj_ref.load.vnum > 0) {
            sprintf(buf, "{W[%ld]{x (unresolved)", mmd->obj_ref.load.vnum);
        } else {
            sprintf(buf, "{Dnone{x");
        }
        olc_display_string(ctx, theme, "  Object:", NULL, buf);

        if (mmd->mob) {
            sprintf(buf, "{W[%ld]{x %s", mmd->mob->vnum, mmd->mob->short_descr);
        } else if (mmd->mob_ref.load.vnum > 0) {
            sprintf(buf, "{W[%ld]{x (unresolved)", mmd->mob_ref.load.vnum);
        } else {
            sprintf(buf, "{Dnone (map placed in first room){x");
        }
        olc_display_string(ctx, theme, "  Carrier:", NULL, buf);

        olc_display_string(ctx, theme, "  Solve:", NULL, mmd->solve ? "{GYes{x" : "{RNo{x");
    }
    else
    {
        olc_display_string(ctx, theme, "Maze Map:", NULL, "(not configured)");
    }
}

/***************************************************************************
 * Tab 4: Notes (Builder Comments)                                         *
 ***************************************************************************/

static void bsedit_show_notes_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    BLUEPRINT_SECTION *bs = (BLUEPRINT_SECTION *)pEdit;
    const OLC_EDITOR_THEME *theme = bsedit_def.theme;

    olc_display_section(ctx, theme, "Builders' Comments");
    add_buf(ctx->buffer, bs->comments ? bs->comments : "(none)\n\r");
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

    return olc_cmd_string(ch, argument, "Name", NULL, &bs->name,
        OLC_STR_DEFAULT, NULL, NULL);
}

BSEDIT( bsedit_description )
{
    BLUEPRINT_SECTION *bs;
    EDIT_BPSECT(ch, bs);

    return olc_cmd_string_append(ch, argument, "Description", NULL,
        &bs->description, NULL, NULL);
}

BSEDIT( bsedit_comments )
{
    BLUEPRINT_SECTION *bs;
    EDIT_BPSECT(ch, bs);

    return olc_cmd_string_append(ch, argument, "Comments", NULL,
        &bs->comments, NULL, NULL);
}

BSEDIT( bsedit_type )
{
    BLUEPRINT_SECTION *bs;
    EDIT_BPSECT(ch, bs);

    return olc_cmd_type_set(ch, argument, "Type", NULL, &bs->type,
        blueprint_section_types, NULL, NULL);
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
    AREA_DATA *rooms_context = bs->rooms_area ? bs->rooms_area : bs->area;
    AREA_DATA *context = olc_relative_widevnum_context(rooms_context, argument);
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

    AREA_DATA *recall_area = bs->rooms_area ? bs->rooms_area : (bs->area ? bs->area : get_system_area_fallback());
    bs->recall_ref.load.vnum = vnum; bs->recall_ref.load.auid = recall_area->uid; bs->recall_room = room;
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
        send_to_char("{YVnums must be in the same area. Use vnum, #vnum, or area#vnum format.{x\n\r", ch);
        return false;
    }

    WNUM wnum_lower, wnum_upper;
    AREA_DATA *context = bs->rooms_area ? bs->rooms_area : ch->in_room->area;

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

    if( validate_vnum_range(ch, bs, wnum_lower.pArea, lvnum, uvnum) )
    {
        bs->lower_vnum = lvnum;
        bs->upper_vnum = uvnum;
        bs->rooms_area = wnum_lower.pArea;
        bs->lower_vnum_ref.load.auid = wnum_lower.pArea->uid;
        bs->lower_vnum_ref.load.vnum = lvnum;
        bs->upper_vnum_ref.load.auid = wnum_upper.pArea->uid;
        bs->upper_vnum_ref.load.vnum = uvnum;

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
        bool is_maze = (bs->type == BSTYPE_MAZE);

        if( !is_maze && (bs->lower_vnum < 1 || bs->upper_vnum < 1) )
        {
            send_to_char("Vnum range must be set first.\n\r", ch);
            return false;
        }

        WNUM room_wnum;
        AREA_DATA *rooms_context = bs->rooms_area ? bs->rooms_area : bs->area;
        AREA_DATA *context = olc_relative_widevnum_context(rooms_context, arg2);
        if (!parse_widevnum(arg2, context, &room_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        long vnum = room_wnum.vnum;
        if( !is_maze && (vnum < bs->lower_vnum || vnum > bs->upper_vnum) )
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

        int door = parse_door(argument);
        if( door < 0 )
        {
            send_to_char("That is an invalid exit.\n\r", ch);
            return false;
        }

        EXIT_DATA *ex = NULL;

        // Maze sections: exits are created at instantiation, skip exit validation
        if( !is_maze )
        {
            bool found = false;
            for( int i = 0; i < MAX_DIR; i++ )
            {
                if( room->exit[i] )
                    found = true;
            }

            if( !found )
            {
                send_to_char("That room has no exits.\n\r", ch);
                return false;
            }

            ex = room->exit[door];
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
        }

        link = new_blueprint_link();
        link->room_ref.load.vnum = vnum;
        link->room_ref.load.auid = room_wnum.pArea ? room_wnum.pArea->uid : 0;
        link->room = room;
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
        AREA_DATA *rooms_context = bs->rooms_area ? bs->rooms_area : bs->area;
        AREA_DATA *context = olc_relative_widevnum_context(rooms_context, argument);
        if (!parse_widevnum(argument, context, &room_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        long vnum = room_wnum.vnum;
        if( bs->type != BSTYPE_MAZE && (vnum < bs->lower_vnum || vnum > bs->upper_vnum) )
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

        // Skip exit validation for maze sections (exits created at instantiation)
        if( bs->type != BSTYPE_MAZE )
        {
            bool found = false;
            for( int i = 0; i < MAX_DIR; i++ )
            {
                if( room->exit[i] )
                    found = true;
            }

            if( !found )
            {
                send_to_char("That room has no exits.\n\r", ch);
                return false;
            }
        }

        link->room_ref.load.vnum = vnum;
        link->room_ref.load.auid = room_wnum.pArea ? room_wnum.pArea->uid : 0;
        link->room = room;
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

        // Skip exit validation for maze sections (exits created at instantiation)
        if( bs->type != BSTYPE_MAZE )
        {
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
        send_to_char("         maze templates exit <#> flags <exit_flags>\n\r", ch);
        send_to_char("         maze templates exit <#> keyword <word>\n\r", ch);
        send_to_char("         maze templates exit <#> strength <value>\n\r", ch);
        send_to_char("         maze templates exit <#> material <name>\n\r", ch);
        send_to_char("         maze templates exit <#> key <obj_vnum>\n\r", ch);
        send_to_char("         maze templates exit <#> pick <chance>\n\r", ch);
        send_to_char("         maze templates exit <#> lockflags <flags>\n\r", ch);
        send_to_char("         maze templates exit <#> clear\n\r", ch);
        send_to_char("         maze fixed list\n\r", ch);
        send_to_char("         maze fixed add <x> <y> <room_vnum> [connected]\n\r", ch);
        send_to_char("         maze fixed remove <#>\n\r", ch);
        send_to_char("         maze map obj <obj_vnum>    - template map object\n\r", ch);
        send_to_char("         maze map mob <mob_vnum>    - mob that carries the map\n\r", ch);
        send_to_char("         maze map solve             - toggle solution path display\n\r", ch);
        send_to_char("         maze map clear             - remove map configuration\n\r", ch);
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
                if (mwr->exit_template.flags & EX_ISDOOR) {
                    sprintf(buf, "         {cExit Template:{x flags={W%s{x keyword={W%s{x",
                        flag_string(exit_flags, mwr->exit_template.flags),
                        mwr->exit_template.keyword ? mwr->exit_template.keyword : "door");
                    send_to_char(buf, ch);
                    if (mwr->exit_template.lock.key_wnum.vnum > 0) {
                        sprintf(buf, " key={W%ld{x", mwr->exit_template.lock.key_wnum.vnum);
                        send_to_char(buf, ch);
                    }
                    if (mwr->exit_template.lock.pick_chance > 0) {
                        sprintf(buf, " pick={W%d%%{x", mwr->exit_template.lock.pick_chance);
                        send_to_char(buf, ch);
                    }
                    if (mwr->exit_template.lock.flags) {
                        sprintf(buf, " lock={W%s{x", flag_string(lock_flags, mwr->exit_template.lock.flags));
                        send_to_char(buf, ch);
                    }
                    send_to_char("\n\r", ch);
                }
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
            AREA_DATA *context = olc_relative_widevnum_context(bs->area, vnum_arg);
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

        if (!str_cmp(arg2, "exit")) {
            char idx_arg[MIL];
            argument = one_argument(argument, idx_arg);

            char prop_arg[MIL];
            argument = one_argument(argument, prop_arg);

            if (idx_arg[0] == '\0' || prop_arg[0] == '\0') {
                send_to_char("Syntax:  maze templates exit <#> <property> [value]\n\r", ch);
                send_to_char("Properties: flags keyword strength material key pick lockflags clear\n\r", ch);
                return false;
            }

            int idx = atoi(idx_arg);
            MAZE_WEIGHTED_ROOM *mwr = (MAZE_WEIGHTED_ROOM *)list_nthdata(bs->maze_templates, idx);
            if (!mwr) {
                send_to_char("Invalid template number.\n\r", ch);
                return false;
            }

            if (!str_cmp(prop_arg, "flags")) {
                if (argument[0] == '\0') {
                    send_to_char("Syntax:  maze templates exit <#> flags <exit_flags>\n\r", ch);
                    return false;
                }
                int value = flag_value(exit_flags, argument);
                if (value == NO_FLAG) {
                    send_to_char("Invalid exit flag(s).\n\r", ch);
                    return false;
                }
                mwr->exit_template.flags = value;
                /* Ensure EX_ISDOOR is always set when any flags present */
                if (mwr->exit_template.flags)
                    SET_BIT(mwr->exit_template.flags, EX_ISDOOR);
                send_to_char("Exit template flags set.\n\r", ch);
                return true;
            }

            if (!str_cmp(prop_arg, "keyword")) {
                if (argument[0] == '\0') {
                    send_to_char("Syntax:  maze templates exit <#> keyword <word>\n\r", ch);
                    return false;
                }
                if (mwr->exit_template.keyword)
                    free_string(mwr->exit_template.keyword);
                mwr->exit_template.keyword = str_dup(argument);
                send_to_char("Exit template keyword set.\n\r", ch);
                return true;
            }

            if (!str_cmp(prop_arg, "strength")) {
                if (argument[0] == '\0') {
                    send_to_char("Syntax:  maze templates exit <#> strength <value>\n\r", ch);
                    return false;
                }
                mwr->exit_template.strength = (int16_t)atoi(argument);
                send_to_char("Exit template strength set.\n\r", ch);
                return true;
            }

            if (!str_cmp(prop_arg, "material")) {
                if (argument[0] == '\0') {
                    send_to_char("Syntax:  maze templates exit <#> material <name>\n\r", ch);
                    return false;
                }
                if (mwr->exit_template.material)
                    free_string(mwr->exit_template.material);
                mwr->exit_template.material = str_dup(argument);
                send_to_char("Exit template material set.\n\r", ch);
                return true;
            }

            if (!str_cmp(prop_arg, "key")) {
                if (argument[0] == '\0') {
                    send_to_char("Syntax:  maze templates exit <#> key <obj_vnum>\n\r", ch);
                    return false;
                }
                WNUM key_wnum;
                AREA_DATA *context = olc_relative_widevnum_context(bs->area, argument);
                if (!parse_widevnum(argument, context, &key_wnum)) {
                    send_to_char("Invalid widevnum format.\n\r", ch);
                    return false;
                }
                OBJ_INDEX_DATA *key_obj = get_obj_index(key_wnum.pArea, key_wnum.vnum);
                if (!key_obj) {
                    send_to_char("That object does not exist.\n\r", ch);
                    return false;
                }
                mwr->exit_template.lock.key_load.auid = key_wnum.pArea ? key_wnum.pArea->uid : 0;
                mwr->exit_template.lock.key_load.vnum = key_wnum.vnum;
                mwr->exit_template.lock.key_wnum = key_wnum;
                char buf[MSL];
                sprintf(buf, "Exit template key set to %ld (%s).\n\r", key_obj->vnum, key_obj->short_descr);
                send_to_char(buf, ch);
                return true;
            }

            if (!str_cmp(prop_arg, "pick")) {
                if (argument[0] == '\0') {
                    send_to_char("Syntax:  maze templates exit <#> pick <chance 0-100>\n\r", ch);
                    return false;
                }
                int pick = atoi(argument);
                if (pick < 0 || pick > 100) {
                    send_to_char("Pick chance must be 0-100.\n\r", ch);
                    return false;
                }
                mwr->exit_template.lock.pick_chance = pick;
                send_to_char("Exit template pick chance set.\n\r", ch);
                return true;
            }

            if (!str_cmp(prop_arg, "lockflags")) {
                if (argument[0] == '\0') {
                    send_to_char("Syntax:  maze templates exit <#> lockflags <flags>\n\r", ch);
                    return false;
                }
                int value = flag_value(lock_flags, argument);
                if (value == NO_FLAG) {
                    send_to_char("Invalid lock flag(s).\n\r", ch);
                    return false;
                }
                mwr->exit_template.lock.flags = value;
                send_to_char("Exit template lock flags set.\n\r", ch);
                return true;
            }

            if (!str_cmp(prop_arg, "clear")) {
                mwr->exit_template.flags = 0;
                if (mwr->exit_template.keyword) {
                    free_string(mwr->exit_template.keyword);
                    mwr->exit_template.keyword = NULL;
                }
                mwr->exit_template.strength = 0;
                if (mwr->exit_template.material) {
                    free_string(mwr->exit_template.material);
                    mwr->exit_template.material = NULL;
                }
                mwr->exit_template.lock.key_load.auid = 0;
                mwr->exit_template.lock.key_load.vnum = 0;
                mwr->exit_template.lock.key_wnum.pArea = NULL;
                mwr->exit_template.lock.key_wnum.vnum = 0;
                mwr->exit_template.lock.pick_chance = 0;
                mwr->exit_template.lock.flags = 0;
                send_to_char("Exit template cleared.\n\r", ch);
                return true;
            }

            send_to_char("Properties: flags keyword strength material key pick lockflags clear\n\r", ch);
            return false;
        }

        send_to_char("Syntax:  maze templates list|add|remove|exit\n\r", ch);
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
            AREA_DATA *context = olc_relative_widevnum_context(bs->area, vnum_arg);
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

    if (!str_cmp(arg, "map")) {
        if (!str_cmp(arg2, "obj")) {
            if (argument[0] == '\0') {
                send_to_char("Syntax:  maze map obj <obj_vnum>\n\r", ch);
                return false;
            }

            char vnum_arg[MIL];
            one_argument(argument, vnum_arg);

            WNUM obj_wnum;
            AREA_DATA *context = olc_relative_widevnum_context(bs->area, vnum_arg);
            if (!parse_widevnum(vnum_arg, context, &obj_wnum)) {
                send_to_char("Invalid widevnum format.\n\r", ch);
                return false;
            }

            OBJ_INDEX_DATA *pObj = get_obj_index(obj_wnum.pArea, obj_wnum.vnum);
            if (!pObj) {
                send_to_char("That object does not exist.\n\r", ch);
                return false;
            }

            if (pObj->item_type != ITEM_MAP) {
                send_to_char("{YWarning:{x Object is not ITEM_MAP type. Map display may not work correctly.\n\r", ch);
            }

            if (!bs->map_data)
                bs->map_data = new_maze_map_data();

            bs->map_data->obj_ref.load.vnum = obj_wnum.vnum;
            bs->map_data->obj_ref.load.auid = obj_wnum.pArea ? obj_wnum.pArea->uid : 0;
            bs->map_data->obj = pObj;

            char buf[MSL];
            sprintf(buf, "Map object set to [%ld] %s.\n\r", pObj->vnum, pObj->short_descr);
            send_to_char(buf, ch);
            return true;
        }

        if (!str_cmp(arg2, "mob")) {
            if (argument[0] == '\0') {
                send_to_char("Syntax:  maze map mob <mob_vnum>\n\r", ch);
                return false;
            }

            char vnum_arg[MIL];
            one_argument(argument, vnum_arg);

            WNUM mob_wnum;
            AREA_DATA *context = olc_relative_widevnum_context(bs->area, vnum_arg);
            if (!parse_widevnum(vnum_arg, context, &mob_wnum)) {
                send_to_char("Invalid widevnum format.\n\r", ch);
                return false;
            }

            MOB_INDEX_DATA *pMob = get_mob_index(mob_wnum.pArea, mob_wnum.vnum);
            if (!pMob) {
                send_to_char("That mobile does not exist.\n\r", ch);
                return false;
            }

            if (!bs->map_data)
                bs->map_data = new_maze_map_data();

            bs->map_data->mob_ref.load.vnum = mob_wnum.vnum;
            bs->map_data->mob_ref.load.auid = mob_wnum.pArea ? mob_wnum.pArea->uid : 0;
            bs->map_data->mob = pMob;

            char buf[MSL];
            sprintf(buf, "Map carrier mob set to [%ld] %s.\n\r", pMob->vnum, pMob->short_descr);
            send_to_char(buf, ch);
            return true;
        }

        if (!str_cmp(arg2, "solve")) {
            if (!bs->map_data)
                bs->map_data = new_maze_map_data();

            bs->map_data->solve = !bs->map_data->solve;

            char buf[MSL];
            sprintf(buf, "Map solution path: %s.\n\r", bs->map_data->solve ? "{GEnabled{x" : "{RDisabled{x");
            send_to_char(buf, ch);
            return true;
        }

        if (!str_cmp(arg2, "clear")) {
            if (bs->map_data) {
                free_maze_map_data(bs->map_data);
                bs->map_data = NULL;
            }
            send_to_char("Map configuration cleared.\n\r", ch);
            return true;
        }

        send_to_char("Syntax:  maze map obj|mob|solve|clear\n\r", ch);
        return false;
    }

    bsedit_maze(ch, "");
    return false;
}