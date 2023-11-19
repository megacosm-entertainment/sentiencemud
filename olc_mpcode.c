/* The following code is based on ILAB OLC by Jason Dinkel */
/* Mobprogram code by Lordrom for Nevermore Mud */

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

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

#define MPEDIT( fun )           bool fun(CHAR_DATA *ch, char*argument)
#define OPEDIT( fun )		bool fun(CHAR_DATA *ch, char*argument)
#define RPEDIT( fun )		bool fun(CHAR_DATA *ch, char*argument)
#define TPEDIT( fun )		bool fun(CHAR_DATA *ch, char*argument)
#define APEDIT( fun )		bool fun(CHAR_DATA *ch, char*argument)
#define IPEDIT( fun )		bool fun(CHAR_DATA *ch, char*argument)
#define DPEDIT( fun )		bool fun(CHAR_DATA *ch, char*argument)
#define SCRIPTEDIT( fun )	bool fun(CHAR_DATA *ch, char*argument)

const struct olc_cmd_type mpedit_table[] =
{
/*	{	command		function	}, */

	{	"commands",	show_commands	},
	{	"list",		mpedit_list	},
	{	"create",	mpedit_create	},
	{	"code",		scriptedit_code	},
	{	"show",		scriptedit_show	},
	{	"comments",	scriptedit_comments	},
	{	"compile",	scriptedit_compile	},
	{	"name",		scriptedit_name	},
	{	"flags",	scriptedit_flags	},
	{	"depth",	scriptedit_depth	},
	{	"security",	scriptedit_security	},
	{	"?",		show_help	},

	{	NULL,		0		}
};


const struct olc_cmd_type opedit_table[] =
{
/*	{	command		function	}, */

	{	"commands",	show_commands	},
	{	"list",		opedit_list	},
	{	"create",	opedit_create	},
	{	"code",		scriptedit_code	},
	{	"show",		scriptedit_show	},
	{	"comments",	scriptedit_comments	},
	{	"compile",	scriptedit_compile	},
	{	"name",		scriptedit_name	},
	{	"flags",	scriptedit_flags	},
	{	"depth",	scriptedit_depth	},
	{	"security",	scriptedit_security	},
	{	"?",		show_help	},

	{	NULL,		0		}
};

const struct olc_cmd_type rpedit_table[] =
{
/*	{	command		function	}, */

	{	"commands",	show_commands	},
	{	"list",		rpedit_list	},
	{	"create",	rpedit_create	},
	{	"code",		scriptedit_code	},
	{	"show",		scriptedit_show	},
	{	"comments",	scriptedit_comments	},
	{	"compile",	scriptedit_compile	},
	{	"name",		scriptedit_name	},
	{	"flags",	scriptedit_flags	},
	{	"depth",	scriptedit_depth	},
	{	"security",	scriptedit_security	},
	{	"?",		show_help	},

	{	NULL,		0		}
};


const struct olc_cmd_type tpedit_table[] =
{
    	{	"commands",	show_commands	},
	{	"list",		tpedit_list	},
	{	"create",	tpedit_create	},
	{	"code",		scriptedit_code	},
	{	"show",		scriptedit_show	},
	{	"comments",	scriptedit_comments	},
	{	"compile",	scriptedit_compile	},
	{	"name",		scriptedit_name	},
	{	"flags",	scriptedit_flags	},
	{	"depth",	scriptedit_depth	},
	{	"security",	scriptedit_security	},
	{	"?",		show_help	},

	{	NULL,		0		}
};


const struct olc_cmd_type apedit_table[] =
{
/*	{	command		function	}, */

	{	"commands",	show_commands	},
	{	"list",		apedit_list	},
	{	"create",	apedit_create	},
	{	"code",		scriptedit_code	},
	{	"show",		scriptedit_show	},
	{	"comments",	scriptedit_comments	},
	{	"compile",	scriptedit_compile	},
	{	"name",		scriptedit_name	},
	{	"flags",	scriptedit_flags	},
	{	"depth",	scriptedit_depth	},
	{	"security",	scriptedit_security	},
	{	"?",		show_help	},

	{	NULL,		0		}
};

const struct olc_cmd_type ipedit_table[] =
{
/*	{	command		function	}, */

	{	"commands",	show_commands	},
	{	"list",		ipedit_list	},
	{	"create",	ipedit_create	},
	{	"code",		scriptedit_code	},
	{	"show",		scriptedit_show	},
	{	"comments",	scriptedit_comments	},
	{	"compile",	scriptedit_compile	},
	{	"name",		scriptedit_name	},
	{	"flags",	scriptedit_flags	},
	{	"depth",	scriptedit_depth	},
	{	"security",	scriptedit_security	},
	{	"?",		show_help	},

	{	NULL,		0		}
};

const struct olc_cmd_type dpedit_table[] =
{
/*	{	command		function	}, */

	{	"commands",	show_commands	},
	{	"list",		dpedit_list	},
	{	"create",	dpedit_create	},
	{	"code",		scriptedit_code	},
	{	"show",		scriptedit_show	},
	{	"comments",	scriptedit_comments	},
	{	"compile",	scriptedit_compile	},
	{	"name",		scriptedit_name	},
	{	"flags",	scriptedit_flags	},
	{	"depth",	scriptedit_depth	},
	{	"security",	scriptedit_security	},
	{	"?",		show_help	},

	{	NULL,		0		}
};

/*
static int olc_script_typeifc[] = {
	IFC_M,
	IFC_O,
	IFC_R,
	IFC_T,
	IFC_A,
	IFC_I,
	IFC_D,
};
*/

// Testports have reduced security checks
bool script_security_check(CHAR_DATA *ch)
{
	if(port == PORT_NORMAL)
		return (bool)(!IS_NPC(ch) && ch->tot_level >= (MAX_LEVEL-1));
	else
		return TRUE;
}

bool script_imp_check(CHAR_DATA *ch)
{
	if(port == PORT_NORMAL)
		return (bool)(!IS_NPC(ch) && ch->tot_level == MAX_LEVEL);
	else
		return (bool)(!IS_NPC(ch) && ch->tot_level >= (MAX_LEVEL-1));
}

