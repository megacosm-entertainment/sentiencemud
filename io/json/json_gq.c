/***************************************************************************
 *  JSON Global Quest Format - GQ Data Serialization with WNUM Support    *
 *                                                                          *
 *  This file contains serialization for global quest data:                *
 *  - GQ mob entries with vnums stored as WNUM                             *
 *  - GQ object entries with vnums stored as WNUM                          *
 *  - Tracking of spawned entities and rewards                             *
 *                                                                          *
 *  Storage: data/gq.json (replaces gq.dat)                                *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <jansson.h>
#include "../../merc.h"
#include "json_gq.h"
#include "../../recycle.h"

/***************************************************************************
 * External References                                                     *
 ***************************************************************************/

extern GQ_DATA global_quest;

/***************************************************************************
 * Constants                                                               *
 ***************************************************************************/

#define GQ_JSON_FILE "data/gq.json"
#define GQ_DAT_FILE  "data/world/gq.dat"

/***************************************************************************
 * Helper Functions                                                        *
 ***************************************************************************/

/**
 * Create a WNUM reference as a JSON string
 * Format: "area_uid#vnum" or bare "vnum"
 */
static json_t *wnum_to_json_str(long area_uid, long vnum)
{
    char buf[256];
    
    if (area_uid > 0) {
        snprintf(buf, sizeof(buf), "%ld#%ld", area_uid, vnum);
    } else {
        snprintf(buf, sizeof(buf), "%ld", vnum);
    }
    
    return json_string(buf);
}

/**
 * Parse a WNUM from JSON string
 * Supports both "area_uid#vnum" and bare "vnum"
 * Returns area_uid (0 if not specified) and vnum
 */
static void parse_wnum_from_json(json_t *json, long *area_uid, long *vnum)
{
    const char *str;
    
    *area_uid = 0;
    *vnum = 0;
    
    if (!json || !json_is_string(json)) {
        return;
    }
    
    str = json_string_value(json);
    if (!str) {
        return;
    }
    
    /* Check for area_uid#vnum format */
    if (strchr(str, '#')) {
        sscanf(str, "%ld#%ld", area_uid, vnum);
    } else {
        /* Bare vnum */
        *vnum = atol(str);
    }
}

/***************************************************************************
 * GQ Mob Serialization                                                    *
 ***************************************************************************/

/**
 * Convert GQ mob data to JSON
 */
json_t *gq_mob_to_json(GQ_MOB_DATA *gq_mob)
{
    json_t *json;
    AREA_DATA *mob_area, *obj_area;
    long mob_auid, obj_auid;
    
    if (!gq_mob) {
        return NULL;
    }
    
    json = json_object();
    
    /* Mob vnum as WNUM */
    mob_area = find_area_by_vnum(gq_mob->vnum, NULL);
    mob_auid = mob_area ? mob_area->uid : 0;
    json_object_set_new(json, "mob_vnum", wnum_to_json_str(mob_auid, gq_mob->vnum));
    
    /* Object vnum as WNUM (object carried by mob) */
    if (gq_mob->obj != 0) {
        obj_area = find_area_by_vnum(gq_mob->obj, NULL);
        obj_auid = obj_area ? obj_area->uid : 0;
        json_object_set_new(json, "obj_vnum", wnum_to_json_str(obj_auid, gq_mob->obj));
    }
    
    /* Properties */
    json_object_set_new(json, "class", json_integer(gq_mob->class));
    json_object_set_new(json, "group", json_boolean(gq_mob->group));
    json_object_set_new(json, "count", json_integer(gq_mob->count));
    json_object_set_new(json, "max", json_integer(gq_mob->max));
    
    return json;
}

/**
 * Convert JSON to GQ mob data
 */
