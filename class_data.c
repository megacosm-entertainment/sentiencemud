/***************************************************************************
 *  Class Data System - Implementation                                     *
 *                                                                         *
 *  Data-driven class backend. Loads class definitions from JSON files     *
 *  in data/classes/, or bootstraps from the legacy sub_class_table[]      *
 *  on first run.                                                          *
 *                                                                         *
 *  Provides O(1) hash table lookup by name and UID index for fast access. *
 ***************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <jansson.h>
#include "merc.h"
#include "tables.h"
#include "class_data.h"
#include "skill_data.h"
#include "skill_group.h"
#include "traits.h"

/***************************************************************************
 * Globals                                                                 *
 ***************************************************************************/

static CLASS_DATA *     class_list = NULL;      /* Alphabetically sorted linked list */
static int              class_total = 0;        /* Total loaded classes */
static int16_t          top_class_uid = 0;      /* Highest assigned UID */

/* Hash table for O(1) name lookup */
typedef struct class_hash_entry {
    char *                  key;
    CLASS_DATA *            value;
    struct class_hash_entry *next;
} CLASS_HASH_ENTRY;

static CLASS_HASH_ENTRY *class_hash_tbl[CLASS_HASH_SIZE];

/* UID index for O(1) lookup by UID */
static CLASS_DATA **    class_uid_index = NULL;
static int              max_class_uid = 0;

/***************************************************************************
 * Flag / Type Tables                                                      *
 ***************************************************************************/

/**
 * class_types - Maps class type names to CLASS_TYPE_* constants.
 */
const struct flag_type class_types[] = {
    { "none",       CLASS_TYPE_NONE,      false, NULL },
    { "mage",       CLASS_TYPE_MAGE,      true,  NULL },
    { "cleric",     CLASS_TYPE_CLERIC,    true,  NULL },
    { "thief",      CLASS_TYPE_THIEF,     true,  NULL },
    { "warrior",    CLASS_TYPE_WARRIOR,   true,  NULL },
    { "crafting",   CLASS_TYPE_CRAFTING,  true,  NULL },
    { "gathering",  CLASS_TYPE_GATHERING, true,  NULL },
    { "explorer",   CLASS_TYPE_EXPLORER,  true,  NULL },
    { NULL,         0,                    false, NULL }
};

/**
 * class_flags - Maps class flag names to CLASS_* bitfield constants.
 */
const struct flag_type class_flags[] = {
    { "combative",   CLASS_COMBATIVE,   true,  NULL },
    { "no_level",    CLASS_NO_LEVEL,    true,  NULL },
    { "caster",      CLASS_CASTER,      true,  NULL },
    { "hidden",      CLASS_HIDDEN,      true,  NULL },
    { "remort_only", CLASS_REMORT_ONLY, true,  NULL },
    { "default",     CLASS_DEFAULT,     true,  NULL },
    { NULL,          0,                 false, NULL }
};

/**
 * reward_types - Maps reward type names to REWARD_* constants.
 */
const struct flag_type reward_types[] = {
    { "skill",   REWARD_SKILL,   true,  "Grant access to a skill" },
    { "group",   REWARD_GROUP,   true,  "Grant all skills in a group" },
    { "title",   REWARD_TITLE,   true,  "Change class display/who name" },
    { "bonus",   REWARD_BONUS,   true,  "Grant a stat bonus" },
    { "token",   REWARD_TOKEN,   true,  "Grant a token" },
    { "script",  REWARD_SCRIPT,  true,  "Execute a script" },
    { "custom",  REWARD_CUSTOM,  true,  "Write to custom_data" },
    { "trait",   REWARD_TRAIT,   true,  "Grant or override a trait" },
    { "song",    REWARD_SONG,    true,  "Grant a song" },
    { NULL,      0,              false, NULL }
};

/**
 * reward_flags - Maps reward flag names to REWARD_* bitfield constants.
 */
const struct flag_type reward_flags[] = {
    { "revoke_on_leave", REWARD_REVOKE_ON_LEAVE, true,  "Revoked when leaving this class" },
    { "one_time",        REWARD_ONE_TIME,        true,  "Only triggers once" },
    { "hidden",          REWARD_HIDDEN,          true,  "Not shown in class level list" },
    { "song_unlock",     REWARD_SONG_UNLOCK,     true,  "Song: unlock for rehearsal only" },
    { NULL,              0,                      false, NULL }
};

/**
 * reward_scopes - Maps reward scope names to REWARD_SCOPE_* constants.
 */
const struct flag_type reward_scopes[] = {
    { "class",   REWARD_SCOPE_CLASS,   true,  "Only when granting class is active" },
    { "type",    REWARD_SCOPE_TYPE,    true,  "Any class of the same type" },
    { "combat",  REWARD_SCOPE_COMBAT,  true,  "Any combative class" },
    { "always",  REWARD_SCOPE_ALWAYS,  true,  "Always available" },
    { NULL,      0,                    false, NULL }
};

/***************************************************************************
 * Hash Function                                                           *
 ***************************************************************************/

/**
 * class_hash - FNV-1a hash of a class name, case-insensitive
 */
static unsigned int class_hash(const char *name)
{
    unsigned int hash = 2166136261u;

    while (*name) {
        hash ^= (unsigned char)LOWER(*name);
        name++;
        hash *= 16777619u;
    }
    return hash % CLASS_HASH_SIZE;
}

/**
 * class_hash_insert - Insert a CLASS_DATA into the hash table
 */
static void class_hash_insert(CLASS_DATA *clazz)
{
    unsigned int idx;
    CLASS_HASH_ENTRY *entry;

    if (!clazz || !clazz->name)
        return;

    idx = class_hash(clazz->name);
    entry = (CLASS_HASH_ENTRY *)alloc_perm(sizeof(CLASS_HASH_ENTRY));
    entry->key = clazz->name;
    entry->value = clazz;
    entry->next = class_hash_tbl[idx];
    class_hash_tbl[idx] = entry;
}

/***************************************************************************
 * Memory Management                                                       *
 ***************************************************************************/

/**
 * new_class_data - Allocate and initialize a new CLASS_DATA
 */
CLASS_DATA *new_class_data(void)
{
    CLASS_DATA *clazz;

    clazz = (CLASS_DATA *)alloc_perm(sizeof(CLASS_DATA));
    memset(clazz, 0, sizeof(CLASS_DATA));

    clazz->valid = true;
    clazz->uid = -1;
    clazz->type = CLASS_TYPE_NONE;
    clazz->max_level = MAX_CLASS_LEVEL;
    clazz->primary_stat = STAT_STR;
    clazz->groups = list_create(false);
    clazz->rewards = list_create(false);
    clazz->titles = list_create(false);

    /* Initialize trait values (may be NULL if traits not yet loaded) */
    class_init_traits(clazz);

    /* Initialize display/who arrays to empty strings */
    for (int i = 0; i < BODY_TYPE_MAX; i++) {
        clazz->display[i] = str_dup("");
        clazz->who[i] = str_dup("");
    }

    return clazz;
}

/**
 * free_class_data - Free a CLASS_DATA and its strings
 */
void free_class_data(CLASS_DATA *data)
{
    if (!data)
        return;

    data->valid = false;

    free_string(data->name);
    free_string(data->description);
    free_string(data->enter_fun_name);
    free_string(data->leave_fun_name);

    for (int i = 0; i < BODY_TYPE_MAX; i++) {
        free_string(data->display[i]);
        free_string(data->who[i]);
    }

    if (data->groups)
        list_destroy(data->groups);

    if (data->rewards) {
        ITERATOR it;
        CLASS_REWARD *reward;
        iterator_start(&it, data->rewards);
        while ((reward = (CLASS_REWARD *)iterator_nextdata(&it))) {
            free_class_reward(reward);
        }
        iterator_stop(&it);
        list_destroy(data->rewards);
    }

    if (data->titles) {
        ITERATOR it;
        CLASS_TITLE *title;
        iterator_start(&it, data->titles);
        while ((title = (CLASS_TITLE *)iterator_nextdata(&it))) {
            free_class_title(title);
        }
        iterator_stop(&it);
        list_destroy(data->titles);
    }

    /* Note: alloc_perm memory is not actually freed, but we clean up strings */
}

/**
 * new_class_level - Allocate and initialize a new CLASS_LEVEL
 */
