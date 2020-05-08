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
#include <math.h>
#include "merc.h"
#include "interp.h"
#include "scripts.h"
#include "tables.h"
#include "wilds.h"

extern void persist_save(void);

// Global variables
int save_number = 0;
int pulse_point;


// Event system for queued events.
EVENT_DATA *events;
EVENT_DATA *events_tail;

// Local functions
int hit_gain	args((CHAR_DATA *ch));
int mana_gain	args((CHAR_DATA *ch));
int move_gain	args((CHAR_DATA *ch));
int toxin_gain(CHAR_DATA *ch);
void mobile_update	args((void));
void char_update	args((void));
void obj_update	args((void));
void aggr_update	args((void));
void msdp_update	args((void));
void ship_update     args((void));
void npc_ship_state_update     args((void));
void who_list	args((void));
void quest_update    args((void));
void remove_port     args((long vnum_boat_dock, int door));
void create_port     args((long vnum_port, long vnum_boat_dock, int door));
void stock_update    args((void));
void update_scuttle	args((void));
void update_hunting	args((void));
void toxin_update args ((CHAR_DATA *ch));
SHIP_DATA *is_being_attacked_by_ship args((SHIP_DATA *pShip));
SHIP_DATA *is_being_chased_by_ship args((SHIP_DATA *pShip));
void scare_update(CHAR_DATA *ch);
void update_has_done(CHAR_DATA *ch);
void reset_waypoint(NPC_SHIP_DATA *npc_ship) ;
void relic_update(void);
void check_relic_vanish(OBJ_DATA *relic);
void update_invasion_quest();
void instance_update();
void dungeon_update();

/*
 * Handle all kinds of updates.
 * Called once per pulse from game loop.
 * Random times to defeat tick-timing clients and players.
 */
void update_handler(void)
{
    static int pulse_area;
    static int pulse_mobile;
    static int pulse_violence;
    static int pulse_auction;
    static int pulse_mail;
    static int pulse_aggr;
    static int pulse_event;
    static int pulse_msdp;
    static int pulse_ships;
    char buf[MSL];
    int i;

    if (merc_down)
    	return;

    if (--pulse_area <= 0)
    {
        pulse_area = PULSE_AREA;
	area_update(FALSE);
	write_permanent_objs();
	persist_save();
//	save_npc_ships();
	write_mail();
	save_projects();
	save_immstaff();
	save_instances();
    }

    if (--pulse_auction <= 0)
    {
        pulse_auction = PULSE_AUCTION;
        auction_update();
    }

    if (--pulse_mail <= 0)
    {
	pulse_mail = PULSE_MAIL;
	mail_update();
    }

    if (--pulse_mobile <= 0)
    {
	pulse_mobile = PULSE_MOBILE;
	mobile_update();
    }

    if (--pulse_violence <= 0)
    {
	pulse_violence = PULSE_VIOLENCE;
	violence_update();
        update_hunting();
    }

    if (--pulse_msdp <= 0)
    {
	pulse_msdp = PULSE_PER_SECOND;
	msdp_update();
    }

    // Check to see if boosts have run out.
    for (i = 0; boost_table[i].name != NULL; i++) {
	if (i != BOOST_RECKONING && boost_table[i].timer != 0
	&&  current_time > boost_table[i].timer)
	{
	    sprintf(buf, "%s {Dboost has ended.{x\n\r", boost_table[i].colour_name);
	    gecho(buf);
	    boost_table[i].timer = 0;
	    boost_table[i].boost = 100;
	}
    }

    // TICK
    if (--pulse_point <= 0)
    {
	wiznet("TICK!", NULL, NULL, WIZ_TICKS, 0, 0);
	pulse_point = PULSE_TICK;

	if (number_percent() < 20)
	    write_churches_new();

	//write_gq();
        //update_ship_exits();
	time_update();
	char_update();
	//update_invasion_quest(); Syn - don't do this. it seems to eat a lot of CPU time.
	obj_update();
	quest_update();
    	pneuma_relic_update();
	relic_update();
	update_area_trade();
	instance_update();
	dungeon_update();
	ships_tick_update();

	/* 2006-07-27 This is now redundant, and this function seems to loop (Syn).
	   Wilderness exits are cleaned up in char_from_room, which is much easier and more elegant.
	if (top_wilderness_exit > MAX_WILDERNESS_EXITS)
	    remove_wilderness_exits(); */

	//update_weather();

	// An autowar has started
	if (auto_war_timer > 0)
	{
	    auto_war_timer--;
	    if (auto_war_timer == 0)
		start_war();
	    else
	    {
		sprintf(buf, "{RGet ready! {RA {Y%s{R war will begin for levels {Y%d{R to {Y%d{R in {Y%d{R minutes!{x\n\r",
			auto_war_table[ auto_war->war_type ].name,
			auto_war->min,
			auto_war->max,
			auto_war_timer);
		gecho(buf);
		gecho("Type 'war join' to enter!\n\r");
	    }
	}

	// End an autowar in progress
	if (auto_war_battle_timer > 0)
	{
	    auto_war_battle_timer--;
	    if (auto_war_battle_timer == 0)
		auto_war_time_finish();
	}

	// Auto-reboot
	if (reboot_timer > 0)
	{
	    if ((reboot_timer - current_time)/60 <= 0)
	    {
		CHAR_DATA *rebooting;

		if ((rebooting = get_char_world(NULL, reboot_by)) == NULL)
		{
		    free_string(reboot_by);
		    gecho("{WIMPLEMENTOR NOT PRESENT. REBOOT AVERTED.{x\n\r");
		    reboot_timer = 0;
		    down_timer = 0;
		}
		else
		{
		    free_string(reboot_by);
		    sprintf(buf, "{WREBOOTING. DOWNTIME WILL BE APPROXIMATELY %d MINUTES.{x\n\r", down_timer);
		    gecho(buf);

		    do_function(rebooting, &do_shutdown, "");
		}
	    }
	    else
	    {
		sprintf(buf, "{WREBOOT IN %ld MINUTE%s.{x\n\r",
			(long int)((reboot_timer - current_time)/60),
			(reboot_timer - current_time)/60 == 1 ? "" : "S");
		gecho(buf);
	    }
	}
    }

    if (--pulse_aggr <= 0)
    {
        pulse_aggr = PULSE_AGGR;
	aggr_update();
    }

    if ( --pulse_event        <= 0 )
    {
	pulse_event             = PULSE_EVENT;
	event_update();
    }

    if ( --pulse_ships <= 0 )
    {
		pulse_ships = PULSE_SHIPS;
		ships_pulse_update();
	}

    tail_chain();
}


// Advance a PC one level.
void advance_level(CHAR_DATA *ch, bool hide)
{
    char buf[MAX_STRING_LENGTH];
    int add_hp;
    int add_mana;
    int add_move;
    int add_prac;

    ch->exp = 0;
    ch->pcdata->last_level =
	(ch->played + (int) (current_time - ch->logon)) / 3600;

    add_hp	= con_app[get_curr_stat(ch,STAT_CON)].hitp + number_range(
		  class_table[get_profession(ch, CLASS_CURRENT)].hp_min,
		  class_table[get_profession(ch, CLASS_CURRENT)].hp_max);

    add_mana 	= get_curr_stat(ch,STAT_INT)/4 +
	          get_curr_stat(ch,STAT_WIS)/4 +
		  number_range(1, 10);

    if (!class_table[get_profession(ch, CLASS_CURRENT)].fMana)
	add_mana /= 2;

    add_move	= get_curr_stat(ch, STAT_STR) / 2 + get_curr_stat(ch, STAT_DEX) / 2 + number_range(1, 3);

    add_prac	= 2 * wis_app[get_curr_stat(ch,STAT_WIS)].practice;

    add_hp = add_hp * 9/10;
    add_mana = add_mana * 9/10;
    add_move = add_move * 9/10;

    add_hp	= UMAX( 2, add_hp  );
    add_mana	= UMAX( 2, add_mana);
    add_move	= UMAX( 6, add_move);

    if (IS_REMORT(ch))
    {
	add_hp = (ch->pcdata->hit_before / 120);
	add_mana = (ch->pcdata->mana_before / 120);
	add_move = (ch->pcdata->move_before / 120);
    }

    if (ch->pcdata->perm_hit + add_hp > pc_race_table[ch->race].max_vital_stats[MAX_HIT])
	add_hp = pc_race_table[ch->race].max_vital_stats[MAX_HIT] - ch->pcdata->perm_hit;
    if (ch->pcdata->perm_mana + add_mana > pc_race_table[ch->race].max_vital_stats[MAX_MANA])
	add_mana = pc_race_table[ch->race].max_vital_stats[MAX_MANA] - ch->pcdata->perm_mana;
    if (ch->pcdata->perm_move + add_move > pc_race_table[ch->race].max_vital_stats[MAX_MOVE])
	add_move = pc_race_table[ch->race].max_vital_stats[MAX_MOVE] - ch->pcdata->perm_move;

    ch->max_hit += add_hp;
    ch->max_mana += add_mana;
    ch->max_move += add_move;
    ch->practice += add_prac;

    if ((ch->tot_level <= 10)
          || (ch->tot_level > 10 && number_percent() < 10))
	ch->train += 1;

    ch->pcdata->perm_hit	+= add_hp;
    ch->pcdata->perm_mana	+= add_mana;
    ch->pcdata->perm_move	+= add_move;

    if (!hide)
    {
	sprintf(buf,
	    "{MYou gain {W%d {Mhit point%s, {W%d {Mmana, {W%d {Mmove, "
	    "and {W%d {Mpractice%s.\n\r{x",
	    add_hp,
	    add_hp == 1 ? "" : "s",
	    add_mana,
	    add_move,
	    add_prac,
	    add_prac == 1 ? "" : "s");
	send_to_char(buf, ch);
    }
}


// Give a character exp
void gain_exp(CHAR_DATA *ch, int gain)
{
	char buf[MAX_STRING_LENGTH];

	// Allow scripts to affect gaining experience, as well as blocking the use of the xp
	ch->tempstore[0] = gain;
	if(p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_XPGAIN, NULL))
		return;

	// Only update gain IF the value is less than what was originally put in, don't allow scripts to boost the XP at this point.
	if( ch->tempstore[0] < gain )
		gain = ch->tempstore[0];

	if (IS_IMMORTAL(ch)) return;

	if(IS_NPC(ch)) {
		if(!IS_SET(ch->act2,ACT2_CANLEVEL) || ch->maxexp < 1) return;
		ch->exp += gain;

		if(ch->exp >= ch->maxexp) {
			if( !p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,TRIG_LEVEL, NULL) ) {
				ch->exp = 0;

				ch->level += 1;
				ch->tot_level += 1;
			}
		}
	} else {
		if (ch->tot_level >= 120)
			return;

		/* make sure you never get more than the exp for your level */
		ch->exp = UMIN(exp_per_level(ch,ch->pcdata->points), ch->exp + gain);

		if (ch->tot_level < LEVEL_HERO && ch->level < MAX_CLASS_LEVEL &&
			ch->exp >= exp_per_level(ch,ch->pcdata->points)) {

			send_to_char("{MYou raise a level!!{x\n\r", ch);
			ch->exp = 0;
			ch->level += 1;
			ch->tot_level += 1;
			if( IS_SET(ch->affected_by2_perm, AFF2_DEATHSIGHT) )
				ch->deathsight_vision = ch->tot_level;

			sprintf(buf,"%s gained level %d",ch->name,ch->level);

			sprintf(buf, "All congratulate %s who is now level %d!!!", ch->name, ch->tot_level);
			crier_announce(buf);

			if (ch->level >= MAX_CLASS_LEVEL) {
				if (ch->tot_level != 120) {
					send_to_char("You are now ready to multiclass."
						"\n\rType 'help multiclass' for more information.\n\r", ch);
				}
			}

			log_string(buf);
			sprintf(buf,"$N has attained level %d!",ch->level);
			wiznet(buf,ch,NULL,WIZ_LEVELS,0,0);
			advance_level(ch,FALSE);

			p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,TRIG_LEVEL, NULL);

			save_char_obj(ch);
		}
	}
}


int hit_gain(CHAR_DATA *ch)
{
    int gain;
    int number;
    long amount;
    char buf[MAX_STRING_LENGTH];

    if (ch->in_room == NULL) {
		sprintf(buf, "hit_gain: %s had null in_room!", IS_NPC(ch) ? ch->short_descr : ch->name);
		bug(buf, 0);
		return 0;
    }

    if (IS_NPC(ch)) {
		gain =  5 + ch->level;
		if (IS_AFFECTED(ch,AFF_REGENERATION))
		    gain *= 2;

		switch(ch->position) {
	    default: 		gain /= 2;		break;
	    case POS_SLEEPING: 	gain = 3 * gain/2;    	break;
	    case POS_RESTING:  				break;
	    case POS_FIGHTING:	gain /= 3;		break;
		}
    } else {
		gain = UMAX(3,get_curr_stat(ch,STAT_CON) - 3 + ch->tot_level/2);
		gain += class_table[get_profession(ch, CLASS_CURRENT)].hp_max - 10;
		number = number_percent();
		if (number < get_skill(ch,gsn_fast_healing)) {
		    gain += number * gain / 100;
		    if (ch->hit < ch->max_hit)
				check_improve(ch,gsn_fast_healing,TRUE,8);
		}

		switch (ch->position) {
	    default:	   	gain = 3 * gain / 2;	break;
	    case POS_SLEEPING: 	gain = 3 * gain;	break;
	    case POS_RESTING:   gain = gain * 2; 	break;
	    case POS_FIGHTING: 	gain /= 2;		break;
		}

		if (ch->pcdata->condition[COND_HUNGER] == 0)
		    gain /= 2;

		if (ch->pcdata->condition[COND_THIRST] == 0)
		    gain /= 2;
    }

    if (ch->in_room->heal_rate > 0)
		gain = gain * ch->in_room->heal_rate / 100;

    if (ch->on && ch->on->item_type == ITEM_FURNITURE && ch->on->value[3] > 0)
		gain = gain * ch->on->value[3] / 100;

    if (IS_AFFECTED(ch, AFF_POISON))
		gain /= 4;

    if (IS_AFFECTED(ch, AFF_PLAGUE))
		gain /= 8;

    if (IS_AFFECTED(ch,AFF_REGENERATION) || (ch->tot_level < 31 && !IS_REMORT(ch)))
		gain *= 2;

    // Druids get 33% more in nature
    if (get_profession(ch, SUBCLASS_CLERIC) == CLASS_CLERIC_DRUID && is_in_nature(ch))
		gain += gain/3;

	/* If you have the relic you get 25% more */
	if (ch->church && vnum_in_treasure_room(ch->church, OBJ_VNUM_RELIC_HP_REGEN))
		gain += gain / 4;

   amount = UMIN(gain, ch->max_hit - ch->hit);
   return amount;
}


int mana_gain(CHAR_DATA *ch)
{
    int gain;
    int number;
    char buf[MSL];

    if (ch->in_room == NULL)
    {
        sprintf(buf, "mana_gain: %s had null in_room!",
	    IS_NPC(ch) ? ch->short_descr : ch->name);
	    bug(buf, 0);
	return 0;
    }

    if (IS_NPC(ch))
    {
        gain = 5 + ch->level;
	switch (ch->position)
	{
	    default:		gain /= 2;		break;
	    case POS_SLEEPING:	gain = 3 * gain/2;	break;
	    case POS_RESTING:				break;
	    case POS_FIGHTING:	gain /= 3;		break;
	}
    }
    else
    {
	gain = get_curr_stat(ch,STAT_WIS)
	      + get_curr_stat(ch,STAT_INT) + ch->tot_level;
	number = number_percent();

	if (number < get_skill(ch,gsn_meditation))
	{
	    gain += number * gain / 100;
	    if (ch->mana < ch->max_mana)
		check_improve(ch,gsn_meditation,TRUE,8);
	}

	if (!class_table[get_profession(ch, CLASS_CURRENT)].fMana)
	    gain /= 2;

	switch (ch->position)
	{
	    default:	   	gain = gain;			break;
	    case POS_SLEEPING: 	gain = 2 * gain;	break;
	    case POS_RESTING:   gain = 3 * gain/2; 			break;
	    case POS_FIGHTING: 	gain /= 2;			break;
	}

	if (ch->pcdata->condition[COND_HUNGER]   == 0)
	    gain /= 2;

	if (ch->pcdata->condition[COND_THIRST] == 0)
	    gain /= 2;

    }

    if (ch->in_room->mana_rate > 0)
	gain = gain * ch->in_room->mana_rate / 100;

    if (ch->on != NULL
    && ch->on->item_type == ITEM_FURNITURE
    && ch->on->value[4] > 0)
	gain = gain * ch->on->value[4] / 100;

    if (IS_AFFECTED(ch, AFF_POISON))
	gain /= 4;

    if (IS_AFFECTED(ch, AFF_PLAGUE))
	gain /= 8;

    if (IS_AFFECTED(ch,AFF_HASTE) || IS_AFFECTED(ch,AFF_SLOW))
	gain /= 2;

    // Druids get 33% more in nature
    if (get_profession(ch, SUBCLASS_CLERIC) == CLASS_CLERIC_DRUID && is_in_nature(ch))
		gain += gain/3;

	if (ch->church && vnum_in_treasure_room(ch->church, OBJ_VNUM_RELIC_MANA_REGEN))
		gain += gain / 4;

   if (IS_ELF(ch))
       gain *= 2;

   if (!str_cmp(race_table[ch->race].name, "lich"))
       gain = (gain * 5)/2;

   if (ch->tot_level < 31 && !IS_REMORT(ch))
       gain *= 2;

   return UMIN(gain, ch->max_mana - ch->mana);
}


