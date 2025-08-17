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
#include <dirent.h>
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
		fMatch = true; \
		break; \
	}

#define SKEY(literal, field) \
	if (IS_KEY(literal)) { \
		free_string(field); \
		field = fread_string(fp); \
		fMatch = true; \
		break; \
	}

#define FKEY(literal, field) \
	if (IS_KEY(literal)) { \
		field = true; \
		fMatch = true; \
		break; \
	}

#define FVKEY(literal, field, string, tbl) \
	if (IS_KEY(literal)) { \
		field = flag_value(tbl, string); \
		fMatch = true; \
		break; \
	}

#define FVDKEY(literal, field, string, tbl, bad, def) \
	if (!str_cmp(word, literal)) { \
		field = flag_value(tbl, string); \
		if( field == bad ) { \
			field = def; \
		} \
		fMatch = true; \
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
void fwrite_affect(FILE *fp, AFFECT_DATA *paf);
AFFECT_DATA *fread_affect(FILE *fp);
void fread_reputation(FILE *fp, CHAR_DATA *ch);
void insert_class_level(CHAR_DATA *ch, CLASS_LEVEL *cl);
void account_add_character(ACCOUNT_DATA *account, CHAR_DATA *ch);
ACCOUNT_DATA *find_account_by_id(unsigned long id0, unsigned long id1);
ACCOUNT_DATA *find_account_by_name(char *username);
void save_account(ACCOUNT_DATA *account);
extern void get_account_id(ACCOUNT_DATA *account);
void migrate_character_objects(CHAR_DATA *ch);
void rebuild_character_file(CHAR_DATA *ch);
void obj_to_char_temp(OBJ_DATA *obj, CHAR_DATA *ch);
void remove_duplicate_objects_from_char(CHAR_DATA *ch);
static void dedupe_obj_list(OBJ_DATA **head, LLIST *seen, LLIST *lworn);
void remove_duplicate_objects_from_list(OBJ_DATA **head, LLIST *seen);
void remove_duplicate_objects_from_char(CHAR_DATA *ch);
PLAYER_CHANNEL_DATA *fread_player_channel_data(const char *name);
void fwrite_player_channel_data(const char *name, PLAYER_CHANNEL_DATA *data);


// Version structures
struct __player_data_version_007
{
    int			class_current;
    int			sub_class_current;
    int			class_mage;
    int			second_class_mage;
    int			sub_class_mage;
    int			second_sub_class_mage;
    int			class_cleric;
    int			second_class_cleric;
    int			sub_class_cleric;
    int			second_sub_class_cleric;
    int			class_thief;
    int			second_class_thief;
    int			sub_class_thief;
    int			second_sub_class_thief;
    int			class_warrior;
    int			second_class_warrior;
    int			sub_class_warrior;
    int			second_sub_class_warrior;
};

static void __init_player_versioning_007(struct __player_data_version_007 *data)
{
	data->class_current = -1;
	data->sub_class_current = -1;
	data->class_mage = -1;
	data->second_class_mage = -1;
	data->sub_class_mage = -1;
	data->second_sub_class_mage = -1;
	data->class_cleric = -1;
	data->second_class_cleric = -1;
	data->sub_class_cleric = -1;
	data->second_sub_class_cleric = -1;
	data->class_thief = -1;
	data->second_class_thief = -1;
	data->sub_class_thief = -1;
	data->second_sub_class_thief = -1;
	data->class_warrior = -1;
	data->second_class_warrior = -1;
	data->sub_class_warrior = -1;
	data->second_sub_class_warrior = -1;
}

struct __player_data_version_008
{
	unsigned int questpoints;
	long quests_completed;
};

static void __init_player_versioning_008(struct __player_data_version_008 *data)
{
	data->questpoints = 0;
	data->quests_completed = 0;
}

struct __player_data_version_009
{
	int armour[4];
};

static void __init_player_versioning_009(struct __player_data_version_009 *data)
{
	for(int i = 0; i < 4; i++)
		data->armour[i] = 0;
}

#define OLD_LEVEL_MINIGOD		150
#define OLD_LEVEL_GOD			151
#define OLD_LEVEL_ASCENDANT		152
#define OLD_LEVEL_SUPREMACY		153
#define OLD_LEVEL_CREATOR		154
#define OLD_LEVEL_IMPLEMENTOR	155
struct __player_data_version_010
{
	int invis_level;
	int incog_level;
};

static void __init_player_versioning_010(struct __player_data_version_010 *data)
{
	data->invis_level = 0;
	data->incog_level = 0;
}

struct __player_data_versioning
{
	struct __player_data_version_007 _007;
	struct __player_data_version_008 _008;
	struct __player_data_version_009 _009;
	struct __player_data_version_010 _010;
};

static void __init_player_versioning(struct __player_data_versioning *data)
{
	__init_player_versioning_007(&data->_007);
	__init_player_versioning_008(&data->_008);
	__init_player_versioning_009(&data->_009);
	__init_player_versioning_010(&data->_010);
}

void fread_char(CHAR_DATA *ch, FILE *fp, struct __player_data_versioning *__versioning);
void fread_channel_entry(PLAYER_CHANNEL_DATA *channels, FILE *fp);
void fix_character( CHAR_DATA *ch, struct __player_data_versioning *__versioning );
void fread_account(ACCOUNT_DATA *account, FILE *fp);
void fread_account_character(ACCOUNT_DATA *account, FILE *fp);

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

void fwrite_reputations_char(CHAR_DATA *ch, FILE *fp)
{
	ITERATOR rpit;
	REPUTATION_DATA *rep;

	iterator_start(&rpit, ch->reputations);
	while((rep = (REPUTATION_DATA *)iterator_nextdata(&rpit)))
	{
		fprintf(fp, "#REPUTATION %ld#%ld\n", rep->pIndexData->area->uid, rep->pIndexData->vnum);
		fprintf(fp, "Rank %d\n", rep->current_rank);
		fprintf(fp, "MaximumRank %d\n", rep->maximum_rank);
		fprintf(fp, "Reputation %ld\n", rep->reputation);
		fprintf(fp, "ParagonLevel %d\n", rep->paragon_level);
		fprintf(fp, "Flags %s\n", print_flags(rep->flags));

		if (IS_VALID(rep->token))
			fwrite_token(rep->token, fp);

		fprintf(fp, "#-REPUTATION\n");
	}
	iterator_stop(&rpit);
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
		remove_duplicate_objects_from_char(ch);

    // Save character to account first
    log_string(ch->pcdata->last_area);
    log_string(ch->pcdata->last_region);
    if (ch->desc && ch->desc->account) {
        account_add_character(ch->desc->account, ch);
    } else if (!IS_NPC(ch) && 
              (ch->pcdata->account_id[0] != 0 || !IS_NULLSTR(ch->pcdata->account_name))) {
        // Try to find and update account - first by ID, then by name
        ACCOUNT_DATA *account = NULL;
        
        if (ch->pcdata->account_id[0] != 0)
            account = find_account_by_id(ch->pcdata->account_id[0], ch->pcdata->account_id[1]);
            
        // Fall back to name lookup if ID lookup fails
        if (account == NULL && !IS_NULLSTR(ch->pcdata->account_name))
            account = find_account_by_name(ch->pcdata->account_name);
            
        if (account != NULL) {
            account_add_character(account, ch);
            save_account(account);
            free_account(account);
        }
    }

if (ch->carrying_temp && ch->version < VERSION_PLAYER_011) {
    OBJ_DATA *obj = ch->carrying_temp;
    OBJ_DATA *next;
    while (obj) {
        next = obj->next_content;
        obj_to_char(obj, ch);
        obj = next;
    }
    ch->carrying_temp = NULL;
    ch->version = VERSION_PLAYER_011;
	save_char_obj(ch);
}

    fclose(fpReserve);
    sprintf(strsave, "%s%c/%s", PLAYER_DIR, tolower(ch->name[0]), capitalize(ch->name));
    if ((fp = fopen(TEMP_FILE, "w")) == NULL)
    {
        bug("Save_char_obj: fopen", 0);
        perror(strsave);
    }
    else
    {
        p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SAVE, NULL, 0, 0, 0, 0, 0);

        fwrite_char(ch, fp);

		// Write equipment section
		fprintf(fp, "#EQUIPMENT\n");
		ITERATOR eit;
		OBJ_DATA *eobj;
		iterator_start(&eit, ch->lworn);
		while ((eobj = (OBJ_DATA *)iterator_nextdata(&eit))) {
    		if (!eobj->locker && eobj->in_obj == NULL && list_haslink(loaded_objects, eobj)) {
        		fwrite_obj_new(ch, eobj, fp, 0);
    		}
		}
		iterator_stop(&eit);
		fprintf(fp, "#ENDEQUIPMENT\n");


		fprintf(fp, "#INVENTORY\n");
		OBJ_DATA *obj, *next;
		for (obj = ch->carrying; obj != NULL; obj = next) {
    		next = obj->next_content;
    		// Only write unequipped, non-locker, top-level objects
    		if (obj->wear_loc == WEAR_NONE && !obj->locker && obj->in_obj == NULL && list_haslink(loaded_objects, obj)) {
        		obj->next_content = NULL; // Prevent recursion through the list
        		fwrite_obj_new(ch, obj, fp, 0);
        		obj->next_content = next; // Restore the link
    		}
		}
		fprintf(fp, "#ENDINVENTORY\n");


        // Write locker section
        fprintf(fp, "#LOCKER\n");
        for (obj = ch->locker; obj != NULL; obj = obj->next_content)
			if (list_haslink(loaded_objects, obj))
            	fwrite_obj_new(ch, obj, fp, 0);
        fprintf(fp, "#ENDLOCKER\n");

        if (ch->tokens != NULL) {
            TOKEN_DATA *token;
            for(token = ch->tokens; token; token = token->next)
                if (token_should_save(token))
                    fwrite_token(token, fp);
        }

        fwrite_stache_char(ch, fp);
        fwrite_skills(ch, fp);
        fwrite_reputations_char(ch, fp);


        fprintf(fp, "#END\n");
    }

    fclose(fp);
    rename(TEMP_FILE, strsave);

	fwrite_player_channel_data(ch->name, ch->pcdata->channels);

    fpReserve = fopen(NULL_FILE, "r");
}


void fwrite_mission_part(FILE *fp, MISSION_PART_DATA *part)
{
	/* Recursion to make sure we don't have list flipping */
	if (part->next != NULL)
	fwrite_mission_part(fp, part->next);

	fprintf(fp, "#MISSIONPART\n");

	if (part->pObj != NULL && !part->complete)
	{
		// Special case. Objects will be extracted on quit, re-loaded on login.
		if (part->pObj->in_room == NULL)
			bug("fwrite_mission_part: trying to save a mission pickup obj with null in_room", 0);
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
		fprintf(fp, "Room %ld#%ld\n", part->area->uid, part->room);
	else if (part->custom_task)
		fprintf(fp, "Custom\n");

	if (part->complete) fprintf(fp, "Complete\n");

	fprintf(fp, "Description %s~\n", fix_string(part->description));
	fprintf(fp, "#-MISSIONPART\n");
}


void fwrite_affect(FILE *fp, AFFECT_DATA *paf)
{
	fprintf(fp, "#AFFECT\n");
	if (paf->custom_name)
		fprintf(fp, "Name %s~\n", paf->custom_name);
	if (IS_VALID(paf->skill))
		fprintf(fp, "Skill %s~\n", paf->skill->name);
	fprintf(fp, "Group %s~\n", flag_string(affgroup_flags,paf->group));
	fprintf(fp, "Where %s~\n", flag_string(apply_types,paf->where));
	fprintf(fp, "Level %d\n", paf->level);
	fprintf(fp, "Duration %d\n", paf->duration);
	fprintf(fp, "Modifier %d\n", paf->modifier);
	fprintf(fp, "Location %s~\n", flag_string(apply_flags,paf->location));
	fprintf(fp, "BitVector %s\n", print_flags(paf->bitvector));
	fprintf(fp, "BitVector2 %s\n", print_flags(paf->bitvector2));
	fprintf(fp, "Slot %s~\n", flag_string(wear_loc_flags,paf->slot));
	fprintf(fp, "#-AFFECT\n");
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
    // Add account reference by both name and ID
    if (!IS_NPC(ch) && ch->desc && ch->desc->account) {
        fprintf(fp, "Account %s~\n", ch->desc->account->username);
        fprintf(fp, "AccountId %ld %ld\n", ch->desc->account->id[0], ch->desc->account->id[1]);
    }
    else if (!IS_NPC(ch) && !IS_NULLSTR(ch->pcdata->account_name)) {
        fprintf(fp, "Account %s~\n", ch->pcdata->account_name);
        fprintf(fp, "AccountId %ld %ld\n", ch->pcdata->account_id[0], ch->pcdata->account_id[1]);
    }
    if(!IS_NPC(ch))
	fprintf(fp, "Created   %ld\n", ch->pcdata->creation_date	);
    fprintf(fp, "Id   %ld\n", ch->id[0]			);
    fprintf(fp, "Id2  %ld\n", ch->id[1]			);
    fprintf(fp, "LogO %ld\n", (long int)current_time		);
    fprintf(fp, "LogI %ld\n", (long int) ch->pcdata->last_login	);

	if(!IS_NPC(ch))
		fprintf(fp, "StaffRank %s~\n", flag_string(staff_ranks, ch->pcdata->staff_rank));

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
    || !str_cmp(ch->prompt,"{B<{x%h{Bhp {x%m{Bm {x%v{Bmv>{x%c"))
        fprintf(fp, "Prom %s~\n",      ch->prompt  	);
    fprintf(fp, "Race %s~\n", ch->race->name);
    fprintf(fp, "Sex  %d\n",	ch->sex			);
    fprintf(fp, "LockerRent %ld\n", (long int)ch->locker_rent   );
	if (ch->deleted)
	{
		fprintf(fp, "Deleted %d\n", ch->deleted);
		fprintf(fp, "DeleteTime %ld\n", (long int)ch->delete_time);
	}

	ITERATOR cit;
	CLASS_LEVEL *cl;
	iterator_start(&cit, ch->pcdata->classes);
	while((cl = (CLASS_LEVEL *)iterator_nextdata(&cit)))
	{
		fprintf(fp, "ClassLevel %s~ %d %ld\n", cl->clazz->name, cl->level, cl->xp);
	}
	iterator_stop(&cit);

	if (IS_VALID(ch->pcdata->current_class) && IS_VALID(ch->pcdata->current_class->clazz))
	{
		fprintf(fp, "Class %s~\n", ch->pcdata->current_class->clazz->name);
	}

#if 0
	// Removed in PLAYER VERSION 007
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
#endif

    if (ch->pcdata->email != NULL)
	fprintf(fp, "Email %s~\n",  ch->pcdata->email	);
    // Add the new email verification fields
    fprintf(fp, "EmailVerified %d\n", ch->pcdata->email_verified);
    if (ch->pcdata->pending_email != NULL)
        fprintf(fp, "PendingEmail %s~\n", ch->pcdata->pending_email);
    if (ch->pcdata->email_verification_code != NULL)
        fprintf(fp, "EmailVerificationCode %s~\n", ch->pcdata->email_verification_code);
    if (ch->pcdata->email_verification_time > 0)
        fprintf(fp, "EmailVerificationTime %ld\n", ch->pcdata->email_verification_time);
    if (ch->pcdata->email_verification_last_sent > 0)
        fprintf(fp, "EmailVerificationLastSent %ld\n", ch->pcdata->email_verification_last_sent);
    
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

    fprintf(fp, "Plyd %d\n", ch->played + (int) (current_time - ch->logon));

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

	fprintf(fp, "LastArea     %s~\n", ch->pcdata->last_area);
	fprintf(fp, "LastRegion   %s~\n", ch->pcdata->last_region);

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
    if (ch->deitypoints != 0)
		fprintf(fp, "DeityPnts %ld\n", ch->deitypoints);
    if (ch->missionpoints != 0)
        fprintf(fp, "MissionPnts %d\n",  ch->missionpoints);
    if (ch->pcdata->missions_completed != 0)
		fprintf(fp, "MissionsCompleted %ld\n", ch->pcdata->missions_completed);
	if (ch->nextmission != 0)
        fprintf(fp, "MissionNext %d\n",  ch->nextmission  );

	ITERATOR mit;
	MISSION_DATA *mission;
	iterator_start(&mit, ch->missions);
	while((mission = (MISSION_DATA *)iterator_nextdata(&mit)))
	{
		fprintf(fp, "#MISSION\n");

		fprintf(fp, "Timer %ld\n", mission->timer);
		fprintf(fp, "GiverType %d\n", mission->giver_type);
		fprintf(fp, "Giver %s\n", widevnum_string_wnum(mission->giver, NULL));
		fprintf(fp, "ReceiverType %d\n", mission->receiver_type);
		fprintf(fp, "Receiver %s\n", widevnum_string_wnum(mission->receiver, NULL));

		fprintf(fp, "Class %s~\n", mission->clazz->name);
		fprintf(fp, "ClassType %s\n", flag_string(class_types, mission->clazz_type));

		if (mission->clazz_restricted)
			fprintf(fp, "ClassRestricted\n");

		if (mission->clazz_type_restricted)
			fprintf(fp, "ClassTypeRestricted\n");

		fwrite_mission_part(fp, mission->parts);
		fprintf(fp, "#-MISSION\n");
	}
	iterator_stop(&mit);


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
		fprintf(fp, "Wizinvis %s\n", flag_string(staff_ranks, ch->invis_level));
    if (ch->incog_level)
		fprintf(fp, "Incognito %s\n", flag_string(staff_ranks, ch->incog_level));
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
	for(int i = 0; i < ARMOR_MAX; i++)
	{
		fprintf(fp, "Armor %s~ %d\n", flag_string(armour_protection_types, i), ch->armour[i]);
	}
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
		if (ch->pcdata->reset_code != NULL)
			fprintf(fp, "ResetCode %s~\n", ch->pcdata->reset_code);

		if (ch->pcdata->reset_time != 0)
			fprintf(fp, "Reset_Time %ld\n", ch->pcdata->reset_time);

		fprintf(fp, "ResetState %d\n", ch->pcdata->reset_state);
		if (ch->pcdata->mfa_key != NULL)
			fprintf(fp, "MFA_Key %s~\n", ch->pcdata->mfa_key);
		//if (ch->pcdata->qr_code_expiration != 0)
		//	fprintf(fp, "MFA_Code_Expiration %ld\n", ch->pcdata->qr_code_expiration);
		if (ch->pcdata->mfa_enabled == TRUE)
			fprintf(fp, "MFA_Enabled\n");
		fprintf(fp, "MFAPendingKey %s~\n", ch->pcdata->mfa_pending_key ? ch->pcdata->mfa_pending_key : "");
		fprintf(fp, "MFAPending %d\n", ch->pcdata->mfa_pending ? 1 : 0);
		fprintf(fp, "RecoveryCodes ");
			for (int i = 0; i < MFA_RECOVERY_CODES; ++i)
    			fprintf(fp, "%s%c", ch->pcdata->recovery_codes[i], (i == MFA_RECOVERY_CODES-1) ? '\n' : ' ');

		fprintf(fp, "RecoveryUsed ");
			for (int i = 0; i < MFA_RECOVERY_CODES; ++i)
    			fprintf(fp, "%d%c", ch->pcdata->recovery_used[i] ? 1 : 0, (i == MFA_RECOVERY_CODES-1) ? '\n' : ' ');
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

		// Saved with tokens
		if (IS_VALID(paf->token))
			continue;

		fwrite_affect(fp, paf);
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

    fprintf(fp, "End\n\n");
}

extern pVARIABLE variable_head;
extern pVARIABLE variable_tail;

/*
 * Load a char and inventory into a new ch structure.
 */
