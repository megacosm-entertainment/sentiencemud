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
#include "../merc.h"
#include "../tables.h"
#include "../olc.h"
#include "../recycle.h"
#include "../interp.h"
#include "../scripts.h"
#include "../wilds.h"
#include "common.h"
#define TABLE_MAX_VISIBLE_WIDTH 80
#define TABLE_BORDER_COLOUR "{G" // Example color for borders
#define TABLE_LABEL_COLOUR "{Y"
#define TABLE_VALUE_COLOUR "{C"
#define TABLE_CLICK_COLOUR "{W"
#define TABLE_UNSET_COLOUR "{D"
#define TABLE_DEFAULT_COLOUR "{x"
// Define fixed column widths (visible characters)
#define COL_LABEL_WIDTH 20
#define COL_VALUE_WIDTH 50 // Total content 70, borders make it up to ~78-80

// Define min/max OLC widths
#define OLC_MIN_SCREEN_WIDTH 80
#define OLC_DEFAULT_SCREEN_WIDTH 80
#define OLC_MAX_SCREEN_WIDTH 160

int get_olc_screen_width(CHAR_DATA *ch) {
    if (!ch || !ch->desc || !ch->desc->pProtocol) {
        return OLC_DEFAULT_SCREEN_WIDTH;
    }

    if (ch->desc->pProtocol->bNAWS && ch->desc->pProtocol->ScreenWidth > 0) {
        int naws_width = ch->desc->pProtocol->ScreenWidth;
        if (naws_width < OLC_MIN_SCREEN_WIDTH) {
            return OLC_MIN_SCREEN_WIDTH;
        }
        if (naws_width > OLC_MAX_SCREEN_WIDTH) {
            return OLC_MAX_SCREEN_WIDTH;
        }
        return naws_width;
    }
    return OLC_DEFAULT_SCREEN_WIDTH;
}

// You might also want a similar function for height if you plan to use it for paging or other layout decisions.
int get_olc_screen_height(CHAR_DATA *ch) {
    if (!ch || !ch->desc || !ch->desc->pProtocol) {
        return 24; // A common default
    }
     if (ch->desc->pProtocol->bNAWS && ch->desc->pProtocol->ScreenHeight > 0) {
        return ch->desc->pProtocol->ScreenHeight;
     }
     return 24; // A common default
}



void process_olc_command(
    CHAR_DATA *ch,
    char *olc_argument, // This is the original argument string from the editor function
    const struct olc_cmd_type olc_table[],
    OLC_FUN *show_func,
    void (*mark_changed_func)(void *pEdit, bool changed)) 
{    
    char command[MAX_INPUT_LENGTH];
    char arg_for_interpret[MAX_STRING_LENGTH];
    char working_argument_buffer[MAX_STRING_LENGTH];
    char *current_command_args_ptr;
    void *pEdit = ch->desc->pEdit; // The actual data being edited
    int cmd_index;

    // Smash tildes in the input argument string. This modifies olc_argument directly.
    smash_tilde(olc_argument);

    // Copy the smashed argument for potential use by interpret()
    strcpy(arg_for_interpret, olc_argument);

    // Use a working buffer for one_argument, as it modifies the string it parses.
    strcpy(working_argument_buffer, olc_argument);
    current_command_args_ptr = working_argument_buffer;

    // Extract the first command word. current_command_args_ptr will point to the rest of the arguments.
    current_command_args_ptr = one_argument(current_command_args_ptr, command);

    // Handle "done" command
    if (!str_cmp(command, "done")) {
        edit_done(ch);
        return;
    }

    // Update last OLC command timestamp
    if (ch->pcdata && ch->pcdata->immortal) {
        ch->pcdata->immortal->last_olc_command = current_time;
    }

    // If no command was entered, call the editor's show function
    if (command[0] == '\0') {
        if (show_func) {
            (*show_func)(ch, current_command_args_ptr); // Pass remaining arguments to show_func
        } else {
            send_to_char("No display function available for this editor.\n\r", ch);
        }
        return;
    }

    // Iterate through the OLC command table for the current editor
    for (cmd_index = 0; olc_table[cmd_index].name != NULL; cmd_index++) {
        if (!str_prefix(command, olc_table[cmd_index].name)) {
            // Execute the command function. It returns true if data was changed.
            if ((*olc_table[cmd_index].olc_fun)(ch, current_command_args_ptr)) {
                if (mark_changed_func) { // Check if a callback is provided
                    (*mark_changed_func)(ch, true); // Calls generic_olc_mark_changed
                }
            }
            // Command was found and handled (or attempted), so return.
            return;
        }
    }

    // If command not found in OLC table, fall back to the main interpreter
    interpret(ch, arg_for_interpret);
}