int move_gain(CHAR_DATA *ch)
{
    int gain;
    long amount;
    char buf[MSL];

    if (ch->in_room == NULL)
    {
        sprintf(buf, "move_gain: %s had null in_room!",
	    IS_NPC(ch) ? ch->short_descr : ch->name);
	    bug(buf, 0);
	return 0;
    }

    if (IS_NPC(ch))
	gain = ch->level;
    else
    {
	gain = UMAX(15, ch->level);

	switch (ch->position)
	{
	    case POS_SLEEPING:
	        gain += get_curr_stat(ch,STAT_DEX)*3;
	        break;
	    case POS_RESTING:
	        gain += get_curr_stat(ch,STAT_DEX) / 2 * 3;
	    	break;
	}

	if (ch->pcdata->condition[COND_HUNGER]   == 0)
	    gain /= 2;

	if (ch->pcdata->condition[COND_THIRST] == 0)
	    gain /= 2;
    }

    if (ch->in_room->move_rate > 0)
	gain = gain * ch->in_room->move_rate/100;

    if (ch->on != NULL
    && ch->on->item_type == ITEM_FURNITURE
    && ch->on->value[5] > 0)
	gain = gain * ch->on->value[5] / 100;

    if (IS_AFFECTED(ch, AFF_POISON))
	gain /= 4;

    if (IS_AFFECTED(ch, AFF_PLAGUE))
	gain /= 8;

    if (IS_AFFECTED(ch,AFF_HASTE) || IS_AFFECTED(ch,AFF_SLOW))
	gain /= 2;

    // Druids get 33% more in nature
    if (get_profession(ch, SUBCLASS_CLERIC) == CLASS_CLERIC_DRUID
    &&   is_in_nature(ch))
	gain += gain/3;

    if (ch->tot_level < 31 && !IS_REMORT(ch))
	gain *= 2;

    amount = UMIN(gain, ch->max_move - ch->move);

    return amount;
}


// Regen sith toxins.
int toxin_gain(CHAR_DATA *ch)
{
    int gain;
    char buf[MAX_STRING_LENGTH];

    if (ch->in_room == NULL)
    {
        sprintf(buf, "toxin_gain: %s had null in_room!",
            IS_NPC(ch) ? ch->short_descr : ch->name);
        bug(buf, 0);
	return 0;
    }

    if (IS_NPC(ch))
    {
	gain =  5 + ch->level;
	if (IS_AFFECTED(ch,AFF_REGENERATION))
	    gain *= 2;

	switch(ch->position)
	{
	    default : 		gain /= 2;			break;
	    case POS_SLEEPING: 	gain = 3 * gain/2;		break;
	    case POS_RESTING:  					break;
	    case POS_FIGHTING:	gain /= 3;		 	break;
	}
    }
    else
    {
	gain = UMAX(3,get_curr_stat(ch,STAT_CON) - 3);

	switch (ch->position)
	{
	    default:	   	gain = 3 * gain / 2;			break;
	    case POS_SLEEPING: 	gain = 3 * gain;	break;
	    case POS_RESTING:   gain = gain * 2; 			break;
	    case POS_FIGHTING: 	gain /= 2;			break;
	}

	if (ch->pcdata->condition[COND_HUNGER]   == 0)
	    gain /= 2;

	if (ch->pcdata->condition[COND_THIRST] == 0)
	    gain /= 2;
    }

    if (IS_AFFECTED(ch, AFF_POISON))
	gain /= 4;

    if (IS_AFFECTED(ch, AFF_PLAGUE))
	gain /= 8;

    if (IS_AFFECTED(ch,AFF_REGENERATION))
	    gain *= 2;

    gain += number_range(1,2);

    return (URANGE(1, gain, 15));
}


// Update a condition (hunger, thirst, drunk, etc.)
void gain_condition(CHAR_DATA *ch, int iCond, int value)
{
    int condition;

    // For the life sustaining quest item.
    if (value < 0
    && (iCond == COND_HUNGER || iCond == COND_THIRST)
    && is_sustained(ch))
    {
        ch->pcdata->condition[iCond] = 48;
		return;
    }

    if (value == 0 || IS_NPC(ch) || ch->level >= LEVEL_IMMORTAL)
		return;

    condition = ch->pcdata->condition[iCond];
    if (condition == -1)
		return;

	// When draining hunger/thirst, they have a CON% chance of not losing it.
	if( (value < 0) && (iCond == COND_HUNGER || iCond == COND_THIRST) &&
		(number_percent() < get_curr_stat(ch, STAT_CON)))
		return;

    ch->pcdata->condition[iCond] = URANGE(0, condition + value, 48);

    if (ch->pcdata->condition[iCond] == 0)
    {
	switch (iCond)
	{
	case COND_HUNGER:
	    send_to_char("You are hungry.\n\r",  ch);
	    break;

	case COND_THIRST:
	    send_to_char("You are thirsty.\n\r", ch);
	    break;

	case COND_DRUNK:
	    if (condition != 0)
		send_to_char("You are sober.\n\r", ch);
	    break;
	}
    }
}


/*
 * Mob autonomous action.
 * This function takes 25% to 35% of ALL Merc cpu time.
 */
void mobile_update(void)
{
	ITERATOR it;
    CHAR_DATA *ch;
    EXIT_DATA *pexit;
    int door;
    char buf[MSL];

	iterator_start(&it, loaded_chars);
	while(( ch = (CHAR_DATA *)iterator_nextdata(&it)))
    {
		if (ch->in_room == NULL || IS_AFFECTED(ch,AFF_CHARM))
	    	continue;

		// Done to allow for TOKEN random type scripts on players, but only if they have tokens!
		if (!IS_NPC(ch)) {
		    if(ch->tokens) {

			p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RANDOM, NULL);

			// Prereckoning
			if (pre_reckoning > 0 && reckoning_timer > 0)
			    p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_PRERECKONING, NULL);

			// Reckoning
			if (!pre_reckoning && reckoning_timer > 0)
			    p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RECKONING, NULL);
		    }
		    continue;
		}

		if (!can_room_update(ch->in_room))
			continue;

		// A dirty hack to remove any Death mobs that have been stranded
		if (ch->pIndexData->vnum == MOB_VNUM_DEATH)	// Replaced the name check to the vnum
		{
			CHAR_DATA *vch;
			CHAR_DATA *vch_next;
			for (vch = ch->in_room->people; vch != NULL; vch = vch_next)
			{
				vch_next = vch->next_in_room;
				if (vch != ch && !IS_NPC(vch))
				{
					send_to_char("{C'Well then.' says Death. 'Looks like that corpse is adequately dead. Cya folks, im outta here.'{x\n\r", vch);
					send_to_char("Death waves happily.\n\r", vch);
					send_to_char("{DDeath sinks into the ground and disappears.{x\n\r", vch);
				}
			}

			extract_char(ch, TRUE);
			continue;
		}

		// Examine call for special procedure
		if (ch->spec_fun != 0)
		{
			if ((*ch->spec_fun)(ch))
				continue;
		}

		if (ch->shop != NULL)
		{
			// Give shop owners gold
			if ((ch->gold * 100 + ch->silver) < ch->pIndexData->wealth)
			{
				ch->gold += ch->pIndexData->wealth * number_range(1,20)/5000000;
				ch->silver += ch->pIndexData->wealth * number_range(1,20)/50000;
			}

			// Restock their supplies
			if( ch->shop->restock_interval > 0 ) {
				if( ch->shop->next_restock < current_time ) {

					bool restocked = FALSE;
					for(SHOP_STOCK_DATA *stock = ch->shop->stock; stock; stock = stock->next)
					{
						if( stock->max_quantity > 0 && stock->restock_rate > 0 && stock->quantity < stock->max_quantity)
						{
							stock->quantity += stock->restock_rate;
							stock->quantity = UMIN(stock->quantity, stock->max_quantity);
							restocked = TRUE;

						}
					}

					if( restocked )
						p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RESTOCKED, NULL);

					ch->shop->next_restock = current_time + ch->shop->restock_interval * 60;
				}
			}
		}

		// Check mob triggers

		// Delay
		if (ch->progs->delay > 0)
		{
			if (--ch->progs->delay <= 0)
			{
			p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_DELAY, NULL);
			continue;
			}
		}

        // Random
		p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RANDOM, NULL);

		// Prereckoning
		if (pre_reckoning > 0 && reckoning_timer > 0) {
			p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_PRERECKONING, NULL);
		}

		// Reckoning
		if (pre_reckoning == 0 && reckoning_timer > 0) {
			p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RECKONING, NULL);
		}

		// get rid of crew when they are past their hired date
		if (ch->belongs_to_ship != NULL &&
			!IS_NPC_SHIP(ch->belongs_to_ship) &&
			current_time > ch->hired_to) {
			extract_char(ch, TRUE);
			continue;
		}

		if( IS_NPC(ch) && IS_SET(ch->act2, ACT2_HIRED) )
		{
			// If hired, check whether their timer has expired OR are no longer grouped (important)
			if( ch->hired_to > 0 && (current_time < ch->hired_to || ch->leader == NULL) )
			{
				// CONTRACT_COMPLETE can allow the mob to remain in existence
				// - when a script gets executed, you need to return a zero to extract the mob
				// - when no script gets executed, extraction will be performed
				if(p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_CONTRACT_COMPLETE, NULL) <= 0)
				{
					extract_char(ch, TRUE);
					continue;
				}

				// Only get here when the return value is positive
				// - script execution with no "end" called
				// - end called with positive value

				if( ch->master != NULL )
				{
					// Un..pet
					if( ch->master->pet == ch )
						ch->master->pet = NULL;
				}

				// Check if they are being ridden
				CHAR_DATA *rider = RIDDEN(ch);
				if( rider != NULL )
				{
					// Silently dismount
					rider->riding = FALSE;
					ch->riding = FALSE;
					ch->rider = NULL;
					rider->mount = NULL;
				}

				die_follower(ch);
				REMOVE_BIT(ch->act2, ACT2_HIRED);
				ch->hired_to = 0;
			}
		}

		// That's all for sleeping / busy monster, and empty zones
		if (ch->position != POS_STANDING)
			continue;

		// Ship Quest masters
		if (IS_SET(ch->act2, ACT2_SHIP_QUESTMASTER) && number_percent() < 5) {
			AREA_DATA *pArea = NULL;

			for (pArea = area_first; pArea != NULL; pArea = pArea->next) {

			if (pArea->invasion_quest != NULL && ch->in_room != NULL &&
				ch->in_room->area->place_flags == pArea->place_flags) {
				sprintf(buf, "We are offering a reward to anyone that can restore order in %s.", pArea->name);
				do_say(ch, buf);
			}
			}
		}

		// Scavenge
		if (IS_SET(ch->act, ACT_SCAVENGER)
		&&   ch->in_room->contents != NULL
		&&   number_bits(6) == 0)
		{
			OBJ_DATA *obj;
			OBJ_DATA *obj_best;
			int max;

			max = 1;
			obj_best = 0;
			for (obj = ch->in_room->contents; obj; obj = obj->next_content)
			{
			if (!can_get_obj(ch, obj, NULL, NULL, TRUE))
				continue;

			if (CAN_WEAR(obj, ITEM_TAKE)
			&&   obj->cost > max
			&&   obj->cost > 0
			&&   !is_quest_token(obj))
			{
				obj_best = obj;
				max = obj->cost;
			}
			}

			if (obj_best != NULL)
			{
			act("$n gets $p.", ch, NULL, NULL, obj_best, NULL, NULL, NULL, TO_ROOM);
			obj_from_room(obj_best);
			obj_to_char(obj_best, ch);
			}
		}

		if (ch->in_room == NULL)
		{
			sprintf(buf, "mobile_update: ch %s (%ld) had null in_room!",
				IS_NPC(ch) ? ch->short_descr : ch->name,
			IS_NPC(ch) ? ch->pIndexData->vnum : 0);
			bug(buf, 0);
			continue;
		}

		/* Wander */

		/* Syn - Only do this for mobs that aren't grouped. Obviously
		   to prevent grouped mobs from wandering off, since wandering
		   can be done with a mprog, while the reverse cannot be done */
		if (ch->leader == NULL // Following AND grouped
		&&  ch->master == NULL // Following only
		&&  !IS_SET(ch->act, ACT_SENTINEL)
		&&  number_bits(3) == 0
		&&  !IS_SET(ch->act, ACT_MOUNT)) {
			door = number_range(0, MAX_DIR - 1);
			if ((pexit = ch->in_room->exit[door]) != NULL
			&&  pexit->u1.to_room != NULL
			&&  !IS_SET(pexit->exit_info, EX_CLOSED)
			&&  !IS_SET(pexit->u1.to_room->room_flags, ROOM_NO_MOB)
			&&  !IS_SET(pexit->u1.to_room->room_flags, ROOM_NO_WANDER)
			&&  (!IS_SET(ch->act2, ACT_STAY_LOCALE) ||
				(pexit->u1.to_room->area == ch->in_room->area &&
					(!ch->in_room->locale || !pexit->u1.to_room->locale || ch->in_room->locale == pexit->u1.to_room->locale)))
			&&  (IS_SET(ch->act2, ACT2_WILDS_WANDERER) || !pexit->u1.to_room->wilds)
			&&  (!IS_SET(ch->act, ACT_STAY_AREA)
			 || pexit->u1.to_room->area == ch->in_room->area)
			&&  (!IS_SET(ch->act, ACT_OUTDOORS)
			 || !IS_SET(pexit->u1.to_room->room_flags,ROOM_INDOORS))
			&&  (!IS_SET(ch->act, ACT_INDOORS)
			 || IS_SET(pexit->u1.to_room->room_flags,ROOM_INDOORS)))
			move_char(ch, door, FALSE);
		}
    }
}


void remove_port(long vnum_boat_dock, int door)
{
#if 0
    ROOM_INDEX_DATA *pWildRoom;
    char buf[MAX_STRING_LENGTH];

    if ((pWildRoom = get_room_index(vnum_boat_dock)) != NULL)
    {
	rp_change_exit(get_room_index(6551), "west delete", DIR_WEST);
	sprintf(buf, "west %ld", vnum_boat_dock-1);
	rp_change_exit(pWildRoom, buf, DIR_WEST);
	sprintf(buf, "north %ld", vnum_boat_dock-348);
	rp_change_exit(pWildRoom, buf, DIR_NORTH);
	sprintf(buf, "south %ld", vnum_boat_dock+348);
	rp_change_exit(pWildRoom, buf, DIR_SOUTH);
	sprintf(buf, "east %ld", vnum_boat_dock+1);
	rp_change_exit(pWildRoom, buf, DIR_EAST);
    }
#endif
}


