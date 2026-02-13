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
#include "../../traits.h"

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

/**
 * race_hash_refresh_key - Update the hash table key for a race
 *
 * After an in-place reload, the hash entry's key pointer becomes stale
 * because race_copy_fields frees the old id string. This refreshes
 * the key to point to the race's current id.
 *
 * @param race  Race whose hash entry key should be refreshed
 */
static void race_hash_refresh_key(RACE_DATA *race)
{
    unsigned int idx = race_hash(race->id);
    RACE_HASH_ENTRY *entry = race_hash_table[idx];

    while (entry) {
        if (entry->value == race) {
            entry->key = race->id;
            return;
        }
        entry = entry->next;
    }
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
    race->trait_values = NULL;

    race_init_traits(race);

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
        pbugf(LOG_INIT, "Error loading race file: %s", filename);
        pbugf(LOG_INIT, "Error details: %s", error.text);
        return NULL;
    }

    /* Validate format */
    str = json_string_value(json_object_get(root, "_format"));
    if (!str || str_cmp(str, "race_data")) {
        pbugf(LOG_INIT, "Invalid format in race file: %s", filename);
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

    /* Traits section */
    obj = json_object_get(root, "traits");
    if (obj && json_is_object(obj)) {
        race_load_traits_json(race, obj);
    }

    json_decref(root);
    return race;
}

/***************************************************************************
 * Hot-Reload                                                              *
 ***************************************************************************/

/**
 * race_copy_fields - Copy all data fields from src to dst in-place
 *
 * Preserves dst's linked-list pointer (next), valid flag, and uid.
 * Transfers ownership of src's allocated memory.
 *
 * @param dst   Existing race struct to update
 * @param src   Temporary race struct with new data (will be gutted)
 */
static void race_copy_fields(RACE_DATA *dst, RACE_DATA *src)
{
    /* Preserve structural fields */
    RACE_DATA *saved_next = dst->next;
    int16_t saved_uid = dst->uid;

    /* Free old strings on dst (alloc_perm'd strings use free_string) */
    free_string(dst->id);
    free_string(dst->name);
    free_string(dst->description);
    free_string(dst->comments);
    free_string(dst->who_name);
    free_string(dst->remort_race_id);
    free_string(dst->remort_into_id);

    if (dst->skills) {
        ITERATOR it;
        char *skill_name;
        iterator_start(&it, dst->skills);
        while ((skill_name = (char *)iterator_nextdata(&it)))
            free_string(skill_name);
        iterator_stop(&it);
        list_destroy(dst->skills);
    }

    if (dst->trait_values) {
        for (int i = 0; i < trait_def_count; i++) {
            if (dst->trait_values[i].string_val)
                free_string(dst->trait_values[i].string_val);
        }
    }

    /* Copy identity */
    dst->id = src->id;
    dst->name = src->name;
    dst->description = src->description;
    dst->comments = src->comments;
    dst->who_name = src->who_name;

    /* Copy flags */
    dst->playable = src->playable;
    dst->starting = src->starting;

    /* Copy combat/physical properties */
    dst->act[0] = src->act[0];
    dst->act[1] = src->act[1];
    dst->aff[0] = src->aff[0];
    dst->aff[1] = src->aff[1];
    dst->off = src->off;
    dst->imm = src->imm;
    dst->res = src->res;
    dst->vuln = src->vuln;
    dst->form = src->form;
    dst->parts = src->parts;

    /* Copy attributes */
    for (int i = 0; i < MAX_STATS; i++) {
        dst->stats[i] = src->stats[i];
        dst->max_stats[i] = src->max_stats[i];
    }
    for (int i = 0; i < 3; i++)
        dst->max_vitals[i] = src->max_vitals[i];

    dst->min_size = src->min_size;
    dst->max_size = src->max_size;
    dst->default_alignment = src->default_alignment;

    /* Copy skills list */
    dst->skills = src->skills;

    /* Copy starting equipment */
    for (int i = 0; i < MAX_RACE_STARTING_EQ; i++)
        dst->starting_eq[i] = src->starting_eq[i];

    /* Copy remort */
    dst->remort_race_id = src->remort_race_id;
    dst->remort_into_id = src->remort_into_id;

    /* Copy traits */
    dst->trait_values = src->trait_values;

    /* Restore structural fields */
    dst->next = saved_next;
    dst->uid = saved_uid;
    dst->valid = true;

    /* Null out src to prevent double-free */
    src->id = NULL;
    src->name = NULL;
    src->description = NULL;
    src->comments = NULL;
    src->who_name = NULL;
    src->remort_race_id = NULL;
    src->remort_into_id = NULL;
    src->skills = NULL;
    src->trait_values = NULL;
}

