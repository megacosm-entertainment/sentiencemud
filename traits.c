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


/**
 * trait_def_lookup_name - Find a trait definition by ID or display name
 *
 * Tries exact ID match first, then exact name match, then prefix matching
 * on both ID and name.
 *
 * @param name  Trait identifier or display name string
 * @return      Pointer to the trait definition, or NULL if not found
 */
TRAIT_DEF *trait_def_lookup_name(const char *name)
{
    TRAIT_DEF *def;

    if (!name || !name[0])
        return NULL;

    /* Exact ID match */
    for (def = trait_def_list; def; def = def->next) {
        if (!str_cmp(def->id, name))
            return def;
    }

    /* Exact name match */
    for (def = trait_def_list; def; def = def->next) {
        if (!str_cmp(def->name, name))
            return def;
    }

    /* Prefix match on ID */
    for (def = trait_def_list; def; def = def->next) {
        if (!str_prefix(name, def->id))
            return def;
    }

    /* Prefix match on name */
    for (def = trait_def_list; def; def = def->next) {
        if (!str_prefix(name, def->name))
            return def;
    }

    return NULL;
}

/***************************************************************************
 * Generic Trait Value Helpers                                             *
 ***************************************************************************/

/**
 * trait_values_alloc - Allocate a trait_values array with defaults
 *
 * Allocates a TRAIT_VALUE array sized to trait_def_count and fills each
 * slot with the trait definition's default value.
 *
 * @return  Allocated and default-initialized array, or NULL if no traits defined
 */
TRAIT_VALUE *trait_values_alloc(void)
{
    TRAIT_VALUE *tv;
    TRAIT_DEF *def;

    if (trait_def_count == 0)
        return NULL;

    tv = (TRAIT_VALUE *)alloc_perm(sizeof(TRAIT_VALUE) * trait_def_count);
    memset(tv, 0, sizeof(TRAIT_VALUE) * trait_def_count);

    for (def = trait_def_list; def; def = def->next) {
        TRAIT_VALUE *slot = &tv[def->index];
        slot->set = false;
        switch (def->type) {
            case TRAIT_BOOLEAN:
                slot->bool_val = def->default_bool;
                break;
            case TRAIT_INTEGER:
                slot->int_val = def->default_int;
                break;
            case TRAIT_STRING:
                slot->string_val = def->default_string ?
                    str_dup(def->default_string) : NULL;
                break;
        }
    }

    return tv;
}

/**
 * trait_values_load_json - Parse trait values from a JSON "traits" object
 *
 * Reads each key from the JSON traits object, looks up the matching trait
 * definition, and sets the value on the trait_values array.
 *
 * @param tv          The trait_values array to load into (must be pre-allocated)
 * @param traits_obj  A json_t* (cast from void*) pointing to the "traits" object
 */
void trait_values_load_json(TRAIT_VALUE *tv, void *traits_obj)
{
    json_t *obj = (json_t *)traits_obj;
    const char *key;
    json_t *val;
    TRAIT_DEF *def;

    if (!tv || !obj || !json_is_object(obj))
        return;

    json_object_foreach(obj, key, val) {
        def = trait_def_lookup(key);
        if (!def)
            continue;

        TRAIT_VALUE *slot = &tv[def->index];
        slot->set = true;

        switch (def->type) {
            case TRAIT_BOOLEAN:
                slot->bool_val = json_is_true(val);
                break;
            case TRAIT_INTEGER:
                slot->int_val = (int)json_integer_value(val);
                break;
            case TRAIT_STRING: {
                const char *str = json_string_value(val);
                slot->string_val = str ? str_dup(str) : NULL;
                break;
            }
        }
    }
}

/**
 * trait_values_save_json - Build a JSON object of non-default trait values
 *
 * Creates a json_t object containing only traits that were explicitly set.
 * Returns NULL if no traits are set.
 *
 * @param tv  The trait_values array to save from
 * @return    A json_t* (cast to void*), or NULL if no traits set
 */
