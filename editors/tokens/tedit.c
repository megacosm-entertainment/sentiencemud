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
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

/* Forward declarations */
static void tedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void tedit_show_values_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void tedit_show_scripts_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);

static AREA_DATA *tedit_get_area(void *pEdit)
{
    TOKEN_INDEX_DATA *token = (TOKEN_INDEX_DATA *)pEdit;
    return token ? token->area : NULL;
}

/*
 * Token Editor Command Table
 */
const struct olc_cmd_type tedit_table[] =
{
    {   "commands",   show_commands      },
    {   "?",          show_help           },
    {   "comments",   tedit_comments      },
    {   "create",     tedit_create        },
    {   "show",       tedit_show          },
    {   "name",       tedit_name          },
    {   "type",       tedit_type          },
    {   "flags",      tedit_flags         },
    {   "timer",      tedit_timer         },
    {   "ed",         tedit_ed            },
    {   "desc",       tedit_description   },
    {   "value",      tedit_value         },
    {   "valuename",  tedit_valuename     },
    {   "addtprog",   tedit_addtprog      },
    {   "deltprog",   tedit_deltprog      },
    {   "varset",     tedit_varset        },
    {   "varclear",   tedit_varclear      },
    {   NULL,         0                   }
};

/*
 * Token Editor Definition
 */
static const OLC_EDITOR_DEF tedit_def = {
    .name           = "TEdit",
    .editor_type    = ED_TOKEN,
    .cmd_table      = tedit_table,
    .show_fn        = tedit_show,
    .tabs           = {
        .count = 3,
        .tabs = {
            { "General",  "Gen", tedit_show_general_tab },
            { "Values",   "Val", tedit_show_values_tab },
            { "Scripts",  "Scr", tedit_show_scripts_tab },
        }
    },
    .theme          = &olc_theme_entity,
    .perm           = {
        .flags          = OLC_PERM_AREA_SECURITY,
    },
    .change_mode    = OLC_CHANGE_AREA_FLAG,
    .get_area_fn    = tedit_get_area,
    .audit_changes  = true,
};

/*
 * Token Editor Interpreter — delegates to framework.
 */
void tedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &tedit_def);
}

/*
 * Token Editor Entry Point
 */
