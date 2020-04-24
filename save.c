/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*	ROM 2.4 is copyright 1993-1998 Russ Taylor			   *
*	ROM has been brought to you by the ROM consortium		   *
*	    Russ Taylor (rtaylor@hypercube.org)				   *
*	    Gabrielle Taylor (gtaylor@hypercube.org)			   *
	    Brian Moore (zump@rom.org)					   *
*	By using this code, you have agreed to follow the terms of the	   *
*	ROM license, in the file Rom24/doc/rom.license			   *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef MALLOC_STDLIB
#include <malloc.h>
#endif
#include "merc.h"
#include "recycle.h"
#include "tables.h"
#include "db.h"
#include "olc.h"
#include "interp.h"
#include "olc_save.h"
#include "scripts.h"
#include "wilds.h"

#if defined(KEY)
#undef KEY
#endif

#define IS_KEY(literal)		(!str_cmp(word,literal))

#define KEY(literal, field, value) \
	if (IS_KEY(literal)) { \
		field = value; \
		fMatch = TRUE; \
		break; \
	}

#define SKEY(literal, field) \
	if (IS_KEY(literal)) { \
		free_string(field); \
		field = fread_string(fp); \
		fMatch = TRUE; \
		break; \
	}

#define FKEY(literal, field) \
	if (IS_KEY(literal)) { \
		field = TRUE; \
		fMatch = TRUE; \
		break; \
	}

#define FVKEY(literal, field, string, tbl) \
	if (IS_KEY(literal)) { \
		field = flag_value(tbl, string); \
		fMatch = TRUE; \
		break; \
	}

#define FVDKEY(literal, field, string, tbl, bad, def) \
	if (!str_cmp(word, literal)) { \
		field = flag_value(tbl, string); \
		if( field == bad ) { \
			field = def; \
		} \
		fMatch = TRUE; \
		break; \
	}

// External functions

// Globals.
OBJ_DATA *	pneuma_relic;
OBJ_DATA *	damage_relic;
OBJ_DATA *	xp_relic;
OBJ_DATA *	hp_regen_relic;
OBJ_DATA *	mana_regen_relic;

// Array of containers read for proper re-nesting of objects.
OBJ_DATA *	rgObjNest[MAX_NEST];
int 		nest_level;


// Output a string of letters corresponding to the bitvalues for a flag
char *print_flags(long flag)
{
    int count, pos = 0;
    static char buf[52];


    for (count = 0; count < 32;  count++)
    {
        if (IS_SET(flag,1<<count))
        {
            if (count < 26)
                buf[pos] = 'A' + count;
            else
                buf[pos] = 'a' + (count - 26);
            pos++;
        }
    }

    if (pos == 0)
    {
        buf[pos] = '0';
        pos++;
    }

    buf[pos] = '\0';

    return buf;
}


// Save a character and inventory.
void save_char_obj(CHAR_DATA *ch)
{
    char strsave[MAX_INPUT_LENGTH];
    FILE *fp;

    if (IS_NPC(ch))
	return;

    if (!IS_VALID(ch))
    {
        bug("save_char_obj: Trying to save an invalidated character.\n", 0);
        return;
    }

    if (ch->desc != NULL && ch->desc->original != NULL)
	ch = ch->desc->original;

#if 0
#if defined(unix)
    /* create god log */
    if (IS_IMMORTAL(ch) && !IS_NPC(ch))
    {
	fclose(fpReserve);
	sprintf(strsave, "%s%s",GOD_DIR, capitalize(ch->name));
	if ((fp = fopen(strsave,"w")) == NULL)
	{
	    bug("Save_char_obj: fopen",0);
	    perror(strsave);
 	}

	fprintf(fp,"{Y Lev %2d{x  %s%s{x\n",
	    ch->level,
	    ch->name,
	    ch->pcdata->title?ch->pcdata->title:"");
	fclose(fp);
	fpReserve = fopen(NULL_FILE, "r");
    }
#endif
#endif

    fclose(fpReserve);
	sprintf( strsave, "%s%c/%s",PLAYER_DIR,tolower(ch->name[0]),
			 capitalize( ch->name ) );
    if ((fp = fopen(TEMP_FILE, "w")) == NULL)
    {
	bug("Save_char_obj: fopen", 0);
	perror(strsave);
    }
    else
    {
	    // Used to do SAVE checks
		p_percent_trigger( ch,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SAVE, NULL );

		fwrite_char(ch, fp);

		if (ch->carrying != NULL)
			fwrite_obj_new(ch, ch->carrying, fp, 0);

		if (ch->locker != NULL)
			fwrite_obj_new(ch, ch->locker, fp, 0);

		if (ch->tokens != NULL) {
			TOKEN_DATA *token;
			for(token = ch->tokens; token; token = token->next)
				if( !token->skill )
					fwrite_token(token, fp);
		}

		fwrite_skills(ch, fp);

	    fprintf(fp, "#END\n");
        fprintf(fp, "#END\n");

    }


    fclose(fp);
    rename(TEMP_FILE,strsave);
    fpReserve = fopen(NULL_FILE, "r");
}


/*
 * Write the char.
 */
void fwrite_char(CHAR_DATA *ch, FILE *fp)
{
    AFFECT_DATA *paf;
    int gn, pos;
    int i = 0;
    COMMAND_DATA *cmd;

    fprintf(fp, "#%s\n", IS_NPC(ch) ? "MOB" : "PLAYER"	);
// VERSION MUST ALWAYS BE THE FIRST FIELD!!!
    fprintf(fp, "Vers %d\n", IS_NPC(ch) ? VERSION_MOBILE : VERSION_PLAYER);
    fprintf(fp, "Name %s~\n",	ch->name		);
    if(!IS_NPC(ch))
	fprintf(fp, "Created   %ld\n", ch->pcdata->creation_date	);
    fprintf(fp, "Id   %ld\n", ch->id[0]			);
    fprintf(fp, "Id2  %ld\n", ch->id[1]			);
    fprintf(fp, "LogO %ld\n", (long int)current_time		);
    fprintf(fp, "LogI %ld\n", (long int) ch->pcdata->last_login	);

    if (ch->dead) {
	fprintf(fp, "DeathTimeLeft %d\n", ch->time_left_death);
        fprintf(fp, "Dead\n");
	if(ch->recall.wuid)
		fprintf(fp, "RepopRoomW %lu %lu %lu %lu\n", 	ch->recall.wuid, ch->recall.id[0], ch->recall.id[1], ch->recall.id[2]);
	else if(ch->recall.id[1] || ch->recall.id[2])
		fprintf(fp, "RepopRoomC %lu %lu %lu\n", 	ch->recall.id[0], ch->recall.id[1], ch->recall.id[2]);
	else
		fprintf(fp, "RepopRoom %ld\n", 	ch->recall.id[0]);
    }

/*
    if (ON_SHIP(ch))
    {
	// Make sure IMM didn't just 'goto' a ship and then quit.
	if (ch->in_room->ship != NULL)
	{
	    if (!IS_NPC_SHIP(ch->in_room->ship))
	    {
		ch->pcdata->owner_of_boat_before_logoff = ch->in_room->ship->owner_name;
		fprintf(fp, "OwnerOfShip %s~\n", ch->pcdata->owner_of_boat_before_logoff);
	    }
	    else
	    {
		ch->pcdata->vnum_of_boat_before_logoff = ch->in_room->ship->npc_ship->pShipData->vnum;
		fprintf(fp, "VnumOfShip %ld\n", ch->pcdata->vnum_of_boat_before_logoff);
	    }
	}
    }
*/
    /*
    if (ch->short_descr[0] != '\0')
      	fprintf(fp, "ShD  %s~\n",	ch->short_descr	);
    if(ch->long_descr[0] != '\0')
	fprintf(fp, "LnD  %s~\n",	ch->long_descr	);
    */
    if (ch->description[0] != '\0')
    	fprintf(fp, "Desc %s~\n", fix_string(ch->description));
    if (ch->prompt != NULL
    || !str_cmp(ch->prompt,"{B<{x%h{Bhp {x%m{Bm {x%v{Bmv>{x "))
        fprintf(fp, "Prom %s~\n",      ch->prompt  	);
    fprintf(fp, "Race %s~\n", pc_race_table[ch->race].name);
    fprintf(fp, "Sex  %d\n",	ch->sex			);
    fprintf(fp, "LockerRent %ld\n", (long int)ch->locker_rent   );
    fprintf(fp, "Cla  %d\n",	ch->pcdata->class_current		);
    fprintf(fp, "Mc0  %d\n",	ch->pcdata->class_mage		);
    fprintf(fp, "Mc1  %d\n",	ch->pcdata->class_cleric		);
    fprintf(fp, "Mc2  %d\n",	ch->pcdata->class_thief		);
    fprintf(fp, "Mc3  %d\n",	ch->pcdata->class_warrior		);
    fprintf(fp, "RMc0  %d\n",   ch->pcdata->second_class_mage	);
    fprintf(fp, "RMc1  %d\n",   ch->pcdata->second_class_cleric );
    fprintf(fp, "RMc2  %d\n",   ch->pcdata->second_class_thief  );
    fprintf(fp, "RMc3  %d\n",   ch->pcdata->second_class_warrior );
    fprintf(fp, "Subcla  %d\n",ch->pcdata->sub_class_current		);
    fprintf(fp, "SMc0  %d\n",	ch->pcdata->sub_class_mage		);
    fprintf(fp, "SMc1  %d\n",	ch->pcdata->sub_class_cleric		);
    fprintf(fp, "SMc2  %d\n",	ch->pcdata->sub_class_thief		);
    fprintf(fp, "SMc3  %d\n",	ch->pcdata->sub_class_warrior		);
    fprintf(fp, "SSMc0  %d\n",	ch->pcdata->second_sub_class_mage		);
    fprintf(fp, "SSMc1  %d\n",	ch->pcdata->second_sub_class_cleric		);
    fprintf(fp, "SSMc2  %d\n",	ch->pcdata->second_sub_class_thief		);
    fprintf(fp, "SSMc3  %d\n",	ch->pcdata->second_sub_class_warrior		);
    if (ch->pcdata->email != NULL)
	fprintf(fp, "Email %s~\n",  ch->pcdata->email	);
    fprintf(fp, "Levl %d\n",	ch->level		);
    fprintf(fp, "TLevl %d\n",	ch->tot_level		);
    fprintf(fp, "Sec  %d\n",    ch->pcdata->security	);	/* OLC */
    fprintf(fp, "ChDelay %d\n", ch->pcdata->challenge_delay);

    if (IS_SHIFTED_SLAYER(ch))
	fprintf(fp, "Shifted Slayer~\n");

    if (IS_SHIFTED_WEREWOLF(ch))
	fprintf(fp, "Shifted Werewolf~\n");
    if (ch->pcdata && ch->pcdata->immortal && ch->pcdata->immortal->imm_flag != NULL &&
    	str_cmp(ch->pcdata->immortal->imm_flag, "none"))
        fprintf(fp, "ImmFlag %s~\n", fix_string(ch->pcdata->immortal->imm_flag));

    if (ch->pcdata->flag != NULL)
	fprintf(fp, "Flag %s~\n", fix_string(ch->pcdata->flag));

    fprintf(fp, "ChannelFlags %ld\n", ch->pcdata->channel_flags);
     /*
    for (i = 0; i < 3; i++)
    {
	fprintf(fp, "Rank%d  %d\n", i, ch->pcdata->rank[i]);
	fprintf(fp, "Reputation%d  %d\n", i, ch->pcdata->reputation[i]);
	fprintf(fp, "ShipQuestPoints%d  %ld\n", i, ch->pcdata->ship_quest_points[i]);
    }
*/
    if (ch->pcdata->danger_range > 0)
    	fprintf(fp, "DangerRange %d\n", ch->pcdata->danger_range);

    if (IS_SET(ch->comm, COMM_AFK) && ch->pcdata->afk_message != NULL)
	fprintf(fp, "Afk_message %s~\n", ch->pcdata->afk_message);

    fprintf(fp, "Need_change_pw %d\n", ch->pcdata->need_change_pw);

    fprintf(fp, "Plyd %d\n", !str_cmp(ch->name, "Syn") ? 0 : ch->played + (int) (current_time - ch->logon));

    if (location_isset(&ch->pcdata->room_before_arena)) {
	if(ch->pcdata->room_before_arena.wuid)
		fprintf(fp, "Room_before_arenaW %lu %lu %lu %lu\n", 	ch->pcdata->room_before_arena.wuid, ch->pcdata->room_before_arena.id[0], ch->pcdata->room_before_arena.id[1], ch->pcdata->room_before_arena.id[2]);
	else if(ch->pcdata->room_before_arena.id[1] || ch->pcdata->room_before_arena.id[2])
		fprintf(fp, "Room_before_arenaC %lu %lu %lu\n", 	ch->pcdata->room_before_arena.id[0], ch->pcdata->room_before_arena.id[1], ch->pcdata->room_before_arena.id[2]);
	else
		fprintf(fp, "Room_before_arena %ld\n", 	ch->pcdata->room_before_arena.id[0]);
    }

    fprintf(fp, "Not  %ld %ld %ld %ld %ld\n",
	(long int)ch->pcdata->last_note,(long int)ch->pcdata->last_idea,(long int)ch->pcdata->last_penalty,
	(long int)ch->pcdata->last_news,(long int)ch->pcdata->last_changes	);
    fprintf(fp, "Scro %d\n", 	ch->lines		);
    if (IS_IMMORTAL(ch))
		fprintf(fp, "LastInquiryRead %ld\n", (long int)ch->pcdata->last_project_inquiry);

	if( ch->checkpoint ) {
		if( ch->checkpoint->wilds )
			fprintf (fp, "Vroom %ld %ld %ld %ld\n",
				ch->checkpoint->x, ch->checkpoint->y, ch->checkpoint->wilds->pArea->uid, ch->checkpoint->wilds->uid);
		else if(ch->checkpoint->source)
			fprintf(fp,"CloneRoom %ld %ld %ld\n",
				ch->checkpoint->source->vnum, ch->checkpoint->id[0], ch->checkpoint->id[1]);
		else
			fprintf(fp,"Room %ld\n", ch->checkpoint->vnum);
	} else if(!ch->in_room)
		fprintf (fp, "Room %ld\n", (long int)ROOM_VNUM_TEMPLE);
	else if(ch->in_wilds) {
		fprintf (fp, "Vroom %ld %ld %ld %ld\n",
			ch->in_room->x, ch->in_room->y, ch->in_wilds->pArea->uid, ch->in_wilds->uid);
	} else if(ch->was_in_room) {
		if(ch->was_in_room->source)
			fprintf(fp,"CloneRoom %ld %ld %ld\n",
				ch->was_in_room->source->vnum, ch->was_in_room->id[0], ch->was_in_room->id[1]);
		else if( ch->was_in_wilds )
			fprintf (fp, "Vroom %ld %ld %ld %ld\n",
				ch->was_in_room->x, ch->was_in_room->y, ch->was_in_wilds->pArea->uid, ch->was_in_wilds->uid);
		else
			fprintf(fp,"Room %ld\n", ch->was_in_room->vnum);
	} else if(ch->in_room->source) {
		fprintf(fp,"CloneRoom %ld %ld %ld\n",
			ch->in_room->source->vnum, ch->in_room->id[0], ch->in_room->id[1]);
	} else
		fprintf(fp,"Room %ld\n", ch->in_room->vnum);

    if (ch->pcdata->ignoring != NULL)
    {
        IGNORE_DATA *ignore;

        for (ignore = ch->pcdata->ignoring; ignore != NULL;
              ignore = ignore->next)
	{
	    fprintf(fp,
	    "Ignore %s~%s~\n", ignore->name, ignore->reason);
	}
    }

    if (ch->pcdata->vis_to_people != NULL)
    {
	STRING_DATA *string;

	for (string = ch->pcdata->vis_to_people; string != NULL;
	      string = string->next)
	{
	    fprintf(fp,
	    "VisTo %s~\n", string->string);
	}
    }

    if (ch->pcdata->quiet_people != NULL)
    {
	STRING_DATA *string;

	for (string = ch->pcdata->quiet_people; string != NULL;
	      string = string->next)
	{
	    fprintf(fp,
	    "QuietTo %s~\n", string->string);
	}
    }

    if (IS_SITH(ch))
    {
	for (i = 0; i < MAX_TOXIN; i++)
	    fprintf(fp, "Toxn%s %d\n", toxin_table[i].name, ch->toxin[i]);
    }

    fprintf(fp, "HMV  %ld %ld %ld %ld %ld %ld\n",
	ch->hit, ch->max_hit, ch->mana, ch->max_mana, ch->move, ch->max_move);
    fprintf(fp, "HBS  %ld %ld %ld\n",
	ch->pcdata->hit_before,
	ch->pcdata->mana_before,
	ch->pcdata->move_before);
    fprintf(fp, "ManaStore  %d\n", ch->manastore);

    if (ch->gold > 0)
      fprintf(fp, "Gold %ld\n",	ch->gold		);
    else
      fprintf(fp, "Gold %d\n", 0			);
    if (ch->silver > 0)
	fprintf(fp, "Silv %ld\n",ch->silver		);
    else
	fprintf(fp, "Silv %d\n",0			);
    if (ch->pcdata->bankbalance > 0)
	fprintf(fp, "Bank %ld\n", ch->pcdata->bankbalance);
    else
	fprintf(fp, "Bank %d\n", 0);

    if (location_isset(&ch->before_social)) {
	if(ch->before_social.wuid)
		fprintf(fp, "Before_socialW %lu %lu %lu %lu\n", 	ch->before_social.wuid, ch->before_social.id[0], ch->before_social.id[1], ch->before_social.id[2]);
	else if(ch->before_social.id[1] || ch->before_social.id[2])
		fprintf(fp, "Before_socialC %lu %lu %lu\n", 	ch->before_social.id[0], ch->before_social.id[1], ch->before_social.id[2]);
	else
		fprintf(fp, "Before_social %ld\n", 	ch->before_social.id[0]);
    }

    if (ch->pneuma != 0)
	fprintf(fp, "Pneuma %ld\n", ch->pneuma);
    if (ch->home != 0)
	fprintf(fp, "Home %ld\n", ch->home);
    if (ch->questpoints != 0)
        fprintf(fp, "QuestPnts %d\n",  ch->questpoints);
    if (ch->pcdata->quests_completed != 0)
	fprintf(fp, "QuestsCompleted %ld\n", ch->pcdata->quests_completed);
    if (ch->deitypoints != 0)
	fprintf(fp, "DeityPnts %ld\n", ch->deitypoints);
    if (ch->nextquest != 0)
        fprintf(fp, "QuestNext %d\n",  ch->nextquest  );
    else if (ch->countdown != 0)
        fprintf(fp, "QuestNext %d\n",  10             );

    if (IS_QUESTING(ch)) {
		fprintf(fp, "Questing\n");
		fprintf(fp, "QuestGiverType %d\n", ch->quest->questgiver_type);
		fprintf(fp, "QuestGiver %ld\n", ch->quest->questgiver);
		fprintf(fp, "QuestReceiverType %d\n", ch->quest->questreceiver_type);
		fprintf(fp, "QuestReceiver %ld\n", ch->quest->questreceiver);

		fwrite_quest_part(fp, ch->quest->parts);
    }

    if (ch->countdown > 0)
	    fprintf(fp, "QCountDown %d\n", ch->countdown);

    fprintf(fp, "DeathCount %d\n",	ch->deaths			);
    fprintf(fp, "ArenaCount %d\n",	ch->arena_deaths			);
    fprintf(fp, "PKCount %d\n",	ch->player_deaths		);
    fprintf(fp, "CPKCount %d\n",	ch->cpk_deaths);
    fprintf(fp, "WarsWon %d\n",	ch->wars_won		);
    fprintf(fp, "ArenaKills %d\n",	ch->arena_kills		);
    fprintf(fp, "PKKills %d\n",	ch->player_kills		);
    fprintf(fp, "CPKKills %d\n",	ch->cpk_kills 	);
    fprintf(fp, "MonsterKills %ld\n",	ch->monster_kills		);

    fprintf(fp, "Exp  %ld\n",	ch->exp			);
    if (ch->act != 0)
	fprintf(fp, "Act  %s\n",   print_flags(ch->act));
    if (ch->act2 != 0)
	fprintf(fp, "Act2 %s\n",   print_flags(ch->act2));
    if (ch->affected_by != 0)		fprintf(fp, "AfBy %s\n",   print_flags(ch->affected_by));
    if (ch->affected_by2 != 0)		fprintf(fp, "AfBy2 %s\n",   print_flags(ch->affected_by2));

    // 20140514 NIB - adding for being able to reset the flags
    if (ch->affected_by_perm != 0)	fprintf(fp, "AfByPerm %s\n",   print_flags(ch->affected_by_perm));
    if (ch->affected_by2_perm != 0) fprintf(fp, "AfBy2Perm %s\n",   print_flags(ch->affected_by2_perm));
    if (ch->imm_flags != 0) fprintf(fp, "Immune %s\n",   print_flags(ch->imm_flags));
    if (ch->imm_flags_perm != 0) fprintf(fp, "ImmunePerm %s\n",   print_flags(ch->imm_flags_perm));
    if (ch->res_flags != 0) fprintf(fp, "Resist %s\n",   print_flags(ch->res_flags));
    if (ch->res_flags_perm != 0) fprintf(fp, "ResistPerm %s\n",   print_flags(ch->res_flags_perm));
    if (ch->vuln_flags != 0) fprintf(fp, "Vuln %s\n",   print_flags(ch->vuln_flags));
    if (ch->vuln_flags_perm != 0) fprintf(fp, "VulnPerm %s\n",   print_flags(ch->vuln_flags_perm));

    fprintf(fp, "Comm %s\n",       print_flags(ch->comm));
    if (ch->wiznet)
    	fprintf(fp, "Wizn %s\n",   print_flags(ch->wiznet));
    if (ch->invis_level)
	fprintf(fp, "Invi %d\n", 	ch->invis_level	);
    if (ch->incog_level)
	fprintf(fp,"Inco %d\n",ch->incog_level);
    fprintf(fp, "Pos  %d\n",
	ch->position == POS_FIGHTING ? POS_STANDING : ch->position);
    if (ch->practice != 0)
    	fprintf(fp, "Prac %d\n",	ch->practice	);
    if (ch->train != 0)
	fprintf(fp, "Trai %d\n",	ch->train	);
    if (ch->saving_throw != 0)
	fprintf(fp, "Save  %d\n",	ch->saving_throw);
    fprintf(fp, "Alig  %d\n",	ch->alignment		);
    if (ch->hitroll != 0)
	fprintf(fp, "Hit   %d\n",	ch->hitroll	);
    if (ch->damroll != 0)
	fprintf(fp, "Dam   %d\n",	ch->damroll	);
    fprintf(fp, "ACs %d %d %d %d\n",
	ch->armour[0],ch->armour[1],ch->armour[2],ch->armour[3]);
    if (ch->wimpy !=0)
	fprintf(fp, "Wimp  %d\n",	ch->wimpy	);
    fprintf(fp, "Attr %d %d %d %d %d\n",
	ch->perm_stat[STAT_STR],
	ch->perm_stat[STAT_INT],
	ch->perm_stat[STAT_WIS],
	ch->perm_stat[STAT_DEX],
	ch->perm_stat[STAT_CON]);

    fprintf (fp, "AMod %d %d %d %d %d\n",
	ch->mod_stat[STAT_STR],
	ch->mod_stat[STAT_INT],
	ch->mod_stat[STAT_WIS],
	ch->mod_stat[STAT_DEX],
	ch->mod_stat[STAT_CON]);

    if (ch->lostparts != 0)
	fprintf(fp, "LostParts  %s\n",   print_flags(ch->lostparts));

    if (IS_NPC(ch))
	fprintf(fp, "Vnum %ld\n",	ch->pIndexData->vnum	);
    else
    {
	fprintf(fp, "Pass %s~\n",	ch->pcdata->pwd		);
	fprintf(fp, "PassVers %d\n", ch->pcdata->pwd_vers);
	/*if (ch->pcdata->immortal->bamfin[0] != '\0')
	    fprintf(fp, "Bin  %s~\n",	ch->pcdata->immortal->bamfin);
	if (ch->pcdata->immortal->bamfout[0] != '\0')
		fprintf(fp, "Bout %s~\n",	ch->pcdata->immortal->bamfout); */
	fprintf(fp, "Titl %s~\n",	ch->pcdata->title	);
	if (ch->church != NULL)
        	fprintf(fp, "Church %s~\n",	ch->church->name	);
	fprintf(fp, "TSex %d\n",	ch->pcdata->true_sex	);
	fprintf(fp, "LLev %d\n",	ch->pcdata->last_level	);
	fprintf(fp, "HMVP %ld %ld %ld\n", ch->pcdata->perm_hit,
						   ch->pcdata->perm_mana,
						   ch->pcdata->perm_move);
	fprintf(fp, "Cnd  %d %d %d %d\n",
	    ch->pcdata->condition[0],
	    ch->pcdata->condition[1],
	    ch->pcdata->condition[2],
	    ch->pcdata->condition[3]);

	/* write alias */
        for (pos = 0; pos < MAX_ALIAS; pos++)
	{
	    if (ch->pcdata->alias[pos] == NULL
	    ||  ch->pcdata->alias_sub[pos] == NULL)
		break;

	    fprintf(fp,"Alias %s %s~\n",ch->pcdata->alias[pos],
		    ch->pcdata->alias_sub[pos]);
	}

	/*
	// Save song list
	for (sn = 0; sn < MAX_SONGS && music_table[sn].name; sn++)
		if( ch->pcdata->songs_learned[sn] )
			fprintf(fp, "Song '%s'\n", music_table[sn].name);

	for (sn = 0; sn < MAX_SKILL && skill_table[sn].name; sn++)
	{
	    if (skill_table[sn].name != NULL && ch->pcdata->learned[sn] != 0)
	    {
		fprintf(fp, "Sk %d '%s'\n",
		    ch->pcdata->learned[sn], skill_table[sn].name);
	    }
	    if (skill_table[sn].name != NULL && ch->pcdata->mod_learned[sn] != 0)
	    {
		fprintf(fp, "SkMod %d '%s'\n",
		    ch->pcdata->mod_learned[sn], skill_table[sn].name);
	    }
	}
	*/

	for (gn = 0; gn < MAX_GROUP; gn++)
        {
            if (group_table[gn].name != NULL && ch->pcdata->group_known[gn])
            {
                fprintf(fp, "Gr '%s'\n",group_table[gn].name);
            }
        }
    }

    for (paf = ch->affected; paf != NULL; paf = paf->next)
    {
	if (!paf->custom_name && (paf->type < 0 || paf->type>= MAX_SKILL))
	    continue;

	fprintf(fp, "%s '%s' '%s' %3d %3d %3d %3d %3d %10ld %10ld %d\n",
	    (paf->custom_name?"Affcgn":"Affcg"),
	    (paf->custom_name?paf->custom_name:skill_table[paf->type].name),
	    flag_string(affgroup_mobile_flags,paf->group),
	    paf->where,
	    paf->level,
	    paf->duration,
	    paf->modifier,
	    paf->location,
	    paf->bitvector,
	    paf->bitvector2,
	    paf->slot);
    }

    for (cmd = ch->pcdata->commands; cmd != NULL; cmd = cmd->next)
	fprintf(fp, "GrantedCommand %s~\n", cmd->name);
	#ifdef IMC
	imc_savechar( ch, fp );
	#endif
    fprintf(fp, "End\n\n");
}

