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

struct afterburn_data {
	int level;
	int decay;
};

void afterburn_hitroom(ROOM_INDEX_DATA *room, CHAR_DATA *ch, int dam, int level)
{
	CHAR_DATA *victim;
	CHAR_DATA *vnext;

	// Maybe add a chance for creating an inferno?

	fire_effect((void *) room, level, dam, TARGET_ROOM);
	for (victim = room->people; victim ; victim = vnext) {
		vnext = victim->next_in_room;
		if (!is_safe(ch, victim, FALSE) && victim != ch) {
			if (!check_spell_deflection(ch, victim, gsk_afterburn, NULL))
				continue;

			damage(ch, victim, dam, gsk_afterburn, TYPE_UNDEFINED, DAM_FIRE, TRUE);
			fire_effect((void *)victim, level, dam, TARGET_CHAR);
		}
	}


}

bool afterburn_progress(ROOM_INDEX_DATA *room, CHAR_DATA *ch, int depth, int door, void *data )
{
	struct afterburn_data *afdata = (struct afterburn_data *)data;
	int level, dam;
	char buf[MSL];

	if(!afdata || !room) return TRUE;

	level = afdata->level - afdata->decay * depth;
	if( level < 1 ) level = 1;

	dam = dice(level, 5);

	if( door >= 0 && door < MAX_DIR ) {
		sprintf(buf, "{RA scorching fireball flies in from the %s!{x\n\r", dir_name[ rev_dir[ door ] ]);
		room_echo(room, buf);
		printf_to_char(ch, "%d] {RA scorching fireball flies in from the %s!{x\n\r", depth, dir_name[ rev_dir[ door ] ]);
	}

	afterburn_hitroom(room, ch, dam, level);

	if( door >= 0 && door < MAX_DIR ) {
		sprintf(buf, "{RA scorching fireball flies %sward!{x\n\r",dir_name[ door ]);
		room_echo(room, buf);
		printf_to_char(ch, "%d] {RA scorching fireball flies %sward!{x\n\r", depth, dir_name[ door ]);
	}

	return FALSE;
}

void afterburn_end(ROOM_INDEX_DATA *room, CHAR_DATA *ch, int depth, int door, void *data, bool canceled )
{
	if(!room) return;

	room_echo(room, "{RA scorching fireball explodes in a roaring inferno!{x\n\r");
	printf_to_char(ch, "%d] {RA scorching fireball explodes in a roaring inferno!{x\n\r", depth);
}

SPELL_FUNC(spell_afterburn)
{
	char *arg = (char *) vo;
	int max_depth;
	int door;
	struct afterburn_data data;

	if (!arg) return FALSE;

	if (!arg[0]) {
		send_to_char("Cast it in which direction?\n\r", ch);
		return FALSE;
	}

	if ((door = parse_direction(arg)) < 0) {
		send_to_char("That's not a direction.\n\r", ch);
		return FALSE;
	}

	act("{RA scorching fireball erupts from your hands and flies $tward!{x", ch, NULL, NULL, NULL, NULL, dir_name[door], NULL, TO_CHAR);
	act("{RA scorching fireball erupts from $n's hands and flies $tward!{x", ch, NULL, NULL, NULL, NULL, dir_name[door], NULL, TO_ROOM);

	max_depth = 5;

	data.level = level;
	data.decay = 6 - get_skill(ch, skill) / 20;

	if( !afterburn_progress(ch->in_room, ch, 0, -1, &data) )
		visit_room_direction(ch, ch->in_room, max_depth, door, &data, afterburn_progress, afterburn_end);

#if 0

	dam = dice(level, 5);
	fire_effect((void *) ch->in_room, level, dam, TARGET_ROOM);

	/* hurt people in the room first */
	for (victim = ch->in_room->people; victim ; victim = vnext) {
		vnext = victim->next_in_room;
		if (!is_safe(ch, victim, FALSE) && victim != ch) {
			if (!check_spell_deflection(ch, victim, sn))
				continue;

			damage(ch, victim, dam, sn, DAM_FIRE, TRUE);
			fire_effect((void *)victim, level, dam, TARGET_CHAR);
		}
	}

	to_room = ch->in_room;
	depth = 0;
	for (exit = ch->in_room->exit[door]; exit; exit = to_room->exit[door]) {
		dam = dice(level, 5);
		if (IS_SET(exit->exit_info, EX_CLOSED) ||
			depth >= max_depth ||
			!to_room->exit[door]) {
			sprintf(buf, "{RA scorching fireball explodes in a roaring inferno!{x\n\r");
			room_echo(to_room, buf);
			break;
		}

		to_room = exit->u1.to_room;
		sprintf(buf, "{RA scorching fireball flies in from the %s!{x\n\r", dir_name[ rev_dir[ door ] ]);
		room_echo(to_room, buf);

		fire_effect((void *) to_room, level, dam, TARGET_ROOM);
		for (victim = to_room->people; victim ; victim = vnext) {
			vnext = victim->next_in_room;
			if (!is_safe(ch, victim, FALSE) && victim != ch) {
				if (!check_spell_deflection(ch, victim, sn))
					continue;

				damage(ch, victim, dam,sn, DAM_FIRE, TRUE);
				fire_effect((void *)victim, level, dam, TARGET_CHAR);
			}
		}

		level = UMAX(level - 5, 1); // strength fades as it goes more rooms
		depth++;

		sprintf(buf, "{RA scorching fireball flies %sward!{x\n\r",dir_name[ door ]);
		room_echo(to_room, buf);
	}
#endif
	return TRUE;
}