CLASS_LEVEL *new_class_level(void)
{
    CLASS_LEVEL *cl;

    cl = (CLASS_LEVEL *)alloc_perm(sizeof(CLASS_LEVEL));
    memset(cl, 0, sizeof(CLASS_LEVEL));

    cl->valid = true;
    cl->custom_data = NULL;

    return cl;
}

/**
 * free_class_level - Free a CLASS_LEVEL
 */
void free_class_level(CLASS_LEVEL *cl)
{
    if (!cl)
        return;

    cl->valid = false;

    free_string(cl->active_title);

    if (cl->custom_data) {
        json_decref(cl->custom_data);
        cl->custom_data = NULL;
    }

    /* Note: alloc_perm memory is not actually freed */
}

/**
 * new_class_reward - Allocate and initialize a new CLASS_REWARD
 *
 * Returns a zeroed CLASS_REWARD with valid=true. Caller must set
 * level, type, name, and other fields as needed.
 */
CLASS_REWARD *new_class_reward(void)
{
    CLASS_REWARD *reward;

    reward = (CLASS_REWARD *)alloc_perm(sizeof(CLASS_REWARD));
    memset(reward, 0, sizeof(CLASS_REWARD));

    reward->valid = true;
    reward->data  = NULL;

    return reward;
}

/**
 * free_class_reward - Free a CLASS_REWARD and its strings/data
 */
void free_class_reward(CLASS_REWARD *reward)
{
    if (!reward)
        return;

    reward->valid = false;

    free_string(reward->name);

    if (reward->data) {
        json_decref(reward->data);
        reward->data = NULL;
    }

    /* Note: alloc_perm memory is not actually freed */
}

/**
 * new_class_title - Allocate and initialize a new CLASS_TITLE
 */
CLASS_TITLE *new_class_title(void)
{
    CLASS_TITLE *title;

    title = (CLASS_TITLE *)alloc_perm(sizeof(CLASS_TITLE));
    memset(title, 0, sizeof(CLASS_TITLE));

    title->valid = true;
    title->is_default = false;

    return title;
}

/**
 * free_class_title - Free a CLASS_TITLE and its strings
 */
void free_class_title(CLASS_TITLE *title)
{
    if (!title)
        return;

    title->valid = false;

    free_string(title->keyword);
    free_string(title->display);
    free_string(title->who_name);

    /* Note: alloc_perm memory is not actually freed */
}

/***************************************************************************
 * Reward Type Helpers                                                     *
 ***************************************************************************/

/**
 * reward_type_to_string - Convert a REWARD_* constant to its string name
 */
static const char *reward_type_to_string(int type)
{
    int i;
    for (i = 0; reward_types[i].name; i++) {
        if (reward_types[i].bit == type)
            return reward_types[i].name;
    }
    return "unknown";
}

/**
 * reward_type_from_string - Convert a string to a REWARD_* constant
 *
 * @return  REWARD_* constant, or -1 if not found
 */
static int reward_type_from_string(const char *name)
{
    int i;
    if (!name || !name[0])
        return -1;

    for (i = 0; reward_types[i].name; i++) {
        if (!str_cmp(name, reward_types[i].name))
            return (int)reward_types[i].bit;
    }
    return -1;
}

/**
 * reward_scope_to_string - Convert a REWARD_SCOPE_* constant to its string name
 */
static const char *reward_scope_to_string(int scope)
{
    int i;
    for (i = 0; reward_scopes[i].name; i++) {
        if (reward_scopes[i].bit == scope)
            return reward_scopes[i].name;
    }
    return "class";
}

/**
 * reward_scope_from_string - Convert a string to a REWARD_SCOPE_* constant
 *
 * @return  REWARD_SCOPE_* constant, or REWARD_SCOPE_CLASS if not found
 */
static int reward_scope_from_string(const char *name)
{
    int i;
    if (!name || !name[0])
        return REWARD_SCOPE_CLASS;

    for (i = 0; reward_scopes[i].name; i++) {
        if (!str_cmp(name, reward_scopes[i].name))
            return (int)reward_scopes[i].bit;
    }
    return REWARD_SCOPE_CLASS;
}

/***************************************************************************
 * Lookup Functions                                                        *
 ***************************************************************************/

/**
 * class_find_exact - Exact match lookup by name (case-insensitive, O(1) average)
 */
CLASS_DATA *class_find_exact(const char *name)
{
    unsigned int idx;
    CLASS_HASH_ENTRY *entry;

    if (!name || !name[0])
        return NULL;

    idx = class_hash(name);
    entry = class_hash_tbl[idx];

    while (entry) {
        if (!str_cmp(entry->key, name))
            return entry->value;
        entry = entry->next;
    }

    return NULL;
}

/**
 * class_find - Prefix match lookup (like legacy class lookup)
 *
 * Returns the first class whose name starts with the given prefix.
 * Linear scan of the sorted list — use class_find_exact for exact matches.
 */
CLASS_DATA *class_find(const char *name)
{
    CLASS_DATA *clazz;

    if (!name || !name[0])
        return NULL;

    for (clazz = class_list; clazz; clazz = clazz->next) {
        if (LOWER(name[0]) == LOWER(clazz->name[0])
        && !str_prefix(name, clazz->name))
            return clazz;
    }

    return NULL;
}

/**
 * class_find_uid - Lookup by UID (O(1) via index array)
 */
CLASS_DATA *class_find_uid(int16_t uid)
{
    if (uid < 0 || uid > max_class_uid || !class_uid_index)
        return NULL;

    return class_uid_index[uid];
}

/**
 * class_name - Get the name of a class, or "none" if NULL
 */
const char *class_name(CLASS_DATA *clazz)
{
    return clazz ? clazz->name : "none";
}

/**
 * class_display - Get the display name for a class by body type
 *
 * Falls back to BODY_TYPE_NEUTRAL if the body type slot is empty,
 * then to class name.
 */
const char *class_display(CLASS_DATA *clazz, int body_type)
{
    if (!clazz) return "none";
    if (body_type < 0 || body_type >= BODY_TYPE_MAX) body_type = BODY_TYPE_NEUTRAL;

    /* Try exact body type first */
    if (clazz->display[body_type] && clazz->display[body_type][0])
        return clazz->display[body_type];

    /* Fall back to neutral */
    if (body_type != BODY_TYPE_NEUTRAL
    && clazz->display[BODY_TYPE_NEUTRAL] && clazz->display[BODY_TYPE_NEUTRAL][0])
        return clazz->display[BODY_TYPE_NEUTRAL];

    return clazz->name;
}

/**
 * class_who - Get the 'who' short name for a class by body type
 *
 * Falls back to BODY_TYPE_NEUTRAL if the body type slot is empty,
 * then to class name.
 */
const char *class_who(CLASS_DATA *clazz, int body_type)
{
    if (!clazz) return "Non";
    if (body_type < 0 || body_type >= BODY_TYPE_MAX) body_type = BODY_TYPE_NEUTRAL;

    /* Try exact body type first */
    if (clazz->who[body_type] && clazz->who[body_type][0])
        return clazz->who[body_type];

    /* Fall back to neutral */
    if (body_type != BODY_TYPE_NEUTRAL
    && clazz->who[BODY_TYPE_NEUTRAL] && clazz->who[BODY_TYPE_NEUTRAL][0])
        return clazz->who[BODY_TYPE_NEUTRAL];

    return clazz->name;
}

/**
 * class_display_ch - Get the display name for a class using character context
 *
 * If the character has a class level with an active title, returns that
 * title's display name. Otherwise checks titles for a default, then falls
 * back to the legacy body-type display array.
 */
const char *class_display_ch(CLASS_DATA *clazz, CHAR_DATA *ch)
{
    if (!ch) return class_display(clazz, BODY_TYPE_NEUTRAL);

    /* Check for title-based display if character has a class level */
    if (!IS_NPC(ch) && ch->pcdata) {
        CLASS_LEVEL *cl = get_class_level(ch, clazz);
        if (clazz && clazz->titles && list_size(clazz->titles) > 0)
            return class_title_display(clazz, cl);
    }

    return class_display(clazz, (int)ch->body_type);
}

/**
 * class_who_ch - Get the 'who' short name for a class using character context
 *
 * Same title-aware logic as class_display_ch.
 */
const char *class_who_ch(CLASS_DATA *clazz, CHAR_DATA *ch)
{
    if (!ch) return class_who(clazz, BODY_TYPE_NEUTRAL);

    /* Check for title-based display if character has a class level */
    if (!IS_NPC(ch) && ch->pcdata) {
        CLASS_LEVEL *cl = get_class_level(ch, clazz);
        if (clazz && clazz->titles && list_size(clazz->titles) > 0)
            return class_title_who(clazz, cl);
    }

    return class_who(clazz, (int)ch->body_type);
}

