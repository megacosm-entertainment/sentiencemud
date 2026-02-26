#include <jansson.h>
#include <string.h>

#include "../../merc.h"
#include "../../tables.h"
#include "json_common.h"
#include "json_corpse.h"

#define CORPSE_JSON_FILE SYSTEM_DIR "corpses.json"
#define CORPSE_JSON_FORMAT "corpse_types"
#define CORPSE_JSON_VERSION 1

static bool corpse_strings_owned = false;

static void corpse_set_string(char **dst, const char *src)
{
    if (!dst)
        return;

    if (corpse_strings_owned && *dst)
        free_string(*dst);

    if (src)
        *dst = str_dup(src);
    else
        *dst = NULL;
}

static void corpse_ensure_runtime_strings(void)
{
    int i;

    if (corpse_strings_owned)
        return;

    for (i = 0; i < corpse_type_count(); i++) {
        struct corpse_info *corpse = &corpse_info_table[i];

        corpse->name = str_dup(corpse->name);
        corpse->short_descr = str_dup(corpse->short_descr);
        corpse->long_descr = str_dup(corpse->long_descr);
        corpse->full_descr = str_dup(corpse->full_descr);
        corpse->short_headless = str_dup(corpse->short_headless);
        corpse->long_headless = str_dup(corpse->long_headless);
        corpse->full_headless = str_dup(corpse->full_headless);

        if (corpse->animate_name)
            corpse->animate_name = str_dup(corpse->animate_name);
        if (corpse->animate_long)
            corpse->animate_long = str_dup(corpse->animate_long);
        if (corpse->animate_descr)
            corpse->animate_descr = str_dup(corpse->animate_descr);
        if (corpse->victim_message)
            corpse->victim_message = str_dup(corpse->victim_message);
        if (corpse->room_message)
            corpse->room_message = str_dup(corpse->room_message);
        if (corpse->decay_message)
            corpse->decay_message = str_dup(corpse->decay_message);
        if (corpse->skull_success)
            corpse->skull_success = str_dup(corpse->skull_success);
        if (corpse->skull_success_other)
            corpse->skull_success_other = str_dup(corpse->skull_success_other);
        if (corpse->skull_fail)
            corpse->skull_fail = str_dup(corpse->skull_fail);
        if (corpse->skull_fail_other)
            corpse->skull_fail_other = str_dup(corpse->skull_fail_other);
    }

    corpse_strings_owned = true;
}

static void corpse_json_set_nullable_string(json_t *obj, const char *key, const char *value)
{
    if (!obj || !key)
        return;

    if (!value)
        json_object_set_new(obj, key, json_null());
    else
        json_object_set_new(obj, key, json_string(value));
}

static const char *corpse_json_get_nullable_string(json_t *obj, const char *key)
{
    json_t *value;

    if (!obj || !key)
        return NULL;

    value = json_object_get(obj, key);
    if (!value || json_is_null(value) || !json_is_string(value))
        return NULL;

    return json_string_value(value);
}

static json_t *corpse_type_to_json(const struct corpse_info *corpse, int type)
{
    json_t *obj = json_object();

    json_object_set_new(obj, "type", json_integer(type));

    corpse_json_set_nullable_string(obj, "name", corpse->name);
    corpse_json_set_nullable_string(obj, "short_descr", corpse->short_descr);
    corpse_json_set_nullable_string(obj, "long_descr", corpse->long_descr);
    corpse_json_set_nullable_string(obj, "full_descr", corpse->full_descr);
    corpse_json_set_nullable_string(obj, "short_headless", corpse->short_headless);
    corpse_json_set_nullable_string(obj, "long_headless", corpse->long_headless);
    corpse_json_set_nullable_string(obj, "full_headless", corpse->full_headless);
    corpse_json_set_nullable_string(obj, "animate_name", corpse->animate_name);
    corpse_json_set_nullable_string(obj, "animate_long", corpse->animate_long);
    corpse_json_set_nullable_string(obj, "animate_descr", corpse->animate_descr);
    corpse_json_set_nullable_string(obj, "victim_message", corpse->victim_message);
    corpse_json_set_nullable_string(obj, "room_message", corpse->room_message);
    corpse_json_set_nullable_string(obj, "decay_message", corpse->decay_message);
    corpse_json_set_nullable_string(obj, "skull_success", corpse->skull_success);
    corpse_json_set_nullable_string(obj, "skull_success_other", corpse->skull_success_other);
    corpse_json_set_nullable_string(obj, "skull_fail", corpse->skull_fail);
    corpse_json_set_nullable_string(obj, "skull_fail_other", corpse->skull_fail_other);

    json_object_set_new(obj, "owner_loot", json_boolean(corpse->owner_loot));
    json_object_set_new(obj, "headless", json_boolean(corpse->headless));
    json_object_set_new(obj, "animate_headless", json_boolean(corpse->animate_headless));
    json_object_set_new(obj, "resurrect_chance", json_integer(corpse->resurrect_chance));
    json_object_set_new(obj, "animation_chance", json_integer(corpse->animation_chance));
    json_object_set_new(obj, "skulling_chance", json_integer(corpse->skulling_chance));
    json_object_set_new(obj, "decay_type", json_integer(corpse->decay_type));
    json_object_set_new(obj, "decay_rate", json_integer(corpse->decay_rate));
    json_object_set_new(obj, "decay_npctimer_min", json_integer(corpse->decay_npctimer_min));
    json_object_set_new(obj, "decay_npctimer_max", json_integer(corpse->decay_npctimer_max));
    json_object_set_new(obj, "decay_pctimer_min", json_integer(corpse->decay_pctimer_min));
    json_object_set_new(obj, "decay_pctimer_max", json_integer(corpse->decay_pctimer_max));
    json_object_set_new(obj, "decay_spill_chance", json_integer(corpse->decay_spill_chance));
    json_object_set_new(obj, "lost_bodyparts", json_integer(corpse->lost_bodyparts));

    return obj;
}

