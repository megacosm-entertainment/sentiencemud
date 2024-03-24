/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include "merc.h"
#include "db.h"
#include "recycle.h"
#include <string.h>

/* local functions */
void do_chat(CHAR_DATA *ch, char *argument);
void do_chat_enter(CHAR_DATA *ch, char *argument);
void do_chat_exit(CHAR_DATA *ch, char *argument);
void do_chat_list(CHAR_DATA *ch, char *argument);
void do_chat_join(CHAR_DATA *ch, char *argument);
void do_chat_topic(CHAR_DATA *ch, char *argument);
void do_chat_delete(CHAR_DATA *ch, char *argument);
void do_chat_op(CHAR_DATA *ch, char *argument);
void do_chat_password(CHAR_DATA *ch, char *argument);
void do_chat_kick(CHAR_DATA *ch, char *argument);
void do_chat_ban(CHAR_DATA *ch, char *argument);
void do_chat_setfounder(CHAR_DATA *ch, char *argument);
void chat_create(CHAR_DATA *ch, char *argument, bool perm);
void chat_add_op(CHAR_DATA *ch, char *arg);
void chat_rem_op(CHAR_DATA *ch, char *arg);
void chat_add_ban(CHAR_DATA *ch, char *argument);
void chat_remove_ban(CHAT_ROOM_DATA *chat, CHAT_BAN_DATA *ban);


void do_chat(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg);

    if (!str_cmp(arg, "enter"))
    {
	do_function(ch, &do_chat_enter, argument);
	return;
    }

    if (!str_cmp(arg, "exit"))
    {
	do_function(ch, &do_chat_exit, argument);
	return;
    }

    if (!str_cmp(arg, "list"))
    {
	do_function(ch, &do_chat_list, argument);
	return;
    }

    if (!str_cmp(arg, "join"))
    {
	do_function(ch, &do_chat_join, argument);
	return;
    }

    if (!str_cmp(arg, "create"))
    {
	chat_create(ch, argument, false);
	return;
    }

    if (!str_cmp(arg, "permcreate"))
    {
	if (!IS_IMMORTAL(ch))
	{
	    send_to_char("To request a permanent chat room, "
		    "contact the Immortals.\n\r", ch);
	    return;
	}
	chat_create(ch, argument, true);
	return;
    }

    if (!str_cmp(arg, "topic"))
    {
	do_function(ch, &do_chat_topic, argument);
	return;
    }

    if (!str_cmp(arg, "delete"))
    {
	do_function(ch, &do_chat_delete, argument);
	return;
    }

    if (!str_cmp(arg, "op"))
    {
	do_function(ch, &do_chat_op, argument);
	return;
    }

    if (!str_cmp(arg, "kick"))
    {
	do_function(ch, &do_chat_kick, argument);
	return;
    }

    if (!str_cmp(arg, "password"))
    {
	do_function(ch, &do_chat_password, argument);
	return;
    }

    if (!str_cmp(arg, "ban"))
    {
	send_to_char("NOT IMPLEMENTED", ch);
	return;
    }

    if (!str_cmp(arg, "setfounder"))
    {
	if (ch->tot_level >= 151)
	{
	    do_function(ch, &do_chat_setfounder, argument);
	}
	else
	{
	    send_to_char("Valid commands are:\n\r"
		"ENTER EXIT LIST CREATE JOIN DELETE TOPIC OP\n\r"
		"KICK PASSWORD\n\r", ch);
	}

	return;
    }

    send_to_char("Valid commands are:\n\r"
	    "ENTER EXIT LIST CREATE JOIN DELETE TOPIC OP\n\r"
	    "KICK PASSWORD\n\r", ch);
}