bool load_char_obj(DESCRIPTOR_DATA *d, char *name)
{
    struct __player_data_versioning __versioning;
    char strsave[MAX_INPUT_LENGTH], buf[MSL];
    CHAR_DATA *ch;
    OBJ_DATA *obj, *objNestList[MAX_NEST];
    IMMORTAL_DATA *immortal;
    FILE *fp;
    bool found = false;
    int stat;
    TOKEN_DATA *token;
    pVARIABLE last_var = variable_tail;
    char *section = NULL;

    __init_player_versioning(&__versioning);

    ch = new_char();
    ch->pcdata = new_pcdata();
    d->character = ch;
    ch->desc = d;
    ch->name = str_dup(name);
    ch->id[0] = ch->id[1] = 0;
    ch->pcdata->creation_date = -1;
    ch->race = gr_human;
    ch->act[0] = PLR_NOSUMMON;
    ch->act[1] = 0;
    ch->comm = COMM_PROMPT;
    ch->num_grouped = 0;
    ch->dead = false;
    ch->prompt = str_dup("{B<{x%h{Bhp {x%m{Bm {x%v{Bmv>{x%c");
    ch->pcdata->confirm_delete = false;
    ch->pcdata->pwd = str_dup("");
    ch->pcdata->pwd_vers = 0;
    ch->pcdata->reset_code = str_dup("");
    ch->pcdata->reset_state = 0;
    ch->pcdata->title = str_dup("");
    for (stat = 0; stat < MAX_STATS; stat++)
    {
        ch->perm_stat[stat] = 13;
        ch->mod_stat[stat] = 0;
        ch->dirty_stat[stat] = true;
    }
    ch->pcdata->condition[COND_THIRST] = 48;
    ch->pcdata->condition[COND_FULL] = 48;
    ch->pcdata->condition[COND_HUNGER] = 48;
    ch->pcdata->condition[COND_STONED] = 0;
    ch->pcdata->security = 0;
    ch->pcdata->challenge_delay = 0;
    ch->pcdata->mfa_key = str_dup("");
    ch->morphed = false;
    ch->locker_rent = 0;
    ch->deathsight_vision = 0;
    ch->carrying_temp = NULL; // for migration

    fclose(fpReserve);
    sprintf(strsave, "%s%c/%s", PLAYER_DIR, tolower(name[0]), capitalize(name));
    if ((fp = fopen(strsave, "r")) != NULL) {
        int iNest;
        for (iNest = 0; iNest < MAX_NEST; iNest++)
            objNestList[iNest] = NULL;

        found = true;
        for (;;) {
            char letter = fread_letter(fp);
            if (letter == '*') { fread_to_eol(fp); continue; }
            if (letter != '#') { bug("Load_char_obj: # not found.", 0); break; }

            char *word = fread_word(fp);

            // Section tracking
            if (!str_cmp(word, "EQUIPMENT")) { section = "EQUIPMENT"; continue; }
            if (!str_cmp(word, "ENDEQUIPMENT")) { section = NULL; continue; }
            if (!str_cmp(word, "INVENTORY")) { section = "INVENTORY"; continue; }
            if (!str_cmp(word, "ENDINVENTORY")) { section = NULL; continue; }
            if (!str_cmp(word, "LOCKER")) { section = "LOCKER"; continue; }
            if (!str_cmp(word, "ENDLOCKER")) { section = NULL; continue; }

            if (!str_cmp(word, "PLAYER")) {
                fread_char(ch, fp, &__versioning);
            } else if (!str_cmp(word, "OBJECT") || !str_cmp(word, "O")) {
                obj = fread_obj_new(fp);
                if (!obj) continue;


                objNestList[obj->nest] = obj;

                if (section) {
					//log_string(formatf("Loading object %s into section %s for %s\n", obj->name, section, ch->name));
                    if (!str_cmp(section, "LOCKER")) {
                        obj_to_locker(obj, ch);
                    } 
					/*
					else if (!str_cmp(section, "EQUIPMENT")) {
                        obj_to_char(obj, ch);
                        if (obj->wear_loc != WEAR_NONE)
                            list_addlink(ch->lworn, obj);
                    } 
					*/
				else if (!str_cmp(section, "EQUIPMENT") || !str_cmp(section, "INVENTORY")) {
    objNestList[obj->nest] = obj;
    if (obj->nest == 0) {
        obj_to_char(obj, ch);
    } else {
        OBJ_DATA *container = objNestList[obj->nest - 1];
        if (container && IS_CONTAINER(container))
            obj_to_obj(obj, container);
        else
            obj_to_char(obj, ch); // fallback if container is missing
    }
    // For equipped items, add to lworn if needed
    if (!str_cmp(section, "EQUIPMENT") && obj->wear_loc != WEAR_NONE)
        list_addlink(ch->lworn, obj);
}
					else if (!str_cmp(section, "INVENTORY")) {
                        if (obj->nest == 0) {
                            obj_to_char(obj, ch);
                        } else {
                            OBJ_DATA *container = objNestList[obj->nest - 1];
                            if (container && IS_CONTAINER(container))
                                obj_to_obj(obj, container);
                            else
                                obj_to_char(obj, ch);
                        }
                    }
                } else if (section == NULL && ch->version < VERSION_PLAYER_011) {
					//log_stringf("Old character, no sections. Loading object %s into inventory (ch->carrying_temp) for %s\n", obj->name, ch->name);
    				if (obj->nest == 0) {
        				obj_to_char_temp(obj, ch);
    				} else {
        				OBJ_DATA *container = objNestList[obj->nest - 1];
        				if (container && IS_CONTAINER(container))
            			obj_to_obj(obj, container);
        			else
            			obj_to_char_temp(obj, ch); // fallback if container is missing
    				}
				}
				else if (ch->version >= VERSION_PLAYER_011 && !section)
				{
					//log_stringf("New character, no sections. Not loading %s\n", obj->name);
					continue;
				}
				
			}



            else if (!str_cmp(word, "L")) {
                obj = fread_obj_new(fp);
                obj_to_locker(obj, ch);
            }
            else if (!str_cmp(word, "STACHE")) {
                fread_stache(fp, ch->lstache);
            }
            else if (!str_cmp(word, "TOKEN")) {
                token = fread_token(fp);
                if (token)
                {
                    token_to_char(token, ch);

                    // Add all affects on the token to the character
                    ITERATOR ait;
                    AFFECT_DATA *taf;
                    iterator_start(&ait, token->affects);
                    while((taf = (AFFECT_DATA *)iterator_nextdata(&ait)))
                    {
                        taf->next = ch->affected;
                        ch->affected = taf;
                    }
                    iterator_stop(&ait);
                }
            }
            else if (!str_cmp(word, "REPUTATION")) {
                fread_reputation(fp, ch);
            }
            else if (!str_cmp(word, "SKILLENTRY")) {
                fread_skill(fp, ch, false);
            }
            else if (!str_cmp(word, "SONGENTRY")) {
                fread_skill(fp, ch, true);
            }
            else if (!str_cmp(word, "END"))
                break;
            else {
                bug("Load_char_obj: bad section.", 0);
                break;
            }
        }

        fclose(fp);
    }
	ch->pcdata->channels = fread_player_channel_data(name);

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

    // Handle immortal data setup
    if (get_staff_rank(ch) > STAFF_PLAYER) {
        if ((immortal = find_immortal(ch->name)) == NULL) {
            snprintf(buf, sizeof(buf), "load_char_obj: no immortal_data found for immortal character %s!", ch->name);
            bug(buf, 0);

            immortal = new_immortal();
            immortal->name = str_dup(ch->name);
            ch->pcdata->immortal = immortal;

            add_immortal(immortal);

        } else {
            snprintf(buf, sizeof(buf), "load_char_obj: reading immortal char %s.\n\r", ch->name);
            log_string(buf);

            ch->pcdata->immortal = immortal;
        }
        immortal->pc = ch->pcdata;
    }
    else if ((immortal = find_immortal(ch->name)) != NULL)
    {
        log_string(formatf("load_char_obj: resolving immortal data for %s.\n\r", ch->name));
        ch->pcdata->staff_rank = STAFF_IMMORTAL;
        ch->pcdata->immortal = immortal;
        immortal->pc = ch->pcdata;
    }

    // Fix char.
    if (found)
        fix_character(ch, &__versioning);

    /* Redo shift. Remember ch->shifted was just used as a placeholder to tell the game
       to re-shift, so we have to switch it to none first. */
    if (ch->shifted != SHIFTED_NONE) {
        ch->shifted = SHIFTED_NONE;
        shift_char(ch, true);
    }

    variable_fix_list(last_var ? last_var : variable_head);

if (ch->version < VERSION_PLAYER_011)
	save_char_obj(ch);

    ch->pcdata->last_login = current_time;
    return found;
}






MISSION_PART_DATA *fread_mission_part(FILE *fp)
{
    MISSION_PART_DATA *part;
    char buf[MSL];
    char *word;
    bool fMatch;
	WNUM_LOAD wnum;

    part = new_mission_part();

    for (; ;)
    {
		word   = feof(fp) ? "#-MISSIONPART" : fread_word(fp);
		fMatch = false;

		if (!str_cmp(word, "#-MISSIONPART")) {
			fMatch = true;
			return part;
		}

		switch (UPPER(word[0]))
		{
			case 'C':
				if (!str_cmp(word, "Complete")) {
					part->complete = true;
					fMatch = true;
					fread_to_eol(fp);
					break;
				}

				if (!str_cmp(word, "Custom")) {
					part->custom_task = true;
					fMatch = true;
					break;
				}
				break;

			case 'D':
				if (!str_cmp(word, "Description")) {
					part->description = fread_string(fp);
					fMatch = true;
					break;
				}
				break;

		    case 'M':
				if (!str_cmp(word, "MPart")) {
					wnum = fread_widevnum(fp, 0);
					part->area = get_area_from_uid(wnum.auid);
					part->mob = wnum.vnum;
					fMatch = true;
					break;
				}

				if (!str_cmp(word, "MRPart")) {
					wnum = fread_widevnum(fp, 0);
					part->area = get_area_from_uid(wnum.auid);
					part->mob_rescue = wnum.vnum;
					fMatch = true;
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

					obj = create_object(obj_i, 1, true);
					obj_to_room(obj, room);

					part->pObj = obj;

					fMatch = true;
					break;
				}

				if (!str_cmp(word, "OSPart")) {
					wnum = fread_widevnum(fp, 0);
					part->area = get_area_from_uid(wnum.auid);
					part->obj_sac = wnum.vnum;
					fMatch = true;
					break;
				}
				break;

		    case 'R':
				if (!str_cmp(word, "Room")) {
					wnum = fread_widevnum(fp, 0);
					part->area = get_area_from_uid(wnum.auid);
					part->room = wnum.vnum;
					fMatch = true;
					break;
				}
				break;
		}

	    if (!fMatch) {
		    snprintf(buf, sizeof(buf), "fread_mission_part: no match for word %s", word);
		    bug(buf, 0);
		    fread_to_eol(fp);
	    }
    }
}


MISSION_DATA *fread_mission(FILE *fp)
{
    MISSION_DATA *mission;
    char buf[MSL];
    char *word;
    bool fMatch;
	WNUM_LOAD wnum;

	mission = new_mission();

    for (; ;)
    {
		word   = feof(fp) ? "#-MISSION" : fread_word(fp);
		fMatch = false;

		if (!str_cmp(word, "#-MISSION")) {
			return mission;
		}

		switch (UPPER(word[0]))
		{
			case '#':
				if (!str_cmp(word, "#MISSIONPART"))
				{
					MISSION_PART_DATA *part = fread_mission_part(fp);
					part->next = mission->parts;
					mission->parts = part;
					fMatch = true;
					break;
				}
				break;

			case 'C':
				KEY("Class", mission->clazz, get_class_data(fread_string(fp)));
				KEY("ClassRestricted", mission->clazz_type_restricted, true);
				KEY("ClassType", mission->clazz_type, stat_lookup(fread_string(fp),class_types,CLASS_NONE));
				KEY("ClassTypeRestricted", mission->clazz_type_restricted, true);
				break;

			case 'G':
				if (!str_cmp(word, "Giver"))
				{
					wnum = fread_widevnum(fp, 0);
					mission->giver.pArea = get_area_from_uid(wnum.auid);
					mission->giver.vnum = wnum.vnum;
					fMatch = true;
					break;
				}
				KEY("GiverType", mission->giver_type, fread_number(fp));
				break;

			case 'R':
				if (!str_cmp(word, "Receiver"))
				{
					wnum = fread_widevnum(fp, 0);
					mission->receiver.pArea = get_area_from_uid(wnum.auid);
					mission->receiver.vnum = wnum.vnum;
					fMatch = true;
					break;
				}
				KEY("ReceiverType", mission->receiver_type, fread_number(fp));
				break;

			case 'T':
				KEY("Timer", mission->timer, fread_number(fp));
				break;
		}

	    if (!fMatch) {
		    snprintf(buf, sizeof(buf), "read_quest_part: no match for word %s", word);
		    bug(buf, 0);
		    fread_to_eol(fp);
	    }
    }

}

AFFECT_DATA *fread_affect(FILE *fp)
{
    AFFECT_DATA *paf;
    char buf[MSL];
    char *word;
    bool fMatch;

	paf = new_affect();
	paf->catalyst_type = -1;

    for (; ;)
    {
		word   = feof(fp) ? "#-AFFECT" : fread_word(fp);
		fMatch = false;

		if (!str_cmp(word, "#-AFFECT")) {
			return paf;
		}

		switch(word[0])
		{
			case 'B':
				KEY("BitVector", paf->bitvector, fread_flag(fp));
				KEY("BitVector2", paf->bitvector2, fread_flag(fp));
				break;

			case 'D':
				KEY("Duration", paf->duration, fread_number(fp));
				break;

			case 'G':
				KEY("Group", paf->group, stat_lookup(fread_string(fp),affgroup_mobile_flags,AFFGROUP_MAGICAL));
				break;

			case 'L':
				KEY("Level", paf->level, fread_number(fp));
				KEY("Location", paf->location, stat_lookup(fread_string(fp),apply_flags,APPLY_NONE));
				break;

			case 'M':
				KEY("Modifier", paf->modifier, fread_number(fp));
				break;

			case 'N':
				KEYS("Name", paf->custom_name, create_affect_cname(fread_string(fp)));
				break;

			case 'S':
				KEY("Skill", paf->skill, get_skill_data(fread_string(fp)));
				KEY("Slot", paf->slot, stat_lookup(fread_string(fp),wear_loc_flags,WEAR_NONE));
				break;

			case 'W':
				KEY("Where", paf->where, stat_lookup(fread_string(fp),apply_types,TO_AFFECTS));
				break;
		}

	    if (!fMatch) {
		    snprintf(buf, sizeof(buf), "fread_affect: no match for word %s", word);
		    bug(buf, 0);
		    fread_to_eol(fp);
	    }
	}
}

void fread_channel_entry(PLAYER_CHANNEL_DATA *channels, FILE *fp)
{
	CHANNEL_ENTRY *entry = new_channel_entry();

	char buf[MSL];
	char *word;
	bool fMatch;

	char *name = fread_string(fp);
	entry->channel = get_channel_data(name);

	while(str_cmp((word = fread_word(fp)), "#-CHANNELENTRY"))
	{
		fMatch = false;

		switch(UPPER(word[0]))
		{
		case 'B':
			KEYS("Banner", entry->banner, fread_string(fp));
			break;

		case 'F':
			KEY("Flags", entry->flags, fread_flag(fp));
			break;
		}
		
		if (!fMatch)
		{
			snprintf(buf, sizeof(buf), "fread_channel_entry: no match for word %s", word);
			bug(buf, 0);
		}
	}

	// Only keep if the channel reference was found.
	if (entry->channel)
	{
		list_appendlink(channels->channels, entry->channel);

		// TODO: Update the joined flag if there is an active ban punishment for the channel
	}
	else
	{
		snprintf(buf, sizeof(buf), "fread_channel_entry: channel '%s' doesn't exist.  Ignoring entry.", name);
		bug(buf, 0);

		free_channel_entry(entry);
	}
}


/*
 * Read in a char.
 */
