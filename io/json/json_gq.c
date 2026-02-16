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
#include "json_common.h"
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

/***************************************************************************
 * Helper Functions                                                        *
 ***************************************************************************/

/* WNUM helpers now provided by json_common.h */

static void gq_set_load_from_wnum(const WNUM *wnum, WNUM_LOAD *load)
{
    if (!load) return;

    load->vnum = (wnum ? wnum->vnum : 0);
    load->auid = (wnum && wnum->pArea) ? wnum->pArea->uid : 0;
}

static void gq_resolve_wnum_load(WNUM_LOAD *load, WNUM *wnum)
{
    AREA_DATA *fallback;

    if (!load || !wnum || load->vnum < 1) {
        if (wnum) *wnum = wnum_zero;
        return;
    }

    WNUM res;
    if (resolve_widevnum(load->vnum, NULL, &res))
        fallback = res.pArea;
    else
        fallback = get_system_area_fallback();

    resolve_wnum_load(load, wnum, fallback);
}

/* WNUM parse now provided by json_common.h */

/***************************************************************************
 * GQ Mob Serialization                                                    *
 ***************************************************************************/

/**
 * Convert GQ mob data to JSON
 */
json_t *gq_mob_to_json(GQ_MOB_DATA *gq_mob)
{
    json_t *json;
    WNUM_LOAD mob_load;
    WNUM_LOAD obj_load;
    
    if (!gq_mob) {
        return NULL;
    }
    
    json = json_object();
    
    /* Mob vnum as WNUM */
    mob_load = gq_mob->vnum_load;
    if (mob_load.vnum == 0) {
        gq_set_load_from_wnum(&gq_mob->vnum_wnum, &mob_load);
    }
    json_object_set_new(json, "mob_vnum", json_wnum_serialize(mob_load.auid, mob_load.vnum));
    
    /* Object vnum as WNUM (object carried by mob) */
    obj_load = gq_mob->obj_load;
    if (obj_load.vnum == 0) {
        gq_set_load_from_wnum(&gq_mob->obj_wnum, &obj_load);
    }
    if (obj_load.vnum != 0) {
        json_object_set_new(json, "obj_vnum", json_wnum_serialize(obj_load.auid, obj_load.vnum));
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
        json_wnum_deserialize(value, &area_uid, &vnum);
        gq_mob->vnum_load.auid = area_uid;
        gq_mob->vnum_load.vnum = vnum;
        gq_resolve_wnum_load(&gq_mob->vnum_load, &gq_mob->vnum_wnum);
    }
    
    /* Object vnum */
    value = json_object_get(json, "obj_vnum");
    if (value) {
        json_wnum_deserialize(value, &area_uid, &vnum);
        gq_mob->obj_load.auid = area_uid;
        gq_mob->obj_load.vnum = vnum;
        gq_resolve_wnum_load(&gq_mob->obj_load, &gq_mob->obj_wnum);
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
    WNUM_LOAD obj_load;
    
    if (!gq_obj) {
        return NULL;
    }
    
    json = json_object();
    
    /* Object vnum as WNUM */
    obj_load = gq_obj->vnum_load;
    if (obj_load.vnum == 0) {
        gq_set_load_from_wnum(&gq_obj->vnum_wnum, &obj_load);
    }
    json_object_set_new(json, "obj_vnum", json_wnum_serialize(obj_load.auid, obj_load.vnum));
    
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
        json_wnum_deserialize(value, &area_uid, &vnum);
        gq_obj->vnum_load.auid = area_uid;
        gq_obj->vnum_load.vnum = vnum;
        gq_resolve_wnum_load(&gq_obj->vnum_load, &gq_obj->vnum_wnum);
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
        log_string("gq.json not found");
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
    
    log_string("GQ data loaded from gq.json");
    return true;
}