GQ_MOB_DATA *json_to_gq_mob(json_t *json)
{
    GQ_MOB_DATA *gq_mob;
    json_t *value;
    long area_uid, vnum;
    
    if (!json || json_is_null(json)) {
        return NULL;
    }
    
    gq_mob = new_gq_mob();
    
    /* Mob vnum */
    value = json_object_get(json, "mob_vnum");
    if (value) {
        parse_wnum_from_json(value, &area_uid, &vnum);
        gq_mob->vnum = vnum;
    }
    
    /* Object vnum */
    value = json_object_get(json, "obj_vnum");
    if (value) {
        parse_wnum_from_json(value, &area_uid, &vnum);
        gq_mob->obj = vnum;
    }
    
    /* Properties */
    value = json_object_get(json, "class");
    if (value) gq_mob->class = json_integer_value(value);
    
    value = json_object_get(json, "group");
    if (value) gq_mob->group = json_is_true(value);
    
    value = json_object_get(json, "count");
    if (value) gq_mob->count = json_integer_value(value);
    
    value = json_object_get(json, "max");
    if (value) gq_mob->max = json_integer_value(value);
    
    return gq_mob;
}

/***************************************************************************
 * GQ Object Serialization                                                 *
 ***************************************************************************/

/**
 * Convert GQ object data to JSON
 */
json_t *gq_obj_to_json(GQ_OBJ_DATA *gq_obj)
{
    json_t *json;
    AREA_DATA *obj_area;
    long obj_auid;
    
    if (!gq_obj) {
        return NULL;
    }
    
    json = json_object();
    
    /* Object vnum as WNUM */
    obj_area = find_area_by_vnum(gq_obj->vnum, NULL);
    obj_auid = obj_area ? obj_area->uid : 0;
    json_object_set_new(json, "obj_vnum", wnum_to_json_str(obj_auid, gq_obj->vnum));
    
    /* Rewards */
    json_object_set_new(json, "qp_reward", json_integer(gq_obj->qp_reward));
    json_object_set_new(json, "prac_reward", json_integer(gq_obj->prac_reward));
    json_object_set_new(json, "exp_reward", json_integer(gq_obj->exp_reward));
    json_object_set_new(json, "silver_reward", json_integer(gq_obj->silver_reward));
    json_object_set_new(json, "gold_reward", json_integer(gq_obj->gold_reward));
    
    /* Tracking */
    json_object_set_new(json, "repop", json_integer(gq_obj->repop));
    json_object_set_new(json, "max", json_integer(gq_obj->max));
    json_object_set_new(json, "count", json_integer(gq_obj->count));
    
    return json;
}

/**
 * Convert JSON to GQ object data
 */
GQ_OBJ_DATA *json_to_gq_obj(json_t *json)
{
    GQ_OBJ_DATA *gq_obj;
    json_t *value;
    long area_uid, vnum;
    
    if (!json || json_is_null(json)) {
        return NULL;
    }
    
    gq_obj = new_gq_obj();
    
    /* Object vnum */
    value = json_object_get(json, "obj_vnum");
    if (value) {
        parse_wnum_from_json(value, &area_uid, &vnum);
        gq_obj->vnum = vnum;
    }
    
    /* Rewards */
    value = json_object_get(json, "qp_reward");
    if (value) gq_obj->qp_reward = json_integer_value(value);
    
    value = json_object_get(json, "prac_reward");
    if (value) gq_obj->prac_reward = json_integer_value(value);
    
    value = json_object_get(json, "exp_reward");
    if (value) gq_obj->exp_reward = json_integer_value(value);
    
    value = json_object_get(json, "silver_reward");
    if (value) gq_obj->silver_reward = json_integer_value(value);
    
    value = json_object_get(json, "gold_reward");
    if (value) gq_obj->gold_reward = json_integer_value(value);
    
    /* Tracking */
    value = json_object_get(json, "repop");
    if (value) gq_obj->repop = json_integer_value(value);
    
    value = json_object_get(json, "max");
    if (value) gq_obj->max = json_integer_value(value);
    
    value = json_object_get(json, "count");
    if (value) gq_obj->count = json_integer_value(value);
    
    return gq_obj;
}