void fread_char(CHAR_DATA *ch, FILE *fp, struct __player_data_versioning *__versioning)
{
    AREA_DATA *pArea = NULL;
    char buf[MAX_STRING_LENGTH];
    char *word;
    char *race_string;
    char *immortal_flag = str_dup("");
    bool fMatch;
    bool fMatchFound = false;
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
	fMatch = false;

	//bug(word, 0);

	switch (UPPER(word[0]))
	{
	case '*':
	    fMatch = true;
	    fread_to_eol(fp);
	    break;

	case '#':
#if 0
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
#endif
		if (!str_cmp(word, "#MISSION"))
		{
			MISSION_DATA *mission = fread_mission(fp);
			list_appendlink(ch->missions, mission);
			fMatch = true;
			break;
		}
		if (!str_cmp(word, "#AFFECT"))
		{
			AFFECT_DATA *paf = fread_affect(fp);
			paf->next = ch->affected;
			ch->affected = paf;
			fMatch = true;
			break;
		}
		break;

	case 'A':
		KEYS("Account", ch->pcdata->account_name, fread_string(fp));
		if (!str_cmp(word, "AccountId")) {
			ch->pcdata->account_id[0] = fread_number(fp);
			ch->pcdata->account_id[1] = fread_number(fp);
			fMatch = true;
			break;
		}
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
		    fMatch = true;
		    break;
		}

		ch->pcdata->alias[count] 	= str_dup(fread_word(fp));
		ch->pcdata->alias_sub[count]	= str_dup(fread_word(fp));
		count++;
		fMatch = true;
		break;
	    }

            if (!str_cmp(word, "Alias"))
            {
                if (count >= MAX_ALIAS)
                {
                    fread_to_eol(fp);
                    fMatch = true;
                    break;
                }

                ch->pcdata->alias[count]        = str_dup(fread_word(fp));
                ch->pcdata->alias_sub[count]    = fread_string(fp);
                count++;
                fMatch = true;
                break;
            }

	    if (!str_cmp(word, "AC") || !str_cmp(word,"Armour"))
	    {
		fread_to_eol(fp);
		fMatch = true;
		break;
	    }

		if (ch->version < VERSION_PLAYER_009)
		{
			if (!str_cmp(word,"ACs"))
			{
			int i;

			for (i = 0; i < 4; i++)
				__versioning->_009.armour[i] = fread_number(fp);
			fMatch = true;
			break;
			}
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
		fMatch = true;
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
                fMatch = true;
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
                fMatch = true;
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
                fMatch = true;
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
                fMatch = true;
                break;
            }

	    if (!str_cmp(word, "AttrMod" ) || !str_cmp(word,"AMod"))
	    {
		int stat;
		for (stat = 0; stat < MAX_STATS; stat ++)
		   set_mod_stat(ch, stat, fread_number(fp));
		fMatch = true;
		break;
	    }

	    if (!str_cmp(word, "AttrPerm") || !str_cmp(word,"Attr"))
	    {
		int stat;

		for (stat = 0; stat < MAX_STATS; stat++)
			set_perm_stat(ch, stat, fread_number(fp));
		fMatch = true;
		break;
	    }

		if (!str_cmp(word, "Armor"))
		{
			char *name = fread_string(fp);
			int value = fread_number(fp);

			int ac = stat_lookup(name, armour_protection_types, NO_FLAG);
			if (ac != NO_FLAG)
			{
				ch->armour[ac] = value;
			}
			fMatch = true;
			break;
		}

		if (!str_cmp(word, "Aura"))
		{
			char *name = fread_string(fp);
			char *long_descr = fread_string(fp);

			add_aura_to_char(ch, name, long_descr);

			fMatch = true;
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
		fMatch = true;
		break;
	    }
            if (!str_cmp(word, "Before_socialC")) {
		location_set(&ch->before_social,get_area_from_uid(fread_number(fp)),0,fread_number(fp),fread_number(fp),fread_number(fp));
		fMatch = true;
		break;
	    }
            if (!str_cmp(word, "Before_socialW")) {
		location_set(&ch->before_social,NULL,fread_number(fp),fread_number(fp),fread_number(fp),fread_number(fp));
		fMatch = true;
		break;
	    }
		break;

	case 'C':
		if (ch->version < VERSION_PLAYER_007)
		{
		    KEY("Cla",	    __versioning->_007.class_current,         fread_number(fp));
	    	KEY("Class",    __versioning->_007.class_current,         fread_number(fp));
		}
		else
		{
			if (!str_cmp(word, "Class"))
			{
				char *name = fread_string(fp);

				CLASS_DATA *clazz = get_class_data(name);
				if (IS_VALID(clazz))
				{
					ch->pcdata->current_class = get_class_level(ch, clazz);

					if (!IS_VALID(ch->pcdata->current_class))
					{
						// Complain
					}
				}

				fMatch = true;
				break;
			}
		}
		if (!str_cmp(word, "ClassLevel"))
		{
			char *name = fread_string(fp);
			CLASS_DATA *clazz = get_class_data(name);
			int level = fread_number(fp);
			long xp = fread_number(fp);

			if (IS_VALID(clazz))
			{
				CLASS_LEVEL *cl = new_class_level();
				cl->clazz = clazz;
				cl->level = level;
				cl->xp = xp;
				insert_class_level(ch, cl);
			}
			fMatch = true;
			break;
		}
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
		        snprintf(buf, sizeof(buf),
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
			snprintf(buf, sizeof(buf),
		    "Couldn't load ch church for %s, church %s",
		        ch->name, ch->church_name);
			bug(buf, 0);
			ch->church = NULL;
			ch->church_member = NULL;
			free_string(ch->church_name);
		}

		fMatch = true;
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
		fMatch = true;
		break;
	    }

	    if (!str_cmp(word, "Condition") || !str_cmp(word,"Cond"))
	    {
		ch->pcdata->condition[0] = fread_number(fp);
		ch->pcdata->condition[1] = fread_number(fp);
		ch->pcdata->condition[2] = fread_number(fp);
		fMatch = true;
		break;
	    }
            if (!str_cmp(word,"Cnd"))
            {
                ch->pcdata->condition[0] = fread_number(fp);
                ch->pcdata->condition[1] = fread_number(fp);
                ch->pcdata->condition[2] = fread_number(fp);
		ch->pcdata->condition[3] = fread_number(fp);
                fMatch = true;
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
		ch->dead = true;
		fMatch = true;
	    }
		KEY("Deleted",	ch->deleted,		fread_number(fp));
		KEY("DeleteTime",	ch->delete_time,	fread_number(fp));

	    break;

        case 'E':
            KEY("EmailVerified", ch->pcdata->email_verified, fread_number(fp));
            KEYS("Email", ch->pcdata->email, fread_string(fp));
            KEY("EmailVerificationTime", ch->pcdata->email_verification_time, fread_number(fp));
            KEY("EmailVerificationLastSent", ch->pcdata->email_verification_last_sent, fread_number(fp));
            KEYS("EmailVerificationCode", ch->pcdata->email_verification_code, fread_string(fp));
            KEY("Exp", ch->exp, fread_number(fp));

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

		fMatch = true;
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

				fMatch = true;
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
			fMatch = true;
			break;
		}

	    if (!str_cmp(word, "HBS"))
	    {
		ch->pcdata->hit_before  = fread_number(fp);
		ch->pcdata->mana_before	= fread_number(fp);
		ch->pcdata->move_before	= fread_number(fp);
		fMatch = true;
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
		fMatch = true;
		break;
	    }

            if (!str_cmp(word, "HpManaMovePerm") || !str_cmp(word,"HMVP"))
            {
		long qp_number = 0;

                ch->pcdata->perm_hit	= fread_number(fp);
                ch->pcdata->perm_mana   = fread_number(fp);
                ch->pcdata->perm_move   = fread_number(fp);
                fMatch = true;

		if (IS_IMMORTAL(ch))
		    break;

		// Hack to make all chars who have hp/mana/move over there max down
		if (ch->pcdata->perm_hit > ch->race->max_vitals[ MAX_HIT ] + 11)
		{
		    // Get difference in hp, and divide it by 10 to find the number of train sessions.
		    qp_number = (ch->pcdata->perm_hit - ch->race->max_vitals[ MAX_HIT ]) / 10;
		    // Multiply sessions by 15 being number of pracs.
		    qp_number *= 15;
		    ch->missionpoints += qp_number;

		    ch->pcdata->perm_hit = ch->race->max_vitals[ MAX_HIT ];
		}

		if (ch->pcdata->perm_mana > ch->race->max_vitals[ MAX_MANA ] + 11)
		{
		    // Get difference in hp, and divide it by 10 to find the number of train sessions.
		    qp_number = (ch->pcdata->perm_mana - ch->race->max_vitals[ MAX_MANA ]) / 10;
		    // Multiply sessions by 15 being number of pracs.
		    qp_number *= 15;
		    ch->missionpoints += qp_number;

		    ch->pcdata->perm_mana = ch->race->max_vitals[ MAX_MANA ];
		}

		if (ch->pcdata->perm_move > ch->race->max_vitals[ MAX_MOVE ] + 11)
		{
		    // Get difference in hp, and divide it by 10 to find the number of train sessions.
		    qp_number = (ch->pcdata->perm_move - ch->race->max_vitals[ MAX_MOVE ]) / 10;
		    // Multiply sessions by 15 being number of pracs.
		    qp_number *= 15;
		    ch->missionpoints += qp_number;

		    ch->pcdata->perm_move = ch->race->max_vitals[ MAX_MOVE ];
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

		if (ch->version < VERSION_PLAYER_010)
		{
			KEY("Inco",	__versioning->_010.incog_level,	fread_number(fp));
			KEY("Invi",	__versioning->_010.invis_level,	fread_number(fp));
		}

		KEY("Incognito", ch->incog_level, stat_lookup(fread_string(fp), staff_ranks, STAFF_PLAYER));

	    if (!str_cmp(word, "Ignore"))
	    {
	        IGNORE_DATA *ignore;

	        ignore = new_ignore();
	        ignore->next = ch->pcdata->ignoring;
	        ch->pcdata->ignoring = ignore;

	        ignore->name   = fread_string(fp);
	        ignore->reason = fread_string(fp);
	        fMatch = true;
	        break;
	    }
	#ifdef IMC
	if( ( fMatch = imc_loadchar( ch, fp, word ) ) )
	break;
	#endif
	    break;

	case 'L':
	    KEY("LastLevel",	ch->pcdata->last_level, fread_number(fp));
		if (!str_cmp(word, "LastArea"))
		{
			ch->pcdata->last_area = fread_string(fp);
			fMatch = TRUE;
		}
		if (!str_cmp(word, "LastRegion"))
		{
			ch->pcdata->last_region = fread_string(fp);
			fMatch = TRUE;
		}
	    KEY("LLev",	ch->pcdata->last_level, fread_number(fp));
	    //KEY("LogO",	lastlogoff,		fread_number(fp));
	    KEY("LogI",	ch->pcdata->last_login,	fread_number(fp));
	    KEY("LongDescr",	ch->long_descr,		fread_string(fp));
	    KEY("LnD",		ch->long_descr,		fread_string(fp));
	    KEY("LockerRent",	ch->locker_rent,	fread_number(fp));
	    KEY("LastInquiryRead",ch->pcdata->last_project_inquiry, fread_number(fp));
	    KEY("LostParts",	ch->lostparts,		fread_flag(fp));

	    // fix mistake where date got saved as a string to the file
	    if (!str_cmp(word, "LastLogin")) {
		fread_to_eol(fp);
		fMatch = true;
	    }

	    if (!str_cmp(word, "LogO"))
	    {
		long time;

		time = fread_number(fp);
		ch->pcdata->last_logoff = time;
		lastlogoff = time;
		fMatch = true;
		break;
	    }

	    break;

	case 'M':
		if (ch->version < VERSION_PLAYER_007)
		{
			KEY("Mc0",		 __versioning->_007.class_mage,		fread_number(fp));
			KEY("Mc1",		 __versioning->_007.class_cleric,		fread_number(fp));
			KEY("Mc2",		 __versioning->_007.class_thief,		fread_number(fp));
			KEY("Mc3",		 __versioning->_007.class_warrior,		fread_number(fp));
		}
		KEY("MFA_KEY",		ch->pcdata->mfa_key,		fread_string(fp));
		//KEY("MFA_Code_Expiration", ch->pcdata->qr_code_expiration, fread_number(fp));
		if (!str_cmp(word, "MFA_Enabled"))
			ch->pcdata->mfa_enabled = true;
		if (!str_cmp(word, "MFAPendingKey")) {
    		free_string(ch->pcdata->mfa_pending_key);
    		ch->pcdata->mfa_pending_key = str_dup(fread_string(fp));
    		fMatch = TRUE;
		}

		if (!str_cmp(word, "MFAPending")) {
		    ch->pcdata->mfa_pending = (fread_number(fp) != 0);
		    fMatch = TRUE;
		}
		KEY("MissionNext",   ch->nextmission,          fread_number(fp));
		KEY("MissionPnts",   ch->missionpoints,        fread_number(fp));
		KEY("MissionsCompleted", ch->pcdata->missions_completed, fread_number(fp));

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
		fMatch = true;
		break;
	    }

	    break;

	case 'O':
	    if (!str_cmp(word,"OwnerOfShip"))
	    {
		//ch->pcdata->owner_of_boat_before_logoff		= fread_string(fp);
		fMatch = true;
		break;
	    }

	    break;

	case 'P':
            KEYS("PendingEmail", ch->pcdata->pending_email, fread_string(fp));
            KEY("Password", ch->pcdata->pwd, fread_string(fp));
            KEY("Pass", ch->pcdata->pwd, fread_string(fp));
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

			fMatch = true;
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
			if (ch->version < VERSION_PLAYER_008)
			{
	            KEY("QuestPnts",   __versioning->_008.questpoints,        fread_number(fp));
			    KEY("QuestsCompleted", __versioning->_008.quests_completed, fread_number(fp));
			}

	    if (!str_cmp(word, "QuietTo"))
	    {
		STRING_DATA *string;

		string = new_string_data();
		string->next = ch->pcdata->quiet_people;
		ch->pcdata->quiet_people = string;

		string->string = fread_string(fp);
		fMatch = true;
		break;
	    }

		// This version update will wipe out their last active quest
#if 0
	    if (!str_cmp(word, "Questing"))
	    {
			ch->quest = (QUEST_DATA *)new_quest();
			fMatch = true;
			break;
		}

	    if (!str_cmp(word, "QuestGiverType"))
	    {
			if( ch->quest == NULL )
			{
				ch->quest = (QUEST_DATA *)new_quest();
			}
			ch->quest->questgiver_type = fread_number(fp);
			fMatch = true;
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
	
			fMatch = true;
			break;
	    }

	    if (!str_cmp(word, "QuestReceiverType"))
	    {
			if( ch->quest == NULL )
			{
				ch->quest = (QUEST_DATA *)new_quest();
			}
			ch->quest->questreceiver_type = fread_number(fp);
			fMatch = true;
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

			fMatch = true;
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
		fMatch = true;
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
		fMatch = true;
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
		fMatch = true;
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
		fMatch = true;
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
		fMatch = true;
		break;
	    }


	    if (!str_cmp(word, "QComplete"))
	    {
			if (ch->quest != NULL)
				ch->quest->parts->complete = true;
		fMatch = true;
	    }
#endif
            break;

	case 'R':
            if (!str_cmp(word, "RepopRoom")) {
				long auid = fread_number(fp);
				long vnum = fread_number(fp);
		location_set(&ch->recall,get_area_from_uid(auid),0,vnum,0,0);
		fMatch = true;
	    }
            if (!str_cmp(word, "RepopRoomC")) {
				long auid = fread_number(fp);
				long vnum = fread_number(fp);
				unsigned long id0 = fread_number(fp);
				unsigned long id1 = fread_number(fp);

		location_set(&ch->recall,get_area_from_uid(auid),0,vnum,id0,id1);
		fMatch = true;
	    }
            if (!str_cmp(word, "RepopRoomW")) {
				long wuid = fread_number(fp);
				long x = fread_number(fp);
				long y = fread_number(fp);
				long z = fread_number(fp);
		location_set(&ch->recall,NULL,wuid,x,y,z);
		fMatch = true;
	    }

			KEY("ResetCode",	ch->pcdata->reset_code,		fread_string(fp));
			KEY("Reset_Time",	ch->pcdata->reset_time,		fread_number(fp));
			KEY("ResetState",	ch->pcdata->reset_state,		fread_number(fp));

            if (!str_cmp(word, "Room_before_arena")) {
				long auid = fread_number(fp);
				long vnum = fread_number(fp);
		location_set(&ch->pcdata->room_before_arena,get_area_from_uid(auid),0,vnum,0,0);
		fMatch = true;
	    }
            if (!str_cmp(word, "Room_before_arenaC")) {
				long auid = fread_number(fp);
				long vnum = fread_number(fp);
				unsigned long id0 = fread_number(fp);
				unsigned long id1 = fread_number(fp);
		location_set(&ch->pcdata->room_before_arena,get_area_from_uid(auid),0,vnum,id0,id1);
		fMatch = true;
	    }
            if (!str_cmp(word, "Room_before_arenaW")) {
				long wuid = fread_number(fp);
				long x = fread_number(fp);
				long y = fread_number(fp);
				long z = fread_number(fp);
		location_set(&ch->pcdata->room_before_arena,NULL,wuid,x,y,z);
		fMatch = true;
	    }
		if (ch->version < VERSION_PLAYER_007)
		{
			KEY("RMc0",		 __versioning->_007.second_class_mage,		fread_number(fp));
			KEY("RMc1",		 __versioning->_007.second_class_cleric,	fread_number(fp));
			KEY("RMc2",		 __versioning->_007.second_class_thief,	fread_number(fp));
			KEY("RMc3",		 __versioning->_007.second_class_warrior,	fread_number(fp));
		}

            if (!str_cmp(word, "Race"))
	    {
                 race_string = fread_string(fp);
		 if (!str_cmp(race_string, "werewolf"))
		     ch->race = gr_sith;
		 else if (!str_cmp(race_string, "guru"))
	             ch->race = gr_mystic;
		 else if (!str_cmp(race_string, "high elf"))
		     ch->race = gr_seraph;
		 else if (!str_cmp(race_string, "high dwarf"))
		     ch->race = gr_berserker;
		 else if (!str_cmp(race_string, "shade"))
		     ch->race = gr_specter;
		 else
		     ch->race = get_race_data(race_string);

		 free_string(race_string);
                 fMatch = true;
                 break;
            }
		if (!str_cmp(word, "RecoveryCodes")) {
    		for (int i = 0; i < MFA_RECOVERY_CODES; ++i)
        		ch->pcdata->recovery_codes[i] = str_dup(fread_word(fp));
    		fMatch = TRUE;
		}
		if (!str_cmp(word, "RecoveryUsed")) {
    		for (int i = 0; i < MFA_RECOVERY_CODES; ++i)
        		ch->pcdata->recovery_used[i] = (fread_number(fp) != 0);
    		fMatch = TRUE;
		}
	    KEY("Resist", ch->res_flags,	fread_flag(fp));
	    KEY("ResistPerm", ch->res_flags_perm,	fread_flag(fp));

	    if (!str_cmp(word, "Room"))
	    {
			WNUM_LOAD w = fread_widevnum(fp, 0);
		ch->in_room = get_room_index_auid(w.auid, w.vnum);
		if ((ch->in_room == NULL) /*|| (ch->tot_level < 150 && !ch->in_room->area->open)*/)
		    ch->in_room = room_index_temple;
		fMatch = true;
		break;
	    }


	    fMatchFound = false;/*
	    for (i = 0; i < 3; i++)
            {
	        char buf2[MSL];

		sprintf(buf2, "Rank%d", i);
		if (!str_cmp(word, buf2))
		{
		    ch->pcdata->rank[i] = fread_number(fp);
		    fMatchFound = true;
		}

		sprintf(buf2, "Reputation%d", i);
		if (!str_cmp(word, buf2))
		{
		    ch->pcdata->reputation[i] = fread_number(fp);
		    fMatchFound = true;
		}
	    }*/

	    if (fMatchFound)
	    {
		fMatch = true;
		break;
	    }

	    break;

	case 'S':
		KEY("StaffRank", ch->pcdata->staff_rank, stat_lookup(fread_string(fp),staff_ranks,STAFF_PLAYER));
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

			fMatch = true;
			break;
		}

	    KEY("ShortDescr",	ch->short_descr,	fread_string(fp));
	    KEY("ShD",		ch->short_descr,	fread_string(fp));
	    KEY("Sec",         ch->pcdata->security,	fread_number(fp));
            KEY("Silv",        ch->silver,             fread_number(fp));
		if (ch->version < VERSION_PLAYER_007)
		{
		    KEY("Subcla",	__versioning->_007.sub_class_current,		fread_number(fp));
			KEY("SMc0",		__versioning->_007.sub_class_mage,	fread_number(fp));
			KEY("SMc1",		__versioning->_007.sub_class_cleric,	fread_number(fp));
			KEY("SMc2",		__versioning->_007.sub_class_thief,	fread_number(fp));
			KEY("SMc3",		__versioning->_007.sub_class_warrior,	fread_number(fp))
			KEY("SSMc0",	__versioning->_007.second_sub_class_mage,	fread_number(fp));
			KEY("SSMc1",	__versioning->_007.second_sub_class_cleric,	fread_number(fp));
			KEY("SSMc2",	__versioning->_007.second_sub_class_thief,	fread_number(fp));
			KEY("SSMc3",	__versioning->_007.second_sub_class_warrior,	fread_number(fp));
		}

	    if (!str_cmp(word, "Shifted"))
	    {
		char *temp;

		temp = fread_string(fp);

		if (!str_cmp(temp, "Werewolf"))
		    ch->shifted = SHIFTED_WEREWOLF;

		if (!str_cmp(temp, "Slayer"))
		    ch->shifted = SHIFTED_SLAYER;

		fMatch = true;
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

			fMatch = true;
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

			fMatch = true;
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

			fMatch = true;
			break;
	    }
		*/

/*
	    fMatchFound = false;
	    for (i = 0; i < 3; i++)
            {
	        char buf2[MSL];

		sprintf(buf2, "ShipQuestPoints%d", i);
		if (!str_cmp(word, buf2))
		{
		    ch->pcdata->ship_quest_points[i] = fread_number(fp);
		    fMatchFound = true;
		}
	    }
	    if (fMatchFound)
            {
		    fMatch = true;
		    break;
	    }
*/
	    break;

	case 'T':
            KEY("TrueSex",     ch->pcdata->true_sex,  	fread_number(fp));
	    KEY("TSex",	ch->pcdata->true_sex,   fread_number(fp));
	    KEY("Trai",	ch->train,		fread_number(fp));
	    KEY("TLevl",	ch->tot_level,		fread_number(fp));

	    if (!str_prefix("Toxn", word))
	    {
		fMatch = true;
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
			fMatch = true;
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

			fMatch = true;
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
		fMatch = true;
		break;
	    }

	    KEY("Vuln", ch->vuln_flags,	fread_flag(fp));
	    KEY("VulnPerm", ch->vuln_flags_perm,	fread_flag(fp));

	    if (!str_cmp(word, "Vnum"))
	    {
			WNUM_LOAD wnum_load = fread_widevnum(fp, 0);

			ch->pIndexData = get_mob_index_auid(wnum_load.auid, wnum_load.vnum);

			fMatch = true;
			break;
	    }