void mpedit( CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pMcode;
    char arg[MAX_INPUT_LENGTH];
    char command[MAX_INPUT_LENGTH];
    int cmd;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument( argument, command);

    EDIT_SCRIPT(ch, pMcode);

    if (pMcode)
    {
		if ( !IS_BUILDER(ch, pMcode->area) )
		{
			send_to_char("MPEdit: Insufficient security to modify code.\n\r", ch);
			edit_done(ch);
			return;
		}
	}

    if (command[0] == '\0')
    {
        scriptedit_show(ch, argument);
        return;
    }

    if (!str_cmp(command, "done") )
    {
        edit_done(ch);
        return;
    }

    for (cmd = 0; mpedit_table[cmd].name != NULL; cmd++)
    {
	if (!str_prefix(command, mpedit_table[cmd].name) )
	{
		if ((*mpedit_table[cmd].olc_fun) (ch, argument) && pMcode)
			SET_BIT(pMcode->area->area_flags, AREA_CHANGED);
		return;
	}
    }

    interpret(ch, arg);

    return;
}

void opedit( CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pOcode;
    char arg[MAX_INPUT_LENGTH];
    char command[MAX_INPUT_LENGTH];
    int cmd;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument( argument, command);

    EDIT_SCRIPT(ch, pOcode);

    if (pOcode)
    {
	if ( !IS_BUILDER(ch, pOcode->area) )
	{
		send_to_char("OPEdit: Insufficient security to modify code.\n\r", ch);
		edit_done(ch);
		return;
	}
    }

    if (command[0] == '\0')
    {
        scriptedit_show(ch, argument);
        return;
    }

    if (!str_cmp(command, "done") )
    {
        edit_done(ch);
        return;
    }

    for (cmd = 0; opedit_table[cmd].name != NULL; cmd++)
    {
	if (!str_prefix(command, opedit_table[cmd].name) )
	{
		if ((*opedit_table[cmd].olc_fun) (ch, argument) && pOcode)
			SET_BIT(pOcode->area->area_flags, AREA_CHANGED);
		return;
	}
    }

    interpret(ch, arg);

    return;
}

void rpedit( CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pRcode;
    char arg[MAX_INPUT_LENGTH];
    char command[MAX_INPUT_LENGTH];
    int cmd;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument( argument, command);

    EDIT_SCRIPT(ch, pRcode);

    if (pRcode)
    {
	if ( !IS_BUILDER(ch, pRcode->area) )
	{
		send_to_char("RPEdit: Insufficient security to modify code.\n\r", ch);
		edit_done(ch);
		return;
	}
    }

    if (command[0] == '\0')
    {
        scriptedit_show(ch, argument);
        return;
    }

    if (!str_cmp(command, "done") )
    {
        edit_done(ch);
        return;
    }

    for (cmd = 0; rpedit_table[cmd].name != NULL; cmd++)
    {
	if (!str_prefix(command, rpedit_table[cmd].name) )
	{
		if ((*rpedit_table[cmd].olc_fun) (ch, argument) && pRcode)
			SET_BIT(pRcode->area->area_flags, AREA_CHANGED);
		return;
	}
    }

    interpret(ch, arg);

    return;
}

void tpedit( CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pTcode;
    char arg[MAX_INPUT_LENGTH];
    char command[MAX_INPUT_LENGTH];
    int cmd;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument( argument, command);

    EDIT_SCRIPT(ch, pTcode);

    if (pTcode)
    {
	if ( !IS_BUILDER(ch, pTcode->area) )
	{
		send_to_char("TPEdit: Insufficient security to modify code.\n\r", ch);
		edit_done(ch);
		return;
	}
    }

    if (command[0] == '\0')
    {
        scriptedit_show(ch, argument);
        return;
    }

    if (!str_cmp(command, "done") )
    {
        edit_done(ch);
        return;
    }

    for (cmd = 0; tpedit_table[cmd].name != NULL; cmd++)
    {
	if (!str_prefix(command, tpedit_table[cmd].name) )
	{
		if ((*tpedit_table[cmd].olc_fun) (ch, argument) && pTcode)
			SET_BIT(pTcode->area->area_flags, AREA_CHANGED);
		return;
	}
    }

    interpret(ch, arg);

    return;
}

void apedit( CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pAcode;
    char arg[MAX_INPUT_LENGTH];
    char command[MAX_INPUT_LENGTH];
    int cmd;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument( argument, command);

    EDIT_SCRIPT(ch, pAcode);

    if (pAcode)
    {

	if ( !IS_BUILDER(ch, pAcode->area) )
	{
		send_to_char("APEdit: Insufficient security to modify code.\n\r", ch);
		edit_done(ch);
		return;
	}
    }

    if (command[0] == '\0')
    {
        scriptedit_show(ch, argument);
        return;
    }

    if (!str_cmp(command, "done") )
    {
        edit_done(ch);
        return;
    }

    for (cmd = 0; apedit_table[cmd].name != NULL; cmd++)
    {
	if (!str_prefix(command, apedit_table[cmd].name) )
	{
		if ((*apedit_table[cmd].olc_fun) (ch, argument) && pAcode)
			SET_BIT(pAcode->area->area_flags, AREA_CHANGED);
		return;
	}
    }

    interpret(ch, arg);

    return;
}

void ipedit( CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pIcode;
    char arg[MAX_INPUT_LENGTH];
    char command[MAX_INPUT_LENGTH];
    int cmd;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument( argument, command);

    EDIT_SCRIPT(ch, pIcode);

    if (pIcode)
    {

	if ( !IS_BUILDER(ch, pIcode->area) )
	{
		send_to_char("IPEdit: Insufficient security to modify code.\n\r", ch);
		edit_done(ch);
		return;
	}
    }


    if (command[0] == '\0')
    {
        scriptedit_show(ch, argument);
        return;
    }

    if (!str_cmp(command, "done") )
    {
        edit_done(ch);
        return;
    }

    for (cmd = 0; ipedit_table[cmd].name != NULL; cmd++)
    {
	if (!str_prefix(command, ipedit_table[cmd].name) )
	{
		if ((*ipedit_table[cmd].olc_fun) (ch, argument) && pIcode)
			SET_BIT(pIcode->area->area_flags, AREA_CHANGED);
		return;
	}
    }

    interpret(ch, arg);

    return;
}

