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

// COSMIC
//  Anything dealing with most conjured sources that don't
//	draw from any particular element.
//  Anything dealing with enhancements, such as enchanting



// No Catalyst:		normal rules
// Weak Catalyst:	stops destruction
// Medium Catalyst:	stops fizzles and destruction (ie, failures)
// Strong Catalyst:	augments the enchanting
// Perfect Catalyst:	allows one more enchant to the maximum
SPELL_FUNC(spell_enchant_armour)
{
	OBJ_DATA *obj;
	AFFECT_DATA *paf;
	int result, fail;

	obj = (OBJ_DATA *) vo;

	if (obj->item_type != ITEM_ARMOUR) {
		send_to_char("That isn't armour.\n\r",ch);
		return FALSE;
	}

	if (obj->wear_loc != -1) {
		send_to_char("You'd better remove it first.\n\r",ch);
		return FALSE;
	}

	if (IS_SET(obj->extra[1], ITEM_NO_ENCHANT)) {
		act("$p is beyond your power to enchant.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	fail = 30;	/* base 30% chance of failure */

	fail -= (level/10);

	fail = URANGE(5,fail,85);

	result = number_percent();

	obj->num_enchanted += 1;
	SET_BIT(obj->extra[1], ITEM_ENCHANTED);

 	/* item destroyed */
	if (result < (fail / 4)) {
		act("$p flares blindingly... and evaporates!",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
		extract_obj(obj);
		return TRUE;
	}

	/* item disenchanted */
	if (result < (fail / 3))  {
		act("$p glows brightly, then fades...oops.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
		act("$p glows brightly, then fades.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM);

		affect_removeall_obj(obj);
		return TRUE;
	}

  	/* failed, no bad result */
	if (result <= fail || obj->num_enchanted > 9) {
		send_to_char("Nothing seemed to happen.\n\r",ch);
		return TRUE;
	}

	act("$p shimmers with a gold aura.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
	SET_BIT(obj->extra[0], ITEM_MAGIC);

	if (obj->affected) {
		for (paf = obj->affected; paf; paf = paf->next) {
			if (paf->location == APPLY_AC) {
				paf->type = sn;
				paf->modifier -= number_range(1, 4);
				paf->level = UMAX(paf->level,level);
			} else if (number_percent() < 15) {
				if (paf->location == APPLY_STR ||
					paf->location == APPLY_DEX ||
					paf->location == APPLY_WIS ||
					paf->location == APPLY_CON ||
					paf->location == APPLY_INT)
					paf->modifier++;

				if (paf->location == APPLY_MANA ||
					paf->location == APPLY_HIT ||
					paf->location == APPLY_MOVE)
					paf->modifier += number_range(5,10);

				if (paf->location == APPLY_HITROLL ||
					paf->location == APPLY_DAMROLL)
					paf->modifier += number_range(1,4);
			}
		}
 	} else {
		/* add a new affect */
		paf = new_affect();

		paf->where = TO_OBJECT;
		paf->group = AFFGROUP_ENCHANT;
		paf->type = sn;
		paf->level = level;
		paf->duration = -1;
		paf->location = APPLY_AC;
		paf->modifier = -1;
		paf->bitvector  = 0;
		paf->bitvector2 = 0;
		paf->next = obj->affected;
		obj->affected = paf;
	}

	SET_BIT(obj->extra[1], ITEM_ENCHANTED);
	return TRUE;
}

SPELL_FUNC(spell_enchant_object)
{
	OBJ_DATA *obj;
	int fail;

	obj = (OBJ_DATA *) vo;

/*
	if (obj->item_type != ITEM_ARMOUR) {
		send_to_char("That isn't armour.\n\r",ch);
		return FALSE;
	}
*/

	if (obj->wear_loc != -1) {
		send_to_char("You'd better remove it first.\n\r",ch);
		return FALSE;
	}

	if (IS_SET(obj->extra[1], ITEM_NO_ENCHANT)) {
		act("$p is beyond your power to enchant.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	fail = 30;	/* base 30% chance of failure */

	fail -= (level/10);

	fail = URANGE(5,fail,85);

	obj->num_enchanted += 1;
	SET_BIT(obj->extra[1], ITEM_ENCHANTED);
	return TRUE;
}



SPELL_FUNC(spell_enchant_weapon)
{
	OBJ_DATA *obj;
	AFFECT_DATA *paf;
	AFFECT_DATA *paf_hit;
	AFFECT_DATA *paf_dam;
	int result;
	int fail;
	int hit_bonus;
	int dam_bonus;

	obj = (OBJ_DATA *) vo;

	if (obj->item_type != ITEM_WEAPON) {
		send_to_char("That isn't a weapon.\n\r",ch);
		return FALSE;
	}

	if (obj->wear_loc != -1) {
		send_to_char("Remove it first.\n\r",ch);
		return FALSE;
	}

	if (IS_SET(obj->extra[1], ITEM_NO_ENCHANT)) {
		act("$p is beyond your power to enchant.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	for (paf_hit = obj->affected; paf_hit; paf_hit = paf_hit->next) {
		if (paf_hit->location == APPLY_HITROLL && paf_hit->type == sn)
			break;
	}

	for (paf_dam = obj->affected; paf_dam; paf_dam = paf_dam->next) {
		if (paf_dam->location == APPLY_DAMROLL && paf_dam->type == sn)
			break;
	}

	/* this means they have no bonus */
	hit_bonus = number_range(level-5000,2);
	hit_bonus = UMAX(1,hit_bonus);
	dam_bonus = number_range(level-5000,2);
	dam_bonus = UMAX(1,dam_bonus);
	fail = 30;	/* base 30% chance of failure */

	/* apply other modifiers */
	fail -= level/10;

	if ((IS_OBJ_STAT(obj,ITEM_BLESS) && ch->alignment > 0) ||
		(IS_OBJ_STAT(obj,ITEM_EVIL) && ch->alignment < 0))
		fail -= 15;

	fail = URANGE(5,fail,95);

	result = number_percent();

	SET_BIT(obj->extra[1], ITEM_ENCHANTED);
	obj->num_enchanted++;

	/* item destroyed */
	if (result < (fail / 4)) {
		act("$p shivers violently and explodes!",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
		extract_obj(obj);
		return TRUE;
	}

 	/* item disenchanted */
	if (result < (fail / 2)) {
		act("$p glows brightly, then fizzles and sparks.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
		act("$n's $p glows brightly, then fizzles and sparks.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM);

		/* remove all affects */
		affect_removeall_obj(obj);
		return TRUE;
	}

	if (result <= fail || obj->num_enchanted > 9) {
		send_to_char("Nothing seemed to happen.\n\r",ch);
		return TRUE;
	}

	act("$p glows blue.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
	SET_BIT(obj->extra[0], ITEM_MAGIC);

	if (paf_dam) {
		paf_dam->modifier+=dam_bonus;
		paf_dam->level = level;
		if (paf_dam->modifier > 4)
			SET_BIT(obj->extra[0],ITEM_HUM);
	} else {
		paf = new_affect();

		paf->where = TO_OBJECT;
		paf->group = AFFGROUP_ENCHANT;
		paf->type = sn;
		paf->level = level;
		paf->duration = -1;
		paf->location = APPLY_DAMROLL;
		paf->modifier =  dam_bonus;
		paf->bitvector  = 0;
		paf->bitvector2 = 0;
		paf->next = obj->affected;
		obj->affected = paf;
	}

	if (paf_hit) {
		paf_hit->modifier+=hit_bonus;
		paf_hit->level = level;
		if (paf_hit->modifier > 4)
			SET_BIT(obj->extra[0],ITEM_HUM);
	} else {
		paf = new_affect();

		paf->where = TO_OBJECT;
		paf->type = sn;
		paf->group = AFFGROUP_ENCHANT;
		paf->level = level;
		paf->duration = -1;
		paf->location = APPLY_HITROLL;
		paf->modifier = hit_bonus;
		paf->bitvector = 0;
		paf->bitvector2 = 0;
		paf->next = obj->affected;
		obj->affected = paf;
	}

	SET_BIT(obj->extra[1], ITEM_ENCHANTED);
	return TRUE;
}
