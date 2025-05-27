/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

/*  */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "merc.h"
#include "tables.h"
#include "olc.h"
#include "recycle.h"
#include "scripts.h"



const struct olc_cmd_type rsgedit_table[] =
{
/*	{	command		function	}, */

	{	"commands",	show_commands	},
	{	"list",		rsgedit_list	},
	{	"create",	rsgedit_create	},
	{	"show",		rsgedit_show	},
	{	"pattern",	rsgedit_pattern	},
	{	"class",	rsgedit_class	},
	{	"generate",	rsgedit_generate	},
	{	"?",		show_help	},

	{	NULL,		0		}
};

const struct olc_cmd_type rsgedit_pattern_table[] =
{
/*	{	command		function	}, */

	{	"list",		rsgedit_pattern_list	},
	{	"create",	rsgedit_pattern_create	},
	{	"show",		rsgedit_pattern_show	},
	{	"delete",	rsgedit_pattern_delete	},
	{	"?",		rsgedit_pattern_help	},

	{	NULL,		0		}
};

const struct olc_cmd_type rsgedit_class_table[] =
{
/*	{	command		function	}, */

	{	"list",		rsgedit_class_list	},
	{	"create",	rsgedit_class_create	},
	{	"show",		rsgedit_class_show	},
	{	"delete",	rsgedit_class_delete	},
	{	"add",		rsgedit_class_add	},
	{	"edit",		rsgedit_class_edit	},
	{	"remove",	rsgedit_class_remove	},
	{	"?",		rsgedit_class_help	},

	{	NULL,		0		}
};

void rsgedit( CHAR_DATA *ch, char *argument)
{
	RANDOM_STRING *rsg;
	char arg[MAX_INPUT_LENGTH];
	char command[MAX_INPUT_LENGTH];
	int cmd;

	smash_tilde(argument);
	strcpy(arg, argument);
	argument = one_argument( argument, command);

	EDIT_RSG(ch, rsg);

	if (!command[0]) {
		rsgedit_show(ch, argument);
		return;
	}

	if (!str_cmp(command, "done")) {
		edit_done(ch);
		return;
	}

    for (cmd = 0; rsgedit_table[cmd].name != NULL; cmd++)
    {
	if (!str_prefix(command, rsgedit_table[cmd].name) )
	{
		(*rsgedit_table[cmd].olc_fun) (ch, argument);
		return;
	}
    }

    interpret(ch, arg);

    return;
}


