/***************************************************************************
 *  Skill Group System - Implementation                                    *
 *                                                                         *
 *  Named collections of skills loaded from JSON files in                  *
 *  data/skill_groups/.                                                    *
 *                                                                         *
 *  Groups are used by REWARD_GROUP to grant batches of skills at once.    *
 *  They are a lightweight organizational convenience, not a complex       *
 *  runtime system.                                                        *
 ***************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <jansson.h>
#include "io/json/json_common.h"
#include "merc.h"
#include "tables.h"
#include "skill_group.h"

/***************************************************************************
 * Globals                                                                 *
 ***************************************************************************/

static SKILL_GROUP *    group_list = NULL;       /* Alphabetically sorted linked list */
static int              group_total = 0;         /* Total loaded groups */

/* Hash table for O(1) name lookup */
typedef struct group_hash_entry {
    char *                      key;
    SKILL_GROUP *               value;
    struct group_hash_entry *   next;
} GROUP_HASH_ENTRY;

static GROUP_HASH_ENTRY *group_hash_tbl[SKILL_GROUP_HASH_SIZE];

/***************************************************************************
 * Hash Function                                                           *
 ***************************************************************************/

/**
 * group_hash - FNV-1a hash of a group name, case-insensitive
 */
static unsigned int group_hash(const char *name)
{
    unsigned int hash = 2166136261u;

    for (; *name; name++) {
        hash ^= (unsigned char)LOWER(*name);
        hash *= 16777619u;
    }

    return hash % SKILL_GROUP_HASH_SIZE;
}

/***************************************************************************
 * Registration                                                            *
 ***************************************************************************/

/**
 * group_register - Add a SKILL_GROUP to the hash table and sorted list
 */
static void group_register(SKILL_GROUP *group)
{
    unsigned int idx = group_hash(group->name);

    /* Hash table insert */
    GROUP_HASH_ENTRY *entry = (GROUP_HASH_ENTRY *)alloc_perm(sizeof(GROUP_HASH_ENTRY));
    entry->key   = group->name;
    entry->value = group;
    entry->next  = group_hash_tbl[idx];
    group_hash_tbl[idx] = entry;

    /* Sorted list insert (alphabetical) */
    if (!group_list || str_cmp(group->name, group_list->name) < 0) {
        group->next = group_list;
        group_list = group;
    } else {
        SKILL_GROUP *prev = group_list;
        while (prev->next && str_cmp(group->name, prev->next->name) >= 0)
            prev = prev->next;
        group->next = prev->next;
        prev->next  = group;
    }

    group_total++;
}

/***************************************************************************
 * Lookup API                                                              *
 ***************************************************************************/

/**
 * skill_group_find - Find a skill group by exact name (case-insensitive)
 *
 * @param name  Group name to find
 * @return      Pointer to SKILL_GROUP, or NULL if not found
 */
SKILL_GROUP *skill_group_find(const char *name)
{
    unsigned int idx;
    GROUP_HASH_ENTRY *entry;

    if (!name || !name[0])
        return NULL;

    idx = group_hash(name);
    for (entry = group_hash_tbl[idx]; entry; entry = entry->next) {
        if (!str_cmp(entry->key, name))
            return entry->value;
    }

    return NULL;
}

/**
 * skill_group_search - Find a skill group by prefix match (case-insensitive)
 *
 * @param prefix  Prefix to match
 * @return        First matching SKILL_GROUP, or NULL
 */
SKILL_GROUP *skill_group_search(const char *prefix)
{
    SKILL_GROUP *group;

    if (!prefix || !prefix[0])
        return NULL;

    for (group = group_list; group; group = group->next) {
        if (!str_prefix(prefix, group->name))
            return group;
    }

    return NULL;
}

/**
 * skill_group_first - Return the first group in the alphabetical list
 */
SKILL_GROUP *skill_group_first(void)
{
    return group_list;
}

/**
 * skill_group_count - Return the total number of loaded groups
 */
int skill_group_count(void)
{
    return group_total;
}

/***************************************************************************
 * Memory Management                                                       *
 ***************************************************************************/

/**
 * new_skill_group - Allocate and initialize a new SKILL_GROUP
 *
 * Returns a zeroed SKILL_GROUP with valid=true and an empty contents list.
 */
SKILL_GROUP *new_skill_group(void)
{
    SKILL_GROUP *group;

    group = (SKILL_GROUP *)alloc_perm(sizeof(SKILL_GROUP));
    memset(group, 0, sizeof(SKILL_GROUP));

    group->valid    = true;
    group->contents = list_create(false);

    return group;
}

/**
 * free_skill_group - Free a SKILL_GROUP and its contents
 */
void free_skill_group(SKILL_GROUP *group)
{
    if (!group)
        return;

    group->valid = false;
    free_string(group->name);

    if (group->contents) {
        ITERATOR it;
        char *skill_name;
        iterator_start(&it, group->contents);
        while ((skill_name = (char *)iterator_nextdata(&it))) {
            free_string(skill_name);
        }
        iterator_stop(&it);
        list_destroy(group->contents);
        group->contents = NULL;
    }

    /* Note: alloc_perm memory is not actually freed */
}

/***************************************************************************
 * JSON Loading                                                            *
 ***************************************************************************/

/**
 * group_load_json - Load a single skill group from a JSON file
 *
 * @param filename  Path to the JSON file
 * @return          Loaded SKILL_GROUP, or NULL on error
 */