extern pVARIABLE variable_head;
extern pVARIABLE variable_tail;

/*
 * Load a char and inventory into a new ch structure.
 */
bool load_char_obj(DESCRIPTOR_DATA *d, char *name)
{
    char strsave[MAX_INPUT_LENGTH];
    char buf[MSL];
    CHAR_DATA *ch;
    OBJ_DATA *obj;
    OBJ_DATA *objNestList[MAX_NEST];
    IMMORTAL_DATA *immortal;
    FILE *fp;
    bool found;
    int stat;
    TOKEN_DATA *token;
    pVARIABLE last_var = variable_tail;

    ch = new_char();
    ch->pcdata = new_pcdata();

    d->character			= ch;
    ch->desc				= d;
    ch->name				= str_dup(name);
    ch->id[0] = ch->id[1]		= 0;
    ch->pcdata->creation_date		= -1;
    ch->race				= race_lookup("human");
    ch->act				= PLR_NOSUMMON;
    ch->act2				= 0;
    ch->comm				= COMM_PROMPT;
    ch->num_grouped			= 0;
    ch->dead = FALSE;
    ch->prompt 				= str_dup("{B<{x%h{Bhp {x%m{Bm {x%v{Bmv>{x ");
    ch->pcdata->confirm_delete		= FALSE;
    ch->pcdata->pwd			= str_dup("");
	ch->pcdata->pwd_vers	= 0;
    //ch->pcdata->bamfin			= str_dup("");
    //ch->pcdata->bamfout			= str_dup("");
    ch->pcdata->title			= str_dup("");
    for (stat =0; stat < MAX_STATS; stat++)
    {
		ch->perm_stat[stat]		= 13;
		ch->mod_stat[stat]		= 0;
		ch->dirty_stat[stat]	= TRUE;
	}
    ch->pcdata->condition[COND_THIRST]	= 48;
    ch->pcdata->condition[COND_FULL]	= 48;
    ch->pcdata->condition[COND_HUNGER]	= 48;
    ch->pcdata->condition[COND_STONED]	= 0;
    ch->pcdata->security		= 0;
    ch->pcdata->challenge_delay		= 0;
    ch->morphed = FALSE;
    ch->locker_rent = 0;
    ch->deathsight_vision = 0;

	#ifdef IMC
	imc_initchar( ch );
	#endif

    found = FALSE;
    fclose(fpReserve);

    #if defined(unix)
    /* decompress if .gz file exists */
    sprintf(strsave, "%s%c/%s%s", PLAYER_DIR, tolower(name[0]), capitalize(name),".gz");
    if ((fp = fopen(strsave, "r")) != NULL)
    {
		fclose(fp);
		sprintf(buf,"gzip -dfq %s",strsave);
		system(buf);
    }
    #endif

    sprintf(strsave, "%s%c/%s", PLAYER_DIR, tolower(name[0]), capitalize(name));
    if ((fp = fopen(strsave, "r")) != NULL) {
		int iNest;

		for (iNest = 0; iNest < MAX_NEST; iNest++)
			rgObjNest[iNest] = NULL;

		found = TRUE;
		for (; ;)
		{
			char letter;
			char *word;

			letter = fread_letter(fp);
			if (letter == '*')
			{
			fread_to_eol(fp);
			continue;
			}

			if (letter != '#')
			{
			bug("Load_char_obj: # not found.", 0);
			break;
			}

			word = fread_word(fp);
			if (!str_cmp(word, "PLAYER"))
				fread_char(ch, fp);
			else if (!str_cmp(word, "OBJECT") || !str_cmp(word, "O"))
			{
				obj = fread_obj_new(fp);

				if (obj == NULL)
					continue;

				objNestList[obj->nest] = obj;
				if (obj->locker == TRUE)
				{
					obj_to_locker(obj, ch);
					continue;
				}

				if (obj->nest == 0) {
					 obj_to_char(obj, ch);

					 if( obj->wear_loc != WEAR_NONE ) {
						 list_addlink(ch->lworn, obj);
					 }
				} else {
					OBJ_DATA *container = objNestList[obj->nest - 1];

					if (container->item_type == ITEM_CONTAINER ||
						container->item_type == ITEM_KEYRING ||
						container->item_type == ITEM_WEAPON_CONTAINER)
						obj_to_obj(obj,objNestList[obj->nest - 1]);
					else {
						sprintf(buf, "load_char_obj: found obj %s(%ld) in item %s(%ld) which is not a container",
							obj->short_descr, obj->pIndexData->vnum,
							container->short_descr, container->pIndexData->vnum);
						log_string(buf);
						obj_to_char(obj, ch);
					}
				}
			} else if (!str_cmp(word, "L")) {
				obj = fread_obj_new(fp);
				obj_to_locker(obj, ch);
			} else if (!str_cmp(word, "TOKEN")) {
				token = fread_token(fp);
				token_to_char(token, ch);
			} else if (!str_cmp(word, "SKILL")) {
				fread_skill(fp, ch);
			} else if (!str_cmp(word, "END"))
				break;
			else {
				bug("Load_char_obj: bad section.", 0);
				break;
			}
		}

		fclose(fp);
    }
    fpReserve = fopen(NULL_FILE, "r");

	if(!IS_NPC(ch)) {
		if(ch->pcdata->creation_date < 0) {
			ch->pcdata->creation_date = ch->id[0];
			ch->id[0] = 0;
		}
		if(!ch->pcdata->creation_date)
			ch->pcdata->creation_date = get_pc_id();
	}

	get_mob_id(ch);

    // Fix char.
    if (found)
	fix_character(ch);

    /* Redo shift. Remember ch->shifted was just used as a placeholder to tell the game
       to re-shift, so we have to switch it to none first. */
    if (ch->shifted != SHIFTED_NONE) {
	ch->shifted = SHIFTED_NONE;
	shift_char(ch, TRUE);
    }

    /* The immortal-only information associated with an imm is stored in a seperate list
       (immortal_list) so that it is always accessible, instead of only when the char
       is logged in. On game shutdown it is written to ../data/world/staff.dat. When
       the player logs in, a pointer to the immortal staff entry is set up for them
       here. */
    if (ch->tot_level >= LEVEL_IMMORTAL) {
	/* If their immortal isn't found, give them a blank one so we don't segfault. */
	if ((immortal = find_immortal(ch->name)) == NULL) {
	    sprintf(buf, "load_char_obj: no immortal_data found for immortal character %s!", ch->name);
	    bug(buf, 0);

	    immortal = new_immortal();
	    immortal->name = str_dup(ch->name);
	    //immortal->level = ch->tot_level;
	    ch->pcdata->immortal = immortal;
	    immortal->pc = ch->pcdata;

	    add_immortal(immortal);

	} else { // Readjust the char's level accordingly.
	    sprintf(buf, "load_char_obj: reading immortal char %s.\n\r", ch->name);
	    log_string(buf);

	    ch->pcdata->immortal = immortal;
	    //ch->level = immortal->level;
	    //ch->tot_level = immortal->level;
	}
    }

	variable_fix_list(last_var ? last_var : variable_head);

    ch->pcdata->last_login = current_time;
    return found;
}


/*
 * Read in a char.
 */
