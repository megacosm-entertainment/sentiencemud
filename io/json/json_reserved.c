/***************************************************************************
 *  File: json_reserved.c                                                  *
 *                                                                         *
 *  JSON serialization/deserialization for reserved entities system.       *
 *                                                                         *
 *  Reserved entities are named vnums that the game uses for special       *
 *  purposes (e.g., limbo room, recall room, donation pit). This module    *
 *  handles saving and loading the reserved.json file with WNUM support.   *
 *                                                                         *
 *  Key features:                                                          *
 *  - Stores area_uid + vnum for widevnum compatibility                    *
 *  - Auto-resolves area_uid from vnum during save if missing              *
 *  - Validates entities on load with detailed logging                     *
 *                                                                         *
 *  File format: data/reserved.json                                        *
 *  Version: 1                                                             *
 *                                                                         *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include "../../merc.h"
#include "json_reserved.h"
#include "../../editors/reserved_vnums/reserved.h"

extern LLIST *reserved_vnums;
extern bool reserved_changed;

/**
 * json_reserved_serialize - Convert a reserved entity to JSON
 *
 * Serializes a single reserved entity including name, type, area_uid,
 * vnum, and removable flag. If area_uid is 0, attempts to auto-resolve
 * it by searching all areas for the vnum.
 *
 * @param reserved  The reserved entity to serialize
 * @return          JSON object or NULL on error (caller must json_decref)
 */
json_t *json_reserved_serialize(RESERVED_DATA *reserved)
{
    json_t *obj;
    const char *type_name;
    long area_uid;
    
    if (!reserved)
        return NULL;
    
    obj = json_object();
    if (!obj)
        return NULL;
    
    /* Name */
    if (reserved->name && reserved->name[0])
        json_object_set_new(obj, "name", json_string(reserved->name));
    else
        json_object_set_new(obj, "name", json_string(""));
    
    /* Type - convert to string */
    type_name = reserved_types_get_name(reserved->type);
    json_object_set_new(obj, "type", json_string(type_name));
    
    /* WNUM - resolve area_uid if it's 0 by searching all areas */
    area_uid = reserved->wnum.auid;
    if (area_uid == 0 && reserved->wnum.vnum > 0 && reserved->type != RESERVED_AREA) {
        AREA_DATA *area;
        ROOM_INDEX_DATA *room;
        
        /* Search all areas for this vnum */
        for (area = area_first; area; area = area->next) {
            room = get_room_index(area, reserved->wnum.vnum);
            if (room) {
                area_uid = area->uid;
                /* Update the in-memory structure too */
                reserved->wnum.auid = area_uid;
                plogf(LOG_INFO, "Resolved reserved '%s' vnum %ld to area %ld", 
                      reserved->name, reserved->wnum.vnum, area_uid);
                break;
            }
        }
    }
    
    json_object_set_new(obj, "area_uid", json_integer(area_uid));
    json_object_set_new(obj, "vnum", json_integer(reserved->wnum.vnum));
    
    /* Removable flag */
    json_object_set_new(obj, "removable", json_boolean(reserved->removable));
    
    /* Description */
    if (reserved->description && reserved->description[0])
        json_object_set_new(obj, "description", json_string(reserved->description));
    else
        json_object_set_new(obj, "description", json_string(""));
    
    return obj;
}

/**
 * json_reserved_deserialize - Parse a reserved entity from JSON
 *
 * Creates a RESERVED_DATA structure from JSON. Attempts to resolve the
 * area from area_uid+vnum using parse_widevnum. Logs warnings if the
 * area cannot be found but stores the values anyway for later resolution.
 *
 * @param json  JSON object containing reserved entity data
 * @return      Newly allocated RESERVED_DATA or NULL on error
 */