void dpedit( CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pDcode;
    char arg[MAX_INPUT_LENGTH];
    char command[MAX_INPUT_LENGTH];
    int cmd;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument( argument, command);

    EDIT_SCRIPT(ch, pDcode);

    if (pDcode)
    {

	if ( !IS_BUILDER(ch, pDcode->area) )
	{
		send_to_char("DPEdit: Insufficient security to modify code.\n\r", ch);
		edit_done(ch);
		return;
	}
    }



    if (command[0] == '\0')
    {
        scriptedit_show(ch, argument);
        return;
    }

    if (!str_cmp(command, "done") )
    {
        edit_done(ch);
        return;
    }

    for (cmd = 0; dpedit_table[cmd].name != NULL; cmd++)
    {
	if (!str_prefix(command, dpedit_table[cmd].name) )
	{
		if ((*dpedit_table[cmd].olc_fun) (ch, argument) && pDcode)
			SET_BIT(pDcode->area->area_flags, AREA_CHANGED);
		return;
	}
    }

    interpret(ch, arg);

    return;
}

void do_mpedit(CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pMcode;
    char command[MAX_INPUT_LENGTH];

    argument = one_argument(argument, command);
	WNUM wnum;

    if( parse_widevnum(command, ch->in_room->area, &wnum) )
    {

	if ( (pMcode = get_script_index(wnum.pArea, wnum.vnum,PRG_MPROG)) == NULL )
	{
		send_to_char("MPEdit : That widevnum does not exist.\n\r",ch);
		return;
	}

	if ( !IS_BUILDER(ch, wnum.pArea) )
	{
		send_to_char("MPEdit : Insufficient security to modify area.\n\r", ch );
		return;
	}

	olc_set_editor(ch, ED_MPCODE, pMcode);

	return;
    }

    if ( !str_cmp(command, "create") )
    {   /*
	if (argument[0] == '\0')
	{
		send_to_char( "Syntax : mpedit create [vnum]\n\r", ch );
		return;
	}
*/
	mpedit_create(ch, argument);
	return;
    }

    send_to_char( "Syntax : mpedit [vnum]\n\r", ch );
    send_to_char( "         mpedit create [vnum]\n\r", ch );

    return;
}

void do_opedit(CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pOcode;
    char command[MAX_INPUT_LENGTH];

    argument = one_argument(argument, command);
	WNUM wnum;

    if( parse_widevnum(command, ch->in_room->area, &wnum) )
    {

	if ( (pOcode = get_script_index(wnum.pArea, wnum.vnum,PRG_OPROG)) == NULL )
	{
		send_to_char("OPEdit : That widevnum does not exist.\n\r",ch);
		return;
	}

	if ( !IS_BUILDER(ch, wnum.pArea) )
	{
		send_to_char("OPEdit : Insufficient security to modify area.\n\r", ch );
		return;
	}

	olc_set_editor(ch, ED_OPCODE, pOcode);

	return;
    }

    if ( !str_cmp(command, "create") )
    {   /*
	if (argument[0] == '\0')
	{
		send_to_char( "Syntax : opedit create [vnum]\n\r", ch );
		return;
	}
	*/
	opedit_create(ch, argument);
	return;
    }

    send_to_char( "Syntax : opedit [vnum]\n\r", ch );
    send_to_char( "         opedit create [vnum]\n\r", ch );

    return;
}

void do_rpedit(CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pRcode;
    char command[MAX_INPUT_LENGTH];

    argument = one_argument(argument, command);
	WNUM wnum;

    if( parse_widevnum(command, ch->in_room->area, &wnum) )
    {

	if ( (pRcode = get_script_index(wnum.pArea, wnum.vnum,PRG_RPROG)) == NULL )
	{
		send_to_char("RPEdit : That widevnum does not exist.\n\r",ch);
		return;
	}


	if ( !IS_BUILDER(ch, wnum.pArea) )
	{
		send_to_char("RPEdit : Insufficient security to modify area.\n\r", ch );
		return;
	}

	olc_set_editor(ch, ED_RPCODE, pRcode);

	return;
    }

    if ( !str_cmp(command, "create") )
    {
	    /*
	if (argument[0] == '\0')
	{
		send_to_char( "Syntax : rpedit create [vnum]\n\r", ch );
		return;
	}*/

	rpedit_create(ch, argument);
	return;
    }

    send_to_char( "Syntax : rpedit [vnum]\n\r", ch );
    send_to_char( "         rpedit create [vnum]\n\r", ch );

    return;
}

void do_tpedit(CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pTcode;
    char command[MAX_INPUT_LENGTH];

    argument = one_argument(argument, command);
	WNUM wnum;

    if( parse_widevnum(command, ch->in_room->area, &wnum) )
    {
	if ( (pTcode = get_script_index(wnum.pArea, wnum.vnum,PRG_TPROG)) == NULL )
	{
		send_to_char("TPEdit : That widevnum does not exist.\n\r",ch);
		return;
	}

	if ( !IS_BUILDER(ch, wnum.pArea) )
	{
		send_to_char("TPEdit : Insufficient security to modify area.\n\r", ch );
		return;
	}

	olc_set_editor(ch, ED_TPCODE, pTcode);

	return;
    }

    if ( !str_cmp(command, "create") )
    {
	    /*
	if (argument[0] == '\0')
	{
		send_to_char( "Syntax : rpedit create [vnum]\n\r", ch );
		return;
	}*/

	tpedit_create(ch, argument);
	return;
    }

    send_to_char( "Syntax : tpedit [vnum]\n\r", ch );
    send_to_char( "         tpedit create [vnum]\n\r", ch );
}