/*
void generic_olc_mark_changed(CHAR_DATA *ch, bool changed_status) {
    if (!changed_status || !ch || !ch->desc || !ch->desc->pEdit) {
        return;
    }

    void *pEdit = ch->desc->pEdit;

    switch (ch->desc->editor) {
        case ED_AREA:
            helper_mark_area_changed(pEdit);
            break;
        case ED_ROOM:
            helper_mark_room_changed(pEdit);
            break;
        case ED_OBJECT:
            helper_mark_object_changed(pEdit);
            break;
        case ED_MOBILE:
            helper_mark_mobile_changed(pEdit);
            break;
        case ED_TOKEN:
            helper_mark_token_changed(pEdit);
            break;
        case ED_WILDS:
            helper_mark_wilds_changed(pEdit);
            break;
        case ED_VLINK:
            helper_mark_vlink_changed(pEdit);
            break;
        case ED_PROJECT:
            helper_mark_project_changed(pEdit); // pEdit might be NULL or not directly used by this helper
            break;
        // Add other editor types as needed, dispatching to their specific helper
        default:
            bug("generic_olc_mark_changed: Unknown editor type %d for marking change.", ch->desc->editor);
            break;
    }
}
*/

const char *format_dice_string(DICE_DATA *dice) {
    static char buf[50]; // Static buffer for return
    if (!dice) return "(unset)";
    sprintf(buf, "%dd%d+%d", dice->number, dice->size, dice->bonus);
    return buf;
}

// Example helper for Armour Class
const char *format_ac_string(MOB_INDEX_DATA *pMob) {
    static char buf[100];
    sprintf(buf, "Pierce: %d Bash: %d Slash: %d Magic: %d",
            pMob->ac[AC_PIERCE], pMob->ac[AC_BASH],
            pMob->ac[AC_SLASH],  pMob->ac[AC_EXOTIC]);
    return buf;
}



void olc_show_progs(BUFFER *buffer, LLIST **progs, int type, const char *title)
{
    char buf[MSL];
    int cnt, slot;

    for (cnt = 0, slot = 0; slot < TRIGSLOT_MAX; slot++)
        if(list_size(progs[slot]) > 0) ++cnt;

    if (cnt > 0) {
        sprintf(buf, "{R%-6s %-12s %-10s %-10s %-9s %-20s\n\r{x", "Number", "Vnum      ", "Trigger", "Phrase", "Status      ", " Name");
        add_buf(buffer, buf);

        sprintf(buf, "{R%-6s %-12s %-10s %-10s %-9s %-20s\n\r{x", "------", "-----------", "-------", "------", "------------", " -----");
        add_buf(buffer, buf);

        for (cnt = 0, slot = 0; slot < TRIGSLOT_MAX; slot++) {
            ITERATOR it;
            PROG_LIST *trigger;
            SCRIPT_DATA *prog;
            iterator_start(&it, progs[slot]);
            while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
                prog = get_script_index_global(trigger->vnum, type);                
                sprintf(buf, "{C[{W%4d{C]{x %-12ld %-10s %-10s %-9s %-5s\n\r", cnt,
                    trigger->vnum, trigger_name(trigger->trig_type),
                    trigger_phrase_olcshow(trigger->trig_type,trigger->trig_phrase, false, false), olc_show_script_status(prog, type), prog ? prog->name : "Unknown");
                add_buf(buffer, buf);
                cnt++;
            }
            iterator_stop(&it);
        }
    }
}

// Rewrite the below function to return a string to the above function
char *olc_show_script_status(SCRIPT_DATA *prog, int type)
{
    static char status[20];

    if (prog) {

        if(IS_SET(prog->flags,SCRIPT_DISABLED))
            sprintf(status, "{D[DISABLED]{x   ");
        else if(prog->lines > 1 && prog->src != prog->edit_src)
            sprintf(status, "{G[MODIFIED]{x   ");
        else if(prog->lines == 1)
            sprintf(status, "{W[BLANK]{x      ");
        else if(prog->code)
            sprintf(status, "{x[COMPILED]{x   ");
        else
            sprintf(status, "{R[UNCOMPILED]{x ");

        return status;
    }
    else return "Unknown";
}

