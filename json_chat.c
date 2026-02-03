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
#include "merc.h"
#include "db.h"
#include "json_chat.h"
#include "log.h"

extern CHAT_ROOM_DATA *chat_room_list;

/**
 * json_chat_op_serialize - Serialize a chat room operator to JSON
 *
 * @param op  Operator to serialize
 * @return JSON object or NULL on error
 */
json_t *json_chat_op_serialize(CHAT_OP_DATA *op)
{
    if (!op) return NULL;
    
    json_t *json = json_object();
    
    json_object_set_new(json, "name", json_string(op->name ? op->name : ""));
    
    return json;
}

/**
 * json_chat_op_deserialize - Deserialize a chat room operator from JSON
 *
 * @param json  JSON object to deserialize
 * @return New operator or NULL on error
 */
CHAT_OP_DATA *json_chat_op_deserialize(json_t *json)
{
    if (!json) return NULL;
    
    CHAT_OP_DATA *op = new_chat_op();
    
    const char *name = json_string_value(json_object_get(json, "name"));
    op->name = str_dup(name ? name : "");
    
    return op;
}

/**
 * json_chat_ban_serialize - Serialize a chat room ban to JSON
 *
 * @param ban  Ban to serialize
 * @return JSON object or NULL on error
 */
json_t *json_chat_ban_serialize(CHAT_BAN_DATA *ban)
{
    if (!ban) return NULL;
    
    json_t *json = json_object();
    
    json_object_set_new(json, "name", json_string(ban->name ? ban->name : ""));
    json_object_set_new(json, "banned_by", json_string(ban->banned_by ? ban->banned_by : ""));
    
    return json;
}

/**
 * json_chat_ban_deserialize - Deserialize a chat room ban from JSON
 *
 * @param json  JSON object to deserialize
 * @return New ban or NULL on error
 */
CHAT_BAN_DATA *json_chat_ban_deserialize(json_t *json)
{
    if (!json) return NULL;
    
    CHAT_BAN_DATA *ban = new_chat_ban();
    
    const char *name = json_string_value(json_object_get(json, "name"));
    ban->name = str_dup(name ? name : "");
    
    const char *banned_by = json_string_value(json_object_get(json, "banned_by"));
    ban->banned_by = str_dup(banned_by ? banned_by : "");
    
    return ban;
}

/**
 * json_chat_room_serialize - Serialize a chat room to JSON
 *
 * @param chat  Chat room to serialize
 * @return JSON object or NULL on error
 */
json_t *json_chat_room_serialize(CHAT_ROOM_DATA *chat)
{
    if (!chat) return NULL;
    
    json_t *json = json_object();
    
    // Basic fields
    json_object_set_new(json, "name", json_string(chat->name ? chat->name : ""));
    json_object_set_new(json, "topic", json_string(chat->topic ? chat->topic : "<not set>"));
    json_object_set_new(json, "password", json_string(chat->password ? chat->password : "none"));
    json_object_set_new(json, "created_by", json_string(chat->created_by ? chat->created_by : ""));
    json_object_set_new(json, "max_people", json_integer(chat->max_people));
    json_object_set_new(json, "permanent", json_boolean(chat->permanent));
    
    // Widevnum support - use combined widevnum string format
    if (chat->area_uid > 0 || chat->vnum > 0) {
        AREA_DATA *area = chat->area_uid > 0 ? get_area_index(chat->area_uid) : NULL;
        json_object_set_new(json, "vnum", json_string(widevnum_string(area, chat->vnum, NULL)));
    }
    
    // Operators list
    json_t *ops_array = json_array();
    for (CHAT_OP_DATA *op = chat->ops; op != NULL; op = op->next) {
        json_t *op_json = json_chat_op_serialize(op);
        if (op_json) {
            json_array_append_new(ops_array, op_json);
        }
    }
    json_object_set_new(json, "operators", ops_array);
    
    // Bans list
    json_t *bans_array = json_array();
    for (CHAT_BAN_DATA *ban = chat->bans; ban != NULL; ban = ban->next) {
        json_t *ban_json = json_chat_ban_serialize(ban);
        if (ban_json) {
            json_array_append_new(bans_array, ban_json);
        }
    }
    json_object_set_new(json, "bans", bans_array);
    
    return json;
}

/**
 * json_chat_room_deserialize - Deserialize a chat room from JSON
 *
 * @param json  JSON object to deserialize
 * @return New chat room or NULL on error
 */
