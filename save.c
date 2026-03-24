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
#include <dirent.h>
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
#include "io/cache/redis_cache.h"
#include "io/json/json_char.h"
#include "io/json/json_account.h"
#include "io/json/json_obj_types.h"
#include "traits.h"
#include "account/preferences.h"
#include "skill_data.h"
#include "class_data.h"
#include "skill_group.h"
#include "song_data.h"


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


// Version structures removed — all pfiles migrated to JSON.
// Legacy version migrations restored for loading old .dat format pfiles.

// Old level constants for staff rank migration from legacy .dat pfiles
#define OLD_LEVEL_MINIGOD              150
#define OLD_LEVEL_GOD                  151
#define OLD_LEVEL_ASCENDANT            152
#define OLD_LEVEL_SUPREMACY            153
#define OLD_LEVEL_CREATOR              154
#define OLD_LEVEL_IMPLEMENTOR  155

void fix_character( CHAR_DATA *ch );


// External functions
extern bool loading_immortal_data;
void account_add_character(ACCOUNT_DATA *account, CHAR_DATA *ch);
ACCOUNT_DATA *find_account_by_id(unsigned long id0, unsigned long id1);
ACCOUNT_DATA *find_account_by_name(char *username);
void save_account(ACCOUNT_DATA *account);
extern void get_account_id(ACCOUNT_DATA *account);
void rebuild_character_file(CHAR_DATA *ch);
void obj_to_char_temp(OBJ_DATA *obj, CHAR_DATA *ch);
void remove_duplicate_objects_from_char(CHAR_DATA *ch);
static void dedupe_obj_list(OBJ_DATA **head, LLIST *seen, LLIST *lworn);
void remove_duplicate_objects_from_list(OBJ_DATA **head, LLIST *seen);
void remove_duplicate_objects_from_char(CHAR_DATA *ch);

// Globals.
OBJ_DATA *	pneuma_relic;
OBJ_DATA *	damage_relic;
OBJ_DATA *	xp_relic;
OBJ_DATA *	hp_regen_relic;
OBJ_DATA *	mana_regen_relic;

// Array of containers read for proper re-nesting of objects.
OBJ_DATA *	rgObjNest[MAX_NEST];
int 		nest_level;


/*
 * Legacy value[] compatibility helpers.
 *
 * Old pfile/object migration logic still maps through obj->value[] slots.
 * Keep all direct slot access centralized here to simplify eventual
 * retirement of legacy pfile/area readers.
 */
static inline int legacy_obj_value_get(const OBJ_DATA *obj, int slot)
{
    if (obj == NULL || slot < 0 || slot > 7)
        return 0;
    return obj->value[slot];
}

static inline void legacy_obj_value_set(OBJ_DATA *obj, int slot, int value)
{
    if (obj == NULL || slot < 0 || slot > 7)
        return;
    obj->value[slot] = value;
}

static inline int legacy_obj_index_value_get(const OBJ_INDEX_DATA *obj, int slot)
{
    if (obj == NULL || slot < 0 || slot > 7)
        return 0;
    return obj->value[slot];
}


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