void do_apedit(CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pAcode;
    char command[MAX_INPUT_LENGTH];

    argument = one_argument(argument, command);
	WNUM wnum;

    if( parse_widevnum(command, ch->in_room->area, &wnum) )
    {
		if ( (pAcode = get_script_index(wnum.pArea, wnum.vnum,PRG_APROG)) == NULL )
		{
			send_to_char("APEdit : That widevnum does not exist.\n\r",ch);
			return;
		}

		if ( !IS_BUILDER(ch, wnum.pArea) )
		{
			send_to_char("APEdit : Insufficient security to modify area.\n\r", ch );
			return;
		}

		olc_set_editor(ch, ED_APCODE, pAcode);
		return;
	}

	if ( !str_cmp(command, "create") )
	{
		apedit_create(ch, argument);
		return;
	}

	send_to_char( "Syntax : apedit [vnum]\n\r", ch );
	send_to_char( "         apedit create [vnum]\n\r", ch );
}


void do_ipedit(CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pIcode;
    char command[MAX_INPUT_LENGTH];

    argument = one_argument(argument, command);
	WNUM wnum;

    if( parse_widevnum(command, ch->in_room->area, &wnum) )
    {
		if ( (pIcode = get_script_index(wnum.pArea,wnum.vnum,PRG_IPROG)) == NULL )
		{
			send_to_char("IPEdit : That widevnum does not exist.\n\r",ch);
			return;
		}

		if ( !IS_BUILDER(ch, wnum.pArea) )
		{
			send_to_char("IPEdit : Insufficient security to modify area.\n\r", ch );
			return;
		}

		olc_set_editor(ch, ED_IPCODE, pIcode);
		return;
	}

	if ( !str_cmp(command, "create") )
	{
		ipedit_create(ch, argument);
		return;
	}

	send_to_char( "Syntax : ipedit [vnum]\n\r", ch );
	send_to_char( "         ipedit create [vnum]\n\r", ch );
}

void do_dpedit(CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pDcode;
    char command[MAX_INPUT_LENGTH];

    argument = one_argument(argument, command);
	WNUM wnum;

    if( parse_widevnum(command, ch->in_room->area, &wnum) )
    {
		if ( (pDcode = get_script_index(wnum.pArea,wnum.vnum,PRG_DPROG)) == NULL )
		{
			send_to_char("DPEdit : That widevnum does not exist.\n\r",ch);
			return;
		}

		if ( !IS_BUILDER(ch, wnum.pArea) )
		{
			send_to_char("DPEdit : Insufficient security to modify area.\n\r", ch );
			return;
		}

		olc_set_editor(ch, ED_DPCODE, pDcode);
		return;
	}

	if ( !str_cmp(command, "create") )
	{
		dpedit_create(ch, argument);
		return;
	}

	send_to_char( "Syntax : dpedit [vnum]\n\r", ch );
	send_to_char( "         dpedit create [vnum]\n\r", ch );
}


MPEDIT (mpedit_create)
{
    SCRIPT_DATA *pMcode;
	WNUM wnum;
    long auto_vnum = 0;

    if ( argument[0] == '\0' || !parse_widevnum(argument, ch->in_room->area, &wnum) )
    {
		// The trick for this will be when auto_vnum overflows from positive to negative
		// That will let the loop know it's run out of room
		for(auto_vnum = ch->in_room->area->top_mprog_index + 1;auto_vnum > 0; auto_vnum++)
		{
			SCRIPT_DATA *temp_prog = get_script_index(ch->in_room->area, auto_vnum, PRG_MPROG );
			if (!temp_prog) break;
		}

		if ( auto_vnum <= 0 )
		{
			// If you can reach this, you've made too many scripts...
			send_to_char("Sorry, this area has no more space left.\n\r", ch );
			return FALSE;
		}

		wnum.pArea = ch->in_room->area;
		wnum.vnum = auto_vnum;
    }

	if (!wnum.pArea) wnum.pArea = ch->in_room->area;

    if ( !IS_BUILDER(ch, wnum.pArea) )
    {
        send_to_char("MPEdit : Insufficient security to create MobProgs.\n\r", ch);
        return FALSE;
    }

    if ( get_script_index(wnum.pArea, wnum.vnum,PRG_MPROG) )
    {
		send_to_char("MPEdit: Code vnum already exists.\n\r",ch);
		return FALSE;
    }

    pMcode					= new_script();
    pMcode->vnum			= wnum.vnum;
    pMcode->next			= wnum.pArea->mprog_list;
    pMcode->type			= PRG_MPROG;
    pMcode->area			= wnum.pArea;
    wnum.pArea->mprog_list	= pMcode;
	olc_set_editor(ch, ED_MPCODE, pMcode);
	wnum.pArea->top_mprog_index = UMAX(wnum.pArea->top_mprog_index, wnum.vnum);

    send_to_char("MobProgram Code Created.\n\r",ch);
    return TRUE;
}

OPEDIT (opedit_create)
{
    SCRIPT_DATA *pOcode;
	WNUM wnum;
    long auto_vnum = 0;

    if ( argument[0] == '\0' || !parse_widevnum(argument, ch->in_room->area, &wnum) )
    {
		// The trick for this will be when auto_vnum overflows from positive to negative
		// That will let the loop know it's run out of room
		for(auto_vnum = ch->in_room->area->top_oprog_index + 1;auto_vnum > 0; auto_vnum++)
		{
			SCRIPT_DATA *temp_prog = get_script_index(ch->in_room->area, auto_vnum, PRG_OPROG );
			if (!temp_prog) break;
		}

		if ( auto_vnum <= 0 )
		{
			// If you can reach this, you've made too many scripts...
			send_to_char("Sorry, this area has no more space left.\n\r", ch );
			return FALSE;
		}

		wnum.pArea = ch->in_room->area;
		wnum.vnum = auto_vnum;
    }

	if (!wnum.pArea) wnum.pArea = ch->in_room->area;

    if ( !IS_BUILDER(ch, wnum.pArea) )
    {
        send_to_char("OPEdit : Insufficient security to create ObjProgs.\n\r", ch);
        return FALSE;
    }

    if ( get_script_index(wnum.pArea, wnum.vnum,PRG_OPROG) )
    {
		send_to_char("OPEdit: Code vnum already exists.\n\r",ch);
		return FALSE;
    }

    pOcode					= new_script();
    pOcode->vnum			= wnum.vnum;
    pOcode->next			= wnum.pArea->oprog_list;
    pOcode->type			= PRG_OPROG;
    pOcode->area			= wnum.pArea;
    wnum.pArea->oprog_list	= pOcode;
	olc_set_editor(ch, ED_OPCODE, pOcode);
	wnum.pArea->top_oprog_index = UMAX(wnum.pArea->top_oprog_index, wnum.vnum);

    send_to_char("ObjProgram Code Created.\n\r",ch);
    return TRUE;
}

