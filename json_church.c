/***************************************************************************
 *  File: json_church.c                                                    *
 *                                                                         *
 *  Church system JSON serialization and deserialization.                  *
 *                                                                         *
 *  This module provides complete JSON persistence for the church/guild    *
 *  system, migrating from the legacy .org text format. Churches are       *
 *  player-created organizations with ranks, members, treasury rooms,      *
 *  and PvP settings.                                                      *
 *                                                                         *
 *  Key features:                                                          *
 *  - Rank serialization with permission flags                             *
 *  - Member roster with donation tracking                                 *
 *  - Treasure room persistence (supports wilderness coordinates)          *
 *  - Backward compatibility with string/integer permission formats        *
 *                                                                         *
 *  File format: data/churches/UID_name.json                               *
 *  Version: JSON_CHURCH_VERSION (currently 1)                             *
 *                                                                         *
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <jansson.h>
#include "merc.h"
#include "wilds.h"
#include "tables.h"
#include "json_area.h"
#include "json_church.h"

/* JSON format version for church files */
#define JSON_CHURCH_VERSION 1

/**
 * json_church_rank_serialize - Convert a church rank to JSON
 *
 * Serializes rank data including UID, name, and permission flags.
 * Permission flags are stored as a JSON array of flag names for
 * human readability.
 *
 * @param rank  The rank structure to serialize
 * @return      JSON object or NULL on error (caller must json_decref)
 */
json_t *json_church_rank_serialize(CHURCH_RANK_DATA *rank)
{
    json_t *obj;
    
    if (!rank)
        return NULL;
    
    obj = json_object();
    if (!obj)
        return NULL;
    
    json_object_set_new(obj, "uid", json_integer(rank->uid));
    json_object_set_new(obj, "name", json_string(rank->rank_name ? rank->rank_name : ""));
    json_object_set_new(obj, "permissions", flags_to_json_array(rank->permissions, church_permission_flags));
    
    return obj;
}

/**
 * json_church_rank_deserialize - Parse a church rank from JSON
 *
 * Restores rank data from JSON. Handles backward compatibility with
 * older formats that stored permissions as a string or integer rather
 * than the current array format.
 *
 * @param json  JSON object containing rank data
 * @return      Newly allocated CHURCH_RANK_DATA or NULL on error
 */
CHURCH_RANK_DATA *json_church_rank_deserialize(json_t *json)
{
    CHURCH_RANK_DATA *rank;
    json_t *value;
    
    if (!json || !json_is_object(json))
        return NULL;
    
    rank = alloc_perm(sizeof(CHURCH_RANK_DATA));
    
    value = json_object_get(json, "uid");
    if (value && json_is_integer(value))
        rank->uid = json_integer_value(value);
    
    value = json_object_get(json, "name");
    if (value && json_is_string(value))
        rank->rank_name = str_dup(json_string_value(value));
    
    value = json_object_get(json, "permissions");
    if (value) {
        if (json_is_array(value)) {
            rank->permissions = json_array_to_flags(value, church_permission_flags);
        } else if (json_is_string(value)) {
            /* Backward compatibility with old string format */
            const char *perm_str = json_string_value(value);
            if (perm_str && perm_str[0] != '\0') {
                long perm_value = flag_value(church_permission_flags, perm_str);
                rank->permissions = (perm_value != NO_FLAG) ? perm_value : 0;
            }
        } else if (json_is_integer(value)) {
            /* Backward compatibility with old integer format */
            rank->permissions = json_integer_value(value);
        }
    }
    
    return rank;
}

/**
 * json_church_member_serialize - Convert a church member to JSON
 *
 * Serializes member data including name, rank UID, and donation amounts
 * (gold, pneuma, deity points).
 *
 * @param member  The member structure to serialize
 * @return        JSON object or NULL on error (caller must json_decref)
 */