void create_port(long vnum_port, long vnum_boat_dock, int door)
{
#if 0
    ROOM_INDEX_DATA *pRoom;
    ROOM_INDEX_DATA *pWildRoom;

    if ((pWildRoom = get_room_index(vnum_boat_dock)) != NULL &&
	    (pRoom = get_room_index(vnum_port)) != NULL)
    {
	rp_change_exit(pWildRoom, "west delete", DIR_WEST);
	rp_change_exit(pWildRoom, "east delete", DIR_EAST);
	rp_change_exit(pWildRoom, "south delete", DIR_SOUTH);
	rp_change_exit(pWildRoom, "north delete", DIR_NORTH);
	rp_change_exit(pRoom, "east 6551", DIR_EAST);
    }
#endif
}

// Update the public boat.
void update_public_boat(int time)
{
#if 0
    ROOM_INDEX_DATA *room;
    AREA_DATA *ship_area;

    ship_area = find_area("Ship");
    if (ship_area == NULL)
    {
	bug("update_public_boat: ship area was null!", 0);
	return;
    }

    if (find_area("Wilderness") == NULL)
    {
	log_string("update_public_boat: no wilderness!");
	return;
    }

    // Boat arrives in at Plith
    if (time == 7)
    {
	room = get_room_index(744104);
	rp_change_exit(room, "command delete", DIR_EAST);
	rp_change_exit(room, "command 6551", DIR_EAST);

	room = get_room_index(6551);
	rp_change_exit(room, "command 744104", DIR_WEST);

	room_echo(room, "{YCaptain Pinot yells 'The Endeavor has arrived in Plith!'{x\n\r");

	sector_echo(ship_area, "{YCaptain Pinot yells 'The Endeavor has arrived in Plith!'{x", SECT_INSIDE);

	sector_echo(find_area("Plith"), "{YYou hear the large horns of the Endeavor as it enters Plith harbour!{x", SECT_CITY);
    }
    else
    // Boat leaves from Plith
    if (time == 9)
    {
	room = get_room_index(744104);
	room_echo(room, "{YCaptain Pinot yells 'The Endeavor has left for Achaeus!'{x\n\r");

	rp_change_exit(room, "command delete", DIR_EAST);
	rp_change_exit(room, "command 744105", DIR_EAST);

	room = get_room_index(6551);
	rp_change_exit(room, "command delete", DIR_WEST);

	sector_echo(ship_area, "{YCaptain Pinot yells 'The Endeavor has left for Achaeus!'{x", SECT_INSIDE);

	sector_echo(find_area("Plith"), "{YYou hear the large horns of the Endeavor as it leaves Plith harbour!{x", SECT_CITY);
    }
    else
    // Boat arrives in at Achaeus
    if (time == 16)
    {
	room = get_room_index(781122);

	rp_change_exit(room, "command delete", DIR_EAST);
	rp_change_exit(room, "command 6551", DIR_EAST);

	room = get_room_index(6551);
	rp_change_exit(room, "command 781122", DIR_WEST);

	room_echo(room, "{YCaptain Pinot yells 'The Endeavor has arrived in Achaeus!'{x\n\r");

	sector_echo(ship_area, "{YCaptain Pinot yells 'The Endeavor has arrived in Achaeus!'{x", SECT_INSIDE);

	sector_echo(find_area("Achaeus"), "{YYou hear the large horns of the Endeavor as it enters Achaeus harbour!{x", SECT_CITY);
    }
    else
    // Boat leaves from Achaeus
    if (time == 18)
    {
	room = get_room_index(781122);

	rp_change_exit(room, "command delete", DIR_EAST);
	rp_change_exit(room, "command 781123", DIR_EAST);

	room = get_room_index(6551);
	rp_change_exit(room, "command delete", DIR_WEST);

	room_echo(room, "{YCaptain Pinot yells 'The Endeavor has left for Olaria!'{x\n\r");

	sector_echo(ship_area, "{YCaptain Pinot yells 'The Endeavor has left for Olaria!'{x", SECT_INSIDE);

	sector_echo(find_area("Achaeus"), "{YYou hear the large horns of the Endeavor as it leaves Achaeus harbour!{x", SECT_CITY);
    }
    else
    // Boat arrives in at Olaria
    if (time == 1)
    {
	room = get_room_index(656895);
	rp_change_exit(room, "command delete", DIR_EAST);
	rp_change_exit(room, "command 6551", DIR_EAST);

	room = get_room_index(6551);
	rp_change_exit(room, "command 656895", DIR_WEST);

	room_echo(room, "{YCaptain Pinot yells 'The Endeavor has arrived in Olaria!'{x\n\r");

	sector_echo(ship_area, "{YCaptain Pinot yells 'The Endeavor has arrived in Olaria!'{x", SECT_INSIDE);

	sector_echo(find_area("Olaria"), "{YYou hear the large horns of the Endeavor as it enters Olaria harbour!{x", SECT_CITY);
    }
    else
    // Boat leaves from Olaria
    if (time == 3)
    {
	room = get_room_index(656895);
	rp_change_exit(room, "command delete", DIR_EAST);
	rp_change_exit(room, "command 656896", DIR_EAST);

	room = get_room_index(6551);
	rp_change_exit(room, "command delete", DIR_WEST);

	room_echo(room, "{YCaptain Pinot yells 'The Endeavor has left for Plith!'{x\n\r");

	sector_echo(ship_area, "{YCaptain Pinot yells 'The Endeavor has left for Plith!'{x", SECT_INSIDE);

	sector_echo(find_area("Olaria"), "{YYou hear the large horns of the Endeavor as it leaves Olaria harbour!{x", SECT_CITY);
    }
#endif
}


// Update the time.
void time_update(void)
{
    char buf[MAX_STRING_LENGTH];
    DESCRIPTOR_DATA *d;
    int hours;

    buf[0] = '\0';

    time_info.hour++;

    // Update public boat.
    switch (time_info.hour)
    {
	case  1: // Boat arrives in Olaria
	    //update_public_boat(time_info.hour);
	    break;

	case  3: // Boat leaves from Olaria
	    //update_public_boat(time_info.hour);
	    break;

	case  5:
	    weather_info.sunlight = SUN_RISE;
	    strcat(buf, "The day has begun.\n\r");
	    break;

	case  6:
	    weather_info.sunlight = SUN_LIGHT;
	    strcat(buf, "The sun rises in the east.\n\r");
	    break;

	case 7: //Boat arrives in plith
	    //update_public_boat(time_info.hour);
	    break;

	case 9: //Boat leaves in plith
	    //update_public_boat(time_info.hour);
	    break;

	case 16: // Boat arrives in achaeus
	    //update_public_boat(time_info.hour);
	    break;

	case 18: // Boat leaves achaeus
	    //update_public_boat(time_info.hour);
	    break;

	case 19:
	    weather_info.sunlight = SUN_SET;
	    strcat(buf, "The sun slowly disappears in the west.\n\r");
	    break;

	case 20:
	    weather_info.sunlight = SUN_DARK;
	    strcat(buf, "{DThe night has begun.{x\n\r");
	    break;

	case 24:
	    time_info.hour = 0;
	    time_info.day++;
	    break;
    }

#if 0
    /*
     * This is for the PLITH Airship. After two hours it must be made to go back home.
     */
    if ( plith_airship != NULL )
    {
  if ( plith_airship->captain != NULL
  && plith_airship->captain->ship_arrival_time == time_info.hour
  && str_cmp( plith_airship->ship->ship->in_room->area->name, "Plith" ) )
  {
      AREA_DATA *plith = find_area( "Plith" );
//sprintf(buf, "Just added plith waypoint %ld %ld ",  plith->x, plith->y );
//log_string(buf);
      add_move_waypoint( plith_airship->ship, plith->x, plith->y );

//      sprintf(buf, "{YThe %s rises high into the sky then disappears!{x\n\r", plith_airship->ship->ship_name );
//      echo_around(plith_airship->ship->ship->in_room, buf);
 //     room_echo(plith_airship->ship->ship->in_room, buf);
				SHIP_STATE(plith_airship->captain, 8);
//gecho("moo");
      //gecho( "ITS GONE TO fly back home!" );

		//plith_airship->captain->ship_depart_time = 80;
	//	plith_airship->captain->ship_dest_x = plith->x;
//		plith_airship->captain->ship_dest_y = plith->y;
  }
  else
  {
      sprintf( buf, "Airship ship_arrival_time=%d. Area name='%s'. Captain exists=%s", plith_airship->captain->ship_arrival_time, plith_airship->ship->ship->in_room->area->name, plith_airship->captain==NULL ? "FALSE" : "TRUE" );
      log_string( buf );
  }
    }
#endif

    buf[0] = '\0';

    if (time_info.day   >= 35)
    {
	time_info.day = 0;
	time_info.month++;
    }

    if (time_info.month >= 12)
    {
	time_info.month = 0;
	time_info.year++;
    }

    hours = ((((time_info.year*12)+time_info.month)*35+time_info.day)*24+time_info.hour+MOON_OFFSET) % MOON_PERIOD;
    hours = (hours + MOON_PERIOD) % MOON_PERIOD;

    if(hours <= (MOON_CARDINAL_HALF)) time_info.moon = MOON_NEW;
    else if(hours < (MOON_CARDINAL_STEP - MOON_CARDINAL_HALF)) time_info.moon = MOON_WAXING_CRESCENT;
    else if(hours <= (MOON_CARDINAL_STEP + MOON_CARDINAL_HALF)) time_info.moon = MOON_FIRST_QUARTER;
    else if(hours < (2*MOON_CARDINAL_STEP - MOON_CARDINAL_HALF)) time_info.moon = MOON_WAXING_GIBBOUS;
    else if(hours <= (2*MOON_CARDINAL_STEP + MOON_CARDINAL_HALF)) time_info.moon = MOON_FULL;
    else if(hours < (3*MOON_CARDINAL_STEP - MOON_CARDINAL_HALF)) time_info.moon = MOON_WANING_GIBBOUS;
    else if(hours <= (3*MOON_CARDINAL_STEP + MOON_CARDINAL_HALF)) time_info.moon = MOON_LAST_QUARTER;
    else if(hours < (4*MOON_CARDINAL_STEP - MOON_CARDINAL_HALF)) time_info.moon = MOON_WANING_CRESCENT;
    else time_info.moon = MOON_NEW;

	if (!reckoning_timer && !pre_reckoning && reckoning_cooldown_timer < current_time &&
		time_info.moon == MOON_FULL &&
		weather_info.sunlight == SUN_DARK) {
		struct tm *reck_time = (struct tm *) localtime(&current_time);
		if (number_percent() < reckoning_chance) {
			// Success


			reck_time->tm_min += reckoning_duration;
		    reckoning_timer = (time_t) mktime(reck_time);
		    reckoning_cooldown_timer = 0;
			pre_reckoning = 1;

		} else {
		    reckoning_timer = 0;

			reck_time->tm_min += 10;	// This should make it through the night
			reckoning_cooldown_timer = (time_t) mktime(reck_time);

			reckoning_chance += 5;
			reckoning_chance = RECKONING_CHANCE(reckoning_chance);

			int rnd = number_range(-10,10);
			reckoning_intensity += UMAX(0, rnd);
			reckoning_intensity = RECKONING_INTENSITY(reckoning_intensity);

			rnd = number_range(-5,5);
			reckoning_duration += UMAX(0, rnd);
			reckoning_duration = RECKONING_DURATION(reckoning_duration);
		}
	}

	// If pre_reckoning > 0 then it is taking place
	if (reckoning_timer > 0 && pre_reckoning > 0) {
		if (pre_reckoning == 5) {
			sprintf(buf, "{MA thick purple hazy mist descends around as you the reckoning takes hold!{x\n\r"
				"{yYou feel a sudden urge to kill the innocent for personal gain!{x\n\r");
			pre_reckoning = 0;
			boost_table[BOOST_RECKONING].timer = reckoning_timer;
			boost_table[BOOST_RECKONING].boost = 100 + URANGE(10,reckoning_intensity,200);	// Allow from 110 to 300%
		} else {
			switch(pre_reckoning++) {
			case 1: sprintf(buf, "You notice a slight discolouration in the sky.\n\r"); break;
			case 2: sprintf(buf, "{MThe sky dims as dark purple and maroon clouds roll in out of nowhere.{x\n\r"); break;
			case 3: sprintf(buf, "{CA strong gust picks up, howling loudly as it rips across the land.{x\n\r"); break;
			case 4: sprintf(buf, "{BSheets of cold blue lightning gather in the sky as a demonic terror grips the world.{x\n\r"); break;
			}
		}
	}

	if (reckoning_timer > 0 && current_time > reckoning_timer) {
		gecho("{MAs quickly as it appeared, the hazy purple mist dissipates. The reckoning has ended.{x\n\r");
		reckoning_timer = 0;

		if( reckoning_chance > RECKONING_CHANCE_MAX_RESET )
			reckoning_chance /= 4;
		else
			reckoning_chance -= 5;
		reckoning_chance = RECKONING_CHANCE_RESET(reckoning_chance);

		reckoning_duration = RECKONING_DURATION_DEFAULT;
		reckoning_intensity = RECKONING_INTENSITY_DEFAULT;

		if( reckoning_cooldown > 0 )
		{
			struct tm *reck_time = (struct tm *) localtime(&current_time);
			reck_time->tm_min += UMAX(RECKONING_COOLDOWN_USE_MIN, reckoning_cooldown);
			reckoning_cooldown_timer = (time_t) mktime(reck_time);
		}
		else
			reckoning_cooldown_timer = 0;

		reckoning_cooldown = 0;

		boost_table[BOOST_RECKONING].boost = 100;
	}

    if (buf[0] != '\0')
    {
	for (d = descriptor_list; d != NULL; d = d->next)
	{
	    if (d->connected == CON_PLAYING
		    &&   (d->character->in_room != NULL &&
			d->character->in_room->sector_type != SECT_INSIDE)//IS_OUTSIDE(d->character)
		    && !IN_EDEN(d->character)
		    && !IN_NETHERWORLD(d->character)
		    &&   IS_AWAKE(d->character)) {
		send_to_char(buf, d->character);
	    }
	}
    }

  buf[0] = '\0';
	for (d = descriptor_list; d != NULL; d = d->next)
	{
	    if (pre_reckoning == 0 && reckoning_timer > 0)
	    {
		if (d->connected == CON_PLAYING
		&& d->character->in_room != NULL
		&& d->character->in_room->sector_type != SECT_INSIDE
		&&   IS_AWAKE(d->character))
		{
		    if (number_percent() < 50)
			sprintf(buf, "{YLightning crashes down around you!{x\n\r");
		    else
			sprintf(buf, "{YThe wind howls as it screams around you!{x\n\r");
		    send_to_char(buf, d->character);
		}
	}
    }
}


/*
 * After an npc ship has killed a ship it needs to continue with its waypoints.
 */
void reset_waypoint( NPC_SHIP_DATA *npc_ship )
{
#if 0
    long index = 0;
    SHIP_DATA *ship = npc_ship->ship;

    if ( ship->current_waypoint != NULL && !(ship->current_waypoint->x==0 && ship->current_waypoint->y==0))
    {
	index = (long)((long)ship->current_waypoint->y * ship->ship->in_room->area->map_size_x + (long)ship->current_waypoint->x + ship->ship->in_room->area->min_vnum + WILDERNESS_VNUM_OFFSET);
	ship->destination = get_room_index(index);
    }
#endif
}


/*
 * Update trade in areas.
 */
