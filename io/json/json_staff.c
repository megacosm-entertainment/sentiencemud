/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvements copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefiting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*	Sentience MUD improvements copyright (C) 2000-2026                *
*       Nibelung Enterprises                                              *
*       All Rights Reserved                                               *
***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include "../../merc.h"
#include "../../log.h"
#include "json_staff.h"

extern IMMORTAL_DATA *immortal_list;

/**
 * json_immortal_serialize - Serialize an immortal entry to JSON
 *
 * @param immortal  Immortal data to serialize
 * @return JSON object or NULL on error
 */
json_t *json_immortal_serialize(IMMORTAL_DATA *immortal)
{
    if (!immortal) return NULL;

    json_t *json = json_object();

    json_object_set_new(json, "name", json_string(immortal->name ? immortal->name : ""));
    json_object_set_new(json, "duties", json_integer(immortal->duties));
    json_object_set_new(json, "created", json_integer((json_int_t)immortal->created));
    json_object_set_new(json, "imm_flag", json_string(immortal->imm_flag ? immortal->imm_flag : ""));
    json_object_set_new(json, "last_olc_command", json_integer((json_int_t)immortal->last_olc_command));

    if (immortal->leader != NULL && str_cmp(immortal->leader, "None"))
        json_object_set_new(json, "leader", json_string(immortal->leader));
    else
        json_object_set_new(json, "leader", json_null());

    json_object_set_new(json, "bamfin", json_string(immortal->bamfin ? immortal->bamfin : ""));
    json_object_set_new(json, "bamfout", json_string(immortal->bamfout ? immortal->bamfout : ""));

    return json;
}

/**
 * json_immortal_deserialize - Deserialize an immortal entry from JSON
 *
 * @param json  JSON object to deserialize
 * @return New IMMORTAL_DATA or NULL on error
 */
IMMORTAL_DATA *json_immortal_deserialize(json_t *json)
{
    if (!json) return NULL;

    IMMORTAL_DATA *immortal = new_immortal();

    const char *name = json_string_value(json_object_get(json, "name"));
    if (name) {
        free_string(immortal->name);
        immortal->name = str_dup(name);
    }

    immortal->duties = json_integer_value(json_object_get(json, "duties"));
    immortal->created = (time_t)json_integer_value(json_object_get(json, "created"));

    const char *imm_flag = json_string_value(json_object_get(json, "imm_flag"));
    if (imm_flag) {
        free_string(immortal->imm_flag);
        immortal->imm_flag = str_dup(imm_flag);
    }

    immortal->last_olc_command = (time_t)json_integer_value(json_object_get(json, "last_olc_command"));

    json_t *leader_val = json_object_get(json, "leader");
    if (leader_val && !json_is_null(leader_val)) {
        const char *leader = json_string_value(leader_val);
        free_string(immortal->leader);
        immortal->leader = str_dup(leader ? leader : "None");
    } else {
        free_string(immortal->leader);
        immortal->leader = NULL;
    }

    const char *bamfin = json_string_value(json_object_get(json, "bamfin"));
    if (bamfin) {
        free_string(immortal->bamfin);
        immortal->bamfin = str_dup(bamfin);
    }

    const char *bamfout = json_string_value(json_object_get(json, "bamfout"));
    if (bamfout) {
        free_string(immortal->bamfout);
        immortal->bamfout = str_dup(bamfout);
    }

    return immortal;
}

/**
 * json_save_staff - Save all immortals to a JSON file
 *
 * Walks the immortal_list linked list and serializes each entry
 * into a JSON array, then writes to the specified path.
 *
 * @param path  File path to write
 * @return true on success, false on error
 */
bool json_save_staff(const char *path)
{
    json_t *root = json_object();
    json_object_set_new(root, "version", json_integer(1));

    json_t *immortals_array = json_array();

    for (IMMORTAL_DATA *imm = immortal_list; imm != NULL; imm = imm->next) {
        json_t *imm_json = json_immortal_serialize(imm);
        if (imm_json) {
            json_array_append_new(immortals_array, imm_json);
        }
    }

    json_object_set_new(root, "immortals", immortals_array);

    if (json_dump_file(root, path, JSON_INDENT(2)) != 0) {
        pbugf(LOG_ERROR, "json_save_staff: Failed to write to %s", path);
        json_decref(root);
        return false;
    }

    json_decref(root);

    plogf(LOG_INFO, "Saved staff data to %s", path);

    return true;
}

/**
 * json_load_staff - Load all immortals from a JSON file
 *
 * Reads the JSON file, deserializes each immortal entry, and
 * prepends them to the immortal_list linked list.
 *
 * @param path  File path to read
 * @return true on success, false on error (file missing or invalid)
 */
bool json_load_staff(const char *path)
{
    json_error_t error;

    json_t *root = json_load_file(path, 0, &error);
    if (!root) {
        return false;
    }

    json_t *immortals_array = json_object_get(root, "immortals");
    if (!json_is_array(immortals_array)) {
        pbugf(LOG_ERROR, "json_load_staff: Invalid format in %s", path);
        json_decref(root);
        return false;
    }

    size_t index;
    json_t *value;
    int loaded_count = 0;

    json_array_foreach(immortals_array, index, value) {
        IMMORTAL_DATA *immortal = json_immortal_deserialize(value);
        if (!immortal) continue;

        immortal->next = immortal_list;
        immortal_list = immortal;
        loaded_count++;

        plogf(LOG_INFO, "Immortal %s", immortal->name);
    }

    json_decref(root);

    plogf(LOG_INFO, "Loaded %d immortal%s from %s",
        loaded_count, loaded_count == 1 ? "" : "s", path);

    return true;
}
