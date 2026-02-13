/***************************************************************************
 *  act_class.c — Class commands and reward system integration             *
 *                                                                         *
 *  Phase 6d: Player-facing class commands (setclass, clslist, classes)    *
 *  Phase 6e: Class reward apply/revoke/resolve system                     *
 ***************************************************************************/

#if defined(macintosh)
#include <types.h>
#include <time.h>
#else
#include <sys/types.h>
#include <sys/time.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <jansson.h>

#include "merc.h"
#include "interp.h"
#include "tables.h"
#include "class_data.h"
#include "skill_data.h"
#include "song_data.h"
#include "skill_group.h"
#include "recycle.h"
#include "db.h"

/* Forward declarations */
static bool has_reward_been_applied(CLASS_LEVEL *cl, int level, int type, const char *name);
static void mark_reward_applied(CLASS_LEVEL *cl, int level, int type, const char *name);
static void apply_single_reward(CHAR_DATA *ch, CLASS_DATA *clazz, CLASS_LEVEL *cl,
                                CLASS_REWARD *reward, bool silent);

/***************************************************************************
 * Reward Tracking — One-Time Reward Deduplication                         *
 *                                                                         *
 * Rewards are tracked in CLASS_LEVEL.custom_data under the key            *
 * "rewards_applied", which is a JSON array of objects like:               *
 *   { "level": 5, "type": 0, "name": "sword" }                           *
 * This ensures that when a character re-joins a class or reloads, the     *
 * reward messages are not shown again and one-time rewards don't re-fire. *
 ***************************************************************************/

/**
 * has_reward_been_applied - Check if a reward has already been applied
 *
 * Checks the CLASS_LEVEL.custom_data "rewards_applied" array for a
 * matching entry. Used to suppress duplicate messages on class re-join
 * and to prevent one-time rewards from firing again.
 *
 * @param cl      The character's class level entry
 * @param level   The reward's level
 * @param type    The reward type (REWARD_SKILL, etc.)
 * @param name    The reward name (skill/group name, or "" if no name)
 * @return        true if already applied
 */
static bool has_reward_been_applied(CLASS_LEVEL *cl, int level, int type, const char *name)
{
    if (!cl || !cl->custom_data)
        return false;

    json_t *applied = json_object_get(cl->custom_data, "rewards_applied");
    if (!applied || !json_is_array(applied))
        return false;

    size_t idx;
    json_t *entry;
    json_array_foreach(applied, idx, entry) {
        if (!json_is_object(entry))
            continue;

        int e_level = (int)json_integer_value(json_object_get(entry, "level"));
        int e_type  = (int)json_integer_value(json_object_get(entry, "type"));
        const char *e_name = json_string_value(json_object_get(entry, "name"));

        if (e_level == level && e_type == type) {
            if (!name || !name[0]) {
                if (!e_name || !e_name[0])
                    return true;
            } else if (e_name && !str_cmp(name, e_name)) {
                return true;
            }
        }
    }

    return false;
}

/**
 * mark_reward_applied - Record that a reward has been applied
 *
 * Adds an entry to the CLASS_LEVEL.custom_data "rewards_applied" array.
 * Creates the custom_data object and array if they don't exist.
 *
 * @param cl      The character's class level entry
 * @param level   The reward's level
 * @param type    The reward type
 * @param name    The reward name (may be NULL/empty)
 */
static void mark_reward_applied(CLASS_LEVEL *cl, int level, int type, const char *name)
{
    if (!cl)
        return;

    /* Ensure custom_data exists */
    if (!cl->custom_data)
        cl->custom_data = json_object();

    /* Ensure rewards_applied array exists */
    json_t *applied = json_object_get(cl->custom_data, "rewards_applied");
    if (!applied || !json_is_array(applied)) {
        applied = json_array();
        json_object_set_new(cl->custom_data, "rewards_applied", applied);
    }

    /* Add the entry */
    json_t *entry = json_object();
    json_object_set_new(entry, "level", json_integer(level));
    json_object_set_new(entry, "type", json_integer(type));
    if (name && name[0])
        json_object_set_new(entry, "name", json_string(name));
    json_array_append_new(applied, entry);
}

/***************************************************************************
 * Reward Application                                                      *
 ***************************************************************************/

/**
 * skill_entry_add_source - Add a class source to a skill entry
 *
 * Adds a SKILL_SOURCE node for the given class/scope. If the class already
 * has a source node, updates its scope if broader. After modifying the list,
 * refreshes the cached source_class and cross_class_scope on the entry to
 * reflect the broadest available scope.
 *
 * @param entry   The skill entry
 * @param clazz   The granting class
 * @param scope   REWARD_SCOPE_* constant
 */
static void skill_entry_add_source(SKILL_ENTRY *entry, CLASS_DATA *clazz, int scope)
{
    SKILL_SOURCE *src;

    if (!entry || !clazz)
        return;

    /* Check if this class already has a source node */
    for (src = entry->sources; src; src = src->next) {
        if (src->clazz == clazz) {
            /* Update scope if the new one is broader */
            if (scope > src->scope)
                src->scope = scope;
            goto refresh;
        }
    }

    /* Add a new source node */
    src = new_skill_source();
    src->clazz = clazz;
    src->scope = scope;
    src->next = entry->sources;
    entry->sources = src;

refresh:
    /* Refresh cached fields — pick the broadest scope source */
    entry->cross_class_scope = 0;
    entry->source_class = NULL;
    for (src = entry->sources; src; src = src->next) {
        if (!entry->source_class || src->scope > entry->cross_class_scope) {
            entry->source_class = src->clazz;
            entry->cross_class_scope = src->scope;
        }
    }
}

