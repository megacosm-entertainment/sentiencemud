/***************************************************************************
 *  redit.c - OLC Room Editor                                              *
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

extern bool redit_blueprint_oncreate;
extern bool change_exit(CHAR_DATA *ch, char *argument, int door);
extern int wear_bit(int loc);

/* Forward declarations for tab show functions */
static void redit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void redit_show_exits_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void redit_show_resets_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void redit_show_extra_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void redit_show_scripts_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);

/***************************************************************************
 * Framework Helpers                                                       *
 ***************************************************************************/

static AREA_DATA *redit_get_area(void *pEdit)
{
    return pEdit ? ((ROOM_INDEX_DATA *)pEdit)->area : NULL;
}

/***************************************************************************
 * Room Editor Command Table (moved from olc.c)                            *
 ***************************************************************************/

const struct olc_cmd_type redit_table[] =
{
    {   "?",            show_help           },
    {   "addcdesc",     redit_addcdesc      },
    {   "addrprog",     redit_addrprog      },
    {   "commands",     show_commands       },
    {   "comments",     redit_comments      },
    {   "coords",       redit_coords        },
    {   "create",       redit_create        },
    {   "delcdesc",     redit_delcdesc      },
    {   "delrprog",     redit_delrprog      },
    {   "description",  redit_desc          },
    {   "dislink",      redit_dislink       },
    {   "down",         redit_down          },
    {   "east",         redit_east          },
    {   "ed",           redit_ed            },
    {   "editcdesc",    redit_editcdesc     },
    {   "heal",         redit_heal          },
    {   "locale",       redit_locale        },
    {   "mana",         redit_mana          },
    {   "move",         redit_move          },
    {   "mreset",       redit_mreset        },
    {   "name",         redit_name          },
    {   "north",        redit_north         },
    {   "northeast",    redit_northeast     },
    {   "northwest",    redit_northwest     },
    {   "oreset",       redit_oreset        },
    {   "owner",        redit_owner         },
    {   "persist",      redit_persist       },
    {   "recall",       redit_recall        },
    {   "room",         redit_room          },
    {   "sector",       redit_sector        },
    {   "show",         redit_show          },
    {   "south",        redit_south         },
    {   "southeast",    redit_southeast     },
    {   "southwest",    redit_southwest     },
    {   "up",           redit_up            },
    {   "west",         redit_west          },
    {   "varset",       redit_varset        },
    {   "varclear",     redit_varclear      },
    {   NULL,           0                   }
};

/***************************************************************************
 * Editor Definition (Unified Framework)                                   *
 ***************************************************************************/

static const OLC_EDITOR_DEF redit_def = {
    .name           = "REdit",
    .editor_type    = ED_ROOM,
    .cmd_table      = redit_table,
    .show_fn        = redit_show,
    .tabs           = {
        .count = 5,
        .tabs = {
            { "General",  "Gen",  redit_show_general_tab },
            { "Exits",    "Exit", redit_show_exits_tab },
            { "Resets",   "Rst",  redit_show_resets_tab },
            { "Extra",    "Ext",  redit_show_extra_tab },
            { "Scripts",  "Scr",  redit_show_scripts_tab },
        }
    },
    .theme          = &olc_theme_world,
    .perm           = {
        .flags          = OLC_PERM_AREA_SECURITY,
    },
    .change_mode    = OLC_CHANGE_AREA_FLAG,
    .get_area_fn    = redit_get_area,
    .audit_changes  = true,
};

/***************************************************************************
 * Room Editor Interpreter — delegates to framework with clone check.      *
 ***************************************************************************/

void redit(CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *pRoom = ch->in_room;

    /* Clone rooms cannot be modified — allow 'done' through */
    if (pRoom && room_is_clone(pRoom)) {
        smash_tilde(argument);
        char cmd[MAX_INPUT_LENGTH];
        one_argument(argument, cmd);
        if (!str_cmp(cmd, "done"))
            edit_done(ch);
        return;
    }

    olc_editor_interp(ch, argument, &redit_def);
}

/***************************************************************************
 * Room Editor Entry Point (moved from olc.c)                              *
 ***************************************************************************/

void do_redit(CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *pRoom;
    char arg1[MAX_STRING_LENGTH];

    if (IS_NPC(ch))
        return;

    argument = one_argument(argument, arg1);

    pRoom = ch->in_room;

    if (!str_cmp(arg1, "reset"))
    {
        if (!has_access_area(ch, pRoom->area))
        {
            send_to_char("Insufficient security to reset - action logged.\n\r", ch);
            return;
        }

        reset_room(pRoom, true);
        send_to_char("Room reset.\n\r", ch);
        return;
    }
    else if (!str_cmp(arg1, "create"))
    {
        if (redit_create(ch, argument))
        {
            char_from_room(ch);
            char_to_room(ch, ch->desc->pEdit);
            SET_BIT(((ROOM_INDEX_DATA *)ch->desc->pEdit)->area->area_flags, AREA_CHANGED);
            olc_editor_enter(ch, &redit_def, ch->desc->pEdit, false);
        }
        return;
    }
    else if (!IS_NULLSTR(arg1))
    {
        WNUM wnum;
        if (!parse_widevnum(arg1, ch->in_room ? ch->in_room->area : NULL, &wnum)) {
            send_to_char("REdit: Invalid widevnum format. Use vnum, #vnum or area#vnum.\n\r", ch);
            return;
        }

        pRoom = get_room_index(wnum.pArea, wnum.vnum);

        if (!pRoom)
        {
            send_to_char("REdit: Room does not exist.\n\r", ch);
            return;
        }

        if (!IS_BUILDER(ch, pRoom->area))
        {
            send_to_char("REdit: Insufficient security to edit room - action logged.\n\r", ch);
            return;
        }

        char_from_room(ch);
        char_to_room(ch, pRoom);
    }
    else if (pRoom && IS_SET(pRoom->room_flag[1], ROOM_VIRTUAL_ROOM))
    {
        send_to_char("REdit: Virtual rooms may not be edited.\n\r", ch);
        return;
    }

    if (!IS_BUILDER(ch, pRoom->area))
    {
        send_to_char("REdit: Insufficient security to edit room - action logged.\n\r", ch);
        return;
    }

    olc_editor_enter(ch, &redit_def, (void *)pRoom, false);
}


/***************************************************************************
 * Tab Show Functions                                                      *
 ***************************************************************************/

