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


SPELL_FUNC(spell_animate_dead)
{
	CHAR_DATA *victim;
	MOB_INDEX_DATA *index;
	char buf[MAX_STRING_LENGTH];
	OBJ_DATA *obj;
	OBJ_DATA *obj_in, *obj_in_next;
	int i,chance, corpse, catalyst, lvl, percent;
	long vnum;

	if (target == TARGET_OBJ) {
		obj = (OBJ_DATA *) vo;

		bool keep_mob = true;
		bool restring_mob = true;

		if (obj->item_type == ITEM_CORPSE_PC) {
			send_to_char("Player corpses cannot be animated.\n\r", ch);
			return false;
		}

		if (obj->item_type != ITEM_CORPSE_NPC) {
			send_to_char("Nothing happens.\n\r", ch);
			return false;
		}

		if (IS_SET(obj->extra[2], ITEM_NO_ANIMATE)) {
			act("$p seems to be immune to your necromantic magic.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			return false;
		}

		corpse = CORPSE_TYPE(obj);

		if (!IS_SET(CORPSE_PARTS(obj),PART_HEAD) && !corpse_info_table[corpse].animate_headless) {
			act("Your magic is not powerful enough.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			return false;
		}

		catalyst = has_catalyst(ch,NULL,CATALYST_DEATH,CATALYST_CARRY|CATALYST_ROOM|CATALYST_ACTIVE,1,CATALYST_MAXSTRENGTH);
		if(catalyst < 0 || catalyst > 4) catalyst = 4;
		if(catalyst) use_catalyst(ch,NULL,CATALYST_DEATH,CATALYST_CARRY|CATALYST_ROOM|CATALYST_ACTIVE,catalyst,1,CATALYST_MAXSTRENGTH,true);

		chance = obj->condition * CORPSE_ANIMATE(obj);
		lvl = ch->tot_level / 2;
		percent = 50;
		for(i=0;i < catalyst; i++) {
			lvl = (ch->tot_level + level + 1) / 2;
			percent = (percent + 101) / 2;
			chance = (20002 + chance) / 3;
		}

		if(number_range(1,10000) > chance) {
			act("Your magic is not powerful enough.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			return false;
		}

		if (obj->level > lvl) {
			act("Your magic is not powerful enough.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			return false;
		}

		vnum = CORPSE_MOBILE(obj) ? CORPSE_MOBILE(obj) : obj->orig_vnum;

		index = get_mob_index(vnum);
		victim = create_mobile(index, false);

		// Regardless what wealth the normal mob has...
		victim->gold = 0;
		victim->silver = 0;

		// Do the animate trigger now so that information that might be needed for PREANIMATE is available.
		if( p_percent_trigger( victim, NULL, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_ANIMATE, NULL) )
			restring_mob = false;

		keep_mob = true;
		// Check the victim if it can be resurrected directly
		if (p_percent_trigger( victim, NULL, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_PREANIMATE, NULL) )
			keep_mob = false;

		// Check the corpse for anything blocking the resurrection
		if (keep_mob && p_percent_trigger( NULL, obj, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_PREANIMATE, NULL) )
			keep_mob = false;

		// Check the ROOM the corpse is in for anything blocking resurrection
		if (keep_mob && p_percent_trigger( NULL, NULL, ch->in_room, NULL, ch, victim, NULL, obj, NULL, TRIG_PREANIMATE, NULL) )
			keep_mob = false;

		if( !keep_mob )
		{
			extract_char(victim, false);
			return false;
		}


		victim->max_hit = percent * victim->max_hit / 100;
		victim->max_mana = percent * victim->max_mana / 100;
		victim->max_move = percent * victim->max_move / 100;
		victim->hit = percent * victim->hit / 100;
		victim->mana = percent * victim->mana / 100;
		victim->move = percent * victim->move / 100;

		if( restring_mob )
		{
			if(corpse_info_table[corpse].animate_name) {
				sprintf(buf, corpse_info_table[corpse].animate_name, victim->name);
				free_string(victim->name);
				victim->name = str_dup(buf);
			}

			if(corpse_info_table[corpse].animate_long) {
				sprintf(buf, corpse_info_table[corpse].animate_long, victim->short_descr);
				free_string(victim->long_descr);
				victim->long_descr = str_dup(buf);
			}

			if(corpse_info_table[corpse].animate_descr) {
				free_string(victim->description);
				victim->description = str_dup(corpse_info_table[corpse].animate_descr);
			}
		}

		victim->comm = COMM_NOTELL|COMM_NOCHANNELS;
		SET_BIT(victim->affected_by[0], AFF_CHARM);
		SET_BIT(victim->act[0], ACT_ANIMATED);
		SET_BIT(victim->act[0], ACT_UNDEAD);
		victim->corpse_vnum = index->zombie;
		victim->parts = CORPSE_PARTS(obj);
		char_to_room(victim, ch->in_room);
		victim->pIndexData->count--;  // Animated mobs dont add to world count.
		act("$p twitches then thrashes violently before rising to its feet!", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

		add_follower(victim, ch, true);
		REMOVE_BIT(victim->act[0], ACT_PET);
		if (!add_grouped(victim, ch, true)) {
			act("$n falls back to the ground.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			char_from_room(victim);
			extract_char(victim, true);
			return true;
		}

		// 20070521 : NIB : Add affects for dealing with missing body parts

		// 20070520 : NIB : Fixed it so failure to add the mob to your group
		//			kept the objects in the CORPSE object.
		if (obj->contains) {
			for (obj_in = obj->contains; obj_in != NULL; obj_in = obj_in_next) {
				obj_in_next = obj_in->next_content;
				obj_from_obj(obj_in);
				obj_to_char(obj_in, victim);
			}
		}

		extract_obj(obj);
		return true;
	}
	return false;
}

SPELL_FUNC(spell_death_grip)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = false;
	memset(&af,0,sizeof(af));

	if (!get_eq_char(victim, WEAR_WIELD)) {
		act("$n's fingers slip through thin air.", victim, ch, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		send_to_char("Your fingers slip through thin air.\n\r", victim);
		return false;
	}

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = true;
	}

	if (perm && is_affected(victim, sn)) {
		affect_strip(victim, sn);
	} else if (IS_AFFECTED(victim, AFF_DEATH_GRIP))	{
		if (victim == ch)
			send_to_char("Your grip won't get any tighter.\n\r",ch);
		else
			act("$N is already blessed with death grip.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = sn;
	af.level = level;
	af.duration = perm ? -1 : (level / 6 + 5);
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = AFF_DEATH_GRIP;
	af.bitvector2 = 0;
	af.slot = obj_wear_loc;
	affect_to_char(victim, &af);

	act("{D$n's weapon grip tightens as in death.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{DYour grip tightens and locks into place.{x\n\r", victim);

	return true;
}

SPELL_FUNC(spell_deathsight)
{
	CHAR_DATA *victim;
	AFFECT_DATA af;
	bool perm = false;
	memset(&af,0,sizeof(af));

	victim = (CHAR_DATA *) vo;

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = true;
	}

	if (perm && is_affected(victim, sn))
		affect_strip(victim, sn);
	else if (is_affected(victim,sn) || IS_AFFECTED2(ch, AFF2_DEATHSIGHT)) {
		if(victim == ch)
			send_to_char("You already sense the world of the dead.\n\r",ch);
		else
			act("$N can already sense the dead.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = sn;
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.level = level;
	af.duration = perm ? -1 : (level/3);
	af.bitvector = 0;
	af.bitvector2 = AFF2_DEATHSIGHT;
	af.slot = obj_wear_loc;
	affect_to_char(ch, &af);
	send_to_char("{DYour mind opens up to the world of the dead.\n\r", victim);
	act("{D$n's eyes momentarily flicker darker.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	return true;
}

SPELL_FUNC(spell_kill)
{
	ROOM_INDEX_DATA *here;
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int chance;

	act ("{YDark shadows loom around $N as you utter the power word 'kill'.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act ("{YDark shadows loom around you as $n utters the power word 'kill'.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act ("{YDark shadows loom around $N as $n utters the power word 'kill'.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);

	if (!IS_NPC(victim) && IS_IMMORTAL(victim)) {
		act("{WYour immortal presence causes the shadows to flee in terror.{x",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		act("{WPowerful forces surround $n drive the shadows away!{x",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		return false;
	}

	chance = 50 - (victim->tot_level - ch->tot_level)/2;

	if (IS_SET(victim->res_flags, RES_KILL))
		chance /= 2;

	if (IS_SET(victim->vuln_flags, VULN_KILL))
		chance *= 2;

	chance = URANGE(5, chance, 97);
	if (IS_AFFECTED(victim, AFF_CURSE))
		chance /= 2;

	if (IS_AFFECTED2(victim,AFF2_LIGHT_SHROUD)) {
		act("The shroud of light around $N deflects your dark power!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("The shroud of light surrounding you deflects $n's dark power!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("The shroud of light around $N deflects $n's dark power!", ch,victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		return false;
	}

	if (IS_AFFECTED(victim, AFF_SANCTUARY) || number_percent () >= chance || IS_SET(victim->imm_flags, IMM_KILL)) {

		act("{DThe dark shadows disperse.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act("{DThe dark shadows disperse.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n appears unaffected.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act("You are unaffected.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return false;
	}

	if (saves_spell(level,victim,DAM_OTHER) || IS_SHIFTED_SLAYER(victim)) {
		send_to_char("Nothing happens.\n\r", ch);
		return false;
	}

	act("{R$n keels over and dies as $s heart stops instantly.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{RYou keel over and die as your heart stops instantly.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);



				// Check if slain victim was part of a quest. Checks if your horse got the kill, too.
		if (!IS_NPC(ch)) {
			check_quest_slay_mob(ch, victim, true);
			check_invasion_quest_slay_mob(ch, victim);
		}

	victim->set_death_type = DEATHTYPE_ALIVE;
	victim->death_type = DEATHTYPE_KILLSPELL;

	        if (ch != victim) {
            group_gain(ch, victim);
            if( ch->fighting == victim )
                stop_fighting(ch, false);
        }

	here = victim->in_room;
	victim->position = POS_STANDING;
	if(!p_percent_trigger(victim, NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_DEATH,NULL))
		p_percent_trigger(NULL, NULL, here, NULL, ch, victim, NULL, NULL, NULL, TRIG_DEATH,NULL);

	if (!IS_NPC(ch) && !IS_NPC(victim))
		player_kill(ch, victim);
	else if (!IS_NPC(ch) && IS_NPC(victim))
		ch->monster_kills++;

	raw_kill(victim, true, true, RAWKILL_NORMAL); /* 1= has head, 0= no head */
	return true;
}


SPELL_FUNC(spell_raise_dead)
{
	CHAR_DATA *victim;
	char buf[MAX_STRING_LENGTH];
	OBJ_DATA *obj;
	OBJ_DATA *in;
	OBJ_DATA *in_next;

	if (target == TARGET_OBJ) {
		obj = (OBJ_DATA *) vo;

		if (obj->item_type != ITEM_CORPSE_NPC && obj->item_type != ITEM_CORPSE_PC) {
			send_to_char("This spell must be cast on a fresh corpse.\n\r", ch);
			return false;
		}

		if (IS_SET(obj->extra[1], ITEM_NO_RESURRECT)) {
			act("$p seems to be immune to your necromantic magic.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			return false;
		}

		if (!IS_SET(CORPSE_PARTS(obj),PART_HEAD)) {
			act("$p is missing its head.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			return false;
		}

		if (obj->level > ch->tot_level) {
			act("You are not powerful enough to raise this being.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			return false;
		}


		if (obj->item_type == ITEM_CORPSE_PC) {
			victim = get_char_world(NULL,obj->owner);
			if (!victim) {
				sprintf(buf, "The soul of %s is no longer within this world.", obj->owner);
				act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				return false;
			}

			if (!IS_DEAD(victim)) {
				sprintf(buf, "The soul of %s has already been resurrected.", obj->owner);
				act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				return false;
			}

			// Only allow resurrection of CPK corpses in CPK rooms
			if( IS_SET(ch->in_room->roomflag[0], ROOM_CPK) && !IS_SET(CORPSE_FLAGS(obj), CORPSE_CPKDEATH) )
			{
				// Any player, or non-holyaura immortal, attempting to do so will be ZOTTED.
				if( !IS_NPC(ch) && (!IS_IMMORTAL(ch) || !IS_SET(ch->act[1], PLR_HOLYAURA)))
				{
					send_to_char("{YAttempting to raise a non-CPK corpse in a CPK room is {RFORBIDDEN{Y!{x\n\r", ch);
					ch->hit = 1;
					ch->mana = 1;
					ch->move = 1;
					return false;
				}
			}

			// Only allow resurrection of PK corpses in PK rooms...
			if( is_room_pk(ch->in_room, true) && !IS_SET(CORPSE_FLAGS(obj), CORPSE_PKDEATH) )
			{
				// No penalty here, just failure.
				act("$p seems to be immune to your divine energies.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
				return false;
			}

			if (obj != victim->pcdata->corpse) {
				act("You can only raise $N's most current corpse.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				return false;
			}

			// Check the victim if it can be resurrected directly
			if (p_percent_trigger( victim, NULL, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_PRERESURRECT, NULL) )
				return false;

			// Check the corpse for anything blocking the resurrection
			if (p_percent_trigger( NULL, obj, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_PRERESURRECT, NULL) )
				return false;

			// Check the ROOM the corpse is in for anything blocking resurrection
			if (p_percent_trigger( NULL, NULL, ch->in_room, NULL, ch, victim, NULL, obj, NULL, TRIG_PRERESURRECT, NULL) )
				return false;


			resurrect_pc(victim);

			// This is entirely.. redundant
//			while (victim->affected)
//				affect_remove(victim, victim->affected);

//			update_pos(victim);
			char_from_room(victim);

/*
			if (victim->church != ch->church || !ch->church || !victim->church) {
				act("As $N is not a member of your church, the spell drains your life energies.", ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				ch->hit = 1;
				ch->mana = 1;
				ch->move = 1;
			}
			*/
		} else {
			bool keep_mob = true;

			victim = create_mobile(get_mob_index(obj->orig_vnum), false);
			// Regardless what wealth the normal mob has...
			victim->gold = 0;
			victim->silver = 0;

			// Take newly created NPCs items off
			for (in = victim->carrying; in != NULL; in = in_next) {
				in_next = in->next_content;
				extract_obj(in);
			}

			// Restore information about the victim, then check to see if they can be resurrected
			p_percent_trigger(victim, NULL, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_RESURRECT, NULL);
			p_percent_trigger(NULL, obj, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_RESURRECT, NULL);

			// Check the corpse for anything blocking the resurrection
			if (p_percent_trigger( victim, NULL, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_PRERESURRECT, NULL) )
				keep_mob = false;

			// Check the corpse for anything blocking the resurrection
			if (keep_mob && p_percent_trigger( NULL, obj, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_PRERESURRECT, NULL) )
				keep_mob = false;

			// Check the ROOM the corpse is in for anything blocking resurrection
			if (keep_mob && p_percent_trigger( NULL, NULL, ch->in_room, NULL, ch, victim, NULL, obj, NULL, TRIG_PRERESURRECT, NULL) )
				keep_mob = false;

			if( !keep_mob )
			{
				extract_char(victim, false);
				return false;
			}
		}

		act("{DThe light within the surrounding area dims and an intense chill sets in.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		char_to_room(victim, ch->in_room);

		act("A dark haze forms around $p.", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

		act("You feel mortal once more as your soul is raised from the dead!", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("You cough and splutter.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		act("The eyes of $n flick open.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act("{+$n begins to cough and splutter.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		act("{+$n has been raised from the dead!", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		// Give back the stuff on the corpse
		for (in = obj->contains; in != NULL; in = in_next) {
			in_next = in->next_content;

			obj_from_obj(in);

			if (in->pIndexData->vnum == OBJ_VNUM_SILVER_ONE) {
				victim->silver++;
				extract_obj(in);
				continue;
			}

			if (in->pIndexData->vnum == OBJ_VNUM_SILVER_SOME) {
				victim->silver += in->value[1];
				extract_obj(in);
				continue;
			}

			if (in->pIndexData->vnum == OBJ_VNUM_GOLD_ONE) {
				victim->gold++;
				extract_obj(in);
				continue;
			}

			if (in->pIndexData->vnum == OBJ_VNUM_GOLD_SOME) {
				victim->gold += in->value[1];
				extract_obj(in);
				continue;
			}

			obj_to_char(in, victim);
		}

		obj_from_room(obj);
		extract_obj(obj);

		/* prevent raise dead / kill over and over again */
		victim->hit = victim->max_hit/3;
		victim->mana = victim->max_mana/3;
		victim->move = victim->max_mana/3;
		return true;
	} else {
		send_to_char("Nothing happens.\n\r", ch);
		return false;
	}
}
