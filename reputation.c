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
*       ROM 2.4 is copyright 1993-1995 Russ Taylor                         *
*       ROM has been brought to you by the ROM consortium                  *
*           Russ Taylor (rtaylor@pacinfo.com)                              *
*           Gabrielle Taylor (gtaylor@pacinfo.com)                         *
*           Brian Moore (rom@rom.efn.org)                                  *
*       By using this code, you have agreed to follow the terms of the     *
*       ROM license, in the file Rom24/doc/rom.license                     *
***************************************************************************/

/***************************************************************************
*  Automated Quest code written by Vassago of MOONGATE, moongate.ams.com   *
*  4000. Copyright (c) 1996 Ryan Addams, All Rights Reserved. Use of this  *
*  code is allowed provided you add a credit line to the effect of:        *
*  "Quest Code (c) 1996 Ryan Addams" to your logon screen with the rest    *
*  of the standard diku/rom credits. If you use this or a modified version *
*  of this code, let me know via email: moongate@moongate.ams.com. Further *
*  updates will be posted to the rom mailing list. If you'd like to get    *
*  the latest version of quest.c, please send a request to the above add-  *
*  ress. Quest Code v2.00.                                                 *
***************************************************************************/

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
#include "recycle.h"
#include "tables.h"
#include "scripts.h"
#include "olc.h"

void show_flag_cmds(CHAR_DATA *ch, const struct flag_type *flag_table);

// Load Reputation Index Rank
REPUTATION_INDEX_RANK_DATA *load_reputation_index_rank(FILE *fp)
{
	REPUTATION_INDEX_RANK_DATA *data;
	bool fMatch;
	char *word;

	data = new_reputation_index_rank_data();
	data->uid = fread_number(fp);

	while(str_cmp((word = fread_word(fp)), "#-RANK"))
	{
		fMatch = false;

		switch(word[0])
		{
		case 'C':
			KEY("Capacity", data->capacity, fread_number(fp));
			KEY("Color", data->color, fread_letter(fp));
			KEYS("Comments", data->comments, fread_string(fp));
			break;

		case 'D':
			KEYS("Description", data->description, fread_string(fp));
			break;

		case 'F':
			KEY("Flags", data->flags, fread_flag(fp));
			break;

		case 'N':
			KEYS("Name", data->name, fread_string(fp));
			break;
		
		}


		if (!fMatch)
		{
			log_stringf("load_reputation_index_rank: unknown word '%s' found.", word);
			fread_to_eol(fp);
		}
	}

	return data;
}

// Load Reputation Index
REPUTATION_INDEX_DATA *load_reputation_index(FILE *fp, AREA_DATA *area)
{
	REPUTATION_INDEX_DATA *data;
	bool fMatch;
	char *word;

	data = new_reputation_index_data();
	data->vnum = fread_number(fp);
	data->area = area;

	while(str_cmp((word = fread_word(fp)), "#-REPUTATION"))
	{
		fMatch = false;

		switch(word[0])
		{
		case '#':
			if (!str_cmp(word, "#RANK"))
			{
				REPUTATION_INDEX_RANK_DATA *rank = load_reputation_index_rank(fp);

				if (rank->uid > data->top_rank_uid)
					data->top_rank_uid = rank->uid;

				list_appendlink(data->ranks, rank);
				rank->ordinal = list_size(data->ranks);
				fMatch = true;
				break;
			}
			break;

		case 'C':
			KEYS("Comments", data->comments, fread_string(fp));
			KEYS("CreatedBy", data->created_by, fread_string(fp));
			break;

		case 'D':
			KEYS("Description", data->description, fread_string(fp));
			break;

		case 'F':
			KEY("Flags", data->flags, fread_flag(fp));
			break;

		case 'I':
			KEY("InitialRank", data->initial_rank, fread_number(fp));
			KEY("InitialReputation", data->initial_reputation, fread_number(fp));
			break;

		case 'N':
			KEYS("Name", data->name, fread_string(fp));
			break;

		case 'T':
			KEY("Token", data->token_load, fread_widevnum(fp, area->uid));
			break;
		}


		if (!fMatch)
		{
			log_stringf("load_reputation_index: unknown word '%s' found.", word);
			fread_to_eol(fp);
		}
	}

	return data;
}

// Save Reputation Rank
void save_reputation_index_rank(FILE *fp, REPUTATION_INDEX_RANK_DATA *rank)
{
	fprintf(fp, "#RANK %d\n", rank->uid);

	fprintf(fp, "Name %s~\n", fix_string(rank->name));
	fprintf(fp, "Description %s~\n", fix_string(rank->description));
	fprintf(fp, "Comments %s~\n", fix_string(rank->comments));
	fprintf(fp, "Flags %s\n", print_flags(rank->flags));
	fprintf(fp, "Capacity %ld\n", rank->capacity);
	fprintf(fp, "Color %c\n", rank->color);

	fprintf(fp, "#-RANK\n");
}

// Save Reputation
void save_reputation_index(FILE *fp, REPUTATION_INDEX_DATA *data)
{
	fprintf(fp, "#REPUTATION %ld\n", data->vnum);

	fprintf(fp, "Name %s~\n", fix_string(data->name));
	fprintf(fp, "Description %s~\n", fix_string(data->description));
	fprintf(fp, "Comments %s~\n", fix_string(data->comments));
	fprintf(fp, "CreatedBy %s~\n", fix_string(data->created_by));

	fprintf(fp, "Flags %s\n", print_flags(data->flags));

	if (data->initial_rank > 0)
		fprintf(fp, "InitialRank %d\n", data->initial_rank);
	fprintf(fp, "InitialReputation %ld\n", data->initial_reputation);

	if (data->token)
		fprintf(fp, "Token %ld#%ld\n", data->token->area->uid, data->token->vnum);

	ITERATOR it;
	REPUTATION_INDEX_RANK_DATA *rank;
	iterator_start(&it, data->ranks);
	while((rank = (REPUTATION_INDEX_RANK_DATA *)iterator_nextdata(&it)))
	{
		save_reputation_index_rank(fp, rank);
	}
	iterator_stop(&it);

	fprintf(fp, "#-REPUTATION\n");
}