static void redit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    ROOM_INDEX_DATA *pRoom = (ROOM_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&redit_def);

    olc_display_string(ctx, theme, "Name:", "name", pRoom->name);
    olc_display_string(ctx, theme, "Area:", NULL,
        formatf("[UID %5ld] %s", pRoom->area->uid, pRoom->area->name));

    if (IS_SET(pRoom->rs_room_flag[1], ROOM_VIRTUAL_ROOM)) {
        olc_display_infof(ctx, theme,
            "VRoom at ({W%ld{x, {W%ld{x), in wilds uid ({W%ld{x) '{W%s{x'",
            pRoom->x, pRoom->y, pRoom->wilds->uid, pRoom->wilds->name);
    } else {
        olc_display_number(ctx, theme, "Vnum:", NULL, pRoom->vnum);
        olc_display_string(ctx, theme, "Sector:", "sector",
            sector_name(room_rs_sector_type(pRoom)));
        if (pRoom->viewwilds)
            olc_display_infof(ctx, theme,
                "Map Coord at ({W%ld{x, {W%ld{x, {W%ld{x), wilds uid ({W%ld{x) '{W%s{x'",
                pRoom->x, pRoom->y, pRoom->z,
                pRoom->viewwilds->uid, pRoom->viewwilds->name);
    }

    olc_display_bool(ctx, theme, "Persist:", "persist", pRoom->persist);
    olc_display_string(ctx, theme, "Room flags:", "room",
        bitmatrix_string(room_flagbank, pRoom->rs_room_flag));

    if (pRoom->rs_heal_rate != 100 || pRoom->rs_mana_rate != 100
        || pRoom->rs_move_rate != 100) {
        olc_display_section(ctx, theme, "Regeneration Rates");
        olc_display_number(ctx, theme, "Health rec:", "heal",
            pRoom->rs_heal_rate);
        olc_display_number(ctx, theme, "Mana rec:", "mana",
            pRoom->rs_mana_rate);
        olc_display_number(ctx, theme, "Move rec:", "move",
            pRoom->rs_move_rate);
    }

    /* Recall location */
    if (rs_location_isset(&pRoom->rs_recall)) {
        if (pRoom->rs_recall.wuid) {
            WILDS_DATA *wilds = get_wilds_from_uid(NULL, pRoom->rs_recall.wuid);
            if (wilds)
                olc_display_infof(ctx, theme,
                    "{WRecall:{x      Wilds %s {R[{x%lu{R]{x at {R<{x%lu,%lu,%lu{R>{x",
                    wilds->name, pRoom->rs_recall.wuid,
                    pRoom->rs_recall.id[0], pRoom->rs_recall.id[1],
                    pRoom->rs_recall.id[2]);
            else
                olc_display_infof(ctx, theme,
                    "{WRecall:{x      Wilds ??? {R[{x%lu{R]{x",
                    pRoom->rs_recall.wuid);
        } else if (pRoom->rs_recall.id[0] > 0) {
            ROOM_INDEX_DATA *recall = get_room_index(pRoom->area,
                pRoom->rs_recall.id[0]);
            if (recall)
                olc_display_infof(ctx, theme,
                    "{WRecall:{x      Room {R[{x%5ld{R]{x %s",
                    pRoom->rs_recall.id[0], recall->name);
            else
                olc_display_infof(ctx, theme,
                    "{WRecall:{x      {R[{x%lu{R]{x none",
                    pRoom->rs_recall.id[0]);
        }
    }

    if (pRoom->locale)
        olc_display_number(ctx, theme, "Locale:", "locale", pRoom->locale);

    if (!IS_NULLSTR(pRoom->owner))
        olc_display_string(ctx, theme, "Owner:", "owner", pRoom->owner);

    if (!IS_NULLSTR(pRoom->home_owner))
        olc_display_string(ctx, theme, "Home owner:", NULL, pRoom->home_owner);

    olc_display_text(ctx, theme, "Description:", "description",
        pRoom->description);
    olc_display_text(ctx, theme, "Builder Comments:", "comments",
        pRoom->comments);
}

static void redit_show_exits_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    ROOM_INDEX_DATA *pRoom = (ROOM_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&redit_def);
    char buf[MAX_STRING_LENGTH];
    bool found = false;
    int door;

    olc_display_section(ctx, theme, "Exits");

    for (door = 0; door < MAX_DIR; door++) {
        EXIT_DATA *pexit = pRoom->exit[door];
        if (!pexit) continue;
        found = true;

        AREA_DATA *pArea = NULL;
        WILDS_DATA *pWilds = NULL;

        if (pRoom->wilds) {
            if (IS_SET(pexit->exit_info, EX_VLINK))
                sprintf(buf, "-{W%-9s{x to {W%6ld{x, Area Uid ({W%ld{x), '{W%s{x'\n\r",
                    capitalize(dir_name[door]),
                    pexit->u1.to_room ? pexit->u1.to_room->vnum : 0,
                    pexit->u1.to_room ? pexit->u1.to_room->area->uid : 0,
                    pexit->u1.to_room ? pexit->u1.to_room->area->name : "{RERROR");
            else
                sprintf(buf, "-{W%-9s{x to ({W%d{x,{W%d{x).\n\r",
                    capitalize(dir_name[door]),
                    pexit->wilds.x, pexit->wilds.y);
        } else {
            if (IS_SET(pexit->exit_info, EX_VLINK)) {
                pArea = get_area_from_uid(pexit->wilds.area_uid);
                pWilds = get_wilds_from_uid(pArea, pexit->wilds.wilds_uid);
                sprintf(buf, "-{W%-9s{x to ({W%d{x,{W%d{x), Wilds Uid ({W%ld{x), '{W%s{x'.\n\r",
                    capitalize(dir_name[door]),
                    pexit->wilds.x, pexit->wilds.y,
                    pWilds ? pWilds->uid : 0,
                    pWilds ? pWilds->name : "(null)");
                add_buf(ctx->buffer, buf);
                sprintf(buf, "                         Area Uid ({W%ld{x), '{W%s{x'.\n\r",
                    pArea ? pArea->uid : 0,
                    pArea ? pArea->name : "(null)");
            } else {
                sprintf(buf, "-{W%-9s{x to {W%6ld{x\n\r",
                    capitalize(dir_name[door]),
                    pexit->u1.to_room ? pexit->u1.to_room->vnum : 0);
            }
        }
        add_buf(ctx->buffer, buf);

        /* Format exit flags — capitalize any not in the reset state */
        {
            char word[MAX_INPUT_LENGTH];
            char reset_state[MAX_STRING_LENGTH];
            char *state;
            int i, length;
            bool ffound = false;

            strcpy(reset_state, flag_string(exit_flags, pexit->rs_flags));
            state = flag_string(exit_flags, pexit->exit_info);
            add_buf(ctx->buffer, "    Exit flags: [{W");

            for (;;) {
                state = one_argument(state, word);
                if (word[0] == '\0') {
                    add_buf(ctx->buffer, "{x]\n\r");
                    break;
                }
                if (str_infix(word, reset_state)) {
                    length = strlen(word);
                    for (i = 0; i < length; i++)
                        word[i] = UPPER(word[i]);
                }
                if (ffound)
                    add_buf(ctx->buffer, " ");
                add_buf(ctx->buffer, word);
                ffound = true;
            }
        }

        if (pexit->long_desc && pexit->long_desc[0] != '\0') {
            sprintf(buf, "    Exit Description:\n\r    {W%s{x\n\r",
                pexit->long_desc);
            add_buf(ctx->buffer, buf);
        }

        sprintf(buf, "    Keywords: [{W%s{x]\n\r"
                     "    Short Description: '{W%s{x'\n\r",
            pexit->keyword && pexit->keyword[0] != '\0'
                ? pexit->keyword : "(Not set)",
            pexit->short_desc && pexit->short_desc[0] != '\0'
                ? pexit->short_desc : "(Not set)");
        add_buf(ctx->buffer, buf);

        if (IS_SET(pexit->rs_flags, EX_ISDOOR)) {
            sprintf(buf, "    -Door Material: [{W%s{x] Strength: [{W%d{x]"
                         "  Lock Flags: [{W%s{x]  Key vnum: [{W%ld{x]"
                         " Pick chance: [{W%d%%{x]\n\r",
                pexit->door.material,
                pexit->door.strength,
                flag_string(lock_flags, pexit->door.lock.flags),
                pexit->door.lock.key_wnum.vnum,
                pexit->door.lock.pick_chance);
            add_buf(ctx->buffer, buf);
        } else {
            add_buf(ctx->buffer, "\n\r");
        }
    }

    if (!found)
        add_buf(ctx->buffer, "    {W(None set){x\n\r");
}

static void redit_show_resets_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    const OLC_EDITOR_THEME *theme = olc_get_theme(&redit_def);

    olc_display_section(ctx, theme, "Resets");
    add_buf(ctx->buffer,
        "M = mobile, R = room, O = object, P = pet, S = shopkeeper\n\r");
    add_buf(ctx->buffer, "(Resets displayed below footer)\n\r");
}

