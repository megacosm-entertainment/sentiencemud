#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "merc.h"
#include "recycle.h"

static REPUTATION_INDEX_RANK_DATA *new_reputation_index_rank_data(void)
{
    REPUTATION_INDEX_RANK_DATA *rank = calloc(1, sizeof(*rank));
    if (rank != NULL)
    {
        rank->valid = true;
        rank->name = str_dup("");
        rank->description = str_dup("");
        rank->comments = str_dup("");
        rank->capacity = 1;
        rank->color = 'Y';
    }
    return rank;
}

static REPUTATION_INDEX_DATA *new_reputation_index_data(void)
{
    REPUTATION_INDEX_DATA *rep = calloc(1, sizeof(*rep));
    if (rep != NULL)
    {
        rep->valid = true;
        rep->name = str_dup("");
        rep->description = str_dup("");
        rep->comments = str_dup("");
        rep->created_by = str_dup("");
        rep->ranks = list_create(false);
        rep->initial_rank = 1;
        rep->initial_reputation = 0;
    }
    return rep;
}

static REPUTATION_DATA *new_reputation_data(void)
{
    REPUTATION_DATA *rep = calloc(1, sizeof(*rep));
    if (rep != NULL)
    {
        rep->valid = true;
    }
    return rep;
}

REPUTATION_INDEX_DATA *load_reputation_index(FILE *fp, AREA_DATA *area)
{
    if (fp == NULL)
    {
        return NULL;
    }

    REPUTATION_INDEX_DATA *rep = new_reputation_index_data();
    if (rep == NULL)
    {
        return NULL;
    }

    rep->vnum = fread_number(fp);
    rep->area = area;

    char *word;
    while (str_cmp((word = fread_word(fp)), "#-REPUTATION"))
    {
        if (!str_cmp(word, "#RANK"))
        {
            REPUTATION_INDEX_RANK_DATA *rank = new_reputation_index_rank_data();
            if (rank == NULL)
            {
                continue;
            }

            rank->uid = fread_number(fp);

            char *rword;
            while (str_cmp((rword = fread_word(fp)), "#-RANK"))
            {
                if (!str_cmp(rword, "Capacity"))
                {
                    rank->capacity = fread_number(fp);
                }
                else if (!str_cmp(rword, "Color"))
                {
                    rank->color = fread_letter(fp);
                }
                else if (!str_cmp(rword, "Comments"))
                {
                    free_string(rank->comments);
                    rank->comments = fread_string(fp);
                }
                else if (!str_cmp(rword, "Description"))
                {
                    free_string(rank->description);
                    rank->description = fread_string(fp);
                }
                else if (!str_cmp(rword, "Flags"))
                {
                    rank->flags = fread_flag(fp);
                }
                else if (!str_cmp(rword, "Name"))
                {
                    free_string(rank->name);
                    rank->name = fread_string(fp);
                }
                else
                {
                    fread_to_eol(fp);
                }
            }

            if (rank->uid > rep->top_rank_uid)
            {
                rep->top_rank_uid = rank->uid;
            }

            list_appendlink(rep->ranks, rank);
            rank->ordinal = list_size(rep->ranks);
            continue;
        }

        if (!str_cmp(word, "Comments"))
        {
            free_string(rep->comments);
            rep->comments = fread_string(fp);
        }
        else if (!str_cmp(word, "CreatedBy"))
        {
            free_string(rep->created_by);
            rep->created_by = fread_string(fp);
        }
        else if (!str_cmp(word, "Description"))
        {
            free_string(rep->description);
            rep->description = fread_string(fp);
        }
        else if (!str_cmp(word, "Flags"))
        {
            rep->flags = fread_flag(fp);
        }
        else if (!str_cmp(word, "InitialRank"))
        {
            rep->initial_rank = fread_number(fp);
        }
        else if (!str_cmp(word, "InitialReputation"))
        {
            rep->initial_reputation = fread_number(fp);
        }
        else if (!str_cmp(word, "Name"))
        {
            free_string(rep->name);
            rep->name = fread_string(fp);
        }
        else if (!str_cmp(word, "Token"))
        {
            WNUM_LOAD wload;
            char *token_value = fread_word(fp);
            if (parse_widevnum_load(token_value, &wload))
            {
                rep->token_load = wload;
            }
            else
            {
                rep->token_load.auid = area ? area->uid : 0;
                rep->token_load.vnum = atol(token_value);
            }
        }
        else
        {
            fread_to_eol(fp);
        }
    }

    return rep;
}