// Save Reputations for Area
void save_reputation_indexes(FILE *fp, AREA_DATA *pArea)
{
	for(int i = 0; i < MAX_KEY_HASH; i++)
	{
		for(REPUTATION_INDEX_DATA *reputation = pArea->reputation_index_hash[i]; reputation; reputation = reputation->next)
		{
			save_reputation_index(fp, reputation);
		}
	}
}

// Get Reputation Index
REPUTATION_INDEX_DATA *get_reputation_index(AREA_DATA *area, long vnum)
{
	if (!area || vnum < 1) return NULL;

	int i = vnum % MAX_KEY_HASH;

	for(REPUTATION_INDEX_DATA *rep = area->reputation_index_hash[i]; rep; rep = rep->next)
	{
		if (rep->vnum == vnum)
		{
			return rep;
		}
	}

	return NULL;
}

REPUTATION_INDEX_DATA *get_reputation_index_auid(long auid, long vnum)
{
	return get_reputation_index(get_area_from_uid(auid), vnum);
}

REPUTATION_INDEX_DATA *get_reputation_index_wnum(WNUM wnum)
{
	return get_reputation_index(wnum.pArea, wnum.vnum);
}

// Get Reputation Rank on Reputation
REPUTATION_INDEX_RANK_DATA *get_reputation_rank(REPUTATION_INDEX_DATA *rep, int ordinal)
{
	if (!IS_VALID(rep) || ordinal < 1 || ordinal > list_size(rep->ranks)) return NULL;

	return (REPUTATION_INDEX_RANK_DATA *)list_nthdata(rep->ranks, ordinal);
}

REPUTATION_INDEX_RANK_DATA *get_reputation_rank_uid(REPUTATION_INDEX_DATA *rep, sh_int uid)
{
	if (!IS_VALID(rep) || uid < 1 || uid > rep->top_rank_uid) return NULL;

	ITERATOR it;
	REPUTATION_INDEX_RANK_DATA *rank;
	iterator_start(&it, rep->ranks);
	while((rank = (REPUTATION_INDEX_RANK_DATA *)iterator_nextdata(&it)))
	{
		if (rank->uid == uid)
			break;
	}
	iterator_stop(&it);

	return rank;
}

REPUTATION_DATA *find_reputation_char(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex)
{
	if (!IS_VALID(ch) || !IS_VALID(repIndex)) return NULL;

	ITERATOR it;
	REPUTATION_DATA *rep;
	iterator_start(&it, ch->reputations);
	while((rep = (REPUTATION_DATA *)iterator_nextdata(&it)))
	{
		if (rep->pIndexData == repIndex)
			break;
	}
	iterator_stop(&it);

	return rep;
}


// Get Reputation on Character
REPUTATION_DATA *get_reputation_char(CHAR_DATA *ch, AREA_DATA *area, long vnum, bool add, bool show)
{
	if (!IS_VALID(ch) || !area || vnum < 1) return NULL;

	ITERATOR it;
	REPUTATION_DATA *rep;
	iterator_start(&it, ch->reputations);
	while((rep = (REPUTATION_DATA *)iterator_nextdata(&it)))
	{
		if (rep->pIndexData->area == area && rep->pIndexData->vnum == vnum)
			break;
	}
	iterator_stop(&it);

	// If they don't have it, automatically give it to them
	if (!rep && add)
	{
		rep = set_reputation_char(ch, get_reputation_index(area, vnum), -1, -1, show);
	}

	return rep;
}

REPUTATION_DATA *get_reputation_char_auid(CHAR_DATA *ch, long auid, long vnum, bool add, bool show)
{
	if (!IS_VALID(ch) || auid < 1 || vnum < 1) return NULL;

	ITERATOR it;
	REPUTATION_DATA *rep;
	iterator_start(&it, ch->reputations);
	while((rep = (REPUTATION_DATA *)iterator_nextdata(&it)))
	{
		if (rep->pIndexData->area->uid == auid && rep->pIndexData->vnum == vnum)
			break;
	}
	iterator_stop(&it);

	// If they don't have it, automatically give it to them
	if (!rep && add)
	{
		rep = set_reputation_char(ch, get_reputation_index(get_area_from_uid(auid), vnum), -1, -1, show);
	}

	return rep;
}

REPUTATION_DATA *get_reputation_char_wnum(CHAR_DATA *ch, WNUM wnum, bool add, bool show)
{
	if (!IS_VALID(ch) || !wnum.pArea || wnum.vnum < 1) return NULL;

	ITERATOR it;
	REPUTATION_DATA *rep;
	iterator_start(&it, ch->reputations);
	while((rep = (REPUTATION_DATA *)iterator_nextdata(&it)))
	{
		if (rep->pIndexData->area == wnum.pArea && rep->pIndexData->vnum == wnum.vnum)
			break;
	}
	iterator_stop(&it);

	// If they don't have it, automatically give it to them
	if (!rep && add)
	{
		rep = set_reputation_char(ch, get_reputation_index(wnum.pArea, wnum.vnum), -1, -1, show);
	}

	return rep;
}


bool has_reputation(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex)
{
	ITERATOR it;
	REPUTATION_DATA *rep;

	iterator_start(&it, ch->reputations);
	while((rep = (REPUTATION_DATA *)iterator_nextdata(&it)))
	{
		if (rep->pIndexData == repIndex)
			break;
	}
	iterator_stop(&it);

	return rep != NULL;
}

