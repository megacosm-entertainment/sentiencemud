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

SPELL_FUNC(spell_soul_essence)
{
	char buf[MSL];
	OBJ_DATA *obj, *obj_next;
	char *arg = (char *) vo;
	int souls, i;
	int skill, skill2;
	bool found = false, all;

	if(IS_NPC(ch)) return false;

	if (!arg) return false;

	if (!arg[0] || (!is_number(arg) && str_cmp(arg,"all"))) {
		send_to_char("How much soul essence did you want to absorb?\n\r", ch);
		return false;
	}

	all = !str_cmp(arg,"all");
	souls = atoi(arg);

	for (obj = ch->carrying, i = 0; obj && (all || i < souls); obj = obj_next) {
		obj_next = obj->next_content;

		if (obj->pIndexData->vnum == OBJ_VNUM_BOTTLED_SOUL) {
			found = true;
			extract_obj(obj);
			i++;
		}
	}

	if (found) {
		skill = get_skill(ch,gsn_soul_essence); skill = UMAX(0,skill);
		skill2 = get_skill(ch,gsn_soul_essence); skill2 = UMAX(0,skill2);

		i = i * skill * skill2 / 10000;

		// Give boost for avatars and wraiths
		if(ch->race == grn_avatar || ch->race == grn_wraith)
			i = i * ( 240 + ch->tot_level ) / 240;

		if(i > 0) {
			sprintf(buf, "{BYou feel {C%d{B soul%s flowing into you!{x\n\r", i, ((i==1)?"":"s"));
			send_to_char(buf,ch);
			act("{B$n glows briefly.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

			ch->pneuma += i;
		} else
			send_to_char("You absorb soul essence, but it completely dissipates...\n\r", ch);
	} else
		send_to_char("You lack soul essence to absorb.\n\r", ch);
	return true;
}


