/***************************************************************************
 *  JSON Area Serialization - Implementation                               *
 *                                                                          *
 *  Converts area files between C structures and JSON format.              *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <jansson.h>
#include "merc.h"
#include "tables.h"
#include "recycle.h"
#include "scripts.h"
#include "json_area.h"
#include "redis_cache.h"

#include "wilds.h"

// --- WILDS_TERRAIN JSON helpers ---
static json_t *wilds_terrain_to_json(WILDS_TERRAIN *terrain) {
    if (!terrain) return NULL;
    json_t *json = json_object();
    json_object_set_new(json, "mapchar", json_integer(terrain->mapchar));
    json_object_set_new(json, "showchar", json_string(terrain->showchar ? terrain->showchar : ""));
    json_object_set_new(json, "showname", json_string(terrain->showname ? terrain->showname : ""));
    json_object_set_new(json, "briefdesc", json_string(terrain->briefdesc ? terrain->briefdesc : ""));
    json_object_set_new(json, "nonroom", terrain->nonroom ? json_true() : json_false());
    if (terrain->template) {
        json_object_set_new(json, "template", json_area_serialize_room(terrain->template));
    }
    return json;
}

static WILDS_TERRAIN *json_to_wilds_terrain(json_t *json, WILDS_DATA *pWilds) {
    if (!json) return NULL;
    WILDS_TERRAIN *terrain = new_terrain(pWilds);
    terrain->mapchar = (char)json_get_int_default(json, "mapchar", '?');
    terrain->showchar = str_dup(json_get_string_default(json, "showchar", ""));
    terrain->showname = str_dup(json_get_string_default(json, "showname", ""));
    terrain->briefdesc = str_dup(json_get_string_default(json, "briefdesc", ""));
    terrain->nonroom = json_get_bool_default(json, "nonroom", false);
    json_t *template_json = json_object_get(json, "template");
    if (template_json) {
        terrain->template = json_area_deserialize_room(template_json, pWilds->pArea);
    }
    return terrain;
}

// --- WILDS_VLINK JSON helpers ---
static json_t *wilds_vlink_to_json(WILDS_VLINK *vlink) {
    if (!vlink) return NULL;
    json_t *json = json_object();
    json_object_set_new(json, "uid", json_integer(vlink->uid));
    json_object_set_new(json, "wildsorigin_x", json_integer(vlink->wildsorigin_x));
    json_object_set_new(json, "wildsorigin_y", json_integer(vlink->wildsorigin_y));
    json_object_set_new(json, "door", json_integer(vlink->door));
    json_object_set_new(json, "map_tile", json_string(vlink->map_tile ? vlink->map_tile : ""));
    json_object_set_new(json, "destvnum", json_integer(vlink->destvnum));
    json_object_set_new(json, "default_linkage", json_integer(vlink->default_linkage));
    json_object_set_new(json, "current_linkage", json_integer(vlink->current_linkage));
    json_object_set_new(json, "orig_description", json_string(vlink->orig_description ? vlink->orig_description : ""));
    json_object_set_new(json, "orig_keyword", json_string(vlink->orig_keyword ? vlink->orig_keyword : ""));
    json_object_set_new(json, "orig_rs_flags", json_integer(vlink->orig_rs_flags));
    json_object_set_new(json, "orig_key", json_integer(vlink->orig_key));
    json_object_set_new(json, "orig_lock", json_integer(vlink->orig_lock));
    json_object_set_new(json, "orig_pick", json_integer(vlink->orig_pick));
    json_object_set_new(json, "rev_description", json_string(vlink->rev_description ? vlink->rev_description : ""));
    json_object_set_new(json, "rev_keyword", json_string(vlink->rev_keyword ? vlink->rev_keyword : ""));
    json_object_set_new(json, "rev_rs_flags", json_integer(vlink->rev_rs_flags));
    json_object_set_new(json, "rev_key", json_integer(vlink->rev_key));
    json_object_set_new(json, "rev_lock", json_integer(vlink->rev_lock));
    json_object_set_new(json, "rev_pick", json_integer(vlink->rev_pick));
    // pWilds, pWildsVroom, pDestRoom not serialized
    return json;
}

static WILDS_VLINK *json_to_wilds_vlink(json_t *json, WILDS_DATA *pWilds) {
    if (!json) return NULL;
    WILDS_VLINK *vlink = new_vlink();
    vlink->pWilds = pWilds;
    vlink->uid = json_get_int_default(json, "uid", 0);
    vlink->wildsorigin_x = json_get_int_default(json, "wildsorigin_x", 0);
    vlink->wildsorigin_y = json_get_int_default(json, "wildsorigin_y", 0);
    vlink->door = json_get_int_default(json, "door", 0);
    vlink->map_tile = str_dup(json_get_string_default(json, "map_tile", ""));
    vlink->destvnum = json_get_int_default(json, "destvnum", 0);
    vlink->default_linkage = json_get_int_default(json, "default_linkage", 0);
    vlink->current_linkage = json_get_int_default(json, "current_linkage", 0);
    vlink->orig_description = str_dup(json_get_string_default(json, "orig_description", ""));
    vlink->orig_keyword = str_dup(json_get_string_default(json, "orig_keyword", ""));
    vlink->orig_rs_flags = json_get_int_default(json, "orig_rs_flags", 0);
    vlink->orig_key = json_get_int_default(json, "orig_key", 0);
    vlink->orig_lock = json_get_int_default(json, "orig_lock", 0);
    vlink->orig_pick = json_get_int_default(json, "orig_pick", 0);
    vlink->rev_description = str_dup(json_get_string_default(json, "rev_description", ""));
    vlink->rev_keyword = str_dup(json_get_string_default(json, "rev_keyword", ""));
    vlink->rev_rs_flags = json_get_int_default(json, "rev_rs_flags", 0);
    vlink->rev_key = json_get_int_default(json, "rev_key", 0);
    vlink->rev_lock = json_get_int_default(json, "rev_lock", 0);
    vlink->rev_pick = json_get_int_default(json, "rev_pick", 0);
    // pWildsVroom, pDestRoom not deserialized
    return vlink;
}

// --- WILDS_DATA JSON helpers ---
static json_t *wilds_to_json(WILDS_DATA *wilds) {
    if (!wilds) return NULL;
    json_t *json = json_object();
    json_object_set_new(json, "uid", json_integer(wilds->uid));
    json_object_set_new(json, "name", json_string(wilds->name ? wilds->name : ""));
    json_object_set_new(json, "wilds_format", json_integer(wilds->wilds_format));
    json_object_set_new(json, "staticmap", json_string(wilds->staticmap ? wilds->staticmap : ""));
    json_object_set_new(json, "map_size_x", json_integer(wilds->map_size_x));
    json_object_set_new(json, "map_size_y", json_integer(wilds->map_size_y));
    json_object_set_new(json, "startx", json_integer(wilds->startx));
    json_object_set_new(json, "starty", json_integer(wilds->starty));
    json_object_set_new(json, "sector_size_x", json_integer(wilds->sector_size_x));
    json_object_set_new(json, "sector_size_y", json_integer(wilds->sector_size_y));
    json_object_set_new(json, "cDefaultTerrain", json_integer(wilds->cDefaultTerrain));
    // Terrain list
    json_t *terrains = json_array();
    for (WILDS_TERRAIN *t = wilds->pTerrain; t; t = t->next) {
        json_t *tjson = wilds_terrain_to_json(t);
        if (tjson) json_array_append_new(terrains, tjson);
    }
    json_object_set_new(json, "terrains", terrains);
    // Vlink list
    json_t *vlinks = json_array();
    for (WILDS_VLINK *vl = wilds->pVLink; vl; vl = vl->next) {
        json_t *vj = wilds_vlink_to_json(vl);
        if (vj) json_array_append_new(vlinks, vj);
    }
    json_object_set_new(json, "vlinks", vlinks);
    return json;
}

static WILDS_DATA *json_to_wilds(json_t *json, AREA_DATA *area) {
    if (!json) return NULL;
    WILDS_DATA *wilds = new_wilds();
    wilds->pArea = area;
    wilds->uid = json_get_int_default(json, "uid", 0);
    wilds->name = str_dup(json_get_string_default(json, "name", ""));
    wilds->wilds_format = json_get_int_default(json, "wilds_format", 0);
    wilds->map_size_x = json_get_int_default(json, "map_size_x", 0);
    wilds->map_size_y = json_get_int_default(json, "map_size_y", 0);

    // Allocate staticmap and map buffers like the .are loader does
    const char *json_map = json_get_string_default(json, "staticmap", "");
    int map_total = wilds->map_size_x * wilds->map_size_y;
    if (map_total > 0) {
        wilds->staticmap = allocate_wildsmap(wilds->map_size_x, wilds->map_size_y);
        wilds->map = allocate_wildsmap(wilds->map_size_x, wilds->map_size_y);

        // Copy map data directly - newlines in the JSON ARE terrain data, not separators
        // The .are format stores rows with trailing newlines stripped, but embedded newlines
        // (like at the start of the map) are valid terrain characters
        memcpy(wilds->staticmap, json_map, map_total);
        // Copy staticmap to map (working copy)
        memcpy(wilds->map, wilds->staticmap, map_total);
    } else {
        wilds->staticmap = str_dup("");
        wilds->map = str_dup("");
    }
    wilds->startx = json_get_int_default(json, "startx", 0);
    wilds->starty = json_get_int_default(json, "starty", 0);
    wilds->sector_size_x = json_get_int_default(json, "sector_size_x", 0);
    wilds->sector_size_y = json_get_int_default(json, "sector_size_y", 0);
    wilds->cDefaultTerrain = (char)json_get_int_default(json, "cDefaultTerrain", '?');
    // Terrains
    json_t *terrains = json_object_get(json, "terrains");
    if (terrains && json_is_array(terrains)) {
        size_t idx; json_t *tjson;
        WILDS_TERRAIN *last = NULL;
        json_array_foreach(terrains, idx, tjson) {
            WILDS_TERRAIN *t = json_to_wilds_terrain(tjson, wilds);
            if (t) {
                if (!wilds->pTerrain) wilds->pTerrain = t;
                else last->next = t;
                t->prev = last;
                last = t;
            }
        }
    }
    // Vlinks
    json_t *vlinks = json_object_get(json, "vlinks");
    if (vlinks && json_is_array(vlinks)) {
        size_t idx; json_t *vj;
        WILDS_VLINK *last = NULL;
        json_array_foreach(vlinks, idx, vj) {
            WILDS_VLINK *vl = json_to_wilds_vlink(vj, wilds);
            if (vl) {
                if (!wilds->pVLink) wilds->pVLink = vl;
                else last->next = vl;
                last = vl;
            }
        }
    }
    return wilds;
}

/* External references */
extern const struct flag_type area_flags[];
extern const struct flag_type room_flags[];
extern const struct flag_type room2_flags[];
extern const struct flag_type exit_flags[];
extern const struct flag_type sector_flags[];
extern const struct flag_type act_flags[];
extern const struct flag_type act2_flags[];
extern const struct flag_type affect_flags[];
extern const struct flag_type affect2_flags[];
extern const struct flag_type off_flags[];
extern const struct flag_type imm_flags[];
extern const struct flag_type res_flags[];
extern const struct flag_type vuln_flags[];
extern const struct flag_type form_flags[];
extern const struct flag_type part_flags[];
extern const struct flag_type extra_flags[];
extern const struct flag_type extra2_flags[];
extern const struct flag_type wear_flags[];
extern const struct flag_type script_flags[];
extern const struct flag_type token_flags[];
extern const struct sex_type sex_table[];
extern const struct size_type size_table[];
extern const struct position_type position_table[];
extern const struct flag_type catalyst_types[];
extern struct trigger_type trigger_table[];
extern const struct trade_type trade_table[];
extern int trigger_table_size;
extern int trigger_index(char *name, int type);

/***************************************************************************
 * Utility Functions                                                       *
 ***************************************************************************/

const char *json_get_string_default(json_t *obj, const char *key, const char *default_val)
{
    json_t *value = json_object_get(obj, key);
    if (!value || !json_is_string(value))
        return default_val;
    return json_string_value(value);
}

long json_get_int_default(json_t *obj, const char *key, long default_val)
{
    json_t *value = json_object_get(obj, key);
    if (!value || !json_is_integer(value))
        return default_val;
    return json_integer_value(value);
}

bool json_get_bool_default(json_t *obj, const char *key, bool default_val)
{
    json_t *value = json_object_get(obj, key);
    if (!value || !json_is_boolean(value))
        return default_val;
    return json_is_true(value);
}

json_t *flags_to_json_array(long flags, const struct flag_type *flag_table)
{
    json_t *array = json_array();
    int i;
    
    if (!flag_table)
        return array;
    
    for (i = 0; flag_table[i].name != NULL; i++) {
        if (IS_SET(flags, flag_table[i].bit)) {
            json_array_append_new(array, json_string(flag_table[i].name));
        }
    }
    
    return array;
}

long json_array_to_flags(json_t *array, const struct flag_type *flag_table)
{
    long flags = 0;
    size_t index;
    json_t *value;
    
    if (!json_is_array(array) || !flag_table)
        return 0;
    
    json_array_foreach(array, index, value) {
        if (json_is_string(value)) {
            const char *name = json_string_value(value);
            int i;
            
            for (i = 0; flag_table[i].name != NULL; i++) {
                if (!str_cmp(name, flag_table[i].name)) {
                    SET_BIT(flags, flag_table[i].bit);
                    break;
                }
            }
        }
    }
    
    return flags;
}

/***************************************************************************
 * Area Metadata Serialization                                            *
 ***************************************************************************/

json_t *json_area_serialize_metadata(AREA_DATA *area)
{
    json_t *root = json_object();
    json_t *vnums = json_object();
    json_t *metadata = json_object();
    json_t *coords = json_object();
    json_t *recall = json_object();
    json_t *versions = json_object();
    
    /* Basic info */
    json_object_set_new(root, "uid", json_integer(area->uid));
    json_object_set_new(root, "name", json_string(area->name ? area->name : ""));
    json_object_set_new(root, "filename", json_string(area->file_name ? area->file_name : ""));
    
    /* Vnum range */
    json_object_set_new(vnums, "min", json_integer(area->min_vnum));
    json_object_set_new(vnums, "max", json_integer(area->max_vnum));
    json_object_set_new(root, "vnums", vnums);
    
    /* Metadata */
    json_object_set_new(metadata, "builders", json_string(area->builders ? area->builders : "None"));
    json_object_set_new(metadata, "credits", json_string(area->credits ? area->credits : ""));
    json_object_set_new(metadata, "security", json_integer(area->security));
    json_object_set_new(metadata, "area_who", json_integer(area->area_who));
    
    json_t *levels = json_object();
    json_object_set_new(levels, "min", json_integer(area->low_range));
    json_object_set_new(levels, "max", json_integer(area->high_range));
    json_object_set_new(metadata, "levels", levels);
    json_object_set_new(root, "metadata", metadata);
    
    /* Flags */
    json_object_set_new(root, "flags", flags_to_json_array(area->area_flags, area_flags));
    /* place_type not in current structure - skip for now */
    
    /* Coordinates */
    json_object_set_new(coords, "x", json_integer(area->x));
    json_object_set_new(coords, "y", json_integer(area->y));
    json_object_set_new(coords, "x_land", json_integer(area->land_x));
    json_object_set_new(coords, "y_land", json_integer(area->land_y));
    json_object_set_new(root, "coordinates", coords);
    
    /* Recall location - recall is a LOCATION structure */
    if (area->recall.wuid > 0) {
        json_object_set_new(recall, "wilds_uid", json_integer(area->recall.wuid));
        json_object_set_new(recall, "x", json_integer(area->recall.id[0]));
        json_object_set_new(recall, "y", json_integer(area->recall.id[1]));
        json_object_set_new(recall, "z", json_integer(area->recall.id[2]));
    } else if (area->recall.id[0] > 0) {
        json_object_set_new(recall, "vnum", json_integer(area->recall.id[0]));
    }
    json_object_set_new(root, "recall", recall);
    
    /* Other settings */
    json_object_set_new(root, "wilds_uid", json_integer(area->wilds_uid));
    json_object_set_new(root, "repop", json_integer(area->repop));
    json_object_set_new(root, "post_office", json_integer(area->post_office));
    json_object_set_new(root, "airship_land", json_integer(area->airship_land_spot));
    
    /* Descriptions */
    json_object_set_new(root, "description", json_string(area->description ? area->description : ""));
    json_object_set_new(root, "comments", json_string(area->comments ? area->comments : ""));
    json_object_set_new(root, "notes", json_string(area->notes ? area->notes : ""));
    
    /* Version tracking */
    json_object_set_new(versions, "area", json_integer(area->version_area));
    json_object_set_new(versions, "mobile", json_integer(area->version_mobile));
    json_object_set_new(versions, "object", json_integer(area->version_object));
    json_object_set_new(versions, "room", json_integer(area->version_room));
    json_object_set_new(versions, "token", json_integer(area->version_token));
    json_object_set_new(versions, "script", json_integer(area->version_script));
    json_object_set_new(versions, "wilds", json_integer(area->version_wilds));
    json_object_set_new(root, "versions", versions);
    
    /* Scripts/progs - area->progs->progs */
    if (area->progs && area->progs->progs) {
        json_t *progs_json = json_area_serialize_progs(area->progs->progs, area);
        if (progs_json)
            json_object_set_new(root, "progs", progs_json);
    }
    
    /* Index vars */
    if (area->index_vars) {
        json_t *index_vars = json_area_serialize_index_vars(area->index_vars, area);
        if (index_vars)
            json_object_set_new(root, "index_vars", index_vars);
    }
    
    /* Trade list */
    if (area->trade_list) {
        json_t *trade_list = json_area_serialize_trade_list(area->trade_list, area);
        if (trade_list)
            json_object_set_new(root, "trade", trade_list);
    }
    
    // Add wilds/wilderness serialization if present
    if (area->wilds) {
        json_t *wilds_json = wilds_to_json(area->wilds);
        if (wilds_json) json_object_set_new(root, "wilderness", wilds_json);
    }
    return root;
}