void save_reputation_indexes(FILE *fp, AREA_DATA *pArea)
{
    if (fp == NULL || pArea == NULL)
    {
        return;
    }

    for (int i = 0; i < MAX_KEY_HASH; i++)
    {
        for (REPUTATION_INDEX_DATA *rep = pArea->reputation_index_hash[i]; rep != NULL; rep = rep->next)
        {
            fprintf(fp, "#REPUTATION %ld\n", rep->vnum);
            fprintf(fp, "Name %s~\n", fix_string(rep->name ? rep->name : ""));
            fprintf(fp, "Description %s~\n", fix_string(rep->description ? rep->description : ""));
            fprintf(fp, "Comments %s~\n", fix_string(rep->comments ? rep->comments : ""));
            fprintf(fp, "CreatedBy %s~\n", fix_string(rep->created_by ? rep->created_by : ""));
            fprintf(fp, "Flags %s\n", print_flags(rep->flags));
            fprintf(fp, "InitialRank %d\n", rep->initial_rank);
            fprintf(fp, "InitialReputation %ld\n", rep->initial_reputation);

            if (rep->token)
            {
                fprintf(fp, "Token %ld#%ld\n", rep->token->area ? rep->token->area->uid : 0L, rep->token->vnum);
            }
            else if (rep->token_load.vnum > 0)
            {
                if (rep->token_load.auid > 0)
                {
                    fprintf(fp, "Token %ld#%ld\n", rep->token_load.auid, rep->token_load.vnum);
                }
                else
                {
                    fprintf(fp, "Token %ld\n", rep->token_load.vnum);
                }
            }

            ITERATOR it;
            iterator_start(&it, rep->ranks);
            REPUTATION_INDEX_RANK_DATA *rank;
            while ((rank = (REPUTATION_INDEX_RANK_DATA *)iterator_nextdata(&it)) != NULL)
            {
                fprintf(fp, "#RANK %d\n", rank->uid);
                fprintf(fp, "Name %s~\n", fix_string(rank->name ? rank->name : ""));
                fprintf(fp, "Description %s~\n", fix_string(rank->description ? rank->description : ""));
                fprintf(fp, "Comments %s~\n", fix_string(rank->comments ? rank->comments : ""));
                fprintf(fp, "Flags %s\n", print_flags(rank->flags));
                fprintf(fp, "Capacity %ld\n", rank->capacity);
                fprintf(fp, "Color %c\n", rank->color ? rank->color : 'Y');
                fprintf(fp, "#-RANK\n");
            }
            iterator_stop(&it);

            fprintf(fp, "#-REPUTATION\n");
        }
    }
}