/**
 * skill_entry_remove_source - Remove a class source from a skill entry
 *
 * Removes the SKILL_SOURCE node for the given class. After removal,
 * refreshes the cached source_class and cross_class_scope.
 *
 * @param entry   The skill entry
 * @param clazz   The class to remove
 * @return        true if sources remain, false if the entry has no more class sources
 */
static bool skill_entry_remove_source(SKILL_ENTRY *entry, CLASS_DATA *clazz)
{
    SKILL_SOURCE *src, *prev = NULL;

    if (!entry || !clazz)
        return (entry && entry->sources != NULL);

    for (src = entry->sources; src; prev = src, src = src->next) {
        if (src->clazz == clazz) {
            if (prev)
                prev->next = src->next;
            else
                entry->sources = src->next;
            free_skill_source(src);
            break;
        }
    }

    /* Refresh cached fields */
    entry->cross_class_scope = 0;
    entry->source_class = NULL;
    for (src = entry->sources; src; src = src->next) {
        if (!entry->source_class || src->scope > entry->cross_class_scope) {
            entry->source_class = src->clazz;
            entry->cross_class_scope = src->scope;
        }
    }

    return (entry->sources != NULL);
}

/**
 * apply_skill_reward - Grant a single skill to a character from a class reward
 *
 * Creates a SKILL_ENTRY for the skill if the character doesn't already have it.
 * Adds the granting class as a source with the specified scope. If the character
 * already has the skill (from another class or source), adds an additional source
 * node without disturbing the existing ones.
 *
 * @param ch       The character
 * @param clazz    The granting class
 * @param name     Skill name string
 * @param rating   Skill rating/difficulty
 * @param scope    REWARD_SCOPE_* constant
 * @param silent   If true, suppress grant messages
 */
static void apply_skill_reward(CHAR_DATA *ch, CLASS_DATA *clazz,
                               const char *name, int rating, int scope, bool silent)
{
    int sn;
    SKILL_ENTRY *entry;

    if (!ch || !name || !name[0])
        return;

    sn = skill_lookup(name);
    if (sn < 0 || sn >= MAX_SKILL)
        return;

    /* Check if character already has this skill */
    entry = skill_entry_findsn(ch->sorted_skills, sn);
    if (entry) {
        /* Already has the skill — add this class as a source */
        skill_entry_add_source(entry, clazz, scope);

        /* If this class grants it at a better rating and character hasn't learned it yet */
        if (rating > 0 && entry->rating <= 0) {
            entry->rating = 1;      /* Minimum usable rating */
            ch->pcdata->learned[sn] = 1;
        }
        return;
    }

    /* Grant the skill */
    if (skill_table[sn].spell_fun != spell_null) {
        skill_entry_addspell(ch, sn, NULL, SKILLSRC_NORMAL, 0);
    } else {
        skill_entry_addskill(ch, sn, NULL, SKILLSRC_NORMAL, 0);
    }

    /* Find the just-inserted entry and set class metadata */
    entry = skill_entry_findsn(ch->sorted_skills, sn);
    if (entry) {
        skill_entry_add_source(entry, clazz, scope);
        if (rating > 0 && entry->rating <= 0) {
            entry->rating = 1;
            ch->pcdata->learned[sn] = 1;
        }
    }

    if (!silent) {
        char buf[MAX_STRING_LENGTH];
        sprintf(buf, "{MYou have learned {W%s{M as a %s.{x\n\r",
                skill_table[sn].name, clazz->name);
        send_to_char(buf, ch);
    }
}

/**
 * apply_group_reward - Grant all skills in a skill group
 *
 * Iterates the group's contents and applies each skill individually.
 *
 * @param ch       The character
 * @param clazz    The granting class
 * @param name     Group name string
 * @param scope    REWARD_SCOPE_* constant
 * @param silent   If true, suppress grant messages
 */
static void apply_group_reward(CHAR_DATA *ch, CLASS_DATA *clazz,
                               const char *name, int scope, bool silent)
{
    SKILL_GROUP *group;

    if (!ch || !name || !name[0])
        return;

    group = skill_group_find(name);
    if (!group) {
        log_stringf("apply_group_reward: unknown group '%s' for class %s",
                     name, clazz ? clazz->name : "?");
        return;
    }

    /* Also track group in the character's known_groups list */
    if (ch->pcdata && ch->pcdata->known_groups) {
        bool found = false;
        ITERATOR it;
        SKILL_GROUP *kg;
        iterator_start(&it, ch->pcdata->known_groups);
        while ((kg = (SKILL_GROUP *)iterator_nextdata(&it))) {
            if (kg == group) {
                found = true;
                break;
            }
        }
        iterator_stop(&it);

        if (!found)
            list_appendlink(ch->pcdata->known_groups, group);
    }

    /* Also set legacy group_known flag for backward compat */
    {
        int g;
        for (g = 0; g < MAX_GROUP; g++) {
            if (group_table[g].name && !str_cmp(group_table[g].name, name)) {
                ch->pcdata->group_known[g] = true;
                break;
            }
        }
    }

    /* Apply each skill in the group */
    ITERATOR it;
    char *skill_name;
    iterator_start(&it, group->contents);
    while ((skill_name = (char *)iterator_nextdata(&it))) {
        apply_skill_reward(ch, clazz, skill_name, 1, scope, silent);
    }
    iterator_stop(&it);

    if (!silent) {
        char buf[MAX_STRING_LENGTH];
        sprintf(buf, "{MYou gain the skill group {W%s{M.{x\n\r", group->name);
        send_to_char(buf, ch);
    }
}