json_t *json_church_member_serialize(CHURCH_PLAYER_DATA *member)
{
    json_t *obj;
    
    if (!member)
        return NULL;
    
    obj = json_object();
    if (!obj)
        return NULL;
    
    json_object_set_new(obj, "name", json_string(member->name ? member->name : ""));
    json_object_set_new(obj, "rank_uid", json_integer(member->rank_uid));
    json_object_set_new(obj, "donated_gold", json_integer(member->dep_gold));
    json_object_set_new(obj, "donated_pneuma", json_integer(member->dep_pneuma));
    json_object_set_new(obj, "donated_dp", json_integer(member->dep_dp));
    
    return obj;
}

/**
 * json_church_member_deserialize - Parse a church member from JSON
 *
 * Restores member data from JSON. The rank pointer is resolved after
 * deserialization by matching rank_uid against loaded ranks.
 *
 * @param json  JSON object containing member data
 * @return      Newly allocated CHURCH_PLAYER_DATA or NULL on error
 */
CHURCH_PLAYER_DATA *json_church_member_deserialize(json_t *json)
{
    CHURCH_PLAYER_DATA *member;
    json_t *value;
    
    if (!json || !json_is_object(json))
        return NULL;
    
    member = alloc_perm(sizeof(CHURCH_PLAYER_DATA));
    
    value = json_object_get(json, "name");
    if (value && json_is_string(value))
        member->name = str_dup(json_string_value(value));
    
    value = json_object_get(json, "rank_uid");
    if (value && json_is_integer(value))
        member->rank_uid = json_integer_value(value);
    
    value = json_object_get(json, "donated_gold");
    if (value && json_is_integer(value))
        member->dep_gold = json_integer_value(value);
    
    value = json_object_get(json, "donated_pneuma");
    if (value && json_is_integer(value))
        member->dep_pneuma = json_integer_value(value);
    
    value = json_object_get(json, "donated_dp");
    if (value && json_is_integer(value))
        member->dep_dp = json_integer_value(value);
    
    return member;
}

/**
 * json_church_treasure_room_serialize - Convert a treasure room to JSON
 *
 * Serializes treasure room location and access permissions. Handles both
 * normal rooms (area_uid + vnum) and wilderness rooms (wilds_uid + coords).
 * Allowed ranks are stored as an array of rank UIDs.
 *
 * @param treasure  The treasure room structure to serialize
 * @return          JSON object or NULL on error (caller must json_decref)
 */
json_t *json_church_treasure_room_serialize(CHURCH_TREASURE_ROOM *treasure)
{
    json_t *obj;
    json_t *ranks_array;
    ROOM_INDEX_DATA *room;
    
    if (!treasure || !treasure->room)
        return NULL;
    
    room = treasure->room;
    obj = json_object();
    if (!obj)
        return NULL;
    
    /* Distinguish wilderness vs normal rooms */
    if (room->wilds) {
        json_object_set_new(obj, "type", json_string("wilderness"));
        json_object_set_new(obj, "wilds_uid", json_integer(room->wilds->uid));
        json_object_set_new(obj, "x", json_integer(room->x));
        json_object_set_new(obj, "y", json_integer(room->y));
        json_object_set_new(obj, "z", json_integer(room->z));
    } else {
        json_object_set_new(obj, "type", json_string("normal"));
        json_object_set_new(obj, "area_uid", json_integer(room->area ? room->area->uid : 0));
        json_object_set_new(obj, "vnum", json_integer(room->vnum));
    }
    
    json_object_set_new(obj, "is_default", json_boolean(treasure->is_default));
    
    /* Serialize allowed ranks */
    if (treasure->allowed_ranks) {
        ITERATOR it;
        CHURCH_RANK_DATA *rank;
        
        ranks_array = json_array();
        iterator_start(&it, treasure->allowed_ranks);
        while ((rank = (CHURCH_RANK_DATA *)iterator_nextdata(&it))) {
            json_array_append_new(ranks_array, json_integer(rank->uid));
        }
        iterator_stop(&it);
        
        json_object_set_new(obj, "allowed_rank_uids", ranks_array);
    }
    
    return obj;
}