/***************************************************************************
 * File I/O                                                                *
 ***************************************************************************/

/**
 * Save global quest data to JSON file
 */
bool save_gq_json(void)
{
    json_t *root, *mob_array, *obj_array;
    GQ_MOB_DATA *gq_mob;
    GQ_OBJ_DATA *gq_obj;
    int ret;
    
    root = json_object();
    
    /* Convert mobs to JSON */
    mob_array = json_array();
    for (gq_mob = global_quest.mobs; gq_mob != NULL; gq_mob = gq_mob->next) {
        json_t *mob_json = gq_mob_to_json(gq_mob);
        if (mob_json) {
            json_array_append_new(mob_array, mob_json);
        }
    }
    json_object_set_new(root, "mobs", mob_array);
    
    /* Convert objects to JSON */
    obj_array = json_array();
    for (gq_obj = global_quest.objects; gq_obj != NULL; gq_obj = gq_obj->next) {
        json_t *obj_json = gq_obj_to_json(gq_obj);
        if (obj_json) {
            json_array_append_new(obj_array, obj_json);
        }
    }
    json_object_set_new(root, "objects", obj_array);
    
    json_object_set_new(root, "version", json_integer(1));
    
    /* Write to file */
    ret = json_dump_file(root, GQ_JSON_FILE, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
    json_decref(root);
    
    if (ret != 0) {
        log_string("save_gq_json: Failed to write gq.json");
        return false;
    }
    
    log_string("GQ data saved to gq.json");
    return true;
}

/**
 * Load global quest data from JSON file
 */
bool load_gq_json(void)
{
    json_t *root, *mob_array, *obj_array;
    json_error_t error;
    size_t idx;
    json_t *elem;
    GQ_MOB_DATA *gq_mob, *last_mob = NULL;
    GQ_OBJ_DATA *gq_obj, *last_obj = NULL;
    
    /* Check if JSON file exists */
    if (access(GQ_JSON_FILE, F_OK) != 0) {
        /* Try legacy .dat file */
        if (access(GQ_DAT_FILE, F_OK) == 0) {
            log_string("gq.json not found, legacy gq.dat will be loaded by read_gq()");
            return false;
        }
        log_string("No GQ file found (tried .json and .dat)");
        return true; /* Not an error, just no GQ data */
    }
    
    /* Load JSON file */
    root = json_load_file(GQ_JSON_FILE, 0, &error);
    if (!root) {
        log_stringf("load_gq_json: JSON parse error on line %d: %s", error.line, error.text);
        return false;
    }
    
    /* Parse mobs array */
    mob_array = json_object_get(root, "mobs");
    if (mob_array && json_is_array(mob_array)) {
        json_array_foreach(mob_array, idx, elem) {
            gq_mob = json_to_gq_mob(elem);
            if (gq_mob) {
                gq_mob->next = NULL;
                if (global_quest.mobs == NULL) {
                    global_quest.mobs = gq_mob;
                } else {
                    last_mob->next = gq_mob;
                }
                last_mob = gq_mob;
            }
        }
    }
    
    /* Parse objects array */
    obj_array = json_object_get(root, "objects");
    if (obj_array && json_is_array(obj_array)) {
        json_array_foreach(obj_array, idx, elem) {
            gq_obj = json_to_gq_obj(elem);
            if (gq_obj) {
                gq_obj->next = NULL;
                if (global_quest.objects == NULL) {
                    global_quest.objects = gq_obj;
                } else {
                    last_obj->next = gq_obj;
                }
                last_obj = gq_obj;
            }
        }
    }
    
    json_decref(root);
    
    /* If we loaded an empty JSON file, try .dat fallback */
    if (global_quest.mobs == NULL && global_quest.objects == NULL) {
        if (access(GQ_DAT_FILE, F_OK) == 0) {
            log_string("gq.json is empty, falling back to gq.dat");
            return false;
        }
    }
    
    log_string("GQ data loaded from gq.json");
    return true;
}