/**
 * apply_single_reward - Apply one CLASS_REWARD to a character
 *
 * Dispatches based on reward type. For REWARD_SKILL and REWARD_GROUP,
 * grants skills with scope metadata. For other types, executes the
 * appropriate action (title, bonus, etc.).
 *
 * @param ch       The character
 * @param clazz    The granting class
 * @param cl       The character's CLASS_LEVEL for this class
 * @param reward   The reward to apply
 * @param silent   If true, suppress all output messages
 */
static void apply_single_reward(CHAR_DATA *ch, CLASS_DATA *clazz, CLASS_LEVEL *cl,
                                CLASS_REWARD *reward, bool silent)
{
    if (!ch || !clazz || !cl || !reward)
        return;

    switch (reward->type) {
        case REWARD_SKILL:
            apply_skill_reward(ch, clazz, reward->name, reward->value,
                               reward->scope, silent);
            break;

        case REWARD_GROUP:
            apply_group_reward(ch, clazz, reward->name, reward->scope, silent);
            break;

        case REWARD_TITLE:
            /* Title rewards unlock a title option for the class.
             * The title data is stored on the CLASS_DATA itself;
             * we just inform the character it's available. */
            if (!silent) {
                const char *title_name = NULL;
                if (reward->data) {
                    json_t *disp = json_object_get(reward->data, "display");
                    if (disp) {
                        title_name = json_string_value(json_object_get(disp, "neutral"));
                        if (!title_name)
                            title_name = json_string_value(json_object_get(disp, "male"));
                    }
                }
                if (title_name) {
                    char buf[MAX_STRING_LENGTH];
                    sprintf(buf, "{MYou have earned the title {W%s{M!{x\n\r", title_name);
                    send_to_char(buf, ch);
                }
            }
            break;

        case REWARD_BONUS:
            /* Apply a stat or attribute bonus.
             * Currently we support hp_max and mana_max. */
            if (reward->name && reward->value != 0) {
                if (!str_cmp(reward->name, "hp_max")) {
                    ch->max_hit += reward->value;
                    ch->pcdata->perm_hit += reward->value;
                } else if (!str_cmp(reward->name, "mana_max")) {
                    ch->max_mana += reward->value;
                    ch->pcdata->perm_mana += reward->value;
                } else if (!str_cmp(reward->name, "move_max")) {
                    ch->max_move += reward->value;
                    ch->pcdata->perm_move += reward->value;
                }

                if (!silent) {
                    char buf[MAX_STRING_LENGTH];
                    sprintf(buf, "{MYou gain {W%+d %s{M from your %s training!{x\n\r",
                            reward->value, reward->name, clazz->name);
                    send_to_char(buf, ch);
                }
            }
            break;

        case REWARD_TOKEN:
            /* Token rewards — create a token from the wnum in reward->data
             * and give it to the character. Tokens can have scripts attached,
             * so this also covers the REWARD_SCRIPT use case. */
            if (reward->data && json_is_object(reward->data)) {
                long auid = (long)json_integer_value(json_object_get(reward->data, "auid"));
                long vnum = (long)json_integer_value(json_object_get(reward->data, "vnum"));
                AREA_DATA *area = get_area_from_uid(auid);
                TOKEN_INDEX_DATA *tidx = area ? get_token_index(area, vnum) : NULL;

                if (tidx) {
                    give_token(tidx, ch, NULL, NULL);
                    if (!silent && reward->name) {
                        char buf[MAX_STRING_LENGTH];
                        sprintf(buf, "{MYou receive {W%s{M!{x\n\r", reward->name);
                        send_to_char(buf, ch);
                    }
                } else {
                    log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                        "apply_reward: REWARD_TOKEN for class '%s' level %d: "
                        "token not found (auid=%ld, vnum=%ld)",
                        clazz->name, reward->level, auid, vnum);
                }
            }
            break;

        case REWARD_SCRIPT:
            /* Script execution is handled via token scripts — configure a
             * REWARD_TOKEN with a token that has the desired script attached.
             * This case is intentionally a no-op. */
            break;

        case REWARD_CUSTOM:
            /* Write a JSON value to CLASS_LEVEL.custom_data */
            if (reward->name && reward->data && cl) {
                if (!cl->custom_data)
                    cl->custom_data = json_object();
                json_object_set(cl->custom_data, reward->name, reward->data);
            }
            break;

        case REWARD_TRAIT:
            /* Trait rewards — set trait on character's personal trait values.
             * This is a placeholder; full trait integration depends on the
             * trait system being used at the character level. */
            break;

        case REWARD_SONG:
            /* Song rewards — grant a song to the character.
             * If REWARD_SONG_UNLOCK is set, only makes the song available
             * for rehearsal (does not auto-learn). Otherwise, directly
             * grants the song as if rehearsed. */
            if (reward->name) {
                SONG_DATA *song = song_lookup(reward->name);
                if (song) {
                    if (IS_SET(reward->flags, REWARD_SONG_UNLOCK)) {
                        /* Unlock only — mark available for rehearsal.
                         * songs_learned[] stays false; do_rehearse will
                         * allow learning without the usual restrictions. */
                        ch->pcdata->songs_unlocked[song->uid] = true;

                        if (!silent) {
                            char buf[MAX_STRING_LENGTH];
                            sprintf(buf, "{MYou can now rehearse {W%s{M.{x\n\r",
                                    song->name);
                            send_to_char(buf, ch);
                        }
                    } else {
                        /* Direct grant — teach the song immediately */
                        if (!ch->pcdata->songs_learned[song->uid]) {
                            ch->pcdata->songs_learned[song->uid] = true;
                            skill_entry_addsong(ch, song, NULL, SKILLSRC_NORMAL);

                            if (!silent) {
                                char buf[MAX_STRING_LENGTH];
                                sprintf(buf, "{MYou have learned the song {W%s{M as a %s.{x\n\r",
                                        song->name, clazz->name);
                                send_to_char(buf, ch);
                            }
                        }
                    }
                } else {
                    log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                        "apply_reward: REWARD_SONG for class '%s' level %d: "
                        "unknown song '%s'",
                        clazz->name, reward->level, reward->name);
                }
            }
            break;

        default:
            break;
    }
}

