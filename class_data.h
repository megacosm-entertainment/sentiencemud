/***************************************************************************
 *  Class Data System - Public API                                         *
 *                                                                         *
 *  Data-driven class definitions loaded from JSON files.                  *
 *  Replaces the legacy static class_table[] and sub_class_table[] arrays. *
 ***************************************************************************/

#ifndef CLASS_DATA_H
#define CLASS_DATA_H

#include "tables.h"

/* Forward declarations — full structs are in merc.h */
typedef struct class_data CLASS_DATA;
typedef struct class_level CLASS_LEVEL;
typedef struct class_reward CLASS_REWARD;
typedef struct class_title CLASS_TITLE;

/***************************************************************************
 * Constants                                                               *
 ***************************************************************************/

#define CLASS_HASH_SIZE         64
#define CLASSES_DIR             DATA_DIR "classes/"

/* Class type categories — broader than the original 4 base classes */
#define CLASS_TYPE_NONE         -1
#define CLASS_TYPE_MAGE          0      /* Arcane caster */
#define CLASS_TYPE_CLERIC        1      /* Divine caster */
#define CLASS_TYPE_THIEF         2      /* Stealth/agility */
#define CLASS_TYPE_WARRIOR       3      /* Melee combat */
#define CLASS_TYPE_CRAFTING      4      /* Item creation (smithing, alchemy, etc.) */
#define CLASS_TYPE_GATHERING     5      /* Resource collection (mining, herbalism) */
#define CLASS_TYPE_EXPLORER      6      /* Discovery/navigation */
#define MAX_CLASS_TYPE           7

/* Class flags (bitfield) */
#define CLASS_COMBATIVE         (A)    /* Class participates in combat */
#define CLASS_NO_LEVEL          (B)    /* Levels do NOT count toward tot_level */
#define CLASS_CASTER            (C)    /* Class uses mana */
#define CLASS_HIDDEN            (D)    /* Not shown in class lists by default */
#define CLASS_REMORT_ONLY       (E)    /* Requires remort to access */
#define CLASS_DEFAULT           (F)    /* Auto-assigned to new characters on creation */

/* Reward type constants — what a class grants at a given level.
 * NOTE: Values start at 1, not 0, because flag_value()/flag_lookup()
 * treat 0 as "not found." */
#define REWARD_SKILL            1      /* Grant access to a skill at a given rating */
#define REWARD_GROUP            2      /* Grant all skills in a skill group */
#define REWARD_TITLE            3      /* Change class display/who name at this level */
#define REWARD_BONUS            4      /* Grant a stat or attribute bonus */
#define REWARD_TOKEN            5      /* Grant a token to the character */
#define REWARD_SCRIPT           6      /* Execute a script against the character */
#define REWARD_CUSTOM           7      /* Write to CLASS_LEVEL.custom_data */
#define REWARD_TRAIT            8      /* Grant or override a trait value */
#define REWARD_SONG             9      /* Grant a song to the character */
#define MAX_REWARD_TYPE         10

/* Reward flags (bitfield) */
#define REWARD_REVOKE_ON_LEAVE  (A)    /* Revoked when leaving this class */
#define REWARD_ONE_TIME         (B)    /* Only triggers once (not re-applied on reload) */
#define REWARD_HIDDEN           (C)    /* Not shown in class level list */
#define REWARD_SONG_UNLOCK      (D)    /* Song reward: unlock for rehearsal only (not direct grant) */

/* Reward scope — controls cross-class availability of granted skills/perks */
#define REWARD_SCOPE_CLASS      0      /* Only when the granting class is active */
#define REWARD_SCOPE_TYPE       1      /* Any class of the same CLASS_TYPE_* */
#define REWARD_SCOPE_COMBAT     2      /* Any class with CLASS_COMBATIVE flag */
#define REWARD_SCOPE_ALWAYS     3      /* Always available regardless of active class */
#define MAX_REWARD_SCOPE        4

/***************************************************************************
 * Callback typedefs                                                       *
 ***************************************************************************/

typedef void CLASS_ENTER_FUN(CHAR_DATA *ch);
typedef void CLASS_LEAVE_FUN(CHAR_DATA *ch);

/***************************************************************************
 * Lookup API                                                              *
 ***************************************************************************/

/* Primary lookups — return NULL if not found */
CLASS_DATA *    class_find(const char *name);            /* Prefix match (case-insensitive) */
CLASS_DATA *    class_find_exact(const char *name);      /* Exact match (case-insensitive) */
CLASS_DATA *    class_find_uid(int16_t uid);             /* By UID (for deserialization) */

/* Convenience */
const char *    class_name(CLASS_DATA *clazz);           /* Returns name, or "none" if NULL */
const char *    class_display(CLASS_DATA *clazz, int body_type);
const char *    class_who(CLASS_DATA *clazz, int body_type);
const char *    class_display_ch(CLASS_DATA *clazz, CHAR_DATA *ch);  /* Uses ch->body_type */
const char *    class_who_ch(CLASS_DATA *clazz, CHAR_DATA *ch);      /* Uses ch->body_type */

