/***************************************************************************
 *  File: json_socials.c                                                   *
 *                                                                         *
 *  JSON serialization/deserialization for the socials table.              *
 *                                                                         *
 *  Socials are player emote commands (smile, wave, etc.) stored in a      *
 *  global array. This module handles saving and loading socials.json.     *
 *                                                                         *
 *  File format: data/system/socials.json                                  *
 *  Version: 1                                                             *
 *                                                                         *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include "../../merc.h"
#include "../../log.h"
#include "json_socials.h"

extern int social_count;
extern struct social_type social_table[MAX_SOCIALS];

/**
 * json_social_serialize - Convert a single social to JSON
 *
 * @param social  Pointer to the social_type to serialize
 * @return        JSON object or NULL on error (caller must json_decref)
 */
static json_t *json_social_serialize(struct social_type *social)
{
    json_t *obj;

    if (!social)
        return NULL;

    obj = json_object();
    if (!obj)
        return NULL;

    json_object_set_new(obj, "name", json_string(social->name));

    if (social->char_no_arg)
        json_object_set_new(obj, "char_no_arg", json_string(social->char_no_arg));
    else
        json_object_set_new(obj, "char_no_arg", json_null());

    if (social->others_no_arg)
        json_object_set_new(obj, "others_no_arg", json_string(social->others_no_arg));
    else
        json_object_set_new(obj, "others_no_arg", json_null());

    if (social->char_found)
        json_object_set_new(obj, "char_found", json_string(social->char_found));
    else
        json_object_set_new(obj, "char_found", json_null());

    if (social->others_found)
        json_object_set_new(obj, "others_found", json_string(social->others_found));
    else
        json_object_set_new(obj, "others_found", json_null());

    if (social->vict_found)
        json_object_set_new(obj, "vict_found", json_string(social->vict_found));
    else
        json_object_set_new(obj, "vict_found", json_null());

    if (social->char_not_found)
        json_object_set_new(obj, "char_not_found", json_string(social->char_not_found));
    else
        json_object_set_new(obj, "char_not_found", json_null());

    if (social->char_auto)
        json_object_set_new(obj, "char_auto", json_string(social->char_auto));
    else
        json_object_set_new(obj, "char_auto", json_null());

    if (social->others_auto)
        json_object_set_new(obj, "others_auto", json_string(social->others_auto));
    else
        json_object_set_new(obj, "others_auto", json_null());

    return obj;
}

/**
 * json_social_deserialize - Parse a single social from JSON
 *
 * Populates a social_type structure from JSON. Uses str_dup for string
 * fields; NULL JSON values map to NULL pointers.
 *
 * @param json    JSON object containing social data
 * @param social  Pointer to social_type to populate
 * @return        true on success, false on error
 */
static bool json_social_deserialize(json_t *json, struct social_type *social)
{
    json_t *value;
    const char *str;

    if (!json || !json_is_object(json) || !social)
        return false;

    memset(social, 0, sizeof(struct social_type));

    value = json_object_get(json, "name");
    if (!value || !json_is_string(value))
        return false;

    str = json_string_value(value);
    if (!str || !str[0])
        return false;

    strncpy(social->name, str, sizeof(social->name) - 1);
    social->name[sizeof(social->name) - 1] = '\0';

    value = json_object_get(json, "char_no_arg");
    if (value && json_is_string(value))
        social->char_no_arg = str_dup(json_string_value(value));
    else
        social->char_no_arg = NULL;

    value = json_object_get(json, "others_no_arg");
    if (value && json_is_string(value))
        social->others_no_arg = str_dup(json_string_value(value));
    else
        social->others_no_arg = NULL;

    value = json_object_get(json, "char_found");
    if (value && json_is_string(value))
        social->char_found = str_dup(json_string_value(value));
    else
        social->char_found = NULL;

    value = json_object_get(json, "others_found");
    if (value && json_is_string(value))
        social->others_found = str_dup(json_string_value(value));
    else
        social->others_found = NULL;

    value = json_object_get(json, "vict_found");
    if (value && json_is_string(value))
        social->vict_found = str_dup(json_string_value(value));
    else
        social->vict_found = NULL;

    value = json_object_get(json, "char_not_found");
    if (value && json_is_string(value))
        social->char_not_found = str_dup(json_string_value(value));
    else
        social->char_not_found = NULL;

    value = json_object_get(json, "char_auto");
    if (value && json_is_string(value))
        social->char_auto = str_dup(json_string_value(value));
    else
        social->char_auto = NULL;

    value = json_object_get(json, "others_auto");
    if (value && json_is_string(value))
        social->others_auto = str_dup(json_string_value(value));
    else
        social->others_auto = NULL;

    return true;
}

