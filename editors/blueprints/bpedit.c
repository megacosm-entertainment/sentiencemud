/***************************************************************************
 *  bpedit.c - OLC Blueprint Editor                                        *
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
#include "channel_registry.h"
#include "../../wilds.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

extern void list_blueprints(CHAR_DATA *ch, char *argument);
extern bool can_edit_blueprints(CHAR_DATA *ch);
extern long top_blueprint_vnum;

/***************************************************************************
 * Forward Declarations — Tab Show Functions                               *
 ***************************************************************************/

static void bpedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void bpedit_show_sections_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void bpedit_show_layout_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void bpedit_show_programs_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void bpedit_show_variables_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void bpedit_show_notes_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);

/***************************************************************************
 * Framework Helpers                                                       *
 ***************************************************************************/

static AREA_DATA *bpedit_get_area(void *pEdit)
{
    return pEdit ? ((BLUEPRINT *)pEdit)->area : NULL;
}

static bool bpedit_perm_check(CHAR_DATA *ch, void *pEdit)
{
    return can_edit_blueprints(ch);
}

static void bpedit_mark_changed(CHAR_DATA *ch, void *pEdit)
{
    BLUEPRINT *bp = (BLUEPRINT *)pEdit;
    if (bp && bp->area)
        SET_BIT(bp->area->area_flags, AREA_CHANGED);
}

static bool bpedit_add_or_fail(CHAR_DATA *ch, BUFFER *buffer, const char *text, const char *message)
{
    if (add_buf(buffer, text))
        return true;

    send_to_char(message, ch);
    return false;
}

/***************************************************************************
 * Blueprint Editor Command Table (moved from blueprint.c)                 *
 ***************************************************************************/

const struct olc_cmd_type bpedit_table[] = {
    { "?",              show_help           },
    { "addiprog",       bpedit_addiprog     },
    { "areawho",        bpedit_areawho      },
    { "commands",       show_commands       },
    { "comments",       bpedit_comments     },
    { "channel",        bpedit_channel      },
    { "create",         bpedit_create       },
    { "deliprog",       bpedit_deliprog     },
    { "description",    bpedit_description  },
    { "flags",          bpedit_flags        },
    { "list",           bpedit_list         },
    { "mode",           bpedit_mode         },
    { "name",           bpedit_name         },
    { "repop",          bpedit_repop        },
    { "section",        bpedit_section      },
    { "show",           bpedit_show         },
    { "static",         bpedit_static       },
    { "varclear",       bpedit_varclear     },
    { "varset",         bpedit_varset       },
    { NULL,             NULL                }
};

/***************************************************************************
 * Editor Definition (Unified Framework)                                   *
 ***************************************************************************/

static const OLC_EDITOR_DEF bpedit_def = {
    .name           = "BPEdit",
    .editor_type    = ED_BLUEPRINT,
    .cmd_table      = bpedit_table,
    .show_fn        = bpedit_show,
    .tabs           = {
        .count      = 6,
        .tabs       = {
            { "General",   "Gen",  bpedit_show_general_tab   },
            { "Sections",  "Sec",  bpedit_show_sections_tab  },
            { "Layout",    "Lay",  bpedit_show_layout_tab    },
            { "Programs",  "Prg",  bpedit_show_programs_tab  },
            { "Variables", "Var",  bpedit_show_variables_tab },
            { "Notes",     "Nts",  bpedit_show_notes_tab     },
        },
    },
    .theme          = &olc_theme_building,
    .perm           = {
        .flags      = OLC_PERM_CUSTOM,
        .check_fn   = bpedit_perm_check,
    },
    .change_mode    = OLC_CHANGE_CUSTOM,
    .mark_changed_fn = bpedit_mark_changed,
    .get_area_fn    = bpedit_get_area,
    .audit_changes  = true,
};

/***************************************************************************
 * Blueprint Editor Interpreter — delegates to framework.                  *
 ***************************************************************************/

void bpedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &bpedit_def);
}

/***************************************************************************
 * Blueprint Editor Entry Point (moved from blueprint.c)                   *
 ***************************************************************************/

/**
 * do_bpedit - Staff command to edit or create blueprints
 *
 * Syntax: bpedit <vnum> - Edit existing blueprint
 *         bpedit create <vnum> - Create new blueprint
 *
 * @param ch        Staff character
 * @param argument  Blueprint vnum or "create <vnum>"
 */