/* Title API */
CLASS_TITLE *   class_find_title(CLASS_DATA *clazz, const char *keyword);  /* By keyword */
CLASS_TITLE *   class_get_default_title(CLASS_DATA *clazz);               /* First is_default */
const char *    class_title_display(CLASS_DATA *clazz, CLASS_LEVEL *cl);  /* Active or default display */
const char *    class_title_who(CLASS_DATA *clazz, CLASS_LEVEL *cl);      /* Active or default who name */

/* Global iteration */
CLASS_DATA *    class_first(void);                       /* First in alphabetical global list */
int             class_count(void);                       /* Total loaded class count */
CLASS_DATA *    class_get_default(void);                 /* First class with CLASS_DEFAULT flag */

/***************************************************************************
 * Character Class API                                                     *
 ***************************************************************************/

/* Current class */
CLASS_DATA *    get_current_class(CHAR_DATA *ch);

/* Class level access */
CLASS_LEVEL *   get_class_level(CHAR_DATA *ch, CLASS_DATA *clazz);  /* NULL clazz = current */
bool            has_class_level(CHAR_DATA *ch, CLASS_DATA *clazz);
void            add_class_level(CHAR_DATA *ch, CLASS_DATA *clazz, int level);
void            remove_class_level(CHAR_DATA *ch, CLASS_DATA *clazz);
void            insert_class_level(CHAR_DATA *ch, CLASS_LEVEL *cl);

/* Queries */
bool            is_current_class_combat(CHAR_DATA *ch);
long            class_exp_per_level(CLASS_DATA *clazz, int level);
const long *    class_default_xp_table(int *out_size);

/***************************************************************************
 * Compatibility / Migration API                                           *
 ***************************************************************************/

/* Map legacy class index (0-3) + sub_class index to CLASS_DATA */
CLASS_DATA *    class_from_legacy(int class_idx, int sub_class_idx);
const char *    class_name_from_legacy(int class_idx);
int             class_legacy_index(CLASS_DATA *clazz);
int             sub_class_legacy_index(CLASS_DATA *clazz);
int             sub_class_legacy_type(int sub_class_idx);
int             sub_class_legacy_alignment(int sub_class_idx);
bool            sub_class_legacy_is_remort(int sub_class_idx);
bool            sub_class_legacy_prereq_match(int sub_class_idx, int profession);

/*
 * CLASS_CACHED — File-local cached class pointer.
 *
 * Provides O(1) access after first lookup, replacing gcl_* globals.
 * Usage:
 *   CLASS_CACHED(cls_paladin, "paladin");
 *   if (clazz == cls_paladin) { ... }
 */
#define CLASS_CACHED(var, name) \
    static CLASS_DATA *var = NULL; \
    if (!(var)) (var) = class_find_exact(name)

/***************************************************************************
 * Boot / Persistence                                                      *
 ***************************************************************************/

/* Load all classes from JSON files (with bootstrap_data seeding fallback) */
void            load_class_data(void);

/* Save a single class to its JSON file */
void            save_class_data(CLASS_DATA *clazz);

/* Save all classes to JSON files */
void            save_all_class_data(void);

/* Reload a single class from its JSON file (in-place if exists, new if not) */
CLASS_DATA *    class_reload(const char *name);

/***************************************************************************
 * Reward System API                                                       *
 ***************************************************************************/

/* Apply class rewards to a character for a level range.
 * on_join=true suppresses messages for already-applied rewards. */
void            apply_class_rewards(CHAR_DATA *ch, CLASS_DATA *clazz,
                    int from_level, int to_level, bool on_join);

/* Revoke rewards with REWARD_REVOKE_ON_LEAVE flag */
void            revoke_class_rewards(CHAR_DATA *ch, CLASS_DATA *clazz);

/* Rebuild skill source metadata from class rewards (called at login) */
void            rebuild_skill_sources(CHAR_DATA *ch);

/* Check if a skill entry is usable with the character's current class */
bool            is_skill_available_for_class(CHAR_DATA *ch, SKILL_ENTRY *entry);

/* Check if a specific skill entry is currently usable right now */
bool            skill_entry_is_usable_now(CHAR_DATA *ch, SKILL_ENTRY *entry);

/* Check if a skill number is currently usable right now */
bool            skill_is_usable_now(CHAR_DATA *ch, int sn);

/* Check if a class grants a specific skill via its rewards */
bool            class_grants_skill(CLASS_DATA *clazz, int sn);

/* Check if any of a character's classes grant a specific skill */
bool            any_class_grants_skill(CHAR_DATA *ch, int sn);

/***************************************************************************
 * Memory Management                                                       *
 ***************************************************************************/

CLASS_DATA *    new_class_data(void);
void            free_class_data(CLASS_DATA *data);
CLASS_LEVEL *   new_class_level(void);
void            free_class_level(CLASS_LEVEL *cl);
CLASS_REWARD *  new_class_reward(void);
void            free_class_reward(CLASS_REWARD *reward);
CLASS_TITLE *   new_class_title(void);
void            free_class_title(CLASS_TITLE *title);

/***************************************************************************
 * Flag / Type Tables (for OLC, serialization)                             *
 ***************************************************************************/

extern const struct flag_type class_types[];
extern const struct flag_type class_flags[];
extern const struct flag_type reward_types[];
extern const struct flag_type reward_flags[];
extern const struct flag_type reward_scopes[];

#endif /* CLASS_DATA_H */
