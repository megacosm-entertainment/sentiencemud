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

SPELL_FUNC(spell_chill_touch)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	int dam;

	memset(&af,0,sizeof(af));

	dam = level + UMIN(victim->max_hit/15, 100);
	if (!saves_spell(level, victim,DAM_COLD)) {
		act("{C$n turns blue and shivers.{x",victim, NULL, NULL, NULL, NULL,NULL,NULL,TO_ROOM);
		af.where = TO_AFFECTS;
		af.group = AFFGROUP_MAGICAL;
		af.type = sn;
		af.level = level;
		af.duration = 6;
		af.location = APPLY_STR;
		af.modifier = -1;
		af.bitvector = 0;
		af.bitvector2 = 0;
		af.slot	= obj_wear_loc;
		affect_join(victim, &af);
	} else
		dam /= 2;

	damage(ch, victim, dam, sn, DAM_COLD,true);
	return true;
}

SPELL_FUNC(spell_frost_barrier)
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
	else if (is_affected(victim, sn)) {
		if (victim == ch)
			send_to_char("You are already surrounded by a frost barrier.\n\r",ch);
		else
			act("$N is already surrounded by a frost barrier.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return false;
	}

	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = sn;
	af.level = level;
	af.duration = perm ? -1 : (level / 3);
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = 0;
	af.bitvector2 = AFF2_FROST_BARRIER;
	af.slot = obj_wear_loc;
	affect_to_char(victim, &af);
	act("{BYou hear a low-pitched humming sound as the temperature around $n drops.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{BThe temperature of the air around you drops rapidly, forming a frost barrier.{x\n\r", victim);
	return true;
}


SPELL_FUNC(spell_frost_breath)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	CHAR_DATA *vch, *vch_next;
	int dam;

	act("$n breathes out a freezing cone of frost!",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_NOTVICT);
	act("You breath out a cone of frost.",ch,NULL,NULL,NULL,NULL,NULL,NULL,TO_CHAR);

	if (check_shield_block_projectile(ch, victim, "freezing cone of frost", NULL))
		return false;

	act("$n breathes a freezing cone of frost over you!", ch,victim, NULL, NULL, NULL, NULL, NULL,TO_VICT);

	dam = level * 13;

	if (IS_DRAGON(ch))
		dam += dam/4;

	cold_effect(victim->in_room,level,dam/7,TARGET_ROOM);

	for (vch = victim->in_room->people; vch; vch = vch_next) {
		vch_next = vch->next_in_room;

		if (is_safe_spell(ch,vch,true) || is_same_group(ch, vch) || (IS_NPC(vch) && IS_NPC(ch) &&
			(ch->fighting != vch || vch->fighting != ch)))
			continue;

		vch->set_death_type = DEATHTYPE_BREATH;

		if (vch == victim) {
			if (saves_spell(level,vch,DAM_COLD)) {
				cold_effect(vch,level/2,dam/4,TARGET_CHAR);
				damage(ch,vch,dam/2,sn,DAM_COLD,true);
			} else {
				cold_effect(vch,level,dam,TARGET_CHAR);
				damage(ch,vch,dam,sn,DAM_COLD,true);
			}
		} else {
			if (saves_spell(level - 2,vch,DAM_COLD)) {
				cold_effect(vch,level/4,dam/8,TARGET_CHAR);
				damage(ch,vch,dam/4,sn,DAM_COLD,true);
			} else {
				cold_effect(vch,level/2,dam/4,TARGET_CHAR);
				damage(ch,vch,dam/2,sn,DAM_COLD,true);
			}
		}
	}

	return true;
}