RPEDIT (rpedit_create)
{
    SCRIPT_DATA *pRcode;
	WNUM wnum;
    long auto_vnum = 0;

    if ( argument[0] == '\0' || !parse_widevnum(argument, ch->in_room->area, &wnum) )
    {
		// The trick for this will be when auto_vnum overflows from positive to negative
		// That will let the loop know it's run out of room
		for(auto_vnum = ch->in_room->area->top_rprog_index + 1;auto_vnum > 0; auto_vnum++)
		{
			SCRIPT_DATA *temp_prog = get_script_index(ch->in_room->area, auto_vnum, PRG_RPROG );
			if (!temp_prog) break;
		}

		if ( auto_vnum <= 0 )
		{
			// If you can reach this, you've made too many scripts...
			send_to_char("Sorry, this area has no more space left.\n\r", ch );
			return FALSE;
		}

		wnum.pArea = ch->in_room->area;
		wnum.vnum = auto_vnum;
    }

	if (!wnum.pArea) wnum.pArea = ch->in_room->area;

    if ( !IS_BUILDER(ch, wnum.pArea) )
    {
        send_to_char("RPEdit : Insufficient security to create RoomProgs.\n\r", ch);
        return FALSE;
    }

    if ( get_script_index(wnum.pArea, wnum.vnum,PRG_RPROG) )
    {
		send_to_char("OPEdit: Code vnum already exists.\n\r",ch);
		return FALSE;
    }

    pRcode					= new_script();
    pRcode->vnum			= wnum.vnum;
    pRcode->next			= wnum.pArea->rprog_list;
    pRcode->type			= PRG_RPROG;
    pRcode->area			= wnum.pArea;
    wnum.pArea->rprog_list	= pRcode;
	olc_set_editor(ch, ED_RPCODE, pRcode);
	wnum.pArea->top_rprog_index = UMAX(wnum.pArea->top_rprog_index, wnum.vnum);

    send_to_char("RoomProgram Code Created.\n\r",ch);
    return TRUE;
}

TPEDIT (tpedit_create)
{
    SCRIPT_DATA *pTcode;
	WNUM wnum;
    long auto_vnum = 0;

    if ( argument[0] == '\0' || !parse_widevnum(argument, ch->in_room->area, &wnum) )
    {
		// The trick for this will be when auto_vnum overflows from positive to negative
		// That will let the loop know it's run out of room
		for(auto_vnum = ch->in_room->area->top_tprog_index + 1;auto_vnum > 0; auto_vnum++)
		{
			SCRIPT_DATA *temp_prog = get_script_index(ch->in_room->area, auto_vnum, PRG_TPROG );
			if (!temp_prog) break;
		}

		if ( auto_vnum <= 0 )
		{
			// If you can reach this, you've made too many scripts...
			send_to_char("Sorry, this area has no more space left.\n\r", ch );
			return FALSE;
		}

		wnum.pArea = ch->in_room->area;
		wnum.vnum = auto_vnum;
    }

	if (!wnum.pArea) wnum.pArea = ch->in_room->area;

    if ( !IS_BUILDER(ch, wnum.pArea) )
    {
        send_to_char("TPEdit : Insufficient security to create TokenProgs.\n\r", ch);
        return FALSE;
    }

    if ( get_script_index(wnum.pArea, wnum.vnum,PRG_TPROG) )
    {
		send_to_char("TPEdit: Code vnum already exists.\n\r",ch);
		return FALSE;
    }

    pTcode					= new_script();
    pTcode->vnum			= wnum.vnum;
    pTcode->next			= wnum.pArea->tprog_list;
    pTcode->type			= PRG_TPROG;
    pTcode->area			= wnum.pArea;
    wnum.pArea->tprog_list	= pTcode;
	olc_set_editor(ch, ED_TPCODE, pTcode);
	wnum.pArea->top_tprog_index = UMAX(wnum.pArea->top_tprog_index, wnum.vnum);

    send_to_char("TokenProgram Code Created.\n\r",ch);
    return TRUE;
}



APEDIT (apedit_create)
{
    SCRIPT_DATA *pAcode;
	WNUM wnum;
    long auto_vnum = 0;

    if ( argument[0] == '\0' || !parse_widevnum(argument, ch->in_room->area, &wnum) )
    {
		// The trick for this will be when auto_vnum overflows from positive to negative
		// That will let the loop know it's run out of room
		for(auto_vnum = ch->in_room->area->top_oprog_index + 1;auto_vnum > 0; auto_vnum++)
		{
			SCRIPT_DATA *temp_prog = get_script_index(ch->in_room->area, auto_vnum, PRG_APROG );
			if (!temp_prog) break;
		}

		if ( auto_vnum <= 0 )
		{
			// If you can reach this, you've made too many scripts...
			send_to_char("Sorry, this area has no more space left.\n\r", ch );
			return FALSE;
		}

		wnum.pArea = ch->in_room->area;
		wnum.vnum = auto_vnum;
    }

	if (!wnum.pArea) wnum.pArea = ch->in_room->area;

    if ( !IS_BUILDER(ch, wnum.pArea) )
    {
        send_to_char("APEdit : Insufficient security to create AreaProgs.\n\r", ch);
        return FALSE;
    }

    if ( get_script_index(wnum.pArea, wnum.vnum,PRG_APROG) )
    {
		send_to_char("APEdit: Code vnum already exists.\n\r",ch);
		return FALSE;
    }

    pAcode					= new_script();
    pAcode->vnum			= wnum.vnum;
    pAcode->next			= wnum.pArea->aprog_list;
    pAcode->type			= PRG_APROG;
    pAcode->area			= wnum.pArea;
    wnum.pArea->aprog_list	= pAcode;
	olc_set_editor(ch, ED_APCODE, pAcode);
	wnum.pArea->top_aprog_index = UMAX(wnum.pArea->top_aprog_index, wnum.vnum);

    send_to_char("AreaProgram Code Created.\n\r",ch);
    return TRUE;
}