void do_bpedit(CHAR_DATA *ch, char *argument)
{
    BLUEPRINT *bp;
    char arg1[MAX_STRING_LENGTH];
    WNUM wnum;

    argument = one_argument(argument, arg1);

    if (IS_NPC(ch))
        return;

    if (!can_edit_blueprints(ch))
    {
        send_to_char("BPEdit:  Insufficient security to edit blueprints.\n\r", ch);
        return;
    }

    if (parse_widevnum(arg1, ch->in_room->area, &wnum))
    {
        if (!(bp = get_blueprint_for_area(wnum.pArea, wnum.vnum)))
        {
            send_to_char("BPEdit:  That blueprint does not exist.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &bpedit_def, (void *)bp, true);
        return;
    }
    else
    {
        if (!str_cmp(arg1, "create"))
        {
            if (bpedit_create(ch, argument))
            {
                olc_editor_enter(ch, &bpedit_def, ch->desc->pEdit, true);
            }

            return;
        }
    }

    send_to_char("Syntax: bpedit <#vnum|area_uid#vnum>\n\r"
                 "        bpedit create <vnum>\n\r", ch);
}

/***************************************************************************
 * Commands                                                                *
 ***************************************************************************/


BPEDIT( bpedit_list )
{
    list_blueprints(ch, argument);
    return false;
}

/***************************************************************************
 * Master Show — dispatches to active tab                                  *
 ***************************************************************************/

BPEDIT( bpedit_show )
{
    BLUEPRINT *bp;
    EDIT_BLUEPRINT(ch, bp);

    const OLC_EDITOR_THEME *theme = bpedit_def.theme;
    OLC_LAYOUT_CTX *ctx = olc_display_new(ch, theme);

    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "%s", widevnum_string_blueprint(bp, bp->area));
    olc_display_header(ctx, "BPEdit", bp->name, id_buf, &bpedit_def);

    /* Dispatch to active tab's show function */
    int tab = olc_show_all_tabs_mode(ch) ? -1 : (ch->desc ? ch->desc->nEditTab : 0);
    if (tab < 0) {
        for (int i = 0; i < bpedit_def.tabs.count; i++) {
            if (bpedit_def.tabs.tabs[i].show_fn)
                bpedit_def.tabs.tabs[i].show_fn(ch, ctx, (void *)bp);
        }
    } else if (tab >= 0 && tab < bpedit_def.tabs.count
        && bpedit_def.tabs.tabs[tab].show_fn) {
        bpedit_def.tabs.tabs[tab].show_fn(ch, ctx, (void *)bp);
    } else {
        bpedit_show_general_tab(ch, ctx, (void *)bp);
    }

    olc_display_footer(ctx, theme);
    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

/***************************************************************************
 * Tab 1: General                                                          *
 ***************************************************************************/

static void bpedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    BLUEPRINT *bp = (BLUEPRINT *)pEdit;
    const OLC_EDITOR_THEME *theme = bpedit_def.theme;
    char buf[MSL];

    sprintf(buf, "[%s] %s", widevnum_string_blueprint(bp, bp->area), bp->name);
    olc_display_string(ctx, theme, "Name:", "name", buf);

    if (bp->repop > 0)
        sprintf(buf, "%d minutes", bp->repop);
    else
        sprintf(buf, "{Dnever{x");
    olc_display_string(ctx, theme, "Repop:", "repop", buf);

    sprintf(buf, "[%s] [%s]", flag_string(area_who_titles, bp->area_who), flag_string(area_who_display, bp->area_who));
    olc_display_string(ctx, theme, "AreaWho:", "areawho", buf);

    olc_display_flags(ctx, theme, "Flags:", "flags", instance_flags, bp->flags);

    switch (bp->mode)
    {
    case BLUEPRINT_MODE_STATIC:
        olc_display_string(ctx, theme, "Mode:", NULL, "{WStatic{x");
        break;
    default:
        olc_display_string(ctx, theme, "Mode:", NULL, "Unknown");
        break;
    }

    olc_display_section(ctx, theme, "Description");
    if (!bpedit_add_or_fail(ch, ctx->buffer, bp->description ? bp->description : "(none)\n\r",
            "Blueprint description output exceeded buffer limits.\n\r"))
        return;
}

/***************************************************************************
 * Tab 2: Sections                                                         *
 ***************************************************************************/

static void bpedit_show_sections_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    BLUEPRINT *bp = (BLUEPRINT *)pEdit;
    const OLC_EDITOR_THEME *theme = bpedit_def.theme;
    char buf[MSL];

    if (list_size(bp->sections) > 0)
    {
        int line = 0;
        BLUEPRINT_SECTION_REF *section_ref;
        ITERATOR sit;

        olc_display_section(ctx, theme, "Sections");
        if (!bpedit_add_or_fail(ch, ctx->buffer, "     [  Vnum  ] [             Name             ]\n\r",
                "Blueprint sections output exceeded buffer limits.\n\r"))
            return;
        if (!bpedit_add_or_fail(ch, ctx->buffer, "------------------------------------------------\n\r",
                "Blueprint sections output exceeded buffer limits.\n\r"))
            return;

        iterator_start(&sit, bp->sections);
        while ((section_ref = (BLUEPRINT_SECTION_REF *)iterator_nextdata(&sit)))
        {
            if (section_ref->section) {
                sprintf(buf, "{W%4d  {G%8s{x   %-30.30s{x\n\r", ++line, widevnum_string_blueprint_section(section_ref->section, bp->area),
                    section_ref->section->name ? section_ref->section->name : "(unnamed)");
            } else {
                sprintf(buf, "{W%4d  {G%8ld#%ld{x   {R(section not found){x\n\r", ++line,
                    section_ref->section_ref.load.auid, section_ref->section_ref.load.vnum);
            }
            if (!bpedit_add_or_fail(ch, ctx->buffer, buf,
                    "Blueprint sections output exceeded buffer limits.\n\r"))
                return;
        }
        iterator_stop(&sit);
        if (!bpedit_add_or_fail(ch, ctx->buffer, "------------------------------------------------\n\r",
                "Blueprint sections output exceeded buffer limits.\n\r"))
            return;
    }
    else
    {
        olc_display_string(ctx, theme, "Sections:", NULL, "None");
    }
}

/***************************************************************************
 * Tab 3: Layout (Static mode: links, recall, entries, exits, specials)    *
 ***************************************************************************/