void fread_char(CHAR_DATA *ch, FILE *fp)
{
    AREA_DATA *pArea = NULL;
    char buf[MAX_STRING_LENGTH];
    char *word;
    char *race_string;
    char *immortal_flag = str_dup("");
    bool fMatch;
    bool fMatchFound = FALSE;
    int count = 0;
    int lastlogoff = current_time;
    int percent;
    int i = 0;

    sprintf(buf,"save.c, fread_char: reading %s.",ch->name);
    log_string(buf);

	ch->version = VERSION_PLAYER_000;
    for (; ;)
    {
	word   = feof(fp) ? "End" : fread_word(fp);
	fMatch = FALSE;

	switch (UPPER(word[0]))
	{
	case '*':
	    fMatch = TRUE;
	    fread_to_eol(fp);
	    break;

	case '#':
	    if (!str_cmp(word, "#QUESTPART")) {
		QUEST_PART_DATA *part;

		if (ch->quest == NULL) {
		    bug("had ch with a quest part but no quest", 0);
		    fread_to_eol(fp);
		    break;
		}

		part = fread_quest_part(fp);

		part->next = ch->quest->parts;
		ch->quest->parts = part;
		break;
	    }

	case 'A':
	    KEY("Act",		ch->act,		fread_flag(fp));
	    KEY("Act2",	ch->act2,		fread_flag(fp));
	    KEY("AffectedBy",	ch->affected_by,	fread_flag(fp));
	    KEY("AfBy",	ch->affected_by,	fread_flag(fp));
	    KEY("AfBy2",	ch->affected_by2,	fread_flag(fp));
	    KEY("AfByPerm", ch->affected_by_perm,	fread_flag(fp));
	    KEY("AfBy2Perm", ch->affected_by2_perm,	fread_flag(fp));
	    KEY("Alignment",	ch->alignment,		fread_number(fp));
	    KEY("Alig",	ch->alignment,		fread_number(fp));
	    KEY("ArenaCount",	ch->arena_deaths,	fread_number(fp));
	    KEY("ArenaKills",	ch->arena_kills,	fread_number(fp));
	    KEY("Afk_message", ch->pcdata->afk_message, fread_string(fp));

	    if (!str_cmp(word, "Alia"))
	    {
		if (count >= MAX_ALIAS)
		{
		    fread_to_eol(fp);
		    fMatch = TRUE;
		    break;
		}

		ch->pcdata->alias[count] 	= str_dup(fread_word(fp));
		ch->pcdata->alias_sub[count]	= str_dup(fread_word(fp));
		count++;
		fMatch = TRUE;
		break;
	    }

            if (!str_cmp(word, "Alias"))
            {
                if (count >= MAX_ALIAS)
                {
                    fread_to_eol(fp);
                    fMatch = TRUE;
                    break;
                }

                ch->pcdata->alias[count]        = str_dup(fread_word(fp));
                ch->pcdata->alias_sub[count]    = fread_string(fp);
                count++;
                fMatch = TRUE;
                break;
            }

	    if (!str_cmp(word, "AC") || !str_cmp(word,"Armour"))
	    {
		fread_to_eol(fp);
		fMatch = TRUE;
		break;
	    }

	    if (!str_cmp(word,"ACs"))
	    {
		int i;

		for (i = 0; i < 4; i++)
		    ch->armour[i] = fread_number(fp);
		fMatch = TRUE;
		break;
	    }

	    if (!str_cmp(word, "AffD"))
	    {
		AFFECT_DATA *paf;
		int sn;

		paf = new_affect();

		sn = skill_lookup(fread_word(fp));
		if (sn < 0)
		    log_string("fread_char: unknown skill.");
		else
		    paf->type = sn;

		paf->level	= fread_number(fp);
		paf->duration	= fread_number(fp);
		paf->modifier	= fread_number(fp);
		paf->location	= fread_number(fp);
		paf->bitvector	= fread_number(fp);
		paf->next	= ch->affected;
		ch->affected	= paf;
		fMatch = TRUE;
		break;
	    }

            if (!str_cmp(word, "Affc"))
            {
                AFFECT_DATA *paf;
                int sn;

                paf = new_affect();

                sn = skill_lookup(fread_word(fp));
                if (sn < 0)
                    log_string("fread_char: unknown skill.");
                else
                    paf->type = sn;

		paf->custom_name = NULL;
		paf->group  = AFFGROUP_MAGICAL;
                paf->where  = fread_number(fp);
                paf->level      = fread_number(fp);
                paf->duration   = fread_number(fp);
                paf->modifier   = fread_number(fp);
                paf->location   = fread_number(fp);
                paf->bitvector  = fread_number(fp);
		if (ch->version >= 9)
		    paf->bitvector2 = fread_number(fp);
                paf->next       = ch->affected;
                ch->affected    = paf;
                fMatch = TRUE;
                break;
            }

            if (!str_cmp(word, "Affcg"))
            {
                AFFECT_DATA *paf;
                int sn;

                paf = new_affect();

                sn = skill_lookup(fread_word(fp));
                if (sn < 0)
                    log_string("fread_char: unknown skill.");
                else
					paf->type = sn;

				paf->custom_name = NULL;
				paf->group  = flag_value(affgroup_mobile_flags,fread_word(fp));
				if(paf->group == NO_FLAG) paf->group = AFFGROUP_MAGICAL;
                paf->where  = fread_number(fp);
                paf->level      = fread_number(fp);
                paf->duration   = fread_number(fp);
                paf->modifier   = fread_number(fp);
                paf->location   = fread_number(fp);
                if(paf->location == APPLY_SKILL) {
					int sn = skill_lookup(fread_word(fp));
					if(sn < 0) {
						paf->location = APPLY_NONE;
						paf->modifier = 0;
					} else
						paf->location += sn;
				}
                paf->bitvector  = fread_number(fp);
				if (ch->version >= 9)
				    paf->bitvector2 = fread_number(fp);
				if (ch->version >= VERSION_PLAYER_004)
					paf->slot = fread_number(fp);
                paf->next       = ch->affected;
                ch->affected    = paf;
                fMatch = TRUE;
                break;
            }

            if (!str_cmp(word, "Affcn"))
            {
                AFFECT_DATA *paf;
                char *name;

                paf = new_affect();

                name = create_affect_cname(fread_word(fp));
                if (!name) {
                    log_string("fread_char: could not create affect name.");
                    free_affect(paf);
                } else
                    paf->custom_name = name;

				paf->type = -1;
				paf->group  = AFFGROUP_MAGICAL;
                paf->where  = fread_number(fp);
                paf->level      = fread_number(fp);
                paf->duration   = fread_number(fp);
                paf->modifier   = fread_number(fp);
                paf->location   = fread_number(fp);
                paf->bitvector  = fread_number(fp);
				if (ch->version >= 9)
				    paf->bitvector2 = fread_number(fp);
                paf->next       = ch->affected;
                ch->affected    = paf;
                fMatch = TRUE;
                break;
            }

            if (!str_cmp(word, "Affcgn"))
            {
                AFFECT_DATA *paf;
                char *name;

                paf = new_affect();

                name = create_affect_cname(fread_word(fp));
                if (!name) {
                    log_string("fread_char: could not create affect name.");
                    free_affect(paf);
                } else
                    paf->custom_name = name;

				paf->type = -1;
				paf->group  = flag_value(affgroup_mobile_flags,fread_word(fp));
				if(paf->group == NO_FLAG) paf->group = AFFGROUP_MAGICAL;
                paf->where  = fread_number(fp);
                paf->level      = fread_number(fp);
                paf->duration   = fread_number(fp);
                paf->modifier   = fread_number(fp);
                paf->location   = fread_number(fp);
                if(paf->location == APPLY_SKILL) {
					int sn = skill_lookup(fread_word(fp));
					if(sn < 0) {
						paf->location = APPLY_NONE;
						paf->modifier = 0;
					} else
					paf->location += sn;
				}
                paf->bitvector  = fread_number(fp);
				if (ch->version >= 9)
				    paf->bitvector2 = fread_number(fp);
				if (ch->version >= VERSION_PLAYER_004)
					paf->slot = fread_number(fp);
                paf->next       = ch->affected;
                ch->affected    = paf;
                fMatch = TRUE;
                break;
            }

	    if (!str_cmp(word, "AttrMod" ) || !str_cmp(word,"AMod"))
	    {
		int stat;
		for (stat = 0; stat < MAX_STATS; stat ++)
		   set_mod_stat(ch, stat, fread_number(fp));
		fMatch = TRUE;
		break;
	    }

	    if (!str_cmp(word, "AttrPerm") || !str_cmp(word,"Attr"))
	    {
		int stat;

		for (stat = 0; stat < MAX_STATS; stat++)
			set_perm_stat(ch, stat, fread_number(fp));
		fMatch = TRUE;
		break;
	    }
	    break;

	case 'B':
	    //KEY("Bamfin",	ch->pcdata->immortal->bamfin,	fread_string(fp));
	    //KEY("Bamfout",	ch->pcdata->immortal->bamfout,	fread_string(fp));
	    //KEY("Bin",		ch->pcdata->immortal->bamfin,	fread_string(fp));
	    //KEY("Bout",	ch->pcdata->immortal->bamfout,	fread_string(fp));
	    KEY("Bank",	ch->pcdata->bankbalance, fread_number(fp));
            if (!str_cmp(word, "Before_social")) {
		location_set(&ch->before_social,0,fread_number(fp),0,0);
		fMatch = TRUE;
	    }
            if (!str_cmp(word, "Before_socialC")) {
		location_set(&ch->before_social,0,fread_number(fp),fread_number(fp),fread_number(fp));
		fMatch = TRUE;
	    }
            if (!str_cmp(word, "Before_socialW")) {
		location_set(&ch->before_social,fread_number(fp),fread_number(fp),fread_number(fp),fread_number(fp));
		fMatch = TRUE;
	    }
		break;

	case 'C':
	    KEY("Class",    ch->pcdata->class_current,         fread_number(fp));
	    KEY("Cla",	     ch->pcdata->class_current,         fread_number(fp));
	    KEY("ChDelay",  ch->pcdata->challenge_delay,  fread_number(fp));
	    KEY("CPKCount",  ch->cpk_deaths,  fread_number(fp));
	    KEY("ChannelFlags",		ch->pcdata->channel_flags, fread_number(fp));
	    KEY("CPKKills", ch->cpk_kills, fread_number(fp));

	    if (!str_cmp(word, "Church"))
	    {
		CHURCH_DATA *church;
		CHURCH_PLAYER_DATA *member;
		ch->church_name = fread_string(fp);

		for (church = church_list; church != NULL;
		     church = church->next)
		{
	   	    if (!str_cmp(ch->church_name, church->name))
		        break;
		}

		if (church != NULL)
		{
		    ch->church = church;
		    for (member = church->people; member != NULL;
			 member = member->next)
		    {
		        if (!str_cmp(member->name, ch->name))
			    break;
		    }

		    if (member != NULL)
		    {
		        member->ch = ch;
			ch->church = church;
			ch->church_member = member;
			ch->church_member->sex = ch->sex;
			ch->church_member->alignment = ch->alignment;

			if (!str_cmp(ch->church->founder, ch->name))
			{
			    ch->church->founder_last_login = current_time;
			}
		    }
		    else
		    {
		        sprintf(buf,
			"No church member found for %s, church_name %s",
			    ch->name, ch->church_name);
			log_string(buf);
			ch->church = NULL;
			ch->church_member = NULL;
			free_string(ch->church_name);
		    }
		}
		else
		{
                    sprintf(buf,
		    "Couldn't load ch church for %s, church %s",
		        ch->name, ch->church_name);
			bug(buf, 0);
			ch->church = NULL;
			ch->church_member = NULL;
			free_string(ch->church_name);
		}

		fMatch = TRUE;
		break;
	    }

	    if (!str_cmp(word, "CloneRoom"))
	    {
		    ROOM_INDEX_DATA *room;
		    room = get_room_index(fread_number(fp));
		    ch->in_room = get_clone_room(room,fread_number(fp),fread_number(fp));

			if (ch->in_room == NULL)
				ch->in_room = get_room_index(11001);
		fMatch = TRUE;
		break;
	    }

	    if (!str_cmp(word, "Condition") || !str_cmp(word,"Cond"))
	    {
		ch->pcdata->condition[0] = fread_number(fp);
		ch->pcdata->condition[1] = fread_number(fp);
		ch->pcdata->condition[2] = fread_number(fp);
		fMatch = TRUE;
		break;
	    }
            if (!str_cmp(word,"Cnd"))
            {
                ch->pcdata->condition[0] = fread_number(fp);
                ch->pcdata->condition[1] = fread_number(fp);
                ch->pcdata->condition[2] = fread_number(fp);
		ch->pcdata->condition[3] = fread_number(fp);
                fMatch = TRUE;
                break;
            }
	    KEY("Comm",		ch->comm,		fread_flag(fp));
	    KEY("Created",	ch->pcdata->creation_date,	fread_number(fp));

	    break;

	case 'D':
	    KEY("Damroll",	ch->damroll,		fread_number(fp));
	    KEY("Dam",		ch->damroll,		fread_number(fp));
	    KEY("DeathCount",	ch->deaths,		fread_number(fp));
	    KEY("DeityPnts",   ch->deitypoints,	fread_number(fp));
	    KEY("Description",	ch->description,	fread_string(fp));
	    KEY("Desc",	ch->description,	fread_string(fp));
	    KEY("DangerRange",	ch->pcdata->danger_range, fread_number(fp));
	    KEY("DeathTimeLeft", ch->time_left_death,	fread_number(fp));

	    if (!str_cmp(word, "Dead")) {
		ch->dead = TRUE;
		fMatch = TRUE;
	    }

	    break;

	case 'E':
	    KEY("Exp",		ch->exp,		fread_number(fp));
	    KEYS("Email",	ch->pcdata->email,	fread_string(fp));

	    if (!str_cmp(word, "End"))
	    {
    		/* adjust hp mana move up  -- here for speed's sake */
    		percent = (current_time - lastlogoff) * 25 / (2 * 60 * 60);
		percent = UMIN(percent,100);

    		if (percent > 0 && !IS_AFFECTED(ch,AFF_POISON)
    		&&  !IS_AFFECTED(ch,AFF_PLAGUE))
    		{
        	    ch->hit	+= (ch->max_hit - ch->hit) * percent / 100;
        	    ch->mana    += (ch->max_mana - ch->mana) * percent / 100;
        	    ch->move    += (ch->max_move - ch->move)* percent / 100;
    		}

    		// If the locker rent is set but before Midnight March 31th, 2009 PDT
    		// Set it to the current time plus one month
    		if (ch->locker_rent > 0 && ch->locker_rent < 1238486400) {
			struct tm *rent_time;

			ch->locker_rent = current_time;
			rent_time = (struct tm *) localtime(&ch->locker_rent);

			rent_time->tm_mon += 1;
			ch->locker_rent = (time_t) mktime(rent_time);
		}

		// Set the immortal flag IF they have the immortal structure
		if(ch->pcdata && ch->pcdata->immortal)
			ch->pcdata->immortal->imm_flag = IS_NULLSTR(immortal_flag) ? str_dup("{R  Immortal  {x") : immortal_flag;

		//ch->version = 9;
		return;
	    }
	    break;

	case 'F':
	    KEYS("Flag",		ch->pcdata->flag,	fread_string(fp));

	case 'G':
	    KEY("Gold",	ch->gold,		fread_number(fp));

	    if (!str_cmp(word, "GrantedCommand"))
	    {
		COMMAND_DATA *cmd;

		cmd = new_command();
		cmd->name = fread_string(fp);
                cmd->next = ch->pcdata->commands;
		ch->pcdata->commands = cmd;

		fMatch = TRUE;
		break;
	    }

	    if (!str_cmp(word, "Group")  || !str_cmp(word,"Gr"))
            {
                int gn;
                char *temp;

                temp = fread_word(fp) ;
                gn = group_lookup(temp);
                if (gn < 0)
                {
                    sprintf(buf, "fread_char: unknown group %s.", temp);
		    log_string(buf);
                }
                else
                	ch->pcdata->group_known[gn] = TRUE;

				fMatch = TRUE;
				break;
            }

	    break;

	case 'H':
	    KEY("Hitroll",	ch->hitroll,		fread_number(fp));
	    KEY("Hit",		ch->hitroll,		fread_number(fp));
	    KEY("Home",	ch->home,		fread_number(fp));

	    if (!str_cmp(word, "HBS"))
	    {
		ch->pcdata->hit_before  = fread_number(fp);
		ch->pcdata->mana_before	= fread_number(fp);
		ch->pcdata->move_before	= fread_number(fp);
		fMatch = TRUE;
		break;
	    }

	    if (!str_cmp(word, "HpManaMove") || !str_cmp(word,"HMV"))
	    {
		ch->hit		= fread_number(fp);
		ch->max_hit	= fread_number(fp);
		ch->mana	= fread_number(fp);
		ch->max_mana	= fread_number(fp);
		ch->move	= fread_number(fp);
		ch->max_move	= fread_number(fp);
		fMatch = TRUE;
		break;
	    }

            if (!str_cmp(word, "HpManaMovePerm") || !str_cmp(word,"HMVP"))
            {
		long qp_number = 0;

                ch->pcdata->perm_hit	= fread_number(fp);
                ch->pcdata->perm_mana   = fread_number(fp);
                ch->pcdata->perm_move   = fread_number(fp);
                fMatch = TRUE;

		if (IS_IMMORTAL(ch))
		    break;

		// Hack to make all chars who have hp/mana/move over there max down
		if (ch->pcdata->perm_hit > pc_race_table[ ch->race ].max_vital_stats[ MAX_HIT ] + 11)
		{
		    // Get difference in hp, and divide it by 10 to find the number of train sessions.
		    qp_number = (ch->pcdata->perm_hit - pc_race_table[ ch->race ].max_vital_stats[ MAX_HIT ]) / 10;
		    // Multiply sessions by 15 being number of pracs.
		    qp_number *= 15;
		    ch->questpoints += qp_number;

		    ch->pcdata->perm_hit = pc_race_table[ ch->race ].max_vital_stats[ MAX_HIT ];
		}

		if (ch->pcdata->perm_mana > pc_race_table[ ch->race ].max_vital_stats[ MAX_MANA ] + 11)
		{
		    // Get difference in hp, and divide it by 10 to find the number of train sessions.
		    qp_number = (ch->pcdata->perm_mana - pc_race_table[ ch->race ].max_vital_stats[ MAX_MANA ]) / 10;
		    // Multiply sessions by 15 being number of pracs.
		    qp_number *= 15;
		    ch->questpoints += qp_number;

		    ch->pcdata->perm_mana = pc_race_table[ ch->race ].max_vital_stats[ MAX_MANA ];
		}

		if (ch->pcdata->perm_move > pc_race_table[ ch->race ].max_vital_stats[ MAX_MOVE ] + 11)
		{
		    // Get difference in hp, and divide it by 10 to find the number of train sessions.
		    qp_number = (ch->pcdata->perm_move - pc_race_table[ ch->race ].max_vital_stats[ MAX_MOVE ]) / 10;
		    // Multiply sessions by 15 being number of pracs.
		    qp_number *= 15;
		    ch->questpoints += qp_number;

		    ch->pcdata->perm_move = pc_race_table[ ch->race ].max_vital_stats[ MAX_MOVE ];
		}

                break;
            }

	    break;

	case 'I':
	    KEY("Id",	ch->id[0],		fread_number(fp));
	    KEY("Id2",	ch->id[1],		fread_number(fp));
	    KEY("InvisLevel",	ch->invis_level,	fread_number(fp));
	    if (!str_cmp(word, "ImmFlag"))
	    {
		free_string(immortal_flag);
		immortal_flag = fread_string(fp);\
		}
	    KEY("Immune", ch->imm_flags,	fread_flag(fp));
	    KEY("ImmunePerm", ch->imm_flags_perm,	fread_flag(fp));

	    KEY("Inco",	ch->incog_level,	fread_number(fp));
	    KEY("Invi",	ch->invis_level,	fread_number(fp));

	    if (!str_cmp(word, "Ignore"))
	    {
	        IGNORE_DATA *ignore;

	        ignore = new_ignore();
	        ignore->next = ch->pcdata->ignoring;
	        ch->pcdata->ignoring = ignore;

	        ignore->name   = fread_string(fp);
	        ignore->reason = fread_string(fp);
	        fMatch = TRUE;
	        break;
	    }
	#ifdef IMC
	if( ( fMatch = imc_loadchar( ch, fp, word ) ) )
	break;
	#endif
	    break;

	case 'L':
	    KEY("LastLevel",	ch->pcdata->last_level, fread_number(fp));
	    KEY("LLev",	ch->pcdata->last_level, fread_number(fp));
	    KEY("Level",	ch->level,		fread_number(fp));
	    KEY("Lev",		ch->level,		fread_number(fp));
	    KEY("Levl",	ch->level,		fread_number(fp));
	    KEY("LogO",	lastlogoff,		fread_number(fp));
	    KEY("LogI",	ch->pcdata->last_login,	fread_number(fp));
	    KEY("LongDescr",	ch->long_descr,		fread_string(fp));
	    KEY("LnD",		ch->long_descr,		fread_string(fp));
	    KEY("LockerRent",	ch->locker_rent,	fread_number(fp));
	    KEY("LastInquiryRead",ch->pcdata->last_project_inquiry, fread_number(fp));
	    KEY("LostParts",	ch->lostparts,		fread_flag(fp));

	    // fix mistake where date got saved as a string to the file
	    if (!str_cmp(word, "LastLogin")) {
		fread_to_eol(fp);
		fMatch = TRUE;
	    }

	    if (!str_cmp(word, "LogO"))
	    {
		long time;

		time = fread_number(fp);
		ch->pcdata->last_logoff = time;
		lastlogoff = time;
		fMatch = TRUE;
		break;
	    }

	    break;

	case 'M':
	    KEY("Mc0",		 ch->pcdata->class_mage,		fread_number(fp));
	    KEY("Mc1",		 ch->pcdata->class_cleric,		fread_number(fp));
	    KEY("Mc2",		 ch->pcdata->class_thief,		fread_number(fp));
	    KEY("Mc3",		 ch->pcdata->class_warrior,		fread_number(fp));
	    KEY("MonsterKills", ch->monster_kills,	fread_number(fp));

	    /*
	    if (!str_cmp(word, "Mount"))
	    {
		int vnum;
		CHAR_DATA *mount;

		vnum = fread_number(fp);

		mount = create_mobile(get_mob_index(vnum), FALSE);
		ch->mount = mount;
		mount->rider = ch;
		ch->riding = TRUE;
		mount->riding = TRUE;
		fMatch = TRUE;
	    }
	    */
	    break;

	case 'N':
	    KEY("Name",	ch->name,		fread_string(fp));
	    KEY("Note",	ch->pcdata->last_note,	fread_number(fp));
	    KEY("Need_change_pw", ch->pcdata->need_change_pw, fread_number(fp));

	    if (!str_cmp(word,"Not"))
	    {
		ch->pcdata->last_note			= fread_number(fp);
		ch->pcdata->last_idea			= fread_number(fp);
		ch->pcdata->last_penalty		= fread_number(fp);
		ch->pcdata->last_news			= fread_number(fp);
		ch->pcdata->last_changes		= fread_number(fp);
		fMatch = TRUE;
		break;
	    }

	    break;

	case 'O':
	    if (!str_cmp(word,"OwnerOfShip"))
	    {
		//ch->pcdata->owner_of_boat_before_logoff		= fread_string(fp);
		fMatch = TRUE;
		break;
	    }

	    break;

	case 'P':
	    KEY("Password",	ch->pcdata->pwd,	fread_string(fp));
	    KEY("Pass",	ch->pcdata->pwd,	fread_string(fp));
		KEY("PassVers", ch->pcdata->pwd_vers,	fread_number(fp))
	    KEY("Played",	ch->played,		fread_number(fp));
	    KEY("Plyd",	ch->played,		fread_number(fp));
	    KEY("Position",	ch->position,		fread_number(fp));
	    KEY("Pos",		ch->position,		fread_number(fp));
	    KEY("Practice",	ch->practice,		fread_number(fp));
	    KEY("Prac",	ch->practice,		fread_number(fp));
            KEYS("Prompt",     ch->prompt,             fread_string(fp));
 	    KEYS("Prom",	ch->prompt,		fread_string(fp));
	    KEY("PKCount",	ch->player_deaths,      fread_number(fp));
	    KEY("PKKills",	ch->player_kills,	fread_number(fp));
	    KEY("Pneuma",      ch->pneuma,	        fread_number(fp));

	    break;
        case 'Q':
            KEY("QuestPnts",   ch->questpoints,        fread_number(fp));
            KEY("QuestNext",   ch->nextquest,          fread_number(fp));
	    KEY("QCountDown",  ch->countdown,		fread_number(fp));
	    KEY("QuestsCompleted",
                ch->pcdata->quests_completed, fread_number(fp));

	    if (!str_cmp(word, "QuietTo"))
	    {
		STRING_DATA *string;

		string = new_string_data();
		string->next = ch->pcdata->quiet_people;
		ch->pcdata->quiet_people = string;

		string->string = fread_string(fp);
		fMatch = TRUE;
		break;
	    }

	    if (!str_cmp(word, "Questing"))
	    {
			ch->quest = (QUEST_DATA *)new_quest();
			fMatch = TRUE;
			break;
		}

	    if (!str_cmp(word, "QuestGiverType"))
	    {
			if( ch->quest != NULL )
			{
				ch->quest = (QUEST_DATA *)new_quest();
			}
			ch->quest->questgiver_type = fread_number(fp);
			fMatch = TRUE;
			break;
	    }

	    if (!str_cmp(word, "QuestGiver"))
	    {
			if( ch->quest != NULL )
			{
				ch->quest = (QUEST_DATA *)new_quest();
			}
			ch->quest->questgiver = fread_number(fp);
			fMatch = TRUE;
			break;
	    }

	    if (!str_cmp(word, "QuestReceiverType"))
	    {
			if( ch->quest != NULL )
			{
				ch->quest = (QUEST_DATA *)new_quest();
			}
			ch->quest->questreceiver_type = fread_number(fp);
			fMatch = TRUE;
			break;
	    }

	    if (!str_cmp(word, "QuestReceiver"))
	    {
			if( ch->quest != NULL )
			{
				ch->quest = (QUEST_DATA *)new_quest();
			}
			ch->quest->questreceiver = fread_number(fp);
			fMatch = TRUE;
			break;
	    }

	    if (!str_cmp(word, "QOPart"))
	    {
	 	QUEST_PART_DATA *part;
		if (ch->quest == NULL)
		    ch->quest = (QUEST_DATA *)new_quest();

		part = (QUEST_PART_DATA *)new_quest_part();
		part->mob = -1;
		part->obj = fread_number(fp);

		part->next = ch->quest->parts;
		ch->quest->parts = part;
		fMatch = TRUE;
		break;
	    }

	    if (!str_cmp(word, "QOSPart"))
	    {
	 	QUEST_PART_DATA *part;
		if (ch->quest == NULL)
		    ch->quest = (QUEST_DATA *)new_quest();

		part = (QUEST_PART_DATA *)new_quest_part();
		part->obj_sac = fread_number(fp);

		part->next = ch->quest->parts;
		ch->quest->parts = part;
		fMatch = TRUE;
		break;
	    }

	    if (!str_cmp(word, "QMRPart"))
	    {
	 	QUEST_PART_DATA *part;
		if (ch->quest == NULL)
		    ch->quest = (QUEST_DATA *)new_quest();

		part = (QUEST_PART_DATA *)new_quest_part();
		part->mob_rescue = fread_number(fp);

		part->next = ch->quest->parts;
		ch->quest->parts = part;
		fMatch = TRUE;
		break;
	    }

	    if (!str_cmp(word, "QMPart"))
	    {
	 	QUEST_PART_DATA *part;
		if (ch->quest == NULL)
		    ch->quest = (QUEST_DATA *)new_quest();

		part = (QUEST_PART_DATA *)new_quest_part();
		part->mob = fread_number(fp);

		part->next = ch->quest->parts;
		ch->quest->parts = part;
		fMatch = TRUE;
		break;
	    }

	    if (!str_cmp(word, "QRoom"))
	    {
	 	QUEST_PART_DATA *part;
		if (ch->quest == NULL)
		    ch->quest = (QUEST_DATA *)new_quest();

		part = (QUEST_PART_DATA *)new_quest_part();
		part->room = fread_number(fp);

		part->next = ch->quest->parts;
		ch->quest->parts = part;
		fMatch = TRUE;
		break;
	    }


	    if (!str_cmp(word, "QComplete"))
	    {
		ch->quest->parts->complete = TRUE;
		fMatch = TRUE;
	    }

            break;

	case 'R':
            if (!str_cmp(word, "RepopRoom")) {
		location_set(&ch->recall,0,fread_number(fp),0,0);
		fMatch = TRUE;
	    }
            if (!str_cmp(word, "RepopRoomC")) {
		location_set(&ch->recall,0,fread_number(fp),fread_number(fp),fread_number(fp));
		fMatch = TRUE;
	    }
            if (!str_cmp(word, "RepopRoomW")) {
		location_set(&ch->recall,fread_number(fp),fread_number(fp),fread_number(fp),fread_number(fp));
		fMatch = TRUE;
	    }
            if (!str_cmp(word, "Room_before_arena")) {
		location_set(&ch->pcdata->room_before_arena,0,fread_number(fp),0,0);
		fMatch = TRUE;
	    }
            if (!str_cmp(word, "Room_before_arenaC")) {
		location_set(&ch->pcdata->room_before_arena,0,fread_number(fp),fread_number(fp),fread_number(fp));
		fMatch = TRUE;
	    }
            if (!str_cmp(word, "Room_before_arenaW")) {
		location_set(&ch->pcdata->room_before_arena,fread_number(fp),fread_number(fp),fread_number(fp),fread_number(fp));
		fMatch = TRUE;
	    }
	    KEY("RMc0",		 ch->pcdata->second_class_mage,		fread_number(fp));
	    KEY("RMc1",		 ch->pcdata->second_class_cleric,	fread_number(fp));
	    KEY("RMc2",		 ch->pcdata->second_class_thief,	fread_number(fp));
	    KEY("RMc3",		 ch->pcdata->second_class_warrior,	fread_number(fp));

            if (!str_cmp(word, "Race"))
	    {
                 race_string = fread_string(fp);
		 if (!str_cmp(race_string, "werewolf"))
		     ch->race = race_lookup("sith");
		 else if (!str_cmp(race_string, "guru"))
	             ch->race = race_lookup("mystic");
		 else if (!str_cmp(race_string, "high elf"))
		     ch->race = race_lookup("seraph");
		 else if (!str_cmp(race_string, "high dwarf"))
		     ch->race = race_lookup("berserker");
		 else if (!str_cmp(race_string, "shade"))
		     ch->race = race_lookup("specter");
		 else
		     ch->race = race_lookup(race_string);

		 free_string(race_string);
                 fMatch = TRUE;
                 break;
            }

	    KEY("Resist", ch->res_flags,	fread_flag(fp));
	    KEY("ResistPerm", ch->res_flags_perm,	fread_flag(fp));

	    if (!str_cmp(word, "Room"))
	    {
		ch->in_room = get_room_index(fread_number(fp));
		if ((ch->in_room == NULL) /*|| (ch->tot_level < 150 && !ch->in_room->area->open)*/)
		    ch->in_room = get_room_index(11001);
		fMatch = TRUE;
		break;
	    }


	    fMatchFound = FALSE;/*
	    for (i = 0; i < 3; i++)
            {
	        char buf2[MSL];

		sprintf(buf2, "Rank%d", i);
		if (!str_cmp(word, buf2))
		{
		    ch->pcdata->rank[i] = fread_number(fp);
		    fMatchFound = TRUE;
		}

		sprintf(buf2, "Reputation%d", i);
		if (!str_cmp(word, buf2))
		{
		    ch->pcdata->reputation[i] = fread_number(fp);
		    fMatchFound = TRUE;
		}
	    }*/

	    if (fMatchFound)
	    {
		fMatch = TRUE;
		break;
	    }

	    break;

	case 'S':
	    KEY("SavingThrow",	ch->saving_throw,	fread_number(fp));
	    KEY("Save",	ch->saving_throw,	fread_number(fp));
	    KEY("Scro",	ch->lines,		fread_number(fp));
	    KEY("Sex",		ch->sex,		fread_number(fp));
	    KEY("ShortDescr",	ch->short_descr,	fread_string(fp));
	    KEY("ShD",		ch->short_descr,	fread_string(fp));
	    KEY("Sec",         ch->pcdata->security,	fread_number(fp));
            KEY("Silv",        ch->silver,             fread_number(fp));
	    KEY("Subcla",	ch->pcdata->sub_class_current,		fread_number(fp));
	    KEY("SMc0",	ch->pcdata->sub_class_mage,	fread_number(fp));
	    KEY("SMc1",	ch->pcdata->sub_class_cleric,	fread_number(fp));
	    KEY("SMc2",	ch->pcdata->sub_class_thief,	fread_number(fp));
	    KEY("SMc3",	ch->pcdata->sub_class_warrior,	fread_number(fp))
	    KEY("SSMc0",	ch->pcdata->second_sub_class_mage,	fread_number(fp));
	    KEY("SSMc1",	ch->pcdata->second_sub_class_cleric,	fread_number(fp));
	    KEY("SSMc2",	ch->pcdata->second_sub_class_thief,	fread_number(fp));
	    KEY("SSMc3",	ch->pcdata->second_sub_class_warrior,	fread_number(fp));


	    if (!str_cmp(word, "Shifted"))
	    {
		char *temp;

		temp = fread_string(fp);

		if (!str_cmp(temp, "Werewolf"))
		    ch->shifted = SHIFTED_WEREWOLF;

		if (!str_cmp(temp, "Slayer"))
		    ch->shifted = SHIFTED_SLAYER;

		fMatch = TRUE;
		break;
	    }

	    if (!str_cmp(word, "Skill") || !str_cmp(word,"Sk"))
	    {
			int sn;
			int value;
			char *temp;
	        int prac;

			value = fread_number(fp);
			temp = fread_word(fp);
			if (ch->version < VERSION_PLAYER_002 && !str_cmp(temp,"wither"))
				sn = gsn_withering_cloud;
			else
				sn = skill_lookup(temp);
			if (sn <= 0)
			{
				sprintf(buf, "fread_char: unknown skill %s", temp);
				log_string(buf);
				if (value > 0)
				{
				prac = value / 4;
				ch->practice += prac;
				}
			}
			else
			{
				ch->pcdata->learned[sn] = value;
				if( skill_table[sn].spell_fun == spell_null)
					skill_entry_addskill(ch, sn, NULL, SKILLSRC_NORMAL, SKILL_AUTOMATIC);
				else
					skill_entry_addspell(ch, sn, NULL, SKILLSRC_NORMAL, SKILL_AUTOMATIC);
			}

			fMatch = TRUE;
			break;
	    }

	    if (!str_cmp(word, "SkillMod") || !str_cmp(word, "SkMod"))
	    {
		int sn;
		int value;
		char *temp;

		value = fread_number(fp);
		temp = fread_word(fp) ;
		if (ch->version < VERSION_PLAYER_002 && !str_cmp(temp,"wither"))
			sn = gsn_withering_cloud;
		else
			sn = skill_lookup(temp);
		if (sn > 0) ch->pcdata->mod_learned[sn] = value;

		fMatch = TRUE;
		break;
	    }


	    if (!str_cmp(word, "Song"))
	    {
			int song;
			char *temp;

			temp = fread_word(fp);
			song = music_lookup(temp);
			if (song < 0)
			{
				sprintf(buf, "fread_char: unknown song %s", temp);
				log_string(buf);
			}
			else
			{
				ch->pcdata->songs_learned[song] = TRUE;
				skill_entry_addsong(ch, song, NULL, SKILLSRC_NORMAL);
			}

			fMatch = TRUE;
			break;
	    }

/*
	    fMatchFound = FALSE;
	    for (i = 0; i < 3; i++)
            {
	        char buf2[MSL];

		sprintf(buf2, "ShipQuestPoints%d", i);
		if (!str_cmp(word, buf2))
		{
		    ch->pcdata->ship_quest_points[i] = fread_number(fp);
		    fMatchFound = TRUE;
		}
	    }
	    if (fMatchFound)
            {
		    fMatch = TRUE;
		    break;
	    }
*/
	    break;

	case 'T':
            KEY("TrueSex",     ch->pcdata->true_sex,  	fread_number(fp));
	    KEY("TSex",	ch->pcdata->true_sex,   fread_number(fp));
	    KEY("Trai",	ch->train,		fread_number(fp));
	    KEY("Trust",	ch->trust,		fread_number(fp));
	    KEY("Tru",		ch->trust,		fread_number(fp));
	    KEY("TLevl",	ch->tot_level,		fread_number(fp));

	    if (!str_prefix("Toxn", word))
	    {
		fMatch = TRUE;
		for (i = 0; i < MAX_TOXIN; i++)
		{
		    if (!str_cmp(word + 4, toxin_table[i].name))
			break;
		}

		if (i < MAX_TOXIN)
		    ch->toxin[i] = fread_number(fp);
		else
		{
		    bug("fread_char: bad toxin type", 0);
		    fread_number(fp);
		}
	    }

	    if (!str_cmp(word, "Title")  || !str_cmp(word, "Titl"))
	    {
		ch->pcdata->title = fread_string(fp);
    		if (ch->pcdata->title[0] != '.' && ch->pcdata->title[0] != ','
		&&  ch->pcdata->title[0] != '!' && ch->pcdata->title[0] != '?')
		{
		    sprintf(buf, " %s", ch->pcdata->title);
		    free_string(ch->pcdata->title);
		    ch->pcdata->title = str_dup(buf);
		}
		fMatch = TRUE;
		break;
	    }

	    break;

	case 'V':
	    KEY("Version",     ch->version,		fread_number (fp));
	    KEY("Vers",	ch->version,		fread_number (fp));

	    if (!str_cmp(word, "VisTo"))
	    {
		STRING_DATA *string;

		string = new_string_data();
		string->next = ch->pcdata->vis_to_people;
		ch->pcdata->vis_to_people = string;

		string->string = fread_string(fp);
		fMatch = TRUE;
		break;
	    }

	    KEY("Vuln", ch->vuln_flags,	fread_flag(fp));
	    KEY("VulnPerm", ch->vuln_flags_perm,	fread_flag(fp));

	    if (!str_cmp(word, "Vnum"))
	    {
		ch->pIndexData = get_mob_index(fread_number(fp));


		fMatch = TRUE;
		break;
	    }
/*
	    if (!str_cmp(word,"VnumOfShip"))
	    {
		ch->pcdata->vnum_of_boat_before_logoff		= fread_number(fp);
		fMatch = TRUE;
		break;
	    }
*/
            if (!str_cmp (word, "Vroom"))
            {
                plogf("save.c, fread_char(): Char is in a Vroom...");
                ch->at_wilds_x = fread_number(fp);
                ch->at_wilds_y = fread_number(fp);
                plogf("save.c, fread_char():     @ ( %d, %d )",
                      ch->at_wilds_x, ch->at_wilds_y);
                pArea = get_area_from_uid (fread_number(fp));
                ch->in_wilds = get_wilds_from_uid (pArea, fread_number(fp));
                /* Vizz - room may not exist yet */
                ch->in_room = NULL;

                fMatch = TRUE;
                break;
            }
	    break;

	case 'W':
 	    KEY("WarsWon",	ch->wars_won,	fread_number(fp));
	    KEY("Wimpy",	ch->wimpy,		fread_number(fp));
	    KEY("Wimp",	ch->wimpy,		fread_number(fp));
	    KEY("Wizn",	ch->wiznet,		fread_flag(fp));

	    break;
	}

	if (!fMatch)
	{
	    sprintf(buf,
	    "Fread_char: no match for ch %s on word %s.", ch->name, word);
	    bug(buf, 0);
	    fread_to_eol(fp);
	}
    }


	// Make sure questing data from old info is configured properly
    if( ch->quest != NULL )
    {
		if( ch->quest->questgiver < 0 )
		{
			// No questgiver info
			free_quest(ch->quest);
			ch->countdown = 0;
			ch->quest = NULL;
		}
		else if( ch->quest->questgiver > 0 )
		{
			if( ch->quest->questgiver_type < 0 ) ch->quest->questgiver_type = QUESTOR_MOB;
			if( ch->quest->questreceiver < 0 || ch->quest->questreceiver_type < 0 )
			{
				ch->quest->questreceiver = ch->quest->questgiver;
				ch->quest->questreceiver_type = QUESTOR_MOB;
			}
		}
	}
}


