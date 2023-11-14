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
#include "interp.h"
#include "magic.h"
#include "recycle.h"
#include "tables.h"
#include "wilds.h"


SPELL_FUNC(spell_deathbarbs)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	CHAR_DATA *temp_char, *next;
	int dam;
	int roll;

	act("{YYou shoot a swarm of razor sharp barbs toward $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{Y$n shoots a swarm of razor sharp barbs toward you!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act("{Y$n shoots a swarm of razor sharp barbs toward $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);

	if (check_shield_block_projectile(ch, victim, "deathbarbs", NULL))
		return FALSE;

	roll = UMAX((level*2-15),5) * (get_skill(ch, skill)/100);
	roll = UMAX(roll, 5);

	while (roll > 0) {
		dam = dice(roll, 3);
		damage(ch, victim, dam, skill, TYPE_UNDEFINED, DAM_PIERCE, TRUE);
		for (temp_char = ch->in_room->people; temp_char; temp_char = next) {
			next = temp_char->next_in_room;
			if (temp_char != victim && is_same_group(temp_char, victim) &&
				!is_safe_spell(ch, temp_char, FALSE)) {
				act("{YThe barbs fan out, hitting $N!{x", ch,  temp_char, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("{YThe barbs fan out, hitting you!{x", ch, temp_char, NULL, NULL, NULL, NULL, NULL, TO_VICT);
				act("{YThe barbs fan out, hitting $N!{x", ch,  temp_char, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
				damage(ch, temp_char, dam, skill, TYPE_UNDEFINED, DAM_PIERCE, TRUE);
			}
		}

		roll -= level/2;
	}
	return TRUE;
}


