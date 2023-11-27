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

// VERSION_OBJECT_004 special defines
#define VO_004_CONT_PICKPROOF	(B)		// For Containers and Books
#define VO_004_CONT_LOCKED		(D)
#define VO_004_CONT_SNAPKEY		(F)
#define VO_004_EX_LOCKED		(C)		// For Portals
#define VO_004_EX_PICKPROOF		(F)
#define VO_004_EX_EASY			(H)
#define VO_004_EX_HARD			(I)
#define VO_004_EX_INFURIATING	(J)


// External functions
extern bool loading_immortal_data;

// Globals.
OBJ_DATA *	pneuma_relic;
OBJ_DATA *	damage_relic;
OBJ_DATA *	xp_relic;
OBJ_DATA *	hp_regen_relic;
OBJ_DATA *	mana_regen_relic;

// Array of containers read for proper re-nesting of objects.
OBJ_DATA *	rgObjNest[MAX_NEST];
int 		nest_level;

void fread_stache(FILE *fp, LLIST *lstache);
void fwrite_stache_obj(CHAR_DATA *ch, OBJ_DATA *obj, FILE *fp);

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

void fwrite_stache_char(CHAR_DATA *ch, FILE *fp)
{
	if (list_size(ch->lstache) > 0)
	{
		fprintf(fp, "#STACHE\n");

		ITERATOR it;
		OBJ_DATA *obj;
		iterator_start(&it, ch->lstache);
		while((obj = (OBJ_DATA *)iterator_nextdata(&it)))
		{
			fwrite_obj_new(ch, obj, fp, 0);
		}

		iterator_stop(&it);

		fprintf(fp, "#-STACHE\n");
	}
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
		p_percent_trigger( ch,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SAVE, NULL ,0,0,0,0,0);

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

		fwrite_stache_char(ch, fp);

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
    int pos;
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
		fprintf(fp, "RepopRoomW %lu %lu %lu %lu\n", ch->recall.wuid, ch->recall.id[0], ch->recall.id[1], ch->recall.id[2]);
	else if(ch->recall.id[1] || ch->recall.id[2])
		fprintf(fp, "RepopRoomC %ld %lu %lu %lu\n", ch->recall.area->uid, ch->recall.id[0], ch->recall.id[1], ch->recall.id[2]);
	else
		fprintf(fp, "RepopRoom %ld %ld\n", ch->recall.area->uid, ch->recall.id[0]);
    }
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
		fprintf(fp, "Room_before_arenaC %lu %lu %lu %lu\n",
			ch->pcdata->room_before_arena.area->uid,
		 	ch->pcdata->room_before_arena.id[0], ch->pcdata->room_before_arena.id[1], ch->pcdata->room_before_arena.id[2]);
	else
		fprintf(fp, "Room_before_arena %ld %ld\n",
			ch->pcdata->room_before_arena.area->uid,
		 	ch->pcdata->room_before_arena.id[0]);
    }

    fprintf(fp, "Not  %ld %ld %ld %ld %ld\n",
	(long int)ch->pcdata->last_note,(long int)ch->pcdata->last_idea,(long int)ch->pcdata->last_penalty,
	(long int)ch->pcdata->last_news,(long int)ch->pcdata->last_changes	);
    fprintf(fp, "Scro %d\n", 	ch->lines		);
    if (IS_IMMORTAL(ch))
		fprintf(fp, "LastInquiryRead %ld\n", (long int)ch->pcdata->last_project_inquiry);

	if( ch->in_room &&
		IS_VALID(ch->in_room->instance_section) &&
		IS_VALID(ch->in_room->instance_section->instance) &&
		IS_VALID(ch->in_room->instance_section->instance->dungeon) )
	{
		DUNGEON *dungeon = ch->in_room->instance_section->instance->dungeon;

		if( dungeon->entry_room )
			fprintf(fp,"Room %s\n", widevnum_string_room(dungeon->entry_room, NULL));
		else
			fprintf (fp, "Room %s\n", widevnum_string_wnum(room_wnum_temple, NULL));
	}
	else if( ch->checkpoint ) {
		if( ch->checkpoint->wilds )
			fprintf (fp, "Vroom %ld %ld %ld %ld\n",
				ch->checkpoint->x, ch->checkpoint->y, ch->checkpoint->wilds->pArea->uid, ch->checkpoint->wilds->uid);
		else if(ch->checkpoint->source)
			fprintf(fp,"CloneRoom %s %ld %ld\n",
				widevnum_string_room(ch->checkpoint->source, NULL), ch->checkpoint->id[0], ch->checkpoint->id[1]);
		else
			fprintf(fp,"Room %s\n", widevnum_string_room(ch->checkpoint, NULL));
	} else if(!ch->in_room)
		fprintf (fp, "Room %s\n", widevnum_string_wnum(room_wnum_temple, NULL));
	else if(ch->in_wilds) {
		fprintf (fp, "Vroom %ld %ld %ld %ld\n",
			ch->in_room->x, ch->in_room->y, ch->in_wilds->pArea->uid, ch->in_wilds->uid);
	} else if(ch->was_in_wilds) {
		fprintf (fp, "Vroom %d %d %ld %ld\n",
			ch->was_at_wilds_x, ch->was_at_wilds_y, ch->was_in_wilds->pArea->uid, ch->was_in_wilds->uid);
	} else if(ch->was_in_room) {
		if(ch->was_in_room->source)
			fprintf(fp,"CloneRoom %s %ld %ld\n",
				widevnum_string_room(ch->was_in_room->source, NULL), ch->was_in_room->id[0], ch->was_in_room->id[1]);
		else
			fprintf(fp,"Room %s\n", widevnum_string_room(ch->was_in_room, NULL));
	} else if(ch->in_room->source) {
		fprintf(fp,"CloneRoom %s %ld %ld\n",
			widevnum_string_room(ch->in_room->source, NULL), ch->in_room->id[0], ch->in_room->id[1]);
	} else
		fprintf(fp,"Room %s\n", widevnum_string_room(ch->in_room, NULL));

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

	// TODO: Add Toxin trait?
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
		fprintf(fp, "Before_socialC %ld#%ld %lu %lu\n", 	ch->before_social.area->uid, ch->before_social.id[0], ch->before_social.id[1], ch->before_social.id[2]);
	else
		fprintf(fp, "Before_social %ld#%ld\n", 	ch->before_social.area->uid, ch->before_social.id[0]);
    }

    if (ch->pneuma != 0)
		fprintf(fp, "Pneuma %ld\n", ch->pneuma);
    if (ch->home.pArea && ch->home.vnum > 0)
		fprintf(fp, "Home %s\n", widevnum_string_wnum(ch->home, NULL));
	if (ch->pcdata->personal_mount.pArea && ch->pcdata->personal_mount.vnum > 0)
		fprintf(fp, "PersonalMount %s\n", widevnum_string_wnum(ch->pcdata->personal_mount, NULL));
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
		fprintf(fp, "QuestGiver %s\n", widevnum_string_wnum(ch->quest->questgiver, NULL));
		fprintf(fp, "QuestReceiverType %d\n", ch->quest->questreceiver_type);
		fprintf(fp, "QuestReceiver %s\n", widevnum_string_wnum(ch->quest->questreceiver, NULL));

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
    if (ch->act[0] != 0)
	fprintf(fp, "Act  %s\n",   print_flags(ch->act[0]));
    if (ch->act[1] != 0)
	fprintf(fp, "Act2 %s\n",   print_flags(ch->act[1]));
    if (ch->affected_by[0] != 0)		fprintf(fp, "AfBy %s\n",   print_flags(ch->affected_by[0]));
    if (ch->affected_by[1] != 0)		fprintf(fp, "AfBy2 %s\n",   print_flags(ch->affected_by[1]));

    // 20140514 NIB - adding for being able to reset the flags
    if (ch->affected_by_perm[0] != 0)	fprintf(fp, "AfByPerm %s\n",   print_flags(ch->affected_by_perm[0]));
    if (ch->affected_by_perm[1] != 0) fprintf(fp, "AfBy2Perm %s\n",   print_flags(ch->affected_by_perm[1]));
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
		fprintf(fp, "Titl %s~\n",	fix_string(ch->pcdata->title)	);
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

		ITERATOR aurait;
		AURA_DATA *aura;

		iterator_start(&aurait, ch->auras);
		while((aura = (AURA_DATA *)iterator_nextdata(&aurait)))
		{
			fprintf(fp, "Aura %s~ %s~\n", aura->name, aura->long_descr);
		}
		iterator_stop(&aurait);

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

		// TODO: Getting segfaults here
		ITERATOR git;
		SKILL_GROUP *group;
		iterator_start(&git, ch->pcdata->group_known);
		while((group = (SKILL_GROUP *)iterator_nextdata(&git)))
		{
			if (IS_VALID(group))
				fprintf(fp, "Gr '%s'\n", group->name);
		}
		iterator_stop(&git);
	}

    for (paf = ch->affected; paf != NULL; paf = paf->next)
    {
	if (!paf->custom_name && !IS_VALID(paf->skill))
	    continue;

	fprintf(fp, "%s '%s' '%s' %3d %3d %3d %3d %3d %10ld %10ld %d\n",
	    (paf->custom_name?"Affcgn":"Affcg"),
	    (paf->custom_name?paf->custom_name:paf->skill->name),
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

    ITERATOR sit;
    SHIP_DATA *ship;
    iterator_start(&sit, ch->pcdata->ships);
	while( (ship = (SHIP_DATA *)iterator_nextdata(&sit)) )
	{
		fprintf(fp, "Ship %lu %lu\n", ship->id[0], ship->id[1]);
	}
    iterator_stop(&sit);

    ITERATOR uait;
    AREA_DATA *unlocked_area;
    iterator_start(&uait, ch->pcdata->unlocked_areas);
    while( (unlocked_area = (AREA_DATA *)iterator_nextdata(&uait)) )
    {
		fprintf(fp, "UnlockedArea %ld\n", unlocked_area->uid);
	}
    iterator_stop(&uait);

    for (cmd = ch->pcdata->commands; cmd != NULL; cmd = cmd->next)
		fprintf(fp, "GrantedCommand %s~\n", cmd->name);
	#ifdef IMC
	imc_savechar( ch, fp );
	#endif

	ITERATOR rpit;
	REPUTATION_DATA *rep;

	iterator_start(&rpit, ch->reputations);
	while((rep = (REPUTATION_DATA *)iterator_nextdata(&rpit)))
	{
		fprintf(fp, "Reputation %ld#%ld %d %ld\n",
			rep->pIndexData->area->uid, rep->pIndexData->vnum,
			rep->current_rank,
			rep->reputation);
	}
	iterator_stop(&rpit);

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
    ch->act[0]				= PLR_NOSUMMON;
    ch->act[1]				= 0;
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

				resolve_special_key(obj);

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

					if (IS_CONTAINER(container))
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
			} else if (!str_cmp(word, "STACHE")) {
				fread_stache(fp, ch->lstache);
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

    // Do not bother fixing ANYTHING on the player
    // The only reason this is true will be during the reading of the staff list
    //   and needed to get the creation date
    if(loading_immortal_data)
    {
    	return found;
	}

	get_mob_id(ch);

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

			add_immortal(immortal);

		} else { // Readjust the char's level accordingly.
			sprintf(buf, "load_char_obj: reading immortal char %s.\n\r", ch->name);
			log_string(buf);

			ch->pcdata->immortal = immortal;
			//ch->level = immortal->level;
			//ch->tot_level = immortal->level;
		}
	    immortal->pc = ch->pcdata;

    }


    // Fix char.
    if (found)
	fix_character(ch);

    /* Redo shift. Remember ch->shifted was just used as a placeholder to tell the game
       to re-shift, so we have to switch it to none first. */
    if (ch->shifted != SHIFTED_NONE) {
	ch->shifted = SHIFTED_NONE;
	shift_char(ch, TRUE);
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

	bug(word, 0);

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
	    KEY("Act",		ch->act[0],		fread_flag(fp));
	    KEY("Act2",	ch->act[1],		fread_flag(fp));
	    KEY("AffectedBy",	ch->affected_by[0],	fread_flag(fp));
	    KEY("AfBy",	ch->affected_by[0],	fread_flag(fp));
	    KEY("AfBy2",	ch->affected_by[1],	fread_flag(fp));
	    KEY("AfByPerm", ch->affected_by_perm[0],	fread_flag(fp));
	    KEY("AfBy2Perm", ch->affected_by_perm[1],	fread_flag(fp));
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

		paf = new_affect();

		SKILL_DATA *skill = get_skill_data(fread_word(fp));
		if (!IS_VALID(skill))
		    log_string("fread_char: unknown skill.");
		else
		    paf->skill = skill;

		paf->catalyst_type = -1;
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

                paf = new_affect();

				SKILL_DATA *skill = get_skill_data(fread_word(fp));
				if (!IS_VALID(skill))
					log_string("fread_char: unknown skill.");
				else
					paf->skill = skill;

				paf->catalyst_type = -1;
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

                paf = new_affect();

				SKILL_DATA *skill = get_skill_data(fread_word(fp));
				if (!IS_VALID(skill))
					log_string("fread_char: unknown skill.");
				else
					paf->skill = skill;

				paf->catalyst_type = -1;
				paf->custom_name = NULL;
				paf->group  = flag_value(affgroup_mobile_flags,fread_word(fp));
				if(paf->group == NO_FLAG) paf->group = AFFGROUP_MAGICAL;
                paf->where  = fread_number(fp);
                paf->level      = fread_number(fp);
                paf->duration   = fread_number(fp);
                paf->modifier   = fread_number(fp);
                paf->location   = fread_number(fp);
                if(paf->location == APPLY_SKILL) {
					SKILL_DATA *sk = get_skill_data(fread_word(fp));
					if(IS_VALID(sk)) {
						paf->location += sk->uid;
					} else {
						paf->location = APPLY_NONE;
						paf->modifier = 0;
					}
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

				paf->catalyst_type = -1;
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

				paf->catalyst_type = -1;
				paf->skill = NULL;
				paf->group  = flag_value(affgroup_mobile_flags,fread_word(fp));
				if(paf->group == NO_FLAG) paf->group = AFFGROUP_MAGICAL;
                paf->where  = fread_number(fp);
                paf->level      = fread_number(fp);
                paf->duration   = fread_number(fp);
                paf->modifier   = fread_number(fp);
                paf->location   = fread_number(fp);
                if(paf->location == APPLY_SKILL) {
					SKILL_DATA *sk = get_skill_data(fread_word(fp));
					if (IS_VALID(sk))
						paf->location += sk->uid;
					else {
						paf->location = APPLY_NONE;
						paf->modifier = 0;
					}
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

		if (!str_cmp(word, "Aura"))
		{
			char *name = fread_string(fp);
			char *long_descr = fread_string(fp);

			add_aura_to_char(ch, name, long_descr);

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
		location_set(&ch->before_social,get_area_from_uid(fread_number(fp)),0,fread_number(fp),0,0);
		fMatch = TRUE;
	    }
            if (!str_cmp(word, "Before_socialC")) {
		location_set(&ch->before_social,get_area_from_uid(fread_number(fp)),0,fread_number(fp),fread_number(fp),fread_number(fp));
		fMatch = TRUE;
	    }
            if (!str_cmp(word, "Before_socialW")) {
		location_set(&ch->before_social,NULL,fread_number(fp),fread_number(fp),fread_number(fp),fread_number(fp));
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
			WNUM_LOAD w = fread_widevnum(fp, 0);
		    unsigned long id1 = fread_number(fp);
		    unsigned long id2 = fread_number(fp);

		    room = get_room_index_auid(w.auid, w.vnum);


		    ch->in_room = get_clone_room(room,id1,id2);

			if (ch->in_room == NULL)
				ch->in_room = room_index_temple;
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
                char *temp;

                temp = fread_word(fp) ;
                SKILL_GROUP *group = group_lookup(temp);
                if (!IS_VALID(group))
                {
                    sprintf(buf, "fread_char: unknown group %s.", temp);
				    log_string(buf);
                }
                else
				{
					list_appendlink(ch->pcdata->group_known, group);
				}

				fMatch = TRUE;
				break;
            }

	    break;

	case 'H':
	    KEY("Hitroll",	ch->hitroll,		fread_number(fp));
	    KEY("Hit",		ch->hitroll,		fread_number(fp));
		if (!str_cmp(word, "Home"))
		{
			WNUM_LOAD load = fread_widevnum(fp, 0);

			ch->home.pArea = get_area_from_uid(load.auid);
			ch->home.vnum = load.vnum;
			fMatch = TRUE;
			break;
		}

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
		if (!str_cmp(word, "PersonalMount"))
		{
			WNUM_LOAD load = fread_widevnum(fp, 0);

			if (get_mob_index_auid(load.auid, load.vnum))
			{
				ch->pcdata->personal_mount.pArea = get_area_from_uid(load.auid);
				ch->pcdata->personal_mount.vnum = load.vnum;
			}
			else
			{
				ch->pcdata->personal_mount.pArea = NULL;
				ch->pcdata->personal_mount.vnum = 0;
			}

			fMatch = TRUE;
			break;
		}
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
			if( ch->quest == NULL )
			{
				ch->quest = (QUEST_DATA *)new_quest();
			}
			ch->quest->questgiver_type = fread_number(fp);
			fMatch = TRUE;
			break;
	    }

	    if (!str_cmp(word, "QuestGiver"))
	    {
			if( ch->quest == NULL )
			{
				ch->quest = (QUEST_DATA *)new_quest();
			}

			WNUM_LOAD load = fread_widevnum(fp, 0);
			ch->quest->questgiver.pArea = get_area_from_uid(load.auid);
			ch->quest->questgiver.vnum = load.vnum;
	
			fMatch = TRUE;
			break;
	    }

	    if (!str_cmp(word, "QuestReceiverType"))
	    {
			if( ch->quest == NULL )
			{
				ch->quest = (QUEST_DATA *)new_quest();
			}
			ch->quest->questreceiver_type = fread_number(fp);
			fMatch = TRUE;
			break;
	    }

	    if (!str_cmp(word, "QuestReceiver"))
	    {
			if( ch->quest == NULL )
			{
				ch->quest = (QUEST_DATA *)new_quest();
			}

			WNUM_LOAD load = fread_widevnum(fp, 0);
			ch->quest->questreceiver.pArea = get_area_from_uid(load.auid);
			ch->quest->questreceiver.vnum = load.vnum;

			fMatch = TRUE;
			break;
	    }

	    if (!str_cmp(word, "QOPart"))
	    {
	 	QUEST_PART_DATA *part;
		if (ch->quest == NULL)
		    ch->quest = (QUEST_DATA *)new_quest();

		part = (QUEST_PART_DATA *)new_quest_part();
		WNUM_LOAD wnum = fread_widevnum(fp, 0);
		part->area = get_area_from_uid(wnum.auid);
		part->mob = -1;
		part->obj = wnum.vnum;

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
		WNUM_LOAD wnum = fread_widevnum(fp, 0);
		part->area = get_area_from_uid(wnum.auid);
		part->obj_sac = wnum.vnum;

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
		WNUM_LOAD wnum = fread_widevnum(fp, 0);
		part->area = get_area_from_uid(wnum.auid);
		part->mob_rescue = wnum.vnum;

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
		WNUM_LOAD wnum = fread_widevnum(fp, 0);
		part->area = get_area_from_uid(wnum.auid);
		part->mob = wnum.vnum;

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
		WNUM_LOAD wnum = fread_widevnum(fp, 0);
		part->area = get_area_from_uid(wnum.auid);
		part->room = wnum.vnum;

		part->next = ch->quest->parts;
		ch->quest->parts = part;
		fMatch = TRUE;
		break;
	    }


	    if (!str_cmp(word, "QComplete"))
	    {
			if (ch->quest != NULL)
				ch->quest->parts->complete = TRUE;
		fMatch = TRUE;
	    }

            break;

	case 'R':
            if (!str_cmp(word, "RepopRoom")) {
				long auid = fread_number(fp);
				long vnum = fread_number(fp);
		location_set(&ch->recall,get_area_from_uid(auid),0,vnum,0,0);
		fMatch = TRUE;
	    }
            if (!str_cmp(word, "RepopRoomC")) {
				long auid = fread_number(fp);
				long vnum = fread_number(fp);
				unsigned long id0 = fread_number(fp);
				unsigned long id1 = fread_number(fp);

		location_set(&ch->recall,get_area_from_uid(auid),0,vnum,id0,id1);
		fMatch = TRUE;
	    }
            if (!str_cmp(word, "RepopRoomW")) {
				long wuid = fread_number(fp);
				long x = fread_number(fp);
				long y = fread_number(fp);
				long z = fread_number(fp);
		location_set(&ch->recall,NULL,wuid,x,y,z);
		fMatch = TRUE;
	    }
			if (!str_cmp(word, "Reputation"))
			{
				WNUM_LOAD wnum_load = fread_widevnum(fp, 0);

				REPUTATION_INDEX_DATA *repIndex = get_reputation_index_auid(wnum_load.auid, wnum_load.vnum);
				if (IS_VALID(repIndex))
				{
					REPUTATION_DATA *rep = new_reputation_data();

					rep->pIndexData = repIndex;
					rep->current_rank = fread_number(fp);
					rep->reputation = fread_number(fp);

					list_appendlink(ch->reputations, rep);
				}
				else
					fread_to_eol(fp);

				fMatch = true;
				break;
			}

            if (!str_cmp(word, "Room_before_arena")) {
				long auid = fread_number(fp);
				long vnum = fread_number(fp);
		location_set(&ch->pcdata->room_before_arena,get_area_from_uid(auid),0,vnum,0,0);
		fMatch = TRUE;
	    }
            if (!str_cmp(word, "Room_before_arenaC")) {
				long auid = fread_number(fp);
				long vnum = fread_number(fp);
				unsigned long id0 = fread_number(fp);
				unsigned long id1 = fread_number(fp);
		location_set(&ch->pcdata->room_before_arena,get_area_from_uid(auid),0,vnum,id0,id1);
		fMatch = TRUE;
	    }
            if (!str_cmp(word, "Room_before_arenaW")) {
				long wuid = fread_number(fp);
				long x = fread_number(fp);
				long y = fread_number(fp);
				long z = fread_number(fp);
		location_set(&ch->pcdata->room_before_arena,NULL,wuid,x,y,z);
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
			WNUM_LOAD w = fread_widevnum(fp, 0);
		ch->in_room = get_room_index_auid(w.auid, w.vnum);
		if ((ch->in_room == NULL) /*|| (ch->tot_level < 150 && !ch->in_room->area->open)*/)
		    ch->in_room = room_index_temple;
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
	    if( !str_cmp(word, "Ship") )
	    {
			unsigned long id1 = fread_number(fp);
			unsigned long id2 = fread_number(fp);

			SHIP_DATA *ship = find_ship_uid(id1, id2);

			if( IS_VALID(ship) )
			{
				list_appendlink(ch->pcdata->ships, ship);
			}

			fMatch = TRUE;
			break;
		}

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

		/*
	    if (!str_cmp(word, "Skill") || !str_cmp(word,"Sk"))
	    {
			SKILL_DATA *skill;
			int value;
			char *temp;
	        int prac;

			value = fread_number(fp);
			temp = fread_word(fp);
			if (ch->version < VERSION_PLAYER_002 && !str_cmp(temp,"wither"))
				skill = gsk_withering_cloud;
			else
				skill = get_skill_data(temp);
			if (!IS_VALID(skill))
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
				SKILL_ENTRY *entry;
				if( is_skill_spell(skill))
					entry = skill_entry_addskill(ch, skill, NULL, SKILLSRC_NORMAL, SKILL_AUTOMATIC);
				else
					entry = skill_entry_addspell(ch, skill, NULL, SKILLSRC_NORMAL, SKILL_AUTOMATIC);

				entry->rating = value;
			}

			fMatch = TRUE;
			break;
	    }

	    if (!str_cmp(word, "SkillMod") || !str_cmp(word, "SkMod"))
	    {
			SKILL_DATA *skill;
			int value;
			char *temp;

			value = fread_number(fp);
			temp = fread_word(fp) ;
			if (ch->version < VERSION_PLAYER_002 && !str_cmp(temp,"wither"))
				skill = gsk_withering_cloud;
			else
				skill = get_skill_data(temp);

			if (IS_VALID(skill))
			{
				SKILL_ENTRY *entry = skill_entry_findskill(ch->sorted_skills, skill);
				if (entry) entry->mod_rating = value;
			}

			fMatch = TRUE;
			break;
	    }


	    if (!str_cmp(word, "Song"))
	    {
			SONG_DATA *song;
			char *temp;

			temp = fread_word(fp);
			song = get_song_data(temp);
			if (!IS_VALID(song))
			{
				sprintf(buf, "fread_char: unknown song %s", temp);
				log_string(buf);
			}
			else
			{
				skill_entry_addsong(ch, song, NULL, SKILLSRC_NORMAL);
			}

			fMatch = TRUE;
			break;
	    }
		*/

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

			if (str_infix("$n", ch->pcdata->title))
			{
				free_string(ch->pcdata->title);
				ch->pcdata->title = str_dup("");	// No title
			}
			fMatch = TRUE;
			break;
	    }

	    break;

	case 'U':
		if(!str_cmp(word, "UnlockedArea"))
		{
			long auid = fread_number(fp);
			AREA_DATA *unlocked_area = get_area_from_uid(auid);

			if( unlocked_area )
			{
				// This will prevent duplication
				player_unlock_area(ch, unlocked_area);
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
			WNUM_LOAD wnum_load = fread_widevnum(fp, 0);

			ch->pIndexData = get_mob_index_auid(wnum_load.auid, wnum_load.vnum);

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
		if( !ch->quest->questgiver.pArea || ch->quest->questgiver.vnum < 1 )
		{
			// No questgiver info
			free_quest(ch->quest);
			ch->countdown = 0;
			ch->quest = NULL;
		}
		else
		{
			if( ch->quest->questgiver_type < 0 ) ch->quest->questgiver_type = QUESTOR_MOB;
			if( !ch->quest->questreceiver.pArea || ch->quest->questreceiver.vnum < 1 || ch->quest->questreceiver_type < 0 )
			{
				ch->quest->questreceiver = ch->quest->questgiver;
				ch->quest->questreceiver_type = QUESTOR_MOB;
			}
		}
	}
}

void fwrite_lock_state(FILE *fp, LOCK_STATE *lock)
{
	fprintf(fp, "#LOCK\n");
	fprintf(fp, "Key %s\n", widevnum_string_wnum(lock->key_wnum, NULL));
	fprintf(fp, "Flags %s\n", print_flags(lock->flags));
	fprintf(fp, "PickChance %d\n", lock->pick_chance);
				
	if (list_size(lock->special_keys) > 0)
	{
		LLIST_UID_DATA *luid;

		ITERATOR skit;
		iterator_start(&skit, lock->special_keys);
		while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&skit)) )
		{
			if (luid->id[0] > 0 && luid->id[1] > 0)
				fprintf(fp, "SpecialKey %lu %lu\n", luid->id[0], luid->id[1]);
		}

		iterator_stop(&skit);
	}

	fprintf(fp, "#-LOCK\n");
}