// Gain Reputation
bool gain_reputation(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex, long amount, int *change, long *total_given, bool show)
{
	long total;
	if (change) *change = 0;
	if (total_given) *total_given = 0;
	if (!IS_VALID(ch) || !IS_VALID(repIndex)) return false;

	REPUTATION_DATA *rep = get_reputation_char(ch, repIndex->area, repIndex->vnum, true, show);

	if (!IS_VALID(rep)) return false;

	REPUTATION_INDEX_RANK_DATA *this_rank = (REPUTATION_INDEX_RANK_DATA *)list_nthdata(repIndex->ranks, rep->current_rank);

	REPUTATION_DATA *old_tempreputation = ch->tempreputation;
	int old_tempstore0 = ch->tempstore[0];

	// Allow for bonuses, either specific to the reputation or all reputations

	ch->tempstore[0] = amount;
	ch->tempreputation = rep;

	// Also.. can block gaining any reputation by returning a positive number (end denied)
	int ret = p_percent_token_trigger( ch, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, rep->token, TRIG_REPUTATION_PREGAIN, NULL,show,0,0,0,0);
	amount = ch->tempstore[0];	

	ch->tempreputation = old_tempreputation;
	ch->tempstore[0] = old_tempstore0;

	if (ret > 0)
		return false;

	if (!amount) return false;
	
	// Gaining rep... but we are either on the final
	if (amount > 0 &&

		/* Final Rank and *not* flagged PARAGON */
		((this_rank->ordinal == list_size(repIndex->ranks) && !IS_SET(this_rank->flags, REPUTATION_RANK_PARAGON)) ||

		/* Rank doesn't allow ranking up automatically */
		IS_SET(this_rank->flags, REPUTATION_RANK_NORANKUP)))
	{
		// Already at the limit?
		if (rep->reputation >= (this_rank->capacity - 1))
			return false;
	}

	total = amount;
	rep->reputation += amount;
	int last_rank = rep->current_rank;

	if (rep->reputation < 0)
	{
		// Lost reputation
		if (rep->current_rank > 1)
		{
			// Down grade to previous rank depending upon how much was lost
			while(rep->reputation < 0 && rep->current_rank > 1)
			{
				rep->current_rank--;
				REPUTATION_INDEX_RANK_DATA *prev = get_reputation_rank(rep->pIndexData, rep->current_rank);
				rep->reputation += prev->capacity;
			}
		}

		// Only possible if at the lowest rank at this point
		if (rep->reputation < 0)
		{
			total -= rep->reputation;
			rep->reputation = 0;
		}
	}
	else
	{
		// Still have ranks to go
		if (rep->current_rank < list_size(rep->pIndexData->ranks))
		{
			REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(rep->pIndexData, rep->current_rank);
			while(rep->current_rank < list_size(rep->pIndexData->ranks) && rep->reputation >= rank->capacity && !IS_SET(rank->flags, REPUTATION_RANK_NORANKUP))
			{
				rep->reputation -= rank->capacity;
				rep->current_rank++;

				// Get next rank
				rank = get_reputation_rank(rep->pIndexData, rep->current_rank);
			}
		}

		// Only possible if at the highest rank or if the current rank is NO_RANK_UP
		REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(rep->pIndexData, rep->current_rank);

		if (rep->reputation >= rank->capacity)
		{
			total += rep->reputation - rank->capacity + 1;	// it's really - (capacity - 1) distributed.

			// Is it the final rank with PARAGON?
			if (rep->current_rank == list_size(rep->pIndexData->ranks) && IS_SET(rank->flags, REPUTATION_RANK_PARAGON))
			{
				rep->reputation = 0;
				paragon_reputation(ch, rep, true);
			}
			else
			{
				rep->reputation = rank->capacity - 1;
			}
		}
	}

	if (total_given) *total_given = total;

	if (show)
	{
		if (total > 0)
			send_to_char(formatf("{BYou received %ld points in %s.{x\n\r", total, repIndex->name), ch);
		else if (total < 0)
			send_to_char(formatf("{RYou lost %ld points from %s.{x\n\r", -total, repIndex->name), ch);

		if (last_rank < rep->current_rank)
		{
			// Gained rank
			REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(rep->pIndexData, rep->current_rank);
			send_to_char(formatf("{WYou have obtained the rank of '%s' for reputation '%s'!{x\n\r", rank->name, rep->pIndexData->name), ch);
		}
		else if (last_rank > rep->current_rank)
		{
			// Lost rank
			REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(rep->pIndexData, rep->current_rank);
			send_to_char(formatf("{DYou have reduced your rank in '%s' to '%s'.{x\n\r", rep->pIndexData->name, rank->name), ch);
		}
	}

	// Lost rank
	if (rep->current_rank < last_rank)
	{
		// If last rank was the highest rank AND had RESET_PARAGON
		if (last_rank == list_size(rep->pIndexData->ranks) && IS_SET(this_rank->flags, REPUTATION_RANK_RESET_PARAGON))
			// Lose all paragon levels
			rep->paragon_level = 0;
	}

	ch->tempreputation = rep;
	p_percent_token_trigger( ch, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, rep->token, TRIG_REPUTATION_GAIN, NULL,show,amount,0,0,0);
	ch->tempreputation = old_tempreputation;

	// Update maximum rank
	if (rep->current_rank > rep->maximum_rank)
	{
		if (IS_VALID(rep->token))
		{
			// Handle each RANKUP trigger, only if the reputation has a token associated with it
			for(int i = rep->current_rank + 1; i <= rep->maximum_rank; i++)
			{
				rep->current_rank = i;
				p_percent_trigger( NULL, NULL, NULL, rep->token, ch, NULL, NULL,NULL, NULL, TRIG_REPUTATION_RANKUP, NULL,show,0,0,0,0);
			}
		}

		rep->maximum_rank = rep->current_rank;
	}

	if (change) *change = rep->current_rank - last_rank;
	return true;
}

void paragon_reputation(CHAR_DATA *ch, REPUTATION_DATA *rep, bool show)
{
	if (!IS_VALID(ch) || !IS_VALID(rep)) return;

	if (rep->current_rank < list_size(rep->pIndexData->ranks)) return;

	// Next paragon level
	++rep->paragon_level;

	// If the reputation has a token, check for PARAGON trigger
	if (IS_VALID(rep->token))
		p_percent_trigger( NULL, NULL, NULL, rep->token, ch, NULL, NULL,NULL, NULL, TRIG_REPUTATION_PARAGON, NULL,show,0,0,0,0);
}

