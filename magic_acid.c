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

SPELL_FUNC(spell_acid_blast)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	if (check_shield_block_projectile(ch, victim, "acid blast", NULL))
		return false;

	dam = dice(level, 6);
	if (saves_spell(level, victim, DAM_ACID)) dam /= 2;

	damage(ch, victim, dam, skill, TYPE_UNDEFINED, DAM_ACID, true);
	return true;
}

SPELL_FUNC(spell_acid_breath)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	act("$n spits acid at $N.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	act("$n spits a stream of corrosive acid at you.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act("You spit acid at $N.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	if (check_shield_block_projectile(ch, victim, "acid stream", NULL))
		return false;

	dam = level * 15;

	if (IS_DRAGON(ch))
		dam += dam/4;

	acid_effect(victim,level,dam/13,TARGET_CHAR);
	victim->set_death_type = DEATHTYPE_BREATH;
	damage(ch,victim,dam,skill,TYPE_UNDEFINED,DAM_ACID,true);
	return true;
}