/***************************************************************************
 * Title API                                                               *
 ***************************************************************************/

/**
 * class_find_title - Find a title in a class by keyword
 *
 * @param clazz   Class to search
 * @param keyword Title keyword (case-insensitive)
 * @return        CLASS_TITLE pointer or NULL
 */
CLASS_TITLE *class_find_title(CLASS_DATA *clazz, const char *keyword)
{
    if (!clazz || !keyword || !clazz->titles)
        return NULL;

    ITERATOR it;
    CLASS_TITLE *title;
    iterator_start(&it, clazz->titles);
    while ((title = (CLASS_TITLE *)iterator_nextdata(&it))) {
        if (title->keyword && !str_cmp(title->keyword, keyword)) {
            iterator_stop(&it);
            return title;
        }
    }
    iterator_stop(&it);
    return NULL;
}

/**
 * class_get_default_title - Get the default title for a class
 *
 * @param clazz  Class to search
 * @return       First CLASS_TITLE with is_default=true, or NULL
 */
CLASS_TITLE *class_get_default_title(CLASS_DATA *clazz)
{
    if (!clazz || !clazz->titles)
        return NULL;

    ITERATOR it;
    CLASS_TITLE *title;
    iterator_start(&it, clazz->titles);
    while ((title = (CLASS_TITLE *)iterator_nextdata(&it))) {
        if (title->is_default) {
            iterator_stop(&it);
            return title;
        }
    }
    iterator_stop(&it);
    return NULL;
}

/**
 * class_title_display - Get the display name for a character's active title
 *
 * Checks the CLASS_LEVEL's active_title, resolves to a CLASS_TITLE,
 * and returns the display name. Falls back to default title, then
 * to the class name.
 *
 * @param clazz  Class definition
 * @param cl     Per-character class level (may be NULL)
 * @return       Display name string (never NULL)
 */
const char *class_title_display(CLASS_DATA *clazz, CLASS_LEVEL *cl)
{
    if (!clazz) return "none";

    /* Try active title on character */
    if (cl && cl->active_title) {
        CLASS_TITLE *title = class_find_title(clazz, cl->active_title);
        if (title && title->display && title->display[0])
            return title->display;
    }

    /* Fall back to default title */
    CLASS_TITLE *def = class_get_default_title(clazz);
    if (def && def->display && def->display[0])
        return def->display;

    return clazz->name;
}

/**
 * class_title_who - Get the 'who' name for a character's active title
 *
 * Same fallback chain as class_title_display.
 *
 * @param clazz  Class definition
 * @param cl     Per-character class level (may be NULL)
 * @return       Who name string (never NULL)
 */
const char *class_title_who(CLASS_DATA *clazz, CLASS_LEVEL *cl)
{
    if (!clazz) return "Non";

    /* Try active title on character */
    if (cl && cl->active_title) {
        CLASS_TITLE *title = class_find_title(clazz, cl->active_title);
        if (title && title->who_name && title->who_name[0])
            return title->who_name;
    }

    /* Fall back to default title */
    CLASS_TITLE *def = class_get_default_title(clazz);
    if (def && def->who_name && def->who_name[0])
        return def->who_name;

    return clazz->name;
}

/**
 * class_first - Get the first class in the global sorted list
 */
CLASS_DATA *class_first(void)
{
    return class_list;
}

/**
 * class_count - Get the total number of loaded classes
 */
int class_count(void)
{
    return class_total;
}

/**
 * class_get_default - Find the first class with CLASS_DEFAULT flag set
 *
 * @return  CLASS_DATA pointer, or NULL if no default class exists
 */
CLASS_DATA *class_get_default(void)
{
    CLASS_DATA *clazz;
    for (clazz = class_list; clazz; clazz = clazz->next) {
        if (IS_SET(clazz->flags, CLASS_DEFAULT))
            return clazz;
    }
    return NULL;
}

/***************************************************************************
 * Character Class API                                                     *
 ***************************************************************************/

/**
 * get_current_class - Get the current active class for a character
 *
 * @param ch     Character to query
 * @return       CLASS_DATA pointer, or NULL if NPC or no class set
 */
CLASS_DATA *get_current_class(CHAR_DATA *ch)
{
    if (IS_NPC(ch) || !ch->pcdata)
        return NULL;

    if (ch->pcdata->current_class && ch->pcdata->current_class->clazz)
        return ch->pcdata->current_class->clazz;

    return NULL;
}

/**
 * get_class_level - Get the CLASS_LEVEL entry for a character in a class
 *
 * @param ch     Character to query
 * @param clazz  Class to look up (NULL = current class)
 * @return       CLASS_LEVEL pointer, or NULL if not found
 */
CLASS_LEVEL *get_class_level(CHAR_DATA *ch, CLASS_DATA *clazz)
{
    ITERATOR it;
    CLASS_LEVEL *cl;

    if (IS_NPC(ch) || !ch->pcdata)
        return NULL;

    /* NULL clazz = current class */
    if (!clazz)
        return ch->pcdata->current_class;

    iterator_start(&it, ch->pcdata->classes);
    while ((cl = (CLASS_LEVEL *)iterator_nextdata(&it))) {
        if (cl->clazz == clazz)
            break;
    }
    iterator_stop(&it);

    return cl;
}

/**
 * has_class_level - Check if a character has any level in a class
 */
bool has_class_level(CHAR_DATA *ch, CLASS_DATA *clazz)
{
    if (IS_NPC(ch) || !clazz)
        return false;

    return (get_class_level(ch, clazz) != NULL);
}

/**
 * insert_class_level - Insert a CLASS_LEVEL into a character's class list, sorted by name
 */
void insert_class_level(CHAR_DATA *ch, CLASS_LEVEL *cl)
{
    ITERATOR it;
    CLASS_LEVEL *existing;

    if (IS_NPC(ch) || !ch->pcdata || !cl || !cl->clazz)
        return;

    iterator_start(&it, ch->pcdata->classes);
    while ((existing = (CLASS_LEVEL *)iterator_nextdata(&it))) {
        if (str_cmp(cl->clazz->name, existing->clazz->name) < 0) {
            iterator_insert_before(&it, cl);
            iterator_stop(&it);
            return;
        }
    }
    iterator_stop(&it);

    /* Append if largest or list is empty */
    list_appendlink(ch->pcdata->classes, cl);
}

/**
 * add_class_level - Add or update a class level for a character
 *
 * If the character already has a level in the class, updates it.
 * Otherwise creates a new CLASS_LEVEL entry.
 */
void add_class_level(CHAR_DATA *ch, CLASS_DATA *clazz, int level)
{
    CLASS_LEVEL *cl;

    if (IS_NPC(ch) || !ch->pcdata || !clazz)
        return;

    cl = get_class_level(ch, clazz);
    if (cl) {
        cl->level = level;
        return;
    }

    /* Create new entry */
    cl = new_class_level();
    cl->clazz = clazz;
    cl->level = level;
    cl->xp = 0;

    insert_class_level(ch, cl);

    /* If no current class set, make this one current */
    if (!ch->pcdata->current_class)
        ch->pcdata->current_class = cl;
}

/**
 * remove_class_level - Remove a class level from a character
 */
void remove_class_level(CHAR_DATA *ch, CLASS_DATA *clazz)
{
    ITERATOR it;
    CLASS_LEVEL *cl;

    if (IS_NPC(ch) || !ch->pcdata || !clazz)
        return;

    iterator_start(&it, ch->pcdata->classes);
    while ((cl = (CLASS_LEVEL *)iterator_nextdata(&it))) {
        if (cl->clazz == clazz) {
            /* If this was the current class, clear it */
            if (ch->pcdata->current_class == cl)
                ch->pcdata->current_class = NULL;

            iterator_remcurrent(&it);
            free_class_level(cl);
            break;
        }
    }
    iterator_stop(&it);

    /* Set a new current class if needed */
    if (!ch->pcdata->current_class && list_size(ch->pcdata->classes) > 0) {
        ch->pcdata->current_class = (CLASS_LEVEL *)list_nthdata(ch->pcdata->classes, 1);
    }
}

/**
 * is_current_class_combat - Check if the character's current class is combat-capable
 */