void olc_buffer_show_tabs(CHAR_DATA *ch, BUFFER *buffer, const char **tab_names)
{
    int tab = ch->desc->nEditTab;

    // Show Tabs
    char tab1[MSL];
    char tab2[MSL];
    char buf[MIL];
    char cmd_buf[32];
    char text_buf[MIL];

    tab1[0] = '\0';
    tab2[0] = '\0';
    for(int i = 0; tab_names[i]; i++)
    {
        if (i > 0)
        {
            strcat(tab1, "   ");
            strcat(tab2, "__");
        }
        else
            strcat(tab1, " ");

        formatf_to(buf, sizeof(buf), "%d %s", i + 1, tab_names[i]);
        if (tab == i)
        {
            strcat(tab2,"{D/{Y");
            strcat(tab2,buf);
        }
        else
        {
            strcat(tab2,"{D/");
            // Use formatf_to to avoid static buffer overlap issues
            formatf_to(cmd_buf, sizeof(cmd_buf), "%d", i + 1);
            formatf_to(text_buf, sizeof(text_buf), "{g%s{x", buf);
            strcat(tab2, (char *)MXPCreateSend(ch->desc, cmd_buf, text_buf));
        }
        int l=strlen(tab1);
        int b=strlen(buf);
        tab1[l+b]=' ';
        tab1[l+b+1]='\0';
        memset(&tab1[l], '_', b);

        if (tab == i)
            strcat(tab2, "{D\\{x");
        else
            strcat(tab2, "{D\\{x");
    }
    strcat(tab1, "\n\r");
    strcat(tab2, "_{x\n\r");
    add_buf(buffer, tab1);
    add_buf(buffer, tab2);
    add_buf(buffer, "\n\r");
}

void olc_buffer_show_string(CHAR_DATA *ch, BUFFER *buffer, const char *value, char *command, char *heading, int indent, char *colors)
{
    char buf[MSL];
    int l=indent-strlen_no_colours(heading);
    l=UMAX(l,0);
    sprintf(buf, formatf("{%c%%s%%%ds", colors[0],l), MXPCreateSend(ch->desc,command, heading), "");
    add_buf(buffer, buf);

    if (IS_NULLSTR(value))
        sprintf(buf, "{%c(unset){x\n\r", colors[1]);
    else
        sprintf(buf, "{%c%s{x\n\r", colors[2], value);
    add_buf(buffer, buf);
}

int olc_buffer_show_flags_ex(CHAR_DATA *ch, BUFFER *buffer,
        const struct flag_type *flag_table, 
        long value, char *command, char *heading, 
        int max_width /*77*/,int first_indent /*16*/, int indent /*5*/,
        const char *colors)
{
    int width;
    int found_count=0;
    bool type_table=is_stat(flag_table);
    int lines=0;

    char openbracket='[';
    char closebracket=']';
    if(type_table){
        openbracket='(';
        closebracket=')';
    }

    char buf[MSL];
    int l=first_indent-strlen_no_colours(heading);
    l=UMAX(l,0);
    sprintf(buf, formatf("{%c%%s%%%ds{%c%c", colors[0],l,colors[1],openbracket),
        MXPCreateSend(ch->desc,command, heading), "");
    width=first_indent;

    char flagbuf[MSL];
    flagbuf[0] = '\0';

    bool match;
    for(int flag = 0; !IS_NULLSTR(flag_table[flag].name); flag++)
    {
        match=false;
        if(type_table){
            if(value==flag_table[flag].bit){
                match=true;
            }
        }else{
            if(IS_SET(value,flag_table[flag].bit)){
                match=true;
            }
        }

        if(flag_table[flag].settable){
            if (type_table)
            {
                if(match){
                    strcpy(flagbuf,formatf("{%c", colors[2]));
                }else{
                    strcpy(flagbuf,formatf("{%c", colors[3]));
                }
            }
            else
            {
                if(match){
                    strcpy(flagbuf,formatf("{%c", colors[4]));
                }else{
                    strcpy(flagbuf,formatf("{%c", colors[5]));
                }
            }
        }else{
            if(match){
                strcpy(flagbuf,formatf("{%c", colors[6]));
            }else{
                // we don't show these flags
                continue;
            }
        }			

        found_count++;
        width+= strlen(flag_table[flag].name)+1;
        if(width>max_width){
            strcat(buf,"\r\n");
            lines++;
            strcat(buf,formatf(formatf("%%%ds", indent), ""));
            width=indent + strlen(flag_table[flag].name) +1;
        }

        strcat(buf, flagbuf);
        if(flag_table[flag].settable){
            strcat(buf, MXPCreateSend(ch->desc,
                             formatf("%s %s", command, flag_table[flag].name),
                                 flag_table[flag].name));
        }else{
            strcat(buf, flag_table[flag].name);
        }
        strcat(buf," ");
    }
    if(found_count==0){
        strcat(buf,"none ");
    }
    buf[strlen(buf)-1]='\0';
    strcat(buf,formatf("{%c%c\n\r", colors[1], closebracket));


    add_buf(buffer, buf);

    lines++;
    return lines;
}

