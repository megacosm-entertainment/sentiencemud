/***************************************************************************
 *  JSON Race Loading - Implementation                                     *
 *  Loads race data from JSON files in data/races/                         *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <jansson.h>
#include "../../merc.h"
#include "../../tables.h"
#include "../../recycle.h"

#define RACES_DIR	DATA_DIR "races/"

/***************************************************************************
 * Global Race Data                                                        *
 ***************************************************************************/

RACE_DATA *		race_list = NULL;
int			race_count = 0;
RACE_HASH_ENTRY *	race_hash_table[RACE_HASH_SIZE];

/* UID index for O(1) lookup by UID */
static RACE_DATA **	race_uid_index = NULL;
static int		max_race_uid = 0;

/***************************************************************************
 * Hash Table Implementation                                               *
 ***************************************************************************/

/* FNV-1a hash function */
static unsigned int race_hash(const char *id)
{
    unsigned int hash = 2166136261u;
    while (*id) {
        hash ^= (unsigned char)LOWER(*id);
        id++;
        hash *= 16777619u;
    }
    return hash % RACE_HASH_SIZE;
}

static void race_hash_insert(RACE_DATA *race)
{
    unsigned int idx;
    RACE_HASH_ENTRY *entry;

    if (!race || !race->id)
        return;

    idx = race_hash(race->id);

    entry = (RACE_HASH_ENTRY *)alloc_perm(sizeof(RACE_HASH_ENTRY));
    entry->key = race->id;
    entry->value = race;
    entry->next = race_hash_table[idx];
    race_hash_table[idx] = entry;
}

/***************************************************************************
 * Memory Management                                                       *
 ***************************************************************************/

RACE_DATA *new_race_data(void)
{
    RACE_DATA *race;
    int i;

    race = (RACE_DATA *)alloc_perm(sizeof(RACE_DATA));
    memset(race, 0, sizeof(RACE_DATA));

    race->valid = true;
    race->skills = list_create(false);

    /* Initialize stats to defaults */
    for (i = 0; i < MAX_STATS; i++) {
        race->stats[i] = 13;
        race->max_stats[i] = 18;
    }
    race->max_vitals[0] = 3000;  /* HP */
    race->max_vitals[1] = 3000;  /* Mana */
    race->max_vitals[2] = 3000;  /* Move */
    race->min_size = SIZE_MEDIUM;
    race->max_size = SIZE_MEDIUM;

    return race;
}

void free_race_data(RACE_DATA *race)
{
    if (!race)
        return;

    /* Note: Since we use alloc_perm, we don't actually free the memory.
     * This function is here for completeness and future changes. */
    race->valid = false;
}

/***************************************************************************
 * Flag Parsing Utilities                                                  *
 ***************************************************************************/

static long flags_from_json_array(const struct flag_type *flag_table, json_t *flags_array)
{
    long bits = 0;
    size_t index;
    json_t *value;
    const char *flag_name;

    if (!flag_table || !flags_array || !json_is_array(flags_array)) {
        return 0;
    }

    json_array_foreach(flags_array, index, value) {
        flag_name = json_string_value(value);
        if (!flag_name) continue;

        /* Look up flag by name */
        for (int i = 0; flag_table[i].name != NULL; i++) {
            if (!str_cmp(flag_table[i].name, flag_name)) {
                SET_BIT(bits, flag_table[i].bit);
                break;
            }
        }
    }

    return bits;
}

/***************************************************************************
 * JSON Race Loading                                                       *
 ***************************************************************************/