CHAT_ROOM_DATA *json_chat_room_deserialize(json_t *json)
{
    if (!json) return NULL;
    
    CHAT_ROOM_DATA *chat = new_chat_room();
    
    // Basic fields
    const char *name = json_string_value(json_object_get(json, "name"));
    chat->name = str_dup(name ? name : "");
    
    const char *topic = json_string_value(json_object_get(json, "topic"));
    chat->topic = str_dup(topic ? topic : "<not set>");
    
    const char *password = json_string_value(json_object_get(json, "password"));
    chat->password = str_dup(password ? password : "none");
    
    const char *created_by = json_string_value(json_object_get(json, "created_by"));
    chat->created_by = str_dup(created_by ? created_by : "");
    
    chat->max_people = json_integer_value(json_object_get(json, "max_people"));
    chat->permanent = json_is_true(json_object_get(json, "permanent"));
    
    // Widevnum support - supports both combined string and legacy separate fields
    json_t *vnum_val = json_object_get(json, "vnum");
    if (json_is_string(vnum_val)) {
        WNUM wnum;
        if (parse_widevnum((char *)json_string_value(vnum_val), NULL, &wnum)) {
            chat->area_uid = wnum.pArea ? wnum.pArea->uid : 0;
            chat->vnum = wnum.vnum;
        }
    } else {
        /* Legacy format with separate area_uid and vnum fields */
        chat->area_uid = json_integer_value(json_object_get(json, "area_uid"));
        chat->vnum = json_integer_value(vnum_val);
    }
    
    // Operators list
    json_t *ops_array = json_object_get(json, "operators");
    if (json_is_array(ops_array)) {
        size_t index;
        json_t *value;
        CHAT_OP_DATA *last_op = NULL;
        
        json_array_foreach(ops_array, index, value) {
            CHAT_OP_DATA *op = json_chat_op_deserialize(value);
            if (op) {
                op->chat_room = chat;
                op->next = NULL;
                
                if (last_op) {
                    last_op->next = op;
                } else {
                    chat->ops = op;
                }
                last_op = op;
            }
        }
    }
    
    // Bans list
    json_t *bans_array = json_object_get(json, "bans");
    if (json_is_array(bans_array)) {
        size_t index;
        json_t *value;
        CHAT_BAN_DATA *last_ban = NULL;
        
        json_array_foreach(bans_array, index, value) {
            CHAT_BAN_DATA *ban = json_chat_ban_deserialize(value);
            if (ban) {
                ban->chat_room = chat;
                ban->next = NULL;
                
                if (last_ban) {
                    last_ban->next = ban;
                } else {
                    chat->bans = ban;
                }
                last_ban = ban;
            }
        }
    }
    
    return chat;
}

/**
 * save_chat_rooms_json - Save all permanent chat rooms to JSON file
 *
 * Writes all permanent chat rooms to data/chat_rooms.json.
 * This replaces the legacy .dat format with structured JSON.
 *
 * @return true on success, false on error
 */
bool save_chat_rooms_json(void)
{
    char filename[MSL];
    
    sprintf(filename, "%s/chat_rooms.json", SYSTEM_DIR);
    
    // Count permanent chat rooms
    int count = 0;
    for (CHAT_ROOM_DATA *chat = chat_room_list; chat != NULL; chat = chat->next) {
        if (chat->permanent) {
            count++;
        }
    }
    
    // Create root JSON object
    json_t *root = json_object();
    json_object_set_new(root, "version", json_integer(1));
    json_object_set_new(root, "count", json_integer(count));
    
    // Create chat rooms array
    json_t *rooms_array = json_array();
    
    for (CHAT_ROOM_DATA *chat = chat_room_list; chat != NULL; chat = chat->next) {
        if (chat->permanent) {
            json_t *chat_json = json_chat_room_serialize(chat);
            if (chat_json) {
                json_array_append_new(rooms_array, chat_json);
            }
        }
    }
    
    json_object_set_new(root, "chat_rooms", rooms_array);
    
    // Write to file
    if (json_dump_file(root, filename, JSON_INDENT(2)) != 0) {
        log_stringf("save_chat_rooms_json: Failed to write to %s", filename);
        json_decref(root);
        return false;
    }
    
    json_decref(root);
    
    log_stringf("Saved %d permanent chat room%s to %s", 
        count, count == 1 ? "" : "s", filename);
    
    return true;
}

/**
 * load_chat_rooms_json - Load permanent chat rooms from JSON file
 *
 * Reads all permanent chat rooms from data/chat_rooms.json.
 * Links each room to its physical room via area_uid and vnum lookup.
 *
 * @return true on success, false on error
 */
bool load_chat_rooms_json(void)
{
    char filename[MSL];
    json_error_t error;
    
    sprintf(filename, "%s/chat_rooms.json", SYSTEM_DIR);
    
    // Load JSON file
    json_t *root = json_load_file(filename, 0, &error);
    if (!root) {
        // File doesn't exist or is invalid - not an error, just means no rooms
        return false;
    }
    
    // Get chat rooms array
    json_t *rooms_array = json_object_get(root, "chat_rooms");
    if (!json_is_array(rooms_array)) {
        log_stringf("load_chat_rooms_json: Invalid format in %s", filename);
        json_decref(root);
        return false;
    }
    
    // Deserialize each chat room
    size_t index;
    json_t *value;
    CHAT_ROOM_DATA *last_chat = NULL;
    int loaded_count = 0;
    int skipped_count = 0;
    
    json_array_foreach(rooms_array, index, value) {
        CHAT_ROOM_DATA *chat = json_chat_room_deserialize(value);
        if (!chat) {
            skipped_count++;
            continue;
        }
        
        // Look up the area by UID
        AREA_DATA *chat_area = get_area_index(chat->area_uid);
        if (!chat_area) {
            log_stringf("load_chat_rooms_json: Room '%s' area_uid %ld not found, skipping",
                chat->name, chat->area_uid);
            free_chat_room(chat);
            skipped_count++;
            continue;
        }
        
        // Look up the physical room
        ROOM_INDEX_DATA *room = get_room_index(chat_area, chat->vnum);
        if (!room) {
            log_stringf("load_chat_rooms_json: Room '%s' vnum %ld not found in area %ld, skipping",
                chat->name, chat->vnum, chat->area_uid);
            free_chat_room(chat);
            skipped_count++;
            continue;
        }
        
        // Link chat room to physical room
        room->chat_room = chat;
        
        // Link to chat room list
        if (last_chat) {
            last_chat->next = chat;
        } else {
            chat_room_list = chat;
        }
        chat->next = NULL;
        last_chat = chat;
        
        loaded_count++;
        
        log_stringf("Loaded chat room '%s' (area %ld, vnum %ld)",
            chat->name, chat->area_uid, chat->vnum);
    }
    
    json_decref(root);
    
    log_stringf("Loaded %d chat room%s from %s (%d skipped)",
        loaded_count, loaded_count == 1 ? "" : "s", filename, skipped_count);
    
    return true;
}
