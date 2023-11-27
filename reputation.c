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
REPUTATION_INDEX_DATA *load_reputation_index(FILE *fp)
{
	REPUTATION_INDEX_DATA *data;
	bool fMatch;
	char *word;

	data = new_reputation_index_data();

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
			break;

		case 'D':
			KEYS("Description", data->description, fread_string(fp));
			break;

		case 'I':
			KEY("InitialRank", data->initial_rank, fread_number(fp));
			KEY("InitialReputation", data->initial_reputation, fread_number(fp));
			break;

		case 'N':
			KEYS("Name", data->name, fread_string(fp));
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

	if (data->initial_rank > 0)
		fprintf(fp, "InitialRank %d\n", data->initial_rank);
	fprintf(fp, "InitialReputation %d\n", data->initial_reputation);

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

// Get Reputation on Character
REPUTATION_DATA *get_reputation_char(CHAR_DATA *ch, AREA_DATA *area, long vnum)
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

	return rep;
}

REPUTATION_DATA *get_reputation_char_auid(CHAR_DATA *ch, long auid, long vnum)
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

	return rep;
}

REPUTATION_DATA *get_reputation_char_wnum(CHAR_DATA *ch, WNUM wnum)
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

	return rep;
}

// Gain Reputation
int gain_reputation(CHAR_DATA *ch, REPUTATION_DATA *rep, int amount, bool show)
{
	if (!IS_VALID(ch) || !IS_VALID(rep) || !amount) return 0;

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
			rep->reputation = 0;
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

		// Only possible if at the highest rank
		REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(rep->pIndexData, rep->current_rank);
		if (rep->reputation > rank->capacity)
			rep->reputation = rank->capacity;
	}

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

	return rep->current_rank - last_rank;
}

// Set Rank on Reputation
bool set_reputation_rank(CHAR_DATA *ch, REPUTATION_DATA *rep, int rank_no, bool show)
{
	if (!IS_VALID(ch) || !IS_VALID(rep) || rank_no < 1 || rank_no > list_size(rep->pIndexData->ranks)) return false;

	if (rep->current_rank == rank_no) return false;

	int last_rank = rep->current_rank;

	rep->current_rank = rank_no;
	REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(rep->pIndexData, rep->current_rank);
	rep->reputation = rank->set;	// Set the reputation value according to the rank (usually 0)

	if (!show)
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

	return true;
}

// Insert Reputation
void insert_reputation(CHAR_DATA *ch, REPUTATION_DATA *rep)
{
	if (!IS_VALID(ch) || !IS_VALID(rep)) return;

	ITERATOR it;
	REPUTATION_DATA *r;
	iterator_start(&it, ch->reputations);
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
		list_appendlink(ch->reputations, rep);
	}
}

// Set Reputation on Character
REPUTATION_DATA *set_reputation_char(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex)
{
	if (!IS_VALID(ch) || !IS_VALID(repIndex)) return NULL;

	REPUTATION_DATA *rep = get_reputation_char(ch, repIndex->area, repIndex->vnum);
	// Already have it
	if (IS_VALID(rep)) return NULL;

	rep = new_reputation_data();
	rep->pIndexData = repIndex;
	rep->current_rank = repIndex->initial_rank;
	rep->reputation = repIndex->initial_reputation;

	insert_reputation(ch, rep);
	return rep;
}

// List Reputations on Character
void do_reputations(CHAR_DATA *ch, char *argument)
{
	char buf[MSL];

	if (argument[0] == '\0')
	{
		if (list_size(ch->reputations) > 0)
		{
			BUFFER *buffer = new_buf();

			int i = 0;
			ITERATOR it;
			REPUTATION_DATA *rep;
			iterator_start(&it, ch->reputations);
			while((rep = (REPUTATION_DATA *)iterator_nextdata(&it)))
			{
				REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(rep->pIndexData, rep->current_rank);
				int percent = 100 * rep->reputation / rank->capacity;
				int slots = 20 * rep->reputation / rank->capacity;

				sprintf(buf, "%3d)  %-20.20s  %-20.20s  %12s{x [{%c%-20s{x]\n\r", ++i,
					rep->pIndexData->name,
					rank->name,
					formatf("{W({x%d%%{W)", percent),
					rank->color ? rank->color : 'Y',
					(slots > 0) ? formatf("%*.*s", slots, slots, "####################") : "");

				add_buf(buffer, buf);
			}
			iterator_stop(&it);

			if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
			{
				send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
			}
			else
			{
				page_to_char(buffer->string, ch);
			}

			free_buf(buffer);
		}
		else
			send_to_char("You haven't acquired any reputations.\n\r", ch);
	}
	else
	{
		// Filter by criteria such as area name, rank name, etc
		send_to_char("Not yet implemented.\n\r", ch);
	}
}



// TODO: Add RepEdit
// TODO: Add Variable support
// TODO: Add Scripting support
// TODO: Add Immortal support
