/***************************************************************************
 *  dngedit.c - OLC Dungeon Editor                                         *
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

extern long top_dungeon_vnum;
extern void list_dungeons(CHAR_DATA *ch, char *argument);
extern bool can_edit_dungeons(CHAR_DATA *ch);

/***************************************************************************
 * Forward Declarations — Tab Show Functions                               *
 ***************************************************************************/

static void dngedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void dngedit_show_entryexit_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void dngedit_show_floors_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void dngedit_show_levels_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void dngedit_show_special_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void dngedit_show_programs_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void dngedit_show_variables_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void dngedit_show_notes_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);

/***************************************************************************
 * Framework Helpers                                                       *
 ***************************************************************************/

static AREA_DATA *dngedit_get_area(void *pEdit)
{
    return pEdit ? ((DUNGEON_INDEX_DATA *)pEdit)->area : NULL;
}

static bool dngedit_perm_check(CHAR_DATA *ch, void *pEdit)
{
    return can_edit_dungeons(ch);
}

static void dngedit_mark_changed(CHAR_DATA *ch, void *pEdit)
{
    DUNGEON_INDEX_DATA *dng = (DUNGEON_INDEX_DATA *)pEdit;
    if (dng && dng->area)
        SET_BIT(dng->area->area_flags, AREA_CHANGED);
}

/***************************************************************************
 * Dungeon Editor Command Table (moved from dungeon.c)                     *
 ***************************************************************************/

const struct olc_cmd_type dngedit_table[] = {
    { "?",              show_help           },
    { "adddprog",       dngedit_adddprog    },
    { "areawho",        dngedit_areawho     },
    { "commands",       show_commands       },
    { "comments",       dngedit_comments    },
    { "channel",        dngedit_channel     },
    { "create",         dngedit_create      },
    { "deathrelease",   dngedit_deathrelease },
    { "deldprog",       dngedit_deldprog    },
    { "description",    dngedit_description },
    { "entry",          dngedit_entry       },
    { "exit",           dngedit_exit        },
    { "flags",          dngedit_flags       },
    { "floors",         dngedit_floors      },
    { "idletimeout",    dngedit_idletimeout },
    { "levels",         dngedit_levels      },
    { "list",           dngedit_list        },
    { "maxgroup",       dngedit_maxgroup    },
    { "maxplayers",     dngedit_maxplayers  },
    { "mingroup",       dngedit_mingroup    },
    { "mountout",       dngedit_mountout    },
    { "name",           dngedit_name        },
    { "portalout",      dngedit_portalout   },
    { "show",           dngedit_show        },
    { "special",        dngedit_special     },
    { "varclear",       dngedit_varclear    },
    { "varset",         dngedit_varset      },
    { "zoneout",        dngedit_zoneout     },
    { NULL,             NULL                }
};

/***************************************************************************
 * Editor Definition (Unified Framework)                                   *
 ***************************************************************************/

static const OLC_EDITOR_DEF dngedit_def = {
    .name           = "DNGEdit",
    .editor_type    = ED_DUNGEON,
    .cmd_table      = dngedit_table,
    .show_fn        = dngedit_show,
    .tabs           = {
        .count      = 8,
        .tabs       = {
            { "General",    "Gen",  dngedit_show_general_tab    },
            { "Entry/Exit", "E-X",  dngedit_show_entryexit_tab  },
            { "Floors",     "Flr",  dngedit_show_floors_tab     },
            { "Levels",     "Lvl",  dngedit_show_levels_tab     },
            { "Special",    "Spc",  dngedit_show_special_tab    },
            { "Programs",   "Prg",  dngedit_show_programs_tab   },
            { "Variables",  "Var",  dngedit_show_variables_tab  },
            { "Notes",      "Nts",  dngedit_show_notes_tab      },
        },
    },
    .theme          = &olc_theme_building,
    .perm           = {
        .flags      = OLC_PERM_CUSTOM,
        .check_fn   = dngedit_perm_check,
    },
    .change_mode    = OLC_CHANGE_CUSTOM,
    .mark_changed_fn = dngedit_mark_changed,
    .get_area_fn    = dngedit_get_area,
    .audit_changes  = true,
};

/***************************************************************************
 * Dungeon Editor Interpreter — delegates to framework.                    *
 ***************************************************************************/

void dngedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &dngedit_def);
}

/***************************************************************************
 * Dungeon Editor Entry Point (moved from dungeon.c)                       *
 ***************************************************************************/

/**
 * do_dngedit - Staff command to edit or create dungeons
 *
 * Syntax: dngedit <vnum> - Edit existing dungeon
 *         dngedit create <vnum> - Create new dungeon
 *
 * @param ch        Staff character
 * @param argument  Dungeon vnum or "create <vnum>"
 */
void do_dngedit(CHAR_DATA *ch, char *argument)
{
    DUNGEON_INDEX_DATA *dng;
    char arg1[MAX_STRING_LENGTH];
    WNUM wnum;

    argument = one_argument(argument, arg1);

    if (IS_NPC(ch))
        return;

    if (!can_edit_dungeons(ch))
    {
        send_to_char("DNGEdit:  Insufficient security to edit dungeons.\n\r", ch);
        return;
    }

    if (parse_widevnum(arg1, ch->in_room->area, &wnum))
    {
        if (!(dng = get_dungeon_index_for_area(wnum.pArea, wnum.vnum)))
        {
            send_to_char("DNGEdit:  That dungeon does not exist.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &dngedit_def, (void *)dng, true);
        return;
    }
    else
    {
        if (!str_cmp(arg1, "create"))
        {
            if (dngedit_create(ch, argument))
            {
                olc_editor_enter(ch, &dngedit_def, ch->desc->pEdit, true);
            }

            return;
        }
    }

    send_to_char("Syntax: dngedit <#vnum|area_uid#vnum>\n\r"
                 "        dngedit create <vnum>\n\r", ch);
}

/***************************************************************************
 * Commands                                                                *
 ***************************************************************************/



DNGEDIT( dngedit_list )
{
    list_dungeons(ch, argument);
    return false;
}

static bool dngedit_add_or_fail(CHAR_DATA *ch, BUFFER *buffer, const char *text, const char *message)
{
    if (add_buf(buffer, text))
        return true;

    send_to_char(message, ch);
    return false;
}

static bool dngedit_render_weighted_exit_list(CHAR_DATA *ch, LLIST *weighted_exits, const char *overflow_message)
{
    BUFFER *buffer = new_buf();
    ITERATOR it;
    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *entry;
    char buf[MSL];
    int index = 1;

    if (!dngedit_add_or_fail(ch, buffer, "     [ Weight ] [ Level ] [ Exit ]\n\r", overflow_message)
        || !dngedit_add_or_fail(ch, buffer, "===================================\n\r", overflow_message))
    {
        free_buf(buffer);
        return false;
    }

    iterator_start(&it, weighted_exits);
    while ((entry = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&it)))
    {
        sprintf(buf, "%4d [ %6d ] [ %5d ] [ %4d ]\n\r", index++, entry->weight, entry->level, entry->door);
        if (!dngedit_add_or_fail(ch, buffer, buf, overflow_message))
        {
            iterator_stop(&it);
            free_buf(buffer);
            return false;
        }
    }
    iterator_stop(&it);

    if (!dngedit_add_or_fail(ch, buffer, "-----------------------------------\n\r", overflow_message))
    {
        free_buf(buffer);
        return false;
    }

    if (!ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH)
    {
        send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
    }
    else
    {
        page_to_char(buffer->string, ch);
    }

    free_buf(buffer);
    return true;
}

static bool dngedit_render_weighted_floor_list(CHAR_DATA *ch, DUNGEON_INDEX_DATA *dng, LLIST *weighted_floors, const char *overflow_message)
{
    BUFFER *buffer = new_buf();
    ITERATOR it;
    DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;
    char buf[MSL];
    int row = 0;

    if (!dngedit_add_or_fail(ch, buffer, "     [ Weight ] [ Floor ] [   Vnum   ] [             Name             ]\n\r", overflow_message)
        || !dngedit_add_or_fail(ch, buffer, "------------------------------------------------------------------------\n\r", overflow_message))
    {
        free_buf(buffer);
        return false;
    }

    iterator_start(&it, weighted_floors);
    while ((weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)iterator_nextdata(&it)))
    {
        BLUEPRINT *bp = list_nthdata(dng->floors, weighted->floor);
        snprintf(buf, MSL - 1, "%4d   %6d     %5d     %8s    %30.30s\n\r", ++row, weighted->weight, weighted->floor, widevnum_string_blueprint(bp, dng->area), bp->name);
        buf[MSL - 1] = '\0';
        if (!dngedit_add_or_fail(ch, buffer, buf, overflow_message))
        {
            iterator_stop(&it);
            free_buf(buffer);
            return false;
        }
    }
    iterator_stop(&it);

    if (!ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH)
    {
        send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
    }
    else
    {
        page_to_char(buffer->string, ch);
    }

    free_buf(buffer);
    return true;
}

static bool dngedit_render_special_room_list(CHAR_DATA *ch, DUNGEON_INDEX_DATA *dng, const char *overflow_message)
{
    BUFFER *buffer = new_buf();
    DUNGEON_INDEX_SPECIAL_ROOM *special;
    ITERATOR sit;
    char buf[MSL];
    int line = 0;

    if (!dngedit_add_or_fail(ch, buffer, "     [             Name             ] [ Level ] [ Room ]\n\r", overflow_message)
        || !dngedit_add_or_fail(ch, buffer, "---------------------------------------------------------\n\r", overflow_message))
    {
        free_buf(buffer);
        return false;
    }

    iterator_start(&sit, dng->special_rooms);
    while ((special = (DUNGEON_INDEX_SPECIAL_ROOM *)iterator_nextdata(&sit)))
    {
        snprintf(buf, MSL - 1, "%4d %30.30s{x    %5d     %4d\n\r", ++line, special->name, special->level, special->room);
        buf[MSL - 1] = '\0';
        if (!dngedit_add_or_fail(ch, buffer, buf, overflow_message))
        {
            iterator_stop(&sit);
            free_buf(buffer);
            return false;
        }
    }
    iterator_stop(&sit);

    if (!dngedit_add_or_fail(ch, buffer, "---------------------------------------------------------d\n\r", overflow_message))
    {
        free_buf(buffer);
        return false;
    }

    if (!ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH)
    {
        send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
    }
    else
    {
        page_to_char(buffer->string, ch);
    }

    free_buf(buffer);
    return true;
}

static bool dngedit_buffer_special_rooms_tab(BUFFER *buffer, DUNGEON_INDEX_DATA *dng)
{
    DUNGEON_INDEX_SPECIAL_ROOM *special;
    ITERATOR sit;
    char buf[MSL];
    int line = 0;

    if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
    {
        return add_buf(buffer, "   {WSCRIPTED{x\n\r");
    }

    if (list_size(dng->special_rooms) < 1)
    {
        return add_buf(buffer, "   None\n\r");
    }

    if (!add_buf(buffer, "     [             Name             ] [ Level ] [ Room ]\n\r")
        || !add_buf(buffer, "---------------------------------------------------------\n\r"))
    {
        return false;
    }

    iterator_start(&sit, dng->special_rooms);
    while ((special = (DUNGEON_INDEX_SPECIAL_ROOM *)iterator_nextdata(&sit)))
    {
        sprintf(buf, "{W%4d  %-30.30s   {G%7d{x     %4d\n\r", ++line, special->name, special->level, special->room);
        if (!add_buf(buffer, buf))
        {
            iterator_stop(&sit);
            return false;
        }
    }
    iterator_stop(&sit);

    return add_buf(buffer, "---------------------------------------------------------\n\r");
}

static bool dngedit_render_channel_list(CHAR_DATA *ch, DUNGEON_INDEX_DATA *dng)
{
    BUFFER *buffer = new_buf();
    ITERATOR it;
    char *id;
    char buf[MSL];
    int index = 1;

    if (!dngedit_add_or_fail(ch, buffer, "{WDungeon Channel Definitions:{x\n\r",
        "Dungeon channel list output exceeded buffer limits.\n\r"))
    {
        free_buf(buffer);
        return false;
    }

    if (!dng->channel_defs || list_size(dng->channel_defs) < 1)
    {
        if (!dngedit_add_or_fail(ch, buffer, "  None\n\r",
            "Dungeon channel list output exceeded buffer limits.\n\r"))
        {
            free_buf(buffer);
            return false;
        }
    }
    else
    {
        iterator_start(&it, dng->channel_defs);
        while ((id = (char *)iterator_nextdata(&it)))
        {
            sprintf(buf, "  {W%2d{x) %s\n\r", index++, id);
            if (!dngedit_add_or_fail(ch, buffer, buf,
                "Dungeon channel list output exceeded buffer limits.\n\r"))
            {
                iterator_stop(&it);
                free_buf(buffer);
                return false;
            }
        }
        iterator_stop(&it);
    }

    page_to_char(buffer->string, ch);
    free_buf(buffer);
    return true;
}

bool dngedit_buffer_floors(BUFFER *buffer, DUNGEON_INDEX_DATA *dng)
{
    char buf[MSL];

    if( list_size(dng->floors) > 0 )
    {
        ITERATOR fit;
        BLUEPRINT *bp;

        add_buf(buffer, "{gFloors:{x\n\r");
        add_buf(buffer, "{g     [    Vnum    ] [             Name             ]\n\r");
        add_buf(buffer, "{g=====================================================\n\r");

        int floor = 0;
        iterator_start(&fit, dng->floors);
        while( (bp = (BLUEPRINT *)iterator_nextdata(&fit)) )
        {
            sprintf(buf, "{W%4d  {G%-12s {x%-.30s{x\n\r", ++floor, widevnum_string_blueprint(bp, NULL), bp->name);
            add_buf(buffer, buf);
        }
        iterator_stop(&fit);
        add_buf(buffer, "=====================================================\n\r");
    }
    else
    {
        add_buf(buffer, "{gFloors:{x\n\r");
        add_buf(buffer, "   None\n\r");
    }

    return buffer->state != BUFFER_OVERFLOW;
}

bool dngedit_buffer_levels(BUFFER *buffer, DUNGEON_INDEX_DATA *dng)
{
    char buf[MSL];

    add_buf(buffer, "{yLevels:{x\n\r");
    if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
    {
        add_buf(buffer, "  {WSCRIPTED{x\n\r");
    }
    else if (list_size(dng->levels) > 0)
    {
        ITERATOR it;
        DUNGEON_INDEX_LEVEL_DATA *level;

        add_buf(buffer, "{y          [  Mode  ]{x\n\r");
        add_buf(buffer, "{y===================================================={x\n\r");

        int levelno = 1;
        iterator_start(&it, dng->levels);
        while( (level = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&it)) )
        {
            switch(level->mode)
            {
                case LEVELMODE_STATIC:
                {
                    BLUEPRINT *bp = (BLUEPRINT *)list_nthdata(dng->floors, level->floor);
                    sprintf(buf, "{W%4d        {YSTATIC{x     %4d - {x%23.23s{x\n\r", levelno++, level->floor, bp->name);
                    add_buf(buffer, buf);
                    break;
                }

                case LEVELMODE_WEIGHTED:
                {
                    ITERATOR wit;
                    DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;
                    BLUEPRINT *bp;

                    sprintf(buf, "{W%4d       {CWEIGHTED{x\n\r", levelno++);
                    add_buf(buffer, buf);
                    add_buf(buffer, "{c               [ Weight ] [              Floor             ]{x\n\r");
                    add_buf(buffer, "{c          ==================================================={x\n\r");

                    int weightno = 1;
                    iterator_start(&wit, level->weighted_floors);
                    while( (weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)iterator_nextdata(&wit)) )
                    {
                        
                        bp = (BLUEPRINT *)list_nthdata(dng->floors, weighted->floor);
                        sprintf(buf, "          {c%4d   {W%6d     {x%4d - %23.23s{x\n\r", weightno++, weighted->weight, weighted->floor, bp->name);
                        add_buf(buffer, buf);
                    }
                    iterator_stop(&wit);
                    add_buf(buffer, "{c          ----------------------------------------------------{x\n\r");

                    break;
                }

                case LEVELMODE_GROUP:
                {
                    ITERATOR git;
                    DUNGEON_INDEX_LEVEL_DATA *lvl;

                    if (list_size(level->group) > 0)
                    {
                        sprintf(buf, "{W%4d{x-{W%-4d    {GGROUP{x\n\r", levelno, levelno + list_size(level->group) - 1);
                        add_buf(buffer, buf);

                        add_buf(buffer, "          {y          [  Mode  ]{x\n\r");
                        add_buf(buffer, "          {y===================================================={x\n\r");

                        int glevelno = 1;
                        iterator_start(&git, level->group);
                        while( (lvl = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&git)) )
                        {
                            if (lvl->mode == LEVELMODE_STATIC)
                            {
                                BLUEPRINT *bp = (BLUEPRINT *)list_nthdata(dng->floors, lvl->floor);
                                sprintf(buf, "          {W%4d        {YSTATIC{x     %4d - {x%23.23s{x\n\r", glevelno++, lvl->floor, bp->name);
                                add_buf(buffer, buf);	
                            }
                            else if(lvl->mode == LEVELMODE_WEIGHTED)
                            {
                                ITERATOR wit;
                                DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;
                                BLUEPRINT *bp;

                                sprintf(buf, "          {W%4d       {CWEIGHTED{x\n\r", levelno++);
                                add_buf(buffer, buf);
                                add_buf(buffer, "{c                         [ Weight ] [              Floor             ]{x\n\r");
                                add_buf(buffer, "{c                    ==================================================={x\n\r");

                                int weightno = 1;
                                iterator_start(&wit, level->weighted_floors);
                                while( (weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)iterator_nextdata(&wit)) )
                                {
                                    
                                    bp = (BLUEPRINT *)list_nthdata(dng->floors, weighted->floor);
                                    sprintf(buf, "                    {c%4d   {W%6d     {x%4d - %23.23s{x\n\r", weightno++, weighted->weight, weighted->floor, bp->name);
                                    add_buf(buffer, buf);
                                }
                                iterator_stop(&wit);
                                add_buf(buffer, "{c                    ----------------------------------------------------{x\n\r");
                            }
                        }
                        iterator_stop(&git);

                        levelno += list_size(level->group);
                    }
                    else
                    {
                        // This is when you've added a group level and it has no sublevels added.
                        sprintf(buf, "{W%4d{x-{D????    {GGROUP{x\n\r", levelno++);
                        add_buf(buffer, buf);
                    }
                    break;
                }
            }
        }
        iterator_stop(&it);
        add_buf(buffer, "{y----------------------------------------------------{x\n\r");
    }
    else
    {
        add_buf(buffer, "  None\n\r");
    }

    return buffer->state != BUFFER_OVERFLOW;
}

