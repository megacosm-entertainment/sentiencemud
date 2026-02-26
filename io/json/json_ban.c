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
#include "../../recycle.h"
#include "../../log.h"
#include "json_ban.h"
#include "json_common.h"

extern BAN_DATA *ban_list;

/**
 * json_save_bans - Save permanent bans to a JSON file
 *
 * Walks the global ban_list and serializes all permanent bans
 * to the specified JSON file. If no permanent bans exist, writes
 * an empty array.
 *
 * @param path  File path to write
 * @return      true on success, false on failure
 */
bool json_save_bans(const char *path)
{
    BAN_DATA *pban;
    json_t *root;
    json_t *bans_array;
    int count = 0;

    root = json_object();
    json_object_set_new(root, "version", json_integer(1));

    bans_array = json_array();

    for (pban = ban_list; pban != NULL; pban = pban->next) {
        if (IS_SET(pban->ban_flags, BAN_PERMANENT)) {
            json_t *ban_obj = json_object();

            json_object_set_new(ban_obj, "name",
                json_string_safe(pban->name));
            json_object_set_new(ban_obj, "level",
                json_integer(pban->level));
            json_object_set_new(ban_obj, "ban_flags",
                json_integer(pban->ban_flags));
            json_object_set_new(ban_obj, "rank",
                json_integer(pban->rank));

            json_array_append_new(bans_array, ban_obj);
            count++;
        }
    }

    json_object_set_new(root, "bans", bans_array);

    if (!json_file_save(root, path, "json_save_bans", JSON_INDENT(2)))
        return false;

    log_stringf("Saved %d permanent ban%s to %s",
        count, count == 1 ? "" : "s", path);

    return true;
}

/**
 * json_load_bans - Load bans from a JSON file
 *
 * Parses the specified JSON file and populates the global ban_list
 * with deserialized BAN_DATA entries.
 *
 * @param path  File path to read
 * @return      true on success, false on failure
 */
bool json_load_bans(const char *path)
{
    json_t *root;
    json_t *bans_array;
    size_t index;
    json_t *value;
    BAN_DATA *ban_last = NULL;
    int loaded_count = 0;

    root = json_file_load(path, "bans", &bans_array, "json_load_bans");
    if (!root)
        return false;

    json_array_foreach(bans_array, index, value) {
        BAN_DATA *pban;
        const char *name;

        if (!json_is_object(value))
            continue;

        pban = new_ban();

        name = json_string_value(json_object_get(value, "name"));
        pban->name = str_dup(name ? name : "");
        pban->level = (int16_t)json_integer_value(
            json_object_get(value, "level"));
        pban->ban_flags = (int16_t)json_integer_value(
            json_object_get(value, "ban_flags"));
        pban->rank = (int16_t)json_integer_value(
            json_object_get(value, "rank"));

        JSON_APPEND_LINK(ban_list, ban_last, pban);

        loaded_count++;
    }

    json_decref(root);

    log_stringf("Loaded %d ban%s from %s",
        loaded_count, loaded_count == 1 ? "" : "s", path);

    return true;
}
