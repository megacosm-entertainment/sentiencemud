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
#include "io/json/json_chat.h"
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
void do_chat_show(CHAR_DATA *ch, char *argument);


/**
 * do_chat - Main chat room system command dispatcher
 *
 * Entry point for all chat room commands. The chat system is a "social"
 * dimension where players can create and join virtual chat rooms while
 * their characters are stored in a safe location.
 *
 * Subcommands:
 * - enter       : Enter the chat dimension from a safe room
 * - exit        : Return to the game world
 * - list        : List all available chat rooms
 * - show [room] : Show details about a chat room
 * - join <room> [password] : Join a specific chat room
 * - create <name> <max> [password] : Create a temporary chat room
 * - permcreate  : (Immortal) Create a permanent chat room
 * - topic <text>: Set the chat room topic (ops only)
 * - delete      : Delete a chat room (creator only)
 * - op [name]   : Add/remove operators, or list current ops
 * - kick <name> : Kick a player from the chat room (ops only)
 * - password    : Change the room password (ops only)
 * - setfounder  : (Staff) Change chat room ownership
 *
 * @param ch        Character using the chat command
 * @param argument  Subcommand and arguments
 */
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

    if (!str_cmp(arg, "show"))
    {
    do_function(ch, &do_chat_show, argument);
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
        "KICK PASSWORD SHOW\n\r", ch);
    }

    return;
    }

send_to_char("Valid commands are:\n\r"
        "ENTER EXIT LIST CREATE JOIN DELETE TOPIC OP\n\r"
        "KICK PASSWORD SHOW\n\r", ch);
}


