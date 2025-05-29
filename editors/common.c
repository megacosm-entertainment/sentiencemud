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
            }
            // Command was found and handled (or attempted), so return.
            return;
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
/*
 * Renders the bottom border of the OLC table.
 */
// ... (includes and other functions) ...

// ... (includes and other functions) ...

void olc_render_table_footer(BUFFER *buffer, const OLC_TABLE_THEME *theme, CHAR_DATA *ch) {
    char buf[MAX_STRING_LENGTH];
    int screen_w = get_olc_screen_width(ch);
    int line_len = screen_w - 2; // -2 for '+' characters

    if (line_len < 10) line_len = 10;
    if (line_len >= MAX_STRING_LENGTH) line_len = MAX_STRING_LENGTH - 1;

    snprintf(buf, sizeof(buf), "%s+%s+%s\n\r",
             theme->border,
             repeat_char('-', line_len),
             theme->default_text);
    add_buf(buffer, buf);
}

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
                prog = get_script_index(trigger->vnum, type);                
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

		strcpy(buf, formatf("%d %s", i + 1, tab_names[i]));
		if (tab == i)
		{
			strcat(tab2,"{x/{Y");
			strcat(tab2,buf);
		}
		else
		{
			strcat(tab2,"{x{_/");
			strcat(tab2, (char *)MXPCreateSend(ch->desc, formatf("%d", i+1), formatf("{g%s{x", buf)));
		}
		int l=strlen(tab1);
		int b=strlen(buf);
		tab1[l+b]=' ';
		tab1[l+b+1]='\0';
		memset(&tab1[l], '_', b);

		if (tab == i)
			strcat(tab2, "{x\\");
		else
			strcat(tab2, "{x{_\\{x");
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