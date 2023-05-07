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

bool visit_func_flash (ROOM_INDEX_DATA *room, void *argv[], int argc, int depth, int door)
{
	CHAR_DATA *vch, *vch_next, *ch;
	int level, max_depth;
	AFFECT_DATA af;

	memset(&af,0,sizeof(af));

	ch = (CHAR_DATA *)argv[0];
	max_depth = (int)(size_t)argv[2];
	level = (int)(size_t)argv[1] * (depth + 1) / (max_depth + 1);

	if(door < MAX_DIR) {
		act("{WA blinding light blasts in from nearby.{x", room->people, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL);
	} else {
		act("{W$n forces the light to explode outward in a blinding flash.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		send_to_char("{WYou force the light to explode outward in a blinding flash.{x\n\r", ch);
	}

	af.slot	= WEAR_NONE;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = gsn_blindness;
	af.level = level;
	af.location = APPLY_HITROLL;
	af.modifier = -4;
	af.duration = 1 + level/6; //1+level > 3 ? 3 : 1+level;
	af.bitvector = AFF_BLIND;
	af.bitvector2 = 0;

	for (vch = room->people; vch != NULL; vch = vch_next) {
		vch_next = vch->next_in_room;
		if (!is_safe(ch, vch, FALSE) && vch != ch) {
			if(!IS_AFFECTED(vch, AFF_BLIND) && number_range(0,(int)(size_t)argv[1]-1) < level && !saves_spell(level, vch, DAM_LIGHT)) {
				affect_to_char(vch, &af);
				send_to_char("You are blinded!\n\r", vch);
				act("$n appears to be blinded.",vch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
			}
			if(IS_VAMPIRE(vch) && !IS_IMMORTAL(vch))
				damage_vampires(vch,dice(level,5));
		}
	}

	return TRUE;
}

SPELL_FUNC(spell_flash)
{
	void *argv[3];

	if(both_hands_full(ch)) {
		act("{Y$n summons a sphere of intense light, floating aloft.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		send_to_char("{YYou summon a sphere of intense light, floating aloft.{x\n\r", ch);
	} else {
		act("{Y$n holds a hand aloft, summoning a sphere of intense light.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		send_to_char("{YYou hold a hand aloft, summoning a sphere of intense light.{x\n\r", ch);
	}

	argv[0] = ch;
	argv[1] = (void *)(size_t)level;
	argv[2] = (void *)3;

	visit_rooms(ch->in_room,visit_func_flash,3,argv,3,TRUE);
	return TRUE;
}

SPELL_FUNC(spell_improved_invisibility)
{
	CHAR_DATA *victim;
	AFFECT_DATA af;
	bool perm = FALSE;

	memset(&af,0,sizeof(af));

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = TRUE;
	}

	/* character target */
	victim = (CHAR_DATA *) vo;

	if (IS_AFFECTED(victim, AFF_INVISIBLE) || IS_AFFECTED2(victim, AFF2_IMPROVED_INVIS)) {
		send_to_char("You are already invisible.\n\r", ch);
		return FALSE;
	}

	af.slot = obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = sn;
	af.level = level;
	af.duration = (perm) ? -1 : (level/4 + 3);
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = 0;
	af.bitvector2 = AFF2_IMPROVED_INVIS;
	affect_to_char(victim, &af);

	send_to_char("You fade out of existence.\n\r", victim);
	act("$n fades out of existence.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	return TRUE;
}


SPELL_FUNC(spell_continual_light)
{
        OBJ_DATA *obj;

	obj = (OBJ_DATA *) vo;

	//makes sometihng glow
	if (IS_SET(obj->extra_flags, ITEM_GLOW)) {
		act("$p is already glowing.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	act("$p starts glowing with a bright light.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	act("$n's $p starts glowing with a bright light.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	SET_BIT(obj->extra_flags, ITEM_GLOW);
	return TRUE;
/* will re-enable later (AO)
	light = create_object(obj_index_light_ball, 0, TRUE);
	obj_to_room(light, ch->in_room);
	act("$n twiddles $s thumbs and $p appears.",   ch, NULL, NULL, light, NULL, NULL, NULL, TO_ROOM);
	act("You twiddle your thumbs and $p appears.", ch, NULL, NULL, light, NULL, NULL, NULL, TO_CHAR);
	return TRUE;  */
}







SPELL_FUNC(spell_starflare)
{
	CHAR_DATA *victim;
	CHAR_DATA *vnext;
	int dam;

	act("{YYou raise your hand and summon solar energy!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{Y$n raises $s hand and summons solar energy!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	for (victim = ch->in_room->people; victim && level > 0; victim = vnext) {
		vnext = victim->next_in_room;

		if (victim != ch) {
			if (!is_safe(victim, ch, FALSE) && !is_same_group(victim, ch)) {
				if (!check_spell_deflection(ch, victim, sn))
					continue;

				dam = dice(level, 9);
				if (saves_spell(level, victim, DAM_LIGHT))
					dam /= 3;

				damage(ch, victim, dam, sn, 0, TRUE);
				spell_blindness(gsn_blindness, level, ch, (void *) victim, TARGET_CHAR, WEAR_NONE);

				level -= 4;
			}
		}
	}
	return TRUE;
}