/**
 * do_chat_enter - Transport player into the chat dimension
 *
 * Moves a player from the game world into the chat lobby. The player's
 * original location is saved for return via do_chat_exit.
 *
 * Entry requirements:
 * - Must not already be in chat (IS_SOCIAL)
 * - Must be at the area's recall point in a ROOM_SAFE room
 * - Must not be in combat
 * - Must not have no_recall timer active (unless immortal)
 * - Cannot enter from Maze areas
 *
 * Side effects:
 * - Saves before_social location for return
 * - Purges tokens with TOKEN_PURGE_RIFT flag
 * - Resets manastore to 0
 * - Moves player to room_chat_lobby reserved room
 *
 * @param ch        Character entering chat
 * @param argument  Unused
 */
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
    pbugf(LOG_ERROR, "do_chat_enter: %s with null in_room!",
        ch->name);
    return;
    }

    if (!str_prefix("Maze-Level", ch->in_room->area->name)) {
        send_to_char("You can't enter chat from here.\n\r", ch);
    return;
    }

    recall = get_area_recall_room(ch->in_room->area);

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

    act("{WA ghostly spirit appears before $n and pulls $m to another dimension.{x",   ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("{WA ghostly spirit appears before you and pulls you to another dimension.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    for (token = ch->tokens; token != NULL; token = token_next) {
    token_next = token->next;

    if (IS_SET(token->flags, TOKEN_PURGE_RIFT)) {
        p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_TOKEN_REMOVED, NULL);
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

    ROOM_INDEX_DATA *chat_lobby = get_reserved_room_index("room_chat_lobby");
    if (!chat_lobby) {
        pbugf(LOG_ERROR, "do_chat_enter: room_chat_lobby not found!");
        send_to_char("Chat is currently unavailable.\n\r", ch);
        return;
    }

    char_from_room(ch);
    char_to_room(ch, chat_lobby);

    act("{W$n has entered chat.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

    //SET_BIT(ch->comm, COMM_SOCIAL);
}


/**
 * do_chat_exit - Return player from chat dimension to game world
 *
 * Moves a player from the chat dimension back to their original
 * location (stored in before_social when they entered).
 *
 * If before_social is invalid, falls back to room_default_recall.
 *
 * @param ch        Character exiting chat
 * @param argument  Unused
 */
void do_chat_exit(CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *room = NULL;

    if (!IS_SOCIAL(ch))
    {
    send_to_char("You aren't in chat.\n\r", ch);
    return;
    }

    room = location_to_room(&ch->before_social);
    location_clear(&ch->before_social);

    if (!room) {
    pbugf(LOG_ERROR, "do_chat_exit: before_social room was null!");

room = get_reserved_room_index("room_default_recall");

    //REMOVE_BIT(ch->comm, COMM_SOCIAL);

    char_from_room(ch);

    char_to_room(ch, room);
    return;
    }

    act("{W$n has left chat.{x",   ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("{WYou exit chat.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    //REMOVE_BIT(ch->comm, COMM_SOCIAL);

    char_from_room(ch);
    char_to_room(ch, room);

    act("{W$n fades in from another dimension.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
}


/**
 * do_chat_list - Display all available chat rooms
 *
 * Shows a formatted table of all chat rooms with:
 * - Room number and name
 * - Current/max occupancy
 * - Creator name
 * - Operators list
 * - Room topic
 *
 * Must be in the chat dimension (IS_SOCIAL) to use.
 *
 * @param ch        Character viewing the list
 * @param argument  Unused
 */
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


/**
 * do_chat_join - Join a specific chat room
 *
 * Moves a player from their current chat location to a named chat room.
 * Password-protected rooms require the correct password unless the
 * player is the creator or an operator.
 *
 * Syntax: chat join <roomname> [password]
 *
 * @param ch        Character joining the room
 * @param argument  Room name and optional password
 */
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

    AREA_DATA *chat_area = chat->area_uid > 0 ? get_area_index(chat->area_uid) : NULL;
    if (!chat_area) chat_area = get_system_area_fallback();
    room = get_room_index(chat_area, chat->vnum);
    if (room == NULL)
    {
    pbugf(LOG_ERROR, "do_chat_join: %s, %s had null chat->vnum\n\r",
        ch->name,
        chat->name);
    return;
    }

    if (room == ch->in_room)
    {
    send_to_char("You're already there.\n\r", ch);
    return;
    }
    
    // Only check password if not creator and not an op
    if (str_cmp(chat->password, "none") && 
        str_cmp(chat->created_by, ch->name) && 
        !is_op(chat, ch->name) && 
        str_cmp(chat->password, arg2))
    {
        sprintf(buf, "#%s is password protected.\n\r"
            "Use /join <chatroom> <password>.\n\r",
            chat->name);
        send_to_char(buf, ch);
        return;
    }

    act("{Y$n leaves for another chat room.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    sprintf(buf, "{YYou join #%s.{x\n\r", chat->name);
    send_to_char(buf, ch);

    char_from_room(ch);
    char_to_room(ch, room);

    sprintf(buf, "$n has joined #%s.", chat->name);
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

    do_function(ch, &do_look, "auto");
}


/**
 * chat_create - Create a new chat room at the current location
 *
 * Creates a chat room linked to the current room in the chat dimension.
 * The creator automatically becomes an operator.
 *
 * Syntax: chat create <name> <max_people> [password]
 *
 * Restrictions:
 * - Must be in chat dimension
 * - Room must not already have a chat room
 * - Cannot exceed MAX_CHAT_ROOMS total
 * - Max people must be 1 to MAX_IN_CHAT_ROOM
 * - Password-protected rooms must have only one exit (prevents blocking)
 * - Name must be unique
 *
 * Temporary rooms are deleted on reboot. Permanent rooms (perm=true)
 * persist via write_chat_rooms/read_chat_rooms.
 *
 * @param ch        Character creating the room
 * @param argument  Room name, max capacity, and optional password
 * @param perm      true for permanent room (immortal only), false for temp
 */
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
    chat->area_uid = (ch->in_room && ch->in_room->area) ? ch->in_room->area->uid : 0;
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


/**
 * do_chat_topic - Set the topic for the current chat room
 *
 * Changes the displayed topic for the chat room. Only operators
 * can modify the topic. Topic is limited to 150 characters.
 *
 * @param ch        Character setting the topic (must be an op)
 * @param argument  New topic text
 */
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
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

    write_chat_rooms();
}


/**
 * do_chat_delete - Delete the current chat room
 *
 * Removes the chat room in the current room. Only the creator can
 * delete their room. Permanent rooms can only be deleted by MAX_LEVEL
 * staff.
 *
 * @param ch        Character deleting the room
 * @param argument  Unused
 */
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

    if (ch->in_room->chat_room->permanent == true
    && ch->tot_level < MAX_LEVEL)
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
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

    /* dislink it from the room */
    ch->in_room->chat_room = NULL;

    sprintf(buf, "{YYou have deleted #%s.{x\n\r", chat->name);
    send_to_char(buf, ch);

    extract_chat_room(chat);
    write_chat_rooms();
}


/**
 * do_chat_op - Manage chat room operators
 *
 * With no argument, lists all current operators.
 * With a player name, toggles their operator status (adds if not op,
 * removes if already op).
 *
 * Only existing operators (or immortals) can modify the op list.
 * The player must exist in the game.
 *
 * @param ch        Character managing operators
 * @param argument  Player name to toggle, or empty to list
 */
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


/**
 * is_op - Check if a player is an operator in a chat room
 *
 * Searches the chat room's operator list for a matching name.
 *
 * @param chat  Chat room to check
 * @param arg   Player name to look for
 * @return      true if player is an operator, false otherwise
 */
bool is_op(CHAT_ROOM_DATA *chat, char *arg)
{
    CHAT_OP_DATA *op;

    if (chat == NULL)
    {
    pbugf(LOG_ERROR, "is_op: null chat_room");
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


/**
 * chat_add_op - Add a player as an operator to the current chat room
 *
 * Creates a new operator entry and adds it to the chat room's ops list.
 * Notifies the target player if they're in the room.
 *
 * @param ch   Character adding the operator
 * @param arg  Name of player to add as operator
 */
void chat_add_op(CHAR_DATA *ch, char *arg)
{
    CHAT_OP_DATA *op;
    CHAT_ROOM_DATA *chat;
    CHAR_DATA *vch;

    chat = ch->in_room->chat_room;

    act("{YYou add $T as an operator.{x", ch, NULL, NULL, NULL, NULL, NULL, arg, TO_CHAR, NULL, NULL);

    if ((vch = get_char_room(ch, NULL, arg)) != NULL)
    {
    act("{Y$n adds you as an operator.{x", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
    }

    for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room)
    {
    if (str_cmp(vch->name, arg))
        act("{Y$n adds $t as an operator.{x", ch, vch, NULL, NULL, NULL, arg, NULL, TO_VICT, NULL, NULL);
    }

    op = new_chat_op();

    op->next = chat->ops;
    chat->ops = op;

    op->name = str_dup(arg);
    op->chat_room = chat;
}


/**
 * chat_rem_op - Remove a player's operator status from the current chat room
 *
 * Finds and removes the operator entry from the chat room's ops list.
 * Players can remove themselves. Notifies the target if in the room.
 *
 * @param ch   Character removing the operator
 * @param arg  Name of player to remove as operator
 */
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
    act("{Y$n removes you as an operator.{x", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
    }

    if (!str_cmp(ch->name, arg))
    {
    sprintf(buf, "{Y$n removes $mself as an operator.{x");
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    }
    else
    {
    for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room)
    {
        if (str_cmp(vch->name, arg))
        act("{Y$n removes $t as an operator.{x", ch, vch, NULL, NULL, NULL, arg, NULL, TO_VICT, NULL, NULL);
    }
    }

    if (prev_op != NULL)
    prev_op->next = op->next;
    else
    op->chat_room->ops = op->next;

    free_chat_op(op);
}


/**
 * do_chat_kick - Kick a player from the current chat room
 *
 * Forcibly moves a player from the current chat room to the chat lobby.
 * Only operators can kick players. Ops can kick themselves.
 *
 * @param ch        Operator kicking the player
 * @param argument  Name of player to kick
 */
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

    to_room = get_reserved_room_index("room_chat_lobby");
    sprintf(buf, "{YYou kick %s out of #%s.{x",
        ch == victim ? "yourself" : "$N",
        ch->in_room->chat_room->name);
    act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    if (ch != victim)
    {
        sprintf(buf, "{Y%s kicks you out of #%s.{x",
            ch->name, ch->in_room->chat_room->name);
        act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
    }

    sprintf(buf, "{Y%s kicks %s out of #%s.{x",
        ch->name,
        ch == victim ? "$mself" : victim->name,
        ch->in_room->chat_room->name);
    act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);

    char_from_room(victim);
    char_to_room(victim, to_room);
}


/**
 * do_chat_ban - Manage banned players in the current chat room
 *
 * With no argument, lists all banned players.
 * With a player name, toggles their ban status.
 *
 * Note: This function appears incomplete - the ban check logic
 * at line 993-999 has a bug where it overwrites 'add' each iteration.
 * Maximum 50 bans per room.
 *
 * @param ch        Operator managing bans
 * @param argument  Player name to toggle ban, or empty to list
 */
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
    CHAT_BAN_DATA *found_ban = NULL;
    for (ban = chat->bans; ban != NULL; ban = ban->next)
    {
    if (!str_cmp(ban->name, arg))
    {
        found_ban = ban;
        add = false;
        break;
    }
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
    else if (found_ban)
    {
    chat_remove_ban(chat, found_ban);
    sprintf(buf, "{YUnbanned \"%s\" from #%s.{x\n\r",
        found_ban->name, chat->name);
    }
}


/**
 * chat_add_ban - Add a player to the chat room's ban list
 *
 * Creates a new ban entry recording who was banned and by whom.
 *
 * @param ch        Operator creating the ban
 * @param argument  Name of player to ban
 */
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


/**
 * chat_remove_ban - Remove a player from the chat room's ban list
 *
 * Note: This function has a bug - it removes ALL bans from the room
 * rather than just the specified ban. The 'ban' parameter is reassigned
 * in the for loop, ignoring the input value.
 *
 * @param chat  Chat room to modify
 * @param ban   Ban entry to remove (currently ignored due to bug)
 */
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


/**
 * do_chat_password - Change the password for the current chat room
 *
 * Sets or changes the password required to join the chat room.
 * Only operators can change the password. Password-protected rooms
 * must have only one exit to prevent blocking navigation.
 *
 * @param ch        Operator changing the password
 * @param argument  New password
 */
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


/**
 * do_chat_setfounder - Admin command to change chat room ownership
 *
 * Changes the creator/founder of a chat room. The new founder must
 * be an existing player. Staff level 151+ required (checked in do_chat).
 *
 * Syntax: chat setfounder <roomname> <playername>
 *
 * @param ch        Staff member changing ownership
 * @param argument  Room name and new founder name
 */
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


/**
 * write_chat_rooms - Save permanent chat rooms to disk
 *
 * Writes all permanent chat rooms (chat->permanent == true) to JSON file.
 * Called after any modification to permanent rooms (create, topic, op changes).
 */
void write_chat_rooms()
{
    // Use JSON save function
    if (!save_chat_rooms_json()) {
        pbugf(LOG_ERROR, "write_chat_rooms: Failed to save chat rooms to JSON");
    }
}
/**
 * read_chat_rooms - Load permanent chat rooms from disk at boot
 *
 * Tries to load from JSON first (data/chat_rooms.json).
 * If JSON doesn't exist, falls back to legacy .dat format.
 * If legacy format is loaded, automatically saves as JSON and archives the old file.
 *
 * Each room is linked to the physical room via room->chat_room pointer.
 * Skips rooms whose area_uid is not found (area may have been deleted).
 */
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
    char chat_file_buf[MAX_INPUT_LENGTH];
    const char *chat_file = resolve_game_path(CHAT_FILE, chat_file_buf, sizeof(chat_file_buf));
    
    // Try JSON format first
    if (load_chat_rooms_json()) {
        log_string("Loaded chat rooms from JSON");
        return;
    }
    
    // Fall back to legacy .dat format
    log_string("JSON not found, trying legacy .dat format");

    fp = fopen(chat_file, "r");

    if (fp == NULL)
    {
        log_string("*** No chat rooms file found (.dat or .json)");
        return;  // Not an error - just no rooms
    }

    // Check for empty file
    int c = fgetc(fp);
    if (c == EOF) {
        log_string("*** Chat room file is empty, skipping load.");
        fclose(fp);
        return;
    }
    ungetc(c, fp);

    counter = 0;
    count = fread_number(fp);
    if (count == 0)
    {
        log_string("*** No chat rooms to read.\n");
        fclose(fp);
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
	
	// Always read new format (area_uid, vnum, max_people)
	// The file has been migrated to the new format
    // Not yet!
	//chat->area_uid = fread_number(fp);
	chat->vnum = fread_number(fp);
	chat->max_people = fread_number(fp);
	last_chat = chat;

	sprintf(buf,
	"*** Chat room %s, pass %s, created_by %s, area_uid %ld, vnum %ld, max %i",
	    chat->name,
	    chat->password,
	    chat->created_by,
	    chat->area_uid,
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

	// Look up the area and room using area_uid + vnum
	AREA_DATA *chat_area = NULL;
    room = NULL;
	if (chat->area_uid > 0) {
	    chat_area = get_area_from_uid(chat->area_uid);
	    if (chat_area) {
	        room = get_room_index(chat_area, chat->vnum);
	    }
	}

	if (room == NULL)
	{
	    pbugf(LOG_ERROR, "read_chat_rooms: %s had null room (area_uid %ld, vnum %ld)!",
	        chat->name, chat->area_uid, chat->vnum);
	    continue;
	}

	room->chat_room = chat;

	if (counter == 0)
	    chat_room_list = chat;
    }

    fclose(fp);
    
    // Migrate to JSON format
    log_string("Migrating chat rooms from .dat to JSON...");
    if (save_chat_rooms_json()) {
        // Archive the old .dat file
        char old_file[MSL];
        char archive_file[MSL];
        sprintf(old_file, "%s", CHAT_FILE);
        sprintf(archive_file, "%s.old", CHAT_FILE);
        rename(old_file, archive_file);
        log_string("Successfully migrated chat rooms to JSON format");
    }
}

/**
 * do_chat_show - Display detailed information about a chat room
 *
 * Shows comprehensive information about a chat room including:
 * - Room name and vnum (immortals only)
 * - Topic
 * - Creator name
 * - Password protection status (actual password shown to ops/staff)
 * - Permanent status
 * - Current capacity
 * - Operators list
 * - Current occupants (for current room or immortals)
 *
 * Operators shown in green in occupant list.
 *
 * @param ch        Character viewing the information
 * @param argument  Room name, or empty for current room
 */
void do_chat_show(CHAR_DATA *ch, char *argument)
{
    CHAT_ROOM_DATA *chat;
    CHAT_OP_DATA *op;
    CHAR_DATA *rch;
    char arg[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    bool found = false;
    int count = 0;

    argument = one_argument(argument, arg);

    if (!IS_SOCIAL(ch))
    {
        send_to_char("You must be in chat to use this command.\n\r", ch);
        return;
    }
    
    // If no argument, show the current chat room
    if (arg[0] == '\0')
    {
        if (ch->in_room->chat_room == NULL)
        {
            send_to_char("You aren't in a chat room.\n\r", ch);
            return;
        }
        
        chat = ch->in_room->chat_room;
    }
    else
    {
        // Look up the chat room by name
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
    }
    
    // Display chat room information
    sprintf(buf, "{W=== Chat Room Information for #{Y%s{W ==={x\n\r", chat->name);
    send_to_char(buf, ch);
    
    line(ch, 65, NULL, NULL);

    // Look up room name using area_uid + vnum
    AREA_DATA *chat_area = NULL;
    ROOM_INDEX_DATA *chat_room = NULL;
    if (chat->area_uid > 0) {
        chat_area = get_area_from_uid(chat->area_uid);
        if (chat_area) {
            chat_room = get_room_index(chat_area, chat->vnum);
        }
    }
    
    if (chat_room) {
        sprintf(buf, "{YRoom:{x %s\n\r", chat_room->name);
        send_to_char(buf, ch);
    }
    
    if (IS_IMMORTAL(ch))
    {
        sprintf(buf, "{YVnum:{x %ld\n\r", chat->vnum);
        send_to_char(buf, ch);
    }
    
    sprintf(buf, "{YTopic:{x %s\n\r", chat->topic);
    send_to_char(buf, ch);
    
    sprintf(buf, "{YCreated by:{x %s\n\r", chat->created_by);
    send_to_char(buf, ch);
    
    sprintf(buf, "{YPassword protected:{x %s\n\r", 
        (str_cmp(chat->password, "none")) ? "Yes" : "No");
    send_to_char(buf, ch);
    
    // Only show the actual password to ops and qualified staff members
    if ((is_op(chat, ch->name) || !str_cmp(ch->name, chat->created_by) ||
         (get_staff_rank(ch) > STAFF_ASCENDANT) || 
         (IS_IMMORTAL(ch) && 
          (is_staff_duty_in_list(ch, "Administrator 'Player Relations'")))) && 
        str_cmp(chat->password, "none"))
    {
        sprintf(buf, "{YPassword:{x %s\n\r", chat->password);
        send_to_char(buf, ch);
    }
    
    sprintf(buf, "{YPermanent:{x %s\n\r", chat->permanent ? "Yes" : "No");
    send_to_char(buf, ch);
    
    sprintf(buf, "{YCapacity:{x %d/%d\n\r", chat->curr_people, chat->max_people);
    send_to_char(buf, ch);
    
    // List operators
    send_to_char("{YOperators:{x ", ch);
    count = 0;
    for (op = chat->ops; op != NULL; op = op->next)
    {
        sprintf(buf, "%s%s", count > 0 ? ", " : "", op->name);
        send_to_char(buf, ch);
        count++;
    }
    
    if (count == 0)
        send_to_char("None", ch);
    send_to_char("\n\r", ch);
    
    // List current occupants if this is for the room the player is in
    // or if they're an immortal
    if (ch->in_room->chat_room == chat || IS_IMMORTAL(ch))
    {
        // Look up the actual room using area_uid + vnum
        ROOM_INDEX_DATA *room = NULL;
        AREA_DATA *occ_area = NULL;
        if (chat->area_uid > 0) {
            occ_area = get_area_from_uid(chat->area_uid);
            if (occ_area) {
                room = get_room_index(occ_area, chat->vnum);
            }
        }
        
        send_to_char("{YCurrent occupants:{x ", ch);
        count = 0;
        
        if (room != NULL)
        {
            for (rch = room->people; rch != NULL; rch = rch->next_in_room)
            {
                if (!IS_NPC(rch))
                {
                    sprintf(buf, "%s%s%s", 
                        count > 0 ? ", " : "",
                        is_op(chat, rch->name) ? "{G" : "", 
                        rch->name);
                    send_to_char(buf, ch);
                    count++;
                }
            }
        }
        
        if (count == 0)
            send_to_char("None", ch);
        send_to_char("{x\n\r", ch);
    }
    
    line(ch, 65, NULL, NULL);
}