static void corpse_type_from_json(json_t *obj)
{
    struct corpse_info *corpse;
    int type;
    int count = corpse_type_count();

    if (!obj || !json_is_object(obj))
        return;

    type = (int)json_get_int(obj, "type", -1);
    if (type < 0 || type >= count)
        return;

    corpse = &corpse_info_table[type];

    corpse_set_string(&corpse->name, corpse_json_get_nullable_string(obj, "name"));
    corpse_set_string(&corpse->short_descr, corpse_json_get_nullable_string(obj, "short_descr"));
    corpse_set_string(&corpse->long_descr, corpse_json_get_nullable_string(obj, "long_descr"));
    corpse_set_string(&corpse->full_descr, corpse_json_get_nullable_string(obj, "full_descr"));
    corpse_set_string(&corpse->short_headless, corpse_json_get_nullable_string(obj, "short_headless"));
    corpse_set_string(&corpse->long_headless, corpse_json_get_nullable_string(obj, "long_headless"));
    corpse_set_string(&corpse->full_headless, corpse_json_get_nullable_string(obj, "full_headless"));
    corpse_set_string(&corpse->animate_name, corpse_json_get_nullable_string(obj, "animate_name"));
    corpse_set_string(&corpse->animate_long, corpse_json_get_nullable_string(obj, "animate_long"));
    corpse_set_string(&corpse->animate_descr, corpse_json_get_nullable_string(obj, "animate_descr"));
    corpse_set_string(&corpse->victim_message, corpse_json_get_nullable_string(obj, "victim_message"));
    corpse_set_string(&corpse->room_message, corpse_json_get_nullable_string(obj, "room_message"));
    corpse_set_string(&corpse->decay_message, corpse_json_get_nullable_string(obj, "decay_message"));
    corpse_set_string(&corpse->skull_success, corpse_json_get_nullable_string(obj, "skull_success"));
    corpse_set_string(&corpse->skull_success_other, corpse_json_get_nullable_string(obj, "skull_success_other"));
    corpse_set_string(&corpse->skull_fail, corpse_json_get_nullable_string(obj, "skull_fail"));
    corpse_set_string(&corpse->skull_fail_other, corpse_json_get_nullable_string(obj, "skull_fail_other"));

    corpse->owner_loot = json_get_bool(obj, "owner_loot", corpse->owner_loot);
    corpse->headless = json_get_bool(obj, "headless", corpse->headless);
    corpse->animate_headless = json_get_bool(obj, "animate_headless", corpse->animate_headless);
    corpse->resurrect_chance = (int)json_get_int(obj, "resurrect_chance", corpse->resurrect_chance);
    corpse->animation_chance = (int)json_get_int(obj, "animation_chance", corpse->animation_chance);
    corpse->skulling_chance = (int)json_get_int(obj, "skulling_chance", corpse->skulling_chance);
    corpse->decay_type = (int)json_get_int(obj, "decay_type", corpse->decay_type);
    corpse->decay_rate = (int)json_get_int(obj, "decay_rate", corpse->decay_rate);
    corpse->decay_npctimer_min = (int)json_get_int(obj, "decay_npctimer_min", corpse->decay_npctimer_min);
    corpse->decay_npctimer_max = (int)json_get_int(obj, "decay_npctimer_max", corpse->decay_npctimer_max);
    corpse->decay_pctimer_min = (int)json_get_int(obj, "decay_pctimer_min", corpse->decay_pctimer_min);
    corpse->decay_pctimer_max = (int)json_get_int(obj, "decay_pctimer_max", corpse->decay_pctimer_max);
    corpse->decay_spill_chance = (int)json_get_int(obj, "decay_spill_chance", corpse->decay_spill_chance);
    corpse->lost_bodyparts = (int)json_get_int(obj, "lost_bodyparts", corpse->lost_bodyparts);
}

bool json_corpse_save_types(int *saved_count)
{
    json_t *root = json_object();
    json_t *array = json_array();
    int count = corpse_type_count();
    int i;

    corpse_ensure_runtime_strings();

    json_object_set_new(root, "_format", json_string(CORPSE_JSON_FORMAT));
    json_object_set_new(root, "_version", json_integer(CORPSE_JSON_VERSION));

    for (i = 0; i < count; i++)
        json_array_append_new(array, corpse_type_to_json(&corpse_info_table[i], i));

    json_object_set_new(root, "corpses", array);

    if (saved_count)
        *saved_count = count;

    return json_file_save(root, CORPSE_JSON_FILE, "json_corpse_save_types",
        JSON_INDENT(2) | JSON_PRESERVE_ORDER);
}

bool json_corpse_load_types(int *loaded_count)
{
    json_t *root;
    json_t *array = NULL;
    size_t index;
    json_t *entry;
    int loaded = 0;
    int count = corpse_type_count();

    corpse_ensure_runtime_strings();

    root = json_file_load(CORPSE_JSON_FILE, "corpses", &array,
        "json_corpse_load_types");
    if (!root) {
        if (loaded_count)
            *loaded_count = 0;
        return false;
    }

    json_array_foreach(array, index, entry) {
        int before = loaded;
        corpse_type_from_json(entry);
        if (json_is_object(entry) && json_get_int(entry, "type", -1) >= 0
            && json_get_int(entry, "type", -1) < count)
            loaded = before + 1;
    }

    if (loaded_count)
        *loaded_count = loaded;

    json_decref(root);
    return true;
}
