/***************************************************************************
 *  wedit.c - OLC Wilderness Editor                                        *
 *                                                                         *
 *  Migrated to the unified OLC Editor Framework (Phase 3).                *
 ***************************************************************************/

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
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

extern void correct_vrooms(WILDS_DATA *pWilds, WILDS_TERRAIN *pTerrain);

/***************************************************************************
 * Forward Declarations — Tab Show Functions                               *
 ***************************************************************************/

static void wedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void wedit_show_map_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void wedit_show_terrain_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void wedit_show_vlinks_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);

/***************************************************************************
 * Framework Helpers                                                       *
 ***************************************************************************/

static AREA_DATA *wedit_get_area(void *pEdit)
{
    return pEdit ? ((WILDS_DATA *)pEdit)->pArea : NULL;
}

/***************************************************************************
 * Wilderness Editor Command Table (moved from olc.c)                      *
 ***************************************************************************/

const struct olc_cmd_type wedit_table[] = {
    {   "?",            show_help       },
    {   "commands",     show_commands   },
    {   "create",       wedit_create    },
    {   "delete",       wedit_delete    },
    {   "name",         wedit_name      },
    {   "placetype",    wedit_placetype },
    {   "region",       wedit_region    },
    {   "show",         wedit_show      },
    {   "terrain",      wedit_terrain   },
    {   "vlink",        wedit_vlink     },
    {   NULL,           0               }
};

/***************************************************************************
 * Editor Definition (Unified Framework)                                   *
 ***************************************************************************/

static const OLC_EDITOR_DEF wedit_def = {
    .name           = "WEdit",
    .editor_type    = ED_WILDS,
    .cmd_table      = wedit_table,
    .show_fn        = wedit_show,
    .tabs           = {
        .count      = 4,
        .tabs       = {
            { "General",  "Gen",  wedit_show_general_tab },
            { "Map",      "Map",  wedit_show_map_tab     },
            { "Terrain",  "Ter",  wedit_show_terrain_tab },
            { "VLinks",   "VLnk", wedit_show_vlinks_tab  },
        },
    },
    .theme          = &olc_theme_world,
    .perm           = {
        .flags          = OLC_PERM_AREA_SECURITY,
    },
    .change_mode    = OLC_CHANGE_AREA_FLAG,
    .get_area_fn    = wedit_get_area,
    .audit_changes  = true,
};

/***************************************************************************
 * Wilderness Editor Interpreter — delegates to framework.                 *
 ***************************************************************************/

void wedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &wedit_def);
}

/***************************************************************************
 * Wilderness Editor Entry Point (moved from olc.c)                        *
 ***************************************************************************/