bool json_area_deserialize_metadata(json_t *json, AREA_DATA *area)
{
    json_t *vnums, *metadata, *coords, *recall, *versions, *levels;
    
    /* Basic info */
    area->uid = json_get_int_default(json, "uid", 0);
    area->name = str_dup(json_get_string_default(json, "name", ""));
    area->file_name = str_dup(json_get_string_default(json, "filename", ""));
    
    /* Vnum range */
    vnums = json_object_get(json, "vnums");
    if (vnums) {
        area->min_vnum = json_get_int_default(vnums, "min", 0);
        area->max_vnum = json_get_int_default(vnums, "max", 0);
    }
    
    /* Metadata */
    metadata = json_object_get(json, "metadata");
    if (metadata) {
        area->builders = str_dup(json_get_string_default(metadata, "builders", "None"));
        area->credits = str_dup(json_get_string_default(metadata, "credits", ""));
        area->security = json_get_int_default(metadata, "security", 9);
        area->area_who = json_get_int_default(metadata, "area_who", 0);
        
        levels = json_object_get(metadata, "levels");
        if (levels) {
            area->low_range = json_get_int_default(levels, "min", 0);
            area->high_range = json_get_int_default(levels, "max", 0);
        }
    }
    
    /* Flags */
    area->area_flags = json_array_to_flags(json_object_get(json, "flags"), area_flags);
    
    /* Coordinates */
    coords = json_object_get(json, "coordinates");
    if (coords) {
        area->x = json_get_int_default(coords, "x", 0);
        area->y = json_get_int_default(coords, "y", 0);
        area->land_x = json_get_int_default(coords, "x_land", -1);
        area->land_y = json_get_int_default(coords, "y_land", -1);
    }
    
    /* Recall - handle LOCATION structure */
    recall = json_object_get(json, "recall");
    if (recall) {
        location_clear(&area->recall);
        if (json_object_get(recall, "wilds_uid")) {
            area->recall.wuid = json_get_int_default(recall, "wilds_uid", 0);
            area->recall.id[0] = json_get_int_default(recall, "x", 0);
            area->recall.id[1] = json_get_int_default(recall, "y", 0);
            area->recall.id[2] = json_get_int_default(recall, "z", 0);
        } else {
            area->recall.id[0] = json_get_int_default(recall, "vnum", 0);
        }
    }
    
    /* Progs */
    if (area->progs && !area->progs->progs)
        area->progs->progs = json_area_deserialize_progs(json_object_get(json, "progs"), area, PRG_MPROG);
    
    /* Index vars */
    area->index_vars = json_area_deserialize_index_vars(json_object_get(json, "index_vars"), area);
    
    /* Trade list */
    json_area_deserialize_trade_list(json_object_get(json, "trade"), area);
    
    /* Other settings */
    area->wilds_uid = json_get_int_default(json, "wilds_uid", 0);
    // Wilderness/wilds deserialization
    json_t *wilds_json = json_object_get(json, "wilderness");
    if (wilds_json) {
        area->wilds = json_to_wilds(wilds_json, area);
        if (area->wilds) {
            area->wilds_uid = area->wilds->uid;
            // Register wilds in global list if needed (implementation-specific)
        }
    }
    area->repop = json_get_int_default(json, "repop", 15);
    area->post_office = json_get_int_default(json, "post_office", 0);
    area->airship_land_spot = json_get_int_default(json, "airship_land", 0);
    
    /* Descriptions */
    area->description = str_dup(json_get_string_default(json, "description", ""));
    area->comments = str_dup(json_get_string_default(json, "comments", ""));
    area->notes = str_dup(json_get_string_default(json, "notes", ""));
    
    /* Versions */
    versions = json_object_get(json, "versions");
    if (versions) {
        area->version_area = json_get_int_default(versions, "area", VERSION_AREA);
        area->version_mobile = json_get_int_default(versions, "mobile", VERSION_MOBILE);
        area->version_object = json_get_int_default(versions, "object", VERSION_OBJECT);
        area->version_room = json_get_int_default(versions, "room", VERSION_ROOM);
        area->version_token = json_get_int_default(versions, "token", VERSION_TOKEN);
        area->version_script = json_get_int_default(versions, "script", VERSION_SCRIPT);
        area->version_wilds = json_get_int_default(versions, "wilds", VERSION_WILDS);
    }
    
    return true;
}

/***************************************************************************
 * Exit Serialization                                                      *
 ***************************************************************************/

json_t *json_area_serialize_exit(EXIT_DATA *exit)
{
    json_t *obj = json_object();
    
    if (!exit)
        return obj;

    json_object_set_new(obj, "direction", json_string(dir_name[exit->orig_door]));

    /* Destination - save the vnum from to_room if it exists */
    if (exit->u1.to_room && exit->u1.to_room->vnum > 0) {
        json_object_set_new(obj, "to_area", json_integer(exit->u1.to_room->area ? exit->u1.to_room->area->uid : 0));
        json_object_set_new(obj, "to_vnum", json_integer(exit->u1.to_room->vnum));
    }

    /* Keywords and descriptions */
    if (exit->keyword && exit->keyword[0] != '\0')
        json_object_set_new(obj, "keywords", json_string(exit->keyword));
    if (exit->short_desc && exit->short_desc[0] != '\0')
        json_object_set_new(obj, "description", json_string(exit->short_desc));
    if (exit->long_desc && exit->long_desc[0] != '\0')
        json_object_set_new(obj, "long_description", json_string(exit->long_desc));

    /* Lock/key/pick fields */
    json_object_set_new(obj, "key_vnum", json_integer(exit->door.lock.key_vnum));
    json_object_set_new(obj, "lock_flags", json_integer(exit->door.lock.flags));
    json_object_set_new(obj, "pick_chance", json_integer(exit->door.lock.pick_chance));

    /* Flags */
    json_object_set_new(obj, "flags", flags_to_json_array(exit->exit_info, exit_flags));

    return obj;
}

EXIT_DATA *json_area_deserialize_exit(json_t *json, AREA_DATA *area)
{
    EXIT_DATA *exit = new_exit();
    json_t *wilds_obj;
    const char *direction;
    int door;
    
    direction = json_get_string_default(json, "direction", "north");
    
    /* Find direction number */
    for (door = 0; door < MAX_DIR; door++) {
        if (!str_cmp(direction, dir_name[door])) {
            exit->orig_door = door;
            break;
        }
    }
    
    /* Destination - will be linked in fix_exits() */
    long to_vnum = json_get_int_default(json, "to_vnum", 0);
    if (to_vnum > 0) {
        exit->u1.vnum = to_vnum;
        /* long to_area_uid = json_get_int_default(json, "to_area", 0); - not yet used */
    }

    /* Wilderness exit */
    wilds_obj = json_object_get(json, "wilderness");
    if (wilds_obj) {
        exit->wilds.x = json_get_int_default(wilds_obj, "x", 0);
        exit->wilds.y = json_get_int_default(wilds_obj, "y", 0);
        exit->wilds.area_uid = json_get_int_default(wilds_obj, "area_uid", 0);
        exit->wilds.wilds_uid = json_get_int_default(wilds_obj, "wilds_uid", 0);
        SET_BIT(exit->exit_info, EX_VLINK);
    }

    /* Keywords and descriptions */
    exit->keyword = str_dup(json_get_string_default(json, "keywords", ""));
    exit->short_desc = str_dup(json_get_string_default(json, "description", ""));
    exit->long_desc = str_dup(json_get_string_default(json, "long_description", ""));

    /* Lock/key/pick fields */
    exit->door.lock.key_vnum = json_get_int_default(json, "key_vnum", 0);
    exit->door.lock.flags = json_get_int_default(json, "lock_flags", 0);
    exit->door.lock.pick_chance = json_get_int_default(json, "pick_chance", 100);

    /* Flags */
    exit->exit_info = json_array_to_flags(json_object_get(json, "flags"), exit_flags);

    return exit;
}

/***************************************************************************
 * Main Load/Save Functions                                                *
 ***************************************************************************/

AREA_DATA *json_area_load(const char *filename)
{
    char path[512];
    json_t *root, *area_obj;
    json_error_t error;
    AREA_DATA *area;
    
    /* Build full path */
    snprintf(path, sizeof(path), "%s%s", AREA_DIR, filename);
    
    /* Load JSON file */
    root = json_load_file(path, 0, &error);
    if (!root) {
        log_stringf("json_area_load: Failed to load %s: %s (line %d)", 
                   path, error.text, error.line);
        return NULL;
    }
    
    /* Verify schema version */
    const char *schema_version = json_get_string_default(root, "schema_version", "");
    if (str_cmp(schema_version, JSON_AREA_SCHEMA_VERSION)) {
        log_stringf("json_area_load: Schema version mismatch in %s (expected %s, got %s)",
                   filename, JSON_AREA_SCHEMA_VERSION, schema_version);
        json_decref(root);
        return NULL;
    }
    
    /* Create new area */
    area = new_area();
    
    /* Deserialize metadata */
    area_obj = json_object_get(root, "area");
    if (area_obj) {
        json_area_deserialize_metadata(area_obj, area);
    }
    
    /* Deserialize rooms */
    json_t *rooms = json_object_get(root, "rooms");
    if (rooms && json_is_array(rooms)) {
        size_t index;
        json_t *room_json;
        json_array_foreach(rooms, index, room_json) {
            ROOM_INDEX_DATA *room = json_area_deserialize_room(room_json, area);
            /* Room is already added to hash table by new_room_index() */
            (void)room;
        }
    }
    
    /* Deserialize mobiles - must be before objects since resets need them */
    json_t *mobiles = json_object_get(root, "mobiles");
    if (mobiles && json_is_array(mobiles)) {
        size_t index;
        json_t *mob_json;
        json_array_foreach(mobiles, index, mob_json) {
            MOB_INDEX_DATA *mob = json_area_deserialize_mobile(mob_json, area);
            /* Mob is already added to hash table by new_mob_index() */
            (void)mob;
        }
    }
    
    /* Deserialize objects - must be before tokens since tokens may reference them */
    json_t *objects = json_object_get(root, "objects");
    if (objects && json_is_array(objects)) {
        size_t index;
        json_t *obj_json;
        json_array_foreach(objects, index, obj_json) {
            OBJ_INDEX_DATA *obj = json_area_deserialize_object(obj_json, area);
            /* Object is already added to hash table by new_obj_index() */
            (void)obj;
        }
    }
    
    /* Deserialize tokens */
    json_t *tokens = json_object_get(root, "tokens");
    if (tokens && json_is_array(tokens)) {
        size_t index;
        json_t *token_json;
        json_array_foreach(tokens, index, token_json) {
            TOKEN_INDEX_DATA *token = json_area_deserialize_token(token_json, area);
            /* Token is already added to hash table by new_token_index() */
            (void)token;
        }
    }
    
    /* NOW fix up resets - all mobs and objects must exist first */
    /* Resets were deferred during room deserialization */
    if (rooms && json_is_array(rooms)) {
        size_t index;
        json_t *room_json;
        json_array_foreach(rooms, index, room_json) {
            long vnum = json_get_int_default(room_json, "vnum", 0);
            ROOM_INDEX_DATA *room = get_room_index(area, vnum);
            if (!room) continue;
            
            /* Process resets for this room */
            json_t *resets = json_object_get(room_json, "resets");
            if (resets && json_is_array(resets)) {
                size_t reset_index;
                json_t *reset_json;
                RESET_DATA *last_reset = NULL;
                
                json_array_foreach(resets, reset_index, reset_json) {
                    RESET_DATA *reset = json_area_deserialize_reset(reset_json, area);
                    if (reset) {
                        if (!room->reset_first) {
                            room->reset_first = reset;
                        }
                        if (last_reset) {
                            last_reset->next = reset;
                        }
                        last_reset = reset;
                        room->reset_last = reset;
                    }
                }
            }
        }
    }
    /* Shop stock pointers will be fixed up globally after all areas are loaded */
    
    /* Deserialize scripts */
    /* Load all 7 types of scripts and add to area prog lists */
    json_t *scripts;
    size_t script_index;
    json_t *script_json;
    SCRIPT_DATA *script;
    
    /* Mobile progs */
    scripts = json_object_get(root, "mobprogs");
    if (scripts && json_is_array(scripts)) {
        json_array_foreach(scripts, script_index, script_json) {
            script = json_area_deserialize_script(script_json, area, IFC_M);
            if (script) {
                /* Add to area's mprog_list */
                script->next = area->mprog_list;
                area->mprog_list = script;
            }
        }
    }
    
    /* Object progs */
    scripts = json_object_get(root, "oprogs");
    if (scripts && json_is_array(scripts)) {
        json_array_foreach(scripts, script_index, script_json) {
            script = json_area_deserialize_script(script_json, area, IFC_O);
            if (script) {
                script->next = area->oprog_list;
                area->oprog_list = script;
            }
        }
    }
    
    /* Room progs */
    scripts = json_object_get(root, "rprogs");
    if (scripts && json_is_array(scripts)) {
        json_array_foreach(scripts, script_index, script_json) {
            script = json_area_deserialize_script(script_json, area, IFC_R);
            if (script) {
                script->next = area->rprog_list;
                area->rprog_list = script;
            }
        }
    }
    
    /* Token progs */
    scripts = json_object_get(root, "tprogs");
    if (scripts && json_is_array(scripts)) {
        json_array_foreach(scripts, script_index, script_json) {
            script = json_area_deserialize_script(script_json, area, IFC_T);
            if (script) {
                script->next = area->tprog_list;
                area->tprog_list = script;
            }
        }
    }
    
    /* Area progs */
    scripts = json_object_get(root, "aprogs");
    if (scripts && json_is_array(scripts)) {
        json_array_foreach(scripts, script_index, script_json) {
            script = json_area_deserialize_script(script_json, area, IFC_A);
            if (script) {
                script->next = area->aprog_list;
                area->aprog_list = script;
            }
        }
    }
    
    /* Instance progs */
    scripts = json_object_get(root, "iprogs");
    if (scripts && json_is_array(scripts)) {
        json_array_foreach(scripts, script_index, script_json) {
            script = json_area_deserialize_script(script_json, area, IFC_I);
            if (script) {
                script->next = area->iprog_list;
                area->iprog_list = script;
            }
        }
    }
    
    /* Dungeon progs */
    scripts = json_object_get(root, "dprogs");
    if (scripts && json_is_array(scripts)) {
        json_array_foreach(scripts, script_index, script_json) {
            script = json_area_deserialize_script(script_json, area, IFC_D);
            if (script) {
                script->next = area->dprog_list;
                area->dprog_list = script;
            }
        }
    }
    
    /* Fix up exit destinations now that all rooms are loaded */
    /* This is needed because exits reference rooms by vnum that may not have been loaded yet */
    int hash_index, hash_count;
    if ((area->max_vnum - area->min_vnum) >= MAX_KEY_HASH) {
        hash_index = 0;
        hash_count = MAX_KEY_HASH;
    } else {
        hash_index = area->min_vnum % MAX_KEY_HASH;
        hash_count = area->max_vnum - area->min_vnum + 1;
    }
    /*
    for (int j = 0; j < hash_count; j++) {
        for (ROOM_INDEX_DATA *room = area->room_index_hash[hash_index]; room; room = room->next) {
            if (room->vnum && room->area == area) {
                for (int door = 0; door < MAX_DIR; door++) {
                    EXIT_DATA *pexit = room->exit[door];
                    if (pexit && pexit->u1.vnum > 0) {
                        pexit->u1.to_room = get_room_index(area, pexit->u1.vnum);
                        if (!pexit->u1.to_room) {
                            log_stringf("json_area_load: Room %ld exit %d: destination vnum %ld not found",
                                       room->vnum, door, pexit->u1.vnum);
                        }
                    }
                }
            }
        }
        
        if (++hash_index == MAX_KEY_HASH)
            hash_index = 0;
    }*/
    
    json_decref(root);
    
    log_stringf("json_area_load: Loaded area '%s' (UID %ld)", area->name, area->uid);
    
    return area;
}