/**
 * json_church_treasure_room_deserialize - Parse a treasure room from JSON
 *
 * Restores treasure room configuration from JSON. Note that the room
 * pointer and allowed rank pointers are NOT resolved during this call -
 * they are set to NULL/stored as UIDs and resolved in a fixup phase
 * after all areas and ranks are loaded.
 *
 * @param json  JSON object containing treasure room data
 * @return      Newly allocated CHURCH_TREASURE_ROOM or NULL on error
 */
CHURCH_TREASURE_ROOM *json_church_treasure_room_deserialize(json_t *json)
{
    CHURCH_TREASURE_ROOM *treasure;
    json_t *value, *type_json, *ranks_array;
    const char *type;
    
    if (!json || !json_is_object(json))
        return NULL;
    
    treasure = alloc_perm(sizeof(CHURCH_TREASURE_ROOM));
    treasure->allowed_ranks = list_create(false);
    
    type_json = json_object_get(json, "type");
    type = type_json && json_is_string(type_json) ? json_string_value(type_json) : "normal";
    
    /* Store vnum/coords temporarily - will resolve to room pointer in fixup */
    if (!str_cmp(type, "wilderness")) {
        /* Wilderness rooms - store coords for later lookup */
        treasure->room = NULL; /* Will be resolved in fixup */
    } else {
        /* Normal rooms - store vnum for later lookup */
        treasure->room = NULL; /* Will be resolved in fixup */
    }
    
    value = json_object_get(json, "is_default");
    if (value && json_is_boolean(value))
        treasure->is_default = json_is_true(value);
    
    /* Deserialize allowed rank UIDs (will resolve to pointers in fixup) */
    ranks_array = json_object_get(json, "allowed_rank_uids");
    if (ranks_array && json_is_array(ranks_array)) {
        size_t index;
        json_t *rank_uid_json;
        
        json_array_foreach(ranks_array, index, rank_uid_json) {
            if (json_is_integer(rank_uid_json)) {
                long rank_uid = json_integer_value(rank_uid_json);
                /* Store UID temporarily - will resolve to rank pointer in fixup */
                list_appendlink(treasure->allowed_ranks, (void *)rank_uid);
            }
        }
    }
    
    return treasure;
}

/**
 * json_church_serialize - Convert entire church to JSON
 *
 * Creates a complete JSON representation of a church including all
 * metadata, members, ranks, treasury, treasure rooms, combat stats,
 * and timestamps. This is the primary serialization function called
 * by save_church_json().
 *
 * JSON structure includes:
 * - Metadata: version, uid, name, flag, deleted status
 * - People: founder, owner names
 * - Resources: gold, pneuma, dp
 * - Stats: alignment, size, max_positions
 * - Location: recall_point vnum
 * - Combat: pk_wins/losses, cpk_wins/losses, wars_won
 * - Settings: pk toggle, settings bitmask
 * - Timestamps: created, various last_login times
 * - Colors: colour1, colour2
 * - Text: motd, rules, info
 * - Arrays: ranks, members, treasure_rooms
 *
 * @param church  The church structure to serialize
 * @return        JSON object or NULL on error (caller must json_decref)
 */