void do_wedit(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *pArea = NULL;
    WILDS_DATA *pWilds = NULL,
               *pLastWilds = NULL;
    WILDS_TERRAIN *pTerrain = NULL;
    char arg1[MIL],
         arg2[MIL],
         arg3[MIL],
         *pMap = NULL,
         *pStaticMap = NULL;
    long value = 0,
         lScount = 0,
         lMapsize = 0;
    int size_x = 0,
        size_y = 0;

    if (IS_NPC(ch))
        return;

    argument = one_argument(argument, arg1);

    pWilds = ch->in_wilds;

    if (IS_NULLSTR(arg1) && pWilds == NULL)
    {
        send_to_char("Wedit Usage:\n\r", ch);
        send_to_char("               wedit                        - defaults to editing the wilds you are in.\n\r", ch);
        send_to_char("               wedit [wilds uid]            - edit wilds via uid\n\r", ch);
        send_to_char("               wedit create <sizex> <sizey> - create new wilds of specified dimensions\n\r", ch);
        return;
    }
    else if (is_number(arg1))
    {
        value = atol(arg1);

        if ((pWilds = get_wilds_from_uid(NULL, value)) == NULL)
        {
            send_to_char("Wedit: That wilds index does not exist.\n\r", ch);
            return;
        }

        if (!has_access_area(ch, pWilds->pArea))
        {
            send_to_char("Wedit: Insufficient security to edit wilds - action logged.\n\r", ch);
            return;
        }
    }
    else
    {
        if (!str_cmp(arg1, "create"))
        {
            if (IS_NULLSTR(argument))
            {
                send_to_char("Wedit Usage:\n\r", ch);
                send_to_char("               wedit create <sizex> <sizey> - create new wilds of specified dimensions\n\r", ch);
                return;
            }
            else
            {
                argument = one_argument(argument, arg2);
                one_argument(argument, arg3);

                if (is_number(arg2) && is_number(arg3))
                {
                    size_x = atoi(arg2);
                    size_y = atoi(arg3);
                    pArea = ch->in_room->area;
                }

                if (!has_access_area(ch, pArea))
                {
                    send_to_char("Insufficient security to edit area - action logged.\n\r", ch);
                    return;
                }
            }

            pWilds = new_wilds();
            pWilds->pArea = ch->in_room->area;
            pWilds->uid = gconfig.next_wilds_uid++;
            gconfig_write();
            pWilds->name = str_dup("New Wilds");
            pWilds->map_size_x = size_x;
            pWilds->map_size_y = size_y;
            lMapsize = pWilds->map_size_x * pWilds->map_size_y;
            pWilds->staticmap = calloc(sizeof(char), lMapsize);
            pWilds->map = calloc(sizeof(char), lMapsize);

            pMap = pWilds->map;
            pStaticMap = pWilds->staticmap;

            for (lScount = 0; lScount < lMapsize; lScount++)
            {
                *pMap++ = 'S';
                *pStaticMap++ = 'S';
            }

            if (pArea->wilds)
            {
                pLastWilds = pArea->wilds;
                while (pLastWilds->next)
                    pLastWilds = pLastWilds->next;

                perrf(LOG_INFO, "olc.c, do_wedit(): Adding Wilds to existing linked-list.");
                pLastWilds->next = pWilds;
            }
            else
            {
                perrf(LOG_INFO, "olc.c, do_wedit(): Adding first Wilds to linked-list.");
                pArea->wilds = pWilds;
            }

            send_to_char("Wedit: New wilds region created.\n\r", ch);
            pTerrain = new_terrain(pWilds);
            pTerrain->mapchar = 'S';
            pTerrain->showchar = str_dup("{B~");
            pWilds->pTerrain = pTerrain;
            send_to_char("Wedit: Default wilds terrain mapping completed.\n\r", ch);
        }
    }

    printf_to_char(ch, "{x[{WWedit{x] Editing Wilds.\n\r");
    olc_editor_enter(ch, &wedit_def, (void *)pWilds, false);
}

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

        plogf(LOG_INFO, "Adding Wilds to existing linked-list.");
        pLastWilds->next = pWilds;
    }
    else
    {
        plogf(LOG_INFO, "Adding first Wilds to linked-list.");
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

/***************************************************************************
 * Tab Show Functions                                                      *
 ***************************************************************************/

/**
 * wedit_show_general_tab - General wilderness properties
 *
 * Shows name, area, map dimensions, default terrain, and current state.
 */
static void wedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    WILDS_DATA *pWilds = (WILDS_DATA *)pEdit;
    WILDS_TERRAIN *pTerrain;
    const OLC_EDITOR_THEME *theme = &olc_theme_world;

    olc_display_section(ctx, theme, "Properties");
    olc_display_string(ctx, theme, "Name:", "name", pWilds->name);
    olc_display_infof(ctx, theme, "Area UID:", "%ld - %s", pWilds->pArea->uid, pWilds->pArea->name);
    olc_display_infof(ctx, theme, "Map size:", "%d x %d (%ld vrooms)",
        pWilds->map_size_x, pWilds->map_size_y,
        (long)(pWilds->map_size_x * pWilds->map_size_y));
    olc_display_string(ctx, theme, "Default region:", "region default",
        flag_string(wilderness_regions, pWilds->defaultRegion));
    olc_display_string(ctx, theme, "Default place:", "placetype",
        flag_string(place_flags, pWilds->defaultPlaceFlags));

    /* Show default terrain if one is set */
    for (pTerrain = pWilds->pTerrain; pTerrain; pTerrain = pTerrain->next)
    {
        if (pTerrain->mapchar == pWilds->cDefaultTerrain)
        {
            char buf[MSL];
            if (pTerrain->mapchar == '{')
                sprintf(buf, "'{W{%c{x' '%s{x' %s",
                    pTerrain->mapchar, pTerrain->showchar,
                    pTerrain->showname ? pTerrain->showname : "(Not Set)");
            else
                sprintf(buf, "'{W%c{x' '%s{x' %s",
                    pTerrain->mapchar, pTerrain->showchar,
                    pTerrain->showname ? pTerrain->showname : "(Not Set)");
            olc_display_string(ctx, theme, "Default terrain:", NULL, buf);
            break;
        }
    }

    olc_display_section(ctx, theme, "Current State");
    olc_display_number(ctx, theme, "Players:", NULL, pWilds->nplayer);
    olc_display_number(ctx, theme, "Age:", NULL, pWilds->age);
}

