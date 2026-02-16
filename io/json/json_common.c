/***************************************************************************
 *  JSON Common Utilities - Implementation                                 *
 *                                                                         *
 *  Shared helper functions for JSON serialization/deserialization.        *
 *  See json_common.h for full API documentation.                          *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <jansson.h>
#include "../../merc.h"
#include "../../tables.h"
#include "json_common.h"

/***************************************************************************
 * Flag / Enum Conversion                                                  *
 ***************************************************************************/

json_t *json_flags_serialize(long bits, const struct flag_type *flag_table)
{
    json_t *array;
    int i;

    array = json_array();

    if (!flag_table)
        return array;

    for (i = 0; flag_table[i].name != NULL; i++) {
        if (!is_stat(flag_table) && IS_SET(bits, flag_table[i].bit)) {
            json_array_append_new(array, json_string(flag_table[i].name));
        } else if (is_stat(flag_table) && flag_table[i].bit == bits) {
            json_array_append_new(array, json_string(flag_table[i].name));
            break;
        }
    }

    return array;
}

long json_flags_deserialize(json_t *array, const struct flag_type *flag_table)
{
    long bits = 0;
    size_t index;
    json_t *value;
    const char *flag_name;

    if (!flag_table || !array || !json_is_array(array))
        return 0;

    json_array_foreach(array, index, value) {
        flag_name = json_string_value(value);
        if (!flag_name) continue;

        for (int i = 0; flag_table[i].name != NULL; i++) {
            if (!str_cmp(flag_table[i].name, flag_name)) {
                SET_BIT(bits, flag_table[i].bit);
                break;
            }
        }
    }

    return bits;
}

void json_enum_serialize(json_t *obj, const char *key, long val,
                         const struct flag_type *flag_table)
{
    for (int i = 0; flag_table[i].name != NULL; i++) {
        if (flag_table[i].bit == val) {
            json_object_set_new(obj, key, json_string(flag_table[i].name));
            return;
        }
    }
    json_object_set_new(obj, key, json_integer((json_int_t)val));
}

long json_enum_deserialize(json_t *obj, const char *key,
                           const struct flag_type *flag_table)
{
    json_t *v = json_object_get(obj, key);
    if (!v) return 0;

    if (json_is_string(v)) {
        const char *name = json_string_value(v);
        for (int i = 0; flag_table[i].name != NULL; i++) {
            if (!str_cmp(flag_table[i].name, name))
                return flag_table[i].bit;
        }
        return 0;
    }

    if (json_is_integer(v))
        return (long)json_integer_value(v);

    return 0;
}

/***************************************************************************
 * Value Getters with Defaults                                             *
 ***************************************************************************/

const char *json_get_string(json_t *obj, const char *key, const char *default_val)
{
    json_t *value = json_object_get(obj, key);
    if (!value || !json_is_string(value))
        return default_val;
    return json_string_value(value);
}

long json_get_int(json_t *obj, const char *key, long default_val)
{
    json_t *value = json_object_get(obj, key);
    if (!value || !json_is_integer(value))
        return default_val;
    return json_integer_value(value);
}

bool json_get_bool(json_t *obj, const char *key, bool default_val)
{
    json_t *value = json_object_get(obj, key);
    if (!value || !json_is_boolean(value))
        return default_val;
    return json_is_true(value);
}

/***************************************************************************
 * WNUM String Conversion                                                  *
 ***************************************************************************/

json_t *json_wnum_serialize(long area_uid, long vnum)
{
    char buf[256];

    if (area_uid > 0) {
        snprintf(buf, sizeof(buf), "%ld#%ld", area_uid, vnum);
    } else {
        snprintf(buf, sizeof(buf), "%ld", vnum);
    }

    return json_string(buf);
}

void json_wnum_deserialize(json_t *json, long *area_uid, long *vnum)
{
    const char *str;

    *area_uid = 0;
    *vnum = 0;

    if (!json || !json_is_string(json))
        return;

    str = json_string_value(json);
    if (!str)
        return;

    if (strchr(str, '#')) {
        sscanf(str, "%ld#%ld", area_uid, vnum);
    } else {
        *vnum = atol(str);
    }
}

/***************************************************************************
 * File I/O Helpers                                                        *
 ***************************************************************************/

bool json_file_is_json(const char *filename)
{
    FILE *fp;
    char first_char;
    bool is_json;

    fp = fopen(filename, "r");
    if (!fp)
        return false;

    first_char = fgetc(fp);
    is_json = (first_char == '{');
    fclose(fp);

    return is_json;
}

bool json_file_save(json_t *root, const char *path, const char *context, int flags)
{
    int ret;

    if (!root || !path) {
        if (root) json_decref(root);
        return false;
    }

    ret = json_dump_file(root, path, flags);
    json_decref(root);

    if (ret != 0) {
        log_stringf("%s: Failed to write %s", context, path);
        return false;
    }

    return true;
}

json_t *json_file_load(const char *path, const char *array_key,
                       json_t **out_array, const char *context)
{
    json_error_t error;
    json_t *root;

    root = json_load_file(path, 0, &error);
    if (!root) {
        log_stringf("%s: parse error in %s on line %d: %s",
                    context, path, error.line, error.text);
        return NULL;
    }

    if (array_key && out_array) {
        *out_array = json_object_get(root, array_key);
        if (!*out_array || !json_is_array(*out_array)) {
            log_stringf("%s: missing or invalid '%s' array in %s",
                        context, array_key, path);
            json_decref(root);
            return NULL;
        }
    }

    return root;
}

/***************************************************************************
 * Directory Helpers                                                       *
 ***************************************************************************/

bool json_ensure_dir(const char *base_dir, const char *name)
{
    char dir_path[256];

    if (!base_dir || !name || !name[0])
        return false;

    snprintf(dir_path, sizeof(dir_path), "%s%c",
             base_dir, tolower(name[0]));

    mkdir(dir_path, 0755);
    return true;
}