RESERVED_DATA *json_reserved_deserialize(json_t *json)
{
    RESERVED_DATA *reserved;
    json_t *value;
    const char *name, *type_str, *description;
    long area_uid, vnum;
    int type = -1;
    bool removable;
    char vnum_str[MAX_INPUT_LENGTH];
    WNUM wnum;
    bool areas_loaded;
    
    if (!json || !json_is_object(json))
        return NULL;
    
    /* Allocate structure */
    reserved = alloc_mem(sizeof(RESERVED_DATA));
    if (!reserved)
        return NULL;
    
    /* Name */
    value = json_object_get(json, "name");
    if (value && json_is_string(value)) {
        name = json_string_value(value);
        reserved->name = str_dup(name);
    } else {
        reserved->name = str_dup("");
    }
    
    /* Type */
    value = json_object_get(json, "type");
    if (value && json_is_string(value)) {
        type_str = json_string_value(value);
        
        /* Find type by name */
        if (!str_cmp(type_str, "mob"))
            type = RESERVED_MOB;
        else if (!str_cmp(type_str, "obj") || !str_cmp(type_str, "object"))
            type = RESERVED_OBJ;
        else if (!str_cmp(type_str, "room"))
            type = RESERVED_ROOM;
        else if (!str_cmp(type_str, "area"))
            type = RESERVED_AREA;
        else if (!str_cmp(type_str, "token"))
            type = RESERVED_TOKEN;
        else if (!str_cmp(type_str, "mprog"))
            type = RESERVED_MPROG;
        else if (!str_cmp(type_str, "oprog"))
            type = RESERVED_OPROG;
        else if (!str_cmp(type_str, "rprog"))
            type = RESERVED_RPROG;
        else if (!str_cmp(type_str, "tprog"))
            type = RESERVED_TPROG;
        else if (!str_cmp(type_str, "aprog"))
            type = RESERVED_APROG;
        else if (!str_cmp(type_str, "blueprint"))
            type = RESERVED_BLUEPRINT;
        else if (!str_cmp(type_str, "dungeon"))
            type = RESERVED_DUNGEON;
        else if (!str_cmp(type_str, "ship"))
            type = RESERVED_SHIP;
        
        reserved->type = type;
    } else {
        reserved->type = -1;
    }
    
    /* WNUM - Load area_uid and vnum */
    value = json_object_get(json, "area_uid");
    if (value && json_is_integer(value))
        area_uid = json_integer_value(value);
    else
        area_uid = 0;
    
    value = json_object_get(json, "vnum");
    if (value && json_is_integer(value))
        vnum = json_integer_value(value);
    else
        vnum = 0;
    
    areas_loaded = (area_first != NULL);

    /* If we have both area_uid and vnum, try to resolve the area */
    if (area_uid > 0 && vnum > 0) {
        /* During early boot, areas are not loaded yet; keep widevnum as-is. */
        if (!areas_loaded) {
            reserved->wnum.auid = area_uid;
            reserved->wnum.vnum = vnum;
        } else {
            /* Format as "area_uid#vnum" and parse */
            sprintf(vnum_str, "%ld#%ld", area_uid, vnum);
            if (parse_widevnum(vnum_str, NULL, &wnum)) {
                reserved->wnum.auid = area_uid;
                reserved->wnum.vnum = vnum;
            } else {
                /* Area not found, but store the values anyway */
                reserved->wnum.auid = area_uid;
                reserved->wnum.vnum = vnum;
                plogf(LOG_WARN, "Reserved '%s': Area UID %ld not found, stored anyway", 
                      reserved->name, area_uid);
            }
        }
    } else if (vnum > 0) {
        /* During early boot, areas are not loaded yet; keep vnum as-is. */
        if (!areas_loaded) {
            reserved->wnum.auid = 0;
            reserved->wnum.vnum = vnum;
        } else {
            /* Only vnum provided - use parse_widevnum to find the area */
            sprintf(vnum_str, "%ld", vnum);
            if (parse_widevnum(vnum_str, NULL, &wnum)) {
                reserved->wnum.auid = wnum.pArea ? wnum.pArea->uid : 0;
                reserved->wnum.vnum = vnum;
                plogf(LOG_INFO, "Reserved '%s': Found vnum %ld in area %ld", 
                      reserved->name, vnum, reserved->wnum.auid);
            } else {
                /* Couldn't find area - store with area 0 */
                reserved->wnum.auid = 0;
                reserved->wnum.vnum = vnum;
                plogf(LOG_WARN, "Reserved '%s': Vnum %ld not found in any area", 
                      reserved->name, vnum);
            }
        }
    } else {
        /* No valid vnum */
        reserved->wnum.auid = 0;
        reserved->wnum.vnum = 0;
    }
    
    /* Removable flag */
    value = json_object_get(json, "removable");
    if (value && json_is_boolean(value))
        removable = json_boolean_value(value);
    else
        removable = true;
    reserved->removable = removable;
    
    /* Description */
    value = json_object_get(json, "description");
    if (value && json_is_string(value)) {
        description = json_string_value(value);
        reserved->description = str_dup(description);
    } else {
        reserved->description = str_dup("");
    }
    
    return reserved;
}

/**
 * save_reserved_json - Write all reserved entities to JSON file
 *
 * Serializes all reserved entities in the reserved_vnums list and
 * writes them to data/reserved.json with pretty-printed formatting.
 *
 * Side effects:
 * - Creates/overwrites SYSTEM_DIR/reserved.json
 * - Logs success/failure via plogf
 *
 * @return  true on success, false on error
 */