bool is_current_class_combat(CHAR_DATA *ch)
{
    CLASS_DATA *clazz = get_current_class(ch);
    if (!clazz)
        return false;

    return IS_SET(clazz->flags, CLASS_COMBATIVE);
}

/***************************************************************************
 * Compatibility / Migration API                                           *
 ***************************************************************************/

/**
 * class_from_legacy - Map a legacy class_table + sub_class index to CLASS_DATA
 *
 * Resolves old sub_class_table indices to new CLASS_DATA pointers.
 * sub_class_idx is the index into sub_class_table[]; if -1, returns NULL.
 */
CLASS_DATA *class_from_legacy(int class_idx, int sub_class_idx)
{
    if (sub_class_idx < 0 || sub_class_idx >= MAX_SUB_CLASS)
        return NULL;

    /* The sub_class_table[].name[0] is the canonical class name */
    return class_find_exact(sub_class_table[sub_class_idx].name[0]);
}

/***************************************************************************
 * Sorted List Insertion                                                   *
 ***************************************************************************/

/**
 * class_insert_sorted - Insert a class into the global list alphabetically
 */
static void class_insert_sorted(CLASS_DATA *clazz)
{
    CLASS_DATA *prev, *curr;

    if (!class_list || str_cmp(clazz->name, class_list->name) < 0) {
        clazz->next = class_list;
        class_list = clazz;
        return;
    }

    for (prev = class_list, curr = class_list->next;
         curr != NULL;
         prev = curr, curr = curr->next) {
        if (str_cmp(clazz->name, curr->name) < 0)
            break;
    }

    clazz->next = curr;
    prev->next = clazz;
}

/***************************************************************************
 * UID Index Management                                                    *
 ***************************************************************************/

/**
 * class_uid_index_grow - Ensure the UID index is large enough for the given UID
 */
static void class_uid_index_grow(int16_t uid)
{
    if (uid < 0)
        return;

    if (uid >= max_class_uid) {
        int new_max = uid + 64;
        CLASS_DATA **new_index = (CLASS_DATA **)alloc_perm(sizeof(CLASS_DATA *) * (new_max + 1));
        memset(new_index, 0, sizeof(CLASS_DATA *) * (new_max + 1));

        if (class_uid_index && max_class_uid > 0) {
            memcpy(new_index, class_uid_index, sizeof(CLASS_DATA *) * (max_class_uid + 1));
        }

        class_uid_index = new_index;
        max_class_uid = new_max;
    }
}

/**
 * class_register - Insert a class into all lookup structures
 */
static void class_register(CLASS_DATA *clazz)
{
    class_hash_insert(clazz);
    class_insert_sorted(clazz);

    class_uid_index_grow(clazz->uid);
    class_uid_index[clazz->uid] = clazz;

    if (clazz->uid > top_class_uid)
        top_class_uid = clazz->uid;

    class_total++;
}

/***************************************************************************
 * JSON Loading                                                            *
 ***************************************************************************/

/**
 * class_type_from_string - Convert a type name string to CLASS_TYPE_* constant
 */
static int16_t class_type_from_string(const char *str)
{
    if (!str || !str[0]) return CLASS_TYPE_NONE;

    for (int i = 0; class_types[i].name != NULL; i++) {
        if (!str_cmp(str, class_types[i].name))
            return (int16_t)class_types[i].bit;
    }

    return CLASS_TYPE_NONE;
}

/**
 * class_type_to_string - Convert a CLASS_TYPE_* constant to string
 */
static const char *class_type_to_string(int16_t type)
{
    for (int i = 0; class_types[i].name != NULL; i++) {
        if (class_types[i].bit == type)
            return class_types[i].name;
    }

    return "none";
}

/**
 * stat_from_string - Convert a stat name to STAT_* constant
 */
static int16_t stat_from_string(const char *str)
{
    if (!str || !str[0])  return STAT_STR;
    if (!str_cmp(str, "strength")     || !str_cmp(str, "str")) return STAT_STR;
    if (!str_cmp(str, "intelligence") || !str_cmp(str, "int")) return STAT_INT;
    if (!str_cmp(str, "wisdom")       || !str_cmp(str, "wis")) return STAT_WIS;
    if (!str_cmp(str, "dexterity")    || !str_cmp(str, "dex")) return STAT_DEX;
    if (!str_cmp(str, "constitution") || !str_cmp(str, "con")) return STAT_CON;
    return STAT_STR;
}

/**
 * stat_to_string - Convert a STAT_* constant to string
 */
static const char *stat_to_string(int16_t stat)
{
    switch (stat) {
        case STAT_STR: return "strength";
        case STAT_INT: return "intelligence";
        case STAT_WIS: return "wisdom";
        case STAT_DEX: return "dexterity";
        case STAT_CON: return "constitution";
        default:       return "strength";
    }
}

/**
 * class_load_json - Load a single CLASS_DATA from a JSON file
 */