IPEDIT (ipedit_create)
{
    SCRIPT_DATA *pIcode;
	WNUM wnum;
    long auto_vnum = 0;

    if ( argument[0] == '\0' || !parse_widevnum(argument, ch->in_room->area, &wnum) )
    {
		// The trick for this will be when auto_vnum overflows from positive to negative
		// That will let the loop know it's run out of room
		for(auto_vnum = ch->in_room->area->top_iprog_index + 1;auto_vnum > 0; auto_vnum++)
		{
			SCRIPT_DATA *temp_prog = get_script_index(ch->in_room->area, auto_vnum, PRG_IPROG );
			if (!temp_prog) break;
		}

		if ( auto_vnum <= 0 )
		{
			// If you can reach this, you've made too many scripts...
			send_to_char("Sorry, this area has no more space left.\n\r", ch );
			return FALSE;
		}

		wnum.pArea = ch->in_room->area;
		wnum.vnum = auto_vnum;
    }

	if (!wnum.pArea) wnum.pArea = ch->in_room->area;

    if ( !IS_BUILDER(ch, wnum.pArea) )
    {
        send_to_char("IPEdit : Insufficient security to create InstanceProgs.\n\r", ch);
        return FALSE;
    }

    if ( get_script_index(wnum.pArea, wnum.vnum,PRG_IPROG) )
    {
		send_to_char("IPEdit: Code vnum already exists.\n\r",ch);
		return FALSE;
    }

    pIcode					= new_script();
    pIcode->vnum			= wnum.vnum;
    pIcode->next			= wnum.pArea->iprog_list;
    pIcode->type			= PRG_IPROG;
    pIcode->area			= wnum.pArea;
    wnum.pArea->iprog_list	= pIcode;
	olc_set_editor(ch, ED_IPCODE, pIcode);
	wnum.pArea->top_iprog_index = UMAX(wnum.pArea->top_iprog_index, wnum.vnum);

    send_to_char("InstanceProgram Code Created.\n\r",ch);
    return TRUE;
}

DPEDIT (dpedit_create)
{
    SCRIPT_DATA *pDcode;
	WNUM wnum;
    long auto_vnum = 0;

    if ( argument[0] == '\0' || !parse_widevnum(argument, ch->in_room->area, &wnum) )
    {
		// The trick for this will be when auto_vnum overflows from positive to negative
		// That will let the loop know it's run out of room
		for(auto_vnum = ch->in_room->area->top_dprog_index + 1;auto_vnum > 0; auto_vnum++)
		{
			SCRIPT_DATA *temp_prog = get_script_index(ch->in_room->area, auto_vnum, PRG_DPROG );
			if (!temp_prog) break;
		}

		if ( auto_vnum <= 0 )
		{
			// If you can reach this, you've made too many scripts...
			send_to_char("Sorry, this area has no more space left.\n\r", ch );
			return FALSE;
		}

		wnum.pArea = ch->in_room->area;
		wnum.vnum = auto_vnum;
    }

	if (!wnum.pArea) wnum.pArea = ch->in_room->area;

    if ( !IS_BUILDER(ch, wnum.pArea) )
    {
        send_to_char("DPEdit : Insufficient security to create DungeonProgs.\n\r", ch);
        return FALSE;
    }

    if ( get_script_index(wnum.pArea, wnum.vnum,PRG_DPROG) )
    {
		send_to_char("DPEdit: Code vnum already exists.\n\r",ch);
		return FALSE;
    }

    pDcode					= new_script();
    pDcode->vnum			= wnum.vnum;
    pDcode->next			= wnum.pArea->dprog_list;
    pDcode->type			= PRG_DPROG;
    pDcode->area			= wnum.pArea;
    wnum.pArea->dprog_list	= pDcode;
	olc_set_editor(ch, ED_DPCODE, pDcode);
	wnum.pArea->top_dprog_index = UMAX(wnum.pArea->top_dprog_index, wnum.vnum);

    send_to_char("DungeonProgram Code Created.\n\r",ch);
    return TRUE;
}


SCRIPTEDIT(scriptedit_show)
{
    SCRIPT_DATA *pCode;
    char buf[MAX_STRING_LENGTH];
    char depth[MIL];

    EDIT_SCRIPT(ch,pCode);

    if(pCode->depth < 0)
    	strcpy(depth,"Infinite");
    else if(!pCode->depth)
	sprintf(depth,"Default (%d)",MAX_CALL_LEVEL);
    else
    	sprintf(depth,"%d",pCode->depth);

    sprintf(buf,
           "Name:       [%s]\n\r"
           "Vnum:       [%ld]\n\r"
           "Call Depth: [%s]\n\r"
           "Security:   [%d]\n\r"
           "Flags       [%s]\n\r"
           "Code:\n\r%s\n\r",
           pCode->name?pCode->name:"",(long int)pCode->vnum,depth,pCode->security,
           flag_string(script_flags, pCode->flags),
           pCode->edit_src);
    send_to_char(buf, ch);
	if (pCode->comments){
		sprintf(buf, "\n\r-----\n\r{WBuilders' Comments:{X\n\r%s\n\r-----\n\r", pCode->comments);
		send_to_char(buf,ch);
	}

    return FALSE;
}

