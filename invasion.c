/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "merc.h"
#include "interp.h"
#include "recycle.h"
#include "tables.h"
#include "olc.h"

INVASION_QUEST* create_invasion_quest(AREA_DATA *pArea, int max_level, long p_leader_vnum, long p_mob_vnum)
{
/*  
    INVASION_QUEST  *quest;
    struct tm *expires;
    CHAR_DATA *leader = NULL;
    long vnum = 0;
    ROOM_INDEX_DATA *pRoom;
    int number = 0;
    int i = 0;
    char buf[MSL];
    long leader_vnum;
    long mob_vnum;

    if (p_leader_vnum > 0 && p_mob_vnum > 0) { 
			leader_vnum = p_leader_vnum;
      mob_vnum = p_mob_vnum;
		  sprintf(buf, "Global Quest: %s has been overrun by an invasion force led by %s! Bring back the head for reward! (Max level %d)", pArea->name, get_mob_index(leader_vnum)->short_descr, max_level);
    }
    else {
			// check type of invasion
			//invasion_type = number_range(0, 3);
			switch(max_level) {
			case 60: 
				 leader_vnum = MOB_VNUM_INVASION_LEADER_GOBLIN;
				 mob_vnum = MOB_VNUM_INVASION_GOBLIN;
				 sprintf(buf, "Global Quest: The goblin horde has invaded %s! (Max level 60)", pArea->name);
				 break;
			case 90: 
				 leader_vnum = MOB_VNUM_INVASION_LEADER_SKELETON;
				 mob_vnum = MOB_VNUM_INVASION_SKELETON;
				 sprintf(buf, "Global Quest: The undead have taken over %s, stop them at all costs! (Max level 90)", pArea->name);
				 break;
			case 120: 
				 leader_vnum = MOB_VNUM_INVASION_LEADER_PIRATE;
				 mob_vnum = MOB_VNUM_INVASION_PIRATE;
				 sprintf(buf, "Global Quest: Swarthy pirates have taken %s by storm! It is imperative that we take it back! (Max level 120)", pArea->name);
				 break;
			case 30: 
				 leader_vnum = MOB_VNUM_INVASION_LEADER_BANDIT;
				 mob_vnum = MOB_VNUM_INVASION_BANDIT;
				 sprintf(buf, "Global Quest: Bandits have stormed %s, we must take it back! (Max level 30)", pArea->name);
				 break;
			default:
				 leader_vnum = MOB_VNUM_INVASION_LEADER_BANDIT;
				 mob_vnum = 11002;
				 sprintf(buf, "Global Quest: Let it be known that %s has been invaded by bandits! (Max level 30)", pArea->name);
				 break;
			 }
     }

     if (get_mob_index(leader_vnum ) == NULL || get_mob_index(mob_vnum) == NULL)
     {
	 bug("create_invasion_quest: leader or mob vnum is null.", 0);
	 return NULL;
     }


     crier_announce(buf);           
     log_string(buf);

    expires = (struct tm *) localtime(&current_time);
    expires->tm_mon +=1;

    quest = new_invasion_quest();
    quest->expires = (time_t) mktime( expires );

    pArea->invasion_quest = quest;


    // create leader
    leader = create_mobile(get_mob_index(leader_vnum), FALSE);
    leader->invasion_quest = quest;
    quest->leader = leader;

    while(TRUE) {
      vnum = number_range(1, pArea->top_room);
      pRoom = get_room_index(pArea, vnum);

      if (pRoom != NULL) {
        break;
      }
    }
     
    // place leader
    char_to_room(leader, pRoom);

    // calc number of mobs to add
    number = number_range(pArea->max_vnum - pArea->min_vnum, 
                          pArea->max_vnum - pArea->min_vnum*3);

    // place mobs
    for (i = 0; i < number; i++) {
      CHAR_DATA *mob; 
    	mob = create_mobile(get_mob_index(mob_vnum), FALSE);

			while(TRUE) {
				vnum = number_range(1, pArea->top_room);
				pRoom = get_room_index(pArea, vnum);

				if (pRoom != NULL) {
					break;
				}
			}
     
      // place mob
      char_to_room(mob, pRoom);

      // Add mob to invasion structure
      char_to_invasion(mob, quest);
    }
    
    return quest;
    */
    return NULL;
}

void extract_invasion_quest(INVASION_QUEST *quest) {
  CHAR_DATA *ch;

  while((ch = quest->invasion_mob_list) != NULL) {
    char_from_invasion(ch, quest);
    if (ch != NULL) {
      char_from_room(ch);
      extract_char(ch, FALSE);
    }
  }

  if (quest->leader != NULL) {
    char_from_room(quest->leader);
  	extract_char(quest->leader, FALSE);
  }

  quest->leader = NULL;

  free_invasion_quest(quest); 
}

void check_invasion_quest_slay_mob(CHAR_DATA *ch, CHAR_DATA *victim) {
  AREA_DATA *pArea;
  char buf[MSL];

  if (ch->in_room == NULL) {
     return;
  }

  pArea = ch->in_room->area;

  if (!IS_NPC(ch) || IS_NPC(victim)) {
		return;
  }

  if (pArea->invasion_quest != NULL &&
      pArea->invasion_quest->leader == victim) {

      if ( pArea->invasion_quest != NULL) {
         if (pArea->invasion_quest->leader == NULL) {
          send_to_char("{YYou have completed the quest, return the head to a quest master!{x\n\r", ch);
					sprintf(buf, "%s has restored order at %s. The invasion has ended.", ch->name, pArea->name);
					crier_announce(buf);
					log_string(buf);

					extract_invasion_quest(pArea->invasion_quest);
					pArea->invasion_quest = NULL; 
          victim->invasion_quest = NULL;
        }
      }
 
  }
}