static CLASS_DATA *class_load_json(const char *filename)
{
    json_t *root, *obj, *arr, *val;
    json_error_t error;
    CLASS_DATA *clazz;
    const char *str;
    size_t index;

    root = json_load_file(filename, 0, &error);
    if (!root) {
        log_stringf("class_load_json: Error loading %s: %s", filename, error.text);
        return NULL;
    }

    /* Validate format */
    str = json_string_value(json_object_get(root, "_format"));
    if (!str || str_cmp(str, "class_data")) {
        log_stringf("class_load_json: Invalid format in %s", filename);
        json_decref(root);
        return NULL;
    }

    clazz = new_class_data();

    /* Identity */
    str = json_string_value(json_object_get(root, "name"));
    free_string(clazz->name);
    clazz->name = str_dup(str ? str : "unknown");

    val = json_object_get(root, "uid");
    clazz->uid = val ? (int16_t)json_integer_value(val) : -1;

    str = json_string_value(json_object_get(root, "description"));
    free_string(clazz->description);
    clazz->description = str ? str_dup(str) : str_dup("");

    str = json_string_value(json_object_get(root, "comments"));
    free_string(clazz->comments);
    clazz->comments = str ? str_dup(str) : str_dup("");

    /* Type */
    str = json_string_value(json_object_get(root, "type"));
    clazz->type = class_type_from_string(str);

    /* Flags */
    arr = json_object_get(root, "flags");
    if (arr && json_is_array(arr)) {
        clazz->flags = 0;
        json_array_foreach(arr, index, val) {
            str = json_string_value(val);
            if (!str) continue;
            if (!str_cmp(str, "combative"))     SET_BIT(clazz->flags, CLASS_COMBATIVE);
            else if (!str_cmp(str, "no_level")) SET_BIT(clazz->flags, CLASS_NO_LEVEL);
            else if (!str_cmp(str, "caster"))   SET_BIT(clazz->flags, CLASS_CASTER);
            else if (!str_cmp(str, "hidden"))   SET_BIT(clazz->flags, CLASS_HIDDEN);
            else if (!str_cmp(str, "remort_only")) SET_BIT(clazz->flags, CLASS_REMORT_ONLY);
            else if (!str_cmp(str, "default"))  SET_BIT(clazz->flags, CLASS_DEFAULT);
        }
    }

    /* Display names (body-type-aware) */
    obj = json_object_get(root, "display");
    if (obj && json_is_object(obj)) {
        str = json_string_value(json_object_get(obj, "neutral"));
        if (str) { free_string(clazz->display[BODY_TYPE_NEUTRAL]); clazz->display[BODY_TYPE_NEUTRAL] = str_dup(str); }
        str = json_string_value(json_object_get(obj, "male"));
        if (str) { free_string(clazz->display[BODY_TYPE_MALE]); clazz->display[BODY_TYPE_MALE] = str_dup(str); }
        str = json_string_value(json_object_get(obj, "female"));
        if (str) { free_string(clazz->display[BODY_TYPE_FEMALE]); clazz->display[BODY_TYPE_FEMALE] = str_dup(str); }
        str = json_string_value(json_object_get(obj, "other"));
        if (str) { free_string(clazz->display[BODY_TYPE_OTHER]); clazz->display[BODY_TYPE_OTHER] = str_dup(str); }
    }

    /* Who names */
    obj = json_object_get(root, "who");
    if (obj && json_is_object(obj)) {
        str = json_string_value(json_object_get(obj, "neutral"));
        if (str) { free_string(clazz->who[BODY_TYPE_NEUTRAL]); clazz->who[BODY_TYPE_NEUTRAL] = str_dup(str); }
        str = json_string_value(json_object_get(obj, "male"));
        if (str) { free_string(clazz->who[BODY_TYPE_MALE]); clazz->who[BODY_TYPE_MALE] = str_dup(str); }
        str = json_string_value(json_object_get(obj, "female"));
        if (str) { free_string(clazz->who[BODY_TYPE_FEMALE]); clazz->who[BODY_TYPE_FEMALE] = str_dup(str); }
        str = json_string_value(json_object_get(obj, "other"));
        if (str) { free_string(clazz->who[BODY_TYPE_OTHER]); clazz->who[BODY_TYPE_OTHER] = str_dup(str); }
    }

    /* Titles (selectable class names for 'who' display) */
    arr = json_object_get(root, "titles");
    if (arr && json_is_array(arr)) {
        json_array_foreach(arr, index, val) {
            if (!json_is_object(val))
                continue;

            CLASS_TITLE *title = new_class_title();

            str = json_string_value(json_object_get(val, "keyword"));
            title->keyword = str_dup(str ? str : "");

            str = json_string_value(json_object_get(val, "display"));
            title->display = str_dup(str ? str : "");

            str = json_string_value(json_object_get(val, "who_name"));
            title->who_name = str_dup(str ? str : "");

            json_t *jdef = json_object_get(val, "default");
            title->is_default = jdef ? json_is_true(jdef) : false;

            list_appendlink(clazz->titles, title);
        }
    }

    /* Progression */
    str = json_string_value(json_object_get(root, "primary_stat"));
    clazz->primary_stat = stat_from_string(str);

    val = json_object_get(root, "max_level");
    if (val) clazz->max_level = (int16_t)json_integer_value(val);

    val = json_object_get(root, "hp_min");
    if (val) clazz->hp_min = (int)json_integer_value(val);

    val = json_object_get(root, "hp_max");
    if (val) clazz->hp_max = (int)json_integer_value(val);

    val = json_object_get(root, "gains_mana");
    if (val) clazz->gains_mana = json_is_true(val);

    val = json_object_get(root, "weapon");
    if (val) clazz->weapon = (long)json_integer_value(val);

    /* XP curve (per-class override, optional) */
    arr = json_object_get(root, "xp_table");
    if (arr && json_is_array(arr)) {
        int count = (int)json_array_size(arr);
        if (count > 0) {
            clazz->xp_table = (long *)alloc_perm(sizeof(long) * count);
            clazz->xp_table_size = count;
            for (int i = 0; i < count; i++) {
                clazz->xp_table[i] = (long)json_integer_value(json_array_get(arr, i));
            }
        }
    }

    /* Groups (resolved to SKILL_GROUP pointers) */
    arr = json_object_get(root, "groups");
    if (arr && json_is_array(arr)) {
        json_array_foreach(arr, index, val) {
            str = json_string_value(val);
            if (str && str[0]) {
                SKILL_GROUP *sg = skill_group_find(str);
                if (sg)
                    list_appendlink(clazz->groups, sg);
                else
                    log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                        "load_class_data: class '%s' references unknown group '%s'",
                        clazz->name, str);
            }
        }
    }

    /* Rewards (level-based progression grants) */
    arr = json_object_get(root, "rewards");
    if (arr && json_is_array(arr)) {
        json_array_foreach(arr, index, val) {
            if (!json_is_object(val))
                continue;

            CLASS_REWARD *reward = new_class_reward();

            json_t *jlevel = json_object_get(val, "level");
            reward->level = jlevel ? (int16_t)json_integer_value(jlevel) : 1;

            str = json_string_value(json_object_get(val, "type"));
            reward->type = (int16_t)reward_type_from_string(str);
            if (reward->type < 0) {
                log_stringf("class_load_json: Unknown reward type '%s' in %s",
                            str ? str : "(null)", filename);
                free_class_reward(reward);
                continue;
            }

            str = json_string_value(json_object_get(val, "name"));
            if (str && str[0])
                reward->name = str_dup(str);

            json_t *jvalue = json_object_get(val, "value");
            if (jvalue)
                reward->value = (int)json_integer_value(jvalue);

            /* Scope (cross-class availability) */
            str = json_string_value(json_object_get(val, "scope"));
            reward->scope = (int16_t)reward_scope_from_string(str);

            /* Flags */
            json_t *jflags = json_object_get(val, "flags");
            if (jflags && json_is_array(jflags)) {
                size_t fi;
                json_t *fval;
                json_array_foreach(jflags, fi, fval) {
                    const char *fname = json_string_value(fval);
                    if (!fname) continue;
                    if (!str_cmp(fname, "revoke_on_leave")) SET_BIT(reward->flags, REWARD_REVOKE_ON_LEAVE);
                    else if (!str_cmp(fname, "one_time"))   SET_BIT(reward->flags, REWARD_ONE_TIME);
                    else if (!str_cmp(fname, "hidden"))     SET_BIT(reward->flags, REWARD_HIDDEN);
                }
            }

            /* Extended data — store the raw JSON object (incref so it survives root decref) */
            json_t *jdata = json_object_get(val, "data");
            if (jdata && !json_is_null(jdata)) {
                reward->data = json_incref(jdata);
            }

            list_appendlink(clazz->rewards, reward);
        }
    }

    /* Traits (class-level trait values) */
    obj = json_object_get(root, "traits");
    if (obj && json_is_object(obj)) {
        /* Ensure trait_values is allocated (may not be if traits loaded after class) */
        if (!clazz->trait_values)
            class_init_traits(clazz);
        class_load_traits_json(clazz, obj);
    }

    /* Callbacks (stored as function name strings, resolved later) */
    str = json_string_value(json_object_get(root, "enter_function"));
    if (str && str[0]) clazz->enter_fun_name = str_dup(str);

    str = json_string_value(json_object_get(root, "leave_function"));
    if (str && str[0]) clazz->leave_fun_name = str_dup(str);

    /* Sync legacy class fields to trait values as fallbacks.
     * If a trait was explicitly set via the "traits" JSON block, it takes
     * priority because trait_values_load_json already wrote it.  We only
     * overwrite traits still at their defaults. */
    if (clazz->trait_values || trait_def_count > 0) {
        if (!clazz->trait_values)
            class_init_traits(clazz);

        if (clazz->trait_values) {
            TRAIT_DEF *def;

            /* hp_gain_min ← hp_min */
            def = trait_def_lookup("hp_gain_min");
            if (def && def->type == TRAIT_INTEGER
                && clazz->trait_values[def->index].int_val == def->default_int)
                clazz->trait_values[def->index].int_val = clazz->hp_min;

            /* hp_gain_max ← hp_max */
            def = trait_def_lookup("hp_gain_max");
            if (def && def->type == TRAIT_INTEGER
                && clazz->trait_values[def->index].int_val == def->default_int)
                clazz->trait_values[def->index].int_val = clazz->hp_max;

            /* uses_mana ← gains_mana */
            def = trait_def_lookup("uses_mana");
            if (def && def->type == TRAIT_BOOLEAN)
                clazz->trait_values[def->index].bool_val = clazz->gains_mana;

            /* primary_stat ← primary_stat (int) */
            def = trait_def_lookup("primary_stat");
            if (def && def->type == TRAIT_INTEGER)
                clazz->trait_values[def->index].int_val = clazz->primary_stat;
        }
    }

    json_decref(root);
    return clazz;
}

/***************************************************************************
 * JSON Saving                                                             *
 ***************************************************************************/

/**
 * save_class_data - Save a single CLASS_DATA to its JSON file
 */
