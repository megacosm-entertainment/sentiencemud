/***************************************************************************
 * traits.c - Trait system implementation                                  *
 *                                                                         *
 * Data-driven trait system that replaces hardcoded IS_RACE() macros.      *
 * Traits are defined in data/traits/traits.json, assigned per-race in     *
 * each race's JSON file, and queried at runtime via indexed array lookup. *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include "merc.h"
#include "traits.h"

#define TRAITS_FILE	DATA_DIR "traits/traits.json"

/***************************************************************************
 * Globals                                                                 *
 ***************************************************************************/

TRAIT_DEF *	trait_def_list = NULL;
int		trait_def_count = 0;

/***************************************************************************
 * Trait Definition Loading                                                *
 ***************************************************************************/

/**
 * load_trait_definitions - Load trait definitions from traits.json
 *
 * Parses the trait definition file and builds the global trait_def_list
 * with sequential indices for array-based lookup. Must be called before
 * load_races() so that races can reference trait definitions.
 */
void load_trait_definitions(void)
{
    json_t *root, *traits_arr, *trait_obj, *val;
    json_error_t error;
    const char *str;
    size_t index;
    TRAIT_DEF *def, *last = NULL;

    log_string("Loading trait definitions...");

    trait_def_list = NULL;
    trait_def_count = 0;

    root = json_load_file(TRAITS_FILE, 0, &error);
    if (!root) {
        pwarnf(LOG_INIT, "Could not load " TRAITS_FILE ": %s", error.text);
        return;
    }

    str = json_string_value(json_object_get(root, "_format"));
    if (!str || str_cmp(str, "trait_definitions")) {
        pbugf(LOG_INIT, "Invalid format in " TRAITS_FILE);
        json_decref(root);
        return;
    }

    traits_arr = json_object_get(root, "traits");
    if (!traits_arr || !json_is_array(traits_arr)) {
        pbugf(LOG_INIT, "No traits array in " TRAITS_FILE);
        json_decref(root);
        return;
    }

    json_array_foreach(traits_arr, index, trait_obj) {
        if (!json_is_object(trait_obj))
            continue;

        def = (TRAIT_DEF *)alloc_perm(sizeof(TRAIT_DEF));
        memset(def, 0, sizeof(TRAIT_DEF));
        def->valid = true;
        def->index = trait_def_count;

        str = json_string_value(json_object_get(trait_obj, "id"));
        def->id = str_dup(str ? str : "unknown");

        str = json_string_value(json_object_get(trait_obj, "name"));
        def->name = str_dup(str ? str : def->id);

        str = json_string_value(json_object_get(trait_obj, "description"));
        def->description = str_dup(str ? str : "");

        str = json_string_value(json_object_get(trait_obj, "category"));
        def->category = str_dup(str ? str : "");

        str = json_string_value(json_object_get(trait_obj, "type"));
        if (!str || !str_cmp(str, "boolean"))
            def->type = TRAIT_BOOLEAN;
        else if (!str_cmp(str, "integer"))
            def->type = TRAIT_INTEGER;
        else if (!str_cmp(str, "string"))
            def->type = TRAIT_STRING;
        else
            def->type = TRAIT_BOOLEAN;

        val = json_object_get(trait_obj, "default");
        switch (def->type) {
            case TRAIT_BOOLEAN:
                def->default_bool = json_is_true(val);
                break;
            case TRAIT_INTEGER:
                def->default_int = val ? (int)json_integer_value(val) : 0;
                break;
            case TRAIT_STRING:
                str = json_string_value(val);
                def->default_string = str ? str_dup(str) : NULL;
                break;
        }

        if (!trait_def_list) {
            trait_def_list = def;
        } else {
            last->next = def;
        }
        last = def;
        def->next = NULL;

        trait_def_count++;
    }

    json_decref(root);
    log_stringf("Loaded %d trait definitions.", trait_def_count);
}

/***************************************************************************
 * Trait Definition Lookup                                                 *
 ***************************************************************************/

/**
 * trait_def_lookup - Find a trait definition by its string ID
 *
 * @param id  Trait identifier string
 * @return    Pointer to the trait definition, or NULL if not found
 */
TRAIT_DEF *trait_def_lookup(const char *id)
{
    TRAIT_DEF *def;

    if (!id || !id[0])
        return NULL;

    for (def = trait_def_list; def; def = def->next) {
        if (!str_cmp(def->id, id))
            return def;
    }

    return NULL;
}

/***************************************************************************
 * Race Trait Initialization                                               *
 ***************************************************************************/

/**
 * race_init_traits - Allocate and initialize trait values array for a race
 *
 * Allocates a TRAIT_VALUE array sized to trait_def_count and fills each
 * slot with the trait definition's default value. Must be called after
 * load_trait_definitions().
 *
 * @param race  The race to initialize traits for
 */
void race_init_traits(RACE_DATA *race)
{
    TRAIT_DEF *def;

    if (!race || trait_def_count == 0)
        return;

    race->trait_values = (TRAIT_VALUE *)alloc_perm(
        sizeof(TRAIT_VALUE) * trait_def_count);
    memset(race->trait_values, 0, sizeof(TRAIT_VALUE) * trait_def_count);

    for (def = trait_def_list; def; def = def->next) {
        TRAIT_VALUE *tv = &race->trait_values[def->index];
        tv->set = false;
        switch (def->type) {
            case TRAIT_BOOLEAN:
                tv->bool_val = def->default_bool;
                break;
            case TRAIT_INTEGER:
                tv->int_val = def->default_int;
                break;
            case TRAIT_STRING:
                tv->string_val = def->default_string ?
                    str_dup(def->default_string) : NULL;
                break;
        }
    }
}

