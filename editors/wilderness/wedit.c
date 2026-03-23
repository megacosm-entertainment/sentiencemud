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
#include "../../wilderness_mods.h"
#include "../../wilderness_storage.h"
#include "../../requirements.h"
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
static void wedit_show_regions_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void wedit_show_terrain_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void wedit_show_vlinks_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);

/***************************************************************************
 * Framework Helpers                                                       *
 ***************************************************************************/

static AREA_DATA *wedit_get_area(void *pEdit)
{
    return pEdit ? ((WILDS_DATA *)pEdit)->pArea : NULL;
}

static bool wedit_parse_rgb_hex(const char *value, unsigned char *r, unsigned char *g, unsigned char *b)
{
    unsigned int tr, tg, tb;

    if (IS_NULLSTR(value) || value[0] != '#' || strlen(value) != 7)
        return false;

    if (sscanf(value + 1, "%02x%02x%02x", &tr, &tg, &tb) != 3)
        return false;

    *r = (unsigned char)tr;
    *g = (unsigned char)tg;
    *b = (unsigned char)tb;
    return true;
}

static int wedit_terrain_showname_col_width(WILDS_DATA *pWilds, int min_width, int max_width)
{
    WILDS_TERRAIN *pTerrain;
    int width = min_width;

    if (!pWilds)
        return min_width;

    for (pTerrain = pWilds->pTerrain; pTerrain; pTerrain = pTerrain->next)
    {
        const char *name = pTerrain->showname ? pTerrain->showname : "(Not Set)";
        int len = (int)strlen(name);
        if (len > width)
            width = len;
    }

    return URANGE(min_width, width, max_width);
}

static bool wedit_region_set_requirements_json(char **target, const char *input, char *err_buf, size_t err_buf_size)
{
    char *json_str;

    if (!target)
        return false;

    if (IS_NULLSTR(input) || !str_cmp(input, "none"))
    {
        free_string(*target);
        *target = str_dup("");
        return true;
    }

    json_str = requirements_text_to_json(input, err_buf, err_buf_size);
    if (!json_str)
        return false;

    free_string(*target);
    *target = str_dup(json_str);
    free(json_str);
    return true;
}

static bool wedit_region_set_spawn_wnum(WILDS_DATA *pWilds, bool is_mob, char **target, const char *input)
{
    WNUM_LOAD load = {0, 0};
    WNUM resolved = {NULL, 0};
    char canonical[MIL];

    if (!pWilds || !target)
        return false;

    if (IS_NULLSTR(input) || !str_cmp(input, "none"))
    {
        free_string(*target);
        *target = str_dup("");
        return true;
    }

    if (!parse_widevnum_load(input, &load))
        return false;

    resolve_wnum_load(&load, &resolved, pWilds->pArea);
    if (!resolved.pArea || resolved.vnum < 1)
        return false;

    if (is_mob && !get_mob_index(resolved.pArea, resolved.vnum))
        return false;
    if (!is_mob && !get_obj_index(resolved.pArea, resolved.vnum))
        return false;

    snprintf(canonical, sizeof(canonical), "%ld#%ld", resolved.pArea->uid, resolved.vnum);
    free_string(*target);
    *target = str_dup(canonical);
    return true;
}

static WILDS_REGION_SPAWN *wedit_region_spawn_new(void)
{
    WILDS_REGION_SPAWN *spawn = alloc_mem(sizeof(*spawn));

    if (!spawn)
        return NULL;

    memset(spawn, 0, sizeof(*spawn));
    spawn->wnum = str_dup("");
    spawn->requirements = str_dup("");
    return spawn;
}

static WILDS_REGION_SPAWN *wedit_region_spawn_clone(const WILDS_REGION_SPAWN *src)
{
    WILDS_REGION_SPAWN *spawn;

    if (!src)
        return NULL;

    spawn = wedit_region_spawn_new();
    if (!spawn)
        return NULL;

    free_string(spawn->wnum);
    spawn->wnum = str_dup(IS_NULLSTR(src->wnum) ? "" : src->wnum);
    spawn->chance = src->chance;
    spawn->cap = src->cap;
    free_string(spawn->requirements);
    spawn->requirements = str_dup(IS_NULLSTR(src->requirements) ? "" : src->requirements);
    return spawn;
}

static WILDS_REGION_SPAWN *wedit_region_spawn_get(LLIST *list, int index)
{
    if (!list || index < 1 || index > list_size(list))
        return NULL;

    return (WILDS_REGION_SPAWN *)list_nthdata(list, index);
}

static const char *wedit_spawn_mob_display_name(MOB_INDEX_DATA *mob)
{
    if (!mob)
        return "(none)";

    if (!IS_NULLSTR(mob->list_name))
        return mob->list_name;

    if (!IS_NULLSTR(mob->short_descr))
        return mob->short_descr;

    if (!IS_NULLSTR(mob->player_name))
        return mob->player_name;

    return "(unnamed)";
}

static const char *wedit_spawn_obj_display_name(OBJ_INDEX_DATA *obj)
{
    if (!obj)
        return "(none)";

    if (!IS_NULLSTR(obj->list_name))
        return obj->list_name;

    if (!IS_NULLSTR(obj->short_descr))
        return obj->short_descr;

    if (!IS_NULLSTR(obj->name))
        return obj->name;

    return "(unnamed)";
}

static void wedit_region_spawn_format_wnum(WILDS_DATA *pWilds, bool is_mob,
    const WILDS_REGION_SPAWN *spawn, char *out, size_t out_size)
{
    WNUM_LOAD load = {0, 0};
    WNUM resolved = {NULL, 0};

    if (!out || out_size < 1)
        return;

    out[0] = '\0';

    if (!pWilds || !spawn || IS_NULLSTR(spawn->wnum))
    {
        snprintf(out, out_size, "(none)");
        return;
    }

    if (!parse_widevnum_load(spawn->wnum, &load))
    {
        snprintf(out, out_size, "%s", spawn->wnum);
        return;
    }

    resolve_wnum_load(&load, &resolved, pWilds->pArea);
    if (!resolved.pArea || resolved.vnum < 1)
    {
        snprintf(out, out_size, "%s", spawn->wnum);
        return;
    }

    if (is_mob)
    {
        MOB_INDEX_DATA *mob = get_mob_index(resolved.pArea, resolved.vnum);
        if (!mob)
        {
            snprintf(out, out_size, "%s", spawn->wnum);
            return;
        }

        snprintf(out, out_size, "%s (%s)", spawn->wnum, wedit_spawn_mob_display_name(mob));
        return;
    }

    {
        OBJ_INDEX_DATA *obj = get_obj_index(resolved.pArea, resolved.vnum);
        if (!obj)
        {
            snprintf(out, out_size, "%s", spawn->wnum);
            return;
        }

        snprintf(out, out_size, "%s (%s)", spawn->wnum, wedit_spawn_obj_display_name(obj));
    }
}