void fwrite_obj_multityping(FILE *fp, OBJ_DATA *obj)
{
	ITERATOR it;

	if (IS_BOOK(obj))
	{
		BOOK_DATA *book = BOOK(obj);
		BOOK_PAGE *page;

		fprintf(fp, "#TYPEBOOK\n");
		fprintf(fp, "Name %s~\n", fix_string(book->name));
		fprintf(fp, "Short %s~\n", fix_string(book->short_descr));
		fprintf(fp, "Flags %s\n", print_flags(book->flags));
		fprintf(fp, "CurrentPage %d\n", book->current_page);
		fprintf(fp, "OpenPage %d\n", book->open_page);

		iterator_start(&it, book->pages);
		while((page = (BOOK_PAGE *)iterator_nextdata(&it)))
		{
			fprintf(fp, "#PAGE %d\n", page->page_no);
			fprintf(fp, "Title %s~\n", fix_string(page->title));
			fprintf(fp, "Text %s~\n", fix_string(page->text));
			fprintf(fp, "#-PAGE\n");
		}
		iterator_stop(&it);

		if (book->lock)
			fwrite_lock_state(fp, book->lock);

		fprintf(fp, "#-TYPEBOOK\n");
	}

	if (IS_PAGE(obj))
	{
		fprintf(fp, "#TYPEPAGE %d\n", PAGE(obj)->page_no);
		fprintf(fp, "Title %s~\n", fix_string(PAGE(obj)->title));
		fprintf(fp, "Text %s~\n", fix_string(PAGE(obj)->text));
		if (PAGE(obj)->book.auid > 0 && PAGE(obj)->book.vnum > 0)
		{
			if (PAGE(obj)->book.auid == obj->pIndexData->area->uid)
				fprintf(fp, "Book #%ld\n", PAGE(obj)->book.vnum);
			else
				fprintf(fp, "Book %ld#%ld\n", PAGE(obj)->book.auid, PAGE(obj)->book.vnum);
		}
		fprintf(fp, "#-TYPEPAGE\n");
	}

	if (IS_CONTAINER(obj))
	{
		CONTAINER_FILTER *filter;

		fprintf(fp, "#TYPECONTAINER\n");
		fprintf(fp, "Name %s\n", fix_string(CONTAINER(obj)->name));
		fprintf(fp, "Short %s\n", fix_string(CONTAINER(obj)->short_descr));
		fprintf(fp, "Flags %s\n", print_flags(CONTAINER(obj)->flags));
		fprintf(fp, "MaxWeight %d\n", CONTAINER(obj)->max_weight);
		fprintf(fp, "WeightMultiplier %d\n", CONTAINER(obj)->weight_multiplier);
		fprintf(fp, "MaxVolume %d\n", CONTAINER(obj)->max_volume);

		iterator_start(&it, CONTAINER(obj)->whitelist);
		while((filter = (CONTAINER_FILTER *)iterator_nextdata(&it)))
		{
			char *subtype = "";
			if (filter->item_type == ITEM_WEAPON && filter->sub_type >= 0)
			{
				subtype = flag_string(weapon_class, filter->sub_type);
			}

			if (IS_NULLSTR(subtype))
				fprintf(fp, "Whitelist %s~\n",
					flag_string(type_flags, filter->item_type));
			else
				fprintf(fp, "Whitelist %s~ %s~\n",
					flag_string(type_flags, filter->item_type),
					subtype);
		}
		iterator_stop(&it);

		iterator_start(&it, CONTAINER(obj)->blacklist);
		while((filter = (CONTAINER_FILTER *)iterator_nextdata(&it)))
		{
			char *subtype = "";
			if (filter->item_type == ITEM_WEAPON && filter->sub_type >= 0)
			{
				subtype = flag_string(weapon_class, filter->sub_type);
			}

			if (IS_NULLSTR(subtype))
				fprintf(fp, "Blacklist %s~\n",
					flag_string(type_flags, filter->item_type));
			else
				fprintf(fp, "Blacklist %s~ %s~\n",
					flag_string(type_flags, filter->item_type),
					subtype);
		}
		iterator_stop(&it);

		if (CONTAINER(obj)->lock)
			fwrite_lock_state(fp, CONTAINER(obj)->lock);

		fprintf(fp, "#-TYPECONTAINER\n");
	}

	if (IS_FLUID_CON(obj))
	{
		fprintf(fp, "#TYPEFLUIDCONTAINER\n");
		fprintf(fp, "Name %s~\n", fix_string(FLUID_CON(obj)->name));
		fprintf(fp, "Short %s~\n", fix_string(FLUID_CON(obj)->short_descr));
		fprintf(fp, "Flags %s\n", print_flags(FLUID_CON(obj)->flags));

		if (IS_VALID(FLUID_CON(obj)->liquid))
			fprintf(fp, "Liquid %s~\n", FLUID_CON(obj)->liquid->name);
		
		fprintf(fp, "Amount %d\n", FLUID_CON(obj)->amount);
		fprintf(fp, "Capacity %d\n", FLUID_CON(obj)->capacity);
		fprintf(fp, "RefillRate %d\n", FLUID_CON(obj)->refill_rate);
		fprintf(fp, "Poison %d\n", FLUID_CON(obj)->poison);
		fprintf(fp, "PoisonRate %d\n", FLUID_CON(obj)->poison_rate);

		if (FLUID_CON(obj)->lock)
			fwrite_lock_state(fp, FLUID_CON(obj)->lock);

		ITERATOR sit;
		SPELL_DATA *spell;
		iterator_start(&sit, FLUID_CON(obj)->spells);
		while((spell = (SPELL_DATA *)iterator_nextdata(&sit)))
		{
			fprintf(fp, "Spell %s~ %d\n", spell->skill->name, spell->level);
		}
		iterator_stop(&sit);

		fprintf(fp, "#-TYPEFLUIDCONTAINER\n");
	}

	if (IS_FOOD(obj))
	{
		FOOD_BUFF_DATA *buff;

		fprintf(fp, "#TYPEFOOD\n");
		fprintf(fp, "Hunger %d\n", FOOD(obj)->hunger);
		fprintf(fp, "Fullness %d\n", FOOD(obj)->full);
		fprintf(fp, "Poison %d\n", FOOD(obj)->poison);

		iterator_start(&it, FOOD(obj)->buffs);
		while((buff = (FOOD_BUFF_DATA *)iterator_nextdata(&it)))
		{
			fprintf(fp, "#FOODBUFF %d\n", buff->where);

			fprintf(fp, "Location %d\n", buff->location);
			fprintf(fp, "Modifier %d\n", buff->modifier);
			fprintf(fp, "Level %d\n", UMAX(0, buff->level));
			fprintf(fp, "Duration %d\n", UMAX(0, buff->duration));

			if (buff->bitvector != 0)		fprintf(fp, "BitVector %s\n", print_flags(buff->bitvector));
			if (buff->bitvector2 != 0)	fprintf(fp, "BitVector2 %s\n", print_flags(buff->bitvector2));

			fprintf(fp, "#-FOODBUFF\n");
		}
		iterator_stop(&it);
		
		fprintf(fp, "#-TYPEFOOD\n");
	}

	if (IS_FURNITURE(obj))
	{
		FURNITURE_COMPARTMENT *compartment;

		fprintf(fp, "#TYPEFURNITURE\n");
		fprintf(fp, "MainCompartment %d\n", FURNITURE(obj)->main_compartment);

		iterator_start(&it, FURNITURE(obj)->compartments);
		while((compartment = (FURNITURE_COMPARTMENT *)iterator_nextdata(&it)))
		{
			fprintf(fp, "#COMPARTMENT\n");
			
			fprintf(fp, "Name %s~\n", fix_string(compartment->name));
			fprintf(fp, "ShortDesc %s~\n", fix_string(compartment->short_descr));
			fprintf(fp, "Description %s~\n", fix_string(compartment->description));

			fprintf(fp, "Flags %s\n", print_flags(compartment->flags));
			fprintf(fp, "MaxWeight %d\n", compartment->max_weight);
			fprintf(fp, "MaxOccupants %d\n", compartment->max_occupants);

			fprintf(fp, "Standing %s\n", print_flags(compartment->standing));
			fprintf(fp, "Hanging %s\n", print_flags(compartment->hanging));
			fprintf(fp, "Sitting %s\n", print_flags(compartment->sitting));
			fprintf(fp, "Resting %s\n", print_flags(compartment->resting));
			fprintf(fp, "Sleeping %s\n", print_flags(compartment->sleeping));

			fprintf(fp, "HealthRegen %d\n", compartment->health_regen);
			fprintf(fp, "ManaRegen %d\n", compartment->mana_regen);
			fprintf(fp, "MoveRegen %d\n", compartment->move_regen);
			
			if (compartment->lock)
				fwrite_lock_state(fp, compartment->lock);
			
			fprintf(fp, "#-COMPARTMENT\n");
		}
		iterator_stop(&it);

		fprintf(fp, "#-TYPEFURNITURE\n");
	}

	if (IS_INK(obj))
	{
		fprintf(fp, "#TYPEINK\n");
		for(int i = 0; i < MAX_INK_TYPES; i++)
		{
			fprintf(fp, "Type %d %s~ %d\n", i+1,
				flag_string(catalyst_types, INK(obj)->types[i]), INK(obj)->amounts[i]);
		}
		fprintf(fp, "#-TYPEINK\n");
	}

	if (IS_INSTRUMENT(obj))
	{
		fprintf(fp, "#TYPEINSTRUMENT\n");

		fprintf(fp, "Type %s~\n", flag_string(instrument_types, INSTRUMENT(obj)->type));
		fprintf(fp, "Flags %s\n", print_flags(INSTRUMENT(obj)->flags));
		fprintf(fp, "Beats %d %d\n", INSTRUMENT(obj)->beats_min, INSTRUMENT(obj)->beats_max);
		fprintf(fp, "Mana %d %d\n", INSTRUMENT(obj)->mana_min, INSTRUMENT(obj)->mana_max);

		for(int i = 0; i < INSTRUMENT_MAX_CATALYSTS; i++)
		{
			fprintf(fp, "Reservoir %d %s~ %d %d\n", i+1,
				flag_string(catalyst_types, INSTRUMENT(obj)->reservoirs[i].type),
				INSTRUMENT(obj)->reservoirs[i].amount, INSTRUMENT(obj)->reservoirs[i].capacity);
		}
		fprintf(fp, "#-TYPEINSTRUMENT\n");
	}

	if (IS_LIGHT(obj))
	{
		fprintf(fp, "#TYPELIGHT\n");
		fprintf(fp, "Flags %s\n", print_flags(LIGHT(obj)->flags));
		fprintf(fp, "Duration %d\n", LIGHT(obj)->duration);
		fprintf(fp, "#-TYPELIGHT\n");
	}

	if (IS_MONEY(obj))
	{
		fprintf(fp, "#TYPEMONEY\n");
		fprintf(fp, "Silver %d\n", MONEY(obj)->silver);
		fprintf(fp, "Gold %d\n", MONEY(obj)->gold);
		fprintf(fp, "#-TYPEMONEY\n");
	}

	if (IS_PORTAL(obj))
	{
		fprintf(fp, "#TYPEPORTAL\n");
		fprintf(fp, "Name %s~\n", fix_string(PORTAL(obj)->name));
		fprintf(fp, "Short %s~\n", fix_string(PORTAL(obj)->short_descr));
		fprintf(fp, "Exit %s\n", print_flags(PORTAL(obj)->exit));
		fprintf(fp, "Flags %s\n", print_flags(PORTAL(obj)->flags));
		fprintf(fp, "Type %s~\n", flag_string(portal_gatetype, PORTAL(obj)->type));

		for(int i = 0; i < MAX_PORTAL_VALUES; i++)
			fprintf(fp, "Param %d %ld\n", i, PORTAL(obj)->params[i]);

		if (PORTAL(obj)->lock)
			fwrite_lock_state(fp, PORTAL(obj)->lock);

		ITERATOR sit;
		SPELL_DATA *spell;
		iterator_start(&sit, SCROLL(obj)->spells);
		while((spell = (SPELL_DATA *)iterator_nextdata(&sit)))
		{
			fprintf(fp, "SpellNew %s~ %d %d\n", spell->skill->name, spell->level, spell->repop);
		}
		iterator_stop(&sit);

		fprintf(fp, "#-TYPEPORTAL\n");
	}

	if (IS_SCROLL(obj))
	{
		fprintf(fp, "#TYPESCROLL\n");

		fprintf(fp, "MaxMana %d\n", SCROLL(obj)->max_mana);
		fprintf(fp, "Flags %s\n", print_flags(SCROLL(obj)->flags));

		ITERATOR sit;
		SPELL_DATA *spell;
		iterator_start(&sit, SCROLL(obj)->spells);
		while((spell = (SPELL_DATA *)iterator_nextdata(&sit)))
		{
			fprintf(fp, "Spell %s~ %d\n", spell->skill->name, spell->level);
		}
		iterator_stop(&sit);

		fprintf(fp, "#-TYPESCROLL\n");
	}

	if (IS_TATTOO(obj))
	{
		fprintf(fp, "#TYPETATTOO\n");

		fprintf(fp, "Touches %d\n", TATTOO(obj)->touches);
		fprintf(fp, "FadeChance %d\n", TATTOO(obj)->fading_chance);
		fprintf(fp, "FadeRate %d\n", TATTOO(obj)->fading_rate);

		ITERATOR sit;
		SPELL_DATA *spell;
		iterator_start(&sit, TATTOO(obj)->spells);
		while((spell = (SPELL_DATA *)iterator_nextdata(&sit)))
		{
			fprintf(fp, "Spell %s~ %d\n", spell->skill->name, spell->level);
		}
		iterator_stop(&sit);

		fprintf(fp, "#-TYPETATTOO\n");
	}

	if (IS_WAND(obj))
	{
		fprintf(fp, "#TYPEWAND\n");

		fprintf(fp, "Charges %d\n", WAND(obj)->charges);
		fprintf(fp, "MaxCharges %d\n", WAND(obj)->max_charges);
		fprintf(fp, "Cooldown %d\n", WAND(obj)->cooldown);
		fprintf(fp, "RechargeTime %d\n", WAND(obj)->recharge_time);

		ITERATOR sit;
		SPELL_DATA *spell;
		iterator_start(&sit, WAND(obj)->spells);
		while((spell = (SPELL_DATA *)iterator_nextdata(&sit)))
		{
			fprintf(fp, "Spell %s~ %d\n", spell->skill->name, spell->level);
		}
		iterator_stop(&sit);

		fprintf(fp, "#-TYPEWAND\n");
	}
}