json_t *json_church_serialize(CHURCH_DATA *church)
{
    json_t *root, *ranks_array, *members_array, *treasure_array;
    CHURCH_RANK_DATA *rank;
    CHURCH_PLAYER_DATA *member;
    ITERATOR it;
    
    if (!church)
        return NULL;
    
    root = json_object();
    if (!root)
        return NULL;
    
    /* Metadata */
    json_object_set_new(root, "version", json_integer(JSON_CHURCH_VERSION));
    json_object_set_new(root, "church_version", json_integer(church->version));
    json_object_set_new(root, "uid", json_integer(church->uid));
    json_object_set_new(root, "name", json_string(church->name ? church->name : ""));
    json_object_set_new(root, "flag", json_string(church->flag ? church->flag : ""));
    json_object_set_new(root, "deleted", json_boolean(church->deleted));
    
    /* People */
    json_object_set_new(root, "founder", json_string(church->founder ? church->founder : ""));
    json_object_set_new(root, "owner", json_string(church->owner ? church->owner : ""));
    
    /* Resources */
    json_object_set_new(root, "pneuma", json_integer(church->pneuma));
    json_object_set_new(root, "gold", json_integer(church->gold));
    json_object_set_new(root, "dp", json_integer(church->dp));
    
    /* Stats */
    json_object_set_new(root, "max_positions", json_integer(church->max_positions));
    json_object_set_new(root, "size", json_integer(church->size));
    json_object_set_new(root, "alignment", json_integer(church->alignment));
    
    /* Location */
    json_object_set_new(root, "recall_point", json_integer(church->recall_point.id[0]));
    
    /* Key object vnum */
    json_object_set_new(root, "key_vnum", json_integer(church->key));
    
    /* Combat stats */
    json_object_set_new(root, "pk_wins", json_integer(church->pk_wins));
    json_object_set_new(root, "pk_losses", json_integer(church->pk_losses));
    json_object_set_new(root, "cpk_wins", json_integer(church->cpk_wins));
    json_object_set_new(root, "cpk_losses", json_integer(church->cpk_losses));
    json_object_set_new(root, "wars_won", json_integer(church->wars_won));
    
    /* Settings */
    json_object_set_new(root, "settings", json_integer(church->settings));
    json_object_set_new(root, "pk", json_boolean(church->pk));
    
    /* Timestamps */
    json_object_set_new(root, "created", json_integer((long)church->created));
    json_object_set_new(root, "founder_last_login", json_integer((long)church->founder_last_login));
    json_object_set_new(root, "owner_last_login", json_integer((long)church->owner_last_login));
    json_object_set_new(root, "member_last_login", json_integer((long)church->member_last_login));
    json_object_set_new(root, "officer_last_login", json_integer((long)church->officer_last_login));
    json_object_set_new(root, "leader_last_login", json_integer((long)church->leader_last_login));
    
    /* Colors */
    json_object_set_new(root, "colour1", json_string(church->colour1 ? church->colour1 : ""));
    json_object_set_new(root, "colour2", json_string(church->colour2 ? church->colour2 : ""));
    
    /* Descriptions */
    json_object_set_new(root, "motd", json_string(church->motd ? church->motd : ""));
    json_object_set_new(root, "rules", json_string(church->rules ? church->rules : ""));
    json_object_set_new(root, "info", json_string(church->info ? church->info : ""));
    
    /* Coffer */
    json_object_set_new(root, "coffer_rent", json_integer((long)church->coffer_rent));
    json_object_set_new(root, "storage_permissions", json_integer(church->storage_permissions));
    
    /* Ranks */
    json_object_set_new(root, "max_ranks", json_integer(church->max_ranks));
    json_object_set_new(root, "num_ranks", json_integer(church->num_ranks));
    json_object_set_new(root, "max_rank_uid", json_integer(church->max_rank_uid));
    json_object_set_new(root, "default_rank_uid", 
                        json_integer(church->default_rank ? church->default_rank->uid : 0));
    
    ranks_array = json_array();
    for (rank = church->ranks; rank; rank = rank->next) {
        json_t *rank_obj = json_church_rank_serialize(rank);
        if (rank_obj)
            json_array_append_new(ranks_array, rank_obj);
    }
    json_object_set_new(root, "ranks", ranks_array);
    
    /* Members */
    members_array = json_array();
    for (member = church->people; member; member = member->next) {
        json_t *member_obj = json_church_member_serialize(member);
        if (member_obj)
            json_array_append_new(members_array, member_obj);
    }
    json_object_set_new(root, "members", members_array);
    
    /* Treasure rooms */
    treasure_array = json_array();
    if (church->treasure_rooms) {
        CHURCH_TREASURE_ROOM *treasure;
        iterator_start(&it, church->treasure_rooms);
        while ((treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it))) {
            json_t *treasure_obj = json_church_treasure_room_serialize(treasure);
            if (treasure_obj)
                json_array_append_new(treasure_array, treasure_obj);
        }
        iterator_stop(&it);
    }
    json_object_set_new(root, "treasure_rooms", treasure_array);
    
    /* Log entry tracking */
    json_object_set_new(root, "last_log_entry_id", json_integer(church->last_log_entry_id));
    json_object_set_new(root, "log_entry_count", json_integer(church->log_entry_count));
    
    return root;
}

