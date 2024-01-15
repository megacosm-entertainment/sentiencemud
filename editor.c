// EDITOR

/***************************************************************************
 *  File: string.c                                                         *
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
#include "merc.h"
#include "tables.h"
#include "olc.h"

char *olc_getline(char *str, char *buf);
void editor_show(CHAR_DATA *ch,char *string, bool shownumbers, bool highlight);
char *editor_string_replace(char * orig, char * old, char * new);
char *editor_line_delete(char *string, int line);
char *editor_line_insert(char *string, char *newstr, int line);

enum {
	SYNTAX_NORMAL=0,
	SYNTAX_KEYWORD,
	SYNTAX_NUMBER,
	SYNTAX_STRING,
	SYNTAX_TOKEN,
	SYNTAX_COMMENT,
};

char *syntax_colours[] = {
	CLEAR,
	C_CYAN,
	C_GREEN,
	C_GREEN,
	C_YELLOW,
	C_MAGENTA,
};

char *syntax_keywords[] = {
	"and",
	"break",
	"do",
	"else",
	"elseif",
	"end",
	"false",
	"for",
	"function",
	"if",
	"in",
	"local",
	"nil",
	"not",
	"or",
	"repeat",
	"return",
	"then",
	"true",
	"until",
	"while",
	NULL
};

char *syntax_tokens[] = {
	"+",
	"-",
	"*",
	"/",
	"%",
	"^",
	"#",
	"==",
	"~=",
	"<=",
	">=",
	"<",
	">",
	"=",
	"(",
	")",
	"{",
	"}",
	"[",
	"]",
	";",
	":",
	",",
	".",
	"..",
	"...",
};

// Simply add the hook for it until it can be done reasonably here.
char *syntax_highlight(char *str)
{
	return str;
}

void editor_start(CHAR_DATA *ch, char **string, bool append)
{
	char buf[MSL];

	sprintf(buf," {W-======- EDITOR: {B%s{W -======-{x\n\n", append?"APPEND":"EDIT");
	send_to_char(buf,ch);
	send_to_char(" {W    Type {B.help{W or {B.?{W for help.\n\r",ch);
	sprintf(buf," {W-================%s========-{x\n\n", append?"======":"====");
	send_to_char(buf,ch);

	if(!*string)
		*string = str_dup("");
	else if(!append)
		**string = '\0';
	else
		editor_show(ch,*string,false,true);

	ch->desc->pString = string;
	ch->desc->connected = CON_EDITOR;
	SET_BIT(ch->comm,COMM_BUSY);
}

void editor_show(CHAR_DATA *ch,char *string, bool shownumbers, bool highlight)
{
	int cnt = 1;
	static char buf[MAX_STRING_LENGTH*2];
	char buf2[MAX_STRING_LENGTH], tmpb[MAX_STRING_LENGTH];

	if(highlight) string = syntax_highlight(string);

	while (*string) {
		string = olc_getline(string, tmpb);
		sprintf(buf2, C_WHITE "%2d. %s\n\r", cnt++, tmpb);
		strcat(buf, buf2);
	}
	strcat(buf,CLEAR);

	send_to_char(buf,ch);
}

char *editor_string_replace(char * orig, char * old, char * new)
{
	char *p;
	char xbuf[MAX_STRING_LENGTH];
	int i;

	xbuf[0] = '\0';
	strcpy(xbuf, orig);
	if ((p = strstr(orig, old))) {
//		i = strlen(orig) - strlen(p);
		i = (int)(p - orig); 	// Pointer math! ... strlen(orig) - strlen(p) is just the pointer delta
		xbuf[i] = '\0';
		strcat(xbuf, new);
		strcat(xbuf, &orig[i+strlen(old)]);
		free_string(orig);
	}

	return str_dup(xbuf);
}


void editor_input(CHAR_DATA *ch, char *argument)
{
	char buf[MSL];

	if (*argument == '.') {
		char arg1[MIL], arg2[MIL], arg3[MIL], tmp[MIL];

		argument = one_argument(argument, arg1);
		argument = first_arg(argument, arg2, false);
		strcpy(tmp, argument);
		argument = first_arg(argument, arg3, false);

        	if (!str_cmp(arg1, ".c")) {
			send_to_char("String cleared.\n\r", ch);
			free_string(*ch->desc->pString);
			*ch->desc->pString = str_dup("");
			return;
	        }

        	if (!str_cmp(arg1, ".s")) {
			send_to_char("String so far:\n\r", ch);
			editor_show(ch,(*ch->desc->pString),true,true);
			return;
		}

		if (!str_cmp(arg1, ".r")) {
			if (!arg2[0]) {
				send_to_char("usage:  .r \"old string\" \"new string\"\n\r", ch);
				return;
			}

			*ch->desc->pString = string_replace(*ch->desc->pString, arg2, arg3);
			sprintf(buf, "'%s' replaced with '%s'.\n\r", arg2, arg3);
			send_to_char(buf, ch);
			return;
		}

		if (!str_cmp(arg1, ".f")) {
			*ch->desc->pString = format_string(*ch->desc->pString);
			send_to_char("String formatted.\n\r", ch);
			return;
		}

		if (!str_cmp(arg1, ".ld")) {
			*ch->desc->pString = editor_line_delete(*ch->desc->pString, atoi(arg2));
			send_to_char("Line deleted.\n\r", ch);
			return;
		}

		if (!str_cmp(arg1, ".li")) {
			*ch->desc->pString = editor_line_insert(*ch->desc->pString, tmp, atoi(arg2));
			send_to_char("Line inserted.\n\r", ch);
			return;
		}

		if (!str_cmp(arg1, ".lr")) {
			*ch->desc->pString = editor_line_delete(*ch->desc->pString, atoi(arg2));
			*ch->desc->pString = editor_line_insert(*ch->desc->pString, tmp, atoi(arg2));
			send_to_char("Line replaced.\n\r", ch);
			return;
		}

        	if (!str_cmp(arg1, ".h")) {
			send_to_char("{WString edit help (commands on blank line):{X\n\r", ch);
			send_to_char("{M.r 'old' 'new'{X - replace a substring \n\r", ch);
			send_to_char("                          (requires '', \"\") \n\r", ch);
			send_to_char("{M.h{X               - get help (this info)\n\r", ch);
			send_to_char("{M.s{X               - show string so far  \n\r", ch);
			send_to_char("{M.f{X               - (word wrap) string  \n\r", ch);
			send_to_char("{M.c{X               - clear string so far \n\r", ch);
			send_to_char("{M.ld <num>{X    - delete line <num>\n\r", ch);
			send_to_char("{M.li <num> <str>{X- insert <str> before line <num>\n\r", ch);
			send_to_char("{M.lr <num> <str>{X- replace line <num> with <str>\n\r", ch);
			send_to_char("{M.q{X               - exit the editor          \n\r", ch);
			return;
		}

		if (!str_cmp(arg1, ".q")) {
			ch->desc->pString = NULL;
			ch->desc->connected = CON_PLAYING;
			REMOVE_BIT(ch->comm,COMM_BUSY);
			return;
		}

		send_to_char("EDITOR:  Invalid dot command.\n\r", ch);
		return;
	}

	strcpy(buf, *ch->desc->pString);

	if(strlen(buf) + strlen(argument) >= (MAX_STRING_LENGTH - 4)) {
		send_to_char("String too long, last line skipped.\n\r", ch);

		// Force character out of editing mode.
		ch->desc->pString = NULL;
		ch->desc->connected = CON_PLAYING;
		REMOVE_BIT(ch->comm,COMM_BUSY);
		return;
	}

	strcat(buf, argument);
	strcat(buf, "\n\r");
	free_string(*ch->desc->pString);
	*ch->desc->pString = str_dup(buf);
	return;
}

char *editor_line_delete(char *string, int line)
{
	char *strtmp;
	char buf[MAX_STRING_LENGTH];
	int cnt = 1, tmp = 0;

	buf[0] = '\0';

	for (strtmp = string; *strtmp; strtmp++) {
		if (cnt != line)
		buf[tmp++] = *strtmp;

		if (*strtmp == '\n') {
			if (*(strtmp + 1) == '\r') {
				if (cnt != line)
					buf[tmp++] = *(++strtmp);
				else
					++strtmp;
			}

			cnt++;
		}
	}

	buf[tmp] = '\0';

	free_string(string);
	return str_dup(buf);
}

char *editor_line_insert(char *string, char *newstr, int line)
{
	char *strtmp;
	int cnt = 1, tmp = 0;
	bool done = false;
	char buf[MAX_STRING_LENGTH];

	buf[0] = '\0';

	for (strtmp = string; *strtmp || (!done && cnt == line); strtmp++) {
		if (cnt == line && !done) {
			strcat(buf, newstr);
			strcat(buf, "\n\r");
			tmp += strlen(newstr) + 2;
			cnt++;
			done = true;
		}

		buf[tmp++] = *strtmp;

		if (done && *strtmp == '\0')
			break;

		if (*strtmp == '\n') {
			if (*(strtmp + 1) == '\r')
			buf[tmp++] = *(++strtmp);

			cnt++;
		}

		buf[tmp] = '\0';
	}

	free_string(string);
	return str_dup(buf);
}