static void bpedit_show_layout_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    BLUEPRINT *bp = (BLUEPRINT *)pEdit;
    const OLC_EDITOR_THEME *theme = bpedit_def.theme;
    char buf[MSL];

    /* Special Rooms (shown for all modes) */
    olc_display_section(ctx, theme, "Special Rooms");
    if (list_size(bp->special_rooms) > 0)
    {
        BLUEPRINT_SPECIAL_ROOM *special;
        ITERATOR sit;
        int line = 0;

        if (!bpedit_add_or_fail(ch, ctx->buffer, "     [             Name             ] [             Room             ]\n\r",
                "Blueprint layout output exceeded buffer limits.\n\r"))
            return;
        if (!bpedit_add_or_fail(ch, ctx->buffer, "---------------------------------------------------------------------------------\n\r",
                "Blueprint layout output exceeded buffer limits.\n\r"))
            return;

        iterator_start(&sit, bp->special_rooms);
        while ((special = (BLUEPRINT_SPECIAL_ROOM *)iterator_nextdata(&sit)))
        {
            BLUEPRINT_SECTION_REF *section_ref = list_nthdata(bp->sections, special->section);
            BLUEPRINT_SECTION *section = section_ref ? section_ref->section : NULL;
            AREA_DATA *area = section ? (section->rooms_area ? section->rooms_area : section->area) : NULL;
            if (!area) area = get_system_area_fallback();
            ROOM_INDEX_DATA *room = get_room_index(area, special->room ? special->room->vnum : special->room_ref.load.vnum);

            if (!IS_VALID(section) || !room || room->vnum < section->lower_vnum || room->vnum > section->upper_vnum)
            {
                snprintf(buf, MSL-1, "{W%4d  %-30.30s   {D-{Winvalid{D-{x\n\r", ++line, special->name);
            }
            else
            {
                snprintf(buf, MSL-1, "{W%4d  %-30.30s   (%s) {Y%s{x in (%s) {Y%s{x\n\r", ++line, special->name, widevnum_string_room(room, bp->area), room->name, widevnum_string_blueprint_section(section, bp->area), section->name);
            }
            if (!bpedit_add_or_fail(ch, ctx->buffer, buf,
                    "Blueprint layout output exceeded buffer limits.\n\r"))
                return;
        }
        iterator_stop(&sit);
        if (!bpedit_add_or_fail(ch, ctx->buffer, "---------------------------------------------------------------------------------\n\r",
                "Blueprint layout output exceeded buffer limits.\n\r"))
            return;
    }
    else
    {
        if (!bpedit_add_or_fail(ch, ctx->buffer, "   None\n\r",
                "Blueprint layout output exceeded buffer limits.\n\r"))
            return;
    }
    if (!bpedit_add_or_fail(ch, ctx->buffer, "\n\r",
            "Blueprint layout output exceeded buffer limits.\n\r"))
        return;

    /* Static mode layout details */
    if (bp->mode != BLUEPRINT_MODE_STATIC)
    {
        olc_display_infof(ctx, theme, "Blueprint mode is not Static. Layout commands require Static mode.");
        return;
    }

    /* Links */
    if (bp->_static.layout)
    {
        int linkno = 0;
        olc_display_section(ctx, theme, "Links");
        if (!bpedit_add_or_fail(ch, ctx->buffer, "     [ Section 1 ] [ Link 1 ] [ Section 2 ] [ Link 2 ]\n\r",
                "Blueprint layout output exceeded buffer limits.\n\r"))
            return;
        if (!bpedit_add_or_fail(ch, ctx->buffer, "-------------------------------------------------------\n\r",
                "Blueprint layout output exceeded buffer limits.\n\r"))
            return;

        STATIC_BLUEPRINT_LINK *sbl;
        for (sbl = bp->_static.layout; sbl; sbl = sbl->next)
        {
            sprintf(buf, "{W%4d   {G%9d     {Y%6d     {G%9d     {Y%6d{x\n\r",
                ++linkno, sbl->section1, sbl->link1, sbl->section2, sbl->link2);
            if (!bpedit_add_or_fail(ch, ctx->buffer, buf,
                    "Blueprint layout output exceeded buffer limits.\n\r"))
                return;
        }
        if (!bpedit_add_or_fail(ch, ctx->buffer, "-------------------------------------------------------\n\r\n\r",
                "Blueprint layout output exceeded buffer limits.\n\r"))
            return;
    }
    else
    {
        olc_display_string(ctx, theme, "Links:", NULL, "None");
    }

    /* Recall */
    if (bp->_static.recall > 0)
    {
        BLUEPRINT_SECTION_REF *bs_ref = (BLUEPRINT_SECTION_REF *)list_nthdata(bp->sections, bp->_static.recall);
        BLUEPRINT_SECTION *bs = bs_ref ? bs_ref->section : NULL;

        if (bs)
            sprintf(buf, "%d [%s] %-.30s", bp->_static.recall, widevnum_string_blueprint_section(bs, bp->area), bs->name);
        else
            sprintf(buf, "%d [---] {Dinvalid{x", bp->_static.recall);
        olc_display_string(ctx, theme, "Recall:", "recall", buf);
    }
    else
    {
        olc_display_string(ctx, theme, "Recall:", "recall", "None");
    }

    /* Entries */
    {
        BLUEPRINT_EXIT_DATA *bex;
        ITERATOR bxit;
        int bxindex = 1;
        if (list_size(bp->_static.entries) > 0)
        {
            iterator_start(&bxit, bp->_static.entries);
            while ((bex = (BLUEPRINT_EXIT_DATA *)iterator_nextdata(&bxit)))
            {
                BLUEPRINT_SECTION_REF *bs_ref = (BLUEPRINT_SECTION_REF *)list_nthdata(bp->sections, bex->section);
                BLUEPRINT_SECTION *bs = bs_ref ? bs_ref->section : NULL;

                if (bs)
                {
                    BLUEPRINT_LINK *bl = get_section_link(bs, bex->link);
                    char section_name[31];
                    strncpy(section_name, bs->name, 30);
                    section_name[30] = '\0';

                    if (bl && (bl->room || (bl->room_ref.load.vnum > 0 && bl->door >= 0 && bl->door < MAX_DIR)))
                    {
                        const char *room_str = bl->room ? widevnum_string_room(bl->room, bp->area) : NULL;
                        if (room_str)
                            sprintf(buf, "[%d] %d [%s] %s (%s:%s)", bxindex++, bex->section, widevnum_string_blueprint_section(bs, bp->area), section_name, room_str, dir_name[bl->door]);
                        else
                            sprintf(buf, "[%d] %d [%s] %s (%ld#%ld:%s)", bxindex++, bex->section, widevnum_string_blueprint_section(bs, bp->area), section_name, bl->room_ref.load.auid, bl->room_ref.load.vnum, dir_name[bl->door]);
                    }
                    else
                    {
                        sprintf(buf, "[%d] %d [%s] %s ({Dinvalid{x)", bxindex++, bex->section, widevnum_string_blueprint_section(bs, bp->area), section_name);
                    }
                }
                else
                {
                    sprintf(buf, "[%d] %d [---] {Dinvalid{x", bxindex++, bex->section);
                }
                olc_display_string(ctx, theme, "Entry:", "entry", buf);
            }
            iterator_stop(&bxit);
        }
        else
        {
            olc_display_string(ctx, theme, "Entry:", "entry", "None");
        }
    }

    /* Exits */
    {
        BLUEPRINT_EXIT_DATA *bex;
        ITERATOR bxit;
        int bxindex = 1;
        if (list_size(bp->_static.exits) > 0)
        {
            iterator_start(&bxit, bp->_static.exits);
            while ((bex = (BLUEPRINT_EXIT_DATA *)iterator_nextdata(&bxit)))
            {
                BLUEPRINT_SECTION_REF *bs_ref = (BLUEPRINT_SECTION_REF *)list_nthdata(bp->sections, bex->section);
                BLUEPRINT_SECTION *bs = bs_ref ? bs_ref->section : NULL;

                if (bs)
                {
                    BLUEPRINT_LINK *bl = get_section_link(bs, bex->link);
                    char section_name[31];
                    strncpy(section_name, bs->name, 30);
                    section_name[30] = '\0';

                    if (bl && (bl->room || (bl->room_ref.load.vnum > 0 && bl->door >= 0 && bl->door < MAX_DIR)))
                    {
                        const char *room_str = bl->room ? widevnum_string_room(bl->room, bp->area) : NULL;
                        if (room_str)
                            sprintf(buf, "[%d] %d [%s] %s (%s:%s)", bxindex++, bex->section, widevnum_string_blueprint_section(bs, bp->area), section_name, room_str, dir_name[bl->door]);
                        else
                            sprintf(buf, "[%d] %d [%s] %s (%ld#%ld:%s)", bxindex++, bex->section, widevnum_string_blueprint_section(bs, bp->area), section_name, bl->room_ref.load.auid, bl->room_ref.load.vnum, dir_name[bl->door]);
                    }
                    else
                    {
                        sprintf(buf, "[%d] %d [%s] %s ({Dinvalid{x)", bxindex++, bex->section, widevnum_string_blueprint_section(bs, bp->area), section_name);
                    }
                }
                else
                {
                    sprintf(buf, "[%d] %d [---] {Dinvalid{x", bxindex++, bex->section);
                }
                olc_display_string(ctx, theme, "Exit:", "exit", buf);
            }
            iterator_stop(&bxit);
        }
        else
        {
            olc_display_string(ctx, theme, "Exit:", "exit", "None");
        }
    }
}