/**
 * apply_class_rewards - Apply all class rewards in a level range
 *
 * Iterates the class's reward list and applies all rewards whose level
 * falls within [from_level, to_level]. Tracks which rewards have been
 * applied via CLASS_LEVEL.custom_data to avoid duplicate messages on
 * class re-join or character reload.
 *
 * @param ch          The character
 * @param clazz       The class whose rewards to apply
 * @param from_level  Start of level range (inclusive)
 * @param to_level    End of level range (inclusive)
 * @param on_join     true if this is a class join/reload (suppress messages for
 *                    rewards that were already applied; still apply their effects)
 */
void apply_class_rewards(CHAR_DATA *ch, CLASS_DATA *clazz,
                         int from_level, int to_level, bool on_join)
{
    CLASS_LEVEL *cl;
    ITERATOR it;
    CLASS_REWARD *reward;

    if (!ch || IS_NPC(ch) || !clazz)
        return;

    cl = get_class_level(ch, clazz);
    if (!cl)
        return;

    iterator_start(&it, clazz->rewards);
    while ((reward = (CLASS_REWARD *)iterator_nextdata(&it))) {
        if (reward->level < from_level || reward->level > to_level)
            continue;

        bool already_applied = has_reward_been_applied(cl, reward->level,
                                                        reward->type,
                                                        reward->name);

        /* Determine whether to show messages for this reward */
        bool silent = already_applied;  /* Already seen = no message */

        /* ONE_TIME rewards that have already fired should be completely skipped */
        if (IS_SET(reward->flags, REWARD_ONE_TIME) && already_applied)
            continue;

        /* Apply the reward (skill/group grants are idempotent) */
        apply_single_reward(ch, clazz, cl, reward, silent);

        /* Mark newly applied rewards */
        if (!already_applied) {
            mark_reward_applied(cl, reward->level, reward->type, reward->name);
        }
    }
    iterator_stop(&it);
}

/**
 * revoke_class_rewards - Revoke rewards flagged with REWARD_REVOKE_ON_LEAVE
 *
 * Called when a character leaves a class. Iterates all rewards for the class
 * and revokes those with the REWARD_REVOKE_ON_LEAVE flag set.
 *
 * Skills/groups are NOT revoked by default (you learned them). Bonuses and
 * titles ARE revoked by default.
 *
 * @param ch      The character
 * @param clazz   The class being left
 */