bool json_area_save_to(AREA_DATA *area, const char *filename)
{
    char path[512];
    json_t *root;
    int ret;
    
    if (!area || !filename) {
        log_string("json_area_save_to: Invalid area or filename");
        return false;
    }
    
    /* Use provided path directly */
    strncpy(path, filename, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    
    /* Create root JSON object */
    root = json_object();
    json_object_set_new(root, "schema_version", json_string(JSON_AREA_SCHEMA_VERSION));
    
    /* Serialize area metadata */
    json_object_set_new(root, "area", json_area_serialize_metadata(area));
    
    /* Serialize rooms */
    json_t *rooms = json_array();
    int hash_index, hash_count;
    if ((area->max_vnum - area->min_vnum) >= MAX_KEY_HASH) {
        hash_index = 0;
        hash_count = MAX_KEY_HASH;
    } else {
        hash_index = area->min_vnum % MAX_KEY_HASH;
        hash_count = area->max_vnum - area->min_vnum + 1;
    }
    
    for (int j = 0; j < hash_count; j++) {
        for (ROOM_INDEX_DATA *room = area->room_index_hash[hash_index]; room; room = room->next) {
            if (room->vnum && room->area == area) {
                json_t *room_json = json_area_serialize_room(room);
                if (room_json) {
                    json_array_append_new(rooms, room_json);
                }
            }
        }
        
        if (++hash_index == MAX_KEY_HASH)
            hash_index = 0;
    }
    json_object_set_new(root, "rooms", rooms);
    
    /* Serialize mobiles */
    json_t *mobiles = json_array();
    if ((area->max_vnum - area->min_vnum) >= MAX_KEY_HASH) {
        hash_index = 0;
        hash_count = MAX_KEY_HASH;
    } else {
        hash_index = area->min_vnum % MAX_KEY_HASH;
        hash_count = area->max_vnum - area->min_vnum + 1;
    }
    
    for (int j = 0; j < hash_count; j++) {
        for (MOB_INDEX_DATA *mob = area->mob_index_hash[hash_index]; mob; mob = mob->next) {
            if (mob->vnum && mob->area == area) {
                json_t *mob_json = json_area_serialize_mobile(mob);
                if (mob_json) {
                    json_array_append_new(mobiles, mob_json);
                }
            }
        }
        
        if (++hash_index == MAX_KEY_HASH)
            hash_index = 0;
    }
    json_object_set_new(root, "mobiles", mobiles);
    
    /* Serialize objects */
    json_t *objects = json_array();
    if ((area->max_vnum - area->min_vnum) >= MAX_KEY_HASH) {
        hash_index = 0;
        hash_count = MAX_KEY_HASH;
    } else {
        hash_index = area->min_vnum % MAX_KEY_HASH;
        hash_count = area->max_vnum - area->min_vnum + 1;
    }
    
    for (int j = 0; j < hash_count; j++) {
        for (OBJ_INDEX_DATA *obj = area->obj_index_hash[hash_index]; obj; obj = obj->next) {
            if (obj->vnum && obj->area == area) {
                json_t *obj_json = json_area_serialize_object(obj);
                if (obj_json) {
                    json_array_append_new(objects, obj_json);
                }
            }
        }
        
        if (++hash_index == MAX_KEY_HASH)
            hash_index = 0;
    }
    json_object_set_new(root, "objects", objects);
    
    /* Serialize shops - shops are linked to mobiles via mob->pShop */
    json_t *shops = json_array();
    if ((area->max_vnum - area->min_vnum) >= MAX_KEY_HASH) {
        hash_index = 0;
        hash_count = MAX_KEY_HASH;
    } else {
        hash_index = area->min_vnum % MAX_KEY_HASH;
        hash_count = area->max_vnum - area->min_vnum + 1;
    }
    
    for (int j = 0; j < hash_count; j++) {
        for (MOB_INDEX_DATA *mob = area->mob_index_hash[hash_index]; mob; mob = mob->next) {
            if (mob->vnum && mob->area == area && mob->pShop) {
                json_t *shop_json = json_area_serialize_shop(mob->pShop, area);
                if (shop_json) {
                    json_array_append_new(shops, shop_json);
                }
            }
        }
        
        if (++hash_index == MAX_KEY_HASH)
            hash_index = 0;
    }
    if (json_array_size(shops) > 0) {
        json_object_set_new(root, "shops", shops);
    } else {
        json_decref(shops);
    }
    
    /* Serialize tokens */
    json_t *tokens = json_array();
    if ((area->max_vnum - area->min_vnum) >= MAX_KEY_HASH) {
        hash_index = 0;
        hash_count = MAX_KEY_HASH;
    } else {
        hash_index = area->min_vnum % MAX_KEY_HASH;
        hash_count = area->max_vnum - area->min_vnum + 1;
    }
    
    for (int j = 0; j < hash_count; j++) {
        for (TOKEN_INDEX_DATA *token = area->token_index_hash[hash_index]; token; token = token->next) {
            if (token->vnum && token->area == area) {
                json_t *token_json = json_area_serialize_token(token);
                if (token_json) {
                    json_array_append_new(tokens, token_json);
                }
            }
        }
        
        if (++hash_index == MAX_KEY_HASH)
            hash_index = 0;
    }
    if (json_array_size(tokens) > 0) {
        json_object_set_new(root, "tokens", tokens);
    } else {
        json_decref(tokens);
    }
    
    /* Write to file with pretty printing */
    ret = json_dump_file(root, path, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
    json_decref(root);
    
    if (ret != 0) {
        log_stringf("json_area_save_to: Failed to write file %s", path);
        return false;
    }
    
    log_stringf("json_area_save_to: Saved area '%s' (UID %ld) to %s", area->name, area->uid, path);
    
    return true;
}

bool json_area_save(AREA_DATA *area)
{
    char path[512];
    json_t *root;
    int ret;
    
    if (!area || !area->file_name) {
        log_string("json_area_save: Invalid area or filename");
        return false;
    }
    
    /* Build full path */
    snprintf(path, sizeof(path), "%s%s", AREA_DIR, area->file_name);
    
    /* Create root JSON object */
    root = json_object();
    json_object_set_new(root, "schema_version", json_string(JSON_AREA_SCHEMA_VERSION));
    
    /* Serialize area metadata */
    json_object_set_new(root, "area", json_area_serialize_metadata(area));
    
    /* Serialize rooms */
    json_t *rooms = json_array();
    int hash_index, hash_count;
    if ((area->max_vnum - area->min_vnum) >= MAX_KEY_HASH) {
        hash_index = 0;
        hash_count = MAX_KEY_HASH;
    } else {
        hash_index = area->min_vnum % MAX_KEY_HASH;
        hash_count = area->max_vnum - area->min_vnum + 1;
    }
    
    for (int j = 0; j < hash_count; j++) {
        for (ROOM_INDEX_DATA *room = area->room_index_hash[hash_index]; room; room = room->next) {
            if (room->vnum && room->area == area) {
                json_t *room_json = json_area_serialize_room(room);
                if (room_json) {
                    json_array_append_new(rooms, room_json);
                }
            }
        }
        
        if (++hash_index == MAX_KEY_HASH)
            hash_index = 0;
    }
    json_object_set_new(root, "rooms", rooms);
    
    /* Serialize mobiles */
    json_t *mobiles = json_array();
    hash_index = (area->max_vnum - area->min_vnum) >= MAX_KEY_HASH ? 0 : area->min_vnum % MAX_KEY_HASH;
    hash_count = (area->max_vnum - area->min_vnum) >= MAX_KEY_HASH ? MAX_KEY_HASH : area->max_vnum - area->min_vnum + 1;
    
    for (int j = 0; j < hash_count; j++) {
        for (MOB_INDEX_DATA *mob = area->mob_index_hash[hash_index]; mob; mob = mob->next) {
            if (mob->vnum && mob->area == area) {
                json_t *mob_json = json_area_serialize_mobile(mob);
                if (mob_json) {
                    json_array_append_new(mobiles, mob_json);
                }
            }
        }
        
        if (++hash_index == MAX_KEY_HASH)
            hash_index = 0;
    }
    json_object_set_new(root, "mobiles", mobiles);
    
    /* Serialize objects */
    json_t *objects = json_array();
    hash_index = (area->max_vnum - area->min_vnum) >= MAX_KEY_HASH ? 0 : area->min_vnum % MAX_KEY_HASH;
    hash_count = (area->max_vnum - area->min_vnum) >= MAX_KEY_HASH ? MAX_KEY_HASH : area->max_vnum - area->min_vnum + 1;
    
    for (int j = 0; j < hash_count; j++) {
        for (OBJ_INDEX_DATA *obj = area->obj_index_hash[hash_index]; obj; obj = obj->next) {
            if (obj->vnum && obj->area == area) {
                json_t *obj_json = json_area_serialize_object(obj);
                if (obj_json) {
                    json_array_append_new(objects, obj_json);
                }
            }
        }
        
        if (++hash_index == MAX_KEY_HASH)
            hash_index = 0;
    }
    json_object_set_new(root, "objects", objects);
    
    /* Serialize tokens */
    json_t *tokens = json_array();
    hash_index = (area->max_vnum - area->min_vnum) >= MAX_KEY_HASH ? 0 : area->min_vnum % MAX_KEY_HASH;
    hash_count = (area->max_vnum - area->min_vnum) >= MAX_KEY_HASH ? MAX_KEY_HASH : area->max_vnum - area->min_vnum + 1;
    
    for (int j = 0; j < hash_count; j++) {
        for (TOKEN_INDEX_DATA *token = area->token_index_hash[hash_index]; token; token = token->next) {
            if (token->vnum && token->area == area) {
                json_t *token_json = json_area_serialize_token(token);
                if (token_json) {
                    json_array_append_new(tokens, token_json);
                }
            }
        }
        
        if (++hash_index == MAX_KEY_HASH)
            hash_index = 0;
    }
    json_object_set_new(root, "tokens", tokens);
    
    /* Serialize scripts */
    /* Save all 7 types of scripts: mobprogs, oprogs, rprogs, tprogs, aprogs, iprogs, dprogs */
    json_t *mobprogs = json_array();
    json_t *oprogs = json_array();
    json_t *rprogs = json_array();
    json_t *tprogs = json_array();
    json_t *aprogs = json_array();
    json_t *iprogs = json_array();
    json_t *dprogs = json_array();
    
    /* Iterate through all possible vnums in area range */
    for (long vnum = area->min_vnum; vnum <= area->max_vnum; vnum++) {
        SCRIPT_DATA *script;
        json_t *script_json;
        
        if ((script = get_script_index(area, vnum, PRG_MPROG))) {
            script_json = json_area_serialize_script(script);
            if (script_json) json_array_append_new(mobprogs, script_json);
        }
        
        if ((script = get_script_index(area, vnum, PRG_OPROG))) {
            script_json = json_area_serialize_script(script);
            if (script_json) json_array_append_new(oprogs, script_json);
        }
        
        if ((script = get_script_index(area, vnum, PRG_RPROG))) {
            script_json = json_area_serialize_script(script);
            if (script_json) json_array_append_new(rprogs, script_json);
        }
        
        if ((script = get_script_index(area, vnum, PRG_TPROG))) {
            script_json = json_area_serialize_script(script);
            if (script_json) json_array_append_new(tprogs, script_json);
        }
        
        if ((script = get_script_index(area, vnum, PRG_APROG))) {
            script_json = json_area_serialize_script(script);
            if (script_json) json_array_append_new(aprogs, script_json);
        }
        
        if ((script = get_script_index(area, vnum, PRG_IPROG))) {
            script_json = json_area_serialize_script(script);
            if (script_json) json_array_append_new(iprogs, script_json);
        }
        
        if ((script = get_script_index(area, vnum, PRG_DPROG))) {
            script_json = json_area_serialize_script(script);
            if (script_json) json_array_append_new(dprogs, script_json);
        }
    }
    
    /* Only save script sections if they have content */
    if (json_array_size(mobprogs) > 0)
        json_object_set_new(root, "mobprogs", mobprogs);
    else
        json_decref(mobprogs);
        
    if (json_array_size(oprogs) > 0)
        json_object_set_new(root, "oprogs", oprogs);
    else
        json_decref(oprogs);
        
    if (json_array_size(rprogs) > 0)
        json_object_set_new(root, "rprogs", rprogs);
    else
        json_decref(rprogs);
        
    if (json_array_size(tprogs) > 0)
        json_object_set_new(root, "tprogs", tprogs);
    else
        json_decref(tprogs);
        
    if (json_array_size(aprogs) > 0)
        json_object_set_new(root, "aprogs", aprogs);
    else
        json_decref(aprogs);
        
    if (json_array_size(iprogs) > 0)
        json_object_set_new(root, "iprogs", iprogs);
    else
        json_decref(iprogs);
        
    if (json_array_size(dprogs) > 0)
        json_object_set_new(root, "dprogs", dprogs);
    else
        json_decref(dprogs);
    
    /* Cache in Redis immediately (if available) */
    if (redis_is_available() && area->file_name && area->file_name[0]) {
        char *json_str = json_dumps(root, JSON_COMPACT);
        if (json_str) {
            if (redis_cache_area_state(area->file_name, json_str)) {
                log_stringf("json_area_save: Cached area '%s' in Redis", area->name);
            }
            free(json_str);
        }
    }

    /* Write to file with pretty printing */
    ret = json_dump_file(root, path, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
    json_decref(root);

    if (ret != 0) {
        log_stringf("json_area_save: Failed to write %s", path);
        return false;
    }

    log_stringf("json_area_save: Saved area '%s' to %s", area->name, path);

    return true;
}

/*
 * Serialize area to compact JSON string for Redis caching.
 * Returns: allocated string (caller must free), NULL on failure.
 */
char *json_area_serialize_to_string(AREA_DATA *area)
{
    json_t *root;
    char *result;

    if (!area) {
        return NULL;
    }

    /* Create root JSON object */
    root = json_object();
    if (!root) return NULL;

    json_object_set_new(root, "schema_version", json_string(JSON_AREA_SCHEMA_VERSION));

    /* Serialize area metadata */
    json_object_set_new(root, "area", json_area_serialize_metadata(area));

    /* Serialize rooms */
    json_t *rooms = json_array();
    int hash_index = (area->max_vnum - area->min_vnum) >= MAX_KEY_HASH ? 0 : area->min_vnum % MAX_KEY_HASH;
    int hash_count = (area->max_vnum - area->min_vnum) >= MAX_KEY_HASH ? MAX_KEY_HASH : area->max_vnum - area->min_vnum + 1;

    for (int j = 0; j < hash_count; j++) {
        for (ROOM_INDEX_DATA *room = area->room_index_hash[hash_index]; room; room = room->next) {
            if (room->vnum && room->area == area) {
                json_t *room_json = json_area_serialize_room(room);
                if (room_json) json_array_append_new(rooms, room_json);
            }
        }
        if (++hash_index == MAX_KEY_HASH) hash_index = 0;
    }
    json_object_set_new(root, "rooms", rooms);

    /* Serialize mobiles */
    json_t *mobiles = json_array();
    hash_index = (area->max_vnum - area->min_vnum) >= MAX_KEY_HASH ? 0 : area->min_vnum % MAX_KEY_HASH;
    for (int j = 0; j < hash_count; j++) {
        for (MOB_INDEX_DATA *mob = area->mob_index_hash[hash_index]; mob; mob = mob->next) {
            if (mob->vnum && mob->area == area) {
                json_t *mob_json = json_area_serialize_mobile(mob);
                if (mob_json) json_array_append_new(mobiles, mob_json);
            }
        }
        if (++hash_index == MAX_KEY_HASH) hash_index = 0;
    }
    json_object_set_new(root, "mobiles", mobiles);

    /* Serialize objects */
    json_t *objects = json_array();
    hash_index = (area->max_vnum - area->min_vnum) >= MAX_KEY_HASH ? 0 : area->min_vnum % MAX_KEY_HASH;
    for (int j = 0; j < hash_count; j++) {
        for (OBJ_INDEX_DATA *obj = area->obj_index_hash[hash_index]; obj; obj = obj->next) {
            if (obj->vnum && obj->area == area) {
                json_t *obj_json = json_area_serialize_object(obj);
                if (obj_json) json_array_append_new(objects, obj_json);
            }
        }
        if (++hash_index == MAX_KEY_HASH) hash_index = 0;
    }
    json_object_set_new(root, "objects", objects);

    /* Serialize tokens */
    json_t *tokens = json_array();
    hash_index = (area->max_vnum - area->min_vnum) >= MAX_KEY_HASH ? 0 : area->min_vnum % MAX_KEY_HASH;
    for (int j = 0; j < hash_count; j++) {
        for (TOKEN_INDEX_DATA *token = area->token_index_hash[hash_index]; token; token = token->next) {
            if (token->vnum && token->area == area) {
                json_t *token_json = json_area_serialize_token(token);
                if (token_json) json_array_append_new(tokens, token_json);
            }
        }
        if (++hash_index == MAX_KEY_HASH) hash_index = 0;
    }
    if (json_array_size(tokens) > 0) {
        json_object_set_new(root, "tokens", tokens);
    } else {
        json_decref(tokens);
    }

    /* Convert to compact JSON string */
    result = json_dumps(root, JSON_COMPACT);
    json_decref(root);

    return result;
}

/***************************************************************************
 * Placeholder stubs for additional serialization functions                *
 * These will be fully implemented as we continue development              *
 ***************************************************************************/

json_t *json_area_serialize_room(ROOM_INDEX_DATA *room)
{
    if (!room) return NULL;
    
    json_t *json = json_object();
    if (!json) return NULL;
    
    // Basic info
    json_object_set_new(json, "vnum", json_integer(room->vnum));
    json_object_set_new(json, "name", json_string(room->name));
    json_object_set_new(json, "description", json_string(room->description));
    
    if (room->persist)
        json_object_set_new(json, "persist", json_true());
    
    if (room->comments && room->comments[0] != '\0')
        json_object_set_new(json, "comments", json_string(room->comments));
    
    if (room->owner && room->owner[0] != '\0')
        json_object_set_new(json, "owner", json_string(room->owner));
    
    if (room->home_owner && room->home_owner[0] != '\0')
        json_object_set_new(json, "home_owner", json_string(room->home_owner));
    
    // Flags and sector
    json_object_set_new(json, "flags", flags_to_json_array(room->rs_room_flag[0], room_flags));
    json_object_set_new(json, "flags2", flags_to_json_array(room->rs_room_flag[1], room2_flags));
    json_object_set_new(json, "sector", json_integer(room->rs_sector_type));
    
    // Rates
    if (room->rs_heal_rate != 100)
        json_object_set_new(json, "heal_rate", json_integer(room->rs_heal_rate));
    if (room->rs_mana_rate != 100)
        json_object_set_new(json, "mana_rate", json_integer(room->rs_mana_rate));
    if (room->rs_move_rate != 100)
        json_object_set_new(json, "move_rate", json_integer(room->rs_move_rate));
    
    // Recall location
    if (rs_location_isset(&room->rs_recall)) {
        json_t *recall = json_object();
        if (room->rs_recall.wuid > 0) {
            json_object_set_new(recall, "wilds_uid", json_integer(room->rs_recall.wuid));
            json_object_set_new(recall, "x", json_integer(room->rs_recall.id[0]));
            json_object_set_new(recall, "y", json_integer(room->rs_recall.id[1]));
            json_object_set_new(recall, "z", json_integer(room->rs_recall.id[2]));
        } else {
            json_object_set_new(recall, "vnum", json_integer(room->rs_recall.id[0]));
        }
        json_object_set_new(json, "recall", recall);
    }
    
    // Wilderness/Blueprint coordinates
    if (room->viewwilds && room->w > 0) {
        json_t *wilds = json_object();
        json_object_set_new(wilds, "uid", json_integer(room->w));
        json_object_set_new(wilds, "x", json_integer(room->x));
        json_object_set_new(wilds, "y", json_integer(room->y));
        json_object_set_new(wilds, "z", json_integer(room->z));
        json_object_set_new(json, "wilds", wilds);
    } else if (IS_SET(room->room_flag[1], ROOM_BLUEPRINT)) {
        json_t *blueprint = json_object();
        json_object_set_new(blueprint, "x", json_integer(room->x));
        json_object_set_new(blueprint, "y", json_integer(room->y));
        json_object_set_new(blueprint, "z", json_integer(room->z));
        json_object_set_new(json, "blueprint", blueprint);
    }
    
    // Extra descriptions
    json_t *extra_descrs = json_array();
    for (EXTRA_DESCR_DATA *ed = room->extra_descr; ed; ed = ed->next) {
        json_t *ed_json = json_object();
        json_object_set_new(ed_json, "keyword", json_string(ed->keyword));
        if (ed->description)
            json_object_set_new(ed_json, "description", json_string(ed->description));
        else
            json_object_set_new(ed_json, "environmental", json_true());
        json_array_append_new(extra_descrs, ed_json);
    }
    if (json_array_size(extra_descrs) > 0)
        json_object_set_new(json, "extra_descrs", extra_descrs);
    else
        json_decref(extra_descrs);
    
    // Conditional descriptions
    json_t *cond_descrs = json_array();
    for (CONDITIONAL_DESCR_DATA *cd = room->conditional_descr; cd; cd = cd->next) {
        json_t *cd_json = json_object();
        json_object_set_new(cd_json, "condition", json_integer(cd->condition));
        json_object_set_new(cd_json, "phrase", json_integer(cd->phrase));
        json_object_set_new(cd_json, "description", json_string(cd->description));
        json_array_append_new(cond_descrs, cd_json);
    }
    if (json_array_size(cond_descrs) > 0)
        json_object_set_new(json, "conditional_descrs", cond_descrs);
    else
        json_decref(cond_descrs);
    
    // Exits
    json_t *exits = json_array();
    for (int door = 0; door < MAX_DIR; door++) {
        EXIT_DATA *ex = room->exit[door];
        if (ex && !IS_SET(ex->rs_flags, EX_VLINK)) {
            json_t *exit_json = json_area_serialize_exit(ex);
            if (exit_json)
                json_array_append_new(exits, exit_json);
        }
    }
    if (json_array_size(exits) > 0)
        json_object_set_new(json, "exits", exits);
    else
        json_decref(exits);
    
    // Resets
    json_t *resets = json_array();
    for (RESET_DATA *reset = room->reset_first; reset; reset = reset->next) {
        json_t *reset_json = json_area_serialize_reset(reset);
        if (reset_json)
            json_array_append_new(resets, reset_json);
    }
    if (json_array_size(resets) > 0)
        json_object_set_new(json, "resets", resets);
    else
        json_decref(resets);
    
    // Progs
    if (room->progs) {
        json_t *progs = json_area_serialize_progs(room->progs->progs, room->area);
        if (progs)
            json_object_set_new(json, "progs", progs);
    }
    
    // Index vars
    if (room->index_vars) {
        json_t *index_vars = json_area_serialize_index_vars(room->index_vars, room->area);
        if (index_vars)
            json_object_set_new(json, "index_vars", index_vars);
    }
    
    return json;
}

ROOM_INDEX_DATA *json_area_deserialize_room(json_t *json, AREA_DATA *area)
{
    if (!json || !area) return NULL;
    
    ROOM_INDEX_DATA *room = new_room_index();
    if (!room) return NULL;
    
    room->area = area;
    
    // Basic info
    room->vnum = json_get_int_default(json, "vnum", 0);
    room->name = str_dup(json_get_string_default(json, "name", "Unnamed Room"));
    room->description = str_dup(json_get_string_default(json, "description", ""));
    room->persist = json_get_bool_default(json, "persist", false);
    
    const char *comments = json_get_string_default(json, "comments", "");
    if (comments && comments[0] != '\0')
        room->comments = str_dup(comments);
    
    const char *owner = json_get_string_default(json, "owner", "");
    if (owner && owner[0] != '\0')
        room->owner = str_dup(owner);
    
    const char *home_owner = json_get_string_default(json, "home_owner", "");
    if (home_owner && home_owner[0] != '\0')
        room->home_owner = str_dup(home_owner);
    
    // Flags and sector
    json_t *flags = json_object_get(json, "flags");
    if (flags) room->rs_room_flag[0] = json_array_to_flags(flags, room_flags);
    
    json_t *flags2 = json_object_get(json, "flags2");
    if (flags2) room->rs_room_flag[1] = json_array_to_flags(flags2, room2_flags);
    
    room->rs_sector_type = json_get_int_default(json, "sector", 0);
    
    // Rates
    room->rs_heal_rate = json_get_int_default(json, "heal_rate", 100);
    room->rs_mana_rate = json_get_int_default(json, "mana_rate", 100);
    room->rs_move_rate = json_get_int_default(json, "move_rate", 100);
    
    // Recall location
    json_t *recall = json_object_get(json, "recall");
    if (recall) {
        if (json_object_get(recall, "wilds_uid")) {
            room->rs_recall.wuid = json_get_int_default(recall, "wilds_uid", 0);
            room->rs_recall.id[0] = json_get_int_default(recall, "x", 0);
            room->rs_recall.id[1] = json_get_int_default(recall, "y", 0);
            room->rs_recall.id[2] = json_get_int_default(recall, "z", 0);
        } else {
            room->rs_recall.wuid = 0;
            room->rs_recall.id[0] = json_get_int_default(recall, "vnum", 0);
            room->rs_recall.id[1] = 0;
            room->rs_recall.id[2] = 0;
        }
    }
    
    // Wilderness coordinates
    json_t *wilds = json_object_get(json, "wilds");
    if (wilds) {
        room->w = json_get_int_default(wilds, "uid", 0);
        room->x = json_get_int_default(wilds, "x", 0);
        room->y = json_get_int_default(wilds, "y", 0);
        room->z = json_get_int_default(wilds, "z", 0);
    }
    
    // Blueprint coordinates
    json_t *blueprint = json_object_get(json, "blueprint");
    if (blueprint) {
        room->x = json_get_int_default(blueprint, "x", 0);
        room->y = json_get_int_default(blueprint, "y", 0);
        room->z = json_get_int_default(blueprint, "z", 0);
    }
    
    // Extra descriptions
    json_t *extra_descrs = json_object_get(json, "extra_descrs");
    if (extra_descrs && json_is_array(extra_descrs)) {
        size_t index;
        json_t *ed_json;
        json_array_foreach(extra_descrs, index, ed_json) {
            EXTRA_DESCR_DATA *ed = alloc_perm(sizeof(*ed));
            ed->keyword = str_dup(json_get_string_default(ed_json, "keyword", ""));
            
            if (json_get_bool_default(ed_json, "environmental", false)) {
                ed->description = NULL;
            } else {
                ed->description = str_dup(json_get_string_default(ed_json, "description", ""));
            }
            
            ed->next = room->extra_descr;
            room->extra_descr = ed;
        }
    }
    
    // Conditional descriptions
    json_t *cond_descrs = json_object_get(json, "conditional_descrs");
    if (cond_descrs && json_is_array(cond_descrs)) {
        size_t index;
        json_t *cd_json;
        json_array_foreach(cond_descrs, index, cd_json) {
            CONDITIONAL_DESCR_DATA *cd = alloc_perm(sizeof(*cd));
            cd->condition = json_get_int_default(cd_json, "condition", 0);
            cd->phrase = json_get_int_default(cd_json, "phrase", 0);
            cd->description = str_dup(json_get_string_default(cd_json, "description", ""));
            
            cd->next = room->conditional_descr;
            room->conditional_descr = cd;
        }
    }
    
    // Exits - will be linked later in fix_exits()
    json_t *exits = json_object_get(json, "exits");
    if (exits && json_is_array(exits)) {
        size_t index;
        json_t *exit_json;
        json_array_foreach(exits, index, exit_json) {
            EXIT_DATA *ex = json_area_deserialize_exit(exit_json, area);
            if (ex && ex->orig_door >= 0 && ex->orig_door < MAX_DIR) {
                ex->from_room = room;
                room->exit[ex->orig_door] = ex;
            }
        }
    }
    
    /* Resets - DEFERRED until after mobs/objects are loaded */
    /* Resets are now processed in json_area_load() after all entities exist */
    /* This prevents "bad vnum" errors when resets reference not-yet-loaded objects */
    
    // Progs
    if (!room->progs)
        room->progs = new_prog_data();
    room->progs->progs = json_area_deserialize_progs(json_object_get(json, "progs"), area, PRG_RPROG);
    
    // Index vars
    room->index_vars = json_area_deserialize_index_vars(json_object_get(json, "index_vars"), area);
    
    // Add to area hash table for lookups
    int iHash = room->vnum % MAX_KEY_HASH;
    room->next = area->room_index_hash[iHash];
    area->room_index_hash[iHash] = room;
    
    // Add to area room list
    list_appendlink(area->room_list, room);
    
    return room;
}

json_t *json_area_serialize_mobile(MOB_INDEX_DATA *mob)
{
    if (!mob) return NULL;
    
    json_t *json = json_object();
    if (!json) return NULL;
    
    // Basic info
    json_object_set_new(json, "vnum", json_integer(mob->vnum));
    json_object_set_new(json, "name", json_string(mob->player_name));
    json_object_set_new(json, "short_descr", json_string(mob->short_descr));
    json_object_set_new(json, "long_descr", json_string(mob->long_descr));
    json_object_set_new(json, "description", json_string(mob->description));
    
    if (mob->owner && mob->owner[0] != '\0')
        json_object_set_new(json, "owner", json_string(mob->owner));
    
    if (mob->sig && mob->sig[0] != '\0')
        json_object_set_new(json, "imp_sig", json_string(mob->sig));
    
    if (mob->creator_sig && mob->creator_sig[0] != '\0')
        json_object_set_new(json, "creator_sig", json_string(mob->creator_sig));
    
    if (mob->persist)
        json_object_set_new(json, "persist", json_true());
    
    if (mob->skeywds && mob->skeywds[0] != '\0')
        json_object_set_new(json, "script_keywords", json_string(mob->skeywds));
    
    if (mob->comments && mob->comments[0] != '\0')
        json_object_set_new(json, "comments", json_string(mob->comments));
    
    // Race
    if (mob->race)
        json_object_set_new(json, "race", json_string(mob->race->name));
    else
        json_object_set_new(json, "race", json_string("unique"));
    
    // Flags
    if (mob->act[0] != 0)
        json_object_set_new(json, "act_flags", flags_to_json_array(mob->act[0], act_flags));
    if (mob->act[1] != 0)
        json_object_set_new(json, "act2_flags", flags_to_json_array(mob->act[1], act2_flags));
    if (mob->affected_by[0] != 0)
        json_object_set_new(json, "affected_by", flags_to_json_array(mob->affected_by[0], affect_flags));
    if (mob->affected_by[1] != 0)
        json_object_set_new(json, "affected_by2", flags_to_json_array(mob->affected_by[1], affect2_flags));
    
    // Stats
    json_object_set_new(json, "level", json_integer(mob->level));
    json_object_set_new(json, "alignment", json_integer(mob->alignment));
    json_object_set_new(json, "hitroll", json_integer(mob->hitroll));
    
    // Dice
    json_t *hit_dice = json_object();
    json_object_set_new(hit_dice, "number", json_integer(mob->hit.number));
    json_object_set_new(hit_dice, "size", json_integer(mob->hit.size));
    json_object_set_new(hit_dice, "bonus", json_integer(mob->hit.bonus));
    json_object_set_new(json, "hit_dice", hit_dice);
    
    json_t *mana_dice = json_object();
    json_object_set_new(mana_dice, "number", json_integer(mob->mana.number));
    json_object_set_new(mana_dice, "size", json_integer(mob->mana.size));
    json_object_set_new(mana_dice, "bonus", json_integer(mob->mana.bonus));
    json_object_set_new(json, "mana_dice", mana_dice);
    
    json_t *damage_dice = json_object();
    json_object_set_new(damage_dice, "number", json_integer(mob->damage.number));
    json_object_set_new(damage_dice, "size", json_integer(mob->damage.size));
    json_object_set_new(damage_dice, "bonus", json_integer(mob->damage.bonus));
    json_object_set_new(json, "damage_dice", damage_dice);
    
    // Combat
    json_object_set_new(json, "dam_type", json_integer(mob->dam_type));
    json_object_set_new(json, "attacks", json_integer(mob->attacks));
    
    if (mob->off_flags != 0)
        json_object_set_new(json, "off_flags", flags_to_json_array(mob->off_flags, off_flags));
    if (mob->imm_flags != 0)
        json_object_set_new(json, "imm_flags", flags_to_json_array(mob->imm_flags, imm_flags));
    if (mob->res_flags != 0)
        json_object_set_new(json, "res_flags", flags_to_json_array(mob->res_flags, res_flags));
    if (mob->vuln_flags != 0)
        json_object_set_new(json, "vuln_flags", flags_to_json_array(mob->vuln_flags, vuln_flags));
    
    // Position and status
    json_object_set_new(json, "start_pos", json_integer(mob->start_pos));
    json_object_set_new(json, "default_pos", json_integer(mob->default_pos));
    json_object_set_new(json, "wealth", json_integer(mob->wealth));
    json_object_set_new(json, "body_type", json_integer(mob->body_type));
    json_object_set_new(json, "parts", json_integer(mob->parts));
    json_object_set_new(json, "size", json_integer(mob->size));
    json_object_set_new(json, "movement", json_integer(mob->move));
    
    if (mob->material && mob->material[0] != '\0')
        json_object_set_new(json, "material", json_string(mob->material));
    
    // Corpse info
    if (mob->corpse_type)
        json_object_set_new(json, "corpse_type", json_integer(mob->corpse_type));
    if (mob->corpse)
        json_object_set_new(json, "corpse_vnum", json_integer(mob->corpse));
    if (mob->zombie)
        json_object_set_new(json, "zombie_vnum", json_integer(mob->zombie));
    
    if (mob->boss)
        json_object_set_new(json, "boss", json_true());
    
    // Pronouns
    if (mob->pronoun_he_she && mob->pronoun_he_she[0] != '\0')
        json_object_set_new(json, "pronoun_he_she", json_string(mob->pronoun_he_she));
    if (mob->pronoun_him_her && mob->pronoun_him_her[0] != '\0')
        json_object_set_new(json, "pronoun_him_her", json_string(mob->pronoun_him_her));
    if (mob->pronoun_his_her && mob->pronoun_his_her[0] != '\0')
        json_object_set_new(json, "pronoun_his_her", json_string(mob->pronoun_his_her));
    if (mob->pronoun_his_hers && mob->pronoun_his_hers[0] != '\0')
        json_object_set_new(json, "pronoun_his_hers", json_string(mob->pronoun_his_hers));
    if (mob->pronoun_himself_herself && mob->pronoun_himself_herself[0] != '\0')
        json_object_set_new(json, "pronoun_himself_herself", json_string(mob->pronoun_himself_herself));
    
    json_object_set_new(json, "verb_preference", json_integer(mob->verb_preference));
    
    // Spec fun
    if (mob->spec_fun)
        json_object_set_new(json, "spec_fun", json_string(spec_name(mob->spec_fun)));
    
    // Progs
    json_t *progs = json_area_serialize_progs(mob->progs, mob->area);
    if (progs)
        json_object_set_new(json, "progs", progs);
    
    // Shop
    if (mob->pShop) {
        json_t *shop = json_area_serialize_shop(mob->pShop, mob->area);
        if (shop)
            json_object_set_new(json, "shop", shop);
    }
    
    // Index vars
    if (mob->index_vars) {
        json_t *index_vars = json_area_serialize_index_vars(mob->index_vars, mob->area);
        if (index_vars)
            json_object_set_new(json, "index_vars", index_vars);
    }
    
    // TODO: Questor, Crew
    
    return json;
}

MOB_INDEX_DATA *json_area_deserialize_mobile(json_t *json, AREA_DATA *area)
{
    if (!json || !area) return NULL;
    
    MOB_INDEX_DATA *mob = new_mob_index();
    if (!mob) return NULL;
    
    mob->area = area;
    
    // Basic info
    mob->vnum = json_get_int_default(json, "vnum", 0);
    mob->player_name = str_dup(json_get_string_default(json, "name", "mobile"));
    mob->short_descr = str_dup(json_get_string_default(json, "short_descr", "a mobile"));
    mob->long_descr = str_dup(json_get_string_default(json, "long_descr", "A mobile is here."));
    mob->description = str_dup(json_get_string_default(json, "description", ""));
    mob->persist = json_get_bool_default(json, "persist", false);
    
    const char *owner = json_get_string_default(json, "owner", "");
    if (owner && owner[0] != '\0')
        mob->owner = str_dup(owner);
    
    const char *sig = json_get_string_default(json, "imp_sig", "");
    if (sig && sig[0] != '\0')
        mob->sig = str_dup(sig);
    
    const char *creator = json_get_string_default(json, "creator_sig", "");
    if (creator && creator[0] != '\0')
        mob->creator_sig = str_dup(creator);
    
    const char *skeywds = json_get_string_default(json, "script_keywords", "");
    if (skeywds && skeywds[0] != '\0')
        mob->skeywds = str_dup(skeywds);
    
    const char *comments = json_get_string_default(json, "comments", "");
    if (comments && comments[0] != '\0')
        mob->comments = str_dup(comments);
    
    // Race
    const char *race_name = json_get_string_default(json, "race", "unique");
    mob->race = race_lookup(race_name);
    
    // Flags
    json_t *act_flags_json = json_object_get(json, "act_flags");
    if (act_flags_json) mob->act[0] = json_array_to_flags(act_flags_json, act_flags);
    
    json_t *act2_flags_json = json_object_get(json, "act2_flags");
    if (act2_flags_json) mob->act[1] = json_array_to_flags(act2_flags_json, act2_flags);
    
    json_t *aff_flags = json_object_get(json, "affected_by");
    if (aff_flags) mob->affected_by[0] = json_array_to_flags(aff_flags, affect_flags);
    
    json_t *aff2_flags = json_object_get(json, "affected_by2");
    if (aff2_flags) mob->affected_by[1] = json_array_to_flags(aff2_flags, affect2_flags);
    
    // Stats
    mob->level = json_get_int_default(json, "level", 1);
    mob->alignment = json_get_int_default(json, "alignment", 0);
    mob->hitroll = json_get_int_default(json, "hitroll", 0);
    
    // Dice
    json_t *hit_dice = json_object_get(json, "hit_dice");
    if (hit_dice) {
        mob->hit.number = json_get_int_default(hit_dice, "number", 1);
        mob->hit.size = json_get_int_default(hit_dice, "size", 8);
        mob->hit.bonus = json_get_int_default(hit_dice, "bonus", 0);
    }
    
    json_t *mana_dice = json_object_get(json, "mana_dice");
    if (mana_dice) {
        mob->mana.number = json_get_int_default(mana_dice, "number", 1);
        mob->mana.size = json_get_int_default(mana_dice, "size", 8);
        mob->mana.bonus = json_get_int_default(mana_dice, "bonus", 0);
    }
    
    json_t *damage_dice = json_object_get(json, "damage_dice");
    if (damage_dice) {
        mob->damage.number = json_get_int_default(damage_dice, "number", 1);
        mob->damage.size = json_get_int_default(damage_dice, "size", 4);
        mob->damage.bonus = json_get_int_default(damage_dice, "bonus", 0);
    }
    
    // Combat
    mob->dam_type = json_get_int_default(json, "dam_type", 0);
    mob->attacks = json_get_int_default(json, "attacks", 0);
    
    json_t *off_flags_json = json_object_get(json, "off_flags");
    if (off_flags_json) mob->off_flags = json_array_to_flags(off_flags_json, off_flags);
    
    json_t *imm_flags_json = json_object_get(json, "imm_flags");
    if (imm_flags_json) mob->imm_flags = json_array_to_flags(imm_flags_json, imm_flags);
    
    json_t *res_flags_json = json_object_get(json, "res_flags");
    if (res_flags_json) mob->res_flags = json_array_to_flags(res_flags_json, res_flags);
    
    json_t *vuln_flags_json = json_object_get(json, "vuln_flags");
    if (vuln_flags_json) mob->vuln_flags = json_array_to_flags(vuln_flags_json, vuln_flags);
    
    // Position and status
    mob->start_pos = json_get_int_default(json, "start_pos", POS_STANDING);
    mob->default_pos = json_get_int_default(json, "default_pos", POS_STANDING);
    mob->wealth = json_get_int_default(json, "wealth", 0);
    mob->body_type = json_get_int_default(json, "body_type", 0);
    mob->parts = json_get_int_default(json, "parts", 0);
    mob->size = json_get_int_default(json, "size", SIZE_MEDIUM);
    mob->move = json_get_int_default(json, "movement", 0);
    
    const char *material = json_get_string_default(json, "material", "");
    if (material && material[0] != '\0')
        mob->material = str_dup(material);
    
    // Corpse info
    mob->corpse_type = json_get_int_default(json, "corpse_type", 0);
    mob->corpse = json_get_int_default(json, "corpse_vnum", 0);
    mob->zombie = json_get_int_default(json, "zombie_vnum", 0);
    mob->boss = json_get_bool_default(json, "boss", false);
    
    // Pronouns
    const char *he_she = json_get_string_default(json, "pronoun_he_she", "");
    if (he_she && he_she[0] != '\0')
        mob->pronoun_he_she = str_dup(he_she);
    
    const char *him_her = json_get_string_default(json, "pronoun_him_her", "");
    if (him_her && him_her[0] != '\0')
        mob->pronoun_him_her = str_dup(him_her);
    
    const char *his_her = json_get_string_default(json, "pronoun_his_her", "");
    if (his_her && his_her[0] != '\0')
        mob->pronoun_his_her = str_dup(his_her);
    
    const char *his_hers = json_get_string_default(json, "pronoun_his_hers", "");
    if (his_hers && his_hers[0] != '\0')
        mob->pronoun_his_hers = str_dup(his_hers);
    
    const char *himself_herself = json_get_string_default(json, "pronoun_himself_herself", "");
    if (himself_herself && himself_herself[0] != '\0')
        mob->pronoun_himself_herself = str_dup(himself_herself);
    
    mob->verb_preference = json_get_int_default(json, "verb_preference", 0);
    
    // Spec function
    const char *spec_name_str = json_get_string_default(json, "spec_fun", "");
    if (spec_name_str && spec_name_str[0] != '\0')
        mob->spec_fun = spec_lookup(spec_name_str);
    
    // Progs
    mob->progs = json_area_deserialize_progs(json_object_get(json, "progs"), area, PRG_MPROG);
    
    // Shop
    json_t *shop_json = json_object_get(json, "shop");
    if (shop_json) {
        mob->pShop = json_area_deserialize_shop(shop_json, area);
    }
    
    // Index vars
    mob->index_vars = json_area_deserialize_index_vars(json_object_get(json, "index_vars"), area);
    
    // Add to area hash table for lookups
    int iHash = mob->vnum % MAX_KEY_HASH;
    mob->next = area->mob_index_hash[iHash];
    area->mob_index_hash[iHash] = mob;
    
    return mob;
}

json_t *json_area_serialize_object(OBJ_INDEX_DATA *obj)
{
    if (!obj) return NULL;
    
    json_t *json = json_object();
    if (!json) return NULL;
    
    // Basic info
    json_object_set_new(json, "vnum", json_integer(obj->vnum));
    json_object_set_new(json, "name", json_string(obj->name));
    json_object_set_new(json, "short_descr", json_string(obj->short_descr));
    json_object_set_new(json, "long_descr", json_string(obj->description));
    json_object_set_new(json, "description", json_string(obj->full_description));
    
    if (obj->material && obj->material[0] != '\0')
        json_object_set_new(json, "material", json_string(obj->material));
    
    if (obj->imp_sig && obj->imp_sig[0] != '\0')
        json_object_set_new(json, "imp_sig", json_string(obj->imp_sig));
    
    if (obj->persist)
        json_object_set_new(json, "persist", json_true());
    
    if (obj->creator_sig && obj->creator_sig[0] != '\0')
        json_object_set_new(json, "creator_sig", json_string(obj->creator_sig));
    
    if (obj->skeywds && obj->skeywds[0] != '\0')
        json_object_set_new(json, "script_keywords", json_string(obj->skeywds));
    
    if (obj->comments && obj->comments[0] != '\0')
        json_object_set_new(json, "comments", json_string(obj->comments));
    
    // Stats
    json_object_set_new(json, "times_allowed_fixed", json_integer(obj->times_allowed_fixed));
    json_object_set_new(json, "fragility", json_integer(obj->fragility));
    json_object_set_new(json, "points", json_integer(obj->points));
    json_object_set_new(json, "update", json_integer(obj->update));
    json_object_set_new(json, "timer", json_integer(obj->timer));
    
    // Type and flags
    json_object_set_new(json, "item_type", json_string(item_name(obj->item_type)));
    json_object_set_new(json, "extra_flags", flags_to_json_array(obj->extra[0], extra_flags));
    json_object_set_new(json, "extra2_flags", flags_to_json_array(obj->extra[1], extra2_flags));
    json_object_set_new(json, "extra3_flags", flags_to_json_array(obj->extra[2], extra3_flags));
    json_object_set_new(json, "extra4_flags", flags_to_json_array(obj->extra[3], extra4_flags));
    json_object_set_new(json, "wear_flags", flags_to_json_array(obj->wear_flags, wear_flags));
    
    // Values array
    json_t *values = json_array();
    for (int i = 0; i < 8; i++)
        json_array_append_new(values, json_integer(obj->value[i]));
    json_object_set_new(json, "values", values);
    
    // Misc stats
    json_object_set_new(json, "level", json_integer(obj->level));
    json_object_set_new(json, "weight", json_integer(obj->weight));
    json_object_set_new(json, "cost", json_integer(obj->cost));
    json_object_set_new(json, "condition", json_integer(obj->condition));
    
    // Waypoints (for maps)
    if (obj->waypoints && list_size(obj->waypoints) > 0) {
        json_t *waypoints = json_array();
        ITERATOR wit;
        WAYPOINT_DATA *wp;
        iterator_start(&wit, obj->waypoints);
        while ((wp = (WAYPOINT_DATA *)iterator_nextdata(&wit))) {
            json_t *wp_json = json_object();
            json_object_set_new(wp_json, "wilds_uid", json_integer(wp->w));
            json_object_set_new(wp_json, "x", json_integer(wp->x));
            json_object_set_new(wp_json, "y", json_integer(wp->y));
            json_object_set_new(wp_json, "name", json_string(wp->name));
            json_array_append_new(waypoints, wp_json);
        }
        iterator_stop(&wit);
        json_object_set_new(json, "waypoints", waypoints);
    }
    
    // Affects
    json_t *affects = json_array();
    for (AFFECT_DATA *af = obj->affected; af; af = af->next) {
        json_t *af_json = json_area_serialize_affect(af);
        if (af_json)
            json_array_append_new(affects, af_json);
    }
    if (json_array_size(affects) > 0)
        json_object_set_new(json, "affects", affects);
    else
        json_decref(affects);
    
    // Catalysts
    json_t *catalysts = json_array();
    for (AFFECT_DATA *cat = obj->catalyst; cat; cat = cat->next) {
        json_t *cat_json = json_area_serialize_catalyst(cat);
        if (cat_json)
            json_array_append_new(catalysts, cat_json);
    }
    if (json_array_size(catalysts) > 0)
        json_object_set_new(json, "catalysts", catalysts);
    else
        json_decref(catalysts);
    
    // Extra descriptions
    json_t *extra_descrs = json_array();
    for (EXTRA_DESCR_DATA *ed = obj->extra_descr; ed; ed = ed->next) {
        json_t *ed_json = json_object();
        json_object_set_new(ed_json, "keyword", json_string(ed->keyword));
        if (ed->description)
            json_object_set_new(ed_json, "description", json_string(ed->description));
        else
            json_object_set_new(ed_json, "environmental", json_true());
        json_array_append_new(extra_descrs, ed_json);
    }
    if (json_array_size(extra_descrs) > 0)
        json_object_set_new(json, "extra_descrs", extra_descrs);
    else
        json_decref(extra_descrs);
    
    // Lock
    if (obj->lock) {
        json_t *lock = json_object();
        json_object_set_new(lock, "key_vnum", json_integer(obj->lock->key_vnum));
        json_object_set_new(lock, "flags", json_integer(obj->lock->flags));
        json_object_set_new(lock, "pick_chance", json_integer(obj->lock->pick_chance));
        json_object_set_new(json, "lock", lock);
    }
    
    // Progs
    json_t *progs = json_area_serialize_progs(obj->progs, obj->area);
    if (progs)
        json_object_set_new(json, "progs", progs);
    
    // Index vars
    if (obj->index_vars) {
        json_t *index_vars = json_area_serialize_index_vars(obj->index_vars, obj->area);
        if (index_vars)
            json_object_set_new(json, "index_vars", index_vars);
    }
    
    // TODO: Spells
    
    return json;
}

OBJ_INDEX_DATA *json_area_deserialize_object(json_t *json, AREA_DATA *area)
{
    if (!json || !area) return NULL;
    
    OBJ_INDEX_DATA *obj = new_obj_index();
    if (!obj) return NULL;
    
    obj->area = area;
    
    // Basic info
    obj->vnum = json_get_int_default(json, "vnum", 0);
    obj->name = str_dup(json_get_string_default(json, "name", "object"));
    obj->short_descr = str_dup(json_get_string_default(json, "short_descr", "an object"));
    obj->description = str_dup(json_get_string_default(json, "long_descr", "An object is here."));
    obj->full_description = str_dup(json_get_string_default(json, "description", "This is an object."));
    
    const char *sig = json_get_string_default(json, "imp_sig", "");
    if (sig && sig[0] != '\0')
        obj->imp_sig = str_dup(sig);
    
    const char *creator = json_get_string_default(json, "creator_sig", "");
    if (creator && creator[0] != '\0')
        obj->creator_sig = str_dup(creator);
    
    const char *skeywds = json_get_string_default(json, "script_keywords", "");
    if (skeywds && skeywds[0] != '\0')
        obj->skeywds = str_dup(skeywds);
    
    const char *comments = json_get_string_default(json, "comments", "");
    if (comments && comments[0] != '\0')
        obj->comments = str_dup(comments);
    
    // Item type
    const char *item_type_str = json_get_string_default(json, "item_type", "trash");
    obj->item_type = item_lookup(item_type_str);
    
    // Flags - note: extra[] is a 4-element array, not extra_flags[]
    json_t *extra_flags_json = json_object_get(json, "extra_flags");
    if (extra_flags_json) obj->extra[0] = json_array_to_flags(extra_flags_json, extra_flags);
    
    json_t *extra2_flags_json = json_object_get(json, "extra2_flags");
    if (extra2_flags_json) obj->extra[1] = json_array_to_flags(extra2_flags_json, extra2_flags);
    
    json_t *extra3_flags_json = json_object_get(json, "extra3_flags");
    if (extra3_flags_json) obj->extra[2] = json_array_to_flags(extra3_flags_json, extra3_flags);
    
    json_t *extra4_flags_json = json_object_get(json, "extra4_flags");
    if (extra4_flags_json) obj->extra[3] = json_array_to_flags(extra4_flags_json, extra4_flags);
    
    json_t *wear_flags_json = json_object_get(json, "wear_flags");
    if (wear_flags_json) obj->wear_flags = json_array_to_flags(wear_flags_json, wear_flags);
    
    // Values
    json_t *values = json_object_get(json, "values");
    if (values && json_is_array(values)) {
        size_t index;
        json_t *value;
        json_array_foreach(values, index, value) {
            if (index < 8 && json_is_integer(value)) {
                obj->value[index] = json_integer_value(value);
            }
        }
    }
    
    // Numeric fields
    obj->level = json_get_int_default(json, "level", 0);
    obj->weight = json_get_int_default(json, "weight", 0);
    obj->cost = json_get_int_default(json, "cost", 0);
    obj->condition = json_get_int_default(json, "condition", 100);
    obj->times_allowed_fixed = json_get_int_default(json, "times_allowed_fixed", 0);
    obj->fragility = json_get_int_default(json, "fragility", 0);
    
    const char *material = json_get_string_default(json, "material", "");
    if (material && material[0] != '\0')
        obj->material = str_dup(material);
    
    // Affects
    json_t *affects = json_object_get(json, "affects");
    if (affects && json_is_array(affects)) {
        size_t index;
        json_t *affect_json;
        json_array_foreach(affects, index, affect_json) {
            AFFECT_DATA *aff = json_area_deserialize_affect(affect_json);
            if (aff) {
                aff->next = obj->affected;
                obj->affected = aff;
            }
        }
    }
    
    // Catalysts
    json_t *catalysts = json_object_get(json, "catalysts");
    if (catalysts && json_is_array(catalysts)) {
        size_t index;
        json_t *catalyst_json;
        json_array_foreach(catalysts, index, catalyst_json) {
            AFFECT_DATA *cat = json_area_deserialize_catalyst(catalyst_json);
            if (cat) {
                cat->next = obj->catalyst;
                obj->catalyst = cat;
            }
        }
    }
    
    // Extra descriptions
    json_t *extra_descr = json_object_get(json, "extra_descr");
    if (extra_descr && json_is_array(extra_descr)) {
        EXTRA_DESCR_DATA *ed_last = NULL;
        size_t index;
        json_t *ed_json;
        json_array_foreach(extra_descr, index, ed_json) {
            EXTRA_DESCR_DATA *ed = alloc_perm(sizeof(*ed));
            ed->keyword = str_dup(json_get_string_default(ed_json, "keyword", ""));
            ed->description = str_dup(json_get_string_default(ed_json, "description", ""));
            ed->next = NULL;
            
            if (!obj->extra_descr) {
                obj->extra_descr = ed;
            } else {
                ed_last->next = ed;
            }
            ed_last = ed;
        }
    }
    
    // Waypoints - using LLIST, skip deserialization for now (TODO)
    // OBJ_INDEX_DATA uses LLIST *waypoints, not arrays
    
    // Lock
    json_t *lock = json_object_get(json, "lock");
    if (lock) {
        if (!obj->lock) {
            obj->lock = alloc_perm(sizeof(LOCK_STATE));
        }
        obj->lock->key_vnum = json_get_int_default(lock, "key_vnum", 0);
        
        json_t *lock_flags_json = json_object_get(lock, "flags");
        if (lock_flags_json) {
            obj->lock->flags = json_array_to_flags(lock_flags_json, lock_flags);
        }
        
        obj->lock->pick_chance = json_get_int_default(lock, "pick_chance", 0);
    }
    
    // Progs
    obj->progs = json_area_deserialize_progs(json_object_get(json, "progs"), area, PRG_OPROG);
    
    // Index vars
    obj->index_vars = json_area_deserialize_index_vars(json_object_get(json, "index_vars"), area);
    
    obj->persist = json_get_bool_default(json, "persist", false);
    
    // Add to area hash table for lookups
    int iHash = obj->vnum % MAX_KEY_HASH;
    obj->next = area->obj_index_hash[iHash];
    area->obj_index_hash[iHash] = obj;
    
    return obj;
}

json_t *json_area_serialize_script(SCRIPT_DATA *script)
{
    if (!script) return NULL;
    
    json_t *json = json_object();
    if (!json) return NULL;
    
    /* Core fields */
    json_object_set_new(json, "vnum", json_integer(script->vnum));
    json_object_set_new(json, "name", json_string(script->name ? script->name : ""));
    
    /* Source code - use edit_src if available, otherwise src */
    const char *code = script->edit_src ? script->edit_src : (script->src ? script->src : "");
    json_object_set_new(json, "code", json_string(code));
    
    /* Flags */
    if (script->flags != 0) {
        char flag_buf[MAX_STRING_LENGTH];
        sprintf(flag_buf, "%s", flag_string(script_flags, script->flags));
        json_object_set_new(json, "flags", json_string(flag_buf));
    }
    
    /* Security and depth */
    json_object_set_new(json, "depth", json_integer(script->depth));
    json_object_set_new(json, "security", json_integer(script->security));
    
    /* Run security if set */
    if (script->run_security > 0) {
        json_object_set_new(json, "run_security", json_integer(script->run_security));
    }
    
    /* Comments */
    if (script->comments) {
        json_object_set_new(json, "comments", json_string(script->comments));
    }
    
    return json;
}

SCRIPT_DATA *json_area_deserialize_script(json_t *json, AREA_DATA *area, int type)
{
    if (!json || !area) return NULL;
    
    SCRIPT_DATA *script = new_script();
    if (!script) return NULL;
    
    /* Core fields */
    script->vnum = json_get_int_default(json, "vnum", 0);
    script->area = area;
    
    const char *name = json_get_string_default(json, "name", "");
    if (name && name[0] != '\0') {
        script->name = str_dup(name);
    }
    
    /* Source code */
    const char *code = json_get_string_default(json, "code", "");
    if (code && code[0] != '\0') {
        script->edit_src = str_dup(code);
    }
    
    /* Flags */
    const char *flags_str = json_get_string_default(json, "flags", "");
    if (flags_str && flags_str[0] != '\0') {
        long value = flag_value(script_flags, flags_str);
        script->flags = (value != NO_FLAG) ? value : 0;
    }
    
    /* Security and depth */
    script->depth = json_get_int_default(json, "depth", 0);
    script->security = json_get_int_default(json, "security", 0);
    script->run_security = json_get_int_default(json, "run_security", 0);
    
    /* Comments */
    const char *comments = json_get_string_default(json, "comments", NULL);
    if (comments) {
        script->comments = str_dup(comments);
    }
    
    /* Compile the script - must have code */
    if (!script->edit_src) {
        free_script(script);
        return NULL;
    }
    
    /* This will compile the script and keep it even if there are errors */
    compile_script(NULL, script, script->edit_src, type);
    
    return script;
}

/* Convert reset command character to human-readable string */
static const char *reset_command_to_string(char cmd)
{
    switch (cmd) {
        case '*': return "comment";
        case 'M': return "spawn_mobile";
        case 'O': return "place_object";
        case 'P': return "put_in_container";
        case 'G': return "give_to_mobile";
        case 'E': return "equip_to_mobile";
        case 'D': return "set_door_state";
        case 'R': return "randomize_exits";
        case 'S': return "stop";
        default:  return "unknown";
    }
}

/* Convert human-readable string to reset command character */
static char reset_string_to_command(const char *str)
{
    if (!str) return 'S';
    if (!str_cmp(str, "comment")) return '*';
    if (!str_cmp(str, "spawn_mobile")) return 'M';
    if (!str_cmp(str, "place_object")) return 'O';
    if (!str_cmp(str, "put_in_container")) return 'P';
    if (!str_cmp(str, "give_to_mobile")) return 'G';
    if (!str_cmp(str, "equip_to_mobile")) return 'E';
    if (!str_cmp(str, "set_door_state")) return 'D';
    if (!str_cmp(str, "randomize_exits")) return 'R';
    if (!str_cmp(str, "stop")) return 'S';
    return 'S'; /* default to stop if unknown */
}

json_t *json_area_serialize_reset(RESET_DATA *reset)
{
    if (!reset) return NULL;
    
    json_t *json = json_object();
    if (!json) return NULL;
    
    json_object_set_new(json, "command", json_string(reset_command_to_string(reset->command)));
    json_object_set_new(json, "arg1", json_integer(reset->arg1));
    json_object_set_new(json, "arg2", json_integer(reset->arg2));
    json_object_set_new(json, "arg3", json_integer(reset->arg3));
    json_object_set_new(json, "arg4", json_integer(reset->arg4));
    
    return json;
}

RESET_DATA *json_area_deserialize_reset(json_t *json, AREA_DATA *area)
{
    if (!json) return NULL;
    
    RESET_DATA *reset = alloc_perm(sizeof(*reset));
    if (!reset) return NULL;
    
    const char *cmd_str = json_get_string_default(json, "command", "stop");
    reset->command = reset_string_to_command(cmd_str);
    reset->arg1 = json_get_int_default(json, "arg1", 0);
    reset->arg2 = json_get_int_default(json, "arg2", 0);
    reset->arg3 = json_get_int_default(json, "arg3", 0);
    reset->arg4 = json_get_int_default(json, "arg4", 0);
    reset->next = NULL;
    
    return reset;
}

/* ============================================================================
 * Shop Stock Serialization
 * ============================================================================ */

json_t *json_area_serialize_shop_stock(SHOP_STOCK_DATA *stock, AREA_DATA *area)
{
    if (!stock) return NULL;
    
    json_t *json = json_object();
    if (!json) return NULL;
    
    /* Stock type and vnum */
    json_object_set_new(json, "type", json_integer(stock->type));
    
    switch (stock->type) {
        case STOCK_OBJECT:
            json_object_set_new(json, "vnum", json_integer(stock->vnum));
            break;
        case STOCK_PET:
        case STOCK_MOUNT:
        case STOCK_GUARD:
            json_object_set_new(json, "mob_vnum", json_integer(stock->vnum));
            break;
        case STOCK_SHIP:
            json_object_set_new(json, "ship_vnum", json_integer(stock->vnum));
            break;
        case STOCK_CREW:
            json_object_set_new(json, "crew_vnum", json_integer(stock->vnum));
            break;
        case STOCK_CUSTOM:
            if (stock->custom_keyword && stock->custom_keyword[0] != '\0')
                json_object_set_new(json, "keyword", json_string(stock->custom_keyword));
            break;
    }
    
    /* Pricing */
    if (stock->custom_price && stock->custom_price[0] != '\0') {
        json_object_set_new(json, "custom_price", json_string(stock->custom_price));
    } else {
        if (stock->silver > 0)
            json_object_set_new(json, "silver", json_integer(stock->silver));
        if (stock->qp > 0)
            json_object_set_new(json, "qp", json_integer(stock->qp));
        if (stock->dp > 0)
            json_object_set_new(json, "dp", json_integer(stock->dp));
        if (stock->pneuma > 0)
            json_object_set_new(json, "pneuma", json_integer(stock->pneuma));
    }
    
    /* Level and discount */
    if (stock->level > 0)
        json_object_set_new(json, "level", json_integer(stock->level));
    if (stock->discount > 0)
        json_object_set_new(json, "discount", json_integer(stock->discount));
    
    /* Quantity and restocking */
    json_object_set_new(json, "quantity", json_integer(stock->quantity));
    if (stock->max_quantity > 0)
        json_object_set_new(json, "max_quantity", json_integer(stock->max_quantity));
    if (stock->restock_rate > 0)
        json_object_set_new(json, "restock_rate", json_integer(stock->restock_rate));
    
    /* Duration */
    if (stock->duration > 0)
        json_object_set_new(json, "duration", json_integer(stock->duration));
    
    /* Custom description */
    if (stock->custom_descr && stock->custom_descr[0] != '\0')
        json_object_set_new(json, "description", json_string(stock->custom_descr));
    
    /* Singular flag */
    if (stock->singular)
        json_object_set_new(json, "singular", json_true());
    
    return json;
}

SHOP_STOCK_DATA *json_area_deserialize_shop_stock(json_t *json, AREA_DATA *area)
{
    if (!json) return NULL;
    
    SHOP_STOCK_DATA *stock = new_shop_stock();
    if (!stock) return NULL;
    
    /* Stock type */
    stock->type = json_get_int_default(json, "type", STOCK_OBJECT);
    
    /* Vnum based on type */
    /* NOTE: Pointer fixup (obj/mob) deferred until after all entities loaded */
    switch (stock->type) {
        case STOCK_OBJECT:
            stock->vnum = json_get_int_default(json, "vnum", 0);
            stock->obj = NULL;  /* Will be fixed up later */
            break;
        case STOCK_PET:
        case STOCK_MOUNT:
        case STOCK_GUARD:
            stock->vnum = json_get_int_default(json, "mob_vnum", 0);
            stock->mob = NULL;  /* Will be fixed up later */
            break;
        case STOCK_SHIP:
            stock->vnum = json_get_int_default(json, "ship_vnum", 0);
            stock->ship = NULL;  /* Will be fixed up later */
            break;
        case STOCK_CREW:
            stock->vnum = json_get_int_default(json, "crew_vnum", 0);
            break;
        case STOCK_CUSTOM:
            stock->custom_keyword = str_dup(json_get_string_default(json, "keyword", ""));
            break;
    }
    
    /* Pricing */
    const char *custom_price = json_get_string_default(json, "custom_price", "");
    if (custom_price && custom_price[0] != '\0') {
        stock->custom_price = str_dup(custom_price);
    } else {
        stock->silver = json_get_int_default(json, "silver", 0);
        stock->qp = json_get_int_default(json, "qp", 0);
        stock->dp = json_get_int_default(json, "dp", 0);
        stock->pneuma = json_get_int_default(json, "pneuma", 0);
    }
    
    /* Level and discount */
    stock->level = json_get_int_default(json, "level", 0);
    stock->discount = json_get_int_default(json, "discount", 0);
    
    /* Quantity and restocking */
    stock->quantity = json_get_int_default(json, "quantity", 0);
    stock->max_quantity = json_get_int_default(json, "max_quantity", 0);
    stock->restock_rate = json_get_int_default(json, "restock_rate", 0);
    
    /* Duration */
    stock->duration = json_get_int_default(json, "duration", 0);
    
    /* Custom description */
    const char *custom_descr = json_get_string_default(json, "description", "");
    if (custom_descr && custom_descr[0] != '\0')
        stock->custom_descr = str_dup(custom_descr);
    
    /* Singular flag */
    stock->singular = json_get_bool_default(json, "singular", false);
    
    return stock;
}

/* ============================================================================
 * Shop Serialization
 * ============================================================================ */

json_t *json_area_serialize_shop(SHOP_DATA *shop, AREA_DATA *ref_area)
{
    if (!shop) return NULL;
    
    json_t *json = json_object();
    if (!json) return NULL;
    
    /* Keeper */
    json_object_set_new(json, "keeper", json_integer(shop->keeper));
    
    /* Buy types */
    json_t *buy_types = json_array();
    for (int i = 0; i < MAX_TRADE; i++) {
        if (shop->buy_type[i] != 0)
            json_array_append_new(buy_types, json_integer(shop->buy_type[i]));
    }
    if (json_array_size(buy_types) > 0)
        json_object_set_new(json, "buy_types", buy_types);
    else
        json_decref(buy_types);
    
    /* Profit margins */
    json_object_set_new(json, "profit_buy", json_integer(shop->profit_buy));
    json_object_set_new(json, "profit_sell", json_integer(shop->profit_sell));
    
    /* Hours */
    json_object_set_new(json, "open_hour", json_integer(shop->open_hour));
    json_object_set_new(json, "close_hour", json_integer(shop->close_hour));
    
    /* Restocking */
    if (shop->restock_interval > 0)
        json_object_set_new(json, "restock_interval", json_integer(shop->restock_interval));
    
    /* Flags and discount */
    if (shop->flags > 0)
        json_object_set_new(json, "flags", json_integer(shop->flags));
    json_object_set_new(json, "discount", json_integer(shop->discount));
    
    /* Shipyard */
    if (shop->shipyard > 0) {
        json_t *shipyard = json_object();
        json_object_set_new(shipyard, "wilds_uid", json_integer(shop->shipyard));
        json_object_set_new(shipyard, "x1", json_integer(shop->shipyard_region[0][0]));
        json_object_set_new(shipyard, "y1", json_integer(shop->shipyard_region[0][1]));
        json_object_set_new(shipyard, "x2", json_integer(shop->shipyard_region[1][0]));
        json_object_set_new(shipyard, "y2", json_integer(shop->shipyard_region[1][1]));
        if (shop->shipyard_description && shop->shipyard_description[0] != '\0')
            json_object_set_new(shipyard, "description", json_string(shop->shipyard_description));
        json_object_set_new(json, "shipyard", shipyard);
    }
    
    /* Stock items */
    json_t *stock_array = json_array();
    for (SHOP_STOCK_DATA *stock = shop->stock; stock; stock = stock->next) {
        json_t *stock_json = json_area_serialize_shop_stock(stock, ref_area);
        if (stock_json)
            json_array_append_new(stock_array, stock_json);
    }
    if (json_array_size(stock_array) > 0)
        json_object_set_new(json, "stock", stock_array);
    else
        json_decref(stock_array);
    
    return json;
}

SHOP_DATA *json_area_deserialize_shop(json_t *json, AREA_DATA *area)
{
    if (!json) return NULL;
    
    SHOP_DATA *shop = new_shop();
    if (!shop) return NULL;
    
    /* Keeper */
    shop->keeper = json_get_int_default(json, "keeper", 0);
    
    /* Buy types */
    json_t *buy_types = json_object_get(json, "buy_types");
    if (buy_types && json_is_array(buy_types)) {
        size_t index;
        json_t *value;
        int i = 0;
        json_array_foreach(buy_types, index, value) {
            if (i < MAX_TRADE && json_is_integer(value)) {
                shop->buy_type[i++] = json_integer_value(value);
            }
        }
    }
    
    /* Profit margins */
    shop->profit_buy = json_get_int_default(json, "profit_buy", 100);
    shop->profit_sell = json_get_int_default(json, "profit_sell", 100);
    
    /* Hours */
    shop->open_hour = json_get_int_default(json, "open_hour", 0);
    shop->close_hour = json_get_int_default(json, "close_hour", 23);
    
    /* Restocking */
    shop->restock_interval = json_get_int_default(json, "restock_interval", 0);
    
    /* Flags and discount */
    shop->flags = json_get_int_default(json, "flags", 0);
    shop->discount = json_get_int_default(json, "discount", 50);
    
    /* Shipyard */
    json_t *shipyard = json_object_get(json, "shipyard");
    if (shipyard) {
        shop->shipyard = json_get_int_default(shipyard, "wilds_uid", 0);
        shop->shipyard_region[0][0] = json_get_int_default(shipyard, "x1", 0);
        shop->shipyard_region[0][1] = json_get_int_default(shipyard, "y1", 0);
        shop->shipyard_region[1][0] = json_get_int_default(shipyard, "x2", 0);
        shop->shipyard_region[1][1] = json_get_int_default(shipyard, "y2", 0);
        const char *desc = json_get_string_default(shipyard, "description", "");
        if (desc && desc[0] != '\0')
            shop->shipyard_description = str_dup(desc);
    }
    
    /* Stock items */
    json_t *stock_array = json_object_get(json, "stock");
    if (stock_array && json_is_array(stock_array)) {
        size_t index;
        json_t *stock_json;
        json_array_foreach(stock_array, index, stock_json) {
            SHOP_STOCK_DATA *stock = json_area_deserialize_shop_stock(stock_json, area);
            if (stock) {
                stock->next = shop->stock;
                shop->stock = stock;
            }
        }
    }
    
    return shop;
}

json_t *json_area_serialize_affect(AFFECT_DATA *affect)
{
    if (!affect) return NULL;
    
    json_t *json = json_object();
    if (!json) return NULL;
    
    json_object_set_new(json, "where", json_integer(affect->where));
    json_object_set_new(json, "location", json_integer(affect->location));
    json_object_set_new(json, "modifier", json_integer(affect->modifier));
    json_object_set_new(json, "level", json_integer(affect->level));
    json_object_set_new(json, "type", json_integer(affect->type));
    json_object_set_new(json, "duration", json_integer(affect->duration));
    
    if (affect->bitvector != 0)
        json_object_set_new(json, "bitvector", json_integer(affect->bitvector));
    if (affect->bitvector2 != 0)
        json_object_set_new(json, "bitvector2", json_integer(affect->bitvector2));
    
    json_object_set_new(json, "random", json_integer(affect->random));
    
    return json;
}

AFFECT_DATA *json_area_deserialize_affect(json_t *json)
{
    if (!json) return NULL;
    
    AFFECT_DATA *af = alloc_perm(sizeof(*af));
    if (!af) return NULL;
    
    af->where = json_get_int_default(json, "where", 0);
    af->location = json_get_int_default(json, "location", 0);
    af->modifier = json_get_int_default(json, "modifier", 0);
    af->level = json_get_int_default(json, "level", 0);
    af->type = json_get_int_default(json, "type", 0);
    af->duration = json_get_int_default(json, "duration", 0);
    af->bitvector = json_get_int_default(json, "bitvector", 0);
    af->bitvector2 = json_get_int_default(json, "bitvector2", 0);
    af->random = json_get_int_default(json, "random", 0);
    af->next = NULL;
    
    return af;
}

json_t *json_area_serialize_catalyst(AFFECT_DATA *cat)
{
    if (!cat) return NULL;
    
    json_t *json = json_object();
    if (!json) return NULL;
    
    json_object_set_new(json, "type", json_string(flag_string(catalyst_types, cat->type)));
    
    if (cat->where == TO_CATALYST_ACTIVE)
        json_object_set_new(json, "active", json_true());
    
    if (cat->custom_name && cat->custom_name[0] != '\0')
        json_object_set_new(json, "name", json_string(cat->custom_name));
    
    json_object_set_new(json, "charges", json_integer(cat->modifier));
    json_object_set_new(json, "strength", json_integer(cat->level));
    json_object_set_new(json, "random", json_integer(cat->random));
    
    return json;
}

AFFECT_DATA *json_area_deserialize_catalyst(json_t *json)
{
    if (!json) return NULL;
    
    AFFECT_DATA *cat = alloc_perm(sizeof(*cat));
    if (!cat) return NULL;
    
    const char *type_str = json_get_string_default(json, "type", "");
    cat->type = flag_lookup(type_str, catalyst_types);
    
    cat->where = json_get_bool_default(json, "active", false) ? TO_CATALYST_ACTIVE : TO_CATALYST_DORMANT;
    
    const char *name = json_get_string_default(json, "name", "");
    if (name && name[0] != '\0')
        cat->custom_name = str_dup(name);
    else
        cat->custom_name = NULL;
    
    cat->modifier = json_get_int_default(json, "charges", 0);
    cat->level = json_get_int_default(json, "strength", 0);
    cat->random = json_get_int_default(json, "random", 0);
    cat->next = NULL;
    
    return cat;
}

/***************************************************************************
 * Script/Prog Serialization                                               *
 ***************************************************************************/

json_t *json_area_serialize_progs(LLIST **progs, AREA_DATA *area)
{
    if (!progs) return NULL;
    
    json_t *array = json_array();
    if (!array) return NULL;
    
    int slot, count = 0;
    ITERATOR it;
    PROG_LIST *trigger;
    
    /* Iterate through all 15 trigger slots */
    for (slot = 0; slot < TRIGSLOT_MAX; slot++) {
        if (!progs[slot] || list_size(progs[slot]) == 0)
            continue;
            
        iterator_start(&it, progs[slot]);
        while ((trigger = (PROG_LIST *)iterator_nextdata(&it))) {
            json_t *prog_json = json_object();
            if (!prog_json) continue;
            
            /* Script vnum */
            json_object_set_new(prog_json, "vnum", json_integer(trigger->vnum));
            
            /* Trigger type name (human-readable) */
            const char *trigger_name = (trigger->trig_type >= 0 && trigger->trig_type < trigger_table_size) 
                ? trigger_table[trigger->trig_type].name : "unknown";
            json_object_set_new(prog_json, "trigger", json_string(trigger_name));
            
            /* Trigger phrase */
            const char *phrase = trigger->trig_phrase ? trigger->trig_phrase : "";
            json_object_set_new(prog_json, "phrase", json_string(phrase));
            
            /* Numeric flag and number (for optimization) */
            json_object_set_new(prog_json, "numeric", trigger->numeric ? json_true() : json_false());
            if (trigger->numeric)
                json_object_set_new(prog_json, "number", json_integer(trigger->trig_number));
            
            json_array_append_new(array, prog_json);
            count++;
        }
        iterator_stop(&it);
    }
    
    /* Return NULL if no progs were serialized */
    if (count == 0) {
        json_decref(array);
        return NULL;
    }
    
    return array;
}

LLIST **json_area_deserialize_progs(json_t *json, AREA_DATA *area, int prog_type)
{
    if (!json || !json_is_array(json))
        return NULL;
    
    LLIST **progs = new_prog_bank();
    if (!progs) return NULL;
    
    size_t index;
    json_t *prog_json;
    
    json_array_foreach(json, index, prog_json) {
        long vnum = json_get_int_default(prog_json, "vnum", 0);
        if (vnum == 0) continue;
        
        const char *trigger_name = json_get_string_default(prog_json, "trigger", "");
        const char *phrase = json_get_string_default(prog_json, "phrase", "");
        
        /* Look up trigger type */
        int tindex = trigger_index((char *)trigger_name, prog_type);
        if (tindex < 0) {
            log_stringf("json_area_deserialize_progs: unknown trigger type '%s'", trigger_name);
            continue;
        }
        
        /* Get trigger slot */
        int slot = trigger_table[tindex].slot;
        if (slot < 0 || slot >= TRIGSLOT_MAX) {
            log_stringf("json_area_deserialize_progs: invalid slot %d for trigger '%s'", slot, trigger_name);
            continue;
        }
        
        /* Create new trigger */
        PROG_LIST *trigger = new_trigger();
        trigger->vnum = vnum;
        trigger->trig_type = tindex;
        trigger->trig_phrase = str_dup(phrase);
        trigger->numeric = json_get_bool_default(prog_json, "numeric", false);
        trigger->trig_number = trigger->numeric ? json_get_int_default(prog_json, "number", 0) : atoi(phrase);
        
        /* Script will be linked in fix_*progs() functions during boot */
        trigger->script = NULL;
        
        /* Add to appropriate slot */
        list_appendlink(progs[slot], trigger);
    }
    
    return progs;
}

/***************************************************************************
 * Index Variables Serialization                                           *
 ***************************************************************************/

json_t *json_area_serialize_index_vars(pVARIABLE index_vars, AREA_DATA *area)
{
    if (!index_vars) return NULL;
    
    json_t *array = json_array();
    if (!array) return NULL;
    
    int count = 0;
    
    for (pVARIABLE var = index_vars; var; var = var->next) {
        json_t *var_json = json_object();
        if (!var_json) continue;
        
        /* Variable name and save flag */
        json_object_set_new(var_json, "name", json_string(var->name));
        json_object_set_new(var_json, "save", var->save ? json_true() : json_false());
        
        /* Type-specific serialization */
        switch (var->type) {
            case VAR_INTEGER:
                json_object_set_new(var_json, "type", json_string("integer"));
                json_object_set_new(var_json, "value", json_integer(var->_.i));
                break;
                
            case VAR_STRING:
            case VAR_STRING_S:
                json_object_set_new(var_json, "type", json_string("string"));
                json_object_set_new(var_json, "value", json_string(var->_.s ? var->_.s : ""));
                break;
                
            case VAR_ROOM:
                if (var->_.r && var->_.r->vnum) {
                    json_object_set_new(var_json, "type", json_string("room"));
                    json_object_set_new(var_json, "vnum", json_integer(var->_.r->vnum));
                } else {
                    /* Skip invalid room references */
                    json_decref(var_json);
                    continue;
                }
                break;
                
            /* VAR_SKILL is currently commented out in olc_save_index_vars */
            /* so we skip it here as well for compatibility */
            
            default:
                /* Unknown or unsupported type - skip */
                json_decref(var_json);
                continue;
        }
        
        json_array_append_new(array, var_json);
        count++;
    }
    
    /* Return NULL if no variables were serialized */
    if (count == 0) {
        json_decref(array);
        return NULL;
    }
    
    return array;
}

pVARIABLE json_area_deserialize_index_vars(json_t *json, AREA_DATA *area)
{
    if (!json || !json_is_array(json))
        return NULL;
    
    pVARIABLE index_vars = NULL;
    
    size_t index;
    json_t *var_json;
    
    json_array_foreach(json, index, var_json) {
        const char *name = json_get_string_default(var_json, "name", NULL);
        if (!name || !name[0]) continue;
        
        bool saved = json_get_bool_default(var_json, "save", false);
        const char *type_str = json_get_string_default(var_json, "type", "");
        
        if (!str_cmp(type_str, "integer")) {
            int value = json_get_int_default(var_json, "value", 0);
            variables_setindex_integer(&index_vars, (char *)name, value, saved);
        } 
        else if (!str_cmp(type_str, "string")) {
            const char *value = json_get_string_default(var_json, "value", "");
            variables_setindex_string(&index_vars, (char *)name, (char *)value, false, saved);
        } 
        else if (!str_cmp(type_str, "room")) {
            long vnum = json_get_int_default(var_json, "vnum", 0);
            if (vnum > 0) {
                variables_setindex_room(&index_vars, (char *)name, vnum, saved);
            }
        }
        /* VAR_SKILL deserialization would go here if needed */
    }
    
    return index_vars;
}

/***************************************************************************
 * Token Serialization                                                     *
 ***************************************************************************/

json_t *json_area_serialize_token(TOKEN_INDEX_DATA *token)
{
    if (!token) return NULL;
    
    json_t *json = json_object();
    if (!json) return NULL;
    
    /* Basic info */
    json_object_set_new(json, "vnum", json_integer(token->vnum));
    json_object_set_new(json, "name", json_string(token->name));
    json_object_set_new(json, "description", json_string(token->description));
    
    if (token->comments && token->comments[0] != '\0')
        json_object_set_new(json, "comments", json_string(token->comments));
    
    /* Type - convert to human-readable string */
    const char *type_name = "general";
    switch (token->type) {
        case TOKEN_GENERAL:  type_name = "general"; break;
        case TOKEN_QUEST:    type_name = "quest"; break;
        case TOKEN_AFFECT:   type_name = "affect"; break;
        case TOKEN_SKILL:    type_name = "skill"; break;
        case TOKEN_SPELL:    type_name = "spell"; break;
        case TOKEN_SONG:     type_name = "song"; break;
        default:             type_name = "general"; break;
    }
    json_object_set_new(json, "type", json_string(type_name));
    
    /* Flags */
    json_object_set_new(json, "flags", flags_to_json_array(token->flags, token_flags));
    
    /* Timer */
    json_object_set_new(json, "timer", json_integer(token->timer));
    
    /* Values array with names */
    json_t *values = json_array();
    for (int i = 0; i < MAX_TOKEN_VALUES; i++) {
        json_t *value_obj = json_object();
        json_object_set_new(value_obj, "name", json_string(token->value_name[i]));
        json_object_set_new(value_obj, "value", json_integer(token->value[i]));
        json_array_append_new(values, value_obj);
    }
    json_object_set_new(json, "values", values);
    
    /* Extra descriptions */
    if (token->ed) {
        json_t *extra_descrs = json_array();
        for (EXTRA_DESCR_DATA *ed = token->ed; ed; ed = ed->next) {
            json_t *ed_json = json_object();
            json_object_set_new(ed_json, "keyword", json_string(ed->keyword));
            if (ed->description)
                json_object_set_new(ed_json, "description", json_string(ed->description));
            else
                json_object_set_new(ed_json, "environmental", json_true());
            json_array_append_new(extra_descrs, ed_json);
        }
        if (json_array_size(extra_descrs) > 0)
            json_object_set_new(json, "extra_descrs", extra_descrs);
        else
            json_decref(extra_descrs);
    }
    
    /* Progs */
    if (token->progs) {
        json_t *progs = json_area_serialize_progs(token->progs, token->area);
        if (progs)
            json_object_set_new(json, "progs", progs);
    }
    
    /* Index vars */
    if (token->index_vars) {
        json_t *index_vars = json_area_serialize_index_vars(token->index_vars, token->area);
        if (index_vars)
            json_object_set_new(json, "index_vars", index_vars);
    }
    
    return json;
}

TOKEN_INDEX_DATA *json_area_deserialize_token(json_t *json, AREA_DATA *area)
{
    if (!json || !area) return NULL;
    
    TOKEN_INDEX_DATA *token = new_token_index();
    if (!token) return NULL;
    
    token->area = area;
    
    /* Basic info */
    token->vnum = json_get_int_default(json, "vnum", 0);
    token->name = str_dup(json_get_string_default(json, "name", "(no name)"));
    token->description = str_dup(json_get_string_default(json, "description", ""));
    
    const char *comments = json_get_string_default(json, "comments", "");
    if (comments && comments[0] != '\0')
        token->comments = str_dup(comments);
    
    /* Type - convert from string */
    const char *type_str = json_get_string_default(json, "type", "general");
    if (!str_cmp(type_str, "general"))       token->type = TOKEN_GENERAL;
    else if (!str_cmp(type_str, "quest"))    token->type = TOKEN_QUEST;
    else if (!str_cmp(type_str, "affect"))   token->type = TOKEN_AFFECT;
    else if (!str_cmp(type_str, "skill"))    token->type = TOKEN_SKILL;
    else if (!str_cmp(type_str, "spell"))    token->type = TOKEN_SPELL;
    else if (!str_cmp(type_str, "song"))     token->type = TOKEN_SONG;
    else                                     token->type = TOKEN_GENERAL;
    
    /* Flags */
    json_t *flags_json = json_object_get(json, "flags");
    if (flags_json)
        token->flags = json_array_to_flags(flags_json, token_flags);
    
    /* Timer */
    token->timer = json_get_int_default(json, "timer", 0);
    
    /* Values array with names */
    json_t *values = json_object_get(json, "values");
    if (values && json_is_array(values)) {
        size_t index;
        json_t *value_obj;
        json_array_foreach(values, index, value_obj) {
            if (index >= MAX_TOKEN_VALUES) break;
            
            const char *value_name = json_get_string_default(value_obj, "name", "Unused");
            token->value_name[index] = str_dup(value_name);
            token->value[index] = json_get_int_default(value_obj, "value", 0);
        }
    }
    
    /* Extra descriptions */
    json_t *extra_descrs = json_object_get(json, "extra_descrs");
    if (extra_descrs && json_is_array(extra_descrs)) {
        size_t index;
        json_t *ed_json;
        json_array_foreach(extra_descrs, index, ed_json) {
            EXTRA_DESCR_DATA *ed = alloc_perm(sizeof(*ed));
            ed->keyword = str_dup(json_get_string_default(ed_json, "keyword", ""));
            
            if (json_get_bool_default(ed_json, "environmental", false)) {
                ed->description = NULL;
            } else {
                ed->description = str_dup(json_get_string_default(ed_json, "description", ""));
            }
            
            ed->next = token->ed;
            token->ed = ed;
        }
    }
    
    /* Progs */
    token->progs = json_area_deserialize_progs(json_object_get(json, "progs"), area, PRG_TPROG);
    
    /* Index vars */
    token->index_vars = json_area_deserialize_index_vars(json_object_get(json, "index_vars"), area);
    
    return token;
}

/*
 * Trade serialization
 */
json_t *json_area_serialize_trade_list(TRADE_ITEM *trade_list, AREA_DATA *area)
{
    if (!trade_list) {
        return NULL;
    }
    
    json_t *trade_array = json_array();
    TRADE_ITEM *trade;
    
    for (trade = trade_list; trade != NULL; trade = trade->next) {
        json_t *trade_obj = json_object();
        
        /* Trade type - convert to human-readable string */
        const char *type_str = "none";
        if (trade->trade_type >= 0 && trade->trade_type < TRADE_LAST) {
            if (trade_table[trade->trade_type].name && trade_table[trade->trade_type].name[0]) {
                type_str = trade_table[trade->trade_type].name;
            }
        }
        json_object_set_new(trade_obj, "type", json_string(type_str));
        json_object_set_new(trade_obj, "type_num", json_integer(trade->trade_type));
        
        /* Prices */
        json_object_set_new(trade_obj, "min_price", json_integer(trade->min_price));
        json_object_set_new(trade_obj, "max_price", json_integer(trade->max_price));
        
        /* Quantity */
        json_object_set_new(trade_obj, "max_qty", json_integer(trade->max_qty));
        
        /* Replenish */
        json_object_set_new(trade_obj, "replenish_amount", json_integer(trade->replenish_amount));
        json_object_set_new(trade_obj, "replenish_time", json_integer(trade->replenish_time));
        
        /* Object vnum */
        json_object_set_new(trade_obj, "obj_vnum", json_integer(trade->obj_vnum));
        
        /* Add role annotation (supplier/consumer) */
        const char *role = (trade->replenish_amount > 0) ? "supplier" : "consumer";
        json_object_set_new(trade_obj, "role", json_string(role));
        
        json_array_append_new(trade_array, trade_obj);
    }
    
    if (json_array_size(trade_array) == 0) {
        json_decref(trade_array);
        return NULL;
    }
    
    return trade_array;
}

/*
 * Trade deserialization
 */
void json_area_deserialize_trade_list(json_t *json, AREA_DATA *area)
{
    if (!json || !json_is_array(json)) {
        return;
    }
    
    size_t index;
    json_t *trade_json;
    
    json_array_foreach(json, index, trade_json) {
        /* Get trade type - try type_num first, then parse type string */
        int trade_type = json_get_int_default(trade_json, "type_num", -1);
        
        if (trade_type == -1) {
            /* Try to parse from type string */
            const char *type_str = json_get_string_default(trade_json, "type", "none");
            trade_type = TRADE_NONE;
            
            /* Search trade_table for matching name */
            for (int i = 0; i < TRADE_LAST; i++) {
                if (trade_table[i].name && !str_cmp(type_str, trade_table[i].name)) {
                    trade_type = i;
                    break;
                }
            }
        }
        
        /* Get other fields */
        long min_price = json_get_int_default(trade_json, "min_price", 0);
        long max_price = json_get_int_default(trade_json, "max_price", 0);
        long max_qty = json_get_int_default(trade_json, "max_qty", 0);
        long replenish_amount = json_get_int_default(trade_json, "replenish_amount", 0);
        long replenish_time = json_get_int_default(trade_json, "replenish_time", 0);
        long obj_vnum = json_get_int_default(trade_json, "obj_vnum", 0);
        
        /* Create the trade item */
        new_trade_item(area, trade_type, replenish_time, replenish_amount, 
                      max_qty, min_price, max_price, obj_vnum);
    }
}

/*
 * Fix up shop stock pointers after all areas are loaded.
 * This must be done globally because shops can reference objects/mobs from other areas.
 */
void fix_shops(void)
{
    AREA_DATA *area;
    
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Fixing shop stocks");
    
    for (area = area_first; area; area = area->next) {
        int hash_index, hash_count;
        
        if ((area->max_vnum - area->min_vnum) >= MAX_KEY_HASH) {
            hash_index = 0;
            hash_count = MAX_KEY_HASH;
        } else {
            hash_index = area->min_vnum % MAX_KEY_HASH;
            hash_count = area->max_vnum - area->min_vnum + 1;
        }
        
        for (int j = 0; j < hash_count; j++) {
            for (MOB_INDEX_DATA *mob = area->mob_index_hash[hash_index]; mob; mob = mob->next) {
                if (mob->vnum && mob->pShop) {
                    for (SHOP_STOCK_DATA *stock = mob->pShop->stock; stock; stock = stock->next) {
                        switch (stock->type) {
                            case STOCK_OBJECT:
                                if (stock->vnum > 0) {
                                    /* Use _global to search ALL areas for the object */
                                    stock->obj = get_obj_index_global(stock->vnum);
                                    if (!stock->obj) {
                                        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                            "fix_shops: Shop stock object vnum %ld not found (mob %ld in %s)",
                                            stock->vnum, mob->vnum, area->file_name);
                                    }
                                }
                                break;
                            case STOCK_PET:
                            case STOCK_MOUNT:
                            case STOCK_GUARD:
                                if (stock->vnum > 0) {
                                    /* Use _global to search ALL areas for the mob */
                                    stock->mob = get_mob_index_global(stock->vnum);
                                    if (!stock->mob) {
                                        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                            "fix_shops: Shop stock mob vnum %ld not found (mob %ld in %s)",
                                            stock->vnum, mob->vnum, area->file_name);
                                    }
                                }
                                break;
                            case STOCK_SHIP:
                                if (stock->vnum > 0) {
                                    stock->ship = get_ship_index(stock->vnum);
                                    if (!stock->ship) {
                                        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                            "fix_shops: Shop ship vnum %ld not found (mob %ld in %s)",
                                            stock->vnum, mob->vnum, area->file_name);
                                    }
                                }
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
            
            if (++hash_index == MAX_KEY_HASH)
                hash_index = 0;
        }
    }
}