/**
 * save_church_json - Write church data to a JSON file
 *
 * Serializes a church and writes it to disk. Filename is derived from
 * the church UID and normalized name: data/churches/UID_name.json
 *
 * Side effects:
 * - Creates/overwrites file at ORG_DIR/UID_name.json
 * - Logs success/failure via plogf/pbugf
 *
 * @param church  The church to save
 * @return        true on success, false on error
 */
bool save_church_json(CHURCH_DATA *church)
{
    json_t *root;
    char *json_str;
    FILE *fp;
    char filename[256];
    char normalized[32];
    char *norm;
    
    if (!church) {
        pbugf(LOG_ERROR, "save_church_json: null church");
        return false;
    }
    
    /* Build filename */
    norm = normalize_filename(church->name);
    strncpy(normalized, norm, 25);
    normalized[25] = '\0';
    snprintf(filename, sizeof(filename), "%s%ld_%s.json", ORG_DIR, church->uid, normalized);
    
    root = json_church_serialize(church);
    if (!root) {
        pbugf(LOG_ERROR, "save_church_json: serialization failed for %s", church->name);
        return false;
    }
    
    json_str = json_dumps(root, JSON_INDENT(2));
    json_decref(root);
    
    if (!json_str) {
        pbugf(LOG_ERROR, "save_church_json: json_dumps failed for %s", church->name);
        return false;
    }
    
    fp = fopen(filename, "w");
    if (!fp) {
        pbugf(LOG_ERROR, "save_church_json: fopen failed for %s", filename);
        free(json_str);
        return false;
    }
    
    fprintf(fp, "%s\n", json_str);
    fclose(fp);
    free(json_str);
    
    plogf(LOG_INFO, "Saved church %s to %s", church->name, filename);
    return true;
}

/**
 * json_church_deserialize - Parse entire church from JSON
 *
 * Creates a CHURCH_DATA structure from a JSON object. This function
 * handles all church data including ranks, members, and treasure rooms.
 *
 * Note: Treasure room pointers (room and allowed_ranks) require a
 * fixup phase after deserialization since rooms may not be loaded yet.
 * This is handled by boot_churches() after area loading completes.
 *
 * @param root  JSON object containing church data
 * @return      Newly allocated CHURCH_DATA or NULL on error
 */
