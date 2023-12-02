/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "merc.h"
#include "interp.h"
#include "magic.h"
#include "recycle.h"
#include "db.h"
#include "tables.h"
#include "wilds.h"

bool is_trusted(CHURCH_PLAYER_DATA *member, char *command);
char *get_chrank(CHURCH_PLAYER_DATA *member);
char *get_chsize_from_number(int size);
char *time_for_log(void);
void church_log(CHURCH_DATA *church, char *string);
void do_chadvance(CHAR_DATA *ch, char *argument);
void do_chcolour(CHAR_DATA *ch, char *argument);
void do_chconvert(CHAR_DATA *ch, char *argument);
void do_chdeduct(CHAR_DATA *ch, char *argument);
void do_chdelete(CHAR_DATA * ch, char *argument);
void do_chdeposit(CHAR_DATA *ch, char *argument);
void do_chdonate(CHAR_DATA *ch, char *argument);
void do_chexcommunicate(CHAR_DATA * ch, char *argument);
void do_chflag(CHAR_DATA *ch, char *argument);
void do_chinfo(CHAR_DATA *ch, char *argument);
void do_chlog(CHAR_DATA *ch, char *argument);
void do_chmotd(CHAR_DATA * ch, char *argument);
void do_choverthrow(CHAR_DATA *ch, char *argument);
void do_chrules(CHAR_DATA * ch, char *argument);
void do_chtoggle(CHAR_DATA * ch, char *argument);
void do_chtransfer(CHAR_DATA *ch, char *argument);
void do_chtreasure(CHAR_DATA *ch, char *argument);
void do_chtrust(CHAR_DATA *ch, char *argument);
void do_churchset(CHAR_DATA *ch, char *argument);
void do_chwhere(CHAR_DATA * ch, char *argument);
void do_chwithdraw(CHAR_DATA *ch, char *argument);
void show_chlist_to_char(CHAR_DATA *ch);
void show_church_commands(CHAR_DATA *ch);
void show_church_info(CHURCH_DATA *church, CHAR_DATA *ch);
void write_churches_new();
void read_churches_new();
void write_church(CHURCH_DATA *church, FILE *fp);
void write_church_member(CHURCH_PLAYER_DATA *member, FILE *fp);
void add_church_to_list(CHURCH_DATA *church, CHURCH_DATA *list);
bool is_excommunicated(CHAR_DATA *ch);
void get_church_id(CHURCH_DATA *church);
void variable_dynamic_fix_church(CHURCH_DATA *church);

/* Church commands */
const struct church_command_type church_command_table[] =
{
	{ "create",			CHURCH_RANK_NONE,	do_chcreate			},
	{ "info",			CHURCH_RANK_NONE,	do_chinfo			},
	{ "list",			CHURCH_RANK_NONE,	do_chlist			},

	{ "deposit",		CHURCH_RANK_A,		do_chdeposit		},
	{ "donate",			CHURCH_RANK_A, 		do_chdonate			},
	{ "gohall",			CHURCH_RANK_A,		do_chgohall			},
	{ "motd",			CHURCH_RANK_A,		do_chmotd		 	},
	{ "quit",			CHURCH_RANK_A,		do_chrem			},
	{ "rules",			CHURCH_RANK_A,		do_chrules 			},
	{ "talk",			CHURCH_RANK_A, 		do_chtalk			},
	{ "treasure",		CHURCH_RANK_A,		do_chtreasure		},
	{ "where",			CHURCH_RANK_A,		do_chwhere 			},

	{ "balance",		CHURCH_RANK_B,		do_chbalance 		},
	{ "withdraw",		CHURCH_RANK_B,		do_chwithdraw 		},

	{ "add",			CHURCH_RANK_D, 		do_chadd			},
	{ "colour",			CHURCH_RANK_D, 		do_chcolour			},
	{ "convert",		CHURCH_RANK_D,		do_chconvert 		},
	{ "delmember",		CHURCH_RANK_D,		do_chrem			},
	{ "demote",			CHURCH_RANK_D,		do_chdem			},
	{ "excommunicate",	CHURCH_RANK_D,		do_chexcommunicate 	},
	{ "overthrow",		CHURCH_RANK_D, 		do_choverthrow		},
	{ "promote",		CHURCH_RANK_D, 		do_chprom			},
	{ "set",			CHURCH_RANK_D, 		do_churchset		},
	{ "setflag",		CHURCH_RANK_D, 		do_chflag			},
	{ "toggle",			CHURCH_RANK_D, 		do_chtoggle			},
	{ "transfer",		CHURCH_RANK_D, 		do_chtransfer		},
	{ "trust",			CHURCH_RANK_D, 		do_chtrust			},

	/* Immortal commands*/
	{ "delete",			CHURCH_RANK_IMM,	do_chdelete 		},
	{ "advance",		CHURCH_RANK_IMM, 	do_chadvance		},
	{ "deduct",			CHURCH_RANK_IMM,	do_chdeduct 		},

    { NULL,				-1,					NULL				}
};

char *lookup_church_command (char *string)
{
    int i;

    i = 0;
    while (church_command_table[i].command != NULL)
    {
	if (!str_cmp(church_command_table[i].command, string))
	    return church_command_table[i].command;

	i++;
    }

    return NULL;
}

int church_get_min_positions(int size)
{
	// (SIZE-1)*(21-10)/(4-1) = (POSITIONS - 10)
	// POSITIONS = (11 * SIZE + 19) / 3

	// BAND(1) = 10
	// CULT(2) = 13
	// ORDER(3) = 17
	// CHURCH(4) = 21

	return (11 * size + 19) / 3;
}

void show_church_commands(CHAR_DATA *ch)
{
    char buf[MSL];
    char buf2[MSL];
    int i;

    sprintf(buf, "Church Commands:\n\r");
    for (i = 0; church_command_table[i].command != NULL; i++)
    {
	if (church_command_table[i].rank == -1 || IS_IMMORTAL(ch))
	{
	    sprintf(buf2, " %-13s", church_command_table[i].command);
	    strcat(buf, buf2);

	    if (i != 0 && i % 4 == 0)
		strcat(buf, "\n\r");
	}
	else
	{
	    if (ch->church_member != NULL
	    && church_command_table[i].rank <= ch->church_member->rank)
	    {
		sprintf(buf2, " %-13s", church_command_table[i].command);
		strcat(buf, buf2);

		if (i != 0 && i % 4 == 0)
		    strcat(buf, "\n\r");
	    }
	}
    }

    send_to_char(buf, ch);
}


void do_church(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    int i;

    argument = one_argument(argument, arg);

    if (lookup_church_command(arg) == NULL)
    {
	if (arg[0] != '\0')
	    send_to_char("There is no such command.\n\r", ch);

        show_church_commands(ch);
	send_to_char("\n\rFor more info see 'help church'\n\r", ch);
	return;
    }

    /* locate command in the table */
    for (i = 0; church_command_table[i].command != NULL; i++)
    {
	if (!str_cmp(church_command_table[i].command, arg))
	    break;
    }

    if (church_command_table[i].rank > 0
    &&  ch->church_member != NULL
    &&  ch->church_member->rank < church_command_table[i].rank
    &&  !is_trusted(ch->church_member, church_command_table[i].command)
    &&  !IS_IMMORTAL(ch))
    {
	send_to_char("There is no such command.\n\r", ch);
	return;
    }

    if (is_excommunicated(ch)
    &&  !(!str_cmp(church_command_table[i].command, "list")
          || !str_cmp(church_command_table[i].command, "rules")
	  || !str_cmp(church_command_table[i].command, "quit")))
    {
	send_to_char("You have been excommunicated from the church.\n\r", ch);
	send_to_char("You may only use the following commands:\n\r", ch);
	send_to_char("LIST   RULES   QUIT\n\r", ch);
	return;
    }

    if (!str_cmp(church_command_table[i].command, "quit"))
	sprintf(argument, "%s", ch->name);

    (church_command_table[i].function)(ch, argument);
}


void do_chadd(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *temp_char;
    CHAR_DATA *target;
    CHURCH_PLAYER_DATA *member;
    CHURCH_PLAYER_DATA *new_member;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_STRING_LENGTH];
    bool found = false;
    int i;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("For use on CHURCH ADD:\n\rHelp Church\n\r", ch);
	return;
    }

    found = false;
    for (temp_char = ch->in_room->people; temp_char != NULL;
	 temp_char = temp_char->next_in_room)
    {
	if (IS_NPC(temp_char)
	&& IS_SET(temp_char->act[1], ACT2_CHURCHMASTER))
            found = true;
    }

    if (!found)
    {
	send_to_char("You must be at an administration office.\n\r", ch);
	return;
    }

    if ((target = get_char_room(ch, NULL, arg)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    if (ch->church == NULL)
    {
	send_to_char("You aren't in a registered group.\n\r", ch);
	return;
    }

    if (target->church != NULL)
    {
	send_to_char("That person is already in a registered group.\n\r", ch);
	return;
    }

    if (IS_NPC(target))
    {
	send_to_char("You may only add players to your church.\n\r", ch);
	return;
    }

    if (ch->church->alignment == CHURCH_EVIL
    && target->alignment > 0)
    {
	send_to_char
	("Only evil and neutral races can join an evil aligned group.\n\r", ch);
	return;
    }

    if (ch->church->alignment == CHURCH_GOOD
    && target->alignment < 0)
    {
	send_to_char
        ("Only benevolent and neutral races can join a good aligned group.\n\r",
	     ch);
	return;
    }

    i = 0;
    for (member = ch->church->people; member != NULL; member = member->next)
        i++;

    if (i >= ch->church->max_positions)
    {
	send_to_char("Your group is already full.\n\r", ch);
	return;
    }

    new_member = new_church_player();
    new_member->ch = target;
    new_member->name = str_dup(target->name);
    new_member->rank = CHURCH_RANK_A;
    new_member->church = ch->church;
    new_member->sex = target->sex;
    new_member->alignment = target->alignment;

    new_member->next = ch->church->people;
    ch->church->people = new_member;

    list_addlink(ch->church->online_players, target);
    list_addlink(ch->church->roster, target);

    target->church = ch->church;
    target->church_member = new_member;
    target->church_name = str_dup(ch->church->name);

    sprintf(buf, "{YYou have joined %s.{x\n\r", ch->church->name);
    send_to_char(buf, target);
    sprintf(buf, "{YYou have added %s.{x\n\r", target->name);
    send_to_char(buf, ch);
    sprintf(buf, "{Y[%s has joined %s]{x\n\r", target->name, ch->church->name);
    gecho(buf);

    sprintf(buf, "%s adds %s.", ch->name, target->name);
    append_church_log(ch->church, buf);

    write_churches_new();
}


void do_chrules(CHAR_DATA * ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    BUFFER *output;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	if (ch->church == NULL)
	{
	    send_to_char("You aren't in a church.\n\r", ch);
	    return;
	}

	output = new_buf();

	if (ch->church->rules == NULL)
	{
	    send_to_char("No rules have been set yet.\n\r", ch);
	    return;
	}

	add_buf(output, ch->church->rules);

	page_to_char(buf_string(output), ch);

	free_buf(output);
	return;
    }

    if (!str_cmp(arg, "edit"))
    {
	if (ch->church_member->rank != CHURCH_RANK_D
	&& !is_trusted(ch->church_member, "rules"))
	{
	    send_to_char("You must be the leader to edit the rules.\n\r", ch);
	    return;
	}

	string_append(ch, &ch->church->rules);
	write_churches_new();
	return;
    }

    send_to_char("Eh?\n\r", ch);
}


void do_chmotd(CHAR_DATA * ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    BUFFER *output;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	if (ch->church == NULL)
	{
	    send_to_char("You aren't in a church.\n\r", ch);
	    return;
	}

	output = new_buf();

	if (ch->church->motd == NULL)
	{
	    send_to_char("No motd has been set yet.\n\r", ch);
	    return;
	}

	add_buf(output, ch->church->motd);

	page_to_char(buf_string(output), ch);

	free_buf(output);
	return;
    }

    if (!str_cmp(arg, "edit"))
    {
	if (ch->church_member->rank != CHURCH_RANK_D
	&& !is_trusted(ch->church_member, "motd"))
	{
	    send_to_char("You must be the leader to edit the motd.\n\r", ch);
	    return;
	}

	string_append(ch, &ch->church->motd);
	write_churches_new();
	return;
    }

    send_to_char("Syntax:\n\rCHURCH MOTD\n\rCHURCH MOTD EDIT\n\r", ch);
}