int olc_buffer_show_flags(CHAR_DATA *ch, BUFFER *buffer,
        const struct flag_type *flag_table,
        long value, char *command, char *heading,
        const char *colors)
{
    return olc_buffer_show_flags_ex(ch, buffer, flag_table, value, command, heading, 77, 16, 5, colors);
}

/*
 * ==========================================================================
 * OLC Editor Framework - New Unified Tab and Display System
 * ==========================================================================
 */

/*
 * Layout Context Management
 */

OLC_LAYOUT_CTX *olc_layout_new(CHAR_DATA *ch)
{
    OLC_LAYOUT_CTX *ctx;

    ctx = alloc_mem(sizeof(OLC_LAYOUT_CTX));
    ctx->ch = ch;
    ctx->buffer = new_buf();
    ctx->screen_width = get_olc_screen_width(ch);
    ctx->current_tab = ch->desc ? ch->desc->nEditTab : 0;

    // Calculate column widths based on screen size
    // Label takes ~20% of width, value takes the rest minus padding
    if (ctx->screen_width <= 80) {
        ctx->label_width = OLC_MIN_LABEL_WIDTH;
    } else if (ctx->screen_width >= 140) {
        ctx->label_width = OLC_MAX_LABEL_WIDTH;
    } else {
        // Scale linearly between min and max
        ctx->label_width = OLC_MIN_LABEL_WIDTH +
            ((ctx->screen_width - 80) * (OLC_MAX_LABEL_WIDTH - OLC_MIN_LABEL_WIDTH)) / 60;
    }

    // Value width is remaining space minus some padding for brackets/spacing
    ctx->value_width = ctx->screen_width - ctx->label_width - 6;
    if (ctx->value_width < 40) ctx->value_width = 40;

    return ctx;
}

void olc_layout_free(OLC_LAYOUT_CTX *ctx)
{
    if (!ctx) return;

    if (ctx->buffer) {
        free_buf(ctx->buffer);
    }
    free_mem(ctx, sizeof(OLC_LAYOUT_CTX));
}

/*
 * Initialize editor state when entering an editor.
 * Call this from do_*edit functions to set up the editor consistently.
 */
void olc_init_editor(CHAR_DATA *ch, int editor_type, void *pEdit)
{
    if (!ch || !ch->desc) return;

    ch->desc->pEdit = pEdit;
    ch->desc->editor = editor_type;
    ch->desc->nEditTab = 0;  // Always start on first tab
}

/*
 * Check if MXP is fully enabled for this character.
 * Both protocol support AND player preference must be true.
 */
static bool olc_use_mxp(CHAR_DATA *ch)
{
    if (!ch || !ch->desc) return false;
    return isMXP(ch->desc) && IS_SET(ch->comm, COMM_MXP);
}

/*
 * Safe MXP wrapper - copies result immediately to avoid static buffer issues.
 * MXPCreateSend uses formatf internally which has rotating static buffers.
 * This function copies the result before it can be overwritten.
 */