/*
	    if (!str_cmp(word,"VnumOfShip"))
	    {
		ch->pcdata->vnum_of_boat_before_logoff		= fread_number(fp);
		fMatch = true;
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

                fMatch = true;
                break;
            }
	    break;

	case 'W':
 	    KEY("WarsWon",	ch->wars_won,	fread_number(fp));
	    KEY("Wimpy",	ch->wimpy,		fread_number(fp));
	    KEY("Wimp",	ch->wimpy,		fread_number(fp));
		KEY("Wizinvis", ch->invis_level, stat_lookup(fread_string(fp), staff_ranks, STAFF_PLAYER));
	    KEY("Wizn",	ch->wiznet,		fread_flag(fp));

	    break;
	}

	if (!fMatch)
	{
	    snprintf(buf, sizeof(buf),
	    "Fread_char: no match for ch %s on word %s.", ch->name, word);
	    bug(buf, 0);
	    fread_to_eol(fp);
	}
    }


	// Make sure mission data from old info is configured properly
	ITERATOR mit;
	MISSION_DATA *mission;
	iterator_start(&mit, ch->missions);
	while((mission = (MISSION_DATA *)iterator_nextdata(&mit)))
	{
		if (!mission->giver.pArea || mission->giver.vnum < 1)
		{
			iterator_remcurrent(&mit);
		}
		else
		{
			if (mission->giver_type < 0) mission->giver_type = MISSIONARY_MOB;
			if (!mission->receiver.pArea || mission->receiver.vnum < 1)
			{
				mission->receiver = mission->giver;
				mission->receiver_type = mission->giver_type;
			}
		}
	}
	iterator_stop(&mit);
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

	if (IS_AMMO(obj))
	{
		fprintf(fp, "#TYPEAMMO\n");

		fprintf(fp, "Type %s~\n", flag_string(ammo_types, AMMO(obj)->type));

		if (!IS_NULLSTR(AMMO(obj)->msg_break))
			fprintf(fp, "MsgBreak %s~\n", fix_string(AMMO(obj)->msg_break));

		fprintf(fp, "Damage %s~ %s %d %d %d\n",
			attack_table[AMMO(obj)->damage_type].noun,
			print_flags(AMMO(obj)->flags),
			AMMO(obj)->damage.number,
			AMMO(obj)->damage.size,
			AMMO(obj)->damage.bonus);

		fprintf(fp, "#-TYPEAMMO\n");
	}

	if (IS_ARMOR(obj))
	{
		ARMOR_DATA *armor = ARMOR(obj);

		fprintf(fp, "#TYPEARMOR\n");

		fprintf(fp, "Type %s~\n", flag_string(armour_types, armor->armor_type));
		fprintf(fp, "Strength %s~\n", flag_string(armour_strength_table, armor->armor_strength));

		for(int i = 0; i < ARMOR_MAX; i++)
		{
			fprintf(fp, "Protection %s~ %d\n", flag_string(armour_protection_types, i), armor->protection[i]);
		}

		fprintf(fp, "MaxAdornments %d\n", armor->max_adornments);
		if (armor->max_adornments > 0 && armor->adornments != NULL)
		{
			for(int i = 0; i < armor->max_adornments; i++)
			{
				ADORNMENT_DATA *adorn = armor->adornments[i];
				if (IS_VALID(adorn))
				{
					fprintf(fp, "#ADORNMENT\n");
					fprintf(fp, "Type %s~\n", flag_string(adornment_types, adorn->type));
					fprintf(fp, "Name %s~\n", fix_string(adorn->name));
					fprintf(fp, "Short %s~\n", fix_string(adorn->short_descr));
					fprintf(fp, "Description %s~\n", fix_string(adorn->description));

					if (adorn->spell != NULL)
						fprintf(fp, "Spell %s~ %d\n", adorn->spell->skill->name, adorn->spell->level);

					fprintf(fp, "#-ADORNMENT\n");
				}
			}
		}

		fprintf(fp, "#-TYPEARMOR\n");
	}

	if (IS_BODY_PART(obj))
	{
		BODY_PART_DATA *bp = BODY_PART(obj);
		fprintf(fp, "#TYPEBODYPART\n");
		if (IS_VALID(bp->race))
			fprintf(fp, "Race %s~\n", bp->race->name);
		fprintf(fp, "Parts %s\n", print_flags(bp->parts));

		if (bp->id[0] > 0 || bp->id[1] > 0)
		{
			fprintf(fp, "ID %ld %ld\n", bp->id[0], bp->id[1]);
		}
		fprintf(fp, "#-TYPEBODYPART\n");
	}

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

	if (IS_CART(obj))
	{
		CART_DATA *cart = CART(obj);
		fprintf(fp, "#TYPECART\n");
		fprintf(fp, "Flags %s\n", print_flags(cart->flags));
		fprintf(fp, "MinStrength %d\n", cart->min_strength);
		fprintf(fp, "MoveDelay %d\n", cart->move_delay);
		fprintf(fp, "#-TYPECART\n");
	}

	if (IS_COMPASS(obj))
	{
		COMPASS_DATA *compass = COMPASS(obj);
		fprintf(fp, "#TYPECOMPASS\n");
		fprintf(fp, "Accuracy %d\n", compass->accuracy);
		fprintf(fp, "Wilds %ld\n", compass->wuid);
		fprintf(fp, "X %ld\n", compass->x);
		fprintf(fp, "Y %ld\n", compass->y);
		fprintf(fp, "#-TYPECOMPASS\n");
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

	if (IS_CORPSE(obj))
	{
		CORPSE_DATA *corpse = CORPSE(obj);
		fprintf(fp, "#TYPECORPSE\n");
		if (IS_VALID(corpse->type))
			fprintf(fp, "Type %s~\n", corpse->type->name);
		if (IS_VALID(corpse->race))
			fprintf(fp, "Race %s~\n", corpse->race->name);
		fprintf(fp, "Flags %s\n", print_flags(corpse->flags));
		if (corpse->player)
			fprintf(fp, "Player\n");
		fprintf(fp, "Resurrect %d\n", corpse->resurrect);
		fprintf(fp, "Animate %d\n", corpse->animate);
		fprintf(fp, "Parts %s\n", print_flags(corpse->parts));
		if (corpse->mobile)
			fprintf(fp, "Mobile %s\n", widevnum_string(corpse->mobile->area, corpse->mobile->vnum, NULL));
		fprintf(fp, "#-TYPECORPSE\n");
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
		fprintf(fp, "Flags %s\n", print_flags(FURNITURE(obj)->flags));
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

	if (IS_JEWELRY(obj))
	{
		fprintf(fp, "#TYPEJEWELRY\n");

		fprintf(fp, "MaxMana %d\n", JEWELRY(obj)->max_mana);

		ITERATOR sit;
		SPELL_DATA *spell;
		iterator_start(&sit, JEWELRY(obj)->spells);
		while((spell = (SPELL_DATA *)iterator_nextdata(&sit)))
		{
			fprintf(fp, "Spell %s~ %d\n", spell->skill->name, spell->level);
		}
		iterator_stop(&sit);

		fprintf(fp, "#-TYPEJEWELRY\n");
	}

	if (IS_LIGHT(obj))
	{
		fprintf(fp, "#TYPELIGHT\n");
		fprintf(fp, "Flags %s\n", print_flags(LIGHT(obj)->flags));
		fprintf(fp, "Duration %d\n", LIGHT(obj)->duration);
		fprintf(fp, "#-TYPELIGHT\n");
	}

	if (IS_MAP(obj))
	{
		fprintf(fp, "#TYPEMAP\n");
		fprintf(fp, "Wilds %ld\n", MAP(obj)->wuid);
		fprintf(fp, "X %ld\n", MAP(obj)->x);
		fprintf(fp, "Y %ld\n", MAP(obj)->y);

		ITERATOR wit;
		WAYPOINT_DATA *wp;
		iterator_start(&wit, MAP(obj)->waypoints);
		while((wp = (WAYPOINT_DATA *)iterator_nextdata(&wit)))
		{
			fprintf(fp, "Waypoint %lu %d %d %s~\n", wp->w, wp->x, wp->y, fix_string(wp->name));
		}
		iterator_stop(&wit);
		fprintf(fp, "#-TYPEMAP\n");
	}

	if (IS_MIST(obj))
	{
		fprintf(fp, "#TYPEMIST\n");
		fprintf(fp, "ObscureMobs %d\n", MIST(obj)->obscure_mobs);
		fprintf(fp, "ObscureObjs %d\n", MIST(obj)->obscure_objs);
		fprintf(fp, "ObscureRoom %d\n", MIST(obj)->obscure_room);

		fprintf(fp, "Icy %d\n", MIST(obj)->icy);
		fprintf(fp, "Fiery %d\n", MIST(obj)->fiery);
		fprintf(fp, "Acidic %d\n", MIST(obj)->acidic);
		fprintf(fp, "Stink %d\n", MIST(obj)->stink);
		fprintf(fp, "Wither %d\n", MIST(obj)->wither);
		fprintf(fp, "Toxic %d\n", MIST(obj)->toxic);
		fprintf(fp, "Shock %d\n", MIST(obj)->shock);
		fprintf(fp, "Fog %d\n", MIST(obj)->fog);
		fprintf(fp, "Sleep %d\n", MIST(obj)->sleep);

		fprintf(fp, "#-TYPEMIST\n");
	}

	if (IS_MONEY(obj))
	{
		fprintf(fp, "#TYPEMONEY\n");
		fprintf(fp, "Silver %d\n", MONEY(obj)->silver);
		fprintf(fp, "Gold %d\n", MONEY(obj)->gold);
		fprintf(fp, "#-TYPEMONEY\n");
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
		iterator_start(&sit, PORTAL(obj)->spells);
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

	if (IS_SEXTANT(obj))
	{
		SEXTANT_DATA *sextant = SEXTANT(obj);

		fprintf(fp, "#TYPESEXTANT\n");
		fprintf(fp, "Accuracy %d\n", sextant->accuracy);
		fprintf(fp, "#-TYPESEXTANT\n");
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

	if (IS_TELESCOPE(obj))
	{
		TELESCOPE_DATA *telescope = TELESCOPE(obj);
		fprintf(fp, "#TYPETELESCOPE\n");
		fprintf(fp, "Distance %d\n", telescope->distance);
		fprintf(fp, "MinDistance %d\n", telescope->min_distance);
		fprintf(fp, "MaxDistance %d\n", telescope->max_distance);
		fprintf(fp, "BonusView %d\n", telescope->bonus_view);
		if (telescope->heading >= 0)
			fprintf(fp, "Heading %d\n", telescope->heading);
		fprintf(fp, "#-TYPETELESCOPE\n");
	}

	if (IS_WAND(obj))
	{
		fprintf(fp, "#TYPEWAND\n");

		fprintf(fp, "MaxMana %d\n", WAND(obj)->max_mana);

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

	if (IS_WEAPON(obj))
	{
		fprintf(fp, "#TYPEWEAPON\n");

		fprintf(fp, "WeaponClass %s~\n", flag_string(weapon_class, WEAPON(obj)->weapon_class));
		fprintf(fp, "MaxMana %d\n", WEAPON(obj)->max_mana);
		fprintf(fp, "Charges %d\n", WEAPON(obj)->charges);
		fprintf(fp, "MaxCharges %d\n", WEAPON(obj)->max_charges);
		fprintf(fp, "Cooldown %d\n", WEAPON(obj)->cooldown);
		fprintf(fp, "RechargeTime %d\n", WEAPON(obj)->recharge_time);

		if (WEAPON(obj)->ammo != AMMO_NONE)
			fprintf(fp, "Ammo %s~\n", flag_string(ammo_types, WEAPON(obj)->ammo));
		if (WEAPON(obj)->range > 0)
			fprintf(fp, "Range %d\n", WEAPON(obj)->range);

		for(int i = 0; i < MAX_ATTACK_POINTS; i++)
		{
			if (WEAPON(obj)->attacks[i].type >= 0)
			{
				fprintf(fp, "Attack %d %s~ %s~ %s~ %s %d %d %d\n", i,
					fix_string(WEAPON(obj)->attacks[i].name),
					fix_string(WEAPON(obj)->attacks[i].short_descr),
					attack_table[WEAPON(obj)->attacks[i].type].noun,
					print_flags(WEAPON(obj)->attacks[i].flags),
					WEAPON(obj)->attacks[i].damage.number,
					WEAPON(obj)->attacks[i].damage.size,
					WEAPON(obj)->attacks[i].damage.bonus);
			}
		}

		ITERATOR sit;
		SPELL_DATA *spell;
		iterator_start(&sit, WEAPON(obj)->spells);
		while((spell = (SPELL_DATA *)iterator_nextdata(&sit)))
		{
			fprintf(fp, "Spell %s~ %d\n", spell->skill->name, spell->level);
		}
		iterator_stop(&sit);

		fprintf(fp, "#-TYPEWEAPON\n");

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
	fprintf(fp, "ExtF %ld\n",	obj->extra[0]	    );
	fprintf(fp, "Ext2F %ld\n",	obj->extra[1]	    );
	fprintf(fp, "Ext3F %ld\n",	obj->extra[2]	    );
	fprintf(fp, "Ext4F %ld\n",	obj->extra[3]	    );
    if (obj->wear_flags != obj->pIndexData->wear_flags)
        fprintf(fp, "WeaF %ld\n",	obj->wear_flags		    );
    if (obj->item_type != obj->pIndexData->item_type)
        fprintf(fp, "Ityp %d\n",	obj->item_type		    );
    if (obj->in_room != NULL)
    	fprintf(fp, "Room %s\n",	widevnum_string_room(obj->in_room, NULL)	    );
    if (IS_SET(obj->extra[1], ITEM_ENCHANTED))
		fprintf(fp,"Enchanted_times %d\n", obj->num_enchanted);
	if (obj->script_created)
	{
		fprintf(fp, "Created_script_type %d\n", obj->created_script_type);
		fprintf(fp, "Created_script_wnum %s\n", obj->created_script_wnum);
	}

	if (obj->creation_time)
	{
		fprintf(fp, "Creation_time %ld\n", obj->creation_time);
	}

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

    if (obj->locker == true) fprintf(fp, "Locker %d\n", obj->locker);

	if (IS_VALID(obj->clazz))
	    fprintf(fp, "Class %s~\n", obj->clazz->name);
	
	if (obj->clazz_type != CLASS_NONE)
		fprintf(fp, "ClassType %s~\n", flag_string(class_types, obj->clazz_type));

	if (list_size(obj->race) > 0)
	{
		ITERATOR rit;
		RACE_DATA *race;
		iterator_start(&rit, obj->race);
		while((race = (RACE_DATA *)iterator_nextdata(&rit)))
		{
			fprintf(fp, "Race %s~\n", race->name);
		}
		iterator_stop(&rit);
	}

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

	for (paf = obj->affected; paf != NULL; paf = paf->next)
	{
		// Saved with tokens
		if (IS_VALID(paf->token))
			continue;

		fwrite_affect(fp, paf);
	}

#if 0
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
#endif

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

	/*
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
	*/

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
	bool fMatch = false;

	lock = new_lock_state();

    while (str_cmp((word = fread_word(fp)), "#-LOCK"))
	{
		fMatch = false;

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
					fMatch = true;
					break;
				}
				break;
		}


		if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_lock_state: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return lock;
}