CHURCH_DATA *json_church_deserialize(json_t *root)
{
    CHURCH_DATA *church;
    json_t *value, *array;
    CHURCH_RANK_DATA *rank, *last_rank = NULL;
    CHURCH_PLAYER_DATA *member, *last_member = NULL;
    size_t index;
    json_t *item;
    long default_rank_uid = 0;
    
    if (!root || !json_is_object(root))
        return NULL;
    
    church = new_church();
    
    /* Metadata */
    value = json_object_get(root, "church_version");
    if (value && json_is_integer(value))
        church->version = json_integer_value(value);
    
    value = json_object_get(root, "uid");
    if (value && json_is_integer(value))
        church->uid = json_integer_value(value);
    
    value = json_object_get(root, "name");
    if (value && json_is_string(value))
        church->name = str_dup(json_string_value(value));
    
    value = json_object_get(root, "flag");
    if (value && json_is_string(value))
        church->flag = str_dup(json_string_value(value));
    
    value = json_object_get(root, "deleted");
    if (value && json_is_boolean(value))
        church->deleted = json_is_true(value);
    
    /* People */
    value = json_object_get(root, "founder");
    if (value && json_is_string(value))
        church->founder = str_dup(json_string_value(value));
    
    value = json_object_get(root, "owner");
    if (value && json_is_string(value))
        church->owner = str_dup(json_string_value(value));
    
    /* Resources */
    value = json_object_get(root, "pneuma");
    if (value && json_is_integer(value))
        church->pneuma = json_integer_value(value);
    
    value = json_object_get(root, "gold");
    if (value && json_is_integer(value))
        church->gold = json_integer_value(value);
    
    value = json_object_get(root, "dp");
    if (value && json_is_integer(value))
        church->dp = json_integer_value(value);
    
    /* Stats */
    value = json_object_get(root, "max_positions");
    if (value && json_is_integer(value))
        church->max_positions = json_integer_value(value);
    
    value = json_object_get(root, "size");
    if (value && json_is_integer(value))
        church->size = json_integer_value(value);
    
    value = json_object_get(root, "alignment");
    if (value && json_is_integer(value))
        church->alignment = json_integer_value(value);
    
    /* Location */
    value = json_object_get(root, "recall_point");
    if (value && json_is_integer(value))
        church->recall_point.id[0] = json_integer_value(value);
    
    /* Key object vnum */
    value = json_object_get(root, "key_vnum");
    if (value && json_is_integer(value))
        church->key = json_integer_value(value);
    
    /* Combat stats */
    value = json_object_get(root, "pk_wins");
    if (value && json_is_integer(value))
        church->pk_wins = json_integer_value(value);
    
    value = json_object_get(root, "pk_losses");
    if (value && json_is_integer(value))
        church->pk_losses = json_integer_value(value);
    
    value = json_object_get(root, "cpk_wins");
    if (value && json_is_integer(value))
        church->cpk_wins = json_integer_value(value);
    
    value = json_object_get(root, "cpk_losses");
    if (value && json_is_integer(value))
        church->cpk_losses = json_integer_value(value);
    
    value = json_object_get(root, "wars_won");
    if (value && json_is_integer(value))
        church->wars_won = json_integer_value(value);
    
    /* Settings */
    value = json_object_get(root, "settings");
    if (value && json_is_integer(value))
        church->settings = json_integer_value(value);
    
    value = json_object_get(root, "pk");
    if (value && json_is_boolean(value))
        church->pk = json_is_true(value);
    
    /* Timestamps */
    value = json_object_get(root, "created");
    if (value && json_is_integer(value))
        church->created = (time_t)json_integer_value(value);
    
    value = json_object_get(root, "founder_last_login");
    if (value && json_is_integer(value))
        church->founder_last_login = (time_t)json_integer_value(value);
    
    value = json_object_get(root, "owner_last_login");
    if (value && json_is_integer(value))
        church->owner_last_login = (time_t)json_integer_value(value);
    
    value = json_object_get(root, "member_last_login");
    if (value && json_is_integer(value))
        church->member_last_login = (time_t)json_integer_value(value);
    
    value = json_object_get(root, "officer_last_login");
    if (value && json_is_integer(value))
        church->officer_last_login = (time_t)json_integer_value(value);
    
    value = json_object_get(root, "leader_last_login");
    if (value && json_is_integer(value))
        church->leader_last_login = (time_t)json_integer_value(value);
    
    /* Colors */
    value = json_object_get(root, "colour1");
    if (value && json_is_string(value))
        church->colour1 = str_dup(json_string_value(value));
    
    value = json_object_get(root, "colour2");
    if (value && json_is_string(value))
        church->colour2 = str_dup(json_string_value(value));
    
    /* Descriptions */
    value = json_object_get(root, "motd");
    if (value && json_is_string(value))
        church->motd = str_dup(json_string_value(value));
    
    value = json_object_get(root, "rules");
    if (value && json_is_string(value))
        church->rules = str_dup(json_string_value(value));
    
    value = json_object_get(root, "info");
    if (value && json_is_string(value))
        church->info = str_dup(json_string_value(value));
    
    /* Coffer */
    value = json_object_get(root, "coffer_rent");
    if (value && json_is_integer(value))
        church->coffer_rent = (time_t)json_integer_value(value);
    
    value = json_object_get(root, "storage_permissions");
    if (value && json_is_integer(value))
        church->storage_permissions = json_integer_value(value);
    
    /* Rank metadata */
    value = json_object_get(root, "max_ranks");
    if (value && json_is_integer(value))
        church->max_ranks = json_integer_value(value);
    
    value = json_object_get(root, "num_ranks");
    if (value && json_is_integer(value))
        church->num_ranks = json_integer_value(value);
    
    value = json_object_get(root, "max_rank_uid");
    if (value && json_is_integer(value))
        church->max_rank_uid = json_integer_value(value);
    
    value = json_object_get(root, "default_rank_uid");
    if (value && json_is_integer(value))
        default_rank_uid = json_integer_value(value);
    
    /* Ranks array */
    array = json_object_get(root, "ranks");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, item) {
            rank = json_church_rank_deserialize(item);
            if (rank) {
                rank->next = NULL;
                
                if (!church->ranks) {
                    church->ranks = rank;
                } else {
                    last_rank->next = rank;
                }
                last_rank = rank;
                
                /* Set default rank pointer */
                if (default_rank_uid > 0 && rank->uid == default_rank_uid)
                    church->default_rank = rank;
            }
        }
    }
    
    /* Members array */
    array = json_object_get(root, "members");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, item) {
            member = json_church_member_deserialize(item);
            if (member) {
                member->church = church;
                member->next = NULL;
                
                /* Resolve rank pointer from UID */
                for (rank = church->ranks; rank; rank = rank->next) {
                    if (rank->uid == member->rank_uid) {
                        member->rank = rank;
                        break;
                    }
                }
                
                if (!church->people) {
                    church->people = member;
                } else {
                    last_member->next = member;
                }
                last_member = member;
                
                /* Add to roster */
                list_appendlink(church->roster, member->name);
            }
        }
    }
    
    /* Treasure rooms - Note: Room pointers will be resolved in fixup phase */
    array = json_object_get(root, "treasure_rooms");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, item) {
            CHURCH_TREASURE_ROOM *treasure = json_church_treasure_room_deserialize(item);
            if (treasure) {
                /* Room pointer resolution will happen in boot_churches() */
                list_appendlink(church->treasure_rooms, treasure);
            }
        }
    }
    
    /* Log entry tracking */
    value = json_object_get(root, "last_log_entry_id");
    if (value && json_is_integer(value))
        church->last_log_entry_id = json_integer_value(value);
    
    value = json_object_get(root, "log_entry_count");
    if (value && json_is_integer(value))
        church->log_entry_count = json_integer_value(value);
    
    return church;
}