static RACE_DATA *race_load_json(const char *filename)
{
    json_t *root, *obj, *arr, *val;
    json_error_t error;
    RACE_DATA *race;
    const char *str;
    size_t index;

    root = json_load_file(filename, 0, &error);
    if (!root) {
        bug("race_load_json: Error loading race file", 0);
        log_string(filename);
        log_string(error.text);
        return NULL;
    }

    /* Validate format */
    str = json_string_value(json_object_get(root, "_format"));
    if (!str || str_cmp(str, "race_data")) {
        bug("race_load_json: Invalid format in race file", 0);
        log_string(filename);
        json_decref(root);
        return NULL;
    }

    race = new_race_data();

    /* Core identification */
    str = json_string_value(json_object_get(root, "id"));
    race->id = str_dup(str ? str : "unknown");

    val = json_object_get(root, "uid");
    race->uid = val ? (int16_t)json_integer_value(val) : 0;

    str = json_string_value(json_object_get(root, "name"));
    race->name = str_dup(str ? str : race->id);

    str = json_string_value(json_object_get(root, "description"));
    race->description = str_dup(str ? str : "");

    str = json_string_value(json_object_get(root, "comments"));
    race->comments = str_dup(str ? str : "");

    /* Playability flags */
    race->playable = json_is_true(json_object_get(root, "playable"));
    race->starting = json_is_true(json_object_get(root, "starting"));

    /* Display section */
    obj = json_object_get(root, "display");
    if (obj && json_is_object(obj)) {
        str = json_string_value(json_object_get(obj, "who_name"));
        race->who_name = str_dup(str ? str : "");
    } else {
        race->who_name = str_dup("");
    }

    /* Physical section */
    obj = json_object_get(root, "physical");
    if (obj && json_is_object(obj)) {
        val = json_object_get(obj, "min_size");
        race->min_size = val ? json_integer_value(val) : SIZE_MEDIUM;

        val = json_object_get(obj, "max_size");
        race->max_size = val ? json_integer_value(val) : race->min_size;

        val = json_object_get(obj, "default_alignment");
        race->default_alignment = val ? json_integer_value(val) : 0;

        race->form = flags_from_json_array(form_flags, json_object_get(obj, "form"));
        race->parts = flags_from_json_array(part_flags, json_object_get(obj, "parts"));
    }

    /* Combat section */
    obj = json_object_get(root, "combat");
    if (obj && json_is_object(obj)) {
        race->act[0] = flags_from_json_array(act_flags, json_object_get(obj, "act"));
        race->act[1] = flags_from_json_array(act2_flags, json_object_get(obj, "act2"));
        race->aff[0] = flags_from_json_array(affect_flags, json_object_get(obj, "affects"));
        race->aff[1] = flags_from_json_array(affect2_flags, json_object_get(obj, "affects2"));
        race->off = flags_from_json_array(off_flags, json_object_get(obj, "offensive"));
        race->imm = flags_from_json_array(imm_flags, json_object_get(obj, "immunities"));
        race->res = flags_from_json_array(res_flags, json_object_get(obj, "resistances"));
        race->vuln = flags_from_json_array(vuln_flags, json_object_get(obj, "vulnerabilities"));
    }

    /* Attributes section */
    obj = json_object_get(root, "attributes");
    if (obj && json_is_object(obj)) {
        json_t *stats = json_object_get(obj, "stats");
        if (stats && json_is_object(stats)) {
            val = json_object_get(stats, "str");
            if (val) race->stats[STAT_STR] = json_integer_value(val);
            val = json_object_get(stats, "int");
            if (val) race->stats[STAT_INT] = json_integer_value(val);
            val = json_object_get(stats, "wis");
            if (val) race->stats[STAT_WIS] = json_integer_value(val);
            val = json_object_get(stats, "dex");
            if (val) race->stats[STAT_DEX] = json_integer_value(val);
            val = json_object_get(stats, "con");
            if (val) race->stats[STAT_CON] = json_integer_value(val);
        }

        stats = json_object_get(obj, "max_stats");
        if (stats && json_is_object(stats)) {
            val = json_object_get(stats, "str");
            if (val) race->max_stats[STAT_STR] = json_integer_value(val);
            val = json_object_get(stats, "int");
            if (val) race->max_stats[STAT_INT] = json_integer_value(val);
            val = json_object_get(stats, "wis");
            if (val) race->max_stats[STAT_WIS] = json_integer_value(val);
            val = json_object_get(stats, "dex");
            if (val) race->max_stats[STAT_DEX] = json_integer_value(val);
            val = json_object_get(stats, "con");
            if (val) race->max_stats[STAT_CON] = json_integer_value(val);
        }

        json_t *vitals = json_object_get(obj, "max_vitals");
        if (vitals && json_is_object(vitals)) {
            val = json_object_get(vitals, "hp");
            if (val) race->max_vitals[0] = json_integer_value(val);
            val = json_object_get(vitals, "mana");
            if (val) race->max_vitals[1] = json_integer_value(val);
            val = json_object_get(vitals, "move");
            if (val) race->max_vitals[2] = json_integer_value(val);
        }
    }

    /* Skills array */
    arr = json_object_get(root, "skills");
    if (arr && json_is_array(arr)) {
        json_array_foreach(arr, index, val) {
            str = json_string_value(val);
            if (str && str[0]) {
                list_appendlink(race->skills, str_dup(str));
            }
        }
    }

    /* Starting equipment */
    arr = json_object_get(root, "starting_equipment");
    if (arr && json_is_array(arr)) {
        size_t count = json_array_size(arr);
        if (count > MAX_RACE_STARTING_EQ)
            count = MAX_RACE_STARTING_EQ;
        for (index = 0; index < count; index++) {
            val = json_array_get(arr, index);
            if (val) {
                race->starting_eq[index] = json_integer_value(val);
            }
        }
    }

    /* Remort section */
    obj = json_object_get(root, "remort");
    if (obj && json_is_object(obj)) {
        str = json_string_value(json_object_get(obj, "prerequisite_race"));
        if (str && str[0]) {
            race->remort_race_id = str_dup(str);
            race->starting = false;  /* Remort races are not starting races */
        }

        str = json_string_value(json_object_get(obj, "remort_into"));
        if (str && str[0]) {
            race->remort_into_id = str_dup(str);
        }
    }

    json_decref(root);
    return race;
}