bool dngedit_buffer_special_exits(BUFFER *buffer, DUNGEON_INDEX_DATA *dng)
{
    char buf[MSL];

    add_buf(buffer, "{xSpecial Exits:{x\n\r");
    if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
    {
        add_buf(buffer, "  {WSCRIPTED{x\n\r");
    }
    else if(list_size(dng->special_exits) > 0)
    {
        ITERATOR it;
        DUNGEON_INDEX_SPECIAL_EXIT *dsex;

        add_buf(buffer, "{x          [    Mode    ]{x\n\r");
        add_buf(buffer, "{x========================================================{x\n\r");

        int exitno = 1;
        iterator_start(&it, dng->special_exits);
        while ( (dsex = (DUNGEON_INDEX_SPECIAL_EXIT *)iterator_nextdata(&it)))
        {
            switch(dsex->mode)
            {
                case EXITMODE_STATIC:
                {
                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(dsex->from, 1);
                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(dsex->to, 1);
                    sprintf(buf, "%4d          {YSTATIC{x\n\r", exitno++);
                    sprintf(buf, "          Source:        %4d (%d)\n\r", from->level, from->door);
                    sprintf(buf, "          Destination:   %4d (%d)\n\r", to->level, to->door);
                    add_buf(buffer, buf);
                    break;	
                }

                case EXITMODE_WEIGHTED_SOURCE:
                {
                    ITERATOR wit;
                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from;
                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(dsex->to, 1);
                    sprintf(buf, "%4d        {CWEIGHTED S{x\n\r", exitno++);
                    add_buf(buffer, buf);

                    int fromexitno = 1;
                    add_buf(buffer, "          Source:\n\r");
                    add_buf(buffer, "               [ Weight ] [ Level ] [ Exit# ]{x\n\r");
                    add_buf(buffer, "          ===================================={x\n\r");
                    iterator_start(&wit, dsex->from);
                    while ( (from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
                    {
                        sprintf(buf, "          %4d   %6d    %5d     %5d\n\r", fromexitno++, from->weight, from->level, from->door);
                        add_buf(buffer, buf);
                    }
                    iterator_stop(&wit);
                    add_buf(buffer, "          ----------------------------------------------------{x\n\r");

                    sprintf(buf,    "          Destination:   %4d (%d)\n\r", to->level, to->door);
                    add_buf(buffer, buf);
                    break;
                }

                case EXITMODE_WEIGHTED_DEST:
                {
                    ITERATOR wit;
                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(dsex->from, 1);
                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to;
                    sprintf(buf, "%4d        {CWEIGHTED D{x\n\r", exitno++);
                    add_buf(buffer, buf);

                    sprintf(buf,    "          Source:        %4d (%d)\n\r", from->level, from->door);
                    add_buf(buffer, buf);

                    int toexitno = 1;
                    add_buf(buffer, "          Destination:\n\r");
                    add_buf(buffer, "               [ Weight ] [ Level ] [ Exit# ]{x\n\r");
                    add_buf(buffer, "          ===================================={x\n\r");
                    iterator_start(&wit, dsex->to);
                    while ( (to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
                    {
                        sprintf(buf, "          %4d   %6d    %5d     %5d\n\r", toexitno++, to->weight, to->level, to->door);
                        add_buf(buffer, buf);
                    }
                    iterator_stop(&wit);
                    add_buf(buffer, "          ----------------------------------------------------{x\n\r");
                    break;
                }

                case EXITMODE_WEIGHTED:
                {
                    ITERATOR wit;
                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from;
                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to;
                    sprintf(buf, "%4d         {CWEIGHTED{x\n\r", exitno++);
                    add_buf(buffer, buf);

                    int fromexitno = 1;
                    add_buf(buffer, "          Source:\n\r");
                    add_buf(buffer, "               [ Weight ] [ Level ] [ Exit# ]{x\n\r");
                    add_buf(buffer, "          ===================================={x\n\r");
                    iterator_start(&wit, dsex->from);
                    while ( (from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
                    {
                        sprintf(buf, "          %4d   %6d    %5d     %5d\n\r", fromexitno++, from->weight, from->level, from->door);
                        add_buf(buffer, buf);
                    }
                    iterator_stop(&wit);
                    add_buf(buffer, "          ----------------------------------------------------{x\n\r");

                    int toexitno = 1;
                    add_buf(buffer, "          Destination:\n\r");
                    add_buf(buffer, "               [ Weight ] [ Level ] [ Exit# ]{x\n\r");
                    add_buf(buffer, "          ===================================={x\n\r");
                    iterator_start(&wit, dsex->to);
                    while ( (to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
                    {
                        sprintf(buf, "          %4d   %6d    %5d     %5d\n\r", toexitno++, to->weight, to->level, to->door);
                        add_buf(buffer, buf);
                    }
                    iterator_stop(&wit);
                    add_buf(buffer, "          ----------------------------------------------------{x\n\r");
                    break;
                }

                case EXITMODE_GROUP:
                {
                    int count = list_size(dsex->group);
                    if (count > 0)
                    {
                        ITERATOR git;
                        DUNGEON_INDEX_SPECIAL_EXIT *gex;

                        sprintf(buf, "%4d-%-4d      {GGROUP{x\n\r", exitno, exitno + count - 1);
                        add_buf(buffer, buf);
                        add_buf(buffer, "{x                    [    Mode    ]{x\n\r");
                        add_buf(buffer, "{x          ========================================================{x\n\r");

                        int gexitno = 1;
                        iterator_start(&git, dsex->group);
                        while ( (gex = (DUNGEON_INDEX_SPECIAL_EXIT *)iterator_nextdata(&git)) )
                        {
                            switch(gex->mode)
                            {
                                case EXITMODE_STATIC:
                                {
                                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(gex->from, 1);
                                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(gex->to, 1);
                                    sprintf(buf, "          %4d          {YSTATIC{x\n\r", gexitno++);
                                    sprintf(buf, "                    Source:        %4d (%d)\n\r", from->level, from->door);
                                    sprintf(buf, "                    Destination:   %4d (%d)\n\r", to->level, to->door);
                                    add_buf(buffer, buf);
                                    break;
                                }

                                case EXITMODE_WEIGHTED_SOURCE:
                                {
                                    ITERATOR wit;
                                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from;
                                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(gex->to, 1);
                                    sprintf(buf, "          %4d        {CWEIGHTED S{x\n\r", gexitno++);
                                    add_buf(buffer, buf);

                                    int fromexitno = 1;
                                    add_buf(buffer, "                    Source:\n\r");
                                    add_buf(buffer, "                         [ Weight ] [ Level ] [ Exit# ]{x\n\r");
                                    add_buf(buffer, "                    ===================================={x\n\r");
                                    iterator_start(&wit, gex->from);
                                    while ( (from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
                                    {
                                        sprintf(buf, "                    %4d   %6d    %5d     %5d\n\r", fromexitno++, from->weight, from->level, from->door);
                                        add_buf(buffer, buf);
                                    }
                                    iterator_stop(&wit);
                                    add_buf(buffer, "                    ----------------------------------------------------{x\n\r");

                                    sprintf(buf,    "                    Destination:   %4d (%d)\n\r", to->level, to->door);
                                    add_buf(buffer, buf);
                                    break;
                                }

                                case EXITMODE_WEIGHTED_DEST:
                                {
                                    ITERATOR wit;
                                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(gex->from, 1);
                                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to;
                                    sprintf(buf, "          %4d        {CWEIGHTED D{x\n\r", gexitno++);
                                    add_buf(buffer, buf);

                                    sprintf(buf,    "                    Source:        %4d (%d)\n\r", from->level, from->door);
                                    add_buf(buffer, buf);

                                    int toexitno = 1;
                                    add_buf(buffer, "                    Destination:\n\r");
                                    add_buf(buffer, "                         [ Weight ] [ Level ] [ Exit# ]{x\n\r");
                                    add_buf(buffer, "                    ===================================={x\n\r");
                                    iterator_start(&wit, gex->to);
                                    while ( (to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
                                    {
                                        sprintf(buf, "                    %4d   %6d    %5d     %5d\n\r", toexitno++, to->weight, to->level, to->door);
                                        add_buf(buffer, buf);
                                    }
                                    iterator_stop(&wit);
                                    add_buf(buffer, "                    ----------------------------------------------------{x\n\r");
                                    break;
                                }

                                case EXITMODE_WEIGHTED:
                                {
                                    ITERATOR wit;
                                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from;
                                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to;
                                    sprintf(buf, "          %4d         {CWEIGHTED{x\n\r", gexitno++);
                                    add_buf(buffer, buf);

                                    int fromexitno = 1;
                                    add_buf(buffer, "                    Source:\n\r");
                                    add_buf(buffer, "                         [ Weight ] [ Level ] [ Exit# ]{x\n\r");
                                    add_buf(buffer, "                    ===================================={x\n\r");
                                    iterator_start(&wit, gex->from);
                                    while ( (from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
                                    {
                                        sprintf(buf, "                    %4d   %6d    %5d     %5d\n\r", fromexitno++, from->weight, from->level, from->door);
                                        add_buf(buffer, buf);
                                    }
                                    iterator_stop(&wit);
                                    add_buf(buffer, "                    ----------------------------------------------------{x\n\r");

                                    int toexitno = 1;
                                    add_buf(buffer, "                    Destination:\n\r");
                                    add_buf(buffer, "                         [ Weight ] [ Level ] [ Exit# ]{x\n\r");
                                    add_buf(buffer, "                    ===================================={x\n\r");
                                    iterator_start(&wit, gex->to);
                                    while ( (to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
                                    {
                                        sprintf(buf, "                    %4d   %6d    %5d     %5d\n\r", toexitno++, to->weight, to->level, to->door);
                                        add_buf(buffer, buf);
                                    }
                                    iterator_stop(&wit);
                                    add_buf(buffer, "                    ----------------------------------------------------{x\n\r");
                                    break;
                                }
                            }

                        }
                        iterator_stop(&git);
                        add_buf(buffer, "{x          --------------------------------------------------------{x\n\r");
                        exitno += count;
                    }
                    else
                    {
                        sprintf(buf, "%4d-????      {GGROUP{x\n\r", exitno++);
                        add_buf(buffer, buf);
                        add_buf(buffer, "          None\n\r");
                    }


                    break;
                }
            }

        }

        iterator_stop(&it);
        add_buf(buffer, "{x----------------------------------------------------{x\n\r");
    }
    else
    {
        add_buf(buffer, "  None\n\r");
    }


    return buffer->state != BUFFER_OVERFLOW;
}

/***************************************************************************
 * Master Show — dispatches to active tab                                  *
 ***************************************************************************/

DNGEDIT( dngedit_show )
{
    DUNGEON_INDEX_DATA *dng;
    EDIT_DUNGEON(ch, dng);

    const OLC_EDITOR_THEME *theme = dngedit_def.theme;
    OLC_LAYOUT_CTX *ctx = olc_display_new(ch, theme);

    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "%s", widevnum_string_dungeon(dng, dng->area));
    olc_display_header(ctx, "DNGEdit", dng->name, id_buf, &dngedit_def);

    /* Dispatch to active tab's show function */
    int tab = olc_show_all_tabs_mode(ch) ? -1 : (ch->desc ? ch->desc->nEditTab : 0);
    if (tab < 0) {
        for (int i = 0; i < dngedit_def.tabs.count; i++) {
            if (dngedit_def.tabs.tabs[i].show_fn)
                dngedit_def.tabs.tabs[i].show_fn(ch, ctx, (void *)dng);
        }
    } else if (tab >= 0 && tab < dngedit_def.tabs.count
        && dngedit_def.tabs.tabs[tab].show_fn) {
        dngedit_def.tabs.tabs[tab].show_fn(ch, ctx, (void *)dng);
    } else {
        dngedit_show_general_tab(ch, ctx, (void *)dng);
    }

    olc_display_footer(ctx, theme);
    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

/***************************************************************************
 * Tab 1: General                                                          *
 ***************************************************************************/

static void dngedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    DUNGEON_INDEX_DATA *dng = (DUNGEON_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = dngedit_def.theme;
    char buf[MSL];

    sprintf(buf, "[%s] %s", widevnum_string_dungeon(dng, dng->area), dng->name);
    olc_display_string(ctx, theme, "Name:", "name", buf);

    olc_display_flags(ctx, theme, "Flags:", "flags", dungeon_flags, dng->flags);
    olc_display_string(ctx, theme, "AreaWho:", "areawho", flag_string(area_who_titles, dng->area_who));

    if (dng->repop > 0)
        sprintf(buf, "%d minutes", dng->repop);
    else
        sprintf(buf, "{Dnever{x");
    olc_display_string(ctx, theme, "Repop:", "repop", buf);

    if (dng->idle_timeout > 0)
        sprintf(buf, "%d minutes", dng->idle_timeout);
    else
        sprintf(buf, "%d minutes {D(default){x", DUNGEON_IDLE_TIMEOUT);
    olc_display_string(ctx, theme, "IdleTimeout:", "idletimeout", buf);

    sprintf(buf, "%d%s", dng->min_group, dng->min_group == 0 ? " (no minimum)" : "");
    olc_display_string(ctx, theme, "MinGroup:", "mingroup", buf);

    sprintf(buf, "%d%s", dng->max_group, dng->max_group == 0 ? " (unlimited)" : "");
    olc_display_string(ctx, theme, "MaxGroup:", "maxgroup", buf);

    sprintf(buf, "%d%s", dng->max_players, dng->max_players == 0 ? " (unlimited)" : "");
    olc_display_string(ctx, theme, "MaxPlayers:", "maxplayers", buf);

    olc_display_string(ctx, theme, "DeathRelease:", "deathrelease", flag_string(death_release_types, dng->death_release));

    olc_display_section(ctx, theme, "Description");
    if (!add_buf(ctx->buffer, dng->description))
    {
        send_to_char("Dungeon description output exceeded buffer limits.\n\r", ch);
        return;
    }
}

/***************************************************************************
 * Tab 2: Entry/Exit                                                       *
 ***************************************************************************/

static void dngedit_show_entryexit_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    DUNGEON_INDEX_DATA *dng = (DUNGEON_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = dngedit_def.theme;
    ROOM_INDEX_DATA *room;
    char buf[MSL];

    /* Entry room */
    room = dng->entry_room;
    if (room)
    {
        sprintf(buf, "[%s] %-.30s", widevnum_string_room(room, dng->area), room->name);
        olc_display_string(ctx, theme, "Entry:", "entry", buf);
    }
    else
        olc_display_string(ctx, theme, "Entry:", "entry", "{Dinvalid{x");

    /* Exit room */
    room = dng->exit_room;
    if (room)
    {
        sprintf(buf, "[%s] %-.30s", widevnum_string_room(room, dng->area), room->name);
        olc_display_string(ctx, theme, "Exit:", "exit", buf);
    }
    else
        olc_display_string(ctx, theme, "Exit:", "exit", "{Dinvalid{x");

    olc_display_string(ctx, theme, "ZoneOut:", "zoneout", dng->zone_out);
    olc_display_string(ctx, theme, "PortalOut:", "portalout", dng->zone_out_portal);
    olc_display_string(ctx, theme, "MountOut:", "mountout", dng->zone_out_mount);
}

/***************************************************************************
 * Tab 3: Floors                                                           *
 ***************************************************************************/

static void dngedit_show_floors_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    DUNGEON_INDEX_DATA *dng = (DUNGEON_INDEX_DATA *)pEdit;
    if (!dngedit_buffer_floors(ctx->buffer, dng))
        send_to_char("Dungeon floors output exceeded buffer limits.\n\r", ch);
}

/***************************************************************************
 * Tab 4: Levels                                                           *
 ***************************************************************************/

static void dngedit_show_levels_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    DUNGEON_INDEX_DATA *dng = (DUNGEON_INDEX_DATA *)pEdit;
    if (!dngedit_buffer_levels(ctx->buffer, dng))
        send_to_char("Dungeon levels output exceeded buffer limits.\n\r", ch);
}

/***************************************************************************
 * Tab 5: Special (Special Rooms + Special Exits)                          *
 ***************************************************************************/

static void dngedit_show_special_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    DUNGEON_INDEX_DATA *dng = (DUNGEON_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = dngedit_def.theme;

    /* Special Rooms */
    olc_display_section(ctx, theme, "Special Rooms");
    if (!dngedit_buffer_special_rooms_tab(ctx->buffer, dng))
    {
        send_to_char("Dungeon special room output exceeded buffer limits.\n\r", ch);
        return;
    }

    if (!add_buf(ctx->buffer, "\n\r"))
    {
        send_to_char("Dungeon special output exceeded buffer limits.\n\r", ch);
        return;
    }

    /* Special Exits */
    if (!dngedit_buffer_special_exits(ctx->buffer, dng))
    {
        send_to_char("Dungeon special exits output exceeded buffer limits.\n\r", ch);
        return;
    }
}

/***************************************************************************
 * Tab 6: Programs                                                         *
 ***************************************************************************/

static void dngedit_show_programs_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    DUNGEON_INDEX_DATA *dng = (DUNGEON_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = dngedit_def.theme;

    if (dng->progs)
        olc_show_progs_grouped(ctx->buffer, dng->progs, PRG_DPROG, "Dungeon Programs");
    else
        olc_display_string(ctx, theme, "Programs:", NULL, "None");
}

/***************************************************************************
 * Tab 7: Variables                                                        *
 ***************************************************************************/

static void dngedit_show_variables_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    DUNGEON_INDEX_DATA *dng = (DUNGEON_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = dngedit_def.theme;
    char buf[MSL];

    if (dng->index_vars)
    {
        pVARIABLE var;
        int cnt;

        for (cnt = 0, var = dng->index_vars; var; var = var->next) ++cnt;

        if (cnt > 0)
        {
            olc_display_section(ctx, theme, "Index Variables");

            sprintf(buf, "{R%-20s %-8s %-5s %-10s\n\r{x", "Name", "Type", "Saved", "Value");
            if (!add_buf(ctx->buffer, buf))
            {
                send_to_char("Dungeon variable output exceeded buffer limits.\n\r", ch);
                return;
            }
            sprintf(buf, "{R%-20s %-8s %-5s %-10s\n\r{x", "----", "----", "-----", "-----");
            if (!add_buf(ctx->buffer, buf))
            {
                send_to_char("Dungeon variable output exceeded buffer limits.\n\r", ch);
                return;
            }

            for (var = dng->index_vars; var; var = var->next)
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
                        sprintf(buf, "{x%-20.20s {GROOM       {Y%c   {W%s {R({W%s{R){x\n\r", var->name, var->save?'Y':'N', var->_.r->name, widevnum_string_room(var->_.r, dng->area));
                    else
                        sprintf(buf, "{x%-20.20s {GROOM       {Y%c   {W-no-where-{x\n\r", var->name, var->save?'Y':'N');
                    break;
                default:
                    continue;
                }
                if (!add_buf(ctx->buffer, buf))
                {
                    send_to_char("Dungeon variable output exceeded buffer limits.\n\r", ch);
                    return;
                }
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
 * Tab 8: Notes (Builder Comments)                                         *
 ***************************************************************************/

static void dngedit_show_notes_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    DUNGEON_INDEX_DATA *dng = (DUNGEON_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = dngedit_def.theme;

    olc_display_section(ctx, theme, "Builders' Comments");
    if (!add_buf(ctx->buffer, dng->comments ? dng->comments : "(none)\n\r"))
        send_to_char("Dungeon comments output exceeded buffer limits.\n\r", ch);
}

DNGEDIT( dngedit_create )
{
    DUNGEON_INDEX_DATA *dng;
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
            if (!get_dungeon_index_for_area(pArea, try_vnum))
            {
                value = try_vnum;
                break;
            }
        }

        if (value == 0)
        {
            send_to_char("DNGEdit: Could not find an available vnum in this area.\n\r", ch);
            return false;
        }
    }
    else
    {
        WNUM dng_wnum;
        if (!parse_widevnum(argument, ch->in_room->area, &dng_wnum)) {
            send_to_char("DNGEdit: Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        value = dng_wnum.vnum;
        pArea = dng_wnum.pArea;
    }

    if (!pArea)
    {
        send_to_char("DNGEdit: That vnum is not assigned an area.\n\r", ch);
        return false;
    }

    if (!IS_BUILDER(ch, pArea))
    {
        send_to_char("DNGEdit: Vnum in an area you cannot build in.\n\r", ch);
        return false;
    }

    if (get_dungeon_index_for_area(pArea, value))
    {
        send_to_char("DNGEdit: That vnum already exists.\n\r", ch);
        return false;
    }

    dng = new_dungeon_index();
    dng->vnum = value;
    dng->area = pArea;

    iHash                                   = dng->vnum % MAX_KEY_HASH;
    dng->next                               = pArea->dungeon_index_hash[iHash];
    pArea->dungeon_index_hash[iHash]        = dng;
    ch->desc->pEdit                         = (void *)dng;

    if (dng->vnum > top_dungeon_vnum)
        top_dungeon_vnum = dng->vnum;

    send_to_char("Dungeon Created.\n\r", ch);
    SET_BIT(pArea->area_flags, AREA_CHANGED);
    return true;
}

DNGEDIT( dngedit_name )
{
    DUNGEON_INDEX_DATA *dng;
    EDIT_DUNGEON(ch, dng);

    return olc_cmd_string(ch, argument, "Name", NULL, &dng->name,
        OLC_STR_DEFAULT | OLC_STR_UTF8_RESTRICT, NULL, NULL);
}

DNGEDIT( dngedit_repop )
{
    DUNGEON_INDEX_DATA *dng;
    EDIT_DUNGEON(ch, dng);

    return olc_cmd_number(ch, argument, "Repop", NULL, &dng->repop,
        0, INT_MAX, NULL, NULL);
}


DNGEDIT( dngedit_description )
{
    DUNGEON_INDEX_DATA *dng;
    EDIT_DUNGEON(ch, dng);

    return olc_cmd_string_append(ch, argument, "Description", NULL,
        &dng->description, NULL, NULL);
}

DNGEDIT( dngedit_comments )
{
    DUNGEON_INDEX_DATA *dng;
    EDIT_DUNGEON(ch, dng);

    return olc_cmd_string_append(ch, argument, "Comments", NULL,
        &dng->comments, NULL, NULL);
}

DNGEDIT( dngedit_areawho )
{
    DUNGEON_INDEX_DATA *dng;
    int value;

    EDIT_DUNGEON(ch, dng);

    if (argument[0] != '\0')
    {
        if ( !str_prefix(argument, "blank") )
        {
            dng->area_who = AREA_BLANK;

            send_to_char("Area who title cleared.\n\r", ch);
            return true;
        }

        if ((value = flag_value(area_who_titles, argument)) != NO_FLAG)
        {
            if( value == AREA_INSTANCE || value == AREA_DUTY )
            {
                send_to_char("Area who title only allowed in blueprints.\n\r", ch);
                return false;
            }

            dng->area_who = value;

            send_to_char("Area who title set.\n\r", ch);
            return true;
        }
    }

    send_to_char("Syntax:  areawho [title]\n\r"
                "Type '? areawho' for a list of who titles.\n\r", ch);
    return false;

}

DNGEDIT( dngedit_floors )
{
    DUNGEON_INDEX_DATA *dng;
    char arg[MIL];

    EDIT_DUNGEON(ch, dng);

    if( argument[0] == '\0' )
    {
        send_to_char("Syntax:  floors add <vnum|#vnum|area#vnum>\n\r", ch);
        send_to_char("         floors remove #\n\r", ch);
        send_to_char("         floors list\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if( !str_prefix(arg, "list") )
    {
        BUFFER *buffer = new_buf();

        if (!dngedit_buffer_floors(buffer, dng))
        {
            send_to_char("Floor list output exceeded buffer limits.\n\r", ch);
            free_buf(buffer);
            return false;
        }

        page_to_char(buffer->string, ch);
        free_buf(buffer);
        return false;
    }

    if( !str_prefix(arg, "add") )
    {
        WNUM bp_wnum;
        if (!parse_widevnum(argument, ch->in_room->area, &bp_wnum) || !bp_wnum.pArea) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        BLUEPRINT *bp = get_blueprint_for_area(bp_wnum.pArea, bp_wnum.vnum);

        if( !bp )
        {
            send_to_char("That blueprint does not exist.\n\r", ch);
            return false;
        }

        if (bp->mode == BLUEPRINT_MODE_STATIC)
        {
            if (list_size(bp->_static.entries) < 1)
            {
                send_to_char("WARNING: Blueprint is missing default entrance.\n\r", ch);
            }

            if (list_size(bp->_static.exits) < 1)
            {
                send_to_char("WARNING: Blueprint is missing default exit.\n\r", ch);
            }
        }

        /*
        // Disabling this type of check because there will be ways to exit a dungeon without using exits
        // This also wouldn't allow single level dungeons with this type of thing
        if( bp->mode == BLUEPRINT_MODE_STATIC )
        {


            if( bp->static_entry_section < 1 || bp->static_entry_link < 1 ||
                bp->static_exit_section < 1 || bp->static_exit_link < 1 )
            {
                send_to_char("Blueprint must have an entrance and exit specified.\n\r", ch);
                return false;
            }
        }
        else
        {
            send_to_char("Blueprint mode not supported yet.\n\r", ch);
            return false;
        }
        */

        list_appendlink(dng->floors, bp);
        send_to_char("Floor added.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "remove") || !str_prefix(arg, "delete") )
    {
        if( !is_number(argument) )
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        int index = atoi(argument);

        if( index < 1 || index > list_size(dng->floors) )
        {
            send_to_char("Index out of range.\n\r", ch);
            return false;
        }

        list_remnthlink(dng->floors, index, false);

        // TODO: Need to go through everything to make sure the floor is no longer referenced

        // Iterate over Level definitions to remove all references to this floor.

        send_to_char("Floor removed.\n\r", ch);
        return true;
    }

    dngedit_floors(ch, "");
    return false;
}

DNGEDIT( dngedit_channel )
{
    DUNGEON_INDEX_DATA *dng;
    char arg[MIL];
    char channel_id[MIL];

    EDIT_DUNGEON(ch, dng);

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
        if (!dngedit_render_channel_list(ch, dng))
            return false;

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

        iterator_start(&it, dng->channel_defs);
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

        list_appendlink(dng->channel_defs, str_dup(def->id));
        send_to_char("Channel definition attached to dungeon.\n\r", ch);
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

            if (index < 1 || index > list_size(dng->channel_defs))
            {
                send_to_char("Index out of range.\n\r", ch);
                return false;
            }

            id = (char *)list_nthdata(dng->channel_defs, index);
            if (id)
                free_string(id);
            list_remnthlink(dng->channel_defs, index, false);
            send_to_char("Channel definition removed from dungeon.\n\r", ch);
            return true;
        }
        else
        {
            ITERATOR it;
            char *id;
            int index = 1;

            iterator_start(&it, dng->channel_defs);
            while ((id = (char *)iterator_nextdata(&it)))
            {
                if (!str_cmp(id, argument))
                {
                    iterator_stop(&it);
                    free_string(id);
                    list_remnthlink(dng->channel_defs, index, false);
                    send_to_char("Channel definition removed from dungeon.\n\r", ch);
                    return true;
                }
                index++;
            }
            iterator_stop(&it);

            send_to_char("That channel id is not attached.\n\r", ch);
            return false;
        }
    }

    dngedit_channel(ch, "");
    return false;
}

DNGEDIT( dngedit_levels )
{
    char buf[MSL];
    DUNGEON_INDEX_DATA *dng;
    char arg[MIL];
    char arg2[MIL];
    int floor;
    
    EDIT_DUNGEON(ch, dng);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  levels add static <floor>\n\r", ch);
        send_to_char("         levels add weighted\n\r", ch);
        send_to_char("         levels add grouped\n\r", ch);
        send_to_char("         levels move <#> up|down|top|bottom|first|last\n\r", ch);
        send_to_char("         levels move <from> <to>\n\r", ch);
        send_to_char("         levels weight <#> list\n\r", ch);
        send_to_char("         levels weight <#> add <weight> <floor>\n\r", ch);
        send_to_char("         levels weight <#> set <#> <weight> <floor>\n\r", ch);
        send_to_char("         levels weight <#> remove <#>\n\r", ch);
        send_to_char("         levels group <#> add static <floor>\n\r", ch);
        send_to_char("         levels group <#> add weighted\n\r", ch);
        send_to_char("         levels group <#> move <#> up|down|top|bottom|first|last\n\r", ch);
        send_to_char("         levels group <#> move <from> <to>\n\r", ch);
        send_to_char("         levels group <#> weight <#> list\n\r", ch);
        send_to_char("         levels group <#> weight <#> add <weight> <floor>\n\r", ch);
        send_to_char("         levels group <#> weight <#> set <#> <weight> <floor>\n\r", ch);
        send_to_char("         levels group <#> weight <#> remove <#>\n\r", ch);
        send_to_char("         levels group <#> remove <#>\n\r", ch);
        send_to_char("         levels remove <#>\n\r", ch);
        send_to_char("         levels scripted <boolean>\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    // levels add static <floor>
    // levels add weighted
    // levels add grouped
    if (!str_prefix(arg, "add"))
    {
        if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
        {
            send_to_char("Please turn off scripted levels to edit levels manually.\n\r", ch);
            return false;
        }

        argument = one_argument(argument, arg2);

        if (!str_prefix(arg2, "static"))
        {
            if (argument[0] == '\0')
            {
                send_to_char("Syntax: levels add static <floor>\n\r", ch);
                return false;
            }

            if (!is_number(argument))
            {
                send_to_char("Please specify a number for a floor.", ch);
                return false;
            }

            if (list_size(dng->floors) < 1)
            {
                send_to_char("Please create floors first.\n\r", ch);
                return false;
            }

            floor = atoi(argument);
            if ( floor <= 0 || floor > list_size(dng->floors))
            {
                sprintf(buf, "Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
                send_to_char(buf, ch);
                return false;
            }

            DUNGEON_INDEX_LEVEL_DATA *level = new_dungeon_index_level();
            level->mode = LEVELMODE_STATIC;
            level->floor = floor;
            list_appendlink(dng->levels, level);

            sprintf(buf, "Static level %d added.\n\r", list_size(dng->levels));
            send_to_char(buf, ch);
            return true;
        }

        if (!str_prefix(arg2, "weighted"))
        {
            DUNGEON_INDEX_LEVEL_DATA *level = new_dungeon_index_level();
            level->mode = LEVELMODE_WEIGHTED;
            level->floor = 0;
            list_appendlink(dng->levels, level);

            sprintf(buf, "Weighted Random level %d added.\n\r", list_size(dng->levels));
            send_to_char(buf, ch);
            return true;
        }

        if (!str_prefix(arg2, "grouped"))
        {
            DUNGEON_INDEX_LEVEL_DATA *level = new_dungeon_index_level();
            level->mode = LEVELMODE_GROUP;
            level->floor = 0;
            list_appendlink(dng->levels, level);

            sprintf(buf, "Group level %d added.\n\r", list_size(dng->levels));
            send_to_char(buf, ch);
            return true;
        }

        send_to_char("Invalid type of level.  Please specify either static, weighted floors or grouped levels.\n\r", ch);
        return false;
    }

    // levels move <#> up|down|top|first|bottom|last
    // levels move <from> <to>
    if (!str_prefix(arg, "move"))
    {
        char arg3[MIL];
        if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
        {
            send_to_char("Please turn off scripted levels to edit levels manually.\n\r", ch);
            return false;
        }

        argument = one_argument(argument, arg2);
        if (!is_number(arg2))
        {
            send_to_char("Please specify a valid level number.", ch);
            return false;
        }

        int index = atoi(arg2);
        if (index <= 0 || index > list_size(dng->levels))
        {
            sprintf(buf, "Please specify a level number from 1 to %d.\n\r", list_size(dng->levels));
            send_to_char(buf, ch);
            return false;
        }

        int to_index = -1;
        argument = one_argument(argument, arg3);
        if (is_number(arg3))
        {
            // levels move <from> <to>
            to_index = atoi(arg3);
            if (to_index < 1 || to_index > list_size(dng->levels))
            {
                sprintf(buf, "Please specify a level number from 1 to %d.\n\r", list_size(dng->levels));
                send_to_char(buf, ch);
                return false;
            }
        }
        else if (!str_prefix(arg3, "up"))
        {
            if (index <= 1)
            {
                send_to_char("That level cannot move up any further.\n\r", ch);
                return false;
            }

            to_index = index - 1;
        }
        else if (!str_prefix(arg3, "down"))
        {
            if (index >= list_size(dng->levels))
            {
                send_to_char("That level cannot move down any further.\n\r", ch);
                return false;
            }

            to_index = index + 1;
        }
        else if (!str_prefix(arg3, "top") || !str_prefix(arg3, "first"))
        {
            if (index <= 1)
            {
                send_to_char("That level is already up as far as it can go.\n\r", ch);
                return false;
            }

            to_index = 1;
        }
        else if (!str_prefix(arg3, "bottom") || !str_prefix(arg3, "last"))
        {
            if (index >= list_size(dng->levels))
            {
                send_to_char("That level is already down ass far as it can go.\n\r", ch);
                return false;
            }

            to_index = list_size(dng->levels);
        }
        else
        {
            send_to_char("Syntax:  levels move <#> up|down|top|bottom|first|last\n\r", ch);
            send_to_char("         levels move <from> <to>\n\r", ch);
            return false;
        }
        
        if (index == to_index)
        {
            send_to_char("You shove the level as hard as possible, barely moving.\n\r", ch);
            return false;
        }

        list_movelink(dng->levels, index, to_index);
        send_to_char("Level moved.\n\r", ch);
        return true;
    }

    // levels weight <#> list
    // levels weight <#> add <weight> <floor>
    // levels weight <#> set <#> <weight> <floor>
    // levels weight <#> remove <#>
    if (!str_prefix(arg, "weight"))
    {
        if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
        {
            send_to_char("Please turn off scripted levels to edit levels manually.\n\r", ch);
            return false;
        }

        char arg3[MIL];
        //char arg4[MIL];
        int index;
        DUNGEON_INDEX_LEVEL_DATA *level;
        //DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;

        argument = one_argument(argument, arg2);
        if (!is_number(arg2))
        {
            send_to_char("Please specify a valid level number.", ch);
            return false;
        }

        index = atoi(arg2);
        if (index <= 0 || index > list_size(dng->levels))
        {
            sprintf(buf, "Please specify a level number from 1 to %d.\n\r", list_size(dng->levels));
            return false;
        }

        level = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, index);
        if (!IS_VALID(level))
        {
            send_to_char("Failed to retrieve level information.\n\r", ch);
            return false;
        }

        if (level->mode != LEVELMODE_WEIGHTED)
        {
            send_to_char("That level is not a weighted random level.  Please specify a weighted random level.\n\r", ch);
            return false;
        }

        argument = one_argument(argument, arg3);

        if (!str_prefix(arg3, "list"))
        {
            if (!dngedit_render_weighted_floor_list(ch, dng, level->weighted_floors,
                "Weighted level output exceeded buffer limits.\n\r"))
            {
                return false;
            }
        }
        else if (!str_prefix(arg3, "add"))
        {
            // levels weight <#> add <weight> <floor>
            char arg4[MIL];

            if (argument[0] == '\0')
            {
                send_to_char("Syntax:  levels weight <#> add <weight> <floor>\n\r", ch);
                return false;
            }

            argument = one_argument(argument, arg4);

            if (!is_number(arg4))
            {
                send_to_char("Please specify a positive number for the weight.\n\r", ch);
                return false;
            }

            int weight = atoi(arg4);
            if (weight < 1)
            {
                send_to_char("Please specify a positive number for the weight.\n\r", ch);
                return false;
            }

            if (argument[0] == '\0')
            {
                send_to_char("Syntax:  levels weight <#> add <weight> <floor>\n\r", ch);
                return false;
            }

            if (!is_number(argument))
            {
                sprintf(buf, "Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
                send_to_char(buf, ch);
                return false;
            }

            int floor = atoi(argument);
            if (floor < 1 || floor > list_size(dng->floors))
            {
                sprintf(buf, "Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
                send_to_char(buf, ch);
                return false;
            }

            DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted = new_weighted_random_floor();
            weighted->weight = weight;
            weighted->floor = floor;

            list_appendlink(level->weighted_floors, weighted);
            level->total_weight += weight;

            send_to_char("Weighted Random entry added.\n\r", ch);
            return true;
        }
        else if (!str_prefix(arg3, "set"))
        {
            // levels weight <#> set <#> <weight> <floor>
            char arg4[MIL];
            char arg5[MIL];

            if (argument[0] == '\0')
            {
                send_to_char("Syntax:  levels weight <#> set <#> <weight> <floor>\n\r", ch);
                return false;
            }

            argument = one_argument(argument, arg4);
            argument = one_argument(argument, arg5);

            if (!is_number(arg4))
            {
                sprintf(buf, "Please specify a weighted random entry number from 1 to %d.\n\r", list_size(level->weighted_floors));
                send_to_char(buf, ch);
                return false;
            }

            int index = atoi(arg4);
            if (index < 1 || index > list_size(level->weighted_floors))
            {
                sprintf(buf, "Please specify a weighted random entry number from 1 to %d.\n\r", list_size(level->weighted_floors));
                send_to_char(buf, ch);
                return false;
            }

            if (!is_number(arg5))
            {
                send_to_char("Please specify a positive number for the weight.\n\r", ch);
                return false;
            }

            int weight = atoi(arg5);
            if (weight < 1)
            {
                send_to_char("Please specify a positive number for the weight.\n\r", ch);
                return false;
            }

            if (argument[0] == '\0')
            {
                send_to_char("Syntax:  levels weight <#> set <#> <weight> <floor>\n\r", ch);
                return false;
            }

            if (!is_number(argument))
            {
                sprintf(buf, "Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
                send_to_char(buf, ch);
                return false;
            }

            int floor = atoi(argument);
            if (floor < 1 || floor > list_size(dng->floors))
            {
                sprintf(buf, "Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
                send_to_char(buf, ch);
                return false;
            }


            DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)list_nthdata(level->weighted_floors, index);

            level->total_weight -= weighted->weight;

            weighted->weight = weight;
            weighted->floor = floor;

            level->total_weight += weight;

            send_to_char("Weighted Random entry set.\n\r", ch);
            return true;
        }
        else if (!str_prefix(arg3, "remove"))
        {
            if (argument[0] == '\0')
            {
                send_to_char("Syntax:  levels weight <#> remove <#>\n\r", ch);
                return false;
            }

            if (!is_number(argument))
            {
                send_to_char("Please specify a number.\n\r", ch);
                return false;
            }

            int index = atoi(argument);
            if (index < 1 || index > list_size(level->weighted_floors))
            {
                sprintf(buf, "Invalid weight entry index.  Please specify a value from 1 to %d.\n\r", list_size(level->weighted_floors));
                send_to_char(buf, ch);
                return false;
            }

            DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)list_nthdata(level->weighted_floors, index);
            
            // Sanity check
            if (weighted)
            {
                level->total_weight -= weighted->weight;
            }

            list_remnthlink(level->weighted_floors, index, true);

            send_to_char("Weight Random entry removed.\n\r", ch);
            return false;
        }
    }

    // levels group <#> add static <floor>\n\r", ch);
    // levels group <#> add weighted\n\r", ch);
    // levels group <#> move <#> up|down|top|bottom|first|last\n\r", ch);
    // levels group <#> move <from> <to>\n\r", ch);
    // levels group <#> weight <#> list\n\r", ch);
    // levels group <#> weight <#> add <weight> <floor>\n\r", ch);
    // levels group <#> weight <#> set <#> <weight> <floor>\n\r", ch);
    // levels group <#> weight <#> remove <#>\n\r", ch);
    // levels group <#> remove <#>\n\r", ch);
    if (!str_prefix(arg, "group"))
    {
        if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
        {
            send_to_char("Please turn off scripted levels to edit levels manually.\n\r", ch);
            return false;
        }

        if (!is_number(arg2))
        {
            send_to_char("Syntax:  levels group {W<#>{x <command>\n\r", ch);
            send_to_char("         Please specify a number.\n\r", ch);
            return false;
        }

        int index = atoi(arg2);
        if (index < 1 || index > list_size(dng->levels))
        {
            send_to_char("Syntax:  levels group {W<#>{x <command>\n\r", ch);
            sprintf(buf, "         Please specify a group number from 1 to %d.\n\r", list_size(dng->levels));
            send_to_char(buf, ch);
            return false;
        }

        DUNGEON_INDEX_LEVEL_DATA *level = list_nthdata(dng->levels, index);
        if (!IS_VALID(level))
        {
            send_to_char("No such level exists.\n\r", ch);
            return false;
        }

        if (level->mode != LEVELMODE_GROUP)
        {
            sprintf(buf, "Level %d is not a group level set.\n\r", index);
            send_to_char(buf, ch);
            return false;
        }

        if (IS_NULLSTR(argument))
        {
            send_to_char("Syntax:  levels group <#> add static <floor>\n\r", ch);
            send_to_char("         levels group <#> add weighted\n\r", ch);
            send_to_char("         levels group <#> move <#> up|down|top|bottom|first|last\n\r", ch);
            send_to_char("         levels group <#> move <from> <to>\n\r", ch);
            send_to_char("         levels group <#> weight <#> list\n\r", ch);
            send_to_char("         levels group <#> weight <#> add <weight> <floor>\n\r", ch);
            send_to_char("         levels group <#> weight <#> set <#> <weight> <floor>\n\r", ch);
            send_to_char("         levels group <#> weight <#> remove <#>\n\r", ch);
            send_to_char("         levels group <#> remove <#>\n\r", ch);
            return false;
        }

        char arg3[MIL];

        argument = one_argument(argument, arg3);

        // levels group <#> add static <floor>
        // levels group <#> add weighted
        if (!str_prefix(arg3, "add"))
        {
            if (IS_NULLSTR(argument))
            {
                send_to_char("Syntax:  levels group <#> add static <floor>\n\r", ch);
                send_to_char("         levels group <#> add weighted\n\r", ch);
                return false;
            }

            char arg4[MIL];
            argument = one_argument(argument, arg4);

            if (!str_prefix(arg4, "static"))
            {
                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  levels group <#> add static <floor>\n\r", ch);
                    return false;
                }

                if (!is_number(argument))
                {
                    send_to_char("Syntax:  levels group <#> add static {W<floor>{x\n\r", ch);
                    send_to_char("         Please specify a number.\n\r", ch);
                    return false;
                }

                int floor = atoi(argument);
                if (floor < 1 || floor > list_size(dng->floors))
                {
                    send_to_char("Syntax:  levels group <#> add static {W<floor>{x\n\r", ch);
                    sprintf(buf, "         Please specify a floor number between 1 and %d\n\r", list_size(dng->floors));
                    send_to_char(buf, ch);
                    return false;
                }

                DUNGEON_INDEX_LEVEL_DATA *lvl = new_dungeon_index_level();
                lvl->mode = LEVELMODE_STATIC;
                lvl->floor = floor;

                list_appendlink(level->group, lvl);
                sprintf(buf, "Static level added to Group Level %d.\n\r", index);
                send_to_char(buf, ch);
                return true;
            }

            if (!str_prefix(arg4, "weighted"))
            {
                DUNGEON_INDEX_LEVEL_DATA *lvl = new_dungeon_index_level();
                lvl->mode = LEVELMODE_WEIGHTED;
                lvl->floor = 0;

                list_appendlink(level->group, lvl);
                sprintf(buf, "Weighted Random level added to Group Level %d.\n\r", index);
                send_to_char(buf, ch);
                return true;
            }

        }

        // levels group <#> move <#> up|down|top|first|bottom|last
        // levels group <#> move <from> <to>
        if (!str_prefix(arg3, "move"))
        {
            if (IS_NULLSTR(argument))
            {
                send_to_char("Syntax:  levels group <#> move <#> up|down|top|first|bottom|last\n\r", ch);
                send_to_char("         levels group <#> move <from> <to>\n\r", ch);
                return false;
            }

            char arg4[MIL];
            //char arg5[MIL];

            argument = one_argument(argument, arg4);
            if (!is_number(arg4))
            {
                send_to_char("Please specify a valid level number.", ch);
                return false;
            }

            int entry = atoi(arg4);
            if (entry <= 0 || entry > list_size(level->group))
            {
                send_to_char("Syntax:  levels group <#> move {W<#>{x up|down|top|first|bottom|last\n\r", ch);
                send_to_char("         levels group <#> move {W<from>{x <to>\n\r", ch);
                sprintf(buf, "         Please specify a level number from 1 to %d.\n\r", list_size(level->group));
                send_to_char(buf, ch);
                return false;
            }

            int to_entry = -1;
            if (is_number(argument))
            {
                // levels move <from> <to>
                to_entry = atoi(argument);
                if (to_entry <= 0 || to_entry > list_size(dng->levels))
                {
                    send_to_char("Syntax:  levels group <#> move {W<#>{x up|down|top|first|bottom|last\n\r", ch);
                    send_to_char("         levels group <#> move {W<from>{x <to>\n\r", ch);
                    sprintf(buf, "         Please specify a level number from 1 to %d.\n\r", list_size(level->group));
                    return false;
                }
            }
            else if (!str_prefix(argument, "up"))
            {
                if (entry <= 1)
                {
                    send_to_char("That level cannot move up any further.\n\r", ch);
                    return false;
                }

                to_entry = entry - 1;
            }
            else if (!str_prefix(argument, "down"))
            {
                if (entry >= list_size(level->group))
                {
                    send_to_char("That level cannot move down any further.\n\r", ch);
                    return false;
                }

                to_entry = entry + 1;
            }
            else if (!str_prefix(argument, "top") || !str_prefix(argument, "first"))
            {
                if (entry <= 1)
                {
                    send_to_char("That level is already up as far as it can go.\n\r", ch);
                    return false;
                }

                to_entry = 1;
            }
            else if (!str_prefix(argument, "bottom") || !str_prefix(argument, "last"))
            {
                if (entry >= list_size(level->group))
                {
                    send_to_char("That level is already down ass far as it can go.\n\r", ch);
                    return false;
                }

                to_entry = list_size(level->group);
            }
            else
            {
                send_to_char("Syntax:  levels group <#> move <#> up|down|top|bottom|first|last\n\r", ch);
                send_to_char("         levels group <#>s move <from> <to>\n\r", ch);
                return false;
            }
            
            if (entry == to_entry)
            {
                send_to_char("You shove the level as hard as possible, barely moving.\n\r", ch);
                return false;
            }

            list_movelink(level->group, entry, to_entry);
            send_to_char("Level moved.\n\r", ch);
            return true;
        }

        // levels group <#> weight <#> list
        // levels group <#> weight <#> add <weight> <floor>
        // levels group <#> weight <#> set <#> <weight> <floor>
        // levels group <#> weight <#> remove <#>
        if (!str_prefix(arg3, "weight"))
        {
            char arg4[MIL];
            char arg5[MIL];
            //char arg6[MIL];
            DUNGEON_INDEX_LEVEL_DATA *lvl;
            //DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;

            argument = one_argument(argument, arg4);
            if (!is_number(arg4))
            {
                send_to_char("Please specify a valid level number.", ch);
                return false;
            }

            int entry = atoi(arg4);
            if (entry < 1 || entry > list_size(level->group))
            {
                sprintf(buf, "Please specify a level number from 1 to %d.\n\r", list_size(level->group));
                return false;
            }

            lvl = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(level->group, entry);
            if (!IS_VALID(lvl))
            {
                send_to_char("Failed to retrieve level information.\n\r", ch);
                return false;
            }

            if (lvl->mode != LEVELMODE_WEIGHTED)
            {
                send_to_char("That level is not a weighted random level.  Please specify a weighted random level.\n\r", ch);
                return false;
            }

            argument = one_argument(argument, arg5);

            if (!str_prefix(arg5, "list"))
            {
                if (!dngedit_render_weighted_floor_list(ch, dng, lvl->weighted_floors,
                    "Weighted group level output exceeded buffer limits.\n\r"))
                {
                    return false;
                }
            }
            else if (!str_prefix(arg5, "add"))
            {
                // levels group <#> weight <#> add <weight> <floor>
                char arg6[MIL];

                if (argument[0] == '\0')
                {
                    send_to_char("Syntax:  levels group <#> weight <#> add <weight> <floor>\n\r", ch);
                    return false;
                }

                argument = one_argument(argument, arg6);

                if (!is_number(arg6))
                {
                    send_to_char("Syntax:  levels group <#> weight <#> add {W<weight>{x <floor>\n\r", ch);
                    send_to_char("         Please specify a positive number for the weight.\n\r", ch);
                    return false;
                }

                int weight = atoi(arg6);
                if (weight < 1)
                {
                    send_to_char("Syntax:  levels group <#> weight <#> add {W<weight>{x <floor>\n\r", ch);
                    send_to_char("         Please specify a positive number for the weight.\n\r", ch);
                    return false;
                }

                if (argument[0] == '\0')
                {
                    send_to_char("Syntax:  levels group <#> weight <#> add <weight> <floor>\n\r", ch);
                    return false;
                }

                if (!is_number(argument))
                {
                    send_to_char("Syntax:  levels group <#> weight <#> add <weight> {W<floor>{x\n\r", ch);
                    sprintf(buf, "         Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
                    send_to_char(buf, ch);
                    return false;
                }

                int floor = atoi(argument);
                if (floor < 1 || floor > list_size(dng->floors))
                {
                    send_to_char("Syntax:  levels group <#> weight <#> add <weight> {W<floor>{x\n\r", ch);
                    sprintf(buf, "         Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
                    send_to_char(buf, ch);
                    return false;
                }

                DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted = new_weighted_random_floor();
                weighted->weight = weight;
                weighted->floor = floor;

                list_appendlink(lvl->weighted_floors, weighted);
                lvl->total_weight += weight;

                send_to_char("Weighted Random entry added.\n\r", ch);
                return true;
            }
            else if (!str_prefix(arg5, "set"))
            {
                // levels weight <#> set <#> <weight> <floor>
                char arg6[MIL];
                char arg7[MIL];

                if (argument[0] == '\0')
                {
                    send_to_char("Syntax:  levels group <#> weight <#> set <#> {W<weight> <floor>{x\n\r", ch);
                    return false;
                }

                argument = one_argument(argument, arg6);
                argument = one_argument(argument, arg7);

                if (!is_number(arg6))
                {
                    send_to_char("Syntax:  levels group <#> weight <#> set {W<#>{x <weight> <floor>\n\r", ch);
                    sprintf(buf, "         Please specify a weighted random entry number from 1 to %d.\n\r", list_size(lvl->weighted_floors));
                    send_to_char(buf, ch);
                    return false;
                }

                int index = atoi(arg6);
                if (index < 1 || index > list_size(lvl->weighted_floors))
                {
                    send_to_char("Syntax:  levels group <#> weight <#> set {W<#>{x <weight> <floor>\n\r", ch);
                    sprintf(buf, "         Please specify a weighted random entry number from 1 to %d.\n\r", list_size(lvl->weighted_floors));
                    send_to_char(buf, ch);
                    return false;
                }

                if (!is_number(arg7))
                {
                    send_to_char("Syntax:  levels group <#> weight <#> set <#> {W<weight>{x <floor>\n\r", ch);
                    send_to_char("         Please specify a positive number for the weight.\n\r", ch);
                    return false;
                }

                int weight = atoi(arg7);
                if (weight < 1)
                {
                    send_to_char("Syntax:  levels group <#> weight <#> set <#> {W<weight>{x <floor>\n\r", ch);
                    send_to_char("         Please specify a positive number for the weight.\n\r", ch);
                    return false;
                }

                if (argument[0] == '\0')
                {
                    send_to_char("Syntax:  levels group <#> weight <#> set <#> <weight> {W<floor>{x\n\r", ch);
                    return false;
                }

                if (!is_number(argument))
                {
                    send_to_char("Syntax:  levels group <#> weight <#> set <#> <weight> {W<floor>{x\n\r", ch);
                    sprintf(buf, "         Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
                    send_to_char(buf, ch);
                    return false;
                }

                int floor = atoi(argument);
                if (floor < 1 || floor > list_size(dng->floors))
                {
                    send_to_char("Syntax:  levels group <#> weight <#> set <#> <weight> {W<floor>{x\n\r", ch);
                    sprintf(buf, "         Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
                    send_to_char(buf, ch);
                    return false;
                }


                DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)list_nthdata(level->weighted_floors, index);

                lvl->total_weight -= weighted->weight;

                weighted->weight = weight;
                weighted->floor = floor;

                lvl->total_weight += weight;

                send_to_char("Weighted Random entry set.\n\r", ch);
                return true;
            }
            else if (!str_prefix(arg5, "remove"))
            {
                if (argument[0] == '\0')
                {
                    send_to_char("Syntax:  levels group <#> weight <#> remove {W<#>{x\n\r", ch);
                    return false;
                }

                if (!is_number(argument))
                {
                    send_to_char("Syntax:  levels group <#> weight <#> remove {W<#>{x\n\r", ch);
                    send_to_char("         Please specify a number.\n\r", ch);
                    return false;
                }

                int index = atoi(argument);
                if (index < 1 || index > list_size(lvl->weighted_floors))
                {
                    sprintf(buf, "Invalid weight entry index.  Please specify a value from 1 to %d.\n\r", list_size(lvl->weighted_floors));
                    send_to_char(buf, ch);
                    return false;
                }

                DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)list_nthdata(lvl->weighted_floors, index);
                
                // Sanity check
                if (weighted)
                {
                    lvl->total_weight -= weighted->weight;
                }

                list_remnthlink(lvl->weighted_floors, index, true);

                send_to_char("Weight Random entry removed.\n\r", ch);
                return false;
            }
        }

        // levels group <#> remove <#>
        if (!str_prefix(arg3, "remove"))
        {
            if (IS_NULLSTR(argument))
            {
                send_to_char("Syntax:  levels group <#> remove {W<#>{x\n\r", ch);
                send_to_char("         Please give a number.\n\r", ch);
                return false;
            }

            if (!is_number(argument))
            {
                send_to_char("Syntax:  levels group <#> remove {W<#>{x\n\r", ch);
                send_to_char("         Please give a number.\n\r", ch);
                return false;
            }

            int index = atoi(argument);
            if (index < 1 || index > list_size(level->group))
            {
                send_to_char("Syntax:  levels group <#> remove {W<#>{x\n\r", ch);
                sprintf(buf, "         Please give a number between 1 and %d\n\r", list_size(level->group));
                send_to_char(buf, ch);
                return false;
            }
        
            list_remnthlink(level->group, index, true);
            send_to_char("Level removed.\n\r", ch);
            return true;
        }

        dngedit_levels(ch, "group");
    }

    if (!str_prefix(arg, "scripted"))
    {
        if (IS_NULLSTR(argument))
        {
            send_to_char("Syntax:  levels scripted <boolean>\n\r", ch);
            send_to_char("Please specify a boolean value: true/yes/on or false/no/off\n\r\n\r", ch);

            if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
                send_to_char("Dungeon uses scripted levels.  Please supply a DUNGEON_SCHEMATIC trigger on the dungeon index.\n\r", ch);
            else
                send_to_char("Dungeon uses manually defined levels.\n\r", ch);
            return false;
        }

        if (!str_prefix(argument, "true") || !str_prefix(argument, "yes") || !str_prefix(argument, "on"))
        {
            if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
            {
                send_to_char("Dungeon already uses scripted level design.\n\r", ch);
                return false;
            }

            // Delete all manual level information
            list_clear(dng->levels);
            list_clear(dng->special_rooms);
            list_clear(dng->special_exits);

            SET_BIT(dng->flags, DUNGEON_SCRIPTED_LEVELS);
            send_to_char("Dungeon now uses scripted level design.  Please make sure a {WDUNGEON_SCHEMATIC{x trigger has been added to the dungeon index.\n\r", ch);
            return true;
        }

        if (!str_prefix(argument, "false") || !str_prefix(argument, "no") || !str_prefix(argument, "off"))
        {
            if (!IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
            {
                send_to_char("Dungeon does not use scripted level design.\n\r", ch);
                return false;
            }

            REMOVE_BIT(dng->flags, DUNGEON_SCRIPTED_LEVELS);
            send_to_char("Dungeon set to manual level design.\n\r", ch);
            return true;
        }

        send_to_char("Syntax:  levels scripted <boolean>\n\r", ch);
        send_to_char("Please specify a boolean value: true/yes/on or false/no/off", ch);
        return false;
    }

    // levels remove <#>
    if (!str_prefix(arg, "remove"))
    {
        if (IS_NULLSTR(argument))
        {
            send_to_char("Syntax:  levels remove {W<#>{x\n\r", ch);
            send_to_char("         Please give a number.\n\r", ch);
            return false;
        }

        if (!is_number(argument))
        {
            send_to_char("Syntax:  levels remove {W<#>{x\n\r", ch);
            send_to_char("         Please give a number.\n\r", ch);
            return false;
        }

        int index = atoi(argument);
        if (index < 1 || index > list_size(dng->levels))
        {
            send_to_char("Syntax:  levels remove {W<#>{x\n\r", ch);
            sprintf(buf, "         Please give a number between 1 and %d\n\r", list_size(dng->levels));
            send_to_char(buf, ch);
            return false;
        }

        DUNGEON_INDEX_LEVEL_DATA *level = list_nthdata(dng->levels, index);
        if (!IS_VALID(level))
        {
            send_to_char("Failed to retrieve level information.\n\r", ch);
            return false;
        }
    
        list_remnthlink(dng->levels, index, true);
        send_to_char("Level removed.\n\r", ch);
        return true;
    }

    dngedit_levels(ch, "");
    return false;
}

DNGEDIT( dngedit_entry )
{
    DUNGEON_INDEX_DATA *dng;

    EDIT_DUNGEON(ch, dng);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  entry <vnum|#vnum|area#vnum>\n\r", ch);
        return false;
    }

    WNUM entry_wnum;
    if (!parse_widevnum(argument, ch->in_room->area, &entry_wnum) || !entry_wnum.pArea) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    ROOM_INDEX_DATA *room = get_room_index(entry_wnum.pArea, entry_wnum.vnum);
    if( !room )
    {
        send_to_char("That room does not exist.\n\r", ch);
        return false;
    }

    /* Store in WNUM_LOAD format */
    dng->entry_ref.load.vnum = entry_wnum.vnum;
    dng->entry_ref.load.auid = entry_wnum.pArea->uid;
    /* Also set the resolved pointer */
    dng->entry_room = room;
    
    send_to_char("Entry room changed.\n\r", ch);
    return true;
}

DNGEDIT( dngedit_exit )
{
    DUNGEON_INDEX_DATA *dng;

    EDIT_DUNGEON(ch, dng);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  exit <vnum|#vnum|area#vnum>\n\r", ch);
        return false;
    }

    WNUM exit_wnum;
    if (!parse_widevnum(argument, ch->in_room->area, &exit_wnum) || !exit_wnum.pArea) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    ROOM_INDEX_DATA *room = get_room_index(exit_wnum.pArea, exit_wnum.vnum);
    if( !room )
    {
        send_to_char("That room does not exist.\n\r", ch);
        return false;
    }

    /* Store in WNUM_LOAD format */
    dng->exit_ref.load.vnum = exit_wnum.vnum;
    dng->exit_ref.load.auid = exit_wnum.pArea->uid;
    /* Also set the resolved pointer */
    dng->exit_room = room;
    
    send_to_char("Exit room changed.\n\r", ch);
    return true;
}

DNGEDIT( dngedit_flags )
{
    DUNGEON_INDEX_DATA *dng;
    int value;

    EDIT_DUNGEON(ch, dng);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  flags <flags>\n\r", ch);
        send_to_char("'? dungeon' for list of flags.\n\r", ch);
        return false;
    }

    if( (value = flag_value(dungeon_flags, argument)) != NO_FLAG )
    {
        dng->flags ^= value;
        send_to_char("Dungeon flags changed.\n\r", ch);
        return true;
    }

    dngedit_flags(ch, "");
    return false;

}

DNGEDIT( dngedit_zoneout )
{
    DUNGEON_INDEX_DATA *dng;
    EDIT_DUNGEON(ch, dng);

    return olc_cmd_string(ch, argument, "ZoneOut", NULL, &dng->zone_out,
        OLC_STR_DEFAULT, NULL, NULL);
}

DNGEDIT( dngedit_portalout )
{
    DUNGEON_INDEX_DATA *dng;
    EDIT_DUNGEON(ch, dng);

    return olc_cmd_string(ch, argument, "PortalOut", NULL,
        &dng->zone_out_portal, OLC_STR_DEFAULT, NULL, NULL);
}

DNGEDIT( dngedit_mountout )
{
    DUNGEON_INDEX_DATA *dng;
    EDIT_DUNGEON(ch, dng);

    return olc_cmd_string(ch, argument, "MountOut", NULL,
        &dng->zone_out_mount, OLC_STR_DEFAULT, NULL, NULL);
}

static int get_blueprint_entrance_count(BLUEPRINT *bp)
{
    switch(bp->mode)
    {
    case BLUEPRINT_MODE_STATIC:
        return list_size(bp->_static.entries);

    default:
        return 0;
    }
}

static int get_blueprint_exit_count(BLUEPRINT *bp)
{
    switch(bp->mode)
    {
    case BLUEPRINT_MODE_STATIC:
        return list_size(bp->_static.exits);

    default:
        return 0;
    }
}

int get_dungeon_index_level_special_entrances(DUNGEON_INDEX_DATA *dng, DUNGEON_INDEX_LEVEL_DATA *level)
{
    BLUEPRINT *bp;
    switch(level->mode)
    {
    case LEVELMODE_STATIC:
        bp = (BLUEPRINT *)list_nthdata(dng->floors, level->floor);

        return get_blueprint_entrance_count(bp);

    case LEVELMODE_WEIGHTED:
        {
            int min_count = -1;

            ITERATOR wit;
            DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;
            iterator_start(&wit, level->weighted_floors);
            while( (weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)iterator_nextdata(&wit)))
            {
                bp = (BLUEPRINT *)list_nthdata(dng->floors, weighted->floor);

                int count = get_blueprint_entrance_count(bp);

                if (min_count < 0 || count < min_count)
                    min_count = count;
            }
            iterator_stop(&wit);
            return UMAX(0, min_count);
        }

    case LEVELMODE_GROUP:
        {
            int min_count = -1;

            ITERATOR git;
            DUNGEON_INDEX_LEVEL_DATA *glevel;
            iterator_start(&git, level->group);
            while( (glevel = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&git)))
            {
                int count = get_dungeon_index_level_special_entrances(dng, glevel);

                if (min_count < 0 || count < min_count)
                    min_count = count;
            }
            iterator_stop(&git);
            return UMAX(0, min_count);
        }
    }

    return 0;
}


int get_dungeon_index_level_special_exits(DUNGEON_INDEX_DATA *dng, DUNGEON_INDEX_LEVEL_DATA *level)
{
    BLUEPRINT *bp;
    switch(level->mode)
    {
    case LEVELMODE_STATIC:
        bp = (BLUEPRINT *)list_nthdata(dng->floors, level->floor);

        return get_blueprint_exit_count(bp);

    case LEVELMODE_WEIGHTED:
        {
            int min_count = -1;

            ITERATOR wit;
            DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;
            iterator_start(&wit, level->weighted_floors);
            while( (weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)iterator_nextdata(&wit)))
            {
                bp = (BLUEPRINT *)list_nthdata(dng->floors, weighted->floor);

                int count = get_blueprint_exit_count(bp);

                if (min_count < 0 || count < min_count)
                    min_count = count;
            }
            iterator_stop(&wit);
            return UMAX(0, min_count);
        }

    case LEVELMODE_GROUP:
        {
            int min_count = -1;

            ITERATOR git;
            DUNGEON_INDEX_LEVEL_DATA *glevel;
            iterator_start(&git, level->group);
            while( (glevel = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&git)))
            {
                int count = get_dungeon_index_level_special_exits(dng, glevel);

                if (min_count < 0 || count < min_count)
                    min_count = count;
            }
            iterator_stop(&git);
            return UMAX(0, min_count);
        }
    }

    return 0;
}

static void add_dungeon_index_weighted_exit_data(LLIST *list, int weight, int level, int door)
{
    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *weighted = new_weighted_random_exit();

    weighted->weight = weight;
    weighted->level = level;
    weighted->door = door;

    list_appendlink(list, weighted);
}

DNGEDIT( dngedit_special )
{
    DUNGEON_INDEX_DATA *dng;
    char arg[MIL];
    char arg2[MIL];
    char arg3[MIL];
    char arg4[MIL];
    char arg5[MIL];
    char arg6[MIL];
    char arg7[MIL];

    EDIT_DUNGEON(ch, dng);

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char("Syntax:  special room list\n\r", ch);
        send_to_char("         special room add <level> <special room> <name>\n\r", ch);
        send_to_char("         special room # remove\n\r", ch);
        send_to_char("         special room # name <name>\n\r", ch);
        send_to_char("         special room # level <level>\n\r", ch);
        send_to_char("         special room # room <special room>\n\r", ch);
        send_to_char("         special exit list\n\r", ch);
        send_to_char("         special exit add static <from-level> <from-exit> <to-level> <to-entrance>\n\r", ch);
        send_to_char("         special exit add source <to-level> <to-entrance>\n\r", ch);
        send_to_char("         special exit add destination <from-level> <from-exit>\n\r", ch);
        send_to_char("         special exit add weighted\n\r", ch);
        send_to_char("         special exit add group\n\r", ch);
        send_to_char("         special exit from # list\n\r", ch);
        send_to_char("         special exit from # add <weight> <from-level> <from-exit>\n\r", ch);
        send_to_char("         special exit from # set # <weight> <from-level> <from-exit>\n\r", ch);
        send_to_char("         special exit from # remove #\n\r", ch);
        send_to_char("         special exit to # list\n\r", ch);
        send_to_char("         special exit to # add <weight> <to-level> <to-entrance>\n\r", ch);
        send_to_char("         special exit to # set # <weight> <to-level> <to-entrance>\n\r", ch);
        send_to_char("         special exit to # remove #\n\r", ch);
        send_to_char("         special exit list\n\r", ch);
        send_to_char("         special exit group # add static <from-level> <from-exit> <to-level> <to-entrance>\n\r", ch);
        send_to_char("         special exit group # add source <to-level> <to-entrance>\n\r", ch);
        send_to_char("         special exit group # add destination <from-level> <from-exit>\n\r", ch);
        send_to_char("         special exit group # add weighted\n\r", ch);
        send_to_char("         special exit group # add group\n\r", ch);
        send_to_char("         special exit group # from # list\n\r", ch);
        send_to_char("         special exit group # from # add <weight> <from-level> <from-exit>\n\r", ch);
        send_to_char("         special exit group # from # set # <weight> <from-level> <from-exit>\n\r", ch);
        send_to_char("         special exit group # from # remove #\n\r", ch);
        send_to_char("         special exit group # to # list\n\r", ch);
        send_to_char("         special exit group # to # add <weight> <to-level> <to-entrance>\n\r", ch);
        send_to_char("         special exit group # to # set # <weight> <to-level> <to-entrance>\n\r", ch);
        send_to_char("         special exit group # to # remove #\n\r", ch);
        send_to_char("         special exit group # remove #\n\r", ch);
        send_to_char("         special exit remove #\n\r", ch);
        return false;
    }

    if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
    {
        send_to_char("Please turn off scripted levels to edit special rooms/exits manually.\n\r", ch);
        return false;
    }

    if (!str_prefix(arg, "room"))
    {
        argument = one_argument(argument, arg2);

        if( !str_prefix(arg2, "list") )
        {
            if( list_size(dng->special_rooms) > 0 )
            {
                if (!dngedit_render_special_room_list(ch, dng,
                    "Special room output exceeded buffer limits.\n\r"))
                {
                    return false;
                }
            }
            else
            {
                send_to_char("Dungeon has no special rooms defined.\n\r", ch);
            }

            return false;
        }

        if( is_number(arg2) )
        {
            int index = atoi(arg2);

            DUNGEON_INDEX_SPECIAL_ROOM *special = list_nthdata(dng->special_rooms, index);

            if( !IS_VALID(special) )
            {
                send_to_char("No such special room.\n\r", ch);
                return false;
            }

            argument = one_argument(argument, arg3);
            if( arg3[0] == '\0' )
            {
                dngedit_special(ch, "");
                return false;
            }

            if( !str_prefix(arg3, "remove") || !str_prefix(arg3, "delete") )
            {
                list_remnthlink(dng->special_rooms, index, true);

                send_to_char("Special room deleted.\n\r", ch);
                return true;
            }

            if( !str_prefix(arg3, "level") )
            {
                argument = one_argument(argument, arg4);
                if( !is_number(arg4) )
                {
                    send_to_char("That is not a number.\n\r", ch);
                    return false;
                }

                int level = atoi(arg4);
                if( level < 1 || level > list_size(dng->levels) )
                {
                    send_to_char("Level out of range.\n\r", ch);
                    return false;
                }

                special->level = level;
                special->room = 0;

                send_to_char("Level changed.\n\r", ch);
                return true;
            }

            if( !str_prefix(arg3, "name") )
            {
                argument = one_argument(argument, arg4);

                if( IS_NULLSTR(arg4) )
                {
                    send_to_char("Syntax:  special room # name [name]\n\r", ch);
                    return false;
                }


                smash_tilde(arg4);
                free_string(special->name);
                special->name = str_dup(arg4);

                send_to_char("Special room name changed.\n\r", ch);
                return true;
            }

            if( !str_prefix(arg3, "room") )
            {
                argument = one_argument(argument, arg4);
                if( !is_number(arg4))
                {
                    send_to_char("That is not a number.\n\r", ch);
                    return false;
                }

                int room = atol(arg4);

                special->room = room;

                send_to_char("Special room changed.\n\r", ch);
                return true;
            }
        }
        else if( !str_prefix(arg2, "add") )
        {
            argument = one_argument(argument, arg3);
            argument = one_argument(argument, arg4);

            if( argument[0] == '\0' )
            {
                send_to_char("Syntax:  special room add [level] [special room] [name]\n\r", ch);
                return false;
            }

            if( !is_number(arg3) || !is_number(arg4) )
            {
                send_to_char("That is not a number.\n\r", ch);
                return false;
            }

            int level = atoi(arg3);
            int room = atoi(arg4);

            if( level < 1 || level > list_size(dng->levels) )
            {
                send_to_char("Level out of range.\n\r", ch);
                return false;
            }

            char name[MIL+1];
            strncpy(name, argument, MIL);
            name[MIL] = '\0';
            smash_tilde(name);

            DUNGEON_INDEX_SPECIAL_ROOM *special = new_dungeon_index_special_room();

            free_string(special->name);
            special->name = str_dup(name);
            special->level = level;
            special->room = room;

            list_appendlink(dng->special_rooms, special);

            send_to_char("Special Room added.\n\r", ch);
            return true;
        }


        send_to_char("Syntax:  special {Wroom{x list\n\r", ch);
        send_to_char("         special {Wroom{x add <level> <special room> <name>\n\r", ch);
        send_to_char("         special {Wroom{x # remove\n\r", ch);
        send_to_char("         special {Wroom{x # name <name>\n\r", ch);
        send_to_char("         special {Wroom{x # level <level>\n\r", ch);
        send_to_char("         special {Wroom{x # room <special room>\n\r", ch);
        return false;
    }

    if (!str_prefix(arg, "exit"))
    {
        argument = one_argument(argument, arg2);

        if (!str_prefix(arg2, "list"))
        {
            if (list_size(dng->special_exits) > 0)
            {
                BUFFER *buffer = new_buf();

                if (!dngedit_buffer_special_exits(buffer, dng))
                {
                    send_to_char("Special exit output exceeded buffer limits.\n\r", ch);
                    free_buf(buffer);
                    return false;
                }

                if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH)
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
                send_to_char("Dungeon has no special exits defined.\n\r", ch);
            return false;
        }

        if (!str_prefix(arg2, "add"))
        {
            char buf[MSL];
            if (list_size(dng->levels) < 1)
            {
                send_to_char("Please add level definitions before adding special exits.\n\r", ch);
                return false;
            }

            if (argument[0] == '\0')
            {
                send_to_char("Syntax:  special exit {Wadd{x static <from-level> <from-exit> <to-level> <to-entrance>\n\r", ch);
                send_to_char("         special exit {Wadd{x source <to-level> <to-entrance>\n\r", ch);
                send_to_char("         special exit {Wadd{x destination <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit {Wadd{x weighted\n\r", ch);
                send_to_char("         special exit {Wadd{x group\n\r", ch);
                return false;
            }

            argument = one_argument(argument, arg3);

            // Add Static Special Exit
            // special exit add static <from-level> <from-exit> <to-level> <to-entrance>
            if (!str_prefix(arg3, "static"))
            {
                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit add static {R<from-level>{x <from-exit> <to-level> <to-entrance>\n\r", ch);
                    sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                argument = one_argument(argument, arg4);

                if (!is_number(arg4))
                {
                    send_to_char("Syntax:  special exit add static {R<from-level>{x <from-exit> <to-level> <to-entrance>\n\r", ch);
                    sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                int from_level = atoi(arg4);
                if (from_level < 1 || from_level > list_size(dng->levels))
                {
                    send_to_char("Syntax:  special exit add static {R<from-level>{x <from-exit> <to-level> <to-entrance>\n\r", ch);
                    sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                DUNGEON_INDEX_LEVEL_DATA *from_level_data = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, from_level);

                int from_exits = get_dungeon_index_level_special_exits(dng, from_level_data);

                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit add static <from-level> {R<from-exit>{x <to-level> <to-entrance>\n\r", ch);
                    sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
                    send_to_char(buf, ch);
                    return false;
                }

                argument = one_argument(argument, arg5);

                if (!is_number(arg5))
                {
                    send_to_char("Syntax:  special exit add static <from-level> {R<from-exit>{x <to-level> <to-entrance>\n\r", ch);
                    sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
                    send_to_char(buf, ch);
                    return false;
                }

                int from_exit = atoi(arg5);
                if (from_exit < 1 || from_exit > from_exits)
                {
                    send_to_char("Syntax:  special exit add static <from-level> {R<from-exit>{x <to-level> <to-entrance>\n\r", ch);
                    sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
                    send_to_char(buf, ch);
                    return false;
                }

                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit add static <from-level> <from-exit> {R<to-level>{x <to-entrance>\n\r", ch);
                    sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                argument = one_argument(argument, arg6);

                if (!is_number(arg6))
                {
                    send_to_char("Syntax:  special exit add static <from-level> <from-exit> {R<to-level>{x <to-entrance>\n\r", ch);
                    sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                int to_level = atoi(arg6);
                if (to_level < 1 || to_level > list_size(dng->levels))
                {
                    send_to_char("Syntax:  special exit add static <from-level> <from-exit> {R<to-level>{x <to-entrance>\n\r", ch);
                    sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                DUNGEON_INDEX_LEVEL_DATA *to_level_data = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, to_level);

                int to_entries = get_dungeon_index_level_special_entrances(dng, to_level_data);

                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit add static <from-level> <from-exit> <to-level> {R<to-entrance>{x\n\r", ch);
                    sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
                    send_to_char(buf, ch);
                    return false;
                }

                if (!is_number(argument))
                {
                    send_to_char("Syntax:  special exit add static <from-level> <from-exit> <to-level> {R<to-entrance>{x\n\r", ch);
                    sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
                    send_to_char(buf, ch);
                    return false;
                }

                int to_entry = atoi(argument);
                if (to_entry < 1 || to_entry > to_entries)
                {
                    send_to_char("Syntax:  special exit add static <from-level> <from-exit> <to-level> {R<to-entrance>{x\n\r", ch);
                    sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
                    send_to_char(buf, ch);
                    return false;
                }

                if (from_level == to_level && from_exit == to_entry)
                {
                    send_to_char("Both exits are the same.  Unable to connect them.\n\r", ch);
                    return false;
                }

                DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
                ex->mode = EXITMODE_STATIC;
                
                add_dungeon_index_weighted_exit_data(ex->from, 1, from_level, from_exit);
                add_dungeon_index_weighted_exit_data(ex->to, 1, to_level, to_entry);

                list_appendlink(dng->special_exits, ex);

                send_to_char("Static special exit added.\n\r", ch);
                return true;
            }

            // Add Source Special Exit
            // special exit add source <to-level> <to-entrance>
            if (!str_prefix(arg3, "source"))
            {
                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit add source {R<to-level>{x <to-entrance>\n\r", ch);
                    sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                argument = one_argument(argument, arg6);

                if (!is_number(arg6))
                {
                    send_to_char("Syntax:  special exit add source {R<to-level>{x <to-entrance>\n\r", ch);
                    sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                int to_level = atoi(arg6);
                if (to_level < 1 || to_level > list_size(dng->levels))
                {
                    send_to_char("Syntax:  special exit add source {R<to-level>{x <to-entrance>\n\r", ch);
                    sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                DUNGEON_INDEX_LEVEL_DATA *to_level_data = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, to_level);

                int to_entries = get_dungeon_index_level_special_entrances(dng, to_level_data);

                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit add source <to-level> {R<to-entrance>{x\n\r", ch);
                    sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
                    send_to_char(buf, ch);
                    return false;
                }

                if (!is_number(argument))
                {
                    send_to_char("Syntax:  special exit add source <to-level> {R<to-entrance>{x\n\r", ch);
                    sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
                    send_to_char(buf, ch);
                    return false;
                }

                int to_entry = atoi(argument);
                if (to_entry < 1 || to_entry > to_entries)
                {
                    send_to_char("Syntax:  special exit add source <to-level> {R<to-entrance>{x\n\r", ch);
                    sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
                    send_to_char(buf, ch);
                    return false;
                }

                DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
                ex->mode = EXITMODE_WEIGHTED_SOURCE;
                
                add_dungeon_index_weighted_exit_data(ex->to, 1, to_level, to_entry);

                list_appendlink(dng->special_exits, ex);

                int index = list_size(dng->special_exits);

                send_to_char("Source special exit added.\n\r", ch);
                sprintf(buf, "Please add source exits using {Wspecial exit from {Y%d{W add ...{x\n\r", index);
                send_to_char(buf, ch);
                return true;
            }

            if (!str_prefix(arg3, "destination"))
            {
                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit add destination {R<from-level>{x <from-exit>\n\r", ch);
                    sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                argument = one_argument(argument, arg4);

                if (!is_number(arg4))
                {
                    send_to_char("Syntax:  special exit add destination {R<from-level>{x <from-exit>\n\r", ch);
                    sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                int from_level = atoi(arg4);
                if (from_level < 1 || from_level > list_size(dng->levels))
                {
                    send_to_char("Syntax:  special exit add destination {R<from-level>{x <from-exit>\n\r", ch);
                    sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                DUNGEON_INDEX_LEVEL_DATA *from_level_data = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, from_level);

                int from_exits = get_dungeon_index_level_special_exits(dng, from_level_data);

                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit add destination <from-level> {R<from-exit>{x\n\r", ch);
                    sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
                    send_to_char(buf, ch);
                    return false;
                }

                if (!is_number(argument))
                {
                    send_to_char("Syntax:  special exit add destination <from-level> {R<from-exit>{x\n\r", ch);
                    sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
                    send_to_char(buf, ch);
                    return false;
                }

                int from_exit = atoi(argument);
                if (from_exit < 1 || from_exit > from_exits)
                {
                    send_to_char("Syntax:  special exit add destination <from-level> {R<from-exit>{x\n\r", ch);
                    sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
                    send_to_char(buf, ch);
                    return false;
                }

                DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
                ex->mode = EXITMODE_WEIGHTED_DEST;
                
                add_dungeon_index_weighted_exit_data(ex->from, 1, from_level, from_exit);

                list_appendlink(dng->special_exits, ex);

                int index = list_size(dng->special_exits);

                send_to_char("Destination special exit added.\n\r", ch);
                sprintf(buf, "Please add target entrances using {Wspecial exit to {Y%d{W add ...{x\n\r", index);
                send_to_char(buf, ch);
                return true;
            }

            if (!str_prefix(arg3, "weighted"))
            {
                if (!IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit add weighted\n\r", ch);
                    return false;
                }
                
                DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
                ex->mode = EXITMODE_WEIGHTED;

                list_appendlink(dng->special_exits, ex);

                int index = list_size(dng->special_exits);

                send_to_char("Weighted special exit added.\n\r", ch);
                sprintf(buf, "Please add source exits using {Wspecial exit from {Y%d{W add ...{x\n\r", index);
                send_to_char(buf, ch);
                sprintf(buf, "Please add target entrances using {Wspecial exit to {Y%d{W add ...{x\n\r", index);
                send_to_char(buf, ch);
                return true;
            }

            if (!str_prefix(arg3, "group"))
            {
                if (!IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit add group\n\r", ch);
                    return false;
                }
                
                DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
                ex->mode = EXITMODE_GROUP;
                
                list_appendlink(dng->special_exits, ex);

                int index = list_size(dng->special_exits);

                send_to_char("Group special exit added.\n\r", ch);
                sprintf(buf, "Please add exit definitions using {Wspecial exit group {Y%d{W add ...{x\n\r", index);
                send_to_char(buf, ch);
                return true;
            }

            send_to_char("Syntax:  special exit add {Wstatic{x <from-level> <from-exit> <to-level> <to-entrance>\n\r", ch);
            send_to_char("         special exit add {Wsource{x <to-level> <to-entrance>\n\r", ch);
            send_to_char("         special exit add {Wdestination{x <from-level> <from-exit>\n\r", ch);
            send_to_char("         special exit add {Wweighted{x\n\r", ch);
            send_to_char("         special exit add {Wgroup{x\n\r", ch);
            return false;
        }

        if (!str_prefix(arg2, "group"))
        {
            char buf[MSL];
            char argg[MIL];			// used for group #
            char argg2[MIL];		// used for subcommand
            if (IS_NULLSTR(argument))
            {
                send_to_char("Syntax:  special exit group {R#{x add static <from-level> <from-exit> <to-level> <to-entrance>\n\r", ch);
                send_to_char("         special exit group {R#{x add source <to-level> <to-entrance>\n\r", ch);
                send_to_char("         special exit group {R#{x add destination <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit group {R#{x add weighted\n\r", ch);
                send_to_char("         special exit group {R#{x from # list\n\r", ch);
                send_to_char("         special exit group {R#{x from # add <weight> <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit group {R#{x from # set[ #] <weight> <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit group {R#{x from # remove #\n\r", ch);
                send_to_char("         special exit group {R#{x to # list\n\r", ch);
                send_to_char("         special exit group {R#{x to # add <weight> <to-level> <to-entrance>\n\r", ch);
                send_to_char("         special exit group {R#{x to # set[ #] <weight> <to-level> <to-entrance>\n\r", ch);
                send_to_char("         special exit group {R#{x to # remove #\n\r", ch);
                send_to_char("         special exit group {R#{x remove #\n\r", ch);
                sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(dng->special_exits));
                send_to_char(buf, ch);
                return false;
            }

            argument = one_argument(argument, argg);
            if (!is_number(argg))
            {
                send_to_char("Syntax:  special exit group {R#{x add static <from-level> <from-exit> <to-level> <to-entrance>\n\r", ch);
                send_to_char("         special exit group {R#{x add source <to-level> <to-entrance>\n\r", ch);
                send_to_char("         special exit group {R#{x add destination <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit group {R#{x add weighted\n\r", ch);
                send_to_char("         special exit group {R#{x from # list\n\r", ch);
                send_to_char("         special exit group {R#{x from # add <weight> <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit group {R#{x from # set[ #] <weight> <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit group {R#{x from # remove #\n\r", ch);
                send_to_char("         special exit group {R#{x to # list\n\r", ch);
                send_to_char("         special exit group {R#{x to # add <weight> <to-level> <to-entrance>\n\r", ch);
                send_to_char("         special exit group {R#{x to # set[ #] <weight> <to-level> <to-entrance>\n\r", ch);
                send_to_char("         special exit group {R#{x to # remove #\n\r", ch);
                send_to_char("         special exit group {R#{x remove #\n\r", ch);
                sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(dng->special_exits));
                send_to_char(buf, ch);
                return false;
            }

            int gindex = atoi(argg);
            if (gindex < 1 || gindex > list_size(dng->special_exits))
            {
                send_to_char("Syntax:  special exit group {R#{x add static <from-level> <from-exit> <to-level> <to-entrance>\n\r", ch);
                send_to_char("         special exit group {R#{x add source <to-level> <to-entrance>\n\r", ch);
                send_to_char("         special exit group {R#{x add destination <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit group {R#{x add weighted\n\r", ch);
                send_to_char("         special exit group {R#{x from # list\n\r", ch);
                send_to_char("         special exit group {R#{x from # add <weight> <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit group {R#{x from # set[ #] <weight> <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit group {R#{x from # remove #\n\r", ch);
                send_to_char("         special exit group {R#{x to # list\n\r", ch);
                send_to_char("         special exit group {R#{x to # add <weight> <to-level> <to-entrance>\n\r", ch);
                send_to_char("         special exit group {R#{x to # set[ #] <weight> <to-level> <to-entrance>\n\r", ch);
                send_to_char("         special exit group {R#{x to # remove #\n\r", ch);
                send_to_char("         special exit group {R#{x remove #\n\r", ch);
                sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(dng->special_exits));
                send_to_char(buf, ch);
                return false;
            }

            DUNGEON_INDEX_SPECIAL_EXIT *gex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(dng->special_exits, gindex);
            if (!IS_VALID(gex))
            {
                sprintf(buf, "Not such special exit %d found.\n\r", gindex);
                send_to_char(buf, ch);
                return false;
            }

            if (gex->mode != EXITMODE_GROUP)
            {
                sprintf(buf, "Special exit %d is not a GROUP exit.\n\r", gindex);
                send_to_char(buf, ch);
                return false;
            }

            argument = one_argument(argument, argg2);			

            if (!str_prefix(argg2, "add"))
            {
                if (list_size(dng->levels) < 1)
                {
                    send_to_char("Please add level definitions before adding special exits.\n\r", ch);
                    return false;
                }

                if (argument[0] == '\0')
                {
                    send_to_char("Syntax:  special exit group # {Wadd{x static <from-level> <from-exit> <to-level> <to-entrance>\n\r", ch);
                    send_to_char("         special exit group # {Wadd{x source <to-level> <to-entrance>\n\r", ch);
                    send_to_char("         special exit group # {Wadd{x destination <from-level> <from-exit>\n\r", ch);
                    send_to_char("         special exit group # {Wadd{x weighted\n\r", ch);
                    return false;
                }

                argument = one_argument(argument, arg3);

                // Add Static Special Exit
                // special exit group # add static <from-level> <from-exit> <to-level> <to-entrance>
                if (!str_prefix(arg3, "static"))
                {
                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit group # add static {R<from-level>{x <from-exit> <to-level> <to-entrance>\n\r", ch);
                        sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    argument = one_argument(argument, arg4);

                    if (!is_number(arg4))
                    {
                        send_to_char("Syntax:  special exit group # add static {R<from-level>{x <from-exit> <to-level> <to-entrance>\n\r", ch);
                        sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    int from_level = atoi(arg4);
                    if (from_level < 1 || from_level > list_size(dng->levels))
                    {
                        send_to_char("Syntax:  special exit group # add static {R<from-level>{x <from-exit> <to-level> <to-entrance>\n\r", ch);
                        sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    DUNGEON_INDEX_LEVEL_DATA *from_level_data = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, from_level);

                    int from_exits = get_dungeon_index_level_special_exits(dng, from_level_data);

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit group # add static <from-level> {R<from-exit>{x <to-level> <to-entrance>\n\r", ch);
                        sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
                        send_to_char(buf, ch);
                        return false;
                    }

                    argument = one_argument(argument, arg5);

                    if (!is_number(arg5))
                    {
                        send_to_char("Syntax:  special exit group # add static <from-level> {R<from-exit>{x <to-level> <to-entrance>\n\r", ch);
                        sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
                        send_to_char(buf, ch);
                        return false;
                    }

                    int from_exit = atoi(arg5);
                    if (from_exit < 1 || from_exit > from_exits)
                    {
                        send_to_char("Syntax:  special exit group # add static <from-level> {R<from-exit>{x <to-level> <to-entrance>\n\r", ch);
                        sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
                        send_to_char(buf, ch);
                        return false;
                    }

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit group # add static <from-level> <from-exit> {R<to-level>{x <to-entrance>\n\r", ch);
                        sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    argument = one_argument(argument, arg6);

                    if (!is_number(arg6))
                    {
                        send_to_char("Syntax:  special exit group # add static <from-level> <from-exit> {R<to-level>{x <to-entrance>\n\r", ch);
                        sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    int to_level = atoi(arg6);
                    if (to_level < 1 || to_level > list_size(dng->levels))
                    {
                        send_to_char("Syntax:  special exit group # add static <from-level> <from-exit> {R<to-level>{x <to-entrance>\n\r", ch);
                        sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    DUNGEON_INDEX_LEVEL_DATA *to_level_data = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, to_level);

                    int to_entries = get_dungeon_index_level_special_entrances(dng, to_level_data);

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit group # add static <from-level> <from-exit> <to-level> {R<to-entrance>{x\n\r", ch);
                        sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
                        send_to_char(buf, ch);
                        return false;
                    }

                    if (!is_number(argument))
                    {
                        send_to_char("Syntax:  special exit group # add static <from-level> <from-exit> <to-level> {R<to-entrance>{x\n\r", ch);
                        sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
                        send_to_char(buf, ch);
                        return false;
                    }

                    int to_entry = atoi(argument);
                    if (to_entry < 1 || to_entry > to_entries)
                    {
                        send_to_char("Syntax:  special exit group # add static <from-level> <from-exit> <to-level> {R<to-entrance>{x\n\r", ch);
                        sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
                        send_to_char(buf, ch);
                        return false;
                    }

                    if (from_level == to_level && from_exit == to_entry)
                    {
                        send_to_char("Both exits are the same.  Unable to connect them.\n\r", ch);
                        return false;
                    }

                    DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
                    ex->mode = EXITMODE_STATIC;
                    
                    add_dungeon_index_weighted_exit_data(ex->from, 1, from_level, from_exit);
                    add_dungeon_index_weighted_exit_data(ex->to, 1, to_level, to_entry);

                    list_appendlink(gex->group, ex);

                    send_to_char("Static special exit added to group.\n\r", ch);
                    return true;
                }

                // Add Source Special Exit
                // special exit group # add source <to-level> <to-entrance>
                if (!str_prefix(arg3, "source"))
                {
                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit group # add source {R<to-level>{x <to-entrance>\n\r", ch);
                        sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    argument = one_argument(argument, arg6);

                    if (!is_number(arg6))
                    {
                        send_to_char("Syntax:  special exit group # add source {R<to-level>{x <to-entrance>\n\r", ch);
                        sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    int to_level = atoi(arg6);
                    if (to_level < 1 || to_level > list_size(dng->levels))
                    {
                        send_to_char("Syntax:  special exit group # add source {R<to-level>{x <to-entrance>\n\r", ch);
                        sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    DUNGEON_INDEX_LEVEL_DATA *to_level_data = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, to_level);

                    int to_entries = get_dungeon_index_level_special_entrances(dng, to_level_data);

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit group # add source <to-level> {R<to-entrance>{x\n\r", ch);
                        sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
                        send_to_char(buf, ch);
                        return false;
                    }

                    if (!is_number(argument))
                    {
                        send_to_char("Syntax:  special exit group # add source <to-level> {R<to-entrance>{x\n\r", ch);
                        sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
                        send_to_char(buf, ch);
                        return false;
                    }

                    int to_entry = atoi(argument);
                    if (to_entry < 1 || to_entry > to_entries)
                    {
                        send_to_char("Syntax:  special exit group # add source <to-level> {R<to-entrance>{x\n\r", ch);
                        sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
                        send_to_char(buf, ch);
                        return false;
                    }

                    DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
                    ex->mode = EXITMODE_WEIGHTED_SOURCE;
                    
                    add_dungeon_index_weighted_exit_data(ex->to, 1, to_level, to_entry);

                    list_appendlink(gex->group, ex);

                    int index = list_size(gex->group);

                    send_to_char("Source special exit group # added.\n\r", ch);
                    sprintf(buf, "Please add source exits using {Wspecial exit group {Y%d{W from {Y%d{W add ...{x\n\r", gindex, index);
                    send_to_char(buf, ch);
                    return true;
                }

                if (!str_prefix(arg3, "destination"))
                {
                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit group # add destination {R<from-level>{x <from-exit>\n\r", ch);
                        sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    argument = one_argument(argument, arg4);

                    if (!is_number(arg4))
                    {
                        send_to_char("Syntax:  special exit group # add destination {R<from-level>{x <from-exit>\n\r", ch);
                        sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    int from_level = atoi(arg4);
                    if (from_level < 1 || from_level > list_size(dng->levels))
                    {
                        send_to_char("Syntax:  special exit group # add destination {R<from-level>{x <from-exit>\n\r", ch);
                        sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    DUNGEON_INDEX_LEVEL_DATA *from_level_data = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, from_level);

                    int from_exits = get_dungeon_index_level_special_exits(dng, from_level_data);

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit group # add destination <from-level> {R<from-exit>{x\n\r", ch);
                        sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
                        send_to_char(buf, ch);
                        return false;
                    }

                    if (!is_number(argument))
                    {
                        send_to_char("Syntax:  special exit group # add destination <from-level> {R<from-exit>{x\n\r", ch);
                        sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
                        send_to_char(buf, ch);
                        return false;
                    }

                    int from_exit = atoi(argument);
                    if (from_exit < 1 || from_exit > from_exits)
                    {
                        send_to_char("Syntax:  special exit group # add destination <from-level> {R<from-exit>{x\n\r", ch);
                        sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
                        send_to_char(buf, ch);
                        return false;
                    }

                    DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
                    ex->mode = EXITMODE_WEIGHTED_DEST;
                    
                    add_dungeon_index_weighted_exit_data(ex->from, 1, from_level, from_exit);

                    list_appendlink(gex->group, ex);

                    int index = list_size(gex->group);

                    send_to_char("Destination special exit added to group.\n\r", ch);
                    sprintf(buf, "Please add target entrances using {Wspecial exit group {Y%d{W to {Y%d{W add ...{x\n\r", gindex, index);
                    send_to_char(buf, ch);
                    return true;
                }

                if (!str_prefix(arg3, "weighted"))
                {
                    if (!IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit group # add weighted\n\r", ch);
                        return false;
                    }
                    
                    DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
                    ex->mode = EXITMODE_WEIGHTED;

                    list_appendlink(gex->group, ex);

                    int index = list_size(gex->group);

                    send_to_char("Weighted special exit group # added.\n\r", ch);
                    sprintf(buf, "Please add source exits using {Wspecial exit group {Y%d{W from {Y%d{W add ...{x\n\r", gindex, index);
                    send_to_char(buf, ch);
                    sprintf(buf, "Please add target entrances using {Wspecial exit group {Y%d{W to {Y%d{W add ...{x\n\r", gindex, index);
                    send_to_char(buf, ch);
                    return true;
                }

                send_to_char("Syntax:  special exit group # add {Wstatic{x <from-level> <from-exit> <to-level> <to-entrance>\n\r", ch);
                send_to_char("         special exit group # add {Wsource{x <to-level> <to-entrance>\n\r", ch);
                send_to_char("         special exit group # add {Wdestination{x <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit group # add {Wweighted{x\n\r", ch);
                return false;
            }

            if (!str_prefix(arg2, "from"))
            {
                if (list_size(gex->group) < 1)
                {
                    send_to_char("Please add a special exit definition first.\n\r", ch);
                    return false;
                }

                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit group # from {R#{x list\n\r", ch);
                    send_to_char("         special exit group # from {R#{x add <weight> <from-level> <from-exit>\n\r", ch);
                    send_to_char("         special exit group # from {R#{x set # <weight> <from-level> <from-exit>\n\r", ch);
                    send_to_char("         special exit group # from {R#{x set <weight> <from-level> <from-exit> (for static and destination exits only)\n\r", ch);
                    send_to_char("         special exit group # from {R#{x remove #\n\r", ch);
                    sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(gex->group));
                    send_to_char(buf, ch);
                    return false;
                }

                argument = one_argument(argument, arg3);
                if (!is_number(arg3))
                {
                    send_to_char("Syntax:  special exit group # from {R#{x list\n\r", ch);
                    send_to_char("         special exit group # from {R#{x add <weight> <from-level> <from-exit>\n\r", ch);
                    send_to_char("         special exit group # from {R#{x set # <weight> <from-level> <from-exit>\n\r", ch);
                    send_to_char("         special exit group # from {R#{x set <weight> <from-level> <from-exit> (for static and destination exits only)\n\r", ch);
                    send_to_char("         special exit group # from {R#{x remove #\n\r", ch);
                    sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(gex->group));
                    send_to_char(buf, ch);
                    return false;
                }

                int index = atoi(arg3);
                if (index < 1 || index > list_size(gex->group))
                {
                    send_to_char("Syntax:  special exit group # from {R#{x list\n\r", ch);
                    send_to_char("         special exit group # from {R#{x add <weight> <from-level> <from-exit>\n\r", ch);
                    send_to_char("         special exit group # from {R#{x set # <weight> <from-level> <from-exit>\n\r", ch);
                    send_to_char("         special exit group # from {R#{x set <weight> <from-level> <from-exit> (for static and destination exits only)\n\r", ch);
                    send_to_char("         special exit group # from {R#{x remove #\n\r", ch);
                    sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(gex->group));
                    send_to_char(buf, ch);
                    return false;
                }

                DUNGEON_INDEX_SPECIAL_EXIT *ex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(gex->group, index);

                if (ex->mode == EXITMODE_GROUP)
                {
                    send_to_char("Cannot alter the From definitions on a GROUP exit.\n\r", ch);
                    return false;
                }

                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit group # from # {Rlist{x\n\r", ch);
                    send_to_char("         special exit group # from # {Radd{x <weight> <from-level> <from-exit>\n\r", ch);
                    send_to_char("         special exit group # from # {Rset{x # <weight> <from-level> <from-exit>\n\r", ch);
                    send_to_char("         special exit group # from # {Rset{x <weight> <from-level> <from-exit> (for static and destination exits only)\n\r", ch);
                    send_to_char("         special exit group # from # {Rremove{x #\n\r", ch);
                    return false;
                }

                argument = one_argument(argument, arg4);

                if (!str_prefix(arg4, "list"))
                {
                    if (list_size(ex->from) > 0)
                    {
                        if (!dngedit_render_weighted_exit_list(ch, ex->from,
                            "Special exit from-list output exceeded buffer limits.\n\r"))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        send_to_char("There are no From definitions on this special exit.\n\r", ch);
                    }
                    return false;
                }

                if (!str_prefix(arg4, "add"))
                {
                    if (ex->mode == EXITMODE_STATIC)
                    {
                        sprintf(buf, "Special exit %d is a STATIC exit.  Cannot add any new From definition.\n\r", index);
                        send_to_char(buf, ch);
                        return false;
                    }

                    if (ex->mode == EXITMODE_WEIGHTED_DEST)
                    {
                        sprintf(buf, "Special exit %d is a DESTINATION exit.  Cannot add any new From definition.\n\r", index);
                        send_to_char(buf, ch);
                        return false;
                    }

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit group # from # add {R<weight>{x <from-level> <from-exit>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    argument = one_argument(argument, arg6);
                    if (!is_number(arg5))
                    {
                        send_to_char("Syntax:  special exit group # from # add {R<weight>{x <from-level> <from-exit>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    int weight = atoi(arg6);
                    if (weight < 1)
                    {
                        send_to_char("Syntax:  special exit group # from # add {R<weight>{x <from-level> <from-exit>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit group # from # add <weight> {R<from-level>{x <from-exit>\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    argument = one_argument(argument, arg7);
                    if (!is_number(arg7))
                    {
                        send_to_char("Syntax:  special exit group # from # add <weight> {R<from-level>{x <from-exit>\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    int flindex = atoi(arg7);
                    if (flindex < 1 || flindex > list_size(dng->levels))
                    {
                        send_to_char("Syntax:  special exit group # from # add <weight> {R<from-level>{x <from-exit>\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    DUNGEON_INDEX_LEVEL_DATA *flevel = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, flindex);
                    int fexits = get_dungeon_index_level_special_exits(dng, flevel);

                    if (!is_number(argument))
                    {
                        send_to_char("Syntax:  special exit group # from # add <weight> <from-level> {R<from-exit>{x\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
                        send_to_char(buf, ch);
                        return false;
                    }

                    int fexit = atoi(argument);
                    if (fexit < 1 || fexit > fexits)
                    {
                        send_to_char("Syntax:  special exit group # from # add <weight> <from-level> {R<from-exit>{x\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
                        send_to_char(buf, ch);
                        return false;
                    }

                    add_dungeon_index_weighted_exit_data(ex->from, weight, flindex, fexit);
                    ex->total_from += weight;

                    sprintf(buf, "From definition added to special exit %d.\n\r", index);
                    send_to_char(buf, ch);
                    return false;
                }

                if (!str_prefix(arg4, "set"))
                {
                    if (ex->mode == EXITMODE_STATIC || ex->mode == EXITMODE_WEIGHTED_DEST)
                    {
                        if (list_size(ex->from) < 1)
                        {
                            send_to_char("Special exit appears to be missing necessary From definition.\n\r", ch);
                            return false;
                        }

                        if (IS_NULLSTR(argument))
                        {
                            send_to_char("Syntax:  special exit group # from # set {R<weight>{x <from-level> <from-exit>\n\r", ch);
                            send_to_char("         Please specify a positive number.\n\r", ch);
                            return false;
                        }

                        argument = one_argument(argument, arg6);
                        if (!is_number(arg5))
                        {
                            send_to_char("Syntax:  special exit group # from # set {R<weight>{x <from-level> <from-exit>\n\r", ch);
                            send_to_char("         Please specify a positive number.\n\r", ch);
                            return false;
                        }

                        int weight = atoi(arg6);
                        if (weight < 1)
                        {
                            send_to_char("Syntax:  special exit group # from # set {R<weight>{x <from-level> <from-exit>\n\r", ch);
                            send_to_char("         Please specify a positive number.\n\r", ch);
                            return false;
                        }

                        if (IS_NULLSTR(argument))
                        {
                            send_to_char("Syntax:  special exit group # from # set <weight> {R<from-level>{x <from-exit>\n\r", ch);
                            sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                            send_to_char(buf, ch);
                            return false;
                        }

                        argument = one_argument(argument, arg7);
                        if (!is_number(arg7))
                        {
                            send_to_char("Syntax:  special exit group # from # set <weight> {R<from-level>{x <from-exit>\n\r", ch);
                            sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                            send_to_char(buf, ch);
                            return false;
                        }

                        int flindex = atoi(arg7);
                        if (flindex < 1 || flindex > list_size(dng->levels))
                        {
                            send_to_char("Syntax:  special exit group # from # set <weight> {R<from-level>{x <from-exit>\n\r", ch);
                            sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                            send_to_char(buf, ch);
                            return false;
                        }

                        DUNGEON_INDEX_LEVEL_DATA *flevel = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, flindex);
                        int fexits = get_dungeon_index_level_special_exits(dng, flevel);

                        if (!is_number(argument))
                        {
                            send_to_char("Syntax:  special exit group # from # set <weight> <from-level> {R<from-exit>{x\n\r", ch);
                            sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
                            send_to_char(buf, ch);
                            return false;
                        }

                        int fexit = atoi(argument);
                        if (fexit < 1 || fexit > fexits)
                        {
                            send_to_char("Syntax:  special exit group # from # set <weight> <from-level> {R<from-exit>{x\n\r", ch);
                            sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
                            send_to_char(buf, ch);
                            return false;
                        }

                        DUNGEON_INDEX_WEIGHTED_EXIT_DATA *fex = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(ex->from, 1);
                        ex->total_from -= fex->weight;
                        ex->total_from += weight;

                        fex->weight = weight;
                        fex->level = flindex;
                        fex->door = fexit;

                        sprintf(buf, "From definition set on special exit %d.\n\r", index);
                        send_to_char(buf, ch);
                        return true;
                    }
                    else
                    {
                        if (list_size(ex->from) < 1)
                        {
                            send_to_char("Special exit has no From definition.\n\r", ch);
                            return false;
                        }

                        if (IS_NULLSTR(argument))
                        {
                            send_to_char("Syntax:  special exit group # from # set {R#{x <weight> <from-level> <from-exit>\n\r", ch);
                            sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
                            send_to_char(buf, ch);
                            return false;
                        }

                        argument = one_argument(argument, arg5);
                        if (!is_number(arg5))
                        {
                            send_to_char("Syntax:  special exit group # from # set {R#{x <weight> <from-level> <from-exit>\n\r", ch);
                            sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
                            send_to_char(buf, ch);
                            return false;
                        }

                        int findex = atoi(arg5);
                        if (findex < 1 || findex > list_size(ex->from))
                        {
                            send_to_char("Syntax:  special exit group # from # set {R#{x <weight> <from-level> <from-exit>\n\r", ch);
                            sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
                            send_to_char(buf, ch);
                            return false;
                        }

                        if (IS_NULLSTR(argument))
                        {
                            send_to_char("Syntax:  special exit group # from # set # {R<weight>{x <from-level> <from-exit>\n\r", ch);
                            send_to_char("         Please specify a positive number.\n\r", ch);
                            return false;
                        }

                        argument = one_argument(argument, arg6);
                        if (!is_number(arg5))
                        {
                            send_to_char("Syntax:  special exit group # from # set # {R<weight>{x <from-level> <from-exit>\n\r", ch);
                            send_to_char("         Please specify a positive number.\n\r", ch);
                            return false;
                        }

                        int weight = atoi(arg6);
                        if (weight < 1)
                        {
                            send_to_char("Syntax:  special exit group # from # set # {R<weight>{x <from-level> <from-exit>\n\r", ch);
                            send_to_char("         Please specify a positive number.\n\r", ch);
                            return false;
                        }

                        if (IS_NULLSTR(argument))
                        {
                            send_to_char("Syntax:  special exit group # from # set # <weight> {R<from-level>{x <from-exit>\n\r", ch);
                            sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                            send_to_char(buf, ch);
                            return false;
                        }

                        argument = one_argument(argument, arg7);
                        if (!is_number(arg7))
                        {
                            send_to_char("Syntax:  special exit group # from # set # <weight> {R<from-level>{x <from-exit>\n\r", ch);
                            sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                            send_to_char(buf, ch);
                            return false;
                        }

                        int flindex = atoi(arg7);
                        if (flindex < 1 || flindex > list_size(dng->levels))
                        {
                            send_to_char("Syntax:  special exit group # from # set # <weight> {R<from-level>{x <from-exit>\n\r", ch);
                            sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                            send_to_char(buf, ch);
                            return false;
                        }

                        DUNGEON_INDEX_LEVEL_DATA *flevel = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, flindex);
                        int fexits = get_dungeon_index_level_special_exits(dng, flevel);

                        if (!is_number(argument))
                        {
                            send_to_char("Syntax:  special exit group # from # set # <weight> <from-level> {R<from-exit>{x\n\r", ch);
                            sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
                            send_to_char(buf, ch);
                            return false;
                        }

                        int fexit = atoi(argument);
                        if (fexit < 1 || fexit > fexits)
                        {
                            send_to_char("Syntax:  special exit group # from # set # <weight> <from-level> {R<from-exit>{x\n\r", ch);
                            sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
                            send_to_char(buf, ch);
                            return false;
                        }

                        DUNGEON_INDEX_WEIGHTED_EXIT_DATA *fex = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(ex->from, findex);
                        ex->total_from -= fex->weight;
                        ex->total_from += weight;

                        fex->weight = weight;
                        fex->level = flindex;
                        fex->door = fexit;

                        sprintf(buf, "From definition %d set on special exit %d.\n\r", findex, index);
                        send_to_char(buf, ch);
                        return true;

                    }
                }

                if (!str_prefix(arg4, "remove"))
                {
                    if (ex->mode == EXITMODE_STATIC)
                    {
                        sprintf(buf, "Special exit %d is a STATIC exit.  Cannot remove the From definition.\n\r", index);
                        send_to_char(buf, ch);
                        return false;
                    }

                    if (ex->mode == EXITMODE_WEIGHTED_DEST)
                    {
                        sprintf(buf, "Special exit %d is a DESTINATION exit.  Cannot remove the From definition.\n\r", index);
                        send_to_char(buf, ch);
                        return false;
                    }

                    if (!is_number(argument))
                    {
                        send_to_char("Syntax:  special exit group # from # remove {R#{x\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
                        send_to_char(buf, ch);
                        return false;
                    }

                    int findex = atoi(argument);
                    if (findex < 1 || findex > list_size(ex->from))
                    {
                        send_to_char("Syntax:  special exit group # from # remove {R#{x\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
                        send_to_char(buf, ch);
                        return false;
                    }

                    list_remnthlink(ex->from, findex, true);
                    send_to_char("From definition removed from special exit.\n\r", ch);
                    if (list_size(ex->from) < 1)
                        send_to_char("{RWarning:{x Please add a from definition for this exit to work.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  special exit group # from # {Rlist{x\n\r", ch);
                send_to_char("         special exit group # from # {Radd{x <weight> <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit group # from # {Rset{x # <weight> <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit group # from # {Rset{x <weight> <from-level> <from-exit> (for static and destination exits only)\n\r", ch);
                send_to_char("         special exit group # from # {Rremove{x #\n\r", ch);
                return false;
            }

            if (!str_prefix(arg2, "to"))
            {
                if (list_size(gex->group) < 1)
                {
                    send_to_char("Please add a special exit definition first.\n\r", ch);
                    return false;
                }

                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit group # to {R#{x list\n\r", ch);
                    send_to_char("         special exit group # to {R#{x add <weight> <to-level> <to-entry>\n\r", ch);
                    send_to_char("         special exit group # to {R#{x set # <weight> <to-level> <to-entry>\n\r", ch);
                    send_to_char("         special exit group # to {R#{x set <weight> <to-level> <to-entry> (for static and source exits only)\n\r", ch);
                    send_to_char("         special exit group # to {R#{x remove #\n\r", ch);
                    sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(gex->group));
                    send_to_char(buf, ch);
                    return false;
                }

                argument = one_argument(argument, arg3);
                if (!is_number(arg3))
                {
                    send_to_char("Syntax:  special exit group # to {R#{x list\n\r", ch);
                    send_to_char("         special exit group # to {R#{x add <weight> <to-level> <to-entry>\n\r", ch);
                    send_to_char("         special exit group # to {R#{x set # <weight> <to-level> <to-entry>\n\r", ch);
                    send_to_char("         special exit group # to {R#{x set <weight> <to-level> <to-entry> (for static and source exits only)\n\r", ch);
                    send_to_char("         special exit group # to {R#{x remove #\n\r", ch);
                    sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(gex->group));
                    send_to_char(buf, ch);
                    return false;
                }

                int index = atoi(arg3);
                if (index < 1 || index > list_size(gex->group))
                {
                    send_to_char("Syntax:  special exit group # to {R#{x list\n\r", ch);
                    send_to_char("         special exit group # to {R#{x add <weight> <to-level> <to-entry>\n\r", ch);
                    send_to_char("         special exit group # to {R#{x set # <weight> <to-level> <to-entry>\n\r", ch);
                    send_to_char("         special exit group # to {R#{x set <weight> <to-level> <to-entry> (for static and source exits only)\n\r", ch);
                    send_to_char("         special exit group # to {R#{x remove #\n\r", ch);
                    sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(gex->group));
                    send_to_char(buf, ch);
                    return false;
                }

                DUNGEON_INDEX_SPECIAL_EXIT *ex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(gex->group, index);

                if (ex->mode == EXITMODE_GROUP)
                {
                    send_to_char("Cannot alter the To definitions on a GROUP exit.\n\r", ch);
                    return false;
                }

                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit group # to # {Rlist{x\n\r", ch);
                    send_to_char("         special exit group # to # {Radd{x <weight> <to-level> <to-entry>\n\r", ch);
                    send_to_char("         special exit group # to # {Rset{x # <weight> <to-level> <to-entry>\n\r", ch);
                    send_to_char("         special exit group # to # {Rset{x <weight> <to-level> <to-entry> (for static and source exits only)\n\r", ch);
                    send_to_char("         special exit group # to # {Rremove{x #\n\r", ch);
                    return false;
                }

                argument = one_argument(argument, arg4);

                if (!str_prefix(arg4, "list"))
                {
                    if (list_size(ex->to) > 0)
                    {
                        if (!dngedit_render_weighted_exit_list(ch, ex->to,
                            "Special exit to-list output exceeded buffer limits.\n\r"))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        send_to_char("There are no To definitions on this special exit.\n\r", ch);
                    }
                    return false;
                }

                if (!str_prefix(arg4, "add"))
                {
                    if (ex->mode == EXITMODE_STATIC)
                    {
                        sprintf(buf, "Special exit %d is a STATIC exit.  Cannot add any new To definition.\n\r", index);
                        send_to_char(buf, ch);
                        return false;
                    }

                    if (ex->mode == EXITMODE_WEIGHTED_SOURCE)
                    {
                        sprintf(buf, "Special exit %d is a SOURCE exit.  Cannot add any new To definition.\n\r", index);
                        send_to_char(buf, ch);
                        return false;
                    }

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit group # to # add {R<weight>{x <to-level> <to-entry>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    argument = one_argument(argument, arg6);
                    if (!is_number(arg5))
                    {
                        send_to_char("Syntax:  special exit group # to # add {R<weight>{x <to-level> <to-entry>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    int weight = atoi(arg6);
                    if (weight < 1)
                    {
                        send_to_char("Syntax:  special exit group # to # add {R<weight>{x <to-level> <to-entry>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit group # to # add <weight> {R<to-level>{x <to-entry>\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    argument = one_argument(argument, arg7);
                    if (!is_number(arg7))
                    {
                        send_to_char("Syntax:  special exit group # to # add <weight> {R<to-level>{x <to-entry>\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    int tlindex = atoi(arg7);
                    if (tlindex < 1 || tlindex > list_size(dng->levels))
                    {
                        send_to_char("Syntax:  special exit group # to # add <weight> {R<to-level>{x <to-entry>\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    DUNGEON_INDEX_LEVEL_DATA *tlevel = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, tlindex);
                    int tentries = get_dungeon_index_level_special_entrances(dng, tlevel);

                    if (!is_number(argument))
                    {
                        send_to_char("Syntax:  special exit group # to # add <weight> <to-level> {R<to-entry>{x\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
                        send_to_char(buf, ch);
                        return false;
                    }

                    int tentry = atoi(argument);
                    if (tentry < 1 || tentry > tentries)
                    {
                        send_to_char("Syntax:  special exit group # to # add <weight> <to-level> {R<to-entry>{x\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
                        send_to_char(buf, ch);
                        return false;
                    }

                    add_dungeon_index_weighted_exit_data(ex->to, weight, tlindex, tentry);
                    ex->total_to += weight;

                    sprintf(buf, "To definition added to special exit %d.\n\r", index);
                    send_to_char(buf, ch);
                    return false;
                }

                if (!str_prefix(arg4, "set"))
                {
                    if (ex->mode == EXITMODE_STATIC || ex->mode == EXITMODE_WEIGHTED_SOURCE)
                    {
                        if (list_size(ex->to) < 1)
                        {
                            send_to_char("Special exit appears to be missing necessary To definition.\n\r", ch);
                            return false;
                        }

                        if (IS_NULLSTR(argument))
                        {
                            send_to_char("Syntax:  special exit group # to # set {R<weight>{x <to-level> <to-entry>\n\r", ch);
                            send_to_char("         Please specify a positive number.\n\r", ch);
                            return false;
                        }

                        argument = one_argument(argument, arg6);
                        if (!is_number(arg5))
                        {
                            send_to_char("Syntax:  special exit group # to # set {R<weight>{x <to-level> <to-entry>\n\r", ch);
                            send_to_char("         Please specify a positive number.\n\r", ch);
                            return false;
                        }

                        int weight = atoi(arg6);
                        if (weight < 1)
                        {
                            send_to_char("Syntax:  special exit group # to # set {R<weight>{x <to-level> <to-entry>\n\r", ch);
                            send_to_char("         Please specify a positive number.\n\r", ch);
                            return false;
                        }

                        if (IS_NULLSTR(argument))
                        {
                            send_to_char("Syntax:  special exit group # to # set <weight> {R<to-level>{x <to-entry>\n\r", ch);
                            sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                            send_to_char(buf, ch);
                            return false;
                        }

                        argument = one_argument(argument, arg7);
                        if (!is_number(arg7))
                        {
                            send_to_char("Syntax:  special exit group # to # set <weight> {R<to-level>{x <to-entry>\n\r", ch);
                            sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                            send_to_char(buf, ch);
                            return false;
                        }

                        int tlindex = atoi(arg7);
                        if (tlindex < 1 || tlindex > list_size(dng->levels))
                        {
                            send_to_char("Syntax:  special exit group # to # set <weight> {R<to-level>{x <to-entry>\n\r", ch);
                            sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                            send_to_char(buf, ch);
                            return false;
                        }

                        DUNGEON_INDEX_LEVEL_DATA *tlevel = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, tlindex);
                        int tentries = get_dungeon_index_level_special_entrances(dng, tlevel);

                        if (!is_number(argument))
                        {
                            send_to_char("Syntax:  special exit group # to # set <weight> <to-level> {R<to-entry>{x\n\r", ch);
                            sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
                            send_to_char(buf, ch);
                            return false;
                        }

                        int tentry = atoi(argument);
                        if (tentry < 1 || tentry > tentries)
                        {
                            send_to_char("Syntax:  special exit group # to # set <weight> <to-level> {R<to-entry>{x\n\r", ch);
                            sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
                            send_to_char(buf, ch);
                            return false;
                        }

                        DUNGEON_INDEX_WEIGHTED_EXIT_DATA *tex = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(ex->to, 1);
                        ex->total_to -= tex->weight;
                        ex->total_to += weight;

                        tex->weight = weight;
                        tex->level = tlindex;
                        tex->door = tentry;

                        sprintf(buf, "To definition set on special exit %d.\n\r", index);
                        send_to_char(buf, ch);
                        return true;
                    }
                    else
                    {
                        if (list_size(ex->to) < 1)
                        {
                            send_to_char("Special exit has no To definition.\n\r", ch);
                            return false;
                        }

                        if (IS_NULLSTR(argument))
                        {
                            send_to_char("Syntax:  special exit group # to # set {R#{x <weight> <to-level> <to-entry>\n\r", ch);
                            sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
                            send_to_char(buf, ch);
                            return false;
                        }

                        argument = one_argument(argument, arg5);
                        if (!is_number(arg5))
                        {
                            send_to_char("Syntax:  special exit group # to # set {R#{x <weight> <to-level> <to-entry>\n\r", ch);
                            sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
                            send_to_char(buf, ch);
                            return false;
                        }

                        int tindex = atoi(arg5);
                        if (tindex < 1 || tindex > list_size(ex->to))
                        {
                            send_to_char("Syntax:  special exit group # to # set {R#{x <weight> <to-level> <to-entry>\n\r", ch);
                            sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
                            send_to_char(buf, ch);
                            return false;
                        }

                        if (IS_NULLSTR(argument))
                        {
                            send_to_char("Syntax:  special exit group # to # set # {R<weight>{x <to-level> <to-entry>\n\r", ch);
                            send_to_char("         Please specify a positive number.\n\r", ch);
                            return false;
                        }

                        argument = one_argument(argument, arg6);
                        if (!is_number(arg5))
                        {
                            send_to_char("Syntax:  special exit group # to # set # {R<weight>{x <to-level> <to-entry>\n\r", ch);
                            send_to_char("         Please specify a positive number.\n\r", ch);
                            return false;
                        }

                        int weight = atoi(arg6);
                        if (weight < 1)
                        {
                            send_to_char("Syntax:  special exit group # to # set # {R<weight>{x <to-level> <to-entry>\n\r", ch);
                            send_to_char("         Please specify a positive number.\n\r", ch);
                            return false;
                        }

                        if (IS_NULLSTR(argument))
                        {
                            send_to_char("Syntax:  special exit group # to # set # <weight> {R<to-level>{x <to-entry>\n\r", ch);
                            sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                            send_to_char(buf, ch);
                            return false;
                        }

                        argument = one_argument(argument, arg7);
                        if (!is_number(arg7))
                        {
                            send_to_char("Syntax:  special exit group # to # set # <weight> {R<to-level>{x <to-entry>\n\r", ch);
                            sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                            send_to_char(buf, ch);
                            return false;
                        }

                        int tlindex = atoi(arg7);
                        if (tlindex < 1 || tlindex > list_size(dng->levels))
                        {
                            send_to_char("Syntax:  special exit group # to # set # <weight> {R<to-level>{x <to-entry>\n\r", ch);
                            sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                            send_to_char(buf, ch);
                            return false;
                        }

                        DUNGEON_INDEX_LEVEL_DATA *tlevel = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, tlindex);
                        int tentries = get_dungeon_index_level_special_entrances(dng, tlevel);

                        if (!is_number(argument))
                        {
                            send_to_char("Syntax:  special exit group # to # set # <weight> <to-level> {R<to-entry>{x\n\r", ch);
                            sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
                            send_to_char(buf, ch);
                            return false;
                        }

                        int tentry = atoi(argument);
                        if (tentry < 1 || tentry > tentries)
                        {
                            send_to_char("Syntax:  special exit group # to # set # <weight> <to-level> {R<to-entry>{x\n\r", ch);
                            sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
                            send_to_char(buf, ch);
                            return false;
                        }

                        DUNGEON_INDEX_WEIGHTED_EXIT_DATA *tex = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(ex->to, tindex);
                        ex->total_to -= tex->weight;
                        ex->total_to += weight;

                        tex->weight = weight;
                        tex->level = tlindex;
                        tex->door = tentry;

                        sprintf(buf, "To definition %d set on special exit %d.\n\r", tindex, index);
                        send_to_char(buf, ch);
                        return true;

                    }
                }

                if (!str_prefix(arg4, "remove"))
                {
                    if (ex->mode == EXITMODE_STATIC)
                    {
                        sprintf(buf, "Special exit %d is a STATIC exit.  Cannot remove the To definition.\n\r", index);
                        send_to_char(buf, ch);
                        return false;
                    }

                    if (ex->mode == EXITMODE_WEIGHTED_SOURCE)
                    {
                        sprintf(buf, "Special exit %d is a SOURCE exit.  Cannot remove the To definition.\n\r", index);
                        send_to_char(buf, ch);
                        return false;
                    }

                    if (!is_number(argument))
                    {
                        send_to_char("Syntax:  special exit group # to # remove {R#{x\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
                        send_to_char(buf, ch);
                        return false;
                    }

                    int tindex = atoi(argument);
                    if (tindex < 1 || tindex > list_size(ex->to))
                    {
                        send_to_char("Syntax:  special exit group # to # remove {R#{x\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
                        send_to_char(buf, ch);
                        return false;
                    }

                    list_remnthlink(ex->to, tindex, true);
                    send_to_char("To definition removed to special exit.\n\r", ch);
                    if (list_size(ex->to) < 1)
                        send_to_char("{RWarning:{x Please add a To definition for this exit to work.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  special exit group # to # {Rlist{x\n\r", ch);
                send_to_char("         special exit group # to # {Radd{x <weight> <to-level> <to-entry>\n\r", ch);
                send_to_char("         special exit group # to # {Rset{x # <weight> <to-level> <to-entry>\n\r", ch);
                send_to_char("         special exit group # to # {Rset{x <weight> <to-level> <to-entry> (for static and destination exits only)\n\r", ch);
                send_to_char("         special exit group # to # {Rremove{x #\n\r", ch);
                return false;
            }

            if (!str_prefix(arg2, "remove"))
            {
                if (argument[0] == '\0')
                {
                    send_to_char("Syntax:  special exit group # remove {R#{x\n\r", ch);
                    sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(gex->group));
                    send_to_char(buf, ch);
                    return false;
                }

                if (!is_number(argument))
                {
                    send_to_char("Syntax:  special exit remove {R#{x\n\r", ch);
                    sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(gex->group));
                    send_to_char(buf, ch);
                    return false;
                }

                if (list_size(dng->special_exits) < 1)
                {
                    send_to_char("There are no special exits.\n\r", ch);
                    return false;
                }

                int index = atoi(argument);
                if (index < 1 || index > list_size(gex->group))
                {
                    send_to_char("Syntax:  special exit remove {R#{x\n\r", ch);
                    sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(gex->group));
                    send_to_char(buf, ch);
                    return false;
                }

                list_remnthlink(gex->group, index, true);
                sprintf(buf, "Special exit %d removed from group %d.\n\r", index, gindex);
                send_to_char(buf, ch);
                return true;
            }


            send_to_char("Syntax:  special exit group # {Radd{x static <from-level> <from-exit> <to-level> <to-entrance>\n\r", ch);
            send_to_char("         special exit group # {Radd{x source <to-level> <to-entrance>\n\r", ch);
            send_to_char("         special exit group # {Radd{x destination <from-level> <from-exit>\n\r", ch);
            send_to_char("         special exit group # {Radd{x weighted\n\r", ch);
            send_to_char("         special exit group # {Rfrom{x # list\n\r", ch);
            send_to_char("         special exit group # {Rfrom{x # add <weight> <from-level> <from-exit>\n\r", ch);
            send_to_char("         special exit group # {Rfrom{x # set[ #] <weight> <from-level> <from-exit>\n\r", ch);
            send_to_char("         special exit group # {Rfrom{x # remove #\n\r", ch);
            send_to_char("         special exit group # {Rto{x # list\n\r", ch);
            send_to_char("         special exit group # {Rto{x # add <weight> <to-level> <to-entrance>\n\r", ch);
            send_to_char("         special exit group # {Rto{x # set[ #] <weight> <to-level> <to-entrance>\n\r", ch);
            send_to_char("         special exit group # {Rto{x # remove #\n\r", ch);
            send_to_char("         special exit group # {Rremove{x #\n\r", ch);
            return false;
        }

        if (!str_prefix(arg2, "from"))
        {
            char buf[MSL];
            if (list_size(dng->special_exits) < 1)
            {
                send_to_char("Please add a special exit definition first.\n\r", ch);
                return false;
            }

            if (IS_NULLSTR(argument))
            {
                send_to_char("Syntax:  special exit from {R#{x list\n\r", ch);
                send_to_char("         special exit from {R#{x add <weight> <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit from {R#{x set # <weight> <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit from {R#{x set <weight> <from-level> <from-exit> (for static and destination exits only)\n\r", ch);
                send_to_char("         special exit from {R#{x remove #\n\r", ch);
                sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(dng->special_exits));
                send_to_char(buf, ch);
                return false;
            }

            argument = one_argument(argument, arg3);
            if (!is_number(arg3))
            {
                send_to_char("Syntax:  special exit from {R#{x list\n\r", ch);
                send_to_char("         special exit from {R#{x add <weight> <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit from {R#{x set # <weight> <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit from {R#{x set <weight> <from-level> <from-exit> (for static and destination exits only)\n\r", ch);
                send_to_char("         special exit from {R#{x remove #\n\r", ch);
                sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(dng->special_exits));
                send_to_char(buf, ch);
                return false;
            }

            int index = atoi(arg3);
            if (index < 1 || index > list_size(dng->special_exits))
            {
                send_to_char("Syntax:  special exit from {R#{x list\n\r", ch);
                send_to_char("         special exit from {R#{x add <weight> <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit from {R#{x set # <weight> <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit from {R#{x set <weight> <from-level> <from-exit> (for static and destination exits only)\n\r", ch);
                send_to_char("         special exit from {R#{x remove #\n\r", ch);
                sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(dng->special_exits));
                send_to_char(buf, ch);
                return false;
            }

            DUNGEON_INDEX_SPECIAL_EXIT *ex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(dng->special_exits, index);

            if (ex->mode == EXITMODE_GROUP)
            {
                send_to_char("Cannot alter the From definitions on a GROUP exit.\n\r", ch);
                return false;
            }

            if (IS_NULLSTR(argument))
            {
                send_to_char("Syntax:  special exit from # {Rlist{x\n\r", ch);
                send_to_char("         special exit from # {Radd{x <weight> <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit from # {Rset{x # <weight> <from-level> <from-exit>\n\r", ch);
                send_to_char("         special exit from # {Rset{x <weight> <from-level> <from-exit> (for static and destination exits only)\n\r", ch);
                send_to_char("         special exit from # {Rremove{x #\n\r", ch);
                return false;
            }

            argument = one_argument(argument, arg4);

            if (!str_prefix(arg4, "list"))
            {
                if (list_size(ex->from) > 0)
                {
                    if (!dngedit_render_weighted_exit_list(ch, ex->from,
                        "Special exit from-list output exceeded buffer limits.\n\r"))
                    {
                        return false;
                    }
                }
                else
                {
                    send_to_char("There are no From definitions on this special exit.\n\r", ch);
                }
                return false;
            }

            if (!str_prefix(arg4, "add"))
            {
                if (ex->mode == EXITMODE_STATIC)
                {
                    sprintf(buf, "Special exit %d is a STATIC exit.  Cannot add any new From definition.\n\r", index);
                    send_to_char(buf, ch);
                    return false;
                }

                if (ex->mode == EXITMODE_WEIGHTED_DEST)
                {
                    sprintf(buf, "Special exit %d is a DESTINATION exit.  Cannot add any new From definition.\n\r", index);
                    send_to_char(buf, ch);
                    return false;
                }

                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit from # add {R<weight>{x <from-level> <from-exit>\n\r", ch);
                    send_to_char("         Please specify a positive number.\n\r", ch);
                    return false;
                }

                argument = one_argument(argument, arg6);
                if (!is_number(arg5))
                {
                    send_to_char("Syntax:  special exit from # add {R<weight>{x <from-level> <from-exit>\n\r", ch);
                    send_to_char("         Please specify a positive number.\n\r", ch);
                    return false;
                }

                int weight = atoi(arg6);
                if (weight < 1)
                {
                    send_to_char("Syntax:  special exit from # add {R<weight>{x <from-level> <from-exit>\n\r", ch);
                    send_to_char("         Please specify a positive number.\n\r", ch);
                    return false;
                }

                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit from # add <weight> {R<from-level>{x <from-exit>\n\r", ch);
                    sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                argument = one_argument(argument, arg7);
                if (!is_number(arg7))
                {
                    send_to_char("Syntax:  special exit from # add <weight> {R<from-level>{x <from-exit>\n\r", ch);
                    sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                int flindex = atoi(arg7);
                if (flindex < 1 || flindex > list_size(dng->levels))
                {
                    send_to_char("Syntax:  special exit from # add <weight> {R<from-level>{x <from-exit>\n\r", ch);
                    sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                DUNGEON_INDEX_LEVEL_DATA *flevel = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, flindex);
                int fexits = get_dungeon_index_level_special_exits(dng, flevel);

                if (!is_number(argument))
                {
                    send_to_char("Syntax:  special exit from # add <weight> <from-level> {R<from-exit>{x\n\r", ch);
                    sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
                    send_to_char(buf, ch);
                    return false;
                }

                int fexit = atoi(argument);
                if (fexit < 1 || fexit > fexits)
                {
                    send_to_char("Syntax:  special exit from # add <weight> <from-level> {R<from-exit>{x\n\r", ch);
                    sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
                    send_to_char(buf, ch);
                    return false;
                }

                add_dungeon_index_weighted_exit_data(ex->from, weight, flindex, fexit);
                ex->total_from += weight;

                sprintf(buf, "From definition added to special exit %d.\n\r", index);
                send_to_char(buf, ch);
                return false;
            }

            if (!str_prefix(arg4, "set"))
            {
                if (ex->mode == EXITMODE_STATIC || ex->mode == EXITMODE_WEIGHTED_DEST)
                {
                    if (list_size(ex->from) < 1)
                    {
                        send_to_char("Special exit appears to be missing necessary From definition.\n\r", ch);
                        return false;
                    }

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit from # set {R<weight>{x <from-level> <from-exit>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    argument = one_argument(argument, arg6);
                    if (!is_number(arg5))
                    {
                        send_to_char("Syntax:  special exit from # set {R<weight>{x <from-level> <from-exit>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    int weight = atoi(arg6);
                    if (weight < 1)
                    {
                        send_to_char("Syntax:  special exit from # set {R<weight>{x <from-level> <from-exit>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit from # set <weight> {R<from-level>{x <from-exit>\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    argument = one_argument(argument, arg7);
                    if (!is_number(arg7))
                    {
                        send_to_char("Syntax:  special exit from # set <weight> {R<from-level>{x <from-exit>\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    int flindex = atoi(arg7);
                    if (flindex < 1 || flindex > list_size(dng->levels))
                    {
                        send_to_char("Syntax:  special exit from # set <weight> {R<from-level>{x <from-exit>\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    DUNGEON_INDEX_LEVEL_DATA *flevel = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, flindex);
                    int fexits = get_dungeon_index_level_special_exits(dng, flevel);

                    if (!is_number(argument))
                    {
                        send_to_char("Syntax:  special exit from # set <weight> <from-level> {R<from-exit>{x\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
                        send_to_char(buf, ch);
                        return false;
                    }

                    int fexit = atoi(argument);
                    if (fexit < 1 || fexit > fexits)
                    {
                        send_to_char("Syntax:  special exit from # set <weight> <from-level> {R<from-exit>{x\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
                        send_to_char(buf, ch);
                        return false;
                    }

                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *fex = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(ex->from, 1);
                    ex->total_from -= fex->weight;
                    ex->total_from += weight;

                    fex->weight = weight;
                    fex->level = flindex;
                    fex->door = fexit;

                    sprintf(buf, "From definition set on special exit %d.\n\r", index);
                    send_to_char(buf, ch);
                    return true;
                }
                else
                {
                    if (list_size(ex->from) < 1)
                    {
                        send_to_char("Special exit has no From definition.\n\r", ch);
                        return false;
                    }

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit from # set {R#{x <weight> <from-level> <from-exit>\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
                        send_to_char(buf, ch);
                        return false;
                    }

                    argument = one_argument(argument, arg5);
                    if (!is_number(arg5))
                    {
                        send_to_char("Syntax:  special exit from # set {R#{x <weight> <from-level> <from-exit>\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
                        send_to_char(buf, ch);
                        return false;
                    }

                    int findex = atoi(arg5);
                    if (findex < 1 || findex > list_size(ex->from))
                    {
                        send_to_char("Syntax:  special exit from # set {R#{x <weight> <from-level> <from-exit>\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
                        send_to_char(buf, ch);
                        return false;
                    }

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit from # set # {R<weight>{x <from-level> <from-exit>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    argument = one_argument(argument, arg6);
                    if (!is_number(arg5))
                    {
                        send_to_char("Syntax:  special exit from # set # {R<weight>{x <from-level> <from-exit>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    int weight = atoi(arg6);
                    if (weight < 1)
                    {
                        send_to_char("Syntax:  special exit from # set # {R<weight>{x <from-level> <from-exit>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit from # set # <weight> {R<from-level>{x <from-exit>\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    argument = one_argument(argument, arg7);
                    if (!is_number(arg7))
                    {
                        send_to_char("Syntax:  special exit from # set # <weight> {R<from-level>{x <from-exit>\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    int flindex = atoi(arg7);
                    if (flindex < 1 || flindex > list_size(dng->levels))
                    {
                        send_to_char("Syntax:  special exit from # set # <weight> {R<from-level>{x <from-exit>\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    DUNGEON_INDEX_LEVEL_DATA *flevel = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, flindex);
                    int fexits = get_dungeon_index_level_special_exits(dng, flevel);

                    if (!is_number(argument))
                    {
                        send_to_char("Syntax:  special exit from # set # <weight> <from-level> {R<from-exit>{x\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
                        send_to_char(buf, ch);
                        return false;
                    }

                    int fexit = atoi(argument);
                    if (fexit < 1 || fexit > fexits)
                    {
                        send_to_char("Syntax:  special exit from # set # <weight> <from-level> {R<from-exit>{x\n\r", ch);
                        sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
                        send_to_char(buf, ch);
                        return false;
                    }

                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *fex = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(ex->from, findex);
                    ex->total_from -= fex->weight;
                    ex->total_from += weight;

                    fex->weight = weight;
                    fex->level = flindex;
                    fex->door = fexit;

                    sprintf(buf, "From definition %d set on special exit %d.\n\r", findex, index);
                    send_to_char(buf, ch);
                    return true;

                }
            }

            if (!str_prefix(arg4, "remove"))
            {
                char buf[MSL];
                if (ex->mode == EXITMODE_STATIC)
                {
                    sprintf(buf, "Special exit %d is a STATIC exit.  Cannot remove the From definition.\n\r", index);
                    send_to_char(buf, ch);
                    return false;
                }

                if (ex->mode == EXITMODE_WEIGHTED_DEST)
                {
                    sprintf(buf, "Special exit %d is a DESTINATION exit.  Cannot remove the From definition.\n\r", index);
                    send_to_char(buf, ch);
                    return false;
                }

                if (!is_number(argument))
                {
                    send_to_char("Syntax:  special exit from # remove {R#{x\n\r", ch);
                    sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
                    send_to_char(buf, ch);
                    return false;
                }

                int findex = atoi(argument);
                if (findex < 1 || findex > list_size(ex->from))
                {
                    send_to_char("Syntax:  special exit from # remove {R#{x\n\r", ch);
                    sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
                    send_to_char(buf, ch);
                    return false;
                }

                list_remnthlink(ex->from, findex, true);
                send_to_char("From definition removed from special exit.\n\r", ch);
                if (list_size(ex->from) < 1)
                    send_to_char("{RWarning:{x Please add a from definition for this exit to work.\n\r", ch);
                return true;
            }

            send_to_char("Syntax:  special exit from # {Rlist{x\n\r", ch);
            send_to_char("         special exit from # {Radd{x <weight> <from-level> <from-exit>\n\r", ch);
            send_to_char("         special exit from # {Rset{x # <weight> <from-level> <from-exit>\n\r", ch);
            send_to_char("         special exit from # {Rset{x <weight> <from-level> <from-exit> (for static and destination exits only)\n\r", ch);
            send_to_char("         special exit from # {Rremove{x #\n\r", ch);
            return false;
        }

        if (!str_prefix(arg2, "to"))
        {
            char buf[MSL];
            if (list_size(dng->special_exits) < 1)
            {
                send_to_char("Please add a special exit definition first.\n\r", ch);
                return false;
            }

            if (IS_NULLSTR(argument))
            {
                send_to_char("Syntax:  special exit to {R#{x list\n\r", ch);
                send_to_char("         special exit to {R#{x add <weight> <to-level> <to-entry>\n\r", ch);
                send_to_char("         special exit to {R#{x set # <weight> <to-level> <to-entry>\n\r", ch);
                send_to_char("         special exit to {R#{x set <weight> <to-level> <to-entry> (for static and source exits only)\n\r", ch);
                send_to_char("         special exit to {R#{x remove #\n\r", ch);
                sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(dng->special_exits));
                send_to_char(buf, ch);
                return false;
            }

            argument = one_argument(argument, arg3);
            if (!is_number(arg3))
            {
                send_to_char("Syntax:  special exit to {R#{x list\n\r", ch);
                send_to_char("         special exit to {R#{x add <weight> <to-level> <to-entry>\n\r", ch);
                send_to_char("         special exit to {R#{x set # <weight> <to-level> <to-entry>\n\r", ch);
                send_to_char("         special exit to {R#{x set <weight> <to-level> <to-entry> (for static and source exits only)\n\r", ch);
                send_to_char("         special exit to {R#{x remove #\n\r", ch);
                sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(dng->special_exits));
                send_to_char(buf, ch);
                return false;
            }

            int index = atoi(arg3);
            if (index < 1 || index > list_size(dng->special_exits))
            {
                send_to_char("Syntax:  special exit to {R#{x list\n\r", ch);
                send_to_char("         special exit to {R#{x add <weight> <to-level> <to-entry>\n\r", ch);
                send_to_char("         special exit to {R#{x set # <weight> <to-level> <to-entry>\n\r", ch);
                send_to_char("         special exit to {R#{x set <weight> <to-level> <to-entry> (for static and source exits only)\n\r", ch);
                send_to_char("         special exit to {R#{x remove #\n\r", ch);
                sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(dng->special_exits));
                send_to_char(buf, ch);
                return false;
            }

            DUNGEON_INDEX_SPECIAL_EXIT *ex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(dng->special_exits, index);

            if (ex->mode == EXITMODE_GROUP)
            {
                send_to_char("Cannot alter the To definitions on a GROUP exit.\n\r", ch);
                return false;
            }

            if (IS_NULLSTR(argument))
            {
                send_to_char("Syntax:  special exit to # {Rlist{x\n\r", ch);
                send_to_char("         special exit to # {Radd{x <weight> <to-level> <to-entry>\n\r", ch);
                send_to_char("         special exit to # {Rset{x # <weight> <to-level> <to-entry>\n\r", ch);
                send_to_char("         special exit to # {Rset{x <weight> <to-level> <to-entry> (for static and source exits only)\n\r", ch);
                send_to_char("         special exit to # {Rremove{x #\n\r", ch);
                return false;
            }

            argument = one_argument(argument, arg4);

            if (!str_prefix(arg4, "list"))
            {
                if (list_size(ex->to) > 0)
                {
                    if (!dngedit_render_weighted_exit_list(ch, ex->to,
                        "Special exit to-list output exceeded buffer limits.\n\r"))
                    {
                        return false;
                    }
                }
                else
                {
                    send_to_char("There are no To definitions on this special exit.\n\r", ch);
                }
                return false;
            }

            if (!str_prefix(arg4, "add"))
            {
                if (ex->mode == EXITMODE_STATIC)
                {
                    sprintf(buf, "Special exit %d is a STATIC exit.  Cannot add any new To definition.\n\r", index);
                    send_to_char(buf, ch);
                    return false;
                }

                if (ex->mode == EXITMODE_WEIGHTED_SOURCE)
                {
                    sprintf(buf, "Special exit %d is a SOURCE exit.  Cannot add any new To definition.\n\r", index);
                    send_to_char(buf, ch);
                    return false;
                }

                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit to # add {R<weight>{x <to-level> <to-entry>\n\r", ch);
                    send_to_char("         Please specify a positive number.\n\r", ch);
                    return false;
                }

                argument = one_argument(argument, arg6);
                if (!is_number(arg5))
                {
                    send_to_char("Syntax:  special exit to # add {R<weight>{x <to-level> <to-entry>\n\r", ch);
                    send_to_char("         Please specify a positive number.\n\r", ch);
                    return false;
                }

                int weight = atoi(arg6);
                if (weight < 1)
                {
                    send_to_char("Syntax:  special exit to # add {R<weight>{x <to-level> <to-entry>\n\r", ch);
                    send_to_char("         Please specify a positive number.\n\r", ch);
                    return false;
                }

                if (IS_NULLSTR(argument))
                {
                    send_to_char("Syntax:  special exit to # add <weight> {R<to-level>{x <to-entry>\n\r", ch);
                    sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                argument = one_argument(argument, arg7);
                if (!is_number(arg7))
                {
                    send_to_char("Syntax:  special exit to # add <weight> {R<to-level>{x <to-entry>\n\r", ch);
                    sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                int tlindex = atoi(arg7);
                if (tlindex < 1 || tlindex > list_size(dng->levels))
                {
                    send_to_char("Syntax:  special exit to # add <weight> {R<to-level>{x <to-entry>\n\r", ch);
                    sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                    send_to_char(buf, ch);
                    return false;
                }

                DUNGEON_INDEX_LEVEL_DATA *tlevel = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, tlindex);
                int tentries = get_dungeon_index_level_special_entrances(dng, tlevel);

                if (!is_number(argument))
                {
                    send_to_char("Syntax:  special exit to # add <weight> <to-level> {R<to-entry>{x\n\r", ch);
                    sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
                    send_to_char(buf, ch);
                    return false;
                }

                int tentry = atoi(argument);
                if (tentry < 1 || tentry > tentries)
                {
                    send_to_char("Syntax:  special exit to # add <weight> <to-level> {R<to-entry>{x\n\r", ch);
                    sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
                    send_to_char(buf, ch);
                    return false;
                }

                add_dungeon_index_weighted_exit_data(ex->to, weight, tlindex, tentry);
                ex->total_to += weight;

                sprintf(buf, "To definition added to special exit %d.\n\r", index);
                send_to_char(buf, ch);
                return false;
            }

            if (!str_prefix(arg4, "set"))
            {
                if (ex->mode == EXITMODE_STATIC || ex->mode == EXITMODE_WEIGHTED_SOURCE)
                {
                    if (list_size(ex->to) < 1)
                    {
                        send_to_char("Special exit appears to be missing necessary To definition.\n\r", ch);
                        return false;
                    }

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit to # set {R<weight>{x <to-level> <to-entry>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    argument = one_argument(argument, arg6);
                    if (!is_number(arg5))
                    {
                        send_to_char("Syntax:  special exit to # set {R<weight>{x <to-level> <to-entry>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    int weight = atoi(arg6);
                    if (weight < 1)
                    {
                        send_to_char("Syntax:  special exit to # set {R<weight>{x <to-level> <to-entry>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit to # set <weight> {R<to-level>{x <to-entry>\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    argument = one_argument(argument, arg7);
                    if (!is_number(arg7))
                    {
                        send_to_char("Syntax:  special exit to # set <weight> {R<to-level>{x <to-entry>\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    int tlindex = atoi(arg7);
                    if (tlindex < 1 || tlindex > list_size(dng->levels))
                    {
                        send_to_char("Syntax:  special exit to # set <weight> {R<to-level>{x <to-entry>\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    DUNGEON_INDEX_LEVEL_DATA *tlevel = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, tlindex);
                    int tentries = get_dungeon_index_level_special_entrances(dng, tlevel);

                    if (!is_number(argument))
                    {
                        send_to_char("Syntax:  special exit to # set <weight> <to-level> {R<to-entry>{x\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
                        send_to_char(buf, ch);
                        return false;
                    }

                    int tentry = atoi(argument);
                    if (tentry < 1 || tentry > tentries)
                    {
                        send_to_char("Syntax:  special exit to # set <weight> <to-level> {R<to-entry>{x\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
                        send_to_char(buf, ch);
                        return false;
                    }

                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *tex = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(ex->to, 1);
                    ex->total_to -= tex->weight;
                    ex->total_to += weight;

                    tex->weight = weight;
                    tex->level = tlindex;
                    tex->door = tentry;

                    sprintf(buf, "To definition set on special exit %d.\n\r", index);
                    send_to_char(buf, ch);
                    return true;
                }
                else
                {
                    if (list_size(ex->to) < 1)
                    {
                        send_to_char("Special exit has no To definition.\n\r", ch);
                        return false;
                    }

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit to # set {R#{x <weight> <to-level> <to-entry>\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
                        send_to_char(buf, ch);
                        return false;
                    }

                    argument = one_argument(argument, arg5);
                    if (!is_number(arg5))
                    {
                        send_to_char("Syntax:  special exit to # set {R#{x <weight> <to-level> <to-entry>\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
                        send_to_char(buf, ch);
                        return false;
                    }

                    int tindex = atoi(arg5);
                    if (tindex < 1 || tindex > list_size(ex->to))
                    {
                        send_to_char("Syntax:  special exit to # set {R#{x <weight> <to-level> <to-entry>\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
                        send_to_char(buf, ch);
                        return false;
                    }

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit to # set # {R<weight>{x <to-level> <to-entry>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    argument = one_argument(argument, arg6);
                    if (!is_number(arg5))
                    {
                        send_to_char("Syntax:  special exit to # set # {R<weight>{x <to-level> <to-entry>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    int weight = atoi(arg6);
                    if (weight < 1)
                    {
                        send_to_char("Syntax:  special exit to # set # {R<weight>{x <to-level> <to-entry>\n\r", ch);
                        send_to_char("         Please specify a positive number.\n\r", ch);
                        return false;
                    }

                    if (IS_NULLSTR(argument))
                    {
                        send_to_char("Syntax:  special exit to # set # <weight> {R<to-level>{x <to-entry>\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    argument = one_argument(argument, arg7);
                    if (!is_number(arg7))
                    {
                        send_to_char("Syntax:  special exit to # set # <weight> {R<to-level>{x <to-entry>\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    int tlindex = atoi(arg7);
                    if (tlindex < 1 || tlindex > list_size(dng->levels))
                    {
                        send_to_char("Syntax:  special exit to # set # <weight> {R<to-level>{x <to-entry>\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
                        send_to_char(buf, ch);
                        return false;
                    }

                    DUNGEON_INDEX_LEVEL_DATA *tlevel = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, tlindex);
                    int tentries = get_dungeon_index_level_special_entrances(dng, tlevel);

                    if (!is_number(argument))
                    {
                        send_to_char("Syntax:  special exit to # set # <weight> <to-level> {R<to-entry>{x\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
                        send_to_char(buf, ch);
                        return false;
                    }

                    int tentry = atoi(argument);
                    if (tentry < 1 || tentry > tentries)
                    {
                        send_to_char("Syntax:  special exit to # set # <weight> <to-level> {R<to-entry>{x\n\r", ch);
                        sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
                        send_to_char(buf, ch);
                        return false;
                    }

                    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *tex = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(ex->to, tindex);
                    ex->total_to -= tex->weight;
                    ex->total_to += weight;

                    tex->weight = weight;
                    tex->level = tlindex;
                    tex->door = tentry;

                    sprintf(buf, "To definition %d set on special exit %d.\n\r", tindex, index);
                    send_to_char(buf, ch);
                    return true;

                }
            }

            if (!str_prefix(arg4, "remove"))
            {
                if (ex->mode == EXITMODE_STATIC)
                {
                    sprintf(buf, "Special exit %d is a STATIC exit.  Cannot remove the To definition.\n\r", index);
                    send_to_char(buf, ch);
                    return false;
                }

                if (ex->mode == EXITMODE_WEIGHTED_SOURCE)
                {
                    sprintf(buf, "Special exit %d is a SOURCE exit.  Cannot remove the To definition.\n\r", index);
                    send_to_char(buf, ch);
                    return false;
                }

                if (!is_number(argument))
                {
                    send_to_char("Syntax:  special exit to # remove {R#{x\n\r", ch);
                    sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
                    send_to_char(buf, ch);
                    return false;
                }

                int tindex = atoi(argument);
                if (tindex < 1 || tindex > list_size(ex->to))
                {
                    send_to_char("Syntax:  special exit to # remove {R#{x\n\r", ch);
                    sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
                    send_to_char(buf, ch);
                    return false;
                }

                list_remnthlink(ex->to, tindex, true);
                send_to_char("To definition removed to special exit.\n\r", ch);
                if (list_size(ex->to) < 1)
                    send_to_char("{RWarning:{x Please add a To definition for this exit to work.\n\r", ch);
                return true;
            }

            send_to_char("Syntax:  special exit to # {Rlist{x\n\r", ch);
            send_to_char("         special exit to # {Radd{x <weight> <to-level> <to-entry>\n\r", ch);
            send_to_char("         special exit to # {Rset{x # <weight> <to-level> <to-entry>\n\r", ch);
            send_to_char("         special exit to # {Rset{x <weight> <to-level> <to-entry> (for static and destination exits only)\n\r", ch);
            send_to_char("         special exit to # {Rremove{x #\n\r", ch);
            return false;
        }

        if (!str_prefix(arg2, "remove"))
        {
            char buf[MSL];
            if (argument[0] == '\0')
            {
                send_to_char("Syntax:  special exit remove {R#{x\n\r", ch);
                sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(dng->special_exits));
                send_to_char(buf, ch);
                return false;
            }

            if (!is_number(argument))
            {
                send_to_char("Syntax:  special exit remove {R#{x\n\r", ch);
                sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(dng->special_exits));
                send_to_char(buf, ch);
                return false;
            }

            if (list_size(dng->special_exits) < 1)
            {
                send_to_char("There are no special exits.\n\r", ch);
                return false;
            }

            int index = atoi(argument);
            if (index < 1 || index > list_size(dng->special_exits))
            {
                send_to_char("Syntax:  special exit remove {R#{x\n\r", ch);
                sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(dng->special_exits));
                send_to_char(buf, ch);
                return false;
            }

            list_remnthlink(dng->special_exits, index, true);
            send_to_char("Special exit removed.\n\r", ch);
            return true;
        }

        send_to_char("Syntax:  special exit {Rlist{x\n\r", ch);
        send_to_char("         special exit {Radd{x static <from-level> <from-exit> <to-level> <to-entrance>\n\r", ch);
        send_to_char("         special exit {Radd{x source <to-level> <to-entrance>\n\r", ch);
        send_to_char("         special exit {Radd{x destination <from-level> <from-exit>\n\r", ch);
        send_to_char("         special exit {Radd{x weighted\n\r", ch);
        send_to_char("         special exit {Radd{x group\n\r", ch);
        send_to_char("         special exit {Rfrom{x # list\n\r", ch);
        send_to_char("         special exit {Rfrom{x # add <weight> <from-level> <from-exit>\n\r", ch);
        send_to_char("         special exit {Rfrom{x # set[ #] <weight> <from-level> <from-exit>\n\r", ch);
        send_to_char("         special exit {Rfrom{x # remove #\n\r", ch);
        send_to_char("         special exit {Rto{x # list\n\r", ch);
        send_to_char("         special exit {Rto{x # add <weight> <to-level> <to-entrance>\n\r", ch);
        send_to_char("         special exit {Rto{x # set[ #] <weight> <to-level> <to-entrance>\n\r", ch);
        send_to_char("         special exit {Rto{x # remove #\n\r", ch);
        send_to_char("         special exit {Rgroup{x # add static <from-level> <from-exit> <to-level> <to-entrance>\n\r", ch);
        send_to_char("         special exit {Rgroup{x # add source <to-level> <to-entrance>\n\r", ch);
        send_to_char("         special exit {Rgroup{x # add destination <from-level> <from-exit>\n\r", ch);
        send_to_char("         special exit {Rgroup{x # add weighted\n\r", ch);
        send_to_char("         special exit {Rgroup{x # add group\n\r", ch);
        send_to_char("         special exit {Rgroup{x # from # list\n\r", ch);
        send_to_char("         special exit {Rgroup{x # from # add <weight> <from-level> <from-exit>\n\r", ch);
        send_to_char("         special exit {Rgroup{x # from # set[ #] <weight> <from-level> <from-exit>\n\r", ch);
        send_to_char("         special exit {Rgroup{x # from # remove #\n\r", ch);
        send_to_char("         special exit {Rgroup{x # to # list\n\r", ch);
        send_to_char("         special exit {Rgroup{x # to # add <weight> <to-level> <to-entrance>\n\r", ch);
        send_to_char("         special exit {Rgroup{x # to # set[ #] <weight> <to-level> <to-entrance>\n\r", ch);
        send_to_char("         special exit {Rgroup{x # to # remove #\n\r", ch);
        send_to_char("         special exit {Rgroup{x # remove #\n\r", ch);
        send_to_char("         special exit {Rremove{x #\n\r", ch);
        return false;
    }


    dngedit_special(ch, "");
    return false;
}

DNGEDIT (dngedit_adddprog)
{
    int tindex, slot;
    DUNGEON_INDEX_DATA *dungeon;
    PROG_LIST *list;
    SCRIPT_DATA *code;
    char trigger[MAX_STRING_LENGTH];
    char phrase[MAX_STRING_LENGTH];
    char num[MAX_STRING_LENGTH];

    EDIT_DUNGEON(ch, dungeon);
    argument = one_argument(argument, num);
    argument = one_argument(argument, trigger);
    argument = one_argument(argument, phrase);

    if (num[0] == '\0' || trigger[0] =='\0' || phrase[0] =='\0')
    {
    send_to_char("Syntax:   adddprog [widevnum] [trigger] [phrase]\n\r",ch);
    return false;
    }

    if ((tindex = trigger_index(trigger, PRG_DPROG)) < 0) {
    send_to_char("Valid flags are:\n\r",ch);
    show_help(ch, "dprog");
    return false;
    }

    slot = trigger_table[tindex].slot;

    WNUM script_wnum;
    AREA_DATA *context = olc_relative_widevnum_context(dungeon->area, num);
    if (!parse_widevnum(num, context, &script_wnum)) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    if ((code = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_DPROG)) == NULL)
    {
    send_to_char("No such DUNGEONProgram.\n\r",ch);
    return false;
    }

    // Make sure this has a list of progs!
    if(!dungeon->progs) dungeon->progs = new_prog_bank();

    if (edit_trigger_exists(dungeon->progs, code, tindex, phrase)) {
        send_to_char("That trigger/phrase pair is already attached to that script on this dungeon.\n\r", ch);
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

    list_appendlink(dungeon->progs[slot], list);

    send_to_char("Dprog Added.\n\r",ch);
    return true;
}

DNGEDIT (dngedit_deldprog)
{
    DUNGEON_INDEX_DATA *dungeon;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    int group_idx, trig_idx;
    PROG_GROUP groups[MAX_PROG_GROUPS];
    int num_groups;

    EDIT_DUNGEON(ch, dungeon);

    if (!dungeon->progs) {
        send_to_char("This dungeon has no programs attached.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0') {
        send_to_char("Syntax:  deldprog <group#>\n\r", ch);
        send_to_char("         deldprog <group#> <trigger#>\n\r", ch);
        return false;
    }

    if (!is_number(arg1)) {
        send_to_char("Please specify a valid group number.\n\r", ch);
        return false;
    }

    group_idx = atoi(arg1);
    num_groups = prog_build_groups(dungeon->progs, groups, MAX_PROG_GROUPS, PRG_DPROG);

    if (group_idx < 1 || group_idx > num_groups) {
        send_to_char("Invalid group number.\n\r", ch);
        return false;
    }

    PROG_GROUP *group = &groups[group_idx - 1];

    if (arg2[0] == '\0') {
        // Delete entire group
        if (edit_delscript(dungeon->progs, group->script)) {
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
        if (edit_deltrigger_specific(dungeon->progs, group->script, entry->entry->trig_type, entry->entry->trig_phrase)) {
            send_to_char("Trigger removed from script group.\n\r", ch);
            return true;
        }
    }

    send_to_char("No such program or trigger found.\n\r", ch);
    return false;
}

DNGEDIT(dngedit_varset)
{
    DUNGEON_INDEX_DATA *dungeon;

    EDIT_DUNGEON(ch, dungeon);

    return olc_varset(&dungeon->index_vars, ch, argument, false);
}

DNGEDIT(dngedit_varclear)
{
    DUNGEON_INDEX_DATA *dungeon;

    EDIT_DUNGEON(ch, dungeon);

    return olc_varclear(&dungeon->index_vars, ch, argument, false);
}

DNGEDIT(dngedit_mingroup)
{
    DUNGEON_INDEX_DATA *dng;
    EDIT_DUNGEON(ch, dng);

    return olc_cmd_number(ch, argument, "Min Group",
        "Syntax:  mingroup <number>  (0 = no minimum)\n\r",
        &dng->min_group, 0, INT_MAX, NULL, NULL);
}

DNGEDIT(dngedit_maxgroup)
{
    DUNGEON_INDEX_DATA *dng;
    EDIT_DUNGEON(ch, dng);

    return olc_cmd_number(ch, argument, "Max Group",
        "Syntax:  maxgroup <number>  (0 = unlimited)\n\r",
        &dng->max_group, 0, INT_MAX, NULL, NULL);
}

DNGEDIT(dngedit_maxplayers)
{
    DUNGEON_INDEX_DATA *dng;
    EDIT_DUNGEON(ch, dng);

    return olc_cmd_number(ch, argument, "Max Players",
        "Syntax:  maxplayers <number>  (0 = unlimited)\n\r",
        &dng->max_players, 0, INT_MAX, NULL, NULL);
}

DNGEDIT(dngedit_deathrelease)
{
    DUNGEON_INDEX_DATA *dng;
    EDIT_DUNGEON(ch, dng);

    return olc_cmd_type_set(ch, argument, "Death Release", NULL,
        &dng->death_release, death_release_types, NULL, NULL);
}

DNGEDIT(dngedit_idletimeout)
{
    DUNGEON_INDEX_DATA *dng;
    EDIT_DUNGEON(ch, dng);

    return olc_cmd_number(ch, argument, "Idle Timeout",
        "Syntax:  idletimeout <minutes>  (0 = default 15 minutes)\n\r",
        &dng->idle_timeout, 0, INT_MAX, NULL, NULL);
}