/**
 * wedit_show_map_tab - Visual map display
 *
 * Renders a header to the buffer, then show_map_to_char is called
 * directly after buffer flush (sends output directly to character).
 */
static void wedit_show_map_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    WILDS_DATA *pWilds = (WILDS_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = &olc_theme_world;

    olc_display_infof(ctx, theme, "Map size:", "%d x %d",
        pWilds->map_size_x, pWilds->map_size_y);
    add_buf(ctx->buffer, "\n\r  {DMap display follows below.{x\n\r");
}

/**
 * wedit_show_terrain_tab - Terrain mappings
 *
 * Shows all terrain tokens with their display characters, names,
 * sectors, and flags.
 */
static void wedit_show_terrain_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    WILDS_DATA *pWilds = (WILDS_DATA *)pEdit;
    WILDS_TERRAIN *pTerrain;
    const OLC_EDITOR_THEME *theme = &olc_theme_world;
    char buf[MSL];
    int col = 0;

    olc_display_section(ctx, theme, "Terrain Key");

    /* Show default terrain */
    for (pTerrain = pWilds->pTerrain; pTerrain; pTerrain = pTerrain->next)
    {
        if (pTerrain->mapchar == pWilds->cDefaultTerrain)
        {
            if (pTerrain->mapchar == '{')
                sprintf(buf, "'{W{%c{x' '%s{x' {W%-12s{x",
                    pTerrain->mapchar, pTerrain->showchar,
                    pTerrain->showname ? pTerrain->showname : "(Not Set)");
            else
                sprintf(buf, "'{W%c{x' '%s{x' {W%-12s{x",
                    pTerrain->mapchar, pTerrain->showchar,
                    pTerrain->showname ? pTerrain->showname : "(Not Set)");
            olc_display_string(ctx, theme, "Default:", NULL, buf);
            break;
        }
    }

    add_buf(ctx->buffer, "\n\r");
    add_buf(ctx->buffer, "  Tile Ansi Name        Tile Ansi Name        Tile Ansi Name\n\r");
    for (pTerrain = pWilds->pTerrain; pTerrain; pTerrain = pTerrain->next)
    {
        if (pTerrain->mapchar == '{')
            sprintf(buf, " '{W{%c{x'  '%s{x' {W%-12s{x{x",
                pTerrain->mapchar, pTerrain->showchar,
                pTerrain->showname ? pTerrain->showname : "(Not Set)");
        else
            sprintf(buf, " '{W%c{x'  '%s{x' {W%-12s{x{x",
                pTerrain->mapchar, pTerrain->showchar,
                pTerrain->showname ? pTerrain->showname : "(Not Set)");

        add_buf(ctx->buffer, buf);

        if (col++ % 3 == 2)
            add_buf(ctx->buffer, "\n\r");
    }

    if (col % 3 != 0)
        add_buf(ctx->buffer, "\n\r");
}

/**
 * wedit_show_vlinks_tab - Virtual links listing
 *
 * Shows all vlinks defined for this wilderness region in a table format.
 * Currently read-only; write support planned for the wilds refactor.
 */