/***************************************************************************
 * Race Trait JSON Loading                                                 *
 ***************************************************************************/

/**
 * race_load_traits_json - Parse trait values from a race's JSON "traits" object
 *
 * Reads each key from the JSON traits object, looks up the matching trait
 * definition, and sets the value on the race's trait_values array.
 *
 * @param race        The race to load traits onto
 * @param traits_obj  A json_t* (cast from void*) pointing to the "traits" object
 */
void race_load_traits_json(RACE_DATA *race, void *traits_obj)
{
    json_t *obj = (json_t *)traits_obj;
    const char *key;
    json_t *val;
    TRAIT_DEF *def;
    TRAIT_VALUE *tv;

    if (!race || !obj || !json_is_object(obj) || !race->trait_values)
        return;

    json_object_foreach(obj, key, val) {
        def = trait_def_lookup(key);
        if (!def) {
            pwarnf(LOG_INIT, "Race '%s': unknown trait '%s'", race->id, key);
            continue;
        }

        tv = &race->trait_values[def->index];
        tv->set = true;

        switch (def->type) {
            case TRAIT_BOOLEAN:
                tv->bool_val = json_is_true(val);
                break;
            case TRAIT_INTEGER:
                tv->int_val = (int)json_integer_value(val);
                break;
            case TRAIT_STRING: {
                const char *str = json_string_value(val);
                tv->string_val = str ? str_dup(str) : NULL;
                break;
            }
        }
    }
}

/***************************************************************************
 * Race Trait JSON Saving                                                  *
 ***************************************************************************/

/**
 * race_save_traits_json - Build a JSON object of non-default trait values
 *
 * Creates a json_t object containing only traits that were explicitly set
 * on this race (differ from defaults). Returns NULL if no traits are set.
 *
 * @param race  The race to save traits for
 * @return      A json_t* (cast to void*), or NULL if no traits set
 */
void *race_save_traits_json(RACE_DATA *race)
{
    json_t *obj;
    TRAIT_DEF *def;
    TRAIT_VALUE *tv;
    int count = 0;

    if (!race || !race->trait_values || trait_def_count == 0)
        return NULL;

    obj = json_object();

    for (def = trait_def_list; def; def = def->next) {
        tv = &race->trait_values[def->index];
        if (!tv->set)
            continue;

        switch (def->type) {
            case TRAIT_BOOLEAN:
                json_object_set_new(obj, def->id,
                    json_boolean(tv->bool_val));
                break;
            case TRAIT_INTEGER:
                json_object_set_new(obj, def->id,
                    json_integer(tv->int_val));
                break;
            case TRAIT_STRING:
                json_object_set_new(obj, def->id,
                    tv->string_val ? json_string(tv->string_val)
                                   : json_null());
                break;
        }
        count++;
    }

    if (count == 0) {
        json_decref(obj);
        return NULL;
    }

    return obj;
}

/***************************************************************************
 * Race Trait Query API                                                    *
 ***************************************************************************/

/**
 * race_has_trait - Check if a race has a trait set to a non-default value
 *
 * For booleans, returns the trait's boolean value.
 * For integers, returns true if non-zero.
 * For strings, returns true if non-NULL.
 *
 * @param race      The race to check
 * @param trait_id  Trait identifier string
 * @return          True if the trait is active/set
 */
bool race_has_trait(RACE_DATA *race, const char *trait_id)
{
    TRAIT_DEF *def;
    TRAIT_VALUE *tv;

    if (!race || !race->trait_values || !trait_id)
        return false;

    def = trait_def_lookup(trait_id);
    if (!def)
        return false;

    tv = &race->trait_values[def->index];
    switch (def->type) {
        case TRAIT_BOOLEAN:
            return tv->bool_val;
        case TRAIT_INTEGER:
            return tv->int_val != 0;
        case TRAIT_STRING:
            return tv->string_val != NULL;
    }

    return false;
}

/**
 * race_get_trait_bool - Get a boolean trait value from a race
 *
 * @param race      The race to query
 * @param trait_id  Trait identifier string
 * @return          The trait's boolean value, or false if not found
 */
bool race_get_trait_bool(RACE_DATA *race, const char *trait_id)
{
    TRAIT_DEF *def;

    if (!race || !race->trait_values || !trait_id)
        return false;

    def = trait_def_lookup(trait_id);
    if (!def)
        return false;

    return race->trait_values[def->index].bool_val;
}

/**
 * race_get_trait_int - Get an integer trait value from a race
 *
 * @param race      The race to query
 * @param trait_id  Trait identifier string
 * @return          The trait's integer value, or 0 if not found
 */
int race_get_trait_int(RACE_DATA *race, const char *trait_id)
{
    TRAIT_DEF *def;

    if (!race || !race->trait_values || !trait_id)
        return 0;

    def = trait_def_lookup(trait_id);
    if (!def)
        return 0;

    return race->trait_values[def->index].int_val;
}

/**
 * race_get_trait_string - Get a string trait value from a race
 *
 * @param race      The race to query
 * @param trait_id  Trait identifier string
 * @return          The trait's string value, or NULL if not found
 */
const char *race_get_trait_string(RACE_DATA *race, const char *trait_id)
{
    TRAIT_DEF *def;

    if (!race || !race->trait_values || !trait_id)
        return NULL;

    def = trait_def_lookup(trait_id);
    if (!def)
        return NULL;

    return race->trait_values[def->index].string_val;
}