static void wedit_region_spawn_list_copy(LLIST *dst, LLIST *src)
{
    ITERATOR it;
    WILDS_REGION_SPAWN *spawn;

    if (!dst || !src)
        return;

    list_clear(dst);

    iterator_start(&it, src);
    while ((spawn = (WILDS_REGION_SPAWN *)iterator_nextdata(&it)) != NULL)
    {
        WILDS_REGION_SPAWN *clone = wedit_region_spawn_clone(spawn);
        if (clone)
            list_appendlink(dst, clone);
    }
    iterator_stop(&it);
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
    {   "overlay",      wedit_overlay   },
    {   "placetype",    wedit_placetype },
    {   "region",       wedit_region    },
    {   "show",         wedit_show      },
    {   "terrain",      wedit_terrain   },
    {   "wildgen",      wedit_wildgen   },
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
        .count      = 5,
        .tabs       = {
            { "General",  "Gen",  wedit_show_general_tab },
            { "Map",      "Map",  wedit_show_map_tab     },
            { "Regions",  "Reg",  wedit_show_regions_tab },
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

            if (!pWilds->staticmap || !pWilds->map) {
                send_to_char("Wedit: memory allocation failed.\n\r", ch);
                free(pWilds->staticmap);
                free(pWilds->map);
                free_wilds(pWilds);
                return;
            }

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

    if (!pWilds->staticmap || !pWilds->map) {
        send_to_char("Wedit: memory allocation failed.\n\r", ch);
        free(pWilds->staticmap);
        free(pWilds->map);
        free_wilds(pWilds);
        return false;
    }

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
    WILDS_CHUNK *chunk;
    const OLC_EDITOR_THEME *theme = &olc_theme_world;
    int runtime_chunk_count = 0;
    int runtime_overlay_records = 0;

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

    wilds_cleanup_expired_temporary_zones(pWilds);
    for (chunk = pWilds->runtime_chunks; chunk; chunk = chunk->next)
    {
        WILDS_OVERLAY *overlay;
        runtime_chunk_count++;
        for (overlay = chunk->overlays; overlay; overlay = overlay->next)
            runtime_overlay_records++;
    }

    olc_display_section(ctx, theme, "Runtime Overlay");
    olc_display_number(ctx, theme, "Chunks:", NULL, runtime_chunk_count);
    olc_display_number(ctx, theme, "Overlay records:", NULL, runtime_overlay_records);

    olc_display_section(ctx, theme, "Wildgen Definition");
    olc_display_string(ctx, theme, "Terrain base:", NULL,
        IS_NULLSTR(pWilds->wildgen_terrain_base) ? "(unset)" : pWilds->wildgen_terrain_base);
    olc_display_string(ctx, theme, "Elevation base:", NULL,
        IS_NULLSTR(pWilds->wildgen_elevation_base) ? "(unset)" : pWilds->wildgen_elevation_base);
    olc_display_infof(ctx, theme, "Grid:", "%d x %d",
        UMAX(1, pWilds->wildgen_grid_rows), UMAX(1, pWilds->wildgen_grid_cols));
    olc_display_infof(ctx, theme, "Grid tile size:", "%d x %d (0x0=auto)",
        UMAX(0, pWilds->wildgen_tile_width), UMAX(0, pWilds->wildgen_tile_height));
    olc_display_number(ctx, theme, "Default elevation:", NULL, pWilds->default_elevation);
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
 * wedit_show_regions_tab - Wilderness region definitions
 *
 * Shows default region plus all explicit region bounds with region/place flags.
 */
static void wedit_show_regions_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    WILDS_DATA *pWilds = (WILDS_DATA *)pEdit;
    WILDS_REGION *pRegion;
    const OLC_EDITOR_THEME *theme = &olc_theme_world;
    char buf[MSL];
    int count = 0;

    for (pRegion = pWilds->pRegion; pRegion; pRegion = pRegion->next)
        count++;

    olc_display_section(ctx, theme, "Defaults");
    olc_display_string(ctx, theme, "Default region:", "region default",
        flag_string(wilderness_regions, pWilds->defaultRegion));
    olc_display_string(ctx, theme, "Default place:", "placetype",
        flag_string(place_flags, pWilds->defaultPlaceFlags));

    olc_display_section(ctx, theme, "Regions");
    olc_display_infof(ctx, theme, "Count:", "%d", count);

    if (count == 0)
    {
        add_buf(ctx->buffer, "  {DNo explicit regions defined. Use: region add <startx> <starty> <endx> <endy> <region> <placetype>{x\n\r");
        return;
    }

    add_buf(ctx->buffer, "\n\r");
    add_buf(ctx->buffer, "  {D[#]  [start x] [start y] [end x] [end y] [region]            [placetype]{x\n\r");

    count = 0;
    for (pRegion = pWilds->pRegion; pRegion; pRegion = pRegion->next)
    {
        sprintf(buf, "  %-3d  %-9d %-9d %-7d %-7d %-19s %-19s\n\r",
            ++count,
            pRegion->startx,
            pRegion->starty,
            pRegion->endx,
            pRegion->endy,
            flag_string(wilderness_regions, pRegion->region),
            flag_string(place_flags, pRegion->area_place_flags));
        add_buf(ctx->buffer, buf);
    }
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
    int name_width = wedit_terrain_showname_col_width(pWilds, 12, 36);
    int col = 0;

    olc_display_section(ctx, theme, "Terrain Key");

    /* Show default terrain */
    for (pTerrain = pWilds->pTerrain; pTerrain; pTerrain = pTerrain->next)
    {
        if (pTerrain->mapchar == pWilds->cDefaultTerrain)
        {
            if (pTerrain->mapchar == '{')
                sprintf(buf, "'{W{%c{x' '%s{x' {W%-*.*s{x",
                    pTerrain->mapchar, pTerrain->showchar,
                    name_width, name_width,
                    pTerrain->showname ? pTerrain->showname : "(Not Set)");
            else
                sprintf(buf, "'{W%c{x' '%s{x' {W%-*.*s{x",
                    pTerrain->mapchar, pTerrain->showchar,
                    name_width, name_width,
                    pTerrain->showname ? pTerrain->showname : "(Not Set)");
            olc_display_string(ctx, theme, "Default:", NULL, buf);
            break;
        }
    }

    add_buf(ctx->buffer, "\n\r");
    sprintf(buf,
        "  %-4s %-6s %-*s  %-4s %-6s %-*s  %-4s %-6s %-*s\n\r",
        "Tile", "Ansi", name_width, "Name",
        "Tile", "Ansi", name_width, "Name",
        "Tile", "Ansi", name_width, "Name");
    add_buf(ctx->buffer, buf);
    for (pTerrain = pWilds->pTerrain; pTerrain; pTerrain = pTerrain->next)
    {
        if (pTerrain->mapchar == '{')
            sprintf(buf, " '{W{%c{x'  '%s{x' {W%-*.*s{x{x",
                pTerrain->mapchar, pTerrain->showchar,
                name_width, name_width,
                pTerrain->showname ? pTerrain->showname : "(Not Set)");
        else
            sprintf(buf, " '{W%c{x'  '%s{x' {W%-*.*s{x{x",
                pTerrain->mapchar, pTerrain->showchar,
                name_width, name_width,
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
    WNUM room_wnum;
    const OLC_EDITOR_THEME *theme = &olc_theme_world;
    char buf[MSL];
    int vlnum = 0;
    int count = 0;
    char dest_buf[MIL];

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
                         "[destination]       [default] [current] [maptile]{x\n\r");

    for (pVLink = pWilds->pVLink; pVLink; pVLink = pVLink->next)
    {
        if (pVLink->destination_mode == VLINK_DEST_DUNGEON)
        {
            snprintf(dest_buf, sizeof(dest_buf), "dng %ld#%ld f%d",
                pVLink->dest_load.auid,
                pVLink->dest_load.vnum,
                UMAX(1, pVLink->dungeon_floor));
        }
        else if (resolve_widevnum(pVLink->destvnum, NULL, &room_wnum) && room_wnum.pArea)
        {
            snprintf(dest_buf, sizeof(dest_buf), "%ld#%ld",
                room_wnum.pArea->uid,
                room_wnum.vnum);
        }
        else if (pVLink->dest_load.auid > 0 && pVLink->dest_load.vnum > 0)
        {
            snprintf(dest_buf, sizeof(dest_buf), "%ld#%ld",
                pVLink->dest_load.auid,
                pVLink->dest_load.vnum);
        }
        else
        {
            snprintf(dest_buf, sizeof(dest_buf), "%ld", pVLink->destvnum);
        }

        sprintf(buf, "  %-5d ({W%6ld{x)  {W%6d   %6d   %-9s   %-17s %10s%10s%s{x\n\r",
            vlnum++,
            pVLink->uid,
            pVLink->wildsorigin_x,
            pVLink->wildsorigin_y,
            dir_name[pVLink->door],
            dest_buf,
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

static bool wedit_region_same_group(WILDS_REGION *a, WILDS_REGION *b)
{
    if (!a || !b)
        return false;

    if (a->uid > 0 && b->uid > 0)
        return a->uid == b->uid;

    if (!IS_NULLSTR(a->name) && !IS_NULLSTR(b->name))
        return !str_cmp(a->name, b->name);

    return a == b;
}

static bool wedit_region_is_representative(WILDS_DATA *pWilds, WILDS_REGION *candidate)
{
    WILDS_REGION *iter;

    if (!pWilds || !candidate)
        return false;

    for (iter = pWilds->pRegion; iter && iter != candidate; iter = iter->next)
        if (wedit_region_same_group(iter, candidate))
            return false;

    return true;
}

static int wedit_region_group_count(WILDS_DATA *pWilds)
{
    WILDS_REGION *iter;
    int count = 0;

    if (!pWilds)
        return 0;

    for (iter = pWilds->pRegion; iter; iter = iter->next)
        if (wedit_region_is_representative(pWilds, iter))
            count++;

    return count;
}

static WILDS_REGION *wedit_region_get_group_by_index(WILDS_DATA *pWilds, int index)
{
    WILDS_REGION *iter;
    int count = 0;

    if (!pWilds || index < 1)
        return NULL;

    for (iter = pWilds->pRegion; iter; iter = iter->next)
    {
        if (!wedit_region_is_representative(pWilds, iter))
            continue;

        if (++count == index)
            return iter;
    }

    return NULL;
}

static WILDS_REGION *wedit_region_find_group(WILDS_DATA *pWilds, const char *selector)
{
    WILDS_REGION *iter;
    long value;

    if (!pWilds || IS_NULLSTR(selector))
        return NULL;

    if (is_number((char *)selector))
    {
        value = atol(selector);

        for (iter = pWilds->pRegion; iter; iter = iter->next)
            if (iter->uid > 0 && iter->uid == value && wedit_region_is_representative(pWilds, iter))
                return iter;

        return wedit_region_get_group_by_index(pWilds, (int)value);
    }

    for (iter = pWilds->pRegion; iter; iter = iter->next)
    {
        if (!wedit_region_is_representative(pWilds, iter))
            continue;

        if (!IS_NULLSTR(iter->name) && !str_cmp(iter->name, selector))
            return iter;
    }

    return NULL;
}

static int wedit_region_box_count(WILDS_DATA *pWilds, WILDS_REGION *group)
{
    WILDS_REGION *iter;
    int count = 0;

    if (!pWilds || !group)
        return 0;

    for (iter = pWilds->pRegion; iter; iter = iter->next)
    {
        if (!wedit_region_same_group(iter, group))
            continue;

        if (iter->startx < 0 || iter->starty < 0 || iter->endx < iter->startx || iter->endy < iter->starty)
            continue;

        count++;
    }

    return count;
}

static WILDS_REGION *wedit_region_get_box_by_index(WILDS_DATA *pWilds, WILDS_REGION *group, int index)
{
    WILDS_REGION *iter;
    int count = 0;

    if (!pWilds || !group || index < 1)
        return NULL;

    for (iter = pWilds->pRegion; iter; iter = iter->next)
    {
        if (!wedit_region_same_group(iter, group))
            continue;

        if (iter->startx < 0 || iter->starty < 0 || iter->endx < iter->startx || iter->endy < iter->starty)
            continue;

        if (++count == index)
            return iter;
    }

    return NULL;
}

static void wedit_region_apply_group_metadata(WILDS_DATA *pWilds, WILDS_REGION *group)
{
    WILDS_REGION *iter;

    if (!pWilds || !group)
        return;

    for (iter = pWilds->pRegion; iter; iter = iter->next)
    {
        if (!wedit_region_same_group(iter, group) || iter == group)
            continue;

        iter->region = group->region;
        iter->area_place_flags = group->area_place_flags;

        free_string(iter->name);
        iter->name = str_dup(IS_NULLSTR(group->name) ? "" : group->name);

        wedit_region_spawn_list_copy(iter->spawn_mobs, group->spawn_mobs);
        wedit_region_spawn_list_copy(iter->spawn_objs, group->spawn_objs);

        iter->uid = group->uid;
    }
}

static int wedit_region_delete_group(WILDS_DATA *pWilds, WILDS_REGION *group)
{
    WILDS_REGION *iter;
    WILDS_REGION *next;
    int removed = 0;

    if (!pWilds || !group)
        return 0;

    for (iter = pWilds->pRegion; iter; iter = next)
    {
        next = iter->next;
        if (!wedit_region_same_group(iter, group))
            continue;

        del_region(pWilds, iter);
        removed++;
    }

    return removed;
}

WEDIT (wedit_region)
{
    WILDS_DATA *pWilds;
    WILDS_REGION *pRegion;
    char buf[MSL];
    char arg[MIL];
    char selector[MIL];
    char subcmd[MIL];
    char subarg[MIL];

    EDIT_WILDS(ch, pWilds);

    argument = one_argument(argument, arg);
    if (arg[0] != '\0')
    {
        if (!str_prefix(arg, "list"))
        {
            BUFFER *buffer = new_buf();
            bool append_ok = true;
            int i = 0;

            sprintf(buf, "Default Region:  %s\n\r\n\r", flag_string(wilderness_regions, pWilds->defaultRegion));
            if (!add_buf(buffer, buf)
                || !add_buf(buffer, "     [#] [uid]   [name]                  [boxes] [region]            [place]\n\r")
                || !add_buf(buffer, "================================================================================\n\r")) {
                send_to_char("Region group list output exceeded buffer limits.\n\r", ch);
                free_buf(buffer);
                return false;
            }

            for (pRegion = pWilds->pRegion; pRegion; pRegion = pRegion->next)
            {
                if (!wedit_region_is_representative(pWilds, pRegion))
                    continue;

                sprintf(buf, "%6d %6ld %-24.24s %6d %-18s %-15s\n\r",
                    ++i,
                    pRegion->uid,
                    IS_NULLSTR(pRegion->name) ? "(unnamed)" : pRegion->name,
                    wedit_region_box_count(pWilds, pRegion),
                    flag_string(wilderness_regions, pRegion->region),
                    flag_string(place_flags, pRegion->area_place_flags));
                if (!add_buf(buffer, buf)) {
                    append_ok = false;
                    break;
                }
            }

            if (append_ok && i < 1)
                append_ok = add_buf(buffer, "{DNo region groups defined.{x\n\r");

            if (!append_ok) {
                send_to_char("Region group list output exceeded buffer limits.\n\r", ch);
                free_buf(buffer);
                return false;
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
            if (IS_NULLSTR(argument))
            {
                send_to_char("Syntax: region add <name>\n\r", ch);
                return false;
            }

            pRegion = new_region(pWilds);
            free_string(pRegion->name);
            pRegion->name = str_dup(argument);
            pRegion->region = pWilds->defaultRegion;
            pRegion->area_place_flags = pWilds->defaultPlaceFlags;
            add_region(pWilds, pRegion);

            send_to_char("Region group added.\n\r", ch);
            return true;
        }

        if (!str_prefix(arg, "remove") || !str_prefix(arg, "delete"))
        {
            pRegion = wedit_region_find_group(pWilds, argument);
            if (!pRegion)
            {
                send_to_char("Syntax: region remove <name|idx|uid>\n\r", ch);
                send_to_char("Use 'region list' to see valid selectors.\n\r", ch);
                return false;
            }

            if (wedit_region_delete_group(pWilds, pRegion) < 1)
            {
                send_to_char("No region group entries removed.\n\r", ch);
                return false;
            }

            send_to_char("Region group removed.\n\r", ch);
            return true;
        }

        strncpy(selector, arg, sizeof(selector) - 1);
        selector[sizeof(selector) - 1] = '\0';
        argument = one_argument(argument, subcmd);

        pRegion = wedit_region_find_group(pWilds, selector);
        if (!pRegion)
        {
            send_to_char("Unknown region group selector. Use name, list index, or uid.\n\r", ch);
            return false;
        }

        if (IS_NULLSTR(subcmd) || !str_prefix(subcmd, "show"))
        {
            BUFFER *buffer = new_buf();
            WILDS_REGION *box;
            ITERATOR sit;
            WILDS_REGION_SPAWN *spawn;
            bool append_ok = true;
            int i = 0;
            int sidx = 0;

            sprintf(buf, "Region Group: %s  (uid %ld)\n\r", IS_NULLSTR(pRegion->name) ? "(unnamed)" : pRegion->name, pRegion->uid);
            if (!add_buf(buffer, buf))
                append_ok = false;
            sprintf(buf, "Region Type:  %s\n\r", flag_string(wilderness_regions, pRegion->region));
            if (append_ok && !add_buf(buffer, buf))
                append_ok = false;
            sprintf(buf, "Place Type:   %s\n\r\n\r", flag_string(place_flags, pRegion->area_place_flags));
            if (append_ok && !add_buf(buffer, buf))
                append_ok = false;
            if (append_ok && !add_buf(buffer, "Mob Spawns:\n\r"))
                append_ok = false;

            if (!append_ok) {
                send_to_char("Region detail output exceeded buffer limits.\n\r", ch);
                free_buf(buffer);
                return false;
            }

            iterator_start(&sit, pRegion->spawn_mobs);
            while ((spawn = (WILDS_REGION_SPAWN *)iterator_nextdata(&sit)) != NULL)
            {
                char *req_text = requirements_json_to_text(spawn->requirements);
                sprintf(buf, "  [%2d] %s (chance %d%%, cap %d) req: %s\n\r",
                    ++sidx,
                    IS_NULLSTR(spawn->wnum) ? "(none)" : spawn->wnum,
                    spawn->chance,
                    spawn->cap,
                    IS_NULLSTR(req_text) ? "(none)" : req_text);
                if (!add_buf(buffer, buf)) {
                    append_ok = false;
                    if (req_text)
                        free(req_text);
                    break;
                }
                if (req_text)
                    free(req_text);
            }
            iterator_stop(&sit);

            if (!append_ok) {
                send_to_char("Region detail output exceeded buffer limits.\n\r", ch);
                free_buf(buffer);
                return false;
            }

            if (sidx < 1 && !add_buf(buffer, "  {D(none){x\n\r")) {
                send_to_char("Region detail output exceeded buffer limits.\n\r", ch);
                free_buf(buffer);
                return false;
            }

            if (!add_buf(buffer, "Object Spawns:\n\r")) {
                send_to_char("Region detail output exceeded buffer limits.\n\r", ch);
                free_buf(buffer);
                return false;
            }
            sidx = 0;

            iterator_start(&sit, pRegion->spawn_objs);
            while ((spawn = (WILDS_REGION_SPAWN *)iterator_nextdata(&sit)) != NULL)
            {
                char *req_text = requirements_json_to_text(spawn->requirements);
                sprintf(buf, "  [%2d] %s (chance %d%%, cap %d) req: %s\n\r",
                    ++sidx,
                    IS_NULLSTR(spawn->wnum) ? "(none)" : spawn->wnum,
                    spawn->chance,
                    spawn->cap,
                    IS_NULLSTR(req_text) ? "(none)" : req_text);
                if (!add_buf(buffer, buf)) {
                    append_ok = false;
                    if (req_text)
                        free(req_text);
                    break;
                }
                if (req_text)
                    free(req_text);
            }
            iterator_stop(&sit);

            if (!append_ok) {
                send_to_char("Region detail output exceeded buffer limits.\n\r", ch);
                free_buf(buffer);
                return false;
            }

            if (sidx < 1 && !add_buf(buffer, "  {D(none){x\n\r")) {
                send_to_char("Region detail output exceeded buffer limits.\n\r", ch);
                free_buf(buffer);
                return false;
            }

            if (!add_buf(buffer, "\n\r")
                || !add_buf(buffer, "     [box] [start x] [start y] [end x] [end y]\n\r")
                || !add_buf(buffer, "===============================================\n\r")) {
                send_to_char("Region detail output exceeded buffer limits.\n\r", ch);
                free_buf(buffer);
                return false;
            }

            for (box = pWilds->pRegion; box; box = box->next)
            {
                if (!wedit_region_same_group(box, pRegion))
                    continue;

                if (box->startx < 0 || box->starty < 0 || box->endx < box->startx || box->endy < box->starty)
                    continue;

                sprintf(buf, "%8d %9d %9d %7d %7d\n\r", ++i, box->startx, box->starty, box->endx, box->endy);
                if (!add_buf(buffer, buf)) {
                    append_ok = false;
                    break;
                }
            }

            if (append_ok && i < 1)
                append_ok = add_buf(buffer, "{D(no coordinate boxes yet){x\n\r");

            if (!append_ok) {
                send_to_char("Region detail output exceeded buffer limits.\n\r", ch);
                free_buf(buffer);
                return false;
            }

            page_to_char(buf_string(buffer), ch);
            free_buf(buffer);
            return false;
        }

        if (!str_prefix(subcmd, "coords"))
        {
            char ccmd[MIL];
            char c1[MIL], c2[MIL], c3[MIL], c4[MIL];
            int startx, starty, endx, endy;
            WILDS_REGION *box;

            argument = one_argument(argument, ccmd);

            if (!str_prefix(ccmd, "add"))
            {
                argument = one_argument(argument, c1);
                argument = one_argument(argument, c2);
                argument = one_argument(argument, c3);
                argument = one_argument(argument, c4);

                if (!is_number(c1) || !is_number(c2) || !is_number(c3) || !is_number(c4))
                {
                    send_to_char("Syntax: region <name|idx|uid> coords add <startx> <starty> <endx> <endy>\n\r", ch);
                    return false;
                }

                startx = atoi(c1);
                starty = atoi(c2);
                endx = atoi(c3);
                endy = atoi(c4);

                if (startx < 0 || startx >= pWilds->map_size_x || endx < 0 || endx >= pWilds->map_size_x)
                {
                    sprintf(buf, "X coordinates must be between 0 and %d.\n\r", pWilds->map_size_x - 1);
                    send_to_char(buf, ch);
                    return false;
                }

                if (starty < 0 || starty >= pWilds->map_size_y || endy < 0 || endy >= pWilds->map_size_y)
                {
                    sprintf(buf, "Y coordinates must be between 0 and %d.\n\r", pWilds->map_size_y - 1);
                    send_to_char(buf, ch);
                    return false;
                }

                box = new_region(pWilds);
                box->uid = pRegion->uid;
                box->region = pRegion->region;
                box->area_place_flags = pRegion->area_place_flags;
                free_string(box->name);
                box->name = str_dup(IS_NULLSTR(pRegion->name) ? "" : pRegion->name);
                box->startx = UMIN(startx, endx);
                box->starty = UMIN(starty, endy);
                box->endx = UMAX(startx, endx);
                box->endy = UMAX(starty, endy);
                add_region(pWilds, box);

                send_to_char("Region coordinate box added.\n\r", ch);
                return true;
            }

            if (!str_prefix(ccmd, "delete") || !str_prefix(ccmd, "remove"))
            {
                int idx;

                argument = one_argument(argument, c1);
                if (!is_number(c1))
                {
                    send_to_char("Syntax: region <name|idx|uid> coords delete <box#>\n\r", ch);
                    return false;
                }

                idx = atoi(c1);
                box = wedit_region_get_box_by_index(pWilds, pRegion, idx);
                if (!box)
                {
                    sprintf(buf, "Box index must be between 1 and %d.\n\r", wedit_region_box_count(pWilds, pRegion));
                    send_to_char(buf, ch);
                    return false;
                }

                del_region(pWilds, box);
                send_to_char("Region coordinate box removed.\n\r", ch);
                return true;
            }

            send_to_char("Syntax: region <name|idx|uid> coords add <startx> <starty> <endx> <endy>\n\r", ch);
            send_to_char("        region <name|idx|uid> coords delete <box#>\n\r", ch);
            return false;
        }

        if (!str_prefix(subcmd, "name"))
        {
            if (IS_NULLSTR(argument))
            {
                send_to_char("Syntax: region <name|idx|uid> name <new name>\n\r", ch);
                return false;
            }

            free_string(pRegion->name);
            pRegion->name = str_dup(argument);
            wedit_region_apply_group_metadata(pWilds, pRegion);
            send_to_char("Region group name updated.\n\r", ch);
            return true;
        }

        if (!str_prefix(subcmd, "region"))
        {
            int value;

            if (IS_NULLSTR(argument) || (value = flag_value(wilderness_regions, argument)) == NO_FLAG)
            {
                send_to_char("Syntax: region <name|idx|uid> region <region>\n\r", ch);
                send_to_char("Type '? wilderness_regions' to list valid regions.\n\r", ch);
                return false;
            }

            pRegion->region = value;
            wedit_region_apply_group_metadata(pWilds, pRegion);
            send_to_char("Region group type updated.\n\r", ch);
            return true;
        }

        if (!str_prefix(subcmd, "placetype") || !str_prefix(subcmd, "place"))
        {
            int value;

            if (IS_NULLSTR(argument))
            {
                send_to_char("Syntax: region <name|idx|uid> placetype <placetype|none>\n\r", ch);
                return false;
            }

            if (!str_cmp(argument, "none"))
                value = PLACE_NOWHERE;
            else
                value = flag_value(place_flags, argument);

            if (value == NO_FLAG)
            {
                send_to_char("Syntax: region <name|idx|uid> placetype <placetype|none>\n\r", ch);
                send_to_char("Type '? placetype' for valid values.\n\r", ch);
                return false;
            }

            pRegion->area_place_flags = value;
            wedit_region_apply_group_metadata(pWilds, pRegion);
            send_to_char("Region group place type updated.\n\r", ch);
            return true;
        }

        if (!str_prefix(subcmd, "spawnmob"))
        {
            char action[MIL];
            char arg2[MIL];
            char arg3[MIL];
            int index;
            WILDS_REGION_SPAWN *spawn;

            argument = one_argument(argument, action);

            if (IS_NULLSTR(action) || !str_prefix(action, "list"))
            {
                BUFFER *buffer = new_buf();
                ITERATOR it;
                bool append_ok = true;
                int i = 0;

                if (!add_buf(buffer, "Mob spawn entries:\n\r")) {
                    send_to_char("Mob spawn list output exceeded buffer limits.\n\r", ch);
                    free_buf(buffer);
                    return false;
                }
                iterator_start(&it, pRegion->spawn_mobs);
                while ((spawn = (WILDS_REGION_SPAWN *)iterator_nextdata(&it)) != NULL)
                {
                    char *req_text = requirements_json_to_text(spawn->requirements);
                    char spawn_name[MSL];

                    wedit_region_spawn_format_wnum(pWilds, true, spawn, spawn_name, sizeof(spawn_name));
                    snprintf(buf, sizeof(buf), "  [%2d] %.200s (chance %d%%, cap %d) req: %.3500s\n\r",
                        ++i,
                        spawn_name,
                        spawn->chance,
                        spawn->cap,
                        IS_NULLSTR(req_text) ? "(none)" : req_text);
                    if (!add_buf(buffer, buf)) {
                        append_ok = false;
                        if (req_text)
                            free(req_text);
                        break;
                    }
                    if (req_text)
                        free(req_text);
                }
                iterator_stop(&it);

                if (append_ok && i < 1)
                    append_ok = add_buf(buffer, "  {D(none){x\n\r");

                if (!append_ok) {
                    send_to_char("Mob spawn list output exceeded buffer limits.\n\r", ch);
                    free_buf(buffer);
                    return false;
                }

                page_to_char(buf_string(buffer), ch);
                free_buf(buffer);
                return false;
            }

            if (!str_prefix(action, "add"))
            {
                char chance_arg[MIL];
                char cap_arg[MIL];
                char err[256];

                argument = one_argument(argument, arg2);
                argument = one_argument(argument, chance_arg);
                argument = one_argument(argument, cap_arg);

                if (IS_NULLSTR(arg2))
                {
                    send_to_char("Syntax: region <name|idx|uid> spawnmob add <wnum> [chance] [cap] [req]\n\r", ch);
                    return false;
                }

                spawn = wedit_region_spawn_new();
                if (!spawn)
                    return false;

                spawn->chance = 25;
                spawn->cap = 1;

                if (!wedit_region_set_spawn_wnum(pWilds, true, &spawn->wnum, arg2))
                {
                    send_to_char("Invalid mob wnum.\n\r", ch);
                    free_string(spawn->wnum);
                    free_string(spawn->requirements);
                    free_mem(spawn, sizeof(*spawn));
                    return false;
                }

                if (!IS_NULLSTR(chance_arg))
                {
                    if (!is_number(chance_arg))
                    {
                        send_to_char("Spawn mob chance must be numeric (0-100).\n\r", ch);
                        free_string(spawn->wnum);
                        free_string(spawn->requirements);
                        free_mem(spawn, sizeof(*spawn));
                        return false;
                    }
                    spawn->chance = URANGE(0, atoi(chance_arg), 100);
                }

                if (!IS_NULLSTR(cap_arg))
                {
                    if (!is_number(cap_arg))
                    {
                        send_to_char("Spawn mob cap must be numeric (>= 0).\n\r", ch);
                        free_string(spawn->wnum);
                        free_string(spawn->requirements);
                        free_mem(spawn, sizeof(*spawn));
                        return false;
                    }
                    spawn->cap = UMAX(0, atoi(cap_arg));
                }

                if (!IS_NULLSTR(argument))
                {
                    if (!wedit_region_set_requirements_json(&spawn->requirements, argument, err, sizeof(err)))
                    {
                        printf_to_char(ch, "Mob requirement parse error: %s\n\r", err);
                        free_string(spawn->wnum);
                        free_string(spawn->requirements);
                        free_mem(spawn, sizeof(*spawn));
                        return false;
                    }
                }

                list_appendlink(pRegion->spawn_mobs, spawn);
                wedit_region_apply_group_metadata(pWilds, pRegion);
                send_to_char("Region group mob spawn entry added.\n\r", ch);
                return true;
            }

            if (!is_number(action))
            {
                send_to_char("Syntax: region <name|idx|uid> spawnmob add <wnum> [chance] [cap] [req]\n\r", ch);
                send_to_char("        region <name|idx|uid> spawnmob <idx> <show|remove|wnum|chance|cap|req> [value]\n\r", ch);
                return false;
            }

            index = atoi(action);
            spawn = wedit_region_spawn_get(pRegion->spawn_mobs, index);
            if (!spawn)
            {
                printf_to_char(ch, "Spawn mob index must be between 1 and %d.\n\r", list_size(pRegion->spawn_mobs));
                return false;
            }

            argument = one_argument(argument, arg2);

            if (IS_NULLSTR(arg2) || !str_prefix(arg2, "show"))
            {
                char *req_text = requirements_json_to_text(spawn->requirements);
                char spawn_name[MSL];

                wedit_region_spawn_format_wnum(pWilds, true, spawn, spawn_name, sizeof(spawn_name));
                printf_to_char(ch, "[%d] %s (chance %d%%, cap %d) req: %s\n\r",
                    index,
                    spawn_name,
                    spawn->chance,
                    spawn->cap,
                    IS_NULLSTR(req_text) ? "(none)" : req_text);
                if (req_text)
                    free(req_text);
                return false;
            }

            if (!str_prefix(arg2, "remove") || !str_prefix(arg2, "delete"))
            {
                list_remnthlink(pRegion->spawn_mobs, index, true);
                wedit_region_apply_group_metadata(pWilds, pRegion);
                send_to_char("Region group mob spawn entry removed.\n\r", ch);
                return true;
            }

            if (!str_prefix(arg2, "wnum") || !str_prefix(arg2, "set"))
            {
                argument = one_argument(argument, arg3);
                if (!wedit_region_set_spawn_wnum(pWilds, true, &spawn->wnum, arg3))
                {
                    send_to_char("Syntax: region <name|idx|uid> spawnmob <idx> wnum <wnum|none>\n\r", ch);
                    return false;
                }
                wedit_region_apply_group_metadata(pWilds, pRegion);
                send_to_char("Mob spawn wnum updated.\n\r", ch);
                return true;
            }

            if (!str_prefix(arg2, "chance"))
            {
                argument = one_argument(argument, arg3);
                if (!is_number(arg3))
                {
                    send_to_char("Syntax: region <name|idx|uid> spawnmob <idx> chance <0-100>\n\r", ch);
                    return false;
                }
                spawn->chance = URANGE(0, atoi(arg3), 100);
                wedit_region_apply_group_metadata(pWilds, pRegion);
                send_to_char("Mob spawn chance updated.\n\r", ch);
                return true;
            }

            if (!str_prefix(arg2, "cap"))
            {
                argument = one_argument(argument, arg3);
                if (!is_number(arg3))
                {
                    send_to_char("Syntax: region <name|idx|uid> spawnmob <idx> cap <0+>\n\r", ch);
                    return false;
                }
                spawn->cap = UMAX(0, atoi(arg3));
                wedit_region_apply_group_metadata(pWilds, pRegion);
                send_to_char("Mob spawn cap updated.\n\r", ch);
                return true;
            }

            if (!str_prefix(arg2, "req") || !str_prefix(arg2, "requirements"))
            {
                char err[256];
                if (!wedit_region_set_requirements_json(&spawn->requirements, argument, err, sizeof(err)))
                {
                    printf_to_char(ch, "Mob requirement parse error: %s\n\r", err);
                    return false;
                }
                wedit_region_apply_group_metadata(pWilds, pRegion);
                send_to_char("Mob spawn requirements updated.\n\r", ch);
                return true;
            }

            if (!wedit_region_set_spawn_wnum(pWilds, true, &spawn->wnum, arg2))
            {
                send_to_char("Syntax: region <name|idx|uid> spawnmob <idx> <wnum|none> [chance] [cap]\n\r", ch);
                return false;
            }

            argument = one_argument(argument, arg3);
            if (!IS_NULLSTR(arg3))
            {
                if (!is_number(arg3))
                {
                    send_to_char("Spawn mob chance must be numeric (0-100).\n\r", ch);
                    return false;
                }
                spawn->chance = URANGE(0, atoi(arg3), 100);
            }

            argument = one_argument(argument, subarg);
            if (!IS_NULLSTR(subarg))
            {
                if (!is_number(subarg))
                {
                    send_to_char("Spawn mob cap must be numeric (>= 0).\n\r", ch);
                    return false;
                }
                spawn->cap = UMAX(0, atoi(subarg));
            }

            wedit_region_apply_group_metadata(pWilds, pRegion);
            send_to_char("Mob spawn entry updated.\n\r", ch);
            return true;
        }

        if (!str_prefix(subcmd, "spawnobj"))
        {
            char action[MIL];
            char arg2[MIL];
            char arg3[MIL];
            int index;
            WILDS_REGION_SPAWN *spawn;

            argument = one_argument(argument, action);

            if (IS_NULLSTR(action) || !str_prefix(action, "list"))
            {
                BUFFER *buffer = new_buf();
                ITERATOR it;
                bool append_ok = true;
                int i = 0;

                if (!add_buf(buffer, "Object spawn entries:\n\r")) {
                    send_to_char("Object spawn list output exceeded buffer limits.\n\r", ch);
                    free_buf(buffer);
                    return false;
                }
                iterator_start(&it, pRegion->spawn_objs);
                while ((spawn = (WILDS_REGION_SPAWN *)iterator_nextdata(&it)) != NULL)
                {
                    char *req_text = requirements_json_to_text(spawn->requirements);
                    char spawn_name[MSL];

                    wedit_region_spawn_format_wnum(pWilds, false, spawn, spawn_name, sizeof(spawn_name));
                    snprintf(buf, sizeof(buf), "  [%2d] %.200s (chance %d%%, cap %d) req: %.3500s\n\r",
                        ++i,
                        spawn_name,
                        spawn->chance,
                        spawn->cap,
                        IS_NULLSTR(req_text) ? "(none)" : req_text);
                    if (!add_buf(buffer, buf)) {
                        append_ok = false;
                        if (req_text)
                            free(req_text);
                        break;
                    }
                    if (req_text)
                        free(req_text);
                }
                iterator_stop(&it);

                if (append_ok && i < 1)
                    append_ok = add_buf(buffer, "  {D(none){x\n\r");

                if (!append_ok) {
                    send_to_char("Object spawn list output exceeded buffer limits.\n\r", ch);
                    free_buf(buffer);
                    return false;
                }

                page_to_char(buf_string(buffer), ch);
                free_buf(buffer);
                return false;
            }

            if (!str_prefix(action, "add"))
            {
                char chance_arg[MIL];
                char cap_arg[MIL];
                char err[256];

                argument = one_argument(argument, arg2);
                argument = one_argument(argument, chance_arg);
                argument = one_argument(argument, cap_arg);

                if (IS_NULLSTR(arg2))
                {
                    send_to_char("Syntax: region <name|idx|uid> spawnobj add <wnum> [chance] [cap] [req]\n\r", ch);
                    return false;
                }

                spawn = wedit_region_spawn_new();
                if (!spawn)
                    return false;

                spawn->chance = 25;
                spawn->cap = 1;

                if (!wedit_region_set_spawn_wnum(pWilds, false, &spawn->wnum, arg2))
                {
                    send_to_char("Invalid obj wnum.\n\r", ch);
                    free_string(spawn->wnum);
                    free_string(spawn->requirements);
                    free_mem(spawn, sizeof(*spawn));
                    return false;
                }

                if (!IS_NULLSTR(chance_arg))
                {
                    if (!is_number(chance_arg))
                    {
                        send_to_char("Spawn obj chance must be numeric (0-100).\n\r", ch);
                        free_string(spawn->wnum);
                        free_string(spawn->requirements);
                        free_mem(spawn, sizeof(*spawn));
                        return false;
                    }
                    spawn->chance = URANGE(0, atoi(chance_arg), 100);
                }

                if (!IS_NULLSTR(cap_arg))
                {
                    if (!is_number(cap_arg))
                    {
                        send_to_char("Spawn obj cap must be numeric (>= 0).\n\r", ch);
                        free_string(spawn->wnum);
                        free_string(spawn->requirements);
                        free_mem(spawn, sizeof(*spawn));
                        return false;
                    }
                    spawn->cap = UMAX(0, atoi(cap_arg));
                }

                if (!IS_NULLSTR(argument))
                {
                    if (!wedit_region_set_requirements_json(&spawn->requirements, argument, err, sizeof(err)))
                    {
                        printf_to_char(ch, "Obj requirement parse error: %s\n\r", err);
                        free_string(spawn->wnum);
                        free_string(spawn->requirements);
                        free_mem(spawn, sizeof(*spawn));
                        return false;
                    }
                }

                list_appendlink(pRegion->spawn_objs, spawn);
                wedit_region_apply_group_metadata(pWilds, pRegion);
                send_to_char("Region group object spawn entry added.\n\r", ch);
                return true;
            }

            if (!is_number(action))
            {
                send_to_char("Syntax: region <name|idx|uid> spawnobj add <wnum> [chance] [cap] [req]\n\r", ch);
                send_to_char("        region <name|idx|uid> spawnobj <idx> <show|remove|wnum|chance|cap|req> [value]\n\r", ch);
                return false;
            }

            index = atoi(action);
            spawn = wedit_region_spawn_get(pRegion->spawn_objs, index);
            if (!spawn)
            {
                printf_to_char(ch, "Spawn obj index must be between 1 and %d.\n\r", list_size(pRegion->spawn_objs));
                return false;
            }

            argument = one_argument(argument, arg2);

            if (IS_NULLSTR(arg2) || !str_prefix(arg2, "show"))
            {
                char *req_text = requirements_json_to_text(spawn->requirements);
                char spawn_name[MSL];

                wedit_region_spawn_format_wnum(pWilds, false, spawn, spawn_name, sizeof(spawn_name));
                printf_to_char(ch, "[%d] %s (chance %d%%, cap %d) req: %s\n\r",
                    index,
                    spawn_name,
                    spawn->chance,
                    spawn->cap,
                    IS_NULLSTR(req_text) ? "(none)" : req_text);
                if (req_text)
                    free(req_text);
                return false;
            }

            if (!str_prefix(arg2, "remove") || !str_prefix(arg2, "delete"))
            {
                list_remnthlink(pRegion->spawn_objs, index, true);
                wedit_region_apply_group_metadata(pWilds, pRegion);
                send_to_char("Region group object spawn entry removed.\n\r", ch);
                return true;
            }

            if (!str_prefix(arg2, "wnum") || !str_prefix(arg2, "set"))
            {
                argument = one_argument(argument, arg3);
                if (!wedit_region_set_spawn_wnum(pWilds, false, &spawn->wnum, arg3))
                {
                    send_to_char("Syntax: region <name|idx|uid> spawnobj <idx> wnum <wnum|none>\n\r", ch);
                    return false;
                }
                wedit_region_apply_group_metadata(pWilds, pRegion);
                send_to_char("Object spawn wnum updated.\n\r", ch);
                return true;
            }

            if (!str_prefix(arg2, "chance"))
            {
                argument = one_argument(argument, arg3);
                if (!is_number(arg3))
                {
                    send_to_char("Syntax: region <name|idx|uid> spawnobj <idx> chance <0-100>\n\r", ch);
                    return false;
                }
                spawn->chance = URANGE(0, atoi(arg3), 100);
                wedit_region_apply_group_metadata(pWilds, pRegion);
                send_to_char("Object spawn chance updated.\n\r", ch);
                return true;
            }

            if (!str_prefix(arg2, "cap"))
            {
                argument = one_argument(argument, arg3);
                if (!is_number(arg3))
                {
                    send_to_char("Syntax: region <name|idx|uid> spawnobj <idx> cap <0+>\n\r", ch);
                    return false;
                }
                spawn->cap = UMAX(0, atoi(arg3));
                wedit_region_apply_group_metadata(pWilds, pRegion);
                send_to_char("Object spawn cap updated.\n\r", ch);
                return true;
            }

            if (!str_prefix(arg2, "req") || !str_prefix(arg2, "requirements"))
            {
                char err[256];
                if (!wedit_region_set_requirements_json(&spawn->requirements, argument, err, sizeof(err)))
                {
                    printf_to_char(ch, "Object requirement parse error: %s\n\r", err);
                    return false;
                }
                wedit_region_apply_group_metadata(pWilds, pRegion);
                send_to_char("Object spawn requirements updated.\n\r", ch);
                return true;
            }

            if (!wedit_region_set_spawn_wnum(pWilds, false, &spawn->wnum, arg2))
            {
                send_to_char("Syntax: region <name|idx|uid> spawnobj <idx> <wnum|none> [chance] [cap]\n\r", ch);
                return false;
            }

            argument = one_argument(argument, arg3);
            if (!IS_NULLSTR(arg3))
            {
                if (!is_number(arg3))
                {
                    send_to_char("Spawn obj chance must be numeric (0-100).\n\r", ch);
                    return false;
                }
                spawn->chance = URANGE(0, atoi(arg3), 100);
            }

            argument = one_argument(argument, subarg);
            if (!IS_NULLSTR(subarg))
            {
                if (!is_number(subarg))
                {
                    send_to_char("Spawn obj cap must be numeric (>= 0).\n\r", ch);
                    return false;
                }
                spawn->cap = UMAX(0, atoi(subarg));
            }

            wedit_region_apply_group_metadata(pWilds, pRegion);
            send_to_char("Object spawn entry updated.\n\r", ch);
            return true;
        }

        if (!str_prefix(subcmd, "delete") || !str_prefix(subcmd, "remove"))
        {
            if (wedit_region_delete_group(pWilds, pRegion) < 1)
            {
                send_to_char("No region group entries removed.\n\r", ch);
                return false;
            }

            send_to_char("Region group removed.\n\r", ch);
            return true;
        }

        if (!str_prefix(subcmd, "add"))
        {
            argument = one_argument(argument, subarg);

            if (!str_prefix(subarg, "show"))
            {
                do_function(ch, &do_wedit, "show");
                return false;
            }

            if (!str_prefix(subarg, "delete") || !str_prefix(subarg, "remove"))
            {
                int idx;
                WILDS_REGION *box;

                argument = one_argument(argument, subarg);
                if (!is_number(subarg))
                {
                    send_to_char("Syntax: region <name|idx|uid> add delete <box#>\n\r", ch);
                    return false;
                }

                idx = atoi(subarg);
                box = wedit_region_get_box_by_index(pWilds, pRegion, idx);
                if (!box)
                {
                    sprintf(buf, "Box index must be between 1 and %d.\n\r", wedit_region_box_count(pWilds, pRegion));
                    send_to_char(buf, ch);
                    return false;
                }

                del_region(pWilds, box);
                send_to_char("Region coordinate box removed.\n\r", ch);
                return true;
            }

            send_to_char("Syntax: region <name|idx|uid> add show\n\r", ch);
            send_to_char("        region <name|idx|uid> add delete <box#>\n\r", ch);
            return false;
        }
    }

    sprintf(buf, "Region groups: %d\n\r", wedit_region_group_count(pWilds));
    send_to_char(buf, ch);
    send_to_char("Syntax: region list\n\r", ch);
    send_to_char("        region default <region|none>\n\r", ch);
    send_to_char("        region add <name>\n\r", ch);
    send_to_char("        region remove <name|idx|uid>\n\r", ch);
    send_to_char("        region <name|idx|uid> show\n\r", ch);
    send_to_char("        region <name|idx|uid> name <new name>\n\r", ch);
    send_to_char("        region <name|idx|uid> region <region>\n\r", ch);
    send_to_char("        region <name|idx|uid> placetype <placetype|none>\n\r", ch);
    send_to_char("        region <name|idx|uid> spawnmob list\n\r", ch);
    send_to_char("        region <name|idx|uid> spawnmob add <wnum> [chance] [cap] [req]\n\r", ch);
    send_to_char("        region <name|idx|uid> spawnmob <idx> <show|remove|wnum|chance|cap|req> [value]\n\r", ch);
    send_to_char("        region <name|idx|uid> spawnobj list\n\r", ch);
    send_to_char("        region <name|idx|uid> spawnobj add <wnum> [chance] [cap] [req]\n\r", ch);
    send_to_char("        region <name|idx|uid> spawnobj <idx> <show|remove|wnum|chance|cap|req> [value]\n\r", ch);
    send_to_char("        region <name|idx|uid> coords add <startx> <starty> <endx> <endy>\n\r", ch);
    send_to_char("        region <name|idx|uid> coords delete <box#>\n\r", ch);
    send_to_char("        region <name|idx|uid> add show\n\r", ch);
    send_to_char("        region <name|idx|uid> add delete <box#>\n\r", ch);
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

WEDIT (wedit_overlay)
{
    WILDS_DATA *pWilds;
    WILDS_CHUNK *chunk;
    WILDS_OVERLAY *overlay;
    char arg[MIL], arg2[MIL], arg3[MIL], arg4[MIL], arg5[MIL], arg6[MIL], arg7[MIL], arg8[MIL];

    EDIT_WILDS(ch, pWilds);

    argument = one_argument(argument, arg);

    if (IS_NULLSTR(arg))
    {
        send_to_char("Syntax: overlay list\n\r", ch);
        send_to_char("        overlay add <x1> <y1> <x2> <y2> <terrain_token> [duration_sec] [region]\n\r", ch);
        send_to_char("        overlay addregion <region> <terrain_token> [duration_sec]\n\r", ch);
        send_to_char("        overlay set <x> <y> <terrain_token>\n\r", ch);
        send_to_char("        overlay clear <zone_id>\n\r", ch);
        send_to_char("        overlay clearregion <region>\n\r", ch);
        send_to_char("        overlay cleanup\n\r", ch);
        send_to_char("        overlay tile <x> <y>\n\r", ch);
        return false;
    }

    if (!str_prefix(arg, "cleanup"))
    {
        int removed = wilds_cleanup_expired_temporary_zones(pWilds);
        printf_to_char(ch, "Overlay cleanup removed %d expired record(s).\n\r", removed);
        return true;
    }

    if (!str_prefix(arg, "list"))
    {
        BUFFER *out = new_buf();
        bool append_ok = true;
        int count = 0;

        wilds_cleanup_expired_temporary_zones(pWilds);
        if (!add_buf(out, "[WEdit Overlay] Chunked runtime overlay records:\n\r")
            || !add_buf(out, "chunk   zone_id  bounds (x1,y1)-(x2,y2)  tile  region                expires\n\r")
            || !add_buf(out, "--------------------------------------------------------------------------------\n\r")) {
            send_to_char("Overlay list output exceeded buffer limits.\n\r", ch);
            free_buf(out);
            return false;
        }

        for (chunk = pWilds->runtime_chunks; chunk; chunk = chunk->next)
        {
            for (overlay = chunk->overlays; overlay; overlay = overlay->next)
            {
                char line[MSL];
                char expires[64];

                if (overlay->expires_at > 0)
                    sprintf(expires, "%ld", (long)(overlay->expires_at - current_time));
                else
                    sprintf(expires, "permanent");

                sprintf(line,
                    "(%2d,%2d)  %-7ld  (%4d,%4d)-(%4d,%4d)   '%c'   %-20s %s\n\r",
                    chunk->cx,
                    chunk->cy,
                    overlay->zone_id,
                    overlay->x1,
                    overlay->y1,
                    overlay->x2,
                    overlay->y2,
                    overlay->tile,
                    flag_string(wilderness_regions, overlay->region),
                    expires);
                if (!add_buf(out, line)) {
                    append_ok = false;
                    break;
                }
                count++;
            }
            if (!append_ok)
                break;
        }

        if (append_ok && count == 0)
            append_ok = add_buf(out, "(none)\n\r");

        if (!append_ok) {
            send_to_char("Overlay list output exceeded buffer limits.\n\r", ch);
            free_buf(out);
            return false;
        }

        page_to_char(buf_string(out), ch);
        free_buf(out);
        return false;
    }

    if (!str_prefix(arg, "add"))
    {
        int x1, y1, x2, y2;
        int duration = 0;
        int region = REGION_UNKNOWN;
        WILDS_TERRAIN *terrain;
        long zone_id;

        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);
        argument = one_argument(argument, arg4);
        argument = one_argument(argument, arg5);
        argument = one_argument(argument, arg6);
        argument = one_argument(argument, arg7);
        argument = one_argument(argument, arg8);

        if (!is_number(arg2) || !is_number(arg3) || !is_number(arg4) || !is_number(arg5) || IS_NULLSTR(arg6))
        {
            send_to_char("Syntax: overlay add <x1> <y1> <x2> <y2> <terrain_token> [duration_sec] [region]\n\r", ch);
            return false;
        }

        x1 = atoi(arg2);
        y1 = atoi(arg3);
        x2 = atoi(arg4);
        y2 = atoi(arg5);

        terrain = get_terrain_by_token(pWilds, arg6[0]);
        if (!terrain)
        {
            send_to_char("Invalid terrain token for this wilderness.\n\r", ch);
            return false;
        }

        if (!IS_NULLSTR(arg7))
        {
            if (!is_number(arg7))
            {
                send_to_char("Duration must be a number of seconds.\n\r", ch);
                return false;
            }
            duration = atoi(arg7);
            if (duration < 0)
                duration = 0;
        }

        if (!IS_NULLSTR(arg8))
        {
            region = flag_value(wilderness_regions, arg8);
            if (region == NO_FLAG)
            {
                send_to_char("Invalid region. Type '? wilderness_regions'.\n\r", ch);
                return false;
            }
        }

        zone_id = wilds_add_temporary_zone(pWilds, x1, y1, x2, y2, terrain->mapchar, region, duration);
        if (zone_id < 1)
        {
            send_to_char("Failed to add overlay zone (bounds outside map?).\n\r", ch);
            return false;
        }

        printf_to_char(ch, "Overlay zone created with zone id %ld.\n\r", zone_id);
        return true;
    }

    if (!str_prefix(arg, "addregion"))
    {
        WILDS_REGION *region_rec;
        WILDS_TERRAIN *terrain;
        int region;
        int duration = 0;
        int created = 0;

        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);
        argument = one_argument(argument, arg4);

        if (IS_NULLSTR(arg2) || IS_NULLSTR(arg3))
        {
            send_to_char("Syntax: overlay addregion <region> <terrain_token> [duration_sec]\n\r", ch);
            return false;
        }

        region = flag_value(wilderness_regions, arg2);
        if (region == NO_FLAG)
        {
            send_to_char("Invalid region. Type '? wilderness_regions'.\n\r", ch);
            return false;
        }

        terrain = get_terrain_by_token(pWilds, arg3[0]);
        if (!terrain)
        {
            send_to_char("Invalid terrain token for this wilderness.\n\r", ch);
            return false;
        }

        if (!IS_NULLSTR(arg4))
        {
            if (!is_number(arg4))
            {
                send_to_char("Duration must be a number of seconds.\n\r", ch);
                return false;
            }
            duration = atoi(arg4);
            if (duration < 0)
                duration = 0;
        }

        for (region_rec = pWilds->pRegion; region_rec; region_rec = region_rec->next)
        {
            if (region_rec->region != region)
                continue;

            if (wilds_add_temporary_zone(
                    pWilds,
                    region_rec->startx,
                    region_rec->starty,
                    region_rec->endx,
                    region_rec->endy,
                    terrain->mapchar,
                    region,
                    duration) > 0)
            {
                created++;
            }
        }

        printf_to_char(ch, "Added %d region overlay zone(s) for %s.\n\r", created, flag_string(wilderness_regions, region));
        return created > 0;
    }

    if (!str_prefix(arg, "set"))
    {
        int x, y;
        WILDS_TERRAIN *terrain;

        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);
        argument = one_argument(argument, arg4);

        if (!is_number(arg2) || !is_number(arg3) || IS_NULLSTR(arg4))
        {
            send_to_char("Syntax: overlay set <x> <y> <terrain_token>\n\r", ch);
            return false;
        }

        x = atoi(arg2);
        y = atoi(arg3);
        terrain = get_terrain_by_token(pWilds, arg4[0]);
        if (!terrain)
        {
            send_to_char("Invalid terrain token for this wilderness.\n\r", ch);
            return false;
        }

        if (!set_wilds_runtime_tile(pWilds, x, y, terrain->mapchar))
        {
            send_to_char("Failed to set runtime tile at those coordinates.\n\r", ch);
            return false;
        }

        send_to_char("Runtime tile updated.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg, "clearregion"))
    {
        int region;
        int removed;

        argument = one_argument(argument, arg2);
        if (IS_NULLSTR(arg2))
        {
            send_to_char("Syntax: overlay clearregion <region>\n\r", ch);
            return false;
        }

        region = flag_value(wilderness_regions, arg2);
        if (region == NO_FLAG)
        {
            send_to_char("Invalid region. Type '? wilderness_regions'.\n\r", ch);
            return false;
        }

        removed = wilds_remove_region_temporary_zones(pWilds, region);
        printf_to_char(ch, "Removed %d overlay record(s) for region %s.\n\r", removed, flag_string(wilderness_regions, region));
        return removed > 0;
    }

    if (!str_prefix(arg, "clear"))
    {
        long zone_id;
        int removed;

        argument = one_argument(argument, arg2);
        if (!is_number(arg2))
        {
            send_to_char("Syntax: overlay clear <zone_id>\n\r", ch);
            return false;
        }

        zone_id = atol(arg2);
        removed = wilds_remove_temporary_zone(pWilds, zone_id);
        printf_to_char(ch, "Removed %d overlay record(s) with zone id %ld.\n\r", removed, zone_id);
        return removed > 0;
    }

    if (!str_prefix(arg, "tile"))
    {
        int x, y;
        char base_tile;
        char effective_tile;
        int region;

        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);

        if (!is_number(arg2) || !is_number(arg3))
        {
            send_to_char("Syntax: overlay tile <x> <y>\n\r", ch);
            return false;
        }

        x = atoi(arg2);
        y = atoi(arg3);
        base_tile = get_wilds_base_tile(pWilds, x, y);
        effective_tile = get_wilds_effective_tile(pWilds, x, y);
        region = get_wilds_effective_region(pWilds, x, y);

        if (base_tile == '\0')
        {
            send_to_char("Coordinates are outside this wilderness map.\n\r", ch);
            return false;
        }

        printf_to_char(ch,
            "Tile (%d,%d): base='%c' effective='%c' region=%s\n\r",
            x,
            y,
            base_tile,
            effective_tile,
            flag_string(wilderness_regions, region));
        return false;
    }

    send_to_char("Unknown overlay command. Type 'overlay' for syntax.\n\r", ch);
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
                      "         terrain <token> wildcolor <#RRGGBB|clear>\n\r"
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
        int showname_width = wedit_terrain_showname_col_width(pWilds, 15, 48);
        bool append_ok = true;

        output = new_buf();
        if (!add_buf(output, "[{WWedit{x] Full Terrain List:\n\r\n\r")) {
            send_to_char("Terrain list output exceeded buffer limits.\n\r", ch);
            free_buf(output);
            return false;
        }
        sprintf(buf, "%-6s %-6s %-*s  %-15s  %-8s  %-9s  %s\n\r",
            "Token", "Ansi", showname_width, "Showname", "Sector", "Nonroom?", "WildColor", "Flags");
        if (!add_buf(output, buf)) {
            send_to_char("Terrain list output exceeded buffer limits.\n\r", ch);
            free_buf(output);
            return false;
        }

        for(pTerrain=pWilds->pTerrain;pTerrain;pTerrain=pTerrain->next)
        {
            sprintf(buf, " '{W%c{x'   '%s{x'   {W%-*.*s{x  {W%-15s{x  {W%s{x  %-9s  {W%s{x\n\r",
                     pTerrain->mapchar, pTerrain->showchar,
                     showname_width, showname_width,
                     pTerrain->showname ? pTerrain->showname : "(Not Set)",
                     sector_name(room_sector_type(pTerrain->template)),
                     pTerrain->nonroom ? "Yes" : "No",
                     pTerrain->wildgen_has_color ?
                        formatf("#%02X%02X%02X", pTerrain->wildgen_r, pTerrain->wildgen_g, pTerrain->wildgen_b) :
                        "(none)",
                     bitmatrix_string(room_flagbank, pTerrain->template->room_flag));
                     //flag_string(room2_flags, pTerrain->template->room_flag[1]));
            if (!add_buf(output, buf)) {
                append_ok = false;
                break;
            }
        }

        if (!append_ok) {
            send_to_char("Terrain list output exceeded buffer limits.\n\r", ch);
            free_buf(output);
            return false;
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

    if (!str_cmp(arg2, "wildcolor"))
    {
        unsigned char r, g, b;

        if (pTerrain == NULL)
        {
            send_to_char ("[Wedit] That token does not exist.\n\r", ch);
            return false;
        }

        if (argument[0] == '\0')
        {
            send_to_char ("Syntax: terrain <token> wildcolor <#RRGGBB|clear>\n\r", ch);
            return false;
        }

        if (!str_cmp(argument, "clear"))
        {
            pTerrain->wildgen_has_color = false;
            pTerrain->wildgen_r = 0;
            pTerrain->wildgen_g = 0;
            pTerrain->wildgen_b = 0;
            send_to_char("[Wedit] Terrain wildgen color cleared.\n\r", ch);
            return true;
        }

        if (!wedit_parse_rgb_hex(argument, &r, &g, &b))
        {
            send_to_char("Syntax: terrain <token> wildcolor <#RRGGBB|clear>\n\r", ch);
            return false;
        }

        pTerrain->wildgen_has_color = true;
        pTerrain->wildgen_r = r;
        pTerrain->wildgen_g = g;
        pTerrain->wildgen_b = b;
        send_to_char("[Wedit] Terrain wildgen color set.\n\r", ch);
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

WEDIT (wedit_wildgen)
{
    WILDS_DATA *pWilds;
    char arg[MIL];
    char arg2[MIL];
    char arg3[MIL];
    char arg4[MIL];
    char arg5[MIL];
    char status_buf[MSL];
    char err_buf[MSL];

    EDIT_WILDS(ch, pWilds);

    argument = one_argument(argument, arg);

    if (IS_NULLSTR(arg))
    {
        send_to_char("Syntax: wildgen check <png_filename.png>\n\r", ch);
        send_to_char("        wildgen bake\n\r", ch);
        send_to_char("        wildgen export static <png_filename.png>\n\r", ch);
        send_to_char("        wildgen export effective <png_filename.png>\n\r", ch);
        send_to_char("        wildgen export elevation <png_filename.png>\n\r", ch);
        send_to_char("Syntax: wildgen import [png_filename.png]\n\r", ch);
        send_to_char("        wildgen importgrid <terrain_base> <rows> <cols> [elevation_base]\n\r", ch);
        send_to_char("        wildgen config show\n\r", ch);
        send_to_char("        wildgen config base <terrain_base|none> [elevation_base|none]\n\r", ch);
        send_to_char("        wildgen config grid <rows> <cols>\n\r", ch);
        send_to_char("        wildgen config tilesize <tile_width> <tile_height>\n\r", ch);
        send_to_char("        wildgen config elevation <value>\n\r", ch);
        send_to_char("        (imports from data/world/wilderness_state/<uid>_<name>/images/)\n\r", ch);
        send_to_char("        (grid naming: base_row_col.png / base_row.png / base_col.png / base.png)\n\r", ch);
        send_to_char("        wildgen status\n\r", ch);
        return false;
    }

    if (!str_prefix(arg, "status"))
    {
        wilds_wildgen_status(pWilds, status_buf, sizeof(status_buf));
        printf_to_char(ch, "%s\n\r", status_buf);
        return false;
    }

    if (!str_prefix(arg, "bake"))
    {
        size_t map_len;
        size_t index;
        int baked = 0;

        if (!pWilds->staticmap || !pWilds->map || pWilds->map_size_x < 1 || pWilds->map_size_y < 1)
        {
            send_to_char("Wildgen bake failed: wilderness map buffers are not initialized.\n\r", ch);
            return false;
        }

        map_len = (size_t)pWilds->map_size_x * (size_t)pWilds->map_size_y;
        for (index = 0; index < map_len; index++)
        {
            if (pWilds->staticmap[index] != pWilds->map[index])
                baked++;
        }

        if (baked < 1)
        {
            send_to_char("Wildgen bake: no runtime tile mods to merge into staticmap.\n\r", ch);
            return false;
        }

        memcpy(pWilds->staticmap, pWilds->map, map_len);
        wilderness_mods_mark_dirty(pWilds, "wildgen bake");

        if (pWilds->pArea)
            wilderness_storage_save_area_now(pWilds->pArea);
        else
            wilderness_storage_save_all_now();

        printf_to_char(ch,
            "Wildgen bake complete: merged %d runtime tile mod%s into staticmap and saved wilderness sidecars.\n\r",
            baked,
            baked == 1 ? "" : "s");
        send_to_char("Note: temporary overlays are separate runtime records and are not baked by this command.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg, "check"))
    {
        argument = one_argument(argument, arg2);
        if (IS_NULLSTR(arg2))
        {
            send_to_char("Syntax: wildgen check <png_filename.png>\n\r", ch);
            return false;
        }

        if (!wilds_wildgen_check_image(pWilds, arg2, status_buf, sizeof(status_buf)))
        {
            printf_to_char(ch, "Wildgen check failed: %s\n\r", status_buf[0] ? status_buf : "unknown error");
            return false;
        }

        printf_to_char(ch, "%s\n\r", status_buf);
        return false;
    }

    if (!str_prefix(arg, "export"))
    {
        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);

        if (IS_NULLSTR(arg2))
        {
            send_to_char("Syntax: wildgen export static <png_filename.png>\n\r", ch);
            send_to_char("        wildgen export effective <png_filename.png>\n\r", ch);
            send_to_char("        wildgen export elevation <png_filename.png>\n\r", ch);
            return false;
        }

        if (!str_prefix(arg2, "static"))
        {
            if (IS_NULLSTR(arg3))
            {
                send_to_char("Syntax: wildgen export static <png_filename.png>\n\r", ch);
                return false;
            }

            if (!wilds_wildgen_export_image(pWilds, arg3, status_buf, sizeof(status_buf)))
            {
                printf_to_char(ch, "Wildgen export failed: %s\n\r", status_buf[0] ? status_buf : "unknown error");
                return false;
            }

            printf_to_char(ch, "%s\n\r", status_buf);
            return true;
        }

        if (!str_prefix(arg2, "effective"))
        {
            if (IS_NULLSTR(arg3))
            {
                send_to_char("Syntax: wildgen export effective <png_filename.png>\n\r", ch);
                return false;
            }

            if (!wilds_wildgen_export_effective_image(pWilds, arg3, status_buf, sizeof(status_buf)))
            {
                printf_to_char(ch, "Wildgen export failed: %s\n\r", status_buf[0] ? status_buf : "unknown error");
                return false;
            }

            printf_to_char(ch, "%s\n\r", status_buf);
            return true;
        }

        if (!str_prefix(arg2, "elevation"))
        {
            if (IS_NULLSTR(arg3))
            {
                send_to_char("Syntax: wildgen export elevation <png_filename.png>\n\r", ch);
                return false;
            }

            if (!wilds_wildgen_export_elevation(pWilds, arg3, status_buf, sizeof(status_buf)))
            {
                printf_to_char(ch, "Wildgen elevation export failed: %s\n\r", status_buf[0] ? status_buf : "unknown error");
                return false;
            }

            printf_to_char(ch, "%s\n\r", status_buf);
            return true;
        }

        send_to_char("Syntax: wildgen export static <png_filename.png>\n\r", ch);
        send_to_char("        wildgen export effective <png_filename.png>\n\r", ch);
        send_to_char("        wildgen export elevation <png_filename.png>\n\r", ch);
        return false;
    }

    if (!str_prefix(arg, "import"))
    {
        if (IS_NULLSTR(argument))
        {
            if (!IS_NULLSTR(pWilds->wildgen_terrain_base))
            {
                if (!wilds_wildgen_enqueue_grid(
                        pWilds,
                        pWilds->wildgen_terrain_base,
                        UMAX(1, pWilds->wildgen_grid_rows),
                        UMAX(1, pWilds->wildgen_grid_cols),
                        IS_NULLSTR(pWilds->wildgen_elevation_base) ? NULL : pWilds->wildgen_elevation_base,
                        err_buf,
                        sizeof(err_buf)))
                {
                    printf_to_char(ch, "Wildgen import failed: %s\n\r", err_buf[0] ? err_buf : "unknown error");
                    return false;
                }

                send_to_char("Wildgen import queued using wilderness definition. Use 'wildgen status'.\n\r", ch);
                return true;
            }

            send_to_char("Syntax: wildgen import [png_filename.png]\n\r", ch);
            send_to_char("Tip: set 'wildgen config base ...' to enable definition-based auto import.\n\r", ch);
            return false;
        }

        if (!wilds_wildgen_enqueue(pWilds, argument, err_buf, sizeof(err_buf)))
        {
            printf_to_char(ch, "Wildgen import failed: %s\n\r", err_buf[0] ? err_buf : "unknown error");
            return false;
        }

        send_to_char("Wildgen import queued on worker thread. Use 'wildgen status' to monitor progress.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg, "importgrid"))
    {
        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);
        argument = one_argument(argument, arg4);
        argument = one_argument(argument, arg5);

        if (IS_NULLSTR(arg2) || !is_number(arg3) || !is_number(arg4))
        {
            send_to_char("Syntax: wildgen importgrid <terrain_base> <rows> <cols> [elevation_base]\n\r", ch);
            return false;
        }

        if (!wilds_wildgen_enqueue_grid(pWilds, arg2, atoi(arg3), atoi(arg4), IS_NULLSTR(arg5) ? NULL : arg5, err_buf, sizeof(err_buf)))
        {
            printf_to_char(ch, "Wildgen grid import failed: %s\n\r", err_buf[0] ? err_buf : "unknown error");
            return false;
        }

        send_to_char("Wildgen grid import queued on worker thread. Use 'wildgen status' to monitor progress.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg, "config"))
    {
        argument = one_argument(argument, arg2);

        if (IS_NULLSTR(arg2) || !str_prefix(arg2, "show"))
        {
            printf_to_char(ch, "Wildgen config:\n\r");
            printf_to_char(ch, "  terrain base: %s\n\r", IS_NULLSTR(pWilds->wildgen_terrain_base) ? "(unset)" : pWilds->wildgen_terrain_base);
            printf_to_char(ch, "  elevation base: %s\n\r", IS_NULLSTR(pWilds->wildgen_elevation_base) ? "(unset)" : pWilds->wildgen_elevation_base);
            printf_to_char(ch, "  grid: %d x %d\n\r", UMAX(1, pWilds->wildgen_grid_rows), UMAX(1, pWilds->wildgen_grid_cols));
            printf_to_char(ch, "  grid tile size: %d x %d (0x0 = auto from map/grid)\n\r",
                UMAX(0, pWilds->wildgen_tile_width), UMAX(0, pWilds->wildgen_tile_height));
            printf_to_char(ch, "  default elevation: %d\n\r", pWilds->default_elevation);
            return false;
        }

        if (!str_prefix(arg2, "base"))
        {
            argument = one_argument(argument, arg3);
            argument = one_argument(argument, arg4);

            if (IS_NULLSTR(arg3))
            {
                send_to_char("Syntax: wildgen config base <terrain_base|none> [elevation_base|none]\n\r", ch);
                return false;
            }

            free_string(pWilds->wildgen_terrain_base);
            if (!str_cmp(arg3, "none"))
                pWilds->wildgen_terrain_base = str_dup("");
            else
                pWilds->wildgen_terrain_base = str_dup(arg3);

            if (!IS_NULLSTR(arg4))
            {
                free_string(pWilds->wildgen_elevation_base);
                if (!str_cmp(arg4, "none"))
                    pWilds->wildgen_elevation_base = str_dup("");
                else
                    pWilds->wildgen_elevation_base = str_dup(arg4);
            }

            send_to_char("Wildgen base configuration updated.\n\r", ch);
            return true;
        }

        if (!str_prefix(arg2, "grid"))
        {
            argument = one_argument(argument, arg3);
            argument = one_argument(argument, arg4);
            if (!is_number(arg3) || !is_number(arg4) || atoi(arg3) < 1 || atoi(arg4) < 1)
            {
                send_to_char("Syntax: wildgen config grid <rows> <cols>\n\r", ch);
                return false;
            }

            pWilds->wildgen_grid_rows = atoi(arg3);
            pWilds->wildgen_grid_cols = atoi(arg4);
            send_to_char("Wildgen grid configuration updated.\n\r", ch);
            return true;
        }

        if (!str_prefix(arg2, "tilesize"))
        {
            argument = one_argument(argument, arg3);
            argument = one_argument(argument, arg4);
            if (!is_number(arg3) || !is_number(arg4) || atoi(arg3) < 0 || atoi(arg4) < 0)
            {
                send_to_char("Syntax: wildgen config tilesize <tile_width> <tile_height>\n\r", ch);
                send_to_char("Use 0 0 to auto-compute from map size and grid rows/cols.\n\r", ch);
                return false;
            }

            pWilds->wildgen_tile_width = atoi(arg3);
            pWilds->wildgen_tile_height = atoi(arg4);
            send_to_char("Wildgen tile-size configuration updated.\n\r", ch);
            return true;
        }

        if (!str_prefix(arg2, "elevation"))
        {
            argument = one_argument(argument, arg3);
            if (IS_NULLSTR(arg3) || !is_number(arg3))
            {
                send_to_char("Syntax: wildgen config elevation <value>\n\r", ch);
                send_to_char("Sets the default (sea level) elevation for this wilderness.\n\r", ch);
                return false;
            }

            pWilds->default_elevation = atoi(arg3);
            printf_to_char(ch, "Default elevation set to %d.\n\r", pWilds->default_elevation);
            return true;
        }

        send_to_char("Syntax: wildgen config show\n\r", ch);
        send_to_char("        wildgen config base <terrain_base|none> [elevation_base|none]\n\r", ch);
        send_to_char("        wildgen config grid <rows> <cols>\n\r", ch);
        send_to_char("        wildgen config tilesize <tile_width> <tile_height>\n\r", ch);
        send_to_char("        wildgen config elevation <value>\n\r", ch);
        return false;
    }

    send_to_char("Syntax: wildgen check <png_filename.png>\n\r", ch);
    send_to_char("        wildgen bake\n\r", ch);
    send_to_char("        wildgen export static <png_filename.png>\n\r", ch);
    send_to_char("        wildgen export effective <png_filename.png>\n\r", ch);
    send_to_char("        wildgen export elevation <png_filename.png>\n\r", ch);
    send_to_char("        wildgen import [png_filename.png]\n\r", ch);
    send_to_char("        wildgen importgrid <terrain_base> <rows> <cols> [elevation_base]\n\r", ch);
    send_to_char("        wildgen config show\n\r", ch);
    send_to_char("        wildgen status\n\r", ch);
    return false;
}

WEDIT ( wedit_vlink )
{
    WILDS_DATA *pWilds;
    WILDS_VLINK *pVLink;
    WNUM room_wnum;
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
    send_to_char("       [wedit] vlink destination <vlinknum> dungeon <wnum|$name> [floor]\n\r", ch);
    send_to_char("               - Sets the destination of the unlinked vlink to a dungeon.\n\r", ch);
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
            int new_linkage = VLINK_UNLINKED;

            if(!str_prefix(arg3,"to_wilds")) new_linkage = VLINK_TO_WILDS;
            else if(!str_prefix(arg3,"from_wilds")) new_linkage = VLINK_FROM_WILDS;
            else if(!str_prefix(arg3,"two_way")) new_linkage = VLINK_TO_WILDS|VLINK_FROM_WILDS;
            else {
                printf_to_char(ch, "Wedit vlink: Invalid linkage.  Valid values are {Wto_wilds{x, {Wfrom_wilds{x and {Wtwo_way{x.\n\r", vlnum);
                return false;
            }

            if (pVLink->destination_mode == VLINK_DEST_DUNGEON && IS_SET(new_linkage, VLINK_TO_WILDS)) {
                send_to_char("Wedit vlink: Dungeon destinations only support {Wfrom_wilds{x linkage.\n\r", ch);
                return false;
            }

            pVLink->default_linkage = new_linkage;
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
            AREA_DATA *context = ch->in_room->area;
            if (!str_cmp(arg3, "dungeon")) {
                DUNGEON_INDEX_DATA *dest_dungeon = NULL;
                WNUM dng_wnum = { NULL, 0 };
                int floor = 1;

                if (!arg4[0]) {
                    send_to_char("Wedit vlink: Syntax is vlink destination <vlinknum> dungeon <wnum|$name> [floor]\n\r", ch);
                    return false;
                }

                if (argument[0]) {
                    if (!is_number(argument)) {
                        send_to_char("Wedit vlink: Floor must be a number.\n\r", ch);
                        return false;
                    }
                    floor = UMAX(1, atoi(argument));
                }

                if (parse_widevnum(arg4, context, &dng_wnum) && dng_wnum.vnum > 0)
                    dest_dungeon = get_dungeon_index_for_area(dng_wnum.pArea, dng_wnum.vnum);

                if (!dest_dungeon) {
                    send_to_char("Wedit vlink: Invalid dungeon destination.\n\r", ch);
                    return false;
                }

                if (IS_SET(pVLink->default_linkage, VLINK_TO_WILDS)) {
                    send_to_char("Wedit vlink: Dungeon destinations require {Wfrom_wilds{x linkage.\n\r", ch);
                    return false;
                }

                pVLink->destination_mode = VLINK_DEST_DUNGEON;
                pVLink->dungeon_floor = floor;
                pVLink->destvnum = dest_dungeon->vnum;
                pVLink->dest_load.auid = dest_dungeon->area ? dest_dungeon->area->uid : 0;
                pVLink->dest_load.vnum = dest_dungeon->vnum;
                pVLink->dest_wnum.pArea = dest_dungeon->area;
                pVLink->dest_wnum.vnum = dest_dungeon->vnum;

                send_to_char("Wedit vlink: Dungeon destination set.\n\r", ch);
                return true;
            } else {
                WNUM room_wnum;
                ROOM_INDEX_DATA *destRoom;

                if (!(parse_widevnum(arg3, context, &room_wnum) && room_wnum.vnum > 0)) {
                    send_to_char("Wedit vlink: Invalid destination", ch);
                    return false;
                }

                destRoom = get_room_index(room_wnum.pArea, room_wnum.vnum);

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

                pVLink->destination_mode = VLINK_DEST_ROOM;
                pVLink->dungeon_floor = 1;
                pVLink->destvnum = room_wnum.vnum;
                pVLink->dest_load.auid = room_wnum.pArea ? room_wnum.pArea->uid : 0;
                pVLink->dest_load.vnum = room_wnum.vnum;
                pVLink->dest_wnum = room_wnum;
                send_to_char("Wedit vlink: Destination set.\n\r", ch);
                return true;
            }
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
                     "[dest] [default] [current] [maptile]\n\r", ch);

        for(vlnum = 0,pVLink=pWilds?pWilds->pVLink:ch->in_room->area->wilds->pVLink;pVLink!=NULL;pVLink = pVLink->next)
        {
            char dest_buf[MSL];
            if (pVLink->destination_mode == VLINK_DEST_DUNGEON) {
                snprintf(dest_buf, sizeof(dest_buf), "dng %ld#%ld f%d",
                    pVLink->dest_load.auid, pVLink->dest_load.vnum, UMAX(1, pVLink->dungeon_floor));
            } else if (resolve_widevnum(pVLink->destvnum, NULL, &room_wnum) && room_wnum.pArea) {
                snprintf(dest_buf, sizeof(dest_buf), "%ld#%ld",
                    room_wnum.pArea->uid, room_wnum.vnum);
            } else if (pVLink->dest_load.auid > 0 && pVLink->dest_load.vnum > 0) {
                snprintf(dest_buf, sizeof(dest_buf), "%ld#%ld",
                    pVLink->dest_load.auid, pVLink->dest_load.vnum);
            } else {
                snprintf(dest_buf, sizeof(dest_buf), "%ld", pVLink->destvnum);
            }

            printf_to_char(ch, "%-5d ({W%6ld{x)  {W%6d   %6d   %-9s   %-14s %10s%10s%s{x\n\r",
                       vlnum++,
                           pVLink->uid,
                           pVLink->wildsorigin_x,
                           pVLink->wildsorigin_y,
                           dir_name[pVLink->door],
                           dest_buf,
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