void revoke_class_rewards(CHAR_DATA *ch, CLASS_DATA *clazz)
{
    ITERATOR it;
    CLASS_REWARD *reward;

    if (!ch || IS_NPC(ch) || !clazz)
        return;

    iterator_start(&it, clazz->rewards);
    while ((reward = (CLASS_REWARD *)iterator_nextdata(&it))) {
        if (!IS_SET(reward->flags, REWARD_REVOKE_ON_LEAVE))
            continue;

        switch (reward->type) {
            case REWARD_BONUS:
                /* Reverse the bonus */
                if (reward->name && reward->value != 0) {
                    if (!str_cmp(reward->name, "hp_max")) {
                        ch->max_hit -= reward->value;
                        ch->pcdata->perm_hit -= reward->value;
                    } else if (!str_cmp(reward->name, "mana_max")) {
                        ch->max_mana -= reward->value;
                        ch->pcdata->perm_mana -= reward->value;
                    } else if (!str_cmp(reward->name, "move_max")) {
                        ch->max_move -= reward->value;
                        ch->pcdata->perm_move -= reward->value;
                    }
                }
                break;

            case REWARD_SKILL: {
                /* Remove this class as a source; only remove the entry
                 * entirely if no other class also grants it */
                int sn = skill_lookup(reward->name);
                if (sn >= 0 && sn < MAX_SKILL) {
                    SKILL_ENTRY *entry = skill_entry_findsn(ch->sorted_skills, sn);
                    if (entry) {
                        bool has_other = skill_entry_remove_source(entry, clazz);
                        if (!has_other) {
                            skill_entry_remove(&ch->sorted_skills, sn, NULL, NULL,
                                               entry->isspell);
                        }
                    }
                }
                break;
            }

            case REWARD_GROUP: {
                /* Revoke group skills — remove this class as source for each */
                SKILL_GROUP *group = skill_group_find(reward->name);
                if (group) {
                    ITERATOR git;
                    char *skill_name;
                    iterator_start(&git, group->contents);
                    while ((skill_name = (char *)iterator_nextdata(&git))) {
                        int sn = skill_lookup(skill_name);
                        if (sn >= 0 && sn < MAX_SKILL) {
                            SKILL_ENTRY *entry = skill_entry_findsn(ch->sorted_skills, sn);
                            if (entry) {
                                bool has_other = skill_entry_remove_source(entry, clazz);
                                if (!has_other) {
                                    skill_entry_remove(&ch->sorted_skills, sn, NULL, NULL,
                                                       entry->isspell);
                                }
                            }
                        }
                    }
                    iterator_stop(&git);
                }
                break;
            }

            default:
                /* Other revocable reward types — titles, tokens, etc.
                 * handled when the type-specific integration is complete */
                break;
        }
    }
    iterator_stop(&it);
}

/***************************************************************************
 * Skill Source Rebuild                                                     *
 ***************************************************************************/

/**
 * rebuild_skill_sources - Populate skill source metadata from class rewards
 *
 * Called at login to ensure all SKILL_ENTRY nodes have correct SKILL_SOURCE
 * data reflecting which classes grant them and at what scope. This handles
 * characters loaded from JSON files that predate the multi-source system
 * (where class_sources was not yet saved), and also ensures cross-class
 * changes made while a character was offline are reflected.
 *
 * Only adds sources to skills the character already has — does not grant
 * new skills or produce any output.
 *
 * @param ch  The character whose skill sources to rebuild
 */
void rebuild_skill_sources(CHAR_DATA *ch)
{
    ITERATOR it;
    CLASS_LEVEL *cl;

    if (!ch || IS_NPC(ch) || !ch->pcdata || !ch->pcdata->classes)
        return;

    /* Iterate every class the character has joined */
    iterator_start(&it, ch->pcdata->classes);
    while ((cl = (CLASS_LEVEL *)iterator_nextdata(&it))) {
        ITERATOR rit;
        CLASS_REWARD *reward;

        if (!cl->clazz || !cl->clazz->rewards)
            continue;

        iterator_start(&rit, cl->clazz->rewards);
        while ((reward = (CLASS_REWARD *)iterator_nextdata(&rit))) {
            if (reward->level > cl->level)
                continue;

            if (reward->type == REWARD_SKILL) {
                /* Direct skill reward */
                int sn = skill_lookup(reward->name);
                if (sn >= 0 && sn < MAX_SKILL) {
                    SKILL_ENTRY *entry = skill_entry_findsn(ch->sorted_skills, sn);
                    if (entry)
                        skill_entry_add_source(entry, cl->clazz, reward->scope);
                }
            } else if (reward->type == REWARD_GROUP) {
                /* Group reward — iterate group contents */
                SKILL_GROUP *group = skill_group_find(reward->name);
                if (group && group->contents) {
                    ITERATOR git;
                    char *skill_name;
                    iterator_start(&git, group->contents);
                    while ((skill_name = (char *)iterator_nextdata(&git))) {
                        int sn = skill_lookup(skill_name);
                        if (sn >= 0 && sn < MAX_SKILL) {
                            SKILL_ENTRY *entry = skill_entry_findsn(ch->sorted_skills, sn);
                            if (entry)
                                skill_entry_add_source(entry, cl->clazz, reward->scope);
                        }
                    }
                    iterator_stop(&git);
                }
            }
        }
        iterator_stop(&rit);
    }
    iterator_stop(&it);
}

/***************************************************************************
 * Skill Scope Checking                                                    *
 ***************************************************************************/

/**
 * is_skill_available_for_class - Check if a skill is usable with current class
 *
 * Checks ALL sources on the SKILL_ENTRY against the character's current active
 * class. A skill is available if ANY source's scope makes it usable.
 *
 * Skills with no sources (non-class-granted, e.g., racial or token) are always
 * available.
 *
 * @param ch     The character
 * @param entry  The skill entry to check
 * @return       true if the skill is available with the current class
 */