/***************************************************************************
 * Tab 4: Programs                                                         *
 ***************************************************************************/

static void bpedit_show_programs_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    BLUEPRINT *bp = (BLUEPRINT *)pEdit;
    const OLC_EDITOR_THEME *theme = bpedit_def.theme;

    if (bp->progs)
        olc_show_progs_grouped(ctx->buffer, bp->progs, PRG_IPROG, "Blueprint Programs");
    else
        olc_display_string(ctx, theme, "Programs:", NULL, "None");
}

/***************************************************************************
 * Tab 5: Variables                                                        *
 ***************************************************************************/

static void bpedit_show_variables_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    BLUEPRINT *bp = (BLUEPRINT *)pEdit;
    const OLC_EDITOR_THEME *theme = bpedit_def.theme;
    char buf[MSL];

    if (bp->index_vars)
    {
        pVARIABLE var;
        int cnt;

        for (cnt = 0, var = bp->index_vars; var; var = var->next) ++cnt;

        if (cnt > 0)
        {
            olc_display_section(ctx, theme, "Index Variables");

            sprintf(buf, "{R%-20s %-8s %-5s %-10s\n\r{x", "Name", "Type", "Saved", "Value");
            if (!bpedit_add_or_fail(ch, ctx->buffer, buf,
                    "Blueprint variable output exceeded buffer limits.\n\r"))
                return;
            sprintf(buf, "{R%-20s %-8s %-5s %-10s\n\r{x", "----", "----", "-----", "-----");
            if (!bpedit_add_or_fail(ch, ctx->buffer, buf,
                    "Blueprint variable output exceeded buffer limits.\n\r"))
                return;

            for (var = bp->index_vars; var; var = var->next)
            {
                switch (var->type) {
                case VAR_INTEGER:
                    sprintf(buf, "{x%-20.20s {GNUMBER     {Y%c   {W%d{x\n\r", var->name, var->save?'Y':'N', var->_.i);
                    break;
                case VAR_STRING:
                case VAR_STRING_S:
                    sprintf(buf, "{x%-20.20s {GSTRING     {Y%c   {W%s{x\n\r", var->name, var->save?'Y':'N', var->_.s?var->_.s:"(empty)");
                    break;
                case VAR_ROOM:
                    if (var->_.r && var->_.r->vnum > 0)
                        sprintf(buf, "{x%-20.20s {GROOM       {Y%c   {W%s {R({W%s{R){x\n\r", var->name, var->save?'Y':'N', var->_.r->name, widevnum_string_room(var->_.r, bp->area));
                    else
                        sprintf(buf, "{x%-20.20s {GROOM       {Y%c   {W-no-where-{x\n\r", var->name, var->save?'Y':'N');
                    break;
                default:
                    continue;
                }
                if (!bpedit_add_or_fail(ch, ctx->buffer, buf,
                        "Blueprint variable output exceeded buffer limits.\n\r"))
                    return;
            }
        }
        else
        {
            olc_display_string(ctx, theme, "Variables:", NULL, "None");
        }
    }
    else
    {
        olc_display_string(ctx, theme, "Variables:", NULL, "None");
    }
}

