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
		damage(ch,victim,dam/4,skill,TYPE_UNDEFINED,DAM_SOUND,true);
	} else {
		act("$N screams in pain, covering $S ears.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		damage(ch,victim,dam,skill,TYPE_UNDEFINED,DAM_SOUND,true);
	}
	return true;
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

	CLASS_LEVEL *cl = get_class_level(ch, NULL);
	CLASS_LEVEL *vcl = get_class_level(victim, NULL);
	lvl = (IS_VALID(vcl) ? vcl->level : victim->tot_level) - (IS_VALID(cl) ? cl->level : ch->tot_level);
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

static bool __silence_function(SKILL_DATA *skill, CHAR_DATA *ch, CHAR_DATA *victim, int level, int duration, int slot, char *message)
{
	if (IS_AFFECTED2(victim, AFF2_SILENCE)) {
		if (victim == ch)
			send_to_char("You are already silenced.\n\r",ch);
		else
			act("$N is already silenced.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return false;
	}

	if (saves_spell(level,victim,DAM_SOUND)) {
		send_to_char("Nothing happens.\n\r", ch);
		return false;
	}

	AFFECT_DATA af;
	memset(&af,0,sizeof(af));
	af.slot	= slot;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.catalyst_type = -1;
	af.skill = skill;
	af.level = level;
	af.duration = duration;
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = 0;
	af.bitvector2 = AFF2_SILENCE;
	affect_to_char(victim, &af);

	send_to_char(message, victim);

	act("You have been silenced!",victim, NULL, NULL, NULL, NULL,NULL,NULL,TO_CHAR);
	act("$n has been silenced!",victim,NULL, NULL, NULL, NULL, NULL,NULL,TO_ROOM);
	return true;
}

SPELL_FUNC(spell_silence)
{
	CHAR_DATA *victim;
	int lvl, catalyst;

	/* character target */
	victim = (CHAR_DATA *) vo;

	CLASS_LEVEL *cl = get_class_level(ch, NULL);
	CLASS_LEVEL *vcl = get_class_level(victim, NULL);
	lvl = (IS_VALID(vcl) ? vcl->level : victim->tot_level) - (IS_VALID(cl) ? cl->level : ch->tot_level);
	lvl = (lvl > 19) ? (lvl / 10) : 1;

	// Catalyst is checked in the prespell
	catalyst = use_catalyst(ch,NULL,CATALYST_SOUND,CATALYST_INVENTORY,lvl,true);

	return __silence_function(skill, ch, victim, level, catalyst / lvl, obj_wear_loc, "You get the feeling there is a huge sock in your throat.\n\r");
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
	return __silence_function(skill, ch, ch, level, gsk_silence->values[1], WEAR_NONE, "Your throat has gone numb.\n\r");
}

PRESCRIBE_FUNC( prescribe_silence )
{
	ch->catalyst_usage[CATALYST_SOUND] += gsk_silence->values[0];
	return true;
}

SCRIBE_FUNC( scribe_silence )
{
	return true;
}

RECITE_FUNC( recite_silence )
{
	return __silence_function(skill, ch, (CHAR_DATA *)vo, level, gsk_silence->values[1], WEAR_NONE, "Your throat has gone numb.\n\r");
}


PREINK_FUNC( preink_silence )
{
	ch->catalyst_usage[CATALYST_SOUND] += gsk_silence->values[0];
	return true;
}

INK_FUNC( ink_silence )
{
	return true;
}

TOUCH_FUNC( touch_silence )
{
	return __silence_function(skill, ch, ch, level, gsk_silence->values[1], tattoo->wear_loc, "Your throat has gone numb.\n\r");
}


PREIMBUE_FUNC( preimbue_silence )
{
	ch->catalyst_usage[CATALYST_SOUND] += gsk_silence->values[0];
	return true;
}

IMBUE_FUNC( imbue_silence )
{
	return true;
}

ZAP_FUNC( zap_silence )
{
	return __silence_function(skill, ch, (CHAR_DATA *)vo, level, gsk_silence->values[1], WEAR_NONE, "Your throat has gone numb.\n\r");
}

BRANDISH_FUNC( brandish_silence )
{
	return __silence_function(skill, ch, victim, level, gsk_silence->values[1], WEAR_NONE, "Your throat has gone numb.\n\r");
}

EQUIP_FUNC( equip_silence )
{
	return __silence_function(skill, ch, ch, level, -1, obj->wear_loc, "Your throat has gone numb.\n\r");
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
		return false;
	}

	if (!ch->in_room->exit[direction]) {
		send_to_char("Nothing happens.", ch);
		return false;
	}

	sprintf(buf, "%s says '%s'.\n\r", ch->name, speaker);
	buf[0] = UPPER(buf[0]);
	return false;
}