bool is_skill_available_for_class(CHAR_DATA *ch, SKILL_ENTRY *entry)
{
    CLASS_DATA *current;
    SKILL_SOURCE *src;

    if (!ch || !entry)
        return false;

    /* Non-class-granted skills are always available */
    if (!entry->sources && !entry->source_class)
        return true;

    /* Token-granted skills are always available */
    if (entry->token)
        return true;

    current = get_current_class(ch);

    /* Check each source — available if ANY source matches */
    for (src = entry->sources; src; src = src->next) {
        if (!src->clazz)
            continue;

        switch (src->scope) {
            case REWARD_SCOPE_ALWAYS:
                return true;

            case REWARD_SCOPE_COMBAT:
                if (IS_SET(src->clazz->flags, CLASS_COMBATIVE)
                    && current && IS_SET(current->flags, CLASS_COMBATIVE))
                    return true;
                break;

            case REWARD_SCOPE_TYPE:
                if (current && src->clazz->type == current->type)
                    return true;
                break;

            case REWARD_SCOPE_CLASS:
            default:
                if (current && src->clazz == current)
                    return true;
                break;
        }
    }

    /* Fallback: check legacy single-source fields if no sources list */
    if (!entry->sources && entry->source_class) {
        if (!current)
            return (entry->cross_class_scope == REWARD_SCOPE_ALWAYS);

        switch (entry->cross_class_scope) {
            case REWARD_SCOPE_ALWAYS:
                return true;
            case REWARD_SCOPE_COMBAT:
                return IS_SET(entry->source_class->flags, CLASS_COMBATIVE)
                    && IS_SET(current->flags, CLASS_COMBATIVE);
            case REWARD_SCOPE_TYPE:
                return (entry->source_class->type == current->type);
            case REWARD_SCOPE_CLASS:
            default:
                return (entry->source_class == current);
        }
    }

    return false;
}

/***************************************************************************
 * Player Commands                                                         *
 ***************************************************************************/

/**
 * do_setclass - Switch the character's active class
 *
 * Allows players to change their active class to one they have already
 * joined. Triggers leave/enter callbacks, revokes applicable rewards
 * from the old class, and applies rewards from the new class. Unequips
 * gear that the character can no longer use.
 *
 * Syntax: setclass <class name>
 */
void do_setclass(CHAR_DATA *ch, char *argument)
{
    CLASS_LEVEL *cl;
    ITERATOR it;
    OBJ_DATA *obj, *obj_next;

    if (IS_NPC(ch)) {
        send_to_char("Only players can switch classes.\n\r", ch);
        return;
    }

    /* Can't change from a combat class in combat */
    if (ch->fighting != NULL && is_current_class_combat(ch)) {
        send_to_char("You can't switch classes while fighting!\n\r", ch);
        return;
    }

    if (argument[0] == '\0') {
        send_to_char("Syntax: setclass <class name>\n\r", ch);
        send_to_char("Use 'classes' to see your available classes.\n\r", ch);
        return;
    }

    /* Find the class level by name match */
    cl = NULL;
    iterator_start(&it, ch->pcdata->classes);
    while ((cl = (CLASS_LEVEL *)iterator_nextdata(&it))) {
        if (cl->clazz && !str_prefix(argument, class_display_ch(cl->clazz, ch)))
            break;
        if (cl->clazz && !str_prefix(argument, cl->clazz->name))
            break;
    }
    iterator_stop(&it);

    if (!cl || !cl->clazz) {
        send_to_char("You don't have that class.\n\r", ch);
        return;
    }

    if (ch->pcdata->current_class == cl) {
        send_to_char("You are already in that class.\n\r", ch);
        return;
    }

    /* Leave old class */
    if (ch->pcdata->current_class && ch->pcdata->current_class->clazz) {
        CLASS_DATA *old_class = ch->pcdata->current_class->clazz;

        /* Call leave callback */
        if (old_class->leave)
            (*old_class->leave)(ch);

        /* Revoke applicable rewards */
        revoke_class_rewards(ch, old_class);
    }

    /* Switch */
    ch->pcdata->current_class = cl;

    {
        char buf[MAX_STRING_LENGTH];
        sprintf(buf, "{MYou switch to {W%s{M.{x\n\r",
                class_display_ch(cl->clazz, ch));
        send_to_char(buf, ch);

        sprintf(buf, "$n switches to %s.", class_display_ch(cl->clazz, ch));
        act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    }

    /* Call enter callback */
    if (cl->clazz->enter)
        (*cl->clazz->enter)(ch);

    /* Apply rewards for the new class (on_join = true, so already-applied get no message) */
    apply_class_rewards(ch, cl->clazz, 1, cl->level, true);

    /* Unequip gear that's too high level for the new class */
    for (obj = ch->carrying; obj != NULL; obj = obj_next) {
        obj_next = obj->next_content;

        if (obj->wear_loc != WEAR_NONE && cl->level < obj->level) {
            act("You remove $p as you can no longer use it.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            unequip_char(ch, obj, true);
        }
    }

    save_char_obj(ch);
}

/**
 * do_classes - List the character's current class levels
 *
 * Shows all classes the character has joined, their levels, XP, and
 * marks the currently active class.
 *
 * Syntax: classes
 */
void do_classes(CHAR_DATA *ch, char *argument)
{
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];
    ITERATOR it;
    CLASS_LEVEL *cl;
    int i = 0;

    if (IS_NPC(ch)) {
        send_to_char("Only players have classes.\n\r", ch);
        return;
    }

    if (!ch->pcdata->classes || list_size(ch->pcdata->classes) == 0) {
        send_to_char("You have no classes.\n\r", ch);
        return;
    }

    buffer = new_buf();

    add_buf(buffer, "{C  # Active  Class                Level  Type{x\n\r");
    add_buf(buffer, "{C  = ======  ===================  =====  ===================={x\n\r");

    iterator_start(&it, ch->pcdata->classes);
    while ((cl = (CLASS_LEVEL *)iterator_nextdata(&it))) {
        if (!cl->clazz)
            continue;

        i++;
        const char *type_name = "unknown";
        if (cl->clazz->type >= 0 && cl->clazz->type < MAX_CLASS_TYPE) {
            int t;
            for (t = 0; class_types[t].name; t++) {
                if (class_types[t].bit == cl->clazz->type) {
                    type_name = class_types[t].name;
                    break;
                }
            }
        }

        sprintf(buf, "  {W%d{x {Y%-6s{x  %-19s  {G%-5d{x  %s\n\r",
                i,
                (ch->pcdata->current_class == cl) ? "{Y*{x     " : "      ",
                class_display_ch(cl->clazz, ch),
                cl->level,
                type_name);
        add_buf(buffer, buf);
    }
    iterator_stop(&it);

    add_buf(buffer, "{C  = ======  ===================  =====  ===================={x\n\r");
    sprintf(buf, "  Total classes: %d\n\r", i);
    add_buf(buffer, buf);

    page_to_char(buffer->string, ch);
    free_buf(buffer);
}