void group_gain_reputation(CHAR_DATA *ch, CHAR_DATA *victim)
{
	CHAR_DATA *gch;

	// Ignore player victim
	if (!IS_NPC(victim)) return;

	// Check here just to make sure
	if (ch->in_room == NULL) {
		bug("group_gain_reputation: ch with null in_room", 0);
		return;
	}

	for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
	{
		/* If this char is in the same form */
		if (is_same_group(gch, ch) && !IS_NPC(ch))
		{
			// Iterate over all of the mob reputations on the victim
			MOB_REPUTATION_DATA *mob_rep;
			for(mob_rep = victim->mob_reputations; mob_rep; mob_rep = mob_rep->next)
			{
				if (!IS_VALID(mob_rep->reputation)) continue;

				REPUTATION_DATA *rep = find_reputation_char(gch, mob_rep->reputation);
				int rankNo = IS_VALID(rep) ? rep->current_rank : mob_rep->reputation->initial_rank;

				// Check the rank is valid
				if (rankNo < 1 || rankNo > list_size(mob_rep->reputation->ranks))
					continue;

				// Check if GCH's rank is in the range of this entry
				if (mob_rep->minimum_rank > 0 && rankNo < mob_rep->minimum_rank) continue;
				if (mob_rep->maximum_rank > 0 && rankNo > mob_rep->maximum_rank) continue;

				// Give reputation to group member
				gain_reputation(gch, mob_rep->reputation, mob_rep->points, NULL, NULL, true);
			}

		}
	}
}


// Set Rank on Reputation
bool set_reputation_rank(CHAR_DATA *ch, REPUTATION_DATA *rep, int rank_no, int rank_rep, bool show)
{
	if (!IS_VALID(ch) || !IS_VALID(rep) || rank_no < 1 || rank_no > list_size(rep->pIndexData->ranks)) return false;

	REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(rep->pIndexData, rep->current_rank);

	if (rep->current_rank == rank_no)
	{
		// Set the reputation

		if (rank_rep < 0)	// Same rank, but automatic rep, leave reputation alone
			return false;

		rep->reputation = rank_rep;
		if (rep->reputation > rank->capacity)
			rep->reputation = rank->capacity - 1;

		// Nothing to show here
		return true;
	}

	int last_rank = rep->current_rank;

	rep->current_rank = rank_no;
	REPUTATION_INDEX_RANK_DATA *new_rank = get_reputation_rank(rep->pIndexData, rank_no);

	if (rank_rep < 0)
		rank_rep = new_rank->set;	// Set the reputation value according to the rank (usually 0)
	rep->reputation = rank_rep;
	if (rep->reputation > new_rank->capacity)
		rep->reputation = new_rank->capacity - 1;

	if (show)
	{
		if (last_rank < rep->current_rank)
		{
			// Gained rank
			REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(rep->pIndexData, rep->current_rank);
			send_to_char(formatf("{WYou have obtained the rank of '%s' for reputation '%s'!{x\n\r", rank->name, rep->pIndexData->name), ch);
		}
		else if (last_rank > rep->current_rank)
		{
			// Lost rank
			REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(rep->pIndexData, rep->current_rank);
			send_to_char(formatf("{DYou have reduced your rank in '%s' to '%s'.{x\n\r", rep->pIndexData->name, rank->name), ch);
		}
	}

	// Update maximum rank
	if (rep->current_rank > rep->maximum_rank)
	{
		if (IS_VALID(rep->token))
		{
			// Handle each RANKUP trigger, only if the reputation has a token associated with it
			for(int i = rep->current_rank + 1; i <= rep->maximum_rank; i++)
			{
				rep->current_rank = i;
				p_percent_trigger( NULL, NULL, NULL, rep->token, ch, NULL, NULL,NULL, NULL, TRIG_REPUTATION_RANKUP, NULL,show,0,0,0,0);
			}
		}

		rep->maximum_rank = rep->current_rank;
	}

	return true;
}

// Set Reputation on Character
REPUTATION_DATA *set_reputation_char(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex, int startingRank, int startingRep, bool show)
{
	if (!IS_VALID(ch) || !IS_VALID(repIndex)) return NULL;

	// Don't do the other way, as it will create an infinite recursion / stack overflow
	ITERATOR it;
	REPUTATION_DATA *rep;
	iterator_start(&it, ch->reputations);
	while((rep = (REPUTATION_DATA *)iterator_nextdata(&it)))
	{
		if (rep->pIndexData == repIndex)
			break;
	}
	iterator_stop(&it);

	// Already have it
	if (IS_VALID(rep)) return NULL;

	// Default starting rank or selected rank is out of range
	if (startingRank < 1 || startingRank > list_size(repIndex->ranks))
		startingRank = repIndex->initial_rank;

	REPUTATION_INDEX_RANK_DATA *rank = (REPUTATION_INDEX_RANK_DATA *)list_nthdata(repIndex->ranks, startingRank);
	if (!IS_VALID(rank)) return NULL;	// Invalid rank selected

	// Default starting reputation
	if (startingRep < 0)
		startingRep = repIndex->initial_reputation;

	// Keep starting reputation within the range of the selected rank
	if (startingRep > rank->capacity)
		startingRep = rank->capacity - 1;

	rep = new_reputation_data();
	rep->pIndexData = repIndex;
	rep->flags = repIndex->flags;
	rep->current_rank = startingRank;
	rep->reputation = startingRep;
	rep->maximum_rank = rep->current_rank;
	rep->paragon_level = 0;

	list_appendlink(ch->reputations, rep);

	if(show)
	{
		REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(rep->pIndexData, rep->current_rank);
		send_to_char(formatf("{YYou have obtained a rank of '%s' in reputation '%s'.{x\n\r", rank->name, rep->pIndexData->name), ch);
	}

	return rep;
}

bool is_reputation_rank_peaceful(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex)
{
	if (!IS_VALID(ch) || !IS_VALID(repIndex)) return false;

	REPUTATION_DATA *rep = find_reputation_char(ch, repIndex);
	if (!IS_VALID(rep)) return false;

	REPUTATION_INDEX_RANK_DATA *rank = (REPUTATION_INDEX_RANK_DATA *)list_nthdata(repIndex->ranks, rep->current_rank);

	if (!IS_VALID(rank)) return false;

	if(!IS_SET(rank->flags, REPUTATION_RANK_PEACEFUL)) return false;

	// Current rank is set to PEACEFUL.  Requires AT WAR to be attackable
	if(IS_SET(rep->flags, REPUTATION_AT_WAR)) return false;

	return true;
}