void do_tedit(CHAR_DATA *ch, char *argument)
{
    TOKEN_INDEX_DATA *token_index = NULL;
    char arg[MAX_STRING_LENGTH];

    if (IS_NPC(ch))
        return;

    argument = one_argument(argument, arg);

    if (arg[0] != '\0' && str_cmp(arg, "create"))
    {
        WNUM wnum;
        AREA_DATA *context = ch->in_room ? ch->in_room->area : NULL;
        if (!parse_widevnum(arg, context, &wnum)) {
            send_to_char("Invalid widevnum format. Use vnum, #vnum or area#vnum.\n\r", ch);
            return;
        }

        if ((token_index = get_token_index(wnum.pArea, wnum.vnum)) == NULL)
        {
            send_to_char("That token vnum does not exist.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &tedit_def, (void *)token_index, true);
    }
    else if (!str_cmp(arg, "create"))
    {
        if (tedit_create(ch, argument))
            olc_editor_enter(ch, &tedit_def, ch->desc->pEdit, false);
    }
    else
    {
        send_to_char(
            "Syntax: tedit <vnum>\n\r"
            "        tedit create <vnum>\n\r", ch);
    }
}


TEDIT(tedit_create)
{
    TOKEN_INDEX_DATA *token_index;
    TOKEN_INDEX_DATA *temp_token;
    AREA_DATA *pArea;
    long value;
    int iHash;

    // Auto-vnum: empty or "0" finds next available
    if (argument[0] == '\0' || !str_cmp(argument, "0"))
    {
        pArea = ch->in_room->area;
        value = 1;
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
        {
            for (temp_token = pArea->token_index_hash[iHash]; temp_token; temp_token = temp_token->next)
            {
                if (temp_token->vnum >= value)
                    value = temp_token->vnum + 1;
            }
        }

        if (value <= 0)
        {
            send_to_char("Unable to allocate a new token vnum.\n\r", ch);
            return false;
        }
    }
    else
    {
        WNUM wnum;
        AREA_DATA *context = ch->in_room->area;
        if (!parse_widevnum(argument, context, &wnum)) {
            send_to_char("Invalid vnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        value = wnum.vnum;
        pArea = wnum.pArea;
    }

    if (!pArea)
    {
        send_to_char("That vnum is not assigned an area.\n\r", ch);
        return false;
    }

    if (!IS_BUILDER(ch, pArea))
    {
        send_to_char("You aren't a builder in that area.\n\r", ch);
        return false;
    }

    if (get_token_index(pArea, value))
    {
        send_to_char("Token vnum already exists.\n\r", ch);
        return false;
    }

    token_index = new_token_index();
    token_index->vnum = value;
    token_index->area = pArea;
    iHash = value % MAX_KEY_HASH;
    token_index->next = pArea->token_index_hash[iHash];
    pArea->token_index_hash[iHash] = token_index;
    ch->desc->pEdit = (void *)token_index;

    send_to_char("Token Created.\n\r", ch);
    SET_BIT(token_index->area->area_flags, AREA_CHANGED);
    return true;
}


/*
 * Tab-specific display functions for token editor
 */

static void tedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    TOKEN_INDEX_DATA *token_index = (TOKEN_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&tedit_def);

    olc_display_string(ctx, theme, "Name:",  "name",  token_index->name);
    olc_display_string(ctx, theme, "Area:",  NULL,    token_index->area->name);
    olc_display_string(ctx, theme, "Vnum:",  NULL,    widevnum_string_token(token_index, token_index->area));
    olc_display_string(ctx, theme, "Type:",  "type",  token_table[token_index->type].name);
    olc_display_flags(ctx, theme,  "Flags:", "flags", token_flags, token_index->flags);
    olc_display_number(ctx, theme, "Timer:", "timer", token_index->timer);

    olc_display_text(ctx, theme, "Description:", "desc", token_index->description);
    olc_display_text(ctx, theme, "Builder Comments:", "comments", token_index->comments);
}

static void tedit_show_values_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    TOKEN_INDEX_DATA *token_index = (TOKEN_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&tedit_def);
    char label[MIL];
    char cmd[MIL];

    olc_display_section(ctx, theme, "Default Values");

    for (int i = 0; i < MAX_TOKEN_VALUES; i++) {
        const char *value_name = token_index_getvaluename(token_index, i);

        if (value_name && value_name[0] != '\0') {
            sprintf(label, "Value [%d]: %s", i, value_name);
        } else {
            sprintf(label, "Value [%d]:", i);
        }

        sprintf(cmd, "value %d", i);
        olc_display_number(ctx, theme, label, cmd, token_index->value[i]);
    }

    olc_display_vars(ctx, theme, token_index->index_vars, "varset", "varclear");
}

static void tedit_show_scripts_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    TOKEN_INDEX_DATA *token_index = (TOKEN_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&tedit_def);

    olc_display_scripts(ctx, theme, token_index->progs, PRG_TPROG,
        "TokProg Vnum", "addtprog", "deltprog");
}

TEDIT(tedit_show)
{
    TOKEN_INDEX_DATA *token_index;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&tedit_def);
    OLC_LAYOUT_CTX *ctx;
    int tab;

    EDIT_TOKEN(ch, token_index);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "TEdit", token_index->name,
        formatf("%s", widevnum_string_token(token_index, token_index->area)), &tedit_def);

    /* Dispatch to active tab's show function */
    tab = olc_show_all_tabs_mode(ch) ? -1 : (ch->desc ? ch->desc->nEditTab : 0);
    if (tab < 0) {
        for (int i = 0; i < tedit_def.tabs.count; i++) {
            if (tedit_def.tabs.tabs[i].show_fn)
                tedit_def.tabs.tabs[i].show_fn(ch, ctx, (void *)token_index);
        }
    } else if (tab >= 0 && tab < tedit_def.tabs.count && tedit_def.tabs.tabs[tab].show_fn) {
        tedit_def.tabs.tabs[tab].show_fn(ch, ctx, (void *)token_index);
    } else {
        tedit_show_general_tab(ch, ctx, (void *)token_index);
    }

    olc_display_footer(ctx, theme);
    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}


