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


SPELL_FUNC(spell_curse)
{
	CHAR_DATA *victim;
	OBJ_DATA *obj;
	AFFECT_DATA af;
	memset(&af,0,sizeof(af));

	/* deal with the object case first */
	if (target == TARGET_OBJ) {
		obj = (OBJ_DATA *) vo;
		if (IS_OBJ_STAT(obj,ITEM_EVIL)) {
			act("$p is already filled with evil.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
			return FALSE;
		}

		if (IS_OBJ_STAT(obj,ITEM_BLESS)) {
			AFFECT_DATA *paf;

			paf = affect_find(obj->affected,gsn_bless);
			if (!saves_dispel(ch,NULL, paf ? paf->level : obj->level)) {
				if (paf) affect_remove_obj(obj,paf);
				act("$p glows with a red aura.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
				REMOVE_BIT(obj->extra_flags,ITEM_BLESS);
				return TRUE;
			} else {
				act("The holy aura of $p is too powerful for you to overcome.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
				return FALSE;
			}
		}

		af.where = TO_OBJECT;
		af.group = AFFGROUP_ENCHANT;
		af.type = sn;
		af.level = level;
		af.duration = URANGE(1,2 * level, 5);
		af.location = 0;
		af.modifier = +1;
		af.bitvector = ITEM_EVIL;
		af.bitvector2 = 0;
		af.slot	= WEAR_NONE;
		affect_to_obj(obj,&af);

		act("$p glows with a malevolent aura.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);

		if (obj->wear_loc != WEAR_NONE)
			ch->saving_throw += 1;
		return TRUE;
	}

	/* character curses */
	if (target == TARGET_CHAR) {
		victim = (CHAR_DATA *) vo;

		if (IS_AFFECTED(victim,AFF_CURSE) || saves_spell(level,victim,DAM_NEGATIVE))
			return FALSE;

		af.where = TO_AFFECTS;
		af.group = AFFGROUP_DIVINE;
		af.type = sn;
		af.level = level;
		af.duration = 2*level;
		af.location = APPLY_HITROLL;
		af.modifier = -1 * (level / 8);
		af.bitvector = AFF_CURSE;
		af.bitvector2 = 0;
		af.slot	= obj_wear_loc;
		affect_to_char(victim, &af);

		send_to_char("You feel unclean.\n\r", victim);
		if (ch != victim)
			act("$N looks very uncomfortable.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return TRUE;
	}

	return FALSE;
}


SPELL_FUNC(spell_demonfire)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	act("{R$n calls forth the demons of Hell upon $N!{x", ch,victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	act("{R$n has assailed you with the demons of Hell!{x", ch,victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	send_to_char("{RYou conjure forth the demons of hell!{x\n\r",ch);

	dam = dice(level, 7);
	if (saves_spell(level, victim,DAM_NEGATIVE))
		dam /= 2;

	damage(ch, victim, dam, sn, DAM_NEGATIVE ,TRUE);
	spell_curse(gsn_curse, 3 * level / 4, ch, (void *) victim,TARGET_CHAR, WEAR_NONE);
	return TRUE;
}

SPELL_FUNC(spell_destruction)
{
	OBJ_DATA *obj = (OBJ_DATA *) vo;

	if (IS_SET(obj->extra_flags, ITEM_NOPURGE) || !IS_SET(obj->wear_flags, ITEM_TAKE)) {
		act("Even with the mightiest of magics, you can't seem to destroy $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	act("A powerful force engulfs $p, and it vanishes instantly!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	act("$n's $p vanishes in a flash of light!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

	if (obj->wear_loc != WEAR_NONE) unequip_char(obj->carried_by, obj, TRUE);

	extract_obj(obj);
	return TRUE;
}

SPELL_FUNC(spell_dispel_good)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	if (IS_EVIL(victim)) {
		act("$N is protected by $S evil.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		return FALSE;
	}

	if (IS_NEUTRAL(victim)) {
		act("$N does not seem to be affected.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	if (victim->hit > (ch->tot_level * 4))
		dam = dice(level, 4);
	else
		dam = UMAX(victim->hit, dice(level,4));
	if (saves_spell(level, victim,DAM_NEGATIVE))
		dam /= 2;
	damage(ch, victim, dam, sn, DAM_NEGATIVE ,TRUE);
	return TRUE;
}

SPELL_FUNC(spell_slow)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	int lvl, catalyst;
	memset(&af,0,sizeof(af));

	lvl = victim->tot_level - ch->tot_level;
	if(IS_REMORT(victim)) lvl += LEVEL_HERO;	// If the victim is remort, it will require MORE catalyst
	if(IS_REMORT(ch)) lvl -= LEVEL_HERO;		// If the caster is remort, it will require LESS catalyst
	lvl = (lvl > 19) ? (lvl / 10) : 1;

	catalyst = has_catalyst(ch,NULL,CATALYST_BODY,CATALYST_INVENTORY,1,CATALYST_MAXSTRENGTH);
	if(catalyst >= 0 && catalyst < lvl) {
		send_to_char("You appear to be missing a required body catalyst.\n\r", ch);
		return FALSE;
	}

	use_catalyst(ch,NULL,CATALYST_BODY,CATALYST_INVENTORY,lvl,1,CATALYST_MAXSTRENGTH,TRUE);

	if (IS_AFFECTED(victim,AFF_SLOW)) {
		if (victim == ch)
			send_to_char("You can't move any slower!\n\r",ch);
		else
			act("$N can't get any slower.", ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return FALSE;
	}

	if (IS_SET(victim->imm_flags,IMM_MAGIC)) {
		if (victim != ch)
			send_to_char("Nothing seemed to happen.\n\r",ch);
		send_to_char("You feel momentarily lethargic.\n\r",victim);
		return FALSE;
	}

	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = sn;
	af.level = level;
	af.duration = (catalyst/lvl)+2;//level/2;
	af.location = APPLY_DEX;
	af.modifier = -1 - (level >= 18) - (level >= 25) - (level >= 32);
	af.bitvector = AFF_SLOW;
	af.bitvector2 = 0;
	af.slot	= obj_wear_loc;
	affect_to_char(victim, &af);
	send_to_char("You feel yourself slow i n g  d  o   w    n...\n\r", victim);
	act("$n starts to move in slow motion.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
	return TRUE;
}

SPELL_FUNC(spell_weaken)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	memset(&af,0,sizeof(af));

	if (is_affected(victim, sn) || saves_spell(level, victim,DAM_OTHER))
		return FALSE;

	af.where     = TO_AFFECTS;
	af.group    = AFFGROUP_MAGICAL;
	af.type      = sn;
	af.level     = level;
	af.duration  = URANGE(1, level / 2, 5);
	af.location  = APPLY_STR;
	af.modifier  = -1 * (level / 13);
	af.bitvector = AFF_WEAKEN;
	af.bitvector2 = 0;
	af.slot	= obj_wear_loc;
	affect_to_char(victim, &af);
	send_to_char("You feel your strength slip away.\n\r", victim);
	act("$n looks tired and weak.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
	return TRUE;
}
