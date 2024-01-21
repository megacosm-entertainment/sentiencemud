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
#include "magic.h"
#include "tables.h"

// TODO: Created quests


QUEST_INDEX_DATA *get_quest_index(AREA_DATA *area, long vnum)
{
    if (!area || vnum < 1) return NULL;

    for(QUEST_INDEX_DATA *quest = area->quest_index_hash[vnum % MAX_KEY_HASH]; quest; quest = quest->next)
    {
        if (quest->vnum == vnum)
            return quest;
    }
    return NULL;
}


void save_quest( FILE *fp, QUEST_INDEX_DATA *pQuestIndex )
{
    ITERATOR it;
    QUEST_INDEX_DATA *quest;

    fprintf(fp, "#QUEST %ld\n", pQuestIndex->vnum);
    fprintf(fp, "Name %s~\n", fix_string(pQuestIndex->name));
    fprintf(fp, "Description %s~\n", fix_string(pQuestIndex->description));

    iterator_start(&it, pQuestIndex->prerequisites);
    while((quest = (QUEST_INDEX_DATA *)iterator_nextdata(&it)))
    {
        fprintf(fp, "Prerequisite %s\n", widevnum_string(quest->area, quest->vnum, pQuestIndex->area));
    }
    iterator_stop(&it);
    // next_quests not saved

    // TODO: Stages

    fprintf(fp, "Flags %s\n", print_flags(pQuestIndex->flags));

    // Class restriction
    // Mutually exclusive
    if (IS_VALID(pQuestIndex->clazz))
    {
        fprintf(fp, "Class %s~\n", pQuestIndex->clazz->name);
        if (pQuestIndex->class_level > 0)
            fprintf(fp, "ClassLevel %d\n", pQuestIndex->class_level);
    }
    else if (pQuestIndex->clazz_type != CLASS_NONE)
    {
        fprintf(fp, "ClassType %s~\n", flag_string(class_types, pQuestIndex->clazz_type));
        if (pQuestIndex->class_level > 0)
            fprintf(fp, "ClassLevel %d\n", pQuestIndex->class_level);
    }
    

    // Racial requirements
    if (IS_VALID(pQuestIndex->race))
    {
        fprintf(fp, "Race %s~\n", pQuestIndex->race->name);
        if (pQuestIndex->race_level > 0)
            fprintf(fp, "RaceLevel %d\n", pQuestIndex->race_level);
    }

    // Reputation Requirements
    if (IS_VALID(pQuestIndex->reputation))
    {
        fprintf(fp, "Reputation %s\n", widevnum_string(pQuestIndex->reputation->area, pQuestIndex->reputation->vnum, pQuestIndex->area));
        if (pQuestIndex->min_reputation_rank > 0)
            fprintf(fp, "MinReputationRank %d\n", pQuestIndex->min_reputation_rank);
        if (pQuestIndex->max_reputation_rank > 0)
            fprintf(fp, "MaxReputationRank %d\n", pQuestIndex->max_reputation_rank);
    }

    // Skill/Song Requirements
    QUEST_INDEX_SKILL_REQUIREMENT *skreq;
    iterator_start(&it, pQuestIndex->skills);
    while((skreq = (QUEST_INDEX_SKILL_REQUIREMENT *)iterator_nextdata(&it)))
    {
        fprintf(fp, "Skill %s~ %d %d\n", skreq->skill->name, skreq->min_rating, skreq->max_rating);
    }
    iterator_stop(&it);

    QUEST_INDEX_SONG_REQUIREMENT *sngreq;
    iterator_start(&it, pQuestIndex->songs);
    while((sngreq = (QUEST_INDEX_SONG_REQUIREMENT *)iterator_nextdata(&it)))
    {
        fprintf(fp, "Song %s~ %d %d\n", sngreq->song->name, sngreq->min_rating, sngreq->max_rating);
    }
    iterator_stop(&it);
    
    // Stats requirements
    for(int i = 0; i < MAX_STATS; i++)
    {
        fprintf(fp, "Stat %s~ %d %d\n", flag_string(stat_types, i), pQuestIndex->stats[i][0], pQuestIndex->stats[i][1]);
    }

    fprintf(fp, "RepeatCooldown %d\n", pQuestIndex->repeat_cooldown);

    if (IS_VALID(pQuestIndex->target_clazz))
        fprintf(fp, "TargetClass %s~\n", pQuestIndex->target_clazz->name);

    fprintf(fp, "XPReward %ld\n", pQuestIndex->exp_reward);
    fprintf(fp, "MPReward %d\n", pQuestIndex->mp_reward);
    fprintf(fp, "PracReward %d\n", pQuestIndex->prac_reward);
    fprintf(fp, "TrainReward %d\n", pQuestIndex->train_reward);
    fprintf(fp, "GoldReward %d\n", pQuestIndex->gold_reward);
    fprintf(fp, "SilverReward %d\n", pQuestIndex->silver_reward);
    fprintf(fp, "PneumaReward %d\n", pQuestIndex->pneuma_reward);
    fprintf(fp, "DeityReward %d\n", pQuestIndex->deity_reward);

    if (IS_VALID(pQuestIndex->reputation_reward))
    {
        fprintf(fp, "ReputationReward %s\n", widevnum_string(pQuestIndex->reputation_reward->area, pQuestIndex->reputation_reward->vnum, pQuestIndex->area));
        if (pQuestIndex->reputation_reward_rank > 0)
            fprintf(fp, "ReputationRewardRank %d\n", pQuestIndex->reputation_reward_rank);
        if (pQuestIndex->reputation_points > 0)
            fprintf(fp, "ReputationPoints %d\n", pQuestIndex->reputation_points);
        if (pQuestIndex->paragon_levels > 0)
            fprintf(fp, "ParagonLevels %d\n", pQuestIndex->paragon_levels);
    }

    QUEST_INDEX_ITEM_REWARD *reward;
    iterator_start(&it, pQuestIndex->item_rewards);
    while((reward = (QUEST_INDEX_ITEM_REWARD *)iterator_nextdata(&it)))
    {
        fprintf(fp, "ItemReward %s %d %d\n", widevnum_string(reward->pObj->area, reward->pObj->vnum, pQuestIndex->area),
            reward->min_count, reward->max_count);
    }
    iterator_stop(&it);

    QUEST_INDEX_SKILL_REWARD *skreward;
    iterator_start(&it, pQuestIndex->skill_rewards);
    while((skreward = (QUEST_INDEX_SKILL_REWARD *)iterator_nextdata(&it)))
    {
        fprintf(fp, "SkillReward %s~ %d\n", skreward->skill->name, skreward->rating);
    }
    iterator_stop(&it);

    QUEST_INDEX_SONG_REWARD *sngreward;
    iterator_start(&it, pQuestIndex->song_rewards);
    while((sngreward = (QUEST_INDEX_SONG_REWARD *)iterator_nextdata(&it)))
    {
        fprintf(fp, "SongReward %s~ %d\n", sngreward->song->name, sngreward->rating);
    }
    iterator_stop(&it);

    if (pQuestIndex->reward_script > 0)
    {
        fprintf(fp, "RewardScript %ld %s~\n", pQuestIndex->reward_script, fix_string(pQuestIndex->reward_script_description));
    }

    // TODO: Completion Followup

    fprintf(fp, "#-QUEST\n");
}