void save_class_data(CLASS_DATA *clazz)
{
    json_t *root, *arr, *obj;
    char path[512];
    char safe_name[256];

    if (!clazz || !clazz->name)
        return;

    /* Create directory if needed */
    mkdir(CLASSES_DIR, 0755);

    /* Build safe filename: replace spaces with underscores */
    snprintf(safe_name, sizeof(safe_name), "%s", clazz->name);
    for (char *p = safe_name; *p; p++) {
        if (*p == ' ') *p = '_';
        else *p = LOWER(*p);
    }

    snprintf(path, sizeof(path), "%s%s.json", CLASSES_DIR, safe_name);

    root = json_object();
    json_object_set_new(root, "_format", json_string("class_data"));
    json_object_set_new(root, "_version", json_integer(1));

    /* Identity */
    json_object_set_new(root, "name", json_string(clazz->name));
    json_object_set_new(root, "uid", json_integer(clazz->uid));

    if (clazz->description && clazz->description[0])
        json_object_set_new(root, "description", json_string(clazz->description));

    if (clazz->comments && clazz->comments[0])
        json_object_set_new(root, "comments", json_string(clazz->comments));

    /* Type */
    json_object_set_new(root, "type", json_string(class_type_to_string(clazz->type)));

    /* Flags */
    arr = json_array();
    if (IS_SET(clazz->flags, CLASS_COMBATIVE))   json_array_append_new(arr, json_string("combative"));
    if (IS_SET(clazz->flags, CLASS_NO_LEVEL))    json_array_append_new(arr, json_string("no_level"));
    if (IS_SET(clazz->flags, CLASS_CASTER))      json_array_append_new(arr, json_string("caster"));
    if (IS_SET(clazz->flags, CLASS_HIDDEN))      json_array_append_new(arr, json_string("hidden"));
    if (IS_SET(clazz->flags, CLASS_REMORT_ONLY)) json_array_append_new(arr, json_string("remort_only"));
    if (IS_SET(clazz->flags, CLASS_DEFAULT))     json_array_append_new(arr, json_string("default"));
    json_object_set_new(root, "flags", arr);

    /* Display names */
    obj = json_object();
    json_object_set_new(obj, "neutral", json_string(clazz->display[BODY_TYPE_NEUTRAL]));
    json_object_set_new(obj, "male",    json_string(clazz->display[BODY_TYPE_MALE]));
    json_object_set_new(obj, "female",  json_string(clazz->display[BODY_TYPE_FEMALE]));
    json_object_set_new(obj, "other",   json_string(clazz->display[BODY_TYPE_OTHER]));
    json_object_set_new(root, "display", obj);

    /* Who names */
    obj = json_object();
    json_object_set_new(obj, "neutral", json_string(clazz->who[BODY_TYPE_NEUTRAL]));
    json_object_set_new(obj, "male",    json_string(clazz->who[BODY_TYPE_MALE]));
    json_object_set_new(obj, "female",  json_string(clazz->who[BODY_TYPE_FEMALE]));
    json_object_set_new(obj, "other",   json_string(clazz->who[BODY_TYPE_OTHER]));
    json_object_set_new(root, "who", obj);

    /* Titles */
    arr = json_array();
    if (clazz->titles && list_size(clazz->titles) > 0) {
        ITERATOR it;
        CLASS_TITLE *title;
        iterator_start(&it, clazz->titles);
        while ((title = (CLASS_TITLE *)iterator_nextdata(&it))) {
            json_t *tobj = json_object();
            json_object_set_new(tobj, "keyword", json_string(title->keyword ? title->keyword : ""));
            json_object_set_new(tobj, "display", json_string(title->display ? title->display : ""));
            json_object_set_new(tobj, "who_name", json_string(title->who_name ? title->who_name : ""));
            if (title->is_default)
                json_object_set_new(tobj, "default", json_true());
            json_array_append_new(arr, tobj);
        }
        iterator_stop(&it);
    }
    json_object_set_new(root, "titles", arr);

    /* Progression */
    json_object_set_new(root, "primary_stat", json_string(stat_to_string(clazz->primary_stat)));
    json_object_set_new(root, "max_level", json_integer(clazz->max_level));
    json_object_set_new(root, "hp_min", json_integer(clazz->hp_min));
    json_object_set_new(root, "hp_max", json_integer(clazz->hp_max));
    json_object_set_new(root, "gains_mana", clazz->gains_mana ? json_true() : json_false());
    json_object_set_new(root, "weapon", json_integer(clazz->weapon));

    /* XP curve (only write if class has a custom curve) */
    if (clazz->xp_table && clazz->xp_table_size > 0) {
        arr = json_array();
        for (int i = 0; i < clazz->xp_table_size; i++) {
            json_array_append_new(arr, json_integer(clazz->xp_table[i]));
        }
        json_object_set_new(root, "xp_table", arr);
    }

    /* Groups */
    arr = json_array();
    if (clazz->groups && list_size(clazz->groups) > 0) {
        ITERATOR it;
        SKILL_GROUP *sg;
        iterator_start(&it, clazz->groups);
        while ((sg = (SKILL_GROUP *)iterator_nextdata(&it))) {
            json_array_append_new(arr, json_string(sg->name));
        }
        iterator_stop(&it);
    }
    json_object_set_new(root, "groups", arr);

    /* Rewards */
    arr = json_array();
    if (clazz->rewards && list_size(clazz->rewards) > 0) {
        ITERATOR it;
        CLASS_REWARD *reward;
        iterator_start(&it, clazz->rewards);
        while ((reward = (CLASS_REWARD *)iterator_nextdata(&it))) {
            json_t *robj = json_object();

            json_object_set_new(robj, "level", json_integer(reward->level));
            json_object_set_new(robj, "type", json_string(reward_type_to_string(reward->type)));

            if (reward->name && reward->name[0])
                json_object_set_new(robj, "name", json_string(reward->name));

            if (reward->value != 0)
                json_object_set_new(robj, "value", json_integer(reward->value));

            /* Scope (only write if non-default) */
            if (reward->scope != REWARD_SCOPE_CLASS)
                json_object_set_new(robj, "scope", json_string(reward_scope_to_string(reward->scope)));

            /* Flags */
            if (reward->flags != 0) {
                json_t *farr = json_array();
                if (IS_SET(reward->flags, REWARD_REVOKE_ON_LEAVE))
                    json_array_append_new(farr, json_string("revoke_on_leave"));
                if (IS_SET(reward->flags, REWARD_ONE_TIME))
                    json_array_append_new(farr, json_string("one_time"));
                if (IS_SET(reward->flags, REWARD_HIDDEN))
                    json_array_append_new(farr, json_string("hidden"));
                json_object_set_new(robj, "flags", farr);
            }

            /* Extended data */
            if (reward->data)
                json_object_set_new(robj, "data", json_deep_copy(reward->data));

            json_array_append_new(arr, robj);
        }
        iterator_stop(&it);
    }
    json_object_set_new(root, "rewards", arr);

    /* Traits */
    {
        void *traits_json = class_save_traits_json(clazz);
        if (traits_json)
            json_object_set_new(root, "traits", (json_t *)traits_json);
    }

    /* Callbacks */
    if (clazz->enter_fun_name && clazz->enter_fun_name[0])
        json_object_set_new(root, "enter_function", json_string(clazz->enter_fun_name));
    else
        json_object_set_new(root, "enter_function", json_null());

    if (clazz->leave_fun_name && clazz->leave_fun_name[0])
        json_object_set_new(root, "leave_function", json_string(clazz->leave_fun_name));
    else
        json_object_set_new(root, "leave_function", json_null());

    /* Write file */
    if (json_dump_file(root, path, JSON_INDENT(4) | JSON_SORT_KEYS) != 0) {
        log_stringf("save_class_data: Failed to write %s", path);
    }

    json_decref(root);
}

/**
 * save_all_class_data - Save all classes to JSON files
 */
void save_all_class_data(void)
{
    CLASS_DATA *clazz;

    log_string("save_all_class_data: saving all classes");

    for (clazz = class_list; clazz; clazz = clazz->next) {
        save_class_data(clazz);
    }

    log_stringf("save_all_class_data: saved %d classes", class_total);
}

/***************************************************************************
 * Bootstrap from Legacy Tables                                            *
 ***************************************************************************/

/**
 * bootstrap_classes_from_table - Create CLASS_DATA entries from sub_class_table[]
 *
 * Called on first boot when no data/classes/ directory exists.
 * Creates one CLASS_DATA per sub_class_table entry.
 */