/*
 * Write an object and its contents.
 */
void fwrite_obj_new(CHAR_DATA *ch, OBJ_DATA *obj, FILE *fp, int iNest)
{
    EXTRA_DESCR_DATA *ed;
    AFFECT_DATA *paf;
    //char buf[MSL];

    /*
     * Slick recursion to write lists backwards,
     * so loading them will load in forwards order.
     */
    if (obj->next_content != NULL)
	fwrite_obj_new(ch, obj->next_content, fp, iNest);

    /*
     * Castrate storage characters.
     */
    /* AO Disabling. This doesn't really apply to us, but I'll keep the code here for other things that shouldn't save *
    if (ch != NULL && !obj->locker && ((ch->tot_level < obj->level - 25 &&
         obj->item_type != ITEM_CONTAINER &&
         obj->item_type != ITEM_WEAPON_CONTAINER &&
	 !IS_REMORT(ch))
    || (obj->level > 145 && !IS_IMMORTAL(ch))))
    {
	char buf2[MSL];

	sprintf(buf2, "%s", obj->short_descr);
	buf2[0] = UPPER(buf2[0]);

	sprintf(buf, "{R%s will not be saved!{x\n\r", buf2);
        send_to_char(buf, ch);
	return;
    } */

    fprintf(fp, "#O\n");

    fprintf(fp, "Vnum %ld\n", obj->pIndexData->vnum);
    fprintf(fp, "UId %ld\n", obj->id[0]);
    fprintf(fp, "UId2 %ld\n", obj->id[1]);
    fprintf(fp, "Version %d\n", VERSION_OBJECT);

    fprintf(fp, "Nest %d\n", iNest);

    /* these data are only used if they do not match the defaults */
    if (obj->name != obj->pIndexData->name)
    	fprintf(fp, "Name %s~\n",	obj->name		    );
    if (obj->short_descr != obj->pIndexData->short_descr)
        fprintf(fp, "ShD  %s~\n",	obj->short_descr	    );
    if (obj->description != obj->pIndexData->description)
        fprintf(fp, "Desc %s~\n",	obj->description	    );
    if (obj->full_description != obj->pIndexData->full_description)
	fprintf(fp, "FullD %s~\n",     fix_string(obj->full_description));
    if (obj->extra_flags != obj->extra_flags_perm)
        fprintf(fp, "ExtF %ld\n",	obj->extra_flags	    );
    if (obj->extra2_flags != obj->extra2_flags_perm)
        fprintf(fp, "Ext2F %ld\n",	obj->extra2_flags	    );
    if (obj->extra3_flags != obj->extra3_flags_perm)
        fprintf(fp, "Ext3F %ld\n",	obj->extra3_flags	    );
    if (obj->extra4_flags != obj->extra4_flags_perm)
        fprintf(fp, "Ext4F %ld\n",	obj->extra4_flags	    );
    if (obj->wear_flags != obj->pIndexData->wear_flags)
        fprintf(fp, "WeaF %d\n",	obj->wear_flags		    );
    if (obj->item_type != obj->pIndexData->item_type)
        fprintf(fp, "Ityp %d\n",	obj->item_type		    );
    if (obj->in_room != NULL)
    	fprintf(fp, "Room %ld\n",	obj->in_room->vnum	    );
    if (IS_SET(obj->extra2_flags, ITEM_ENCHANTED))
	fprintf(fp,"Enchanted_times %d\n", obj->num_enchanted);

    /*
    if (obj->weight != obj->pIndexData->weight)
        fprintf(fp, "Wt   %d\n",	obj->weight		    );
    */
    if (obj->condition != obj->pIndexData->condition)
	fprintf(fp, "Cond %d\n",	obj->condition		    );
    if (obj->times_fixed > 0)
        fprintf(fp, "Fixed %d\n",      obj->times_fixed	    );
    if (obj->owner != NULL)
	fprintf(fp, "Owner %s~\n",      obj->owner            );
    if (obj->old_name != NULL)
	fprintf(fp, "OldName %s~\n",   obj->old_name  );
    if (obj->old_short_descr != NULL)
	fprintf(fp, "OldShort %s~\n",   obj->old_short_descr  );
    if (obj->old_description != NULL)
        fprintf(fp, "OldDescr %s~\n",   obj->old_description  );
    if (obj->old_full_description != NULL)
	fprintf(fp, "OldFullDescr %s~\n", obj->old_full_description);
    if (obj->loaded_by != NULL)
        fprintf(fp, "LoadedBy %s~\n",   obj->loaded_by  );

    if (obj->fragility != obj->pIndexData->fragility)
	fprintf(fp, "Fragility %d\n", obj->fragility);
    if (obj->times_allowed_fixed != obj->pIndexData->times_allowed_fixed)
	fprintf(fp, "TimesAllowedFixed %d\n", obj->times_allowed_fixed);
    if (obj->locker == TRUE)
    	fprintf(fp, "Locker %d\n", obj->locker);

	// Permanent flags based
    fprintf(fp, "PermExtra %ld\n",	obj->extra_flags_perm );
    fprintf(fp, "PermExtra2 %ld\n",	obj->extra2_flags_perm );
	fprintf(fp, "PermExtra3 %ld\n",	obj->extra3_flags_perm );
	fprintf(fp, "PermExtra4 %ld\n",	obj->extra4_flags_perm );
	if( obj->item_type == ITEM_WEAPON )
		fprintf(fp, "PermWeapon %ld\n",	obj->weapon_flags_perm );

    /* variable data */
    fprintf(fp, "Wear %d\n",   obj->wear_loc               );
    fprintf(fp, "LastWear %d\n",   obj->last_wear_loc               );
    if (obj->level != obj->pIndexData->level)
        fprintf(fp, "Lev  %d\n",	obj->level		    );
    if (obj->timer != 0)
        fprintf(fp, "Time %d\n",	obj->timer	    );
    fprintf(fp, "Cost %ld\n",	obj->cost		    );

     if (obj->value[0] != obj->pIndexData->value[0]
     ||  obj->value[1] != obj->pIndexData->value[1]
     ||  obj->value[2] != obj->pIndexData->value[2]
     ||  obj->value[3] != obj->pIndexData->value[3]
     ||  obj->value[4] != obj->pIndexData->value[4]
     ||  obj->value[5] != obj->pIndexData->value[5]
     ||  obj->value[6] != obj->pIndexData->value[6]
     ||  obj->value[7] != obj->pIndexData->value[7])
    	fprintf(fp, "Val  %d %d %d %d %d %d %d %d\n",
	    obj->value[0], obj->value[1], obj->value[2], obj->value[3],
	    obj->value[4], obj->value[5], obj->value[6], obj->value[7] );

    if (obj->spells != NULL)
	save_spell(fp, obj->spells);

    // This is for spells on the objects.
    for (paf = obj->affected; paf != NULL; paf = paf->next)
    {
        if (paf->type < 0 || paf->type >= MAX_SKILL || paf->custom_name)
	    continue;

	if(paf->location >= APPLY_SKILL && paf->location < APPLY_SKILL_MAX) {
		if(!skill_table[paf->location - APPLY_SKILL].name) continue;
		fprintf(fp, "Affcg '%s' %3d %3d %3d %3d %3d %3d '%s' %10ld %10ld\n",
			skill_table[paf->type].name,
			paf->where,
			paf->group,
			paf->level,
			paf->duration,
			paf->modifier,
			APPLY_SKILL,
			skill_table[paf->location - APPLY_SKILL].name,
			paf->bitvector,
			paf->bitvector2
		);
	} else {
		fprintf(fp, "Affcg '%s' %3d %3d %3d %3d %3d %3d %10ld %10ld\n",
			skill_table[paf->type].name,
			paf->where,
			paf->group,
			paf->level,
			paf->duration,
			paf->modifier,
			paf->location,
			paf->bitvector,
			paf->bitvector2
		);
	}
    }

    // This is for spells on the objects.
    for (paf = obj->affected; paf != NULL; paf = paf->next)
    {
        if (!paf->custom_name) continue;

	if(paf->location >= APPLY_SKILL && paf->location < APPLY_SKILL_MAX) {
		if(!skill_table[paf->location - APPLY_SKILL].name) continue;
		fprintf(fp, "Affcgn '%s' %3d %3d %3d %3d %3d %3d '%s' %10ld %10ld\n",
			paf->custom_name,
			paf->where,
			paf->group,
			paf->level,
			paf->duration,
			paf->modifier,
			APPLY_SKILL,
			skill_table[paf->location - APPLY_SKILL].name,
			paf->bitvector,
			paf->bitvector2
		);
	} else {
		fprintf(fp, "Affcgn '%s' %3d %3d %3d %3d %3d %3d %10ld %10ld\n",
			paf->custom_name,
			paf->where,
			paf->group,
			paf->level,
			paf->duration,
			paf->modifier,
			paf->location,
			paf->bitvector,
			paf->bitvector2
		);
	}
    }

    // for random affect eq
    for (paf = obj->affected; paf != NULL; paf = paf->next)
    {
	/* filter out "none" and "unknown" affects, as well as custom named affects */
	if (paf->type != -1 || paf->custom_name != NULL
        || ((paf->location < APPLY_SKILL || paf->location >= APPLY_SKILL_MAX) && !str_cmp(flag_string(apply_flags, paf->location), "none")))
	    continue;

	if(paf->location >= APPLY_SKILL && paf->location < APPLY_SKILL_MAX) {
		if(!skill_table[paf->location - APPLY_SKILL].name) continue;
		fprintf(fp, "Affrg %3d %3d %3d %3d %3d %3d '%s' %10ld %10ld\n",
			paf->where,
			paf->group,
			paf->level,
			paf->duration,
			paf->modifier,
			APPLY_SKILL,
			skill_table[paf->location - APPLY_SKILL].name,
			paf->bitvector,
			paf->bitvector2
		);
	} else {
		fprintf(fp, "Affrg %3d %3d %3d %3d %3d %3d %10ld %10ld\n",
			paf->where,
			paf->group,
			paf->level,
			paf->duration,
			paf->modifier,
			paf->location,
			paf->bitvector,
			paf->bitvector2
		);
	}
    }

    // for catalysts
    for (paf = obj->catalyst; paf != NULL; paf = paf->next)
    {
		if( IS_NULLSTR(paf->custom_name) )
		{
			fprintf(fp, "%s '%s' %3d %3d %3d\n",
				((paf->where == TO_CATALYST_ACTIVE) ? "CataA" : "Cata"),
				flag_string( catalyst_types, paf->type ),
				paf->level,
				paf->modifier,
				paf->duration);
		}
		else
		{
			fprintf(fp, "%s '%s' %3d %3d %3d %s\n",
				((paf->where == TO_CATALYST_ACTIVE) ? "CataNA" : "CataN"),
				flag_string( catalyst_types, paf->type ),
				paf->level,
				paf->modifier,
				paf->duration,
				paf->custom_name
				);
		}
    }

    for (ed = obj->extra_descr; ed != NULL; ed = ed->next)
    {
	fprintf(fp, "ExDe %s~ %s~\n",
	    ed->keyword, ed->description);
    }

    if(obj->progs && obj->progs->vars) {
		pVARIABLE var;

		for(var = obj->progs->vars; var; var = var->next)
			if(var->save)
				variable_fwrite(var, fp);
    }

    if( !IS_NULLSTR(obj->owner_name) ) {
    	fprintf(fp, "OwnerName %s~\n", obj->owner_name);
	}

    if( !IS_NULLSTR(obj->owner_short) ) {
    	fprintf(fp, "OwnerShort %s~\n", obj->owner_short);
	}

	if(obj->tokens != NULL) {
		TOKEN_DATA *token;
		for(token = obj->tokens; token; token = token->next)
			fwrite_token(token, fp);
	}


    fprintf(fp, "End\n\n");

    if (obj->contains != NULL)
	fwrite_obj_new(ch, obj->contains, fp, iNest + 1);
}


