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

static int olc_script_typeifc[] = {
	IFC_M,
	IFC_O,
	IFC_R,
	IFC_T,
	IFC_A,
	IFC_I,
	IFC_D,
};

// Testports have reduced security checks
bool script_security_check(CHAR_DATA *ch)
{
	if(port == PORT_NORMAL)
		return (bool)(!IS_NPC(ch) && ch->tot_level >= (MAX_LEVEL-1));
	else
		return true;
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
    AREA_DATA *ad;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument( argument, command);

    EDIT_SCRIPT(ch, pMcode);

    if (pMcode)
    {
	ad = get_vnum_area( pMcode->vnum );

	if ( ad == NULL ) /* ??? */
	{
		edit_done(ch);
		return;
	}

	if ( !IS_BUILDER(ch, ad) )
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
			if ((ad = get_vnum_area(pMcode->vnum)) != NULL)
				SET_BIT(ad->area_flags, AREA_CHANGED);
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
    AREA_DATA *ad;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument( argument, command);

    EDIT_SCRIPT(ch, pOcode);

    if (pOcode)
    {
	ad = get_vnum_area( pOcode->vnum );

	if ( ad == NULL ) /* ??? */
	{
		edit_done(ch);
		return;
	}

	if ( !IS_BUILDER(ch, ad) )
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
			if ((ad = get_vnum_area(pOcode->vnum)) != NULL)
				SET_BIT(ad->area_flags, AREA_CHANGED);
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
    AREA_DATA *ad;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument( argument, command);

    EDIT_SCRIPT(ch, pRcode);

    if (pRcode)
    {
	ad = get_vnum_area( pRcode->vnum );

	if ( ad == NULL ) /* ??? */
	{
		edit_done(ch);
		return;
	}

	if ( !IS_BUILDER(ch, ad) )
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
			if ((ad = get_vnum_area(pRcode->vnum)) != NULL)
				SET_BIT(ad->area_flags, AREA_CHANGED);
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
    AREA_DATA *ad;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument( argument, command);

    EDIT_SCRIPT(ch, pTcode);

    if (pTcode)
    {
	ad = get_vnum_area( pTcode->vnum );

	if ( ad == NULL ) /* ??? */
	{
		edit_done(ch);
		return;
	}

	if ( !IS_BUILDER(ch, ad) )
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
			if ((ad = get_vnum_area(pTcode->vnum)) != NULL)
				SET_BIT(ad->area_flags, AREA_CHANGED);
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
    AREA_DATA *ad;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument( argument, command);

    EDIT_SCRIPT(ch, pAcode);

    if (pAcode)
    {
	ad = get_vnum_area( pAcode->vnum );

	if ( ad == NULL ) /* ??? */
	{
		edit_done(ch);
		return;
	}

	if ( !IS_BUILDER(ch, ad) )
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
			if ((ad = get_vnum_area(pAcode->vnum)) != NULL)
				SET_BIT(ad->area_flags, AREA_CHANGED);
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

	if ( !can_edit_blueprints(ch) )
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
			blueprints_changed = true;
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

	if ( !can_edit_dungeons(ch) )
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
			dungeons_changed = true;
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

    if( is_number(command) )
    {
	long vnum = atol(command);
	AREA_DATA *ad;

	if ( (pMcode = get_script_index(vnum,PRG_MPROG)) == NULL )
	{
		send_to_char("MPEdit : That vnum does not exist.\n\r",ch);
		return;
	}

	ad = get_vnum_area(vnum);

	if ( ad == NULL )
	{
		send_to_char( "MPEdit : Vnum is not assigned an area.\n\r", ch );
		return;
	}

	if ( !IS_BUILDER(ch, ad) )
	{
		send_to_char("MPEdit : Insufficient security to modify area.\n\r", ch );
		return;
	}

	ch->desc->pEdit		= (void *)pMcode;
	ch->desc->editor	= ED_MPCODE;

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

    if( is_number(command) )
    {
	long vnum = atol(command);
	AREA_DATA *ad;

	if ( (pOcode = get_script_index(vnum,PRG_OPROG)) == NULL )
	{
		send_to_char("OPEdit : That vnum does not exist.\n\r",ch);
		return;
	}

	ad = get_vnum_area(vnum);

	if ( ad == NULL )
	{
		send_to_char( "OPEdit : Vnum is not assigned an area.\n\r", ch );
		return;
	}

	if ( !IS_BUILDER(ch, ad) )
	{
		send_to_char("OPEdit : Insufficient security to modify area.\n\r", ch );
		return;
	}

	ch->desc->pEdit		= (void *)pOcode;
	ch->desc->editor	= ED_OPCODE;

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

    if( is_number(command) )
    {
	long vnum = atol(command);
	AREA_DATA *ad;

	if ( (pRcode = get_script_index(vnum,PRG_RPROG)) == NULL )
	{
		send_to_char("RPEdit : That vnum does not exist.\n\r",ch);
		return;
	}

	ad = get_vnum_area(vnum);

	if ( ad == NULL )
	{
		send_to_char( "RPEdit : Vnum is not assigned an area.\n\r", ch );
		return;
	}

	if ( !IS_BUILDER(ch, ad) )
	{
		send_to_char("RPEdit : Insufficient security to modify area.\n\r", ch );
		return;
	}

	ch->desc->pEdit		= (void *)pRcode;
	ch->desc->editor	= ED_RPCODE;

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

    if( is_number(command) )
    {
	long vnum = atol(command);
	AREA_DATA *ad;

	if ( (pTcode = get_script_index(vnum,PRG_TPROG)) == NULL )
	{
		send_to_char("TPEdit : That vnum does not exist.\n\r",ch);
		return;
	}

	ad = get_vnum_area(vnum);

	if ( ad == NULL )
	{
		send_to_char( "TPEdit : Vnum is not assigned an area.\n\r", ch );
		return;
	}

	if ( !IS_BUILDER(ch, ad) )
	{
		send_to_char("TPEdit : Insufficient security to modify area.\n\r", ch );
		return;
	}

	ch->desc->pEdit		= (void *)pTcode;
	ch->desc->editor	= ED_TPCODE;

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

    if( is_number(command) )
    {
		long vnum = atol(command);
		AREA_DATA *ad;

		if ( (pAcode = get_script_index(vnum,PRG_APROG)) == NULL )
		{
			send_to_char("APEdit : That vnum does not exist.\n\r",ch);
			return;
		}

		ad = get_vnum_area(vnum);

		if ( ad == NULL )
		{
			send_to_char( "APEdit : Vnum is not assigned an area.\n\r", ch );
			return;
		}

		if ( !IS_BUILDER(ch, ad) )
		{
			send_to_char("APEdit : Insufficient security to modify area.\n\r", ch );
			return;
		}

		ch->desc->pEdit		= (void *)pAcode;
		ch->desc->editor	= ED_APCODE;

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

    if( is_number(command) )
    {
		long vnum = atol(command);

		if ( (pIcode = get_script_index(vnum,PRG_IPROG)) == NULL )
		{
			send_to_char("IPEdit : That vnum does not exist.\n\r",ch);
			return;
		}

		if ( !can_edit_blueprints(ch) )
		{
			send_to_char("IPEdit : Insufficient security to modify blueprints.\n\r", ch );
			return;
		}

		ch->desc->pEdit		= (void *)pIcode;
		ch->desc->editor	= ED_IPCODE;

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

    if( is_number(command) )
    {
		long vnum = atol(command);

		if ( (pDcode = get_script_index(vnum,PRG_DPROG)) == NULL )
		{
			send_to_char("DPEdit : That vnum does not exist.\n\r",ch);
			return;
		}

		if ( !can_edit_dungeons(ch) )
		{
			send_to_char("DPEdit : Insufficient security to modify dungeons.\n\r", ch );
			return;
		}

		ch->desc->pEdit		= (void *)pDcode;
		ch->desc->editor	= ED_DPCODE;

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
    long value = atol(argument);
    AREA_DATA *ad;
    long auto_vnum = 0;

    if ( argument[0] == '\0' || value < 1 )
    {
	//send_to_char( "Syntax:  medit create [vnum]\n\r", ch );
	SCRIPT_DATA *temp_prog;

	auto_vnum = ch->in_room->area->min_vnum;
	temp_prog = get_script_index( auto_vnum, PRG_MPROG );
	if ( temp_prog != NULL ) {
		while ( temp_prog != NULL )
		{
			temp_prog = get_script_index( auto_vnum, PRG_MPROG );
			if ( temp_prog == NULL ) break;
			auto_vnum++;
		}
	}

	if ( auto_vnum > ch->in_room->area->max_vnum ) {
		send_to_char("Sorry, this area has no more space left.\n\r",
				ch );
		return false;
	}
    }

    if ( auto_vnum != 0 ) value = auto_vnum;

    ad = get_vnum_area(value);

    if ( ad == NULL )
    {
    	send_to_char( "MPEdit : Vnum is not assigned an area.\n\r", ch );
    	return false;
    }

    if ( !IS_BUILDER(ch, ad) )
    {
        send_to_char("MPEdit : Insufficient security to create MobProgs.\n\r", ch);
        return false;
    }

    if ( get_script_index(value,PRG_MPROG) )
    {
	send_to_char("MPEdit: Code vnum already exists.\n\r",ch);
	return false;
    }

    pMcode			= new_script();
    pMcode->vnum		= value;
    pMcode->next		= mprog_list;
    pMcode->type		= PRG_MPROG;
    pMcode->area		= ad;
    mprog_list			= pMcode;
    ch->desc->pEdit		= (void *)pMcode;
    ch->desc->editor		= ED_MPCODE;

    send_to_char("MobProgram Code Created.\n\r",ch);

    return true;
}

OPEDIT (opedit_create)
{
    SCRIPT_DATA *pOcode;
    long value = atol(argument);
    AREA_DATA *ad;
    long auto_vnum = 0;

    if ( argument[0] == '\0' || value < 1 )
    {
	//send_to_char( "Syntax:  medit create [vnum]\n\r", ch );
	SCRIPT_DATA *temp_prog;

	auto_vnum = ch->in_room->area->min_vnum;
	temp_prog = get_script_index( auto_vnum, PRG_OPROG );
	if ( temp_prog != NULL ) {
		while ( temp_prog != NULL )
		{
			temp_prog = get_script_index( auto_vnum, PRG_OPROG );
			if ( temp_prog == NULL ) break;
			auto_vnum++;
		}
	}

	if ( auto_vnum > ch->in_room->area->max_vnum ) {
		send_to_char("Sorry, this area has no more space left.\n\r",
				ch );
		return false;
	}
    }

    if ( auto_vnum != 0 ) value = auto_vnum;
/*
    if (IS_NULLSTR(argument) || value < 1)
    {
	send_to_char( "Syntax : opedit create [vnum]\n\r", ch );
	return false;
    }*/

    ad = get_vnum_area(value);

    if ( ad == NULL )
    {
    	send_to_char( "OPEdit : Vnum is not assigned an area.\n\r", ch );
    	return false;
    }

    if ( !IS_BUILDER(ch, ad) )
    {
        send_to_char("OPEdit : Insufficient security to create ObjProgs.\n\r", ch);
        return false;
    }

    if ( get_script_index(value,PRG_OPROG) )
    {
	send_to_char("OPEdit: Code vnum already exists.\n\r",ch);
	return false;
    }

    pOcode			= new_script();
    pOcode->vnum		= value;
    pOcode->next		= oprog_list;
    pOcode->area		= ad;
    oprog_list			= pOcode;
    pOcode->type		= PRG_OPROG;
    ch->desc->pEdit		= (void *)pOcode;
    ch->desc->editor		= ED_OPCODE;

    send_to_char("ObjProgram Code Created.\n\r",ch);

    return true;
}

RPEDIT (rpedit_create)
{
    SCRIPT_DATA *pRcode;
    long value = atol(argument);
    AREA_DATA *ad;
    long auto_vnum = 0;

    if ( argument[0] == '\0' || value < 1 )
    {
	//send_to_char( "Syntax:  medit create [vnum]\n\r", ch );
	SCRIPT_DATA *temp_prog;

	auto_vnum = ch->in_room->area->min_vnum;
	temp_prog = get_script_index( auto_vnum, PRG_RPROG );
	if ( temp_prog != NULL ) {
		while ( temp_prog != NULL )
		{
			temp_prog = get_script_index( auto_vnum, PRG_RPROG );
			if ( temp_prog == NULL ) break;
			auto_vnum++;
		}
	}

	if ( auto_vnum > ch->in_room->area->max_vnum ) {
		send_to_char("Sorry, this area has no more space left.\n\r",
				ch );
		return false;
	}
    }

    if ( auto_vnum != 0 ) value = auto_vnum;

    /*
    if (IS_NULLSTR(argument) || value < 1)
    {
	send_to_char( "Syntax : rpedit create [vnum]\n\r", ch );
	return false;
    }
*/
    ad = get_vnum_area(value);

    if ( ad == NULL )
    {
    	send_to_char( "RPEdit : Vnum is not assigned an area.\n\r", ch );
    	return false;
    }

    if ( !IS_BUILDER(ch, ad) )
    {
        send_to_char("RPEdit : Insufficient security to create RoomProgs.\n\r", ch);
        return false;
    }

    if ( get_script_index(value,PRG_RPROG) )
    {
	send_to_char("RPEdit: Code vnum already exists.\n\r",ch);
	return false;
    }

    pRcode			= new_script();
    pRcode->vnum		= value;
    pRcode->next		= rprog_list;
    pRcode->area		= ad;
    rprog_list			= pRcode;
    pRcode->type		= PRG_RPROG;
    ch->desc->pEdit		= (void *)pRcode;
    ch->desc->editor		= ED_RPCODE;

    send_to_char("RoomProgram Code Created.\n\r",ch);

    return true;
}

TPEDIT (tpedit_create)
{
    SCRIPT_DATA *pTcode;
    long value = atol(argument);
    AREA_DATA *ad;
    long auto_vnum = 0;

    if ( argument[0] == '\0' || value < 1 )
    {
	//send_to_char( "Syntax:  tpedit create [vnum]\n\r", ch );
	SCRIPT_DATA *temp_prog;

	auto_vnum = ch->in_room->area->min_vnum;
	temp_prog = get_script_index( auto_vnum, PRG_TPROG );
	if ( temp_prog != NULL ) {
		while ( temp_prog != NULL )
		{
			temp_prog = get_script_index( auto_vnum, PRG_TPROG );
			if ( temp_prog == NULL ) break;
			auto_vnum++;
		}
	}

	if ( auto_vnum > ch->in_room->area->max_vnum ) {
		send_to_char("Sorry, this area has no more space left.\n\r",
				ch );
		return false;
	}
    }

    if ( auto_vnum != 0 ) value = auto_vnum;

    /*
    if (IS_NULLSTR(argument) || value < 1)
    {
	send_to_char( "Syntax : rpedit create [vnum]\n\r", ch );
	return false;
    }
*/
    ad = get_vnum_area(value);

    if ( ad == NULL )
    {
    	send_to_char( "TPEdit : Vnum is not assigned an area.\n\r", ch );
    	return false;
    }

    if ( !IS_BUILDER(ch, ad) )
    {
        send_to_char("TPEdit : Insufficient security to create TokenProgs.\n\r", ch);
        return false;
    }

    if ( get_script_index(value,PRG_TPROG) )
    {
	send_to_char("TPEdit: Code vnum already exists.\n\r",ch);
	return false;
    }

    pTcode			= new_script();
    pTcode->vnum		= value;
    pTcode->area		= ad;
    pTcode->next		= tprog_list;
    tprog_list			= pTcode;
    pTcode->type		= PRG_TPROG;
    ch->desc->pEdit		= (void *)pTcode;
    ch->desc->editor		= ED_TPCODE;

    send_to_char("TokenProgram Code Created.\n\r",ch);

    return true;
}



APEDIT (apedit_create)
{
	SCRIPT_DATA *pAcode;
	long value = atol(argument);
	AREA_DATA *ad;
	long auto_vnum = 0;

	if ( argument[0] == '\0' || value < 1 )
	{
		//send_to_char( "Syntax:  tpedit create [vnum]\n\r", ch );
		SCRIPT_DATA *temp_prog;

		auto_vnum = ch->in_room->area->min_vnum;
		temp_prog = get_script_index( auto_vnum, PRG_APROG );
		if ( temp_prog != NULL ) {
			while ( temp_prog != NULL )
			{
				temp_prog = get_script_index( auto_vnum, PRG_APROG );
				if ( temp_prog == NULL ) break;
				auto_vnum++;
			}
		}

		if ( auto_vnum > ch->in_room->area->max_vnum ) {
			send_to_char("Sorry, this area has no more space left.\n\r", ch );
			return false;
		}
	}

	if ( auto_vnum != 0 ) value = auto_vnum;

	ad = get_vnum_area(value);

	if ( ad == NULL )
	{
		send_to_char( "APEdit : Vnum is not assigned an area.\n\r", ch );
		return false;
	}

	if ( !IS_BUILDER(ch, ad) )
	{
	send_to_char("APEdit : Insufficient security to create AreaProgs.\n\r", ch);
	return false;
	}

	if ( get_script_index(value,PRG_APROG) )
	{
	send_to_char("APEdit: Code vnum already exists.\n\r",ch);
	return false;
	}

	pAcode			= new_script();
	pAcode->vnum		= value;
	pAcode->area		= ad;
	pAcode->next		= aprog_list;
	aprog_list			= pAcode;
	pAcode->type		= PRG_APROG;
	ch->desc->pEdit		= (void *)pAcode;
	ch->desc->editor		= ED_APCODE;

	send_to_char("AreaProgram Code Created.\n\r",ch);

	return true;
}

IPEDIT (ipedit_create)
{
	SCRIPT_DATA *pIcode;
	long value = atol(argument);
	long auto_vnum = 0;

	if ( argument[0] == '\0' || value < 1 )
	{
		SCRIPT_DATA *temp_prog;

		auto_vnum = 1;
		temp_prog = get_script_index( auto_vnum, PRG_IPROG );
		if ( temp_prog != NULL ) {
			while ( temp_prog != NULL )
			{
				temp_prog = get_script_index( auto_vnum, PRG_IPROG );
				if ( temp_prog == NULL ) break;
				auto_vnum++;
			}
		}
	}

	if ( auto_vnum != 0 ) value = auto_vnum;

	if ( !can_edit_blueprints(ch) )
	{
		send_to_char("IPEdit : Insufficient security to create InstanceProgs.\n\r", ch);
		return false;
	}

	if ( get_script_index(value,PRG_IPROG) )
	{
		send_to_char("IPEdit: Code vnum already exists.\n\r",ch);
		return false;
	}

	pIcode			= new_script();
	pIcode->vnum		= value;
	pIcode->area		= NULL;
	pIcode->next		= iprog_list;
	iprog_list			= pIcode;
	pIcode->type		= PRG_IPROG;
	ch->desc->pEdit		= (void *)pIcode;
	ch->desc->editor		= ED_IPCODE;

	if( value > top_iprog_index )
		top_iprog_index = value;

	send_to_char("InstanceProgram Code Created.\n\r",ch);

	return true;
}

DPEDIT (dpedit_create)
{
	SCRIPT_DATA *pDcode;
	long value = atol(argument);
	long auto_vnum = 0;

	if ( argument[0] == '\0' || value < 1 )
	{
		SCRIPT_DATA *temp_prog;

		auto_vnum = 1;
		temp_prog = get_script_index( auto_vnum, PRG_DPROG );
		if ( temp_prog != NULL ) {
			while ( temp_prog != NULL )
			{
				temp_prog = get_script_index( auto_vnum, PRG_DPROG );
				if ( temp_prog == NULL ) break;
				auto_vnum++;
			}
		}
	}

	if ( auto_vnum != 0 ) value = auto_vnum;

	if ( !can_edit_dungeons(ch) )
	{
		send_to_char("DPEdit : Insufficient security to create DungeonProgs.\n\r", ch);
		return false;
	}

	if ( get_script_index(value,PRG_DPROG) )
	{
		send_to_char("DPEdit: Code vnum already exists.\n\r",ch);
		return false;
	}

	pDcode			= new_script();
	pDcode->vnum		= value;
	pDcode->area		= NULL;
	pDcode->next		= dprog_list;
	dprog_list			= pDcode;
	pDcode->type		= PRG_DPROG;
	ch->desc->pEdit		= (void *)pDcode;
	ch->desc->editor		= ED_DPCODE;

	if( value > top_dprog_index )
		top_dprog_index = value;

	send_to_char("DungeonProgram Code Created.\n\r",ch);

	return true;
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

    return false;
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
			ret = true;
		} else
			ret = false;
						//
		if(pCode->edit_src == pCode->src) pCode->edit_src = str_dup(pCode->src);
			string_append(ch, &pCode->edit_src);
		return ret;
	}

	send_to_char("Syntax: code\n\r",ch);
	return false;
}

SCRIPTEDIT(scriptedit_comments)
{
    SCRIPT_DATA *pCode;

    EDIT_SCRIPT(ch, pCode);

    if (argument[0] != '\0')
    {
	send_to_char("Syntax:  comment\n\r", ch);
	return false;
    }

    string_append(ch, &pCode->comments);
    return true;
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
			return false;
		}

		if (ch->tot_level < (MAX_LEVEL-1))
			pCode->flags |= SCRIPT_INSPECT;

		if (pCode->src && pCode->edit_src && ((pCode->src == pCode->edit_src) || !str_cmp(pCode->src,pCode->edit_src))) {
			send_to_char("Script is up-to-date.  Nothing to compile.\n\r",ch);
			return false;
		}

		if(compile_script(buffer,pCode,pCode->edit_src,olc_script_typeifc[pCode->type]))
			add_buf(buffer,"Script saved...\n\r");

		page_to_char(buf_string(buffer), ch);
		free_buf(buffer);

		return true;
	}

	send_to_char("Syntax: compile\n\r",ch);
	return false;
}

SCRIPTEDIT(scriptedit_name)
{
	SCRIPT_DATA *pCode;
	EDIT_SCRIPT(ch, pCode);

	if (!argument[0]) {
		send_to_char("Syntax:  name [string]\n\r", ch);
		return false;
	}

	free_string(pCode->name);
	pCode->name = str_dup(argument);

	send_to_char("Name set.\n\r", ch);
	return true;
}

SCRIPTEDIT(scriptedit_flags)
{
	SCRIPT_DATA *pCode;
	int value;

	EDIT_SCRIPT(ch, pCode);

	if (argument[0]) {

		if (!script_security_check(ch)) {
			send_to_char("You must be level 154 or higher to toggle these.\n\r", ch);
			return false;
		}

		if ((value = flag_value(script_flags, argument)) != NO_FLAG) {
			if(IS_SET(value,SCRIPT_SECURED) && !IS_SET(pCode->flags,SCRIPT_SECURED) && !script_imp_check(ch)) {
				send_to_char("Insufficent security to set script as secured.\n\r", ch);
				return false;
			}

			if(IS_SET(value,SCRIPT_SYSTEM) && !IS_SET(pCode->flags,SCRIPT_SYSTEM) && !script_imp_check(ch)) {
				send_to_char("Insufficent security to set script as system.\n\r", ch);
				return false;
			}

			TOGGLE_BIT(pCode->flags, value);

			send_to_char("Script flag toggled.\n\r", ch);
			return true;
		}
	}

	send_to_char("Syntax:  flags [flag]\n\r"
		"Type '? scriptflags' for a list of flags.\n\r", ch);
	return false;
}

SCRIPTEDIT(scriptedit_depth)
{
	SCRIPT_DATA *pCode;
	int value;

	EDIT_SCRIPT(ch, pCode);

	if (!script_security_check(ch)) {
		send_to_char("You must be level 154 or higher to set call depth.\n\r", ch);
		return false;
	}


	if (argument[0]) {

		if(is_number(argument)) {
			value = atoi(argument);
			if(value < 1) {
				send_to_char("Invalid call depth.\n\r", ch);
				send_to_char("Syntax:  depth [num>0|infinite|default]\n\r", ch);
				return false;
			}
		} else if(!str_prefix(argument,"infinite"))
			value = -1;
		else if(!str_prefix(argument,"default"))
			value = 0;
		else {
			send_to_char("Invalid call depth.\n\r", ch);
			send_to_char("Syntax:  depth [num>0|infinite|default]\n\r", ch);
			return false;
		}


		pCode->depth = value;

		send_to_char("Script call depth set.\n\r", ch);
		return true;
	}

	send_to_char("Syntax:  depth [num>0|infinite|default]\n\r", ch);
	return false;
}


SCRIPTEDIT(scriptedit_security)
{
	SCRIPT_DATA *pCode;
	int value;

	EDIT_SCRIPT(ch, pCode);

	if (!script_imp_check(ch)) {
		send_to_char("Only an IMP can set security on a script.\n\r", ch);
		return false;
	}

	if (is_number(argument)) {
		value = atoi(argument);
		if(value < MIN_SCRIPT_SECURITY || value > MAX_SCRIPT_SECURITY) {
			char buf[MIL];
			sprintf(buf,"Security may only be from %d to %d.\n\r", MIN_SCRIPT_SECURITY, MAX_SCRIPT_SECURITY);
			send_to_char(buf, ch);
			return false;
		}

		pCode->security = value;

		send_to_char("Script security set.\n\r", ch);
		return true;
	}

	send_to_char("Syntax:  security <0-9>\n\r", ch);
	return false;
}


void show_script_list(CHAR_DATA *ch, char *argument,int type)
{
    int count = 1, len;
    SCRIPT_DATA *prg;
    char buf[MSL], *noc;
    BUFFER *buffer;
    int min,max,tmp;
    bool error;
    AREA_DATA *area, *ad;


    area = ch->in_room->area;

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
    case PRG_MPROG:
    case PRG_OPROG:
    case PRG_RPROG:
    case PRG_TPROG:
    case PRG_APROG:
		if( min < 0 )
		{
			min = area->min_vnum;
			max = area->max_vnum;
		}
    	break;

    case PRG_IPROG:
    	prg = iprog_list;
    	if( min < 0 )
    	{
			min = 1;
			max = top_iprog_index;
		}

    	break;
    case PRG_DPROG:
    	prg = dprog_list;
    	if( min < 0 )
    	{
			min = 1;
			max = top_dprog_index;
		}
    	break;
    default: return;
    }



    if(!ch->lines)
		send_to_char("{RWARNING:{W Having scrolling off limits how many scripts you can see.{x\n\r", ch);

    buffer = new_buf();

    error = false;
    for( long vnum = min; vnum <= max; vnum++)
    {
    	prg = get_script_index(vnum, type);
    	if( !prg ) continue;

		ad = get_vnum_area(prg->vnum);

		len = sprintf(buf,"{B[{W%-4d{B]  ",count);
		if(!ad)
			len += sprintf(buf+len,"  ");
		else
			len += sprintf(buf+len,"{W%-2ld",ad->anum);

		len += sprintf(buf+len,"  {W%c%c{B {G%-8d {W%-5d ",
			((ad && IS_BUILDER(ch, ad)) ? 'B' : ' '),
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
			error = true;
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

//	sprintf(buf,"%d\n\r",strlen(buffer->string));
//	send_to_char(buf,ch);
	page_to_char(buf_string(buffer), ch);
    }
    free_buf(buffer);

}

MPEDIT( mpedit_list )
{
	show_script_list(ch,argument,PRG_MPROG);
	return false;
}

OPEDIT( opedit_list )
{
	show_script_list(ch,argument,PRG_OPROG);
	return false;
}

RPEDIT( rpedit_list )
{
	show_script_list(ch,argument,PRG_RPROG);
	return false;
}

TPEDIT( tpedit_list )
{
	show_script_list(ch,argument,PRG_TPROG);
	return false;
}

APEDIT( apedit_list )
{
	show_script_list(ch,argument,PRG_APROG);
	return false;
}

IPEDIT( ipedit_list )
{
	show_script_list(ch,argument,PRG_IPROG);
	return false;
}

DPEDIT( dpedit_list )
{
	show_script_list(ch,argument,PRG_DPROG);
	return false;
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