/***************************************************************************
 * Tab 6: Notes (Builder Comments)                                         *
 ***************************************************************************/

static void bpedit_show_notes_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    BLUEPRINT *bp = (BLUEPRINT *)pEdit;
    const OLC_EDITOR_THEME *theme = bpedit_def.theme;

    olc_display_section(ctx, theme, "Builders' Comments");
    if (!bpedit_add_or_fail(ch, ctx->buffer, bp->comments ? bp->comments : "(none)\n\r",
            "Blueprint comments output exceeded buffer limits.\n\r"))
        return;
}

BPEDIT( bpedit_create )
{
    BLUEPRINT *bp;
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
            if (!get_blueprint_for_area(pArea, try_vnum))
            {
                value = try_vnum;
                break;
            }
        }

        if (value == 0)
        {
            send_to_char("BPEdit: Could not find an available vnum in this area.\n\r", ch);
            return false;
        }
    }
    else
    {
        WNUM bp_wnum;
        if (!parse_widevnum(argument, ch->in_room->area, &bp_wnum)) {
            send_to_char("BPEdit: Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        value = bp_wnum.vnum;
        pArea = bp_wnum.pArea;
    }

    if (!pArea)
    {
        send_to_char("BPEdit: That vnum is not assigned an area.\n\r", ch);
        return false;
    }

    if (!IS_BUILDER(ch, pArea))
    {
        send_to_char("BPEdit: Vnum in an area you cannot build in.\n\r", ch);
        return false;
    }

    if (get_blueprint_for_area(pArea, value))
    {
        send_to_char("BPEdit: That vnum already exists.\n\r", ch);
        return false;
    }

    bp = new_blueprint();
    bp->vnum = value;
    bp->area = pArea;

    iHash                           = bp->vnum % MAX_KEY_HASH;
    bp->next                        = pArea->blueprint_hash[iHash];
    pArea->blueprint_hash[iHash]    = bp;
    ch->desc->pEdit                 = (void *)bp;

    if (bp->vnum > top_blueprint_vnum)
        top_blueprint_vnum = bp->vnum;

    send_to_char("Blueprint Created.\n\r", ch);
    SET_BIT(pArea->area_flags, AREA_CHANGED);
    return true;

}


BPEDIT( bpedit_name )
{
    BLUEPRINT *bp;
    EDIT_BLUEPRINT(ch, bp);

    return olc_cmd_string(ch, argument, "Name", NULL, &bp->name,
        OLC_STR_DEFAULT | OLC_STR_UTF8_RESTRICT, NULL, NULL);
}

BPEDIT( bpedit_repop )
{
    BLUEPRINT *bp;
    EDIT_BLUEPRINT(ch, bp);

    return olc_cmd_number(ch, argument, "Repop", NULL, &bp->repop,
        0, INT_MAX, NULL, NULL);
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

    return olc_cmd_string_append(ch, argument, "Description", NULL,
        &bp->description, NULL, NULL);
}

BPEDIT( bpedit_comments )
{
    BLUEPRINT *bp;
    EDIT_BLUEPRINT(ch, bp);

    return olc_cmd_string_append(ch, argument, "Comments", NULL,
        &bp->comments, NULL, NULL);
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
        bp->_static.layout = NULL;
        bp->_static.recall = -1;
        list_clear(bp->_static.entries);
        list_clear(bp->_static.exits);

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
        send_to_char("Syntax:  section add <vnum|#vnum|area#vnum>\n\r", ch);
        send_to_char("         section delete <#>\n\r", ch);
        send_to_char("         section list\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if( !str_prefix(arg, "add") )
    {
        WNUM wnum;
        if (!parse_widevnum(argument, ch->in_room->area, &wnum) || !wnum.pArea) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        bs = get_blueprint_section_for_area(wnum.pArea, wnum.vnum);
        if( !bs )
        {
            send_to_char("That blueprint section does not exist.\n\r", ch);
            return false;
        }

        BLUEPRINT_SECTION_REF *ref = alloc_perm(sizeof(BLUEPRINT_SECTION_REF));
        ref->section_ref.load.auid = wnum.pArea->uid;
        ref->section_ref.load.vnum = wnum.vnum;
        ref->section = bs;

        if( !list_appendlink(bp->sections, ref) )
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
            for(cur = bp->_static.layout; cur; cur = next)
            {
                next = cur->next;

                // Link references deleted section
                if( cur->section1 == index || cur->section2 == index )
                {
                    if( !prev )
                        bp->_static.layout = next;
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
            if( bp->_static.recall == index )
                bp->_static.recall = -1;
            else if( bp->_static.recall > index )
                bp->_static.recall--;

            ITERATOR bxit;
            BLUEPRINT_EXIT_DATA *bex;

            iterator_start(&bxit, bp->_static.entries);
            while( (bex = (BLUEPRINT_EXIT_DATA *)iterator_nextdata(&bxit)) )
            {
                if (bex->section == index)
                {
                    iterator_remcurrent(&bxit);
                }
                else if (bex->section > index)
                {
                    bex->section--;
                }
            }
            iterator_stop(&bxit);

            iterator_start(&bxit, bp->_static.exits);
            while( (bex = (BLUEPRINT_EXIT_DATA *)iterator_nextdata(&bxit)) )
            {
                if (bex->section == index)
                {
                    iterator_remcurrent(&bxit);
                }
                else if (bex->section > index)
                {
                    bex->section--;
                }
            }
            iterator_stop(&bxit);
        }

        return true;
    }

    if( !str_prefix(arg, "list") )
    {
        if( list_size(bp->sections) > 0 )
        {
            BUFFER *buffer = new_buf();
            bool append_ok = true;

            char buf[MSL];
            int line = 0;

            ITERATOR sit;

            append_ok = add_buf(buffer, "     [  Vnum  ] [             Name             ]\n\r");
            append_ok = append_ok && add_buf(buffer, "------------------------------------------------\n\r");

            if (!append_ok)
            {
                send_to_char("Section list output exceeded buffer limits.\n\r", ch);
                free_buf(buffer);
                return false;
            }

            BLUEPRINT_SECTION_REF *section_ref;

            iterator_start(&sit, bp->sections);
            while( (section_ref = (BLUEPRINT_SECTION_REF *)iterator_nextdata(&sit)) )
            {
                if (section_ref->section) {
                    sprintf(buf, "{W%4d  {G%8ld{x   %-30.30s{x\n\r", ++line, 
                        section_ref->section->vnum, 
                        section_ref->section->name ? section_ref->section->name : "(unnamed)");
                } else {
                    sprintf(buf, "{W%4d  {G%8ld#%ld{x   {R(section not found){x\n\r", ++line,
                        section_ref->section_ref.load.auid, section_ref->section_ref.load.vnum);
                }
                if (!add_buf(buffer, buf))
                {
                    append_ok = false;
                    break;
                }
            }

            iterator_stop(&sit);

            if (append_ok)
                append_ok = add_buf(buffer, "------------------------------------------------\n\r");

            if (!append_ok)
            {
                send_to_char("Section list output exceeded buffer limits.\n\r", ch);
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
            send_to_char("Blueprint has no blueprint sections assigned.\n\r", ch);
        }

        return false;
    }

    bpedit_section(ch, "");
    return false;
}

BPEDIT( bpedit_channel )
{
    BLUEPRINT *bp;
    char arg[MIL];
    char channel_id[MIL];

    EDIT_BLUEPRINT(ch, bp);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  channel add <channel_id>\n\r", ch);
        send_to_char("         channel del <#|channel_id>\n\r", ch);
        send_to_char("         channel list\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if (!str_prefix(arg, "list"))
    {
        BUFFER *buffer = new_buf();
        bool append_ok = true;
        ITERATOR it;
        char *id;
        char buf[MSL];
        int index = 1;

        append_ok = add_buf(buffer, "{WBlueprint Channel Definitions:{x\n\r");
        if (!bp->channel_defs || list_size(bp->channel_defs) < 1)
        {
            append_ok = append_ok && add_buf(buffer, "  None\n\r");
        }
        else
        {
            iterator_start(&it, bp->channel_defs);
            while ((id = (char *)iterator_nextdata(&it)))
            {
                sprintf(buf, "  {W%2d{x) %s\n\r", index++, id);
                if (!add_buf(buffer, buf))
                {
                    append_ok = false;
                    break;
                }
            }
            iterator_stop(&it);
        }

        if (!append_ok)
        {
            send_to_char("Channel list output exceeded buffer limits.\n\r", ch);
            free_buf(buffer);
            return false;
        }

        page_to_char(buffer->string, ch);
        free_buf(buffer);
        return false;
    }

    if (!str_prefix(arg, "add"))
    {
        const CHANNEL_DEF_DATA *def;
        ITERATOR it;
        char *existing;

        argument = one_argument(argument, channel_id);
        if (channel_id[0] == '\0')
        {
            send_to_char("Please specify a channel id.\n\r", ch);
            return false;
        }

        def = channel_registry_find(channel_id);
        if (!def)
        {
            send_to_char("Unknown channel id. Use cedit to create it first.\n\r", ch);
            return false;
        }

        iterator_start(&it, bp->channel_defs);
        while ((existing = (char *)iterator_nextdata(&it)))
        {
            if (!str_cmp(existing, channel_id))
            {
                iterator_stop(&it);
                send_to_char("That channel id is already attached.\n\r", ch);
                return false;
            }
        }
        iterator_stop(&it);

        list_appendlink(bp->channel_defs, str_dup(def->id));
        send_to_char("Channel definition attached to blueprint.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg, "del") || !str_prefix(arg, "delete") || !str_prefix(arg, "remove"))
    {
        if (argument[0] == '\0')
        {
            send_to_char("Please specify an index or channel id.\n\r", ch);
            return false;
        }

        if (is_number(argument))
        {
            int index = atoi(argument);
            char *id;

            if (index < 1 || index > list_size(bp->channel_defs))
            {
                send_to_char("Index out of range.\n\r", ch);
                return false;
            }

            id = (char *)list_nthdata(bp->channel_defs, index);
            if (id)
                free_string(id);
            list_remnthlink(bp->channel_defs, index, false);
            send_to_char("Channel definition removed from blueprint.\n\r", ch);
            return true;
        }
        else
        {
            ITERATOR it;
            char *id;
            int index = 1;

            iterator_start(&it, bp->channel_defs);
            while ((id = (char *)iterator_nextdata(&it)))
            {
                if (!str_cmp(id, argument))
                {
                    iterator_stop(&it);
                    free_string(id);
                    list_remnthlink(bp->channel_defs, index, false);
                    send_to_char("Channel definition removed from blueprint.\n\r", ch);
                    return true;
                }
                index++;
            }
            iterator_stop(&it);

            send_to_char("That channel id is not attached.\n\r", ch);
            return false;
        }
    }

    bpedit_channel(ch, "");
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
        send_to_char("         static entry add <section#> <link#>\n\r", ch);
        send_to_char("         static entry remove <#>\n\r", ch);
        send_to_char("         static exit add <section#> <link#>\n\r", ch);
        send_to_char("         static exit remove <#>\n\r", ch);
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
                if( !is_number(arg4) )
                {
                    send_to_char("Section must be a number.\n\r", ch);
                    return false;
                }

                int section = atoi(arg4);

                if( section < 1 || section > list_size(bp->sections) )
                {
                    send_to_char("Section number out of range.\n\r", ch);
                    return false;
                }

                BLUEPRINT_SECTION_REF *bs_ref = list_nthdata(bp->sections, section);
                BLUEPRINT_SECTION *bs = bs_ref ? bs_ref->section : NULL;

                WNUM room_wnum;
                AREA_DATA *context = olc_relative_widevnum_context(bp->area, argument);
                if (!parse_widevnum(argument, context, &room_wnum)) {
                    send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
                    return false;
                }

                long vnum = room_wnum.vnum;

                if( vnum < bs->lower_vnum || vnum > bs->upper_vnum )
                {
                    send_to_char("Room vnum not in the section.\n\r", ch);
                    return false;
                }

                ROOM_INDEX_DATA *room = get_room_index(room_wnum.pArea, vnum);
                if( !room )
                {
                    send_to_char("Room does not exist.\n\r", ch);
                    return false;
                }

                special->section = section;
                special->room_ref.load.vnum = vnum;
                special->room_ref.load.auid = room_wnum.pArea ? room_wnum.pArea->uid : 0;
                special->room = room;

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


            if( !is_number(arg3) )
            {
                send_to_char("Section must be a number.\n\r", ch);
                return false;
            }

            int section = atoi(arg3);

            if( section < 1 || section > list_size(bp->sections) )
            {
                send_to_char("Section number out of range.\n\r", ch);
                return false;
            }

            BLUEPRINT_SECTION_REF *bs_ref = list_nthdata(bp->sections, section);
            BLUEPRINT_SECTION *bs = bs_ref ? bs_ref->section : NULL;

            WNUM room_wnum;
            AREA_DATA *context = olc_relative_widevnum_context(bp->area, arg4);
            if (!parse_widevnum(arg4, context, &room_wnum)) {
                send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
                return false;
            }

            long vnum = room_wnum.vnum;

            if( vnum < bs->lower_vnum || vnum > bs->upper_vnum )
            {
                send_to_char("Room vnum not in the section.\n\r", ch);
                return false;
            }

            ROOM_INDEX_DATA *room = get_room_index(room_wnum.pArea, vnum);
            if( !room )
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
            special->room_ref.load.vnum = vnum;
            special->room_ref.load.auid = room_wnum.pArea ? room_wnum.pArea->uid : 0;
            special->room = room;

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
            BLUEPRINT_SECTION_REF *bs_ref;
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

            bs_ref = (BLUEPRINT_SECTION_REF *)list_nthdata(bp->sections, section1);
            bs = bs_ref ? bs_ref->section : NULL;
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

            bs_ref = (BLUEPRINT_SECTION_REF *)list_nthdata(bp->sections, section2);
            bs = bs_ref ? bs_ref->section : NULL;
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

            sbl->next = bp->_static.layout;
            bp->_static.layout = sbl;

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
            for(cur = bp->_static.layout; cur && index > 0; prev = cur, cur = cur->next)
            {
                if( !--index )
                {
                    if( prev )
                        prev->next = cur->next;
                    else
                        bp->_static.layout = cur->next;

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

            bp->_static.recall = index;
            send_to_char("Blueprint recall section changed.\n\r", ch);
            return true;
        }

        if( !str_prefix(argument, "clear") )
        {
            bp->_static.recall = -1;
            send_to_char("Blueprint recall section cleared.\n\r", ch);
            return true;
        }

        bpedit_static(ch, "recall");
        return false;
    }

    if( !str_prefix(arg, "entry") )
    {
        char arg2[MIL];
        char arg3[MIL];


        if( argument[0] == '\0' )
        {
            send_to_char("Syntax:  static entry add <section#> <link#>\n\r", ch);
            send_to_char("         static entry remove <#>\n\r", ch);
            return false;
        }

        argument = one_argument(argument, arg2);

        if (!str_prefix(arg2, "add"))
        {
            argument = one_argument(argument, arg3);

            if( is_number(arg3) && is_number(argument) )
            {
                int section = atoi(arg3);
                int link = atoi(argument);

                if( section < 1 || section > list_size(bp->sections) )
                {
                    send_to_char("Section index out of range.\n\r", ch);
                    return false;
                }

                BLUEPRINT_SECTION_REF *bs_ref = (BLUEPRINT_SECTION_REF *)list_nthdata(bp->sections, section);
                BLUEPRINT_SECTION *bs = bs_ref ? bs_ref->section : NULL;

                if( !get_section_link(bs, link) )
                {
                    send_to_char("Link index out of range.\n\r", ch);
                    return false;
                }

                BLUEPRINT_EXIT_DATA *bex = new_blueprint_exit_data();
                bex->section = section;
                bex->link = link;
                list_appendlink(bp->_static.entries, bex);

                send_to_char("Blueprint entry point added.\n\r", ch);
                return true;
            }

            send_to_char("Syntax:  static entry add <section#> <link#>\n\r", ch);
            return false;
        }

        if( !str_prefix(arg2, "remove") )
        {
            if (!is_number(argument))
            {
                send_to_char("Syntax:  static entry remove <#>\n\r", ch);
                return false;
            }

            int index = atoi(argument);
            if (index < 1 || index > list_size(bp->_static.entries))
            {
                send_to_char("No such entry point.\n\r", ch);
                return false;
            }

            list_remnthlink(bp->_static.entries, index, true);

            send_to_char("Blueprint entry point removed.\n\r", ch);
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
            send_to_char("Syntax:  static exit add <section#> <link#>\n\r", ch);
            send_to_char("         static exit remove <#>\n\r", ch);
            return false;
        }

        argument = one_argument(argument, arg2);

        if (!str_prefix(arg2, "add"))
        {
            char arg3[MIL];
            
            argument = one_argument(argument, arg3);

            if( is_number(arg3) && is_number(argument) )
            {
                int section = atoi(arg3);
                int link = atoi(argument);

                if( section < 1 || section > list_size(bp->sections) )
                {
                    send_to_char("Section index out of range.\n\r", ch);
                    return false;
                }

                BLUEPRINT_SECTION_REF *bs_ref = (BLUEPRINT_SECTION_REF *)list_nthdata(bp->sections, section);
                BLUEPRINT_SECTION *bs = bs_ref ? bs_ref->section : NULL;

                if( !get_section_link(bs, link) )
                {
                    send_to_char("Link index out of range.\n\r", ch);
                    return false;
                }

                BLUEPRINT_EXIT_DATA *bex = new_blueprint_exit_data();
                bex->section = section;
                bex->link = link;
                list_appendlink(bp->_static.exits, bex);

                send_to_char("Blueprint exit point added.\n\r", ch);
                return true;
            }

            send_to_char("Syntax:  static exit add <section#> <link#>\n\r", ch);
            return false;
        }

        if( !str_prefix(arg2, "remove") )
        {
            if (!is_number(argument))
            {
                send_to_char("Syntax:  static exit remove <#>\n\r", ch);
                return false;
            }

            int index = atoi(argument);
            if (index < 1 || index > list_size(bp->_static.exits))
            {
                send_to_char("No such exit point.\n\r", ch);
                return false;
            }

            list_remnthlink(bp->_static.exits, index, true);

            send_to_char("Blueprint exit point removed.\n\r", ch);
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

    if (num[0] == '\0' || trigger[0] =='\0' || phrase[0] =='\0')
    {
    send_to_char("Syntax:   addiprog [widevnum] [trigger] [phrase]\n\r",ch);
    return false;
    }

    if ((tindex = trigger_index(trigger, PRG_IPROG)) < 0) {
    send_to_char("Valid flags are:\n\r",ch);
    show_help(ch, "iprog");
    return false;
    }

    slot = trigger_table[tindex].slot;

    WNUM script_wnum;
    AREA_DATA *context = olc_relative_widevnum_context(blueprint->area, num);
    if (!parse_widevnum(num, context, &script_wnum)) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    if ((code = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_IPROG)) == NULL)
    {
    send_to_char("No such INSTANCEProgram.\n\r",ch);
    return false;
    }

    // Make sure this has a list of progs!
    if(!blueprint->progs) blueprint->progs = new_prog_bank();

    if (edit_trigger_exists(blueprint->progs, code, tindex, phrase)) {
        send_to_char("That trigger/phrase pair is already attached to that script on this blueprint.\n\r", ch);
        return false;
    }

    list                  = new_trigger();
    list->vnum            = script_wnum.vnum;
    list->script_is_widevnum = (script_wnum.pArea != NULL);
    if (list->script_is_widevnum) { list->script_load.auid = script_wnum.pArea->uid; list->script_load.vnum = script_wnum.vnum; }
    list->trig_type       = tindex;
    list->trig_phrase     = str_dup(phrase);
    if (is_widevnum_format(phrase)) {
        list->numeric = true;
        list->trig_is_widevnum = true;
        parse_widevnum_load(phrase, &list->trig_load);
        list->trig_number = (int)list->trig_load.vnum;
    } else {
        list->trig_number = atoi(list->trig_phrase);
        list->numeric = is_number(list->trig_phrase);
    }
    list->script          = code;

    list_appendlink(blueprint->progs[slot], list);

    send_to_char("Iprog Added.\n\r",ch);
    return true;
}

BPEDIT (bpedit_deliprog)
{
    BLUEPRINT *blueprint;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    int group_idx, trig_idx;
    PROG_GROUP groups[MAX_PROG_GROUPS];
    int num_groups;

    EDIT_BLUEPRINT(ch, blueprint);

    if (!blueprint->progs) {
        send_to_char("This blueprint has no programs attached.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0') {
        send_to_char("Syntax:  deliprog <group#>\n\r", ch);
        send_to_char("         deliprog <group#> <trigger#>\n\r", ch);
        return false;
    }

    if (!is_number(arg1)) {
        send_to_char("Please specify a valid group number.\n\r", ch);
        return false;
    }

    group_idx = atoi(arg1);
    num_groups = prog_build_groups(blueprint->progs, groups, MAX_PROG_GROUPS, PRG_IPROG);

    if (group_idx < 1 || group_idx > num_groups) {
        send_to_char("Invalid group number.\n\r", ch);
        return false;
    }

    PROG_GROUP *group = &groups[group_idx - 1];

    if (arg2[0] == '\0') {
        // Delete entire group
        if (edit_delscript(blueprint->progs, group->script)) {
            send_to_char("Script group removed.\n\r", ch);
            return true;
        }
    } else {
        // Delete specific trigger within group
        if (!is_number(arg2)) {
            send_to_char("Please specify a valid trigger number within the group.\n\r", ch);
            return false;
        }

        trig_idx = atoi(arg2);
        if (trig_idx < 1 || trig_idx > group->trigger_count) {
            send_to_char("Invalid trigger number within that group.\n\r", ch);
            return false;
        }

        PROG_GROUP_ENTRY *entry = &group->triggers[trig_idx - 1];
        if (edit_deltrigger_specific(blueprint->progs, group->script, entry->entry->trig_type, entry->entry->trig_phrase)) {
            send_to_char("Trigger removed from script group.\n\r", ch);
            return true;
        }
    }

    send_to_char("No such program or trigger found.\n\r", ch);
    return false;
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