// Insert Reputation
void insert_reputation(LLIST *lp, REPUTATION_DATA *rep)
{
	if (!IS_VALID(lp) || !IS_VALID(rep)) return;

	ITERATOR it;
	REPUTATION_DATA *r;
	iterator_start(&it, lp);
	while((r = (REPUTATION_DATA *)iterator_nextdata(&it)))
	{
		int cmp = str_cmp(rep->pIndexData->name, r->pIndexData->name);
		if(cmp < 0)
		{
			iterator_insert_before(&it, rep);
			break;
		}
	}
	iterator_stop(&it);

	// After everything else
	if (!r)
	{
		list_appendlink(lp, rep);
	}
}

LLIST *sort_reputations(CHAR_DATA *ch)
{
	LLIST *list = list_create(FALSE);

	ITERATOR it;
	REPUTATION_DATA *rep;
	iterator_start(&it, ch->reputations);
	while((rep = (REPUTATION_DATA *)iterator_nextdata(&it)))
	{
		if ((IS_IMMORTAL(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT)) || !IS_SET(rep->flags, REPUTATION_HIDDEN))
			insert_reputation(list, rep);
	}
	iterator_stop(&it);

	return list;
}

// List Reputations on Character
void do_reputations(CHAR_DATA *ch, char *argument)
{
	char buf[MSL];

	if (list_size(ch->reputations) > 0)
	{
		// TODO: Parse filters

		LLIST *reps = sort_reputations(ch);

		BUFFER *buffer = new_buf();

		add_buf(buffer, " ##  [        Name        ] [        Rank        ] [                    Reputation                   ]\n\r");
		add_buf(buffer, "=======================================================================================================\n\r");

		int i = 0;
		ITERATOR it;
		REPUTATION_DATA *rep;
		iterator_start(&it, reps);
		while((rep = (REPUTATION_DATA *)iterator_nextdata(&it)))
		{
			REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(rep->pIndexData, rep->current_rank);

			// TODO: Check filters

			int percent = 100 * rep->reputation / rank->capacity;
			int slots = 20 * rep->reputation / rank->capacity;

			char repColor;
			if (IS_SET(rep->flags, REPUTATION_AT_WAR))
				repColor = 'R';
			else if (IS_SET(rep->flags, REPUTATION_PEACEFUL) || IS_SET(rank->flags, REPUTATION_RANK_PEACEFUL))
				repColor = 'G';
			else
				repColor = 'Y';
			if (IS_SET(rep->flags, REPUTATION_HIDDEN))
				repColor = LOWER(repColor);


			sprintf(buf, "%3d)  {%c%-20.20s   {%c%-20.20s   %12s{x {W%-5ld [{%c%-20s{x] {W%5ld %s{x\n\r", ++i,
				repColor,
				rep->pIndexData->name,
				rank->color ? rank->color : 'Y',
				rank->name,
				formatf("{W({x%d%%{W)", percent),
				rep->reputation,
				rank->color ? rank->color : 'Y',
				(slots > 0) ? formatf("%*.*s", slots, slots, "####################") : "",
				rank->capacity,
				(rep->paragon_level > 0) ? formatf("{W({x%d{Y*{W)", rep->paragon_level) : "");

			add_buf(buffer, buf);
		}
		iterator_stop(&it);
		add_buf(buffer, "-------------------------------------------------------------------------------------------------------\n\r");

		list_destroy(reps);

		if (i > 0)
		{
			if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
			{
				send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
			}
			else
			{
				page_to_char(buffer->string, ch);
			}
		}
		else
			send_to_char("No matching reputations found.\n\r", ch);

		free_buf(buffer);
	}
	else
		send_to_char("You haven't acquired any reputations.\n\r", ch);
}

void do_atwar(CHAR_DATA *ch, char *argument)
{
	if (IS_NPC(ch))
	{
		send_to_char("Huh?\n\r", ch);
		return;
	}

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  atwar <reputation name>\n\r", ch);
		return;
	}

	int count;
	char arg[MIL];
	count = number_argument(argument, arg);

	ITERATOR it;
	REPUTATION_DATA *rep;
	iterator_start(&it, ch->reputations);
	while((rep = (REPUTATION_DATA *)iterator_nextdata(&it)))
	{
		if ((IS_IMMORTAL(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT)) || !IS_SET(rep->flags, REPUTATION_HIDDEN))
		{
			if (is_name(arg, rep->pIndexData->name) && !--count)
				break;
		}
	}
	iterator_stop(&it);

	if (!IS_VALID(rep))
	{
		send_to_char("No such reputation by that name.\n\r", ch);
		return;
	}

	if (IS_SET(rep->flags, REPUTATION_AT_WAR))
	{
		REMOVE_BIT(rep->flags, REPUTATION_AT_WAR);
		send_to_char(formatf("{YYou are no longer AT WAR with %s.{x\n\r", rep->pIndexData->name), ch);
	}
	else if(IS_SET(rep->flags, REPUTATION_PEACEFUL))
	{
		send_to_char(formatf("{CYou feel compelled to remain peaceful with %s.{x\n\r", rep->pIndexData->name), ch);
	}
	else
	{
		SET_BIT(rep->flags, REPUTATION_AT_WAR);
		send_to_char(formatf("{RYou are now AT WAR with %s.{x\n\r", rep->pIndexData->name), ch);
	}
}

