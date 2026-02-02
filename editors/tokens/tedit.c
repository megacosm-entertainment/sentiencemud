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
#include "strings.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../scripts.h"
#include "../../wilds.h"
#include "../common.h"

/*
 * Token Editor Tab Definitions
 */
const OLC_EDITOR_TABS tedit_tabs = {
    10, {
        { "General",  "Gen" },
        { "Values",   "Val" },
        { "Scripts",  "Scr" },
        { "Another", "Ano" },
        { "More", "Mre" },
        { "Even More" "EMre" },
        { "7th", "T7"},
        { "8th", "T8"},
        { "9th", "T9"},
        { "A Really Long Tab Name", "Rly"}
    }
};


TEDIT(tedit_create)
{
    TOKEN_INDEX_DATA *token_index;
    AREA_DATA *pArea;
    long value;
    int iHash;

    EDIT_TOKEN(ch, token_index);

    // Auto-vnum: empty or "0" finds next available
    if (argument[0] == '\0' || !str_cmp(argument, "0"))
    {
        // Find next available token vnum in current area
        pArea = ch->in_room->area;
        value = pArea->min_vnum;
        while (value <= pArea->max_vnum && get_token_index(pArea, value))
            value++;
        
        if (value > pArea->max_vnum)
        {
            send_to_char("No available vnums in current area.\n\r", ch);
            return false;
        }
    }
    else
    {
        // Parse widevnum
        WNUM token_wnum;
        AREA_DATA *context = strchr(argument, '#') ? ch->in_room->area : NULL;
        if (!parse_widevnum(argument, context, &token_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        
        pArea = token_wnum.pArea;
        value = token_wnum.vnum;
    }

    if (pArea == NULL)
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

    send_to_char("Token created.\n\r", ch);
    SET_BIT(token_index->area->area_flags, AREA_CHANGED);
    return true;
}


/*
 * Tab-specific display functions for token editor
 */

static void tedit_show_general(OLC_LAYOUT_CTX *ctx, TOKEN_INDEX_DATA *token_index)
{
    olc_render_string(ctx, "Name:",  "name",  token_index->name);
    olc_render_string(ctx, "Area:",  NULL,    token_index->area->name);
    olc_render_number(ctx, "Vnum:",  NULL,    token_index->vnum);
    // token_table uses item_type struct, not flag_type - render as string
    olc_render_string(ctx, "Type:",  "type",  token_table[token_index->type].name);
    olc_render_flags(ctx,  "Flags:", "flags", token_flags, token_index->flags);
    olc_render_number(ctx, "Timer:", "timer", token_index->timer);

    olc_render_section(ctx, "Description");
    olc_render_text(ctx, NULL, "desc", token_index->description);

    olc_render_section(ctx, "Builder Comments");
    olc_render_text(ctx, NULL, "comments", token_index->comments);
}

static void tedit_show_values(OLC_LAYOUT_CTX *ctx, TOKEN_INDEX_DATA *token_index)
{
    char label[MIL];
    char cmd[MIL];
    int i;

    add_buf(ctx->buffer, "{YDefault Values:{x\n\r\n\r");

    for (i = 0; i < MAX_TOKEN_VALUES; i++) {
        const char *value_name = token_index_getvaluename(token_index, i);

        // Build label like "Value [0]: Rating"
        if (value_name && value_name[0] != '\0') {
            sprintf(label, "Value [%d]: %s", i, value_name);
        } else {
            sprintf(label, "Value [%d]:", i);
        }

        sprintf(cmd, "value %d", i);
        olc_render_number(ctx, label, cmd, token_index->value[i]);
    }

    // Show index variables if any
    if (token_index->index_vars) {
        olc_render_section(ctx, "Index Variables");
        olc_show_index_vars(ctx->buffer, token_index->index_vars);
    }
}

static void tedit_show_scripts(OLC_LAYOUT_CTX *ctx, TOKEN_INDEX_DATA *token_index)
{
    add_buf(ctx->buffer, "{YAttached Token Programs:{x\n\r\n\r");

    if (token_index->progs) {
        olc_show_progs(ctx->buffer, token_index->progs, PRG_TPROG, "TokProg Vnum");
    } else {
        add_buf(ctx->buffer, "   {D(none){x\n\r");
    }

    add_buf(ctx->buffer, "\n\r{DSyntax: addtprog <vnum> <trigger> <phrase>{x\n\r");
    add_buf(ctx->buffer, "{D        deltprog <number>{x\n\r");
}

TEDIT(tedit_show)
{
    TOKEN_INDEX_DATA *token_index;
    OLC_LAYOUT_CTX *ctx;
    char buf[MSL];

    EDIT_TOKEN(ch, token_index);

    ctx = olc_layout_new(ch);

    // Header with token name and vnum
    sprintf(buf, "{WToken: {C%s{W [{x%ld{W]{x\n\r\n\r",
        token_index->name, token_index->vnum);
    add_buf(ctx->buffer, buf);

    // Render tab bar
    olc_render_tabs(ctx, &tedit_tabs);

    // Dispatch to tab-specific display
    switch (ctx->current_tab) {
        case 0:
            tedit_show_general(ctx, token_index);
            break;
        case 1:
            tedit_show_values(ctx, token_index);
            break;
        case 2:
            tedit_show_scripts(ctx, token_index);
            break;
        default:
            tedit_show_general(ctx, token_index);
            break;
    }

    // Output to character
    if (!ch->lines && strlen(ctx->buffer->string) > MAX_STRING_LENGTH) {
        send_to_char("Too much to display. Please enable scrolling.\n\r", ch);
    } else {
        page_to_char(ctx->buffer->string, ch);
    }

    olc_layout_free(ctx);
    return false;
}


TEDIT(tedit_name)
{
    TOKEN_INDEX_DATA *token_index;

    EDIT_TOKEN(ch, token_index);

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  name [string]\n\r", ch);
    return false;
    }

    free_string(token_index->name);
    token_index->name = str_dup(argument);
    send_to_char("Name set.\n\r", ch);
    return true;
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
    int value;

    if (argument[0] == '\0'
    || ((value = flag_value(token_flags, argument)) == NO_FLAG))
    {
    send_to_char("Syntax:  flags [token flag]\n\rType '? tokenflags' for a list of flags.\n\r", ch);
    return false;
    }

    TOGGLE_BIT(token_index->flags, value);
    send_to_char("Token flag toggled.\n\r", ch);
    return true;
}


TEDIT(tedit_timer)
{
    TOKEN_INDEX_DATA *token_index;
    EDIT_TOKEN(ch, token_index);
    int value;

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  timer [number of ticks]\n\r", ch);
    return false;
    }

    if ((value = atoi(argument)) < 0 || value > 65000)
    {
    send_to_char("Invalid value. Must be a number of ticks between 0 and 65,000.\n\r", ch);
    return false;
    }

    token_index->timer = value;
    send_to_char("Timer set.\n\r", ch);
    return true;
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

    if (argument[0] != '\0')
    {
    send_to_char("Syntax:  desc\n\r", ch);
    return false;
    }

    string_append(ch, &token_index->description);
    return true;
}