// Read an object and its contents
OBJ_DATA *fread_obj_new(FILE *fp)
{
	OBJ_DATA *obj;
	char *word;
	int iNest, vtype;
	bool fMatch;
	bool fVnum;
	bool first;
	bool make_new;
	char buf[MSL];
	//ROOM_INDEX_DATA *room = NULL;

	fVnum = FALSE;
	obj = NULL;
	first = TRUE;  /* used to counter fp offset */
	make_new = FALSE;

	word   = feof(fp) ? "End" : fread_word(fp);
	if (!str_cmp(word,"Vnum"))
	{
		long vnum;
		first = FALSE;  /* fp will be in right place */

		vnum = fread_number(fp);
		if ( get_obj_index(vnum)  == NULL)
			bug("Fread_obj: bad vnum %ld.", vnum);
		else
			obj = create_object_noid(get_obj_index(vnum),-1, FALSE);
	}

	if (obj == NULL)  /* either not found or old style */
	{
		obj = new_obj();
		obj->name		= str_dup("");
		obj->short_descr	= str_dup("");
		obj->description	= str_dup("");
	}

	obj->version	= VERSION_OBJECT_000;
	obj->id[0] = obj->id[1] = 0;

	fVnum		= TRUE;
	iNest		= 0;

	for (; ;)
	{
		if (first)
			first = FALSE;
		else if(feof(fp))
		{
			bug("EOF encountered reading object from pfile", 0);
			word = "End";
		} else
			word   = fread_word(fp);
		fMatch = FALSE;

//		sprintf(buf, "Fread_obj_new: word = '%s'", word);
//		bug(buf, 0);

		switch (UPPER(word[0]))
		{
		case '*':
			fMatch = TRUE;
			fread_to_eol(fp);
			break;
		case '#':
			if (!str_cmp(word, "#TOKEN"))
			{
				TOKEN_DATA *token = fread_token(fp);
				token_to_obj(token, obj);
				fMatch		= TRUE;
				break;
			}
			break;

		case 'A':
			if (!str_cmp(word,"AffD"))
			{
				AFFECT_DATA *paf;
				int sn;

				paf = new_affect();

				sn = skill_lookup(fread_word(fp));
				if (sn < 0)
					bug("Fread_obj: unknown skill.",0);
				else
					paf->type = sn;

				paf->level	= fread_number(fp);
				paf->duration	= fread_number(fp);
				paf->modifier	= fread_number(fp);
				paf->location	= fread_number(fp);
				paf->bitvector	= fread_number(fp);
				paf->next	= obj->affected;
				obj->affected	= paf;
				fMatch		= TRUE;
				break;
			}

			if (!str_cmp(word,"Affr"))
			{
				AFFECT_DATA *paf;

				paf = new_affect();

				paf->type = -1;

				paf->where	= fread_number(fp);
				paf->level      = fread_number(fp);
				paf->duration   = fread_number(fp);
				paf->modifier   = fread_number(fp);
				paf->location   = fread_number(fp);
				paf->bitvector  = fread_number(fp);
				paf->next       = obj->affected;
				obj->affected   = paf;
				fMatch          = TRUE;
				break;
			}

			if (!str_cmp(word,"Affrg"))
			{
				AFFECT_DATA *paf;

				paf = new_affect();

				paf->type = -1;
				paf->where	= fread_number(fp);
				paf->group	= fread_number(fp);
				paf->level      = fread_number(fp);
				paf->duration   = fread_number(fp);
				paf->modifier   = fread_number(fp);
				paf->location   = fread_number(fp);
				if(paf->location == APPLY_SKILL) {
					int sn = skill_lookup(fread_word(fp));
					if(sn < 0) {
						paf->location = APPLY_NONE;
						paf->modifier = 0;
					} else
						paf->location += sn;
				}
				paf->bitvector  = fread_number(fp);
				if( obj->version >= VERSION_OBJECT_003 )
					paf->bitvector2 = fread_number(fp);

				paf->next       = obj->affected;
				obj->affected   = paf;
				fMatch          = TRUE;
				break;
			}

			if (!str_cmp(word,"Affc"))
			{
				AFFECT_DATA *paf;
				int sn;

				paf = new_affect();

				sn = skill_lookup(fread_word(fp));
				if (sn < 0)
					bug("Fread_obj: unknown skill.",0);
				else
					paf->type = sn;

				paf->where	= fread_number(fp);
				paf->group	= AFFGROUP_MAGICAL;
				paf->level      = fread_number(fp);
				paf->duration   = fread_number(fp);
				paf->modifier   = fread_number(fp);
				paf->location   = fread_number(fp);
				paf->bitvector  = fread_number(fp);
				paf->next       = obj->affected;
				obj->affected   = paf;
				fMatch          = TRUE;
				break;
			}

			if (!str_cmp(word,"Affcg"))
			{
				AFFECT_DATA *paf;
				int sn;

				paf = new_affect();

				sn = skill_lookup(fread_word(fp));
				if (sn < 0)
					bug("Fread_obj: unknown skill.",0);
				else
					paf->type = sn;

				paf->where	= fread_number(fp);
				paf->group	= fread_number(fp);
				paf->level      = fread_number(fp);
				paf->duration   = fread_number(fp);
				paf->modifier   = fread_number(fp);
				paf->location   = fread_number(fp);
				if(paf->location == APPLY_SKILL) {
					int sn = skill_lookup(fread_word(fp));
					if(sn < 0) {
						paf->location = APPLY_NONE;
						paf->modifier = 0;
					} else
						paf->location += sn;
				}
				paf->bitvector  = fread_number(fp);
				if( obj->version >= VERSION_OBJECT_003 )
					paf->bitvector2 = fread_number(fp);
				paf->next       = obj->affected;
				obj->affected   = paf;
				fMatch          = TRUE;
				break;
			}

			if (!str_cmp(word, "Affcn"))
			{
				AFFECT_DATA *paf;
				char *name;

				paf = new_affect();

				name = create_affect_cname(fread_word(fp));
				if (!name) {
					log_string("fread_char: could not create affect name.");
					free_affect(paf);
				} else {
					paf->custom_name = name;

					paf->type = -1;
					paf->where  = fread_number(fp);
					paf->level      = fread_number(fp);
					paf->duration   = fread_number(fp);
					paf->modifier   = fread_number(fp);
					paf->location   = fread_number(fp);
					paf->bitvector  = fread_number(fp);
					paf->next       = obj->affected;
					obj->affected    = paf;
				}
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "Affcgn"))
			{
				AFFECT_DATA *paf;
				char *name;

				paf = new_affect();

				name = create_affect_cname(fread_word(fp));
				if (!name) {
					log_string("fread_char: could not create affect name.");
					free_affect(paf);
				} else {
					paf->custom_name = name;

					paf->type = -1;
					paf->where  = fread_number(fp);
					paf->group	= fread_number(fp);
					paf->level      = fread_number(fp);
					paf->duration   = fread_number(fp);
					paf->modifier   = fread_number(fp);
					paf->location   = fread_number(fp);
					if(paf->location == APPLY_SKILL) {
						int sn = skill_lookup(fread_word(fp));
						if(sn < 0) {
							paf->location = APPLY_NONE;
							paf->modifier = 0;
						} else
							paf->location += sn;
					}
					paf->bitvector  = fread_number(fp);
					if( obj->version >= VERSION_OBJECT_003 )
						paf->bitvector2 = fread_number(fp);
					paf->next       = obj->affected;
					obj->affected    = paf;
				}
				fMatch = TRUE;
				break;
			}
			break;

		case 'C':
			if (!str_cmp(word, "Cata"))
			{
				AFFECT_DATA *paf;

				paf = new_affect();

				paf->type = flag_value(catalyst_types,fread_word(fp));
				if(paf->type == NO_FLAG) {
					log_string("fread_char: invalid catalyst type.");
					free_affect(paf);
				} else {
					paf->custom_name = NULL;
					paf->where		= TO_CATALYST_DORMANT;
					paf->level       = fread_number(fp);
					paf->modifier    = fread_number(fp);
					paf->duration    = fread_number(fp);
					paf->next        = obj->catalyst;
					obj->catalyst    = paf;
				}
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "CataA"))
			{
				AFFECT_DATA *paf;

				paf = new_affect();

				paf->type = flag_value(catalyst_types,fread_word(fp));
				if(paf->type == NO_FLAG) {
					log_string("fread_char: invalid catalyst type.");
					free_affect(paf);
				} else {
					paf->custom_name = NULL;
					paf->where		= TO_CATALYST_ACTIVE;
					paf->level       = fread_number(fp);
					paf->modifier    = fread_number(fp);
					paf->duration    = fread_number(fp);
					paf->next        = obj->catalyst;
					obj->catalyst    = paf;
				}
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "CataN"))
			{
				AFFECT_DATA *paf;

				paf = new_affect();

				paf->type = flag_value(catalyst_types,fread_word(fp));
				if(paf->type == NO_FLAG) {
					log_string("fread_char: invalid catalyst type.");
					free_affect(paf);
				} else {
					paf->where		= TO_CATALYST_DORMANT;
					paf->level       = fread_number(fp);
					paf->modifier    = fread_number(fp);
					paf->duration    = fread_number(fp);
					paf->custom_name = fread_string_eol(fp);
					paf->next        = obj->catalyst;
					obj->catalyst    = paf;
				}
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "CataNA"))
			{
				AFFECT_DATA *paf;

				paf = new_affect();

				paf->type = flag_value(catalyst_types,fread_word(fp));
				if(paf->type == NO_FLAG) {
					log_string("fread_char: invalid catalyst type.");
					free_affect(paf);
				} else {
					paf->where		= TO_CATALYST_ACTIVE;
					paf->level       = fread_number(fp);
					paf->modifier    = fread_number(fp);
					paf->duration    = fread_number(fp);
					paf->custom_name = fread_string_eol(fp);
					paf->next        = obj->catalyst;
					obj->catalyst    = paf;
				}
				fMatch = TRUE;
				break;
			}
			KEY("Cond",	obj->condition,		fread_number(fp));
			KEY("Cost",	obj->cost,		fread_number(fp));
			break;

		case 'D':
			KEY("Description",	obj->description,	fread_string(fp));
			KEY("Desc",	obj->description,	fread_string(fp));
			break;

		case 'E':
			KEY("Enchanted_times", obj->num_enchanted, fread_number(fp));

			if (!str_cmp(word, "ExtraFlags") || !str_cmp(word, "ExtF"))
			{
				obj->extra_flags = fread_number(fp);
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "Extra2Flags") || !str_cmp(word, "Ext2F"))
			{
				obj->extra2_flags = fread_number(fp);
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "Extra3Flags") || !str_cmp(word, "Ext3F"))
			{
				obj->extra3_flags = fread_number(fp);
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "Extra4Flags") || !str_cmp(word, "Ext4F"))
			{
				obj->extra4_flags = fread_number(fp);
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "ExtraDescr") || !str_cmp(word,"ExDe"))
			{
				EXTRA_DESCR_DATA *ed;

				ed = new_extra_descr();

				ed->keyword		= fread_string(fp);
				ed->description		= fread_string(fp);
				ed->next		= obj->extra_descr;
				obj->extra_descr	= ed;
				fMatch = TRUE;
			}

			if (!str_cmp(word, "End"))
			{
				if ((fVnum && obj->pIndexData == NULL))
				{
					bug("Fread_obj: incomplete object.", 0);
					free_obj(obj);
					return NULL;
				}
				else
				{
					if (!fVnum)
					{
						free_obj(obj);
						obj = create_object(get_obj_index(OBJ_VNUM_DUMMY), 0 , FALSE);
					}

					if (make_new)
					{
						int wear;

						wear = obj->wear_loc;
						extract_obj(obj);

						obj = create_object(obj->pIndexData,0, FALSE);

						obj->wear_loc = wear;
					}

					get_obj_id(obj);

					obj->times_allowed_fixed = obj->pIndexData->times_allowed_fixed;
					fix_object(obj);
					return obj;
				}
			}
			break;

		case 'F':
			KEY("Fixed",	obj->times_fixed,	fread_number(fp));
			KEY("Fragility",	obj->fragility,		fread_number(fp));
			KEYS("FullD",	obj->full_description,  fread_string(fp));
			break;

		case 'I':
			// Don't save item type as we're changing this all the time.
			if (!str_cmp(word, "ItemType"))
			{
				obj->item_type = fread_number(fp);
				obj->item_type = obj->pIndexData->item_type;
				fMatch = TRUE;
			}
			break;

		case 'K':
			if (!str_cmp(word, "Key"))
			{
				OBJ_DATA *key;
				OBJ_INDEX_DATA *pIndexData;
				long vnum;

				vnum = fread_number(fp);
				if ((pIndexData = get_obj_index(vnum)) != NULL)
				{
					key = create_object(pIndexData, pIndexData->level, FALSE);
					obj_to_obj(key, obj);
				}

				fMatch = TRUE;
			}
			break;

		case 'L':
			KEY("LastWear",	obj->last_wear_loc,	fread_number(fp));
			KEY("Locker",	obj->locker,		fread_number(fp));

			if (!str_cmp(word, "Level") || !str_cmp(word, "Lev"))
			{
				obj->level = fread_number(fp);

				if (obj->pIndexData != NULL && obj->pIndexData->vnum == 100035)
				{
					int armour;
					int armour_exotic;

					armour=(int) calc_obj_armour(obj->level, obj->value[4]);
					armour_exotic=(int) armour * .90;

					obj->value[0] = armour;
					obj->value[1] = armour;
					obj->value[2] = armour;
					obj->value[3] = armour_exotic;
				}

				fMatch = TRUE;
			}

			KEY("LoadedBy",	obj->loaded_by,		fread_string(fp));
			break;

		case 'N':
			KEY("Name",	obj->name,		fread_string(fp));

			if (!str_cmp(word, "Nest"))
			{
				iNest = fread_number(fp);
				if (iNest < 0 || iNest >= MAX_NEST)
				{
					bug("Fread_obj: bad nest %d.", iNest);
				}
				else
				{
					obj->nest = iNest;
				}
				fMatch = TRUE;
			}
			break;

		case 'O':
			KEY("Owner",	obj->owner,	       fread_string(fp));
			KEY("OwnerName",	obj->owner_name,	       fread_string(fp));
			KEY("OwnerShort",	obj->owner_short,	       fread_string(fp));
			KEY("OldName",	obj->old_name,  fread_string(fp));
			KEY("OldShort",	obj->old_short_descr,  fread_string(fp));
			KEY("OldDescr",	obj->old_description,  fread_string(fp));
			KEY("OldFullDescr", obj->old_full_description, fread_string(fp));

			break;

		case 'R':
			if (!str_cmp(word, "Room"))
			{
				ROOM_INDEX_DATA *room;

				room = get_room_index(fread_number(fp));
				obj->in_room = room;
				fMatch = TRUE;
			}
			break;

		case 'P':
			KEY("PermExtra",		obj->extra_flags_perm,	fread_number(fp));
			KEY("PermExtra2",		obj->extra2_flags_perm,	fread_number(fp));
			KEY("PermExtra3",		obj->extra3_flags_perm,	fread_number(fp));
			KEY("PermExtra4",		obj->extra4_flags_perm,	fread_number(fp));
			KEY("PermWeapon",		obj->weapon_flags_perm,	fread_number(fp));
			break;

		case 'S':
			KEY("ShortDescr",	obj->short_descr,	fread_string(fp));
			KEY("ShD",		obj->short_descr,	fread_string(fp));

			if (!str_cmp(word, "SpellNew"))
			{
				int sn;
				SPELL_DATA *spell;

				fMatch = TRUE;
				if ((sn = skill_lookup(fread_string(fp))) > -1)
				{
					spell = new_spell();
					spell->sn = sn;
					spell->level = fread_number(fp);
					spell->repop = fread_number(fp);

					spell->next = obj->spells;
					obj->spells = spell;
				}
				else
				{
					sprintf(buf, "Bad spell name for %s (%ld).", obj->short_descr, obj->pIndexData->vnum);
					bug(buf,0);
				}
			}

			if (!str_cmp(word, "Spell"))
			{
				int iValue;
				int sn;

				iValue = fread_number(fp);
				sn = skill_lookup(fread_word(fp));
				if (iValue < 0 || iValue > 7)
					bug("Fread_obj: bad iValue %d.", iValue);
				else if (sn < 0)
					bug("Fread_obj: unknown skill.", 0);
				else
				{
					if (obj->item_type == ITEM_WEAPON || obj->item_type == ITEM_ARMOUR)
					{
						if (iValue == 1)
							obj->value[6] = sn;
						else
							obj->value[7] = sn;
					}
					else
						obj->value[iValue] = sn;
				}
				fMatch = TRUE;
				break;
			}

			break;

		case 'T':
			KEY("TimesAllowedFixed", obj->times_allowed_fixed, fread_number(fp));
			KEY("Timer",	obj->timer,		fread_number(fp));
			KEY("Time",	obj->timer,		fread_number(fp));
			break;
		case 'U':
			KEY("UId",		obj->id[0],		fread_number(fp));
			KEY("UId2",		obj->id[1],		fread_number(fp));
			break;

		case 'V':
			KEY("Version", obj->version, fread_number(fp));

			if (!str_cmp(word, "Values") || !str_cmp(word,"Vals") || !str_cmp(word,"Val"))
			{
				fMatch		= TRUE;

				obj->value[0]	= fread_number(fp);
				obj->value[1]	= fread_number(fp);
				obj->value[2]	= fread_number(fp);
				obj->value[3]	= fread_number(fp);
				obj->value[4]	= fread_number(fp);
				if (obj->version > 0)
				{
					obj->value[5] = fread_number(fp);
					obj->value[6] = fread_number(fp);
					obj->value[7] = fread_number(fp);
				}

				if (obj->item_type == ITEM_WEAPON && obj->value[0] == 0)
					obj->value[0] = obj->pIndexData->value[0];

				break;
			}

			if ((!str_cmp(word, "Val")) && obj->item_type != ITEM_WEAPON && obj->item_type != ITEM_ARMOUR)
			{
				obj->value[0] 	= fread_number(fp);
				obj->value[1]	= fread_number(fp);
				obj->value[2] 	= fread_number(fp);
				obj->value[3]	= fread_number(fp);
				obj->value[4]	= fread_number(fp);
				obj->value[5]	= fread_number(fp);
				fMatch = TRUE;
				break;
			}

			if( (vtype = variable_fread_type(word)) != VAR_UNKNOWN ) {
				variable_fread(&obj->progs->vars, vtype, fp);
				fMatch = TRUE;
			}

			if (!str_cmp(word, "Vnum"))
			{
				long vnum;

				vnum = fread_number(fp);
				if ((obj->pIndexData = get_obj_index(vnum)) == NULL)
					bug("Fread_obj: bad vnum %ld.", vnum);
				else
					fVnum = TRUE;

				fMatch = TRUE;
				break;
			}
			break;

		case 'W':
			KEY("WearFlags",	obj->wear_flags,	fread_number(fp));
			KEY("WeaF",	obj->wear_flags,	fread_number(fp));
			KEY("WearLoc",	obj->wear_loc,		fread_number(fp));
			KEY("Wear",	obj->wear_loc,		fread_number(fp));
			KEY("Weight",	obj->weight,		fread_number(fp));
			break;

		}

		if (!fMatch)
		{
			//char buf[MAX_STRING_LENGTH];
			//sprintf(buf, "fread_obj: unknown obj flag %s", word);
			//bug(buf, 0);
			fread_to_eol(fp);
		}
	}
}