static void wedit_show_vlinks_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    WILDS_DATA *pWilds = (WILDS_DATA *)pEdit;
    WILDS_VLINK *pVLink;
    const OLC_EDITOR_THEME *theme = &olc_theme_world;
    char buf[MSL];
    int vlnum = 0;
    int count = 0;

    /* Count vlinks */
    for (pVLink = pWilds->pVLink; pVLink; pVLink = pVLink->next)
        count++;

    olc_display_infof(ctx, theme, "VLinks:", "%d defined", count);

    if (count == 0)
    {
        add_buf(ctx->buffer, "  {DNo virtual links defined.{x\n\r");
        return;
    }

    add_buf(ctx->buffer, "\n\r");
    add_buf(ctx->buffer, "  {D[num] [uid]   [x coor] [y coor] [direction] "
                         "[destvnum] [default] [current] [maptile]{x\n\r");

    for (pVLink = pWilds->pVLink; pVLink; pVLink = pVLink->next)
    {
        sprintf(buf, "  %-5d ({W%6ld{x)  {W%6d   %6d   %-9s   %-8ld   %10s%10s%s{x\n\r",
            vlnum++,
            pVLink->uid,
            pVLink->wildsorigin_x,
            pVLink->wildsorigin_y,
            dir_name[pVLink->door],
            pVLink->destvnum,
            vlinkage_bit_name(pVLink->default_linkage),
            vlinkage_bit_name(pVLink->current_linkage),
            pVLink->map_tile);
        add_buf(ctx->buffer, buf);
    }
}

/***************************************************************************
 * Master Show Function — dispatches to active tab.                        *
 ***************************************************************************/

WEDIT ( wedit_show )
{
    WILDS_DATA *pWilds;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&wedit_def);
    OLC_LAYOUT_CTX *ctx;
    int tab;

    EDIT_WILDS(ch, pWilds);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "WEdit", pWilds->name,
        formatf("UID %ld", pWilds->uid), &wedit_def);

    /* Dispatch to active tab's show function */
    tab = olc_show_all_tabs_mode(ch) ? -1 : (ch->desc ? ch->desc->nEditTab : 0);
    if (tab < 0) {
        for (int i = 0; i < wedit_def.tabs.count; i++) {
            if (wedit_def.tabs.tabs[i].show_fn)
                wedit_def.tabs.tabs[i].show_fn(ch, ctx, (void *)pWilds);
        }
    } else if (tab >= 0 && tab < wedit_def.tabs.count
        && wedit_def.tabs.tabs[tab].show_fn) {
        wedit_def.tabs.tabs[tab].show_fn(ch, ctx, (void *)pWilds);
    } else {
        wedit_show_general_tab(ch, ctx, (void *)pWilds);
    }

    olc_display_footer(ctx, theme);
    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);

    /* Map tab: show_map_to_char sends directly to character */
    if (tab == 1 || tab < 0)
        show_map_to_char(ch, ch, 3, 3, true);

    return false;
}

WEDIT (wedit_name)
{
    WILDS_DATA *pWilds;

    EDIT_WILDS (ch, pWilds);

    return olc_cmd_string(ch, argument, "Name", "name <string>",
                          &pWilds->name, OLC_STR_DEFAULT, NULL, NULL);
}