/**
 * json_save_socials - Write all socials to a JSON file
 *
 * Serializes social_table[0..social_count-1] and writes to the
 * specified path with pretty-printed formatting.
 *
 * @param path  File path to write to
 * @return      true on success, false on error
 */
bool json_save_socials(const char *path)
{
    FILE *fp;
    json_t *root, *socials_array;
    char *json_str;
    int i;

    root = json_object();
    if (!root) {
        pbugf(LOG_ERROR, "json_save_socials: Failed to create JSON root object");
        return false;
    }

    json_object_set_new(root, "version", json_integer(1));
    json_object_set_new(root, "count", json_integer(social_count));

    socials_array = json_array();
    if (!socials_array) {
        json_decref(root);
        pbugf(LOG_ERROR, "json_save_socials: Failed to create socials array");
        return false;
    }

    for (i = 0; i < social_count; i++) {
        json_t *social_obj = json_social_serialize(&social_table[i]);
        if (social_obj)
            json_array_append_new(socials_array, social_obj);
    }

    json_object_set_new(root, "socials", socials_array);

    fp = fopen(path, "w");
    if (!fp) {
        json_decref(root);
        pbugf(LOG_ERROR, "json_save_socials: Failed to open %s for writing", path);
        return false;
    }

    json_str = json_dumps(root, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
    if (!json_str) {
        fclose(fp);
        json_decref(root);
        pbugf(LOG_ERROR, "json_save_socials: Failed to serialize JSON");
        return false;
    }

    fprintf(fp, "%s\n", json_str);

    free(json_str);
    fclose(fp);
    json_decref(root);

    plogf(LOG_INFO, "Saved %d socials to JSON", social_count);

    return true;
}

/**
 * json_load_socials - Load socials from a JSON file
 *
 * Parses the specified JSON file and populates social_table and
 * social_count. Validates each entry and skips invalid ones.
 *
 * @param path  File path to read from
 * @return      true on success, false if file doesn't exist or parse error
 */
bool json_load_socials(const char *path)
{
    json_t *root, *socials_array, *social_obj;
    json_error_t error;
    size_t i, count;
    int version;
    int loaded = 0;

    root = json_load_file(path, 0, &error);
    if (!root)
        return false;

    if (!json_is_object(root)) {
        json_decref(root);
        pbugf(LOG_ERROR, "json_load_socials: Root is not a JSON object");
        return false;
    }

    version = json_integer_value(json_object_get(root, "version"));
    if (version != 1) {
        json_decref(root);
        pbugf(LOG_ERROR, "json_load_socials: Unsupported version %d", version);
        return false;
    }

    socials_array = json_object_get(root, "socials");
    if (!socials_array || !json_is_array(socials_array)) {
        json_decref(root);
        pbugf(LOG_ERROR, "json_load_socials: No socials array found");
        return false;
    }

    count = json_array_size(socials_array);
    if (count > MAX_SOCIALS) {
        plogf(LOG_WARN, "json_load_socials: File contains %zu socials, max is %d; truncating",
              count, MAX_SOCIALS);
        count = MAX_SOCIALS;
    }

    for (i = 0; i < count; i++) {
        social_obj = json_array_get(socials_array, i);
        if (!social_obj || !json_is_object(social_obj))
            continue;

        if (json_social_deserialize(social_obj, &social_table[loaded]))
            loaded++;
    }

    social_count = loaded;

    json_decref(root);

    plogf(LOG_INFO, "Loaded %d socials from JSON", social_count);

    return true;
}