REPUTATION_INDEX_DATA *get_reputation_index(AREA_DATA *area, long vnum)
{
    if (area == NULL || vnum < 1)
    {
        return NULL;
    }

    int hash = vnum % MAX_KEY_HASH;
    for (REPUTATION_INDEX_DATA *rep = area->reputation_index_hash[hash]; rep != NULL; rep = rep->next)
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

REPUTATION_INDEX_RANK_DATA *get_reputation_rank(REPUTATION_INDEX_DATA *rep, int ordinal)
{
    if (rep == NULL || rep->ranks == NULL || ordinal < 1)
    {
        return NULL;
    }

    ITERATOR it;
    iterator_start(&it, rep->ranks);

    REPUTATION_INDEX_RANK_DATA *rank = NULL;
    while ((rank = (REPUTATION_INDEX_RANK_DATA *)iterator_nextdata(&it)) != NULL)
    {
        if (rank->ordinal == ordinal)
        {
            iterator_stop(&it);
            return rank;
        }
    }

    iterator_stop(&it);
    return NULL;
}

REPUTATION_INDEX_RANK_DATA *get_reputation_rank_uid(REPUTATION_INDEX_DATA *rep, int16_t uid)
{
    if (rep == NULL || rep->ranks == NULL || uid < 1)
    {
        return NULL;
    }

    ITERATOR it;
    iterator_start(&it, rep->ranks);

    REPUTATION_INDEX_RANK_DATA *rank = NULL;
    while ((rank = (REPUTATION_INDEX_RANK_DATA *)iterator_nextdata(&it)) != NULL)
    {
        if (rank->uid == uid)
        {
            iterator_stop(&it);
            return rank;
        }
    }

    iterator_stop(&it);
    return NULL;
}

REPUTATION_DATA *find_reputation_char(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex)
{
    if (ch == NULL || repIndex == NULL || ch->reputations == NULL)
    {
        return NULL;
    }

    ITERATOR it;
    iterator_start(&it, ch->reputations);

    REPUTATION_DATA *rep = NULL;
    while ((rep = (REPUTATION_DATA *)iterator_nextdata(&it)) != NULL)
    {
        if (rep->pIndexData == repIndex)
        {
            iterator_stop(&it);
            return rep;
        }
    }

    iterator_stop(&it);
    return NULL;
}

REPUTATION_DATA *set_reputation_char(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex, int startingRank, int startingRep, bool show)
{
    (void)show;

    if (ch == NULL || repIndex == NULL || IS_NPC(ch))
    {
        return NULL;
    }

    REPUTATION_DATA *rep = find_reputation_char(ch, repIndex);
    if (rep == NULL)
    {
        rep = new_reputation_data();
        if (rep == NULL)
        {
            return NULL;
        }

        rep->pIndexData = repIndex;

        if (ch->reputations == NULL)
        {
            ch->reputations = list_create(false);
        }

        if (ch->reputations == NULL || !list_appendlink(ch->reputations, rep))
        {
            free(rep);
            return NULL;
        }
    }

    if (startingRank > 0)
    {
        rep->current_rank = startingRank;
    }
    else if (rep->current_rank < 1)
    {
        rep->current_rank = (repIndex->initial_rank > 0) ? repIndex->initial_rank : 1;
    }

    rep->reputation = (startingRep >= 0) ? startingRep : repIndex->initial_reputation;
    if (rep->maximum_rank < rep->current_rank)
    {
        rep->maximum_rank = rep->current_rank;
    }

    return rep;
}

REPUTATION_DATA *get_reputation_char(CHAR_DATA *ch, AREA_DATA *area, long vnum, bool add, bool show)
{
    REPUTATION_INDEX_DATA *repIndex = get_reputation_index(area, vnum);
    if (repIndex == NULL)
    {
        return NULL;
    }

    REPUTATION_DATA *rep = find_reputation_char(ch, repIndex);
    if (rep == NULL && add)
    {
        rep = set_reputation_char(ch, repIndex, repIndex->initial_rank, repIndex->initial_reputation, show);
    }

    return rep;
}

REPUTATION_DATA *get_reputation_char_auid(CHAR_DATA *ch, long auid, long vnum, bool add, bool show)
{
    return get_reputation_char(ch, get_area_from_uid(auid), vnum, add, show);
}

REPUTATION_DATA *get_reputation_char_wnum(CHAR_DATA *ch, WNUM wnum, bool add, bool show)
{
    return get_reputation_char(ch, wnum.pArea, wnum.vnum, add, show);
}

bool set_reputation_rank(CHAR_DATA *ch, REPUTATION_DATA *rep, int rank_no, int rank_rep, bool show)
{
    (void)ch;
    (void)show;

    if (rep == NULL || rep->pIndexData == NULL)
    {
        return false;
    }

    REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(rep->pIndexData, rank_no);
    if (rank == NULL)
    {
        return false;
    }

    rep->current_rank = rank_no;
    rep->reputation = UMAX(0, rank_rep);
    rep->maximum_rank = UMAX(rep->maximum_rank, rep->current_rank);
    return true;
}

bool gain_reputation(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex, long amount, int *change, long *total_given, bool show)
{
    (void)show;

    if (change != NULL)
    {
        *change = 0;
    }
    if (total_given != NULL)
    {
        *total_given = 0;
    }

    REPUTATION_DATA *rep = set_reputation_char(ch, repIndex, repIndex ? repIndex->initial_rank : 0, repIndex ? repIndex->initial_reputation : 0, false);
    if (rep == NULL)
    {
        return false;
    }

    long before = rep->reputation;
    long after = before + amount;
    if (after < 0)
    {
        after = 0;
    }

    rep->reputation = after;

    if (change != NULL)
    {
        *change = (int)(after - before);
    }
    if (total_given != NULL)
    {
        *total_given = (after - before);
    }

    return true;
}

bool has_reputation(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex)
{
    return find_reputation_char(ch, repIndex) != NULL;
}

void paragon_reputation(CHAR_DATA *ch, REPUTATION_DATA *rep, bool show)
{
    (void)ch;
    (void)show;

    if (rep != NULL)
    {
        rep->paragon_level++;
    }
}

bool is_reputation_rank_peaceful(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex)
{
    REPUTATION_DATA *rep = find_reputation_char(ch, repIndex);
    if (rep == NULL)
    {
        return false;
    }

    REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(repIndex, rep->current_rank);
    return rank != NULL && IS_SET(rank->flags, REPUTATION_RANK_PEACEFUL);
}

bool is_reputation_rank_hostile(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex)
{
    REPUTATION_DATA *rep = find_reputation_char(ch, repIndex);
    if (rep == NULL)
    {
        return false;
    }

    REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(repIndex, rep->current_rank);
    return rank != NULL && IS_SET(rank->flags, REPUTATION_RANK_HOSTILE);
}

void group_gain_reputation(CHAR_DATA *ch, CHAR_DATA *victim)
{
    (void)ch;
    (void)victim;
}

void check_mob_factions(CHAR_DATA *ch, CHAR_DATA *victim)
{
    (void)ch;
    (void)victim;
}

bool check_mob_factions_peaceful(CHAR_DATA *ch, CHAR_DATA *victim)
{
    (void)ch;
    (void)victim;
    return false;
}

bool check_mob_factions_hostile(CHAR_DATA *ch, CHAR_DATA *victim)
{
    (void)ch;
    (void)victim;
    return false;
}

void do_reputations(CHAR_DATA *ch, char *argument)
{
    (void)argument;

    if (IS_NPC(ch))
    {
        send_to_char("Huh?\n\r", ch);
        return;
    }

    if (ch->reputations == NULL || list_size(ch->reputations) < 1)
    {
        send_to_char("You haven't acquired any reputations yet.\n\r", ch);
        return;
    }

    BUFFER *buffer = new_buf();
    if (buffer == NULL)
    {
        send_to_char("No reputations available.\n\r", ch);
        return;
    }

    add_buf(buffer, "{W[#]  [Reputation]                [Rank]                      [Points]{x\n\r");
    add_buf(buffer, "{W--------------------------------------------------------------------------{x\n\r");

    int row = 0;
    ITERATOR it;
    iterator_start(&it, ch->reputations);

    REPUTATION_DATA *rep = NULL;
    while ((rep = (REPUTATION_DATA *)iterator_nextdata(&it)) != NULL)
    {
        REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(rep->pIndexData, rep->current_rank);
        const char *rep_name = (rep->pIndexData && !IS_NULLSTR(rep->pIndexData->name)) ? rep->pIndexData->name : "(unknown)";
        const char *rank_name = (rank && !IS_NULLSTR(rank->name)) ? rank->name : "(none)";

        bprintf(buffer, "%3d  %-26.26s %-27.27s %8ld\n\r", ++row, rep_name, rank_name, rep->reputation);
    }

    iterator_stop(&it);

    if (row < 1)
    {
        send_to_char("You haven't acquired any reputations yet.\n\r", ch);
    }
    else
    {
        page_to_char(buf_string(buffer), ch);
    }

    free_buf(buffer);
}