void save_quests( FILE *fp, AREA_DATA *pArea )
{
    if (!pArea) return;
    
    for(int i = 0; i < MAX_KEY_HASH; i++)
    {
        for(QUEST_INDEX_DATA *quest = pArea->quest_index_hash[i]; quest; quest = quest->next)
        {
            save_quest(fp, quest);
        }
    }
}

QUEST_INDEX_DATA *read_quest(FILE *fp, AREA_DATA *area)
{
    QUEST_INDEX_DATA *quest;
    char *word;
    bool fMatch;

    quest = new_quest_index();
    quest->area = area;
    quest->vnum = fread_number(fp);

	while(str_cmp((word = fread_word(fp)), "#-QUEST"))
    {
        fMatch = false;

        switch(word[0])
        {
            case '#':
                // #COMPLETION
                // #STAGE
                break;

            case 'C':
                KEY("Class", quest->clazz, get_class_data(fread_string(fp)));
                KEY("ClassLevel", quest->class_level, fread_number(fp));
                KEY("ClassType", quest->clazz_type, stat_lookup(fread_string(fp), class_types, CLASS_NONE));
                break;

            case 'D':
                KEY("DeityReward", quest->deity_reward, fread_number(fp));
                KEYS("Description", quest->description, fread_string(fp));
                break;

            case 'F':
                KEY("Flags", quest->flags, fread_flag(fp));
                break;

            case 'G':
                KEY("GoldReward", quest->gold_reward, fread_number(fp));
                break;

            case 'I':
                if (!str_cmp(word, "ItemReward"))
                {
                    WNUM_LOAD wnum = fread_widevnum(fp, area->uid);
                    int min_count = fread_number(fp);
                    int max_count = fread_number(fp);

                    if (wnum.auid > 0 && wnum.vnum > 0)
                    {
                        QUEST_INDEX_ITEM_REWARD *reward = new_quest_item_reward();
                        reward->wnum = wnum;
                        reward->min_count = min_count;
                        reward->max_count = max_count;
                        list_appendlink(quest->item_rewards, reward);
                    }

                    fMatch = true;
                    break;
                }
                break;

            case 'M':
                KEY("MaxReputationRank", quest->max_reputation_rank, fread_number(fp));
                KEY("MinReputationRank", quest->min_reputation_rank, fread_number(fp));
                KEY("MPReward", quest->mp_reward, fread_number(fp));
                break;

            case 'N':
                KEYS("Name", quest->name, fread_string(fp));
                break;

            case 'P':
                KEY("ParagonLevels", quest->paragon_levels, fread_number(fp));
                KEY("PneumaReward", quest->pneuma_reward, fread_number(fp));
                KEY("PracReward", quest->prac_reward, fread_number(fp));
                if (!str_cmp(word, "Prerequisite"))
                {
                    WNUM_LOAD *load = fread_widevnumptr(fp, area->uid);

                    list_appendlink(quest->prerequisites, load);
                    fMatch = true;
                    break;
                }
                break;

            case 'R':
                KEY("Race", quest->race, get_race_data(fread_string(fp)));
                KEY("RaceLevel", quest->race_level, fread_number(fp));
                KEY("RepeatCooldown", quest->repeat_cooldown, fread_number(fp));
                KEY("Reputation", quest->load_reputation, fread_widevnum(fp, area->uid));
                KEY("ReputationPoints", quest->reputation_points, fread_number(fp));
                KEY("ReputationReward", quest->load_reputation_reward, fread_widevnum(fp, area->uid));
                KEY("ReputationRewardRank", quest->reputation_reward_rank, fread_number(fp));
                if (!str_cmp(word, "RewardScript"))
                {
                    quest->reward_script = fread_number(fp);
                    quest->reward_script_description = fread_string(fp);

                    fMatch = true;
                    break;
                }
                break;

            case 'S':
                KEY("SilverReward", quest->silver_reward, fread_number(fp));
                if (!str_cmp(word, "Skill"))
                {
                    char *name = fread_string(fp);
                    int min_rating = fread_number(fp);
                    int max_rating = fread_number(fp);

                    SKILL_DATA *skill = get_skill_data(name);
                    if (IS_VALID(skill))
                    {
                        QUEST_INDEX_SKILL_REQUIREMENT *req = new_quest_skill_requirement();
                        req->skill = skill;
                        req->min_rating = min_rating;
                        req->max_rating = max_rating;
                        list_appendlink(quest->skills, req);
                    }

                    fMatch = true;
                    break;
                }
                if (!str_cmp(word, "SkillReward"))
                {
                    char *name = fread_string(fp);
                    int rating = fread_number(fp);

                    SKILL_DATA *skill = get_skill_data(name);
                    if (IS_VALID(skill))
                    {
                        QUEST_INDEX_SKILL_REWARD *reward = new_quest_skill_reward();
                        reward->skill = skill;
                        reward->rating = rating;
                        list_appendlink(quest->skill_rewards, reward);
                    }

                    fMatch = true;
                    break;
                }
                if (!str_cmp(word, "Song"))
                {
                    char *name = fread_string(fp);
                    int min_rating = fread_number(fp);
                    int max_rating = fread_number(fp);

                    SONG_DATA *song = get_song_data(name);
                    if (IS_VALID(song))
                    {
                        QUEST_INDEX_SONG_REQUIREMENT *req = new_quest_song_requirement();
                        req->song = song;
                        req->min_rating = min_rating;
                        req->max_rating = max_rating;
                        list_appendlink(quest->songs, req);
                    }

                    fMatch = true;
                    break;
                }
                if (!str_cmp(word, "SongReward"))
                {
                    char *name = fread_string(fp);
                    int rating = fread_number(fp);

                    SONG_DATA *song = get_song_data(name);
                    if (IS_VALID(song))
                    {
                        QUEST_INDEX_SONG_REWARD *reward = new_quest_song_reward();
                        reward->song = song;
                        reward->rating = rating;
                        list_appendlink(quest->song_rewards, reward);
                    }

                    fMatch = true;
                    break;
                }
                if (!str_cmp(word, "Stat"))
                {
                    char *name = fread_string(fp);
                    int min_stat = fread_number(fp);
                    int max_stat = fread_number(fp);

                    int stat = stat_lookup(name, stat_types, NO_FLAG);
                    if (stat != NO_FLAG)
                    {
                        quest->stats[stat][0] = min_stat;
                        quest->stats[stat][1] = max_stat;
                    }

                    fMatch = true;
                    break;
                }
                break;

            case 'T':
                KEY("TargetClass", quest->target_clazz, get_class_data(fread_string(fp)));
                KEY("TrainReward", quest->train_reward, fread_number(fp));
                break;
            
            case 'X':
                KEY("XPReward", quest->exp_reward, fread_number(fp));
                break;
        }

		if (!fMatch)
		{
			log_stringf("read_quest: unknown word '%s' found.", word);
			fread_to_eol(fp);
		}
    }

    // Clear out some stuff that aren't "needed"
    if (!IS_VALID(quest->clazz) && quest->clazz_type == CLASS_NONE)
        quest->class_level = 0;

    if (!IS_VALID(quest->race))
        quest->race_level = 0;
    
    if (quest->load_reputation.auid < 1 || quest->load_reputation.vnum < 1)
    {
        quest->min_reputation_rank = -1;
        quest->max_reputation_rank = -1;
    }

    if (quest->load_reputation_reward.auid < 1 || quest->load_reputation_reward.vnum < 1)    
    {
        quest->reputation_reward_rank = -1;
        quest->reputation_points = 0;
        quest->paragon_levels = 0;
    }

    return quest;
}