/**
 * load_church_json - Load a church from a JSON file
 *
 * Parses a church JSON file and creates a CHURCH_DATA structure.
 * Validates the file version and returns an error if the version
 * is newer than supported.
 *
 * Side effects:
 * - Logs success/failure via plogf/pbugf
 * - Allocates memory for the returned church (caller owns)
 *
 * @param filename    Path to the JSON file to load
 * @param church_out  Output pointer for the loaded church
 * @return            true on success, false on error
 */
bool load_church_json(const char *filename, CHURCH_DATA **church_out)
{
    json_t *root;
    json_error_t error;
    CHURCH_DATA *church;
    int version;
    json_t *value;
    
    if (!filename || !church_out) {
        pbugf(LOG_ERROR, "load_church_json: null parameters");
        return false;
    }
    
    if (access(filename, F_OK) != 0)
        return false;
    
    root = json_load_file(filename, 0, &error);
    if (!root) {
        pbugf(LOG_ERROR, "load_church_json: JSON parse error on line %d: %s", 
              error.line, error.text);
        return false;
    }
    
    if (!json_is_object(root)) {
        pbugf(LOG_ERROR, "load_church_json: Root is not a JSON object");
        json_decref(root);
        return false;
    }
    
    /* Check version */
    value = json_object_get(root, "version");
    version = value && json_is_integer(value) ? json_integer_value(value) : 0;
    
    if (version > JSON_CHURCH_VERSION) {
        pbugf(LOG_ERROR, "load_church_json: Unsupported version %d", version);
        json_decref(root);
        return false;
    }
    
    /* Deserialize church */
    church = json_church_deserialize(root);
    json_decref(root);
    
    if (!church) {
        pbugf(LOG_ERROR, "load_church_json: Deserialization failed for %s", filename);
        return false;
    }
    
    *church_out = church;
    plogf(LOG_INFO, "Loaded church %s from %s", church->name, filename);
    return true;
}