SPELL_FUNC(spell_burning_hands)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	dam = dice(level, 6) + dice(2, level / 10);

	if (saves_spell(level, victim,DAM_FIRE)) dam /= 2;

	dam = UMIN(dam, 2500);

	damage(ch, victim, dam, skill, TYPE_UNDEFINED, DAM_FIRE,TRUE);
	return TRUE;
}

SPELL_FUNC(spell_fire_barrier)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = FALSE;
	memset(&af,0,sizeof(af));

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = TRUE;
	}

	if (perm && is_affected(victim, skill))
		affect_strip(victim, skill);
	else if (is_affected(victim, skill)) {
		if (victim == ch)
			send_to_char("You are already surrounded by a fire barrier.\n\r",ch);
		else
			act("$N is already surrounded by a fire barrier.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	af.slot = obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level;
	af.duration = perm ? -1 : (level / 3);
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = 0;
	af.bitvector2 = AFF2_FIRE_BARRIER;
	affect_to_char(victim, &af);
	act("{RA barrier fire blooms around $n, defending $m from harm.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{RA barrier of fire blooms around you.{x\n\r", victim);
	return TRUE;
}


SPELL_FUNC(spell_fire_breath)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	CHAR_DATA *vch, *vch_next;
	int dam;

	act("$n breathes forth a cone of fire.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_NOTVICT);
	act("$n breathes a cone of hot fire over you!",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_VICT);
	act("You breathe forth a cone of fire.",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_CHAR);

	if (check_shield_block_projectile(ch, victim, "cone of fire", NULL))
		return FALSE;

	dam = level * 12;

	if (IS_DRAGON(ch))
		dam += dam/4;

	fire_effect(victim->in_room,level,dam/6,TARGET_ROOM);

	for (vch = victim->in_room->people; vch; vch = vch_next) {
		vch_next = vch->next_in_room;

		if (is_safe_spell(ch, vch, TRUE) || is_same_group(ch, vch) || (IS_NPC(vch) && IS_NPC(ch) &&
			(ch->fighting != vch || vch->fighting != ch)))
			continue;

		vch->set_death_type = DEATHTYPE_BREATH;

		if (vch == victim) {
			if (saves_spell(level,vch,DAM_FIRE)) {
				fire_effect(vch,level/2,dam/4,TARGET_CHAR);
				damage(ch,vch,dam/2,skill,TYPE_UNDEFINED,DAM_FIRE,TRUE);
			} else	{
				fire_effect(vch,level,dam,TARGET_CHAR);
				damage(ch,vch,dam,skill,TYPE_UNDEFINED,DAM_FIRE,TRUE);
			}
		} else {
			if (saves_spell(level - 2,vch,DAM_FIRE)) {
				fire_effect(vch,level/4,dam/8,TARGET_CHAR);
				damage(ch,vch,dam/4,skill,TYPE_UNDEFINED,DAM_FIRE,TRUE);
			} else {
				fire_effect(vch,level/2,dam/4,TARGET_CHAR);
				damage(ch,vch,dam/2,skill,TYPE_UNDEFINED,DAM_FIRE,TRUE);
			}
		}
	}
	return TRUE;
}


SPELL_FUNC(spell_fire_cloud)
{
	OBJ_DATA *fire_cloud;
	OBJ_DATA *obj;
	ROOM_INDEX_DATA *room;
	int dir = 0;
	bool exists = FALSE;

	if (!obj_index_inferno) {
		bug("spell_fire_cloud: null obj_index!\n", 0);
		return FALSE;
	}

	act("{RA cloud of burning fire erupts from $n's mouth setting light to everything!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{RYou breathe forth a massive cloud of fire!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	for (obj = ch->in_room->contents; obj; obj = obj->next_content) {
		if (obj->item_type == ITEM_ROOM_FLAME) {
			exists = TRUE;
			break;
		}
	}

	if (!exists) {
		fire_cloud = create_object(obj_index_inferno, 0, TRUE);
		fire_cloud->timer = 4;
		obj_to_room(fire_cloud, ch->in_room);
	}

	echo_around(ch->in_room, "{RA huge fireball flies into the room setting light to everything!{x\n\r");

	exists = FALSE;
	for (dir = 0; dir < MAX_DIR; dir++) {
		if (!ch->in_room->exit[dir]) continue;

		if (IS_SET(ch->in_room->exit[dir]->exit_info, EX_CLOSED)) continue;

		if(!(room = exit_destination(ch->in_room->exit[dir]))) continue;

		for (obj = room->contents; obj != NULL; obj = obj->next_content) {
			if (obj->item_type == ITEM_ROOM_FLAME) {
				exists = TRUE;
				break;
			}
		}

		if (!exists) {
			fire_cloud = create_object(obj_index_inferno, 0, TRUE);
			fire_cloud->timer = 4;
			obj_to_room(fire_cloud, room);
		}
	}

	return TRUE;
}


SPELL_FUNC(spell_fireball)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	act("{RA large fireball shoots from the palms of your hands.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{RA large fireball flies from $n's hands toward $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	act("{RA large fireball flies from $n's hands towards you!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);

	if (check_shield_block_projectile(ch, victim, "fireball", NULL))
		return FALSE;

	level = UMAX(0, level);
	dam = dice(level, level/7);

	if (saves_spell(level, victim, DAM_FIRE))
		dam /= 2;

	/* CAP */
	dam = UMIN(dam, 2500);

	damage(ch,victim,dam,skill, TYPE_UNDEFINED, DAM_FIRE, TRUE);
	// fire_effect(victim, level, dam, target);
	return TRUE;
}


SPELL_FUNC(spell_fireproof)
{
	OBJ_DATA *obj = (OBJ_DATA *) vo;
	AFFECT_DATA af;
	memset(&af,0,sizeof(af));

	if (IS_OBJ_STAT(obj,ITEM_BURN_PROOF)) {
		act("$p is already protected from burning.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
		return FALSE;
	}

	if (obj->item_type == ITEM_SCROLL || (IS_FLUID_CON(obj) && list_size(FLUID_CON(obj)->spells) > 0)) {
		act("$p is already invested with enough magic. You can't make it fireproof.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	af.slot	= WEAR_NONE;
	af.where = TO_OBJECT;
	af.group = AFFGROUP_ENCHANT;
	af.skill = skill;
	af.level = level;
	af.duration = number_fuzzy(level / 4);
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = ITEM_BURN_PROOF;
	af.bitvector2 = 0;

	affect_to_obj(obj,&af);

	act("You protect $p from fire.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
	act("$p is surrounded by a protective aura.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM);
	return TRUE;
}

SPELL_FUNC(spell_flamestrike)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	act("{RA large stream of fire shoots from your fingertips toward $N.{x",ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{RA large stream of fire shoots from $n's fingertips toward $N.{x",ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	act("{RA large stream of fire shoots from $n's fingertips toward you.{x",ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);

	if (check_shield_block_projectile(ch, victim, "flamestrike", NULL))
		return FALSE;

	dam = dice(level/3 + 2, level/3 - 2);

	dam = (dam * get_skill(ch, skill))/100;

	damage(ch, victim, dam, skill,TYPE_UNDEFINED, DAM_FIRE ,TRUE);
	return TRUE;
}


SPELL_FUNC(spell_inferno)
{
	OBJ_DATA *inferno;
	OBJ_DATA *obj;

	for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
		if (obj->item_type == ITEM_ROOM_FLAME) {
			send_to_char("The room is already a raging inferno.\n\r", ch);
			return FALSE;
		}

	inferno = create_object(obj_index_inferno, 0, TRUE);
	inferno->timer = 4;
	obj_to_room(inferno, ch->in_room);
	act("With a whisper, the room is ablaze with the burning fires of Hell!",   ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL);
	return TRUE;
}

SPELL_FUNC(spell_hell_forge)
{
	return TRUE;
}

SPELL_FUNC(spell_magma_flow)
{
	return TRUE;
}