TEDIT(tedit_name)
{
    TOKEN_INDEX_DATA *token_index;
    EDIT_TOKEN(ch, token_index);
    return olc_cmd_string(ch, argument, "Name", NULL, &token_index->name,
        OLC_STR_DEFAULT, NULL, NULL);
}


TEDIT(tedit_type)
{
    TOKEN_INDEX_DATA *token_index;
    int i;

    EDIT_TOKEN(ch, token_index);

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  type [general|quest|affect|skill|spell]\n\r", ch);
    return false;
    }

    for (i = 0; token_table[i].name != NULL; i++) {
    if (!str_prefix(argument, token_table[i].name))
        break;
    }

    if (token_table[i].name == NULL) {
    send_to_char("That token type doesn't exist.\n\r", ch);
    return false;
    }

    if(i == TOKEN_SPELL && ch->tot_level < MAX_LEVEL) {
        send_to_char("Only IMPs can make spell tokens.\n\r", ch);
        return false;
    }

    if(i == TOKEN_SONG && ch->tot_level < MAX_LEVEL) {
        send_to_char("Only IMPs can make song tokens.\n\r", ch);
        return false;
    }

    token_index->type = token_table[i].type;
    act("Set token type to $t.", ch, NULL, NULL, NULL, NULL, token_table[i].name, NULL, TO_CHAR, NULL, NULL);
    return true;
}


TEDIT(tedit_flags)
{
    TOKEN_INDEX_DATA *token_index;
    EDIT_TOKEN(ch, token_index);
    return olc_cmd_flag_toggle(ch, argument, "Token Flags",
        "Syntax:  flags [token flag]\n\rType '? tokenflags' for a list of flags.\n\r",
        &token_index->flags, token_flags, NULL, NULL);
}


TEDIT(tedit_timer)
{
    TOKEN_INDEX_DATA *token_index;
    EDIT_TOKEN(ch, token_index);
    return olc_cmd_number(ch, argument, "Timer",
        "Syntax:  timer [number of ticks]  (0 to 65000)\n\r",
        &token_index->timer, 0, 65000, NULL, NULL);
}