/**
 * do_clslist - Staff command to list all defined classes
 *
 * Shows all loaded CLASS_DATA entries with their UIDs, types, and flags.
 *
 * Syntax: clslist
 */
void do_clslist(CHAR_DATA *ch, char *argument)
{
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];
    CLASS_DATA *clazz;
    int i = 0;

    buffer = new_buf();

    sprintf(buf, "{C%-4s %-20s %-12s %-10s %-5s{x\n\r",
            "#", "Name", "Type", "Flags", "Max");
    add_buf(buffer, buf);
    sprintf(buf, "{C%-4s %-20s %-12s %-10s %-5s{x\n\r",
            "====", "====================", "============", "==========", "=====");
    add_buf(buffer, buf);

    for (clazz = class_first(); clazz; clazz = clazz->next) {
        i++;
        const char *type_name = "unknown";
        int t;
        for (t = 0; class_types[t].name; t++) {
            if (class_types[t].bit == clazz->type) {
                type_name = class_types[t].name;
                break;
            }
        }

        char flag_str[64] = "";
        if (IS_SET(clazz->flags, CLASS_COMBATIVE))  strcat(flag_str, "Cmb ");
        if (IS_SET(clazz->flags, CLASS_CASTER))      strcat(flag_str, "Cst ");
        if (IS_SET(clazz->flags, CLASS_REMORT_ONLY))  strcat(flag_str, "Rmt ");
        if (IS_SET(clazz->flags, CLASS_HIDDEN))       strcat(flag_str, "Hid ");
        if (IS_SET(clazz->flags, CLASS_DEFAULT))      strcat(flag_str, "Def ");
        if (IS_SET(clazz->flags, CLASS_NO_LEVEL))     strcat(flag_str, "NLv ");

        sprintf(buf, "{W%-4d{x %-20s %-12s %-10s {G%-5d{x\n\r",
                clazz->uid, clazz->name, type_name, flag_str, clazz->max_level);
        add_buf(buffer, buf);
    }

    sprintf(buf, "{C%-4s %-20s %-12s %-10s %-5s{x\n\r",
            "====", "====================", "============", "==========", "=====");
    add_buf(buffer, buf);
    sprintf(buf, "Total: %d classes\n\r", i);
    add_buf(buffer, buf);

    page_to_char(buffer->string, ch);
    free_buf(buffer);
}

/**
 * do_freelevel - Spend free levels on a class
 *
 * Allows a character to spend accumulated free levels (from class
 * max_level overflow) to gain a level in a class.
 *
 * Syntax: freelevel <class name>
 */
void do_freelevel(CHAR_DATA *ch, char *argument)
{
    CLASS_LEVEL *cl;
    ITERATOR it;

    if (IS_NPC(ch)) {
        send_to_char("Only players can use free levels.\n\r", ch);
        return;
    }

    if (ch->pcdata->pending_free_levels <= 0) {
        send_to_char("You have no free levels to spend.\n\r", ch);
        return;
    }

    if (argument[0] == '\0') {
        char buf[MAX_STRING_LENGTH];
        sprintf(buf, "You have %d free level%s available.\n\r"
                     "Syntax: freelevel <class name>\n\r",
                ch->pcdata->pending_free_levels,
                ch->pcdata->pending_free_levels == 1 ? "" : "s");
        send_to_char(buf, ch);
        return;
    }

    /* Find class level */
    cl = NULL;
    iterator_start(&it, ch->pcdata->classes);
    while ((cl = (CLASS_LEVEL *)iterator_nextdata(&it))) {
        if (cl->clazz && !str_prefix(argument, class_display_ch(cl->clazz, ch)))
            break;
        if (cl->clazz && !str_prefix(argument, cl->clazz->name))
            break;
    }
    iterator_stop(&it);

    if (!cl || !cl->clazz) {
        send_to_char("You don't have that class.\n\r", ch);
        return;
    }

    if (cl->level >= cl->clazz->max_level) {
        char buf[MAX_STRING_LENGTH];
        sprintf(buf, "You are already at the maximum level (%d) in %s.\n\r",
                cl->clazz->max_level, class_display_ch(cl->clazz, ch));
        send_to_char(buf, ch);
        return;
    }

    /* Apply the free level */
    cl->level++;
    ch->pcdata->pending_free_levels--;

    /* Update tot_level if class allows it */
    if (!IS_SET(cl->clazz->flags, CLASS_NO_LEVEL))
        ch->tot_level++;

    {
        char buf[MAX_STRING_LENGTH];
        sprintf(buf, "{MYou spend a free level and advance to level {W%d{M in {W%s{M!{x\n\r"
                     "You have {W%d{M free level%s remaining.{x\n\r",
                cl->level, class_display_ch(cl->clazz, ch),
                ch->pcdata->pending_free_levels,
                ch->pcdata->pending_free_levels == 1 ? "" : "s");
        send_to_char(buf, ch);
    }

    /* Apply rewards for the new level */
    apply_class_rewards(ch, cl->clazz, cl->level, cl->level, false);

    save_char_obj(ch);
}