SPELL_FUNC(spell_ice_shards)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	if(both_hands_full(ch)) {
		act("{CThe air before you freezes in a flash.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{CThe air before $n freezes in a flash.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		act("{WYou hurl shards of ice at $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{W$n hurls shards of ice at you!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("{W$n hurls shards of ice at $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	} else {

		act("{CThe air around your hand freezes in a flash.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{CThe air around $n's hand freezes in a flash.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		act("{WYou throw out your hand, hurling shards of ice at $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{W$n throw out $s hand, hurling shards of ice at you!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("{W$n throw out $s hand, hurling shards of ice at $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	}

	if (IS_SET(ch->in_room->room2_flags, ROOM_FIRE) || ch->in_room->sector_type == SECT_LAVA) {
		act("{RThe intense heat in the area melts the shards with a sizzle.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL);
		return false;
	}

	if (check_shield_block_projectile(ch, victim, "ice shards", NULL))
		return false;

	level	= UMAX(0, level);
	dam	= dice(level, level/7);

	if (saves_spell(level, victim, DAM_COLD)) dam /= 2;

	/* CAP */
	dam = UMIN(dam, 2500);

	damage(ch,victim,dam,gsn_ice_shards, DAM_COLD, true);

	return true;
}

void spellassist_room_freeze(ROOM_INDEX_DATA *room, CHAR_DATA *ch, int level, int sides, int sn)
{
	CHAR_DATA *victim;
	CHAR_DATA *vnext;
	int dam;

	cold_effect((void *) room, level, dice(level,sides), TARGET_ROOM);
	for (victim = room->people; victim != NULL; victim = vnext) {
		vnext = victim->next_in_room;
		if (!is_safe(ch, victim, false) && victim != ch) {
			dam = dice(level,sides);

			damage(ch, victim, dam, sn, DAM_COLD, true);
			cold_effect((void *)victim, level, dam, TARGET_CHAR);
		}
	}
}

int get_room_heat(ROOM_INDEX_DATA *room, int catalyst)
{
	OBJ_DATA *obj;
	int heat = 0;

	if(room) {
		if (IS_SET(room->room2_flags, ROOM_FIRE)) heat += 25;
		if (room->sector_type == SECT_LAVA) heat += 60;

		for (obj = room->contents; obj != NULL; obj = obj->next_content) {
			if (obj->item_type == ITEM_ROOM_FLAME) heat += 15;
		}

		heat = URANGE(0,heat,100);
		heat = heat * (100 - catalyst*catalyst) / 100;	// If they use catalysts, reduce the heat quadratically
	}

	return heat;
}

struct glacialwave_data {
	int level;
	int catalyst;
	int depth_scale;
	bool large;
	bool do_ice;
};


bool glacialwave_progress(ROOM_INDEX_DATA *room, CHAR_DATA *ch, int depth, int door, void *data )
{
	struct glacialwave_data *gw = (struct glacialwave_data *)data;
	int level, heat, catalyst, i;
	char buf[MSL];

	if(!gw || !room) return true;

	level = gw->level;
	catalyst = gw->catalyst;
	for(i = 0; i < depth; i++) {
		level = level * gw->depth_scale / 100;
		if( catalyst > 0 ) catalyst--;

		if(gw->large) gw->large = number_range(0,9) < catalyst;
	}

	heat = get_room_heat(room, catalyst);

	if( door >= 0 && door < MAX_DIR ) {
		if(gw->large)
			sprintf(buf, "{CAn enormous glacial wave rushes in from the %s!{x\n\r", dir_name[ rev_dir[ door ] ]);
		else
			sprintf(buf, "{CA glacial wave flies in from the %s!{x\n\r", dir_name[ rev_dir[ door ] ]);
		room_echo(room, buf);
	}

	if (gw->large && number_range(0,24) < heat) {
		gw->large = false;	// It takes little to prevent ice storms
		room_echo(room,"{RThe intense heat in the area diminishes the glacial wave significantly.{x");

	} else if (number_percent() < heat) {
		room_echo(room,"{RThe intense heat in the area absorbs the glacial wave.{x");
		return true;
	}

	if (gw->do_ice && number_range(0,24) < heat) gw->do_ice = false;	// It takes little to prevent ice storms

	spellassist_room_freeze(room,ch,level,gw->large?8:5,gsn_glacial_wave);

	if( door >= 0 && door < MAX_DIR ) {
		if(gw->large)
			sprintf(buf, "{CAn enormous glacial wave rushes %sward!{x\n\r", dir_name[ door ]);
		else
			sprintf(buf, "{CA glacial wave flies %sward!{x\n\r", dir_name[ door ]);
		room_echo(room, buf);
	}


	return false;
}

void glacialwave_end(ROOM_INDEX_DATA *room, CHAR_DATA *ch, int depth, int door, void *data, bool canceled )
{
	struct glacialwave_data *gw = (struct glacialwave_data *)data;
	int level, catalyst, i;
	char buf[MSL];

	if(!gw || !room) return;

	level = gw->level;
	catalyst = gw->catalyst;
	for(i = 0; i < depth; i++) {
		level = level * gw->depth_scale / 100;
		if( catalyst > 0 ) catalyst--;

		if(gw->large) gw->large = number_range(0,9) < catalyst;
	}

	if(gw->large)
		sprintf(buf, "{CAn enormous glacial wave explodes in a fury of ice!{x\n\r");
	else
		sprintf(buf, "{CA glacial wave explodes in a fury of ice!{x\n\r");
	room_echo(room, buf);

	if(gw->large) spellassist_room_freeze(room,ch,level,5,gsn_glacial_wave);

}


SPELL_FUNC(spell_glacial_wave)
{
	struct glacialwave_data data;
	OBJ_DATA *obj;
	char *arg = (char *) vo;
	int depth_scale;
	int max_depth;
	int door;
	int catalyst, i;
	bool do_ice, large = false;

	if (!arg) return false;

	if (arg[0] == '\0') {
		send_to_char("Cast it in which direction?\n\r", ch);
		return false;
	}

	if ((door = parse_direction(arg)) == -1) {
		send_to_char("That's not a direction.\n\r", ch);
		return false;
	}
/*
	if (!ch->in_room->exit[door] || !ch->in_room->exit[door]->u1.to_room || (IS_SET(ch->in_room->exit[door]->exit_info,EX_HIDDEN) && !IS_SET(ch->in_room->exit[door]->exit_info,EX_FOUND)) || IS_SET(ch->in_room->exit[door]->exit_info,EX_CLOSED)) {
		send_to_char("You need an open direction.\n\r", ch);
		return false;
	}
*/
	if(IS_NPC(ch) && ch->pIndexData->vnum == MOB_VNUM_OBJCASTER) {	// non-mob caster
		max_depth = 5;
		depth_scale = 50;
		do_ice = false;
		catalyst = 0;
	} else {
		catalyst = use_catalyst(ch,NULL,CATALYST_ICE,CATALYST_ROOM|CATALYST_HOLD|CATALYST_ACTIVE,10,1,CATALYST_MAXSTRENGTH,true);

		// If they have an ice catalyst...
		if(catalyst > 0) {
			level = level * number_range(20,20+5*catalyst)/10;
			max_depth = 6 + number_range(0,catalyst)/3;
			for(depth_scale = 50, i=0;i<catalyst;i++, depth_scale = (2099 + 80 * depth_scale) / 100);
			do_ice = true;		// Enable ice storms
			large = number_range(0,9) < catalyst;

		// If not...
		} else {
			max_depth = 5;
			depth_scale = 50;
			do_ice = false;
		}

		act("{BA great cold envelops the hands of $n as the air rapidly cools to well past freezing.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		send_to_char("{BA great cold envelops your hands as the air rapidly cools to well past freezing.{x\n\r", ch);

		// Affect all eq at these locations
		if((obj = get_eq_char(ch, WEAR_HANDS))) cold_effect(obj,level,level,TARGET_OBJ);
		if((obj = get_eq_char(ch, WEAR_WIELD))) cold_effect(obj,level,level,TARGET_OBJ);
		if((obj = get_eq_char(ch, WEAR_HOLD))) cold_effect(obj,level,level,TARGET_OBJ);
		if((obj = get_eq_char(ch, WEAR_SHIELD))) cold_effect(obj,level,level,TARGET_OBJ);
		if((obj = get_eq_char(ch, WEAR_SECONDARY))) cold_effect(obj,level,level,TARGET_OBJ);

		act("{C$n gathers cold energy until $e hurls it $tward as a glacial wave.{x", ch, NULL, NULL, NULL, NULL, dir_name[door], NULL, TO_ROOM);
		act("{CYou gather cold energy until you hurl it $tward as a glacial wave.{x", ch, NULL, NULL, NULL, NULL, dir_name[door], NULL, TO_CHAR);
	}

	data.level = level;
	data.catalyst = catalyst;
	data.depth_scale = depth_scale;
	data.do_ice = do_ice;
	data.large = large;

	if( !glacialwave_progress(ch->in_room, ch, 0, -1, &data ) )
		visit_room_direction(ch, ch->in_room, max_depth, door, &data, glacialwave_progress, glacialwave_end);

	return true;
}

SPELL_FUNC(spell_ice_storm)
{
	OBJ_DATA *obj;
	CHAR_DATA *vch;
	ROOM_INDEX_DATA *room;


	if (IS_SET(ch->in_room->room2_flags, ROOM_FIRE)) {
		send_to_char("The intense heat in the area melts your ice storm as soon as it appears.\n\r", ch);
		act("$n summons an ice storm, but it melts instantly.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		return false;
	}

	room = ch->in_room;
	if (room->sector_type == SECT_AIR || room->sector_type == SECT_NETHERWORLD) {
		send_to_char("You can't seem to summon the powers of ice here.\n\r", ch);
		return false;
	}

	obj = create_object(get_obj_index(OBJ_VNUM_ICE_STORM), 0, true);
	act("{BYou summon a huge ice storm!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{B$n summons a huge ice storm!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	obj_to_room(obj, ch->in_room);
	cold_effect(ch->in_room, level, dice(4,8), TARGET_ROOM);
	for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room) {
		if (!is_safe_spell(ch, vch, true))
			cold_effect(vch, level/2, dice(4,8), TARGET_CHAR);
	}

	obj->timer = UMAX(level/30, 1);

	return true;
}