void update_area_trade( void )
{
    AREA_DATA *pArea = NULL;
    TRADE_ITEM *pItem = NULL;
    char buf[MAX_STRING_LENGTH];

    log_string( "Updating area trade data..." );
    for ( pArea = area_first; pArea != NULL; pArea = pArea->next )
    {
	if ( pArea->trade_list == NULL )
	{
	    continue;
	}

	for ( pItem = pArea->trade_list; pItem != NULL; pItem = pItem->next )
	{
	    int time = ++pItem->replenish_current_time;

	    if ( time >= pItem->replenish_time )
	    {
		pItem->replenish_current_time = 0;
		pItem->qty += pItem->replenish_amount;
		if ( pItem->qty < 0 )
		{
		    pItem->qty = 0;
		}

		if ( pItem->qty > pItem->max_qty )
		{
		    pItem->qty = pItem->max_qty;
		}

		/* Is the area a supplier of the trade good */
		if ( pItem->replenish_amount > 0 )
		{
		    /* As the quantity increases, the buy price drops */
		    pItem->buy_price = UMAX( (long) (pItem->max_price *
				((float)(pItem->max_qty - pItem->qty)/(float)pItem->max_qty)), pItem->min_price);

		    /* Sell price is 80% of buy price */
		    pItem->sell_price = (long) (pItem->buy_price * 0.8);

		    sprintf( buf, "Updating area %s, supplier trade item %s, to buy price %ld and sell price %ld, min price %ld max price %ld",
			    pArea->name, trade_table[ pItem->trade_type ].name, pItem->buy_price, pItem->sell_price, pItem->min_price, pItem->max_price );
		    log_string( buf );
		}
		else
		{
		    /* Otherwise we are a cosumer */
		    TRADE_ITEM *pTempTrade = NULL;
		    long max_qty = 0;
		    AREA_DATA *pTempArea = NULL;

		    // Look for largest max qty supplier in other areas and base sell price off that
		    for ( pTempArea = area_first; pTempArea != NULL; pTempArea = pTempArea->next )
		    {
			if ( pTempArea == pArea || abs(get_coord_distance(pArea->x, pArea->y, pTempArea->x, pTempArea->y)) > 400)
			{
			    continue;
			}

			for ( pTempTrade = pTempArea->trade_list; pTempTrade != NULL; pTempTrade = pTempTrade->next )
			{
			    if ( pTempTrade->trade_type == pItem->trade_type && pTempTrade->replenish_amount > 0 )
			    {
				if ( pTempTrade->qty > max_qty )
				{
				    max_qty = pTempTrade->qty;
				}
			    }
			}
		    }

		    // What happens if there is not supplier???
		    if ( pTempTrade == NULL )
		    {
			pTempTrade = pItem;
		    }

		    // If there its lots of stock at the supplier then lower the sell price (not as much demand).
		    pItem->sell_price = UMAX( (long) (pItem->max_price * ( (float) ( pTempTrade->max_qty - pTempTrade->qty ) / (float) pTempTrade->max_qty ) ), pItem->min_price );

		    // Buying is 20% more than sell price
		    pItem->buy_price =(long) (pItem->sell_price * 1.2);
		}

	    }
	}
    }
}