/////////////////////////////////////////////////////
// Reputation Editor
REPEDIT( repedit_show )
{
	char buf[MSL];
	REPUTATION_INDEX_DATA *pRep;
	REPUTATION_INDEX_RANK_DATA *rank;

	EDIT_REPUTATION(ch, pRep);

	BUFFER *buffer = new_buf();

	sprintf(buf, "Reputation: {W%ld{x#{W%ld{x\n\r", pRep->area->uid, pRep->vnum);
	add_buf(buffer, buf);

	sprintf(buf, "[Name              ]:  %s\n\r", pRep->name);
	add_buf(buffer, buf);

	sprintf(buf, "[Area              ]:  %s (%ld)\n\r", pRep->area->name, pRep->area->uid);
	add_buf(buffer, buf);

	sprintf(buf, "[Created by        }:  %s\n", pRep->created_by);

	sprintf(buf, "[Description       ]:\n  %s\n\r", pRep->description);
	add_buf(buffer, buf);

	sprintf(buf, "[Flags             ]:  %s\n", flag_string(reputation_flags, pRep->flags));
	add_buf(buffer, buf);

	if (pRep->initial_rank > 0)
		rank = (REPUTATION_INDEX_RANK_DATA *)list_nthdata(pRep->ranks, pRep->initial_rank);
	else
		rank = NULL;
	sprintf(buf, "[Initial Rank      ]:  %s (%d)\n\r", (rank ? rank->name : "-invalid-"), pRep->initial_rank);
	add_buf(buffer, buf);

	sprintf(buf, "[Initial Reputation]:  %ld\n\r", pRep->initial_reputation);
	add_buf(buffer, buf);

	if (pRep->token)
	{
		sprintf(buf, "[Token             ]:  %s (%ld#%ld)\n\r", pRep->token->name, pRep->token->area->uid, pRep->token->vnum);
		add_buf(buffer, buf);
	}

	sprintf(buf, "\n\r-----\n\r{WBuilders' Comments:{X\n\r%s\n\r-----\n\r", pRep->comments);
	add_buf(buffer, buf);

	add_buf(buffer, "\n\rRanks:\n\r");
	if (list_size(pRep->ranks) > 0)
	{
		add_buf(buffer, " ##  [        Name        ] [Color] [Capacity] [ Flags ]\n\r");
		add_buf(buffer, "=========================================================\n\r");
		int i = 0;
		ITERATOR it;
		iterator_start(&it, pRep->ranks);
		while((rank = (REPUTATION_INDEX_RANK_DATA *)iterator_nextdata(&it)))
		{
			sprintf(buf, "%3d)   %-20.20s     {%c%c{x      %6ld    {x%s\n\r", ++i,
				rank->name,
				rank->color ? rank->color : 'Y',
				rank->color ? rank->color : 'Y',
				rank->capacity,
				flag_string(reputation_rank_flags, rank->flags));
			add_buf(buffer, buf);
		}
		iterator_stop(&it);
		add_buf(buffer, "---------------------------------------------------------\n\r");
	}
	else
		add_buf(buffer, "   None.\n\r");

	if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
	{
		send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
	}
	else
	{
		page_to_char(buffer->string, ch);
	}

	free_buf(buffer);
	return false;
}

REPEDIT( repedit_create )
{
	REPUTATION_INDEX_DATA *pRep;
	WNUM wnum;

	if (!parse_widevnum(argument, ch->in_room->area, &wnum))
	{
		send_to_char("Syntax:  create {R<widevnum>{x\n\r", ch);
		send_to_char("Please specify a widevnum.\n\r", ch);
		return false;
	}

    if (!IS_BUILDER(ch, wnum.pArea))
    {
		send_to_char("RepEdit:  Widevnum in an area you cannot build in.\n\r", ch);
		return false;
    }

    if (get_reputation_index(wnum.pArea, wnum.vnum))
    {
		send_to_char("RepEdit:  Reputation already exists.\n\r", ch);
		return false;
    }

	pRep = new_reputation_index_data();
	pRep->area = wnum.pArea;
	pRep->vnum = wnum.vnum;

	if (wnum.vnum > wnum.pArea->top_reputation_vnum)
		wnum.pArea->top_reputation_vnum = wnum.vnum;

	int iHash = wnum.vnum % MAX_KEY_HASH;
	pRep->next = wnum.pArea->reputation_index_hash[iHash];
	wnum.pArea->reputation_index_hash[iHash] = pRep;
	olc_set_editor(ch, ED_REPEDIT, pRep);

    SET_BIT(pRep->area->area_flags, AREA_CHANGED);
    free_string(pRep->created_by);
    pRep->created_by = str_dup(ch->name);

	send_to_char("Reputation created.\n\r", ch);
	return true;
}

REPEDIT( repedit_name )
{
	REPUTATION_INDEX_DATA *pRep;

	EDIT_REPUTATION(ch, pRep);

    if (argument[0] == '\0')
    {
	    send_to_char("Syntax:  name {R<name>{x\n\r", ch);
		send_to_char("Please specify a name.\n\r", ch);
		return false;
    }

	smash_tilde(argument);
	free_string(pRep->name);
	pRep->name = str_dup(argument);
	send_to_char("Reputation name set.\n\r", ch);
    return true;
}

REPEDIT( repedit_description )
{
	REPUTATION_INDEX_DATA *pRep;

	EDIT_REPUTATION(ch, pRep);

    if (argument[0] == '\0')
    {
		string_append(ch, &pRep->description);
		return true;
    }

    send_to_char("Syntax:  description\n\r", ch);
    return false;
}

REPEDIT( repedit_comments )
{
	REPUTATION_INDEX_DATA *pRep;

	EDIT_REPUTATION(ch, pRep);

    if (argument[0] == '\0')
    {
		string_append(ch, &pRep->comments);
		return true;
    }

    send_to_char("Syntax:  comments\n\r", ch);
    return false;
}

REPEDIT( repedit_flags )
{
	REPUTATION_INDEX_DATA *pRep;

	EDIT_REPUTATION(ch, pRep);

	long value;
    if ((value = flag_value(reputation_flags, argument)) == NO_FLAG)
	{
		send_to_char("Syntax:  flags {R<flags>{x\n\r", ch);
		send_to_char("Invalid reputation flags.  Use '? reputation' to see valid list of flags.\n\r", ch);
		show_flag_cmds(ch, reputation_flags);
		return false;
	}

	TOGGLE_BIT(pRep->flags, value);
	send_to_char("Reputation flags toggled.\n\r", ch);
	return true;
}

struct color_name_type {
	char *name;
	char color;
};