// @@@NIB : 20070123 : Made the editor use a COPY of the script source
SCRIPTEDIT(scriptedit_code)
{
	SCRIPT_DATA *pCode;
	EDIT_SCRIPT(ch, pCode);

	if (!argument[0]) {
		int ret;

		// If they so much as EDIT the code and not authorized.
		if (IS_SET(pCode->flags,SCRIPT_SECURED) && !script_imp_check(ch)) {
			REMOVE_BIT(pCode->flags,SCRIPT_SECURED);
			ret = TRUE;
		} else
			ret = FALSE;
						//
		if(pCode->edit_src == pCode->src) pCode->edit_src = str_dup(pCode->src);
		{
			ch->desc->skip_blank_lines = TRUE;
			string_append(ch, &pCode->edit_src);
		}
		return ret;
	}

	send_to_char("Syntax: code\n\r",ch);
	return FALSE;
}

SCRIPTEDIT(scriptedit_comments)
{
    SCRIPT_DATA *pCode;

    EDIT_SCRIPT(ch, pCode);

    if (argument[0] != '\0')
    {
	send_to_char("Syntax:  comment\n\r", ch);
	return FALSE;
    }

    string_append(ch, &pCode->comments);
    return TRUE;
}

SCRIPTEDIT(scriptedit_compile)
{
	SCRIPT_DATA *pCode;
	BUFFER *buffer;

	EDIT_SCRIPT(ch, pCode);

	if (!argument[0]) {
		buffer = new_buf();
		if(!buffer) {
			send_to_char("WTF?! Couldn't create the buffer!\n\r",ch);
			return FALSE;
		}

		if (ch->tot_level < (MAX_LEVEL-1))
			pCode->flags |= SCRIPT_INSPECT;

		if (pCode->src && pCode->edit_src && ((pCode->src == pCode->edit_src) || !str_cmp(pCode->src,pCode->edit_src))) {
			send_to_char("Script is up-to-date.  Nothing to compile.\n\r",ch);
			return FALSE;
		}

		if(compile_script(buffer,pCode,pCode->edit_src,pCode->type))
			add_buf(buffer,"Script saved...\n\r");

		page_to_char(buf_string(buffer), ch);
		free_buf(buffer);

		return TRUE;
	}

	send_to_char("Syntax: compile\n\r",ch);
	return FALSE;
}

SCRIPTEDIT(scriptedit_name)
{
	SCRIPT_DATA *pCode;
	EDIT_SCRIPT(ch, pCode);

	if (!argument[0]) {
		send_to_char("Syntax:  name [string]\n\r", ch);
		return FALSE;
	}

	free_string(pCode->name);
	pCode->name = str_dup(argument);

	send_to_char("Name set.\n\r", ch);
	return TRUE;
}

SCRIPTEDIT(scriptedit_flags)
{
	SCRIPT_DATA *pCode;
	int value;

	EDIT_SCRIPT(ch, pCode);

	if (argument[0]) {

		if (!script_security_check(ch)) {
			send_to_char("You must be level 154 or higher to toggle these.\n\r", ch);
			return FALSE;
		}

		if ((value = flag_value(script_flags, argument)) != NO_FLAG) {
			if(IS_SET(value,SCRIPT_SECURED) && !IS_SET(pCode->flags,SCRIPT_SECURED) && !script_imp_check(ch)) {
				send_to_char("Insufficent security to set script as secured.\n\r", ch);
				return FALSE;
			}

			if(IS_SET(value,SCRIPT_SYSTEM) && !IS_SET(pCode->flags,SCRIPT_SYSTEM) && !script_imp_check(ch)) {
				send_to_char("Insufficent security to set script as system.\n\r", ch);
				return FALSE;
			}

			TOGGLE_BIT(pCode->flags, value);

			send_to_char("Script flag toggled.\n\r", ch);
			return TRUE;
		}
	}

	send_to_char("Syntax:  flags [flag]\n\r"
		"Type '? scriptflags' for a list of flags.\n\r", ch);
	return FALSE;
}

SCRIPTEDIT(scriptedit_depth)
{
	SCRIPT_DATA *pCode;
	int value;

	EDIT_SCRIPT(ch, pCode);

	if (!script_security_check(ch)) {
		send_to_char("You must be level 154 or higher to set call depth.\n\r", ch);
		return FALSE;
	}


	if (argument[0]) {

		if(is_number(argument)) {
			value = atoi(argument);
			if(value < 1) {
				send_to_char("Invalid call depth.\n\r", ch);
				send_to_char("Syntax:  depth [num>0|infinite|default]\n\r", ch);
				return FALSE;
			}
		} else if(!str_prefix(argument,"infinite"))
			value = -1;
		else if(!str_prefix(argument,"default"))
			value = 0;
		else {
			send_to_char("Invalid call depth.\n\r", ch);
			send_to_char("Syntax:  depth [num>0|infinite|default]\n\r", ch);
			return FALSE;
		}


		pCode->depth = value;

		send_to_char("Script call depth set.\n\r", ch);
		return TRUE;
	}

	send_to_char("Syntax:  depth [num>0|infinite|default]\n\r", ch);
	return FALSE;
}


SCRIPTEDIT(scriptedit_security)
{
	SCRIPT_DATA *pCode;
	int value;

	EDIT_SCRIPT(ch, pCode);

	if (!script_imp_check(ch)) {
		send_to_char("Only an IMP can set security on a script.\n\r", ch);
		return FALSE;
	}

	if (is_number(argument)) {
		value = atoi(argument);
		if(value < MIN_SCRIPT_SECURITY || value > MAX_SCRIPT_SECURITY) {
			char buf[MIL];
			sprintf(buf,"Security may only be from %d to %d.\n\r", MIN_SCRIPT_SECURITY, MAX_SCRIPT_SECURITY);
			send_to_char(buf, ch);
			return FALSE;
		}

		pCode->security = value;

		send_to_char("Script security set.\n\r", ch);
		return TRUE;
	}

	send_to_char("Syntax:  security <0-9>\n\r", ch);
	return FALSE;
}