void *trait_values_save_json(TRAIT_VALUE *tv)
{
    json_t *obj;
    TRAIT_DEF *def;
    int count = 0;

    if (!tv || trait_def_count == 0)
        return NULL;

    obj = json_object();

    for (def = trait_def_list; def; def = def->next) {
        TRAIT_VALUE *slot = &tv[def->index];
        if (!slot->set)
            continue;

        switch (def->type) {
            case TRAIT_BOOLEAN:
                json_object_set_new(obj, def->id, json_boolean(slot->bool_val));
                break;
            case TRAIT_INTEGER:
                json_object_set_new(obj, def->id, json_integer(slot->int_val));
                break;
            case TRAIT_STRING:
                json_object_set_new(obj, def->id,
                    slot->string_val ? json_string(slot->string_val)
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
 * Race Trait Initialization                                               *
 ***************************************************************************/

/**
 * race_init_traits - Allocate and initialize trait values array for a race
 *
 * Uses trait_values_alloc() to create a default-filled TRAIT_VALUE array
 * and assigns it to the race. Must be called after load_trait_definitions().
 *
 * @param race  The race to initialize traits for
 */
void race_init_traits(RACE_DATA *race)
{
    if (!race || trait_def_count == 0)
        return;

    race->trait_values = trait_values_alloc();
}

/***************************************************************************
 * Race Trait JSON Loading                                                 *
 ***************************************************************************/

/**
 * race_load_traits_json - Parse trait values from a race's JSON "traits" object
 *
 * Delegates to trait_values_load_json with the race's trait_values array.
 * Logs warnings for unknown trait keys.
 *
 * @param race        The race to load traits onto
 * @param traits_obj  A json_t* (cast from void*) pointing to the "traits" object
 */
void race_load_traits_json(RACE_DATA *race, void *traits_obj)
{
    json_t *obj = (json_t *)traits_obj;
    const char *key;
    json_t *val;

    if (!race || !obj || !json_is_object(obj) || !race->trait_values)
        return;

    /* Log unknown traits specifically for races */
    json_object_foreach(obj, key, val) {
        TRAIT_DEF *def = trait_def_lookup(key);
        if (!def) {
            pwarnf(LOG_INIT, "Race '%s': unknown trait '%s'", race->id, key);
        }
    }

    trait_values_load_json(race->trait_values, traits_obj);
}

/***************************************************************************
 * Race Trait JSON Saving                                                  *
 ***************************************************************************/

/**
 * race_save_traits_json - Build a JSON object of non-default trait values
 *
 * Delegates to trait_values_save_json with the race's trait_values array.
 *
 * @param race  The race to save traits for
 * @return      A json_t* (cast to void*), or NULL if no traits set
 */
void *race_save_traits_json(RACE_DATA *race)
{
    if (!race || !race->trait_values || trait_def_count == 0)
        return NULL;

    return trait_values_save_json(race->trait_values);
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


/***************************************************************************
 * Class Trait API                                                         *
 ***************************************************************************/

/**
 * class_init_traits - Allocate and initialize trait values array for a class
 *
 * @param clazz  The class to initialize traits for
 */
void class_init_traits(CLASS_DATA *clazz)
{
    if (!clazz || trait_def_count == 0)
        return;

    clazz->trait_values = trait_values_alloc();
}

/**
 * class_load_traits_json - Parse trait values from a class's JSON "traits" object
 *
 * @param clazz       The class to load traits onto
 * @param traits_obj  A json_t* (cast from void*) pointing to the "traits" object
 */
void class_load_traits_json(CLASS_DATA *clazz, void *traits_obj)
{
    json_t *obj = (json_t *)traits_obj;
    const char *key;
    json_t *val;

    if (!clazz || !obj || !json_is_object(obj) || !clazz->trait_values)
        return;

    /* Log unknown traits specifically for classes */
    json_object_foreach(obj, key, val) {
        TRAIT_DEF *def = trait_def_lookup(key);
        if (!def) {
            pwarnf(LOG_INIT, "Class '%s': unknown trait '%s'", clazz->name, key);
        }
    }

    trait_values_load_json(clazz->trait_values, traits_obj);
}

/**
 * class_save_traits_json - Build a JSON object of non-default trait values
 *
 * @param clazz  The class to save traits for
 * @return       A json_t* (cast to void*), or NULL if no traits set
 */
void *class_save_traits_json(CLASS_DATA *clazz)
{
    if (!clazz || !clazz->trait_values || trait_def_count == 0)
        return NULL;

    return trait_values_save_json(clazz->trait_values);
}

/**
 * class_has_trait - Check if a class has a trait set to a non-default value
 */
bool class_has_trait(CLASS_DATA *clazz, const char *trait_id)
{
    TRAIT_DEF *def;
    TRAIT_VALUE *tv;

    if (!clazz || !clazz->trait_values || !trait_id)
        return false;

    def = trait_def_lookup(trait_id);
    if (!def)
        return false;

    tv = &clazz->trait_values[def->index];
    switch (def->type) {
        case TRAIT_BOOLEAN: return tv->bool_val;
        case TRAIT_INTEGER: return tv->int_val != 0;
        case TRAIT_STRING:  return tv->string_val != NULL;
    }
    return false;
}

/**
 * class_get_trait_bool - Get a boolean trait value from a class
 */
bool class_get_trait_bool(CLASS_DATA *clazz, const char *trait_id)
{
    TRAIT_DEF *def;

    if (!clazz || !clazz->trait_values || !trait_id)
        return false;

    def = trait_def_lookup(trait_id);
    if (!def)
        return false;

    return clazz->trait_values[def->index].bool_val;
}

/**
 * class_get_trait_int - Get an integer trait value from a class
 */
int class_get_trait_int(CLASS_DATA *clazz, const char *trait_id)
{
    TRAIT_DEF *def;

    if (!clazz || !clazz->trait_values || !trait_id)
        return 0;

    def = trait_def_lookup(trait_id);
    if (!def)
        return 0;

    return clazz->trait_values[def->index].int_val;
}

/**
 * class_get_trait_string - Get a string trait value from a class
 */
const char *class_get_trait_string(CLASS_DATA *clazz, const char *trait_id)
{
    TRAIT_DEF *def;

    if (!clazz || !clazz->trait_values || !trait_id)
        return NULL;

    def = trait_def_lookup(trait_id);
    if (!def)
        return NULL;

    return clazz->trait_values[def->index].string_val;
}

/***************************************************************************
 * Personal (Character) Trait API                                          *
 ***************************************************************************/

/**
 * char_init_traits - Allocate and initialize personal trait values for a PC
 *
 * @param ch  The character to initialize traits for (must have pcdata)
 */
void char_init_traits(CHAR_DATA *ch)
{
    if (!ch || !ch->pcdata || trait_def_count == 0)
        return;

    ch->pcdata->trait_values = trait_values_alloc();
}

/**
 * char_load_traits_json - Parse personal trait values from a character's JSON
 *
 * @param ch          The character to load traits onto
 * @param traits_obj  A json_t* (cast from void*) pointing to the "traits" object
 */
void char_load_traits_json(CHAR_DATA *ch, void *traits_obj)
{
    if (!ch || !ch->pcdata || !ch->pcdata->trait_values)
        return;

    trait_values_load_json(ch->pcdata->trait_values, traits_obj);
}

/**
 * char_save_traits_json - Build a JSON object of personal trait overrides
 *
 * @param ch  The character to save traits for
 * @return    A json_t* (cast to void*), or NULL if no personal traits set
 */
void *char_save_traits_json(CHAR_DATA *ch)
{
    if (!ch || !ch->pcdata || !ch->pcdata->trait_values || trait_def_count == 0)
        return NULL;

    return trait_values_save_json(ch->pcdata->trait_values);
}

/***************************************************************************
 * Unified Character Trait Query API (layered: personal > class > race)    *
 *                                                                         *
 * These check all three trait layers in priority order:                    *
 *   1. Personal overrides (PC_DATA.trait_values)                          *
 *   2. Current class traits (CLASS_DATA.trait_values)                     *
 *   3. Race traits (RACE_DATA.trait_values)                               *
 *                                                                         *
 * For booleans: returns true if ANY layer has it set to true (OR).        *
 * For integers: returns the value from the highest-priority layer that    *
 *               has the trait explicitly set; falls back to race default. *
 * For strings:  same as integers — highest-priority set value wins.       *
 ***************************************************************************/

/**
 * ch_has_trait - Check if a character has a trait active at any layer
 *
 * @param ch        The character to check
 * @param trait_id  Trait identifier string
 * @return          True if the trait is active at any layer
 */
bool ch_has_trait(CHAR_DATA *ch, const char *trait_id)
{
    TRAIT_DEF *def;

    if (!ch || !trait_id)
        return false;

    def = trait_def_lookup(trait_id);
    if (!def)
        return false;

    /* Personal override (PC only) */
    if (ch->pcdata && ch->pcdata->trait_values) {
        TRAIT_VALUE *tv = &ch->pcdata->trait_values[def->index];
        if (tv->set) {
            switch (def->type) {
                case TRAIT_BOOLEAN: if (tv->bool_val) return true; break;
                case TRAIT_INTEGER: if (tv->int_val != 0) return true; break;
                case TRAIT_STRING:  if (tv->string_val) return true; break;
            }
        }
    }

    /* Class traits (current class, PC only) */
    if (ch->pcdata && ch->pcdata->current_class && ch->pcdata->current_class->clazz) {
        CLASS_DATA *clazz = ch->pcdata->current_class->clazz;
        if (clazz->trait_values) {
            TRAIT_VALUE *tv = &clazz->trait_values[def->index];
            if (tv->set) {
                switch (def->type) {
                    case TRAIT_BOOLEAN: if (tv->bool_val) return true; break;
                    case TRAIT_INTEGER: if (tv->int_val != 0) return true; break;
                    case TRAIT_STRING:  if (tv->string_val) return true; break;
                }
            }
        }
    }

    /* Race traits */
    if (ch->race && ch->race->trait_values) {
        TRAIT_VALUE *tv = &ch->race->trait_values[def->index];
        switch (def->type) {
            case TRAIT_BOOLEAN: return tv->bool_val;
            case TRAIT_INTEGER: return tv->int_val != 0;
            case TRAIT_STRING:  return tv->string_val != NULL;
        }
    }

    return false;
}

/**
 * ch_get_trait_bool - Get a boolean trait value across all layers
 *
 * Returns true if ANY layer (personal, class, or race) has the trait set
 * to true. This provides OR semantics: a character with a race that has
 * blood_feeding OR a class that grants blood_feeding will return true.
 *
 * @param ch        The character to query
 * @param trait_id  Trait identifier string
 * @return          True if any layer has the trait set to true
 */
bool ch_get_trait_bool(CHAR_DATA *ch, const char *trait_id)
{
    TRAIT_DEF *def;

    if (!ch || !trait_id)
        return false;

    def = trait_def_lookup(trait_id);
    if (!def)
        return false;

    /* Personal override */
    if (ch->pcdata && ch->pcdata->trait_values
        && ch->pcdata->trait_values[def->index].set
        && ch->pcdata->trait_values[def->index].bool_val)
        return true;

    /* Class traits */
    if (ch->pcdata && ch->pcdata->current_class && ch->pcdata->current_class->clazz) {
        CLASS_DATA *clazz = ch->pcdata->current_class->clazz;
        if (clazz->trait_values
            && clazz->trait_values[def->index].set
            && clazz->trait_values[def->index].bool_val)
            return true;
    }

    /* Race traits (fallback) */
    if (ch->race && ch->race->trait_values)
        return ch->race->trait_values[def->index].bool_val;

    return false;
}

/**
 * ch_get_trait_int - Get an integer trait value across all layers
 *
 * Returns the value from the highest-priority layer where the trait is
 * explicitly set. Priority: personal > class > race.
 *
 * @param ch        The character to query
 * @param trait_id  Trait identifier string
 * @return          The integer trait value, or 0 if not found
 */
int ch_get_trait_int(CHAR_DATA *ch, const char *trait_id)
{
    TRAIT_DEF *def;

    if (!ch || !trait_id)
        return 0;

    def = trait_def_lookup(trait_id);
    if (!def)
        return 0;

    /* Personal override */
    if (ch->pcdata && ch->pcdata->trait_values
        && ch->pcdata->trait_values[def->index].set)
        return ch->pcdata->trait_values[def->index].int_val;

    /* Class traits */
    if (ch->pcdata && ch->pcdata->current_class && ch->pcdata->current_class->clazz) {
        CLASS_DATA *clazz = ch->pcdata->current_class->clazz;
        if (clazz->trait_values && clazz->trait_values[def->index].set)
            return clazz->trait_values[def->index].int_val;
    }

    /* Race traits (fallback) */
    if (ch->race && ch->race->trait_values)
        return ch->race->trait_values[def->index].int_val;

    return 0;
}

/**
 * ch_get_trait_string - Get a string trait value across all layers
 *
 * Returns the value from the highest-priority layer where the trait is
 * explicitly set. Priority: personal > class > race.
 *
 * @param ch        The character to query
 * @param trait_id  Trait identifier string
 * @return          The string trait value, or NULL if not found
 */
const char *ch_get_trait_string(CHAR_DATA *ch, const char *trait_id)
{
    TRAIT_DEF *def;

    if (!ch || !trait_id)
        return NULL;

    def = trait_def_lookup(trait_id);
    if (!def)
        return NULL;

    /* Personal override */
    if (ch->pcdata && ch->pcdata->trait_values
        && ch->pcdata->trait_values[def->index].set)
        return ch->pcdata->trait_values[def->index].string_val;

    /* Class traits */
    if (ch->pcdata && ch->pcdata->current_class && ch->pcdata->current_class->clazz) {
        CLASS_DATA *clazz = ch->pcdata->current_class->clazz;
        if (clazz->trait_values && clazz->trait_values[def->index].set)
            return clazz->trait_values[def->index].string_val;
    }

    /* Race traits (fallback) */
    if (ch->race && ch->race->trait_values)
        return ch->race->trait_values[def->index].string_val;

    return NULL;
}

/***************************************************************************
 * Trait Definition Saving                                                 *
 ***************************************************************************/

/**
 * save_trait_definitions - Write all trait definitions to traits.json
 *
 * Serializes the global trait_def_list back to the JSON format used
 * by load_trait_definitions(). Overwrites the existing file.
 *
 * @return  true on success, false on failure
 */
bool save_trait_definitions(void)
{
    json_t *root, *traits_arr, *trait_obj;
    TRAIT_DEF *def;

    root = json_object();
    json_object_set_new(root, "_format", json_string("trait_definitions"));
    json_object_set_new(root, "_version", json_integer(1));

    traits_arr = json_array();

    for (def = trait_def_list; def; def = def->next) {
        if (!def->valid)
            continue;

        trait_obj = json_object();
        json_object_set_new(trait_obj, "id", json_string(def->id));
        json_object_set_new(trait_obj, "name", json_string(def->name ? def->name : ""));
        json_object_set_new(trait_obj, "description", json_string(def->description ? def->description : ""));
        json_object_set_new(trait_obj, "category", json_string(def->category ? def->category : ""));

        switch (def->type) {
            case TRAIT_BOOLEAN:
                json_object_set_new(trait_obj, "type", json_string("boolean"));
                json_object_set_new(trait_obj, "default", json_boolean(def->default_bool));
                break;
            case TRAIT_INTEGER:
                json_object_set_new(trait_obj, "type", json_string("integer"));
                json_object_set_new(trait_obj, "default", json_integer(def->default_int));
                break;
            case TRAIT_STRING:
                json_object_set_new(trait_obj, "type", json_string("string"));
                json_object_set_new(trait_obj, "default",
                    def->default_string ? json_string(def->default_string) : json_null());
                break;
        }

        json_array_append_new(traits_arr, trait_obj);
    }

    json_object_set_new(root, "traits", traits_arr);

    if (json_dump_file(root, TRAITS_FILE, JSON_INDENT(2)) != 0) {
        pbugf(LOG_ERROR, "save_trait_definitions: Failed to write " TRAITS_FILE);
        json_decref(root);
        return false;
    }

    json_decref(root);
    return true;
}