const struct color_name_type color_name_table[] =
{
	{"blue",		'B'},
	{"cyan",		'C'},
	{"dark",		'D'},
	{"green",		'G'},
	{"magenta",		'M'},
	{"red",			'R'},
	{"white",		'W'},
	{"yellow",		'Y'},
	{ NULL,			' '}
};

static char color_name_lookup(const char *name)
{
	for(int i = 0; color_name_table[i].name; i++)
		if (!str_prefix(name, color_name_table[i].name))
			return color_name_table[i].color;
	
	return ' ';
}

static void __reorder_reputation_ranks(REPUTATION_INDEX_DATA *rep)
{
	int ordinal = 0;
	ITERATOR it;
	REPUTATION_INDEX_RANK_DATA *rank;
	iterator_start(&it, rep->ranks);
	while((rank = (REPUTATION_INDEX_RANK_DATA *)iterator_nextdata(&it)))
	{
		rank->ordinal = ++ordinal;
	}
	iterator_stop(&it);
}

static void __add_reputation_rank(REPUTATION_INDEX_DATA *rep, char *name, char color, long capacity, long flags)
{
	REPUTATION_INDEX_RANK_DATA *rank = new_reputation_index_rank_data();

	rank->uid = ++rep->top_rank_uid;
	rank->capacity = capacity;
	rank->flags = flags;
	rank->color = color;
	free_string(rank->name);
	rank->name = str_dup(name);

	list_appendlink(rep->ranks, rank);
}

