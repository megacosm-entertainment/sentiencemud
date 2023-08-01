/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "merc.h"


void do_locker(CHAR_DATA *ch, char* argument)
{
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	char buf[MSL];
	OBJ_DATA *obj;
	bool item = FALSE;
	struct tm *rent_time;

	argument = one_argument(argument, arg1);
	argument = one_argument(argument, arg2);

	rent_time = (struct tm *)localtime(&ch->locker_rent);

	// locker forgive <player>
	// if they have a locker and it is expired, it will set it active with one month.
	// Level requirements: 153+
	if( !IS_NPC(ch) && IS_IMMORTAL(ch) && IS_TRUSTED(ch, (MAX_LEVEL - 2)) )
	{
		if( !str_cmp(arg1, "forgive") )
		{
			CHAR_DATA *player;

			if( arg2[0] == '\0' )
			{
				send_to_char("Forgive whose locker rent?\n\r", ch);
				return;
			}

			player = get_char_world(ch, arg2);
			if( player == NULL || IS_NPC(player) )
			{
				send_to_char("That player is not online.\n\r", ch);
				return;
			}

			if( player->locker_rent == 0 )
			{
				send_to_char("That player does not have a valid locker.\n\r", ch);
				return;
			}

			if( player->locker_rent > current_time ) {
				send_to_char("That player's locker is currently active.\n\r", ch);
				return;
			}

			player->locker_rent = current_time;

			rent_time = (struct tm *) localtime(&player->locker_rent);
			rent_time->tm_mon += 1;
			player->locker_rent = (time_t) mktime(rent_time);

			act("Locker rent for $N has been forgiven.", ch, player, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			send_to_char("{WYour locker rent has been forgiven.{x", player);

			return;
		}
	}

	for (obj = ch->carrying; obj != NULL; obj = obj->next_content) {
		if (IS_SET(obj->extra[1], ITEM_LOCKER)) {
			item = TRUE;
			break;
		}
	}

	if (!IS_SET(ch->in_room->room_flags, ROOM_LOCKER) && !item)
	{
		send_to_char("You can't do that here.\n\r", ch);
		return;
	}

	if (arg1[0] == '\0')
	{
		send_to_char("Do what with your locker?\n\r", ch);
		return;
	}

	if (!str_cmp(arg1, "rent"))
	{
		if (ch->pcdata->bankbalance < 5000)
		{
			send_to_char("You do not have 5000 gold in your bank account.\n\r", ch);
			return;
		}

		ch->pcdata->bankbalance -= 5000;

		if (ch->locker_rent < current_time)
			ch->locker_rent = current_time;

		rent_time = (struct tm *) localtime(&ch->locker_rent);
		rent_time->tm_mon += 1;
		ch->locker_rent = (time_t) mktime(rent_time);
		send_to_char("Rent extended to:\n\r", ch);
		send_to_char((char *) ctime(&ch->locker_rent), ch);
		return;
	}

	if (!str_cmp(arg1, "info"))
	{
		if (ch->locker_rent == 0)
		{
			send_to_char("You have not rented a locker.\n\r", ch);
			return;
		}
		send_to_char("You last paid your locker rent till:\n\r", ch);
		send_to_char((char *) ctime(&ch->locker_rent), ch);
		return;
	}

	if (current_time > ch->locker_rent)
	{
		send_to_char("You either do not have a locker or it has expired.\n\r", ch);
		return;
	}

	if (!str_cmp(arg1, "list"))
	{
		int i = 0;

		for (obj = ch->locker; obj != NULL; obj = obj->next_content)
			i++;

		sprintf(buf, "You look in your locker and see %d items:\n\r", i);
		send_to_char(buf, ch);
		show_list_to_char(ch->locker, ch, TRUE, TRUE);

		act("$n looks over the contents of $s locker.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		return;
	}

	if (arg2[0] == '\0')
	{
		send_to_char("Do what with your locker?\n\r", ch);
		return;
	}

	if (!str_cmp(arg1, "store") || !str_cmp(arg1, "put"))
	{
		if ((obj = get_obj_carry(ch, arg2, ch)) == NULL)
		{
			send_to_char("You do not have that item.\n\r", ch);
			return;
		}

		if ((obj->pIndexData == obj_index_skull || obj->pIndexData == obj_index_gold_skull) && obj->affected != NULL)
		{
			send_to_char("You can't store that enchanted item in your locker.\n\r", ch);
			return;
		}

		if (obj->timer > 0)
		{
			send_to_char("You can only store permanent items in your locker.\n\r", ch);
			return;
		}

		if (obj->item_type == ITEM_CONTAINER && obj->contains)
		{
			send_to_char("You can't put containers in your locker unless they are empty.\n\r", ch);
			return;
		}

		if (count_char_locker(ch) >= MAX_ITEMS_IN_LOCKER)
		{
			send_to_char("It won't fit!\n\r", ch);
			return;
		}

		if (IS_SET(obj->extra[1], ITEM_NOLOCKER) || obj_nest_clones(obj) > 0) {
			send_to_char("You can't put that item in your locker.\n\r", ch);
			return;
		}

		act("You place $p in your locker.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		act("$n places $p in $s locker.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

		obj_from_char(obj);
		obj_to_locker(obj, ch);
		return;
	}

	if (!str_cmp(arg1, "get"))
	{
		if ((obj = get_obj_locker(ch, arg2)) == NULL)
		{
			send_to_char("You do not have that item.\n\r", ch);
			return;
		}

		act("You get $p from your locker.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		act("$n gets $p from $s locker.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

		obj_from_locker(obj);
		obj_to_char(obj, ch);
		return;
	}


	send_to_char("Do what with your locker? (get, put, list)\n\r", ch);
}