TEDIT(tedit_ed)
{
    TOKEN_INDEX_DATA *token_index;
    EXTRA_DESCR_DATA *ed;
    char command[MAX_INPUT_LENGTH];
    char keyword[MAX_INPUT_LENGTH];
    char copy_item[MAX_INPUT_LENGTH];

    return false; // ED not implemented yet

    EDIT_TOKEN(ch, token_index);

    argument = one_argument(argument, command);
    argument = one_argument(argument, keyword);
    one_argument(argument, copy_item);

    if (command[0] == '\0' || keyword[0] == '\0')
    {
    send_to_char("Syntax:  ed add [keyword]\n\r", ch);
    send_to_char("         ed edit [keyword]\n\r", ch);
    send_to_char("         ed delete [keyword]\n\r", ch);
    send_to_char("         ed show [keyword]\n\r", ch);
    send_to_char("         ed format [keyword]\n\r", ch);
    send_to_char("         ed copy existing_keyword new_keyword\n\r", ch);
    send_to_char("         ed environment [keyword]\n\r", ch);


    return false;
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
    ed->next		=   token_index->ed;
    token_index->ed	=   ed;

    send_to_char("Enviromental extra description added.\n\r", ch);

    return true;
    }

    if (!str_cmp(command, "copy"))
    {
    EXTRA_DESCR_DATA *ed2;

        if (keyword[0] == '\0' || copy_item[0] == '\0')
    {
       send_to_char("Syntax:  ed copy existing_keyword new_keyword\n\r", ch);
       return false;
        }

    for (ed = token_index->ed; ed; ed = ed->next)
    {
        if (is_name(keyword, ed->keyword))
        break;
    }

    if (!ed)
    {
        send_to_char("TEdit:  Extra description keyword not found.\n\r", ch);
        return false;
    }

    ed2			=   new_extra_descr();
    ed2->keyword		=   str_dup(copy_item);
    if( ed->description )
        ed2->description		= str_dup(ed->description);
    else
        ed2->description		= NULL;
    ed2->next		=   token_index->ed;
    token_index->ed	=   ed2;

    send_to_char("Done.\n\r", ch);

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
    ed->next		=   token_index->ed;
    token_index->ed	=   ed;

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

    for (ed = token_index->ed; ed; ed = ed->next)
    {
        if (is_name(keyword, ed->keyword))
        break;
    }

    if (!ed)
    {
        send_to_char("TEdit:  Extra description keyword not found.\n\r", ch);
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

    for (ed = token_index->ed; ed; ed = ed->next)
    {
        if (is_name(keyword, ed->keyword))
        break;
        ped = ed;
    }

    if (!ed)
    {
        send_to_char("TEdit:  Extra description keyword not found.\n\r", ch);
        return false;
    }

    if (!ped)
        token_index->ed = ed->next;
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

    for (ed = token_index->ed; ed; ed = ed->next)
    {
        if (is_name(keyword, ed->keyword))
        break;
    }

    if (!ed)
    {
        send_to_char("TEDIT:  Extra description keyword not found.\n\r", ch);
        return false;
    }

    if( !ed->description )
    {
        send_to_char("TEdit:  Extra description is an environmental extra description.\n\r", ch);
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

        for (ed = token_index->ed; ed; ed = ed->next)
        {
            if (is_name(keyword, ed->keyword))
                break;
        }

        if (!ed)
        {
            send_to_char("TEdit:  Extra description keyword not found.\n\r", ch);
            return false;
        }

        if (!ed->description)
        {
            send_to_char("TEdit:  Cannot show environmental extra description.\n\r", ch);
            return false;
        }

        page_to_char(ed->description, ch);

        return true;
    }

    tedit_ed(ch, "");
    return true;
}


TEDIT(tedit_description)
{
    TOKEN_INDEX_DATA *token_index;
    EDIT_TOKEN(ch, token_index);
    return olc_cmd_string_append(ch, argument, "Description", NULL,
        &token_index->description, NULL, NULL);
}

TEDIT(tedit_comments)
{
    TOKEN_INDEX_DATA *token_index;
    EDIT_TOKEN(ch, token_index);
    return olc_cmd_string_append(ch, argument, "Comments", NULL,
        &token_index->comments, NULL, NULL);
}

TEDIT(tedit_value)
{
    TOKEN_INDEX_DATA *token_index;
    char arg[MSL];
    char arg2[MSL];
    char buf[MSL];
    int value_num;
    unsigned long value_value;

    EDIT_TOKEN(ch, token_index);

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0') {
    send_to_char("Syntax:  value <number> <value>\n\r", ch);
    return false;
    }

    if ((value_num = atoi(arg)) < 0 || value_num >= MAX_TOKEN_VALUES) {
    sprintf(buf, "Number must be 0-%d\n\r", MAX_TOKEN_VALUES);
    send_to_char(buf, ch);
    return false;
    }

    if(token_index->type == TOKEN_SPELL) {
        switch(value_num) {
        case TOKVAL_SPELL_RATING:			// Max rating
            value_value = atoi(arg2);
            if(value_value < 0 || value_value > 1000000) {
                send_to_char("Max rating must be within range of 0 (for 100%%) to 1000000.\n\r", ch);
                return false;
            }

            send_to_char("Max rating set.\n\r", ch);
            break;
        case TOKVAL_SPELL_DIFFICULTY:			// Difficulty
            value_value = atoi(arg2);
            if(value_value < 1 || value_value > 1000000) {
                send_to_char("Difficulty must be within range of 1 to 1000000.\n\r", ch);
                return false;
            }

            send_to_char("Difficulty set.\n\r", ch);
            break;
        case TOKVAL_SPELL_TARGET:			// Target Type
            value_value = flag_value(spell_target_types, arg2);
            if(value_value == NO_FLAG) value_value = TAR_IGNORE;

            send_to_char("Target type set.\n\r", ch);
            break;
        case TOKVAL_SPELL_POSITION:			// Minimum Position

            if ((value_value = flag_value(position_flags, arg2)) == NO_FLAG) {
                send_to_char("Invalid position for spell.\n\r", ch);
                return false;
            }

            send_to_char("Minimum position set.\n\r", ch);
            break;
        case TOKVAL_SPELL_MANA:			// Mana cost
            value_value = atoi(arg2);
            if(value_value < 0 || value_value > 1000000) {
                send_to_char("Mana cost must be within range of 0 to 1000000.\n\r", ch);
                return false;
            }

            send_to_char("Mana cost set.\n\r", ch);
            break;
        case TOKVAL_SPELL_LEARN:			// Learn cost
            value_value = atoi(arg2);
            if(value_value < 0 || value_value > 1000000) {
                send_to_char("Learn cost must be within range of 0 to 1000000.\n\r", ch);
                return false;
            }

            send_to_char("Learn cost set.\n\r", ch);
            break;
        default:
            // Need to check for various things.
            value_value = atol(arg2);

            sprintf(buf, "Set value %d to %ld.\n\r", value_num, value_value);
            send_to_char(buf, ch);
            break;
        }

        token_index->value[value_num] = value_value;
    } else if(token_index->type == TOKEN_SKILL) {
        switch(value_num) {
        case TOKVAL_SPELL_RATING:			// Max rating
            value_value = atoi(arg2);
            if(value_value < 0 || value_value > 1000000) {
                send_to_char("Max rating must be within range of 0 (for 100%%) to 1000000.\n\r", ch);
                return false;
            }

            send_to_char("Max rating set.\n\r", ch);
            break;
        case TOKVAL_SPELL_DIFFICULTY:			// Difficulty
            value_value = atoi(arg2);
            if(value_value < 1 || value_value > 1000000) {
                send_to_char("Difficulty must be within range of 1 to 1000000.\n\r", ch);
                return false;
            }

            send_to_char("Difficulty set.\n\r", ch);
            break;
        case TOKVAL_SPELL_LEARN:			// Learn cost
            value_value = atoi(arg2);
            if(value_value < 0 || value_value > 1000000) {
                send_to_char("Learn cost must be within range of 0 to 1000000.\n\r", ch);
                return false;
            }

            send_to_char("Learn cost set.\n\r", ch);
            break;
        default:
            // Need to check for various things.
            value_value = atol(arg2);

            sprintf(buf, "Set value %d to %ld.\n\r", value_num, value_value);
            send_to_char(buf, ch);
            break;
        }

        token_index->value[value_num] = value_value;
    } else if(token_index->type == TOKEN_SONG) {
        switch(value_num) {
        case TOKVAL_SPELL_TARGET:			// Target Type
            value_value = flag_value(song_target_types, arg2);
            //Not sure what this one is for?
//			if();

            if(	value_value == NO_FLAG )
            {
                send_to_char("Invalid target for the song.\n\r", ch);
                send_to_char("See '? song_targets' \n\r", ch);
                return false;
            }


            send_to_char("Target type set.\n\r", ch);
            break;
        case TOKVAL_SPELL_MANA:			// Mana cost
            value_value = atoi(arg2);
            if(value_value < 0 || value_value > 1000000) {
                send_to_char("Mana cost must be within range of 0 to 1000000.\n\r", ch);
                return false;
            }

            send_to_char("Mana cost set.\n\r", ch);
            break;
        case TOKVAL_SPELL_LEARN:			// Learn cost
            value_value = atoi(arg2);
            if(value_value < 0 || value_value > 1000000) {
                send_to_char("Learn cost must be within range of 0 to 1000000.\n\r", ch);
                return false;
            }

            send_to_char("Learn cost set.\n\r", ch);
            break;
        default:
            // Need to check for various things.
            value_value = atol(arg2);

            sprintf(buf, "Set value %d to %ld.\n\r", value_num, value_value);
            send_to_char(buf, ch);
            break;
        }

        token_index->value[value_num] = value_value;
    } else {
        // Need to check for various things.
        value_value = atol(arg2);

        token_index->value[value_num] = value_value;
        sprintf(buf, "Set value %d to %ld.\n\r", value_num, value_value);
        send_to_char(buf, ch);
    }
    return true;
}


TEDIT(tedit_valuename)
{
    TOKEN_INDEX_DATA *token_index;
    char arg[MSL];
    char buf[MSL];
    int value_num;

    EDIT_TOKEN(ch, token_index);

    argument = one_argument(argument, arg);

    if (arg[0] == '\0' || argument[0] == '\0') {
    send_to_char("Syntax:  valuename <number> <string>\n\r", ch);
    return false;
    }

    if ((value_num = atoi(arg)) < 0 || value_num >= MAX_TOKEN_VALUES) {
    sprintf(buf, "Number must be 0-%d\n\r", MAX_TOKEN_VALUES);
    send_to_char(buf, ch);
    return false;
    }

    if (strlen(argument) <= 2) {
    send_to_char("Value name must have at least 3 characters.\n\r", ch);
    return false;
    }

    free_string(token_index->value_name[value_num]);
    token_index->value_name[value_num] = str_dup(argument);
    sprintf(buf, "Set token %s's value %d to be named '%s'.\n\r",
        widevnum_string_token(token_index, token_index->area), value_num, argument);
    send_to_char(buf, ch);
    return true;
}


TEDIT (tedit_addtprog)
{
    int tindex, value, slot;
    TOKEN_INDEX_DATA *token_index;
    PROG_LIST *list;
    SCRIPT_DATA *code;
    char trigger[MAX_STRING_LENGTH];
    char phrase[MAX_STRING_LENGTH];
    char num[MAX_STRING_LENGTH];

    EDIT_TOKEN(ch, token_index);
    argument = one_argument(argument, num);
    argument = one_argument(argument, trigger);
    argument = one_argument(argument, phrase);

    if (num[0] == '\0' || trigger[0] =='\0' || phrase[0] =='\0')
    {
    send_to_char("Syntax:   addtprog [widevnum] [trigger] [phrase]\n\r",ch);
    return false;
    }

    if ((tindex = trigger_index(trigger, PRG_TPROG)) < 0) {
    send_to_char("Valid flags are:\n\r",ch);
    show_help(ch, "tprog");
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
             value == TRIG_KNOCK ||
             value == TRIG_KNOCKING)
    {
        if( !str_cmp(phrase, "*") )
        {
            strcpy(phrase, "-1");
        }
        else
        {
            int door = parse_door(phrase);
            if( door < 0 ) {
                send_to_char("Invalid direction for exit/exall/knock/knocking trigger.\n\r", ch);
                return false;
            }
            sprintf(phrase,"%d",door);
        }
    } else if( value == TRIG_OPEN || value == TRIG_CLOSE) {
        if( !str_cmp(phrase, "*") )
        {
            strcpy(phrase, "-1");
        }
        else
        {
            int door = parse_door(phrase);
            if( door >= 0 && door < MAX_DIR ) {
                sprintf(phrase,"%d",door);
            }
        }
    }


    WNUM script_wnum;
    AREA_DATA *context = olc_relative_widevnum_context(token_index->area, num);
    if (!parse_widevnum(num, context, &script_wnum)) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    if ((code = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_TPROG)) == NULL)
    {
    send_to_char("No such TokenProgram.\n\r",ch);
    return false;
    }

    // Make sure this has a list of progs!
    if(!token_index->progs) token_index->progs = new_prog_bank();

    if(!token_index->progs) {
    send_to_char("Could not define token_index->progs!\n\r",ch);
    return false;
    }

    if (edit_trigger_exists(token_index->progs, code, tindex, phrase)) {
        send_to_char("That trigger/phrase pair is already attached to that script on this token.\n\r", ch);
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
    //SET_BIT(token_index->mprog_flags,value);
    list_appendlink(token_index->progs[slot], list);

    send_to_char("Tprog Added.\n\r",ch);
    return true;
}


TEDIT (tedit_deltprog)
{
    TOKEN_INDEX_DATA *token_index;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    int group_idx, trig_idx;
    PROG_GROUP groups[MAX_PROG_GROUPS];
    int num_groups;

    EDIT_TOKEN(ch, token_index);

    if (!token_index->progs) {
        send_to_char("This token has no programs attached.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0') {
        send_to_char("Syntax:  deltprog <group#>\n\r", ch);
        send_to_char("         deltprog <group#> <trigger#>\n\r", ch);
        return false;
    }

    if (!is_number(arg1)) {
        send_to_char("Please specify a valid group number.\n\r", ch);
        return false;
    }

    group_idx = atoi(arg1);
    num_groups = prog_build_groups(token_index->progs, groups, MAX_PROG_GROUPS, PRG_TPROG);

    if (group_idx < 1 || group_idx > num_groups) {
        send_to_char("Invalid group number.\n\r", ch);
        return false;
    }

    PROG_GROUP *group = &groups[group_idx - 1];

    if (arg2[0] == '\0') {
        // Delete entire group (all triggers for this script)
        if (edit_delscript(token_index->progs, group->script)) {
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
        if (edit_deltrigger_specific(token_index->progs, group->script, entry->entry->trig_type, entry->entry->trig_phrase)) {
            send_to_char("Trigger removed from script group.\n\r", ch);
            return true;
        }
    }

    send_to_char("No such program or trigger found.\n\r", ch);
    return false;
}

TEDIT(tedit_varset)
{
    TOKEN_INDEX_DATA *token_index;

    EDIT_TOKEN(ch, token_index);

    return olc_varset(&token_index->index_vars, ch, argument, false);
}

TEDIT(tedit_varclear)
{
    TOKEN_INDEX_DATA *token_index;

    EDIT_TOKEN(ch, token_index);

    return olc_varclear(&token_index->index_vars, ch, argument, false);
}

char *token_index_getvaluename(TOKEN_INDEX_DATA *token, int v)
{
    if(token->type == TOKEN_SPELL )
    {
        if( v == TOKVAL_SPELL_RATING ) return "Rating";
        else if( v == TOKVAL_SPELL_DIFFICULTY ) return "Difficulty";
        else if( v == TOKVAL_SPELL_TARGET ) return "Spell Target";
        else if( v == TOKVAL_SPELL_POSITION ) return "Min Position";
        else if( v == TOKVAL_SPELL_MANA ) return "Mana Cost";
        else if( v == TOKVAL_SPELL_LEARN ) return "Learn Cost";
    }
    else if( token->type == TOKEN_SKILL )
    {
        if( v == TOKVAL_SPELL_RATING ) return "Rating";
        else if( v == TOKVAL_SPELL_DIFFICULTY ) return "Difficulty";
        else if( v == TOKVAL_SPELL_LEARN ) return "Learn Cost";
    }
    else if( token->type == TOKEN_SONG )
    {
        if( v == TOKVAL_SPELL_TARGET ) return "Song Target";
        else if( v == TOKVAL_SPELL_MANA ) return "Mana Cost";
        else if( v == TOKVAL_SPELL_LEARN ) return "Learn Cost";
    }

    return token->value_name[v];
}