bool save_reserved_json(void)
{
    FILE *fp;
    json_t *root, *entities_array;
    ITERATOR it;
    RESERVED_DATA *reserved;
    char *json_str;
    char reserved_path_buf[MAX_INPUT_LENGTH];
    const char *reserved_path;
    
    if (!reserved_vnums) {
        plogf(LOG_ERROR, "save_reserved_json: reserved_vnums list is NULL");
        return false;
    }
    
    /* Create root object */
    root = json_object();
    if (!root) {
        plogf(LOG_ERROR, "save_reserved_json: Failed to create JSON root object");
        return false;
    }
    
    /* Add version */
    json_object_set_new(root, "version", json_integer(1));
    
    /* Add count */
    json_object_set_new(root, "count", json_integer(list_size(reserved_vnums)));
    
    /* Create entities array */
    entities_array = json_array();
    if (!entities_array) {
        json_decref(root);
        plogf(LOG_ERROR, "save_reserved_json: Failed to create entities array");
        return false;
    }
    
    /* Serialize each reserved entity */
    iterator_start(&it, reserved_vnums);
    while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
        json_t *entity_obj = json_reserved_serialize(reserved);
        if (entity_obj)
            json_array_append_new(entities_array, entity_obj);
    }
    iterator_stop(&it);
    
    /* Add array to root */
    json_object_set_new(root, "entities", entities_array);

    reserved_path = resolve_game_path(SYSTEM_DIR "reserved.json", reserved_path_buf, sizeof(reserved_path_buf));
    
    /* Open file for writing */
    fp = fopen(reserved_path, "w");
    if (!fp) {
        json_decref(root);
        plogf(LOG_ERROR, "save_reserved_json: Failed to open file for writing: %s", reserved_path);
        return false;
    }
    
    /* Convert to pretty-printed string */
    json_str = json_dumps(root, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
    if (!json_str) {
        fclose(fp);
        json_decref(root);
        plogf(LOG_ERROR, "save_reserved_json: Failed to serialize JSON");
        return false;
    }
    
    /* Write to file */
    fprintf(fp, "%s\n", json_str);
    
    /* Clean up */
    free(json_str);
    fclose(fp);
    json_decref(root);
    
    plogf(LOG_INFO, "Saved %d reserved entities to JSON", list_size(reserved_vnums));
    
    return true;
}

/**
 * load_reserved_json - Load reserved entities from JSON file
 *
 * Parses data/reserved.json and populates the reserved_vnums list.
 * Clears any existing entities before loading. Validates each entity
 * and skips invalid entries with logging.
 *
 * Side effects:
 * - Clears and repopulates reserved_vnums global
 * - Logs success/failure and per-entity warnings via plogf
 *
 * @return  true on success, false if file doesn't exist or parse error
 */
bool load_reserved_json(void)
{
    FILE *fp;
    json_t *root, *entities_array, *entity_obj;
    json_error_t error;
    size_t i, count;
    int version;
    RESERVED_DATA *reserved;
    char reserved_path_buf[MAX_INPUT_LENGTH];
    const char *reserved_path;

    reserved_path = resolve_game_path(SYSTEM_DIR "reserved.json", reserved_path_buf, sizeof(reserved_path_buf));
    
    /* Try to open JSON file */
    fp = fopen(reserved_path, "r");
    if (!fp)
        return false;  /* File doesn't exist */
    
    fclose(fp);
    
    /* Parse JSON file */
    root = json_load_file(reserved_path, 0, &error);
    if (!root) {
        plogf(LOG_ERROR, "load_reserved_json: JSON parse error on line %d: %s", 
              error.line, error.text);
        return false;
    }
    
    if (!json_is_object(root)) {
        json_decref(root);
        plogf(LOG_ERROR, "load_reserved_json: Root is not a JSON object");
        return false;
    }
    
    /* Check version */
    version = json_integer_value(json_object_get(root, "version"));
    if (version != 1) {
        json_decref(root);
        plogf(LOG_ERROR, "load_reserved_json: Unsupported version %d", version);
        return false;
    }
    
    /* Get entities array */
    entities_array = json_object_get(root, "entities");
    if (!entities_array || !json_is_array(entities_array)) {
        json_decref(root);
        plogf(LOG_ERROR, "load_reserved_json: No entities array found");
        return false;
    }
    
    /* Clear existing reserved entities */
    if (reserved_vnums && list_size(reserved_vnums) > 0) {
        ITERATOR it;
        RESERVED_DATA *old;
        
        iterator_start(&it, reserved_vnums);
        while ((old = (RESERVED_DATA *)iterator_nextdata(&it))) {
            free_string(old->name);
            free_string(old->description);
            free_mem(old, sizeof(RESERVED_DATA));
        }
        iterator_stop(&it);
        list_clear(reserved_vnums);
    }
    
    /* Deserialize each entity */
    count = json_array_size(entities_array);
    for (i = 0; i < count; i++) {
        entity_obj = json_array_get(entities_array, i);
        if (!entity_obj || !json_is_object(entity_obj))
            continue;
        
        reserved = json_reserved_deserialize(entity_obj);
        if (reserved) {
            /* Validate entity */
            if (reserved->name && reserved->name[0] && reserved->type != -1) {
                list_appendlink(reserved_vnums, reserved);
            } else {
                /* Free invalid entry */
                if (reserved->name)
                    free_string(reserved->name);
                if (reserved->description)
                    free_string(reserved->description);
                free_mem(reserved, sizeof(RESERVED_DATA));
            }
        }
    }
    
    /* Clean up */
    json_decref(root);
    
    plogf(LOG_INFO, "Loaded %d reserved entities from JSON", list_size(reserved_vnums));
    
    return true;
}