// Write the permanent objects - the ones which save over reboots, etc.
void write_permanent_objs()
{
    FILE *fp;
    CHURCH_DATA *church;

    if ((fp = fopen(PERM_OBJS_FILE, "w")) == NULL)
	bug("perm_objs_new.dat: Couldn't open file.",0);
	else
    {
    	wiznet("writing permanent objects...", NULL, NULL, WIZ_TESTING, 0, 0);

	// save relics
	if (pneuma_relic != NULL && !is_in_treasure_room(pneuma_relic))
	    fwrite_obj_new(NULL, pneuma_relic, fp, 0);

	if (damage_relic != NULL && !is_in_treasure_room(damage_relic))
	    fwrite_obj_new(NULL, damage_relic, fp, 0);

	if (xp_relic != NULL && !is_in_treasure_room(xp_relic))
	    fwrite_obj_new(NULL, xp_relic, fp, 0);

	if (mana_regen_relic != NULL && !is_in_treasure_room(mana_regen_relic))
	    fwrite_obj_new(NULL, mana_regen_relic, fp, 0);

	if (hp_regen_relic != NULL && !is_in_treasure_room(hp_regen_relic))
	    fwrite_obj_new(NULL, hp_regen_relic, fp, 0);

	// save church treasure rooms
	for (church = church_list; church != NULL; church = church->next)
	{
		CHURCH_TREASURE_ROOM *treasure;
		ITERATOR it;

		iterator_start(&it, church->treasure_rooms);
		while( (treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it))) {
			if( treasure->room->contents != NULL )
				fwrite_obj_new(NULL, treasure->room->contents, fp, 0);
		}
		iterator_stop(&it);
	}

	fprintf(fp, "#END\n");

	fclose(fp);
	}
}


void read_permanent_objs()
{
    FILE *fp;
    OBJ_DATA *obj;
    OBJ_DATA *objNestList[MAX_NEST];
    char *word;

    log_string("Loading permanent objs");
    if ((fp = fopen(PERM_OBJS_FILE, "r")) == NULL)
	bug("perm_objs_new.dat: Couldn't open file.",0);
    else
    {
    	for (;;)
		{
			word = fread_word(fp);
			if (!str_cmp(word, "#O"))
			{
				obj = fread_obj_new(fp);
			objNestList[obj->nest] = obj;

			if (obj->in_room != NULL)
			{
				if (obj->nest > 0)
				{
				obj->in_room = NULL;
				obj_to_obj(obj, objNestList[obj->nest - 1]);
				}
				else
				{
				ROOM_INDEX_DATA *to_room = get_room_index(obj->in_room->vnum);
				obj->in_room = NULL;
				obj_to_room(obj, to_room == NULL ? get_room_index(1) : to_room);
				}
			}
			}
			else if (!str_cmp(word, "#END"))
				break;
			else {
			bug("perm_objs_new.dat: bad format", 0);
			break;
			}
		}

		fclose(fp);
	}
}


/* This is used for updating objects when we want them to be.
 * use this function because in fread_obj, the pIndexData might be null,
 * as is the case if an area has been removed. */
bool update_object(OBJ_DATA *obj)
{
     if (obj->pIndexData == NULL)
         return FALSE;

     if (obj->pIndexData->update == TRUE)
	 return TRUE;

     //if (obj->pIndexData->item_type == ITEM_WEAPON)
       //  return TRUE;

     return FALSE;
}


