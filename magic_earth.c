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

SPELL_FUNC(spell_earth_walk)
{
	char buf[MIL];
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int distance, catalyst;
	int t1, t2, s1, s2;

	if (victim == NULL) {
		send_to_char("Nobody like that is around.\n\r", ch);
		return false;
	}

	if (IS_NPC(victim)) {
		send_to_char("The earth seems impenetrable.\n\r", ch);
		return false;
	}

	if (victim == ch) {
		send_to_char("What would be the point?\n\r", ch);
		return false;
	}

	if (!victim || !victim->in_room) {
		send_to_char("They aren't anywhere in the world.\n\r", ch);

		sprintf(buf, "spell_earth_walk: %s tried to earth walk to %s who had null in_room!",
			ch->name, (victim == NULL) ? "nobody???" : victim->name);
		bug(buf, 0);
		return false;
	}

	if (ch->in_room == victim->in_room) {
		send_to_char("What would be the point?\n\r", ch);
		return false;
	}

	if (MOUNTED(ch)) {
		send_to_char("Earth walking is a solitary action.  Dismount before trying again.\n\r", ch);
		return false;
	}

#if 0
	sprintf(buf,"victim: %08X\n\r", victim);
	send_to_char(buf,ch);
	if(victim) {
		sprintf(buf,"victim->in_room: %08X\n\r", victim->in_room);
		send_to_char(buf,ch);
	}
	sprintf(buf,"ch->in_room: %08X\n\r", ch->in_room);
	send_to_char(buf,ch);
	return;
#endif

	t1 = ch->in_room->sector_type;
	t2 = victim->in_room->sector_type;

#if 0
	sprintf(buf,"t1: %d\n\r", t1);
	send_to_char(buf,ch);
	sprintf(buf,"t2: %d\n\r", t2);
	send_to_char(buf,ch);
	return;
#endif

	if (!str_cmp(ch->in_room->area->name, "Netherworld") ||
		!str_cmp(ch->in_room->area->name, "Eden") ||
		!str_cmp(victim->in_room->area->name, "Netherworld") ||
		!str_cmp(victim->in_room->area->name, "Eden") ||
		!str_cmp(ch->in_room->area->name, "Arena") ||
		!str_cmp(victim->in_room->area->name, "Arena") ||
		!str_prefix("Church Temples", ch->in_room->area->name) ||
		!str_prefix("Church Temples", victim->in_room->area->name) ||
		(ch->in_room->area->place_flags == PLACE_NOWHERE) ||
		(victim->in_room->area->place_flags == PLACE_NOWHERE) ||
		(ch->in_room->area->place_flags == PLACE_OTHER_PLANE) ||
		(victim->in_room->area->place_flags == PLACE_OTHER_PLANE) ||
		IS_SET(victim->in_room->area->area_flags, AREA_NO_RECALL)) {
		send_to_char("Outside interference stops your earth walk.\n\r", ch);
		return false;
	}

	if (IS_SET(victim->in_room->room_flags, ROOM_NO_RECALL) ||
		IS_SET(victim->in_room->room_flags, ROOM_NOMAGIC) ||
		IS_SET(victim->in_room->room_flags, ROOM_CPK)) {
		send_to_char("That room is protected from gating magic.\n\r", ch);
		return false;
	}

	switch(t1) {
	case SECT_FOREST:
	case SECT_JUNGLE:
	case SECT_ENCHANTED_FOREST:
	case SECT_BRAMBLE:
		s1 = 1; break;
	case SECT_DESERT:
		s1 = 2; break;
	case SECT_HILLS:
	case SECT_MOUNTAIN:
	case SECT_CAVE:
		s1 = 3; break;
	case SECT_SWAMP:
		s1 = 4; break;
	case SECT_FIELD:
		s1 = 5; break;
	default:
		send_to_char("You aren't near earthen terrain.\n\r", ch);
		return false;
	}

	switch(t2) {
	case SECT_FOREST:
	case SECT_JUNGLE:
	case SECT_ENCHANTED_FOREST:
	case SECT_BRAMBLE:
		s2 = 1; break;
	case SECT_DESERT:
		s2 = 2; break;
	case SECT_HILLS:
	case SECT_MOUNTAIN:
	case SECT_CAVE:
		s2 = 3; break;
	case SECT_SWAMP:
		s2 = 4; break;
	case SECT_FIELD:
		s2 = 5; break;
	default:
		send_to_char("You can't find earthen terrain to lock onto.\n\r", ch);
		return false;
	}


	// Requires a catalyst if different types of terrain
	if(s1 != s2) {
		// Add distance calculations
		distance = 1;

		catalyst = has_catalyst(ch,NULL,CATALYST_NATURE,CATALYST_HERE|CATALYST_ACTIVE,1,CATALYST_MAXSTRENGTH);
		if(catalyst >= 0 && catalyst < distance) {
			send_to_char("You appear to be missing a required natural catalyst.\n\r", ch);
			return false;
		}

		catalyst = use_catalyst(ch,NULL,CATALYST_NATURE,CATALYST_INVENTORY|CATALYST_ACTIVE,distance,1,CATALYST_MAXSTRENGTH,true);
	} else
		catalyst = 0;

	switch(s1) {
	case 1:	// Forest/Wooded
		act("{g$n fades into the brush.{x",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		send_to_char("{gYou fade into the brush.\n\r{x",ch);
		break;
	case 2:	// Desert
		act("{y$n descends into the sand.{x",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		send_to_char("{yYou descend into the sand below.\n\r{x",ch);
		break;
	case 3:	// Mountainous
		act("{x$n walks into nearby rock.{x",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		send_to_char("{xYou pass through the surface of a nearby rock.\n\r{x",ch);
		break;
	case 4:	// Swamp
		act("{g$n descends into the murky swamp.{x",ch,NULL, NULL, NULL, NULL, NULL,NULL,TO_ROOM);
		send_to_char("{gYou descend into the murky swamp.\n\r{x",ch);
		break;
	case 5:	// Field
		act("{g$n disappears into the grass.{x",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		send_to_char("{gYou disappear into the grass.\n\r{x",ch);
		break;
	}

	char_from_room(ch);
	char_to_room(ch,victim->in_room);

	act("\n\r{WYou feel your spirit rush toward $N through the earth...{x\n\r",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);

	switch(s2) {
	case 1:	// Forest/Wooded
		act("{G$n steps out from the brush.{x",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		send_to_char("{gYou step out from the brush.\n\r{x",ch);
		break;
	case 2:	// Desert
		act("{Y$n rises from the sand dunes.{x",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		send_to_char("{YYou arise from the sand.\n\r{x",ch);
		break;
	case 3:	// Mountainous
		act("{x$n emerges from a nearby rock.{x",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		send_to_char("{xYou pass through the surface of a nearby rock.\n\r{x",ch);
		break;
	case 4:	// Swamp
		act("{G$n emerges from the murky depths of the swamp.{x",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		send_to_char("{gYou emerge from the murky depths of the swamp.\n\r{x",ch);
		break;
	case 5:	// Field
		act("{g$n rises from the grass.{x",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		send_to_char("{gYou arise from the grass.\n\r{x",ch);
		break;
	}

	do_function(ch, &do_look, "auto");
	return true;
}



SPELL_FUNC(spell_earthquake)
{
	CHAR_DATA *vch;
	CHAR_DATA *vch_next;

	send_to_char("{YThe earth trembles beneath your feet!{x\n\r", ch);
	act("{Y$n makes the earth tremble and shiver.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	for (vch = ch->in_room->people; vch; vch = vch_next) {
		vch_next = vch->next_in_room;

		if (vch->in_room == ch->in_room) {
			if (vch != ch && !is_same_group(ch, vch)) {
				if (IS_AFFECTED(vch,AFF_FLYING))
					damage(ch,vch,0,sn,DAM_BASH,true);
				else
					damage(ch,vch,level + dice(2, 8), sn, DAM_BASH,true);
			}

			continue;
		}
	}
	return true;
}


SPELL_FUNC(spell_giant_strength)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = false;
	memset(&af,0,sizeof(af));

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = true;
	}

	if (perm && is_affected(victim, sn)) {
		affect_strip(victim, sn);
	} else if (is_affected(victim, sn)) {
		if (victim == ch)
			send_to_char("You are already as strong as you can get!\n\r",ch);
		else
			act("$N can't get any stronger.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = sn;
	af.level = level;
	af.duration = perm ? -1 : level;
	af.location = APPLY_STR;
	af.modifier = 1 + (level >= 18) + (level >= 25) + (level >= 32);
	af.bitvector = 0;
	af.bitvector2 = 0;
	af.slot = obj_wear_loc;
	affect_to_char(victim, &af);

	send_to_char("Your muscles surge with heightened power!\n\r", victim);
	act("$n's muscles surge with heightened power.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);

	return true;
}

SPELL_FUNC(spell_stone_skin)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = false;
	memset(&af,0,sizeof(af));

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = true;
	}

	if (perm && is_affected(victim, sn))
		affect_strip(victim, sn);
	else if (is_affected(ch, sn)) {
		if (victim == ch)
			send_to_char("Your skin is already as hard as a rock.\n\r",ch);
		else
			act("$N's skin is already as hard as can be.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = sn;
	af.level = level;
	af.duration = perm?-1:level;
	af.location = APPLY_AC;
	af.modifier = -30 - (level/2);
	af.bitvector = 0;
	af.bitvector2 = AFF2_STONE_SKIN;
	af.slot = obj_wear_loc;
	affect_to_char(victim, &af);
	act("$n's skin turns to stone.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("Your skin turns to stone.\n\r", victim);
	return true;
}


SPELL_FUNC(spell_stone_spikes)
{
	EXIT_DATA *pExit;
	CHAR_DATA *victim, *victim_next;
	int door;
	ROOM_INDEX_DATA *to_room;

	if (ch->in_room->sector_type == SECT_AIR ||
		ch->in_room->sector_type == SECT_NETHERWORLD ||
		ch->in_room->sector_type == SECT_WATER_SWIM ||
		ch->in_room->sector_type == SECT_WATER_NOSWIM) {
		send_to_char("You fail to invoke your elemental magic here.\n\r", ch);
		return false;
	}

	act("Three huge stone spikes jut up from the ground!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	for (victim = ch->in_room->people; victim != NULL; victim = victim_next) {
		victim_next = victim->next_in_room;

		if (!is_safe(ch, victim, false) && ch != victim) {
			if (number_percent() < get_curr_stat(victim, STAT_DEX)) {
				send_to_char("You dodge the spikes!\n\r", victim);
				act("$n dodges the spikes!", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			} else {
				damage(ch, victim, dice(level/4, 8), sn, DAM_PIERCE, true);
				affect_strip(victim, gsn_sneak);
				REMOVE_BIT(victim->affected_by, AFF_HIDE);
				REMOVE_BIT(victim->affected_by, AFF_SNEAK);
			}
		}
	}

	for (door=0; door < MAX_DIR; door++) {
		if ((pExit = ch->in_room->exit[door]) &&
			(to_room = pExit->u1.to_room) &&
			!(to_room->sector_type == SECT_AIR ||
			to_room->sector_type == SECT_NETHERWORLD ||
			to_room->sector_type == SECT_WATER_SWIM ||
			to_room->sector_type == SECT_WATER_NOSWIM)) {
			room_echo(to_room,"Three huge stone spikes jut up from the ground!\n\r");
			for (victim = to_room->people; victim; victim = victim_next) {
				victim_next = victim->next_in_room;

				if (!is_safe(ch, victim, false)) {
					if (number_percent() < get_curr_stat(victim, STAT_DEX)) {
						send_to_char("You dodge the spikes!\n\r", victim);
						act("$n dodges the spikes!", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					} else {
						damage(ch, victim, dice(level/4, 8), sn, DAM_PIERCE, true);
						affect_strip(victim, gsn_sneak);
						REMOVE_BIT(victim->affected_by, AFF_HIDE);
						REMOVE_BIT(victim->affected_by, AFF_SNEAK);
					}
				}
			}
		}
	}
	return true;
}

SPELL_FUNC(spell_stone_touch)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	memset(&af,0,sizeof(af));

	// Is an obj
	if (level > 9000) {
		level -= 9000;
	} else {
		// Check for reagent
	}

	if (is_affected(ch, sn)) {
		if (victim == ch)
			send_to_char("You are already a statue.\n\r",ch);
		else
			act("$N is already a statue.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	af.slot	= obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = sn;
	af.level = level;
	af.duration = level;
	af.location = APPLY_AC;
	af.modifier  = -40;
	af.bitvector = 0;
	af.bitvector2 = AFF2_STONE_SKIN;
	affect_to_char(victim, &af);

	af.duration = 3*level/4;
	af.location = APPLY_NONE;
	af.modifier  = 0;
	af.bitvector = 0;
	af.bitvector2 = AFF2_IMMOBILE|AFF2_PROTECTED;
	affect_to_char(victim, &af);
	act("$n's entire body turns to stone, becoming immobilized like a statue.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("Your entire body turns to stone, leaving you immobilized like a statue.\n\r", victim);
	return true;
}