void show_script_list(CHAR_DATA *ch, char *argument,int type)
{
    int count = 1, len;
    SCRIPT_DATA *prg;
    char buf[MSL], *noc;
    BUFFER *buffer;
    int min,max,tmp;
    bool error;
    AREA_DATA *area = ch->in_room->area;

    if(argument[0]) {
		argument = one_argument(argument,buf);
		if(!is_number(buf)) return;
		min = atoi(buf);

		if( min < 1 ) return;

		argument = one_argument(argument,buf);
		if(!is_number(buf)) return;
		max = atoi(buf);

		if( max < 1 ) return;

		if(max < min) {
			tmp = max;
			max = min;
			min = tmp;
		}
	} else {
		min = -1;
		max = -1;
    }

    switch(type) {
    case PRG_MPROG: if (min < 0) { min = 1; max = area->top_mprog_index; } break;
    case PRG_OPROG: if (min < 0) { min = 1; max = area->top_oprog_index; } break;
    case PRG_RPROG: if (min < 0) { min = 1; max = area->top_rprog_index; } break;
    case PRG_TPROG: if (min < 0) { min = 1; max = area->top_tprog_index; } break;
    case PRG_APROG: if (min < 0) { min = 1; max = area->top_aprog_index; } break;
    case PRG_IPROG: if (min < 0) { min = 1; max = area->top_iprog_index; } break;
    case PRG_DPROG: if (min < 0) { min = 1; max = area->top_dprog_index; } break;
    default: return;
    }

    if(!ch->lines)
		send_to_char("{RWARNING:{W Having scrolling off limits how many scripts you can see.{x\n\r", ch);

    buffer = new_buf();

    error = FALSE;
    for( long vnum = min; vnum <= max; vnum++)
    {
    	prg = get_script_index(area, vnum, type);
    	if( !prg ) continue;

		len = sprintf(buf,"{B[{W%-4d{B]  ",count);
		len += sprintf(buf+len,"{W%-2ld", area->uid);
		len += sprintf(buf+len,"  {W%c%c{B {G%-8ld {W%-5d ",
			((area && IS_BUILDER(ch, area)) ? 'B' : ' '),
			(IS_SET(prg->flags, SCRIPT_WIZNET) ? 'W' : ' '),
			prg->vnum,(prg->lines > 1)?(prg->lines-1):0);

		if(prg->depth < 0)
			len += sprintf(buf+len," {RINF ");
		else if(!prg->depth)
			len += sprintf(buf+len," {GDEF ");
		else
			len += sprintf(buf+len," {W%-3d ", prg->depth);

		if(IS_SET(prg->flags,SCRIPT_DISABLED))
			len += sprintf(buf+len, "{DDisabled{x   ");
		else if(prg->lines > 1 && prg->src != prg->edit_src)
			len += sprintf(buf+len, "{GModified{x   ");
		else if(prg->lines == 1)
			len += sprintf(buf+len, "{WBlank{x      ");
		else if(prg->code)
			len += sprintf(buf+len, "{xCompiled{x   ");
		else
			len += sprintf(buf+len, "{RUncompiled{x ");

		if(prg->name && *prg->name) {
			noc = nocolour(prg->name);
			len += sprintf(buf+len, "%.40s", noc);
			free_string(noc);
		}

		strcpy(buf+len, "\n\r");
		buf[len+2] = 0;
		count++;
		if(!add_buf(buffer, buf) || (!ch->lines && strlen(buf_string(buffer)) > MAX_STRING_LENGTH)) {
			error = TRUE;
			break;
		}

//		send_to_char(buf, ch);

	}

    if (error) {
		send_to_char("Too many scripts to list.  Please shorten!\n\r", ch);
    } else {
		if ( count == 1 ) {
			add_buf( buffer, "No existing scripts in that range.\n\r" );
		} else {
			send_to_char("{BCount  Area BW   Vnum   Lines Depth   Status   Name\n\r", ch);
			send_to_char("{b-------------------------------------------------------------------------\n\r", ch);
		}

		page_to_char(buf_string(buffer), ch);
    }
    free_buf(buffer);

}

MPEDIT( mpedit_list )
{
	show_script_list(ch,argument,PRG_MPROG);
	return FALSE;
}

OPEDIT( opedit_list )
{
	show_script_list(ch,argument,PRG_OPROG);
	return FALSE;
}

RPEDIT( rpedit_list )
{
	show_script_list(ch,argument,PRG_RPROG);
	return FALSE;
}

TPEDIT( tpedit_list )
{
	show_script_list(ch,argument,PRG_TPROG);
	return FALSE;
}

APEDIT( apedit_list )
{
	show_script_list(ch,argument,PRG_APROG);
	return FALSE;
}

IPEDIT( ipedit_list )
{
	show_script_list(ch,argument,PRG_IPROG);
	return FALSE;
}

DPEDIT( dpedit_list )
{
	show_script_list(ch,argument,PRG_DPROG);
	return FALSE;
}


void do_mplist (CHAR_DATA *ch, char *argument)
{
	show_script_list(ch,argument,PRG_MPROG);
}

void do_oplist (CHAR_DATA *ch, char *argument)
{
	show_script_list(ch,argument,PRG_OPROG);
}

void do_rplist (CHAR_DATA *ch, char *argument)
{
	show_script_list(ch,argument,PRG_RPROG);
}

void do_tplist (CHAR_DATA *ch, char *argument)
{
	show_script_list(ch,argument,PRG_TPROG);
}

void do_aplist (CHAR_DATA *ch, char *argument)
{
	show_script_list(ch,argument,PRG_APROG);
}

void do_iplist (CHAR_DATA *ch, char *argument)
{
	show_script_list(ch,argument,PRG_IPROG);
}

void do_dplist (CHAR_DATA *ch, char *argument)
{
	show_script_list(ch,argument,PRG_DPROG);
}


