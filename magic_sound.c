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

SPELL_FUNC(spell_shriek)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	act("$n fills your ears with high-pitched shriek!",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act("You inflict $N with an ear-piercing shriek!",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	dam = level * 16;

	victim->set_death_type = DEATHTYPE_MAGIC;

	if (saves_spell(level,victim,DAM_SOUND)) {
		damage(ch,victim,dam/4,skill,TYPE_UNDEFINED,DAM_SOUND,TRUE);
	} else {
		act("$N screams in pain, covering $S ears.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		damage(ch,victim,dam,skill,TYPE_UNDEFINED,DAM_SOUND,TRUE);
	}
	return TRUE;
}

////////////////////////////////
// Spell: SILENCE
//
// Purpose: Applies SILENCE affect, preventing them from speaking or casting.
//
// Remarks: Requires a SOUND catalyst.
//
// Artificing:
// - Brewing requires a catalyst with 5 units to brew, but only lasts for a few hours.
//
SPELL_FUNC(prespell_silence)
{
	CHAR_DATA *victim;
	int lvl, catalyst;
	char buf[MSL];

	/* character target */
	victim = (CHAR_DATA *) vo;

	lvl = victim->tot_level - ch->tot_level;
	if(IS_REMORT(victim)) lvl += LEVEL_HERO;	// If the victim is remort, it will require MORE catalyst
	if(IS_REMORT(ch)) lvl -= LEVEL_HERO;		// If the caster is remort, it will require LESS catalyst
	lvl = (lvl > 19) ? (lvl / 10) : 1;

	catalyst = has_catalyst(ch,NULL,CATALYST_SOUND,CATALYST_INVENTORY);
	if(catalyst >= 0 && catalyst < lvl) {
		sprintf(buf,"You appear to be missing a required sound catalyst. (%d/%d)\n\r",catalyst,lvl);
		send_to_char(buf, ch);
		return false;
	}

	if (IS_AFFECTED2(victim, AFF2_SILENCE)) {
		if (victim == ch)
			send_to_char("You are already silenced.\n\r",ch);
		else
			act("$N is already silenced.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return false;
	}

	return true;
}

SPELL_FUNC(spell_silence)
{
	CHAR_DATA *victim;
	int lvl, catalyst;

	/* character target */
	victim = (CHAR_DATA *) vo;

	lvl = victim->tot_level - ch->tot_level;
	if(IS_REMORT(victim)) lvl += LEVEL_HERO;	// If the victim is remort, it will require MORE catalyst
	if(IS_REMORT(ch)) lvl -= LEVEL_HERO;		// If the caster is remort, it will require LESS catalyst
	lvl = (lvl > 19) ? (lvl / 10) : 1;

	// Catalyst is checked in the prespell
	catalyst = use_catalyst(ch,NULL,CATALYST_SOUND,CATALYST_INVENTORY,lvl,TRUE);

	if (IS_AFFECTED2(victim, AFF2_SILENCE)) {
		if (victim == ch)
			send_to_char("You are already silenced.\n\r",ch);
		else
			act("$N is already silenced.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	if (saves_spell(level,victim,DAM_SOUND)) {
		send_to_char("Nothing happens.\n\r", ch);
		return FALSE;
	}

	AFFECT_DATA af;
	memset(&af,0,sizeof(af));
	af.slot	= obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.catalyst_type = -1;
	af.skill = skill;
	af.level = level;
	af.duration = catalyst / lvl;
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = 0;
	af.bitvector2 = AFF2_SILENCE;
	affect_to_char(victim, &af);

	send_to_char("You get the feeling there is a huge sock in your throat.\n\r", victim);

	act("You have been silenced!",victim, NULL, NULL, NULL, NULL,NULL,NULL,TO_CHAR);
	act("$n has been silenced!",victim,NULL, NULL, NULL, NULL, NULL,NULL,TO_ROOM);
	return TRUE;
}

PREBREW_FUNC( prebrew_silence )
{
	ch->catalyst_usage[CATALYST_SOUND] += gsk_silence->values[0];
	return true;
}

BREW_FUNC( brew_silence )
{
	return true;
}

QUAFF_FUNC( quaff_silence )
{
	if (IS_AFFECTED2(ch, AFF2_SILENCE)) {
		send_to_char("You are already silenced.\n\r",ch);
		return false;
	}

	if (saves_spell(level,ch,DAM_SOUND)) {
		send_to_char("Nothing happens.\n\r", ch);
		return false;
	}

	AFFECT_DATA af;
	memset(&af,0,sizeof(af));
	af.slot	= WEAR_NONE;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.catalyst_type = -1;
	af.skill = skill;
	af.level = level;
	af.duration = gsk_silence->values[1];
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = 0;
	af.bitvector2 = AFF2_SILENCE;
	affect_to_char(ch, &af);

	send_to_char("Your throat has gone numb.\n\r", ch);

	act("You have been silenced!",ch, NULL, NULL, NULL, NULL,NULL,NULL,TO_CHAR);
	act("$n has been silenced!",ch,NULL, NULL, NULL, NULL, NULL,NULL,TO_ROOM);
	return true;
}
////////////////////////////////


SPELL_FUNC(spell_vocalize)
{
	char buf[MAX_STRING_LENGTH];
	char speaker[MAX_INPUT_LENGTH];
	char dir[MAX_INPUT_LENGTH];
	int direction;

	if ((direction = parse_direction(dir)) == -1) {
		send_to_char("That's not a direction.", ch);
		return FALSE;
	}

	if (!ch->in_room->exit[direction]) {
		send_to_char("Nothing happens.", ch);
		return FALSE;
	}

	sprintf(buf, "%s says '%s'.\n\r", ch->name, speaker);
	buf[0] = UPPER(buf[0]);
	return FALSE;
}