void do_chrem(CHAR_DATA *ch, char *argument)
{
    CHURCH_PLAYER_DATA *member;
    char arg[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    bool found;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Remove whom?\n\r", ch);
	return;
    }

    member = NULL;

	if (IS_IMMORTAL(ch))
	{
		CHURCH_DATA *church;
		found = false;
		for (church = church_list; church != NULL; church = church->next)
		{
		    for (member = church->people; member != NULL; member = member->next)
		    {
				if (!str_prefix(member->name, arg))
				{
				    found = true;
				    break;
				}
		    }

		    if (found)
				break;
		}

		if (!found)
		{
		    send_to_char("Member not found.\n\r", ch);
		    return;
		}

		if (!str_cmp(ch->name, member->name))
		{
		    act("{Y[You have removed yourself.]{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		}
		else
		{
		    sprintf(buf, "{Y[You removed %s from %s]{x", member->name, church->name);
	    	act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		    if (member->ch != NULL)
		        act("{YYou have been removed by $N.{x", member->ch, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		}

		remove_member(member);
    }
    else
    {
		if (ch->church == NULL)
		{
		    send_to_char("You aren't in a registered group.\n\r", ch);
		    return;
		}

		found = false;
		for (member = ch->church->people; member != NULL; member = member->next)
		{
		    if (!str_prefix(member->name, arg) ||
		    	(!str_cmp(member->name, ch->name) &&
			    	(!str_cmp(arg, "self") || !str_cmp(arg, "me"))))
	    	{
				found = true;
				break;
	    	}
		}

		if (!found)
		{
			send_to_char("Member not found.\n\r", ch);
			return;
		}

		if (!IS_IMMORTAL(ch) &&
			find_char_position_in_church(ch) != CHURCH_RANK_D &&
			!is_trusted(ch->church_member, "remove") &&
			str_cmp(arg, ch->name) && str_cmp(arg, "self") &&
			str_cmp(arg, "me"))
		{
			act("Only a leader may remove members.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		if (!str_cmp(member->church->founder, arg) &&
			str_cmp(member->church->founder, ch->name))
		{
			send_to_char("You can't remove the founder of the church.\n\r", ch);
			return;
		}

		if ((!str_cmp(ch->name, arg) || !str_cmp(arg, "self") || !str_cmp(arg, "me")) &&
			!str_cmp(member->church->founder, ch->name))
		{
			send_to_char("{RWarning: {xIf you leave your church it will be disbanded.\n\r", ch);
			send_to_char("{YAre you sure you want to do this?{x\n\r", ch);
			ch->remove_question = member;
		}
		else if (!str_cmp(ch->name, arg) || !str_cmp(arg, "self") || !str_cmp(arg, "me"))
		{
			send_to_char("{RWarning: {xIf you leave this church you will be shunned by the gods.\n\r", ch);
			send_to_char("You will NOT lose all deity points and ALL pneuma.\n\r", ch);
			send_to_char("{YAre you sure you want to do this?{x\n\r", ch);
			ch->remove_question = member;
		}
		else
		{
			sprintf(buf, "{YYou have removed %s.{x", member->name);
			act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			sprintf(buf, "{Y[%s has been removed from %s]{x\n\r", member->name, ch->church->name);
			gecho(buf);

			sprintf(buf, "%s removes %s.", ch->name, member->name);
			append_church_log(ch->church, buf);

		    if (member->ch != NULL)
				act("{YYou have been removed by $N.{x", member->ch, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		    remove_member(member);
		}
    }

    write_churches_new();
    return;
}


void remove_member(CHURCH_PLAYER_DATA * member)
{
    CHURCH_PLAYER_DATA *prev_member;
    CHURCH_PLAYER_DATA *member2;
    bool found;
    char buf[MAX_STRING_LENGTH];

    if (member->church == NULL)
    {
        sprintf(buf, "remove_member: ch with null church %s",
            member->name);
	return;
    }

    found = false;
    prev_member = NULL;
    for (member2 = member->church->people; member2 != NULL;
	 prev_member = member2, member2 = member2->next)
    {
	if (member2 == member)
	{
	    found = true;
	    break;
	}
    }

    if (!found)
	return;

    if (prev_member != NULL)
	prev_member->next = member->next;
    else
	member->church->people = member->next;

    if (member->ch != NULL)
    {
	member->ch->church = NULL;
	free_string(member->ch->church_name);

	member->ch->church_member = NULL;
        list_remlink(member->church->online_players, member->ch);
    }

   	list_remlink(member->church->roster, member->name);

    free_church_player(member);
}


void do_chprom(CHAR_DATA *ch, char *argument)
{
    CHURCH_DATA *church;
    CHURCH_PLAYER_DATA *member;
    char arg[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    bool found;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Promote whom?\n\r", ch);
	return;
    }

    if (IS_IMMORTAL(ch))
    {
	found = false;
	member = NULL;
	for (church = church_list; church != NULL; church = church->next)
	{
	    for (member = church->people; member != NULL; member = member->next)
	    {
		if (!str_cmp(arg, member->name))
		{
		    found = true;
		    break;
		}
	    }

	    if (found)
	        break;
	}

	if (!found || !member)
	{
	    send_to_char("No church member with that name.\n\r", ch);
	    return;
	}

	if (member->rank >= CHURCH_RANK_D)
	{
	    send_to_char("You can't promote that member any further.\n\r", ch);
	    return;
	}

	member->rank++;

	sprintf(buf, "{YYou have been promoted to %s!{x\n\r",
			get_chrank(member));

	if (member->ch != NULL)
		send_to_char(buf, member->ch);

	sprintf(buf, "{YYou promoted %s to %s!{x\n\r", member->name,
			get_chrank(member));
	send_to_char(buf, ch);

	return;
    }

    if (ch->church == NULL)
    {
	send_to_char("You aren't in a registered group.\n\r", ch);
	return;
    }

    for (member = ch->church->people; member != NULL;
	 member = member->next)
	if (!str_cmp(member->name, arg))
	    break;

    if (member == NULL)
    {
	send_to_char("Member not found.\n\r", ch);
	return;
    }

    if (member->rank >= CHURCH_RANK_D || member->rank >= ch->church_member->rank)
    {
	send_to_char("You can't promote that member any further.\n\r", ch);
	return;
    }

    member->rank++;

    sprintf(buf, "{YYou have been promoted to %s!{x\n\r",
    	get_chrank(member));
	if (member->ch != NULL)
		send_to_char(buf, member->ch);

    sprintf(buf, "{YYou promoted %s to %s!{x\n\r", member->name,
         get_chrank(member));
    send_to_char(buf, ch);
}


void do_chdem(CHAR_DATA *ch, char *argument)
{
    CHURCH_PLAYER_DATA *member;
    CHURCH_DATA *church;
    char arg[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Demote whom?\n\r", ch);
	return;
    }

    if (IS_IMMORTAL(ch))
    {
	bool found = false;
	member = NULL;
	for (church = church_list; church != NULL; church = church->next)
	{
	    for (member = church->people; member != NULL; member = member->next)
	    {
		if (!str_cmp(arg, member->name))
		{
		    found = true;
		    break;
		}
	    }

	    if (found)
		    break;
	}

	if (!found || !member)
	{
	    send_to_char("No church member with that name.\n\r", ch);
	    return;
	}

	if (!str_cmp(member->name, church->founder))
	{
	    send_to_char("You can't demote the church founder.\n\r", ch);
	    return;
	}

	if (member->rank <= CHURCH_RANK_A)
	{
	    send_to_char("You can't demote that member any further.\n\r", ch);
	    return;
	}

	member->rank--;

	sprintf(buf, "{YYou have been demoted to %s!{x\n\r",
			get_chrank(member));

	if (member->ch != NULL)
		send_to_char(buf, member->ch);

	sprintf(buf, "{YYou demoted %s to %s!{x\n\r", member->name,
			get_chrank(member));
	send_to_char(buf, ch);

	return;
    }

    if (ch->church == NULL)
    {
	send_to_char("You aren't in a registered group.\n\r", ch);
	return;
    }

    for (member = ch->church->people; member != NULL;
	 member = member->next)
	if (!str_cmp(member->name, arg))
	    break;

    if (member == NULL)
    {
	send_to_char("Member not found.\n\r", ch);
	return;
    }

    if (member->rank <= CHURCH_RANK_A)
    {
	send_to_char("You can't demote that member any further.\n\r", ch);
	return;
    }

    member->rank--;

    sprintf(buf, "{YYou have been demoted to %s!{x\n\r",
    get_chrank(member));
    if (member->ch != NULL)
	send_to_char(buf, member->ch);

    sprintf(buf, "{YYou demoted %s to %s!{x\n\r",
        member->name,
        get_chrank(member));

    send_to_char(buf, ch);
}


void do_chgohall(CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *location;
    long pneuma_cost;
    long dp_cost;
    char buf[MSL];

    if (ch->church == NULL)
    {
	send_to_char("You must be in a registered group.\n\r", ch);
	return;
    }

    if (is_dead(ch))
	return;

    if (ch->church->size < CHURCH_SIZE_CULT)
    {
	send_to_char("Your group does not have a temple.\n\r", ch);
	return;
    }

    if (PULLING_CART(ch))
    {
	send_to_char("You must first drop what you are pulling.\n\r", ch);
	return;
    }

    if (IS_AFFECTED(ch, AFF_CURSE))
    {
	send_to_char("The curse keeps you where you are.\n\r", ch);
	return;
    }

    if ((location = location_to_room(&ch->church->recall_point)) == NULL)
    {
	send_to_char("You don't have a recall point.\n\r", ch);
	return;
    }

    if (ch->fighting != NULL)
    {
	send_to_char("You are fighting!\n\r", ch);
	return;
    }

	if (IS_DEAD(ch))
	{
		send_to_char("You can't gohall while dead.\n\r", ch);
		return;
	}
    if (ch->position == POS_SLEEPING)
    {
        send_to_char("Wake up first!\n\r", ch);
        return;
    }

    if (IN_WILDERNESS(ch) && ch->in_room->sector_type == SECT_WATER_NOSWIM)
    {
        send_to_char("The magic binding you to your church is powerless over water.\n\r", ch);
        return;
    }

    if (ch->no_recall > 0)
    {
        send_to_char("You can't summon enough energy.\n\r", ch);
        return;
    }

    pneuma_cost = 500;
    dp_cost = 50000;

    /* within areas (non-wilderness) */
    if ((ch->in_room->area->place_flags == PLACE_NOWHERE
    || ch->in_room->area->place_flags == PLACE_OTHER_PLANE
    || ch->in_room->area->place_flags == PLACE_ISLAND
    || !is_same_place(ch->in_room, location))
    && !IS_WILDERNESS(ch->in_room))
    {
        if (!IS_SET(ch->church->settings, CHURCH_ALLOW_CROSSZONES)
	&&  ch->church_member->rank < CHURCH_RANK_D)
	{
	    send_to_char("Your church leader has forsaken members from gohalling cross-zone.\n\r", ch);
	    return;
	}

	if (ch->church_member->rank < CHURCH_RANK_C
	&& !is_trusted(ch->church_member, "gohall"))
	{
            send_to_char("You aren't of high enough rank to recall that far.\n\r", ch);
	    return;
	}

	if (ch->church->pneuma < pneuma_cost
	|| ch->church->dp < dp_cost)
	{
            sprintf(buf,
	    "It costs %ld pneuma and %ld dp to recall that far.\n\r"
	    "Your church doesn't have enough.\n\r", pneuma_cost, dp_cost);
	    send_to_char(buf, ch);
	    return;
	}

	sprintf(buf,
	"{RWARNING:{x you are about to recall cross-zone.\n\rThis will cost your church %ld pneuma and %ld karma.\n\r", pneuma_cost, dp_cost);
	send_to_char(buf, ch);

	send_to_char("Are you sure you want to do this? (yes/no)\n\r", ch);

	ch->cross_zone_question = true;
	return;
    }

    if (!str_cmp(ch->in_room->area->name, "Wilderness"))
    {
	if (((location->area->place_flags == PLACE_FIRST_CONTINENT) && get_region(ch->in_room) != REGION_FIRST_CONTINENT) ||
		((location->area->place_flags == PLACE_SECOND_CONTINENT) && get_region(ch->in_room) != REGION_SECOND_CONTINENT) ||
		((location->area->place_flags == PLACE_THIRD_CONTINENT) && get_region(ch->in_room) != REGION_THIRD_CONTINENT) ||
		((location->area->place_flags == PLACE_FOURTH_CONTINENT) && get_region(ch->in_room) != REGION_FOURTH_CONTINENT))
	{
	    if (!IS_SET(ch->church->settings, CHURCH_ALLOW_CROSSZONES)
	    && ch->church_member->rank < CHURCH_RANK_D)
	    {
		send_to_char("Your church leader has forsaken members from gohalling cross-zone.\n\r", ch);
		return;
	    }

	    if (ch->church_member->rank < CHURCH_RANK_C
	    && !is_trusted(ch->church_member, "gohall"))
	    {
		send_to_char("You aren't of high enough rank to recall that far.\n\r", ch);
		return;
	    }

	    if (ch->church->pneuma < pneuma_cost
	    || ch->church->dp < dp_cost)
	    {
		sprintf(buf,
		    "It costs %ld pneuma and %ld dp to recall that far.\n\r"
		    "Your church doesn't have enough.\n\r",
		    pneuma_cost, dp_cost);
		send_to_char(buf, ch);
		return;
	    }

	    sprintf(buf,
		    "{RWARNING:{x you are about to recall cross-zone.\n\rThis will cost your church %ld pneuma and %ld karma.\n\r", pneuma_cost, dp_cost);
	    send_to_char(buf, ch);

	    send_to_char("Are you sure you want to do this? (yes/no)\n\r", ch);

	    ch->cross_zone_question = true;
	    return;
	}
    }

    act("{R$n disappears, leaving a resounding echo of discord.{X", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    char_from_room(ch);
    char_to_room(ch, location_to_room(&ch->church->recall_point));
    act("$n appears in the room.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    do_function(ch, &do_look, "auto");
}


void do_chflag(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *temp_char;
    char arg1[MAX_STRING_LENGTH];
    char buf[MSL];
    bool found = false;

    argument = one_argument_norm(argument, arg1);

    if (ch->church == NULL)
    {
	send_to_char("You must be in a registered group.\n\r", ch);
	return;
    }

    found = false;
    for (temp_char = ch->in_room->people; temp_char != NULL;
	 temp_char = temp_char->next_in_room)
    {
	if (IS_NPC(temp_char) && IS_SET(temp_char->act[1], ACT2_CHURCHMASTER))
	{
	    found = true;
	    break;
	}
    }

    if (!found)
    {
	send_to_char("You must be at a group administration office.\n\r", ch);
	return;
    }

    if (arg1[0] == '\0')
    {
	send_to_char("church setflag 'flag'", ch);
	return;
    }

    if (strlen_no_colours(arg1) > 16)
    {
        sprintf(buf, "Sorry %s, that flag is too long.", pers(ch, temp_char));
	do_say(temp_char, buf);
        return;
    }

    act("$n scribbles something down on a piece of parchment.",
	temp_char, ch, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    act("{C$n says 'Very well $N, your flag has now been changed.'{x",
	temp_char, ch, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

    if (ch->church->flag != NULL)
	free_string(ch->church->flag);

    smash_tilde(arg1);
    ch->church->flag = str_dup(arg1);
    return;
}


void do_chdeposit(CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *temp_char;
    char buf[MAX_STRING_LENGTH];
    char arg1[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    bool found;
    int amount;

    if (ch->church == NULL)
    {
	send_to_char(
	"You must be in a registered group to deposit.\n\r", ch);
	return;
    }


    found = false;
    for (temp_char = ch->in_room->people; temp_char != NULL;
	 temp_char = temp_char->next_in_room)
    {
	if (IS_NPC(temp_char) && IS_SET(temp_char->act[1], ACT2_CHURCHMASTER))
	    found = true;
    }

    if (!found)
    {
	send_to_char("You must be at a group administration office.\n\r", ch);
	return;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    amount = atoi(arg2);

    if (IS_NULLSTR(arg1) || IS_NULLSTR(arg2)
    || (str_cmp(arg1, "dp")
         && str_cmp(arg1, "pneuma")
         && str_cmp(arg1,"gold")))
    {
        send_to_char("Syntax: church deposit dp|pneuma|gold <amount>\n\r", ch);
	return;
    }

    if (!is_number(arg2))
    {
	send_to_char("You must provide a number.\n\r", ch);
	return;
    }

    if (amount <= 0)
    {
	send_to_char("Invalid amount.\n\r", ch);
	return;
    }

    if ((!str_cmp(arg1, "dp") && amount > ch->deitypoints)
	 || (!str_cmp(arg1, "pneuma") && amount > ch->pneuma)
	 || (!str_cmp(arg1, "gold") && amount > ch->gold))
    {
	send_to_char("You don't have that much.\n\r", ch);
	return;
    }

    if (!str_cmp(arg1, "pneuma"))
    {
    	ch->church->pneuma += amount;
    	ch->pneuma -= amount;
	ch->church_member->dep_pneuma += amount;
    	sprintf(buf, "{Y[%d pneuma transferred.]{x\n\r", amount);
    }

    if (!str_cmp(arg1, "dp"))
    {
        ch->church->dp += amount;
        ch->deitypoints -= amount;
        ch->church_member->dep_dp += amount;
        sprintf(buf, "{Y[%d deity points transferred.]{x\n\r", amount);
    }

    if (!str_cmp(arg1, "gold"))
    {
        ch->church->gold += amount;
        ch->gold -= amount;
        ch->church_member->dep_gold += amount;
        sprintf(buf, "{Y[%d gold deposited.]{x\n\r", amount);
    }

    send_to_char(buf, ch);

    write_churches_new();
    save_char_obj(ch);
    return;
}


void do_chbalance(CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *temp_char;
    CHURCH_DATA *church;
    char buf[MAX_STRING_LENGTH];
    char arg[MSL];
    bool found = false;

    argument = one_argument(argument, arg);

    if (ch->church == NULL && !IS_IMMORTAL(ch))
    {
	send_to_char("You must be in a registered group.\n\r", ch);
	return;
    }

    if (IS_IMMORTAL(ch))
    {
	if (arg[0] == '\0')
	{
	    send_to_char("Syntax: church balance <#>\n\r", ch);
	    return;
	}

	if ((church = find_church(atoi(arg))) == NULL)
	{
	    send_to_char("Church not found.\n\r", ch);
	    return;
	}

	sprintf(buf, "%s has %ld pneuma, %ld karma, and %ld gold.\n\r",
	    church->name,
	    church->pneuma,
	    church->dp,
	    church->gold);
	send_to_char(buf, ch);
	return;
    }

    found = false;
    for (temp_char = ch->in_room->people; temp_char != NULL;
	 temp_char = temp_char->next_in_room)
    {
	if (IS_NPC(temp_char) && IS_SET(temp_char->act[1], ACT2_CHURCHMASTER))
	    found = true;
    }

    if (!found)
    {
	send_to_char("You must be at a group administration office.\n\r",
		     ch);
	return;
    }

    sprintf(buf,
    "{CErrol says 'You have %ld pneuma, %ld karma, and %ld gold in your account.'{x\n\r",
        ch->church->pneuma, ch->church->dp, ch->church->gold);
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    return;
}


void do_chcreate(CHAR_DATA *ch, char *argument)
{
    CHURCH_DATA *church;
    CHURCH_PLAYER_DATA *player;
    CHAR_DATA *temp_char;
    char buf[MAX_STRING_LENGTH];
    char arg1[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    char arg3[MAX_STRING_LENGTH];
    bool found;
    int i;

    if (ch->church != NULL)
    {
	send_to_char(
        "You're in a church already!\n\r", ch);
	return;
    }

    found = false;
    for (temp_char = ch->in_room->people; temp_char != NULL;
	 temp_char = temp_char->next_in_room)
    {
        log_string(temp_char->name);
	if (IS_NPC(temp_char) && IS_SET(temp_char->act[1], ACT2_CHURCHMASTER))
	    found = true;
    }

    if (!found)
    {
	send_to_char("You must be at an administration office.\n\r", ch);
	return;
    }

    if (ch->deitypoints < 3000000)
    {
	send_to_char(
        "You must have 3,000,000 deity points to create a band.\n\r", ch);
	return;
    }

    for (i = 0, church = church_list; church != NULL; church = church->next)
	i++;

    if (i >= MAX_CHURCHES) {
	sprintf(buf, "Sentience only allows %d religions.\n\r", MAX_CHURCHES);
	send_to_char(buf, ch);
	return;
    }

    argument = one_argument_norm(argument, arg1);
    argument = one_argument_norm(argument, arg2);
    argument = one_argument(argument, arg3);

    if (strlen(arg2) > 25)
    {
        arg2[25] = '\0';
    }

    if (arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0')
    {
	send_to_char("CHURCH CREATE \"name\" 'flag' evil|good|neutral\n\r", ch);
	send_to_char(
	"For information on creating a band:\n\rHelp Church\n\r", ch);
	return;
    }

    if (str_cmp(arg3, "evil")
    && str_cmp(arg3, "good")
    && str_cmp(arg3, "neutral"))
    {
	send_to_char(
	"Your group must be registered as evil, good or neutral.\n\r", ch);
	return;
    }

	church = NULL;
	player = NULL;
	if( (church = new_church()) &&
		(player = new_church_player()) &&
		list_appendlink(church->online_players, ch) &&
		list_appendlink(church->roster, ch->name) &&
		list_appendlink(list_churches,church) ) {

		if (!church_list)
			church_list = church;
		else {
			CHURCH_DATA *temp_church;
			temp_church = church_list;
			while (temp_church->next)
				temp_church = temp_church->next;
			temp_church->next = church;
		}

		church->next = NULL;
		church->name = str_dup(arg1);
		church->max_positions = 10;
		church->pneuma = 0;
		church->size = CHURCH_SIZE_BAND;
		church->flag = str_dup(arg2);
		church->founder_last_login = current_time;
		church->created = current_time;
		church->rules = str_dup("No rules have been set yet.\n\r");
		church->motd = str_dup("No motd has been set yet.\n\r");
		church->founder = str_dup(ch->name);

		if (!str_cmp(arg3, "evil"))
			church->alignment = CHURCH_EVIL;
		else if (!str_cmp(arg3, "good"))
			church->alignment = CHURCH_GOOD;
		else
			church->alignment = CHURCH_NEUTRAL;

		player->next = NULL;
		player->ch = ch;
		player->name = str_dup(ch->name);
		player->rank = CHURCH_RANK_D;
		player->church = church;
		player->sex = ch->sex;

		church->people = player;
		ch->church = church;
		ch->church_name = str_dup(church->name);
		ch->church_member = player;

		ch->deitypoints -= 3000000;

		sprintf(buf, "{Y[%s has registered the Band of %s{Y]{x\n\r", ch->name, church->name);
		gecho(buf);

		write_churches_new();
	} else {
		if( church ) {
			list_remlink(list_churches, church);
			free_church(church);
		}
		if( player ) free_church_player( player);
		send_to_char("The gods do not smile upon you at this moment.", ch);
	}
}


void do_chdelete(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    int counter = 0;

    argument = one_argument(argument, arg);

	/* Add in a requirement for HEAD CHURCH SHAPER*/
    if(IS_NPC(ch) || !IS_IMMORTAL(ch) || (ch->tot_level < MAX_LEVEL || ch->pcdata->security < 9)) {
	send_to_char("You can't do that.\n\r", ch);
	return;
    }

    if (arg[0] == '\0')
    {
	send_to_char("This command is used to delete churches.\n\r", ch);
	send_to_char("Usage: church delete <church no.>\n\r", ch);
	send_to_char("Example: church delete 1\n\r", ch);
	send_to_char("\n\rThis would delete church 1.\n\r", ch);
	send_to_char("\n\r", ch);

	show_chlist_to_char(ch);
    }
    else
    {
	CHURCH_DATA *church = NULL;

	counter = 0;
	for (church = church_list; church != NULL; church = church->next)
	{
	    counter++;
	    if (counter == atoi(arg))
		break;
	}

	if (church == NULL)
	{
	    send_to_char("Group number not found.\n\r", ch);
	    return;
	}

	list_remlink(list_churches, church);
	extract_church(church);

	send_to_char("Church deleted.\n\r", ch);

	write_churches_new();
	return;
    }
}


void do_chlist(CHAR_DATA *ch, char *argument)
{
    CHURCH_DATA *church;
    CHURCH_PLAYER_DATA *member;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_STRING_LENGTH];
    int counter = 0;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
        show_chlist_to_char(ch);
    }
    else
    {
	if ((church = find_church(atoi(arg))) == NULL)
	{
	    send_to_char("Group number not found.\n\r", ch);
	    return;
	}

	send_to_char("{YThe ", ch);
	if (church->size == CHURCH_SIZE_BAND)
	{
	    send_to_char("Band", ch);
	}
	else if (church->size == CHURCH_SIZE_CULT)
	{
	    send_to_char("Cult", ch);
	}
	else if (church->size == CHURCH_SIZE_ORDER)
	{
	    send_to_char("Order", ch);
	}
	else if (church->size == CHURCH_SIZE_CHURCH)
	{
	    send_to_char("Church", ch);
	}

	sprintf(buf, " of %s. %s{x\n\r",
			church->name,
			(IS_IMMORTAL(ch)) ?
			((char *) ctime(&church->founder_last_login)) :
			"");
	send_to_char(buf, ch);
	send_to_char("{YNo.  Name                  Rank{x\n\r", ch);
	send_to_char("{Y-----------------------------------------{x\n\r", ch);
	counter = 0;
	for (member = church->people; member != NULL; member = member->next)
	{
	    bool online = false;
	    DESCRIPTOR_DATA *d;

	    if (member->ch != NULL && member->ch->desc != NULL)
	    {
                CHAR_DATA *wch = NULL;

		d = member->ch->desc;

		if (d->connected == CON_PLAYING)
		{
	 	    wch = (d->original != NULL) ? d->original : d->character;
		}

		if (wch != NULL)
	 	    online = true;
	    }

	    counter++;
	    sprintf(buf, "{G%-3d %s{Y%-21s %-15s %s{x\n\r",
		    counter,
	 	    online ? "{M*" : " ",
		    member->name,
		    get_chrank(member),
		    IS_SET(member->flags, CHURCH_PLAYER_EXCOMMUNICATED) ? "{RExcommunicated{x"
			     : "");
	    send_to_char(buf, ch);
	}

	send_to_char("{Y-----------------------------------------{x\n\r",
		     ch);
	sprintf(buf, "{Y%d member(s).{x\n\r", counter);
	send_to_char(buf, ch);
    }
}


void do_chtalk(CHAR_DATA *ch, char *argument)
{
    DESCRIPTOR_DATA *d;
    char buf[MAX_STRING_LENGTH];
    int counter;

    counter = 0;

    if (ch->church == NULL)
    {
	send_to_char("You aren't in a church.\n\r", ch);
	return;
    }

    if (argument[0] == '\0')
    {
        if (IS_SET(ch->comm, COMM_NOCT))
	{
	    send_to_char("You will now hear church talks.\n\r", ch);
	    REMOVE_BIT(ch->comm, COMM_NOCT);
	}
	else
	{
	    send_to_char("You will no longer hear church talks.\n\r", ch);
	    SET_BIT(ch->comm, COMM_NOCT);
	}

	return;
    }

    if (IS_SET(ch->in_room->room_flag[0], ROOM_NOCOMM))
    {
        send_to_char("You can't seem to gather enough energy to do it.\n\r", ch);
	return;
    }

    for (d = descriptor_list; d != NULL; d = d->next)
    {
	CHAR_DATA *victim;

	victim = d->original ? d->original : d->character;

	if (d->connected == CON_PLAYING
	&& d->character != ch
	&& !is_ignoring(d->character, ch)
	&& !IS_SET(d->character->comm, COMM_NOCT)
	&& d->character->church == ch->church)
	{
	    counter++;
	    if (!IS_NPC(ch) && ch->pcdata->flag != NULL && SHOW_CHANNEL_FLAG(victim, FLAG_CT))
	    {
		sprintf(buf, "{%c[{%c%s{%c] says '%s {%c%s{%c'{x\n\r",
		    ch->church->colour2,
		    ch->church->colour1,
		    ch->name,
		    ch->church->colour2,
		    ch->pcdata->flag,
		    ch->church->colour1,
		    argument,
		    ch->church->colour2);
	    }
	    else
	    {
		sprintf(buf, "{%c[{%c%s{%c] says '{%c%s{%c'{x\n\r",
		    ch->church->colour2,
		    ch->church->colour1,
		    ch->name,
		    ch->church->colour2,
		    ch->church->colour1,
		    argument,
		    ch->church->colour2);
	    }
	    send_to_char(buf, d->character);
	}
    }

    if (ch->pcdata->flag != NULL && SHOW_CHANNEL_FLAG(ch, FLAG_CT))
    {
	if (counter > 1)
	{
	    sprintf(buf, "{%c[{%c%d{%c] people heard you say '%s {%c%s{%c'{x\n\r",
		    ch->church->colour2,
		    ch->church->colour1,
		    counter,
		    ch->church->colour2,
		    ch->pcdata->flag,
		    ch->church->colour1,
		    argument,
		    ch->church->colour2);
	    send_to_char(buf, ch);
	}
	else if (counter == 1)
	{
	    sprintf(buf, "{%c[{%c%d{%c] person heard you say '%s {%c%s{%c'{x\n\r",
		    ch->church->colour2,
		    ch->church->colour1,
		    counter,
		    ch->church->colour2,
		    ch->pcdata->flag,
		    ch->church->colour1,
		    argument,
		    ch->church->colour2);
	    send_to_char(buf, ch);
	}
	else
	    send_to_char("No one hears your voice.\n\r", ch);
    }
    else
    {
	if (counter > 1)
	{
	    sprintf(buf, "{%c[{%c%d{%c] people heard you say '{%c%s{%c'{x\n\r",
		    ch->church->colour2,
		    ch->church->colour1,
		    counter,
		    ch->church->colour2,
		    ch->church->colour1,
		    argument,
		    ch->church->colour2);
	    send_to_char(buf, ch);
	}
	else if (counter == 1)
	{
	    sprintf(buf, "{%c[{%c%d{%c] person heard you say '{%c%s{%c'{x\n\r",
		    ch->church->colour2,
		    ch->church->colour1,
		    counter,
		    ch->church->colour2,
		    ch->church->colour1,
		    argument,
		    ch->church->colour2);
	    send_to_char(buf, ch);
	}
	else
	    send_to_char("No one hears your voice.\n\r", ch);
    }

}


char *get_chrank(CHURCH_PLAYER_DATA *member)
{
    char *rank_name;
    char buf[MAX_STRING_LENGTH];

    if (member == NULL)
    {
        sprintf(buf, "get_chrank: member was null!");
	bug(buf, 0) ;
	return NULL;
    }

    if (member->church == NULL)
    {
	sprintf(buf, "get_chrank: member->church was null! %s",
		member->name);
	bug (buf, 0);
	return NULL;
    }

    if (member->sex == SEX_FEMALE)
    {
	switch (member->church->size)
	{
  	    case CHURCH_SIZE_BAND:
              	rank_name = church_band_rank_table[member->rank].frank_name;
		break;
	    case CHURCH_SIZE_CULT:
		rank_name = church_cult_rank_table[member->rank].frank_name;
		break;
	    case CHURCH_SIZE_ORDER:
		rank_name = church_order_rank_table[member->rank].frank_name;
		break;
	    case CHURCH_SIZE_CHURCH:
		rank_name = church_church_rank_table[member->rank].frank_name;
		break;
	    default:
		rank_name = church_band_rank_table[0].frank_name;
		break;
	}
    }
    else
    {
        switch (member->church->size)
	{
	    case CHURCH_SIZE_BAND:
   	        rank_name = church_band_rank_table[member->rank].mrank_name;
		break;
	    case CHURCH_SIZE_CULT:
		rank_name = church_cult_rank_table[member->rank].mrank_name;
		break;
	    case CHURCH_SIZE_ORDER:
		rank_name = church_order_rank_table[member->rank].mrank_name;
		break;
	    case CHURCH_SIZE_CHURCH:
		rank_name = church_church_rank_table[member->rank].mrank_name;
		break;
	    default:
		rank_name = church_band_rank_table[0].mrank_name;
		break;
	}
    }

    return rank_name;
}


void do_chexcommunicate(CHAR_DATA * ch, char *argument)
{
    CHURCH_PLAYER_DATA *member;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char
	    ("For use on CHURCH EXCOMMUNICATE: \n\rHelp Church\n\r", ch);
	return;
    }

    for (member = ch->church->people; member != NULL; member = member->next)
    {
	if (!str_cmp(member->name, arg))
	    break;
    }

    if (member == NULL)
    {
	send_to_char("That isn't a member of your church.\n\r", ch);
	return;
    }

    if (member->rank == CHURCH_RANK_D)
    {
	act("You may not excommunicate a church leader.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    if (IS_SET(member->flags, CHURCH_PLAYER_EXCOMMUNICATED))
    {
	REMOVE_BIT(member->flags, CHURCH_PLAYER_EXCOMMUNICATED);
	sprintf(buf, "{R%s is no longer excommunicated.{x\n\r", member->name);
	church_echo(ch->church, buf);
	return;
    }
    else
    {
	SET_BIT(member->flags, CHURCH_PLAYER_EXCOMMUNICATED);
	sprintf(buf, "{R%s has been excommunicated.{x\n\r", member->name);
	church_echo(ch->church, buf);
	return;
    }
}


/* show list of churches to a ch. Used in other functions. */
void show_chlist_to_char(CHAR_DATA *ch)
{
    char buf[MAX_STRING_LENGTH];
    CHURCH_DATA *church;
    int i;

    send_to_char("{YRegistered Factions within Sentience:{x\n\r", ch);
    send_to_char(
    "{YNo. PK  Name                          Max   Alignment    Size{x\n\r", ch);
    line(ch , 83);
    i = 0;
    for (church = church_list; church != NULL; church = church->next)
    {
        i++;

        sprintf(buf, "{G%-3d{x {R%-3s{x %-30.30s %-4d %-15s{x",
            i,
            church->pk == true ? "PK" : "",
	    church->name,
	    church->max_positions,
	    church->alignment == CHURCH_EVIL ?
	    "{REvil" : (church->alignment == CHURCH_GOOD ?
		    "{WGood" : "{xNeutral"));
	    send_to_char(buf, ch);

	    if (church->size == CHURCH_SIZE_BAND)
		send_to_char("Band   ", ch);
	    else if (church->size == CHURCH_SIZE_CULT)
		send_to_char("Cult   ", ch);
	    else if (church->size == CHURCH_SIZE_ORDER)
		send_to_char("Order  ", ch);
	    else if (church->size == CHURCH_SIZE_CHURCH)
		send_to_char("Church ", ch);

	    sprintf(buf, "%s\n\r", church->flag);
	    send_to_char(buf, ch);
	}

	line (ch, 83);
	sprintf(buf, "{Y%d group(s) found.{x\n\r", i);

	send_to_char(buf, ch);
}


/* message all members of a church */
void msg_church_members(CHURCH_DATA *church, char *argument)
{
    CHURCH_PLAYER_DATA *member;

    if (church == NULL)
        return;

    for (member = church->people; member != NULL; member = member->next)
    {
	if (member->ch != NULL)
	    send_to_char(argument, member->ch);
    }
}


void do_chadvance(CHAR_DATA *ch, char* argument)
{
    char arg[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHURCH_DATA *church;

    argument = one_argument(argument, arg);

    if (ch->tot_level < 155)
    {
        send_to_char("Huh?\n\r", ch);
        return;
    }

    if (arg[0] == '\0')
    {
        send_to_char("Advance which church?\n\r", ch);
	show_chlist_to_char(ch);
        return;
    }

    if (!is_number(arg))
    {
        send_to_char("That's not even a number!\n\r", ch);
		show_chlist_to_char(ch);
		return;
    }

    if ((church = find_church(atoi (arg))) == NULL)
    {
        send_to_char("No such church.\n\r", ch);
        return;
    }

    if (church->size == CHURCH_SIZE_CHURCH)
    {
		send_to_char("They're already at the maximum level.\n\r", ch);
		return;
    }

    church->size += 1;

	// Update their max roster size if necessary
    int max_pos = church_get_min_positions(church->size);
    church->max_positions = UMAX(church->max_positions, max_pos);

    sprintf(buf, "{Y[%s is now %s %s!]{x\n\r",
        church->name,
        church->size == CHURCH_SIZE_ORDER ? "an" : "a",
	get_chsize_from_number(church->size));

    gecho(buf);

    write_churches_new();
}


void do_chdeduct(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    char arg3[MAX_STRING_LENGTH];
    char buf[2*MAX_STRING_LENGTH];
    int amt;
    int i;
    CHURCH_DATA *church;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (ch->tot_level < 155)
    {
        send_to_char("Huh?\n\r", ch);
        return;
    }

    if (arg[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0')
    {
        send_to_char(
        "Syntax:\n\rchurch deduct <church#> <pneuma|dp|gold> <amt>\n\r", ch);
	show_chlist_to_char(ch);
        return;
    }

    i = 0;
    for (church = church_list; church != NULL; church = church->next)
    {
        i++;
        if (i == atoi(arg))
   	    break;
    }

    if (church == NULL)
    {
        send_to_char("No such church.\n\r", ch);
        return;
    }

    if (str_cmp(arg2, "dp")
    && str_cmp(arg2, "pneuma")
    && str_cmp(arg2, "gold"))
    {
        send_to_char(
        "Syntax:\n\rchurch <church#> <pneuma|dp|gold> <amt>\n\r", ch);
	show_chlist_to_char(ch);
	return;
    }

    amt = atoi(arg3);

    if (amt < 0)
    {
        send_to_char("Invalid amount.\n\r", ch);
        return;
    }

    if ((!str_cmp(arg2,"pneuma") && amt > church->pneuma)
    || (!str_cmp(arg2, "dp") && amt > church->dp)
    || (!str_cmp(arg2, "gold") && amt > church->gold))
    {
        send_to_char("They don't have that much.\n\r", ch);
        return;
    }

    if (!str_cmp(arg2, "pneuma")) church->pneuma -= amt;

    if (!str_cmp(arg2, "dp")) church->dp -= amt;

    if (!str_cmp(arg2, "gold")) church->gold -= amt;

    sprintf(buf,
        "{Y[%s has deducted %d %s from your church.]{x\n\r",
        ch->name, amt, arg2);

    msg_church_members(church, buf);

    sprintf(buf, "You have deducted %d %s from %s.\n\r",
        amt, arg2, church->name);
    send_to_char(buf, ch);

    write_churches_new();
}


char *get_chsize_from_number(int size)
{
    if (size == CHURCH_SIZE_BAND)
        return "Band";
    else if (size == CHURCH_SIZE_CULT)
 	return "Cult";
    else if (size == CHURCH_SIZE_ORDER)
 	return "Order";
    else
 	return "Church";
}


void do_chinfo(CHAR_DATA *ch, char *argument)
{
    CHURCH_DATA *church;
    CHURCH_PLAYER_DATA *member;
    char arg[MSL];
    char buf[MAX_STRING_LENGTH];
    char buf2[MSL];
    BUFFER *buffer;
    int i;
    int x;
    int box_width;

    argument = one_argument(argument, arg);

    if (arg[0] != '\0')
    {
	/* Edit your church info*/
	if (!str_cmp(arg, "edit"))
	{
	    if ((church = ch->church) == NULL || ch->church_member == NULL)
	    {
		send_to_char("You aren't in a church.\n\r", ch);
		return;
	    }

	    if (ch->church_member->rank < CHURCH_RANK_D
            && !is_trusted(ch->church_member, "info"))
	    {
		send_to_char("You aren't high enough rank to do that.\n\r", ch);
		return;
	    }

	    string_append(ch, &ch->church->info);
	    write_churches_new();
	    return;
	}

	/* imms can look up info on churches for convenience*/
	if (ch->tot_level >= MAX_LEVEL - 3)
	{
	    if ((church = find_church(atoi(arg))) == NULL)
	    {
		send_to_char("There is no such church.\n\r", ch);
		return;
	    }

	    show_church_info(church, ch);
	    return;
	}

        /* Look up info of another church*/
	if ((church = find_church(atoi(arg))) == NULL)
	{
	    send_to_char("There is no such church.\n\r", ch);
	    return;
	}

	box_width = 70;

	buffer = new_buf();

        /* Top edge*/
	add_buf(buffer, "{b.");
	for (x = 0; x < box_width; x++)
	    add_buf(buffer, "-");

        add_buf(buffer, ".{x\n\r");

	/* Blank line*/
	add_buf(buffer, "{b|");
	for (x = 0; x < box_width; x++)
	    add_buf(buffer, " ");

	add_buf(buffer, "{b|\n\r");

        /* Name*/
	sprintf(buf, "{b|    {WThe %s of %s{x",
	    get_chsize_from_number(church->size), church->name);
	for (x = strlen(buf) - 7; x < box_width; x++)
	    strcat(buf, " ");

	strcat(buf, "{b|{x\n\r");
	add_buf(buffer, buf);

	add_buf(buffer, "{b|");

	for (x = 0; x < box_width; x++)
	    add_buf(buffer, " ");

	add_buf(buffer, "{b|\n\r");

	/* Date created
	sprintf(buf2, "{b|    {xDate Created: %s{x",
	    church->created == 0 ? "No Record" : time_string);
	strcat(buf, buf2);

	free_string(time_string);
	*/

	/* PK record*/
	if (IS_SET(church->settings, CHURCH_SHOW_PKS))
	{
	    sprintf(buf, "{b|    {YPlayer kills:          {x%ld", church->pk_wins);
	    for (x = strlen(buf) - 7; x < box_width; x++)
		strcat(buf, " ");

	    strcat(buf, "{b|{x\n\r");
	    add_buf(buffer, buf);

	    sprintf(buf, "{b|    {YChaotic player kills:  {x%ld", church->cpk_wins);
	    for (x = strlen(buf) - 7; x < box_width; x++)
		strcat(buf, " ");

	    strcat(buf, "{b|{x\n\r");
	    add_buf(buffer, buf);

	    sprintf(buf, "{b|    {YWars won:              {x%ld", church->wars_won);
	    for (x = strlen(buf) - 7; x < box_width; x++)
		strcat(buf, " ");

	    strcat(buf, "{b|{x\n\r");
	    add_buf(buffer, buf);
	}

	/* Blank line*/
	add_buf(buffer, "{b|");
	for (x = 0; x < box_width; x++)
	    add_buf(buffer, " ");

	add_buf(buffer, "{b|\n\r");

	/* Info*/
	sprintf(buf, "{b|    {x");

        for (i = 0, x = 6; church->info[i] != '\0'; i++)
	{
	    if (church->info[i] == '\n')
	    {
		i++;
		for (; x < box_width + 2; x++)
		    strcat(buf, " ");

		strcat(buf, "{b|\n\r");
		strcat(buf, "{b|    {x");
		x = 6;
		continue;
	    }

	    sprintf(buf2, "%c", church->info[i]);
	    strcat(buf, buf2);
	    if (church->info[i] != '{'
	    &&  !(i > 0 && church->info[i-1] == '{'))
		x++;

	    if (x == box_width - 2)
	    {
		for (x = 0; x < 4; x++)
		    strcat(buf, " ");

		x = 6;
		strcat(buf, "{b|{x\n\r");
		strcat(buf, "{b|    {x");
	    }
	}

	for (; x < box_width + 2; x++)
	    strcat(buf, " ");

	strcat(buf, "{b|\n\r");

	add_buf(buffer, buf);

	/* Bottom edge*/
	add_buf(buffer, "{b``");
	for (x = 0; x < box_width; x++)
	    add_buf(buffer, "-");

        add_buf(buffer, "'{x\n\r");

	page_to_char(buf_string(buffer), ch);
	free_buf(buffer);
	return;
    }
    else
    {
	church = ch->church;
	if (church == NULL)
	{
	    send_to_char("You aren't in a church.\n\r", ch);
	    return;
	}
    }

    /* Show own church info*/
    if (IS_SET(church->settings, CHURCH_SHOW_PKS))
    {
	sprintf(buf,
	    "{Y #  %-12s %-10s %-10s %-10s %-8s %-8s %-4s{x\n\r",
	    "Name", "Pneuma", "Karma", "Gold", "PK", "CPK", "Wars");
	send_to_char(buf, ch);
	line(ch,78);
	i = 0;

	for (member = church->people; member != NULL; member = member->next)
	{
	    i++;

	    sprintf(buf,
		"{Y%2d){x %-12s %-10ld %-10ld %-8ld",
		i,
		member->name,
		member->dep_pneuma,
		member->dep_dp,
		member->dep_gold);

	    sprintf(buf2, "%4ld-%-4ld", member->pk_wins, member->pk_losses);
	    strcat(buf, buf2);

	    sprintf(buf2, "%4ld-%-4ld", member->cpk_wins, member->cpk_losses);
	    strcat(buf, buf2);

	    sprintf(buf2, "%4ld\n\r", member->wars_won);
	    strcat(buf, buf2);

	    send_to_char(buf, ch);
	}

	line(ch,78);
    }
    else
    {
	sprintf(buf,
	    "{Y #  %-12s %-10s %-10s %-10s{x\n\r",
	    "Name", "Pneuma", "Karma", "Gold");
	send_to_char(buf, ch);
	line(ch, 55);
	i = 0;

	for (member = church->people; member != NULL; member = member->next)
	{
	    i++;

	    sprintf(buf,
		"{Y%2d){x %-12s %-10ld %-10ld %-8ld\n\r",
		i,
		member->name,
		member->dep_pneuma,
		member->dep_dp,
		member->dep_gold);

	    send_to_char(buf, ch);
	}

	line(ch, 55);
    }

    CHURCH_TREASURE_ROOM *treasure;
    ITERATOR it;
	iterator_start(&it, church->treasure_rooms);
	while( (treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it)) ) {
		if( treasure->room != NULL )
		{
			ROOM_INDEX_DATA *room = treasure->room;
			for (OBJ_DATA *obj = room->contents; obj != NULL; obj = obj->next_content)
			{
				if (is_relic(obj->pIndexData))
				{
					sprintf(buf, "{M*{x Your church is currently in the possession of %s.\n\r", obj->short_descr);
					send_to_char(buf, ch);
				}
			}
		}
	}
	iterator_stop(&it);
}


void do_chtransfer(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    char arg3[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHURCH_DATA *church;
    long amount;
    bool found_admin = false;
    CHAR_DATA *temp_char;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (ch->church == NULL)
    {
        send_to_char("You aren't even in a church!\n\r", ch);
        return;
    }

    found_admin = false;
    for (temp_char = ch->in_room->people;
          temp_char != NULL;
          temp_char = temp_char->next_in_room)
    {
        if (IS_NPC(temp_char)
        && IS_SET(temp_char->act[1], ACT2_CHURCHMASTER))
	{
	    found_admin = true;
	    break;
	}
    }

    if (!found_admin)
    {
        send_to_char(
	    "You must be at an administration office.\n\r", ch);
	return;
    }

    if (arg[0]  == '\0'
    ||   arg2[0] == '\0'
    ||   arg3[0] == '\0'
    || !is_number(arg)
    || !is_number(arg3)
    || (str_cmp(arg2, "pneuma")
         && str_cmp(arg2, "dp")
         && str_cmp(arg2, "gold")))
    {
        send_to_char(
            "Syntax: church transfer <#> <pneuma|dp|gold> <amount>\n\r", ch);
	show_chlist_to_char(ch);
	return;
    }

    if ((church = find_church(atoi(arg))) == NULL )
    {
        send_to_char("No church with that number was found.\n\r", ch);
	return;
    }

    if (church == ch->church)
    {
 	send_to_char("That would be quite pointless.\n\r", ch);
	return;
    }

    amount = atol(arg3);

    if ((!str_cmp(arg2, "pneuma") && amount > ch->church->pneuma)
    || (!str_cmp(arg2, "dp") && amount > ch->church->dp)
    || (!str_cmp(arg2, "gold") && amount > ch->church->gold))
    {
        send_to_char("You don't have that much.\n\r", ch);
        return;
    }

    if (!str_cmp(arg2, "pneuma"))
    {
        ch->church->pneuma -= amount;
	church->pneuma     += amount;

	sprintf(buf,
	    "{Y[%s transferred %ld pneuma into %s's account.]\n\r",
	    ch->name, amount, church->name);
	msg_church_members(ch->church, buf);

	sprintf(buf,
	"{Y[%s transferred %ld pneuma from %s into your account.]{x\n\r",
	    ch->name, amount, ch->church->name);
	msg_church_members(church, buf);
	return;
    }

    if (!str_cmp(arg2, "dp"))
    {
        ch->church->dp    -= amount;
	church->dp        += amount;

	sprintf(buf,
	    "{Y[%s transferred %ld karma into %s's account.]\n\r",
	    ch->name, amount, church->name);
	msg_church_members(ch->church, buf);

	sprintf(buf,
	    "{Y[%s transferred %ld karma from %s into your account.]{x\n\r",
	    ch->name, amount, ch->church->name);
	msg_church_members(church, buf);
	return;
    }

    if (!str_cmp(arg2, "gold"))
    {
        ch->church->gold -= amount;
	church->gold     += amount;

	sprintf(buf,
	    "{Y[%s transferred %ld gold into %s's account.]\n\r",
	    ch->name, amount, church->name);
	    msg_church_members(ch->church, buf);

	    sprintf(buf,
	    "{Y[%s transferred %ld gold from %s into your account.]{x\n\r",
			    ch->name, amount, ch->church->name);
	    msg_church_members(church, buf);
	    return;
    }
}


void do_chwithdraw(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    char arg3[MSL];
    char buf[2*MAX_STRING_LENGTH];
    CHAR_DATA *victim;
    CHAR_DATA *mob;
    CHURCH_PLAYER_DATA *member;
    long amt;
    bool found = false;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (ch->church == NULL)
    {
        send_to_char("You aren't even in a church.\n\r", ch);
        return;
    }

    /*
    if (ch->church_member->rank < CHURCH_RANK_B && !is_trusted(ch->church_member, "withdraw"))
    {
        send_to_char("You're not of the proper rank to do it.\n\r", ch);
	return;
    }
    */

    for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
    {
	if (IS_NPC(mob) && IS_SET(mob->act[1], ACT2_CHURCHMASTER))
	{
	    found = true;
	    break;
	}
    }

    if (!found)
    {
	send_to_char("You can't do that here.\n\r", ch);
	return;
    }

    if ( arg[0] == '\0'
    ||   arg2[0] == '\0')
    {
        send_to_char(
	    "Syntax: church withdraw <pneuma|dp|gold> <amount>\n\r"
	    "        church withdraw <person> <pneuma|dp|gold> <amount> (leaders only)\n\r", ch);
	return;
    }

    if (arg3[0] != '\0')
    {
	if (ch->church_member->rank < CHURCH_RANK_D
	&& !is_trusted(ch->church_member, "withdraw"))
	{
	     send_to_char("Only a leader may transfer balance to church members.\n\r", ch);
	     return;
	}

	if (arg3[0] == '\0'
	|| (str_cmp(arg2, "pneuma")
	     && str_cmp(arg2, "dp")
	     && str_cmp(arg2, "gold")))
	{
	    send_to_char("Syntax: church withdraw <person> <pneuma|dp|gold> <amount>\n\r", ch);
	    return;
	}

	victim = get_char_room(ch, NULL, arg);
	if (victim == NULL)
	{
	    send_to_char("They must be in the same room as you.\n\r", ch);
	    return;
	}

	amt = atol(arg3);
	if ((!str_cmp(arg2, "pneuma") && ch->church->pneuma < amt)
	|| (!str_cmp(arg2, "dp") && ch->church->dp < amt)
	|| (!str_cmp(arg2, "gold") && ch->church->gold < amt))
	{
	    sprintf(buf, "Your church doesn't have that much %s.\n\r", arg2);
	    send_to_char(buf, ch);
	    return;
	}

	if (!str_cmp(arg2, "pneuma"))
	{
	    sprintf(buf, "{Y[You transferred %ld pneuma to %s.]{x\n\r",
	    	amt, victim->name);
	    send_to_char(buf, ch);

	    sprintf(buf, "{Y[%s has transferred %ld pneuma to you from %s.]{x\n\r", ch->name, amt, ch->church->name);
	    send_to_char(buf, victim);

	    for (member = ch->church->people; member != NULL;
	          member = member->next)
	    {
		if (member->ch != NULL && member->ch->desc != NULL
		&& member->rank == CHURCH_RANK_D && member->ch != ch
		&& member->ch != victim)
		{
		    sprintf(buf, "{Y[%s has transferred %ld pneuma to %s.]{x\n\r", ch->name, amt, victim->name);
		    send_to_char(buf, member->ch);
		}
	    }

	    ch->church->pneuma -= amt;
	    victim->pneuma += amt;
	    return;
	}

	if (!str_cmp(arg2, "dp"))
	{
	    sprintf(buf, "{Y[You transferred %ld dp to %s.]{x\n\r",
	    	amt, victim->name);
	    send_to_char(buf, ch);

	    sprintf(buf, "{Y[%s has transferred %ld dp to you from %s.]{x\n\r", ch->name, amt, ch->church->name);
	    send_to_char(buf, victim);

	    for (member = ch->church->people; member != NULL;
	          member = member->next)
	    {
		if (member->ch != NULL && member->ch->desc != NULL
		&& member->rank == CHURCH_RANK_D && member->ch != ch
		&& member->ch != victim)
		{
		    sprintf(buf, "{Y[%s has transferred %ld dp to %s.]{x\n\r", ch->name, amt, victim->name);
		    send_to_char(buf, member->ch);
		}
	    }

	    ch->church->dp -= amt;
	    victim->deitypoints += amt;
	    return;
	}

	if (!str_cmp(arg2, "gold"))
	{
	    sprintf(buf, "{Y[You transferred %ld gold to %s.]{x\n\r",
	    	amt, victim->name);
	    send_to_char(buf, ch);

	    sprintf(buf, "{Y[%s has transferred %ld gold to you from %s.]{x\n\r", ch->name, amt, ch->church->name);
	    send_to_char(buf, victim);

	    for (member = ch->church->people; member != NULL;
	          member = member->next)
	    {
		if (member->ch != NULL && member->ch->desc != NULL
		&& member->rank == CHURCH_RANK_D && member->ch != ch
		&& member->ch != victim)
		{
		    sprintf(buf, "{Y[%s has transferred %ld gold to %s.]{x\n\r", ch->name, amt, victim->name);
		    send_to_char(buf, member->ch);
		}
	    }

	    ch->church->gold -= amt;
	    victim->gold += amt;
	    return;
	}
    }
    else
    {
	amt = atol(arg2);
	if (amt <= 0)
	{
	    send_to_char("That amount is invalid.\n\r", ch);
	    return;
	}

	if (!str_cmp(arg, "pneuma"))
	{
	    if (amt > ch->church_member->dep_pneuma)
	    {
		send_to_char(
			"You can't take out more than you put in.\n\r", ch);
		return;
	    }

	    if (amt > ch->church->pneuma)
	    {
		send_to_char(
			"Sorry, there's not that much in the account.\n\r", ch);
		return;
	    }

	    sprintf(buf, "{Y[%s has withdrawn %ld pneuma.]{x\n\r",
		    ch->name, amt);
	    msg_church_members(ch->church, buf);

	    /*
	       if (ch->church->log != NULL)
	       new_log = str_dup(ch->church->log);
	       else
	       new_log = str_dup("");

	       free_string(ch->church->log);

	       sprintf(new_log, "\n\r[%s] %s withdrew %ld pneuma.",
	       (char *) ctime(&current_time), ch->name, amt);
	       ch->church->log = new_log; */

	    ch->church->pneuma -= amt;
	    ch->church_member->dep_pneuma -= amt;
	    ch->pneuma += amt;
	}

	if (!str_cmp(arg, "dp") || !str_cmp(arg, "karma"))
	{
	    if (amt > ch->church_member->dep_dp)
	    {
		send_to_char(
			"You can't take out more than you put in.\n\r", ch);
		return;
	    }

	    if (amt > ch->church->dp)
	    {
		send_to_char(
			"Sorry, there's not that much in the account.\n\r", ch);
		return;
	    }

	    sprintf(buf, "{Y[%s has withdrawn %ld karma.]{x\n\r",
		    ch->name, amt);
	    msg_church_members(ch->church, buf);
	    /*
		sprintf(buf, "\n\r[%s] %s withdrew %ld karma.",
		(char *) ctime(&current_time), ch->name, amt);
		strcat(ch->church->log, buf);
	     */
	    ch->church->dp -= amt;
	    ch->church_member->dep_dp -= amt;
	    ch->deitypoints += amt;
	}

	if (!str_cmp(arg, "gold"))
	{
	    if (amt > ch->church_member->dep_gold)
	    {
		send_to_char(
			"You can't take out more than you put in.\n\r", ch);
		return;
	    }

	    if (amt > ch->church->gold)
	    {
		send_to_char(
			"Sorry, there's not that much in the account.\n\r", ch);
		return;
	    }

	    sprintf(buf, "{Y[%s has withdrawn %ld gold.]{x\n\r",
		    ch->name, amt);
	    msg_church_members(ch->church, buf);
	    /*
	       sprintf(buf, "\n\r[%s] %s withdrew %ld gold.",
	       (char *) ctime(&current_time), ch->name, amt);
	       strcat(ch->church->log, buf);
	     */
	    ch->church->gold -= amt;
	    ch->church_member->dep_gold -= amt;
	    ch->gold += amt;
	}
    }
}


/* church log- shows important stuff which is logged, such as withdrawls. */
void do_chlog(CHAR_DATA *ch, char *argument)
{
    CHURCH_DATA *church;
    char arg[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    BUFFER *output;

    argument = one_argument(argument, arg);

    if (ch->church == NULL)
    {
        send_to_char("You aren't even in a church!\n\r", ch);
        return;
    }

    if (ch->church_member->rank < CHURCH_RANK_C)
    {
        send_to_char("You may not view the log.\n\r", ch);
	return;
    }

    church = ch->church;

    if (!str_cmp(arg, "edit"))
    {
        string_append(ch, &ch->church->log);
        return;
    }

    if (church->log == NULL)
    {
	send_to_char("Nothing has been written to the log yet.\n\r", ch);
	return;
    }

    output = new_buf();

    if (ch->church->log == NULL)
    {
       send_to_char("Nothing as of yet has been written in the log.\n\r", ch);
       return;
    }

    sprintf(buf, "{YThe Log of %s:{x\n\r", ch->church->name);
    send_to_char(buf, ch);

    add_buf(output, ch->church->log);

    page_to_char(buf_string(output), ch);

    free_buf(output);
}


void do_choverthrow(CHAR_DATA *ch, char *argument)
{
    CHURCH_DATA *church;
    CHURCH_PLAYER_DATA *member;
    char buf[MSL];

    if ((church = ch->church) == NULL)
    {
  	send_to_char("You aren't even in a church.\n\r", ch);
 	return;
    }

    if (!str_cmp(ch->name, church->founder))
    {
	send_to_char("That would be pointless, you're already the founder.\n\r", ch);
	return;
    }

    if (church->size == CHURCH_SIZE_CHURCH)
    {
	send_to_char("You can't overthrow a church that big.\n\r", ch);
	return;
    }

    member = NULL;
    for (member = church->people; member != NULL; member = member->next)
    {
	if (member != ch->church_member) /* demote everyone else */
	    member->rank = CHURCH_RANK_A;
    }

    sprintf(buf,"{Y[%s has overthrown the %s of %s!]{x\n\r",
        ch->name,
	get_chsize_from_number(church->size),
	church->name);
    gecho(buf);

    free_string(church->founder);
    church->founder = str_dup(ch->name);

    write_churches_new();
}


void do_chwhere(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    DESCRIPTOR_DATA *d;
    bool found;

    one_argument(argument, arg);

    if (ch->church == NULL)
    {
	send_to_char("You aren't in a church.\n\r", ch);
	return;
    }

    if (arg[0] == '\0')
    {
	send_to_char(
	"You detect the location of your fellow members:\n\r{x", ch);
	found = false;
	for (d = descriptor_list; d; d = d->next)
	{
	    if (d->connected == CON_PLAYING
	    &&  (victim = d->character) != NULL
	    &&  victim->church != NULL
	    &&  victim->church == ch->church && !IS_NPC(victim)
	    &&  victim->in_room != NULL)
	    {
		found = true;
		    sprintf(buf, "{Y%-28s{X %s\n\r",
			    pers(victim, ch), victim->in_room->name);
		send_to_char(buf, ch);
	    }
	}
	if (!found)
	    send_to_char("None\n\r", ch);
    }
}


void do_chtoggle(CHAR_DATA * ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *temp_char;
    bool found = false;

    argument = one_argument(argument, arg);

    if (ch->church == NULL)
    {
	send_to_char("You aren't in a church.\n\r", ch);
	return;
    }

    found = false;
    for (temp_char = ch->in_room->people; temp_char != NULL;
	 temp_char = temp_char->next_in_room) {
	if (IS_NPC(temp_char) &&
			IS_SET(temp_char->act[1], ACT2_CHURCHMASTER))
	    found = true;
    }

    if (!found) {
	send_to_char
	    ("You must be at an administration office.\n\r",
	     ch);
	return;
    }

    if (ch->church->pk == true)
    {
	if(ch->church->pneuma >= 5000)
	    ch->church->pneuma -= 5000;
	else
	{
	    send_to_char("It costs 5000 pneuma to toggle off your PK flag.\n\r", ch);
	    return;
	}

	ch->church->pk = false;
	sprintf(buf, "{Y[%s is no longer a player killing church!]{x\n\r", ch->church->name);
	gecho(buf);
    }
    else
    {
	send_to_char("{RWarning: {xYour church will be a player killing church!\n\r",
		ch);
	send_to_char("{YAre you sure you want to do this?{x\n\r", ch);
	ch->pk_question = true;
    }
}

// @@@REMOVEME: it is redundant... just reference ch->church directly
CHURCH_DATA *find_char_church(CHAR_DATA * ch)
{
    CHURCH_DATA *chr;
    chr = NULL;

    if (ch->church == NULL)
	return NULL;

    for (chr = church_first; chr != NULL; chr = chr->next)
    {
	send_to_char(ch->church_name, ch);

	if (!str_cmp(ch->church_name, chr->name))
	    return chr;
    }

    return NULL;
}


int find_char_position_in_church(CHAR_DATA * ch)
{
    if (ch->church != NULL && ch->church_member != NULL)
	return ch->church_member->rank;
    else
	return -1;
}


/* return the structure given a # from chlist */
CHURCH_DATA *find_church(int number)
{
	CHURCH_DATA *church;
	ITERATOR it;

	iterator_start(&it, list_churches);

	while( (church = (CHURCH_DATA *)iterator_nextdata(&it)) && --number > 0);

	iterator_stop(&it);

	return church;
}

CHURCH_DATA *find_church_name(char *name)
{
	CHURCH_DATA *church;
	ITERATOR it;

	iterator_start(&it, list_churches);

	while( (church = (CHURCH_DATA *)iterator_nextdata(&it)) ) {
		if( !str_cmp( church->name, name ) )
			break;
	}

	iterator_stop(&it);

	return church;
}

/* is room players treasure church ? */
bool is_treasure_room(CHURCH_DATA *church, ROOM_INDEX_DATA *room)
{
    CHURCH_TREASURE_ROOM *treasure;
    ITERATOR it;


    if (!church) {
		ITERATOR cit;

		iterator_start(&cit, list_churches);
		while(( church = (CHURCH_DATA *)iterator_nextdata(&cit)))
			if( is_treasure_room(church, room) )
				break;
		iterator_stop(&cit);

		return church && true;
	}

	iterator_start(&it, church->treasure_rooms);
	while( (treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it)) ) {
		if( treasure->room == room )
			break;
	}
	iterator_stop(&it);

    return treasure && true;
}


/* lets leaders set their own churchtalk colours */
void do_chcolour(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char arg2[MSL];
    char letter;
    char letter2;

    argument = one_argument_norm(argument, arg);
    argument = one_argument_norm(argument, arg2);

    if (ch->church == NULL)
    {
	send_to_char("You aren't in a church.\n\r", ch);
	return;
    }

    if (arg[0] == '\0' || arg2[0] == '\0')
    {
	send_to_char(
	    "Syntax: church colour <colour1> <colour2>\n\r"
	    "Ex.: church colour Y B\n\r", ch);
	return;
    }

    letter = arg[0];
    letter2 = arg2[0];
    if (letter != 'x' && letter != 'X' && letter != 'R'
    && letter != 'B' && letter != 'r' && letter != 'b'
    && letter != 'W' && letter != 'w' && letter != 'C'
    && letter != 'M' && letter != 'c' && letter != 'm'
    && letter != 'Y' && letter != 'g' && letter != 'G'
    && letter != 'y' && letter != 'D')
    {
	send_to_char("Invalid colour code. To see available codes, type 'help colour'.\n\r", ch);
	return;
    }

    if (letter2 != 'x' && letter2 != 'X' && letter2 != 'R'
    && letter2 != 'B' && letter2 != 'r' && letter2 != 'b'
    && letter2 != 'W' && letter2 != 'w' && letter2 != 'C'
    && letter2 != 'M' && letter2 != 'c' && letter2 != 'm'
    && letter2 != 'Y' && letter2 != 'g' && letter2 != 'G'
    && letter2 != 'y' && letter2 != 'D')
    {
	send_to_char("Invalid colour code. To see available codes, type 'help colour'.\n\r", ch);
	return;
    }

    ch->church->colour1 = letter;
    ch->church->colour2 = letter2;
    send_to_char("Church colours set.\n\r", ch);
}


void do_chtrust(CHAR_DATA *ch, char *argument)
{
    CHURCH_DATA *church;
    CHURCH_PLAYER_DATA *church_player;
    STRING_DATA *string;
    STRING_DATA *string_prev;
    char arg[MSL];
    char arg2[MSL];
    char buf[MSL];
    int i;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0')
    {
	send_to_char("Syntax: church trust <person>\n\r"
	             "        church trust <person> <command>\n\r", ch);
	return;
    }

    if ((church = ch->church) == NULL)
    {
	send_to_char("You're not even in a church!\n\r", ch);
	return;
    }

    if (str_cmp(ch->name, church->founder))
    {
	send_to_char("Only the founder of the church can entrust people with commands.\n\r", ch);
	return;
    }

    if (arg[0] == '\0')
    {
	send_to_char("Syntax: church trust <member> <command>\n\r", ch);
	send_to_char("        church trust <member>\n\r", ch);
	return;
    }

    church_player = NULL;
    for (church_player = church->people; church_player != NULL;
          church_player = church_player->next)
    {
	if (!str_cmp(church_player->name, arg))
	    break;
    }

    if (church_player == NULL)
    {
	send_to_char("There is no such member in your church.\n\r", ch);
	return;
    }

    if (!str_cmp(arg2, "trust"))
    {
	send_to_char("Sorry, you can't do that.\n\r", ch);
	return;
    }

    /* no args, show list */
    if (arg2[0] == '\0')
    {
	sprintf(buf, "{Y%s is entrusted with the following commands:{x\n\r",
	    church_player->name);
	send_to_char(buf, ch);
	line(ch, 50);

	i = 0;
	for (string = church_player->commands; string != NULL; string = string->next)
	{
	    sprintf(buf, "{Y%i): {x%s\n\r", i + 1, string->string);
	    send_to_char(buf, ch);

	    i++;
	}

	if (i == 0)
	    send_to_char("No commands.\n\r", ch);

	line(ch, 50);

	return;
    }

    if (church_player->rank >= CHURCH_RANK_D)
    {
	act("That would be pointless. $t is a leader and already has all commands.", ch, NULL, NULL, NULL, NULL, church_player->name, NULL, TO_CHAR);
	return;
    }

    for (i = 0; church_command_table[i].command != NULL; i++)
    {
	if (!str_cmp(church_command_table[i].command, arg2))
	    break;
    }

    if (church_command_table[i].command == NULL
    || church_command_table[i].rank > 3)
    {
	send_to_char("There is no such command.\n\r", ch);
	return;
    }

    if (church_command_table[i].rank < 1
    && str_cmp(arg2, "rules")
    && str_cmp(arg2, "motd"))
    {
	send_to_char("That would be redundant. All members can use that command.\n\r", ch);
	return;
    }

    string_prev = NULL;
    for (string = church_player->commands; string != NULL; string = string->next)
    {
	if (!str_cmp(arg2, string->string))
	    break;

	string_prev = string;
    }

    /* remove a command from the list */
    if (string != NULL)
    {
	act("$t is no longer entrusted with the '$T' command.",
	    ch, NULL, NULL, NULL, NULL, church_player->name, string->string, TO_CHAR);
	if (string_prev != NULL)
	    string_prev->next = string->next;
	else
	    church_player->commands = string->next;

	free_string_data(string);
	return;
    }
    else
    {
	string = new_string_data();
	string->string = str_dup(arg2);
	act("$t is now entrusted with the '$T' command.", ch, NULL, NULL, NULL, NULL, church_player->name, string->string, TO_CHAR);
	string->next = church_player->commands;
	church_player->commands = string;
    }
}


bool is_trusted(CHURCH_PLAYER_DATA *member, char *command)
{
    STRING_DATA *string;

    if (member == NULL)
    {
	bug("is_trusted: null member!", 0);
	return false;
    }

    for (string = member->commands; string != NULL; string = string->next)
    {
	if (!str_cmp(string->string, command))
	    return true;
    }

    return false;
}


/* add something to the church's log */
void append_church_log(CHURCH_DATA *church, char *string)
{
    char buf[MSL];
    char buf2[MSL];
    char *time;

    if (church == NULL)
    {
		bug("append_church_log: null church.", 1);
		return;
    }

    sprintf(buf, "append_church_log: appended %s to log of %s",
        string, church->name);
    log_string(buf);

    if (church->log != NULL)
		sprintf(buf, "%s", church->log);
    else
		sprintf(buf, "{x");

    time = time_for_log();

    sprintf(buf2, "{%c[{%c%s{%c]{x %s\n\r",
        church->colour1,
	church->colour2,
	time,
	church->colour1,
        string);

    strcat(buf, buf2);

    free_string(time);

    free_string(church->log);
    church->log = str_dup(buf);
}


char *time_for_log(void)
{
    char buf[MSL];
    char buf2[MSL];
    int i;

    sprintf(buf, "%s", ctime(&current_time));
    i = 0;
    while (buf[i] != '\n')
    {
	buf2[i] = buf[i];
	i++;
    }

    buf2[i] = '\0';

    return str_dup(buf2);
}


void show_church_info(CHURCH_DATA *church, CHAR_DATA *ch)
{
    BUFFER *buffer;
    CHURCH_PLAYER_DATA *member;
    char buf[2*MSL];
    char buf2[MSL];
    int i;

    buffer = new_buf();

    sprintf(buf, "{BChurch Info:{x\n\r");
    add_buf(buffer, buf);

    sprintf(buf, "{YName:{x %s\n\r", church->name);
    add_buf(buffer, buf);

    sprintf(buf, "{YFlag:{x %s\n\r", church->flag);
    add_buf(buffer, buf);

    sprintf(buf, "{YFounder:{x %s\n\r", church->founder);
    add_buf(buffer, buf);

    sprintf(buf, "{YPneuma:{x %ld\n\r", church->pneuma);
    add_buf(buffer, buf);

    sprintf(buf, "{YGold:{x %ld\n\r", church->gold);
    add_buf(buffer, buf);

    sprintf(buf, "{YKarma:{x %ld\n\r", church->dp);
    add_buf(buffer, buf);

    sprintf(buf, "{YMax positions:{x %d\n\r", church->max_positions);
    add_buf(buffer, buf);

    switch(church->size)
    {
	case CHURCH_SIZE_BAND:
	    strcpy(buf2, "Band");
	    break;
	case CHURCH_SIZE_CULT:
	    strcpy(buf2, "Cult");
	    break;
	case CHURCH_SIZE_ORDER:
	    strcpy(buf2, "Order");
	    break;
	case CHURCH_SIZE_CHURCH:
	    strcpy(buf2, "Church");
	    break;
	default:
	    strcpy(buf2, "Unknown");
	    break;
    }

    sprintf(buf, "{YSize:{x %s\n\r", buf2);
    add_buf(buffer, buf);

    if (church->alignment == CHURCH_GOOD)
	sprintf(buf2, "Good");
    else if (church->alignment == CHURCH_EVIL)
	sprintf(buf2, "Evil");
    else
	sprintf(buf2, "Neutral");

    sprintf(buf, "{YAlignment:{x %s\n\r", buf2);
    add_buf(buffer, buf);

    sprintf(buf, "{YRecall Point:{x %ld - %s\n\r",
        church->recall_point.id[0],
	get_room_index(church->recall_point.id[0]) == NULL ?
	    "none" : get_room_index(church->recall_point.id[0])->name);
    add_buf(buffer, buf);

    sprintf(buf, "{YKey:{x %ld - %s\n\r",
        church->key,
	get_obj_index(church->key) == NULL ?
	    "none" : get_obj_index(church->key)->short_descr);
    add_buf(buffer, buf);

	sprintf(buf, "{YTreasure Room(s):{x\n\r");
	add_buf(buffer, buf);

    CHURCH_TREASURE_ROOM *treasure;
    ITERATOR it;
	iterator_start(&it, church->treasure_rooms);
	while( (treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it)) ) {
		if( treasure->room != NULL )
		{
			ROOM_INDEX_DATA *room = treasure->room;
			sprintf(buf, "{x\t\t%ld - %s{x\n\r", room->vnum, room->name);
			add_buf(buffer,buf);
		}
	}
	iterator_stop(&it);
	

    sprintf(buf, "{YPK record:{x %ld wins, %ld losses\n\r",
    	church->pk_wins, church->pk_losses);
    add_buf(buffer, buf);

    sprintf(buf, "{YCPK record:{x %ld wins, %ld losses\n\r",
    	church->cpk_wins, church->cpk_losses);
    add_buf(buffer, buf);

    sprintf(buf, "{YWars Won:{x %ld\n\r", church->wars_won);
    add_buf(buffer, buf);

    sprintf(buf, "{YPK:{x %s\n\r", church->pk ? "yes" : "no");
    add_buf(buffer, buf);

    sprintf(buf, "{YColours:{x {%cColour One{x and {%cColour Two{x\n\r",
        church->colour1, church->colour2);
    add_buf(buffer, buf);

    sprintf(buf, "{BMember Info:{x\n\r");
    add_buf(buffer, buf);

    sprintf(buf,
        "{Y%-12s %-10s %-10s %-10s %-8s %-8s %-4s{x\n\r{x",
	"Name", "Pneuma", "Karma", "Gold", "PK", "CPK", "Wars");
    add_buf(buffer, buf);

    i = 0;
    for (member = church->people; member != NULL; member = member->next)
    {
        i++;

	sprintf(buf,
	    "%-12s %-10ld %-10ld %-8ld",
	    member->name,
	    member->dep_pneuma,
	    member->dep_dp,
	    member->dep_gold);

	sprintf(buf2, "%4ld-%-4ld", member->pk_wins, member->pk_losses);
	strcat(buf, buf2);

	sprintf(buf2, "%4ld-%-4ld", member->cpk_wins, member->cpk_losses);
	strcat(buf, buf2);

	sprintf(buf2, "%4ld\n\r", member->wars_won);
	strcat(buf, buf2);

        add_buf(buffer, buf);
    }

    page_to_char(buf_string(buffer), ch);

    free_buf(buffer);
}


void do_churchset(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    CHURCH_DATA *church;
    long value;

    if ((church = ch->church) == NULL)
    {
	send_to_char("You aren't even in a church.\n\r", ch);
	return;
    }

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Syntax: church set <field>\n\r"
	             "For fields see \"help church\"", ch);
	return;
    }

    /* find value to toggle */
    if ((value = flag_value(church_flags, arg)) == NO_FLAG)
    {
	send_to_char("There is no such setting. See \"help church\" for available settings.\n\r", ch);
	return;
    }

    if (IS_SET(church->settings, value))
    {
	REMOVE_BIT(church->settings, value);
	send_to_char("Setting toggled OFF.\n\r", ch);
    }
    else
    {
	SET_BIT(church->settings, value);
	send_to_char("Setting toggled ON.\n\r", ch);
    }
}


void do_chconvert(CHAR_DATA *ch, char *argument)
{
    CHURCH_DATA *church;
    char arg[MSL];
    char buf[MSL];
    int align = -1;
    long pneuma_cost;
    long dp_cost;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0'
    || (str_cmp(arg, "good") && str_cmp(arg, "evil")
        && str_cmp(arg, "neutral")))
    {
	send_to_char("Syntax: church convert <good|neutral|evil>\n\r", ch);
	return;
    }

    if (!str_cmp(arg, "good"))
	align = CHURCH_GOOD;
    else if (!str_cmp(arg, "neutral"))
	align = CHURCH_NEUTRAL;
    else
	align = CHURCH_EVIL;

    if ((church = ch->church) == NULL)
    {
	send_to_char("You aren't even in a church.\n\r", ch);
	return;
    }

    if (str_cmp(ch->name, church->founder))
    {
	send_to_char("Only the founder of a church can change its faith.\n\r", ch);
	return;
    }

    if (align == church->alignment)
    {
	send_to_char("That would be pointless. Your church already follows that alignment.\n\r", ch);
	return;
    }

    if ((ch->alignment < 0 && align == CHURCH_GOOD)
    ||   (ch->alignment > 0 && align == CHURCH_EVIL))
    {
	send_to_char("You cannot convert to that alignment as you, the founder, cannot follow that faith.\n\r", ch);
	return;
    }

    if (church->alignment != CHURCH_NEUTRAL)
    {
	send_to_char("Only neutral churches can change their faith.\n\r", ch);
	return;
    }

    pneuma_cost = 10000;
    dp_cost = 2500000;
    if (church->pneuma < pneuma_cost || church->dp < dp_cost)
    {
	sprintf(buf, "It costs %ld pneuma and %ld karma to convert your alignment. You don't have enough.\n\r", pneuma_cost, dp_cost);
	send_to_char(buf, ch);
	return;
    }

    ch->pcdata->convert_church = align;
    sprintf(buf, "Are you SURE you want to convert to the faith of %s? (y/n)\n\r"
                  "{R***WARNING***:{x all members who cannot follow that faith will be removed on their next login!!!\n\r",
       ch->pcdata->convert_church == CHURCH_GOOD ? "the Pious" :
	  ch->pcdata->convert_church == CHURCH_NEUTRAL ? "Neutrality" :
	  "Malice");
    send_to_char(buf, ch);
}


void do_chdonate(CHAR_DATA *ch, char *argument)
{
	// church donate <obj>[ <room no>]
    OBJ_DATA *obj;
    char arg[MIL];

    if (ch->church == NULL)
    {
        send_to_char("You aren't in a church!\n\r", ch);
		return;
    }

    if(is_excommunicated(ch))
    {
        send_to_char("You have been excommunicated.\n\r", ch);
		return;
	}

	int avail = church_available_treasure_rooms(ch);

    if (avail < 1)
    {
    	send_to_char("You do not have access to a treasure room.\n\r", ch);
		return;
    }

	argument = one_argument(argument, arg);

    if ((obj = get_obj_carry(ch, arg, ch)) == NULL)
    {
		send_to_char("You don't have that object.\n\r", ch);
		return;
    }

    int roomno = 1;
    if( !IS_NULLSTR(argument) )
    {
		if(!is_number(argument))
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		roomno = atoi(argument);

		if( roomno < 1 || roomno > avail)
		{
			send_to_char("That is not a valid room number.\n\rPlease review the list in {WCHURCH TREASURE LIST{x.\n\r", ch);
			return;
		}
	}


    CHURCH_TREASURE_ROOM *treasure = get_church_treasure_room(ch, ch->church, roomno);
    if( !treasure || !treasure->room )
    {
		send_to_char("Something went wrong.  Could not find the treasure room.\n\r", ch);
		return;
	}

	// Only check rooms after the default room
	if( roomno > 1 && ch->church_member->rank < treasure->min_rank )
	{
		send_to_char("You do not have permission to use that room.\n\r", ch);
		return;
	}

    if (count_items_list_nest(treasure->room->contents) > MAX_CHURCH_TREASURE)
    {
    	send_to_char("That church temple treasure room is quite full already.\n\rPlease try another room.\n\r", ch);
		return;
    }

    if (obj->timer > 0 || IS_SET(obj->extra[1], ITEM_NO_DONATE))
    {
		act("You cannot donate $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
    }

    if (!can_drop_obj(ch, obj, true) || IS_SET(obj->extra[1], ITEM_KEPT))
    {
		send_to_char("It's stuck to you.\n\r", ch);
		return;
    }

    act("You toss $p into the air and it disappears into a swirling vortex.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
    act("$n tosses $p into the air and it disappears into a swirling vortex.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
    obj_from_char(obj);
    obj_to_room(obj, treasure->room);

/*
#if 1


    send_to_char("Donation is disabled for the time being.\n\r", ch);
#else
    OBJ_DATA *obj;
    ROOM_INDEX_DATA *room;

    if (ch->church == NULL)
    {
        send_to_char("You aren't in a church!\n\r", ch);
	return;
    }

    if (!list_size(ch->church->treasure_rooms))
    {
    	send_to_char("Your church doesn't have a treasure room.\n\r", ch);
		return;
    }

    if ((obj = get_obj_carry(ch, argument, ch)) == NULL)
    {
	send_to_char("You don't have that object.\n\r", ch);
	return;
    }

    if (count_items_list_nest(room->contents) > MAX_CHURCH_TREASURE)
    {
    	send_to_char("Your church temple treasure room is quite full already.\n\r", ch);
	return;
    }

    if (obj->timer > 0 || IS_SET(obj->extra[1], ITEM_NO_DONATE))
    {
    	act("You cannot donate $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    if (!can_drop_obj(ch, obj, true) || IS_SET(obj->extra[1], ITEM_KEPT))
    {
    	send_to_char("It's stuck to you.\n\r", ch);
	return;
    }

    act("You toss $p into the air and it disappears into a swirling vortex.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
    act("$n tosses $p into the air and it disappears into a swirling vortex.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
    obj_from_char(obj);
    obj_to_room(obj, room);
#endif
*/
}


void write_churches_new()
{
    CHURCH_DATA *church;
    FILE *fp;

    log_string("Writing churches...");
    if ((fp = fopen(CHURCHES_FILE, "w")) == NULL) {
    	bug("write_churches: couldn't open churches.dat", 0);
	return;
    }

    for (church = church_list; church != NULL; church = church->next) {
        write_church(church, fp);
	fprintf(fp, "\n");
    }

    fprintf(fp, "#ENDFILE\n");

    fclose(fp);
}


void write_church(CHURCH_DATA *church, FILE *fp)
{
    char buf[MSL];
    ITERATOR it;
    ROOM_INDEX_DATA *room;

    fprintf(fp, "#CHURCH %s~\n", church->name);
    fprintf(fp, "UID %ld\n", church->uid);
    fprintf(fp, "Alignment %d\n", church->alignment);
    fprintf(fp, "CPKLosses %ld\n", church->cpk_losses);
    fprintf(fp, "CPKWins %ld\n", church->cpk_wins);
    fprintf(fp, "Colour1 %c\n", church->colour1);
    fprintf(fp, "Colour2 %c\n", church->colour2);
    fprintf(fp, "DeityPoints %ld\n", church->dp);
    fprintf(fp, "Flag %s~\n", fix_string(church->flag));
    fprintf(fp, "Founder %s~\n", church->founder);
    fprintf(fp, "LastLoginFounder %ld\n", (long int)church->founder_last_login);
    fprintf(fp, "Gold %ld\n", church->gold);
    fprintf(fp, "Key %ld\n", church->key);
    fprintf(fp, "MaxPositions %d\n", church->max_positions);
    fprintf(fp, "PKLosses %ld\n", church->pk_losses);
    fprintf(fp, "PKWins %ld\n", church->pk_wins);
    fprintf(fp, "Pneuma %ld\n", church->pneuma);
    fprintf(fp, "RecallPoint %ld\n", church->recall_point.id[0]);
    fprintf(fp, "Settings %s\n", fwrite_flag(church->settings, buf));
    fprintf(fp, "Size %d\n", church->size);
    fprintf(fp, "ToggledPK %d\n", church->pk);
    fprintf(fp, "WarsWon %ld\n", church->wars_won);

    fprintf(fp, "Motd %s~\n", fix_string(church->motd));
    fprintf(fp, "Rules %s~\n", fix_string(church->rules));
    if (church->info != NULL)
		fprintf(fp, "Info %s~\n", fix_string(church->info));

	iterator_start(&it, church->treasure_rooms);
	CHURCH_TREASURE_ROOM *treasure;
	while(( treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it))) {
		room = treasure->room;
		if(room->wilds)
			fprintf(fp, "TreasureVRoomMR %ld %ld %ld %ld %d\n", room->wilds->uid, room->x, room->y, room->z, treasure->min_rank);
		else
			fprintf(fp, "TreasureRoomMR %ld %d\n", room->vnum, treasure->min_rank);
	}
	iterator_stop(&it);

    if (church->people != NULL)
		write_church_member(church->people, fp);

    fprintf(fp, "#-CHURCH\n");
}


/* Recursively writes a church's members (to preserve order of lists).*/
void write_church_member(CHURCH_PLAYER_DATA *member, FILE *fp)
{
    if (member->next != NULL)
        write_church_member(member->next, fp);

    fprintf(fp, "\n#MEMBER %s~\n", member->name);
    fprintf(fp, "Alignment %d\n", member->alignment);
    fprintf(fp, "CPKLosses %ld\n", member->cpk_losses);
    fprintf(fp, "CPKWins %ld\n", member->cpk_wins);
    fprintf(fp, "DepositedDp %ld\n", member->dep_dp);
    fprintf(fp, "DepositedGold %ld\n", member->dep_gold);
    fprintf(fp, "DepositedPneuma %ld\n", member->dep_pneuma);
    fprintf(fp, "Flags %ld\n", member->flags);
    fprintf(fp, "PKLosses %ld\n", member->pk_losses);
    fprintf(fp, "PKWins %ld\n", member->pk_wins);
    fprintf(fp, "Rank %d\n", member->rank);
    fprintf(fp, "Sex %d\n", member->sex);
    fprintf(fp, "WarsWon %ld\n", member->wars_won);
    fprintf(fp, "#-MEMBER\n");
}


/* Add a church to the END of a list.*/
void add_church_to_list(CHURCH_DATA *church, CHURCH_DATA *list)
{
    CHURCH_DATA *tmp;

    church->next = NULL;

    for (tmp = list; tmp->next != NULL; tmp = tmp->next)
	;

    tmp->next = church;
}


void read_churches_new()
{
    FILE *fp;
    char *word;
    CHURCH_DATA *church;

    log_string("Reading churches...");
    if ((fp = fopen(CHURCHES_FILE, "r")) == NULL) {
        bug("read_churches: can't open churches.dat", 0);
	return;
    }

    while(true) {
        word = fread_word(fp);
	if (!str_cmp(word, "#ENDFILE"))
	    break;

	church = read_church(fp);
	if (church_list == NULL)
	    church_list = church;
	else
	    add_church_to_list(church, church_list);
		if( !list_appendlink(list_churches, church) ) {
			bug("Failed to load churches due to memory issue with 'list_appendlink'", 0);
			abort();
		}
    }


    fclose(fp);
}


CHURCH_DATA *read_church(FILE *fp)
{
    char *word;
    char buf[MSL];
    CHURCH_DATA *church;
    CHURCH_PLAYER_DATA *member;
    bool fMatch;

    church = new_church();
    church->name = fread_string(fp);

    for (; ;) {
        word = fread_word(fp);
	fMatch = false;

        if (!str_cmp(word, "#-CHURCH"))
	    break;

	switch (word[0]) {
	    case '#':
	        if (!str_cmp(word, "#MEMBER")) {
		    member = read_church_member(fp);

		    if (!player_exists(member->name)) {
			sprintf(buf, "read_church: had church member data %s but no pfile, deleting", member->name);
			log_string(buf);
			free_church_player(member);
		    } else {
			member->next = church->people;
			church->people = member;
			member->church = church;

				if( !list_appendlink(church->roster, member->name) ) {
					bug("Failed to load church member information due to memory issues with 'list_appendlink'.", 0);
					abort();
				}
		    }
		}

		break;

	    case 'A':
		KEY("Alignment",	church->alignment,	fread_number(fp));
		break;

	    case 'C':
	        KEY("CPKLosses",	church->cpk_losses,		fread_number(fp));
		KEY("CPKWins",		church->cpk_wins,		fread_number(fp));
		KEY("Colour1",		church->colour1,			fread_letter(fp));
		KEY("Colour2",		church->colour2,			fread_letter(fp));
		break;

	    case 'D':
	        KEY("DeityPoints",	church->dp,			fread_number(fp));
		break;

	    case 'F':
		KEYS("Flag",		church->flag,			fread_string(fp));
		KEYS("Founder",		church->founder,		fread_string(fp));
		break;

	    case 'G':
	        KEY("Gold",		church->gold,			fread_number(fp));
		break;

            case 'I':
	        KEYS("Info",		church->info,			fread_string(fp));

	    case 'K':
	        KEY("Key",		church->key,			fread_number(fp));
		break;

            case 'L':
	        KEY("LastLoginFounder", church->founder_last_login,	fread_number(fp));
		break;

            case 'M':
	        KEY("MaxPositions",	church->max_positions,		fread_number(fp));
		KEYS("Motd",		church->motd,			fread_string(fp));
		break;

            case 'P':
	        KEY("PKLosses",		church->pk_losses,		fread_number(fp));
		KEY("PKWins",		church->pk_wins,		fread_number(fp));
		KEY("Pneuma",		church->pneuma,			fread_number(fp));
		break;

	    case 'R':
	        KEY("RecallPoint",	church->recall_point.id[0],		fread_number(fp));
		KEYS("Rules",		church->rules,			fread_string(fp));
		break;

	    case 'S':
	        KEY("Settings",		church->settings,		fread_flag(fp));
		KEY("Size",		church->size,			fread_number(fp));
		break;

	    case 'T':
	        KEY("ToggledPK",	church->pk,			fread_number(fp));
			if( !str_cmp(word, "TreasureRoom") ) {
				ROOM_INDEX_DATA *room = get_room_index(fread_number(fp));

				if( room ) {
					if( !church_add_treasure_room(church, room, CHURCH_RANK_A) ) {
						bug("Failed to add church treasure room due to memory issues with 'list_appendlink'.", 0);
						abort();
					}
				}

				fMatch = true;
				break;
			}
			if( !str_cmp(word, "TreasureRoomMR") ) {
				ROOM_INDEX_DATA *room = get_room_index(fread_number(fp));
				int min_rank = fread_number(fp);
				if( min_rank < CHURCH_RANK_A ) min_rank = CHURCH_RANK_A;
				if( min_rank > CHURCH_RANK_D ) min_rank = CHURCH_RANK_D;

				if( room ) {
					if( !church_add_treasure_room(church, room, min_rank) ) {
						bug("Failed to add church treasure room due to memory issues with 'list_appendlink'.", 0);
						abort();
					}
				}

				fMatch = true;
				break;
			}
			if( !str_cmp(word, "TreasureVRoom") ) {
				WILDS_DATA *wilds;
				ROOM_INDEX_DATA *room;
				int x;
				int y;
				//int z;

				wilds = get_wilds_from_uid(NULL,fread_number(fp));
				x = fread_number(fp);
				y = fread_number(fp);
				/*z = */(void)fread_number(fp);	// z isn't being used, but exists in the file?
				room = get_wilds_vroom(wilds, x, y);
				if(!room)
					room = create_wilds_vroom(wilds,x, y);

				if( room ) {
					if( !church_add_treasure_room(church, room, CHURCH_RANK_A) ) {
						bug("Failed to add church treasure room due to memory issues with 'list_appendlink'.", 0);
						abort();
					}
				}

				fMatch = true;
				break;
			}
			if( !str_cmp(word, "TreasureVRoomMR") ) {
				WILDS_DATA *wilds;
				ROOM_INDEX_DATA *room;
				int x;
				int y;
				//int z;

				wilds = get_wilds_from_uid(NULL,fread_number(fp));
				x = fread_number(fp);
				y = fread_number(fp);
				/*z = */(void)fread_number(fp);	// z isn't being used, but exists in the file?
				int min_rank = fread_number(fp);
				if( min_rank < CHURCH_RANK_A ) min_rank = CHURCH_RANK_A;
				if( min_rank > CHURCH_RANK_D ) min_rank = CHURCH_RANK_D;

				room = get_wilds_vroom(wilds, x, y);
				if(!room)
					room = create_wilds_vroom(wilds,x, y);

				if( room ) {
					if( !church_add_treasure_room(church, room, min_rank) ) {
						bug("Failed to add church treasure room due to memory issues with 'list_appendlink'.", 0);
						abort();
					}
				}

				fMatch = true;
				break;
			}
		break;

		case 'U':
			KEY("UID",		church->uid,			fread_number(fp));

	    case 'W':
		KEY("WarsWon",		church->wars_won,		fread_number(fp));
		break;

	    if (!fMatch) {
	        sprintf(buf, "read_churches: no match on word %s", word);
		bug(buf, 0);
	    }
	}
    }

	// Force their max positions to meet the minimum requirements, if lower
    int max_pos = church_get_min_positions(church->size);
    church->max_positions = UMAX(church->max_positions, max_pos);

    if (church->info == NULL)
		church->info = str_dup("No info set.");

	get_church_id(church);

	variable_dynamic_fix_church(church);

    return church;
}


CHURCH_PLAYER_DATA *read_church_member(FILE *fp)
{
    CHURCH_PLAYER_DATA *member;
    char *word;
    char buf[MSL];
    bool fMatch;

    member = new_church_player();
    member->name = fread_string(fp);

    for (; ;) {
        word = fread_word(fp);
	fMatch = false;

        if (!str_cmp(word, "#-MEMBER"))
	    break;

	switch (word[0]) {
	    case 'A':
		KEY("Alignment",	member->alignment,		fread_number(fp));
		break;

	    case 'C':
	        KEY("CPKLosses",	member->cpk_losses,		fread_number(fp));
		KEY("CPKWins",		member->cpk_wins,		fread_number(fp));
		break;

	    case 'D':
	        KEY("DepositedDp",	member->dep_dp,			fread_number(fp));
	        KEY("DepositedPneuma",	member->dep_pneuma,		fread_number(fp));
	        KEY("DepositedGold",	member->dep_gold,		fread_number(fp));
		break;

            case 'F':
	        KEY("Flags",		member->flags,			fread_number(fp));
		break;

            case 'P':
	        KEY("PKLosses",	member->pk_losses,		fread_number(fp));
		KEY("PKWins",		member->pk_wins,		fread_number(fp));
		break;

	    case 'R':
		KEY("Rank",		member->rank,			fread_number(fp));
		break;

	    case 'S':
	        KEY("Sex",		member->sex,			fread_number(fp));
		break;

	    case 'W':
		KEY("WarsWon",		member->wars_won,		fread_number(fp));
		break;

	    if (!fMatch) {
	        sprintf(buf, "read_churches: no match on word %s", word);
		bug(buf, 0);
	    }
	}
    }

    return member;
}


bool is_in_treasure_room(OBJ_DATA *obj)
{
    ROOM_INDEX_DATA *room = obj->in_room;

    if (room == NULL)
		return false;

	return is_treasure_room(NULL, room);
}

bool vnum_in_treasure_room(CHURCH_DATA *church, long vnum)
{
	CHURCH_TREASURE_ROOM *treasure;
	OBJ_DATA *obj = NULL;
	ITERATOR rit, oit;

	iterator_start(&rit, church->treasure_rooms);
	while( (treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&rit)) && !obj) {
		iterator_start(&oit, treasure->room->lcontents);
		while( (obj = (OBJ_DATA *)iterator_nextdata(&oit))) {
			if( obj->pIndexData->vnum == vnum )
				break;
		}
		iterator_stop(&oit);
	}
	iterator_stop(&rit);

    return obj && true;
}


void update_church_pks(void)
{
    CHURCH_DATA *church;
    int pk_wins;
    int pk_losses;

    log_string("update_church_pks: updating church PKs...");
    for (church = church_list; church != NULL; church = church->next) {
	pk_wins = church->cpk_wins;
	pk_losses = church->cpk_losses;

	church->cpk_wins = church->pk_wins;
	church->cpk_losses = church->pk_losses;
	church->pk_wins = pk_wins;
	church->pk_losses = pk_losses;
    }
}


bool is_excommunicated(CHAR_DATA *ch)
{
    if (ch->church == NULL || ch->church_member == NULL)
		return false;

    return IS_SET(ch->church_member->flags, CHURCH_PLAYER_EXCOMMUNICATED);
}

bool church_add_treasure_room(CHURCH_DATA *church, ROOM_INDEX_DATA *room, int min_rank)
{
	CHURCH_TREASURE_ROOM *treasure = alloc_mem(sizeof(CHURCH_TREASURE_ROOM));

	if( !treasure ) return false;

	treasure->room = room;
	treasure->min_rank = min_rank;

	if(!list_appendlink(church->treasure_rooms, treasure))
	{
		free_mem(treasure, sizeof(CHURCH_TREASURE_ROOM));
		return false;
	}

	return true;
}

void church_remove_treasure_room(CHURCH_DATA *church, ROOM_INDEX_DATA *room)
{
	CHURCH_TREASURE_ROOM *treasure;
	ITERATOR it;

	iterator_start(&it, church->treasure_rooms);
	while( (treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it))) {
		if( treasure->room == room)
			break;
	}
	iterator_stop(&it);

	if(treasure != NULL) {
		list_remlink(church->treasure_rooms, treasure);
		free_mem(treasure, sizeof(CHURCH_TREASURE_ROOM));
	}
}

CHURCH_TREASURE_ROOM *get_church_treasure_room(CHAR_DATA *ch, CHURCH_DATA *church, int nth)
{
	int min_rank = CHURCH_RANK_A;

	if( ch != NULL )
	{
		if( IS_NPC(ch) ) return NULL;
		if( ch->church != church ) return NULL;
		if( is_excommunicated(ch) ) return NULL;	// Excommunicated members cannot access treasure rooms

		min_rank = ch->church_member->rank;
	}

	if( nth < 1 ) return NULL;

	CHURCH_TREASURE_ROOM *treasure = NULL;
	ITERATOR it;

	iterator_start(&it, church->treasure_rooms);
	while( (treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it))) {
		if( min_rank >= treasure->min_rank && !--nth)
		{
			break;
		}
	}
	iterator_stop(&it);

	return treasure;

}


bool church_set_treasure_room_rank(CHURCH_DATA *church, int nth, int min_rank)
{
	if( nth < 1 ) return false;

	CHURCH_TREASURE_ROOM *treasure;
	ITERATOR it;

	iterator_start(&it, church->treasure_rooms);
	while( (treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it))) {
		if( !--nth)
		{
			treasure->min_rank = min_rank;
			return true;
		}
	}
	iterator_stop(&it);

	return false;
}

int church_available_treasure_rooms(CHAR_DATA *ch)
{
	if( ch == NULL ) return 0;
	if( IS_NPC(ch) ) return 0;
	if( ch->church == NULL ) return 0;
	if( is_excommunicated(ch) ) return 0;	// Excommunicated members cannot access treasure rooms

	CHURCH_TREASURE_ROOM *treasure = NULL;
	ITERATOR it;

	int count = 0;

	iterator_start(&it, ch->church->treasure_rooms);
	while( (treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it))) {
		if( ch->church_member->rank >= treasure->min_rank)
		{
			count++;
		}
	}
	iterator_stop(&it);

	return count;
}

void do_chtreasure(CHAR_DATA *ch, char *argument)
{
	static char *rank_names[MAX_CHURCH_RANK] = {
		"Rank 1",
		"Rank 2",
		"Rank 3",
		"Rank 4"
	};

	CHAR_DATA *temp_char;
    char buf[MAX_STRING_LENGTH];
    char arg[MIL];
    char arg2[MIL];
    bool found = false;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (ch->church == NULL)
    {
		send_to_char("You aren't in a registered group.\n\r", ch);
		return;
    }

    if (is_excommunicated(ch))
    {
		send_to_char("You have been excommunicated.  That information is forbidden.\n\r", ch);
		return;
	}

    if (arg[0] == '\0')
    {
		send_to_char("CHURCH TREASURE LIST\n\r", ch);
		if( ch->church_member->rank >= CHURCH_RANK_D )
			send_to_char("                RANK <room#> <rank>\n\r", ch);
		return;
    }

    if(!str_cmp(arg, "list"))
    {
		CHURCH_TREASURE_ROOM *treasure;
		ITERATOR it;

		bool skipped = false;

		int i = 0;
		iterator_start(&it, ch->church->treasure_rooms);
		while( (treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it)) )
		{
			if( ch->church_member->rank < treasure->min_rank )
			{
				skipped = true;
				continue;
			}

			if( i == 0 ) {
				sprintf(buf, "{YTreasure rooms for {W%s{Y:\n\r", ch->church->name);
				send_to_char(buf, ch);
				send_to_char("{Y========================================================{x\n\r", ch);
			}

			i++;
			if( i == 1 )
				sprintf(buf, "%2d [{WDEFAULT{x] {Y%s{x\n\r", i, treasure->room->name);
			else
				sprintf(buf, "%2d [{W%-7s{x] {Y%s{x\n\r", i, rank_names[treasure->min_rank], treasure->room->name);
			send_to_char(buf, ch);
		}
		iterator_stop(&it);

		if( i == 0 ) {
			if( skipped )
				send_to_char("There are no treasure rooms available to you.\n\r", ch);	// Should NEVER happen, but no telling in the future.
			else
				send_to_char("Your church has no treasure rooms yet.\n\r", ch);
		}

		return;
	}

	if(!str_cmp(arg, "rank"))
	{
		if( ch->church_member->rank < CHURCH_RANK_D )
		{
			send_to_char("CHURCH TREASURE LIST\n\r", ch);
			return;
		}

		found = false;
		for (temp_char = ch->in_room->people; temp_char != NULL; temp_char = temp_char->next_in_room)
		{
			if (IS_NPC(temp_char) && IS_SET(temp_char->act[1], ACT2_CHURCHMASTER))
				found = true;
		}

		if (!found)
		{
			send_to_char("You must be at an administration office.\n\r", ch);
			return;
		}


		if(!is_number(arg2) || !is_number(argument))
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		int roomno = atoi(arg2);

		if( roomno < 0 || roomno > list_size(ch->church->treasure_rooms))
		{
			send_to_char("Room number is out of range.\n\r", ch);
			return;
		}

		if( roomno == 1 )
		{
			send_to_char("The first treasure room cannot be restricted.\n\r", ch);
			return;
		}

		int rank = atoi(argument) - 1;

		if( rank < CHURCH_RANK_A || rank > CHURCH_RANK_D )
		{
			send_to_char("That is not a church rank.  Please specify a rank from 1 to 4.\n\r", ch);
			return;
		}

		CHURCH_TREASURE_ROOM *treasure = get_church_treasure_room(NULL, ch->church, roomno);

		if(!treasure)
		{
			send_to_char("Could not find the specified treasure room.\n\r", ch);
			return;
		}

		treasure->min_rank = rank;
		send_to_char("Rank changed.\n\r", ch);

		sprintf(buf, "%s changes minimum rank for treasure room %s to %s.", ch->name, treasure->room->name, rank_names[treasure->min_rank]);
		append_church_log(ch->church, buf);

		write_churches_new();
		return;
	}

	send_to_char("CHURCH TREASURE LIST\n\r", ch);
	if( ch->church_member->rank >= CHURCH_RANK_D )
		send_to_char("                RANK <room#> <rank>\n\r", ch);
	return;
}


void church_announce_theft(CHAR_DATA *ch, OBJ_DATA *obj)
{
    char buf[MAX_STRING_LENGTH];
	CHURCH_DATA *church;

	for (church = church_list; church != NULL; church = church->next) {
		if ((ch->church != church || is_excommunicated(ch)) && is_treasure_room(church, ch->in_room)) {
			if( obj != NULL)
			{
				sprintf(buf, "{Y%s has stolen %s from a %s treasure room!{x\n\r", ch->name, obj->short_descr, church->name);
			}
			else
			{
				sprintf(buf, "{Y%s has stolen from a %s treasure room!{x\n\r", ch->name, church->name);
			}
			gecho(buf);
		}
	}
}