/**
 * race_reload - Reload a race definition from its JSON file
 *
 * If the race already exists, updates it in-place (preserving all
 * pointers). If it doesn't exist, loads it as a new race and registers it.
 *
 * @param id    Race ID (matches the JSON filename, e.g. "human")
 * @return      The (re)loaded RACE_DATA, or NULL on failure
 */
RACE_DATA *race_reload(const char *id)
{
    char path[512];
    RACE_DATA *existing, *temp;

    if (!id || !id[0])
        return NULL;

    /* Build path: data/races/<id>.json */
    snprintf(path, sizeof(path), "%s%s.json", RACES_DIR, id);

    /* Parse the JSON file */
    temp = race_load_json(path);
    if (!temp) {
        log_stringf("race_reload: Failed to parse %s", path);
        return NULL;
    }

    /* Check if race already exists by ID */
    existing = race_lookup(temp->id);

    if (existing) {
        /* In-place update */
        race_copy_fields(existing, temp);

        /* Refresh the hash table key — race_copy_fields freed the old
         * id string that the hash entry was pointing to. */
        race_hash_refresh_key(existing);

        log_stringf("race_reload: Reloaded race '%s' (uid %d) in-place",
                     existing->name, existing->uid);
        return existing;
    } else {
        /* New race — assign UID if needed, add to list and indexes */
        if (temp->uid <= 0)
            temp->uid = ++max_race_uid;

        /* Append to linked list */
        if (!race_list) {
            race_list = temp;
        } else {
            RACE_DATA *last = race_list;
            while (last->next)
                last = last->next;
            last->next = temp;
        }
        temp->next = NULL;

        /* Add to hash table and UID index */
        race_hash_insert(temp);

        /* Grow UID index if needed */
        if (temp->uid > max_race_uid) {
            max_race_uid = temp->uid;
        }
        /* Note: UID index may need realloc for new entries beyond boot-time max.
         * For safety, we skip UID indexing if the index wasn't allocated large enough.
         * This only affects uid-based lookup for brand-new races added at runtime. */
        if (race_uid_index && temp->uid <= max_race_uid) {
            race_uid_index[temp->uid] = temp;
        }

        race_count++;

        log_stringf("race_reload: Loaded new race '%s' (uid %d)",
                     temp->name, temp->uid);
        return temp;
    }
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
        pbugf(LOG_INIT, "Could not access RACES_DIR at %s", RACES_DIR);
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
        pbugf(LOG_INIT, "No races loaded! Check if there are valid JSON files in %s", RACES_DIR);
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


/***************************************************************************
 * JSON Race Saving                                                        *
 ***************************************************************************/

/**
 * flags_to_json_array - Convert a bitmask to a JSON array of flag name strings
 *
 * Inverse of flags_from_json_array. Iterates the flag table and adds
 * the name of each flag whose bit is set.
 *
 * @param flag_table  The flag_type table to use for name lookup
 * @param bits        The bitmask to convert
 * @return            A json_t array of strings (caller must decref)
 */
static json_t *flags_to_json_array(const struct flag_type *flag_table, long bits)
{
    json_t *arr = json_array();

    if (!flag_table || bits == 0)
        return arr;

    for (int i = 0; flag_table[i].name != NULL; i++) {
        if (flag_table[i].bit != 0 && IS_SET(bits, flag_table[i].bit))
            json_array_append_new(arr, json_string(flag_table[i].name));
    }

    return arr;
}


/**
 * save_race_json - Save a race to its JSON file in data/races/
 *
 * Writes the race data in the same JSON format that race_load_json reads.
 * The file is written to DATA_DIR "races/<race_id>.json".
 *
 * @param race  The race to save
 * @return      true on success, false on failure
 */
bool save_race_json(RACE_DATA *race)
{
    json_t *root, *obj, *arr;
    char path[512];
    ITERATOR it;
    char *skill;

    if (!race || !race->id) {
        pbugf(LOG_ERROR, "save_race_json: NULL race or id");
        return false;
    }

    root = json_object();

    /* Metadata */
    json_object_set_new(root, "_format", json_string("race_data"));
    json_object_set_new(root, "_version", json_integer(1));

    /* Core identification */
    json_object_set_new(root, "id", json_string(race->id));
    json_object_set_new(root, "uid", json_integer(race->uid));
    json_object_set_new(root, "name", json_string(race->name ? race->name : ""));
    json_object_set_new(root, "description", json_string(race->description ? race->description : ""));
    json_object_set_new(root, "comments", json_string(race->comments ? race->comments : ""));

    /* Playability */
    json_object_set_new(root, "playable", json_boolean(race->playable));
    json_object_set_new(root, "starting", json_boolean(race->starting));

    /* Display */
    obj = json_object();
    json_object_set_new(obj, "who_name", json_string(race->who_name ? race->who_name : ""));
    json_object_set_new(root, "display", obj);

    /* Physical */
    obj = json_object();
    json_object_set_new(obj, "min_size", json_integer(race->min_size));
    json_object_set_new(obj, "max_size", json_integer(race->max_size));
    json_object_set_new(obj, "default_alignment", json_integer(race->default_alignment));
    json_object_set_new(obj, "form", flags_to_json_array(form_flags, race->form));
    json_object_set_new(obj, "parts", flags_to_json_array(part_flags, race->parts));
    json_object_set_new(root, "physical", obj);

    /* Combat */
    obj = json_object();
    json_object_set_new(obj, "act", flags_to_json_array(act_flags, race->act[0]));
    json_object_set_new(obj, "act2", flags_to_json_array(act2_flags, race->act[1]));
    json_object_set_new(obj, "affects", flags_to_json_array(affect_flags, race->aff[0]));
    json_object_set_new(obj, "affects2", flags_to_json_array(affect2_flags, race->aff[1]));
    json_object_set_new(obj, "offensive", flags_to_json_array(off_flags, race->off));
    json_object_set_new(obj, "immunities", flags_to_json_array(imm_flags, race->imm));
    json_object_set_new(obj, "resistances", flags_to_json_array(res_flags, race->res));
    json_object_set_new(obj, "vulnerabilities", flags_to_json_array(vuln_flags, race->vuln));
    json_object_set_new(root, "combat", obj);

    /* Attributes */
    obj = json_object();
    {
        json_t *stats = json_object();
        json_object_set_new(stats, "str", json_integer(race->stats[STAT_STR]));
        json_object_set_new(stats, "int", json_integer(race->stats[STAT_INT]));
        json_object_set_new(stats, "wis", json_integer(race->stats[STAT_WIS]));
        json_object_set_new(stats, "dex", json_integer(race->stats[STAT_DEX]));
        json_object_set_new(stats, "con", json_integer(race->stats[STAT_CON]));
        json_object_set_new(obj, "stats", stats);

        json_t *max_stats = json_object();
        json_object_set_new(max_stats, "str", json_integer(race->max_stats[STAT_STR]));
        json_object_set_new(max_stats, "int", json_integer(race->max_stats[STAT_INT]));
        json_object_set_new(max_stats, "wis", json_integer(race->max_stats[STAT_WIS]));
        json_object_set_new(max_stats, "dex", json_integer(race->max_stats[STAT_DEX]));
        json_object_set_new(max_stats, "con", json_integer(race->max_stats[STAT_CON]));
        json_object_set_new(obj, "max_stats", max_stats);

        json_t *vitals = json_object();
        json_object_set_new(vitals, "hp", json_integer(race->max_vitals[0]));
        json_object_set_new(vitals, "mana", json_integer(race->max_vitals[1]));
        json_object_set_new(vitals, "move", json_integer(race->max_vitals[2]));
        json_object_set_new(obj, "max_vitals", vitals);
    }
    json_object_set_new(root, "attributes", obj);

    /* Skills */
    arr = json_array();
    if (race->skills) {
        iterator_start(&it, race->skills);
        while ((skill = (char *)iterator_nextdata(&it))) {
            json_array_append_new(arr, json_string(skill));
        }
        iterator_stop(&it);
    }
    json_object_set_new(root, "skills", arr);

    /* Starting equipment */
    arr = json_array();
    for (int i = 0; i < MAX_RACE_STARTING_EQ; i++) {
        if (race->starting_eq[i] != 0)
            json_array_append_new(arr, json_integer(race->starting_eq[i]));
    }
    json_object_set_new(root, "starting_equipment", arr);

    /* Remort */
    obj = json_object();
    if (race->remort_race_id && race->remort_race_id[0])
        json_object_set_new(obj, "prerequisite_race", json_string(race->remort_race_id));
    if (race->remort_into_id && race->remort_into_id[0])
        json_object_set_new(obj, "remort_into", json_string(race->remort_into_id));
    json_object_set_new(root, "remort", obj);

    /* Traits */
    {
        json_t *traits = (json_t *)race_save_traits_json(race);
        if (traits)
            json_object_set_new(root, "traits", traits);
    }

    /* Write to file */
    snprintf(path, sizeof(path), "%sraces/%s.json", DATA_DIR, race->id);

    if (json_dump_file(root, path, JSON_INDENT(2) | JSON_SORT_KEYS) != 0) {
        pbugf(LOG_ERROR, "save_race_json: Failed to write %s", path);
        json_decref(root);
        return false;
    }

    json_decref(root);
    return true;
}