static void bootstrap_classes_from_table(void)
{
    int i;
    CLASS_DATA *clazz;
    int parent_class;

    log_string("bootstrap_classes_from_table: Creating classes from sub_class_table[]");

    for (i = 0; i < MAX_SUB_CLASS; i++) {
        if (!sub_class_table[i].name[0])
            break;

        clazz = new_class_data();
        clazz->uid = ++top_class_uid;

        /* Name from sub_class_table */
        free_string(clazz->name);
        clazz->name = str_dup(sub_class_table[i].name[0]);

        /* Display names (neutral, male, female — BODY_TYPE_OTHER reuses neutral) */
        for (int s = 0; s < 3 && s < BODY_TYPE_MAX; s++) {
            free_string(clazz->display[s]);
            clazz->display[s] = str_dup(sub_class_table[i].name[s]);
            free_string(clazz->who[s]);
            clazz->who[s] = str_dup(sub_class_table[i].who_name[s]);
        }
        /* BODY_TYPE_OTHER = same as neutral */
        free_string(clazz->display[BODY_TYPE_OTHER]);
        clazz->display[BODY_TYPE_OTHER] = str_dup(sub_class_table[i].name[0]);
        free_string(clazz->who[BODY_TYPE_OTHER]);
        clazz->who[BODY_TYPE_OTHER] = str_dup(sub_class_table[i].who_name[0]);

        /* Create titles from legacy display/who names */
        {
            /* Default title = neutral name */
            CLASS_TITLE *def = new_class_title();
            def->keyword   = str_dup("default");
            def->display   = str_dup(sub_class_table[i].name[0]);
            def->who_name  = str_dup(sub_class_table[i].who_name[0]);
            def->is_default = true;
            list_appendlink(clazz->titles, def);

            /* If male name differs from neutral, create a male-variant title */
            if (str_cmp(sub_class_table[i].name[1], sub_class_table[i].name[0])) {
                CLASS_TITLE *male = new_class_title();
                male->keyword   = str_dup("male");
                male->display   = str_dup(sub_class_table[i].name[1]);
                male->who_name  = str_dup(sub_class_table[i].who_name[1]);
                male->is_default = false;
                list_appendlink(clazz->titles, male);
            }

            /* If female name differs from neutral, create a female-variant title */
            if (str_cmp(sub_class_table[i].name[2], sub_class_table[i].name[0])) {
                CLASS_TITLE *female = new_class_title();
                female->keyword   = str_dup("female");
                female->display   = str_dup(sub_class_table[i].name[2]);
                female->who_name  = str_dup(sub_class_table[i].who_name[2]);
                female->is_default = false;
                list_appendlink(clazz->titles, female);
            }
        }

        /* Parent class determines type and base stats */
        parent_class = sub_class_table[i].class;
        clazz->type = parent_class;  /* CLASS_MAGE/CLERIC/THIEF/WARRIOR map directly to CLASS_TYPE_* */

        /* All legacy classes are combative */
        SET_BIT(clazz->flags, CLASS_COMBATIVE);

        /* Caster flag for mage and cleric types */
        if (parent_class == CLASS_MAGE || parent_class == CLASS_CLERIC)
            SET_BIT(clazz->flags, CLASS_CASTER);

        /* Remort flag */
        if (sub_class_table[i].remort)
            SET_BIT(clazz->flags, CLASS_REMORT_ONLY);

        /* Stats from parent class_table */
        if (parent_class >= 0 && parent_class < MAX_CLASS) {
            clazz->primary_stat = class_table[parent_class].attr_prime;
            clazz->hp_min = class_table[parent_class].hp_min;
            clazz->hp_max = class_table[parent_class].hp_max;
            clazz->gains_mana = class_table[parent_class].fMana;
            clazz->weapon = class_table[parent_class].weapon;
        }

        /* Sync legacy stats into trait values */
        if (clazz->trait_values) {
            TRAIT_DEF *def;

            def = trait_def_lookup("hp_gain_min");
            if (def && def->type == TRAIT_INTEGER)
                clazz->trait_values[def->index].int_val = clazz->hp_min;

            def = trait_def_lookup("hp_gain_max");
            if (def && def->type == TRAIT_INTEGER)
                clazz->trait_values[def->index].int_val = clazz->hp_max;

            def = trait_def_lookup("uses_mana");
            if (def && def->type == TRAIT_BOOLEAN)
                clazz->trait_values[def->index].bool_val = clazz->gains_mana;

            def = trait_def_lookup("primary_stat");
            if (def && def->type == TRAIT_INTEGER)
                clazz->trait_values[def->index].int_val = clazz->primary_stat;
        }

        clazz->max_level = MAX_CLASS_LEVEL;

        /* Groups — base group from parent class + default group from sub_class */
        if (parent_class >= 0 && parent_class < MAX_CLASS
        && class_table[parent_class].base_group) {
            SKILL_GROUP *sg = skill_group_find(class_table[parent_class].base_group);
            if (sg) list_appendlink(clazz->groups, sg);
        }
        if (sub_class_table[i].default_group) {
            SKILL_GROUP *sg = skill_group_find(sub_class_table[i].default_group);
            if (sg) list_appendlink(clazz->groups, sg);
        }

        /* Bootstrap rewards from group assignments.
         * Each group the class owns becomes a REWARD_GROUP at level 1.
         * Group scope is REWARD_SCOPE_CLASS (the default) — each class
         * owns its own skill grants. Cross-class sharing is set per-reward
         * by builders after bootstrap. */
        {
            ITERATOR git;
            SKILL_GROUP *sg;
            iterator_start(&git, clazz->groups);
            while ((sg = (SKILL_GROUP *)iterator_nextdata(&git))) {
                CLASS_REWARD *reward = new_class_reward();
                reward->level = 1;
                reward->type  = REWARD_GROUP;
                reward->scope = REWARD_SCOPE_CLASS;
                reward->name  = str_dup(sg->name);
                list_appendlink(clazz->rewards, reward);
            }
            iterator_stop(&git);
        }

        /* Description placeholder */
        free_string(clazz->description);
        clazz->description = str_dup("");

        class_register(clazz);

        log_stringf("  Bootstrapped class: %s (uid=%d, type=%s%s)",
            clazz->name, clazz->uid,
            class_type_to_string(clazz->type),
            sub_class_table[i].remort ? ", remort" : "");
    }

    log_stringf("bootstrap_classes_from_table: Created %d classes", class_total);
}

/***************************************************************************
 * Hot-Reload                                                              *
 ***************************************************************************/

/**
 * class_build_path - Build the JSON file path for a class by name
 *
 * @param name      Class name (spaces become underscores, lowered)
 * @param buf       Output buffer
 * @param bufsize   Size of output buffer
 */
static void class_build_path(const char *name, char *buf, size_t bufsize)
{
    char safe_name[256];

    snprintf(safe_name, sizeof(safe_name), "%s", name);
    for (char *p = safe_name; *p; p++) {
        if (*p == ' ') *p = '_';
        else *p = LOWER(*p);
    }

    snprintf(buf, bufsize, "%s%s.json", CLASSES_DIR, safe_name);
}

/**
 * class_hash_refresh_key - Update the hash table key for a class
 *
 * After an in-place reload, the hash entry's key pointer becomes stale
 * because class_copy_fields frees the old name string. This refreshes
 * the key to point to the class's current name.
 *
 * @param clazz  Class whose hash entry key should be refreshed
 */
static void class_hash_refresh_key(CLASS_DATA *clazz)
{
    unsigned int idx = class_hash(clazz->name);
    CLASS_HASH_ENTRY *entry = class_hash_tbl[idx];

    while (entry) {
        if (entry->value == clazz) {
            entry->key = clazz->name;
            return;
        }
        entry = entry->next;
    }
}

/**
 * class_copy_fields - Copy all data fields from src to dst in-place
 *
 * Preserves dst's linked-list pointer (next), valid flag, and gcl pointer.
 * Frees old strings/lists on dst before overwriting. Transfers ownership
 * of src's allocated memory — caller must NOT free src's contents after.
 *
 * @param dst   Existing class struct to update
 * @param src   Temporary class struct with new data (will be gutted)
 */