WEDIT (wedit_region)
{
    WILDS_DATA *pWilds;
    WILDS_REGION *pRegion;
    char buf[MSL];
    char arg[MIL];

    EDIT_WILDS(ch, pWilds);

    argument = one_argument(argument, arg);
    if (arg[0] != '\0')
    {
        if (!str_prefix(arg, "list"))
        {
            BUFFER *buffer = new_buf();
            int i = 0;

            sprintf(buf, "Default Region:  %s\n\r\n\r", flag_string(wilderness_regions, pWilds->defaultRegion));
            add_buf(buffer, buf);
            add_buf(buffer, "     [Start X] [Start Y] [ End X ] [ End Y ] [      Region      ] [     Place     ]\n\r");
            add_buf(buffer, "====================================================================================\n\r");

            for (pRegion = pWilds->pRegion; pRegion; pRegion = pRegion->next)
            {
                sprintf(buf, "%4d  %7d   %7d   %7d   %7d   %-18s   %-15s\n\r", ++i,
                    pRegion->startx, pRegion->starty,
                    pRegion->endx, pRegion->endy,
                    flag_string(wilderness_regions, pRegion->region),
                    flag_string(place_flags, pRegion->area_place_flags));
                add_buf(buffer, buf);
            }

            page_to_char(buf_string(buffer), ch);
            free_buf(buffer);
            return false;
        }

        if (!str_prefix(arg, "default"))
        {
            if (argument[0] != '\0')
            {
                if (!str_cmp(argument, "none"))
                {
                    pWilds->defaultRegion = REGION_UNKNOWN;
                    send_to_char("Default region cleared.\n\r", ch);
                    return true;
                }

                int value = flag_value(wilderness_regions, argument);
                if (value != NO_FLAG)
                {
                    pWilds->defaultRegion = value;
                    send_to_char("Default region set.\n\r", ch);
                    return true;
                }
            }

            send_to_char("Syntax: region default <region|none>\n\r", ch);
            send_to_char("Type '? wilderness_regions' to list valid regions.\n\r", ch);
            return false;
        }

        if (!str_prefix(arg, "add"))
        {
            char arg2[MIL];
            char arg3[MIL];
            char arg4[MIL];
            char arg5[MIL];
            char arg6[MIL];
            int startx, starty, endx, endy;
            int region;
            int place;

            argument = one_argument(argument, arg2);
            argument = one_argument(argument, arg3);
            argument = one_argument(argument, arg4);
            argument = one_argument(argument, arg5);
            argument = one_argument(argument, arg6);

            if (!is_number(arg2) || (startx = atoi(arg2)) < 0 || startx >= pWilds->map_size_x)
            {
                sprintf(buf, "Start X must be between 0 and %d.\n\r", pWilds->map_size_x - 1);
                send_to_char(buf, ch);
                return false;
            }

            if (!is_number(arg3) || (starty = atoi(arg3)) < 0 || starty >= pWilds->map_size_y)
            {
                sprintf(buf, "Start Y must be between 0 and %d.\n\r", pWilds->map_size_y - 1);
                send_to_char(buf, ch);
                return false;
            }

            if (!is_number(arg4) || (endx = atoi(arg4)) < 0 || endx >= pWilds->map_size_x)
            {
                sprintf(buf, "End X must be between 0 and %d.\n\r", pWilds->map_size_x - 1);
                send_to_char(buf, ch);
                return false;
            }

            if (!is_number(arg5) || (endy = atoi(arg5)) < 0 || endy >= pWilds->map_size_y)
            {
                sprintf(buf, "End Y must be between 0 and %d.\n\r", pWilds->map_size_y - 1);
                send_to_char(buf, ch);
                return false;
            }

            region = flag_value(wilderness_regions, arg6);
            if (region == NO_FLAG)
            {
                send_to_char("Invalid region. Type '? wilderness_regions'.\n\r", ch);
                return false;
            }

            place = flag_value(place_flags, argument);
            if (place == NO_FLAG)
            {
                send_to_char("Invalid place type. Type '? placetype'.\n\r", ch);
                return false;
            }

            pRegion = new_region(pWilds);
            pRegion->startx = UMIN(startx, endx);
            pRegion->starty = UMIN(starty, endy);
            pRegion->endx = UMAX(startx, endx);
            pRegion->endy = UMAX(starty, endy);
            pRegion->region = region;
            pRegion->area_place_flags = place;
            add_region(pWilds, pRegion);

            send_to_char("Region added.\n\r", ch);
            return true;
        }

        if (!str_prefix(arg, "remove"))
        {
            int count = 0;
            int index;

            for (pRegion = pWilds->pRegion; pRegion; pRegion = pRegion->next)
                count++;

            if (argument[0] == '\0' || !is_number(argument))
            {
                send_to_char("Syntax: region remove <#>\n\r", ch);
                if (count > 0)
                {
                    sprintf(buf, "Please specify a number from 1 to %d.\n\r", count);
                    send_to_char(buf, ch);
                }
                return false;
            }

            index = atoi(argument);
            if (index < 1 || index > count)
            {
                sprintf(buf, "Please specify a number from 1 to %d.\n\r", count);
                send_to_char(buf, ch);
                return false;
            }

            for (pRegion = pWilds->pRegion; pRegion; pRegion = pRegion->next)
                if (!--index)
                    break;

            del_region(pWilds, pRegion);
            send_to_char("Region removed.\n\r", ch);
            return true;
        }
    }

    send_to_char("Syntax: region list\n\r", ch);
    send_to_char("        region default <region|none>\n\r", ch);
    send_to_char("        region add <startx> <starty> <endx> <endy> <region> <placetype>\n\r", ch);
    send_to_char("        region remove <#>\n\r", ch);
    return false;
}

