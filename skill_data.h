/***************************************************************************
 *  Skill Data System - Public API                                         *
 *                                                                         *
 *  Data-driven skill/spell definitions loaded from JSON files.            *
 *  Replaces the legacy static skill_table[] array and gsn_* globals.      *
 ***************************************************************************/

#ifndef SKILL_DATA_H
#define SKILL_DATA_H

/* Forward declarations — full structs are in merc.h */
typedef struct skill_data SKILL_DATA;

/***************************************************************************
 * Constants                                                               *
 ***************************************************************************/

#define SKILL_HASH_SIZE         256
#define SKILLS_DIR              DATA_DIR "skills/"

/* Skill flags */
#define SKILLFLAG_NONE          0
#define SKILLFLAG_RACIAL        (A)     /* Racial skill */
#define SKILLFLAG_REMORT        (B)     /* Remort-only */
#define SKILLFLAG_NO_PRACTICE   (C)     /* Cannot be practiced */
#define SKILLFLAG_NO_IMPROVE    (D)     /* Cannot improve through use */
#define SKILLFLAG_PASSIVE       (E)     /* Passive skill (no invocation) */
#define SKILLFLAG_TOKEN_DRIVEN  (F)     /* Implementation is via token script, not spell_fun */

/* Spell invocation methods — passed to SPELL_FUN */
#define INVOC_CAST              0       /* Normal spellcasting */
#define INVOC_QUAFF             1       /* Drinking a potion */
#define INVOC_RECITE            2       /* Reading a scroll */
#define INVOC_BRANDISH          3       /* Brandishing a staff */
#define INVOC_ZAP               4       /* Zapping with a wand */
#define INVOC_BREW              5       /* Brewing into a potion */
#define INVOC_SCRIBE            6       /* Scribing onto a scroll */
#define INVOC_INK               7       /* Tattooing */
#define INVOC_IMBUE             8       /* Imbuing an item */
#define INVOC_EQUIP             9       /* Equipment-triggered effect */
#define INVOC_TOUCH             10      /* Contact/touch effect */
#define INVOC_TOKEN             11      /* Token script trigger */
#define INVOC_INTERNAL          12      /* Internal/system call */
#define MAX_INVOC               13

/***************************************************************************
 * Lookup API                                                              *
 ***************************************************************************/

/* Primary lookups — return NULL if not found */
SKILL_DATA *    skill_find(const char *name);           /* Exact match by name (case-insensitive) */
SKILL_DATA *    skill_search(const char *prefix);       /* Prefix match (like old skill_lookup) */
SKILL_DATA *    skill_find_uid(int16_t uid);            /* By UID (for deserialization/affects) */

/* Convenience */
const char *    skill_name(SKILL_DATA *skill);          /* Returns name, or "none" if NULL */

/* Global iteration */
SKILL_DATA *    skill_first(void);                      /* First in alphabetical global list */
int             skill_count(void);                      /* Total loaded skill count */

/***************************************************************************
 * Compatibility / Migration API                                           *
 ***************************************************************************/

/* Map old sn (skill_table index) to new SKILL_DATA pointer
 * During migration, the bootstrap assigns uid = original sn index,
 * so this is equivalent to skill_find_uid(sn) */
SKILL_DATA *    skill_from_sn(int sn);

/* Reverse: get the UID (which matches old sn after bootstrap) */
int16_t         skill_sn(SKILL_DATA *skill);

/* Resolve a gsn by name — returns uid for backward compatibility with gsn_ patterns */
int16_t         skill_resolve_gsn(const char *name);

/*
 * SKILL_CACHED — File-local cached skill pointer.
 *
 * Provides O(1) access after first lookup, replacing gsn_ globals.
 * Usage:
 *   SKILL_CACHED(sk_backstab, "backstab");
 *   if (skill == sk_backstab) { ... }
 */
#define SKILL_CACHED(var, name) \
    static SKILL_DATA *var = NULL; \
    if (!(var)) (var) = skill_find(name)

/***************************************************************************
 * SPELL_FUN Name<->Pointer Resolution                                    *
 ***************************************************************************/

/* Resolve a function name string to a SPELL_FUN pointer */
SPELL_FUN *     spell_fun_lookup(const char *name);

/* Resolve a SPELL_FUN pointer to its function name string */
const char *    spell_fun_name(SPELL_FUN *fun);

/***************************************************************************
 * Boot / Persistence                                                      *
 ***************************************************************************/

/* Load all skills from JSON files (or bootstrap from skill_table on first run) */
void            load_skill_data(void);

/* Save a single skill to its JSON file */
void            save_skill_data(SKILL_DATA *skill);

/* Save all skills to JSON files */
void            save_all_skill_data(void);

/***************************************************************************
 * Memory Management                                                       *
 ***************************************************************************/

SKILL_DATA *         new_skill_data(void);

#endif /* SKILL_DATA_H */