/***************************************************************************
 * Race System Initialization                                              *
 ***************************************************************************/

void load_races(void)
{
    DIR *dir;
    struct dirent *entry;
    char path[512];  // Increased from 256 to handle longer paths safely
    RACE_DATA *race, *last;
    int i;

    log_string("Loading races from JSON files...");

    /* Initialize hash table */
    for (i = 0; i < RACE_HASH_SIZE; i++) {
        race_hash_table[i] = NULL;
    }

    race_list = NULL;
    last = NULL;
    race_count = 0;
    max_race_uid = 0;

    /* Open races directory */
    dir = opendir(RACES_DIR);
    if (!dir) {
        bug("load_races: Cannot open races directory: %s", 0);
        log_string(RACES_DIR);
        return;
    }

    /* Load all .json files */
    while ((entry = readdir(dir)) != NULL) {
        /* Skip if not a .json file */
        if (strlen(entry->d_name) < 6)
            continue;
        if (str_cmp(entry->d_name + strlen(entry->d_name) - 5, ".json"))
            continue;

        snprintf(path, sizeof(path), "%s%s", RACES_DIR, entry->d_name);
        race = race_load_json(path);

        if (race) {
            /* Add to linked list */
            if (!race_list) {
                race_list = race;
            } else {
                last->next = race;
            }
            last = race;
            race->next = NULL;

            /* Track max UID */
            if (race->uid > max_race_uid)
                max_race_uid = race->uid;

            race_count++;
        }
    }
    closedir(dir);

    if (race_count == 0) {
        bug("load_races: No races loaded!", 0);
        return;
    }

    /* Build UID index for O(1) lookup */
    race_uid_index = (RACE_DATA **)alloc_perm(sizeof(RACE_DATA *) * (max_race_uid + 1));
    for (i = 0; i <= max_race_uid; i++) {
        race_uid_index[i] = NULL;
    }

    /* Populate hash table and UID index */
    for (race = race_list; race; race = race->next) {
        race_hash_insert(race);
        if (race->uid >= 0 && race->uid <= max_race_uid) {
            race_uid_index[race->uid] = race;
        }
    }

    log_stringf("Loaded %d races (max UID: %d)", race_count, max_race_uid);
}

void free_races(void)
{
    /* Since we use alloc_perm, nothing to free */
    race_list = NULL;
    race_count = 0;
}

/***************************************************************************
 * Race Lookup Functions                                                   *
 ***************************************************************************/

RACE_DATA *race_lookup(const char *id)
{
    unsigned int idx;
    RACE_HASH_ENTRY *entry;

    if (!id || !id[0])
        return NULL;

    idx = race_hash(id);
    entry = race_hash_table[idx];

    while (entry) {
        if (!str_cmp(entry->key, id))
            return entry->value;
        entry = entry->next;
    }

    return NULL;
}

RACE_DATA *race_lookup_uid(int16_t uid)
{
    if (uid < 0 || uid > max_race_uid || !race_uid_index)
        return NULL;

    return race_uid_index[uid];
}

RACE_DATA *race_lookup_name(const char *name)
{
    RACE_DATA *race;

    if (!name || !name[0])
        return NULL;

    /* First try exact match */
    for (race = race_list; race; race = race->next) {
        if (!str_cmp(race->name, name))
            return race;
    }

    /* Then try prefix match */
    for (race = race_list; race; race = race->next) {
        if (!str_prefix(name, race->name))
            return race;
    }

    /* Also try matching by id */
    for (race = race_list; race; race = race->next) {
        if (!str_prefix(name, race->id))
            return race;
    }

    return NULL;
}

bool race_is_remort(RACE_DATA *race)
{
    if (!race)
        return false;

    return (race->remort_race_id != NULL && race->remort_race_id[0] != '\0');
}

RACE_DATA *race_get_remort_into(RACE_DATA *race)
{
    if (!race || !race->remort_into_id)
        return NULL;

    return race_lookup(race->remort_into_id);
}

RACE_DATA *race_get_prerequisite(RACE_DATA *race)
{
    if (!race || !race->remort_race_id)
        return NULL;

    return race_lookup(race->remort_race_id);
}

bool race_has_skill(RACE_DATA *race, const char *skill_name)
{
    ITERATOR it;
    char *skill;

    if (!race || !race->skills || !skill_name)
        return false;

    iterator_start(&it, race->skills);
    while ((skill = (char *)iterator_nextdata(&it))) {
        if (!str_cmp(skill, skill_name)) {
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);
    return false;
}