REPEDIT( repedit_rank )
{
	char buf[MSL];
	char arg[MIL];
	REPUTATION_INDEX_DATA *pRep;
	REPUTATION_INDEX_RANK_DATA *rank;

	EDIT_REPUTATION(ch, pRep);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  rank add <name>\n\r", ch);
		send_to_char("         rank clear\n\r", ch);
		send_to_char("         rank standard <faction|individual>\n\r", ch);
		send_to_char("         rank <#> capacity <number>\n\r", ch);
		send_to_char("         rank <#> color <name>\n\r", ch);
		send_to_char("         rank <#> comments\n\r", ch);
		send_to_char("         rank <#> description\n\r", ch);
		send_to_char("         rank <#> flags <flags>\n\r", ch);
		send_to_char("         rank <#> name <name>\n\r", ch);
		send_to_char("         rank <#> remove\n\r", ch);
		return false;
	}

	argument = one_argument(argument, arg);
	if (is_number(arg))
	{
		if (list_size(pRep->ranks) < 1)
		{
			send_to_char("Please add a rank first.\n\r", ch);
			return false;
		}

		int index = atoi(arg);
		if (index < 1 || index > list_size(pRep->ranks))
		{
			send_to_char("Syntax:  rank {R<#>{x capacity <number>\n\r", ch);
			send_to_char("         rank {R<#>{x color <name>\n\r", ch);
			send_to_char("         rank {R<#>{x comments\n\r", ch);
			send_to_char("         rank {R<#>{x description\n\r", ch);
			send_to_char("         rank {R<#>{x flags <flags>\n\r", ch);
			send_to_char("         rank {R<#>{x name <name>\n\r", ch);
			send_to_char("         rank {R<#>{x remove\n\r", ch);
			send_to_char(formatf("Please specify a number from 1 to %d.\n\r", list_size(pRep->ranks)), ch);
			return false;
		}

		rank = (REPUTATION_INDEX_RANK_DATA *)list_nthdata(pRep->ranks, index);

		argument = one_argument(argument, arg);
		if (!str_prefix(arg, "capacity"))
		{
			int capacity;
			if (!is_number(argument) || (capacity = atoi(argument)) < 1)
			{
				send_to_char("Syntax:  rank <#> capacity {R<number>{x\n\r", ch);
				send_to_char("Please specify a positive number.\n\r", ch);
				return false;
			}

			// If this rank is the initial rank, make sure the initial reputation is within bounds
			if (pRep->initial_rank == index)
				pRep->initial_reputation = UMIN(pRep->initial_reputation, capacity - 1);

			rank->capacity = capacity;
			send_to_char(formatf("Rank #%d capacity set.\n\r", index), ch);
			return true;
		}

		if (!str_prefix(arg, "color"))
		{
			char color = color_name_lookup(argument);
			if (color == ' ')
			{
				send_to_char("Syntax:  rank <#> color {R<name>{x\n\r", ch);
				send_to_char("Invalid color name.  Select one of the following:\n\r", ch);
				buf[0] = '\0';
				for(int i = 0; color_name_table[i].name; i++)
				{
					if (i > 0)
						sprintf(arg, "{x, {%c%s", color_name_table[i].color, color_name_table[i].name);
					else
						sprintf(arg, "   {%c%s", color_name_table[i].color, color_name_table[i].name);
					
					strcat(buf, arg);
				}
				strcat(buf, "\n\r");
				send_to_char(buf, ch);
				return false;
			}

			rank->color = color;
			send_to_char(formatf("Rank #%d color set.\n\r", index), ch);
			return true;
		}

		if (!str_prefix(arg, "comments"))
		{
			string_append(ch, &rank->comments);
			return true;
		}

		if (!str_prefix(arg, "description"))
		{
			string_append(ch, &rank->description);
			return true;
		}

		if (!str_prefix(arg, "flags"))
		{
			int value;
			if ((value = flag_value(reputation_rank_flags, argument)) == NO_FLAG)
			{
				send_to_char("Syntax:  rank <#> flags {R<flags>{x\n\r", ch);
				send_to_char("Invalid reputation rank flags.  Use '? reputation_rank' for a list of valid flags.\n\r", ch);
				show_flag_cmds(ch, reputation_rank_flags);
				return false;
			}

			TOGGLE_BIT(rank->flags, value);
			send_to_char(formatf("Rank #%d flags toggled.\n\r", index), ch);
			return true;
		}

		if (!str_prefix(arg, "name"))
		{
			if (argument[0] == '\0')
			{
				send_to_char("Syntax:  rank <#> name {R<name>{x\n\r", ch);
				send_to_char("Please specify a name.\n\r", ch);
				return false;
			}

			smash_tilde(argument);
			free_string(rank->name);
			rank->name = str_dup(argument);

			send_to_char(formatf("Rank #%d name set.\n\r", index), ch);
			return true;
		}

		if (!str_prefix(arg, "remove"))
		{
			list_remnthlink(pRep->ranks, index);
			__reorder_reputation_ranks(pRep);

			send_to_char(formatf("Rank #%d removed.\n\r", index), ch);
			return true;
		}

		send_to_char("Syntax:  rank <#> {Rcapacity{x <number>\n\r", ch);
		send_to_char("         rank <#> {Rcolor{x <name>\n\r", ch);
		send_to_char("         rank <#> {Rcomments{x\n\r", ch);
		send_to_char("         rank <#> {Rdescription{x\n\r", ch);
		send_to_char("         rank <#> {Rflags{x <flags>\n\r", ch);
		send_to_char("         rank <#> {Rname{x <name>\n\r", ch);
		send_to_char("         rank <#> {Rremove{x\n\r", ch);
		return false;
	}

	if (!str_prefix(arg, "add"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  rank add {R<name>{x\n\r", ch);
			send_to_char("Please specify a name.\n\r", ch);
			return false;
		}

		rank = new_reputation_index_rank_data();
		rank->uid = ++pRep->top_rank_uid;
		rank->capacity = 1;
		free_string(rank->name);
		smash_tilde(argument);
		rank->name = str_dup(argument);

		list_appendlink(pRep->ranks, rank);
		__reorder_reputation_ranks(pRep);

		send_to_char(formatf("Rank #%d added.\n\r", rank->ordinal), ch);
		return true;
	}

	if (!str_prefix(arg, "clear"))
	{
		if (list_size(pRep->ranks) < 1)
		{
			send_to_char("There are no ranks to remove.\n\r", ch);
			return false;
		}

		list_clear(pRep->ranks);
		send_to_char("Reputation ranks cleared.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "standard"))
	{
		// Generate the standard list of reputations
		if (!str_prefix(argument, "faction"))
		{
			list_clear(pRep->ranks);

			// Do faction lists
			__add_reputation_rank(pRep, "Hated",		'R', 36000,		0);
			__add_reputation_rank(pRep, "Hostile",		'R', 3000,		0);
			__add_reputation_rank(pRep, "Unfriendly",	'R', 3000,		0);
			__add_reputation_rank(pRep, "Neutral",		'Y', 3000,		0);
			__add_reputation_rank(pRep, "Friendly",		'G', 6000,		0);
			__add_reputation_rank(pRep, "Honored",		'G', 12000,		REPUTATION_RANK_NORANKUP);
			__add_reputation_rank(pRep, "Revered",		'C', 21000,		REPUTATION_RANK_NORANKUP);
			__add_reputation_rank(pRep, "Exalted",		'B', 1000,		0);
			__reorder_reputation_ranks(pRep);

			pRep->initial_rank = 4;
			pRep->initial_reputation = 0;

			send_to_char("Standard {WFaction{x Ranks generated.\n\r", ch);
			return true;
		}

		if (!str_prefix(argument, "individual"))
		{
			list_clear(pRep->ranks);

			// Do faction lists
			__add_reputation_rank(pRep, "Stranger",		'Y', 8400,		0);
			__add_reputation_rank(pRep, "Pal",			'Y', 8400,		0);
			__add_reputation_rank(pRep, "Buddy",		'G', 8400,		0);
			__add_reputation_rank(pRep, "Friend",		'G', 8400,		0);
			__add_reputation_rank(pRep, "Good Friend",	'C', 8400,		0);
			__add_reputation_rank(pRep, "Best Friend",	'B', 1,			0);

			__reorder_reputation_ranks(pRep);

			pRep->initial_rank = 1;
			pRep->initial_reputation = 0;

			send_to_char("Standard {WIndividual{x Ranks generated.\n\r", ch);
			return true;
		}

		send_to_char("Please specify either {Wfaction{x or {Windividual{x for standard ranks.\n\r", ch);
		return false;
	}

	repedit_rank(ch, "");
	return false;
}

REPEDIT( repedit_initial )
{
	char arg[MIL];
	REPUTATION_INDEX_DATA *pRep;

	EDIT_REPUTATION(ch, pRep);

	argument = one_argument(argument, arg);
	int index;
    if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(pRep->ranks))
    {
		send_to_char("Syntax:  initial {R<rank#>{x <reputation>\n\r", ch);
		send_to_char(formatf("Please specify a number from 1 to %d.\n\r", list_size(pRep->ranks)), ch);
		return false;
	}

	REPUTATION_INDEX_RANK_DATA *rank = (REPUTATION_INDEX_RANK_DATA *)list_nthdata(pRep->ranks, index);

	int rep;
	if (!is_number(argument) || (rep = atoi(argument)) < 0 || rep >= rank->capacity)
	{
		send_to_char("Syntax:  initial <rank#> {R<reputation>{x\n\r", ch);
		send_to_char(formatf("Please specify a number from 0 to %ld.\n\r", rank->capacity - 1), ch);
		return false;
	}

	pRep->initial_rank = index;
	pRep->initial_reputation = rep;
	send_to_char("Reputation initial values set.\n\r", ch);
	return true;
}

REPEDIT( repedit_token )
{
	REPUTATION_INDEX_DATA *pRep;

	EDIT_REPUTATION(ch, pRep);

	WNUM wnum;

	if (!str_prefix(argument, "none"))
	{
		pRep->token = NULL;
		send_to_char("Token cleared.\n\r", ch);
	}
	else if (parse_widevnum(argument, ch->in_room->area, &wnum))
	{
		TOKEN_INDEX_DATA *token = get_token_index(wnum.pArea, wnum.vnum);

		if (!token)
		{
			send_to_char("There is no token with that widevnum.\n\r", ch);
			return false;
		}

		pRep->token = token;
		send_to_char("Token set.\n\r", ch);
	}
	else
	{
		send_to_char("Syntax:  token {R<widenum|none>{x\n\r", ch);
		send_to_char("Please specify a widevnum or {Wnone{x.\n\r", ch);
		return false;
	}

	return true;
}


// TODO: Add Variable support
// TODO: Add Scripting support
// TODO: Add Immortal support