void fwrite_stache_obj(CHAR_DATA *ch, OBJ_DATA *obj, FILE *fp)
{
	if (list_size(obj->lstache) > 0)
	{
		fprintf(fp, "#STACHE\n");

		ITERATOR it;
		OBJ_DATA *item;
		iterator_start(&it, obj->lstache);
		while((item = (OBJ_DATA *)iterator_nextdata(&it)))
		{
			fwrite_obj_new(ch, item, fp, 0);
		}

		iterator_stop(&it);

		fprintf(fp, "#-STACHE\n");
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

    fprintf(fp, "Vnum %ld#%ld\n", obj->pIndexData->area->uid, obj->pIndexData->vnum);
    fprintf(fp, "UId %ld\n", obj->id[0]);
    fprintf(fp, "UId2 %ld\n", obj->id[1]);
    fprintf(fp, "Version %d\n", VERSION_OBJECT);
	fprintf(fp, "Persist %d\n", obj->persist);

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
    if (obj->extra[0] != obj->extra_perm[0])
        fprintf(fp, "ExtF %ld\n",	obj->extra[0]	    );
    if (obj->extra[1] != obj->extra_perm[1])
        fprintf(fp, "Ext2F %ld\n",	obj->extra[1]	    );
    if (obj->extra[2] != obj->extra_perm[2])
        fprintf(fp, "Ext3F %ld\n",	obj->extra[2]	    );
    if (obj->extra[3] != obj->extra_perm[3])
        fprintf(fp, "Ext4F %ld\n",	obj->extra[3]	    );
    if (obj->wear_flags != obj->pIndexData->wear_flags)
        fprintf(fp, "WeaF %d\n",	obj->wear_flags		    );
    if (obj->item_type != obj->pIndexData->item_type)
        fprintf(fp, "Ityp %d\n",	obj->item_type		    );
    if (obj->in_room != NULL)
    	fprintf(fp, "Room %s\n",	widevnum_string_room(obj->in_room, NULL)	    );
    if (IS_SET(obj->extra[1], ITEM_ENCHANTED))
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

	/*
    if (obj->lock) {
    	fprintf(fp, "Lock %s %d %d", widevnum_string_wnum(obj->lock->key_wnum, NULL), obj->lock->flags, obj->lock->pick_chance);

		if (list_size(obj->lock->keys) > 0)
		{
			LLIST_UID_DATA *luid;

			ITERATOR skit;
			iterator_start(&skit, obj->lock->keys);
			while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&skit)) )
			{
				if (luid->id[0] > 0 && luid->id[1] > 0)
					fprintf(fp, " %lu %lu", luid->id[0], luid->id[1]);
			}

			iterator_stop(&skit);
		}

		fprintf(fp, " 0\n");
	}
	*/

	// Permanent flags based
    fprintf(fp, "PermExtra %ld\n",	obj->extra_perm[0] );
    fprintf(fp, "PermExtra2 %ld\n",	obj->extra_perm[1] );
	fprintf(fp, "PermExtra3 %ld\n",	obj->extra_perm[2] );
	fprintf(fp, "PermExtra4 %ld\n",	obj->extra_perm[3] );
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
     ||  obj->value[7] != obj->pIndexData->value[7]
     ||  obj->value[8] != obj->pIndexData->value[8]
     ||  obj->value[9] != obj->pIndexData->value[9])
	{
    	fprintf(fp, "Val  %d %d %d %d %d %d %d %d %d %d\n",
	    obj->value[0], obj->value[1], obj->value[2], obj->value[3],
	    obj->value[4], obj->value[5], obj->value[6], obj->value[7],
		obj->value[8], obj->value[9] );
	}

	if (obj->spells != NULL)
		save_spell(fp, obj->spells);

	fwrite_obj_multityping(fp, obj);

    // This is for spells on the objects.
    for (paf = obj->affected; paf != NULL; paf = paf->next)
    {
        if (!IS_VALID(paf->skill) || paf->custom_name)
	    continue;

	if(paf->location >= APPLY_SKILL) {
		SKILL_DATA *skill = get_skill_data_uid(paf->location - APPLY_SKILL);
		if(!IS_VALID(skill)) continue;
		fprintf(fp, "Affcg '%s' %3d %3d %3d %3d %3d %3d '%s' %10ld %10ld\n",
			paf->skill->name,
			paf->where,
			paf->group,
			paf->level,
			paf->duration,
			paf->modifier,
			APPLY_SKILL,
			skill->name,
			paf->bitvector,
			paf->bitvector2
		);
	} else {
		fprintf(fp, "Affcg '%s' %3d %3d %3d %3d %3d %3d %10ld %10ld\n",
			paf->skill->name,
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

	if(paf->location >= APPLY_SKILL) {
		SKILL_DATA *skill = get_skill_data_uid(paf->location - APPLY_SKILL);
		if(!IS_VALID(skill)) continue;
		fprintf(fp, "Affcgn '%s' %3d %3d %3d %3d %3d %3d '%s' %10ld %10ld\n",
			paf->custom_name,
			paf->where,
			paf->group,
			paf->level,
			paf->duration,
			paf->modifier,
			APPLY_SKILL,
			skill->name,
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
	if (!IS_VALID(paf->skill) || paf->custom_name != NULL
        || ((paf->location < APPLY_SKILL) && !str_cmp(flag_string(apply_flags, paf->location), "none")))
	    continue;

	if(paf->location >= APPLY_SKILL) {
		SKILL_DATA *skill = get_skill_data_uid(paf->location - APPLY_SKILL);
		if(!IS_VALID(skill)) continue;
		fprintf(fp, "Affrg %3d %3d %3d %3d %3d %3d '%s' %10ld %10ld\n",
			paf->where,
			paf->group,
			paf->level,
			paf->duration,
			paf->modifier,
			APPLY_SKILL,
			skill->name,
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
				flag_string( catalyst_types, paf->catalyst_type ),
				1,
				paf->modifier,
				paf->duration);
		}
		else
		{
			fprintf(fp, "%s '%s' %3d %3d %3d %s\n",
				((paf->where == TO_CATALYST_ACTIVE) ? "CataNA" : "CataN"),
				flag_string( catalyst_types, paf->catalyst_type ),
				1,
				paf->modifier,
				paf->duration,
				paf->custom_name
				);
		}
    }

	if( obj->waypoints )
	{
		ITERATOR wit;
		WAYPOINT_DATA *wp;

		iterator_start(&wit, obj->waypoints);
		while( (wp = (WAYPOINT_DATA *)iterator_nextdata(&wit)) )
		{
			fprintf(fp, "MapWaypoint %lu %d %d %s~\n", wp->w, wp->x, wp->y, fix_string(wp->name));
		}
		iterator_stop(&wit);
	}


    for (ed = obj->extra_descr; ed != NULL; ed = ed->next)
    {
		if( ed->description )
			fprintf(fp, "ExDe %s~ %s~\n", ed->keyword, ed->description);
		else
			fprintf(fp, "ExDeEnv %s~\n", ed->keyword);
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

	fwrite_stache_obj(ch, obj, fp);
}

LOCK_STATE *fread_lock_state(FILE *fp)
{
	LOCK_STATE *lock;
	char buf[MSL];
    char *word;
	bool fMatch = FALSE;

	lock = new_lock_state();

    while (str_cmp((word = fread_word(fp)), "#-LOCK"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
			case 'F':
				KEY("Flags", lock->flags, fread_flag(fp));
				break;

			case 'K':
				if (!str_cmp(word, "Key"))
				{
					WNUM_LOAD key_load = fread_widevnum(fp, 0);

					WNUM wnum;
					wnum.pArea = get_area_from_uid(key_load.auid);
					wnum.vnum = key_load.vnum;
					
					lock->key_wnum = wnum;
				}
				break;

			case 'P':
				KEY("PickChance", lock->pick_chance, fread_flag(fp));
				break;

			case 'S':
				if (!str_cmp(word, "SpecialKey"))
				{
					unsigned long id0 = fread_number(fp);
					unsigned long id1 = fread_number(fp);

					LLIST_UID_DATA *luid = new_list_uid_data();
					luid->ptr = NULL;	// Resolve it later
					luid->id[0] = id0;
					luid->id[1] = id1;

					list_appendlink(lock->special_keys, luid);
					fMatch = TRUE;
					break;
				}
				break;
		}


		if (!fMatch) {
			sprintf(buf, "fread_lock_state: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return lock;
}

BOOK_PAGE *fread_book_page(FILE *fp, char *closer)
{
	BOOK_PAGE *page = new_book_page();
	char buf[MSL];
	char *word;
	bool fMatch;

	page->page_no = fread_number(fp);

	while(str_cmp((word = fread_word(fp)), closer))
	{
		fMatch = FALSE;

		switch(word[0])
		{
			case 'B':
				KEY("Book", page->book, fread_widevnum(fp, 0));
				break;

			case 'T':
				KEYS("Title", page->title, fread_string(fp));
				KEYS("Text", page->text, fread_string(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "fread_book_page: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return page;
}

BOOK_DATA *fread_obj_book_data(FILE *fp)
{
	BOOK_DATA *book = new_book_data();
	char buf[MSL];
	char *word;
	bool fMatch;

	while(str_cmp((word = fread_word(fp)), "#-TYPEBOOK"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#LOCK"))
				{
					book->lock = fread_lock_state(fp);
					fMatch = TRUE;
					break;
				}
				if (!str_cmp(word, "#PAGE"))
				{
					BOOK_PAGE *page = fread_book_page(fp, "#-PAGE");

					if (!book_insert_page(book, page))
					{
						sprintf(buf, "fread_obj_book_data: page with duplicate page number (%d) found!  Discarding.", page->page_no);
						bug(buf, 0);
						free_book_page(page);	
					}
					fMatch = TRUE;
					break;
				}
				break;
			
			case 'C':
				KEY("CurrentPage", book->current_page, fread_number(fp));
				break;

			case 'F':
				KEY("Flags", book->flags, fread_flag(fp));
				break;

			case 'N':
				KEYS("Name", book->name, fread_string(fp));
				break;

			case 'O':
				KEY("OpenPage", book->open_page, fread_number(fp));
				break;
			
			case 'S':
				KEYS("Short", book->short_descr, fread_string(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "fread_obj_book_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return book;
}


CONTAINER_DATA *fread_obj_container_data(FILE *fp)
{
	CONTAINER_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch = FALSE;

	data = new_container_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPECONTAINER"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#LOCK"))
				{
					if (data->lock) free_lock_state(data->lock);

					data->lock = fread_lock_state(fp);
					fMatch = TRUE;
					break;
				}
				break;

			case 'B':
				if (!str_cmp(word, "Blacklist"))
				{
					int item_type = stat_lookup(fread_string(fp), type_flags, NO_FLAG);
					int sub_type = -1;

					if (item_type != NO_FLAG)
					{
						if (item_type == ITEM_WEAPON)
						{
							sub_type = stat_lookup(fread_string(fp), weapon_class, -1);
						}

						CONTAINER_FILTER *filter = new_container_filter();
						filter->item_type = item_type;
						filter->sub_type = sub_type;

						list_appendlink(data->blacklist, filter);
					}

					fMatch = TRUE;
					break;
				}
				break;

			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'M':
				KEY("MaxWeight", data->max_weight, fread_number(fp));
				KEY("MaxVolume", data->max_volume, fread_number(fp));
				break;
			
			case 'N':
				KEYS("Name", data->name, fread_string(fp));
				break;
			
			case 'S':
				KEYS("Short", data->short_descr, fread_string(fp));
				break;

			case 'W':
				KEY("WeightMultiplier", data->weight_multiplier, fread_number(fp));
				if (!str_cmp(word, "Whitelist"))
				{
					int item_type = stat_lookup(fread_string(fp), type_flags, NO_FLAG);
					int sub_type = -1;

					if (item_type != NO_FLAG)
					{
						if (item_type == ITEM_WEAPON)
						{
							sub_type = stat_lookup(fread_string(fp), weapon_class, -1);
						}

						CONTAINER_FILTER *filter = new_container_filter();
						filter->item_type = item_type;
						filter->sub_type = sub_type;

						list_appendlink(data->whitelist, filter);
					}

					fMatch = TRUE;
					break;
				}
				break;
		}

		if (!fMatch) {
			sprintf(buf, "fread_obj_container_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

FLUID_CONTAINER_DATA *fread_obj_fluid_container_data(FILE *fp)
{
	FLUID_CONTAINER_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_fluid_container_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEFLUIDCONTAINER"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#LOCK"))
				{
					if (data->lock) free_lock_state(data->lock);

					data->lock = fread_lock_state(fp);
					fMatch = TRUE;
					break;
				}
				break;

			case 'A':
				KEY("Amount", data->amount, fread_number(fp));
				break;

			case 'C':
				KEY("Capacity", data->capacity, fread_number(fp));
				break;

			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'L':
				if (!str_cmp(word, "Liquid"))
				{
					data->liquid = liquid_lookup(fread_string(fp));
					fMatch = true;
					break;
				}
				break;

			case 'N':
				KEYS("Name", data->name, fread_string(fp));
				break;

			case 'P':
				KEY("Poison", data->poison, fread_number(fp));
				KEY("PoisonRate", data->poison_rate, fread_number(fp));
				break;

			case 'R':
				KEY("RefillRate", data->refill_rate, fread_number(fp));
				break;

			case 'S':
				KEYS("Short", data->short_descr, fread_string(fp));

				if (!str_cmp(word, "Spell"))
				{
					char *name = fread_string(fp);
					int level = fread_number(fp);

					SKILL_DATA *skill = get_skill_data(name);
					if (IS_VALID(skill) && is_skill_spell(skill))
					{
						SPELL_DATA *spell = new_spell();
						spell->skill = skill;
						spell->level = level;
						spell->repop = 100;
						spell->next = NULL;

						list_appendlink(data->spells, spell);
					}

					fMatch = true;
					break;
				}
				break;
		}

		if (!fMatch) {
			sprintf(buf, "fread_obj_fluid_container_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}


FOOD_BUFF_DATA *fread_food_buff(FILE *fp)
{
	FOOD_BUFF_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_food_buff_data();

	data->where = fread_number(fp);

    while (str_cmp((word = fread_word(fp)), "#-FOODBUFF"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
			case 'B':
				KEY("Bitvector", data->bitvector, fread_flag(fp));
				KEY("Bitvector2", data->bitvector2, fread_flag(fp));
				break;

			case 'D':
				KEY("Duration", data->duration, fread_number(fp));
				break;

			case 'L':
				KEY("Level", data->level, fread_number(fp));
				KEY("Location", data->location, fread_number(fp));
				break;

			case 'M':
				KEY("Modifier", data->modifier, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_food_buff: no match for word %s", word);
			bug(buf, 0);
		}
	}

	// Correct for possible file edits
	data->level = UMAX(0, data->level);
	data->duration = UMAX(0, data->duration);

	return data;
}

FOOD_DATA *fread_obj_food_data(FILE *fp)
{
	FOOD_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_food_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEFOOD"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#FOODBUFF"))
				{
					FOOD_BUFF_DATA *buff = fread_food_buff(fp);

					list_appendlink(data->buffs, buff);

					fMatch = TRUE;
				}
				break;

			case 'F':
				KEY("Fullness", data->full, fread_number(fp));
				break;

			case 'H':
				KEY("Hunger", data->hunger, fread_number(fp));
				break;

			case 'P':
				KEY("Poison", data->poison, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "fread_obj_food_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

FURNITURE_COMPARTMENT *fread_furniture_compartment(FILE *fp)
{
	FURNITURE_COMPARTMENT *data = NULL;

	char buf[MSL];
    char *word;

	data = new_furniture_compartment();

    while (str_cmp((word = fread_word(fp)), "#-COMPARTMENT"))
	{
		bool fMatch = FALSE;
		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#LOCK"))
				{
					if (data->lock) free_lock_state(data->lock);

					data->lock = fread_lock_state(fp);
					fMatch = TRUE;
					break;
				}
				break;

			case 'D':
				KEYS("Description", data->description, fread_string(fp));
				break;

			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'H':
				KEY("Hanging", data->hanging, fread_flag(fp));
				KEY("HealthRegen", data->health_regen, fread_number(fp));
				break;


			case 'M':
				KEY("ManaRegen", data->mana_regen, fread_number(fp));
				KEY("MaxOccupants", data->max_occupants, fread_number(fp));
				KEY("MaxWeight", data->max_weight, fread_number(fp));
				KEY("MoveRegen", data->move_regen, fread_number(fp));
				break;

			case 'N':
				KEYS("Name", data->name, fread_string(fp));
				break;

			case 'R':
				KEY("Resting", data->resting, fread_flag(fp));
				break;
			
			case 'S':
				KEYS("ShortDesc", data->short_descr, fread_string(fp));
				KEY("Sitting", data->sitting, fread_flag(fp));
				KEY("Sleeping", data->sleeping, fread_flag(fp));
				KEY("Standing", data->standing, fread_flag(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "fread_furniture_compartment: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

FURNITURE_DATA *fread_obj_furniture_data(FILE *fp)
{
	FURNITURE_DATA *data = NULL;

	char buf[MSL];
    char *word;

	data = new_furniture_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEFURNITURE"))
	{
		bool fMatch = FALSE;
		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#COMPARTMENT"))
				{
					FURNITURE_COMPARTMENT *compartment = fread_furniture_compartment(fp);
					list_appendlink(data->compartments, compartment);

					compartment->ordinal = list_size(data->compartments);

					fMatch = TRUE;
					break;
				}
				break;

			case 'M':
				KEY("MainCompartment", data->main_compartment, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "fread_obj_furniture_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

INK_DATA *fread_obj_ink_data(FILE *fp)
{
	INK_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_ink_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEINK"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
			case 'T':
				if (!str_cmp(word, "Type"))
				{
					int index = fread_number(fp);
					char *name = fread_string(fp);
					int amount = fread_number(fp);

					if (index >= 1 && index <= MAX_INK_TYPES)
					{
						int type = stat_lookup(name, catalyst_types, CATALYST_NONE);
						if (type != CATALYST_NONE)
						{
							data->types[index-1] = type;
							data->amounts[index-1] = amount;
						}
					}

					fMatch = true;
					break;
				}
				break;
		}

		if (!fMatch) {
			sprintf(buf, "fread_obj_ink_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}


INSTRUMENT_DATA *fread_obj_instrument_data(FILE *fp)
{
	INSTRUMENT_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_instrument_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEINSTRUMENT"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
			case 'B':
				if (!str_cmp(word, "Beats"))
				{
					data->beats_min = fread_number(fp);
					data->beats_max = fread_number(fp);

					fMatch = true;
					break;					
				}
				break;

			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'M':
				if (!str_cmp(word, "mana"))
				{
					data->mana_min = fread_number(fp);
					data->mana_max = fread_number(fp);

					fMatch = true;
					break;					
				}
				break;

			case 'R':
				if (!str_cmp(word, "Reservoir"))
				{
					int index = fread_number(fp);
					char *name = fread_string(fp);
					int amount = fread_number(fp);
					int capacity = fread_number(fp);

					if (index >= 1 && index <= INSTRUMENT_MAX_CATALYSTS)
					{
						int type = stat_lookup(name, catalyst_types, CATALYST_NONE);
						if (type != CATALYST_NONE)
						{
							data->reservoirs[index - 1].type = type;
							data->reservoirs[index - 1].amount = amount;
							data->reservoirs[index - 1].capacity = capacity;
						}
					}

					fMatch = true;
					break;
				}
				break;

			case 'T':
				KEY("Type", data->type, stat_lookup(fread_string(fp), catalyst_types, CATALYST_NONE));
				break;

		}

		if (!fMatch) {
			sprintf(buf, "fread_obj_instrument_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}


LIGHT_DATA *fread_obj_light_data(FILE *fp)
{
	LIGHT_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_light_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPELIGHT"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
			case 'D':
				KEY("Duration", data->duration, fread_number(fp));
				break;
			
			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "fread_obj_light_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

MONEY_DATA *fread_obj_money_data(FILE *fp)
{
	MONEY_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_money_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEMONEY"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
			case 'G':
				KEY("Gold", data->gold, fread_number(fp));
				break;

			case 'S':
				KEY("Silver", data->silver, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "fread_obj_money_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

PORTAL_DATA *fread_obj_portal_data(FILE *fp)
{
	PORTAL_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_portal_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEPORTAL"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#LOCK"))
				{
					if (!data->lock) free_lock_state(data->lock);
					data->lock = fread_lock_state(fp);
					fMatch = TRUE;
					break;
				}
				break;

			case 'E':
				KEY("Exit", data->exit, fread_flag(fp));
				break;

			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'N':
				KEYS("Name", data->name, fread_string(fp));
				break;

			case 'P':
				if (!str_cmp(word, "Param"))
				{
					int p = fread_number(fp);
					long v = fread_number(fp);

					if (p >= 0 && p < MAX_PORTAL_VALUES)
					{
						data->params[p] = v;
					}
					else
					{
						// Complain
					}

					fMatch = TRUE;
					break;
				}
				break;

			case 'S':
				KEYS("Short", data->short_descr, fread_string(fp));

				if (!str_cmp(word, "SpellNew"))
				{
					SKILL_DATA *skill = get_skill_data(fread_string(fp));

					fMatch = TRUE;
					if (IS_VALID(skill) && is_skill_spell(skill))
					{
						SPELL_DATA *spell = new_spell();
						spell->skill = skill;
						spell->level = fread_number(fp);
						spell->repop = fread_number(fp);
						spell->next = NULL;

						list_appendlink(data->spells, spell);
					}
					else
					{
						// Complain
					}
				}

				break;

			case 'T':
				if (!str_cmp(word, "Type"))
				{
					data->type = flag_lookup(fread_string(fp), portal_gatetype);

					fMatch = TRUE;
					break;
				}
				break;
		}

		if (!fMatch) {
			sprintf(buf, "fread_obj_portal_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

SCROLL_DATA *fread_obj_scroll_data(FILE *fp)
{
	SCROLL_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_scroll_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPESCROLL"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'M':
				KEY("MaxMana", data->max_mana, fread_number(fp));
				break;

			case 'S':
				if (!str_cmp(word, "Spell"))
				{
					char *name = fread_string(fp);
					int level = fread_number(fp);

					SKILL_DATA *skill = get_skill_data(name);
					if (IS_VALID(skill) && is_skill_spell(skill))
					{
						SPELL_DATA *spell = new_spell();
						spell->skill = skill;
						spell->level = level;
						spell->repop = 100;
						spell->next = NULL;

						list_appendlink(data->spells, spell);
					}

					fMatch = true;
					break;
				}
				break;
		}

		if (!fMatch) {
			sprintf(buf, "fread_obj_scroll_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

TATTOO_DATA *fread_obj_tattoo_data(FILE *fp)
{
	TATTOO_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_tattoo_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPETATTOO"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
			case 'F':
				KEY("FadeChance", data->fading_chance, fread_number(fp));
				KEY("FadeRate", data->fading_rate, fread_number(fp));
				break;

			case 'S':
				if (!str_cmp(word, "Spell"))
				{
					char *name = fread_string(fp);
					int level = fread_number(fp);

					SKILL_DATA *skill = get_skill_data(name);
					if (IS_VALID(skill) && is_skill_spell(skill))
					{
						SPELL_DATA *spell = new_spell();
						spell->skill = skill;
						spell->level = level;
						spell->repop = 100;
						spell->next = NULL;

						list_appendlink(data->spells, spell);
					}

					fMatch = true;
					break;
				}
				break;

			case 'T':
				KEY("Touches", data->touches, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "fread_obj_tattoo_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

WAND_DATA *fread_obj_wand_data(FILE *fp)
{
	WAND_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_wand_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEWAND"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
			case 'C':
				KEY("Charges", data->charges, fread_number(fp));
				KEY("Cooldown", data->cooldown, fread_number(fp));
				break;

			case 'M':
				KEY("MaxCharges", data->max_charges, fread_number(fp));
				break;

			case 'R':
				KEY("RechargeTime", data->recharge_time, fread_number(fp));
				break;

			case 'S':
				if (!str_cmp(word, "Spell"))
				{
					char *name = fread_string(fp);
					int level = fread_number(fp);

					SKILL_DATA *skill = get_skill_data(name);
					if (IS_VALID(skill) && is_skill_spell(skill))
					{
						SPELL_DATA *spell = new_spell();
						spell->skill = skill;
						spell->level = level;
						spell->repop = 100;
						spell->next = NULL;

						list_appendlink(data->spells, spell);
					}

					fMatch = true;
					break;
				}
				break;
		}

		if (!fMatch) {
			sprintf(buf, "fread_obj_wand_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}




void fread_obj_reset_multityping(OBJ_DATA *obj)
{
	free_book_data(BOOK(obj));				BOOK(obj) = NULL;
	free_container_data(CONTAINER(obj));	CONTAINER(obj) = NULL;
	free_fluid_container_data(FLUID_CON(obj));	FLUID_CON(obj) = NULL;
	free_food_data(FOOD(obj));				FOOD(obj) = NULL;
	free_furniture_data(FURNITURE(obj));	FURNITURE(obj) = NULL;
	free_ink_data(INK(obj));				INK(obj) = NULL;
	free_instrument_data(INSTRUMENT(obj));	INSTRUMENT(obj) = NULL;
	free_light_data(LIGHT(obj));			LIGHT(obj) = NULL;
	free_money_data(MONEY(obj));			MONEY(obj) = NULL;
	free_book_page(PAGE(obj));				PAGE(obj) = NULL;
	free_portal_data(PORTAL(obj));			PORTAL(obj) = NULL;
	free_scroll_data(SCROLL(obj));			SCROLL(obj) = NULL;
	free_tattoo_data(TATTOO(obj));			TATTOO(obj) = NULL;
	free_wand_data(WAND(obj));				WAND(obj) = NULL;
}

void fread_obj_check_version(OBJ_DATA *obj, long values[MAX_OBJVALUES])
{
	if (obj->version < VERSION_OBJECT_005)
	{
		if (obj->item_type == ITEM_MONEY)
		{
			if (!IS_MONEY(obj)) MONEY(obj) = new_money_data();

			MONEY(obj)->silver = values[0];
			MONEY(obj)->gold = values[1];
		}
	}

	if (obj->version < VERSION_OBJECT_006)
	{
		if (obj->item_type == ITEM_FOOD)
		{
			if (!IS_FOOD(obj)) FOOD(obj) = new_food_data();

			FOOD(obj)->full = values[0];
			FOOD(obj)->hunger = values[1];
			FOOD(obj)->poison = values[3];

			// No food buffs
		}
	}

	if (obj->version < VERSION_OBJECT_007)
	{
		if (obj->item_type == ITEM_CONTAINER)
		{
			if (!IS_CONTAINER(obj)) CONTAINER(obj) = new_container_data();

			CONTAINER(obj)->max_weight = values[0];
			CONTAINER(obj)->flags = values[1];
			CONTAINER(obj)->max_volume = values[3];
			CONTAINER(obj)->weight_multiplier = values[4];

			CONTAINER(obj)->lock = obj->lock;
			obj->lock = NULL;
		}
		else if (obj->item_type == ITEM_WEAPON_CONTAINER)
		{
			obj->item_type = ITEM_CONTAINER;
			if (!IS_CONTAINER(obj)) CONTAINER(obj) = new_container_data();

			CONTAINER(obj)->max_weight = values[0];
			CONTAINER(obj)->flags = 0;
			CONTAINER(obj)->max_volume = values[3];
			CONTAINER(obj)->weight_multiplier = values[4];

			CONTAINER_FILTER *filter = new_container_filter();
			filter->item_type = ITEM_WEAPON;
			filter->sub_type = values[1];
			list_appendlink(CONTAINER(obj)->whitelist, filter);

			CONTAINER(obj)->lock = obj->lock;
			obj->lock = NULL;
		}
		else if (obj->item_type == ITEM_KEYRING)
		{
			obj->item_type = ITEM_CONTAINER;
			if (!IS_CONTAINER(obj)) CONTAINER(obj) = new_container_data();

			CONTAINER(obj)->max_weight = -1;	// No weight limit
			CONTAINER(obj)->flags = CONT_SINGULAR | CONT_NO_DUPLICATES;
			CONTAINER(obj)->max_volume = 50;
			CONTAINER(obj)->weight_multiplier = 100;

			CONTAINER_FILTER *filter = new_container_filter();
			filter->item_type = ITEM_KEY;
			filter->sub_type = -1;
			list_appendlink(CONTAINER(obj)->whitelist, filter);

			CONTAINER(obj)->lock = obj->lock;
			obj->lock = NULL;
		}
	}

	if (obj->version < VERSION_OBJECT_008)
	{
		if (obj->item_type == ITEM_LIGHT)
		{
			if (!IS_LIGHT(obj)) LIGHT(obj) = new_light_data();

			if (values[2] > 0)
			{
				LIGHT(obj)->flags = LIGHT_REMOVE_ON_EXTINGUISH;
				LIGHT(obj)->duration = values[2];
			}
			else if(values[2] < 0)
				LIGHT(obj)->duration = -1;
		}
	}

	if (obj->version < VERSION_OBJECT_009)
	{
		if (obj->item_type == ITEM_FURNITURE)
		{
			if (!IS_FURNITURE(obj)) FURNITURE(obj) = new_furniture_data();

			FURNITURE_COMPARTMENT *compartment = new_furniture_compartment();

			// Compartment copies the string data of the object itself.
			free_string(compartment->name);
			compartment->name = str_dup(obj->name);
			free_string(compartment->short_descr);
			compartment->short_descr = str_dup(obj->short_descr);
			free_string(compartment->description);
			compartment->description = str_dup(obj->full_description);

			compartment->max_occupants = values[0];
			compartment->max_weight = values[1];

			if (IS_SET(values[2], STAND_AT)) SET_BIT(compartment->standing, FURNITURE_AT);
			if (IS_SET(values[2], STAND_ON)) SET_BIT(compartment->standing, FURNITURE_ON);
			if (IS_SET(values[2], STAND_IN)) SET_BIT(compartment->standing, FURNITURE_IN);
			if (IS_SET(values[2], SIT_AT)) SET_BIT(compartment->sitting, FURNITURE_AT);
			if (IS_SET(values[2], SIT_ON)) SET_BIT(compartment->sitting, FURNITURE_ON);
			if (IS_SET(values[2], SIT_IN)) SET_BIT(compartment->sitting, FURNITURE_IN);
			if (IS_SET(values[2], REST_AT)) SET_BIT(compartment->resting, FURNITURE_AT);
			if (IS_SET(values[2], REST_ON)) SET_BIT(compartment->resting, FURNITURE_ON);
			if (IS_SET(values[2], REST_IN)) SET_BIT(compartment->resting, FURNITURE_IN);
			if (IS_SET(values[2], SLEEP_AT)) SET_BIT(compartment->sleeping, FURNITURE_AT);
			if (IS_SET(values[2], SLEEP_ON)) SET_BIT(compartment->sleeping, FURNITURE_ON);
			if (IS_SET(values[2], SLEEP_IN)) SET_BIT(compartment->sleeping, FURNITURE_IN);

			compartment->health_regen = values[3];
			compartment->mana_regen = values[4];
			compartment->move_regen = values[5];

			list_appendlink(FURNITURE(obj)->compartments, compartment);
			FURNITURE(obj)->main_compartment = 1;

			compartment->lock = obj->lock;
			obj->lock = NULL;
		}
	}

	if (obj->version < VERSION_OBJECT_010)
	{
		if (obj->item_type == ITEM_PORTAL)
		{
			if (!IS_PORTAL(obj)) PORTAL(obj) = new_portal_data();

			free_string(PORTAL(obj)->name);
			PORTAL(obj)->name = str_dup(obj->name);
			free_string(PORTAL(obj)->short_descr);
			PORTAL(obj)->short_descr = str_dup(obj->short_descr);

			PORTAL(obj)->charges = values[0];
			PORTAL(obj)->exit = values[1];
			PORTAL(obj)->flags = values[2];
			PORTAL(obj)->type = values[3];

			for(int i = 0; i < MAX_PORTAL_VALUES; i++)
				PORTAL(obj)->params[i] = values[5 + i];

			PORTAL(obj)->lock = obj->lock;
			obj->lock = NULL;

			SPELL_DATA *spell, *spell_next;
			for(spell = obj->spells; spell; spell = spell_next)
			{
				spell_next = spell->next;
				spell->next = NULL;
				list_appendlink(PORTAL(obj)->spells, spell);
			}
			obj->spells = NULL;
		}
	}

	if (obj->version < VERSION_OBJECT_011)
	{
		if (obj->item_type == ITEM_BOOK)
		{
			if(!IS_BOOK(obj)) BOOK(obj) = new_book_data();

			free_string(BOOK(obj)->name);
			BOOK(obj)->name = str_dup(obj->name);
			free_string(BOOK(obj)->short_descr);
			BOOK(obj)->short_descr = str_dup(obj->short_descr);

			BOOK(obj)->flags = values[1] & (BOOK_CLOSEABLE | BOOK_CLOSED | BOOK_CLOSELOCK | BOOK_PUSHOPEN);

			BOOK(obj)->lock = obj->lock;
			obj->lock = NULL;
		}
	}

	if (obj->version < VERSION_OBJECT_013)
	{
		if (obj->item_type == ITEM_INK)
		{
			// This will only ever have atmost THREE different catalyst types from the old format

			if (IS_INK(obj)) free_ink_data(INK(obj));

			INK(obj) = new_ink_data();
			sh_int amounts[CATALYST_MAX];

			// Tally up how much is on each type
			// Example:
			// v0  air
			// v1  air
			// v2  water
			//
			// Should tally up to 2 air and 1 water
			for(int i = 0; i < CATALYST_MAX; i++) amounts[i] = 0;
			for(int i = 0; i < 3; i++)
				if (values[i] > CATALYST_NONE && values[i] < CATALYST_MAX)
					amounts[values[i]]++;

			// Copy over tallies
			int n = 0;
			for(int i = 0; i < CATALYST_MAX && n < 3; i++)
			{
				if (amounts[i] > 0)
				{
					INK(obj)->types[n] = i;
					INK(obj)->amounts[n] = amounts[i];
				}
			}
		}
		else if (obj->item_type == ITEM_INSTRUMENT)
		{
			if (IS_INSTRUMENT(obj)) free_instrument_data(INSTRUMENT(obj));

			INSTRUMENT(obj) = new_instrument_data();

			INSTRUMENT(obj)->type = values[0];
			INSTRUMENT(obj)->flags = values[1];
			INSTRUMENT(obj)->beats_min = values[2];
			INSTRUMENT(obj)->beats_max = values[3];
		}
	}

	for(int i = 0; i < MAX_OBJVALUES; i++)
		obj->value[i] = values[i];
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
	long values[MAX_OBJVALUES];

	fVnum = FALSE;
	obj = NULL;
	first = TRUE;  /* used to counter fp offset */
	make_new = FALSE;

	word   = feof(fp) ? "End" : fread_word(fp);
	if (!str_cmp(word,"Vnum"))
	{
		WNUM_LOAD w = fread_widevnum(fp, 0);
		first = FALSE;  /* fp will be in right place */

		OBJ_INDEX_DATA *index = get_obj_index_auid(w.auid, w.vnum);
		if ( index != NULL)
			obj = create_object_noid(index,-1, FALSE, FALSE);

	}

	if (obj == NULL)  /* either not found or old style */
	{
		obj = new_obj();
		obj->name		= str_dup("");
		obj->short_descr	= str_dup("");
		obj->description	= str_dup("");
	}

	for(int i = 0; i < MAX_OBJVALUES; i++)
		values[i] = obj->value[i];

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
			if (!str_cmp(word, "#STACHE"))
			{
				fread_stache(fp, obj->lstache);
				fMatch = true;
				break;
			}

			if (!str_cmp(word, "#TOKEN"))
			{
				TOKEN_DATA *token = fread_token(fp);
				token_to_obj(token, obj);
				fMatch		= TRUE;
				break;
			}
			if (!str_cmp(word, "#TYPEBOOK"))
			{
				if (IS_BOOK(obj)) free_book_data(BOOK(obj));

				BOOK(obj) = fread_obj_book_data(fp);
				fMatch = TRUE;
				break;
			}
			if (!str_cmp(word, "#TYPECONTAINER"))
			{
				if (IS_CONTAINER(obj)) free_container_data(CONTAINER(obj));

				CONTAINER(obj) = fread_obj_container_data(fp);
				fMatch = TRUE;
				break;
			}
			if (!str_cmp(word, "#TYPEFLUIDCONTAINER"))
			{
				if (IS_FLUID_CON(obj)) free_fluid_container_data(FLUID_CON(obj));

				FLUID_CON(obj) = fread_obj_fluid_container_data(fp);
				fMatch = TRUE;
				break;
			}
			if (!str_cmp(word, "#TYPEFOOD"))
			{
				if (IS_FOOD(obj)) free_food_data(FOOD(obj));

				FOOD(obj) = fread_obj_food_data(fp);
				fMatch = TRUE;
				break;
			}
			if (!str_cmp(word, "#TYPEFURNITURE"))
			{
				if (IS_FURNITURE(obj)) free_furniture_data(FURNITURE(obj));

				FURNITURE(obj) = fread_obj_furniture_data(fp);

				fMatch = TRUE;
				break;
			}
			if (!str_cmp(word, "#TYPEINK"))
			{
				if (IS_INK(obj)) free_ink_data(INK(obj));

				INK(obj) = fread_obj_ink_data(fp);
				fMatch = TRUE;
				break;
			}
			if (!str_cmp(word, "#TYPEINSTRUMENT"))
			{
				if (IS_INSTRUMENT(obj)) free_instrument_data(INSTRUMENT(obj));

				INSTRUMENT(obj) = fread_obj_instrument_data(fp);
				fMatch = TRUE;
				break;
			}
			if (!str_cmp(word, "#TYPELIGHT"))
			{
				if (IS_LIGHT(obj)) free_light_data(LIGHT(obj));

				LIGHT(obj) = fread_obj_light_data(fp);
				fMatch = TRUE;
				break;
			}
			if (!str_cmp(word, "#TYPEMONEY"))
			{
				if (IS_MONEY(obj)) free_money_data(MONEY(obj));

				MONEY(obj) = fread_obj_money_data(fp);
				fMatch = TRUE;
				break;
			}
			if (!str_cmp(word, "#TYPEPAGE"))
			{
				if (IS_PAGE(obj)) free_book_page(PAGE(obj));

				PAGE(obj) = fread_book_page(fp, "#-TYPEPAGE");
				fMatch = TRUE;
				break;
			}
			if (!str_cmp(word, "#TYPEPORTAL"))
			{
				if (IS_PORTAL(obj)) free_portal_data(PORTAL(obj));

				PORTAL(obj) = fread_obj_portal_data(fp);
				fMatch = TRUE;
				break;
			}
			if (!str_cmp(word, "#TYPESCROLL"))
			{
				if (IS_SCROLL(obj)) free_scroll_data(SCROLL(obj));

				SCROLL(obj) = fread_obj_scroll_data(fp);
				fMatch = TRUE;
				break;
			}
			if (!str_cmp(word, "#TYPETATTOO"))
			{
				if (IS_TATTOO(obj)) free_tattoo_data(TATTOO(obj));

				TATTOO(obj) = fread_obj_tattoo_data(fp);
				fMatch = TRUE;
				break;
			}
			if (!str_cmp(word, "#TYPEWAND"))
			{
				if (IS_WAND(obj)) free_wand_data(WAND(obj));

				WAND(obj) = fread_obj_wand_data(fp);
				fMatch = TRUE;
				break;
			}
			break;

		case 'A':
			if (!str_cmp(word,"AffD"))
			{
				AFFECT_DATA *paf;

				paf = new_affect();

				SKILL_DATA *skill = get_skill_data(fread_word(fp));
				if (!IS_VALID(skill))
					bug("Fread_obj: unknown skill.",0);
				else
					paf->skill = skill;

				paf->catalyst_type = -1;
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

				paf->skill = NULL;

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

				paf->skill = NULL;
				paf->where	= fread_number(fp);
				paf->group	= fread_number(fp);
				paf->level      = fread_number(fp);
				paf->duration   = fread_number(fp);
				paf->modifier   = fread_number(fp);
				paf->location   = fread_number(fp);
				if(paf->location == APPLY_SKILL) {
					SKILL_DATA *sk = get_skill_data(fread_word(fp));
					if(IS_VALID(sk))
						paf->location += sk->uid;
					else
					{
						paf->location = APPLY_NONE;
						paf->modifier = 0;
					}
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

				paf = new_affect();

				SKILL_DATA *skill = get_skill_data(fread_word(fp));
				if (!IS_VALID(skill))
					bug("Fread_obj: unknown skill.",0);
				else
					paf->skill = skill;

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

				paf = new_affect();

				SKILL_DATA *skill = get_skill_data(fread_word(fp));
				if (!IS_VALID(skill))
					bug("Fread_obj: unknown skill.",0);
				else
					paf->skill = skill;

				paf->where	= fread_number(fp);
				paf->group	= fread_number(fp);
				paf->level      = fread_number(fp);
				paf->duration   = fread_number(fp);
				paf->modifier   = fread_number(fp);
				paf->location   = fread_number(fp);
				if(paf->location == APPLY_SKILL) {
					SKILL_DATA *sk = get_skill_data(fread_word(fp));
					if(IS_VALID(sk))
						paf->location += sk->uid;
					else
					{
						paf->location = APPLY_NONE;
						paf->modifier = 0;
					}
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

					paf->where  = fread_number(fp);
					paf->group	= fread_number(fp);
					paf->level      = fread_number(fp);
					paf->duration   = fread_number(fp);
					paf->modifier   = fread_number(fp);
					paf->location   = fread_number(fp);
					if(paf->location == APPLY_SKILL) {
						SKILL_DATA *sk = get_skill_data(fread_word(fp));
						if(IS_VALID(sk))
							paf->location += sk->uid;
						else
						{
							paf->location = APPLY_NONE;
							paf->modifier = 0;
						}
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

				paf->catalyst_type = flag_value(catalyst_types,fread_word(fp));
				if(paf->catalyst_type == NO_FLAG) {
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

				paf->catalyst_type = flag_value(catalyst_types,fread_word(fp));
				if(paf->catalyst_type == NO_FLAG) {
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

				paf->catalyst_type = flag_value(catalyst_types,fread_word(fp));
				if(paf->catalyst_type == NO_FLAG) {
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

				paf->catalyst_type = flag_value(catalyst_types,fread_word(fp));
				if(paf->catalyst_type == NO_FLAG) {
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
				obj->extra[0] = fread_number(fp);
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "Extra2Flags") || !str_cmp(word, "Ext2F"))
			{
				obj->extra[1] = fread_number(fp);
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "Extra3Flags") || !str_cmp(word, "Ext3F"))
			{
				obj->extra[2] = fread_number(fp);
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "Extra4Flags") || !str_cmp(word, "Ext4F"))
			{
				obj->extra[3] = fread_number(fp);
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

			if (!str_cmp(word, "ExtraDescrEnv") || !str_cmp(word,"ExDeEnv"))
			{
				EXTRA_DESCR_DATA *ed;

				ed = new_extra_descr();

				ed->keyword		= fread_string(fp);
				ed->description		= NULL;
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
						return NULL;
						//obj = create_object(obj_index_dummy, 0 , FALSE);
					}

					fread_obj_check_version(obj, values);

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

					for(int i = 0; i < MAX_OBJVALUES; i++)
						obj->value[i] = values[i];

					if (obj->persist)
						persist_addobject(obj);
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
				WNUM_LOAD load = fread_widevnum(fp, 0);

				if ((pIndexData = get_obj_index_auid(load.auid, load.vnum)) != NULL)
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

			/*
			if( !str_cmp(word,"Lock") )
			{
				if( !obj->lock )
				{
					obj->lock = new_lock_state();
				}

				WNUM_LOAD load = fread_widevnum(fp, 0);
				obj->lock->key_load = load;
				obj->lock->key_wnum.pArea = get_area_from_uid(load.auid);
				obj->lock->key_wnum.vnum = load.vnum;
				obj->lock->flags = fread_number(fp);
				obj->lock->pick_chance = fread_number(fp);

				unsigned long id0 = fread_number(fp);
				while( id0 > 0 )
				{
					unsigned long id1 = fread_number(fp);
					
					LLIST_UID_DATA *luid = new_list_uid_data();
					luid->ptr = NULL;	// Resolve it later
					luid->id[0] = id0;
					luid->id[1] = id1;

					list_appendlink(obj->lock->keys, luid);

					id0 = fread_number(fp);
				}

				fMatch = TRUE;
				break;
			}
			*/

			if (!str_cmp(word, "Level") || !str_cmp(word, "Lev"))
			{
				obj->level = fread_number(fp);

				/*
				TODO: some special armor that updates with level
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
				*/

				fMatch = TRUE;
			}

			KEY("LoadedBy",	obj->loaded_by,		fread_string(fp));
			break;
		case 'M':
	    	if( !str_cmp(word, "MapWaypoint") )
	    	{
				WAYPOINT_DATA *wp = new_waypoint();

				wp->w = fread_number(fp);
				wp->x = fread_number(fp);
				wp->y = fread_number(fp);
				wp->name = fread_string(fp);

				if( !obj->waypoints )
				{
					obj->waypoints = new_waypoints_list();
				}

				list_appendlink(obj->waypoints, wp);

				fMatch = TRUE;
				break;
			}

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

		case 'P':
			KEY("PermExtra",		obj->extra_perm[0],	fread_number(fp));
			KEY("PermExtra2",		obj->extra_perm[1],	fread_number(fp));
			KEY("PermExtra3",		obj->extra_perm[2],	fread_number(fp));
			KEY("PermExtra4",		obj->extra_perm[3],	fread_number(fp));
			KEY("PermWeapon",		obj->weapon_flags_perm,	fread_number(fp));
			KEY("Persist", obj->persist, fread_number(fp));
			break;

		case 'R':
			if (!str_cmp(word, "Room"))
			{
				ROOM_INDEX_DATA *room;
				WNUM_LOAD w = fread_widevnum(fp, 0);

				room = get_room_index_auid(w.auid, w.vnum);
				obj->in_room = room;
				fMatch = TRUE;
			}
			break;

		case 'S':
			KEY("ShortDescr",	obj->short_descr,	fread_string(fp));
			KEY("ShD",		obj->short_descr,	fread_string(fp));

			if (!str_cmp(word, "SpellNew"))
			{
				SKILL_DATA *skill;
				SPELL_DATA *spell;

				fMatch = TRUE;

				skill = get_skill_data(fread_string(fp));
				if (IS_VALID(skill))
				{
					spell = new_spell();
					spell->skill = skill;
					spell->level = fread_number(fp);
					spell->repop = fread_number(fp);

					spell->next = obj->spells;
					obj->spells = spell;
				}
				else
				{
					sprintf(buf, "Bad spell name for %s (%ld#%ld).", obj->short_descr, obj->pIndexData->area->uid, obj->pIndexData->vnum);
					bug(buf,0);
				}
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

				for(int i = 0; i < MAX_OBJVALUES; i++)
					values[i] = fread_number(fp);

				break;
			}

			if( (vtype = variable_fread_type(word)) != VAR_UNKNOWN ) {
				variable_fread(&obj->progs->vars, vtype, fp);
				fMatch = TRUE;
			}

			if (!str_cmp(word, "Vnum"))
			{
				WNUM_LOAD wnum = fread_widevnum(fp, 0);

				// TODO: fix bug
				if ((obj->pIndexData = get_obj_index_auid(wnum.auid, wnum.vnum)) == NULL)
					bug("Fread_obj: bad vnum %ld.", wnum.vnum);
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


void fread_stache(FILE *fp, LLIST *lstache)
{
	char *word;
	bool fMatch;

    while (str_cmp((word = fread_word(fp)), "#-STACHE"))
	{
		fMatch = false;

		if (!str_cmp(word, "#O"))
		{
			OBJ_DATA *obj = fread_obj_new(fp);

			if (IS_VALID(obj))
			{
				obj->stached = true;
				list_appendlink(lstache, obj);
			}
			fMatch = true;
		}

		if (!fMatch)
		{
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

				ROOM_INDEX_DATA *to_room = obj->in_room;
				obj->in_room = NULL;
				obj_to_room(obj, to_room == NULL ? room_index_default : to_room);
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
    if (obj == NULL) {
		bug("fix_object: obj was null.", 0);
		return;
    }

    if (obj->pIndexData == NULL) {
		bug("fix_object: pIndexData was null.", 0);
		return;
    }

    //////////////////////////////////////////////////////////////////////
    // UPDATES

	// Just update it
	obj->version = VERSION_OBJECT;
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


// Resolve all special keys
void fix_lockstate(LOCK_STATE *state)
{
	if (state)
	{
		LLIST_UID_DATA *luid;
		ITERATOR skit;

		iterator_start(&skit, state->special_keys);
		while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&skit)) )
		{
			luid->ptr = idfind_object(luid->id[0], luid->id[1]);
		}
		iterator_stop(&skit);
	}	
}

void fix_object_lockstate(OBJ_DATA *obj)
{
	// IS_BOOK

	if (IS_CONTAINER(obj) && CONTAINER(obj)->lock)
		fix_lockstate(CONTAINER(obj)->lock);
	
	if (IS_FURNITURE(obj))
	{
		ITERATOR it;
		FURNITURE_COMPARTMENT *compartment;
		iterator_start(&it, FURNITURE(obj)->compartments);
		while((compartment = (FURNITURE_COMPARTMENT *)iterator_nextdata(&it)))
		{
			if (compartment->lock)
				fix_lockstate(compartment->lock);
		}
		iterator_stop(&it);
	}

	// IS_PORTAL


	if (obj->contains)
	{
		OBJ_DATA *content;
		for (content = obj->contains; content != NULL; content = content->next_content)
			fix_object_lockstate(content);
	}
}

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

	LLIST *groups = ch->pcdata->group_known;
	ch->pcdata->group_known = list_create(FALSE);
	ITERATOR git;
	SKILL_GROUP *group;
	iterator_start(&git, groups);
	while((group = (SKILL_GROUP *)iterator_nextdata(&git)))
	{
		gn_add(ch, group);
	}
	iterator_stop(&git);
	list_destroy(groups);

    /* make sure they have any new race skills */
    for (i = 0; pc_race_table[ch->race].skills[i] != NULL; i++)
		group_add(ch,pc_race_table[ch->race].skills[i],FALSE);


	// TODO: Readd checks for dealing with racial affects, affects2, imm, res and vuln

	/* 20203003 - Tieryo - Fix missing racial perm affects */

	if (ch->affected_by_perm[0] != race_table[ch->race].aff || ch->affected_by_perm[1] != race_table[ch->race].aff2)
	{
	ch->affected_by_perm[0] = race_table[ch->race].aff;
	ch->affected_by_perm[1] = race_table[ch->race].aff2;
	resetaffects = TRUE;
	}
	if( resetaffects )
	{
		// Reset flags
		ch->imm_flags = ch->imm_flags_perm;
		ch->res_flags = ch->res_flags_perm;
		ch->vuln_flags = ch->vuln_flags_perm;
		ch->affected_by[0] = ch->affected_by_perm[0];
		ch->affected_by[1] = ch->affected_by_perm[1];

		// Iterate through all affects
		for(paf = ch->affected; paf; paf = paf->next)
		{
			switch (paf->where)
			{
				case TO_AFFECTS:
					SET_BIT(ch->affected_by[0], paf->bitvector);
					SET_BIT(ch->affected_by[1], paf->bitvector2);

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
							SET_BIT(ch->affected_by[0], paf->bitvector);
							SET_BIT(ch->affected_by[1], paf->bitvector2);

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
	ch->deathsight_vision = ( IS_SET(ch->affected_by_perm[1], AFF2_DEATHSIGHT) ) ? ch->tot_level : 0;
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

	// Fix all object lockstates
	for(obj = ch->carrying; obj; obj = obj->next_content)
		fix_object_lockstate(obj);

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
			REMOVE_BIT(ch->affected_by[1], AFF2_ENSNARE);

		if (ch->pcdata->second_sub_class_thief == CLASS_THIEF_SAGE)
			SET_BIT(ch->affected_by[0], AFF_DETECT_HIDDEN);

		ch->version = 7;
    }

    if (ch->version < 8)
    {
		REMOVE_BIT(ch->comm, COMM_NOAUTOWAR);
		ch->version = 8;
    }

    if (ch->version < 10)
    {
		REMOVE_BIT(ch->act[0], PLR_PK);
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
    if (!IS_IMMORTAL(ch) && IS_SET(ch->act[0], PLR_BUILDING))
    {
		sprintf(buf, "fix_character: toggling off builder flag for non-immortal %s", ch->name);
		log_string(buf);
		REMOVE_BIT(ch->act[0], PLR_BUILDING);
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

	if( ch->version < VERSION_PLAYER_005 )
	{
		if( IS_IMMORTAL(ch) )
		{
			// Give existing immortals HOLYWARP
			SET_BIT(ch->act[1], PLR_HOLYWARP);
		}


		ch->version = VERSION_PLAYER_005;
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

// TODO: AUDIT: do we still need this
/* Check for screwed up subclasses. Ie people who have a mage class
   where their warrior class should be. No clue how it originally happened
   but here is the fix based on skill group.*/
void descrew_subclasses(CHAR_DATA *ch)
{
#if 0
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
#endif
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
    SKILL_GROUP *group;


    switch (class)
    {
	case CLASS_MAGE: 	group = group_lookup("mage skills"); 	break;
	case CLASS_CLERIC:	group = group_lookup("cleric skills");	break;
	case CLASS_THIEF:	group = group_lookup("thief skills");	break;
	case CLASS_WARRIOR:	group = group_lookup("warrior skills");	break;
	default:
	    bug("find_class_skill: bad class.", 0);
	    return FALSE;
    }

	if(!IS_VALID(group)) return false;

	ITERATOR sit;
	char *str;
	iterator_start(&sit, group->contents);
	while((str = (char *)iterator_nextdata(&sit)))
	{
		if (get_skill(ch, get_skill_data(str)) > 0)
			break;
	}
	iterator_stop(&sit);

	return str != NULL;
}


/* write a token */
void fwrite_token(TOKEN_DATA *token, FILE *fp)
{
	int i;

	fprintf(fp, "#TOKEN %ld#%ld\n", token->pIndexData->area->uid, token->pIndexData->vnum);
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
    WNUM_LOAD wnum;
    char buf[MSL];
    char *word;
    bool fMatch;
    int vtype;

	wnum = fread_widevnum(fp, 0);
    if ((token_index = get_token_index_auid(wnum.auid, wnum.vnum)) == NULL) {
//	sprintf(buf, "fread_token: no token index found for vnum %ld", vnum);
//	bug(buf, 0);
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
			fprintf(fp, "Flags %s\n", flag_string( skill_entry_flags, entry->flags));
		if( IS_VALID(entry->token) ) {
			fwrite_token(entry->token, fp);
		}

		if( entry->skill ) {
			fprintf(fp, "Sk %d %d %s~\n",
				entry->rating,
				entry->mod_rating,
				entry->skill->name);
		}

		if( entry->song ) {
			fprintf(fp, "Song %s~\n", entry->song->name);
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
    SKILL_DATA *skill = NULL;
	SKILL_ENTRY *entry = NULL;
    SONG_DATA *song = NULL;
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
			if(IS_VALID(token))
				token_to_char(token, ch);

			if( IS_VALID(song) ) {
				skill_entry_addsong(ch, song, token, source);
			} else if(IS_VALID(skill)) {
				//log_stringf("Adding %s '%s' to %s", (is_skill_spell(skill)?"Spell":"Skill"), skill->name, ch->name);
				if( is_skill_spell(skill) )
					entry = skill_entry_addspell(ch, skill, token, source, flags);
				else
					entry = skill_entry_addskill(ch, skill, token, source, flags);

				entry->rating = rating;
				entry->mod_rating = mod;
			}
			
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
			FVKEY("Flags",	flags, fread_string_eol(fp), skill_entry_flags);
			break;

		case 'S':
			if(IS_KEY("Sk")) {
				rating = fread_number(fp);
				mod = fread_number(fp);
				
				skill = get_skill_data(fread_string(fp));
				fMatch = TRUE;
				break;
			}

			if(IS_KEY("Song")) {
				song = get_song_data(fread_string(fp));
				fMatch = TRUE;
				break;
			}

			break;

		case 'T':
			if(IS_KEY("TypeAffect"))
			{
				source = SKILLSRC_AFFECT;
				fMatch = TRUE;
				break;
			}
			if(IS_KEY("TypeScript"))
			{
				source = SKILLSRC_SCRIPT;
				fMatch = TRUE;
				break;
			}
			if(IS_KEY("TypeScriptPerm"))
			{
				source = SKILLSRC_SCRIPT_PERM;
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
	fprintf(fp, "MPart %ld#%ld\n", part->area->uid, part->mob);
    else if (part->obj_sac != -1)
	fprintf(fp, "OSPart %ld#%ld\n", part->area->uid, part->obj_sac);
    else if (part->mob_rescue != -1)
	fprintf(fp, "MRPart %ld#%ld\n", part->area->uid, part->mob_rescue);
    else if (part->room != -1)
	fprintf(fp, "QRoom %ld#%ld\n", part->area->uid, part->room);
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
	WNUM_LOAD wnum;

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
				wnum = fread_widevnum(fp, 0);
				part->area = get_area_from_uid(wnum.auid);
			    part->mob = wnum.vnum;
			    fMatch = TRUE;
			    break;
			}

			if (!str_cmp(word, "MRPart")) {
				wnum = fread_widevnum(fp, 0);
				part->area = get_area_from_uid(wnum.auid);
			    part->mob_rescue = wnum.vnum;
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
			    WNUM_LOAD room_vnum;

				wnum = fread_widevnum(fp, 0);
				part->area = get_area_from_uid(wnum.auid);
			    part->obj = wnum.vnum;

			    obj_i = get_obj_index(part->area, part->obj);

			    room_vnum = fread_widevnum(fp, 0);
			    room = get_room_index_auid(room_vnum.auid, room_vnum.vnum);

			    obj = create_object(obj_i, 1, TRUE);
			    obj_to_room(obj, room);

			    part->pObj = obj;

			    fMatch = TRUE;
			    break;
			}

			if (!str_cmp(word, "OSPart")) {
				wnum = fread_widevnum(fp, 0);
				part->area = get_area_from_uid(wnum.auid);
			    part->obj_sac = wnum.vnum;
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
				wnum = fread_widevnum(fp, 0);
				part->area = get_area_from_uid(wnum.auid);
			    part->room = wnum.vnum;
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