AMMO_DATA *fread_obj_ammo_data(FILE *fp)
{
	AMMO_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_ammo_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEAMMO"))
	{
		fMatch = false;

		switch(word[0])
		{
		case 'D':
			if (!str_cmp(word, "Damage"))
			{
				data->damage_type = attack_lookup(fread_string(fp));
				data->flags = fread_flag(fp);
				data->damage.number = fread_number(fp);
				data->damage.size = fread_number(fp);
				data->damage.bonus = fread_number(fp);
				data->damage.last_roll = 0;
				fMatch = true;
				break;
			}
			break;

		case 'M':
			KEYS("MsgBreak", data->msg_break, fread_string(fp));
			break;

		case 'T':
			KEY("Type", data->type, stat_lookup(fread_string(fp), ammo_types, AMMO_NONE));
			break;
		}

		if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_obj_ammo_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

ADORNMENT_DATA *fread_adornment_data(FILE *fp)
{
	ADORNMENT_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_adornment_data();

    while (str_cmp((word = fread_word(fp)), "#-ADORNMENT"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'D':
				KEYS("Description", data->description, fread_string(fp));
				break;

			case 'N':
				KEYS("Name", data->name, fread_string(fp));
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
						if (data->spell != NULL)
							free_spell(data->spell);

						SPELL_DATA *spell = new_spell();
						spell->skill = skill;
						spell->level = level;
						spell->repop = 100;
						spell->next = NULL;

						data->spell = spell;
					}

					fMatch = true;
					break;
				}
				break;

			case 'T':
				KEY("Type", data->type, stat_lookup(fread_string(fp),adornment_types,ADORNMENT_NONE));
				break;
		}

		if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_adornment_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

ARMOR_DATA *fread_obj_armor_data(FILE *fp)
{
	ARMOR_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_armor_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEARMOR"))
	{
		fMatch = false;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#ADORNMENT"))
				{
					ADORNMENT_DATA *adornment = fread_adornment_data(fp);

					bool found = false;
					for(int i = 0; i < data->max_adornments; i++)
					{
						if(data->adornments[i] == NULL)
						{
							data->adornments[i] = adornment;
							found = true;
							break;
						}
					}

					if (!found)
					{
						free_adornment_data(adornment);
						// Complain
					}

					fMatch = true;
					break;
				}
				break;

			case 'M':
				if (!str_cmp(word, "MaxAdornments"))
				{
					int max = fread_number(fp);

					if (data->adornments != NULL)
					{
						// complain about already being defined.
						break;
					}

					if (max >= 1 && max <= MAX_ADORNMENTS)
					{
						data->max_adornments = max;
						data->adornments = alloc_mem(max * sizeof(ADORNMENT_DATA *));
						for (int i = 0; i < max; i++)
							data->adornments[i] = NULL;
					}
					else
						data->max_adornments = 0;
					fMatch = true;
					break;
				}
				break;

			case 'P':
				if (!str_cmp(word,"Protection"))
				{
					int rating = stat_lookup(fread_string(fp),armour_protection_types,NO_FLAG);
					int value = fread_number(fp);

					if (rating != NO_FLAG)
						data->protection[rating] = value;

					fMatch = true;
					break;
				}
				break;

			case 'S':
				KEY("Strength", data->armor_strength, stat_lookup(fread_string(fp), armour_strength_table, OBJ_ARMOUR_NOSTRENGTH));
				break;

			case 'T':
				KEY("Type", data->armor_type, stat_lookup(fread_string(fp), armour_types, ARMOR_TYPE_NONE));
				break;
		}

		if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_obj_armor_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

BODY_PART_DATA *fread_obj_body_part_data(FILE *fp)
{
	BODY_PART_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_body_part_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEBODYPART"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'I':
				if (!str_cmp(word, "ID"))
				{
					long id0 = fread_number(fp);
					long id1 = fread_number(fp);

					data->id[0] = id0;
					data->id[1] = id1;

					fMatch = true;
					break;
				}
				break;
			case 'P':
				KEY("Parts", data->parts, fread_flag(fp));
				break;

			case 'R':
				KEY("Race", data->race, get_race_data(fread_string(fp)));
				break;
		}

		if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_obj_body_part_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
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
		fMatch = false;

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
			snprintf(buf, sizeof(buf), "fread_book_page: no match for word %s", word);
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
		fMatch = false;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#LOCK"))
				{
					book->lock = fread_lock_state(fp);
					fMatch = true;
					break;
				}
				if (!str_cmp(word, "#PAGE"))
				{
					BOOK_PAGE *page = fread_book_page(fp, "#-PAGE");

					if (!book_insert_page(book, page))
					{
						snprintf(buf, sizeof(buf), "fread_obj_book_data: page with duplicate page number (%d) found!  Discarding.", page->page_no);
						bug(buf, 0);
						free_book_page(page);	
					}
					fMatch = true;
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
			snprintf(buf, sizeof(buf), "fread_obj_book_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return book;
}

CART_DATA *fread_obj_cart_data(FILE *fp)
{
	CART_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_cart_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPECART"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'M':
				KEY("MinStrength", data->min_strength, fread_number(fp));
				KEY("MoveDelay", data->move_delay, fread_number(fp));
				break;
		}

		if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_obj_cart_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}


COMPASS_DATA *fread_obj_compass_data(FILE *fp)
{
	COMPASS_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_compass_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPECOMPASS"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'A':
				KEY("Accuracy", data->accuracy, fread_number(fp));
				break;

			case 'W':
				KEY("Wilds", data->wuid, fread_number(fp));
				break;
			
			case 'X':
				KEY("X", data->x, fread_number(fp));
				break;
			
			case 'Y':
				KEY("Y", data->y, fread_number(fp));
				break;
		}

		if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_obj_compass_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}


CONTAINER_DATA *fread_obj_container_data(FILE *fp)
{
	CONTAINER_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch = false;

	data = new_container_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPECONTAINER"))
	{
		fMatch = false;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#LOCK"))
				{
					if (data->lock) free_lock_state(data->lock);

					data->lock = fread_lock_state(fp);
					fMatch = true;
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

					fMatch = true;
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

					fMatch = true;
					break;
				}
				break;
		}

		if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_obj_container_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

CORPSE_DATA *fread_obj_corpse_data(FILE *fp)
{
	CORPSE_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_corpse_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPECORPSE"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'A':
				KEY("Animate", data->animate, fread_number(fp));
				break;

			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'M':
				if (!str_cmp(word, "Mobile"))
				{
					WNUM_LOAD wnum = fread_widevnum(fp, 0);

					if (wnum.auid > 0 && wnum.vnum > 0)
					{
						MOB_INDEX_DATA *mob = get_mob_index_auid(wnum.auid, wnum.vnum);

						if (mob)
						{
							data->mobile = mob;
						}
						else
						{
							// TODO: Complain
						}

					}
					else
						data->mobile = NULL;

					fMatch = true;
					break;
				}
				KEY("Mobile", data->mobile_load, fread_widevnum(fp, 0));
				break;

			case 'P':
				KEY("Parts", data->parts, fread_flag(fp));
				KEY("Player", data->player, true);
				break;

			case 'R':
				KEY("Race", data->race, get_race_data(fread_string(fp)));
				KEY("Resurrect", data->resurrect, fread_number(fp));
				break;

			case 'T':
				KEY("Type", data->type, get_corpse_type(fread_string(fp)));
				break;
		}

		if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_obj_corpse_data: no match for word %s", word);
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
		fMatch = false;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#LOCK"))
				{
					if (data->lock) free_lock_state(data->lock);

					data->lock = fread_lock_state(fp);
					fMatch = true;
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
			snprintf(buf, sizeof(buf), "fread_obj_fluid_container_data: no match for word %s", word);
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
		fMatch = false;

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
			snprintf(buf, sizeof(buf), "read_food_buff: no match for word %s", word);
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
		fMatch = false;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#FOODBUFF"))
				{
					FOOD_BUFF_DATA *buff = fread_food_buff(fp);

					list_appendlink(data->buffs, buff);

					fMatch = true;
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
			snprintf(buf, sizeof(buf), "fread_obj_food_data: no match for word %s", word);
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
		bool fMatch = false;
		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#LOCK"))
				{
					if (data->lock) free_lock_state(data->lock);

					data->lock = fread_lock_state(fp);
					fMatch = true;
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
			snprintf(buf, sizeof(buf), "fread_furniture_compartment: no match for word %s", word);
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
		bool fMatch = false;
		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#COMPARTMENT"))
				{
					FURNITURE_COMPARTMENT *compartment = fread_furniture_compartment(fp);
					list_appendlink(data->compartments, compartment);

					compartment->ordinal = list_size(data->compartments);

					fMatch = true;
					break;
				}
				break;

			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'M':
				KEY("MainCompartment", data->main_compartment, fread_number(fp));
				break;
		}

		if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_obj_furniture_data: no match for word %s", word);
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
		fMatch = false;

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
			snprintf(buf, sizeof(buf), "fread_obj_ink_data: no match for word %s", word);
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
		fMatch = false;

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
			snprintf(buf, sizeof(buf), "fread_obj_instrument_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

JEWELRY_DATA *fread_obj_jewelry_data(FILE *fp)
{
	JEWELRY_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_jewelry_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEJEWELRY"))
	{
		fMatch = false;

		switch(word[0])
		{
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
			snprintf(buf, sizeof(buf), "fread_obj_jewelry_data: no match for word %s", word);
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
		fMatch = false;

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
			snprintf(buf, sizeof(buf), "fread_obj_light_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

MAP_DATA *fread_obj_map_data(FILE *fp)
{
	MAP_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_map_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEMAP"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'W':
				if (!str_cmp(word, "Waypoint"))
				{
					WAYPOINT_DATA *wp = new_waypoint();

					wp->w = fread_number(fp);
					wp->x = fread_number(fp);
					wp->y = fread_number(fp);
					wp->name = fread_string(fp);

					list_appendlink(data->waypoints, wp);
					fMatch = true;
					break;
				}
				KEY("Wilds", data->wuid, fread_number(fp));
				break;
			
			case 'X':
				KEY("X", data->x, fread_number(fp));
				break;
			
			case 'Y':
				KEY("Y", data->y, fread_number(fp));
				break;
		}

		if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_obj_map_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}


MIST_DATA *fread_obj_mist_data(FILE *fp)
{
	MIST_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_mist_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEMIST"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'A':
				KEY("Acidic", data->acidic, fread_number(fp));
				break;

			case 'F':
				KEY("Fiery", data->fiery, fread_number(fp));
				KEY("Fog", data->fog, fread_number(fp));
				break;

			case 'I':
				KEY("Icy", data->icy, fread_number(fp));
				break;

			case 'O':
				KEY("ObscureMobs", data->obscure_mobs, fread_number(fp));
				KEY("ObscureObjs", data->obscure_objs, fread_number(fp));
				KEY("ObscureRoom", data->obscure_room, fread_number(fp));
				break;

			case 'S':
				KEY("Shock", data->shock, fread_number(fp));
				KEY("Sleep", data->sleep, fread_number(fp));
				KEY("Stink", data->stink, fread_number(fp));
				break;

			case 'T':
				KEY("Toxic", data->toxic, fread_number(fp));
				break;

			case 'W':
				KEY("Wither", data->wither, fread_number(fp));
				break;
		}

		if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_obj_mist_data: no match for word %s", word);
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
		fMatch = false;

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
			snprintf(buf, sizeof(buf), "fread_obj_money_data: no match for word %s", word);
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
		fMatch = false;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#LOCK"))
				{
					if (!data->lock) free_lock_state(data->lock);
					data->lock = fread_lock_state(fp);
					fMatch = true;
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

					fMatch = true;
					break;
				}
				break;

			case 'S':
				KEYS("Short", data->short_descr, fread_string(fp));

				if (!str_cmp(word, "SpellNew"))
				{
					SKILL_DATA *skill = get_skill_data(fread_string(fp));

					fMatch = true;
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

					fMatch = true;
					break;
				}
				break;
		}

		if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_obj_portal_data: no match for word %s", word);
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
		fMatch = false;

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
			snprintf(buf, sizeof(buf), "fread_obj_scroll_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

SEXTANT_DATA *fread_obj_sextant_data(FILE *fp)
{
	SEXTANT_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_sextant_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPESEXTANT"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'A':
				KEY("Accuracy", data->accuracy, fread_number(fp));
				break;
		}

		if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_obj_sextant_data: no match for word %s", word);
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
		fMatch = false;

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
			snprintf(buf, sizeof(buf), "fread_obj_tattoo_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

TELESCOPE_DATA *fread_obj_telescope_data(FILE *fp)
{
	TELESCOPE_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_telescope_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPETELESCOPE"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'B':
				KEY("BonusView", data->bonus_view, fread_number(fp));
				break;

			case 'D':
				KEY("Distance", data->distance, fread_number(fp));
				break;

			case 'H':
				KEY("Heading", data->heading, fread_number(fp));
				break;

			case 'M':
				KEY("MaxDistance", data->max_distance, fread_number(fp));
				KEY("MinDistance", data->min_distance, fread_number(fp));
				break;
		}

		if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_obj_telescope_data: no match for word %s", word);
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
		fMatch = false;

		switch(word[0])
		{
			case 'C':
				KEY("Charges", data->charges, fread_number(fp));
				KEY("Cooldown", data->cooldown, fread_number(fp));
				break;

			case 'M':
				KEY("MaxMana", data->max_mana, fread_number(fp));
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
			snprintf(buf, sizeof(buf), "fread_obj_wand_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}


WEAPON_DATA *fread_obj_weapon_data(FILE *fp)
{
	WEAPON_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_weapon_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEWEAPON"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'A':
				KEY("Ammo", data->ammo, stat_lookup(fread_string(fp), ammo_types, AMMO_NONE));
				if (!str_cmp(word, "Attack"))
				{
					int index = fread_number(fp);
					char *name = fread_string(fp);
					char *short_descr = fread_string(fp);
					int16_t type = attack_lookup(fread_string(fp));
					long flags = fread_flag(fp);
					int number = fread_number(fp);
					int size = fread_number(fp);
					int bonus = fread_number(fp);

					if (index >= 0 && index < MAX_ATTACK_POINTS)
					{
						data->attacks[index].type = type;
						data->attacks[index].name = name;
						data->attacks[index].short_descr = short_descr;
						data->attacks[index].flags = flags;
						data->attacks[index].damage.number = number;
						data->attacks[index].damage.size = size;
						data->attacks[index].damage.bonus = bonus;
						data->attacks[index].damage.last_roll = 0;
					}
					fMatch = true;
					break;
				}
				break;

			case 'C':
				KEY("Charges", data->charges, fread_number(fp));
				KEY("Cooldown", data->cooldown, fread_number(fp));
				break;

			case 'M':
				KEY("MaxCharges", data->max_charges, fread_number(fp));
				KEY("MaxMana", data->max_mana, fread_number(fp));
				break;

			case 'R':
				KEY("Range", data->range, fread_number(fp));
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

			case 'W':
				KEY("WeaponClass", data->weapon_class, stat_lookup(fread_string(fp), weapon_class, WEAPON_EXOTIC));
				break;
		}

		if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_obj_weapon_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}



void fread_obj_reset_multityping(OBJ_DATA *obj)
{
	free_ammo_data(AMMO(obj));				AMMO(obj) = NULL;
	free_armor_data(ARMOR(obj));			ARMOR(obj) = NULL;
	free_body_part_data(BODY_PART(obj));	BODY_PART(obj) = NULL;
	free_book_data(BOOK(obj));				BOOK(obj) = NULL;
	free_compass_data(COMPASS(obj));		COMPASS(obj) = NULL;
	free_container_data(CONTAINER(obj));	CONTAINER(obj) = NULL;
	free_corpse_data(CORPSE(obj));			CORPSE(obj) = NULL;
	free_fluid_container_data(FLUID_CON(obj));	FLUID_CON(obj) = NULL;
	free_food_data(FOOD(obj));				FOOD(obj) = NULL;
	free_furniture_data(FURNITURE(obj));	FURNITURE(obj) = NULL;
	free_ink_data(INK(obj));				INK(obj) = NULL;
	free_instrument_data(INSTRUMENT(obj));	INSTRUMENT(obj) = NULL;
	free_jewelry_data(JEWELRY(obj));		JEWELRY(obj) = NULL;
	free_light_data(LIGHT(obj));			LIGHT(obj) = NULL;
	free_map_data(MAP(obj));				MAP(obj) = NULL;
	free_mist_data(MIST(obj));				MIST(obj) = NULL;
	free_money_data(MONEY(obj));			MONEY(obj) = NULL;
	free_book_page(PAGE(obj));				PAGE(obj) = NULL;
	free_portal_data(PORTAL(obj));			PORTAL(obj) = NULL;
	free_scroll_data(SCROLL(obj));			SCROLL(obj) = NULL;
	free_tattoo_data(TATTOO(obj));			TATTOO(obj) = NULL;
	free_telescope_data(TELESCOPE(obj));	TELESCOPE(obj) = NULL;
	free_wand_data(WAND(obj));				WAND(obj) = NULL;
	free_weapon_data(WEAPON(obj));			WEAPON(obj) = NULL;
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
#if 0
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
#endif
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
			int16_t amounts[CATALYST_MAX];

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

	if (obj->version < VERSION_OBJECT_016)
	{
		int new_fragility = obj->fragility;
		switch(obj->fragility)
		{
			case OLD_OBJ_FRAGILE_WEAK: new_fragility = OBJ_FRAGILE_WEAK; break;
			case OLD_OBJ_FRAGILE_NORMAL: new_fragility = OBJ_FRAGILE_NORMAL; break;
			case OLD_OBJ_FRAGILE_STRONG: new_fragility = OBJ_FRAGILE_STRONG; break;
			case OLD_OBJ_FRAGILE_SOLID: new_fragility = OBJ_FRAGILE_SOLID; break;
		}

		obj->fragility = new_fragility;
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

	fVnum = false;
	obj = NULL;
	first = true;  /* used to counter fp offset */
	make_new = false;

	word   = feof(fp) ? "End" : fread_word(fp);
	if (!str_cmp(word,"Vnum"))
	{
		WNUM_LOAD w = fread_widevnum(fp, 0);
		first = false;  /* fp will be in right place */

		OBJ_INDEX_DATA *index = get_obj_index_auid(w.auid, w.vnum);
		if ( index != NULL)
			obj = create_object_noid(index,-1, false, false, false);

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
	list_clear(obj->race);	// Remove what the index gives, because the saved copy might have.. other restrictions...

	fVnum		= true;
	iNest		= 0;

	for (; ;)
	{
		if (first)
			first = false;
		else if(feof(fp))
		{
			bug("EOF encountered reading object from pfile", 0);
			word = "End";
		} else
			word   = fread_word(fp);
		fMatch = false;

//		snprintf(buf, sizeof(buf), "Fread_obj_new: word = '%s'", word);
//		bug(buf, 0);

		switch (UPPER(word[0]))
		{
		case '*':
			fMatch = true;
			fread_to_eol(fp);
			break;
		case '#':
			if (!str_cmp(word, "#STACHE"))
			{
				fread_stache(fp, obj->lstache);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#AFFECT"))
			{
				AFFECT_DATA *paf = fread_affect(fp);
				paf->next = obj->affected;
				obj->affected = paf;
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TOKEN"))
			{
				TOKEN_DATA *token = fread_token(fp);
				if (token)
				{
					token_to_obj(token, obj);

					// Add all affects on the token to the character
					ITERATOR ait;
					AFFECT_DATA *taf;
					iterator_start(&ait, token->affects);
					while((taf = (AFFECT_DATA *)iterator_nextdata(&ait)))
					{
						taf->next = obj->affected;
						obj->affected = taf;
					}
					iterator_stop(&ait);
				}

				fMatch		= true;
				break;
			}
			else if (!str_cmp(word, "#TYPEAMMO"))
			{
				if (IS_AMMO(obj)) free_ammo_data(AMMO(obj));

				AMMO(obj) = fread_obj_ammo_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPEARMOR"))
			{
				if (IS_ARMOR(obj)) free_armor_data(ARMOR(obj));

				ARMOR(obj) = fread_obj_armor_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPEBODYPART"))
			{
				if (IS_BODY_PART(obj)) free_body_part_data(BODY_PART(obj));

				BODY_PART(obj) = fread_obj_body_part_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPEBOOK"))
			{
				if (IS_BOOK(obj)) free_book_data(BOOK(obj));

				BOOK(obj) = fread_obj_book_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPECART"))
			{
				if (IS_CART(obj)) free_cart_data(CART(obj));

				CART(obj) = fread_obj_cart_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPECOMPASS"))
			{
				if (IS_COMPASS(obj)) free_compass_data(COMPASS(obj));

				COMPASS(obj) = fread_obj_compass_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPECONTAINER"))
			{
				if (IS_CONTAINER(obj)) free_container_data(CONTAINER(obj));

				CONTAINER(obj) = fread_obj_container_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPECORPSE"))
			{
				if (IS_CORPSE(obj)) free_corpse_data(CORPSE(obj));

				CORPSE(obj) = fread_obj_corpse_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPEFLUIDCONTAINER"))
			{
				if (IS_FLUID_CON(obj)) free_fluid_container_data(FLUID_CON(obj));

				FLUID_CON(obj) = fread_obj_fluid_container_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPEFOOD"))
			{
				if (IS_FOOD(obj)) free_food_data(FOOD(obj));

				FOOD(obj) = fread_obj_food_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPEFURNITURE"))
			{
				if (IS_FURNITURE(obj)) free_furniture_data(FURNITURE(obj));

				FURNITURE(obj) = fread_obj_furniture_data(fp);

				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPEINK"))
			{
				if (IS_INK(obj)) free_ink_data(INK(obj));

				INK(obj) = fread_obj_ink_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPEINSTRUMENT"))
			{
				if (IS_INSTRUMENT(obj)) free_instrument_data(INSTRUMENT(obj));

				INSTRUMENT(obj) = fread_obj_instrument_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPEJEWELRY"))
			{
				if (IS_JEWELRY(obj)) free_jewelry_data(JEWELRY(obj));

				JEWELRY(obj) = fread_obj_jewelry_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPELIGHT"))
			{
				if (IS_LIGHT(obj)) free_light_data(LIGHT(obj));

				LIGHT(obj) = fread_obj_light_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPEMAP"))
			{
				if (IS_MAP(obj)) free_map_data(MAP(obj));

				MAP(obj) = fread_obj_map_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPEMIST"))
			{
				if (IS_MIST(obj)) free_mist_data(MIST(obj));

				MIST(obj) = fread_obj_mist_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPEMONEY"))
			{
				if (IS_MONEY(obj)) free_money_data(MONEY(obj));

				MONEY(obj) = fread_obj_money_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPEPAGE"))
			{
				if (IS_PAGE(obj)) free_book_page(PAGE(obj));

				PAGE(obj) = fread_book_page(fp, "#-TYPEPAGE");
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPEPORTAL"))
			{
				if (IS_PORTAL(obj)) free_portal_data(PORTAL(obj));

				PORTAL(obj) = fread_obj_portal_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPESCROLL"))
			{
				if (IS_SCROLL(obj)) free_scroll_data(SCROLL(obj));

				SCROLL(obj) = fread_obj_scroll_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPESEXTANT"))
			{
				if (IS_SEXTANT(obj)) free_sextant_data(SEXTANT(obj));

				SEXTANT(obj) = fread_obj_sextant_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPETATTOO"))
			{
				if (IS_TATTOO(obj)) free_tattoo_data(TATTOO(obj));

				TATTOO(obj) = fread_obj_tattoo_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPETELESCOPE"))
			{
				if (IS_TELESCOPE(obj)) free_telescope_data(TELESCOPE(obj));

				TELESCOPE(obj) = fread_obj_telescope_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPEWAND"))
			{
				if (IS_WAND(obj)) free_wand_data(WAND(obj));

				WAND(obj) = fread_obj_wand_data(fp);
				fMatch = true;
				break;
			}
			else if (!str_cmp(word, "#TYPEWEAPON"))
			{
				if (IS_WEAPON(obj)) free_weapon_data(WEAPON(obj));

				WEAPON(obj) = fread_obj_weapon_data(fp);
				fMatch = true;
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
				fMatch		= true;
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
				fMatch          = true;
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
				fMatch          = true;
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
				fMatch          = true;
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
				fMatch          = true;
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
				fMatch = true;
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
				fMatch = true;
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
				fMatch = true;
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
				fMatch = true;
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
				fMatch = true;
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
				fMatch = true;
				break;
			}
			KEY("Class", obj->clazz, get_class_data(fread_string(fp)));
			KEY("ClassType", obj->clazz_type, stat_lookup(fread_string(fp), class_types, CLASS_NONE));
			KEY("Cond",	obj->condition,		fread_number(fp));
			KEY("Cost",	obj->cost,		fread_number(fp));

			if (!str_cmp(word, "Created_script_type"))
			{
				obj->created_script_type = fread_number(fp);
				obj->script_created = true;
				fMatch = true;
				break;
			}

			if (!str_cmp(word, "Created_script_wnum"))
			{
				obj->created_script_wnum = fread_string(fp);
				obj->script_created = true;
				fMatch = true;
				break;
			}
			KEY("Creation_time", obj->creation_time, fread_number(fp));
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
				fMatch = true;
				break;
			}

			if (!str_cmp(word, "Extra2Flags") || !str_cmp(word, "Ext2F"))
			{
				obj->extra[1] = fread_number(fp);
				fMatch = true;
				break;
			}

			if (!str_cmp(word, "Extra3Flags") || !str_cmp(word, "Ext3F"))
			{
				obj->extra[2] = fread_number(fp);
				fMatch = true;
				break;
			}

			if (!str_cmp(word, "Extra4Flags") || !str_cmp(word, "Ext4F"))
			{
				obj->extra[3] = fread_number(fp);
				fMatch = true;
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
				fMatch = true;
			}

			if (!str_cmp(word, "ExtraDescrEnv") || !str_cmp(word,"ExDeEnv"))
			{
				EXTRA_DESCR_DATA *ed;

				ed = new_extra_descr();

				ed->keyword		= fread_string(fp);
				ed->description		= NULL;
				ed->next		= obj->extra_descr;
				obj->extra_descr	= ed;
				fMatch = true;
			}

			if (!str_cmp(word, "End"))
			{
				if ((fVnum && obj->pIndexData == NULL))
				{
					bug("Fread_obj: incomplete object.", 0);
					free_obj(obj);
					return NULL;
				}
				else if (is_duplicate_object(obj))
				{
const char *where = "Unknown";
if (obj->carried_by && obj->carried_by->name)
    where = obj->carried_by->name;
else if (obj->in_room && obj->in_room->name)
    where = obj->in_room->name;
else if (obj->in_obj && obj->in_obj->short_descr)
    where = obj->in_obj->short_descr;


log_stringf("Duplicate object detected: %s (id %ld, id2 %ld, vnum %ld) for %s. Skipping.",
    obj->short_descr, obj->id[0], obj->id[1],
    obj->pIndexData ? obj->pIndexData->vnum : 0,
    where);
					free_obj(obj);
					return NULL;
				}

				else
				{
					if (!fVnum)
					{
						free_obj(obj);
						return NULL;
						//obj = create_object(obj_index_dummy, 0 , false);
					}
				
					if (!list_haslink(loaded_objects, obj))
					{
						list_appendlink(loaded_objects, obj);
						obj->pIndexData->count++;
					}
					fread_obj_check_version(obj, values);

					if (make_new)
					{
						int wear;

						wear = obj->wear_loc;
						extract_obj(obj);

						obj = create_object(obj->pIndexData,0, false);

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
				fMatch = true;
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
					key = create_object(pIndexData, pIndexData->level, false);
					obj_to_obj(key, obj);
				}

				fMatch = true;
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

				fMatch = true;
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

				fMatch = true;
			}

			KEY("LoadedBy",	obj->loaded_by,		fread_string(fp));
			break;
		case 'M':
			/*
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

				fMatch = true;
				break;
			}
			*/
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
				fMatch = true;
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
			KEY("Persist", obj->persist, fread_number(fp));
			break;

		case 'R':
			if (!str_cmp(word, "Race")) {
				char *name = fread_string(fp);
				RACE_DATA *race = get_race_data(name);

				if (IS_VALID(race) && !list_contains(obj->race, race, NULL))
					list_appendlink(obj->race, race);

				fMatch = true;
				break;
			}
			if (!str_cmp(word, "Room"))
			{
				ROOM_INDEX_DATA *room;
				WNUM_LOAD w = fread_widevnum(fp, 0);

				room = get_room_index_auid(w.auid, w.vnum);
				obj->in_room = room;
				fMatch = true;
			}
			break;

		case 'S':
			KEY("ShortDescr",	obj->short_descr,	fread_string(fp));
			KEY("ShD",		obj->short_descr,	fread_string(fp));

			if (!str_cmp(word, "SpellNew"))
			{
				SKILL_DATA *skill;
				SPELL_DATA *spell;

				fMatch = true;

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
					snprintf(buf, sizeof(buf), "Bad spell name for %s (%ld#%ld).", obj->short_descr, obj->pIndexData->area->uid, obj->pIndexData->vnum);
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
				fMatch		= true;

				for(int i = 0; i < MAX_OBJVALUES; i++)
					values[i] = fread_number(fp);

				break;
			}

			if( (vtype = variable_fread_type(word)) != VAR_UNKNOWN ) {
				variable_fread(&obj->progs->vars, vtype, fp);
				fMatch = true;
			}

			if (!str_cmp(word, "Vnum"))
			{
				WNUM_LOAD wnum = fread_widevnum(fp, 0);

				// TODO: fix bug
				if ((obj->pIndexData = get_obj_index_auid(wnum.auid, wnum.vnum)) == NULL)
					bug("Fread_obj: bad vnum %ld.", wnum.vnum);
				else
					fVnum = true;

				fMatch = true;
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
			//snprintf(buf, sizeof(buf), "fread_obj: unknown obj flag %s", word);
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
         return false;

     if (obj->pIndexData->update == true)
	 return true;

     //if (obj->pIndexData->item_type == ITEM_WEAPON)
       //  return true;

     return false;
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
		snprintf(buf, sizeof(buf), "cleanup_affects: obj %s (%ld)",
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
	if (IS_BOOK(obj) && BOOK(obj)->lock)
		fix_lockstate(BOOK(obj)->lock);

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

	if (IS_PORTAL(obj) && PORTAL(obj)->lock)
		fix_lockstate(PORTAL(obj)->lock);

	if (obj->contains)
	{
		OBJ_DATA *content;
		for (content = obj->contains; content != NULL; content = content->next_content)
			fix_object_lockstate(content);
	}
}

static inline void __add_class_level(CHAR_DATA *ch, int sub_class, struct __player_data_versioning *__versioning)
{
	if (sub_class == -1) return;

	int level = MAX_CLASS_LEVEL;
	long xp = 0;
	if (!IS_IMMORTAL(ch) && sub_class == __versioning->_007.sub_class_current)
	{
		level = 1;
		xp = 0;		// Current experience
	}

	CLASS_DATA *clazz = get_class_data(__sub_class_table[sub_class].name[0]);
	if (!IS_VALID(clazz)) return;

	CLASS_LEVEL *cl = new_class_level();
	cl->clazz = clazz;
	cl->level = level;
	cl->xp = xp;

	insert_class_level(ch, cl);
}

void fix_character(CHAR_DATA *ch, struct __player_data_versioning *__versioning)
{
    int i;
    char buf[MSL];
    bool resetaffects = false;
	AFFECT_DATA *paf;
	OBJ_DATA *obj;

    if (!IS_VALID(ch->race))
		ch->race = gr_human;

    //ch->size = ch->race->min_size;
    ch->dam_type = 17; /*punch */

	LLIST *groups = ch->pcdata->group_known;
	ch->pcdata->group_known = list_create(false);
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
	ITERATOR skit;
	SKILL_DATA *skill;
	iterator_start(&skit, ch->race->skills);
	while((skill = (SKILL_DATA *)iterator_nextdata(&skit)))
	{
		skill_add(ch, skill);
	}
	iterator_stop(&skit);

	// TODO: Readd checks for dealing with racial affects, affects2, imm, res and vuln

	/* 20203003 - Tieryo - Fix missing racial perm affects */

	if (ch->affected_by_perm[0] != ch->race->aff[0] || ch->affected_by_perm[1] != ch->race->aff[1])
	{
	ch->affected_by_perm[0] = ch->race->aff[0];
	ch->affected_by_perm[1] = ch->race->aff[1];
	resetaffects = true;
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
			ITERATOR it;
			OBJ_DATA *obj;
			iterator_start(&it, ch->lworn);
			while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
    		for (paf = obj->affected; paf; paf = paf->next) {
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
			iterator_stop(&it);
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

    ch->form = ch->race->form;
    ch->parts = ch->race->parts & ~ch->lostparts;
	ch->lostparts = 0;

    if (ch->version < 2)
    {
		group_add(ch,"global skills",false);
		group_add(ch,__class_table[__versioning->_007.class_current].base_group,false);
		ch->version = 2;
    }

#if 0
    /* make sure they have any new skills that have been added */
    if (ch->pcdata->class_mage != -1)		group_add(ch, class_table[ch->pcdata->class_mage].base_group, false);
    if (ch->pcdata->class_cleric != -1)		group_add(ch, class_table[ch->pcdata->class_cleric].base_group, false);
    if (ch->pcdata->class_thief != -1)		group_add(ch, class_table[ch->pcdata->class_thief].base_group, false);
    if (ch->pcdata->class_warrior != -1)	group_add(ch, class_table[ch->pcdata->class_warrior].base_group, false);

    if (ch->pcdata->second_sub_class_mage != -1)	group_add(ch, sub_class_table[ch->pcdata->second_sub_class_mage].default_group, false);
    if (ch->pcdata->second_sub_class_cleric != -1)	group_add(ch, sub_class_table[ch->pcdata->second_sub_class_cleric].default_group, false);
    if (ch->pcdata->second_sub_class_thief != -1)	group_add(ch, sub_class_table[ch->pcdata->second_sub_class_thief].default_group, false);
    if (ch->pcdata->second_sub_class_warrior != -1)	group_add(ch, sub_class_table[ch->pcdata->second_sub_class_warrior].default_group, false);
#endif

    if (ch->version < 6)
		ch->version = 6;

    /* reset affects */
    if (ch->version < 7)
    {
		if (IS_AFFECTED2(ch, AFF2_ENSNARE))
			REMOVE_BIT(ch->affected_by[1], AFF2_ENSNARE);

		//if (ch->pcdata->second_sub_class_thief == CLASS_THIEF_SAGE)
		//	SET_BIT(ch->affected_by[0], AFF_DETECT_HIDDEN);

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

	if (ch->version < VERSION_PLAYER_006)
	{
		// Create all of the class level entries
		__add_class_level(ch, __versioning->_007.sub_class_cleric, __versioning);
		__add_class_level(ch, __versioning->_007.sub_class_mage, __versioning);
		__add_class_level(ch, __versioning->_007.sub_class_thief, __versioning);
		__add_class_level(ch, __versioning->_007.sub_class_warrior, __versioning);

		__add_class_level(ch, __versioning->_007.second_sub_class_cleric, __versioning);
		__add_class_level(ch, __versioning->_007.second_sub_class_mage, __versioning);
		__add_class_level(ch, __versioning->_007.second_sub_class_thief, __versioning);
		__add_class_level(ch, __versioning->_007.second_sub_class_warrior, __versioning);

		ch->version = VERSION_PLAYER_006;
	}

	ITERATOR clit, sgit;
	CLASS_LEVEL *cl;
	SKILL_GROUP *sg;
	iterator_start(&clit, ch->pcdata->classes);
	while((cl = (CLASS_LEVEL *)iterator_nextdata(&clit)))
	{
		iterator_start(&sgit, cl->clazz->groups);
		while((sg = (SKILL_GROUP *)iterator_nextdata(&sgit)))
		{
			gn_add(ch, sg);
		}
		iterator_stop(&sgit);
	}
	iterator_stop(&clit);

	if (ch->version < VERSION_PLAYER_007)
	{
		ch->version = VERSION_PLAYER_007;
	}

	if (ch->version < VERSION_PLAYER_008)
	{
		ch->missionpoints = __versioning->_008.questpoints;
		ch->pcdata->missions_completed = __versioning->_008.quests_completed;
		ch->version = VERSION_PLAYER_008;
	}

	if (ch->version < VERSION_PLAYER_010)
	{
		if (ch->tot_level >= OLD_LEVEL_MINIGOD)
		{
			switch(ch->tot_level)
			{
			default:					ch->pcdata->staff_rank = STAFF_IMMORTAL; break;
			case OLD_LEVEL_ASCENDANT:	ch->pcdata->staff_rank = STAFF_ASCENDANT; break;
			case OLD_LEVEL_SUPREMACY:	ch->pcdata->staff_rank = STAFF_SUPREMACY; break;
			case OLD_LEVEL_CREATOR:		ch->pcdata->staff_rank = STAFF_CREATOR; break;
			case OLD_LEVEL_IMPLEMENTOR:	ch->pcdata->staff_rank = STAFF_IMPLEMENTOR; break;
			}
		}
		else
			ch->pcdata->staff_rank = STAFF_PLAYER;


		switch(__versioning->_010.invis_level)
		{
			default:					ch->invis_level = STAFF_PLAYER; break;
			case OLD_LEVEL_MINIGOD:		ch->invis_level = STAFF_IMMORTAL; break;
			case OLD_LEVEL_GOD:			ch->invis_level = STAFF_IMMORTAL; break;
			case OLD_LEVEL_ASCENDANT:	ch->invis_level = STAFF_ASCENDANT; break;
			case OLD_LEVEL_SUPREMACY:	ch->invis_level = STAFF_SUPREMACY; break;
			case OLD_LEVEL_CREATOR:		ch->invis_level = STAFF_CREATOR; break;
			case OLD_LEVEL_IMPLEMENTOR:	ch->invis_level = STAFF_IMPLEMENTOR; break;
		}

		switch(__versioning->_010.incog_level)
		{
			default:					ch->incog_level = STAFF_PLAYER; break;
			case OLD_LEVEL_MINIGOD:		ch->incog_level = STAFF_IMMORTAL; break;
			case OLD_LEVEL_GOD:			ch->incog_level = STAFF_IMMORTAL; break;
			case OLD_LEVEL_ASCENDANT:	ch->incog_level = STAFF_ASCENDANT; break;
			case OLD_LEVEL_SUPREMACY:	ch->incog_level = STAFF_SUPREMACY; break;
			case OLD_LEVEL_CREATOR:		ch->incog_level = STAFF_CREATOR; break;
			case OLD_LEVEL_IMPLEMENTOR:	ch->incog_level = STAFF_IMPLEMENTOR; break;
		}

		ch->tot_level = 0;
		ITERATOR lit;
		CLASS_LEVEL *level;
		iterator_start(&lit, ch->pcdata->classes);
		while((level = (CLASS_LEVEL *)iterator_nextdata(&lit)))
		{
			if (!IS_SET(level->clazz->flags, CLASS_NO_LEVEL))
			{
				ch->tot_level += level->level;
			}
		}
		iterator_stop(&lit);
	}

    if (IS_IMMORTAL(ch))
    {
		i = 0;
		while (wiznet_table[i].name != NULL)
		{
			if (get_staff_rank(ch) < wiznet_table[i].rank)
			{
			REMOVE_BIT(ch->wiznet, wiznet_table[i].flag);
			}

			i++;
		}
    }

    for (i = 0; i < MAX_STATS; i++)
		if (ch->perm_stat[i] > ch->race->max_stats[i])
	    	set_perm_stat(ch, i, ch->race->max_stats[i]);

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



#if 0
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

    if (classes < (IS_REMORT(ch)?120:ch->tot_level) / 31 + 1) return true;
    else return false;
}
#endif

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
	snprintf(buf, sizeof(buf), "descrew_subclasses: %s had a non-mage class!",
		ch->name);
	bug(buf, 0);
	if (ch->pcdata->group_known[group_lookup("necromancer skills")] == true)
	    ch->pcdata->sub_class_mage = CLASS_MAGE_NECROMANCER;
	else if (ch->pcdata->group_known[group_lookup("sorcerer skills")] == true)
	    ch->pcdata->sub_class_mage = CLASS_MAGE_SORCERER;
	else if (ch->pcdata->group_known[group_lookup("wizard skills")] == true)
	    ch->pcdata->sub_class_mage = CLASS_MAGE_WIZARD;

	//sprintf(buf, "{WYou had a screwed up mage class... it has been fixed to {Y%s.{x\n\r",
	//    sub_class_table[ch->pcdata->sub_class_mage].name);
	//send_to_char(buf, ch);
    }

    if (missing_class(ch) || (ch->pcdata->sub_class_cleric != -1
    && (ch->pcdata->sub_class_cleric < 6 || ch->pcdata->sub_class_cleric > 8)))
    {
	snprintf(buf, sizeof(buf), "descrew_subclasses: %s had a non-cleric class!",
		ch->name);
	bug(buf, 0);
	if (ch->pcdata->group_known[group_lookup("witch skills")] == true)
	    ch->pcdata->sub_class_cleric = CLASS_CLERIC_WITCH;
	else if (ch->pcdata->group_known[group_lookup("druid skills")] == true)
	    ch->pcdata->sub_class_cleric = CLASS_CLERIC_DRUID;
	else if (ch->pcdata->group_known[group_lookup("monk skills")] == true)
	    ch->pcdata->sub_class_cleric = CLASS_CLERIC_MONK;

//	sprintf(buf, "{WYou had a screwed up cleric class... it has been fixed to {Y%s.{x\n\r",
//	    sub_class_table[ch->pcdata->sub_class_cleric].name);
//	send_to_char(buf, ch);
    }

    if (missing_class(ch) || (ch->pcdata->sub_class_thief != -1
    && (ch->pcdata->sub_class_thief < 9 || ch->pcdata->sub_class_thief > 11)))
    {
	snprintf(buf, sizeof(buf), "descrew_subclasses: %s had a non-thief class!",
		ch->name);
	bug(buf, 0);

	if (ch->pcdata->group_known[group_lookup("assassin skills")] == true)
	    ch->pcdata->sub_class_thief = CLASS_THIEF_ASSASSIN;
	if (ch->pcdata->group_known[group_lookup("rogue skills")] == true)
	    ch->pcdata->sub_class_thief = CLASS_THIEF_ROGUE;
	if (ch->pcdata->group_known[group_lookup("bard skills")] == true)
	    ch->pcdata->sub_class_thief = CLASS_THIEF_BARD;

//	sprintf(buf, "{WYou had a screwed up thief class... it has been fixed to {Y%s.{x\n\r",
//	    sub_class_table[ch->pcdata->sub_class_thief].name);
//	send_to_char(buf, ch);
    }

    if (missing_class(ch) || (ch->pcdata->sub_class_thief != -1
    && (ch->pcdata->sub_class_warrior > 2)))
    {
	snprintf(buf, sizeof(buf), "descrew_subclasses: %s had a non-warrior class!",
		ch->name);
	bug(buf, 0);
	if (ch->pcdata->group_known[group_lookup("marauder skills")] == true)
	    ch->pcdata->sub_class_warrior = CLASS_WARRIOR_MARAUDER;
	if (ch->pcdata->group_known[group_lookup("gladiator skills")] == true)
	    ch->pcdata->sub_class_warrior = CLASS_WARRIOR_GLADIATOR;
	if (ch->pcdata->group_known[group_lookup("paladin skills")] == true)
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
#if 0
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
	return false;
    } else
#endif
	return true;
}


void fix_broken_classes(CHAR_DATA *ch)
{
#if 0
    char buf[MSL];
    int mage    = ch->pcdata->class_mage;
    int cleric  = ch->pcdata->class_cleric;
    int thief   = ch->pcdata->class_thief;
    int warrior = ch->pcdata->class_warrior;

    sprintf(buf, "fix_broken_classes: fixing broken classes for %s, a level %d %s.",
        ch->name, ch->tot_level, ch->race->name);
    log_string(buf);

    if (mage == -1 && find_class_skill(ch, CLASS_MAGE) == true) {
	//send_to_char("Found mage skills but no mage class, setting mage class.\n\r", ch);
	log_string("Set mage class");
	ch->pcdata->class_mage = CLASS_MAGE;
	group_add(ch, "mage skills", false);
    }

    if (cleric == -1 && find_class_skill(ch, CLASS_CLERIC) == true) {
	//send_to_char("Found cleric skills but no cleric class, setting cleric class.\n\r", ch);
	log_string("Set cleric class");
	ch->pcdata->class_cleric = CLASS_CLERIC;
	group_add(ch, "cleric skills", false);
    }

    if (thief == -1 && find_class_skill(ch, CLASS_THIEF) == true) {
	//send_to_char("Found thief skills but no thief class, setting thief class.\n\r", ch);
	log_string("Set thief class");
	ch->pcdata->class_thief = CLASS_THIEF;
	group_add(ch, "thief skills", false);
    }

    if (warrior == -1 && find_class_skill(ch, CLASS_WARRIOR) == true) {
	//send_to_char("Found warrior skills but no warrior class, setting warrior class.\n\r", ch);
	log_string("Set warrior class");
	ch->pcdata->class_warrior = CLASS_WARRIOR;
	group_add(ch, "warrior skills", false);
    }

    save_char_obj(ch);
    //send_to_char("All fixed!\n\r", ch);
#endif
}

#if 0
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
	    return false;
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
#endif


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

	// All affects associated with tokens
	ITERATOR it;
	AFFECT_DATA *paf;
	iterator_start(&it, token->affects);
	while((paf = (AFFECT_DATA *)iterator_nextdata(&it)))
	{
		if (!paf->custom_name && !IS_VALID(paf->skill))
			continue;

		fwrite_affect(fp, paf);
	}
	iterator_stop(&it);

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
		snprintf(buf, sizeof(buf), "fread_token: no token index found for widevnum %ld#%ld", wnum.auid, wnum.vnum);
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

    variable_copylist(&token_index->index_vars,&token->progs->vars,false);

    for (; ;)
    {
		word   = feof(fp) ? "End" : fread_word(fp);
		fMatch = false;

		if (!str_cmp(word, "End")) {
			get_token_id(token);
			fMatch = true;
			return token;
		}

		switch (UPPER(word[0]))
		{
			case '#':
				if (!str_cmp(word, "#AFFECT"))
				{
					AFFECT_DATA *paf = fread_affect(fp);
					paf->token = token;
					list_appendlink(token->affects, paf);
					fMatch = true;
					break;
				}
				break;

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
				fMatch = true;
			}

			if( (vtype = variable_fread_type(word)) != VAR_UNKNOWN ) {
				variable_fread(&token->progs->vars, vtype, fp);
				fMatch = true;
			}

			break;
		}

	    if (!fMatch) {
			snprintf(buf, sizeof(buf), "read_token: no match for word %s", word);
			bug(buf, 0);
			fread_to_eol(fp);
	    }
    }

    return token;
}

void fwrite_skill(CHAR_DATA *ch, SKILL_ENTRY *entry, FILE *fp)
{
	if (entry->skill)
		fprintf(fp, "#SKILLENTRY %s~\n", entry->skill->name);
	else if (entry->song)
		fprintf(fp, "#SONGENTRY %s~\n", entry->song->name);
	else
		return;
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

	fprintf(fp, "Rating %d %d\n", entry->rating, entry->mod_rating);
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

void fread_skill(FILE *fp, CHAR_DATA *ch, bool is_song)
{
    TOKEN_DATA *token = NULL;
    SKILL_DATA *skill = NULL;
	SKILL_ENTRY *entry = NULL;
    SONG_DATA *song = NULL;
    long flags = SKILL_AUTOMATIC;
    int rating = -1, mod = 0;
    char source = SKILLSRC_NORMAL;
    char buf[MSL];
    char *word;
    bool fMatch;

	char *skill_name = fread_string(fp);
	if (is_song)
		song = get_song_data(skill_name);
	else
		skill = get_skill_data(skill_name);

    for (; ;)
    {
		word   = feof(fp) ? "End" : fread_word(fp);
		fMatch = false;

		if (!str_cmp(word, "End")) {
			if (!song && !skill)
			{
				// No skill nor song
				if (IS_VALID(token))
					free_token(token);

				log_stringf("fread_skill: no %s named '%s' found.\n\r", (is_song?"song":"skill"), skill_name);
				return;
			}

			if(IS_VALID(token))
				token_to_char(token, ch);

			if( IS_VALID(song) ) {
				entry = skill_entry_addsong(ch, song, token, source, flags);
			} else if(IS_VALID(skill)) {
				//log_stringf("Adding %s '%s' to %s", (is_skill_spell(skill)?"Spell":"Skill"), skill->name, ch->name);
				if( is_skill_spell(skill) )
					entry = skill_entry_addspell(ch, skill, token, source, flags);
				else
					entry = skill_entry_addskill(ch, skill, token, source, flags);
			}

			entry->rating = rating;
			entry->mod_rating = mod;
			
		    fMatch = true;
			return;
		}

		switch (UPPER(word[0]))
		{
		case '#':
			if( IS_KEY("#TOKEN") ) {
				token = fread_token(fp);
				fMatch = true;
				break;
			}
			break;

		case 'F':
			FVKEY("Flags",	flags, fread_string_eol(fp), skill_entry_flags);
			break;

		case 'R':
			if (!str_cmp(word, "Rating"))
			{
				rating = fread_number(fp);
				mod = fread_number(fp);
				fMatch = true;
				break;
			}
			break;

		case 'T':
			if(IS_KEY("TypeAffect"))
			{
				source = SKILLSRC_AFFECT;
				fMatch = true;
				break;
			}
			if(IS_KEY("TypeScript"))
			{
				source = SKILLSRC_SCRIPT;
				fMatch = true;
				break;
			}
			if(IS_KEY("TypeScriptPerm"))
			{
				source = SKILLSRC_SCRIPT_PERM;
				fMatch = true;
				break;
			}
			break;
		}

	    if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_skill: no match for word %s", word);
			bug(buf, 0);
			fread_to_eol(fp);
	    }
	}

}

void fread_reputation(FILE *fp, CHAR_DATA *ch)
{
    char buf[MSL];
    char *word;
    bool fMatch;

	WNUM_LOAD wnum_load = fread_widevnum(fp, 0);
	REPUTATION_INDEX_DATA *repIndex = get_reputation_index_auid(wnum_load.auid, wnum_load.vnum);
	
	REPUTATION_DATA *rep = new_reputation_data();
	rep->pIndexData = repIndex;

	while(str_cmp((word = fread_word(fp)), "#-REPUTATION"))
    {
		fMatch = false;

		switch (UPPER(word[0]))
		{
		case '#':
			if( IS_KEY("#TOKEN") ) {
				rep->token = fread_token(fp);
				fMatch = true;
				break;
			}
			break;

		case 'F':
			KEY("Flags", rep->flags, fread_flag(fp));
			break;

		case 'M':
			KEY("MaximumRank", rep->maximum_rank, fread_number(fp));
			break;

		case 'P':
			KEY("ParagonLevel", rep->paragon_level, fread_number(fp));
			break;

		case 'R':
			KEY("Rank", rep->current_rank, fread_number(fp));
			KEY("Reputation", rep->reputation, fread_number(fp));
			break;
		}

	    if (!fMatch) {
			snprintf(buf, sizeof(buf), "fread_reputation: no match for word %s", word);
			bug(buf, 0);
			fread_to_eol(fp);
	    }
	}

	if (IS_VALID(rep->token))
	{
		rep->token->reputation = rep;
		token_to_char(rep->token, ch);
	}

	list_appendlink(ch->reputations, rep);
}

/* write a quest to disk */

#if 0
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
		fMatch = false;

		if (!str_cmp(word, "End")) {
			fMatch = true;
			return part;
		}

		switch (UPPER(word[0]))
		{
		    case 'M':
			if (!str_cmp(word, "MPart")) {
				wnum = fread_widevnum(fp, 0);
				part->area = get_area_from_uid(wnum.auid);
			    part->mob = wnum.vnum;
			    fMatch = true;
			    break;
			}

			if (!str_cmp(word, "MRPart")) {
				wnum = fread_widevnum(fp, 0);
				part->area = get_area_from_uid(wnum.auid);
			    part->mob_rescue = wnum.vnum;
			    fMatch = true;
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

			    obj = create_object(obj_i, 1, true);
			    obj_to_room(obj, room);

			    part->pObj = obj;

			    fMatch = true;
			    break;
			}

			if (!str_cmp(word, "OSPart")) {
				wnum = fread_widevnum(fp, 0);
				part->area = get_area_from_uid(wnum.auid);
			    part->obj_sac = wnum.vnum;
			    fMatch = true;
			    break;
			}
			break;

		    case 'Q':
			if (!str_cmp(word, "QCustom")) {
				part->custom_task = true;
				fMatch = true;
				break;
			}
			if (!str_cmp(word, "QDescription")) {
				part->description = fread_string(fp);
				fMatch = true;
				break;
			}
			if (!str_cmp(word, "QRoom")) {
				wnum = fread_widevnum(fp, 0);
				part->area = get_area_from_uid(wnum.auid);
			    part->room = wnum.vnum;
			    fMatch = true;
			    break;
			}

			if (!str_cmp(word, "QComplete")) {
			    part->complete = true;
			    fMatch = true;
			    fread_to_eol(fp);
			    break;
			}

			break;
		}

	    if (!fMatch) {
		    snprintf(buf, sizeof(buf), "read_quest_part: no match for word %s", word);
		    bug(buf, 0);
		    fread_to_eol(fp);
	    }
    }
}
#endif
/*
 * Load an account from disk.
 */
bool load_account(DESCRIPTOR_DATA *d, char *name)
{
    char strsave[MAX_INPUT_LENGTH];
    char buf[MSL];
    ACCOUNT_DATA *account;
    FILE *fp;
    bool found;

    // Create a new account structure
    account = new_account();
    d->account = account;
    account->username = str_dup(name);
    account->creation_date = 0;
    account->passwd = str_dup("");
    account->passwd_version = 0;
    account->reset_code = str_dup("");
    account->reset_state = 0;
    account->mfa_key = str_dup("");
    //account->qr_code_expiration = 0;
    account->email = str_dup("");
    account->last_login = 0;
    account->acct_flags = 0;
    account->characters = list_create(false);

	    account->email_verified = false;
    account->pending_email = str_dup("");
    account->email_verification_code = str_dup("");
    account->email_verification_time = 0;
    account->email_verification_last_sent = 0;

    found = false;
    fclose(fpReserve);

    /* decompress if .gz file exists */
    sprintf(strsave, "%s%c/%s%s", ACCOUNT_DIR, tolower(name[0]), capitalize(name), ".gz");
    if ((fp = fopen(strsave, "r")) != NULL)
    {
        fclose(fp);
        sprintf(buf, "gzip -dfq %s", strsave);
        system(buf);
    }

    sprintf(strsave, "%s%c/%s", ACCOUNT_DIR, tolower(name[0]), capitalize(name));
    if ((fp = fopen(strsave, "r")) != NULL) {
        found = true;
        for (;;)
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
                bug("Load_account: # not found.", 0);
                break;
            }

            word = fread_word(fp);
            if (!str_cmp(word, "ACCOUNT"))
                fread_account(account, fp);
            else if (!str_cmp(word, "CHARACTER"))
                fread_account_character(account, fp);
            else if (!str_cmp(word, "VAULT"))
            {
                // Process vault items
                OBJ_DATA *obj;
                OBJ_DATA *objNestList[MAX_NEST];
                int iNest;

                for (iNest = 0; iNest < MAX_NEST; iNest++)
                    objNestList[iNest] = NULL;
                
                while (!str_cmp((word = fread_word(fp)), "#O"))
                {
                    obj = fread_obj_new(fp);
                    if (obj == NULL)
                        continue;
                    
                    objNestList[obj->nest] = obj;
                    
                    if (obj->nest == 0) {
                        obj->next_content = account->vault_items;
                        account->vault_items = obj;
                    } else {
                        OBJ_DATA *container = objNestList[obj->nest - 1];
                        if (IS_CONTAINER(container))
                            obj_to_obj(obj, container);
                        else {
                            sprintf(buf, "load_account: found obj %s in non-container in vault",
                                obj->short_descr);
                            log_string(buf);
                            // Put it at top level if container is invalid
                            obj->next_content = account->vault_items;
                            account->vault_items = obj;
                        }
                    }
                }
                
                // Skip to end of vault section
                while (str_cmp(word, "#ENDVAULT"))
                {
                    word = fread_word(fp);
                }
            }
            else if (!str_cmp(word, "END"))
                break;
            else {
                bug("Load_account: bad section.", 0);
                break;
            }
        }

        fclose(fp);
    }
    
    fpReserve = fopen(NULL_FILE, "r");

    if (!account->creation_date)
        account->creation_date = get_pc_id();
        
    // Generate account IDs if needed
    if (account->id[0] == 0 || account->id[1] == 0)
        get_account_id(account);

    account->character_count = list_size(account->characters);
    account->staff_account = FALSE;
    ITERATOR cit;
    ACCOUNT_CHARACTER *ch_entry;
    iterator_start(&cit, account->characters);
    while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&cit))) {
        if (ch_entry->staff && ch_entry->staff_rank >= STAFF_IMMORTAL) {
            account->staff_account = TRUE;
            break;
        }
    }
    iterator_stop(&cit);

ITERATOR it;
//ACCOUNT_CHARACTER *next_entry;

iterator_start(&it, account->characters);
while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
//    next_entry = (ACCOUNT_CHARACTER *)iterator_peek_nextdata(&it); // Save next in case we remove
    if (should_purge_deleted_character(ch_entry)) {
        // Load the character file
        DESCRIPTOR_DATA temp_d;
        memset(&temp_d, 0, sizeof(temp_d));
        if (load_char_obj(&temp_d, ch_entry->name) && temp_d.character) {
            delete_character(temp_d.character); // This handles unlinking and file move
            free_char(temp_d.character);
        }
        list_remlink(account->characters, ch_entry, false);
        free_account_character(ch_entry);
    }
}
iterator_stop(&it);

    // Add to loaded_accounts list if found
    if (found && loaded_accounts)
	{
		if (!list_haslink(loaded_accounts, account))
        list_appendlink(loaded_accounts, account);
	}
    account->last_login = current_time;
    return found;

}

/*
 * Read account data from file.
 */
void fread_account(ACCOUNT_DATA *account, FILE *fp)
{
    char buf[MAX_STRING_LENGTH];
    char *word;
    bool fMatch;

    sprintf(buf,"save.c, fread_account: reading %s.", account->username);
    log_string(buf);

    for (;;)
    {
        word   = feof(fp) ? "End" : fread_word(fp);
        fMatch = false;

        switch (UPPER(word[0]))
        {
        case '*':
            fMatch = true;
            fread_to_eol(fp);
            break;
			case '#':
			 if (!str_cmp(word, "#ACCNOTE"))
{
    ACCOUNT_NOTE_DATA *note = alloc_mem(sizeof(ACCOUNT_NOTE_DATA));
    note->author = str_dup("");
    note->subject = str_dup("");
    note->text = str_dup("");
    note->timestamp = current_time;
    
    for (;;)
    {
        word = feof(fp) ? "#END_NOTE" : fread_word(fp);
        
        if (!str_cmp(word, "#END_NOTE") || !str_cmp(word, "#END"))
            break;
            
        if (!str_cmp(word, "Author"))
        {
            free_string(note->author);
            note->author = fread_string(fp);
        }
        else if (!str_cmp(word, "Subject"))
        {
            free_string(note->subject);
            note->subject = fread_string(fp);
        }
        else if (!str_cmp(word, "Text"))
        {
            free_string(note->text);
            note->text = fread_string(fp);
        }
        else if (!str_cmp(word, "Timestamp"))
        {
            note->timestamp = fread_number(fp);
        }
    }
    
    // Add to the beginning of the list
    note->next = account->staff_notes;
    account->staff_notes = note;
    fMatch = TRUE;
}
break;

        case 'C':
			if (!str_cmp(word, "CharCount")) {
    			account->character_count = fread_number(fp);
    			fMatch = TRUE;
			}
			KEY("CharacterLimit", account->character_limit, fread_number(fp));
            KEY("Created", account->creation_date, fread_number(fp));
			if (!str_cmp(word, "CreationIP")) {
 			   free_string(account->creation_host);
    			account->creation_host = fread_string(fp);
    			fMatch = TRUE;
			}
            break;

        case 'E':
            if (!str_cmp(word, "End")) {
                return;
            }
                KEYS("Email", account->email, fread_string(fp));
                KEY("EmailVerified", account->email_verified, fread_number(fp));
                KEYS("EmailVerificationCode", account->email_verification_code, fread_string(fp));
                KEY("EmailVerificationTime", account->email_verification_time, fread_number(fp));
                KEY("EmailVerificationLastSent", account->email_verification_last_sent, fread_number(fp));
            break;

        case 'F':
            KEY("Flags", account->acct_flags, fread_flag(fp));
            break;
		case 'I':
			KEY("Id", account->id[0], fread_number(fp));
			KEY("Id2", account->id[1], fread_number(fp));
			break;

        case 'L':
			if (!str_cmp(word, "LastHost")) {
    			free_string(account->last_login_host);
    			account->last_login_host = fread_string(fp);
    			fMatch = TRUE;
			}
			if (!str_cmp(word, "LastLogin")) {
    			account->last_login = fread_number(fp);
    			fMatch = TRUE;
			}
            KEY("LogI", account->last_login, fread_number(fp));
            break;

        case 'M':
            KEY("MFA_Key", account->mfa_key, fread_string(fp));
            //KEY("MFA_Code_Expiration", account->qr_code_expiration, fread_number(fp));
            if (!str_cmp(word, "MFA_Enabled"))
                account->mfa_enabled = true;
			if (!str_cmp(word, "MFAPendingKey")) {
    			free_string(account->mfa_pending_key);
    			account->mfa_pending_key = str_dup(fread_string(fp));
    			fMatch = TRUE;
			}
			if (!str_cmp(word, "MFAPending")) {
    			account->mfa_pending = (fread_number(fp) != 0);
    			fMatch = TRUE;
			}
            break;

        case 'N':
            KEY("Name", account->username, fread_string(fp));
            break;

        case 'P':
                KEY("Password", account->passwd, fread_string(fp));
                KEY("Pass", account->passwd, fread_string(fp));
                KEY("PassVers", account->passwd_version, fread_number(fp));
                KEYS("PendingEmail", account->pending_email, fread_string(fp));
            break;

        case 'R':
			if (!str_cmp(word, "RecoveryCodes")) {
    			for (int i = 0; i < MFA_RECOVERY_CODES; ++i)
        			account->recovery_codes[i] = str_dup(fread_word(fp));
    			fMatch = TRUE;
			}
			if (!str_cmp(word, "RecoveryUsed")) {
    			for (int i = 0; i < MFA_RECOVERY_CODES; ++i)
        			account->recovery_used[i] = (fread_number(fp) != 0);
    			fMatch = TRUE;
			}
            KEYS("ResetCode", account->reset_code, fread_string(fp));
            KEY("Reset_Time", account->reset_time, fread_number(fp));
            KEY("ResetState", account->reset_state, fread_number(fp));
            break;

		case 'V':
            KEY("VaultRent", account->vault_rent, fread_number(fp));
            break;
		}

        if (!fMatch) {
            sprintf(buf, "Fread_account: no match for account %s on word %s.", 
                account->username, word);
            bug(buf, 0);
            fread_to_eol(fp);
        }
    }
}

void fread_account_character(ACCOUNT_DATA *account, FILE *fp)
{
    ACCOUNT_CHARACTER *acct_char;
    char buf[MAX_STRING_LENGTH];
    char *word;
    bool fMatch;
    
    acct_char = new_account_character();
    
    for (;;)
    {
        word = feof(fp) ? "End" : fread_word(fp);
        fMatch = false;
        
        switch (UPPER(word[0]))
        {
        case '*':
            fMatch = true;
            fread_to_eol(fp);
            break;
            
        case 'C':
            KEY("Created", acct_char->creation_date, fread_number(fp));
            KEYS("Class", acct_char->class_name, fread_string(fp));
            break;
		
		case 'D':
			KEY("Deleted", acct_char->deleted, fread_number(fp));
			KEY("DeleteTime", acct_char->delete_time, fread_number(fp));
            
        case 'E':
            if (!str_cmp(word, "End")) {
                // Only add if we have a character name
                if (!IS_NULLSTR(acct_char->name)) {
                    list_appendlink(account->characters, acct_char);
                } else {
                    free_account_character(acct_char);
                }
                return;
            }
            break;
            
        case 'I':
            KEY("Id", acct_char->id[0], fread_number(fp));
            KEY("Id2", acct_char->id[1], fread_number(fp));
            break;
            
        case 'L':
			if (!str_cmp(word, "LastArea"))
			{
				acct_char->last_area = fread_string(fp);
				fMatch = TRUE;
			}
			if (!str_cmp(word, "LastRegion"))
			{
				acct_char->last_region = fread_string(fp);
				fMatch = TRUE;
			}
            KEY("LastLogin", acct_char->last_login, fread_number(fp));
			KEY("LastHost", acct_char->last_host, fread_string(fp));
			KEY("LastLogoff", acct_char->last_logoff, fread_number(fp));
            KEY("Level", acct_char->current_level, fread_number(fp));
            break;
            
        case 'N':
            KEYS("Name", acct_char->name, fread_string(fp));
            break;
            
        case 'R':
            KEYS("Race", acct_char->race_name, fread_string(fp));
            break;
        
        case 'S':
            if (!str_cmp(word, "Staff")) {
                acct_char->staff = true;
                fMatch = true;
                break;
            }
            if (!str_cmp(word, "StaffRank")) {
                acct_char->staff_rank = stat_lookup(fread_string(fp), staff_ranks, STAFF_PLAYER);
                fMatch = true;
                break;
            }
            break;
		case 'T':
			KEY("TLevel", acct_char->tot_level, fread_number(fp));
			break;
        }
        
        if (!fMatch) {
            sprintf(buf, "Fread_account_character: no match for character %s on word %s.", 
                acct_char->name ? acct_char->name : "unknown", word);
            bug(buf, 0);
            fread_to_eol(fp);
        }
    }
}

/*
 * Write an account data structure to a file.
 */
void fwrite_account(ACCOUNT_DATA *account, FILE *fp)
{
    fprintf(fp, "#ACCOUNT\n");
    
    fprintf(fp, "Name %s~\n", account->username);
	fprintf(fp, "Id   %ld\n", account->id[0]			);
    fprintf(fp, "Id2  %ld\n", account->id[1]			);
    fprintf(fp, "Password %s~\n", account->passwd);
    fprintf(fp, "PassVers %d\n", account->passwd_version);
    fprintf(fp, "Created %ld\n", account->creation_date);
    fprintf(fp, "LogI %ld\n", account->last_login);
    
    if (account->acct_flags != 0)
        fprintf(fp, "Flags %s\n", print_flags(account->acct_flags));
    
    if (!IS_NULLSTR(account->email))
        fprintf(fp, "Email %s~\n", account->email);
    
    fprintf(fp, "EmailVerified %d\n", account->email_verified);
    
    if (!IS_NULLSTR(account->pending_email))
        fprintf(fp, "PendingEmail %s~\n", account->pending_email);
        
    if (!IS_NULLSTR(account->email_verification_code))
        fprintf(fp, "EmailVerificationCode %s~\n", account->email_verification_code);
        
    if (account->email_verification_time > 0)
        fprintf(fp, "EmailVerificationTime %ld\n", account->email_verification_time);
        
    if (account->email_verification_last_sent > 0)
        fprintf(fp, "EmailVerificationLastSent %ld\n", account->email_verification_last_sent);
        
    if (!IS_NULLSTR(account->reset_code)) {
        fprintf(fp, "ResetCode %s~\n", account->reset_code);
        fprintf(fp, "ResetState %d\n", account->reset_state);
        fprintf(fp, "Reset_Time %ld\n", account->reset_time);
    }
    if (account->character_limit > 0)
		fprintf(fp, "CharacterLimit %d\n", account->character_limit);

    if (!IS_NULLSTR(account->mfa_key)) {
        fprintf(fp, "MFA_Key %s~\n", account->mfa_key);
        //fprintf(fp, "MFA_Code_Expiration %ld\n", account->qr_code_expiration);
        if (account->mfa_enabled)
            fprintf(fp, "MFA_Enabled\n");
    }

	fprintf(fp, "MFAPendingKey %s~\n", account->mfa_pending_key ? account->mfa_pending_key : "");
	fprintf(fp, "MFAPending %d\n", account->mfa_pending ? 1 : 0);
    
	fprintf(fp, "RecoveryCodes ");
		for (int i = 0; i < MFA_RECOVERY_CODES; ++i)
   		fprintf(fp, "%s%c", account->recovery_codes[i], (i == MFA_RECOVERY_CODES-1) ? '\n' : ' ');

	fprintf(fp, "RecoveryUsed ");
		for (int i = 0; i < MFA_RECOVERY_CODES; ++i)
    		fprintf(fp, "%d%c", account->recovery_used[i] ? 1 : 0, (i == MFA_RECOVERY_CODES-1) ? '\n' : ' ');
	fprintf(fp, "CreationHost %s~\n", account->creation_host ? account->creation_host : "");
	fprintf(fp, "LastHost %s~\n", account->last_login_host ? account->last_login_host : "");
	fprintf(fp, "LastLogin %ld\n", account->last_login);
	fprintf(fp, "CharCount %d\n", account->character_count);
	fprintf(fp, "StaffAccount %d\n", account->staff_account ? 1 : 0);
	    // Save account notes
    ACCOUNT_NOTE_DATA *note;
    for (note = account->staff_notes; note != NULL; note = note->next)
    {
        fprintf(fp, "#ACCNOTE\n");
        fprintf(fp, "Author %s~\n", note->author);
        fprintf(fp, "Subject %s~\n", note->subject);
        fprintf(fp, "Text %s~\n", note->text);
        fprintf(fp, "Timestamp %ld\n", (long)note->timestamp);
        fprintf(fp, "#END_NOTE\n");
    }

	fprintf(fp, "VaultRent %ld\n", account->vault_rent);

    /* Write vault items if any */
    if (account->vault_items)
    {
        fprintf(fp, "#VAULT\n");
        OBJ_DATA *obj;
        for (obj = account->vault_items; obj != NULL; obj = obj->next_content)
        {
            fwrite_obj_new(NULL, obj, fp, 0);
        }
        fprintf(fp, "#ENDVAULT\n");
    }

    fprintf(fp, "End\n\n");
}

/*
 * Write character reference data for an account.
 */
void fwrite_account_character(ACCOUNT_CHARACTER *character, FILE *fp)
{
	log_string("Fwrite_account_character: writing character data");
    fprintf(fp, "#CHARACTER\n");
    
    fprintf(fp, "Name %s~\n", character->name);
    
    if (!IS_NULLSTR(character->race_name))
        fprintf(fp, "Race %s~\n", character->race_name);

    // Add logging here
    log_stringf("fwrite_account_character: writing %s last_area='%s' last_region='%s'",
        character->name,
        character->last_area ? character->last_area : "(null)",
        character->last_region ? character->last_region : "(null)");

    if (!IS_NULLSTR(character->class_name))
        fprintf(fp, "Class %s~\n", character->class_name);
    fprintf(fp, "Level %d\n", character->current_level);
    fprintf(fp, "TLevel %d\n", character->tot_level);
    fprintf(fp, "Created %ld\n", character->creation_date);
    fprintf(fp, "LastLogin %ld\n", character->last_login);
    fprintf(fp, "LastArea %s~\n", character->last_area);
    fprintf(fp, "LastRegion %s~\n", character->last_region);
    fprintf(fp, "LastHost" " %s~\n", character->last_host ? character->last_host : "");
    fprintf(fp, "LastLogoff %ld\n", (long int)current_time);
	if (character->deleted)
	{
		fprintf(fp, "Deleted %d\n", character->deleted);
		fprintf(fp, "DeleteTime %ld\n", character->delete_time);
	}

    if (character->id[0] != 0 || character->id[1] != 0) {
        fprintf(fp, "Id %ld\n", character->id[0]);
        fprintf(fp, "Id2 %ld\n", character->id[1]);
    }
    
    if (character->staff) {
        fprintf(fp, "Staff\n");
        fprintf(fp, "StaffRank %s~\n", flag_string(staff_ranks, character->staff_rank));
    }

    fprintf(fp, "End\n\n");
}

/*
 * Save an account to disk.
 */
void save_account(ACCOUNT_DATA *account)
{
	log_string("save_account: saving account data");
    char strsave[MAX_INPUT_LENGTH];
    FILE *fp;
    
    if (account == NULL) {
        bug("save_account: null account pointer", 0);
        return;
    }
    
    if (IS_NULLSTR(account->username)) {
        bug("save_account: account has no username", 0);
        return;
    }
    
    // Ensure account has IDs before saving
    if (account->id[0] == 0 || account->id[1] == 0)
        get_account_id(account);
    
    /* Close reserve file */
    fclose(fpReserve);
    
    sprintf(strsave, "%s%c/%s", ACCOUNT_DIR, tolower(account->username[0]), capitalize(account->username));
    if ((fp = fopen(TEMP_FILE, "w")) == NULL) {
        bug("save_account: fopen", 0);
        perror(strsave);
    } else {
        /* Write account data */
        fwrite_account(account, fp);
        
        /* Write character references */
        if (account->characters && list_size(account->characters) > 0) {
            ITERATOR it;
            ACCOUNT_CHARACTER *acct_char;
            
            iterator_start(&it, account->characters);
            while ((acct_char = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
                fwrite_account_character(acct_char, fp);
            }
            iterator_stop(&it);
        }
        
        fprintf(fp, "#END\n");
        fclose(fp);
        rename(TEMP_FILE, strsave);
    }
    
    /* Reopen reserve file */
    fpReserve = fopen(NULL_FILE, "r");
}
void account_add_character(ACCOUNT_DATA *account, CHAR_DATA *ch)
{
    log_string("account_add_character: adding character to account");
    ACCOUNT_CHARACTER *acct_char = NULL;
    ITERATOR it;

    if (!account || !ch || IS_NPC(ch)) {
        bug("account_add_character: invalid parameters", 0);
        return;
    }

    /* Check if character already exists in account */
    iterator_start(&it, account->characters);
    while ((acct_char = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (!str_cmp(acct_char->name, ch->name)) {
            // ... update fields ...
            acct_char->last_login = current_time;
            acct_char->tot_level = ch->tot_level;
            if (ch->pcdata && ch->pcdata->current_class && IS_VALID(ch->pcdata->current_class->clazz))
                acct_char->current_level = ch->pcdata->current_class->level;
            else
                acct_char->current_level = ch->tot_level; // fallback

            acct_char->staff = IS_IMMORTAL(ch);
            acct_char->staff_rank = IS_IMMORTAL(ch) ? ch->pcdata->staff_rank : 0;

            if (ch->pcdata && ch->pcdata->current_class && IS_VALID(ch->pcdata->current_class->clazz)) {
                if (acct_char->class_name)
                    free_string(acct_char->class_name);
                acct_char->class_name = str_dup(ch->pcdata->current_class->clazz->name);
            }

            acct_char->id[0] = ch->id[0];
            acct_char->id[1] = ch->id[1];

            free_string(acct_char->last_area);
            acct_char->last_area = str_dup(!IS_NULLSTR(ch->pcdata->last_area) ? ch->pcdata->last_area : "");

            free_string(acct_char->last_region);
            acct_char->last_region = str_dup(!IS_NULLSTR(ch->pcdata->last_region) ? ch->pcdata->last_region : "");

            break; // <-- just break, do not return!
        }
    }
    iterator_stop(&it);

    // If not found, create new entry
    if (!acct_char) {
        acct_char = new_account_character();
        acct_char->name = str_dup(ch->name);
        acct_char->race_name = str_dup(ch->race->name);
        acct_char->tot_level = ch->tot_level;

        if (ch->pcdata && ch->pcdata->current_class && IS_VALID(ch->pcdata->current_class->clazz))
            acct_char->current_level = ch->pcdata->current_class->level;
        else
            acct_char->current_level = ch->tot_level; // fallback

        acct_char->creation_date = ch->pcdata->creation_date;
        acct_char->last_login = current_time;
        acct_char->id[0] = ch->id[0];
        acct_char->id[1] = ch->id[1];

        acct_char->staff = IS_IMMORTAL(ch);
        acct_char->staff_rank = IS_IMMORTAL(ch) ? ch->pcdata->staff_rank : 0;

        if (ch->pcdata && ch->pcdata->current_class && IS_VALID(ch->pcdata->current_class->clazz))
            acct_char->class_name = str_dup(ch->pcdata->current_class->clazz->name);

        acct_char->last_area = str_dup(!IS_NULLSTR(ch->pcdata->last_area) ? ch->pcdata->last_area : "");
        acct_char->last_region = str_dup(!IS_NULLSTR(ch->pcdata->last_region) ? ch->pcdata->last_region : "");

        list_appendlink(account->characters, acct_char);
    }

    account->character_count = list_size(account->characters);
    account->staff_account = FALSE;
    ITERATOR cit;
    ACCOUNT_CHARACTER *ch_entry;
    iterator_start(&cit, account->characters);
    while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&cit))) {
        if (ch_entry->staff && ch_entry->staff_rank >= STAFF_IMMORTAL) {
            account->staff_account = TRUE;
            break;
        }
    }
    iterator_stop(&cit);

    /* Save the updated account */
    log_stringf("account_add_character: saving %s last_area='%s' last_region='%s'",
        acct_char->name,
        acct_char->last_area ? acct_char->last_area : "(null)",
        acct_char->last_region ? acct_char->last_region : "(null)");
    save_account(account);
}

/*
 * Remove a character from an account.
 */
void account_remove_character(ACCOUNT_DATA *account, const char *name)
{
    ITERATOR it;
    ACCOUNT_CHARACTER *acct_char;
    bool found = FALSE;

    if (!account || IS_NULLSTR(name)) {
        bug("account_remove_character: invalid parameters", 0);
        return;
    }

    // Remove from account's character list
    iterator_start(&it, account->characters);
    while ((acct_char = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (!str_cmp(acct_char->name, name)) {
            iterator_remcurrent(&it);
            free_account_character(acct_char);
            found = TRUE;
            break;
        }
    }
    iterator_stop(&it);

    // Unlink account info from the character file
    DESCRIPTOR_DATA d;
    memset(&d, 0, sizeof(d));
    if (load_char_obj(&d, (char *)name) && d.character && d.character->pcdata) {
        CHAR_DATA *ch = d.character;
        free_string(ch->pcdata->account_name);
        ch->pcdata->account_name = str_dup("");
        ch->pcdata->account_id[0] = 0;
        ch->pcdata->account_id[1] = 0;
        save_char_obj(ch);
        free_char(ch);
    }

    if (found) {
        save_account(account);
    }
}

// Update a character's entry in an account when they log out
void update_account_character(CHAR_DATA *ch)
{
    if (IS_NPC(ch) || !ch->desc || !ch->desc->account)
        return;
        
    ACCOUNT_DATA *acct = ch->desc->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    ITERATOR it;
    
    // Find existing entry or create a new one
    iterator_start(&it, acct->characters);
    while ((acct_char = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (!str_cmp(acct_char->name, ch->name))
            break;
    }
    iterator_stop(&it);
    
    // Create new entry if not found
    if (!acct_char) {
        acct_char = new_account_character();
        acct_char->name = str_dup(ch->name);
        list_appendlink(acct->characters, acct_char);
    }
    
    // Update character info
    acct_char->last_login = current_time;
    acct_char->tot_level = ch->tot_level;
	acct_char->current_level = ch->pcdata->current_class->level;
    acct_char->id[0] = ch->id[0];
    acct_char->id[1] = ch->id[1];
    
    // Update race/class
    if (acct_char->race_name)
        free_string(acct_char->race_name);
    acct_char->race_name = str_dup(ch->race->name);
    
    if (acct_char->class_name)
        free_string(acct_char->class_name);
        
    if (ch->pcdata && ch->pcdata->current_class && IS_VALID(ch->pcdata->current_class->clazz))
        acct_char->class_name = str_dup(ch->pcdata->current_class->clazz->name);
    else
        acct_char->class_name = str_dup("Adventurer");
        
    save_account(acct);
}

/*
 * Find an account by character name
 */
ACCOUNT_DATA *find_account(char *char_name)
{
    CHAR_DATA *dummy_ch;
    bool found = false;
    ACCOUNT_DATA *account = NULL;
    char account_name[MAX_STRING_LENGTH] = "";

    // Create a temporary descriptor to load the character file
    DESCRIPTOR_DATA d;
    memset(&d, 0, sizeof(d));

    // Try to load the character file first
    found = load_char_obj(&d, char_name);
    dummy_ch = d.character;
    
    if (found && dummy_ch && dummy_ch->pcdata && 
        !IS_NULLSTR(dummy_ch->pcdata->account_name)) {
        // We found the account name in the character file
        strcpy(account_name, dummy_ch->pcdata->account_name);
        free_char(dummy_ch);
        
        // Now load the account
        account = new_account();
        d.account = account;
        
        if (load_account(&d, account_name)) {
            return account;
        } else {
            free_account(account);
            return NULL;
        }
    }
    
    // If we didn't find an account name in the character file,
    // fall back to the existing search method
    free_char(dummy_ch);
    
    // Existing directory search code...
    
    return account;
}

/*
 * Find an account by ID
 */
ACCOUNT_DATA *find_account_by_id(unsigned long id0, unsigned long id1)
{
    DIR *dir;
    struct dirent *entry;
    char dir_path[MAX_INPUT_LENGTH];
    char acct_path[MAX_STRING_LENGTH];
    FILE *fp;
    char letter, *word;
    ACCOUNT_DATA *account = NULL;
    bool found = false;
    char c;

    // Check each letter directory
    for (c = 'a'; c <= 'z' && !found; c++) {
        sprintf(dir_path, "%s%c", ACCOUNT_DIR, c);
        dir = opendir(dir_path);
        
        if (!dir) 
            continue;

        // Check each file in this directory
        while ((entry = readdir(dir)) != NULL && !found) {
            // Skip . and .. entries
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
                continue;
                
            sprintf(acct_path, "%s/%s", dir_path, entry->d_name);
            
            fp = fopen(acct_path, "r");
            if (!fp) 
                continue;
                
            // Create a temporary account to parse
            account = new_account();
            account->username = str_dup("");
            account->characters = list_create(false);
            
            // Look for account ID in the file
            for (;;) {
                letter = fread_letter(fp);
                if (letter == '*') {
                    fread_to_eol(fp);
                    continue;
                }

                if (letter != '#') {
                    break;
                }

                word = fread_word(fp);
                if (!str_cmp(word, "ACCOUNT")) {
                    fread_account(account, fp);
                    
                    // Check if this is the account we're looking for
                    if (account->id[0] == id0 && account->id[1] == id1) {
                        found = true;
                        break;
                    }
                } 
                else if (!str_cmp(word, "END")) {
                    break;
                }
                else {
                    fread_to_eol(fp);
                }
            }
            
            fclose(fp);
            
            if (!found) {
                free_account(account);
                account = NULL;
            }
        }
        
        closedir(dir);
    }
    
    return account;
}

/*
 * Find an account by username
 */
ACCOUNT_DATA *find_account_by_name(char *username)
{
    char strsave[MAX_INPUT_LENGTH];
    FILE *fp;
    ACCOUNT_DATA *account = NULL;
    DESCRIPTOR_DATA d;

    if (IS_NULLSTR(username)) {
        bug("find_account_by_name: null or empty username", 0);
        return NULL;
    }

    // Initialize temporary descriptor
    memset(&d, 0, sizeof(d));

    // Try to load the account directly
    sprintf(strsave, "%s%c/%s", ACCOUNT_DIR, tolower(username[0]), capitalize(username));
    
    // First check if file exists 
    if ((fp = fopen(strsave, "r")) == NULL)
        return NULL;
    fclose(fp);
    
    // Now attempt to load it
    account = new_account();
    d.account = account;
    
    if (load_account(&d, username)) {
        return account;
    } else {
        // Something went wrong during loading
        free_account(account);
        return NULL;
    }
}



void obj_to_char_temp(OBJ_DATA *obj, CHAR_DATA *ch)
{
    obj->next_content = ch->carrying_temp;
    ch->carrying_temp = obj;
    obj->carried_by = ch;
    obj->in_room = ch->in_room;
    // Only add to lworn if equipped
    if (obj->wear_loc != WEAR_NONE)
        list_addlink(ch->lworn, obj);
    // DO NOT add to lcarrying here!
}

// Helper to dedupe a linked list of objects recursively
static void dedupe_obj_list(OBJ_DATA **head, LLIST *seen, LLIST *lworn) {
    OBJ_DATA *obj = *head, *prev = NULL, *next;
    while (obj) {
        next = obj->next_content;
        bool duplicate = false;
        ITERATOR it;
        OBJ_DATA *existing;
        iterator_start(&it, seen);
        while ((existing = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (existing->id[0] == obj->id[0] &&
                existing->id[1] == obj->id[1] &&
                existing->pIndexData == obj->pIndexData) {
                duplicate = true;
                break;
            }
        }
        iterator_stop(&it);

        // If this object is in lworn, do NOT remove it from carrying/carrying_temp
        bool is_equipped = (lworn && list_contains(lworn, obj, NULL));

        if (duplicate && !is_equipped) {
            // Remove from list
            if (prev)
                prev->next_content = next;
            else
                *head = next;
            // Optionally free_obj(obj);
        } else {
            list_appendlink(seen, obj);
            // Recurse into contents
            if (obj->contains)
                dedupe_obj_list(&obj->contains, seen, lworn);
            prev = obj;
        }
        obj = next;
    }
}

void remove_duplicate_objects_from_char(CHAR_DATA *ch) {
    LLIST *seen = list_create(FALSE);
    LLIST *lworn = list_create(FALSE);

    // 1. Add all equipped objects to seen and lworn first (so they are prioritized)
    ITERATOR it;
    OBJ_DATA *obj;
    iterator_start(&it, ch->lworn);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        if (!list_contains(seen, obj, NULL)) {
            list_appendlink(seen, obj);
        }
        if (!list_contains(lworn, obj, NULL)) {
            list_appendlink(lworn, obj);
        }
    }
    iterator_stop(&it);

    // 2. Dedupe carrying, carrying_temp, and locker against seen, but don't remove if in lworn
    OBJ_DATA **lists[3] = { &ch->carrying, &ch->carrying_temp, &ch->locker };
    for (int l = 0; l < 3; l++) {
        dedupe_obj_list(lists[l], seen, lworn);
    }

    list_destroy(seen);
    list_destroy(lworn);
}

void remove_duplicate_objects_from_list(OBJ_DATA **head, LLIST *seen) {
    OBJ_DATA *obj = *head, *prev = NULL, *next;
    while (obj) {
        next = obj->next_content;
        if (!list_contains(seen, obj, NULL)) {
            list_appendlink(seen, obj);
            if (obj->contains)
                remove_duplicate_objects_from_list(&obj->contains, seen);
            prev = obj;
        } else {
            // Remove duplicate from this list
            if (prev)
                prev->next_content = next;
            else
                *head = next;
        }
        obj = next;
    }
}

char *fread_char_account_name(const char *name)
{
	char *account_name = NULL;
	char path[MSL];

    sprintf(path, "%s%c/%s", PLAYER_DIR, tolower(name[0]), capitalize(name));
	FILE *fp;

	if ((fp = fopen(path, "r")) != NULL) {
		char *word;
		bool fMatch = false;

		while(str_cmp((word = fread_word(fp)),"#END"))
		{
			KEYS("Account", account_name, fread_string(fp));
		}

		fclose(fp);
	}

	return account_name;
}

PLAYER_CHANNEL_DATA *fread_player_channel_data(const char *name)
{
	PLAYER_CHANNEL_DATA *data = new_player_channel_data();
	char path[MSL];

    sprintf(path, "%s%c/%s.channels", PLAYER_DIR, tolower(name[0]), capitalize(name));
	FILE *fp;
	
	if ((fp = fopen(path, "r")) != NULL) {
		char *word;
		bool fMatch;

		while(str_cmp((word = fread_word(fp)),"#END"))
		{
			fMatch = false;

			switch(UPPER(word[0]))
			{
				case '#':
					if (!str_cmp(word, "#CHANNELENTRY"))
					{
						fread_channel_entry(data, fp);
						fMatch = true;
						break;
					}
					break;
			}
		}

		fclose(fp);
	}

	return data;
}

void fwrite_player_channel_data(const char *name, PLAYER_CHANNEL_DATA *data)
{
	char path[MSL];

    sprintf(path, "%s%c/%s.channels", PLAYER_DIR, tolower(name[0]), capitalize(name));
	FILE *fp;
	
	if ((fp = fopen(TEMP_FILE, "w")) != NULL) {

		if (data && IS_VALID(data->channels))
		{
			ITERATOR it;
			CHANNEL_ENTRY *entry;
			iterator_start(&it, data->channels);
			while((entry = (CHANNEL_ENTRY *)iterator_nextdata(&it)))
			{
				fprintf(fp, "#CHANNELENTRY %s~\n\r", entry->channel->name);
				fprintf(fp, "Flags %s\n\r", print_flags(entry->flags));
				fprintf(fp, "Banner %s~\n\r", entry->banner);
				fprintf(fp, "#-CHANNELENTRY\n\r");
			}
			iterator_stop(&it);
		}

		fprintf(fp, "#END\n\r");
		fclose(fp);
	    rename(TEMP_FILE, path);
	}
	else
	{
		// Complain
	}
}