void do_chat_enter(CHAR_DATA *ch, char *argument)
{
    char buf[MSL];
    TOKEN_DATA *token, *token_next;
    ROOM_INDEX_DATA *recall;

    if (IS_SOCIAL(ch))
    {
	send_to_char("You're already in chat.\n\r", ch);
	return;
    }

    if (ch->in_room == NULL)
    {
	char buf[MAX_STRING_LENGTH];

	sprintf(buf, "do_chat_enter: %s with null in_room!",
		ch->name);
	bug(buf, 0);
	return;
    }

    if (!str_prefix("Maze-Level", ch->in_room->area->name)) {
        send_to_char("You can't enter chat from here.\n\r", ch);
	return;
    }

    recall = location_to_room(&ch->in_room->area->region.recall);

    if (!IS_IMMORTAL(ch) && (!recall || ch->in_room != recall || !IS_SET(ch->in_room->room_flag[0], ROOM_SAFE)))
    {
	send_to_char("You can only enter chat from the recall point of the area you are in.{x\n\r", ch);
	return;
    }

    if (ch->fighting != NULL)
    {
	send_to_char("You must stop fighting first.{x\n\r", ch);
	return;
    }

    if (ch->no_recall > 0 && !IS_IMMORTAL(ch))
    {
	send_to_char("You can't gather enough energy for that.\n\r", ch);
	return;
    }

	location_from_room(&ch->before_social,ch->in_room);

    act("{WA ghostly spirit appears before $n and pulls $m to another dimension.{x",   ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    act("{WA ghostly spirit appears before you and pulls you to another dimension.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

    for (token = ch->tokens; token != NULL; token = token_next) {
	token_next = token->next;

	if (IS_SET(token->flags, TOKEN_PURGE_RIFT)) {
		p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_TOKEN_REMOVED, NULL,0,0,0,0,0);
	    sprintf(buf, "char update: token %s(%ld) char %s(%ld) was purged because of rift",
		    token->name, token->pIndexData->vnum, HANDLE(ch), IS_NPC(ch) ? ch->pIndexData->vnum :
		    0);
	    log_string(buf);
	    token_from_char(token);
	    free_token(token);
	}
    }

    // Reset manastore upon entering the rift
    ch->manastore = 0;

    char_from_room(ch);
    char_to_room(ch, room_index_chat);
    act("{W$n has entered chat.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

    SET_BIT(ch->comm, COMM_SOCIAL);
}


void do_chat_exit(CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *room;
    char buf[MAX_STRING_LENGTH];

    if (!IS_SOCIAL(ch))
    {
	send_to_char("You aren't in chat.\n\r", ch);
	return;
    }

    room = location_to_room(&ch->before_social);
    location_clear(&ch->before_social);

    if (!room) {
	sprintf(buf, "do_chat_exit: before_social room was null!");
	bug(buf, 0);

	room = room_index_temple;

	REMOVE_BIT(ch->comm, COMM_SOCIAL);

	char_from_room(ch);

	char_to_room(ch, room);
	return;
    }

    act("{W$n has left chat.{x",   ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    act("{WYou exit chat.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

    REMOVE_BIT(ch->comm, COMM_SOCIAL);

    char_from_room(ch);
    char_to_room(ch, room);

    act("{W$n fades in from another dimension.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
}


void do_chat_list(CHAR_DATA *ch, char *argument)
{
    CHAT_ROOM_DATA *chat;
    CHAT_OP_DATA *op;
    int i = 0;
    char buf[MAX_STRING_LENGTH];
    char buf2[MAX_STRING_LENGTH];

    if (!IS_SOCIAL(ch))
    {
	send_to_char("You must be in chat to do that.\n\r", ch);
	return;
    }

    sprintf(buf, "{Y%s %-32s %-7s %-10s %5s{x\n\r",
	    "Num",
	    "Name",
	    "People",
	    "Creator",
	    "Ops");
    send_to_char(buf, ch);

    line(ch, 70, NULL, NULL);

    i = 0;
    for (chat = chat_room_list; chat != NULL; chat = chat->next)
    {
	i++;

	for (op = chat->ops; op != NULL; op = op->next)
	{
	    sprintf(buf2, "%s ", op->name);
	}

	sprintf(buf, "{Y%2i){x %-31.27s {M%2d/%-5d{x %-12s %-12.12s{x\n\r",
		i,
		chat->name,
		chat->curr_people,
		chat->max_people,
		chat->created_by,
		buf2);
	send_to_char(buf, ch) ;
	sprintf(buf, "    Topic: %s{x\n\r", chat->topic);
	send_to_char(buf, ch);
    }

    if (i == 0)
	send_to_char("No chat rooms found.\n\r", ch);

    line(ch, 70, NULL, NULL);
}


void do_chat_join(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHAT_ROOM_DATA *chat;
    ROOM_INDEX_DATA *room;
    bool found = false;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (!IS_SOCIAL(ch))
    {
	send_to_char("You must be in chat to join a chat room.\n\r", ch);
	return;
    }

    if (arg[0] == '\0')
    {
	send_to_char("Join which chat room?\n\r", ch);
	return;
    }

    for (chat = chat_room_list; chat != NULL; chat = chat->next)
    {
	if (!str_cmp(chat->name, arg))
	{
	    found = true;
	    break;
	}
    }

    if (!found)
    {
	send_to_char("No such chat room found.\n\r", ch);
	return;
    }

    room = get_room_index(area_chat, chat->vnum);
    if (room == NULL)
    {
	sprintf(buf, "do_chat_join: %s, %s had null chat->vnum\n\r",
		ch->name,
		chat->name);
	bug(buf, 0);
	return;
    }

    if (room == ch->in_room)
    {
	send_to_char("You're already there.\n\r", ch);
	return;
    }

    if (str_cmp(chat->password, "none")
    && str_cmp(chat->password, arg2))
    {
	sprintf(buf, "#%s is password protected.\n\r"
		"Use /join <chatroom> <password>.\n\r",
		chat->name);
	send_to_char(buf, ch);
	return;
    }

    act("{Y$n leaves for another chat room.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    sprintf(buf, "{YYou join #%s.{x\n\r", chat->name);
    send_to_char(buf, ch);

    char_from_room(ch);
    char_to_room(ch, room);

    sprintf(buf, "$n has joined #%s.", chat->name);
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

    do_function(ch, &do_look, "auto");
}


void chat_create(CHAR_DATA *ch, char *argument, bool perm)
{
    CHAT_ROOM_DATA *chat;
    CHAT_OP_DATA *op;
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    char arg3[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    int num;
    int i = 0;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (!IS_SOCIAL(ch))
    {
	send_to_char("You must be in chat to create a chat room.\n\r", ch);
	return;
    }

    if (arg[0] == '\0')
    {
	sprintf(buf,
		"Syntax:\n\rchat create <name> <max_people> [password]\n\r");
	send_to_char(buf, ch);
	return;
    }

    if (arg2[0] == '\0')
    {
	send_to_char("Syntax:\n\r"
		"chat create <name> <max_people> [password]\n\r", ch);
	return;
    }

    for (chat = chat_room_list ; chat != NULL; chat = chat->next)
    {
	i++;

	if (!str_cmp(arg, chat->name))
	{
	    send_to_char("There is already a chat room with that name.\n\r", ch);
	    return;
	}
    }

    if (i > MAX_CHAT_ROOMS)
    {
	sprintf(buf, "Sorry, there are already %i chat rooms.\n\r", MAX_CHAT_ROOMS);
	send_to_char(buf, ch);
	return;
    }

    if (ch->in_room->chat_room != NULL)
    {
	send_to_char("There is already a chat room here.\n\r", ch);
	return;
    }

    if (!is_number(arg2))
    {
	send_to_char("Value must be numeric.\n\r", ch);
	return;
    }

    num = atoi(arg2);

    if (num < 1 || num > MAX_IN_CHAT_ROOM)
    {
	sprintf(buf, "Limit must be between 1 and %i people.\n\r",
		MAX_IN_CHAT_ROOM);
	send_to_char(buf, ch);
	return;
    }

    // Keep people from blocking off areas of social with pws
    if (arg3[0] != '\0' && count_exits(ch->in_room) > 1)
    {
	send_to_char("Sorry, you can't make a protected chat room in a room with more than one exit.\n\r", ch);
	return;
    }

    chat = new_chat_room();

    if (chat_room_list == NULL)
	chat_room_list = chat;
    else
    {
	CHAT_ROOM_DATA *temp_chat;
	temp_chat = chat_room_list;
	while (temp_chat->next != NULL)
	{
	    temp_chat = temp_chat->next;
	}
	temp_chat->next = chat;
    }

    chat->next = NULL;
    chat->topic = str_dup("<not set>");
    chat->name  = str_dup(arg);

    if (arg3[0] != '\0')
	chat->password = str_dup(arg3) ;
    else
	chat->password = str_dup ("none");
    chat->max_people = num;
    chat->curr_people = 1;
    if (perm == true)
	chat->permanent = true;
    else
	chat->permanent = false;
    chat->vnum = ch->in_room->vnum;
    chat->created_by = str_dup(ch->name);

    sprintf(buf, "You created {Y#%s{x in {Y%s{x, max of {Y%d{x people.{x\n\r",
	    chat->name,
	    ch->in_room->name,
	    chat->max_people);
    send_to_char(buf, ch);

    if (chat->password != NULL)
    {
	sprintf(buf, "Password is {Y\"%s\"{x.\n\r", chat->password);
	send_to_char(buf, ch);
    }

    /* link the new chatroom to the room and player */
    ch->in_room->chat_room = chat;

    /* give the guy ops */
    op = new_chat_op();

    op->name = str_dup(ch->name);
    op->chat_room = chat;

    chat->ops = op;
    chat->ops->next = NULL;

    do_function(ch, &do_look, "auto");

    write_chat_rooms();
}


void do_chat_topic(CHAR_DATA *ch, char *argument)
{
    CHAT_ROOM_DATA *chat;
    char buf[MAX_STRING_LENGTH];

    if (!IS_SOCIAL(ch))
    {
	send_to_char("You aren't even in chat.\n\r", ch);
	return;
    }

    chat = ch->in_room->chat_room;
    if (chat == NULL)
    {
	send_to_char("You aren't in a chat room.\n\r", ch);
	return;
    }

    if (!is_op(chat, ch->name))
    {
	send_to_char("Only ops may change the topic.\n\r", ch);
	return;
    }

    if (argument[0] == '\0')
    {
	send_to_char("Change topic to what?\n\r", ch);
	return;
    }

    if (strlen_no_colours(argument) > 150)
	{
        send_to_char("Chat topic must be under 150 characters.\n\r", ch);
        return;
    }

    smash_tilde(argument);

    if (chat->topic != NULL)
	free_string(chat->topic);

    chat->topic = str_dup(argument);

    sprintf(buf, "Topic changed to \"%s{x\".\n\r", argument);
    send_to_char(buf, ch);
    sprintf(buf, "$n has changed the topic to \"%s{x\".", argument);
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

    write_chat_rooms();
}


void do_chat_delete(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    CHAT_ROOM_DATA *chat;

    if (!IS_SOCIAL(ch))
    {
	send_to_char("You aren't even in chat.\n\r", ch);
	return;
    }

    if (ch->in_room->chat_room == NULL)
    {
	send_to_char("There is no chat room here.\n\r", ch);
	return;
    }

    if (ch->in_room->chat_room->permanent == true && !IS_IMPLEMENTOR(ch))
    {
	send_to_char(
	    "You may not delete a permanent chat room.\n\r"
	    "To have this chatroom deleted contact coder@megacosm.net\n\r", ch);
	return;
    }

    if (str_cmp(ch->name, ch->in_room->chat_room->created_by)
    && !IS_IMMORTAL(ch))
    {
	send_to_char("Only the creator of a chat room can delete it.\n\r", ch);
	return;
    }

    chat = ch->in_room->chat_room;

    sprintf(buf, "{Y$n has deleted #%s.{x", chat->name);
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

    /* dislink it from the room */
    ch->in_room->chat_room = NULL;

    sprintf(buf, "{YYou have deleted #%s.{x\n\r", chat->name);
    send_to_char(buf, ch);

    extract_chat_room(chat);
    write_chat_rooms();
}


void do_chat_op(CHAR_DATA *ch, char *argument)
{
    CHAT_OP_DATA *op;
    CHAT_ROOM_DATA *chat;
    BUFFER *buffer;
    char arg[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg);

    if (!IS_SOCIAL(ch))
    {
	send_to_char("You aren't in chat.\n\r", ch);
	return;
    }

    if (ch->in_room->chat_room == NULL)
    {
	send_to_char("You aren't in a chat room.\n\r", ch);
	return;
    }

    chat = ch->in_room->chat_room;

    /* show them a list of ops if no argument */
    if (arg[0] == '\0')
    {
	int i = 0;

	buffer = new_buf();

	add_buf(buffer, "{YCurrent operators:{x\n\r");
	add_buf(buffer, "{Y---------------------------------------------------------------{x\n\r");
	for (op = ch->in_room->chat_room->ops; op != NULL; op = op->next)
	{
	    i++;

	    if (op->name != NULL)
	    {
		sprintf(buf, "%-12s ", op->name);
		add_buf(buffer, buf);
	    }

	    if (i % 4 == 0 && op->next != NULL)
		add_buf(buffer, "\n\r");
	}

        if (i == 0)
	{
	    add_buf(buffer, "No operators found.");
	}

	add_buf(buffer, "\n\r");

	add_buf(buffer, "{Y---------------------------------------------------------------{x\n\r");

	page_to_char(buf_string(buffer), ch);
	free_buf(buffer);
	return;
    }

    if (!is_op(chat, ch->name) && !IS_IMMORTAL(ch))
    {
	send_to_char("Only ops may add and remove operators.\n\r", ch);
	return;
    }

    if (!player_exists(arg))
    {
	send_to_char("That player doesn't exist.\n\r", ch);
	return;
    }

    arg[0] = UPPER(arg[0]);

    if (is_op(chat, arg))
	chat_rem_op(ch, arg);
    else
	chat_add_op(ch, arg);

    write_chat_rooms();
}


bool is_op(CHAT_ROOM_DATA *chat, char *arg)
{
    CHAT_OP_DATA *op;
    char buf[MAX_STRING_LENGTH];

    if (chat == NULL)
    {
	sprintf(buf, "is_op: null chat_room");
	bug(buf, 0);
	return false;
    }

    if (chat->ops == NULL)
	return false;

    for (op = chat->ops; op != NULL; op = op->next)
    {
	if (op->name == NULL)
	    continue;

	if (!str_cmp(op->name, arg))
	    return true;
    }

    return false;
}


void chat_add_op(CHAR_DATA *ch, char *arg)
{
    CHAT_OP_DATA *op;
    CHAT_ROOM_DATA *chat;
    CHAR_DATA *vch;

    chat = ch->in_room->chat_room;

    act("{YYou add $T as an operator.{x", ch, NULL, NULL, NULL, NULL, NULL, arg, TO_CHAR);

    if ((vch = get_char_room(ch, NULL, arg)) != NULL)
    {
	act("{Y$n adds you as an operator.{x", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
    }

    for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room)
    {
	if (str_cmp(vch->name, arg))
	    act("{Y$n adds $t as an operator.{x", ch, vch, NULL, NULL, NULL, arg, NULL, TO_VICT);
    }

    op = new_chat_op();

    op->next = chat->ops;
    chat->ops = op;

    op->name = str_dup(arg);
    op->chat_room = chat;
}


void chat_rem_op(CHAR_DATA *ch, char *arg)
{
    CHAT_OP_DATA *op;
    CHAT_OP_DATA *prev_op;
    CHAR_DATA *vch;
    char buf[MAX_STRING_LENGTH];
    bool found = false;
    int op_count = 0;

    op = NULL;
    prev_op = NULL;

    /* see if they are an op in the room */
    for (op = ch->in_room->chat_room->ops; op != NULL; prev_op = op, op = op->next)
    {
	op_count++;

	if (!str_cmp(op->name, arg))
	{
	    found = true;
	    break;
	}
    }

    if (!found)
    {
	send_to_char("That person isn't an op here.\n\r", ch);
	return;
    }

    sprintf(buf, "{YYou remove %s as an operator.{x\n\r",
	    (!str_cmp(ch->name, arg)) ? "yourself" : op->name);
    send_to_char(buf, ch);

    vch = get_char_room(ch, NULL, arg);
    if (vch != NULL && ch != vch)
    {
	act("{Y$n removes you as an operator.{x", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
    }

    if (!str_cmp(ch->name, arg))
    {
	sprintf(buf, "{Y$n removes $mself as an operator.{x");
	act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    }
    else
    {
	for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room)
	{
	    if (str_cmp(vch->name, arg))
		act("{Y$n removes $t as an operator.{x", ch, vch, NULL, NULL, NULL, arg, NULL, TO_VICT);
	}
    }

    if (prev_op != NULL)
	prev_op->next = op->next;
    else
	op->chat_room->ops = op->next;

    free_chat_op(op);
}


void do_chat_kick(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *victim;
    ROOM_INDEX_DATA *to_room;
    char arg[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg);

    if (!IS_SOCIAL(ch))
    {
		send_to_char("You aren't even in chat.\n\r", ch);
		return;
    }

    if (ch->in_room->chat_room == NULL)
    {
		send_to_char("You aren't in a chat room.\n\r", ch);
		return;
    }

    if (!is_op(ch->in_room->chat_room, ch->name))
    {
		send_to_char("Only ops may kick people out.\n\r", ch);
		return;
    }

    if (arg[0] == '\0')
    {
		send_to_char("Kick whom?\n\r" , ch);
		return;
    }

    victim = get_char_room(ch, NULL, arg);
    if (victim == NULL)
    {
		send_to_char ("They aren't here.\n\r", ch);
		return;
    }

    to_room = room_index_chat;
    sprintf(buf, "{YYou kick %s out of #%s.{x",
	    ch == victim ? "yourself" : "$N",
	    ch->in_room->chat_room->name);
    act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

    if (ch != victim)
    {
		sprintf(buf, "{Y%s kicks you out of #%s.{x",
			ch->name, ch->in_room->chat_room->name);
		act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
    }

    sprintf(buf, "{Y%s kicks %s out of #%s.{x",
	    ch->name,
	    ch == victim ? "$mself" : victim->name,
	    ch->in_room->chat_room->name);
    act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);

    char_from_room(victim);
    char_to_room(victim, to_room);
}


void do_chat_ban(CHAR_DATA *ch, char *argument)
{
    CHAT_BAN_DATA *ban;
    CHAT_ROOM_DATA *chat;
    char arg[MAX_STRING_LENGTH];
    char buf[2*MAX_STRING_LENGTH];
    bool add = true;
    int n = 0;

    argument = one_argument_norm(argument, arg);

    if (!IS_SOCIAL(ch))
    {
	send_to_char("You aren't even in chat.\n\r", ch);
	return;
    }

    if (ch->in_room->chat_room == NULL)
    {
	send_to_char("You aren't in a chat room.\n\r", ch);
	return;
    }

    if (!is_op(ch->in_room->chat_room, arg))
    {
	send_to_char("You must be an op to ban somebody.\n\r", ch);
	return;
    }

    chat = ch->in_room->chat_room;

    if (arg[0] == '\0')
    {
	int i = 0;

	send_to_char("{YCurrent bans:{x\n\r"
		"{Y------------{x\n\r", ch);
	for (ban = ch->in_room->chat_room->bans; ban != NULL; ban = ban->next)
	{
	    i++;

	    if (ban->name != NULL)
	    {
		sprintf(buf, "%s ", ban->name);
		send_to_char(buf, ch);
	    }

	    // 4 names to a line
	    if (i % 4 == 0)
		send_to_char ("\n\r", ch);
	}

	if (i == 0)
	    send_to_char("No bans.", ch);

	send_to_char("\n\r", ch);
	return;
    }

    // count the bans too see if there are too many.
    for (ban = chat->bans; ban != NULL; ban = ban->next)
    {
	n++;

	if (n > 50)
	{
	    send_to_char("There are too many bans already!\n\r", ch);
	    return;
	}
    }

    // see if we are adding or removing
    for (ban = chat->bans; ban != NULL; ban = ban->next)
    {
	if (!str_cmp(ban->name, arg))
	    add = false;
	else
	    add = true;
    }

    if (strlen(arg) > 12)
	arg[12] = '\0';

    if (add)
    {
	chat_add_ban(ch, arg);
	sprintf(buf, "{YBanned \"%s\" from #%s.{x\n\r",
		arg, chat->name);
	send_to_char(buf, ch);
    }
    else
    {
	chat_remove_ban(chat, ban);
	sprintf(buf, "{YUnbanned \"%s\" from #%s.{x\n\r",
		ban->name, chat->name);
    }
}


void chat_add_ban(CHAR_DATA *ch, char *argument)
{
    CHAT_BAN_DATA *ban;
    CHAT_ROOM_DATA *chat;

    chat = ch->in_room->chat_room;

    ban = new_chat_ban();

    ban->next = chat->bans;
    chat->bans = ban;

    ban->chat_room = ch->in_room->chat_room;
    ban->name      = str_dup(argument);
    ban->banned_by = str_dup(ch->name);
}


void chat_remove_ban(CHAT_ROOM_DATA *chat, CHAT_BAN_DATA *ban)
{
    CHAT_BAN_DATA *prev_ban;

    prev_ban = NULL;
    for (ban = chat->bans;
	  ban != NULL;
	  prev_ban = ban, ban = ban->next)
    {

	if (prev_ban != NULL)
	    prev_ban->next = ban->next;
	else
	    ban->chat_room->bans = ban->next;

	free_chat_ban(ban);
    }
}


void do_chat_password(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char buf[2*MSL];

    if (!IS_SOCIAL(ch))
    {
	send_to_char("You aren't even in chat.\n\r", ch);
	return;
    }

    if (ch->in_room->chat_room == NULL)
    {
	send_to_char("You aren't in a chat room.\n\r", ch);
	return;
    }

    if (!is_op(ch->in_room->chat_room, ch->name))
    {
	send_to_char("Only ops can change the password.\n\r", ch);
	return;
    }

    argument = one_argument_norm(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Change the password to what?\n\r" , ch);
	return;
    }

    // Keep people from blocking off areas of social with pws
    if (count_exits(ch->in_room) > 1)
    {
	send_to_char("Sorry, you can't make a protected chat room in a room with more than one exit.\n\r", ch);
	return;
    }

    sprintf(buf, "{YYou have changed the password of #%s to '%s'.{x",
	ch->in_room->chat_room->name,
	arg);

    send_to_char(buf, ch);

    free_string(ch->in_room->chat_room->password);
    ch->in_room->chat_room->password = str_dup(arg);
}


void do_chat_setfounder(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char arg2[MSL];
    char buf[2*MSL];
    CHAT_ROOM_DATA *chat_room = NULL;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0')
    {
	send_to_char("Syntax: chat setfounder <#room> <founder>\n\r", ch);
	return;
    }

    for (chat_room = chat_room_list; chat_room != NULL;
          chat_room = chat_room->next)
    {
	if (!str_cmp(chat_room->name, arg))
	    break;
    }

    if (chat_room == NULL)
    {
	send_to_char("There is no such chatroom.\n\r", ch);
	return;
    }

    if (!player_exists(arg2))
    {
	send_to_char("That player doesn't exist.\n\r", ch);
	return;
    }

    arg2[0] = UPPER(arg2[0]);
    free_string(chat_room->created_by);
    chat_room->created_by = str_dup(arg2);

    sprintf(buf, "Set creator of #%s to '%s'.\n\r", chat_room->name, arg2);
    send_to_char(buf, ch);
}


void write_chat_rooms()
{
    FILE *fp;
    CHAT_ROOM_DATA *chat;
    CHAT_OP_DATA *op;
    int count;
    int op_count;

    fp = fopen(CHAT_FILE, "w");
    if (fp == NULL)
    {
        bug("Couldn't load chat_rooms.dat", 0);
	exit(1);
    }

    count = 0;
    for (chat = chat_room_list; chat != NULL; chat = chat->next)
    {
        if (chat->permanent == true)
            count++;
    }

    if (count == 0)
    {
        fprintf(fp, "%d\n", count);
        return;
    }

    fprintf(fp, "%d\n", count);

    for (chat = chat_room_list; chat != NULL; chat = chat->next)
    {
 	if (chat->permanent == true)
	{
   	    fprintf(fp, "%s~\n", fix_string(chat->name));
	    fprintf(fp, "%s~\n", fix_string(chat->topic)) ;
	    fprintf(fp, "%s~\n", fix_string(chat->password));
	    fprintf(fp, "%s~\n", chat->created_by);
	    fprintf(fp, "%ld\n", chat->vnum);
	    fprintf(fp, "%d\n", chat->max_people);

	    /* write ops */
	    op_count = 0;
	    for (op = chat->ops; op != NULL; op = op->next)
	    {
                op_count++;
	    }

	    fprintf(fp, "%d\n", op_count);

            for (op = chat->ops; op != NULL; op = op->next)
            {
	  	fprintf(fp, "%s~\n", op->name);
	    }
	}
    }
    fclose(fp);
}


void read_chat_rooms()
{
    CHAT_ROOM_DATA *chat;
    CHAT_ROOM_DATA *last_chat;
    CHAT_OP_DATA *op;
    CHAT_OP_DATA *last_op;
    ROOM_INDEX_DATA *room;
    char buf[MAX_STRING_LENGTH];
    FILE *fp;
    int count;
    int counter;
    int op_count;

    fp = fopen(CHAT_FILE, "r");

    if (fp == NULL)
    {
        bug("Couldn't load chat_rooms.dat", 0);
        exit(1);
    }

    counter = 0;
    count = fread_number(fp);
    if (count == 0)
    {
        log_string("*** No chat rooms to read.\n");
        return;
    }
    last_chat = NULL;

    for (counter = 0; counter < count; counter++)
    {
        chat = new_chat_room();

	if (last_chat != NULL)
	    last_chat->next = chat;

	chat->name = fread_string(fp);
	chat->topic = fread_string(fp);
	chat->password = fread_string(fp);
	chat->permanent = true;
	chat->created_by = fread_string(fp);
	chat->vnum = fread_number(fp);
	chat->max_people = fread_number(fp);
	last_chat = chat;

	sprintf(buf,
	"*** Chat room %s, pass %s, created_by %s, vnum %ld, max %i",
	    chat->name,
	    chat->password,
	    chat->created_by,
	    chat->vnum,
	    chat->max_people);
	log_string(buf);

	op_count = fread_number(fp);
	if (op_count == 0)
  	    log_string("No operators.");
	else
	{
	    int i;

	    last_op = NULL;
	    for (i = 0; i < op_count; i++)
	    {
	        op = new_chat_op();
		if (last_op != NULL)
		    last_op->next = op;

		op->name = fread_string(fp);
		op->next = NULL;
		op->chat_room = chat;

		sprintf(buf, "Operator: %s for #%s",
				op->name, chat->name);
		log_string(buf);
		last_op = op;

		if (chat->ops == NULL)
			chat->ops = op;
	    }
	}

	room = get_room_index(area_chat, chat->vnum);

	if (room == NULL)
	{
	    sprintf(buf, "read_chat_rooms: %s had null room!",
	        chat->name);
	    bug(buf, 0);
	    exit(1);
	}

	room->chat_room = chat;

	if (counter == 0)
	    chat_room_list = chat;
    }

    fclose(fp);
}