static void olc_mxp_send(CHAR_DATA *ch, const char *command,
                         const char *text, char *dest, size_t dest_size)
{
    const char *result;

    // If no command or MXP not fully enabled, just copy the text
    if (!command || command[0] == '\0' || !olc_use_mxp(ch)) {
        strncpy(dest, text, dest_size - 1);
        dest[dest_size - 1] = '\0';
        return;
    }

    // MXP is enabled - get result and immediately copy it
    result = MXPCreateSend(ch->desc, command, text);
    strncpy(dest, result, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/*
 * Tab System
 */

void olc_render_tabs(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_TABS *tabs)
{
    char line1[MSL];
    char line2[MSL];
    char buf[MIL];
    char mxp_buf[MIL];
    int i;
    int pos1 = 0, pos2 = 0;
    bool use_mxp;

    if (!ctx || !ctx->buffer || !tabs || tabs->tab_count < 1) return;

    // Use centralized MXP check
    use_mxp = olc_use_mxp(ctx->ch);

    // Build both lines at once to avoid buffer issues
    // Line 1: underscores for each tab roof
    // Line 2: /label\ for each tab
    //
    // Visual goal:
    //  __________   _________   __________
    // /1 General\__/2 Values\__/3 Scripts\_
    //
    // The underscore count = label length + 2 (for / and \)
    // First tab: leading space on line1, no leading char on line2

    pos1 = 0;
    pos2 = 0;

    for (i = 0; i < tabs->tab_count; i++) {
        const char *name = tabs->tabs[i].name;
        int label_len;

        // Use short name if screen is narrow
        if (ctx->screen_width < 100 && tabs->tabs[i].short_name) {
            name = tabs->tabs[i].short_name;
        }

        // Calculate the label text: "N Name"
        label_len = sprintf(buf, "%d %s", i + 1, name);

        // Line 1: underscores only over the label text, not the slashes
        // First tab gets a leading space (over the '/'), others don't
        if (i == 0) {
            line1[pos1++] = ' ';  // Space over the opening '/'
        }
        memset(line1 + pos1, '_', label_len);  // Over label only
        pos1 += label_len;

        // Build line 2 content for this tab
        if (ctx->current_tab == i) {
            // Active tab - yellow
            pos2 += sprintf(line2 + pos2, "/{Y%s{x\\", buf);
        } else {
            // Inactive tab - green with optional MXP
            if (use_mxp) {
                const char *mxp_result;
                char cmd[32];
                sprintf(cmd, "%d", i + 1);
                mxp_result = MXPCreateSend(ctx->ch->desc, cmd, buf);
                strncpy(mxp_buf, mxp_result, sizeof(mxp_buf) - 1);
                mxp_buf[sizeof(mxp_buf) - 1] = '\0';
                pos2 += sprintf(line2 + pos2, "/{g%s{x\\", mxp_buf);
            } else {
                pos2 += sprintf(line2 + pos2, "/{g%s{x\\", buf);
            }
        }

        // Add spacing between tabs (not after last)
        if (i < tabs->tab_count - 1) {
            // Line 2: backslash already added, now add floor "__"
            line2[pos2++] = '_';
            line2[pos2++] = '_';
            // Line 1: spaces over '\', '__', and next '/'
            line1[pos1++] = ' ';  // over this tab's '\'
            line1[pos1++] = ' ';  // over first '_'
            line1[pos1++] = ' ';  // over second '_'
            line1[pos1++] = ' ';  // over next tab's '/'
        } else {
            // Last tab: just space over the trailing '\'
            line1[pos1++] = ' ';
        }
    }

    // Terminate lines
    line1[pos1++] = '\n';
    line1[pos1++] = '\r';
    line1[pos1] = '\0';

    line2[pos2++] = '\n';
    line2[pos2++] = '\r';
    line2[pos2] = '\0';

    add_buf(ctx->buffer, line1);
    add_buf(ctx->buffer, line2);
    add_buf(ctx->buffer, "\n\r");
}

bool olc_tab_switch(CHAR_DATA *ch, const char *argument, const OLC_EDITOR_TABS *tabs)
{
    char arg[MAX_INPUT_LENGTH];
    int tab_num;
    int i;

    if (!ch || !ch->desc || !tabs || tabs->tab_count < 1) return false;
    if (!argument || argument[0] == '\0') return false;

    // Make a copy to work with
    one_argument((char *)argument, arg);

    // Check for numeric tab selection (1, 2, 3, etc.)
    if (is_number(arg)) {
        tab_num = atoi(arg);
        if (tab_num >= 1 && tab_num <= tabs->tab_count) {
            ch->desc->nEditTab = tab_num - 1;
            return true;
        }
        return false;
    }

    // Check for "tab <name>" command
    if (!str_cmp(arg, "tab")) {
        char tab_name[MAX_INPUT_LENGTH];
        argument = one_argument((char *)argument, arg); // skip "tab"
        one_argument((char *)argument, tab_name);

        if (tab_name[0] == '\0') {
            send_to_char("Switch to which tab?\n\r", ch);
            return true; // Consumed the command
        }

        // Match by prefix
        for (i = 0; i < tabs->tab_count; i++) {
            if (!str_prefix(tab_name, tabs->tabs[i].name) ||
                (tabs->tabs[i].short_name && !str_prefix(tab_name, tabs->tabs[i].short_name))) {
                ch->desc->nEditTab = i;
                return true;
            }
        }

        send_to_char("No such tab.\n\r", ch);
        return true; // Consumed the command even though invalid
    }

    return false;
}

/*
 * Field Renderers
 */

void olc_render_string(OLC_LAYOUT_CTX *ctx, const char *label,
                       const char *command, const char *value)
{
    char buf[MSL];
    char label_buf[MIL];
    int pad;

    if (!ctx || !ctx->buffer) return;

    // Format label with MXP if command provided (safely copied)
    olc_mxp_send(ctx->ch, command, label, label_buf, sizeof(label_buf));

    // Calculate padding
    pad = ctx->label_width - strlen_no_colours(label);
    if (pad < 0) pad = 0;

    // Render the field
    if (IS_NULLSTR(value)) {
        sprintf(buf, "{Y%s%*s {D(unset){x\n\r", label_buf, pad, "");
    } else {
        sprintf(buf, "{Y%s%*s {C%s{x\n\r", label_buf, pad, "", value);
    }

    add_buf(ctx->buffer, buf);
}

void olc_render_number(OLC_LAYOUT_CTX *ctx, const char *label,
                       const char *command, long value)
{
    char buf[MSL];
    char label_buf[MIL];
    int pad;

    if (!ctx || !ctx->buffer) return;

    // Format label with MXP if command provided (safely copied)
    olc_mxp_send(ctx->ch, command, label, label_buf, sizeof(label_buf));

    // Calculate padding
    pad = ctx->label_width - strlen_no_colours(label);
    if (pad < 0) pad = 0;

    sprintf(buf, "{Y%s%*s {C%ld{x\n\r", label_buf, pad, "", value);
    add_buf(ctx->buffer, buf);
}

void olc_render_dice(OLC_LAYOUT_CTX *ctx, const char *label,
                     const char *command, DICE_DATA *dice)
{
    char buf[MSL];
    char label_buf[MIL];
    int pad;

    if (!ctx || !ctx->buffer) return;

    // Format label with MXP if command provided (safely copied)
    olc_mxp_send(ctx->ch, command, label, label_buf, sizeof(label_buf));

    // Calculate padding
    pad = ctx->label_width - strlen_no_colours(label);
    if (pad < 0) pad = 0;

    if (!dice) {
        sprintf(buf, "{Y%s%*s {D(unset){x\n\r", label_buf, pad, "");
    } else {
        sprintf(buf, "{Y%s%*s {C%dd%d+%d{x\n\r",
            label_buf, pad, "", dice->number, dice->size, dice->bonus);
    }

    add_buf(ctx->buffer, buf);
}

void olc_render_flags(OLC_LAYOUT_CTX *ctx, const char *label,
                      const char *command, const struct flag_type *table,
                      long value)
{
    // Use the existing olc_buffer_show_flags_ex with framework colors
    // Colors: label, bracket, stat-set, stat-unset, flag-set, flag-unset, readonly
    static const char colors[] = "YCCWGDD";

    if (!ctx || !ctx->buffer || !table) return;

    olc_buffer_show_flags_ex(ctx->ch, ctx->buffer, table, value,
        (char *)(command ? command : ""), (char *)label,
        ctx->screen_width - 3, ctx->label_width, 5, colors);
}

void olc_render_type(OLC_LAYOUT_CTX *ctx, const char *label,
                     const char *command, const struct flag_type *table,
                     int value)
{
    char buf[MSL];
    char label_buf[MIL];
    const char *type_name = "unknown";
    int pad;

    if (!ctx || !ctx->buffer) return;

    // Look up the type name
    if (table) {
        type_name = flag_string(table, value);
    }

    // Format label with MXP if command provided (safely copied)
    olc_mxp_send(ctx->ch, command, label, label_buf, sizeof(label_buf));

    // Calculate padding
    pad = ctx->label_width - strlen_no_colours(label);
    if (pad < 0) pad = 0;

    sprintf(buf, "{Y%s%*s {C(%s){x\n\r", label_buf, pad, "", type_name);
    add_buf(ctx->buffer, buf);
}

void olc_render_text(OLC_LAYOUT_CTX *ctx, const char *label,
                     const char *command, const char *text)
{
    char buf[MSL];
    char label_buf[MIL];

    if (!ctx || !ctx->buffer) return;

    // If there's a label, render it first with MXP
    if (label && label[0] != '\0') {
        olc_mxp_send(ctx->ch, command, label, label_buf, sizeof(label_buf));
        sprintf(buf, "{Y%s{x\n\r", label_buf);
        add_buf(ctx->buffer, buf);
    }

    // Render the text content with indentation
    if (IS_NULLSTR(text)) {
        add_buf(ctx->buffer, "   {D(unset){x\n\r");
    } else {
        // Simple indented output - could add word wrapping later
        char text_buf[MSL];
        add_buf(ctx->buffer, "   ");
        strncpy(text_buf, text, sizeof(text_buf) - 1);
        text_buf[sizeof(text_buf) - 1] = '\0';
        add_buf(ctx->buffer, text_buf);
        // Ensure ends with newline
        if (text[strlen(text) - 1] != '\n' && text[strlen(text) - 1] != '\r') {
            add_buf(ctx->buffer, "\n\r");
        }
    }
}

void olc_render_section(OLC_LAYOUT_CTX *ctx, const char *title)
{
    char buf[MSL];
    int title_len;
    int dash_count;
    int pos;

    if (!ctx || !ctx->buffer) return;

    add_buf(ctx->buffer, "\n\r");

    if (title && title[0] != '\0') {
        title_len = strlen(title);
        dash_count = (ctx->screen_width - title_len - 6) / 2;
        if (dash_count < 3) dash_count = 3;

        // Format: --- Title --- (built without formatf to avoid static buffer issues)
        pos = 0;
        buf[pos++] = '{';
        buf[pos++] = 'G';
        memset(buf + pos, '-', dash_count);
        pos += dash_count;
        buf[pos++] = ' ';
        buf[pos++] = '{';
        buf[pos++] = 'W';
        strcpy(buf + pos, title);
        pos += title_len;
        buf[pos++] = '{';
        buf[pos++] = 'G';
        buf[pos++] = ' ';
        memset(buf + pos, '-', dash_count);
        pos += dash_count;
        buf[pos++] = '{';
        buf[pos++] = 'x';
        buf[pos++] = '\n';
        buf[pos++] = '\r';
        buf[pos] = '\0';

        add_buf(ctx->buffer, buf);
    }
}

void olc_render_hr(OLC_LAYOUT_CTX *ctx)
{
    char buf[MSL];
    int dash_count;

    if (!ctx || !ctx->buffer) return;

    dash_count = ctx->screen_width - 4;
    if (dash_count < 10) dash_count = 10;
    if (dash_count > (int)sizeof(buf) - 10) dash_count = sizeof(buf) - 10;

    sprintf(buf, "{G");
    memset(buf + 2, '-', dash_count);
    buf[2 + dash_count] = '\0';
    strcat(buf, "{x\n\r");

    add_buf(ctx->buffer, buf);
}

/*
 * Enhanced command processor with tab support
 */

void olc_process_command_tabbed(
    CHAR_DATA *ch,
    char *argument,
    const struct olc_cmd_type olc_table[],
    const OLC_EDITOR_TABS *tabs,
    OLC_FUN *show_func,
    void (*mark_changed_func)(void *pEdit, bool changed))
{
    // First, check if this is a tab switch command
    if (tabs && tabs->tab_count > 0) {
        if (olc_tab_switch(ch, argument, tabs)) {
            // Tab was switched, show the editor
            if (show_func) {
                (*show_func)(ch, "");
            }
            return;
        }
    }

    // Not a tab command, process normally
    process_olc_command(ch, argument, olc_table, show_func, mark_changed_func);
}