static SKILL_GROUP *group_load_json(const char *filename)
{
    json_t *root, *arr, *val;
    json_error_t error;
    const char *str;
    size_t index;
    SKILL_GROUP *group;

    root = json_load_file(filename, 0, &error);
    if (!root) {
        log_stringf("group_load_json: Failed to load %s: %s", filename, error.text);
        return NULL;
    }

    if (!json_is_object(root)) {
        log_stringf("group_load_json: Root is not an object in %s", filename);
        json_decref(root);
        return NULL;
    }

    group = new_skill_group();

    /* Name */
    str = json_get_string(root, "name", "");
    if (str && str[0]) {
        free_string(group->name);
        group->name = str_dup(str);
    } else {
        log_stringf("group_load_json: Missing name in %s", filename);
        free_skill_group(group);
        json_decref(root);
        return NULL;
    }

    /* Contents (list of skill name strings) */
    arr = json_object_get(root, "skills");
    if (arr && json_is_array(arr)) {
        json_array_foreach(arr, index, val) {
            str = json_string_value(val);
            if (str && str[0]) {
                list_appendlink(group->contents, str_dup(str));
            }
        }
    }

    json_decref(root);
    return group;
}

/***************************************************************************
 * JSON Saving                                                             *
 ***************************************************************************/

/**
 * save_skill_group - Save a single SKILL_GROUP to its JSON file
 */
void save_skill_group(SKILL_GROUP *group)
{
    json_t *root, *arr;
    char path[512];
    char safe_name[256];

    if (!group || !group->name)
        return;

    /* Build safe filename from group name (replace spaces with underscores) */
    {
        const char *src = group->name;
        char *dst = safe_name;
        int len = 0;
        while (*src && len < (int)sizeof(safe_name) - 1) {
            *dst++ = (*src == ' ') ? '_' : LOWER(*src);
            src++;
            len++;
        }
        *dst = '\0';
    }

    snprintf(path, sizeof(path), "%s%s.json", SKILL_GROUPS_DIR, safe_name);

    root = json_object();

    json_object_set_new(root, "name", json_string(group->name));

    /* Skills */
    arr = json_array();
    if (group->contents && list_size(group->contents) > 0) {
        ITERATOR it;
        char *skill_name;
        iterator_start(&it, group->contents);
        while ((skill_name = (char *)iterator_nextdata(&it))) {
            json_array_append_new(arr, json_string(skill_name));
        }
        iterator_stop(&it);
    }
    json_object_set_new(root, "skills", arr);

    /* Write file */
    if (json_dump_file(root, path, JSON_INDENT(4) | JSON_SORT_KEYS) != 0) {
        log_stringf("save_skill_group: Failed to write %s", path);
    }

    json_decref(root);
}

/**
 * save_all_skill_groups - Save all groups to JSON files
 */
void save_all_skill_groups(void)
{
    SKILL_GROUP *group;

    log_string("save_all_skill_groups: saving all skill groups");

    for (group = group_list; group; group = group->next) {
        save_skill_group(group);
    }

    log_stringf("save_all_skill_groups: saved %d groups", group_total);
}

/***************************************************************************
 * Bootstrap (legacy — group_table[] removed in Phase 9)                    *
 ***************************************************************************/

/**
 * bootstrap_groups_from_table - Stub (group_table[] removed in Phase 9)
 *
 * Previously created SKILL_GROUP entries from the static group_table[].
 * Now logs an error since the table no longer exists.
 */
static void bootstrap_groups_from_table(void)
{
    log_string("bootstrap_groups_from_table: ERROR — group_table[] removed in Phase 9. "
               "Skill group JSON files must exist in data/skill_groups/.");
}

/***************************************************************************
 * Boot / Load                                                             *
 ***************************************************************************/

/**
 * load_skill_groups - Load all skill groups from JSON files
 *
 * Loads skill group definitions from data/skill_groups/ JSON files.
 * If no files exist, logs an error (group_table[] removed in Phase 9).
 */
void load_skill_groups(void)
{
    DIR *dir;
    struct dirent *ent;
    struct stat st;
    int loaded = 0;
    char filepath[512];

    log_string("load_skill_groups: Loading skill groups...");

    /* Initialize hash table */
    memset(group_hash_tbl, 0, sizeof(group_hash_tbl));

    /* Check if data directory exists */
    if (stat(SKILL_GROUPS_DIR, &st) != 0) {
        log_string("load_skill_groups: No skill_groups directory — cannot bootstrap (group_table removed)");
        bootstrap_groups_from_table();
        return;
    }

    /* Load JSON files */
    dir = opendir(SKILL_GROUPS_DIR);
    if (!dir) {
        log_stringf("load_skill_groups: Could not open %s — cannot bootstrap (group_table removed)",
                     SKILL_GROUPS_DIR);
        bootstrap_groups_from_table();
        return;
    }

    while ((ent = readdir(dir)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len < 6 || strcmp(ent->d_name + len - 5, ".json") != 0)
            continue;

        snprintf(filepath, sizeof(filepath), "%s%s", SKILL_GROUPS_DIR, ent->d_name);

        SKILL_GROUP *group = group_load_json(filepath);
        if (group) {
            group_register(group);
            loaded++;
        }
    }

    closedir(dir);

    if (loaded == 0) {
        log_string("load_skill_groups: No JSON files found — cannot bootstrap (group_table removed)");
        bootstrap_groups_from_table();
    } else {
        log_stringf("load_skill_groups: Loaded %d skill groups from JSON", loaded);
    }
}