static void redit_show_extra_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    ROOM_INDEX_DATA *pRoom = (ROOM_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&redit_def);
    char buf[MAX_STRING_LENGTH];
    CONDITIONAL_DESCR_DATA *cd;
    int i;

    /* Extra descriptions */
    olc_display_section(ctx, theme, "Extra Descriptions");

    if (pRoom->extra_descr) {
        EXTRA_DESCR_DATA *ed;
        add_buf(ctx->buffer, "  Keywords: {r[{x");
        for (ed = pRoom->extra_descr; ed; ed = ed->next) {
            add_buf(ctx->buffer, ed->keyword);
            if (ed->next)
                add_buf(ctx->buffer, " ");
        }
        add_buf(ctx->buffer, "{r]{x\n\r");
    } else {
        add_buf(ctx->buffer, "  {D(none){x\n\r");
    }

    /* Conditional descriptions */
    if (pRoom->conditional_descr) {
        olc_display_section(ctx, theme, "Conditional Descriptions");

        add_buf(ctx->buffer, "{Y Num  Condition Phrase{x\n\r");
        add_buf(ctx->buffer, "{Y ---  --------- ------{x\n\r");

        for (i = 0, cd = pRoom->conditional_descr; cd; cd = cd->next, i++) {
            char phrase[MIL];

            if (cd->condition == CONDITION_HOUR
                || cd->condition == CONDITION_SCRIPT)
                sprintf(phrase, "%d", cd->phrase);
            else {
                strncpy(phrase,
                    condition_phrase_to_name(cd->condition, cd->phrase),
                    MIL - 1);
                phrase[MIL - 1] = '\0';
            }

            sprintf(buf, "{r[{x%3d{r]{x %-9s %s\n\r",
                i, condition_type_to_name(cd->condition), phrase);
            add_buf(ctx->buffer, buf);
        }
    }
}

static void redit_show_scripts_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    ROOM_INDEX_DATA *pRoom = (ROOM_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&redit_def);

    olc_display_scripts(ctx, theme, pRoom->progs->progs, PRG_RPROG,
        "RoomProg Vnum", "addrprog", "delrprog");

    olc_display_vars(ctx, theme, pRoom->index_vars, "varset", "varclear");
}

/***************************************************************************
 * Master Show Function — dispatches to active tab.                        *
 ***************************************************************************/

REDIT(redit_show)
{
    ROOM_INDEX_DATA *pRoom;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&redit_def);
    OLC_LAYOUT_CTX *ctx;
    int tab;

    EDIT_ROOM(ch, pRoom);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "REdit", pRoom->name,
        formatf("%ld", pRoom->vnum), &redit_def);

    /* Dispatch to active tab's show function */
    tab = ch->desc ? ch->desc->nEditTab : 0;
    if (tab >= 0 && tab < redit_def.tabs.count
        && redit_def.tabs.tabs[tab].show_fn) {
        redit_def.tabs.tabs[tab].show_fn(ch, ctx, (void *)pRoom);
    } else {
        redit_show_general_tab(ch, ctx, (void *)pRoom);
    }

    olc_display_footer(ctx, theme);
    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);

    /* Resets tab: display_resets sends directly to character */
    if (tab == 2 && ch->in_room && ch->in_room->reset_first)
        display_resets(ch);

    return false;
}

REDIT(redit_north)
{
    if (change_exit(ch, argument, DIR_NORTH))
    return true;

    return false;
}


REDIT(redit_west)
{
    if (change_exit(ch, argument, DIR_WEST))
    return true;

    return false;
}



REDIT(redit_south)
{
    if (change_exit(ch, argument, DIR_SOUTH))
    return true;

    return false;
}



REDIT(redit_east)
{
    if (change_exit(ch, argument, DIR_EAST))
    return true;

    return false;
}


REDIT(redit_southeast)
{
    if (change_exit(ch, argument, DIR_SOUTHEAST))
    return true;

    return false;
}


REDIT(redit_southwest)
{
    if (change_exit(ch, argument, DIR_SOUTHWEST))
    return true;

    return false;
}


REDIT(redit_northeast)
{
    if (change_exit(ch, argument, DIR_NORTHEAST))
    return true;

    return false;
}


REDIT(redit_northwest)
{
    if (change_exit(ch, argument, DIR_NORTHWEST))
    return true;

    return false;
}


REDIT(redit_up)
{
    if (change_exit(ch, argument, DIR_UP))
    return true;

    return false;
}


REDIT(redit_down)
{
    if (change_exit(ch, argument, DIR_DOWN))
    return true;

    return false;
}
/*
REDIT(redit_varset)
{
    ROOM_INDEX_DATA *pRoom;
    char name[MIL];
    char type[MIL];
    char yesno[MIL];
    bool saved;

    EDIT_ROOM(ch, pRoom);

    if (argument[0] == '\0') {
    send_to_char("Syntax:  varset <name> <number|string|room> <yes|no> <value>\n\r", ch);
    return false;
    }

    argument = one_argument(argument, name);
    argument = one_argument(argument, type);
    argument = one_argument(argument, yesno);

    if(!variable_validname(name)) {
    send_to_char("Variable names can only have alphabetical characters.\n\r", ch);
    return false;
    }

    saved = !str_cmp(yesno,"yes");

    if(!argument[0]) {
    send_to_char("Set what on the variable?\n\r", ch);
    return false;
    }

    if(!str_cmp(type,"room")) {
    if(!is_number(argument)) {
        send_to_char("Specify a room vnum.\n\r", ch);
        return false;
    }

    variables_setindex_room(&pRoom->index_vars,name,atoi(argument), saved);
    } else if(!str_cmp(type,"string"))
    variables_setindex_string(&pRoom->index_vars,name,argument,false, saved);
    else if(!str_cmp(type,"number")) {
    if(!is_number(argument)) {
        send_to_char("Specify an integer.\n\r", ch);
        return false;
    }

    variables_setindex_integer(&pRoom->index_vars,name,atoi(argument), saved);
    } else {
    send_to_char("Invalid type of variable.\n\r", ch);
    return false;
    }

    variable_copyto(&pRoom->index_vars,&pRoom->progs->vars,name,name,false);
    send_to_char("Variable set.\n\r", ch);
    return true;
}

REDIT(redit_varclear)
{
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);

    if (argument[0] == '\0') {
    send_to_char("Syntax:  varclear <name>\n\r", ch);
    return false;
    }

    if(!variable_validname(argument)) {
    send_to_char("Variable names can only have alphabetical characters.\n\r", ch);
    return false;
    }

    if(!variable_remove(&pRoom->index_vars,argument)) {
    send_to_char("No such variable defined.\n\r", ch);
    return false;
    }

    variable_remove(&pRoom->progs->vars,argument);
    send_to_char("Variable cleared.\n\r", ch);
    return true;
}
*/

REDIT(redit_varset)
{
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);

    if(olc_varset(&pRoom->index_vars, ch, argument, false))
    {
        // This will *NOT* update cloned rooms...
        olc_varset(&pRoom->progs->vars, ch, argument, true);
        return true;
    }
    return false;
}

REDIT(redit_varclear)
{
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);

    if(olc_varclear(&pRoom->index_vars, ch, argument, false))
    {
        // This will *NOT* update cloned rooms...
        olc_varclear(&pRoom->progs->vars, ch, argument, true);
        return true;
    }

    return false;
}