/**
 * do_classinfo - Display detailed information about a class
 *
 * Shows the class description, type, key stats, and links to help files.
 * Players can view info about any class, not just ones they hold.
 *
 * Syntax: classinfo <class name>
 */
void do_classinfo(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CLASS_DATA *clazz;
    BUFFER *buffer;

    one_argument(argument, arg);

    if (arg[0] == '\0') {
        send_to_char("Syntax: classinfo <class name>\n\r", ch);
        return;
    }

    clazz = class_find(arg);
    if (!clazz) {
        send_to_char("No such class found.\n\r", ch);
        return;
    }

    /* Hide hidden classes from mortals */
    if (IS_SET(clazz->flags, CLASS_HIDDEN) && !IS_IMMORTAL(ch)) {
        send_to_char("No such class found.\n\r", ch);
        return;
    }

    buffer = new_buf();

    /* Header */
    add_buf(buffer, formatf("{C=== Class: {W%s{C ==={x\n\r",
                            class_display_ch(clazz, ch)));

    /* Description */
    if (!IS_NULLSTR(clazz->description)) {
        add_buf(buffer, formatf("\n\r%s{x\n\r", clazz->description));
    }

    add_buf(buffer, "\n\r");

    /* Type */
    const char *type_name = "unknown";
    int t;
    for (t = 0; class_types[t].name; t++) {
        if (class_types[t].bit == clazz->type) {
            type_name = class_types[t].name;
            break;
        }
    }
    add_buf(buffer, formatf("{xType:        {W%s{x\n\r", type_name));

    /* Max level */
    add_buf(buffer, formatf("{xMax level:   {G%d{x\n\r", clazz->max_level));

    /* Primary stat */
    if (clazz->primary_stat >= 0 && clazz->primary_stat < MAX_STATS) {
        static const char *stat_names[] = {
            "Strength", "Intelligence", "Wisdom", "Dexterity", "Constitution"
        };
        add_buf(buffer, formatf("{xPrimary stat:{W %s{x\n\r",
                                stat_names[clazz->primary_stat]));
    }

    /* Flags */
    char flag_str[256] = "";
    if (IS_SET(clazz->flags, CLASS_COMBATIVE))   strcat(flag_str, "Combative ");
    if (IS_SET(clazz->flags, CLASS_CASTER))       strcat(flag_str, "Caster ");
    if (IS_SET(clazz->flags, CLASS_REMORT_ONLY))  strcat(flag_str, "Remort ");
    if (flag_str[0])
        add_buf(buffer, formatf("{xTraits:      {Y%s{x\n\r", flag_str));

    /* HP gain */
    if (clazz->hp_min > 0 || clazz->hp_max > 0)
        add_buf(buffer, formatf("{xHP per level:{G %d-%d{x\n\r", clazz->hp_min, clazz->hp_max));

    /* Mana */
    add_buf(buffer, formatf("{xGains mana:  %s\n\r",
                            clazz->gains_mana ? "{GYes{x" : "{DNo{x"));

    /* Player's relationship to this class */
    if (!IS_NPC(ch) && ch->pcdata->classes) {
        CLASS_LEVEL *cl;
        ITERATOR it;
        bool found = false;
        iterator_start(&it, ch->pcdata->classes);
        while ((cl = (CLASS_LEVEL *)iterator_nextdata(&it))) {
            if (cl->clazz == clazz) {
                found = true;
                add_buf(buffer, formatf("\n\r{xYour level:  {G%d{x%s\n\r",
                                        cl->level,
                                        (ch->pcdata->current_class == cl)
                                            ? " {Y(active){x" : ""));
                break;
            }
        }
        iterator_stop(&it);
        if (!found)
            add_buf(buffer, "\n\r{xYou are not currently in this class.{x\n\r");
    }

    /* Help file link */
    HELP_DATA *help = lookup_help_exact(clazz->name, get_staff_rank(ch), topHelpCat);
    if (help) {
        add_buf(buffer, formatf("\n\r{xHelp:        \t<send href=\"help #%d\">{Whelp %s{x\t</send>\n\r",
                                help->index, clazz->name));
    }

    page_to_char(buffer->string, ch);
    free_buf(buffer);
}