// Fix an object. Clean up any mess we have made before.
void fix_object(OBJ_DATA *obj)
{
    char buf[MSL];
    int i, sn, level;
    int af_level = 0;
    int af_hr_mod = 0;
    int af_dr_mod = 0;
    AFFECT_DATA *af, *af_next;
    SPELL_DATA *spell, *spell_new;

    if (obj == NULL) {
		bug("fix_object: obj was null.", 0);
		return;
    }

    if (obj->pIndexData == NULL) {
		bug("fix_object: pIndexData was null.", 0);
		return;
    }

    //////////////////////////////////////////////////////////////////////
    // LEGACY UPDATES

    if (obj->version == 0) {
		bool fEnchanted = FALSE;

		if (IS_SET(obj->extra2_flags, ITEM_ENCHANTED))
			fEnchanted = TRUE;

		obj->extra2_flags = obj->pIndexData->extra2_flags | obj->extra2_flags;
		obj->extra3_flags = obj->pIndexData->extra3_flags | obj->extra3_flags;
		obj->extra4_flags = obj->pIndexData->extra4_flags | obj->extra4_flags;

		if (fEnchanted)
			SET_BIT(obj->extra2_flags, ITEM_ENCHANTED);

		if (is_quest_item(obj))
			obj->cost = obj->pIndexData->cost;
    }

    if (obj->version == 1)
    {
		// remove dup affects - don't do for now
		// cleanup_affects(obj);

		// Fix skulls
		if (obj->pIndexData->vnum == OBJ_VNUM_SKULL || obj->pIndexData->vnum == OBJ_VNUM_GOLD_SKULL) {
			int i;
			char buf[MSL];

			if (obj->owner == NULL)
				obj->owner = str_dup("Nobody");

			// Fix name
			if (str_infix(obj->owner, obj->name))
			{
				sprintf(buf, "skull %s", obj->owner);

				for (i = 0; buf[i] != '\0'; i++)
				{
				buf[i] = LOWER(buf[i]);
				}

				free_string(obj->name);
				obj->name = str_dup(buf);
			}

			// Fix full desc field
			free_string(obj->full_description);
			sprintf(buf, obj->pIndexData->full_description, obj->owner);
			obj->full_description = str_dup(buf);
		}

		/* Syn - this is also in fread_obj so let's not do it twice unless there is some reason
		   I am not seeing.
		if (update_object(obj))
		{
		i = 0;
		while (i <= 8)
		{
			obj->value[i] = obj->pIndexData->value[i];
			i++;
		}

		obj->wear_flags = obj->pIndexData->wear_flags;
		obj->extra_flags = obj->extra_flags | obj->pIndexData->extra_flags;
		obj->extra2_flags = obj->extra2_flags | obj->pIndexData->extra2_flags;
		obj->extra3_flags = obj->extra3_flags | obj->pIndexData->extra3_flags;
		obj->extra4_flags = obj->extra4_flags | obj->pIndexData->extra4_flags;
		free_string(obj->material);
		obj->material = str_dup(obj->pIndexData->material);
		}
		 */
		// Fix dual enchant affects
		if (IS_SET(obj->extra2_flags, ITEM_ENCHANTED))
		{
			for (af = obj->affected; af != NULL; af = af_next)
			{
				af_next = af->next;

				if (af->type == gsn_enchant_weapon)
				{
					af_level = af->level;

					if (af->location == APPLY_DAMROLL)
						af_dr_mod += af->modifier;
					else
						af_hr_mod += af->modifier;

					affect_remove_obj(obj, af);
				}
			}

			if (af_level > 0)
			{
				// HR mods
				af = new_affect();
				af->group = AFFGROUP_ENCHANT;
				af->level = af_level;
				af->duration = -1;
				af->location = APPLY_HITROLL;
				af->modifier = af_hr_mod;
				af->type = gsn_enchant_weapon;
				affect_to_obj(obj, af);

				// DR mods
				af = new_affect();
				af->group = AFFGROUP_ENCHANT;
				af->level = af_level;
				af->duration = -1;
				af->location = APPLY_DAMROLL;
				af->modifier = af_dr_mod;
				af->type = gsn_enchant_weapon;
				affect_to_obj(obj, af);
			}
		}

		// Update spells to be done the correct way.
		if (obj->spells == NULL)
		switch (obj->item_type)
		{
		case ITEM_PILL:
		case ITEM_POTION:
		case ITEM_SCROLL:
				if (obj->value[0] > 0)
			level = obj->value[0];
				else
			level = obj->level;

				for (i = 1; i < 4; i++)
			{
			if ((sn = obj->value[i]) > 0 && sn < MAX_SKILL
			&&  skill_table[sn].spell_fun != spell_null)
			{
				spell_new = new_spell();
				spell_new->sn = sn;
				spell_new->level = level;

				spell_new->next = obj->spells;
				obj->spells = spell_new;
			}
			}

			break;
		case ITEM_WAND:
		case ITEM_STAFF:
				if (obj->value[0] > 0)
			level = obj->value[0];
			else
			level = obj->level;

			if ((sn = obj->value[3]) > 0 && sn < MAX_SKILL
			&&   skill_table[sn].spell_fun != spell_null)
			{
			spell_new = new_spell();
			spell_new->sn = sn;
			spell_new->level = level;

			spell_new->next = obj->spells;
			obj->spells = spell_new;
			}

			break;

		default:
			for (spell = obj->pIndexData->spells; spell != NULL; spell = spell->next)
			{
			spell_new = new_spell();
			*spell_new = *spell;

			spell_new->next = obj->spells;
			obj->spells = spell_new;
			}
		}

		obj->version = 2;
	}

    // Fix magic items that haven't been scribed/brewed
    if (obj->version == 2) {
		switch (obj->item_type)
		{
			case ITEM_PILL:
			case ITEM_POTION:
			case ITEM_SCROLL:
			case ITEM_WAND:
			case ITEM_STAFF:
			if (obj->pIndexData->vnum != ITEM_SCROLL
			&&  obj->pIndexData->vnum != ITEM_POTION
			&&  obj->spells == NULL) {
				for (spell = obj->pIndexData->spells; spell != NULL; spell = spell->next)
				{
				spell_new = new_spell();
				*spell_new = *spell;

				spell_new->next = obj->spells;
				obj->spells = spell_new;
				}
			}

			break;
			default:
			break;
		}
		obj->version = 3;
    }

	if( obj->version == 3) {
		if (IS_SET(obj->extra_flags, ITEM_HIDDEN)) {
			REMOVE_BIT(obj->extra_flags, ITEM_HIDDEN);
			sprintf(buf, "fix_object: removing hidden flag from inventory object %s(%ld)",
				obj->short_descr, obj->pIndexData->vnum);
			log_string(buf);
		}
		obj->version = 4;
	}

	/////////////////////////////////////////////////
	// NEW UPDATES

	if( obj->version < VERSION_OBJECT_002)
	{
		// Initializes objects to use the perm values for flags manipulated by affects

		AFFECT_DATA *paf;
		bool is_enchanted = FALSE;

		if (IS_SET(obj->extra2_flags, ITEM_ENCHANTED))
			is_enchanted = TRUE;


		obj->extra_flags = obj->extra_flags_perm = obj->pIndexData->extra_flags;
		obj->extra2_flags = obj->extra2_flags_perm = obj->pIndexData->extra2_flags;
		obj->extra3_flags = obj->extra3_flags_perm = obj->pIndexData->extra3_flags;
		obj->extra4_flags = obj->extra4_flags_perm = obj->pIndexData->extra4_flags;

		if( obj->item_type == ITEM_WEAPON )
		{
			obj->value[4] = obj->weapon_flags_perm = obj->pIndexData->value[4];
		}

		for(paf = obj->affected; paf; paf = paf->next )
		{
			if (paf->bitvector)
			{
				switch (paf->where)
				{
				case TO_OBJECT:
					SET_BIT(obj->extra_flags,paf->bitvector);
					break;
				case TO_OBJECT2:
					SET_BIT(obj->extra2_flags,paf->bitvector);
					break;
				case TO_OBJECT3:
					SET_BIT(obj->extra3_flags,paf->bitvector);
					break;
				case TO_OBJECT4:
					SET_BIT(obj->extra4_flags,paf->bitvector);
					break;
				case TO_WEAPON:
					if (obj->item_type == ITEM_WEAPON)
						SET_BIT(obj->value[4],paf->bitvector);
				break;
				}
			}
		}

		if( is_enchanted )
			SET_BIT(obj->extra2_flags, ITEM_ENCHANTED);

		obj->version = VERSION_OBJECT_002;
	}

	if( obj->version < VERSION_OBJECT_003 ) {


		obj->version = VERSION_OBJECT_003;
	}

}


/* Cleanup affects on an obj. This currently just removes dual affects. */
void cleanup_affects(OBJ_DATA *obj)
{
    AFFECT_DATA *af;
    AFFECT_DATA *af_next;
    char buf[MSL];
    int count;
    int apply_type;

    if (obj == NULL)
    {
	bug("cleanup_affects: obj was null!", 0);
	return;
    }

    for(apply_type = 1; apply_type < APPLY_MAX; apply_type ++)
    {
	count = 0;
	for (af = obj->affected; af != NULL; af = af_next)
	{
	    af_next = af->next;

	    if (apply_type == af->location)
		count++;

	    if (count > 1 && af->location == apply_type)
	    {
		affect_remove_obj(obj, af);
		sprintf(buf, "cleanup_affects: obj %s (%ld)",
		    obj->short_descr, obj->pIndexData->vnum);
		bug(buf, 1);
	    }
	}

    }
}


#define HAS_ALL_BITS(a, b) (((a) & (b)) == (a))

void fix_character(CHAR_DATA *ch)
{
    int i;
    char buf[MSL];
    bool resetaffects = FALSE;
	AFFECT_DATA *paf;
	OBJ_DATA *obj;

    if (ch->race == 0)
	ch->race = race_lookup("human");

    ch->size = pc_race_table[ch->race].size;
    ch->dam_type = 17; /*punch */

	// Add groups it should know
    for(i=0;i < MAX_GROUP; i++)
    	if( ch->pcdata->group_known[i] )
    		gn_add(ch, i);

    /* make sure they have any new race skills */
    for (i = 0; pc_race_table[ch->race].skills[i] != NULL; i++)
		group_add(ch,pc_race_table[ch->race].skills[i],FALSE);


	// TODO: Readd checks for dealing with racial affects, affects2, imm, res and vuln

	/* 20203003 - Tieryo - Fix missing racial perm affects */

	if (ch->affected_by_perm != race_table[ch->race].aff || ch->affected_by2_perm != race_table[ch->race].aff2)
	{
	ch->affected_by_perm = race_table[ch->race].aff;
	ch->affected_by2_perm = race_table[ch->race].aff2;
	resetaffects = TRUE;
	}
	if( resetaffects )
	{
		// Reset flags
		ch->imm_flags = ch->imm_flags_perm;
		ch->res_flags = ch->res_flags_perm;
		ch->vuln_flags = ch->vuln_flags_perm;
		ch->affected_by = ch->affected_by_perm;
		ch->affected_by2 = ch->affected_by2_perm;

		// Iterate through all affects
		for(paf = ch->affected; paf; paf = paf->next)
		{
			switch (paf->where)
			{
				case TO_AFFECTS:
					SET_BIT(ch->affected_by, paf->bitvector);
					SET_BIT(ch->affected_by2, paf->bitvector2);

					if( IS_SET(paf->bitvector2, AFF2_DEATHSIGHT) && (paf->level > ch->deathsight_vision) )
						ch->deathsight_vision = paf->level;

					break;
				case TO_IMMUNE:
					SET_BIT(ch->imm_flags,paf->bitvector);
					break;
				case TO_RESIST:
					SET_BIT(ch->res_flags,paf->bitvector);
					break;
				case TO_VULN:
					SET_BIT(ch->vuln_flags,paf->bitvector);
					break;
			}
		}

		// Iterate through all worn objects
		for(obj = ch->carrying; obj; obj = obj->next_content)
		{
			if( !obj->locker && obj->wear_loc != WEAR_NONE )
			{
				for(paf = obj->affected; paf; paf = paf->next)
				{
					switch (paf->where)
					{
						case TO_AFFECTS:
							SET_BIT(ch->affected_by, paf->bitvector);
							SET_BIT(ch->affected_by2, paf->bitvector2);

							if( IS_SET(paf->bitvector2, AFF2_DEATHSIGHT) && (paf->level > ch->deathsight_vision) )
								ch->deathsight_vision = paf->level;

							break;
						case TO_IMMUNE:
							SET_BIT(ch->imm_flags,paf->bitvector);
							break;
						case TO_RESIST:
							SET_BIT(ch->res_flags,paf->bitvector);
							break;
						case TO_VULN:
							SET_BIT(ch->vuln_flags,paf->bitvector);
							break;
					}
				}
			}
		}
	}

	// Update deathsight vision
	ch->deathsight_vision = ( IS_SET(ch->affected_by2_perm, AFF2_DEATHSIGHT) ) ? ch->tot_level : 0;
	for(paf = ch->affected; paf; paf = paf->next)
	{
		if( (paf->where == TO_AFFECTS) && IS_SET(paf->bitvector2, AFF2_DEATHSIGHT) && (paf->level > ch->deathsight_vision) )
			ch->deathsight_vision = paf->level;
	}
	for(obj = ch->carrying; obj; obj = obj->next_content)
	{
		if( !obj->locker && obj->wear_loc != WEAR_NONE )
		{
			for(paf = obj->affected; paf; paf = paf->next)
			{
				if( (paf->where == TO_AFFECTS) && IS_SET(paf->bitvector2, AFF2_DEATHSIGHT) && (paf->level > ch->deathsight_vision) )
					ch->deathsight_vision = paf->level;
			}
		}
	}

    ch->form = race_table[ch->race].form;
    ch->parts = race_table[ch->race].parts & ~ch->lostparts;
	ch->lostparts = 0;

    if (ch->version < 2)
    {
		group_add(ch,"global skills",FALSE);
		group_add(ch,class_table[ch->pcdata->class_current].base_group,FALSE);
		ch->version = 2;
    }

    /* make sure they have any new skills that have been added */
    if (ch->pcdata->class_mage != -1)		group_add(ch, class_table[ch->pcdata->class_mage].base_group, FALSE);
    if (ch->pcdata->class_cleric != -1)		group_add(ch, class_table[ch->pcdata->class_cleric].base_group, FALSE);
    if (ch->pcdata->class_thief != -1)		group_add(ch, class_table[ch->pcdata->class_thief].base_group, FALSE);
    if (ch->pcdata->class_warrior != -1)	group_add(ch, class_table[ch->pcdata->class_warrior].base_group, FALSE);

    if (ch->pcdata->second_sub_class_mage != -1)	group_add(ch, sub_class_table[ch->pcdata->second_sub_class_mage].default_group, FALSE);
    if (ch->pcdata->second_sub_class_cleric != -1)	group_add(ch, sub_class_table[ch->pcdata->second_sub_class_cleric].default_group, FALSE);
    if (ch->pcdata->second_sub_class_thief != -1)	group_add(ch, sub_class_table[ch->pcdata->second_sub_class_thief].default_group, FALSE);
    if (ch->pcdata->second_sub_class_warrior != -1)	group_add(ch, sub_class_table[ch->pcdata->second_sub_class_warrior].default_group, FALSE);

    if (ch->version < 6)
		ch->version = 6;

    /* reset affects */
    if (ch->version < 7)
    {
		if (IS_AFFECTED2(ch, AFF2_ENSNARE))
			REMOVE_BIT(ch->affected_by2, AFF2_ENSNARE);

		if (ch->pcdata->second_sub_class_thief == CLASS_THIEF_SAGE)
			SET_BIT(ch->affected_by, AFF_DETECT_HIDDEN);

		ch->version = 7;
    }

    if (ch->version < 8)
    {
		REMOVE_BIT(ch->comm, COMM_NOAUTOWAR);
		ch->version = 8;
    }

    if (ch->version < 10)
    {
		REMOVE_BIT(ch->act, PLR_PK);
		ch->version = 10;
    }

    if (IS_IMMORTAL(ch))
    {
		i = 0;
		while (wiznet_table[i].name != NULL)
		{
			if (ch->tot_level < wiznet_table[i].level)
			{
			REMOVE_BIT(ch->wiznet, wiznet_table[i].flag);
			}

			i++;
		}
    }

    for (i = 0; i < MAX_STATS; i++)
		if (ch->perm_stat[i] > pc_race_table[ch->race].max_stats[i])
	    	set_perm_stat(ch, i, pc_race_table[ch->race].max_stats[i]);

    // If imm flag not set, set the default flag
    /*if (ch->tot_level >= LEVEL_IMMORTAL && (ch->pcdata->immortal->imm_flag == NULL))
	switch (ch->level)
	{
	    default:
		break;
		{
		    case MAX_LEVEL - 0:
			ch->pcdata->immortal->imm_flag = str_dup("{w  -{W=I{DM{WP={w-{x   ");
			break;
		    case MAX_LEVEL - 1:
			ch->pcdata->immortal->imm_flag = str_dup("{R  C{rr{Re{ra{Rt{ro{RR{x   ");
			break;
		    case MAX_LEVEL - 2:
			ch->pcdata->immortal->imm_flag = str_dup("{W Sup{Drem{WacY{x  ");
			break;
		    case MAX_LEVEL - 3:
			ch->pcdata->immortal->imm_flag = str_dup("{b Asc{Bend{bant  ");
			break;
		    case MAX_LEVEL - 4:
			if (ch->sex == SEX_FEMALE)
			    ch->pcdata->immortal->imm_flag = str_dup("{w  Go{Wdde{wss   ");
			else
			    ch->pcdata->immortal->imm_flag = str_dup("{w    G{Wo{wd     ");
			break;
		    case MAX_LEVEL - 5:
			ch->pcdata->immortal->imm_flag = str_dup("{B  M{Ci{MN{Di{YG{Go{Wd   ");
			break;
		    case MAX_LEVEL - 6:
			ch->pcdata->immortal->imm_flag = str_dup("{x  -{m=G{xIM{mP={x-  ");
			break;
		}
	} */


    // Make sure non imms dont have builder flag!!
    if (!IS_IMMORTAL(ch) && IS_SET(ch->act, PLR_BUILDING))
    {
		sprintf(buf, "fix_character: toggling off builder flag for non-immortal %s", ch->name);
		log_string(buf);
		REMOVE_BIT(ch->act, PLR_BUILDING);
    }

    // Everyone with an expired locker rent as of this login point will have their locker rent auto-forgiven.
    if( ch->version < VERSION_PLAYER_003)
	{
		if( ch->locker_rent > 0 )
		{
			struct tm *now_time;
			struct tm *rent_time;

			now_time = (struct tm *)localtime(&current_time);
			rent_time = (struct tm *)localtime(&ch->locker_rent);

			if( now_time > rent_time )
			{
				ch->locker_rent = current_time;
				rent_time = (struct tm *)localtime(&ch->locker_rent);
				rent_time->tm_mon += 1;
				ch->locker_rent = (time_t) mktime(rent_time);
			}
		}
		ch->version = VERSION_PLAYER_003;
	}

	if( ch->version < VERSION_PLAYER_004 ) {
		// Update all affects from object to include their wear slot


		ch->version = VERSION_PLAYER_004;
	}

}




bool missing_class(CHAR_DATA *ch)
{
    int classes;

    classes = 0;
    if (ch->pcdata->sub_class_mage != -1)
	classes++;
    if (ch->pcdata->sub_class_cleric != -1)
	classes++;
    if (ch->pcdata->sub_class_thief != -1)
	classes++;
    if (ch->pcdata->sub_class_warrior != -1)
	classes++;

    if (classes < (IS_REMORT(ch)?120:ch->tot_level) / 31 + 1) return TRUE;
    else return FALSE;
}


/* Check for screwed up subclasses. Ie people who have a mage class
   where their warrior class should be. No clue how it originally happened
   but here is the fix based on skill group.*/