REDIT(redit_ed)
{
    ROOM_INDEX_DATA *pRoom;
    EXTRA_DESCR_DATA *ed;

    char command[MAX_INPUT_LENGTH];
    char keyword[MAX_INPUT_LENGTH];
    char copy_item[MAX_INPUT_LENGTH];

    EDIT_ROOM(ch, pRoom);

    argument = one_argument(argument, command);
    argument = one_argument(argument, keyword);
    one_argument(argument, copy_item);

    if (command[0] == '\0' || keyword[0] == '\0')
    {
    send_to_char("Syntax:  ed add [keyword]\n\r", ch);
    send_to_char("         ed edit [keyword]\n\r", ch);
    send_to_char("         ed show [keyword]\n\r", ch);
    send_to_char("         ed delete [keyword]\n\r", ch);
    send_to_char("         ed format [keyword]\n\r", ch);
    send_to_char("         ed copy existing_keyword new_keyword\n\r", ch);
    send_to_char("         ed environment [keyword]\n\r", ch);

    return false;
    }

    if (!str_cmp(command, "copy"))
    {
    EXTRA_DESCR_DATA *ed2;

        if (keyword[0] == '\0' || copy_item[0] == '\0')
    {
       send_to_char("Syntax:  ed copy existing_keyword new_keyword\n\r", ch);
       return false;
        }

    for (ed = pRoom->extra_descr; ed; ed = ed->next)
    {
        if (is_name(keyword, ed->keyword))
        break;
    }

    if (!ed)
    {
        send_to_char("REdit:  Extra description keyword not found.\n\r", ch);
        return false;
    }

    ed2			=   new_extra_descr();
    ed2->keyword		=   str_dup(copy_item);
    if( ed->description )
        ed2->description		= str_dup(ed->description);
    else
        ed2->description		= NULL;
    ed2->next		=   pRoom->extra_descr;
    pRoom->extra_descr	=   ed2;

    send_to_char("Done.\n\r", ch);

    return true;
    }

    if (!str_cmp(command, "environment"))
    {
    if (keyword[0] == '\0')
    {
        send_to_char("Syntax:  ed environment [keyword]\n\r", ch);
        return false;
    }

    ed			=   new_extra_descr();
    ed->keyword		=   str_dup(keyword);
    ed->description		= NULL;
    ed->next		=   pRoom->extra_descr;
    pRoom->extra_descr	=   ed;

    send_to_char("Enviromental extra description added.\n\r", ch);

    return true;
    }

    if (!str_cmp(command, "add"))
    {
    if (keyword[0] == '\0')
    {
        send_to_char("Syntax:  ed add [keyword]\n\r", ch);
        return false;
    }

    ed			=   new_extra_descr();
    ed->keyword		=   str_dup(keyword);
    ed->description		=   str_dup("");
    ed->next		=   pRoom->extra_descr;
    pRoom->extra_descr	=   ed;

    string_append(ch, &ed->description);

    return true;
    }


    if (!str_cmp(command, "edit"))
    {
    if (keyword[0] == '\0')
    {
        send_to_char("Syntax:  ed edit [keyword]\n\r", ch);
        return false;
    }

    for (ed = pRoom->extra_descr; ed; ed = ed->next)
    {
        if (is_name(keyword, ed->keyword))
        break;
    }

    if (!ed)
    {
        send_to_char("REdit:  Extra description keyword not found.\n\r", ch);
        return false;
    }

    if( !ed->description )
        ed->description = str_dup("");

    string_append(ch, &ed->description);

    return true;
    }


    if (!str_cmp(command, "delete"))
    {
    EXTRA_DESCR_DATA *ped = NULL;

    if (keyword[0] == '\0')
    {
        send_to_char("Syntax:  ed delete [keyword]\n\r", ch);
        return false;
    }

    for (ed = pRoom->extra_descr; ed; ed = ed->next)
    {
        if (is_name(keyword, ed->keyword))
        break;
        ped = ed;
    }

    if (!ed)
    {
        send_to_char("REdit:  Extra description keyword not found.\n\r", ch);
        return false;
    }

    if (!ped)
        pRoom->extra_descr = ed->next;
    else
        ped->next = ed->next;

    free_extra_descr(ed);

    send_to_char("Extra description deleted.\n\r", ch);
    return true;
    }


    if (!str_cmp(command, "format"))
    {
    if (keyword[0] == '\0')
    {
        send_to_char("Syntax:  ed format [keyword]\n\r", ch);
        return false;
    }

    for (ed = pRoom->extra_descr; ed; ed = ed->next)
    {
        if (is_name(keyword, ed->keyword))
        break;
    }

    if (!ed)
    {
        send_to_char("REdit:  Extra description keyword not found.\n\r", ch);
        return false;
    }

    if( !ed->description )
    {
        send_to_char("REdit:  Extra description is an environmental extra description.\n\r", ch);
        return false;
    }

    ed->description = format_string(ed->description);

    send_to_char("Extra description formatted.\n\r", ch);
    return true;
    }

    if (!str_cmp(command, "show"))
    {
    if (keyword[0] == '\0')
    {
        send_to_char("Syntax:  ed show [keyword]\n\r", ch);
        return false;
    }

    for (ed = pRoom->extra_descr; ed; ed = ed->next)
    {
        if (is_name(keyword, ed->keyword))
        break;
    }

    if (!ed)
    {
        send_to_char("REdit:  Extra description keyword not found.\n\r", ch);
        return false;
    }

    if (!ed->description)
    {
        send_to_char("REdit:  Cannot show environmental extra description.\n\r", ch);
        return false;
    }

    page_to_char(ed->description, ch);

    return true;
    }

    redit_ed(ch, "");
    return false;
}


REDIT(redit_create)
{
    AREA_DATA *pArea;
    ROOM_INDEX_DATA *pRoom;
    WNUM wnum;
    int iHash;

    if (IS_NULLSTR(argument))
    {
        send_to_char("Syntax: create <widevnum>\n\r", ch);
        return false;
    }

    // Try parsing as widevnum first
    if (!parse_widevnum(argument, ch->in_room->area, &wnum))
    {
        send_to_char("REdit:  Invalid widevnum format.\n\r", ch);
        return false;  
    }

    pArea = wnum.pArea;
    if (!pArea)
    {
        send_to_char("REdit:  That area does not exist, or that legacy vnum is not assigned to an area.\n\r", ch);
        return false;
    }

    if (!IS_BUILDER(ch, pArea))
    {
        send_to_char("REdit:  Vnum in an area you cannot build in.\n\r", ch);
        return false;
    }

    if (get_room_index(pArea, wnum.vnum))
    {
        send_to_char("REdit:  Room vnum already exists.\n\r", ch);
        return false;
    }

    pRoom			= new_room_index();
    pRoom->area			= pArea;
    list_appendlink(pArea->room_list, pRoom);	// Add to the area
    pRoom->vnum			= wnum.vnum;
    if (wnum.vnum > top_vnum_room)
        top_vnum_room = wnum.vnum;

    // Check whether to automatically set the room as blueprint
    if( redit_blueprint_oncreate )
    {
        ROOM_INDEX_DATA *pPrevRoom;

        EDIT_ROOM(ch, pPrevRoom);
        // Only copy if the new room is in the same area as the previous room
        if( pPrevRoom && pPrevRoom->area == pArea )
        {
            SET_BIT(pRoom->room_flag[1], ROOM_BLUEPRINT);
        }
        redit_blueprint_oncreate = false;
    }

    iHash			= wnum.vnum % MAX_KEY_HASH;
    pRoom->next			= pArea->room_index_hash[iHash];
    pArea->room_index_hash[iHash]	= pRoom;
    ch->desc->pEdit		= (void *)pRoom;

    SET_BIT(pRoom->area->area_flags, AREA_CHANGED);
    send_to_char("Room created.\n\r", ch);
    return true;
}