WEDIT (wedit_placetype)
{
    WILDS_DATA *pWilds;
    int value;

    EDIT_WILDS(ch, pWilds);

    if (argument[0] != '\0')
    {
        if (!str_cmp(argument, "none"))
        {
            pWilds->defaultPlaceFlags = PLACE_NOWHERE;
            send_to_char("Wilds default place type cleared.\n\r", ch);
            return true;
        }

        if ((value = flag_value(place_flags, argument)) != NO_FLAG)
        {
            pWilds->defaultPlaceFlags = value;
            send_to_char("Wilds default place type set.\n\r", ch);
            return true;
        }
    }

    send_to_char("Syntax: placetype <placetype|none>\n\r", ch);
    send_to_char("Type '? placetype' for valid values.\n\r", ch);
    return false;
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
                     sector_name(room_sector_type(pTerrain->template)),
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
    char row[MSL];

    if ((value = sector_lookup(argument)) == NO_FLAG)
    {
        send_to_char("Syntax: terrain <token> sector <sector>\n\r", ch);
        send_to_char("Available sectors:\n\r", ch);
        for (int i = 0; i < sector_count(); i++)
        {
            snprintf(row, sizeof(row), "  %-3d %s\n\r", i, sector_name(i));
            send_to_char(row, ch);
        }
        return false;
    }

    room_set_sector_type(pTerrain->template, value);
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
            perrf(LOG_ERROR, "Failed to link vlink ch->in_wilds is NULL");
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
    } else {
        send_to_char("Wedit vlink unlink: Could not find vlink. Try 'vlink show' for a list.", ch);
        return false;
    }
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
    } else {
        send_to_char("Wedit vlink unlink: Could not find vlink. Try 'vlink show' for a list.", ch);
        return false;
    }
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
            perrf(LOG_ERROR, "Failed to link vlink ch->in_wilds is NULL");
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
    } else {
        send_to_char("Wedit vlink: Could not find vlink. Try 'vlink show' for a list.", ch);
        return false;
    }
    }

    if (!str_cmp(arg, "direction"))
    {
        int vlnum = 0;
        int value;

        if (!pWilds)
        {
            perrf(LOG_ERROR, "Failed to link vlink ch->in_wilds is NULL");
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
    } else {
        send_to_char("Wedit vlink: Could not find vlink. Try 'vlink show' for a list.", ch);
        return false;
    }
    }

    if (!str_cmp(arg, "destination"))
    {
        int vlnum = 0;
        int value;

        if (!pWilds)
        {
            perrf(LOG_ERROR, "Failed to link vlink ch->in_wilds is NULL");
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
            WNUM room_wnum;
            AREA_DATA *context = ch->in_room->area;
            if (parse_widevnum(arg3, context, &room_wnum) && room_wnum.vnum > 0) {
                value = room_wnum.vnum;
                ROOM_INDEX_DATA *destRoom = get_room_index(room_wnum.pArea, room_wnum.vnum);

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
    } else {
        send_to_char("Wedit vlink: Could not find vlink. Try 'vlink show' for a list.", ch);
        return false;
    }
    }


    if (!str_cmp(arg, "location"))
    {
        int vlnum = 0;
        int x, y;

        if (!pWilds)
        {
            perrf(LOG_ERROR, "Failed to link vlink ch->in_wilds is NULL");
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
    } else {
        send_to_char("Wedit vlink: Could not find vlink. Try 'vlink show' for a list.", ch);
        return false;
    }
    }

    if (!str_cmp(arg, "maptile"))
    {
        int vlnum = 0;

        if (!pWilds)
        {
            perrf(LOG_ERROR, "Failed to link vlink ch->in_wilds is NULL");
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
    } else {
        send_to_char("Wedit vlink: Could not find vlink. Try 'vlink show' for a list.", ch);
        return false;
    }
    }

    if (!str_cmp(arg, "list"))
    {
        int vlnum;
        if (!ch->in_room)
        {
            perrf(LOG_ERROR, "ch->in_room invalid.");
            return false;
        }
        else
            if (!ch->in_room->area)
            {
                perrf(LOG_ERROR, "ch->in_room->area invalid.");
                return false;
            }
            else
                if (!ch->in_room->area->wilds)
                {
                    perrf(LOG_ERROR, "ch->in_room->area->wilds invalid.");
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