static void class_copy_fields(CLASS_DATA *dst, CLASS_DATA *src)
{
    /* Preserve structural fields */
    CLASS_DATA *saved_next = dst->next;
    CLASS_DATA **saved_gcl = dst->gcl;
    int16_t saved_uid = dst->uid;   /* UID must not change */

    /* Free old content on dst */
    free_string(dst->name);
    free_string(dst->description);
    free_string(dst->comments);
    free_string(dst->enter_fun_name);
    free_string(dst->leave_fun_name);

    for (int i = 0; i < BODY_TYPE_MAX; i++) {
        free_string(dst->display[i]);
        free_string(dst->who[i]);
    }

    if (dst->titles) {
        ITERATOR it;
        CLASS_TITLE *title;
        iterator_start(&it, dst->titles);
        while ((title = (CLASS_TITLE *)iterator_nextdata(&it)))
            free_class_title(title);
        iterator_stop(&it);
        list_destroy(dst->titles);
    }

    if (dst->rewards) {
        ITERATOR it;
        CLASS_REWARD *reward;
        iterator_start(&it, dst->rewards);
        while ((reward = (CLASS_REWARD *)iterator_nextdata(&it)))
            free_class_reward(reward);
        iterator_stop(&it);
        list_destroy(dst->rewards);
    }

    if (dst->groups)
        list_destroy(dst->groups);

    if (dst->trait_values) {
        /* Free any trait strings before releasing the array */
        for (int i = 0; i < trait_def_count; i++) {
            if (dst->trait_values[i].string_val)
                free_string(dst->trait_values[i].string_val);
        }
        /* alloc_perm memory — can't free the array itself */
    }

    if (dst->xp_table) {
        /* alloc_perm memory — can't free */
    }

    /* Copy all data fields from src */
    dst->name = src->name;
    dst->description = src->description;
    dst->comments = src->comments;
    dst->type = src->type;
    dst->flags = src->flags;
    dst->primary_stat = src->primary_stat;
    dst->max_level = src->max_level;
    dst->hp_min = src->hp_min;
    dst->hp_max = src->hp_max;
    dst->gains_mana = src->gains_mana;
    dst->weapon = src->weapon;
    dst->xp_table = src->xp_table;
    dst->xp_table_size = src->xp_table_size;
    dst->enter_fun_name = src->enter_fun_name;
    dst->leave_fun_name = src->leave_fun_name;
    dst->enter = src->enter;
    dst->leave = src->leave;
    dst->groups = src->groups;
    dst->rewards = src->rewards;
    dst->titles = src->titles;
    dst->trait_values = src->trait_values;

    for (int i = 0; i < BODY_TYPE_MAX; i++) {
        dst->display[i] = src->display[i];
        dst->who[i] = src->who[i];
    }

    /* Restore structural fields */
    dst->next = saved_next;
    dst->gcl = saved_gcl;
    dst->uid = saved_uid;
    dst->valid = true;

    /* Null out src's pointers so free_class_data() won't double-free */
    src->name = NULL;
    src->description = NULL;
    src->comments = NULL;
    src->enter_fun_name = NULL;
    src->leave_fun_name = NULL;
    src->groups = NULL;
    src->rewards = NULL;
    src->titles = NULL;
    src->trait_values = NULL;
    src->xp_table = NULL;
    for (int i = 0; i < BODY_TYPE_MAX; i++) {
        src->display[i] = NULL;
        src->who[i] = NULL;
    }
}

/**
 * class_reload - Reload a class definition from its JSON file
 *
 * If the class already exists, updates it in-place (preserving all
 * pointers to the CLASS_DATA struct). If it doesn't exist, loads it
 * as a new class and registers it.
 *
 * @param name  Class name to reload (matches the JSON filename pattern)
 * @return      The (re)loaded CLASS_DATA, or NULL on failure
 */
CLASS_DATA *class_reload(const char *name)
{
    char path[512];
    CLASS_DATA *existing, *temp;

    if (!name || !name[0])
        return NULL;

    class_build_path(name, path, sizeof(path));

    /* Parse the JSON file */
    temp = class_load_json(path);
    if (!temp) {
        log_stringf("class_reload: Failed to parse %s", path);
        return NULL;
    }

    /* Check if this class already exists */
    existing = class_find_exact(temp->name);

    if (existing) {
        /* In-place update — preserve the pointer */
        class_copy_fields(existing, temp);

        /* Refresh the hash table key — class_copy_fields freed the old
         * name string that the hash entry was pointing to. */
        class_hash_refresh_key(existing);

        /* Free the empty shell (all contents already transferred) */
        /* Note: alloc_perm memory, can't actually free the struct itself */

        log_stringf("class_reload: Reloaded class '%s' (uid %d) in-place",
                     existing->name, existing->uid);
        return existing;
    } else {
        /* New class — assign UID and register */
        if (temp->uid < 0)
            temp->uid = ++top_class_uid;

        class_register(temp);

        log_stringf("class_reload: Loaded new class '%s' (uid %d)",
                     temp->name, temp->uid);
        return temp;
    }
}

/***************************************************************************
 * Boot / Load                                                             *
 ***************************************************************************/

/**
 * load_class_data - Load all classes from JSON files, or bootstrap on first run
 *
 * Called from boot_db() after load_skill_data().
 */
void load_class_data(void)
{
    DIR *dir;
    struct dirent *entry;
    char path[512];
    int loaded = 0;

    log_string("load_class_data: Loading classes...");

    memset(class_hash_tbl, 0, sizeof(class_hash_tbl));

    /* Try to open the classes directory */
    dir = opendir(CLASSES_DIR);

    if (!dir) {
        /* No directory — bootstrap from legacy tables */
        log_stringf("load_class_data: %s not found, bootstrapping from sub_class_table", CLASSES_DIR);
        bootstrap_classes_from_table();
        save_all_class_data();
        log_stringf("load_class_data: Bootstrap complete. %d classes saved.", class_total);
        return;
    }

    /* Load all .json files from the directory */
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len < 6 || strcmp(entry->d_name + len - 5, ".json") != 0)
            continue;

        snprintf(path, sizeof(path), "%s%s", CLASSES_DIR, entry->d_name);

        CLASS_DATA *clazz = class_load_json(path);
        if (clazz) {
            /* Assign UID if missing */
            if (clazz->uid < 0)
                clazz->uid = ++top_class_uid;

            class_register(clazz);
            loaded++;
        }
    }

    closedir(dir);

    if (loaded == 0) {
        log_stringf("WARNING: No classes loaded from %s!", CLASSES_DIR);
        log_string("load_class_data: Bootstrapping from sub_class_table as fallback");
        bootstrap_classes_from_table();
        save_all_class_data();
    } else {
        log_stringf("load_class_data: Loaded %d classes from JSON", loaded);
    }
}

/***************************************************************************
 * XP Curve — Per-class experience requirements                            *
 ***************************************************************************/

/**
 * Default XP table used when a class has no custom xp_table.
 *
 * This is the standard 30-level progression curve. When classes don't
 * specify their own XP requirements, they use this table. Classes can
 * override this with a per-level "xp_table" array in their JSON definition.
 *
 * Indexed 0..29 for levels 1..30.
 */
static const long default_xp_table[] = {
    250,        /*  1 */
    500,        /*  2 */
    1000,       /*  3 */
    2500,       /*  4 */
    3000,       /*  5 */
    4000,       /*  6 */
    7500,       /*  7 */
    8000,       /*  8 */
    9000,       /*  9 */
    10000,      /* 10 */
    12500,      /* 11 */
    15000,      /* 12 */
    17500,      /* 13 */
    18500,      /* 14 */
    20500,      /* 15 */
    25000,      /* 16 */
    40000,      /* 17 */
    60000,      /* 18 */
    90000,      /* 19 */
    100000,     /* 20 */
    120000,     /* 21 */
    130000,     /* 22 */
    130000,     /* 23 */
    150000,     /* 24 */
    175000,     /* 25 */
    250000,     /* 26 */
    500000,     /* 27 */
    750000,     /* 28 */
    850000,     /* 29 */
    1000000,    /* 30 */
};
static const int default_xp_table_size = sizeof(default_xp_table) / sizeof(default_xp_table[0]);

/**
 * class_default_xp_table - Return a pointer to the default XP table
 *
 * @param out_size  If non-NULL, receives the number of entries
 * @return          Pointer to the default XP table array
 */
const long *class_default_xp_table(int *out_size)
{
    if (out_size)
        *out_size = default_xp_table_size;
    return default_xp_table;
}

/**
 * class_exp_per_level - Get XP required to advance a given level in a class
 *
 * Returns the experience needed to go from 'level' to 'level+1' for the
 * specified class. Uses the class's custom xp_table if set, otherwise
 * falls back to the default progression curve.
 *
 * @param clazz  The class to query (NULL = use default table)
 * @param level  The current level (1-based)
 * @return       XP required, or 0 if at/beyond max level
 */
long class_exp_per_level(CLASS_DATA *clazz, int level)
{
    const long *table;
    int table_size;

    if (level < 1)
        return 0;

    if (clazz) {
        if (level >= clazz->max_level)
            return 0;

        if (clazz->xp_table && clazz->xp_table_size > 0) {
            table = clazz->xp_table;
            table_size = clazz->xp_table_size;
        } else {
            table = default_xp_table;
            table_size = default_xp_table_size;
        }
    } else {
        table = default_xp_table;
        table_size = default_xp_table_size;
    }

    /* level is 1-based, table is 0-indexed */
    int idx = level - 1;
    if (idx < 0 || idx >= table_size)
        return 0;

    return table[idx];
}
