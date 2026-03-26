/***************************************************************************
 *  Skill Group System - Public API                                        *
 *                                                                         *
 *  Named collections of skills used by the class reward system to grant   *
 *  batches of skills at once via REWARD_GROUP. Loaded from JSON files in  *
 *  data/skill_groups/.                                                    *
 ***************************************************************************/

#ifndef SKILL_GROUP_H
#define SKILL_GROUP_H

/* Forward declaration — full struct is in merc.h */
typedef struct skill_group_data SKILL_GROUP;

/***************************************************************************
 * Constants                                                               *
 ***************************************************************************/

#define SKILL_GROUP_HASH_SIZE   64
#define SKILL_GROUPS_DIR        DATA_DIR "skill_groups/"

/***************************************************************************
 * Lookup API                                                              *
 ***************************************************************************/

/* Primary lookups — return NULL if not found */
SKILL_GROUP *   skill_group_find(const char *name);         /* Exact match (case-insensitive) */
SKILL_GROUP *   skill_group_search(const char *prefix);     /* Prefix match (case-insensitive) */

/* Global iteration */
SKILL_GROUP *   skill_group_first(void);                    /* First in alphabetical list */
int             skill_group_count(void);                    /* Total loaded groups */

/***************************************************************************
 * Boot / Persistence                                                      *
 ***************************************************************************/

/* Load all groups from JSON files */
void            load_skill_groups(void);

/* Save a single group to its JSON file */
void            save_skill_group(SKILL_GROUP *group);

/* Save all groups to JSON files */
void            save_all_skill_groups(void);

/***************************************************************************
 * Memory Management                                                       *
 ***************************************************************************/

SKILL_GROUP *   new_skill_group(void);
void            free_skill_group(SKILL_GROUP *group);

#endif /* SKILL_GROUP_H */