TEDIT(tedit_comments)
{
    TOKEN_INDEX_DATA *token_index;

    EDIT_TOKEN(ch, token_index);

    if (argument[0] != '\0')
    {
    send_to_char("Syntax:  comment\n\r", ch);
    return false;
    }

    string_append(ch, &token_index->comments);
    return true;
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
    sprintf(buf, "Set token %ld's value %d to be named '%s'.\n\r", token_index->vnum,
        value_num, argument);
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
    AREA_DATA *context = strchr(num, '#') ? token_index->area : NULL;
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

    list                  = new_trigger();
    list->vnum            = script_wnum.vnum;
    list->trig_type       = tindex;
    list->trig_phrase     = str_dup(phrase);
    list->trig_number		= atoi(list->trig_phrase);
    list->numeric		= is_number(list->trig_phrase);
    list->script          = code;
    //SET_BIT(token_index->mprog_flags,value);
    list_appendlink(token_index->progs[slot], list);

    send_to_char("Tprog Added.\n\r",ch);
    return true;
}


TEDIT (tedit_deltprog)
{
    TOKEN_INDEX_DATA *token_index;
    char tprog[MAX_STRING_LENGTH];
    int value;

    EDIT_TOKEN(ch, token_index);

    one_argument(argument, tprog);
    if (!is_number(tprog) || tprog[0] == '\0')
    {
       send_to_char("Syntax:  delmprog [#mprog]\n\r",ch);
       return false;
    }

    value = atol (tprog);

    if (value < 0)
    {
        send_to_char("Only non-negative tprog-numbers allowed.\n\r",ch);
        return false;
    }

    if(!edit_deltrigger(token_index->progs,value)) {
    send_to_char("No such mprog.\n\r",ch);
    return false;
    }

    send_to_char("Tprog removed.\n\r", ch);
    return true;
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