REDIT(redit_name)
{
    ROOM_INDEX_DATA *pRoom;
    EDIT_ROOM(ch, pRoom);

    return olc_cmd_string(ch, argument, "Name", NULL, &pRoom->name,
        OLC_STR_DEFAULT, NULL, NULL);
}


REDIT(redit_desc)
{
    ROOM_INDEX_DATA *pRoom;
    EDIT_ROOM(ch, pRoom);
    return olc_cmd_string_append(ch, argument, "description",
        NULL, &pRoom->description, NULL, NULL);
}

REDIT(redit_comments)
{
    ROOM_INDEX_DATA *pRoom;
    EDIT_ROOM(ch, pRoom);
    return olc_cmd_string_append(ch, argument, "comments",
        NULL, &pRoom->comments, NULL, NULL);
}

REDIT(redit_recall)
{
    ROOM_INDEX_DATA *pRoom;
    char arg1[MIL];
    char arg2[MIL];
    char arg3[MIL];
    char arg4[MIL];
    int vnum, x, y, z;

    EDIT_ROOM(ch, pRoom);

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);
    argument = one_argument(argument, arg4);

    if (!arg1[0]) {
        send_to_char("Syntax:  recall <widevnum>\n\r", ch);
        send_to_char("         recall <wuid> <x> <y> <z>\n\r", ch);
        return false;
    }

    // Try parsing as widevnum first
    WNUM wnum;
    if (parse_widevnum(arg1, pRoom->area, &wnum)) {
        // Room vnum format
        if(!arg2[0]) {
            if(!get_room_index(wnum.pArea, wnum.vnum)) {
                send_to_char("REdit:  Room vnum does not exist.\n\r", ch);
                return false;
            }
            rs_location_set(&pRoom->rs_recall,0,wnum.vnum,0,0);
            send_to_char("Recall set.\n\r", ch);
            return true;
        }
    }
    
    // Wilderness format: wuid x y z
    if (!is_number(arg1)) {
        send_to_char("Syntax:  recall <widevnum>\n\r", ch);
        send_to_char("         recall <wuid> <x> <y> <z>\n\r", ch);
        return false;
    }
    
    vnum = atoi(arg1);

    if(vnum < 1) {
        rs_location_clear(&pRoom->rs_recall);
        send_to_char("Recall cleared.\n\r", ch);
    } else if(!arg3[0] || !arg4[0] || !is_number(arg2) || !is_number(arg3) || !is_number(arg4)) {
        send_to_char("Syntax:  recall <widevnum>\n\r", ch);
        send_to_char("         recall <wuid> <x> <y> <z>\n\r", ch);
        return false;
    } else if(!get_wilds_from_uid(NULL,vnum)) {
        send_to_char("REdit:  Wilderness UID does not exist.\n\r", ch);
        return false;
    } else {
        x = atoi(arg2);
        y = atoi(arg3);
        z = atoi(arg4);
        rs_location_set(&pRoom->rs_recall,vnum,x,y,z);
        send_to_char("Recall set.\n\r", ch);
    }

    return true;
}


REDIT(redit_heal)
{
    ROOM_INDEX_DATA *pRoom;
    EDIT_ROOM(ch, pRoom);
    return olc_cmd_number(ch, argument, "heal rate",
        NULL, &pRoom->rs_heal_rate, INT_MIN, INT_MAX, NULL, NULL);
}


REDIT(redit_mana)
{
    ROOM_INDEX_DATA *pRoom;
    EDIT_ROOM(ch, pRoom);
    return olc_cmd_number(ch, argument, "mana rate",
        NULL, &pRoom->rs_mana_rate, INT_MIN, INT_MAX, NULL, NULL);
}


REDIT(redit_move)
{
    ROOM_INDEX_DATA *pRoom;
    EDIT_ROOM(ch, pRoom);
    return olc_cmd_number(ch, argument, "move rate",
        NULL, &pRoom->rs_move_rate, INT_MIN, INT_MAX, NULL, NULL);
}