// Update all chars, including mobs
void char_update(void)
{
    ITERATOR it, tit;
    char buf[MSL];
    CHAR_DATA *ch;
    CHAR_DATA *ch_quit;
    AFFECT_DATA *paf;
    AFFECT_DATA *paf_next;
    TOKEN_DATA *token;
    TOKEN_DATA *token_next;
    ch_quit	= NULL;

    // Update save counter
	save_number++;
	if (save_number > 29)
		save_number = 0;

	iterator_start(&it, loaded_chars);
	while(( ch = (CHAR_DATA *)iterator_nextdata(&it)))
	{
		if (!IS_VALID(ch))
			continue;

		/* check if in_room is null for logging purposes */
		if (ch->in_room == NULL && IS_NPC(ch))
			sprintf(buf, "char_update: null in_room on ch %s (%ld)", ch->short_descr, ch->pIndexData->vnum);


		// Characters in social aren't updated
		if (IS_SOCIAL(ch))
			continue;

		// Update tokens on a character. Remove the one for which the timer has run out.
		iterator_start(&tit, ch->ltokens);
		while(( token = (TOKEN_DATA *)iterator_nextdata(&tit)))
		{

			if (IS_SET(token->flags, TOKEN_REVERSETIMER)) {
				++token->timer;
			} else if (token->timer > 0) {
				--token->timer;
				if (token->timer <= 0) {
					sprintf(buf, "char update: token %s(%ld) char %s(%ld) was extracted because of timer",
						token->name, token->pIndexData->vnum, HANDLE(ch), IS_NPC(ch) ? ch->pIndexData->vnum : 0);
					log_string(buf);
					p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_EXPIRE, NULL);
					token_from_char(token);
					free_token(token);
				}
			}
		}


		// Kick out people after they idle long enough
		if (ch->timer > 30)
			ch_quit = ch;

		if (ch->position >= POS_STUNNED) {
			// Stranded mobs are extracted after a while
			if (0 && IS_NPC(ch) && ch->desc == NULL &&
				ch->fighting == NULL && !IS_AFFECTED(ch,AFF_CHARM) &&
				ch->leader == NULL &&  ch->master == NULL &&
				ch->in_room != ch->home_room && !IS_SET(ch->act,ACT_SENTINEL) &&
				number_percent() < 1) {
				act("$n wanders on home.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				if(ch->home_room == NULL) {
					extract_char(ch, TRUE);
					continue;
				} else {
					char_from_room(ch);
					char_to_room(ch, ch->home_room);
				}
			}

			// Regen hit, mana and move.
			if (ch->hit < ch->max_hit)
				ch->hit += hit_gain(ch);
			else
				ch->hit = ch->max_hit;

			if (ch->mana < ch->max_mana)
				ch->mana += mana_gain(ch);
			else
				ch->mana = ch->max_mana;

			if (ch->move < ch->max_move)
				ch->move += move_gain(ch);
			else
				ch->move = ch->max_move;
		}

		if (ch->position == POS_STUNNED)
			update_pos(ch);

        // PCs drown in the water
		if (!IS_NPC(ch) && !IS_AFFECTED(ch, AFF_SWIM) &&
			!IS_SET(ch->parts, PART_GILLS) && !IS_SET(ch->imm_flags, IMM_WATER) &&
			IS_SET(ch->in_room->room_flags, ROOM_UNDERWATER) && !IS_IMMORTAL(ch)) {
			send_to_char("You choke and gag as your lungs fill with water!\n\r", ch);
			act("$n thrashes about in the water gasping for air!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			damage(ch, ch, ch->hit/2, TYPE_UNDEFINED, DAM_DROWNING,FALSE);
		}

		// Toxin regeneration for siths
		if (IS_SITH(ch))
		{
		    int i;
		    for (i = 0; i < MAX_TOXIN; i++)
				ch->toxin[i] += UMIN(toxin_gain(ch), 100 - ch->toxin[i]);
		}

        // Decrease challenge delay for people.
		if (!IS_NPC(ch) && ch->pcdata->challenge_delay > 0)
		    ch->pcdata->challenge_delay--;

		// Return people from the maze after a while.
		if (ch->maze_time_left > 0)
		{
			ch->maze_time_left--;
			if (ch->maze_time_left <= 0)
			{
				send_to_char("{WThe gods have returned you to the mortal realm.{x\n\r", ch);
				return_from_maze(ch);
			}
		}

		// Return from dead
		if (!IS_NPC(ch) && ch->time_left_death > 0)
		{
			bool run_death_timer = TRUE;
			OBJ_DATA *corpse = ch->pcdata->corpse;
			ROOM_INDEX_DATA *corpse_room = obj_room(corpse);
			// Check here, prevent the timer from counting down if the
			if( p_percent_trigger(ch, NULL, NULL, NULL, ch, ch, NULL, corpse, NULL, TRIG_DEATH_TIMER, NULL) )
				run_death_timer = FALSE;

			if( run_death_timer && IS_VALID(corpse) && p_percent_trigger(NULL, corpse, NULL, NULL, ch, ch, NULL, corpse, NULL, TRIG_DEATH_TIMER, NULL) )
				run_death_timer = FALSE;

			if( run_death_timer && corpse_room && p_percent_trigger(NULL, NULL, corpse_room, NULL, ch, ch, NULL, corpse, NULL, TRIG_DEATH_TIMER, NULL) )
				run_death_timer = FALSE;

			// Perform actions while the player is dead regardless of timer running
			if( ch->manastore > 0) {
				// Decay manastore while dead
				ch->manastore = ch->manastore / 2;
			}


			if( run_death_timer )
			{
				ch->time_left_death--;

				if (ch->time_left_death <= 0)
				{
					send_to_char("{WThe gods take pity on you and return you to your body.\n\r", ch);
					resurrect_pc(ch);
				}
			}
		}

		// Updates for NON-IMM players who aren't dead.
		if (!IS_NPC(ch) && ch->tot_level < LEVEL_IMMORTAL && !IS_DEAD(ch))
		{
		    OBJ_DATA *obj;

			// Decrease light
			if ((obj = get_eq_char(ch, WEAR_LIGHT)) != NULL &&
				obj->item_type == ITEM_LIGHT &&
				obj->value[2] > 0)
			{
				if (--obj->value[2] <= 0 && ch->in_room != NULL)
				{
					--ch->in_room->light;
					act("$p goes out.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
					act("$p flickers and goes out.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
					log_string("it went out");
					extract_obj(obj);
				}
				else if (obj->value[2] <= 5 && ch->in_room != NULL)
					act("$p flickers.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
			}

			// Limbo timer (doesn't apply to imms)
			if (IS_IMMORTAL(ch))
				ch->timer = 0;

			if (++ch->timer >= 12)
			{
				/* remove any PURGE_IDLE tokens on the character */
				for (token = ch->tokens; token != NULL; token = token_next)
				{
					token_next = token->next;
					if (IS_SET(token->flags, TOKEN_PURGE_IDLE))
					{
						p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_TOKEN_REMOVED, NULL);
						sprintf(buf, "char update: token %s(%ld) char %s(%ld) was purged on idle",
							token->name, token->pIndexData->vnum, HANDLE(ch), IS_NPC(ch) ? ch->pIndexData->vnum : 0);
						log_string(buf);
						token_from_char(token);
						free_token(token);
					}
				}

				// Idling will clear your manastore
				ch->manastore = 0;

				if (ch->was_in_room == NULL && ch->in_room != NULL)
				{
					ch->was_in_room = ch->in_room;

					if(ch->in_wilds) {
						ch->was_in_wilds = ch->in_wilds;
						ch->was_at_wilds_x = ch->at_wilds_x;
						ch->was_at_wilds_y = ch->at_wilds_y;
					}
					else if(ch->in_room->source)
					{
						ch->was_in_room_id[0] = ch->in_room->id[0];
						ch->was_in_room_id[1] = ch->in_room->id[1];
					}


					if (ch->fighting != NULL)
						stop_fighting(ch, TRUE);

					act("{D$n disappears into the void.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					send_to_char("{DYou disappear into the void.\n\r{x", ch);

					if (ch->level > 1)
						save_char_obj(ch);

					char_from_room(ch);
					char_to_room(ch, get_room_index(ROOM_VNUM_LIMBO));
				}
			}

			// Reckoning effects
			if (pre_reckoning == 0 && reckoning_timer > 0)
			{
				int num = number_range(0,5);
				int sn = skill_lookup("lightning bolt");
				//int attack_rand = number_percent();
				int lbdam;
				int lbchance = 5;


				lbdam = number_range(500,30000) * reckoning_intensity / 100;
				if ( reckoning_intensity > 100 )
				{
					lbchance = 5 + ((reckoning_intensity - 100) / 5);
				}

				if (ch->in_room != NULL &&
					ch->in_room->sector_type != SECT_INSIDE &&
					!IS_SET(ch->in_room->room_flags, ROOM_INDOORS))
				{
					switch(num)
					{
					case 0:
						act("{YLightning forks down into the earth from above.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
						break;
					case 1:
						act("{MThe wind howls loudly then knocks you to your knees!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
						ch->position = POS_RESTING;
						break;
					case 2:
						act("{MThe sky groans loudly as the clouds above swirl chaotically.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
						break;
					case 3:
						act("{YLightning crashes to the ground next to you!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
						break;
					case 4:
						if (number_percent() < lbchance && !IS_SET(ch->in_room->room_flags, ROOM_SAFE) && (IS_NPC(ch) || !IS_SET(ch->act2, PLR_NORECKONING)) && ch->fighting == NULL)
						{
							act("{YZAAAAAAAAAAAAAAP! You are struck by a bolt from the sky...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

							damage(ch, ch, lbdam, sn, DAM_LIGHTNING, FALSE);
						}
						break;
					case 5:
						act("{YThe wind screams around you, threatening to blow you over.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
						break;
					}
				}

			}

			// Vampires take sun damage
			if (IS_OUTSIDE(ch))
				hurt_vampires(ch);

			// Shifted slayer effects
			if (IS_SHIFTED_SLAYER(ch) && number_percent() < 10)
			{
				switch(number_range(0,4))
				{
					case 0:
					act("$n snorts and shakes some of the rancid mucus from $s body.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					break;
					case 1:
					act("$n's lets out a deep chilling growl.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					break;
					case 2:
					act("$n nibbles on $s long sharp claws.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					break;
					case 3:
					act("$n growls at you intimidatingly.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					break;
				}
			}

			// Slayers have a bad habit of attacking things.
			if (IS_SHIFTED_SLAYER(ch) && number_percent() < 25 && ch->fighting == NULL)
			{
				CHAR_DATA *player;

				// Find someone to SLAUGHTER
				for (player = ch->in_room->people; player != NULL; player = player->next_in_room)
				{
					if (player->fighting == NULL && !is_safe(ch, player,FALSE) && player->alignment < 150 && !is_same_group(player,ch))
						break;
				}

				if (player != NULL && ch != player)
				{
					act("$n snorts loudly then viciously attacks $N!", ch, player, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					act("You lash out at $N uncontrollably!", ch, player, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					set_fighting(ch, player);
				}
			}

		    // Anti-evil/anti-good items scorch and get dropped.
		    if (!IS_NPC(ch))
			    for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
			    {
					char buf[MAX_STRING_LENGTH];

					if ((ch->alignment < 0 && IS_OBJ_STAT(obj, ITEM_ANTI_EVIL)) ||
						(ch->alignment > 0 && IS_OBJ_STAT(obj, ITEM_ANTI_GOOD)))
					{
						sprintf(buf, "{R$n is scorched by %s!{x", obj->short_descr);
						act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
						sprintf(buf, "{RYou are scorched by %s!{x\n\r", obj->short_descr);
						send_to_char(buf, ch);

						if (obj->wear_loc != WEAR_NONE)
							remove_obj(ch, obj->wear_loc, TRUE);

						do_function(ch, &do_drop, obj->name);
						damage(ch, ch, obj->level, TYPE_UNDEFINED, DAM_NONE,FALSE);
					}
				}

			// No magical flying over the ocean.  Physical flight is ok
			if (ch->in_room->sector_type == SECT_WATER_NOSWIM &&
				ch->in_room->vnum != ROOM_VNUM_SEA_PLITH_HARBOUR &&
				ch->in_room->vnum != ROOM_VNUM_SEA_NORTHERN_HARBOUR &&
				ch->in_room->vnum != ROOM_VNUM_SEA_SOUTHERN_HARBOUR &&
				!IS_NPC(ch) && is_affected(ch, gsn_fly))
			{
				act("{MThe air sparks as the ocean's magical shield dispels your ability to fly.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("You plummet into the ocean.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("{MThe air around $n sparks, $n plummets into the ocean.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				affect_strip(ch, gsn_fly);
			}

			// If they are physically flying... drain movement slowly
			if (!IS_NPC(ch) && is_affected(ch, gsn_flight))
			{
				bool fall = FALSE;
				int amount, weight;
				char *reason = "Feeling exhausted";

				// Lacking WINGS?
				if(!IS_SET(ch->parts,PART_WINGS)) {
					reason = "Lacking the ability to stay airborne";
					fall = TRUE;
				} else {
					amount = get_curr_stat(ch,STAT_CON);
					amount = URANGE(3,amount,50);
					amount = number_range(10,500/amount);	// con(3)=[1,16.7], con(50)=[1,1]
					amount = UMAX(1,amount);

					weight = get_carry_weight(ch);
					if(RIDDEN(ch)) weight += get_carry_weight(RIDDEN(ch)) + size_weight[RIDDEN(ch)->size]; // plus weight of rider

					// if the weight is too high, it uses more...
					if(!IS_IMMORTAL(ch) && (number_range(0,can_carry_w(ch))) < weight)
						amount = 3 * amount / 2;

					// athletics

					// other things?

					ch->move -= amount/10;
					ch->move = UMAX(0,ch->move);

					if(number_range(0,ch->max_move/get_curr_stat(ch,STAT_CON)) > ch->move) {
						reason = "Exhausted from flying";
						fall = TRUE;
					}
				}

				if(fall) {
					affect_strip(ch,gsn_flight);
					if(	ch->in_room->sector_type == SECT_WATER_NOSWIM ||
						ch->in_room->sector_type == SECT_WATER_SWIM ||
						ch->in_room->sector_type == SECT_UNDERWATER ||
						ch->in_room->sector_type == SECT_DEEP_UNDERWATER) {
						act("$t, you plummet into the water below.", ch, NULL, NULL, NULL, NULL, reason, NULL, TO_CHAR);
						act("$t, $n plummets into the water below.", ch, NULL, NULL, NULL, NULL, reason, NULL, TO_ROOM);
						damage(ch, ch, number_range(10,100), TYPE_UNDEFINED, IS_AFFECTED(ch,AFF_SWIM)?DAM_WATER:DAM_DROWNING, FALSE);
						if(RIDDEN(ch)) damage(RIDDEN(ch), RIDDEN(ch), number_range(10,100), TYPE_UNDEFINED, IS_AFFECTED(RIDDEN(ch),AFF_SWIM)?DAM_WATER:DAM_DROWNING, FALSE);
					} else {
						act("$t, you plummet to the ground below.", ch, NULL, NULL, NULL, NULL, reason, NULL, TO_CHAR);
						act("$t, $n plummets to the ground below.", ch, NULL, NULL, NULL, NULL, reason, NULL, TO_ROOM);
						damage(ch, ch, number_range(20,250), TYPE_UNDEFINED, DAM_BASH, FALSE);
						if(RIDDEN(ch)) damage(RIDDEN(ch), RIDDEN(ch), number_range(20,250), TYPE_UNDEFINED, DAM_BASH, FALSE);
					}
				}
		    }


			// Drown them!
			if (ch->in_room->sector_type == SECT_WATER_NOSWIM && !IS_NPC(ch) &&
				ch->move <= 50 && !IS_AFFECTED(ch, AFF_FLYING))
			{
				act("Completely exhausted, you find little energy to keep swimming.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("Completely exhausted, $n stops swimming from lack of energy.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

				if (!IS_AFFECTED(ch, AFF_SWIM))
				{
				    act("You cough and splutter as you breath in a lung full of water.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				    act("$n coughs and splutters as $s breaths in a lung full of water.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

				    damage(ch, ch, 30000, TYPE_UNDEFINED, DAM_DROWNING, FALSE);
				}
		    }

			// Fire off deathtraps.
		    if (IS_SET(ch->in_room->room_flags, ROOM_DEATH_TRAP) &&
		    	!IS_SET(ch->in_room->room_flags, ROOM_CPK)) {		// no cpk-deathtraps
					raw_kill(ch, TRUE, FALSE, RAWKILL_NORMAL);
		    }

			// The enchanted forest saps hit,mana, and move.
		    if (ch->in_room->sector_type == SECT_ENCHANTED_FOREST && ch->position == POS_SLEEPING)
	    	{
				ch->hit = ch->hit - ch->max_hit/3;

				update_pos(ch);

				if (ch->hit <= 0)
				{
				    send_to_char("You feel yourself disintegrate into dust.\n\r", ch);
				    act("$n disintegrates into dust.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				    raw_kill(ch, FALSE, TRUE, RAWKILL_INCINERATE);
				}

				ch->mana = ch->mana - ch->max_mana/4;
				ch->move = ch->move - ch->max_move/4;

				ch->mana = UMAX(ch->mana, 0);
				ch->move = UMAX(ch->move, 0);
		    }

		    // non-demons without a light in the demon area get fucked
		    if (IS_SET(ch->in_room->room_flags, ROOM_ATTACK_IF_DARK) && !IS_DEMON(ch))
		    {
				if (!IS_SET(ch->act, PLR_HOLYLIGHT))
				{
				    // No light = death
				    if (room_is_dark(ch->in_room))
				    {
						send_to_char("{RSomething bites you on the ass really hard.{x\n\r", ch);
						damage(ch, ch, ch->max_hit, TYPE_UNDEFINED, DAM_NONE,FALSE);
				    }
				    else // Creepy messages
				    {
						int num = number_percent();
						if (num < 10)
							send_to_char("{YThere is a sudden flapping as something takes off out of sight.{x\n\r", ch);
						else if (num < 20)
							send_to_char("{DYour heart pounds rapidly as you hear sounds from the darkness...{x\n\r", ch);
						else if (num < 30)
							send_to_char("{YA sudden chill runs up your spine.\n\r{x", ch);
						else if (num < 40)
							send_to_char("{CSomeone whispers 'Tuuuurn offff yoour liiight mooortal.'{x\n\r", ch);
						else if (num < 50)
							send_to_char("{RProwling, inhuman eyes materialize from the shadows, then flit away at the sight of your light.{X\n\r", ch);
						else if (num < 60)
							send_to_char("{YYou feel as if someone or something is following you.{x\n\r", ch);
						else if (num < 70)
							send_to_char("You feel something brush past your shoulder.\n\r", ch);
						else
							send_to_char("{YYour light flickers momentarily.{x\n\r", ch);
				    }
				}
		    }

			// Hints for newbs
		    if (!IS_SET(ch->comm, COMM_NOHINTS))
		    {
				char buf[MAX_STRING_LENGTH];

				send_to_char("{MHint: ", ch);
			 	sprintf(buf, "%s", hintsTable[number_percent() % 15].hint);
				send_to_char(buf, ch);
				send_to_char("{x", ch);
		    }

			// Update conditions
			gain_condition(ch, COND_DRUNK, -1);
			gain_condition(ch, COND_FULL, -1);
			gain_condition(ch, COND_STONED, -1);
			gain_condition(ch, COND_THIRST, -1);
			gain_condition(ch, COND_HUNGER, -1);
		}

		// Update affects on the character
		for (paf = ch->affected; paf != NULL; paf = paf_next)
		{
		    paf_next = paf->next;

		    if (paf->duration > 0) // spells with a finite duration
		    {
				paf->duration--;
				if (number_range(0,4) == 0 && paf->level > 0)
			  		paf->level--;  // spell strength fades with time
			}
		    else if (paf->duration < 0) // infinite spells, like on eq
		    {
				;
		    }
		    else // remove worn-out spells
		    {
				if (paf_next == NULL || paf_next->type != paf->type ||
					paf_next->duration > 0)
				{
		    		if (paf->type > 0 && skill_table[paf->type].msg_off)
		    		{
						send_to_char(skill_table[paf->type].msg_off, ch);
						send_to_char("\n\r", ch);
		    		}
				}

				affect_remove(ch, paf);
		    }
		}

		if (ch->fighting == NULL)
		    update_has_done(ch);

		// Scary people can make others flee.
		if (can_scare(ch))
		    scare_update(ch);

		/* Toggle off builder flag for people who haven't built in 30 minutes. */
		if (!IS_NPC(ch) && IS_IMMORTAL(ch) && IS_SET(ch->act, PLR_BUILDING)) {
		    if ((current_time - ch->pcdata->immortal->last_olc_command)/60 >= MAX_BUILDER_IDLE_MINUTES) {
				sprintf(buf, "%d minutes have passed for %s without any OLC commands; toggling off builder flag.\n\r",
					MAX_BUILDER_IDLE_MINUTES, ch->name);
				wiznet(buf, NULL, NULL, WIZ_BUILDING, 0, 0);
				REMOVE_BIT(ch->act, PLR_BUILDING);
	    	} else  // Increment #minutes built by 1
				ch->pcdata->immortal->builder->minutes++;
		}

		// Effects of poison.
		if (IS_AFFECTED(ch, AFF_POISON) && !IS_AFFECTED(ch, AFF_SLOW))
		{
			AFFECT_DATA *poison;

			poison = affect_find(ch->affected,gsn_poison);

		    if (poison != NULL)
		    {
				act("$n shivers and suffers.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				send_to_char("You shiver and suffer.\n\r", ch);
				ch->set_death_type = DEATHTYPE_TOXIN;
				damage(ch, ch, poison->level/10 + 1, gsn_poison, DAM_POISON, FALSE);
		    }
		}
		// Folks on the verge of death eventually go the whole way w/o help.
		else if (ch->position == POS_INCAP && number_range(0,1) == 0)
		    damage(ch, ch, 1, TYPE_UNDEFINED, DAM_NONE,FALSE);
		else if (ch->position == POS_MORTAL)
		    damage(ch, ch, 1, TYPE_UNDEFINED, DAM_NONE,FALSE);
    }


    // Autosave and autoquit. Check that these chars still exist.
	iterator_start(&it, loaded_chars);
	while(( ch = (CHAR_DATA *)iterator_nextdata(&it)))
    {

		if (ch->desc != NULL && ch->desc->descriptor % 15 == save_number)
		    save_char_obj(ch);

        if (ch == ch_quit)
            do_function(ch, &do_quit, NULL);
    }
    iterator_stop(&it);
}


// Update all objs (performance-sensitive)
void obj_update(void)
{
	ITERATOR it, tit;
	TOKEN_DATA *token;
	OBJ_DATA *obj;
	AFFECT_DATA *paf, *paf_next;
	CHAR_DATA *rch, *rch_next;
	char *message;
	char buf[MAX_STRING_LENGTH];
	bool nuke_obj;
	int spill_contents;
	long uid[2];

	log_string("Update objects...");

	iterator_start(&it, loaded_objects);
	while(( obj = (OBJ_DATA *)iterator_nextdata(&it))) {

		if( !IS_VALID(obj) ) continue;

		// Unmarked objects in the rift will not update.
		// Principle objects that are normally allowed to tick in the rift: room spell objects
		ROOM_INDEX_DATA *cur_room = obj_room(obj);
		if( cur_room != NULL && cur_room->area->area_who == AREA_CHAT && !IS_SET(obj->extra3_flags, ITEM_RIFT_UPDATE))
			continue;


		// Adjust obj affects - except for people in social
		if (obj->carried_by == NULL || !IS_SOCIAL(obj->carried_by)) {
			for (paf = obj->affected; paf != NULL; paf = paf_next) {
				paf_next = paf->next;

				if (paf->duration > 0) {
					paf->duration--;

					// Affect strength fades with time
					if (number_range(0,4) == 0 && paf->level > 0)
					paf->level--;

				// Affect wears off, send message if applicable
				} else if (!paf->duration) {
					if (!paf_next || paf_next->type != paf->type || paf_next->duration > 0) {
						if (paf->type > 0 && skill_table[paf->type].msg_obj) {
							if (obj->carried_by != NULL) {
								rch = obj->carried_by;
								act(skill_table[paf->type].msg_obj, rch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
							} else if (obj->in_room && obj->in_room->people) {
								rch = obj->in_room->people;
								act(skill_table[paf->type].msg_obj, rch, NULL, NULL, obj, NULL, NULL, NULL, TO_ALL);
							}
						}
					}
					affect_remove_obj(obj, paf);
				}
			}
		}

		uid[0] = obj->id[0];
		uid[1] = obj->id[1];
		// Oprog triggers - need a room to function in
		if (obj_room(obj) != NULL) {
			if (obj->progs->delay > 0) {
				if (--obj->progs->delay <= 0)
					p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_DELAY, NULL);

				// Make sure the object is still there before proceeding
				if(!obj->valid || obj->id[0] != uid[0] || obj->id[1] != uid[1])
					continue;
			}

			if (!obj->locker)
			{

				p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RANDOM, NULL);

				// Prereckoning
				if (pre_reckoning > 0 && reckoning_timer > 0)
					p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_PRERECKONING, NULL);

				// Reckoning
				if (!pre_reckoning && reckoning_timer > 0)
					p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RECKONING, NULL);
			}
		}

		// Make sure the object is still there before proceeding
		if(!obj->valid || obj->id[0] != uid[0] || obj->id[1] != uid[1])
			continue;

		// Update tokens on object. Remove the one for which the timer has run out.
		iterator_start(&tit, obj->ltokens);
		while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
			if (IS_SET(token->flags, TOKEN_REVERSETIMER)) {
				++token->timer;
			} else if (token->timer > 0) {
				--token->timer;
				if (token->timer <= 0) {
					sprintf(buf, "obj update: token %s(%ld) obj %s(%ld) was extracted because of timer",
							token->name, token->pIndexData->vnum, obj->short_descr, obj->pIndexData->vnum);
					log_string(buf);
					p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_EXPIRE, NULL);
					token_from_obj(token);
					free_token(token);
				}
			}
		}
		iterator_stop(&tit);

		// Make seeds grow.
		if (obj->item_type == ITEM_SEED && obj->in_room != NULL && IS_OBJ_STAT(obj, ITEM_PLANTED)) {
			obj->value[0]--;
			if (obj->value[0] <= 0) {

				// Force this object to prevent its own destruction as it will be destroyed by default
				SET_BIT(obj->progs->entity_flags,PROG_NODESTRUCT);

				if( !p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_GROW, NULL) )
				{
					if (obj->value[1] == 0)
						bug("Seed has 0 vnum.", obj->pIndexData->vnum);
					else {
						OBJ_DATA *new_obj;
						if (get_obj_index(obj->value[1]) == NULL) {
							bug("Seed is buggered. Value 1 doesn't match anything:", obj->pIndexData->vnum);
							continue;
						}

						new_obj = create_object(get_obj_index(obj->value[1]), obj->level, TRUE);
						obj_to_room(new_obj, obj->in_room);

						p_percent_trigger(NULL, new_obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
					}
				}
				extract_obj(obj);
			}
		}

		// Ice storms - work in PK rooms
		if (obj->in_room != NULL && obj->item_type == ITEM_ICE_STORM && is_room_pk(obj->in_room, TRUE)) {
			for (rch = obj->in_room->people; rch != NULL; rch = rch_next) {
				rch_next = rch->next_in_room;

				switch (check_immune(rch, DAM_COLD)) {
				case IS_IMMUNE:
					break;

				case IS_RESISTANT:
					act("You shiver a bit, but are able to withstand the cold.", rch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("$n shivers a bit, but is able to withstand the cold.",  rch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					break;

				default:
					act("You shiver from the intense ice storm.", rch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("$n shivers from the intense ice storm.", rch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					break;
				}
			}

			cold_effect(obj->in_room, 1, dice(4,8), TARGET_ROOM);
		}

		// Handle timers for decaying objs, etc
		if ((obj->timer <= 0 || --obj->timer > 0))
			continue;

		nuke_obj = TRUE;
		spill_contents = 100;

		switch (obj->item_type)
		{
		// Simple messages
		default:					message = "$p crumbles into dust."; break;
		case ITEM_FOUNTAIN:			message = "$p dries up."; break;
		case ITEM_ROOM_FLAME:		message = "{DThe flames die down and disappear.{x"; break;
		case ITEM_ROOM_DARKNESS:	message = "{YThe light returns.{x"; break;
		case ITEM_ROOM_ROOMSHIELD:	message = "{YThe energy field shielding the room fades away.{X"; break;
		case ITEM_STINKING_CLOUD:	message = "{YThe poisonous haze disappears.{x"; break;
		case ITEM_WITHERING_CLOUD:	message = "{YThe poisonous haze disappears.{x"; break;
		case ITEM_FOOD:				message = "$p decomposes."; break;
		case ITEM_ICE_STORM:		message = "{W$p dies down and melts.{x"; break;
		case ITEM_POTION:			message = "$p has evaporated from disuse.";break;
		case ITEM_TATTOO:			message = "$p fades away as the ink dries.";break;
		case ITEM_PORTAL:			message = "$p fades out of existence."; break;

		// Corpse decaying
		case ITEM_CORPSE_NPC:
		case ITEM_CORPSE_PC:
			message = corpse_info_table[CORPSE_TYPE(obj)].decay_message;

			if (obj->carried_by)
				act(message, obj->carried_by, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			else if (obj->in_room && obj->in_room->people)
				act(message, obj->in_room->people, NULL, NULL, obj, NULL, NULL, NULL, TO_ALL);
			message = NULL;

			if(corpse_info_table[CORPSE_TYPE(obj)].decay_type != RAWKILL_NOCORPSE) {
				spill_contents = corpse_info_table[CORPSE_TYPE(obj)].decay_spill_chance;
				set_corpse_data(obj,corpse_info_table[CORPSE_TYPE(obj)].decay_type);
				spill_contents += corpse_info_table[CORPSE_TYPE(obj)].decay_spill_chance;
				spill_contents /= 2;	// Split the difference
				nuke_obj = FALSE;
			}
			break;

		case ITEM_CONTAINER:
			if (CAN_WEAR(obj,ITEM_WEAR_FLOAT)) {
				if (obj->contains)
					message = "$p flickers and vanishes, spilling its contents on the floor.";
				else
					message = "$p flickers and vanishes.";
			} else
				message = "$p crumbles into dust.";
			break;
		}

		// Do we have any message to process?
		if(!IS_NULLSTR(message)) {
			if (obj->carried_by)
				act(message, obj->carried_by, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			else if (obj->in_room && obj->in_room->people)
				act(message, obj->in_room->people, NULL, NULL, obj, NULL, NULL, NULL, TO_ALL);
		}

		// Send the contents of decaying corpses somewhere, depending on where the corpse is.
		if (spill_contents > 0 && (obj->item_type == ITEM_CORPSE_PC || obj->item_type == ITEM_CORPSE_NPC) && obj->contains) {
			OBJ_DATA *t_obj, *next_obj;

			for (t_obj = obj->contains; t_obj != NULL; t_obj = next_obj) {
				next_obj = t_obj->next_content;

				if(spill_contents >= 100 || number_percent() < spill_contents) {
					obj_from_obj(t_obj);

					if (obj->in_obj) // in another object
						obj_to_obj(t_obj,obj->in_obj);
					else if (obj->carried_by)
						obj_to_char(t_obj,obj->carried_by);
					else if (obj->in_room != NULL) // to the room
						obj_to_room(t_obj,obj->in_room);
					else { // junk it
						bug("obj_update: decaying corpse room was null!@!# extracted", 0);
						extract_obj(t_obj);
					}
				}
			}
		}

		if (nuke_obj && obj) extract_obj(obj);
	}

	iterator_stop(&it);
}


/*
 * Aggress.
 *
 * for each mortal PC
 *     for each mob in room
 *         aggress on some random PC
 *
 */
void aggr_update(void)
{
    CHAR_DATA *wch;
    CHAR_DATA *ch;
    CHAR_DATA *ch_next;
    CHAR_DATA *vch;
    CHAR_DATA *vch_next;
    CHAR_DATA *victim;
    OBJ_DATA *obj;
    AFFECT_DATA *paf, *tox, af;
    char buf[MAX_STRING_LENGTH];
    ITERATOR it;

    memset(&af,0,sizeof(af));

    iterator_start(&it, loaded_chars);
    while(( wch = (CHAR_DATA *)iterator_nextdata(&it)))
    {

	// if NPC then this is a good place to update casting as aggr_update runs frequently
	if (IS_NPC(wch))
	{
	    if (wch->cast > 0)
	    {
			wch->cast--;
			if (wch->cast <= 0)
			{
			    wch->cast = 0;
			    cast_end(wch);
			} else if(wch->cast_token && IS_SET(wch->cast_token->pIndexData->flags, TOKEN_SPELLBEATS))
 				p_percent_trigger(NULL, NULL, NULL, wch->cast_token, wch, NULL, NULL, NULL, NULL, TRIG_SPELLBEAT, NULL);

	    }

	    if (wch->script_wait > 0)
	    {
			wch->script_wait--;
			if (wch->script_wait <= 0)
			{
			    script_end_success(wch);
			}
			else
				script_end_pulse(wch);
	    }


	    if (wch->bashed > 0)
	    {
		--wch->bashed;
		if (wch->bashed <= 0)
		{
		    send_to_char("You scramble to your feet!\n\r", wch);
		    act("$n scrambles to $s feet.", wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		    wch->on = NULL;

		    if (wch->fighting != NULL)
			wch->position = POS_FIGHTING;
		    else
			wch->position = POS_STANDING;
		}
	    }

	    if (wch != NULL && wch->ranged > 0)
	    {
		--wch->ranged;
		if (wch->fighting != NULL && number_percent() > get_ranged_skill(wch) + get_curr_stat(wch, STAT_DEX))
		{
		    send_to_char("You lose your aim and lower your weapon.\n\r", wch);
		    wch->ranged = 0;
		}
		else
		    if (wch->ranged <= 0)
			ranged_end(wch);
	    }
	}

	if (IS_NPC(wch) && IS_SET(wch->act2, ACT2_TAKES_SKULLS)
	&&  wch->in_room->contents != NULL)
	{
	    int i;

            i = 0;
	    for (obj = wch->in_room->contents; obj != NULL; obj = obj->next_content)
	    {
		if (is_name("corpse", obj->name))
		    i++;

		if (number_percent() < 95)
		    continue;

		if (obj->item_type == ITEM_CORPSE_PC && IS_SET(CORPSE_PARTS(obj),PART_HEAD))
		{
		    sprintf(buf, "%d.corpse", i);
		    do_function(wch, &do_skull, buf);
		}
	    }
	}

	if (wch->bitten > 0 && number_percent() >
	    (get_curr_stat(wch, STAT_CON) - (wch->bitten_level - wch->tot_level)/3))
	    bitten_update(wch);

	// @@@NIB : 20070126 --------
	if(wch->in_room) {
		int chance = 0;
 		if(!IS_IMMORTAL(wch)) {
			if((tox = affect_find(wch->affected,gsn_toxic_fumes))) {
				int cough = FALSE;
				// is the mobile in a Toxic Bog?
				if(wch->in_room &&
					(IS_SET(wch->in_room->room2_flags, ROOM_TOXIC_BOG) ||
					(wch->in_room->sector_type == SECT_TOXIC_BOG))) {
					bool dec;

					if(IS_SET(wch->in_room->room2_flags, ROOM_TOXIC_BOG)) chance += 10;
					if(wch->in_room->sector_type == SECT_TOXIC_BOG) chance += 10;

					dec = (number_percent() < chance);
					for(paf = tox;paf;paf = paf->next)
						if (paf->type == gsn_toxic_fumes) {
							if(paf->duration > 0)	// Switch all non-permanent affects to permanent
								paf->duration = -paf->duration;
							else if(!paf->duration)
								paf->duration = -1;
							else if(dec && paf->duration > -100)	// Make permanent affects "longer"
								--paf->duration;
							if(paf->level < 120)
								paf->level = 120;
						}

					// Coughing messages
					if(number_range(0,199) < chance) {
						cough = TRUE;
						if(number_percent() < 50)
							send_to_char("{xYou cough uncontrollably from the toxic fumes.\n\r", wch);
						else
							send_to_char("{xYou inhale the toxic fumes, coughing uncontrollably.\n\r", wch);
						act("{x$n coughs uncontrollably from toxic fumes.", wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					}
				} else {
					// Change all affects to non-permanent
					for(paf = tox;paf;paf = paf->next)
						if (paf->type == gsn_toxic_fumes && paf->duration < 0)
							paf->duration = -paf->duration;

					// Coughing messages
					if(number_percent() < 4) {
						cough = TRUE;
						send_to_char("{xYou cough uncontrollably.\n\r", wch);
						act("{x$n coughs uncontrollably.", wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					}
				}

				// Do cough lag and damage
				if(cough) {
					cough = number_range(15,25);
					DAZE_STATE(wch,cough);
					cough = number_range(10,cough-1);
					WAIT_STATE(wch,cough);

					if(number_percent() < 50)
						damage(wch, wch, number_range(5,10), gsn_toxic_fumes, DAM_NONE, FALSE);
				}
			} else {
				if(IS_SET(wch->in_room->room2_flags, ROOM_TOXIC_BOG)) chance += 50;
				if(wch->in_room->sector_type == SECT_TOXIC_BOG) chance += 50;

				if(chance > 0 && number_percent() < chance)
					toxic_fumes_effect(wch,NULL);
			}
		}

		if(IS_SET(wch->in_room->room2_flags, ROOM_DRAIN_MANA)) {
			wch->mana -= number_range(5,15);
			if(wch->in_room->sector_type == SECT_CURSED_SANCTUM)
				wch->mana -= number_range(5,15);
			if(wch->mana < 0) wch->mana = 0;
			if(!number_percent())
				send_to_char("You feel your magical essense slipping away from you.\n\r", wch);
		}

		chance = 0;
		if(IS_SET(wch->in_room->room2_flags, ROOM_BRIARS)) chance += 5;
		if(wch->in_room->sector_type == SECT_BRAMBLE) chance += 5;

		if(chance > 0 && number_percent() < chance) {
			if(number_percent() < 2)
				send_to_char("{gThe sharp thorns scratch at your skin.{x\n\r", wch);
			damage(wch, wch, number_range(chance/2,chance), TYPE_UNDEFINED, DAM_NONE, FALSE);
		}
	}
	// @@@NIB : 20070126 --------


	if (wch->paralyzed > 0)
	{
	    --wch->paralyzed;
	    if (wch->paralyzed <= 0)
	    {
		send_to_char("You feel the power of movement coming back to your muscles.\n\r", wch);
		act("$n feels the power of movement coming back to $s muscles.",
			wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		wch->paralyzed = 0;
	    }
	}

	// This is for inferno, withering cloud, etc.
	if (wch->in_room != NULL
	&&  number_percent() < 10
	&&  !is_safe(wch, wch, FALSE)
	&&  ((IS_NPC(wch) && wch->shop == NULL) ||
	    (IS_SET(wch->in_room->room_flags, ROOM_PK)
	     || IS_SET(wch->in_room->room_flags, ROOM_CPK))
   	     || is_pk(wch)))
	{
	    for (obj = wch->in_room->contents; obj != NULL; obj = obj->next_content)
	    {
		// Room flames (inferno)
		if (obj->item_type == ITEM_ROOM_FLAME && !IS_SET(wch->in_room->room_flags, ROOM_SAFE))
		{
		    if (number_percent() <= 2)
		    {
			act("{RYou are scorched by flames!{x",
				wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{R$n is scorched by flames!{x",
				wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			damage(wch, wch, number_range(50, 500),
				TYPE_UNDEFINED, DAM_FIRE, FALSE);
		    }
		    else
		    if (number_percent() <= 2)
		    {
			/* Don't apply the blind affect twice */
			if (!IS_SET(wch->affected_by, AFF_BLIND)) {

			    act("{DYou are blinded by smoke!{x",
				    wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			    act("{D$n is blinded by smoke!{x",
				    wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

				af.slot	= WEAR_NONE;
			    af.where     = TO_AFFECTS;
			    af.group     = AFFGROUP_PHYSICAL;
			    af.type      = gsn_blindness;
			    af.level     = obj->level;
			    af.location  = APPLY_HITROLL;
			    af.modifier  = -4;
			    af.duration  = 2;
			    af.bitvector = AFF_BLIND;
			    af.bitvector2 = 0;
			    affect_to_char(wch, &af);
			}
		    }
		    else
		    if (number_percent() <= 2)
		    {
			act("{RYou are scorched by flames!{x",
				wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{R$n is scorched by flames!{x",
				wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			fire_effect((void *) wch,obj->level,	number_range(0, wch->tot_level * 10),TARGET_CHAR);
		    }
		}
		else
		// Withering clouds (wither spell)
		if (obj->item_type == ITEM_WITHERING_CLOUD)
		{
		    if (number_percent() <= 2)
		    {
			act("You splutter and gag!", wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$n splutters and gags!", wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		    }
		    else
		    if (number_percent() <= 2)
		    {
			act("You cough and splutter!", wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$n coughs and splutters violently!", wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		    }
		    if (number_percent() <= 2 && wch->fighting == NULL
		    && IS_AWAKE(wch) && wch->position == POS_STANDING
		    &&  !(IS_NPC(wch) && (IS_SET(wch->act,ACT_PROTECTED) || wch->shop != NULL))
		    &&  !(!IS_NPC(wch) && IS_IMMORTAL(wch)))
		    {
			act("$n stumbles about choking and gagging!",
				wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			act("You stumble about choking and gagging!",
				wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			do_function(wch, &do_flee, NULL);
		    }
		    else
		    if (number_percent() <= 2)
		    {
			act("$n is blinded by the toxic haze!",
				wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			act("You are blinded by the toxic haze around you!",
				wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			af.slot	= WEAR_NONE;
			af.where     = TO_AFFECTS;
			af.type      = gsn_blindness;
			af.level     = obj->level;
			af.location  = APPLY_HITROLL;
			af.modifier  = -4;
			af.duration  = 2; //1+level > 3 ? 3 : 1+level;
			af.bitvector = AFF_BLIND;
			af.bitvector2 = 0;
			affect_to_char(wch, &af);
		    }
		    else
		    if (number_percent() <= 2
		    && check_immune(wch, DAM_POISON) != IS_IMMUNE)
		    {
			act("$n is poisoned by the toxic haze!",
				wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			act("You are poisoned by the toxic haze around you!",
				wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			af.slot	= WEAR_NONE;
			af.where     = TO_AFFECTS;
			af.type      = gsn_poison;
			af.level     = obj->level * 3/4;
			af.duration  = URANGE(1,obj->level / 2, 5);
			af.location  = APPLY_STR;
			af.modifier  = -1;
			af.bitvector = AFF_POISON;
			af.bitvector2 = 0;
			affect_to_char(wch, &af);
		    }
		    else
		    if (number_percent() <= 2)
			acid_effect((void *)wch,obj->level,number_range(0, wch->tot_level * 10),TARGET_CHAR);
		}
		else
		// Stinking clouds (smoke bombs)
		if (obj->item_type == ITEM_STINKING_CLOUD)
		{
		    if (number_percent() <= 3)
		    {
			CHAR_DATA *victim, *vnext;

			for (victim = wch->in_room->people; victim != NULL; victim = vnext)
			{
			    vnext = victim->next_in_room;

			    if (IS_NPC(victim))
			    {
				if (victim->shop != NULL
				||   IS_SET(victim->act, ACT_PROTECTED	)
				||   IS_SET(victim->act, ACT_SENTINEL	))
				    continue;
			    }

			    if (victim->tot_level <= obj->level
			    &&  (victim->tot_level > 30 || IS_REMORT(victim))// no newbs!
			    &&  victim->fighting == NULL
			    &&  victim->position == POS_STANDING
			    &&  number_percent() < 20)
			    {
				act("{GYou choke on the acrid fumes from $p!{x", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
				act("{G$n chokes on the acrid fumes from $p!{x", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
				do_flee(victim, NULL);
			    }
			}
		    }
		}
	    }
	}

	/*
	if (IS_NPC(wch) && wch->boarded_ship != NULL)
	{
	    CHAR_DATA *captain;
	    if (IS_NPC_SHIP(wch->boarded_ship))
	    {
		captain = wch->boarded_ship->npc_ship->captain;
	    }
	    else
	    {
		captain = wch->boarded_ship->owner;
	    }

	    // If captain null then ship is scuttled
	    if (wch->belongs_to_ship != wch->boarded_ship && captain == NULL)
	    {
		//    gecho("Ship scuttled");
	    }

	    // It could be the mobs ship which is scuttled!
	    if (wch->belongs_to_ship == wch->boarded_ship && captain == NULL)
	    {
		//  gecho("Mobs ship scuttled");
	    }
	}

	// If mob isn't fighting and has boarded a ship then find enemy
	if (IS_NPC(wch)
	&&  wch->fighting == NULL
	&&  wch->hunting == NULL
	&&  wch->boarded_ship != NULL)
	{
	    int i;
	    bool no_enemy_left = TRUE;

	    for (i = 0; i < MAX_SHIP_ROOMS; i++)
	    {
		ROOM_INDEX_DATA *pRoom;
		if ((pRoom = wch->boarded_ship->ship_rooms[i]) != NULL)
		{
		    //gecho("look");
		    for (vch = pRoom->people; vch != NULL; vch = vch_next)
		    {
			//	gecho(vch->short_descr);
			//	gecho("\n\r");
			//	if (vch->boarded_ship != NULL)
			//	  gecho("boarded");
			vch_next = vch->next_in_room;

			sprintf(buf,
				"Currently looking at the %s who has boarded ship = %s,"
				" the other is %s who has boarded ship = %s.. "
				"they belong to %s and %s\n\r",
				vch->short_descr,
				vch->boarded_ship ? "yes" : "no",
				wch->short_descr,
				wch->boarded_ship ? "yes" : "no",
				(vch->belongs_to_ship == NULL) ? "no" :
				vch->belongs_to_ship->ship_name,
				wch->belongs_to_ship->ship_name);

			//	gecho(buf);

			if (vch != wch &&
				vch->boarded_ship == wch->boarded_ship &&
				vch->belongs_to_ship != wch->belongs_to_ship)
			{
			    no_enemy_left = FALSE;
			    //	    gecho("FOUND!@!##@!!");
			    if (wch->in_room == vch->in_room)
			    {
				set_fighting(wch, vch);
			    }
			    hunt_char(wch, vch);
			}
		    }
		}
	    }
	    if (no_enemy_left)
	    {
		//gecho("\n\r{RAll enemy have been killed!{x");
		for (vch = wch->belongs_to_ship->crew_list;
			vch != NULL;
			vch = vch_next)
		{
		    vch_next = vch->next_in_crew;
		    vch->boarded_ship = NULL;
		    char_from_room(vch);
		    char_to_room(vch,
			    get_room_index(vch->belongs_to_ship->first_room));
		}
	    }
	}

  // NPC Player hunters should be hunting players
  if (IS_NPC(wch) &&
      IS_SET(wch->act2, ACT2_PLAYER_HUNTER) &&
      wch->target_name != NULL &&
      wch->fighting == NULL) {
     CHAR_DATA *target = get_player(wch->target_name);
     bool consider_going = FALSE;

     if (target != NULL) {
     	if (wch->in_room == target->in_room) {
				set_fighting(wch, target);
      }
      else {
        if (wch->in_room->area != target->in_room->area) {
          consider_going = TRUE;
        }
        else {
          if (number_percent() < 10) {
            act("{W$n sprints off into the distance.{x", wch, NULL, NULL, TO_ROOM);
            char_from_room(wch);
            char_to_room(wch, target->in_room);
            act("{W$n has arrived.{x", wch, NULL, NULL, TO_ROOM);
          }
			    hunt_char(wch, target);
        }
     }
     }
     else {
       consider_going = TRUE;
     }

     if (number_percent() < 2 && number_percent() < 50) {
       act("$n gives up with the hunt and leaves.", wch, NULL, NULL, TO_ROOM);
       extract_char(wch, FALSE);
       continue;
     }
  }

    */
	// Stop there for NPCs; for mortal PCs, aggress
	if (IS_NPC(wch)
	||  wch->level >= LEVEL_IMMORTAL
	||  wch->in_room == NULL
	||  !can_room_update(wch->in_room))
	    continue;

	if (wch->boarded_ship == NULL)
	{
	    for (ch = wch->in_room->people; ch != NULL; ch = ch_next)
	    {
		int count;

		ch_next	= ch->next_in_room;

		if (!IS_NPC(ch)
		||  ch->in_room == NULL
		||  (!IS_SET(ch->act, ACT_AGGRESSIVE) && ch->boarded_ship == NULL)
		||  IS_SET(ch->in_room->room_flags, ROOM_SAFE)
		||  IS_AFFECTED(ch, AFF_CALM)
		||  ch->fighting != NULL
		||  IS_AFFECTED(ch, AFF_CHARM)
		||  !IS_AWAKE(ch)
		||  IS_SET(ch->act, ACT_WIMPY)
		||  !can_see(ch, wch)
		||  number_bits(1) == 0)
		    continue;

		// Evasion lets you get away from aggro mobs.
		if (check_evasion(wch) == TRUE)
		{
		    check_improve(wch, gsn_evasion, TRUE, 8);
		    continue;
		}
		else
		    check_improve(wch, gsn_evasion, FALSE, 8);

		// Make the NPC agressor (ch) attack a RANDOM person in the room.
		count = 0;
		victim = NULL;
		for (vch = wch->in_room->people; vch != NULL; vch = vch_next)
		{
		    vch_next = vch->next_in_room;

		    // If mob is boarding a ship then may attack anyone
		    if (ch->boarded_ship != NULL
		    &&   ch->belongs_to_ship != vch->belongs_to_ship
		    &&   can_see(ch, vch))
		    {
			if (number_range(0, count) == 0)
			    victim = vch;

			count++;
		    }
		    else
		    if (!IS_NPC(vch)
		    &&  vch->level < LEVEL_IMMORTAL
		    &&  ch->level >= vch->level - 5
		    &&  (!IS_SET(ch->act, ACT_WIMPY) || !IS_AWAKE(vch))
		    &&  can_see(ch, vch))
		    {
			if (number_range(0, count) == 0)
			    victim = vch;

			count++;
		    }
		}

		if (victim == NULL)
		    continue;

		multi_hit(ch, victim, TYPE_UNDEFINED);
	    }
	}
    }
    iterator_stop(&it);
}


// Update NPC hunting
void update_hunting(void)
{
    CHAR_DATA *mob;
    int direction = 0;

    for (mob = hunt_last; mob != NULL; mob = mob->next_in_hunting)
    {
	if (!IS_NPC(mob)
	||  number_percent() < 20
	||  mob->fighting != NULL
	||  !IS_AWAKE(mob))
	    continue;

	if (mob->hunting == NULL
	||  IS_DEAD(mob->hunting)
	||  IS_SET(mob->hunting->affected_by, AFF_HIDE)
	||  mob->hunting->in_room == NULL
	||  mob->hunting->in_room->area != mob->in_room->area
	||  IS_SET(mob->hunting->in_room->room_flags, ROOM_SAFE)
	||  !can_see(mob, mob->hunting)
	||  number_percent() < get_skill(mob->hunting, gsn_trackless_step)/2
	||  (check_evasion(mob->hunting) == TRUE && number_percent() < 33))
	    stop_hunt(mob, FALSE);
	else
	{
	    int result;

	    if (mob->in_room == NULL)
	    {
		stop_hunt(mob, TRUE);
		continue;
	    }

            // Mob has found player
  	    if (mob->in_room == mob->hunting->in_room)
	    {
		if (IS_SET(mob->in_room->room_flags,ROOM_SAFE)
		||  IS_AFFECTED(mob, AFF_CALM)
		||  IS_AFFECTED(mob, AFF_CHARM)
		||  !IS_AWAKE(mob)
		||  !can_see(mob, mob->hunting)
		||  number_bits(1) == 0
		||  check_evasion(mob->hunting) == TRUE)
		    continue;

		multi_hit(mob, mob->hunting, TYPE_UNDEFINED);
		continue;
	    }

	    if (mob->in_room == NULL || mob->hunting == NULL || mob->hunting->in_room == NULL)
	    {
		direction = -1;
		continue;
	    }
	    else
		direction = find_path(mob->in_room->vnum, mob->hunting->in_room->vnum, mob, -600, FALSE);

	    if (direction == -1)
	    {
		stop_hunt(mob, FALSE);
		continue;
	    }

	    move_char(mob, direction, FALSE);

	    result = number_range(0, 2);

            if (number_percent() < 5)
	    switch(result)
	    {
		case 0:
		    act("{DYou get the feeling something is following you...{x", mob->hunting, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	   	    break;
		case 1:
		    act("{DYou hear footsteps behind you...{x", mob->hunting, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	   	    break;
		case 2:
		    act("{DYou hear noises as if something is looking for you...{x", mob->hunting, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	   	    break;
	    }
        }
    }
}


// Auto-hunt, PC version.
void update_hunting_pc(CHAR_DATA *ch)
{
    CHAR_DATA *victim = ch->hunting;
    CHAR_DATA *rch;
    int direction = 0;
    int chance;

    if (ch == NULL)
    {
	bug("update_hunting_pc: null ch!", 0);
	return;
    }

    if (victim == NULL)
    {
	bug("update_hunting_pc: null victim = ch->hunting", 0);
	return;
    }

    if (victim->in_room == NULL)
    {
	bug("update_hunting_pc: victim in_room null!", 0);
	ch->hunting = NULL;
	return;
    }

    if (ch->position != POS_STANDING)
    {
	ch->hunting = NULL;
	return;
    }

    // Chance of failing
    chance = get_skill(ch, gsn_hunt) * 3/4
             + (get_curr_stat(ch, STAT_INT)
	     +   get_curr_stat(ch, STAT_WIS)
	     +   get_curr_stat(ch, STAT_DEX)) / 5;

    if (number_percent() > chance && number_percent() < 25)
    {
	send_to_char("You lost the trail.\n\r", ch);
	act("$n has lost the trail.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	ch->hunting = NULL;
	return;
    }

    direction = find_path(ch->in_room->vnum, victim->in_room->vnum, ch, -500, FALSE);

    if (direction == -1)
    {
	send_to_char("You lost the trail.\n\r", ch);
	ch->hunting = NULL;
	return;
    }
    else
    {
	if (number_percent() < 20)
	{
	    if (number_percent() < 20)
	    {
		send_to_char("You stop and sniff the air.\n\r", ch);
		act("$n stops and sniffs the air.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	    }
	    else
	    if (number_percent() < 40)
	    {
		send_to_char("You analyze some tracks.\n\r", ch);
		act("$n analyzes some tracks.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	    }
	    else
	    if (number_percent() < 60)
	    {
		send_to_char("You look around warily.\n\r", ch);
		act("$n looks around warily.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	    }
	    else
	    if (number_percent() < 80)
	    {
		act("You scan the horizons for $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n scans the horizons for $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	    }
	    else
	    {
		act("You move towards $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n moves towards $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	    }
	}

	if (number_percent() < 5)
	    send_to_char("You get the feeling that someone is following you.\n\r", victim);

	deduct_move(ch, 5);

	move_char(ch, direction, TRUE);

	// Found them
	if (ch->in_room == victim->in_room)
	    ch->hunting = NULL;

	// For NPCs, if you are hunting one mob and come across another
	// of the same mob it stops.
	for (rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room)
	{
	    if (IS_NPC(rch))
	    {
		if (rch->pIndexData == victim->pIndexData)
		{
		    ch->hunting = NULL;
		    break;
		}
	    }
	}
    }
}


// Update the pneuma relic. Add another pneuma to its stash.
void pneuma_relic_update(void)
{
    OBJ_DATA *pneuma;
    CHAR_DATA *people;
    int chance;
    char buf[MAX_STRING_LENGTH];
    log_string("Update pneuma relic...");

    if (pneuma_relic != NULL && pneuma_relic->in_room != NULL)
    {
	chance = number_percent();

        if (chance > 80)
	{
	    pneuma = create_object(get_obj_index(OBJ_VNUM_BOTTLED_SOUL), 0, TRUE);
	    obj_to_room(pneuma, pneuma_relic->in_room);

            for (people = pneuma_relic->in_room->people; people != NULL; people = people->next_in_room)
	    {
	        if (!IS_NPC(people))
	        {
		    sprintf(buf,
		    "%s glows gently then neatly drops a bottled soul.\n\r",
		        can_see_obj(people, pneuma_relic) ?
			    pneuma_relic->short_descr :
			    "Something");
		    buf[0] = UPPER(buf[0]);
		    send_to_char(buf, people);
		}
	    }
	}
    }
}


// Update relics, find out if they need to vanish.
void relic_update(void)
{
	log_string("Update all relics...");
    if (pneuma_relic != NULL)
	check_relic_vanish(pneuma_relic);

    if (damage_relic != NULL)
	check_relic_vanish(damage_relic);

    if (xp_relic != NULL)
	check_relic_vanish(xp_relic);

    if (hp_regen_relic != NULL)
	check_relic_vanish(hp_regen_relic);

    if (mana_regen_relic != NULL)
	check_relic_vanish(mana_regen_relic);
}


// Check if a relic needs to vanish from a treasure room.
void check_relic_vanish(OBJ_DATA *relic)
{
	ITERATOR cit, rit, oit;
    ROOM_INDEX_DATA *to_room;
    CHURCH_DATA *church;
    CHURCH_TREASURE_ROOM *treasure;
    OBJ_DATA *obj;
    char buf[MSL];
    int chance1 = 5;
    int chance2 = 3;

    if (number_percent() < chance1 && number_percent() < chance2) {
		to_room = get_random_room(NULL, 0);

		sprintf(buf, "{M%s vanishes in a swirl of purple mist.{x\n\r", relic->short_descr);
		buf[2] = UPPER(buf[2]);
		room_echo(relic->in_room, buf);

		// Inform church the relic has vanished
		iterator_start(&cit, list_churches);
		while((church = (CHURCH_DATA *)iterator_nextdata(&cit))) {
			iterator_start(&rit, church->treasure_rooms);
			while(( treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&rit))) {
				iterator_start(&oit, treasure->room->lcontents);
				while(( obj = (OBJ_DATA *)iterator_nextdata(&oit))) {
					if (obj == relic) {
						sprintf(buf,
							"{Y[You feel an ancient power depart your church as %s vanishes from your treasure room.]{x\n\r",
								obj->short_descr);
						church_echo(church, buf);
						break;
					}
				}
				iterator_stop(&oit);
			}
			iterator_stop(&rit);
		}
		iterator_stop(&cit);

		obj_from_room(relic);
		obj_to_room(relic, to_room);

		sprintf(buf, "relic_update: %s has vanished to %s (%ld)",
			relic->short_descr,
			to_room->name,
			to_room->vnum);
		log_string(buf);
    }
}


// Update people bitten by a sith.
void bitten_update(CHAR_DATA *ch)
{
    char buf[MAX_STRING_LENGTH];
    int percent = number_percent();

    if (number_percent() < 10)
    {
 	if (percent > 90)
	{
	    send_to_char("{RYou feel slightly uncomfortable.{x\n\r",
		    ch);
	    act("{R$n begins to look uncomfortable.{x", ch,
		    NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}
	else if (percent > 80)
	{
	    send_to_char("{RYou feel feverish.{x\n\r", ch);
	    act("{R$n sneezes, looking feverish.{x", ch,
		    NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}
	else if (percent > 70)
	{
	    sprintf(buf, "{RYou pale as the toxins race through your veins.{x\n\r");
	    act("{R$n pales as venomous toxins race through $s body.{x", ch,
		    NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	    send_to_char(buf , ch);
	}
	else if (percent > 60)
	{
	    send_to_char("{RDizzy, you swoon back and forth.{x\n\r", ch);
	    act("{R$n swoons back and forth, dizzy.{x", ch,
		    NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}
	else if (percent > 50)
	{
	    send_to_char("{RYour glands swell up as poison races through them.{x\n\r", ch);
	}
	else if (percent > 40)
	{
	    send_to_char("{RYou twitch nervously as you feel an unfamiliar venom in your body.{x\n\r", ch);
	    act("{R$n twitches nervously.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}
	else if (percent > 30)
	{
	    send_to_char("{RYou lose your hearing for a moment.{x\n\r", ch);
	}
	else if (percent > 20)
	{
	    send_to_char("{RYour vision goes black for a second, then returns.{x\n\r", ch);
	}
	else if (percent > 10)
	{
	    send_to_char("{RYour eyes roll back in your head.{x\n\r", ch);
	    act("{R$n's eyes roll back in $s head.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}
    }

    damage(ch, ch, dice(UMAX(ch->bitten_level/8, 4),1), gsn_toxins, DAM_POISON, FALSE);

    --ch->bitten;
    if (ch->bitten <= 0)
    {
        ch->bitten = 0;
	bitten_end(ch);
    }
}


// Regen sith toxins. Only happens when they're asleep.
void toxin_update(CHAR_DATA *ch)
{
    int i;

    if (IS_AWAKE(ch))
        return;

    for (i = 0; i < MAX_TOXIN; i++)
    {
	ch->toxin[i] = URANGE(0, ch->toxin[i], 100);
	ch->toxin[i] = UMIN(100, ch->toxin[i] + toxin_gain(ch));
    }
}


// Update the healing locket/ring of argyle evenhand.
void locket_update(OBJ_DATA *obj)
{
#if 0
    CHAR_DATA *ch;
    int sn;

    ch = obj->carried_by;
    if (ch == NULL)
	return;

    if (IS_AFFECTED(ch, AFF_POISON))
    {
	sn = skill_lookup("cure poison");

	act("$p shimmers softly.",
		ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	act("$n's $p shimmers softly.",
		ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

	obj_cast_spell(sn , ch->level * 2, ch,
		ch, obj);
	return;
    }

    if (IS_AFFECTED(ch, AFF_PLAGUE))
    {
	sn = skill_lookup("cure disease");

	act("$p hums quietly for a second.",
		ch, obj, NULL, TO_CHAR);
	act("$n's $p hums quietly.",
		ch, obj, NULL, TO_ROOM);

	obj_cast_spell(sn , ch->level * 2, ch,
		ch, obj);
	return;
    }

    if (IS_AFFECTED(ch, AFF_CURSE))
    {
	sn = skill_lookup("remove curse");

	act("$p vibrates for a second, then quiets down.",
		ch, obj, NULL, TO_CHAR);
	act("$n's $p vibrates for a second.",
		ch, obj, NULL, TO_ROOM);

	obj_cast_spell(sn , ch->level * 2, ch,
		ch, obj);
	return;
    }

    if (IS_AFFECTED(ch, AFF_BLIND))
    {
	sn = skill_lookup("cure blindness");

	act("$p glimmers briefly.",
		ch, obj, NULL, TO_CHAR);
	act("$n's $p glimmers briefly.",
		ch, obj, NULL, TO_ROOM);

	obj_cast_spell(sn , ch->level * 2, ch,
		ch, obj);
	return;
    }

    if (number_percent() < 20)
    {
        // Healing
        if (ch->hit < ch->max_hit)
	{
	    act("$p glows with a vibrant blue aura.",
		    ch, obj, NULL, TO_CHAR);
	    act("$n's $p glows with a vibrant blue aura.",
		    ch, obj, NULL, TO_ROOM);

	    if (number_percent() > 60)
		sn = skill_lookup("cure critical");
	    else
		sn = skill_lookup("heal");

	    obj_cast_spell(sn, ch->level, ch, ch, obj);

	    return;
	}

	// Mana
	if (ch->mana < ch->max_mana)
	{
	    int mana;

	    act("$p glows with a vibrant purple aura.",
		    ch, obj, NULL, TO_CHAR);
	    send_to_char("You feel energized!\n\r", ch);
	    act("$n's $p glows with a vibrant purple aura.",
		    ch, obj, NULL, TO_ROOM);

	    mana = ch->max_mana/number_range(10, 20);
	    mana = UMIN(mana, ch->max_mana - mana);

	    ch->mana += mana;

	    return;
	}

	// Move
	if (ch->move < ch->max_move)
	{
	    sn = skill_lookup( "refresh");

	    act("$p glows with a vibrant green aura.",
		    ch, obj, NULL, TO_CHAR);
	    act("$n's $p glows with a vibrant green aura.",
		    ch, obj, NULL, TO_ROOM);

	    obj_cast_spell(sn, ch->level, ch, ch, obj);

	    return;
	}
    }

    if (number_percent() < 15)
    {
	switch(number_range(1, 6))
	{
	    case 1:
		if (!IS_AFFECTED(ch, AFF_SANCTUARY))
		{
		    sn = skill_lookup( "sanctuary");

		    act("$p glows with a magical aura.",
			    ch, obj, NULL, TO_CHAR);
		    act("$n's $p glows with a magical aura.",
			    ch, obj, NULL, TO_ROOM);

		    obj_cast_spell(sn, ch->level, ch, ch, obj);

		    return;
		    break;
		}
	    case 2:
		if (!IS_AFFECTED(ch, AFF_DETECT_INVIS))
		{
		    sn = skill_lookup( "detect invis");

		    act("$p glows with a magical aura.",
			    ch, obj, NULL, TO_CHAR);
		    act("$n's $p glows with a magical aura.",
			    ch, obj, NULL, TO_ROOM);

		    obj_cast_spell(sn, ch->level, ch, ch, obj);

		    return;
		    break;
		}
	    case 3:
		if (!IS_AFFECTED(ch, AFF_DETECT_HIDDEN))
		{
		    sn = skill_lookup( "detect hidden");

		    act("$p glows with a magical aura.",
			    ch, obj, NULL, TO_CHAR);
		    act("$n's $p glows with a magical aura.",
			    ch, obj, NULL, TO_ROOM);

		    obj_cast_spell(sn, ch->level, ch, ch, obj);

		    return;
		    break;
		}
	    case 4:
		if (IS_AFFECTED2(ch, AFF2_IMPROVED_INVIS))
		{
		    sn = skill_lookup( "improved invisibility");

		    act("$p glows with a magical aura.",
			    ch, obj, NULL, TO_CHAR);
		    act("$n's $p glows with a magical aura.",
			    ch, obj, NULL, TO_ROOM);

		    obj_cast_spell(sn, ch->level, ch, ch, obj);

		    return;
		    break;
		}
	    case 5:
		if (!is_affected(ch, skill_lookup("shield")))
		{
		    sn = skill_lookup( "shield");

		    act("$p glows with a magical aura.",
			    ch, obj, NULL, TO_CHAR);
		    act("$n's $p glows with a magical aura.",
			    ch, obj, NULL, TO_ROOM);

		    obj_cast_spell(sn, ch->level, ch, ch, obj);

		    return;
		    break;
		}
	    case 6:
		if (!is_affected(ch, skill_lookup("stone skin")))
		{
		    sn = skill_lookup( "stone skin");

		    act("$p glows with a magical aura.",
			    ch, obj, NULL, TO_CHAR);
		    act("$n's $p glows with a magical aura.",
			    ch, obj, NULL, TO_ROOM);

		    obj_cast_spell(sn, ch->level, ch, ch, obj);

		    return;
		    break;
		}
	    case 7:
		if (!IS_AFFECTED2(ch, AFF2_ELECTRICAL_BARRIER))
		{
		    sn = skill_lookup( "electrical barrier");

		    act("$p glows with a magical aura.",
			    ch, obj, NULL, TO_CHAR);
		    act("$n's $p glows with a magical aura.",
			    ch, obj, NULL, TO_ROOM);

		    obj_cast_spell(sn, ch->level, ch, ch, obj);

		    return;
		    break;
		}
	}
    }
#endif
}


void scare_update(CHAR_DATA *ch)
{
    CHAR_DATA *victim, *vnext;

    if (ch == NULL) {
	bug("scare_update: NULL ch", 0);
	return;
    }

    if (ch->in_room == NULL) {
	bug("scare_update: NULL ch->in_room", 0);
	return;
    }

    for (victim = ch->in_room->people; victim != NULL; victim = vnext)
    {
	vnext = victim->next_in_room;

        // Certain NPCs are protected
	if (IS_NPC(victim))
	{
	    if (victim->shop != NULL
	    ||  IS_SET(victim->act, ACT_PROTECTED)
	    ||  IS_SET(victim->act, ACT_SENTINEL))
		continue;
	}

	if (victim != ch
	&&  victim->tot_level <= ch->tot_level
	&&  victim->fighting == NULL
	&&  victim->position == POS_STANDING
	&&  ch->invis_level < LEVEL_IMMORTAL
	&&  !is_same_group(victim, ch)
        &&  victim != MOUNTED(ch))
	{
	    if (number_percent() < 25)
	    {
		if (can_see(victim, ch))
		{
		    act("You balk with fear at the sight of $n!",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		    act("$N balks with fear at the sight of you!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		    act("$N balks with fear at the sight of $n!",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		}
		else
		{
		    act("You balk with terror at a terrifying ominous presence in the room!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		    act("$n balks with terror at a terrifying ominous presence in the room!", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}

		do_flee(victim, NULL);
	    }
	}
    }
}


// Update things a person has done in a combat round -- currently just reverie
void update_has_done(CHAR_DATA *ch)
{
    if (IS_SET(ch->has_done, DONE_REVERIE))
	REMOVE_BIT(ch->has_done, DONE_REVERIE);
}

void update_invasion_quest()
{
    AREA_DATA *pArea = NULL;
    INVASION_QUEST *quest = NULL;
    char buf[MSL];
    int max_quests = 1;
    int current_quests = 0;
    log_string("Update invasion quest...");

    for (pArea = area_first; pArea != NULL; pArea = pArea->next) {

	if ( pArea->invasion_quest != NULL) {
	    if (current_time > pArea->invasion_quest->expires) {

		sprintf(buf, "The invasion at %s has ended.", pArea->name);
		crier_announce(buf);
		log_string(buf);

		extract_invasion_quest(pArea->invasion_quest);
		pArea->invasion_quest = NULL;
	    }
	    else {
		current_quests++;
	    }
	}
    }

    // dont want too many quests at once
    if (current_quests < max_quests)
    {
	for (pArea = area_first; pArea != NULL; pArea = pArea->next) {

	    // Only towns should be invaded
	    if (pArea->area_who != AREA_TOWNE) continue;

	    // Newbies are in Plith so better not invade Plith
	    if (!str_cmp(pArea->name, "Plith")) {
		continue;
	    }

	    // only seralia or athemia continents should be invaded
	    if ( (pArea->place_flags == PLACE_FIRST_CONTINENT) ||
		    (pArea->place_flags == PLACE_SECOND_CONTINENT)) {

		if (number_percent() < 10) {
		    int level = number_range(0, 3);
		    switch(level) {
			case 0:
			    level = 30;
			    break;
			case 1:
			    level = 60;
			    break;
			case 2:
			    level = 90;
			    break;
			case 3:
			    level = 120;
			    break;
		    }
//		    quest = create_invasion_quest(pArea, level, 0, 0);

		    pArea->invasion_quest = quest;
		    break;
		}
	    }
	}
    }
}


/*
* Handle events.
*/
void event_update(void)
{
	extern EVENT_DATA *next_event;
	EVENT_DATA tmp;
	EVENT_DATA *ev;
	int depth, sec;

	if (!events) return;

	next_event = NULL;

	for (ev = events; ev; ev = next_event) {
		next_event = ev->next;

		// Delay has expired - perform action and remove event from lists
		if (ev->delay-- <= 0) {
			tmp = *ev;
			ev->args = NULL;
			ev->info = NULL;
			extract_event(ev);

			depth = script_call_depth;
			sec = script_security;
			script_call_depth = tmp.depth;
			script_security = tmp.security;

			switch (tmp.event_type) {

			// These are used by QUEUES
			case EVENT_MOBQUEUE:
			case EVENT_OBJQUEUE:
			case EVENT_ROOMQUEUE:
			case EVENT_TOKENQUEUE:
				do_function((void *)(tmp.info), tmp.function, tmp.args);
				break;
			case EVENT_ECHO:
				room_echo((ROOM_INDEX_DATA *)tmp.entity,tmp.args);
				break;
			}

			script_call_depth = depth;
			script_security = sec;

			free_string(tmp.args);
			if(tmp.info) free(tmp.info);
		}
	}
}
void msdp_update( void )
{
    DESCRIPTOR_DATA *d;
    int PlayerCount = 0;

    for ( d = descriptor_list; d != NULL; d = d->next )
    {
	if ( d->character && d->connected == CON_PLAYING && !IS_NPC(d->character) )
        {
            char buf[MAX_STRING_LENGTH];
            CHAR_DATA *pOpponent = d->character->fighting;
            ROOM_INDEX_DATA *pRoom = d->character->in_room;
//            AFFECT_DATA *paf;

            ++PlayerCount;

            MSDPSetString( d, eMSDP_CHARACTER_NAME, d->character->name );
            MSDPSetNumber( d, eMSDP_ALIGNMENT, d->character->alignment );
            MSDPSetNumber( d, eMSDP_EXPERIENCE, d->character->exp );
            MSDPSetNumber( d, eMSDP_EXPERIENCE_MAX, exp_per_level(d->character,
               d->character->pcdata->points)  );
            MSDPSetNumber( d, eMSDP_EXPERIENCE_TNL, ((d->character->level + 1) *
               exp_per_level(d->character, d->character->pcdata->points) -
               d->character->exp ) );

            MSDPSetNumber( d, eMSDP_HEALTH, d->character->hit );
            MSDPSetNumber( d, eMSDP_HEALTH_MAX, d->character->max_hit );
            MSDPSetNumber( d, eMSDP_LEVEL, d->character->level );
/*
            MSDPSetNumber( d, eMSDP_RACE, TBD );
            MSDPSetNumber( d, eMSDP_CLASS, TBD );
*/
            MSDPSetNumber( d, eMSDP_MANA, d->character->mana );
            MSDPSetNumber( d, eMSDP_MANA_MAX, d->character->max_mana );
            MSDPSetNumber( d, eMSDP_WIMPY, d->character->wimpy );
            MSDPSetNumber( d, eMSDP_PRACTICE, d->character->practice );
            MSDPSetNumber( d, eMSDP_MONEY, d->character->gold );
            MSDPSetNumber( d, eMSDP_MOVEMENT, d->character->move );
            MSDPSetNumber( d, eMSDP_MOVEMENT_MAX, d->character->max_move );
            MSDPSetNumber( d, eMSDP_HITROLL, GET_HITROLL(d->character) );
            MSDPSetNumber( d, eMSDP_DAMROLL, GET_DAMROLL(d->character) );
//            MSDPSetNumber( d, eMSDP_AC, GET_AC(d->character) );
            MSDPSetNumber( d, eMSDP_STR, get_curr_stat(d->character, STAT_STR) );
            MSDPSetNumber( d, eMSDP_INT, get_curr_stat(d->character, STAT_INT) );
            MSDPSetNumber( d, eMSDP_WIS, get_curr_stat(d->character, STAT_WIS) );
            MSDPSetNumber( d, eMSDP_DEX, get_curr_stat(d->character, STAT_DEX) );
            MSDPSetNumber( d, eMSDP_CON, get_curr_stat(d->character, STAT_CON) );
            MSDPSetNumber( d, eMSDP_STR_PERM, d->character->perm_stat[STAT_STR] );
            MSDPSetNumber( d, eMSDP_INT_PERM, d->character->perm_stat[STAT_INT] );
            MSDPSetNumber( d, eMSDP_WIS_PERM, d->character->perm_stat[STAT_WIS] );
            MSDPSetNumber( d, eMSDP_DEX_PERM, d->character->perm_stat[STAT_DEX] );
            MSDPSetNumber( d, eMSDP_CON_PERM, d->character->perm_stat[STAT_CON] );

            /* This would be better moved elsewhere */
            if ( pOpponent != NULL )
            {
                int hit_points = (pOpponent->hit * 100) / pOpponent->max_hit;
                MSDPSetNumber( d, eMSDP_OPPONENT_HEALTH, hit_points );
                MSDPSetNumber( d, eMSDP_OPPONENT_HEALTH_MAX, 100 );
                MSDPSetNumber( d, eMSDP_OPPONENT_LEVEL, pOpponent->level );
                MSDPSetString( d, eMSDP_OPPONENT_NAME, pOpponent->name );
            }
            else /* Clear the values */
            {
                MSDPSetNumber( d, eMSDP_OPPONENT_HEALTH, 0 );
                MSDPSetNumber( d, eMSDP_OPPONENT_LEVEL, 0 );
                MSDPSetString( d, eMSDP_OPPONENT_NAME, "" );
            }

            /* Only update room stuff if they've changed room */
            if ( pRoom && pRoom->vnum != d->pProtocol->pVariables[eMSDP_ROOM_VNUM]->ValueInt )
            {
                int i; /* Loop counter */
                buf[0] = '\0';

                for ( i = DIR_NORTH; i < MAX_DIR; ++i )
                {
                    if ( pRoom->exit[i] != NULL )
                    {
                        const char MsdpVar[] = { (char)MSDP_VAR, '\0' };
                        const char MsdpVal[] = { (char)MSDP_VAL, '\0' };
                        extern char *const dir_name[];

                        strcat( buf, MsdpVar );
                        strcat( buf, dir_name[i] );
                        strcat( buf, MsdpVal );

                        if ( IS_SET(pRoom->exit[i]->exit_info, EX_CLOSED) )
                            strcat( buf, "C" );
                        else /* The exit is open */
                            strcat( buf, "O" );
                    }
                }

                if ( pRoom->area != NULL )
                    MSDPSetString( d, eMSDP_AREA_NAME, pRoom->area->name );

                MSDPSetString( d, eMSDP_ROOM_NAME, pRoom->name );
                MSDPSetTable( d, eMSDP_ROOM_EXITS, buf );
                MSDPSetNumber( d, eMSDP_ROOM_VNUM, pRoom->vnum );
            }
/*
            MSDPSetNumber( d, eMSDP_WORLD_TIME, d->character-> );
*/

            buf[0] = '\0';
/*            for ( paf = d->character->affected; paf; paf = paf->next )
            {
                char skill_buf[MAX_STRING_LENGTH];
                sprintf( skill_buf, "%c%s%c%d",
                    (char)MSDP_VAR, skill_table[paf->type].name,
                    (char)MSDP_VAL, paf->duration );
                strcat( buf, skill_buf );
            }
            MSDPSetTable( d, eMSDP_AFFECTS, buf );*/

            MSDPUpdate( d );
        }
    }

    /* Ideally this should be called once at startup, and again whenever
     * someone leaves or joins the mud.  But this works, and it keeps the
     * snippet simple.  Optimise as you see fit.
     */
    MSSPSetPlayers( PlayerCount );
}