void save_char_obj(CHAR_DATA *ch)
{
    char strsave[MAX_INPUT_LENGTH];
    struct timeval start_time, end_time, dedup_time, write_time, section_start, section_end;
    long dedup_ms, total_ms, section_ms;

    if (IS_NPC(ch))
    return;

    if (!IS_VALID(ch))
    {
        pbugf(LOG_ERROR, "save_char_obj: Trying to save an invalidated character.\n");
        return;
    }

    gettimeofday(&start_time, NULL);
    remove_duplicate_objects_from_char(ch);
    gettimeofday(&dedup_time, NULL);

    dedup_ms = (dedup_time.tv_sec - start_time.tv_sec) * 1000 +
              (dedup_time.tv_usec - start_time.tv_usec) / 1000;
    if (dedup_ms > 50)
        log_stringf("PERFORMANCE save_char_obj %s: dedup took %ldms", ch->name, dedup_ms);

    // CRITICAL: Prevent recursive save loop and duplicate file writes
    // save_char_obj() can be called from account_add_character() at line 6361
    // Track recursion depth to prevent:
    // 1. Infinite recursion (account_add_character calling save_char_obj calling account_add_character...)
    // 2. Multiple file writes (both the outer and inner save writing the pfile)
    static int save_depth = 0;
    bool is_top_level_save = (save_depth == 0);
    save_depth++;

    // Update account metadata (may trigger recursive save)
    // BUT: Only do this if we're the top-level save to prevent recursion
    // When save_char_obj is called from account_add_character, we should NOT
    // call account_add_character again (infinite loop!)
    if (is_top_level_save) {
        gettimeofday(&section_start, NULL);
        if (ch->desc && ch->desc->account) {
            account_add_character(ch->desc->account, ch);
            // Account saving is typically handled at player quit or via specific account commands.
            // account_add_character may trigger a save if migration occurs.
        } else if (!IS_NPC(ch) &&
                  (!IS_NULLSTR(ch->pcdata->account_name) || ch->pcdata->account_id[0] != 0)) {
            // Fallback: Try to find and update account if not already on descriptor.
            // This might happen during auto-saves or other scenarios where desc might be temporarily unavailable
            // or the account link wasn't established.
            ACCOUNT_DATA *account = NULL;

            // Try by account name first
            if (!IS_NULLSTR(ch->pcdata->account_name)) {
                account = find_account_by_name(ch->pcdata->account_name);
            }

            // If not found by name, try by ID
            if (account == NULL && ch->pcdata->account_id[0] != 0) {
                account = find_account_by_id(ch->pcdata->account_id[0], ch->pcdata->account_id[1]);
            }

                if (account != NULL) {
                    account_add_character(account, ch); // This might save the account if data is migrated
                    // If ch->desc is available, link the found account to it.
                    if (ch->desc) {
                        ch->desc->account = account; // Link it for the current session
                    } else {
                        save_account(account); // Explicitly save if we're not attaching to a descriptor.
                        if (!ch->desc) { // Only free if there's no descriptor to hold it
                            // Check if it's in the global list before freeing
                            bool is_globally_loaded = false;
                            if (loaded_accounts) {
                                ITERATOR acc_it;
                                ACCOUNT_DATA *glob_acct;
                                iterator_start(&acc_it, loaded_accounts);
                                while((glob_acct = (ACCOUNT_DATA *)iterator_nextdata(&acc_it))) {
                                    if (glob_acct == account) {
                                        is_globally_loaded = true;
                                        break;
                                    }
                                }
                                iterator_stop(&acc_it);
                            }
                            if (!is_globally_loaded) {
                                free_account(account);
                            }
                        }
                    }
                } else {
                    char msg[MSL];
                    char extra[256];
                    snprintf(msg, sizeof(msg),
                             "save_char_obj: Character %s has account identifiers but account could not be found by name ('%s') or ID (%lu %lu).",
                             ch->name,
                             ch->pcdata->account_name ? ch->pcdata->account_name : "NULL",
                             ch->pcdata->account_id[0], ch->pcdata->account_id[1]);
                    snprintf(extra, sizeof(extra),
                             "{\"account_name\":\"%s\",\"account_uid\":[%lu,%lu]}",
                             ch->pcdata->account_name ? ch->pcdata->account_name : "",
                             ch->pcdata->account_id[0], ch->pcdata->account_id[1]);
                    log_context_t ctx = {
                        .actor_type = IS_NPC(ch) ? "npc" : "player",
                        .actor_name = IS_NPC(ch) ? ch->short_descr : ch->name,
                        .actor_uid = { ch->id[0], ch->id[1] },
                        .actor_wnum = (IS_NPC(ch) && ch->pIndexData)
                                      ? widevnum_string_mobile(ch->pIndexData, NULL) : NULL,
                        .action = "save_account_link_missing",
                        .extra_json = extra,
                    };
                    log_event_t ev = {
                        .severity = EVENT_SEV_WARN,
                        .category = LOG_ERROR,
                        .plain_message = msg,
                        .context = &ctx,
                        .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__,
                    };
                    log_emit_event(&ev, NULL);
                }
            }
        }
        gettimeofday(&section_end, NULL);
        section_ms = (section_end.tv_sec - section_start.tv_sec) * 1000 +
                    (section_end.tv_usec - section_start.tv_usec) / 1000;
        if (section_ms > 50)
            log_stringf("PERFORMANCE save_char_obj %s: account_add_character took %ldms", ch->name, section_ms);

    // Only write the file if this is the top-level save call
    // Recursive saves (from account_add_character) should not write the file
    // The top-level save will write the file after all account updates are complete
    if (!is_top_level_save) {
        save_depth--;
        return;
    }

    // Remove carrying_temp code that's no longer needed
    if (fpReserve != NULL) {
        fclose(fpReserve);
        fpReserve = NULL;  // Mark as closed
    }
    char player_dir_buf[MAX_INPUT_LENGTH];
    const char *player_dir = resolve_game_path(PLAYER_DIR, player_dir_buf, sizeof(player_dir_buf));
    snprintf(strsave, sizeof(strsave), "%s%c/%s", player_dir, tolower(ch->name[0]), capitalize(ch->name));

    // JSON-only mode: initialize timing anchor for parity with performance logging
    gettimeofday(&write_time, NULL);

    fpReserve = fopen(NULL_FILE, "r");

    // Save to JSON format only.
    if (is_top_level_save) {
        char json_path[512];
        json_t *char_json = NULL;

        json_get_char_path(ch->name, json_path, sizeof(json_path));

        // Serialize character to JSON (needed for both disk and cache)
        gettimeofday(&section_start, NULL);
        char_json = char_to_json(ch);
        gettimeofday(&section_end, NULL);
        section_ms = (section_end.tv_sec - section_start.tv_sec) * 1000 +
                    (section_end.tv_usec - section_start.tv_usec) / 1000;
        if (section_ms > 50)
            log_stringf("PERFORMANCE save_char_obj %s: char_to_json took %ldms", ch->name, section_ms);

        if (char_json) {
            // Write to disk
            gettimeofday(&section_start, NULL);
            int result = json_dump_file(char_json, json_path, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
            gettimeofday(&section_end, NULL);
            section_ms = (section_end.tv_sec - section_start.tv_sec) * 1000 +
                        (section_end.tv_usec - section_start.tv_usec) / 1000;
            if (section_ms > 50)
                log_stringf("PERFORMANCE save_char_obj %s: json_dump_file took %ldms", ch->name, section_ms);

            if (result != 0) {
                char msg[MSL];
                snprintf(msg, sizeof(msg), "save_char_obj: Failed to write JSON for %s", ch->name);
                log_context_t ctx = {
                    .actor_type = IS_NPC(ch) ? "npc" : "player",
                    .actor_name = IS_NPC(ch) ? ch->short_descr : ch->name,
                    .actor_uid = { ch->id[0], ch->id[1] },
                    .actor_wnum = (IS_NPC(ch) && ch->pIndexData)
                                  ? widevnum_string_mobile(ch->pIndexData, NULL) : NULL,
                    .action = "save_json_write_failed",
                    .target_type = "storage",
                    .target_name = "disk",
                };
                log_event_t ev = {
                    .severity = EVENT_SEV_ERROR,
                    .category = LOG_ERROR,
                    .plain_message = msg,
                    .context = &ctx,
                    .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__,
                };
                log_emit_event(&ev, NULL);
                pbugf(LOG_ERROR, "save_char_obj: JSON write failed and old pfile format disabled!");
            }

            // Write-through cache: Update Redis with full character data
            gettimeofday(&section_start, NULL);
            redis_cache_char_full(ch, char_json);
            gettimeofday(&section_end, NULL);
            section_ms = (section_end.tv_sec - section_start.tv_sec) * 1000 +
                        (section_end.tv_usec - section_start.tv_usec) / 1000;
            if (section_ms > 50)
                log_stringf("PERFORMANCE save_char_obj %s: redis_cache_char_full took %ldms", ch->name, section_ms);

            // Cleanup
            json_decref(char_json);
        } else {
            char msg[MSL];
            snprintf(msg, sizeof(msg), "save_char_obj: Failed to serialize JSON for %s", ch->name);
            log_context_t ctx = {
                .actor_type = IS_NPC(ch) ? "npc" : "player",
                .actor_name = IS_NPC(ch) ? ch->short_descr : ch->name,
                .actor_uid = { ch->id[0], ch->id[1] },
                .actor_wnum = (IS_NPC(ch) && ch->pIndexData)
                              ? widevnum_string_mobile(ch->pIndexData, NULL) : NULL,
                .action = "save_json_serialize_failed",
                .target_type = "storage",
                .target_name = "json",
            };
            log_event_t ev = {
                .severity = EVENT_SEV_ERROR,
                .category = LOG_ERROR,
                .plain_message = msg,
                .context = &ctx,
                .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__,
            };
            log_emit_event(&ev, NULL);
        }
    }

    // Cache character info in Redis for fast account menu display
    // Only cache on top-level saves (not during recursive account updates)
    if (is_top_level_save) {
        gettimeofday(&section_start, NULL);
        redis_cache_char_info(ch);
        if (ch->desc) {
            redis_set_char_active(ch->name, true);
        }
        if (ch->pcdata)
            leaderboard_update_wealth(ch);
        gettimeofday(&section_end, NULL);
        section_ms = (section_end.tv_sec - section_start.tv_sec) * 1000 +
                    (section_end.tv_usec - section_start.tv_usec) / 1000;
        if (section_ms > 50)
            log_stringf("PERFORMANCE save_char_obj %s: redis_info+active+leaderboard took %ldms", ch->name, section_ms);
    }

    gettimeofday(&end_time, NULL);
    save_depth--;

    // Always log total save time if it exceeds threshold
    total_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
              (end_time.tv_usec - start_time.tv_usec) / 1000;

    int obj_count = (ch->lcarrying ? list_size(ch->lcarrying) : 0) +
                   (ch->llocker ? list_size(ch->llocker) : 0) +
                   (ch->lworn ? list_size(ch->lworn) : 0);

    if (total_ms > 100) {
        log_stringf("PERFORMANCE save_char_obj: %s total: %ldms (objects: %d) [loaded_objects: %d]",
                   ch->name, total_ms, obj_count, loaded_objects ? loaded_objects->size : 0);
    }
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
            fprintf(fp, "StaffRank %s~\n", flag_string(staff_ranks, ch->pcdata->staff_rank));



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
    fprintf(fp, "Race %s~\n", ch->race ? ch->race->id : "human");
    fprintf(fp, "BodyType %d\n", ch->body_type );
    if (ch->pronoun_he_she && ch->pronoun_he_she[0] != '\0')
        fprintf(fp, "PronounSS %s~\n", ch->pronoun_he_she);
    if (ch->pronoun_him_her && ch->pronoun_him_her[0] != '\0')
        fprintf(fp, "PronounOS %s~\n", ch->pronoun_him_her);
    if (ch->pronoun_his_her && ch->pronoun_his_her[0] != '\0')
        fprintf(fp, "PronounPAS %s~\n", ch->pronoun_his_her);
    if (ch->pronoun_his_hers && ch->pronoun_his_hers[0] != '\0')
        fprintf(fp, "PronounPPS %s~\n", ch->pronoun_his_hers);
    if (ch->pronoun_himself_herself && ch->pronoun_himself_herself[0] != '\0')
        fprintf(fp, "PronounRS %s~\n", ch->pronoun_himself_herself);
    fprintf(fp, "VerbPref %d\n", ch->verb_preference);

    fprintf(fp, "LockerRent %ld\n", (long int)ch->locker_rent   );
    if (ch->deleted)
    {
        fprintf(fp, "Deleted %d\n", ch->deleted);
        fprintf(fp, "DeleteTime %ld\n", (long int)ch->delete_time);
    }
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

    fprintf(fp, "Plyd %d\n", ch->played + (int) (current_time - ch->logon));

    if (location_isset(&ch->pcdata->room_before_arena)) {
    if(ch->pcdata->room_before_arena.wuid)
        fprintf(fp, "Room_before_arenaW %lu %lu %lu %lu\n", 	ch->pcdata->room_before_arena.wuid, ch->pcdata->room_before_arena.id[0], ch->pcdata->room_before_arena.id[1], ch->pcdata->room_before_arena.id[2]);
    else if(ch->pcdata->room_before_arena.id[1] || ch->pcdata->room_before_arena.id[2])
        fprintf(fp, "Room_before_arenaC %lu %lu %lu\n", 	ch->pcdata->room_before_arena.id[0], ch->pcdata->room_before_arena.id[1], ch->pcdata->room_before_arena.id[2]);
    else
        fprintf(fp, "Room_before_arena %ld\n", 	ch->pcdata->room_before_arena.id[0]);
    }

    if (ch->in_room != NULL)
        fprintf(fp, "LastArea     %s~\n", format_location_string(ch->in_room));

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
            fprintf(fp,"Room %ld\n", dungeon->entry_room->vnum);
        else {
            ROOM_INDEX_DATA *default_recall = get_reserved_room_index("room_default_recall");
            fprintf (fp, "Room %ld\n", default_recall ? default_recall->vnum : 0);
        }
    }
    else if( ch->checkpoint ) {
        if( ch->checkpoint->wilds )
            fprintf (fp, "Vroom %ld %ld %ld %ld\n",
                ch->checkpoint->x, ch->checkpoint->y, ch->checkpoint->wilds->pArea->uid, ch->checkpoint->wilds->uid);
        else if(ch->checkpoint->source)
            fprintf(fp,"CloneRoom %ld %ld %ld\n",
                ch->checkpoint->source->vnum, ch->checkpoint->id[0], ch->checkpoint->id[1]);
        else
            fprintf(fp,"Room %ld\n", ch->checkpoint->vnum);
    } else if(!ch->in_room) {
        ROOM_INDEX_DATA *default_recall = get_reserved_room_index("room_default_recall");
        fprintf (fp, "Room %ld\n", default_recall ? default_recall->vnum : 0);
    }
    else if(ch->in_wilds) {
        fprintf (fp, "Vroom %ld %ld %ld %ld\n",
            ch->in_room->x, ch->in_room->y, ch->in_wilds->pArea->uid, ch->in_wilds->uid);
    } else if(ch->was_in_wilds) {
        fprintf (fp, "Vroom %d %d %ld %ld\n",
        ch->was_at_wilds_x, ch->was_at_wilds_y, ch->was_in_wilds->pArea->uid, ch->was_in_wilds->uid);
    } else if(ch->was_in_room) {
        if(ch->was_in_room->source)
            fprintf(fp,"CloneRoom %ld %ld %ld\n",
                ch->was_in_room->source->vnum, ch->was_in_room->id[0], ch->was_in_room->id[1]);
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

    if (race_has_trait(ch->race, "toxin_system"))
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
    if (IS_QUESTING(ch)) {
        WNUM questgiver_wnum = ch->quest->questgiver_wnum;
        WNUM questreceiver_wnum = ch->quest->questreceiver_wnum;

        if (!questgiver_wnum.pArea && ch->quest->questgiver_load.vnum > 0) {
            AREA_DATA *fallback = NULL;
            WNUM wnum;
            if (resolve_widevnum(ch->quest->questgiver_load.vnum, NULL, &wnum))
                fallback = wnum.pArea;
            if (!fallback) fallback = get_system_area_fallback();
            resolve_wnum_load(&ch->quest->questgiver_load, &questgiver_wnum, fallback);
        }

        if (!questreceiver_wnum.pArea && ch->quest->questreceiver_load.vnum > 0) {
            AREA_DATA *fallback = NULL;
            WNUM wnum;
            if (resolve_widevnum(ch->quest->questreceiver_load.vnum, NULL, &wnum))
                fallback = wnum.pArea;
            if (!fallback) fallback = get_system_area_fallback();
            resolve_wnum_load(&ch->quest->questreceiver_load, &questreceiver_wnum, fallback);
        }

        fprintf(fp, "Questing\n");
        fprintf(fp, "QuestGiverType %d\n", ch->quest->questgiver_type);
        fprintf(fp, "QuestGiverW %s\n", widevnum_string_wnum(questgiver_wnum, NULL));
        fprintf(fp, "QuestReceiverType %d\n", ch->quest->questreceiver_type);
        fprintf(fp, "QuestReceiverW %s\n", widevnum_string_wnum(questreceiver_wnum, NULL));

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
    fprintf(fp, "Invi %s\n", 	flag_string(staff_ranks,ch->invis_level	));
    if (ch->incog_level)
    fprintf(fp,"Inco %s\n",flag_string(staff_ranks,ch->incog_level));
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
    /*
     * AUTH DATA: Only save for unlinked characters (no account)
     * Linked characters have their auth stored in account file
     * Note: As characters migrate, these fields should be empty for linked chars
     */
    bool is_unlinked = IS_NULLSTR(ch->pcdata->account_name);
    bool has_auth_data = !IS_NULLSTR(ch->pcdata->pwd) || ch->pcdata->mfa_enabled;

    /* Save password data if unlinked or if fields are non-empty (mid-migration) */
    if (is_unlinked || has_auth_data) {
        if (!IS_NULLSTR(ch->pcdata->pwd)) {
            fprintf(fp, "Pass %s~\n", ch->pcdata->pwd);
            fprintf(fp, "PassVers %d\n", ch->pcdata->pwd_vers);
        }

        if (ch->pcdata->reset_code != NULL && !IS_NULLSTR(ch->pcdata->reset_code))
            fprintf(fp, "ResetCode %s~\n", ch->pcdata->reset_code);

        if (ch->pcdata->reset_time != 0)
            fprintf(fp, "Reset_Time %ld\n", ch->pcdata->reset_time);

        if (ch->pcdata->reset_state != 0)
            fprintf(fp, "ResetState %d\n", ch->pcdata->reset_state);

        if (ch->pcdata->mfa_key != NULL && !IS_NULLSTR(ch->pcdata->mfa_key))
            fprintf(fp, "MFA_Key %s~\n", ch->pcdata->mfa_key);

        if (ch->pcdata->mfa_enabled == true)
        {
            fprintf(fp, "MFA_Enabled\n");
            fprintf(fp, "MFAPendingKey %s~\n", ch->pcdata->mfa_pending_key ? ch->pcdata->mfa_pending_key : "");
            fprintf(fp, "MFAPending %d\n", ch->pcdata->mfa_pending ? 1 : 0);
            fprintf(fp, "RecoveryCodes ");
                for (int i = 0; i < MFA_RECOVERY_CODES; ++i)
                    fprintf(fp, "%s%c", ch->pcdata->recovery_codes[i], (i == MFA_RECOVERY_CODES-1) ? '\n' : ' ');

            fprintf(fp, "RecoveryUsed ");
                for (int i = 0; i < MFA_RECOVERY_CODES; ++i)
                    fprintf(fp, "%d%c", ch->pcdata->recovery_used[i] ? 1 : 0, (i == MFA_RECOVERY_CODES-1) ? '\n' : ' ');
        }
    }
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

    {
        ITERATOR sg_it;
        SKILL_GROUP *sg;
        iterator_start(&sg_it, ch->pcdata->known_groups);
        while ((sg = (SKILL_GROUP *)iterator_nextdata(&sg_it))) {
            if (sg->name)
                fprintf(fp, "Gr '%s'\n", sg->name);
        }
        iterator_stop(&sg_it);
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

    ITERATOR udit;
    DUNGEON_INDEX_DATA *unlocked_dungeon;
    iterator_start(&udit, ch->pcdata->unlocked_dungeons);
    while( (unlocked_dungeon = (DUNGEON_INDEX_DATA *)iterator_nextdata(&udit)) )
    {
        if (unlocked_dungeon->area)
            fprintf(fp, "UnlockedDungeon %ld %ld\n", unlocked_dungeon->area->uid, unlocked_dungeon->vnum);
    }
    iterator_stop(&udit);

    for (cmd = ch->pcdata->commands; cmd != NULL; cmd = cmd->next)
    fprintf(fp, "GrantedCommand %s~\n", cmd->name);
    fprintf(fp, "End\n\n");
}

extern pVARIABLE variable_head;
extern pVARIABLE variable_tail;

/*
 * Load a char and inventory into a new ch structure.
 */
// Internal function with load_full parameter
static bool load_char_obj_internal(DESCRIPTOR_DATA *d, const char *name, bool load_full)
{
    char strsave[MAX_INPUT_LENGTH];
    char buf[MSL];
    CHAR_DATA *ch;
    OBJ_DATA *obj;
    FILE *fp;
    bool found = false;
    int stat;
    TOKEN_DATA *token;
    pVARIABLE last_var = variable_tail;
    char *section = NULL;
    IMMORTAL_DATA *immortal;
    OBJ_DATA *objNestList[MAX_NEST];
    int iNest;
    struct timeval start_time, end_time;
    long total_ms;

    gettimeofday(&start_time, NULL);

    ch = new_char();
    ch->pcdata = new_pcdata();

    d->character = ch;
    ch->desc = d;
    ch->name = str_dup(name);
    ch->id[0] = ch->id[1] = 0;
    ch->pcdata->creation_date = -1;
    ch->race = race_lookup("human");
    ch->act[0] = PLR_NOSUMMON;
    ch->act[1] = 0;
    ch->comm = COMM_PROMPT;
    ch->num_grouped = 0;
    ch->dead = false;
    ch->prompt = str_dup("{B<{x%h{Bhp {x%m{Bm {x%v{Bmv>{x ");
    ch->pcdata->confirm_delete = false;
    ch->pcdata->pwd = str_dup("");
    ch->pcdata->pwd_vers = 0;
    ch->pcdata->reset_code = str_dup("");
    ch->pcdata->reset_state = 0;
    ch->pcdata->title = str_dup("");
    for (stat = 0; stat < MAX_STATS; stat++) {
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


    // Initialize the object nesting table
    for (iNest = 0; iNest < MAX_NEST; iNest++)
        objNestList[iNest] = NULL;

    found = false;
    if (fpReserve != NULL) {
        fclose(fpReserve);
        fpReserve = NULL;  // Mark as closed so we know to reopen it
    }

    char player_dir_buf[MAX_INPUT_LENGTH];
    const char *player_dir = resolve_game_path(PLAYER_DIR, player_dir_buf, sizeof(player_dir_buf));

    /* decompress if .gz file exists */
    snprintf(strsave, sizeof(strsave), "%s%c/%s%s", player_dir, tolower(name[0]), capitalize(name), ".gz");
    if ((fp = fopen(strsave, "r")) != NULL) {
        fclose(fp);
        sprintf(buf,"gzip -dfq %s",strsave);
        system(buf);
    }

    snprintf(strsave, sizeof(strsave), "%s%c/%s", player_dir, tolower(name[0]), capitalize(name));
    sprintf(buf, "Trying to load %s", strsave);
    log_string(buf);

    // Read-through cache: Try Redis first for faster load
    bool loaded_from_cache = false;
    json_t *cached_json = redis_get_char_full(name);
    if (cached_json) {
        // Found in Redis cache - load directly from JSON object
        log_stringf("load_char_obj: Loading %s from Redis cache", name);
        if (load_full) {
            found = json_read_char_from_json(ch, cached_json);
            if (!found) {
                char msg[MSL];
                snprintf(msg, sizeof(msg), "load_char_obj: Failed to load cached JSON for %s, falling back to disk", name);
                log_context_t ctx = {
                    .actor_type = "player",
                    .actor_name = name,
                    .action = "load_cache_fallback",
                    .target_type = "storage",
                    .target_name = "redis",
                };
                log_event_t ev = {
                    .severity = EVENT_SEV_WARN,
                    .category = LOG_ERROR,
                    .plain_message = msg,
                    .context = &ctx,
                    .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__,
                };
                log_emit_event(&ev, NULL);
            }
        } else {
            found = json_read_char_basic_from_json(ch, cached_json);
            if (!found) {
                char msg[MSL];
                snprintf(msg, sizeof(msg), "load_char_obj_basic: Failed to load cached JSON for %s, falling back to disk", name);
                log_context_t ctx = {
                    .actor_type = "player",
                    .actor_name = name,
                    .action = "load_cache_fallback_basic",
                    .target_type = "storage",
                    .target_name = "redis",
                };
                log_event_t ev = {
                    .severity = EVENT_SEV_WARN,
                    .category = LOG_ERROR,
                    .plain_message = msg,
                    .context = &ctx,
                    .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__,
                };
                log_emit_event(&ev, NULL);
            }
        }
        json_decref(cached_json);

        if (found) {
            loaded_from_cache = true;
        }
    }

    // Only try disk if cache didn't work
    if (!loaded_from_cache) {
        // Check if file exists and detect format (JSON vs old pfile)
        if (json_is_json_file(strsave)) {
        // JSON format file
        found = true;
        if (load_full) {
            if (!json_read_char(ch, strsave)) {
                char msg[MSL];
                snprintf(msg, sizeof(msg), "load_char_obj: Failed to load JSON character %s", name);
                log_context_t ctx = {
                    .actor_type = "player",
                    .actor_name = name,
                    .action = "load_json_failed",
                    .target_type = "storage",
                    .target_name = "disk",
                };
                log_event_t ev = {
                    .severity = EVENT_SEV_ERROR,
                    .category = LOG_ERROR,
                    .plain_message = msg,
                    .context = &ctx,
                    .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__,
                };
                log_emit_event(&ev, NULL);
                found = false;
            }
        } else {
            if (!json_read_char_basic(ch, strsave)) {
                char msg[MSL];
                snprintf(msg, sizeof(msg), "load_char_obj_basic: Failed to load JSON character %s", name);
                log_context_t ctx = {
                    .actor_type = "player",
                    .actor_name = name,
                    .action = "load_json_failed_basic",
                    .target_type = "storage",
                    .target_name = "disk",
                };
                log_event_t ev = {
                    .severity = EVENT_SEV_ERROR,
                    .category = LOG_ERROR,
                    .plain_message = msg,
                    .context = &ctx,
                    .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__,
                };
                log_emit_event(&ev, NULL);
                found = false;
            }
        }
    }
        fpReserve = fopen(NULL_FILE, "r");
    } // end if (!loaded_from_cache)

    // Ensure fpReserve is open (may have been closed and not reopened if loaded from cache)
    if (fpReserve == NULL) {
        fpReserve = fopen(NULL_FILE, "r");
    }

    // LOG: Count loaded items for diagnosis
    if (!IS_NPC(ch) && ch->lcarrying) {
        int loaded_count = list_size(ch->lcarrying);
        if (loaded_count > 100) {
            log_stringf("load_char_obj: Loaded %d inventory items for %s",
                       loaded_count, ch->name ? ch->name : "(unknown)");
        }
    }

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
    // and needed to get the creation date
    if(loading_immortal_data) {
        return found;
    }

    get_mob_id(ch);

    // Handle immortal data setup
    if (get_staff_rank(ch) > STAFF_PLAYER) {
        IMMORTAL_DATA *immortal;
        if ((immortal = find_immortal(ch->name)) == NULL) {
            pbugf(LOG_ERROR, "load_char_obj: no immortal_data found for immortal character %s!", ch->name);

            immortal = new_immortal();
            immortal->name = str_dup(ch->name);
            ch->pcdata->immortal = immortal;

            add_immortal(immortal);

        } else {
            plogf(LOG_INFO, "load_char_obj: reading immortal char %s.\n\r", ch->name);

            ch->pcdata->immortal = immortal;
        }
        immortal->pc = ch->pcdata;
    }
    else if ((immortal = find_immortal(ch->name)) != NULL) {
        log_string(formatf("load_char_obj: resolving immortal data for %s.\n\r", ch->name));
        ch->pcdata->staff_rank = STAFF_IMMORTAL;
        ch->pcdata->immortal = immortal;
        immortal->pc = ch->pcdata;
    }

    // Fix char.
    if (found)
        fix_character(ch);

    /* Redo shift. Remember ch->shifted was just used as a placeholder to tell the game
       to re-shift, so we have to switch it to none first. */
    if (ch->shifted != SHIFTED_NONE) {
        ch->shifted = SHIFTED_NONE;
        shift_char(ch, true);
    }

    variable_fix_list(last_var ? last_var : variable_head);

    ch->pcdata->last_login = current_time;

if (found && !IS_NPC(ch) && 
    (ch->pcdata->account_id[0] != 0 || !IS_NULLSTR(ch->pcdata->account_name)) && d->host != NULL) {
    
    // First determine if migration is needed
    bool migration_needed = ((!IS_NULLSTR(ch->pcdata->pwd) && ch->pcdata->account_pwd_override) || 
                           !IS_NULLSTR(ch->pcdata->reset_code) ||
                           ch->pcdata->mfa_enabled ||
                           !IS_NULLSTR(ch->pcdata->email));
    
    // Only lookup account if we need to migrate or if descriptor doesn't have an account
    if (migration_needed || d->account == NULL) {
        ACCOUNT_DATA *account = NULL;
        
        // Try name lookup first (faster)
        if (!IS_NULLSTR(ch->pcdata->account_name)) {
            log_string(formatf("load_char_obj: looking up account %s by name", ch->pcdata->account_name));
            account = find_account_by_name(ch->pcdata->account_name);
        }
        
        // Only fall back to ID lookup if necessary and we have valid IDs
        if (account == NULL && ch->pcdata->account_id[0] != 0) {
            log_string(formatf("load_char_obj: falling back to ID lookup for account"));
            account = find_account_by_id(ch->pcdata->account_id[0], ch->pcdata->account_id[1]);
        }
        
        if (account != NULL) {
            // Perform migration if needed
            if (migration_needed) {
                log_string(formatf("load_char_obj: migrating authentication data for %s to account %s",
                    ch->name, account->username));
                
                // Update account with character's data
                account_add_character(account, ch);
                
                // Save the account with migrated data
                save_account(account);
                
                // Save character with cleared auth data
                save_char_obj(ch);
            }
            
            // Update descriptor's account pointer if needed
            if (d->account == NULL) {
                d->account = account;
            } else if (d->account != account) {
                // We already had an account loaded into the descriptor
                // Just free this one we loaded temporarily
                free_account(account);
            }
        } else if (migration_needed) {
            log_stringf("load_char_obj: Character %s has account identifiers but account could not be found by name ('%s') or ID (%lu %lu).",
                ch->name,
                ch->pcdata->account_name ? ch->pcdata->account_name : "NULL",
                ch->pcdata->account_id[0], ch->pcdata->account_id[1]);
        }
    }
    }  // end if (found && !IS_NPC(ch) && ...)

    // Performance logging for any character with inventory
    if (found && ch) {
        gettimeofday(&end_time, NULL);
        int obj_count = (ch->lcarrying ? list_size(ch->lcarrying) : 0) +
                       (ch->llocker ? list_size(ch->llocker) : 0) +
                       (ch->lworn ? list_size(ch->lworn) : 0);

        if (obj_count > 10) {
            total_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                      (end_time.tv_usec - start_time.tv_usec) / 1000;
            log_stringf("PERFORMANCE load_char_obj: %s with %d objects - total: %ldms",
                       ch->name, obj_count, total_ms);
        }

        // Mark character as fully loaded (JSON sets this in json_read_char/_basic, old pfile always loads everything)
        if (ch->pcdata && load_full) {
            ch->pcdata->fully_loaded = true;
        }

        // Cache character info on FULL load (not basic load) to warm cache
        // Benefits: reconnects, admin commands, account features
        // Only happens on login/reconnect, not on every access
        if (load_full && ch->pcdata) {
            redis_cache_char_info(ch);
        }
    }

    return found;
}

// Public wrapper - load full character data (inventory, equipment, skills, etc.)
bool load_char_obj(DESCRIPTOR_DATA *d, const char *name)
{
    return load_char_obj_internal(d, name, true);
}

// Public wrapper - load basic character data only (no inventory, equipment, skills)
// Used for character menu display - defers heavy loading until game entry
bool load_char_obj_basic(DESCRIPTOR_DATA *d, const char *name)
{
    return load_char_obj_internal(d, name, false);
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
     *
     * MIGRATION NOTE: For top-level objects (in_obj=NULL), skip next_content
     * traversal because they're managed by LLIST now (llocker, lcarrying, lworn).
     * Their next_content is legacy from the old linked-list system.
     *
     * ONLY traverse next_content for nested objects inside containers (in_obj != NULL).
     * These use next_content to link siblings within the same container.
     */
    bool is_nested = (obj->in_obj != NULL);
    if (obj->next_content != NULL && is_nested)
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
        fprintf(fp, "Room %ld\n",	obj->in_room->vnum	    );
    if (IS_SET(obj->extra[1], ITEM_ENCHANTED))
        fprintf(fp,"Enchanted_times %d\n", obj->num_enchanted);


    if (obj->script_created)
    {
        fprintf(fp, "Created_script_type %d\n", obj->created_script_type);
        fprintf(fp, "Created_script_vnum %ld\n", obj->created_script_load.vnum);
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
    if (obj->locker == true)
        fprintf(fp, "Locker %d\n", obj->locker);
    if (obj->stached == true)
        fprintf(fp, "Stached %d\n", obj->stached);

    if (obj->lock)
        fprintf(fp, "LockW %ld %ld %d %d\n", obj->lock->key_load.auid, obj->lock->key_load.vnum, obj->lock->flags, obj->lock->pick_chance);

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

    /* Type-specific data (canonical, as JSON) — replaces legacy value[] */
    {
        json_t *td = obj_type_data_to_json(obj);
        if (td) {
            char *td_str = json_dumps(td, JSON_COMPACT | JSON_SORT_KEYS);
            if (td_str) {
                fprintf(fp, "TypeData %s~\n", td_str);
                free(td_str);
            }
            json_decref(td);
        }
    }

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
    for (CATALYST_DATA *cat = obj->catalyst; cat != NULL; cat = cat->next)
    {
        if( IS_NULLSTR(cat->custom_name) )
        {
            fprintf(fp, "%s '%s' %3d %3d %3d\n",
                ((cat->where == TO_CATALYST_ACTIVE) ? "CataA" : "Cata"),
                flag_string( catalyst_types, cat->type ),
                cat->level,
                cat->modifier,
                cat->duration);
        }
        else
        {
            fprintf(fp, "%s '%s' %3d %3d %3d %s\n",
                ((cat->where == TO_CATALYST_ACTIVE) ? "CataNA" : "CataN"),
                flag_string( catalyst_types, cat->type ),
                cat->level,
                cat->modifier,
                cat->duration,
                cat->custom_name
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
    //ROOM_INDEX_DATA *room = NULL;

    fVnum = false;
    obj = NULL;
    first = true;  /* used to counter fp offset */
    make_new = false;

    word   = feof(fp) ? "End" : fread_word(fp);
    if (!str_cmp(word,"Vnum"))
    {
        long vnum;
        first = false;  /* fp will be in right place */

        vnum = fread_number(fp);
        if ( get_obj_index_global(vnum)  == NULL)
            pbugf(LOG_ERROR, "Fread_obj: bad vnum %ld.", vnum);
        else
            obj = create_object_noid(get_obj_index_global(vnum),-1, false, false);
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

    fVnum		= true;
    iNest		= 0;

    for (; ;)
    {
        if (first)
            first = false;
        else if(feof(fp))
        {
            pbugf(LOG_ERROR, "EOF encountered reading object from pfile");
            word = "End";
        } else
            word   = fread_word(fp);
        fMatch = false;

        switch (UPPER(word[0]))
        {
        case '*':
            fMatch = true;
            fread_to_eol(fp);
            break;
        case '#':
            if (!str_cmp(word, "#TOKEN"))
            {
                TOKEN_DATA *token = fread_token(fp);
                if (token)
                    token_to_obj(token, obj);
                fMatch		= true;
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
                    pbugf(LOG_ERROR, "Fread_obj: unknown skill.");
                else
                    paf->type = sn;
                paf->skill = skill_find_uid(sn);

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

                paf->type = -1;

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
                fMatch          = true;
                break;
            }

            if (!str_cmp(word,"Affc"))
            {
                AFFECT_DATA *paf;
                int sn;

                paf = new_affect();

                sn = skill_lookup(fread_word(fp));
                if (sn < 0)
                    pbugf(LOG_ERROR, "Fread_obj: unknown skill.");
                else
                    paf->type = sn;
                paf->skill = skill_find_uid(sn);

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
                int sn;

                paf = new_affect();

                sn = skill_lookup(fread_word(fp));
                if (sn < 0)
                    pbugf(LOG_ERROR, "Fread_obj: unknown skill.");
                else
                    paf->type = sn;
                paf->skill = skill_find_uid(sn);

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
                fMatch = true;
                break;
            }
            break;

        case 'C':
            if (!str_cmp(word, "Cata"))
            {
                CATALYST_DATA *cat;

                cat = new_catalyst();

                cat->type = flag_value(catalyst_types,fread_word(fp));
                if(cat->type == NO_FLAG) {
                    log_string("fread_char: invalid catalyst type.");
                    free_catalyst(cat);
                } else {
                    cat->custom_name = NULL;
                    cat->where		= TO_CATALYST_DORMANT;
                    cat->level       = fread_number(fp);
                    cat->modifier    = fread_number(fp);
                    cat->duration    = fread_number(fp);
                    cat->next        = obj->catalyst;
                    obj->catalyst    = cat;
                }
                fMatch = true;
                break;
            }

            if (!str_cmp(word, "CataA"))
            {
                CATALYST_DATA *cat;

                cat = new_catalyst();

                cat->type = flag_value(catalyst_types,fread_word(fp));
                if(cat->type == NO_FLAG) {
                    log_string("fread_char: invalid catalyst type.");
                    free_catalyst(cat);
                } else {
                    cat->custom_name = NULL;
                    cat->where		= TO_CATALYST_ACTIVE;
                    cat->level       = fread_number(fp);
                    cat->modifier    = fread_number(fp);
                    cat->duration    = fread_number(fp);
                    cat->next        = obj->catalyst;
                    obj->catalyst    = cat;
                }
                fMatch = true;
                break;
            }

            if (!str_cmp(word, "CataN"))
            {
                CATALYST_DATA *cat;

                cat = new_catalyst();

                cat->type = flag_value(catalyst_types,fread_word(fp));
                if(cat->type == NO_FLAG) {
                    log_string("fread_char: invalid catalyst type.");
                    free_catalyst(cat);
                } else {
                    cat->where		= TO_CATALYST_DORMANT;
                    cat->level       = fread_number(fp);
                    cat->modifier    = fread_number(fp);
                    cat->duration    = fread_number(fp);
                    cat->custom_name = fread_string_eol(fp);
                    cat->next        = obj->catalyst;
                    obj->catalyst    = cat;
                }
                fMatch = true;
                break;
            }

            if (!str_cmp(word, "CataNA"))
            {
                CATALYST_DATA *cat;

                cat = new_catalyst();

                cat->type = flag_value(catalyst_types,fread_word(fp));
                if(cat->type == NO_FLAG) {
                    log_string("fread_char: invalid catalyst type.");
                    free_catalyst(cat);
                } else {
                    cat->where		= TO_CATALYST_ACTIVE;
                    cat->level       = fread_number(fp);
                    cat->modifier    = fread_number(fp);
                    cat->duration    = fread_number(fp);
                    cat->custom_name = fread_string_eol(fp);
                    cat->next        = obj->catalyst;
                    obj->catalyst    = cat;
                }
                fMatch = true;
                break;
            }
            KEY("Cond",	obj->condition,		fread_number(fp));
            KEY("Cost",	obj->cost,		fread_number(fp));

            if (!str_cmp(word, "Created_script_type"))
            {
                obj->created_script_type = fread_number(fp);
                obj->script_created = true;
                fMatch = true;
                break;
            }

            if (!str_cmp(word, "Created_script_vnum"))
            {
                obj->created_script_load.vnum = fread_number(fp);
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
                    pbugf(LOG_ERROR, "Fread_obj: incomplete object.");
                    free_obj(obj);
                    return NULL;
                }
                // OPTIMIZATION: Disable expensive O(n²) duplicate detection during load
                // All duplication bugs have been fixed (see COMPLETE_DUPLICATION_FIX_SUMMARY.md)
                // This was causing 5+ million comparisons for a 3271-object character!
                // Deduplication still runs during SAVE (remove_duplicate_objects_from_char)
                // If duplicates somehow appear, they'll be caught and cleaned up on next save.
                /* DISABLED FOR PERFORMANCE - was taking ~1 second for 3271 objects
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
                */
                else
                {
                    if (!fVnum)
                    {
                        free_obj(obj);
                        obj = create_object(get_reserved_obj_index("obj_system_dummy"), 0 , false);
                        // create_object already added to loaded_objects
                    }
                    else
                    {
                        // fVnum path: created by create_object_noid with add_to_loaded_objs=false,
                        // so it won't be in the list yet — add directly without O(n) scan.
                        list_appendlink(loaded_objects, obj);
                        loaded_obj_hash_add(obj);
                        obj->pIndexData->count++;
                    }
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
                long vnum;

                vnum = fread_number(fp);
                if ((pIndexData = get_obj_index_global(vnum)) != NULL)
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

            if( !str_cmp(word,"Lock") )
            {
                if( !obj->lock )
                {
                    obj->lock = new_lock_state();
                }

                obj->lock->key_load.auid = 0;
                obj->lock->key_load.vnum = fread_number(fp);
                obj->lock->flags = fread_number(fp);
                obj->lock->pick_chance = fread_number(fp);

                fMatch = true;
                break;
            }

            if( !str_cmp(word,"LockW") )
            {
                if( !obj->lock )
                {
                    obj->lock = new_lock_state();
                }

                obj->lock->key_load.auid = fread_number(fp);
                obj->lock->key_load.vnum = fread_number(fp);
                obj->lock->flags = fread_number(fp);
                obj->lock->pick_chance = fread_number(fp);

                fMatch = true;
                break;
            }


            if (!str_cmp(word, "Level") || !str_cmp(word, "Lev"))
            {
                obj->level = fread_number(fp);

                if (obj->pIndexData != NULL)
                {
                    OBJ_INDEX_DATA *alumnos_armor = get_reserved_obj_index("obj_alemnos_armor");
                    if (alumnos_armor != NULL && obj->pIndexData == alumnos_armor) {
                        int armour;
                        int armour_exotic;

                        armour=(int) calc_obj_armour(obj->level, legacy_obj_value_get(obj, 4));
                        armour_exotic=(int) armour * .90;

                        legacy_obj_value_set(obj, 0, armour);
                        legacy_obj_value_set(obj, 1, armour);
                        legacy_obj_value_set(obj, 2, armour);
                        legacy_obj_value_set(obj, 3, armour_exotic);
                    }
                }

                fMatch = true;
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

                fMatch = true;
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
                    pbugf(LOG_ERROR, "Fread_obj: bad nest %d.", iNest);
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
            KEY("PermExtra",		obj->extra_perm[0],	fread_number(fp));
            KEY("PermExtra2",		obj->extra_perm[1],	fread_number(fp));
            KEY("PermExtra3",		obj->extra_perm[2],	fread_number(fp));
            KEY("PermExtra4",		obj->extra_perm[3],	fread_number(fp));
            KEY("PermWeapon",		obj->weapon_flags_perm,	fread_number(fp));
            KEY("Persist",			obj->persist,			fread_number(fp));
            break;

        case 'R':
            if (!str_cmp(word, "Room"))
            {
                ROOM_INDEX_DATA *room;
                long vnum = fread_number(fp);
                AREA_DATA *area = NULL;
                WNUM wnum;
                if (resolve_widevnum(vnum, NULL, &wnum))
                    area = wnum.pArea;
                if (!area) area = get_system_area_fallback();
                room = get_room_index(area, vnum);
                obj->in_room = room;
                fMatch = true;
            }
            break;

        case 'S':
            KEY("ShortDescr",	obj->short_descr,	fread_string(fp));
            KEY("ShD",		obj->short_descr,	fread_string(fp));

            if (!str_cmp(word, "SpellNew"))
            {
                int sn;
                SPELL_DATA *spell;

                fMatch = true;
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
                    pbugf(LOG_ERROR, "Bad spell name for %s (%ld).", obj->short_descr, obj->pIndexData->vnum);
                }
            }

            if (!str_cmp(word, "Spell"))
            {
                int iValue;
                int sn;

                iValue = fread_number(fp);
                sn = skill_lookup(fread_word(fp));
                if (iValue < 0 || iValue > 7)
                    pbugf(LOG_ERROR, "Fread_obj: bad iValue %d.", iValue);
                else if (sn < 0)
                    pbugf(LOG_ERROR, "Fread_obj: unknown skill.");
                else
                {
                    if (obj->item_type == ITEM_WEAPON || obj->item_type == ITEM_ARMOUR)
                    {
                        if (iValue == 1)
                            legacy_obj_value_set(obj, 6, sn);
                        else
                            legacy_obj_value_set(obj, 7, sn);
                    }
                    else
                        legacy_obj_value_set(obj, iValue, sn);
                }
                fMatch = true;
                break;
            }

            break;

        case 'T':
            KEY("TimesAllowedFixed", obj->times_allowed_fixed, fread_number(fp));
            KEY("Timer",	obj->timer,		fread_number(fp));
            KEY("Time",	obj->timer,		fread_number(fp));

            if (!str_cmp(word, "TypeData"))
            {
                char *td_str = fread_string(fp);
                if (td_str && td_str[0]) {
                    json_error_t err;
                    json_t *td = json_loads(td_str, 0, &err);
                    if (td) {
                        obj_type_data_from_json(obj, td);
                        json_decref(td);
                    }
                }
                free_string(td_str);
                fMatch = true;
            }
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

                {
                    int value_count = (obj->version > 0) ? 8 : 5;
                    int vi;

                    for (vi = 0; vi < value_count; vi++)
                        obj->value[vi] = fread_number(fp);

                    for (; vi < 8; vi++)
                        obj->value[vi] = 0;
                }

                if (obj->item_type == ITEM_WEAPON && legacy_obj_value_get(obj, 0) == 0)
                    legacy_obj_value_set(obj, 0, legacy_obj_index_value_get(obj->pIndexData, 0));

                break;
            }

            if ((!str_cmp(word, "Val")) && obj->item_type != ITEM_WEAPON && obj->item_type != ITEM_ARMOUR)
            {
                int vi;
                for (vi = 0; vi <= 5; vi++)
                    obj->value[vi] = fread_number(fp);
                fMatch = true;
                break;
            }

            if( (vtype = variable_fread_type(word)) != VAR_UNKNOWN ) {
                variable_fread(&obj->progs->vars, vtype, fp);
                fMatch = true;
            }

            if (!str_cmp(word, "Vnum"))
            {
                long vnum;

                vnum = fread_number(fp);
                if ((obj->pIndexData = get_obj_index_global(vnum)) == NULL)
                    pbugf(LOG_ERROR, "Fread_obj: bad vnum %ld.", vnum);
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
            //pbugf(LOG_ERROR, "fread_obj: unknown obj flag %s", word);
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
    pbugf(LOG_ERROR, "perm_objs_new.dat: Couldn't open file.");
    else
    {
        log_event_t ev = {
            .severity = EVENT_SEV_INFO,
            .category = LOG_DEBUG,
            .plain_message = "writing permanent objects...",
            .staff_message = "writing permanent objects...",
            .wiznet_flag = WIZ_TESTING,
            .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__,
        };
        log_emit_event(&ev, NULL);

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
ITERATOR chit;
iterator_start(&chit, list_churches);
while ((church = (CHURCH_DATA *)iterator_nextdata(&chit))) {
    CHURCH_TREASURE_ROOM *treasure;
    ITERATOR it;
    iterator_start(&it, church->treasure_rooms);
    while ((treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it))) {
        if (treasure->room->contents != NULL)
            fwrite_obj_new(NULL, treasure->room->contents, fp, 0);
    }
    iterator_stop(&it);
}
iterator_stop(&chit);

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
    pbugf(LOG_ERROR, "perm_objs_new.dat: Couldn't open file.");
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
                AREA_DATA *area = NULL;
                WNUM wnum;
                if (resolve_widevnum(obj->in_room->vnum, NULL, &wnum))
                    area = wnum.pArea;
                if (!area) area = get_system_area_fallback();
                ROOM_INDEX_DATA *to_room = get_room_index(area, obj->in_room->vnum);
                obj->in_room = NULL;
                obj_to_room(obj, to_room == NULL ? get_room_index(get_system_area_fallback(), 1) : to_room);
                }
            }
            }
            else if (!str_cmp(word, "#END"))
                break;
            else {
            pbugf(LOG_ERROR, "perm_objs_new.dat: bad format");
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
    char buf[MSL];
    int i, sn, level;
    int af_level = 0;
    int af_hr_mod = 0;
    int af_dr_mod = 0;
    AFFECT_DATA *af, *af_next;
    SPELL_DATA *spell, *spell_new;

    if (obj == NULL) {
        pbugf(LOG_ERROR, "fix_object: obj was null.");
        return;
    }

    if (obj->pIndexData == NULL) {
        pbugf(LOG_ERROR, "fix_object: pIndexData was null.");
        return;
    }

    //////////////////////////////////////////////////////////////////////
    // LEGACY UPDATES

    if (obj->version == 0) {
        bool fEnchanted = false;

        if (IS_SET(obj->extra[1], ITEM_ENCHANTED))
            fEnchanted = true;

        obj->extra[1] = obj->pIndexData->extra[1] | obj->extra[1];
        obj->extra[2] = obj->pIndexData->extra[2] | obj->extra[2];
        obj->extra[3] = obj->pIndexData->extra[3] | obj->extra[3];

        if (fEnchanted)
            SET_BIT(obj->extra[1], ITEM_ENCHANTED);

        if (is_quest_item(obj))
            obj->cost = obj->pIndexData->cost;
    }

    if (obj->version == 1)
    {
        // remove dup affects - don't do for now
        // cleanup_affects(obj);

        // Fix skulls
        if (obj->pIndexData == get_reserved_obj_index("obj_skull_normal") || obj->pIndexData == get_reserved_obj_index("obj_skull_golden")) {
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
        if (IS_SET(obj->extra[1], ITEM_ENCHANTED))
        {
            int16_t sn_ench = skill_resolve_gsn("enchant weapon");
            for (af = obj->affected; af != NULL; af = af_next)
            {
                af_next = af->next;

                if (af->type == sn_ench)
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
                af->type = sn_ench;
                affect_to_obj(obj, af);

                // DR mods
                af = new_affect();
                af->group = AFFGROUP_ENCHANT;
                af->level = af_level;
                af->duration = -1;
                af->location = APPLY_DAMROLL;
                af->modifier = af_dr_mod;
                af->type = sn_ench;
                affect_to_obj(obj, af);
            }
        }

        // Update spells to be done the correct way.
        if (obj->spells == NULL)
        {
        int legacy_values[8];
        for (i = 0; i < 8; i++)
            legacy_values[i] = obj->value[i];

        switch (obj->item_type)
        {
        case ITEM_PILL:
        case ITEM_POTION:
        case ITEM_SCROLL:
                if (legacy_values[0] > 0)
            level = legacy_values[0];
                else
            level = obj->level;

                for (i = 1; i < 4; i++)
            {
            if ((sn = legacy_values[i]) > 0 && sn < MAX_SKILL
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
                if (legacy_values[0] > 0)
            level = legacy_values[0];
            else
            level = obj->level;

            if ((sn = legacy_values[3]) > 0 && sn < MAX_SKILL
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
        if (IS_SET(obj->extra[0], ITEM_HIDDEN)) {
            REMOVE_BIT(obj->extra[0], ITEM_HIDDEN);
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
        bool is_enchanted = false;

        if (IS_SET(obj->extra[1], ITEM_ENCHANTED))
            is_enchanted = true;


        obj->extra[0] = obj->extra_perm[0] = obj->pIndexData->extra[0];
        obj->extra[1] = obj->extra_perm[1] = obj->pIndexData->extra[1];
        obj->extra[2] = obj->extra_perm[2] = obj->pIndexData->extra[2];
        obj->extra[3] = obj->extra_perm[3] = obj->pIndexData->extra[3];

        if( obj->item_type == ITEM_WEAPON )
        {
            obj->weapon_flags_perm = legacy_obj_index_value_get(obj->pIndexData, 4);
            legacy_obj_value_set(obj, 4, obj->weapon_flags_perm);
        }

        for(paf = obj->affected; paf; paf = paf->next )
        {
            if (paf->bitvector)
            {
                switch (paf->where)
                {
                case TO_OBJECT:
                    SET_BIT(obj->extra[0],paf->bitvector);
                    break;
                case TO_OBJECT2:
                    SET_BIT(obj->extra[1],paf->bitvector);
                    break;
                case TO_OBJECT3:
                    SET_BIT(obj->extra[2],paf->bitvector);
                    break;
                case TO_OBJECT4:
                    SET_BIT(obj->extra[3],paf->bitvector);
                    break;
                case TO_WEAPON:
                    if (obj->item_type == ITEM_WEAPON)
                    {
                        int weapon_flags = legacy_obj_value_get(obj, 4);
                        SET_BIT(weapon_flags, paf->bitvector);
                        legacy_obj_value_set(obj, 4, weapon_flags);
                    }
                break;
                }
            }
        }

        if( is_enchanted )
            SET_BIT(obj->extra[1], ITEM_ENCHANTED);

        obj->version = VERSION_OBJECT_002;
    }

    if( obj->version < VERSION_OBJECT_003 ) {


        obj->version = VERSION_OBJECT_003;
    }

    if( obj->version < VERSION_OBJECT_004 )
    {
        if( !obj->lock )
        {
            switch(obj->item_type)
            {
            case ITEM_CONTAINER:
            case ITEM_BOOK:
                // Value[1] == CONT flags
                // Value[2] == Key

                {
                long lock_flags = legacy_obj_value_get(obj, 1);
                if (obj->item_type == ITEM_CONTAINER && IS_CONTAINER(obj))
                    lock_flags = CONTAINER(obj)->flags;
                else if (obj->item_type == ITEM_BOOK && IS_BOOK(obj))
                    lock_flags = BOOK(obj)->flags;

                if( (legacy_obj_value_get(obj, 2) > 0) || IS_SET(lock_flags, VO_004_CONT_LOCKED) )
                {
                    obj->lock = new_lock_state();
                    obj->lock->key_load.vnum = legacy_obj_value_get(obj, 2);
                    obj->lock->flags = 0;
                    obj->lock->pick_chance = 100;

                    if( IS_SET(lock_flags, VO_004_CONT_LOCKED) )
                    {
                        SET_BIT(obj->lock->flags, LOCK_LOCKED);
                    }

                    if( IS_SET(lock_flags, VO_004_CONT_PICKPROOF) )
                    {
                        obj->lock->pick_chance = 0;
                    }

                    if( IS_SET(lock_flags, VO_004_CONT_SNAPKEY) )
                    {
                        SET_BIT(obj->lock->flags, LOCK_SNAPKEY);
                    }

                    // Remove the old data
                    REMOVE_BIT(lock_flags, (VO_004_CONT_PICKPROOF|VO_004_CONT_LOCKED|VO_004_CONT_SNAPKEY));
                    if (obj->item_type == ITEM_CONTAINER && IS_CONTAINER(obj))
                        CONTAINER(obj)->flags = lock_flags;
                    else if (obj->item_type == ITEM_BOOK && IS_BOOK(obj))
                        BOOK(obj)->flags = lock_flags;
                    legacy_obj_value_set(obj, 1, lock_flags);
                    legacy_obj_value_set(obj, 2, 0);
                }
                }
                break;

            case ITEM_PORTAL:
                // Value[1] == EXIT flags
                // Value[4] == Key

                {
                long exit_flags = (IS_PORTAL(obj) ? PORTAL(obj)->exit : legacy_obj_value_get(obj, 1));

                if( (legacy_obj_value_get(obj, 4) > 0) || IS_SET(exit_flags, VO_004_EX_LOCKED) )
                {
                    obj->lock = new_lock_state();
                    obj->lock->key_load.vnum = legacy_obj_value_get(obj, 4);
                    obj->lock->flags = 0;
                    obj->lock->pick_chance = 100;

                    if( IS_SET(exit_flags, VO_004_EX_LOCKED) )
                    {
                        SET_BIT(obj->lock->flags, LOCK_LOCKED);
                    }

                    if( IS_SET(exit_flags, VO_004_EX_PICKPROOF) )
                    {
                        obj->lock->pick_chance = 0;
                    }
                    else if( IS_SET(exit_flags, VO_004_EX_INFURIATING) )
                    {
                        obj->lock->pick_chance = 10;
                    }
                    else if( IS_SET(exit_flags, VO_004_EX_HARD) )
                    {
                        obj->lock->pick_chance = 40;
                    }
                    else if( IS_SET(exit_flags, VO_004_EX_EASY) )
                    {
                        obj->lock->pick_chance = 80;
                    }


                    REMOVE_BIT(exit_flags, (VO_004_EX_LOCKED|VO_004_EX_PICKPROOF|VO_004_EX_INFURIATING|VO_004_EX_HARD|VO_004_EX_EASY));
                    if (IS_PORTAL(obj))
                        PORTAL(obj)->exit = exit_flags;
                    legacy_obj_value_set(obj, 1, exit_flags);
                    legacy_obj_value_set(obj, 4, 0);
                }
                }
                break;


//			case ITEM_WEAPON_CONTAINER:
//			case ITEM_DRINKCONTAINER:
//				break;

            }
        }


        obj->version = VERSION_OBJECT_004;
    }

    if( obj->version < VERSION_OBJECT_005 )
    {
        // Populate type-specific data structs from legacy value[] array.
        // After this migration, the type structs are the canonical data source.
        obj_migrate_values_to_types(obj);

        obj->version = VERSION_OBJECT_005;
    }

    // Resolve bare portal destination vnums to area UIDs
    if (obj->item_type == ITEM_PORTAL) {
        long dest_vnum = 0;
        long dest_area_uid = 0;
        long portal_flags = 0;

        if (IS_PORTAL(obj)) {
            dest_vnum = PORTAL(obj)->params[0];
            dest_area_uid = PORTAL(obj)->params[4];
            portal_flags = PORTAL(obj)->flags;
        } else {
            dest_vnum = legacy_obj_value_get(obj, 3);
            dest_area_uid = legacy_obj_value_get(obj, 4);
            portal_flags = legacy_obj_value_get(obj, 2);
        }

        if (dest_vnum > 0 && dest_area_uid == 0 && !IS_SET(portal_flags, GATE_DUNGEON)) {
            AREA_DATA *dest_area = find_area_by_vnum(dest_vnum,
                obj->pIndexData ? obj->pIndexData->area : NULL);
            if (dest_area) {
                if (IS_PORTAL(obj))
                    PORTAL(obj)->params[4] = dest_area->uid;
                else
                    legacy_obj_value_set(obj, 4, dest_area->uid);
            }
        }
    }

    // Just update it
    obj->version = VERSION_OBJECT;
}


/* Cleanup affects on an obj. This currently just removes dual affects. */
void cleanup_affects(OBJ_DATA *obj)
{
    AFFECT_DATA *af;
    AFFECT_DATA *af_next;
    int count;
    int apply_type;

    if (obj == NULL)
    {
    pbugf(LOG_ERROR, "cleanup_affects: obj was null!");
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
        pbugf(LOG_ERROR, "cleanup_affects: obj %s (%ld)",
            obj->short_descr, obj->pIndexData->vnum);
        }
    }

    }
}


#define HAS_ALL_BITS(a, b) (((a) & (b)) == (a))

void fix_character( CHAR_DATA *ch )
{
    int i;
    char buf[MSL];
    bool resetaffects = false;
    AFFECT_DATA *paf;

    if (!ch->race)
    ch->race = race_lookup("human");

    ch->size = ch->race ? ch->race->min_size : SIZE_MEDIUM;
    ch->dam_type = 17; /*punch */

    // Add groups it should know
    {
        ITERATOR sg_it;
        SKILL_GROUP *sg;
        iterator_start(&sg_it, ch->pcdata->known_groups);
        while ((sg = (SKILL_GROUP *)iterator_nextdata(&sg_it))) {
            gn_add(ch, sg);
        }
        iterator_stop(&sg_it);
    }

    /* make sure they have any new race skills */
    if (ch->race && ch->race->skills) {
        ITERATOR it;
        char *skill_name;
        iterator_start(&it, ch->race->skills);
        while ((skill_name = (char *)iterator_nextdata(&it))) {
            group_add(ch, skill_name, false);
        }
        iterator_stop(&it);
    }

    // TODO: Readd checks for dealing with racial affects, affects2, imm, res and vuln

    /* 20203003 - Tieryo - Fix missing racial perm affects */

    if (ch->race && (ch->affected_by_perm[0] != ch->race->aff[0] || ch->affected_by_perm[1] != ch->race->aff[1]))
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

        // Iterate through all worn objects using lworn linked list
        if (ch->lworn && IS_VALID(ch->lworn)) {
            ITERATOR it;
            OBJ_DATA *obj;
            iterator_start(&it, ch->lworn);
            while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
                for (paf = obj->affected; paf; paf = paf->next) {
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
    
    // Check worn items for deathsight using lworn
    if (ch->lworn && IS_VALID(ch->lworn)) {
        ITERATOR it;
        OBJ_DATA *obj;
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            for(paf = obj->affected; paf; paf = paf->next)
            {
                if( (paf->where == TO_AFFECTS) && IS_SET(paf->bitvector2, AFF2_DEATHSIGHT) && (paf->level > ch->deathsight_vision) )
                    ch->deathsight_vision = paf->level;
            }
        }
        iterator_stop(&it);
    }

    ch->form = ch->race ? ch->race->form : 0;
    ch->parts = ch->race ? (ch->race->parts & ~ch->lostparts) : 0;
    ch->lostparts = 0;

    /* Legacy version migrations for old .dat format pfiles.
     * These are needed when loading characters saved before the JSON migration.
     * Each block bumps ch->version so the migration only runs once per character. */

    if (ch->version < 2)
    {
        group_add(ch,"global skills",false);
        {
            CLASS_DATA *v2_class = class_from_legacy(ch->pcdata->class_current, -1);
            if (v2_class) {
                ITERATOR git;
                SKILL_GROUP *sg;
                iterator_start(&git, v2_class->groups);
                while ((sg = (SKILL_GROUP *)iterator_nextdata(&git)))
                    group_add(ch, sg->name, false);
                iterator_stop(&git);
            } else {
                pbugf(LOG_INIT, "fix_character: unable to map legacy base class %d for %s",
                    ch->pcdata->class_current, ch->name ? ch->name : "(unknown)");
            }
        }
        ch->version = 2;
    }

    /* make sure they have any new skills that have been added */
    {
        int *class_ptrs[] = {
            &ch->pcdata->class_mage, &ch->pcdata->class_cleric,
            &ch->pcdata->class_thief, &ch->pcdata->class_warrior,
        };
        int *remort_ptrs[] = {
            &ch->pcdata->second_sub_class_mage, &ch->pcdata->second_sub_class_cleric,
            &ch->pcdata->second_sub_class_thief, &ch->pcdata->second_sub_class_warrior,
        };
        for (int si = 0; si < 4; si++) {
            if (*class_ptrs[si] != -1) {
                CLASS_DATA *bc = class_from_legacy(*class_ptrs[si], -1);
                if (bc) {
                    ITERATOR git; SKILL_GROUP *sg;
                    iterator_start(&git, bc->groups);
                    while ((sg = (SKILL_GROUP *)iterator_nextdata(&git)))
                        group_add(ch, sg->name, false);
                    iterator_stop(&git);
                } else {
                    pbugf(LOG_INIT,
                        "fix_character: unable to map legacy base class slot %d value %d for %s",
                        si, *class_ptrs[si], ch->name ? ch->name : "(unknown)");
                }
            }
            if (*remort_ptrs[si] != -1) {
                CLASS_DATA *rc = class_from_legacy(0, *remort_ptrs[si]);
                if (rc) {
                    ITERATOR git; SKILL_GROUP *sg;
                    iterator_start(&git, rc->groups);
                    while ((sg = (SKILL_GROUP *)iterator_nextdata(&git)))
                        group_add(ch, sg->name, false);
                    iterator_stop(&git);
                } else {
                    pbugf(LOG_INIT,
                        "fix_character: unable to map legacy subclass slot %d value %d for %s",
                        si, *remort_ptrs[si], ch->name ? ch->name : "(unknown)");
                }
            }
        }
    }

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
            if (ch->tot_level < wiznet_table[i].rank)
            {
            REMOVE_BIT(ch->wiznet, wiznet_table[i].flag);
            }

            i++;
        }
    }

    for (i = 0; i < MAX_STATS; i++)
        if (ch->race && ch->perm_stat[i] > ch->race->max_stats[i])
            set_perm_stat(ch, i, ch->race->max_stats[i]);

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

    if (ch->version < VERSION_PLAYER_007 )
    {
        SET_BIT(ch->act[1], PLR_COMPASS);
        SET_BIT(ch->act[1], PLR_AUTOCAT);

        ch->version = VERSION_PLAYER_007;
    }

    /* Legacy level-to-staff-rank migration for old .dat format pfiles.
     * Old pfiles stored immortal level as tot_level (150-155).
     * Convert to modern staff_rank system. */
    if (ch->tot_level >= OLD_LEVEL_MINIGOD && ch->pcdata->staff_rank == STAFF_PLAYER)
    {
        switch(ch->tot_level)
        {
        default:                               ch->pcdata->staff_rank = STAFF_IMMORTAL; break;
        case OLD_LEVEL_ASCENDANT:      ch->pcdata->staff_rank = STAFF_ASCENDANT; break;
        case OLD_LEVEL_SUPREMACY:      ch->pcdata->staff_rank = STAFF_SUPREMACY; break;
        case OLD_LEVEL_CREATOR:                ch->pcdata->staff_rank = STAFF_CREATOR; break;
        case OLD_LEVEL_IMPLEMENTOR:    ch->pcdata->staff_rank = STAFF_IMPLEMENTOR; break;
        }
    }

    /* VERSION_PLAYER_010: Migrate legacy bitfield state into preferences.
     * Characters without explicit preferences get their current toggle,
     * channel, prompt, wimpy, and scroll state snapshotted so account
     * preference inheritance works correctly. */
    if (ch->version < VERSION_PLAYER_010) {
        pref_migrate_character(ch);
        ch->version = VERSION_PLAYER_010;
    }

    /* VERSION_PLAYER_011: Migrate learned[]/mod_learned[] into SKILL_ENTRY.
     * Prior to this version, skill percentages lived only in the PC_DATA
     * arrays. This one-time migration copies them into the SKILL_ENTRY
     * fields (rating/mod_rating) so the entry becomes the source of truth.
     * The learned[] arrays are still written to for backward compatibility
     * during the transition period. */
    if (ch->version < VERSION_PLAYER_011) {
        SKILL_ENTRY *entry;
        for (entry = ch->sorted_skills; entry; entry = entry->next) {
            if (entry->sn > 0 && entry->sn < MAX_SKILL) {
                entry->rating = ch->pcdata->learned[entry->sn];
                entry->mod_rating = ch->pcdata->mod_learned[entry->sn];
            }
        }
        ch->version = VERSION_PLAYER_011;
    }

    /* VERSION_PLAYER_012: Migrate legacy class fields to CLASS_LEVEL entries.
     * Characters saved with the old 18-field system (class_mage, sub_class_mage,
     * etc.) need their data converted into the new CLASS_LEVEL list.  If the
     * character was already saved in JSON with class data, the list will be
     * non-empty and we skip the bootstrap. */
    if (ch->version < VERSION_PLAYER_012) {
        if (ch->pcdata && list_size(ch->pcdata->classes) == 0) {
            /* Map each of the 8 subclass slots to a CLASS_DATA entry. */
            int sub_slots[8] = {
                ch->pcdata->sub_class_mage,
                ch->pcdata->sub_class_cleric,
                ch->pcdata->sub_class_thief,
                ch->pcdata->sub_class_warrior,
                ch->pcdata->second_sub_class_mage,
                ch->pcdata->second_sub_class_cleric,
                ch->pcdata->second_sub_class_thief,
                ch->pcdata->second_sub_class_warrior,
            };

            for (int i = 0; i < 8; i++) {
                if (sub_slots[i] < 0 || sub_slots[i] >= MAX_SUB_CLASS)
                    continue;

                CLASS_DATA *clazz = class_from_legacy(0, sub_slots[i]);
                if (!clazz)
                    continue;

                /* Current subclass gets ch->level; completed classes get MAX_CLASS_LEVEL. */
                int level = (sub_slots[i] == ch->pcdata->sub_class_current)
                          ? ch->level
                          : MAX_CLASS_LEVEL;

                add_class_level(ch, clazz, level);

                if (sub_slots[i] == ch->pcdata->sub_class_current)
                    ch->pcdata->current_class = get_class_level(ch, clazz);
            }

            /* Level overflow: clamp any class level that exceeds its class max_level. */
            if (list_size(ch->pcdata->classes) > 0) {
                ITERATOR cl_it;
                CLASS_LEVEL *cl;
                int total_overflow = 0;

                iterator_start(&cl_it, ch->pcdata->classes);
                while ((cl = (CLASS_LEVEL *)iterator_nextdata(&cl_it))) {
                    if (cl->clazz && cl->level > cl->clazz->max_level) {
                        int overflow = cl->level - cl->clazz->max_level;
                        total_overflow += overflow;
                        log_stringf("fix_character: %s class %s level %d clamped to max %d (overflow %d)",
                                    ch->name, cl->clazz->name, cl->level,
                                    cl->clazz->max_level, overflow);
                        cl->level = cl->clazz->max_level;
                    }
                }
                iterator_stop(&cl_it);

                if (total_overflow > 0) {
                    ch->pcdata->pending_free_levels += total_overflow;
                    log_stringf("fix_character: %s has %d overflow free levels pending account transfer",
                                ch->name, total_overflow);
                }
            }

            if (list_size(ch->pcdata->classes) > 0) {
                log_stringf("fix_character: migrated %d class levels for %s",
                            list_size(ch->pcdata->classes), ch->name);
            }
        }

        ch->version = VERSION_PLAYER_012;
    }

    /* Future version-gated migrations go here:
     * if (ch->version < VERSION_PLAYER_013) { ... ch->version = VERSION_PLAYER_013; }
     */

    if (ch->pcdata != NULL) {
        if (ch->pronoun_he_she == NULL || ch->pronoun_he_she[0] == '\0') {
            free_string(ch->pronoun_he_she);
            ch->pronoun_he_she = str_dup(body_type_info[ch->body_type].default_he_she);
        }
        if (ch->pronoun_him_her == NULL || ch->pronoun_him_her[0] == '\0') {
            free_string(ch->pronoun_him_her);
            ch->pronoun_him_her = str_dup(body_type_info[ch->body_type].default_him_her);
        }
        if (ch->pronoun_his_her == NULL || ch->pronoun_his_her[0] == '\0') {
            free_string(ch->pronoun_his_her);
            ch->pronoun_his_her = str_dup(body_type_info[ch->body_type].default_his_her);
        }
        if (ch->pronoun_his_hers == NULL || ch->pronoun_his_hers[0] == '\0') {
            free_string(ch->pronoun_his_hers);
            ch->pronoun_his_hers = str_dup(body_type_info[ch->body_type].default_his_hers);
        }
        if (ch->pronoun_himself_herself == NULL || ch->pronoun_himself_herself[0] == '\0') {
            free_string(ch->pronoun_himself_herself);
            ch->pronoun_himself_herself = str_dup(body_type_info[ch->body_type].default_himself_herself);
        }
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

    if (classes < (IS_REMORT(ch)?120:ch->tot_level) / 31 + 1) return true;
    else return false;
}


/* Check for screwed up subclasses. Ie people who have a mage class
   where their warrior class should be. No clue how it originally happened
   but here is the fix based on skill group.*/
void descrew_subclasses(CHAR_DATA *ch)
{

    if (ch == NULL)
    {
    pbugf(LOG_ERROR, "descrew_subclasses: null ch.");
    return;
    }

    if (missing_class(ch) || (ch->pcdata->sub_class_mage != -1
    && (ch->pcdata->sub_class_mage < 3 || ch->pcdata->sub_class_mage > 5 )))
    {
    pbugf(LOG_ERROR, "descrew_subclasses: %s had a non-mage class!",
        ch->name);
    if (char_knows_group(ch, skill_group_find("necromancer skills")))
        ch->pcdata->sub_class_mage = CLASS_MAGE_NECROMANCER;
    else if (char_knows_group(ch, skill_group_find("sorcerer skills")))
        ch->pcdata->sub_class_mage = CLASS_MAGE_SORCERER;
    else if (char_knows_group(ch, skill_group_find("wizard skills")))
        ch->pcdata->sub_class_mage = CLASS_MAGE_WIZARD;

    //sprintf(buf, "{WYou had a screwed up mage class... it has been fixed to {Y%s.{x\n\r",
    //    sub_class_table[ch->pcdata->sub_class_mage].name);
    //send_to_char(buf, ch);
    }

    if (missing_class(ch) || (ch->pcdata->sub_class_cleric != -1
    && (ch->pcdata->sub_class_cleric < 6 || ch->pcdata->sub_class_cleric > 8)))
    {
    pbugf(LOG_ERROR, "descrew_subclasses: %s had a non-cleric class!",
        ch->name);
    if (char_knows_group(ch, skill_group_find("witch skills")))
        ch->pcdata->sub_class_cleric = CLASS_CLERIC_WITCH;
    else if (char_knows_group(ch, skill_group_find("druid skills")))
        ch->pcdata->sub_class_cleric = CLASS_CLERIC_DRUID;
    else if (char_knows_group(ch, skill_group_find("monk skills")))
        ch->pcdata->sub_class_cleric = CLASS_CLERIC_MONK;

//	sprintf(buf, "{WYou had a screwed up cleric class... it has been fixed to {Y%s.{x\n\r",
//	    sub_class_table[ch->pcdata->sub_class_cleric].name);
//	send_to_char(buf, ch);
    }

    if (missing_class(ch) || (ch->pcdata->sub_class_thief != -1
    && (ch->pcdata->sub_class_thief < 9 || ch->pcdata->sub_class_thief > 11)))
    {
    pbugf(LOG_ERROR, "descrew_subclasses: %s had a non-thief class!",
        ch->name);

    if (char_knows_group(ch, skill_group_find("assassin skills")))
        ch->pcdata->sub_class_thief = CLASS_THIEF_ASSASSIN;
    if (char_knows_group(ch, skill_group_find("rogue skills")))
        ch->pcdata->sub_class_thief = CLASS_THIEF_ROGUE;
    if (char_knows_group(ch, skill_group_find("bard skills")))
        ch->pcdata->sub_class_thief = CLASS_THIEF_BARD;

//	sprintf(buf, "{WYou had a screwed up thief class... it has been fixed to {Y%s.{x\n\r",
//	    sub_class_table[ch->pcdata->sub_class_thief].name);
//	send_to_char(buf, ch);
    }

    if (missing_class(ch) || (ch->pcdata->sub_class_thief != -1
    && (ch->pcdata->sub_class_warrior > 2)))
    {
    pbugf(LOG_ERROR, "descrew_subclasses: %s had a non-warrior class!",
        ch->name);
    if (char_knows_group(ch, skill_group_find("marauder skills")))
        ch->pcdata->sub_class_warrior = CLASS_WARRIOR_MARAUDER;
    if (char_knows_group(ch, skill_group_find("gladiator skills")))
        ch->pcdata->sub_class_warrior = CLASS_WARRIOR_GLADIATOR;
    if (char_knows_group(ch, skill_group_find("paladin skills")))
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
    return false;
    } else
    return true;
}


void fix_broken_classes(CHAR_DATA *ch)
{
    char buf[MSL];
    int mage    = ch->pcdata->class_mage;
    int cleric  = ch->pcdata->class_cleric;
    int thief   = ch->pcdata->class_thief;
    int warrior = ch->pcdata->class_warrior;

    sprintf(buf, "fix_broken_classes: fixing broken classes for %s, a level %d %s.",
        ch->name, ch->tot_level, ch->race ? ch->race->name : "unknown");
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
}


// Find out if a player has a skill, ANY skill, which belongs to a general class.
bool find_class_skill(CHAR_DATA *ch, int class)
{
    char *group_name;

    switch (class)
    {
    case CLASS_MAGE: 	group_name = "mage skills"; 	break;
    case CLASS_CLERIC:	group_name = "cleric skills";	break;
    case CLASS_THIEF:	group_name = "thief skills";	break;
    case CLASS_WARRIOR:	group_name = "warrior skills";	break;
    default:
        pbugf(LOG_ERROR, "find_class_skill: bad class.");
        return false;
    }

    SKILL_GROUP *sg = skill_group_find(group_name);
    if (!sg)
        return false;

    ITERATOR it;
    char *skill_name;
    iterator_start(&it, sg->contents);
    while ((skill_name = (char *)iterator_nextdata(&it))) {
        if (get_skill(ch, skill_lookup(skill_name)) > 0) {
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);

    return false;
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
    char *word;
    bool fMatch;
    int vtype;

    vnum = fread_number(fp);
    if ((token_index = get_token_index_global(vnum)) == NULL) {
    pbugf(LOG_ERROR, "fread_token: no token index found for vnum %ld", vnum);
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
            pbugf(LOG_ERROR, "read_token: no match for word %s", word);
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

        if( entry->song != NULL ) {
            fprintf(fp, "Song %s~\n", entry->song->name);
        }
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
    SONG_DATA *song = NULL;
    long flags = SKILL_AUTOMATIC;
    int rating = -1, mod = 0;	// For built-in skills
    char source = SKILLSRC_NORMAL;
    char *word;
    bool fMatch;

    for (; ;)
    {
        word   = feof(fp) ? "End" : fread_word(fp);
        fMatch = false;

        if (!str_cmp(word, "End")) {
            if( song != NULL ) {
                ch->pcdata->songs_learned[song->uid] = true;
                skill_entry_addsong(ch, song, NULL, source);
            } else if(sn > 0) {
                ch->pcdata->learned[sn] = rating;
                ch->pcdata->mod_learned[sn] = mod;
                if( skill_table[sn].spell_fun == spell_null)
                    skill_entry_addskill(ch, sn, NULL, source, flags);
                else
                    skill_entry_addspell(ch, sn, NULL, source, flags);

                // Populate entry rating from loaded data
                SKILL_ENTRY *se = skill_entry_findsn(ch->sorted_skills, sn);
                if (se) {
                    se->rating = rating;
                    se->mod_rating = mod;
                }
            } else if(IS_VALID(token))
                token_to_char_ex(token, ch, source, flags);

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
            FVKEY("Flags",	flags, fread_string_eol(fp), skill_flags);
            break;

        case 'S':
            if(IS_KEY("Sk")) {
                rating = fread_number(fp);
                mod = fread_number(fp);
                sn = skill_lookup(fread_string(fp));
                fMatch = true;
                break;
            }

            if(IS_KEY("Song")) {
                song = song_lookup(fread_string(fp));
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
            pbugf(LOG_ERROR, "fread_skill: no match for word %s", word);
            fread_to_eol(fp);
        }
    }

}

static void quest_part_resolve_wnum(WNUM_LOAD *load, WNUM *wnum)
{

    if (!load || !wnum || wnum->pArea || load->vnum < 1) {
        return;
    }

    AREA_DATA *fallback = NULL;
    WNUM res;
    if (resolve_widevnum(load->vnum, NULL, &res))
        fallback = res.pArea;
    if (!fallback) fallback = get_system_area_fallback();
    resolve_wnum_load(load, wnum, fallback);
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
        pbugf(LOG_ERROR, "fwrite_quest_part: trying to save a quest pickup obj with null in_room");
    else
        fprintf(fp, "OPartW %s %s\n",
            widevnum_string_object(part->pObj->pIndexData, NULL),
            widevnum_string_room(part->pObj->in_room, NULL));
    }
    else if (part->mob_load.vnum != -1) {
        quest_part_resolve_wnum(&part->mob_load, &part->mob_wnum);
        fprintf(fp, "MPartW %s\n", widevnum_string_wnum(part->mob_wnum, NULL));
    } else if (part->obj_sac_load.vnum != -1) {
        quest_part_resolve_wnum(&part->obj_sac_load, &part->obj_sac_wnum);
        fprintf(fp, "OSPartW %s\n", widevnum_string_wnum(part->obj_sac_wnum, NULL));
    } else if (part->mob_rescue_load.vnum != -1) {
        quest_part_resolve_wnum(&part->mob_rescue_load, &part->mob_rescue_wnum);
        fprintf(fp, "MRPartW %s\n", widevnum_string_wnum(part->mob_rescue_wnum, NULL));
    } else if (part->room_load.vnum != -1) {
        quest_part_resolve_wnum(&part->room_load, &part->room_wnum);
        fprintf(fp, "QRoomW %s\n", widevnum_string_wnum(part->room_wnum, NULL));
    }
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
    char *word;
    bool fMatch;
    int i;

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
            if (!str_cmp(word, "MPartW")) {
                char *wnum_str = fread_word(fp);
                if (parse_widevnum_load(wnum_str, &part->mob_load)) {
                    quest_part_resolve_wnum(&part->mob_load, &part->mob_wnum);
                }
                fMatch = true;
                break;
            }

            if (!str_cmp(word, "MPart")) {
                i = fread_number(fp);
                part->mob_load.auid = 0;
                part->mob_load.vnum = i;
                quest_part_resolve_wnum(&part->mob_load, &part->mob_wnum);
                fMatch = true;
                break;
            }

            if (!str_cmp(word, "MRPartW")) {
                char *wnum_str = fread_word(fp);
                if (parse_widevnum_load(wnum_str, &part->mob_rescue_load)) {
                    quest_part_resolve_wnum(&part->mob_rescue_load, &part->mob_rescue_wnum);
                }
                fMatch = true;
                break;
            }

            if (!str_cmp(word, "MRPart")) {
                i = fread_number(fp);
                part->mob_rescue_load.auid = 0;
                part->mob_rescue_load.vnum = i;
                quest_part_resolve_wnum(&part->mob_rescue_load, &part->mob_rescue_wnum);
                fMatch = true;
                break;
            }
            break;

            case 'O':
            /* Special Case - Make an Obj */
            if (!str_cmp(word, "OPartW")) {
                ROOM_INDEX_DATA *room;
                OBJ_DATA *obj;
                OBJ_INDEX_DATA *obj_i;
                char *obj_str = fread_word(fp);
                char *room_str = fread_word(fp);

                if (parse_widevnum_load(obj_str, &part->obj_load)) {
                    quest_part_resolve_wnum(&part->obj_load, &part->obj_wnum);
                }
                if (parse_widevnum_load(room_str, &part->room_load)) {
                    quest_part_resolve_wnum(&part->room_load, &part->room_wnum);
                }

                obj_i = get_obj_index(part->obj_wnum.pArea, part->obj_wnum.vnum);
                room = get_room_index(part->room_wnum.pArea, part->room_wnum.vnum);
                if (obj_i && room) {
                    obj = create_object(obj_i, 1, true);
                    obj_to_room(obj, room);
                    part->pObj = obj;
                }

                fMatch = true;
                break;
            }

            if (!str_cmp(word, "OPart")) {
                ROOM_INDEX_DATA *room;
                OBJ_DATA *obj;
                OBJ_INDEX_DATA *obj_i;
                int room_vnum;

                i = fread_number(fp);
                part->obj_load.auid = 0;
                part->obj_load.vnum = i;
                quest_part_resolve_wnum(&part->obj_load, &part->obj_wnum);

                obj_i = get_obj_index(part->obj_wnum.pArea, part->obj_wnum.vnum);

                room_vnum = fread_number(fp);
                part->room_load.auid = 0;
                part->room_load.vnum = room_vnum;
                quest_part_resolve_wnum(&part->room_load, &part->room_wnum);
                room = get_room_index(part->room_wnum.pArea, part->room_wnum.vnum);

                if (obj_i && room) {
                    obj = create_object(obj_i, 1, true);
                    obj_to_room(obj, room);
                    part->pObj = obj;
                }

                fMatch = true;
                break;
            }

            if (!str_cmp(word, "OSPartW")) {
                char *wnum_str = fread_word(fp);
                if (parse_widevnum_load(wnum_str, &part->obj_sac_load)) {
                    quest_part_resolve_wnum(&part->obj_sac_load, &part->obj_sac_wnum);
                }
                fMatch = true;
                break;
            }

            if (!str_cmp(word, "OSPart")) {
                i = fread_number(fp);
                part->obj_sac_load.auid = 0;
                part->obj_sac_load.vnum = i;
                quest_part_resolve_wnum(&part->obj_sac_load, &part->obj_sac_wnum);
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
            if (!str_cmp(word, "QRoomW")) {
                char *wnum_str = fread_word(fp);
                if (parse_widevnum_load(wnum_str, &part->room_load)) {
                    quest_part_resolve_wnum(&part->room_load, &part->room_wnum);
                }
                fMatch = true;
                break;
            }

            if (!str_cmp(word, "QRoom")) {
                i = fread_number(fp);
                part->room_load.auid = 0;
                part->room_load.vnum = i;
                quest_part_resolve_wnum(&part->room_load, &part->room_wnum);
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
            pbugf(LOG_ERROR, "read_quest_part: no match for word %s", word);
            fread_to_eol(fp);
        }
    }
}

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
    struct timeval start_time, end_time;
    long total_ms;

    if (loaded_accounts) {
        ITERATOR it;
        ACCOUNT_DATA *acct;
        iterator_start(&it, loaded_accounts);
        while ((acct = (ACCOUNT_DATA *)iterator_nextdata(&it))) {
            if (!str_cmp(acct->username, name)) {
                d->account = acct;
                acct->refcount++;  // Increment refcount for cached account
                iterator_stop(&it);
                log_stringf("load_account: Using cached account %s (refcount now %d)",
                           acct->username, acct->refcount);
                return true;
            }
        }
        iterator_stop(&it);
    }

    // Start timing for account load
    gettimeofday(&start_time, NULL);

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
    account->lang = default_localization;

    found = false;
    if (fpReserve != NULL) {
        fclose(fpReserve);
        fpReserve = NULL;  // Mark as closed
    }

    char account_dir_buf[MAX_INPUT_LENGTH];
    const char *account_dir = resolve_game_path(ACCOUNT_DIR, account_dir_buf, sizeof(account_dir_buf));

    /* decompress if .gz file exists */
    snprintf(strsave, sizeof(strsave), "%s%c/%s%s", account_dir, tolower(name[0]), capitalize(name), ".gz");
    if ((fp = fopen(strsave, "r")) != NULL)
    {
        fclose(fp);
        sprintf(buf, "gzip -dfq %s", strsave);
        system(buf);
    }

    snprintf(strsave, sizeof(strsave), "%s%c/%s", account_dir, tolower(name[0]), capitalize(name));
    if ((fp = fopen(strsave, "r")) != NULL) {
        // Check if file is JSON format
        if (json_is_account_json(strsave)) {
            fclose(fp);

            // Load using JSON format
            if (json_read_account(account, strsave)) {
                found = true;
                plogf("load_account: Loaded JSON account %s", name);
            } else {
                pbugf(LOG_ERROR, "load_account: Failed to read JSON account file %s", name);
                found = false;
            }
        } else {
            fclose(fp);
            pbugf(LOG_ERROR, "load_account: Legacy pfile format no longer supported for %s", name);
            found = false;
        }
    }
    
    fpReserve = fopen(NULL_FILE, "r");

    if (!account->creation_date)
        account->creation_date = get_pc_id();
        
    // Generate account IDs if needed
    if (account->id[0] == 0 || account->id[1] == 0)
        get_account_id(account);

    account->character_count = list_size(account->characters);
    account->staff_account = false;
    ITERATOR cit;
    ACCOUNT_CHARACTER *ch_entry;
    iterator_start(&cit, account->characters);
    while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&cit))) {
        if (ch_entry->staff && ch_entry->staff_rank >= STAFF_IMMORTAL) {
            account->staff_account = true;
            break;
        }
    }
    iterator_stop(&cit);

    // Purge soft-deleted characters that have passed the expiration delay.
    // Uses delete_character_by_name() to avoid loading the full character just
    // to get a name. After purging, save the account so removals persist to disk.
    ITERATOR it;
    bool purged_any = false;

    iterator_start(&it, account->characters);
    while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (should_purge_deleted_character(ch_entry)) {
            log_stringf("load_account: Purging expired character '%s' from account '%s'",
                ch_entry->name, account->username);
            delete_character_by_name(ch_entry->name);
            list_remlink(account->characters, ch_entry, false);
            free_account_character(ch_entry);
            purged_any = true;
        }
    }
    iterator_stop(&it);

    if (purged_any) {
        account->character_count = list_size(account->characters);
        save_account(account);
    }

    // Add to loaded_accounts list if found
    if (found && loaded_accounts)
    {
        if (!list_haslink(loaded_accounts, account))
            list_appendlink(loaded_accounts, account);

        if (account->refcount < 1)
            account->refcount = 1;
    }
    account->last_login = current_time;

    // Performance logging
    gettimeofday(&end_time, NULL);
    total_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
              (end_time.tv_usec - start_time.tv_usec) / 1000;
    int char_count = account->characters ? list_size(account->characters) : 0;
    log_stringf("PERFORMANCE load_account: %s with %d characters - total: %ldms",
               name, char_count, total_ms);

    return found;

}



/*
 * Save an account to disk.
 */
void save_account(ACCOUNT_DATA *account)
{
    log_string("save_account: saving account data");
    char strsave[MAX_INPUT_LENGTH];

    if (account == NULL) {
        pbugf(LOG_ERROR, "save_account: null account pointer");
        return;
    }

    if (IS_NULLSTR(account->username)) {
        pbugf(LOG_ERROR, "save_account: account has no username");
        return;
    }

    // Ensure account has IDs before saving
    if (account->id[0] == 0 || account->id[1] == 0)
        get_account_id(account);

    /* Close reserve file */
    if (fpReserve != NULL) {
        fclose(fpReserve);
        fpReserve = NULL;  // Mark as closed
    }

    // Get account path
    json_get_account_path(account->username, strsave, sizeof(strsave));

    // Write using JSON format (handles backup, atomic write, all sections)
    if (!json_write_account(account, strsave)) {
        pbugf(LOG_ERROR, "save_account: json_write_account failed for %s", account->username);
    }

    /* Reopen reserve file */
    fpReserve = fopen(NULL_FILE, "r");
}

void account_add_character(ACCOUNT_DATA *account, CHAR_DATA *ch)
{
    ACCOUNT_CHARACTER *acct_char = NULL;
    ITERATOR it;
    bool need_save_char = false;
    bool need_save_account = false;  // Only save account for structural changes, not metadata updates

    // CRITICAL: Prevent recursive calls to account_add_character
    // This function can be called from save_char_obj, which can be called from
    // account_add_character, creating an infinite loop
    static int add_char_depth = 0;
    if (add_char_depth > 0) {
        log_stringf("account_add_character: SKIPPING recursive call for %s (depth %d)",
                   ch->name ? ch->name : "(unknown)", add_char_depth);
        return;
    }
    add_char_depth++;

    // DIAGNOSTIC: Log entry to account_add_character
    int initial_obj_count = ch->lcarrying ? list_size(ch->lcarrying) : 0;
    log_stringf("account_add_character: ENTRY - %s has %d items in lcarrying",
               ch->name ? ch->name : "(unknown)", initial_obj_count);

    if (!account || !ch || IS_NPC(ch)) {
        pbugf(LOG_ERROR, "account_add_character: invalid parameters");
        add_char_depth--;
        return;
    }

    if (IS_NULLSTR(ch->name)) {
        log_stringf("account_add_character: skipping character with empty name");
        add_char_depth--;
        return;
    }

    /* Check if character already exists in account */
    iterator_start(&it, account->characters);
    while ((acct_char = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (!str_cmp(acct_char->name, ch->name)) {
            // Update basic fields
            acct_char->last_login = current_time;
            acct_char->tot_level = ch->tot_level;
            if (ch->pcdata && ch->pcdata->sub_class_current)
                acct_char->current_level = ch->level;
            else
                acct_char->current_level = ch->tot_level; // fallback

            acct_char->staff = IS_IMMORTAL(ch);
            acct_char->staff_rank = IS_IMMORTAL(ch) ? ch->pcdata->staff_rank : 0;

            if (ch->pcdata && ch->pcdata->class_current) {
                const char *class_name_str = NULL;
                /* Use new CLASS_DATA system for class name */
                CLASS_DATA *acct_class = get_current_class(ch);
                if (acct_class)
                    class_name_str = class_display_ch(acct_class, ch);
                if (!IS_NULLSTR(class_name_str)) {
                    free_string(acct_char->class_name);
                    acct_char->class_name = str_dup(class_name_str);
                }
            }

            acct_char->id[0] = ch->id[0];
            acct_char->id[1] = ch->id[1];

            free_string(acct_char->last_area);
            acct_char->last_area = str_dup(format_location_string(ch->in_room));
            
            // Migrate password data if needed (for character override passwords)
            if (ch->pcdata && !IS_NULLSTR(ch->pcdata->pwd) && ch->pcdata->account_pwd_override) {
                free_string(acct_char->pwd);
                acct_char->pwd = str_dup(ch->pcdata->pwd);
                acct_char->pwd_vers = ch->pcdata->pwd_vers;

                // Also migrate old_pwd if present
                if (!IS_NULLSTR(ch->pcdata->old_pwd)) {
                    free_string(acct_char->old_pwd);
                    acct_char->old_pwd = str_dup(ch->pcdata->old_pwd);
                    free_string(ch->pcdata->old_pwd);
                    ch->pcdata->old_pwd = str_dup("");
                }

                // Clear from character file
                free_string(ch->pcdata->pwd);
                ch->pcdata->pwd = str_dup("");
                ch->pcdata->account_pwd_override = false;
                need_save_char = true;
                need_save_account = true;  // Migration requires account save
                need_save_account = true;  // Password migration requires account save
            }
            // For standard linked characters, ensure pwd is cleared
            else if (ch->pcdata && !IS_NULLSTR(ch->pcdata->pwd) && !ch->pcdata->account_pwd_override) {
                free_string(ch->pcdata->pwd);
                ch->pcdata->pwd = str_dup("");
                if (!IS_NULLSTR(ch->pcdata->old_pwd)) {
                    free_string(ch->pcdata->old_pwd);
                    ch->pcdata->old_pwd = str_dup("");
                }
                ch->pcdata->account_pwd_override = false;
                need_save_char = true;
                need_save_account = true;  // Migration requires account save
                need_save_account = true;  // Password cleanup requires account save
            }
            
            // Migrate MFA settings
            if (ch->pcdata && ch->pcdata->mfa_enabled) {
                acct_char->mfa_enabled = ch->pcdata->mfa_enabled;

                if (!IS_NULLSTR(ch->pcdata->mfa_key)) {
                    free_string(acct_char->mfa_key);
                    acct_char->mfa_key = str_dup(ch->pcdata->mfa_key);
                    free_string(ch->pcdata->mfa_key);
                    ch->pcdata->mfa_key = str_dup("");
                }

                // Migrate pending MFA key if present
                if (!IS_NULLSTR(ch->pcdata->mfa_pending_key)) {
                    free_string(acct_char->mfa_pending_key);
                    acct_char->mfa_pending_key = str_dup(ch->pcdata->mfa_pending_key);
                    free_string(ch->pcdata->mfa_pending_key);
                    ch->pcdata->mfa_pending_key = str_dup("");
                }

                // Handle recovery codes
                for (int i = 0; i < MFA_RECOVERY_CODES; i++) {
                    if (!IS_NULLSTR(ch->pcdata->recovery_codes[i])) {
                        if (acct_char->recovery_codes[i])
                            free_string(acct_char->recovery_codes[i]);
                        acct_char->recovery_codes[i] = str_dup(ch->pcdata->recovery_codes[i]);
                        acct_char->recovery_used[i] = ch->pcdata->recovery_used[i];

                        free_string(ch->pcdata->recovery_codes[i]);
                        ch->pcdata->recovery_codes[i] = str_dup("");
                        ch->pcdata->recovery_used[i] = false;
                    }
                }

                // Clear MFA state on character since it's now at account level
                ch->pcdata->mfa_enabled = false;
                ch->pcdata->mfa_pending = false;
                need_save_char = true;
                need_save_account = true;  // Migration requires account save
            }
            // For standard linked characters, ensure MFA is cleared
            else if (ch->pcdata && (!IS_NULLSTR(ch->pcdata->mfa_key) || ch->pcdata->mfa_enabled)) {
                if (!IS_NULLSTR(ch->pcdata->mfa_key)) {
                    free_string(ch->pcdata->mfa_key);
                    ch->pcdata->mfa_key = str_dup("");
                }
                if (!IS_NULLSTR(ch->pcdata->mfa_pending_key)) {
                    free_string(ch->pcdata->mfa_pending_key);
                    ch->pcdata->mfa_pending_key = str_dup("");
                }
                ch->pcdata->mfa_enabled = false;
                ch->pcdata->mfa_pending = false;
                need_save_char = true;
                need_save_account = true;  // Migration requires account save
            }
            
            // Migrate reset data
            if (ch->pcdata && !IS_NULLSTR(ch->pcdata->reset_code)) {
                free_string(acct_char->reset_code);
                acct_char->reset_code = str_dup(ch->pcdata->reset_code);
                acct_char->reset_time = ch->pcdata->reset_time;
                acct_char->reset_state = ch->pcdata->reset_state;
                
                // Clear from character
                free_string(ch->pcdata->reset_code);
                ch->pcdata->reset_code = str_dup("");
                ch->pcdata->reset_time = 0;
                ch->pcdata->reset_state = 0;
                need_save_char = true;
                need_save_account = true;  // Migration requires account save
            }
            
            // Migrate email data
            if (ch->pcdata && !IS_NULLSTR(ch->pcdata->email)) {
                // Send notification email before clearing the data
                if (ch->pcdata->email_verified) {
                    char subject[256], body[1024];
                    sprintf(subject, "Sentience: Character %s Linked to Account", ch->name);
                    sprintf(body, 
                        "Your character %s has been linked to the account '%s'.\n\n"
                        "This email address previously registered with the character has been "
                        "transferred to your account profile.\n\n"
                        "If you did not authorize this action, please contact game staff immediately.\n\n"
                        "Thank you for playing Sentience!",
                        ch->name, account->username);
                    
                    send_email_async_ex(ch, account, ch->pcdata->email, subject, body, NULL, NULL);
                }
                
                free_string(acct_char->email);
                acct_char->email = str_dup(ch->pcdata->email);
                acct_char->email_verified = ch->pcdata->email_verified;
                
                // Email verification data
                if (!IS_NULLSTR(ch->pcdata->pending_email)) {
                    free_string(acct_char->pending_email);
                    acct_char->pending_email = str_dup(ch->pcdata->pending_email);
                    free_string(ch->pcdata->pending_email);
                    ch->pcdata->pending_email = str_dup("");
                }
                
                if (!IS_NULLSTR(ch->pcdata->email_verification_code)) {
                    free_string(acct_char->email_verification_code);
                    acct_char->email_verification_code = str_dup(ch->pcdata->email_verification_code);
                    free_string(ch->pcdata->email_verification_code);
                    ch->pcdata->email_verification_code = str_dup("");
                }
                
                acct_char->email_verification_time = ch->pcdata->email_verification_time;
                acct_char->email_verification_last_sent = ch->pcdata->email_verification_last_sent;
                
                // Clear email data from character
                free_string(ch->pcdata->email);
                ch->pcdata->email = str_dup("");
                ch->pcdata->email_verified = false;
                ch->pcdata->email_verification_time = 0;
                ch->pcdata->email_verification_last_sent = 0;
                need_save_char = true;
                need_save_account = true;  // Migration requires account save
            }
            
            break;
        }
    }
    iterator_stop(&it);

    // If not found, create new entry
    if (!acct_char) {
        need_save_account = true;  // NEW character requires account save
        acct_char = new_account_character();
        acct_char->name = str_dup(ch->name);
        acct_char->race_name = str_dup(ch->race ? ch->race->name : "unknown");
        acct_char->tot_level = ch->tot_level;

        if (ch->pcdata && ch->pcdata->sub_class_current)
            acct_char->current_level = ch->level;
        else
            acct_char->current_level = ch->tot_level; // fallback

        acct_char->creation_date = ch->pcdata->creation_date;
        acct_char->last_login = current_time;
        acct_char->id[0] = ch->id[0];
        acct_char->id[1] = ch->id[1];

        acct_char->staff = IS_IMMORTAL(ch);
        acct_char->staff_rank = IS_IMMORTAL(ch) ? ch->pcdata->staff_rank : 0;

        if (ch->pcdata && ch->pcdata->sub_class_current) {
            const char *class_name_str = NULL;
            /* Use new CLASS_DATA system for class name */
            CLASS_DATA *acct_class = get_current_class(ch);
            if (acct_class)
                class_name_str = class_display_ch(acct_class, ch);
            if (!IS_NULLSTR(class_name_str)) {
                acct_char->class_name = str_dup(class_name_str);
            }
        }

        if (ch->in_room != NULL)
            acct_char->last_area = str_dup(!IS_NULLSTR(ch->in_room->area->name) ? ch->in_room->area->name : "");

        // Copy authentication data from character to account character (for character override)
        if (ch->pcdata && !IS_NULLSTR(ch->pcdata->pwd) && ch->pcdata->account_pwd_override) {
            acct_char->pwd = str_dup(ch->pcdata->pwd);
            acct_char->pwd_vers = ch->pcdata->pwd_vers;

            // Also copy old_pwd if present
            if (!IS_NULLSTR(ch->pcdata->old_pwd)) {
                acct_char->old_pwd = str_dup(ch->pcdata->old_pwd);
                free_string(ch->pcdata->old_pwd);
                ch->pcdata->old_pwd = str_dup("");
            }

            // Clear from character file
            free_string(ch->pcdata->pwd);
            ch->pcdata->pwd = str_dup("");
            ch->pcdata->account_pwd_override = false;
            need_save_char = true;
                need_save_account = true;  // Migration requires account save
        }
        // For standard linked characters, ensure pwd is cleared
        else if (ch->pcdata && !IS_NULLSTR(ch->pcdata->pwd)) {
            free_string(ch->pcdata->pwd);
            ch->pcdata->pwd = str_dup("");
            if (!IS_NULLSTR(ch->pcdata->old_pwd)) {
                free_string(ch->pcdata->old_pwd);
                ch->pcdata->old_pwd = str_dup("");
            }
            ch->pcdata->account_pwd_override = false;
            need_save_char = true;
                need_save_account = true;  // Migration requires account save
        }

        // Copy MFA settings
        if (ch->pcdata && ch->pcdata->mfa_enabled) {
            acct_char->mfa_enabled = ch->pcdata->mfa_enabled;

            if (!IS_NULLSTR(ch->pcdata->mfa_key)) {
                acct_char->mfa_key = str_dup(ch->pcdata->mfa_key);
                free_string(ch->pcdata->mfa_key);
                ch->pcdata->mfa_key = str_dup("");
            }

            // Copy pending MFA key if present
            if (!IS_NULLSTR(ch->pcdata->mfa_pending_key)) {
                acct_char->mfa_pending_key = str_dup(ch->pcdata->mfa_pending_key);
                free_string(ch->pcdata->mfa_pending_key);
                ch->pcdata->mfa_pending_key = str_dup("");
            }

            // Copy recovery codes
            for (int i = 0; i < MFA_RECOVERY_CODES; i++) {
                if (!IS_NULLSTR(ch->pcdata->recovery_codes[i])) {
                    acct_char->recovery_codes[i] = str_dup(ch->pcdata->recovery_codes[i]);
                    acct_char->recovery_used[i] = ch->pcdata->recovery_used[i];

                    free_string(ch->pcdata->recovery_codes[i]);
                    ch->pcdata->recovery_codes[i] = str_dup("");
                    ch->pcdata->recovery_used[i] = false;
                }
            }

            // Clear MFA state on character
            ch->pcdata->mfa_enabled = false;
            ch->pcdata->mfa_pending = false;
            need_save_char = true;
                need_save_account = true;  // Migration requires account save
        }
        // For standard linked characters, ensure MFA is cleared
        else if (ch->pcdata && (!IS_NULLSTR(ch->pcdata->mfa_key) || ch->pcdata->mfa_enabled)) {
            if (!IS_NULLSTR(ch->pcdata->mfa_key)) {
                free_string(ch->pcdata->mfa_key);
                ch->pcdata->mfa_key = str_dup("");
            }
            if (!IS_NULLSTR(ch->pcdata->mfa_pending_key)) {
                free_string(ch->pcdata->mfa_pending_key);
                ch->pcdata->mfa_pending_key = str_dup("");
            }
            ch->pcdata->mfa_enabled = false;
            ch->pcdata->mfa_pending = false;
            need_save_char = true;
                need_save_account = true;  // Migration requires account save
        }
        
        // Copy reset data
        if (ch->pcdata && !IS_NULLSTR(ch->pcdata->reset_code)) {
            acct_char->reset_code = str_dup(ch->pcdata->reset_code);
            acct_char->reset_time = ch->pcdata->reset_time;
            acct_char->reset_state = ch->pcdata->reset_state;
            
            // Clear from character
            free_string(ch->pcdata->reset_code);
            ch->pcdata->reset_code = str_dup("");
            ch->pcdata->reset_time = 0;
            ch->pcdata->reset_state = 0;
            need_save_char = true;
                need_save_account = true;  // Migration requires account save
        }
        
        // Copy email data
        if (ch->pcdata && !IS_NULLSTR(ch->pcdata->email)) {
            // Send notification email before clearing the data
            if (ch->pcdata->email_verified) {
                char subject[256], body[1024];
                sprintf(subject, "Sentience: Character %s Linked to Account", ch->name);
                sprintf(body, 
                    "Your character %s has been linked to the account '%s'.\n\n"
                    "This email address previously registered with the character has been "
                    "transferred to your account profile.\n\n"
                    "If you did not authorize this action, please contact game staff immediately.\n\n"
                    "Thank you for playing Sentience!",
                    ch->name, account->username);
                
                send_email_async_ex(ch, account, ch->pcdata->email, subject, body, NULL, NULL);
            }
            
            acct_char->email = str_dup(ch->pcdata->email);
            acct_char->email_verified = ch->pcdata->email_verified;
            
            if (!IS_NULLSTR(ch->pcdata->pending_email))
                acct_char->pending_email = str_dup(ch->pcdata->pending_email);
                
            if (!IS_NULLSTR(ch->pcdata->email_verification_code))
                acct_char->email_verification_code = str_dup(ch->pcdata->email_verification_code);
                
            acct_char->email_verification_time = ch->pcdata->email_verification_time;
            acct_char->email_verification_last_sent = ch->pcdata->email_verification_last_sent;
            
            // Clear email data from character
            free_string(ch->pcdata->email);
            ch->pcdata->email = str_dup("");
            ch->pcdata->email_verified = false;
            
            if (!IS_NULLSTR(ch->pcdata->pending_email)) {
                free_string(ch->pcdata->pending_email);
                ch->pcdata->pending_email = str_dup("");
            }
            
            if (!IS_NULLSTR(ch->pcdata->email_verification_code)) {
                free_string(ch->pcdata->email_verification_code);
                ch->pcdata->email_verification_code = str_dup("");
            }
            
            ch->pcdata->email_verification_time = 0;
            ch->pcdata->email_verification_last_sent = 0;
            need_save_char = true;
                need_save_account = true;  // Migration requires account save
        }
        
        list_appendlink(account->characters, acct_char);
    }
    
    // Save the character file ONCE if any changes were made
    if (need_save_char) {
        int before_save = ch->lcarrying ? list_size(ch->lcarrying) : 0;
        log_stringf("account_add_character: BEFORE save_char_obj - %s has %d items",
                   ch->name, before_save);
        save_char_obj(ch);
        int after_save = ch->lcarrying ? list_size(ch->lcarrying) : 0;
        log_stringf("account_add_character: AFTER save_char_obj - %s has %d items",
                   ch->name, after_save);
    }

    // DIAGNOSTIC: Log exit from account_add_character
    int final_obj_count = ch->lcarrying ? list_size(ch->lcarrying) : 0;
    log_stringf("account_add_character: EXIT - %s has %d items in lcarrying",
               ch->name ? ch->name : "(unknown)", final_obj_count);

    // Update account metadata
    account->character_count = list_size(account->characters);
    account->staff_account = false;
    ITERATOR cit;
    ACCOUNT_CHARACTER *ch_entry;
    iterator_start(&cit, account->characters);
    while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&cit))) {
        if (ch_entry->staff && ch_entry->staff_rank >= STAFF_IMMORTAL) {
            account->staff_account = true;
            break;
        }
    }
    iterator_stop(&cit);

    /* Save the updated account ONLY if structural changes occurred (migration, etc.) */
    /* Routine metadata updates (last_login, level, last_area) are updated in memory */
    /* but not persisted until character quit/disconnect to avoid excessive account saves */
    if (need_save_account) {
        log_stringf("account_add_character: STRUCTURAL CHANGE - saving account for %s", acct_char->name);
        save_account(account);
    } else {
        log_stringf("account_add_character: metadata updated for %s (no account save needed)", acct_char->name);
    }

    add_char_depth--;
}

/*
 * Remove a character from an account.
 */
void account_remove_character(ACCOUNT_DATA *account, const char *name)
{
    ITERATOR it;
    ACCOUNT_CHARACTER *acct_char;
    bool found = false;

    if (!account || IS_NULLSTR(name)) {
        pbugf(LOG_ERROR, "account_remove_character: invalid parameters");
        return;
    }

    // Remove from account's character list
    iterator_start(&it, account->characters);
    while ((acct_char = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (!str_cmp(acct_char->name, name)) {
            iterator_remcurrent(&it);
            free_account_character(acct_char);
            found = true;
            break;
        }
    }
    iterator_stop(&it);

    // Unlink account info from the character file and remove any outstanding character auth data.
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
    acct_char->current_level = ch->level;
    acct_char->id[0] = ch->id[0];
    acct_char->id[1] = ch->id[1];
    
    // Update race/class
    if (acct_char->race_name)
        free_string(acct_char->race_name);
    acct_char->race_name = str_dup(ch->race ? ch->race->name : "unknown");

    if (acct_char->class_name)
        free_string(acct_char->class_name);

    if (ch->pcdata && ch->pcdata->sub_class_current) {
        const char *class_name_str = NULL;
        /* Use new CLASS_DATA system for class name */
        CLASS_DATA *acct_class = get_current_class(ch);
        if (acct_class)
            class_name_str = class_display_ch(acct_class, ch);
        if (!IS_NULLSTR(class_name_str)) {
            acct_char->class_name = str_dup(class_name_str);
        } else {
            acct_char->class_name = str_dup("Adventurer");
        }
    } else {
        acct_char->class_name = str_dup("Adventurer");
    }
        
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
    char account_dir_buf[MAX_INPUT_LENGTH];
    const char *account_dir = resolve_game_path(ACCOUNT_DIR, account_dir_buf, sizeof(account_dir_buf));

    // Check loaded_accounts cache first
    if (loaded_accounts) {
        ITERATOR it;
        ACCOUNT_DATA *acct;
        iterator_start(&it, loaded_accounts);
        while ((acct = (ACCOUNT_DATA *)iterator_nextdata(&it))) {
            if (acct->id[0] == id0 && acct->id[1] == id1) {
                iterator_stop(&it);
                return acct;
            }
        }
        iterator_stop(&it);
    }

    // Check each letter directory
    for (c = 'a'; c <= 'z' && !found; c++) {
        snprintf(dir_path, sizeof(dir_path), "%s%c", account_dir, c);
        dir = opendir(dir_path);
        
        if (!dir) 
            continue;

        // Check each file in this directory
        while ((entry = readdir(dir)) != NULL && !found) {
            // Skip . and .. entries
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
                continue;
                
            snprintf(acct_path, sizeof(acct_path), "%s/%s", dir_path, entry->d_name);

            // Try JSON format first
            if (json_is_account_json(acct_path)) {
                DESCRIPTOR_DATA d;
                memset(&d, 0, sizeof(d));
                if (load_account(&d, entry->d_name) && d.account) {
                    if (d.account->id[0] == id0 && d.account->id[1] == id1) {
                        account = d.account;
                        found = true;
                    } else {
                        if (d.account->refcount > 0)
                            d.account->refcount--;

                        if (d.account->refcount <= 0) {
                            if (loaded_accounts)
                                list_remlink(loaded_accounts, d.account, false);
                            free_account(d.account);
                        }
                    }
                }
                continue;
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
        pbugf(LOG_ERROR, "find_account_by_name: null or empty username");
        return NULL;
    }

    // Check loaded_accounts cache first to avoid allocating an orphaned
    // account object.  load_account() also checks this cache, but it
    // overwrites d.account with the cached pointer — previously this
    // function ignored that and returned the pre-allocated (orphaned)
    // new_account instead.  Checking here avoids the allocation entirely.
    if (loaded_accounts) {
        ITERATOR it;
        ACCOUNT_DATA *acct;
        iterator_start(&it, loaded_accounts);
        while ((acct = (ACCOUNT_DATA *)iterator_nextdata(&it))) {
            if (!str_cmp(acct->username, username)) {
                iterator_stop(&it);
                return acct;
            }
        }
        iterator_stop(&it);
    }

    // Initialize temporary descriptor
    memset(&d, 0, sizeof(d));

    char account_dir_buf[MAX_INPUT_LENGTH];
    const char *account_dir = resolve_game_path(ACCOUNT_DIR, account_dir_buf, sizeof(account_dir_buf));

    // Try to load the account directly
    snprintf(strsave, sizeof(strsave), "%s%c/%s", account_dir, tolower(username[0]), capitalize(username));
    
    // First check if file exists 
    if ((fp = fopen(strsave, "r")) == NULL)
        return NULL;
    fclose(fp);
    
    // Now attempt to load it
    account = new_account();
    d.account = account;
    
    if (load_account(&d, username)) {
        // Use d.account — load_account may have replaced it with a
        // cached version.  If it did, free the orphaned allocation.
        if (d.account != account) {
            free_account(account);
        }
        return d.account;
    } else {
        // Something went wrong during loading
        free_account(account);
        return NULL;
    }
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

        // If this object is in lworn, do NOT remove it from the list
        bool is_equipped = (lworn && list_contains(lworn, obj, NULL));

        if (duplicate && !is_equipped) {
            // Remove from list
            if (prev)
                prev->next_content = next;
            else
                *head = next;
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
    LLIST *seen = list_create(false);
    LLIST *obj_seen = list_create(false);
    OBJ_DATA *obj;
    ITERATOR it;

    // DIAGNOSTIC: Log entry to deduplication
    int initial_carrying = ch->lcarrying ? list_size(ch->lcarrying) : 0;
    int initial_locker = ch->llocker ? list_size(ch->llocker) : 0;
    log_stringf("remove_duplicate_objects_from_char: ENTRY - %s has %d carrying, %d locker",
               ch->name ? ch->name : "(unknown)", initial_carrying, initial_locker);

    // 1. First add all worn items to seen list (these take priority)
    // We should NEVER remove worn items during deduplication
    if (ch->lworn && IS_VALID(ch->lworn)) {
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            list_appendlink(seen, obj);
            list_appendlink(obj_seen, obj);
            
            // Recursively add contents of worn items
            if (obj->contains) {
                OBJ_DATA *content;
                content = obj->contains;
                while (content) {
                    list_appendlink(seen, content);
                    content = content->next_content;
                }
            }
        }
        iterator_stop(&it);
    }
    
    // 2. Process carried items (lcarrying)
    if (ch->lcarrying && IS_VALID(ch->lcarrying)) {
        LLIST *remove_list = list_create(false);
        
        iterator_start(&it, ch->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            bool duplicate = false;
            ITERATOR dit;
            OBJ_DATA *exist;
            
            // Check if this object is a duplicate
            iterator_start(&dit, seen);
            while ((exist = (OBJ_DATA *)iterator_nextdata(&dit))) {
                if (exist != obj && exist->id[0] == obj->id[0] && 
                    exist->id[1] == obj->id[1] &&
                    exist->pIndexData == obj->pIndexData) {
                    duplicate = true;
                    break;
                }
            }
            iterator_stop(&dit);
            
            if (duplicate && !list_contains(ch->lworn, obj, NULL)) {
                // Mark for removal after iteration - only if not worn
                list_appendlink(remove_list, obj);
            } else {
                // Add to seen list
                list_appendlink(seen, obj);
                list_appendlink(obj_seen, obj);
                
                // Process contents recursively
                if (obj->contains) {
                    dedupe_obj_list(&obj->contains, seen, ch->lworn);
                }
            }
        }
        iterator_stop(&it);
        
        // Now remove any duplicates from lcarrying
        iterator_start(&it, remove_list);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            list_remlink(ch->lcarrying, obj, false);
        }
        iterator_stop(&it);

        list_destroy(remove_list);
    }
    
    // 3. Process locker items (llocker)
    if (ch->llocker && IS_VALID(ch->llocker)) {
        LLIST *remove_list = list_create(false);
        
        iterator_start(&it, ch->llocker);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            bool duplicate = false;
            ITERATOR dit;
            OBJ_DATA *exist;
            
            // Check if this object is a duplicate
            iterator_start(&dit, seen);
            while ((exist = (OBJ_DATA *)iterator_nextdata(&dit))) {
                if (exist != obj && exist->id[0] == obj->id[0] && 
                    exist->id[1] == obj->id[1] &&
                    exist->pIndexData == obj->pIndexData) {
                    duplicate = true;
                    break;
                }
            }
            iterator_stop(&dit);
            
            if (duplicate && !list_contains(ch->lworn, obj, NULL)) {
                // Mark for removal after iteration - only if not worn
                list_appendlink(remove_list, obj);
            } else {
                // Add to seen list
                list_appendlink(seen, obj);
                list_appendlink(obj_seen, obj);
                
                // Process contents recursively
                if (obj->contains) {
                    dedupe_obj_list(&obj->contains, seen, ch->lworn);
                }
            }
        }
        iterator_stop(&it);
        
        // Now remove any duplicates from llocker
        iterator_start(&it, remove_list);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            list_remlink(ch->llocker, obj, false);
        }
        iterator_stop(&it);

        list_destroy(remove_list);
    }
    
    list_destroy(seen);
    list_destroy(obj_seen);
    // We DO NOT destroy lworn - it's the character's equipment list

    // DIAGNOSTIC: Log exit from deduplication
    int final_carrying = ch->lcarrying ? list_size(ch->lcarrying) : 0;
    int final_locker = ch->llocker ? list_size(ch->llocker) : 0;
    log_stringf("remove_duplicate_objects_from_char: EXIT - %s has %d carrying, %d locker",
               ch->name ? ch->name : "(unknown)", final_carrying, final_locker);
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