void descrew_subclasses(CHAR_DATA *ch)
{
    char buf[MSL];

    if (ch == NULL)
    {
	bug("descrew_subclasses: null ch.", 0);
	return;
    }

    if (missing_class(ch) || (ch->pcdata->sub_class_mage != -1
    && (ch->pcdata->sub_class_mage < 3 || ch->pcdata->sub_class_mage > 5 )))
    {
	sprintf(buf, "descrew_subclasses: %s had a non-mage class!",
		ch->name);
	bug(buf, 0);
	if (ch->pcdata->group_known[group_lookup("necromancer skills")] == TRUE)
	    ch->pcdata->sub_class_mage = CLASS_MAGE_NECROMANCER;
	else if (ch->pcdata->group_known[group_lookup("sorcerer skills")] == TRUE)
	    ch->pcdata->sub_class_mage = CLASS_MAGE_SORCERER;
	else if (ch->pcdata->group_known[group_lookup("wizard skills")] == TRUE)
	    ch->pcdata->sub_class_mage = CLASS_MAGE_WIZARD;

	//sprintf(buf, "{WYou had a screwed up mage class... it has been fixed to {Y%s.{x\n\r",
	//    sub_class_table[ch->pcdata->sub_class_mage].name);
	//send_to_char(buf, ch);
    }

    if (missing_class(ch) || (ch->pcdata->sub_class_cleric != -1
    && (ch->pcdata->sub_class_cleric < 6 || ch->pcdata->sub_class_cleric > 8)))
    {
	sprintf(buf, "descrew_subclasses: %s had a non-cleric class!",
		ch->name);
	bug(buf, 0);
	if (ch->pcdata->group_known[group_lookup("witch skills")] == TRUE)
	    ch->pcdata->sub_class_cleric = CLASS_CLERIC_WITCH;
	else if (ch->pcdata->group_known[group_lookup("druid skills")] == TRUE)
	    ch->pcdata->sub_class_cleric = CLASS_CLERIC_DRUID;
	else if (ch->pcdata->group_known[group_lookup("monk skills")] == TRUE)
	    ch->pcdata->sub_class_cleric = CLASS_CLERIC_MONK;

//	sprintf(buf, "{WYou had a screwed up cleric class... it has been fixed to {Y%s.{x\n\r",
//	    sub_class_table[ch->pcdata->sub_class_cleric].name);
//	send_to_char(buf, ch);
    }

    if (missing_class(ch) || (ch->pcdata->sub_class_thief != -1
    && (ch->pcdata->sub_class_thief < 9 || ch->pcdata->sub_class_thief > 11)))
    {
	sprintf(buf, "descrew_subclasses: %s had a non-thief class!",
		ch->name);
	bug(buf, 0);

	if (ch->pcdata->group_known[group_lookup("assassin skills")] == TRUE)
	    ch->pcdata->sub_class_thief = CLASS_THIEF_ASSASSIN;
	if (ch->pcdata->group_known[group_lookup("rogue skills")] == TRUE)
	    ch->pcdata->sub_class_thief = CLASS_THIEF_ROGUE;
	if (ch->pcdata->group_known[group_lookup("bard skills")] == TRUE)
	    ch->pcdata->sub_class_thief = CLASS_THIEF_BARD;

//	sprintf(buf, "{WYou had a screwed up thief class... it has been fixed to {Y%s.{x\n\r",
//	    sub_class_table[ch->pcdata->sub_class_thief].name);
//	send_to_char(buf, ch);
    }

    if (missing_class(ch) || (ch->pcdata->sub_class_thief != -1
    && (ch->pcdata->sub_class_warrior > 2)))
    {
	sprintf(buf, "descrew_subclasses: %s had a non-warrior class!",
		ch->name);
	bug(buf, 0);
	if (ch->pcdata->group_known[group_lookup("marauder skills")] == TRUE)
	    ch->pcdata->sub_class_warrior = CLASS_WARRIOR_MARAUDER;
	if (ch->pcdata->group_known[group_lookup("gladiator skills")] == TRUE)
	    ch->pcdata->sub_class_warrior = CLASS_WARRIOR_GLADIATOR;
	if (ch->pcdata->group_known[group_lookup("paladin skills")] == TRUE)
	    ch->pcdata->sub_class_warrior = CLASS_WARRIOR_PALADIN;

//	sprintf(buf, "{WYou had a screwed up warrior class... it has been fixed to {Y%s.{x\n\r",
//	    sub_class_table[ch->pcdata->sub_class_warrior].name);
//	send_to_char(buf, ch);
    }
}


// Check someone's classes. Used because for some godawful reason people can be missing a mage class, etc.
bool has_correct_classes(CHAR_DATA *ch)
{
    int correctnum;
    int num = 0;
    char buf[MSL];

    // figure out how many classes they're supposed to have.
    if (IS_REMORT(ch) || ch->tot_level > 90)
	correctnum = 4;
    else if (ch->tot_level > 60)
	correctnum = 3;
    else if (ch->tot_level > 30)
	correctnum = 2;
    else
	correctnum = 1;

    // figure out how many classes they do have
    if (ch->pcdata->class_mage != -1) num++;

    if (ch->pcdata->class_cleric != -1) num++;

    if (ch->pcdata->class_thief != -1) num++;

    if (ch->pcdata->class_warrior != -1) num++;

    if (num != correctnum) {
	sprintf(buf, "Class problem detected. #classes needed: %d, #had: %d.", correctnum, num);
	log_string(buf);
	//send_to_char(buf,ch); send_to_char("\n\r", ch);
	return FALSE;
    } else
	return TRUE;
}


void fix_broken_classes(CHAR_DATA *ch)
{
    char buf[MSL];
    int mage    = ch->pcdata->class_mage;
    int cleric  = ch->pcdata->class_cleric;
    int thief   = ch->pcdata->class_thief;
    int warrior = ch->pcdata->class_warrior;

    sprintf(buf, "fix_broken_classes: fixing broken classes for %s, a level %d %s.",
        ch->name, ch->tot_level, race_table[ch->race].name);
    log_string(buf);

    if (mage == -1 && find_class_skill(ch, CLASS_MAGE) == TRUE) {
	//send_to_char("Found mage skills but no mage class, setting mage class.\n\r", ch);
	log_string("Set mage class");
	ch->pcdata->class_mage = CLASS_MAGE;
	group_add(ch, "mage skills", FALSE);
    }

    if (cleric == -1 && find_class_skill(ch, CLASS_CLERIC) == TRUE) {
	//send_to_char("Found cleric skills but no cleric class, setting cleric class.\n\r", ch);
	log_string("Set cleric class");
	ch->pcdata->class_cleric = CLASS_CLERIC;
	group_add(ch, "cleric skills", FALSE);
    }

    if (thief == -1 && find_class_skill(ch, CLASS_THIEF) == TRUE) {
	//send_to_char("Found thief skills but no thief class, setting thief class.\n\r", ch);
	log_string("Set thief class");
	ch->pcdata->class_thief = CLASS_THIEF;
	group_add(ch, "thief skills", FALSE);
    }

    if (warrior == -1 && find_class_skill(ch, CLASS_WARRIOR) == TRUE) {
	//send_to_char("Found warrior skills but no warrior class, setting warrior class.\n\r", ch);
	log_string("Set warrior class");
	ch->pcdata->class_warrior = CLASS_WARRIOR;
	group_add(ch, "warrior skills", FALSE);
    }

    save_char_obj(ch);
    //send_to_char("All fixed!\n\r", ch);
}


// Find out if a player has a skill, ANY skill, which belongs to a general class.
bool find_class_skill(CHAR_DATA *ch, int class)
{
    int gn;
    int i;

    switch (class)
    {
	case CLASS_MAGE: 	gn = group_lookup("mage skills"); 	break;
	case CLASS_CLERIC:	gn = group_lookup("cleric skills");	break;
	case CLASS_THIEF:	gn = group_lookup("thief skills");	break;
	case CLASS_WARRIOR:	gn = group_lookup("warrior skills");	break;
	default:
	    bug("find_class_skill: bad class.", 0);
	    return FALSE;
    }

    for (i = 0; group_table[gn].spells[i] != NULL; i++) {
	if (get_skill(ch, skill_lookup(group_table[gn].spells[i])) > 0)
	    return TRUE;
    }

    return FALSE;
}


/* write a token */
void fwrite_token(TOKEN_DATA *token, FILE *fp)
{
	int i;

	fprintf(fp, "#TOKEN %ld\n", token->pIndexData->vnum);
	fprintf(fp, "UId %d\n", (int)token->id[0]);
	fprintf(fp, "UId2 %d\n", (int)token->id[1]);
	fprintf(fp, "Timer %d\n", token->timer);
	for (i = 0; i < MAX_TOKEN_VALUES; i++)
		fprintf(fp, "Value %d %ld\n", i, token->value[i]);

	if(token->progs && token->progs->vars) {
		pVARIABLE var;

		for(var = token->progs->vars; var; var = var->next) {
			if(var->save)
				variable_fwrite(var, fp);
		}
	}

	fprintf(fp, "End\n\n");
}


/* read a token from a file. */
TOKEN_DATA *fread_token(FILE *fp)
{
    TOKEN_DATA *token;
    TOKEN_INDEX_DATA *token_index;
    long vnum;
    char buf[MSL];
    char *word;
    bool fMatch;
    int vtype;

    vnum = fread_number(fp);
    if ((token_index = get_token_index(vnum)) == NULL) {
	sprintf(buf, "fread_token: no token index found for vnum %ld", vnum);
	bug(buf, 0);
	return NULL;
    }

    token = new_token();
    token->pIndexData = token_index;
    token->name = str_dup(token_index->name);
    token->description = str_dup(token_index->description);
    token->type = token_index->type;
    token->flags = token_index->flags;
    token->progs = new_prog_data();
    token->progs->progs = token_index->progs;
    token_index->loaded++;	// @@@NIB : 20070127 : for "tokenexists" ifcheck
    token->id[0] = token->id[1] = 0;
	token->global_next = global_tokens;
	global_tokens = token;

    variable_copylist(&token_index->index_vars,&token->progs->vars,FALSE);

    for (; ;)
    {
		word   = feof(fp) ? "End" : fread_word(fp);
		fMatch = FALSE;

		if (!str_cmp(word, "End")) {
			get_token_id(token);
			fMatch = TRUE;
			return token;
		}

		switch (UPPER(word[0]))
		{
			case 'T':
			KEY("Timer",	token->timer,		fread_number(fp));
			break;

			case 'U':
			KEY("UId",	token->id[0],		fread_number(fp));
			KEY("UId2",	token->id[1],		fread_number(fp));
			break;

			case 'V':
			if (!str_cmp(word, "Value")) {
				int i;

				i = fread_number(fp);
				token->value[i] = fread_number(fp);
				fMatch = TRUE;
			}

			if( (vtype = variable_fread_type(word)) != VAR_UNKNOWN ) {
				variable_fread(&token->progs->vars, vtype, fp);
				fMatch = TRUE;
			}

			break;
		}

	    if (!fMatch) {
			sprintf(buf, "read_token: no match for word %s", word);
			bug(buf, 0);
			fread_to_eol(fp);
	    }
    }

    return token;
}

void fwrite_skill(CHAR_DATA *ch, SKILL_ENTRY *entry, FILE *fp)
{
		fprintf(fp, "#SKILL\n");
		switch(entry->source) {
		case SKILLSRC_SCRIPT:		fprintf(fp, "TypeScript\n"); break;
		case SKILLSRC_SCRIPT_PERM:	fprintf(fp, "TypeScriptPerm\n"); break;
		case SKILLSRC_AFFECT:		fprintf(fp, "TypeAffect\n"); break;
		// Normal is default
		}

		// Only save if it's
		if( (entry->flags & ~SKILL_SPELL) != SKILL_AUTOMATIC)
			fprintf(fp, "Flags %s\n", flag_string( skill_flags, entry->flags));
		if( IS_VALID(entry->token) ) {
			fwrite_token(entry->token, fp);
		}

		if( entry->sn > 0 && entry->sn < MAX_SKILL ) {
			fprintf(fp, "Sk %d %d %s~\n",
			    ch->pcdata->learned[entry->sn],
			    ch->pcdata->mod_learned[entry->sn],
			    skill_table[entry->sn].name);
		}

		if( entry->song >= 0 && entry->song < MAX_SONGS ) {
			fprintf(fp, "Song %s~\n", music_table[entry->song].name);
		}
/*
	for (sn = 0; sn < MAX_SONGS && music_table[sn].name; sn++)
		if( ch->pcdata->songs_learned[sn] )
			fprintf(fp, "Song '%s'\n", music_table[sn].name);

	for (sn = 0; sn < MAX_SKILL && skill_table[sn].name; sn++)
	{
	    if (skill_table[sn].name != NULL && ch->pcdata->learned[sn] != 0)
	    {
		fprintf(fp, "Sk %d '%s'\n",
		    ch->pcdata->learned[sn], skill_table[sn].name);
	    }
	    if (skill_table[sn].name != NULL && ch->pcdata->mod_learned[sn] != 0)
	    {
		fprintf(fp, "SkMod %d '%s'\n",
		    ch->pcdata->mod_learned[sn], skill_table[sn].name);
	    }
	}
*/
		fprintf(fp, "End\n\n");
}

void fwrite_skills(CHAR_DATA *ch, FILE *fp)
{
	SKILL_ENTRY *entry;

	for(entry = ch->sorted_skills; entry; entry = entry->next)
		fwrite_skill(ch, entry, fp);

	for(entry = ch->sorted_songs; entry; entry = entry->next)
		fwrite_skill(ch, entry, fp);
}

void fread_skill(FILE *fp, CHAR_DATA *ch)
{
    TOKEN_DATA *token = NULL;
    int sn = -1;
    int song = -1;
    long flags = SKILL_AUTOMATIC;
    int rating = -1, mod = 0;	// For built-in skills
    char source = SKILLSRC_NORMAL;
    char buf[MSL];
    char *word;
    bool fMatch;

    for (; ;)
    {
		word   = feof(fp) ? "End" : fread_word(fp);
		fMatch = FALSE;

		if (!str_cmp(word, "End")) {
			if( song >= 0 ) {
				ch->pcdata->songs_learned[song] = TRUE;
				skill_entry_addsong(ch, song, NULL, source);
			} else if(sn > 0) {
				ch->pcdata->learned[sn] = rating;
				ch->pcdata->mod_learned[sn] = mod;
				if( skill_table[sn].spell_fun == spell_null)
					skill_entry_addskill(ch, sn, NULL, source, flags);
				else
					skill_entry_addspell(ch, sn, NULL, source, flags);
			} else if(IS_VALID(token))
				token_to_char_ex(token, ch, source, flags);

		    fMatch = TRUE;
			return;
		}

		switch (UPPER(word[0]))
		{
		case '#':
			if( IS_KEY("#TOKEN") ) {
				token = fread_token(fp);
				fMatch = TRUE;
			}
			break;

		case 'F':
			FVKEY("Flags",	flags, fread_string_eol(fp), skill_flags);
			break;

		case 'S':
			if(IS_KEY("Sk")) {
				rating = fread_number(fp);
				mod = fread_number(fp);
				sn = skill_lookup(fread_string(fp));
				fMatch = TRUE;
				break;
			}

			if(IS_KEY("Song")) {
				song = music_lookup(fread_string(fp));
				fMatch = TRUE;
				break;
			}

			break;
		}

	    if (!fMatch) {
			sprintf(buf, "fread_skill: no match for word %s", word);
			bug(buf, 0);
			fread_to_eol(fp);
	    }
	}

}

/* write a quest to disk */
void fwrite_quest_part(FILE *fp, QUEST_PART_DATA *part)
{
    /* Recursion to make sure we don't have list flipping */
    if (part->next != NULL)
	fwrite_quest_part(fp, part->next);

    fprintf(fp, "#QUESTPART\n");

    if (part->pObj != NULL && !part->complete) { // Special case. Objects will be extracted on quit, re-loaded on login.
	if (part->pObj->in_room == NULL)
	    bug("fwrite_quest_part: trying to save a quest pickup obj with null in_room", 0);
	else
	    fprintf(fp, "OPart %ld %ld\n", part->pObj->pIndexData->vnum, part->pObj->in_room->vnum);
    }
    else if (part->mob != -1)
	fprintf(fp, "MPart %ld\n", part->mob);
    else if (part->obj_sac != -1)
	fprintf(fp, "OSPart %ld\n", part->obj_sac);
    else if (part->mob_rescue != -1)
	fprintf(fp, "MRPart %ld\n", part->mob_rescue);
    else if (part->room != -1)
	fprintf(fp, "QRoom %ld\n", part->room);
    else if (part->custom_task)
		fprintf(fp, "QCustom\n");

    if (part->complete)
		fprintf(fp, "QComplete\n");
	fprintf(fp, "QDescription %s~\n", part->description);

    fprintf(fp, "End\n");
}

QUEST_PART_DATA *fread_quest_part(FILE *fp)
{
    QUEST_PART_DATA *part;
    char buf[MSL];
    char *word;
    bool fMatch;
    int i;

    part = new_quest_part();

    for (; ;)
    {
		word   = feof(fp) ? "End" : fread_word(fp);
		fMatch = FALSE;

		if (!str_cmp(word, "End")) {
			fMatch = TRUE;
			return part;
		}

		switch (UPPER(word[0]))
		{
		    case 'M':
			if (!str_cmp(word, "MPart")) {
			    i = fread_number(fp);
			    part->mob = i;
			    fMatch = TRUE;
			    break;
			}

			if (!str_cmp(word, "MRPart")) {
			    i = fread_number(fp);
			    part->mob_rescue = i;
			    fMatch = TRUE;
			    break;
			}
			break;

		    case 'O':
			/* Special Case - Make an Obj */
			if (!str_cmp(word, "OPart")) {
			    ROOM_INDEX_DATA *room;
			    OBJ_DATA *obj;
			    OBJ_INDEX_DATA *obj_i;
			    int room_vnum;

			    i = fread_number(fp);
			    part->obj = i;

			    obj_i = get_obj_index(part->obj);

			    room_vnum = fread_number(fp);
			    room = get_room_index(room_vnum);

			    obj = create_object(obj_i, 1, TRUE);
			    obj_to_room(obj, room);

			    part->pObj = obj;

			    fMatch = TRUE;
			    break;
			}

			if (!str_cmp(word, "OSPart")) {
			    i = fread_number(fp);
			    part->obj_sac = i;
			    fMatch = TRUE;
			    break;
			}
			break;

		    case 'Q':
			if (!str_cmp(word, "QCustom")) {
				part->custom_task = TRUE;
				fMatch = TRUE;
				break;
			}
			if (!str_cmp(word, "QDescription")) {
				part->description = fread_string(fp);
				fMatch = TRUE;
				break;
			}
			if (!str_cmp(word, "QRoom")) {
			    i = fread_number(fp);
			    part->room = i;
			    fMatch = TRUE;
			    break;
			}

			if (!str_cmp(word, "QComplete")) {
			    part->complete = TRUE;
			    fMatch = TRUE;
			    fread_to_eol(fp);
			    break;
			}

			break;
		}

	    if (!fMatch) {
		    sprintf(buf, "read_quest_part: no match for word %s", word);
		    bug(buf, 0);
		    fread_to_eol(fp);
	    }
    }
}