REDIT(redit_mreset)
{
    ROOM_INDEX_DATA	*pRoom;
    MOB_INDEX_DATA	*pMobIndex;
    CHAR_DATA		*newmob;
    char		arg [ MAX_INPUT_LENGTH ];
    char		arg2 [ MAX_INPUT_LENGTH ];

    RESET_DATA		*pReset;
    char		output [ MAX_STRING_LENGTH ];

    EDIT_ROOM(ch, pRoom);

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0')
    {
    send_to_char ("Syntax:  mreset <widevnum> <max #x> <mix #x>\n\r", ch);
    return false;
    }

    WNUM mob_wnum;
    if (!parse_widevnum(arg, pRoom->area, &mob_wnum))
    {
    send_to_char("REdit: Invalid widevnum format. Use: vnum, area#vnum, or #vnum\n\r", ch);
    return false;
    }

    if (!(pMobIndex = get_mob_index(mob_wnum.pArea, mob_wnum.vnum)))
    {
    send_to_char("REdit: No mobile has that vnum.\n\r", ch);
    return false;
    }

    if (pMobIndex->area != pRoom->area)
    {
    send_to_char("REdit: No such mobile in this area.\n\r", ch);
    return false;
    }

    /*
     * Create the mobile reset.
     */
    pReset              = new_reset_data();
    pReset->command	= 'M';
    pReset->arg1.wnum.pArea = mob_wnum.pArea;
    pReset->arg1.wnum.vnum = mob_wnum.vnum;
    pReset->arg2	= is_number(arg2) ? atoi(arg2) : MAX_MOB;
    pReset->arg3.value	= pRoom->vnum;
    pReset->arg4	= is_number(argument) ? atoi (argument) : 1;
    add_reset(pRoom, pReset, 0/* Last slot*/);

    /*
     * Create the mobile.
     */
    newmob = create_mobile(pMobIndex, false);
    char_to_room(newmob, pRoom);
//    if (HAS_TRIGGER_MOB(newmob, TRIG_REPOP))
    p_percent_trigger(newmob, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
    sprintf(output, "%s (%s) has been loaded and added to resets.\n\r"
    "There will be a maximum of %ld loaded to this room.\n\r",
    capitalize(pMobIndex->short_descr),
    widevnum_string_mobile(pMobIndex, pRoom->area),
    pReset->arg2);
    send_to_char(output, ch);
    act("$n has created $N!", ch, newmob, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    return true;
}

REDIT(redit_oreset)
{
    ROOM_INDEX_DATA	*pRoom;
    OBJ_INDEX_DATA	*pObjIndex;
    OBJ_DATA		*newobj;
    OBJ_DATA		*to_obj;
    CHAR_DATA		*to_mob;
    char		arg1 [ MAX_INPUT_LENGTH ];
    char		arg2 [ MAX_INPUT_LENGTH ];
    int			olevel = 0;

    RESET_DATA		*pReset;
    char		output [ MAX_STRING_LENGTH ];

    EDIT_ROOM(ch, pRoom);

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0')
    {
    send_to_char ("Syntax:  oreset <widevnum> <args>\n\r", ch);
    send_to_char ("        -no_args               = into room\n\r", ch);
    send_to_char ("        -<obj_name>            = into obj\n\r", ch);
    send_to_char ("        -<mob_name> <wear_loc> = into mob\n\r", ch);
    return false;
    }

    WNUM obj_wnum;
    if (!parse_widevnum(arg1, pRoom->area, &obj_wnum))
    {
    send_to_char("REdit: Invalid widevnum format. Use: vnum, area#vnum, or #vnum\n\r", ch);
    return false;
    }

    if (!(pObjIndex = get_obj_index(obj_wnum.pArea, obj_wnum.vnum)))
    {
    send_to_char("REdit: No object has that vnum.\n\r", ch);
    return false;
    }

    if (pObjIndex->area != pRoom->area)
    {
    send_to_char("REdit: No such object in this area.\n\r", ch);
    return false;
    }

    /*
     * Load into room.
     */
    if (arg2[0] == '\0')
    {
    pReset		= new_reset_data();
    pReset->command	= 'O';
    pReset->arg1.wnum.pArea = obj_wnum.pArea;
    pReset->arg1.wnum.vnum	= obj_wnum.vnum;
    pReset->arg2	= 0;
    pReset->arg3.value	= pRoom->vnum;
    pReset->arg4	= 0;
    add_reset(pRoom, pReset, 0/* Last slot*/);

    newobj = create_object(pObjIndex, number_fuzzy(olevel), true);
    obj_to_room(newobj, pRoom);

    sprintf(output, "%s (%s) has been loaded and added to resets.\n\r",
        capitalize(pObjIndex->short_descr),
        widevnum_string_object(pObjIndex, pRoom->area));
    send_to_char(output, ch);
    }
    else
    /*
     * Load into object's inventory.
     */
    if (argument[0] == '\0'
    && ((to_obj = get_obj_list(ch, arg2, pRoom->contents)) != NULL))
    {
    pReset		= new_reset_data();
    pReset->command	= 'P';
    pReset->arg1.wnum.pArea = obj_wnum.pArea;
    pReset->arg1.wnum.vnum	= obj_wnum.vnum;
    pReset->arg2	= 0;
    AREA_DATA *container_area = to_obj->pIndexData->area;
    if (!container_area) container_area = get_system_area_fallback();
    pReset->arg3.wnum.pArea = container_area;
    pReset->arg3.wnum.vnum	= to_obj->pIndexData->vnum;
    pReset->arg4	= 1;
    add_reset(pRoom, pReset, 0/* Last slot*/);

    newobj = create_object(pObjIndex, number_fuzzy(olevel), true);
    newobj->cost = 0;
    obj_to_obj(newobj, to_obj);

    {
        char wnum1[64];
        strcpy(wnum1, widevnum_string_object(newobj->pIndexData, pRoom->area));
        sprintf(output, "%s (%s) has been loaded into "
            "%s (%s) and added to resets.\n\r",
            capitalize(newobj->short_descr),
            wnum1,
            to_obj->short_descr,
            widevnum_string_object(to_obj->pIndexData, pRoom->area));
    }
    send_to_char(output, ch);
    }
    else
    /*
     * Load into mobile's inventory.
     */
    if ((to_mob = get_char_room(ch, NULL, arg2)) != NULL)
    {
    int	wear_loc;

    /*
     * Make sure the location on mobile is valid.
     */
    if ((wear_loc = flag_value(wear_loc_flags, argument)) == NO_FLAG)
    {
        send_to_char("REdit: Invalid wear_loc.  '? wear-loc'\n\r", ch);
        return false;
    }

    /*
     * Disallow loading a sword(WEAR_WIELD) into WEAR_HEAD.
     */
    if (!IS_SET(pObjIndex->wear_flags, wear_bit(wear_loc)))
    {
        sprintf(output,
            "%s (%s) has wear flags: [%s]\n\r",
            capitalize(pObjIndex->short_descr),
            widevnum_string_object(pObjIndex, pRoom->area),
        flag_string(wear_flags, pObjIndex->wear_flags));
        send_to_char(output, ch);
        return false;
    }

    /*
     * Can't load into same position.
     */
    if (get_eq_char(to_mob, wear_loc))
    {
        send_to_char("REdit:  Object already equipped.\n\r", ch);
        return false;
    }

    pReset		= new_reset_data();
    pReset->arg1.wnum.pArea = obj_wnum.pArea;
    pReset->arg1.wnum.vnum	= obj_wnum.vnum;
    pReset->arg2	= wear_loc;
    if (pReset->arg2 == WEAR_NONE)
        pReset->command = 'G';
    else
        pReset->command = 'E';
    pReset->arg3.value	= wear_loc;

    add_reset(pRoom, pReset, 0/* Last slot*/);

    olevel  = URANGE(0, to_mob->level - 2, LEVEL_HERO);
        newobj = create_object(pObjIndex, number_fuzzy(olevel), true);

#if 0
    if (to_mob->pIndexData->pShop)	/* Shop-keeper? */
    {
        switch (pObjIndex->item_type)
        {
        default:		olevel = 0;				break;
        case ITEM_PILL:	olevel = number_range( 0, 10);	break;
        case ITEM_POTION:	olevel = number_range( 0, 10);	break;
        case ITEM_SCROLL:	olevel = number_range( 5, 15);	break;
        case ITEM_WAND:	olevel = number_range(10, 20);	break;
        case ITEM_STAFF:	olevel = number_range(15, 25);	break;
        case ITEM_TATTOO:	olevel = number_range( 0, 10);	break;
        case ITEM_ARMOUR:	olevel = number_range( 5, 15);	break;
        case ITEM_SEED:	olevel = number_range( 5, 15);	break;
        case ITEM_RANGED_WEAPON:	olevel = number_range( 5, 15);	break;
        case ITEM_WEAPON:	if (pReset->command == 'G')
                        olevel = number_range(5, 15);
                else
                    olevel = number_fuzzy(olevel);
        break;
        }

        newobj = create_object(pObjIndex, olevel, true);
        if (pReset->arg2 == WEAR_NONE)
        SET_BIT(newobj->extra[0], ITEM_INVENTORY);
    }
    else
#endif
        newobj = create_object(pObjIndex, number_fuzzy(olevel), true);


    obj_to_char(newobj, to_mob);
    if (pReset->command == 'E')
        equip_char(to_mob, newobj, pReset->arg3.value);

    {
        char wnum1[64];
        strcpy(wnum1, widevnum_string_object(pObjIndex, pRoom->area));
        sprintf(output, "%s (%s) has been loaded "
            "%s of %s (%s) and added to resets.\n\r",
            capitalize(pObjIndex->short_descr),
            wnum1,
            flag_string(wear_loc_strings, pReset->arg3.value),
            to_mob->short_descr,
            widevnum_string_mobile(to_mob->pIndexData, pRoom->area));
    }
    send_to_char(output, ch);
    }
    else	/* Display Syntax */
    {
    send_to_char("REdit:  That mobile isn't here.\n\r", ch);
    return false;
    }

    act("$n has created $p!", ch, NULL, NULL, newobj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    return true;
}

REDIT(redit_persist)
{
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);


    if (!str_cmp(argument,"on")) {
        if (ch->tot_level < (MAX_LEVEL-1)) {
            send_to_char("Insufficient security.  Department of Homeland Security has been notified.\n\r", ch);
            return false;
        }

        persist_addroom(pRoom);
        send_to_char("Persistance enabled.\n\r", ch);
    } else if (!str_cmp(argument,"off")) {
        persist_removeroom(pRoom);
        send_to_char("Persistance disabled.\n\r", ch);
    } else {
        send_to_char("Usage: persist on/off\n\r", ch);
        return false;
    }

    return true;
}

REDIT(redit_owner)
{
    ROOM_INDEX_DATA *pRoom;
    EDIT_ROOM(ch, pRoom);

    return olc_cmd_string(ch, argument, "Owner", NULL, &pRoom->owner,
        OLC_STR_CLEARABLE, NULL, NULL);
}




REDIT(redit_room)
{
    ROOM_INDEX_DATA *room;
    //long value;

        EDIT_ROOM(ch, room);

        long bits[2];
        if (!bitmatrix_lookup(argument, room_flagbank, bits))
        {
            send_to_char("Syntax:  room <flags>\n\r", ch);
            send_to_char("Type '? room' for list of flags.\n\r", ch);
            return false;
        }
        

        if( IS_SET(bits[1], ROOM_BLUEPRINT) )
        {
            // Only those that can edit blueprints can toggle this flag
            if( !can_edit_blueprints(ch) )
            {
                bits[1] &= ~ROOM_BLUEPRINT;

                if( !bits[1] )
                {
                    send_to_char("Syntax: room [flags]\n\r", ch);
                    return false;
                }
            }
            else if( !IS_SET(bits[1], ROOM_NOCLONE) && IS_SET(room->rs_room_flag[1], ROOM_NOCLONE) )
            {
                send_to_char("No-clone room cannot be used in blueprints.\n\r", ch);
                return false;
            }
            else if( IS_SET(bits[1], ROOM_NOCLONE) && !IS_SET(room->rs_room_flag[1], ROOM_NOCLONE) )
            {
                send_to_char("BLUEPRINT and NO_CLONE cannot mix.\n\r", ch);
                return false;
            }
        }

        if( IS_SET(bits[1], ROOM_NOCLONE) )
        {
            if( !IS_SET(bits[1], ROOM_BLUEPRINT) && IS_SET(room->rs_room_flag[1], ROOM_BLUEPRINT) )
            {
                send_to_char("Blueprint rooms cannot be no-clone.\n\r", ch);
                return false;
            }

            // Check if room is already used in a section
            if( get_blueprint_section_byroom(room->vnum) )
            {
                send_to_char("Room is currently used in a blueprint.\n\r", ch);
                // Clear it out, JIC
                if( IS_SET(room->rs_room_flag[1], ROOM_NOCLONE) )
                {
                    REMOVE_BIT(room->rs_room_flag[1], ROOM_NOCLONE);
                    return true;
                }

                return false;
            }
        }

        for(int i = 0; i < 2; i++)
               TOGGLE_BIT(room->rs_room_flag[i], bits[i]);

        send_to_char("Room flag(s) toggled.\n\r", ch);
        return true;
    
}

/*
REDIT(redit_room2)
{
    ROOM_INDEX_DATA *room;
    int value;

    EDIT_ROOM(ch, room);

    if ((value = flag_value(room2_flags, argument)) == NO_FLAG)
    {
        send_to_char("Syntax: room2 [flags]\n\r", ch);
        return false;
    }

    if( IS_SET(value, ROOM_BLUEPRINT) )
    {
        // Only those that can edit blueprints can toggle this flag
        if( !can_edit_blueprints(ch) )
        {
            value &= ~ROOM_BLUEPRINT;

            if( !value )
            {
                send_to_char("Syntax: room2 [flags]\n\r", ch);
                return false;
            }
        }
        else if( !IS_SET(value, ROOM_NOCLONE) && IS_SET(room->room2_flags, ROOM_NOCLONE) )
        {
            send_to_char("No-clone room cannot be used in blueprints.\n\r", ch);
            return false;
        }
        else if( IS_SET(value, ROOM_NOCLONE) && !IS_SET(room->room2_flags, ROOM_NOCLONE) )
        {
            send_to_char("BLUEPRINT and NO_CLONE cannot mix.\n\r", ch);
            return false;
        }
    }

    if( IS_SET(value, ROOM_NOCLONE) )
    {
        if( !IS_SET(value, ROOM_BLUEPRINT) && IS_SET(room->room2_flags, ROOM_BLUEPRINT) )
        {
            send_to_char("Blueprint rooms cannot be no-clone.\n\r", ch);
            return false;
        }

        // Check if room is already used in a section
        if( get_blueprint_section_byroom(room->vnum) )
        {
            send_to_char("Room is currently used in a blueprint.\n\r", ch);
            // Clear it out, JIC
            if( IS_SET(room->room2_flags, ROOM_NOCLONE) )
            {
                REMOVE_BIT(room->room2_flags, ROOM_NOCLONE);
                return true;
            }

            return false;
        }
    }

    TOGGLE_BIT(room->room2_flags, value);
    send_to_char("Room flags toggled.\n\r", ch);
    return true;
}
*/

REDIT(redit_sector)
{
    ROOM_INDEX_DATA *room;
    int value;
    char row[MSL];

    EDIT_ROOM(ch, room);

    // Another hack because the SECT_INSIDE is 0 or the same as FLAG_NONE
    if (!str_cmp(argument, "inside"))
    value = 0;
    else
    if ((value = sector_lookup(argument)) == NO_FLAG)
    {
    send_to_char("Syntax: sector [type]\n\r", ch);
    send_to_char("Available sectors:\n\r", ch);
    for (int i = 0; i < sector_count(); i++)
    {
        snprintf(row, sizeof(row), "  %-3d %s\n\r", i, sector_name(i));
        send_to_char(row, ch);
    }
    return false;
    }

    room_set_rs_sector_type(room, value);
    send_to_char("Sector type set.\n\r", ch);

    return true;
}


REDIT(redit_coords)
{
    char arg1[MIL];
    char arg2[MIL];
    char arg3[MIL];
    ROOM_INDEX_DATA *room;
    WILDS_DATA *w;
    int x,y,z;

    EDIT_ROOM(ch, room);

    if(room->wilds) {
        send_to_char("Wilderness rooms cannot be modified.\n\r",ch);
        return false;
    }

    if(IS_NULLSTR(argument)) {
        send_to_char("coords <x> <y> <z>[ <wilds uid>]\n\r",ch);
        send_to_char("<wilds uid> can be omitted if dealing with a blueprint room.\n\r", ch);
        return false;
    }

    if(!str_cmp(argument,"none")) {
        room->viewwilds = NULL;
        room->x = 0;
        room->y = 0;
        room->z = 0;
    } else {
        argument = one_argument(argument,arg1);
        argument = one_argument(argument,arg2);
        argument = one_argument(argument,arg3);


        x = atoi(arg1);
        y = atoi(arg2);
        z = atoi(arg3);

        if( !IS_SET(room->room_flag[1], ROOM_BLUEPRINT) )
        {
            w = get_wilds_from_uid(NULL,atoi(argument));
            if(!w) {
                send_to_char("No such wilderness.\n\r",ch);
                return false;
            }

            if(x < 0 || x >= w->map_size_x) {
                send_to_char("Invalid map coordinate.\n\r",ch);
                return false;
            }
            if(y < 0 || y >= w->map_size_y) {
                send_to_char("Invalid map coordinate.\n\r",ch);
                return false;
            }

            room->viewwilds = w;
        }
        else
            room->viewwilds = NULL;

        room->x = x;
        room->y = y;
        room->z = z;

        send_to_char("Coordinate set.\n\r", ch);
    }

    return true;
}

REDIT(redit_locale)
{
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);

    if (IS_NULLSTR(argument) || !is_number(argument)) {
        send_to_char("Syntax: locale <#locale>\n\r", ch);
        return false;
    }

    pRoom->locale = atoi(argument);
    send_to_char("Locale set.\n\r", ch);
    return true;
}





REDIT (redit_addrprog)
{
    int tindex, value, slot;
    PROG_LIST *list;
    SCRIPT_DATA *code;
    ROOM_INDEX_DATA *pRoom;
    char trigger[MAX_STRING_LENGTH];
    char phrase[MAX_STRING_LENGTH];
    char num[MAX_STRING_LENGTH];

    EDIT_ROOM(ch, pRoom);
    argument=one_argument(argument, num);
    argument=one_argument(argument, trigger);
    argument=one_argument(argument, phrase);

    if (num[0] == '\0' || trigger[0] =='\0' || phrase[0] =='\0')
    {
    send_to_char("Syntax:   addrprog [widevnum] [trigger] [phrase]\n\r",ch);
    return false;
    }

    WNUM script_wnum;
    if (!parse_widevnum(num, pRoom->area, &script_wnum)) {
        send_to_char("Invalid script widevnum format. Use: vnum, area#vnum, or #vnum\n\r", ch);
        return false;
    }

    if ((tindex = trigger_index(trigger, PRG_RPROG)) < 0) {
    send_to_char("Valid flags are:\n\r",ch);
    show_help(ch, "rprog");
    return false;
    }

    value = tindex;//trigger_table[tindex].value;
    slot = trigger_table[tindex].slot;

    if(value == TRIG_SPELLCAST) {
        if( !str_cmp(phrase, "*") )
        {
            strcpy(phrase, "0");
        }
        else
        {
            int sn = skill_lookup(phrase);
            if(sn < 0 || skill_table[sn].spell_fun == spell_null) {
                send_to_char("Invalid spell for trigger.\n\r",ch);
                return false;
            }
            sprintf(phrase,"%d",sn);
        }
    }
    else if( value == TRIG_EXIT ||
             value == TRIG_EXALL ||
             value == TRIG_OPEN ||
             value == TRIG_CLOSE ||
             value == TRIG_KNOCK ||
             value == TRIG_KNOCKING )
    {
        if( !str_cmp(phrase, "*") )
        {
            strcpy(phrase, "-1");
        }
        else
        {
            int door = parse_door(phrase);
            if( door < 0 ) {
                send_to_char("Invalid direction for exit/exall/open/close/knock/knocking trigger.\n\r", ch);
                return false;
            }
            sprintf(phrase,"%d",door);
        }
    }

    if ((code = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_RPROG)) == NULL)
    {
    send_to_char("No such ROOMProgram.\n\r",ch);
    return false;
    }

    // Make sure this has a list of progs!
    if(!pRoom->progs->progs) pRoom->progs->progs = new_prog_bank();

    if (edit_trigger_exists(pRoom->progs->progs, code, tindex, phrase)) {
        send_to_char("That trigger/phrase pair is already attached to that script on this room.\n\r", ch);
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
    //SET_BIT(pMob->mprog_flags,value);
    list_appendlink(pRoom->progs->progs[slot], list);

    send_to_char("Rprog Added.\n\r",ch);
    return true;
}


REDIT (redit_delrprog)
{
    ROOM_INDEX_DATA *pRoom;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    int group_idx, trig_idx;
    PROG_GROUP groups[MAX_PROG_GROUPS];
    int num_groups;

    EDIT_ROOM(ch, pRoom);

    if (!pRoom->progs->progs) {
        send_to_char("This room has no programs attached.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0') {
        send_to_char("Syntax:  delrprog <group#>\n\r", ch);
        send_to_char("         delrprog <group#> <trigger#>\n\r", ch);
        return false;
    }

    if (!is_number(arg1)) {
        send_to_char("Please specify a valid group number.\n\r", ch);
        return false;
    }

    group_idx = atoi(arg1);
    num_groups = prog_build_groups(pRoom->progs->progs, groups, MAX_PROG_GROUPS, PRG_RPROG);

    if (group_idx < 1 || group_idx > num_groups) {
        send_to_char("Invalid group number.\n\r", ch);
        return false;
    }

    PROG_GROUP *group = &groups[group_idx - 1];

    if (arg2[0] == '\0') {
        // Delete entire group (all triggers for this script)
        if (edit_delscript(pRoom->progs->progs, group->script)) {
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
        if (edit_deltrigger_specific(pRoom->progs->progs, group->script, entry->entry->trig_type, entry->entry->trig_phrase)) {
            send_to_char("Trigger removed from script group.\n\r", ch);
            return true;
        }
    }

    send_to_char("No such program or trigger found.\n\r", ch);
    return false;
}

REDIT(redit_addcdesc)
{
    int value;
    ROOM_INDEX_DATA *pRoom;
    char type[MSL];
    char phrase[MSL];
    CONDITIONAL_DESCR_DATA *cd;

    EDIT_ROOM(ch, pRoom);

    argument = one_argument(argument, type);
    argument = one_argument(argument, phrase);

    if (type[0] == '\0' || phrase[0] == '\0')
    {
    send_to_char("Syntax: addcdesc [type] [phrase]\n\r", ch);
    return false;
    }

    if ((value = flag_value(room_condition_flags, type)) == NO_FLAG)
    {
    send_to_char("Valid condition types are:\n\r", ch);
    show_help(ch, "condition");
    return false;
    }

    if (cd_phrase_lookup(value, phrase) == -1)
    {
    send_to_char("Invalid phrase.\n\r", ch);
    return false;
    }

    for (cd = pRoom->conditional_descr; cd != NULL; cd = cd->next)
    {
    if (cd->condition == value && cd->phrase == cd_phrase_lookup(value, phrase))
    {
        send_to_char("That would be redundant.\n\r", ch);
        return false;
    }
    }

    cd = new_conditional_descr();
    cd->condition = value;
    cd->phrase = cd_phrase_lookup(value, phrase);
    cd->next = pRoom->conditional_descr;
    pRoom->conditional_descr = cd;

    string_append(ch, &cd->description);

    return true;
}


REDIT(redit_dislink)
{
    ROOM_INDEX_DATA *pRoom;
    bool changed = false;

    EDIT_ROOM(ch, pRoom);

    if (!str_cmp(argument, "junk")) {
    free_string(pRoom->name);
    pRoom->name = str_dup("NULL");
    changed = true;
    }

    if (dislink_room(pRoom))
    {
    send_to_char("Room dislinked.\n\r", ch);
    changed = true;
    }
    else
    send_to_char("No exits to dislink.\n\r", ch);

    return changed;
}

REDIT(redit_delcdesc)
{
    CONDITIONAL_DESCR_DATA *cd;
    CONDITIONAL_DESCR_DATA *cd_prev;
    int i = 0;
    char cDesc[MSL];
    int value;
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);

    one_argument(argument, cDesc);
    if (!is_number(cDesc) || cDesc[0] == '\0')
    {
    send_to_char("Syntax: delcdesc [#cdesc]\n\r", ch);
    return false;
    }

    value = atoi(cDesc);
    if (value < 0)
    {
    send_to_char("Invalid value.\n\r", ch);
    return false;
    }

    cd_prev = NULL;
    for (cd = pRoom->conditional_descr; cd != NULL; cd = cd->next)
    {
    if (i == value)
        break;

    cd_prev = cd;
    i++;
    }

    if (cd == NULL)
    {
    send_to_char("Conditional description not found in list.\n\r", ch);
    return false;
    }

    if (cd_prev == NULL) // head of list
    {
    pRoom->conditional_descr = cd->next;
    }
    else
    {
    cd_prev->next = cd->next;
    }

    free_conditional_descr(cd);

    send_to_char("Conditional description removed.\n\r", ch);
    return true;
}


REDIT(redit_editcdesc)
{
    CONDITIONAL_DESCR_DATA *cd;
    ROOM_INDEX_DATA *pRoom;
    int i;
    char arg[MSL];
    int num;

    EDIT_ROOM(ch, pRoom);

    one_argument(argument, arg);
    if (arg[0] == '\0')
    {
    send_to_char("Syntax: editcdesc [#cdesc]\n\r", ch);
    return false;
    }

    num = atoi(arg);
    if (num < 0)
    {
    send_to_char("Invalid argument.\n\r", ch);
    return false;
    }

    i = 0;
    for (cd = pRoom->conditional_descr; cd != NULL; cd = cd->next)
    {
    if (i == num)
        break;

    i++;
    }

    if (cd == NULL)
    {
    send_to_char("Conditional description not found in list.\n\r", ch);
    return false;
    }

    string_append(ch, &cd->description);

    return true;
}