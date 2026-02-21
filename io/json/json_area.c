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
#include "../../merc.h"
#include "../../tables.h"
#include "../../recycle.h"
#include "../../scripts.h"
#include "json_area.h"
#include "json_obj_types.h"
#include "../cache/redis_cache.h"
#include "../../editors/common.h"

#include "../../wilds.h"

// Forward declarations for serialize functions
json_t *json_area_serialize_blueprint_section(BLUEPRINT_SECTION *section, AREA_DATA *area);
json_t *json_area_serialize_blueprint(BLUEPRINT *blueprint, AREA_DATA *area);
json_t *json_area_serialize_dungeon(DUNGEON_INDEX_DATA *dungeon, AREA_DATA *area);
json_t *json_area_serialize_ship(SHIP_INDEX_DATA *ship, AREA_DATA *area);
static json_t *json_area_serialize_reputation(REPUTATION_INDEX_DATA *reputation, AREA_DATA *area);
static REPUTATION_INDEX_DATA *json_area_deserialize_reputation(json_t *json, AREA_DATA *area);
static json_t *json_area_serialize_quest_v2(QUEST_INDEX_V2_DATA *quest_index_v2, AREA_DATA *area);
static QUEST_INDEX_V2_DATA *json_area_deserialize_quest_v2(json_t *json, AREA_DATA *area);
static bool json_script_array_has_vnum(json_t *scripts, long vnum);

// --- WILDS_TERRAIN JSON helpers ---
static json_t *wilds_terrain_to_json(WILDS_TERRAIN *terrain) {
    if (!terrain) return NULL;
    json_t *json = json_object();
    json_object_set_new(json, "mapchar", json_integer(terrain->mapchar));
    json_object_set_new(json, "showchar", json_string_safe(terrain->showchar));
    json_object_set_new(json, "showname", json_string_safe(terrain->showname));
    json_object_set_new(json, "briefdesc", json_string_safe(terrain->briefdesc));
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
    json_object_set_new(json, "map_tile", json_string_safe(vlink->map_tile));
    if (vlink->dest_load.auid > 0 && vlink->dest_load.vnum > 0) {
        char wnum_buf[MIL];
        snprintf(wnum_buf, sizeof(wnum_buf), "%ld#%ld", vlink->dest_load.auid, vlink->dest_load.vnum);
        json_object_set_new(json, "destvnum", json_string(wnum_buf));
    } else if (vlink->pDestRoom) {
        json_object_set_new(json, "destvnum", json_string(widevnum_string_room(vlink->pDestRoom, NULL)));
    } else if (vlink->destvnum > 0) {
        json_object_set_new(json, "destvnum", json_integer(vlink->destvnum));
    }
    json_object_set_new(json, "destination_mode", json_integer(vlink->destination_mode));
    json_object_set_new(json, "dungeon_floor", json_integer(UMAX(1, vlink->dungeon_floor)));
    json_object_set_new(json, "default_linkage", json_integer(vlink->default_linkage));
    json_object_set_new(json, "current_linkage", json_integer(vlink->current_linkage));
    json_object_set_new(json, "orig_description", json_string_safe(vlink->orig_description));
    json_object_set_new(json, "orig_keyword", json_string_safe(vlink->orig_keyword));
    json_object_set_new(json, "orig_rs_flags", json_integer(vlink->orig_rs_flags));
    json_object_set_new(json, "orig_key", json_integer(vlink->orig_key));
    json_object_set_new(json, "orig_lock", json_integer(vlink->orig_lock));
    json_object_set_new(json, "orig_pick", json_integer(vlink->orig_pick));
    json_object_set_new(json, "rev_description", json_string_safe(vlink->rev_description));
    json_object_set_new(json, "rev_keyword", json_string_safe(vlink->rev_keyword));
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
    /* Parse destvnum - supports both widevnum string and legacy integer */
    {
        json_t *destvnum_val = json_object_get(json, "destvnum");
        if (json_is_string(destvnum_val)) {
            WNUM_LOAD wload;
            if (parse_widevnum_load(json_string_value(destvnum_val), &wload)) {
                vlink->destvnum = wload.vnum;
                vlink->dest_load = wload;
                vlink->pDestRoom = NULL;
            }
        } else {
            vlink->destvnum = json_get_int_default(json, "destvnum", 0);
            vlink->dest_load.auid = 0;
            vlink->dest_load.vnum = vlink->destvnum;
        }
    }
    vlink->destination_mode = json_get_int_default(json, "destination_mode", VLINK_DEST_ROOM);
    vlink->dungeon_floor = UMAX(1, json_get_int_default(json, "dungeon_floor", 1));
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
    json_object_set_new(json, "name", json_string_safe(wilds->name));
    json_object_set_new(json, "wilds_format", json_integer(wilds->wilds_format));
    json_object_set_new(json, "staticmap", json_string_safe(wilds->staticmap));
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

static json_t *json_area_serialize_region_data(AREA_REGION *region)
{
    if (!region)
        return NULL;

    json_t *json = json_object();
    json_t *recall = json_object();

    json_object_set_new(json, "uid", json_integer(region->uid));
    json_object_set_new(json, "name", json_string_safe(region->name));
    json_object_set_new(json, "description", json_string_safe(region->description));
    json_object_set_new(json, "comments", json_string_safe(region->comments));
    json_object_set_new(json, "area_who", json_integer(region->area_who));
    json_object_set_new(json, "flags", flags_to_json_array(region->flags, area_region_flags));
    json_object_set_new(json, "place_flags", json_integer(region->rs_place_flags));

    if (region->rs_recall.wuid > 0) {
        json_object_set_new(recall, "wilds_uid", json_integer(region->rs_recall.wuid));
        json_object_set_new(recall, "x", json_integer(region->rs_recall.id[0]));
        json_object_set_new(recall, "y", json_integer(region->rs_recall.id[1]));
        json_object_set_new(recall, "z", json_integer(region->rs_recall.id[2]));
    } else {
        json_object_set_new(recall, "vnum", json_integer(region->rs_recall.id[0]));
    }
    json_object_set_new(json, "recall", recall);

    json_t *coords = json_object();
    json_object_set_new(coords, "x", json_integer(region->rs_x));
    json_object_set_new(coords, "y", json_integer(region->rs_y));
    json_object_set_new(coords, "land_x", json_integer(region->rs_land_x));
    json_object_set_new(coords, "land_y", json_integer(region->rs_land_y));
    json_object_set_new(json, "coordinates", coords);

    json_object_set_new(json, "airship_land", json_integer(region->rs_airship_land_spot));
    json_object_set_new(json, "post_office", json_integer(region->post_office));

    return json;
}

static void json_area_deserialize_region_data(json_t *json, AREA_REGION *region)
{
    if (!json || !region)
        return;

    region->uid = json_get_int_default(json, "uid", 0);

    free_string(region->name);
    region->name = str_dup(json_get_string_default(json, "name", ""));
    free_string(region->description);
    region->description = str_dup(json_get_string_default(json, "description", ""));
    free_string(region->comments);
    region->comments = str_dup(json_get_string_default(json, "comments", ""));

    region->area_who = json_get_int_default(json, "area_who", AREA_BLANK);

    json_t *flags = json_object_get(json, "flags");
    if (flags && json_is_array(flags))
        region->flags = json_array_to_flags(flags, area_region_flags);
    else
        region->flags = json_get_int_default(json, "flags", 0);

    region->rs_place_flags = json_get_int_default(json, "place_flags", PLACE_NOWHERE);

    rs_location_clear(&region->rs_recall);
    json_t *recall = json_object_get(json, "recall");
    if (recall) {
        if (json_object_get(recall, "wilds_uid")) {
            region->rs_recall.wuid = json_get_int_default(recall, "wilds_uid", 0);
            region->rs_recall.id[0] = json_get_int_default(recall, "x", 0);
            region->rs_recall.id[1] = json_get_int_default(recall, "y", 0);
            region->rs_recall.id[2] = json_get_int_default(recall, "z", 0);
        } else {
            region->rs_recall.wuid = 0;
            region->rs_recall.id[0] = json_get_int_default(recall, "vnum", 0);
        }
    }

    region->rs_x = -1;
    region->rs_y = -1;
    region->rs_land_x = -1;
    region->rs_land_y = -1;
    json_t *coords = json_object_get(json, "coordinates");
    if (coords) {
        region->rs_x = json_get_int_default(coords, "x", -1);
        region->rs_y = json_get_int_default(coords, "y", -1);
        region->rs_land_x = json_get_int_default(coords, "land_x", -1);
        region->rs_land_y = json_get_int_default(coords, "land_y", -1);
    }

    region->rs_airship_land_spot = json_get_int_default(json, "airship_land", 0);
    region->post_office = json_get_int_default(json, "post_office", 0);
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
    json_t *regions = json_array();
    
    /* Basic info */
    json_object_set_new(root, "uid", json_integer(area->uid));
    json_object_set_new(root, "name", json_string_safe(area->name));
    json_object_set_new(root, "filename", json_string_safe(area->file_name));
    
    /* Vnum range */
    json_object_set_new(vnums, "min", json_integer(area->min_vnum));
    json_object_set_new(vnums, "max", json_integer(area->max_vnum));
    json_object_set_new(root, "vnums", vnums);
    
    /* Metadata */
    json_object_set_new(metadata, "builders", json_string(area->builders ? area->builders : "None"));
    json_object_set_new(metadata, "credits", json_string_safe(area->credits));
    json_object_set_new(metadata, "security", json_integer(area->security));
    json_object_set_new(metadata, "area_who", json_integer(area->area_who));
    
    json_t *levels = json_object();
    json_object_set_new(levels, "min", json_integer(area->low_range));
    json_object_set_new(levels, "max", json_integer(area->high_range));
    json_object_set_new(metadata, "levels", levels);
    json_object_set_new(root, "metadata", metadata);
    
    /* Flags */
    json_object_set_new(root, "flags", flags_to_json_array(area->area_flags, area_flags));
    if (area->place_flags) {
        json_object_set_new(root, "place_flags", json_integer(area->place_flags));
    }
    if (area->open) {
        json_object_set_new(root, "open", json_boolean(area->open));
    }
    if (area->min_level || area->max_level) {
        json_t *olc_levels = json_object();
        json_object_set_new(olc_levels, "min", json_integer(area->min_level));
        json_object_set_new(olc_levels, "max", json_integer(area->max_level));
        json_object_set_new(root, "olc_levels", olc_levels);
    }
    
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
        /* Use widevnum format for room recall */
        ROOM_INDEX_DATA *recall_room = get_room_index_global(area->recall.id[0]);
        if (recall_room) {
            json_object_set_new(recall, "vnum", json_string(widevnum_string_room(recall_room, NULL)));
        } else {
            json_object_set_new(recall, "vnum", json_integer(area->recall.id[0]));
        }
    }
    json_object_set_new(root, "recall", recall);
    
    /* Other settings */
    json_object_set_new(root, "wilds_uid", json_integer(area->wilds_uid));
    json_object_set_new(root, "repop", json_integer(area->repop));
    json_object_set_new(root, "post_office", json_integer(area->post_office_load.vnum));
    json_object_set_new(root, "airship_land", json_integer(area->airship_land_load.vnum));
    
    /* Descriptions */
    json_object_set_new(root, "description", json_string_safe(area->description));
    json_object_set_new(root, "comments", json_string_safe(area->comments));
    json_object_set_new(root, "notes", json_string_safe(area->notes));
    
    /* OLC Point Boosts */
    if (area->points) {
        json_t *boosts = json_array();
        OLC_POINT_BOOST *boost;
        for (boost = area->points; boost; boost = boost->next) {
            json_t *b = json_object();
            json_object_set_new(b, "category", json_integer(boost->category));
            json_object_set_new(b, "usage", json_integer(boost->usage));
            json_object_set_new(b, "imp", json_integer(boost->imp));
            json_object_set_new(b, "area", json_integer(boost->area));
            json_array_append_new(boosts, b);
        }
        json_object_set_new(root, "olc_point_boosts", boosts);
    }

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

    json_t *default_region = json_area_serialize_region_data(&area->region);
    if (default_region)
        json_object_set_new(root, "default_region", default_region);

    if (area->regions) {
        ITERATOR it;
        AREA_REGION *region;
        iterator_start(&it, area->regions);
        while ((region = (AREA_REGION *)iterator_nextdata(&it))) {
            json_t *region_json = json_area_serialize_region_data(region);
            if (region_json)
                json_array_append_new(regions, region_json);
        }
        iterator_stop(&it);
    }
    json_object_set_new(root, "regions", regions);
    json_object_set_new(root, "top_region_uid", json_integer(area->top_region_uid));

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
    area->place_flags = json_get_int_default(json, "place_flags", 0);
    area->open = json_get_bool_default(json, "open", false);
    
    /* OLC level range (separate from display range) */
    {
        json_t *olc_levels = json_object_get(json, "olc_levels");
        if (olc_levels) {
            area->min_level = json_get_int_default(olc_levels, "min", 0);
            area->max_level = json_get_int_default(olc_levels, "max", 0);
        }
    }
    
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
            /* Handle widevnum string or legacy integer */
            json_t *vnum_json = json_object_get(recall, "vnum");
            if (vnum_json && json_is_string(vnum_json)) {
                WNUM_LOAD wload;
                if (parse_widevnum_load(json_string_value(vnum_json), &wload)) {
                    area->recall.id[0] = wload.vnum;
                }
            } else {
                area->recall.id[0] = json_get_int_default(recall, "vnum", 0);
            }
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
    area->post_office_load.vnum = json_get_int_default(json, "post_office", 0);
    area->airship_land_load.vnum = json_get_int_default(json, "airship_land", 0);
    
    /* Descriptions */
    area->description = str_dup(json_get_string_default(json, "description", ""));
    area->comments = str_dup(json_get_string_default(json, "comments", ""));
    area->notes = str_dup(json_get_string_default(json, "notes", ""));

    json_t *default_region = json_object_get(json, "default_region");
    if (default_region)
        json_area_deserialize_region_data(default_region, &area->region);

    area->region.area = area;
    area->region.uid = 0;
    area->region.valid = true;
    area->top_region_uid = json_get_int_default(json, "top_region_uid", 0);

    json_t *regions = json_object_get(json, "regions");
    if (regions && json_is_array(regions)) {
        size_t index;
        json_t *region_json;
        json_array_foreach(regions, index, region_json) {
            AREA_REGION *region = new_area_region();
            if (!region)
                continue;

            json_area_deserialize_region_data(region_json, region);
            region->area = area;
            region->valid = true;

            if (region->uid > area->top_region_uid)
                area->top_region_uid = region->uid;

            list_appendlink(area->regions, region);
        }
    }
    
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

    /* OLC Point Boosts */
    {
        json_t *boosts = json_object_get(json, "olc_point_boosts");
        if (boosts && json_is_array(boosts)) {
            OLC_POINT_BOOST *last = NULL;
            size_t bidx;
            json_t *belem;
            json_array_foreach(boosts, bidx, belem) {
                OLC_POINT_BOOST *boost = new_olc_point_boost();
                boost->category = json_get_int_default(belem, "category", 0);
                boost->usage = json_get_int_default(belem, "usage", 0);
                boost->imp = json_get_int_default(belem, "imp", 0);
                boost->area = json_get_int_default(belem, "area", 0);
                JSON_APPEND_LINK(area->points, last, boost);
            }
        }
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

    /* Destination - save as widevnum string */
    if (exit->u1.to_room && exit->u1.to_room->vnum > 0) {
        json_object_set_new(obj, "destination", json_string(widevnum_string_room(exit->u1.to_room, NULL)));
    } else if (!IS_SET(exit->exit_info, EX_VLINK) && exit->from_room) {
        /* Exit has no resolved destination and isn't a wilderness vlink.
         * This likely indicates data corruption - log it so we can investigate. */
        log_stringf("Warning: exit %s from room %ld (%s) has no destination - "
                    "exit data may be corrupt",
                    dir_name[exit->orig_door],
                    exit->from_room->vnum,
                    exit->from_room->name ? exit->from_room->name : "unknown");
    }

    /* Keywords and descriptions */
    if (exit->keyword && exit->keyword[0] != '\0')
        json_object_set_new(obj, "keywords", json_string(exit->keyword));
    if (exit->short_desc && exit->short_desc[0] != '\0')
        json_object_set_new(obj, "description", json_string(exit->short_desc));
    if (exit->long_desc && exit->long_desc[0] != '\0')
        json_object_set_new(obj, "long_description", json_string(exit->long_desc));

    /* Lock/key/pick fields - use widevnum for key */
    if (exit->door.lock.key_wnum.pArea && exit->door.lock.key_wnum.vnum > 0) {
        OBJ_INDEX_DATA *key_obj = get_obj_index(exit->door.lock.key_wnum.pArea, exit->door.lock.key_wnum.vnum);
        if (key_obj) {
            json_object_set_new(obj, "key_vnum", json_string(widevnum_string_object(key_obj, NULL)));
        } else {
            /* Fallback to bare vnum if object not found */
            json_object_set_new(obj, "key_vnum", json_integer(exit->door.lock.key_wnum.vnum));
        }
    }
    json_object_set_new(obj, "lock_flags", flags_to_json_array(exit->door.lock.flags, lock_flags));
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
    
    /* Destination - extract raw vnum and area UID for fix_rooms() to resolve later.
     * We use parse_widevnum_load() instead of parse_widevnum() because during area
     * loading the current area isn't in the global list yet, so parse_widevnum can't
     * resolve self-referencing exits (e.g., "955#11484" when area 955 is being loaded). */
    json_t *dest_json = json_object_get(json, "destination");
    long to_vnum = 0;
    long to_area_uid = 0;

    if (dest_json && json_is_string(dest_json)) {
        WNUM_LOAD wload;
        if (parse_widevnum_load(json_string_value(dest_json), &wload)) {
            to_vnum = wload.vnum;
            to_area_uid = wload.auid;
        }
    } else {
        /* Legacy separate fields */
        to_vnum = json_get_int_default(json, "destination_vnum", 0);
        to_area_uid = json_get_int_default(json, "destination_area_uid", 0);

        /* Even older legacy field names */
        if (to_vnum == 0) to_vnum = json_get_int_default(json, "to_vnum", 0);
        if (to_area_uid == 0) to_area_uid = json_get_int_default(json, "to_area", 0);
    }

    if (to_vnum > 0) {
        exit->u1.vnum = to_vnum;
        /* Store destination area UID for cross-area exit linking */
        if (to_area_uid > 0) {
            exit->wilds.area_uid = to_area_uid;
        }
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

    /* Lock/key/pick fields - use parse_widevnum_load (same reason as destination) */
    json_t *key_json = json_object_get(json, "key_vnum");
    if (key_json && json_is_string(key_json)) {
        WNUM_LOAD wload;
        if (parse_widevnum_load(json_string_value(key_json), &wload)) {
            exit->door.lock.key_load = wload;
        }
    } else {
        exit->door.lock.key_load.vnum = json_get_int_default(json, "key_vnum", 0);
    }
    
    /* Load lock_flags - try as array first, fall back to integer for legacy */
    json_t *lock_flags_json = json_object_get(json, "lock_flags");
    if (lock_flags_json && json_is_array(lock_flags_json)) {
        exit->door.lock.flags = json_array_to_flags(lock_flags_json, lock_flags);
    } else {
        exit->door.lock.flags = json_get_int_default(json, "lock_flags", 0);
    }
    
    exit->door.lock.pick_chance = json_get_int_default(json, "pick_chance", 100);

    /* Flags */
    exit->exit_info = json_array_to_flags(json_object_get(json, "flags"), exit_flags);

    return exit;
}

static json_t *json_area_serialize_reputation(REPUTATION_INDEX_DATA *reputation, AREA_DATA *area)
{
    (void)area;

    if (!reputation)
        return NULL;

    json_t *json = json_object();
    if (!json)
        return NULL;

    json_object_set_new(json, "vnum", json_integer(reputation->vnum));
    json_object_set_new(json, "name", json_string_safe(reputation->name));
    json_object_set_new(json, "description", json_string_safe(reputation->description));
    json_object_set_new(json, "comments", json_string_safe(reputation->comments));
    json_object_set_new(json, "created_by", json_string_safe(reputation->created_by));
    json_object_set_new(json, "flags", json_integer(reputation->flags));
    json_object_set_new(json, "initial_rank", json_integer(reputation->initial_rank));
    json_object_set_new(json, "initial_reputation", json_integer(reputation->initial_reputation));

    if (reputation->token)
    {
        json_object_set_new(json, "token", json_string(widevnum_string_token(reputation->token, NULL)));
    }
    else if (reputation->token_load.vnum > 0)
    {
        if (reputation->token_load.auid > 0)
            json_object_set_new(json, "token", json_string(formatf("%ld#%ld", reputation->token_load.auid, reputation->token_load.vnum)));
        else
            json_object_set_new(json, "token", json_integer(reputation->token_load.vnum));
    }

    json_t *ranks = json_array();
    if (reputation->ranks)
    {
        ITERATOR it;
        REPUTATION_INDEX_RANK_DATA *rank;
        iterator_start(&it, reputation->ranks);
        while ((rank = (REPUTATION_INDEX_RANK_DATA *)iterator_nextdata(&it)))
        {
            json_t *rank_json = json_object();
            json_object_set_new(rank_json, "uid", json_integer(rank->uid));
            json_object_set_new(rank_json, "ordinal", json_integer(rank->ordinal));
            json_object_set_new(rank_json, "name", json_string_safe(rank->name));
            json_object_set_new(rank_json, "description", json_string_safe(rank->description));
            json_object_set_new(rank_json, "comments", json_string_safe(rank->comments));
            json_object_set_new(rank_json, "capacity", json_integer(rank->capacity));
            json_object_set_new(rank_json, "flags", json_integer(rank->flags));
            json_object_set_new(rank_json, "set", json_integer(rank->set));

            char color[2] = { rank->color ? rank->color : 'Y', '\0' };
            json_object_set_new(rank_json, "color", json_string(color));

            json_array_append_new(ranks, rank_json);
        }
        iterator_stop(&it);
    }
    json_object_set_new(json, "ranks", ranks);

    return json;
}

static REPUTATION_INDEX_DATA *json_area_deserialize_reputation(json_t *json, AREA_DATA *area)
{
    if (!json || !area)
        return NULL;

    REPUTATION_INDEX_DATA *reputation = alloc_perm(sizeof(REPUTATION_INDEX_DATA));
    if (!reputation)
        return NULL;

    memset(reputation, 0, sizeof(*reputation));
    reputation->valid = true;
    reputation->area = area;
    reputation->name = str_dup(json_get_string_default(json, "name", ""));
    reputation->description = str_dup(json_get_string_default(json, "description", ""));
    reputation->comments = str_dup(json_get_string_default(json, "comments", ""));
    reputation->created_by = str_dup(json_get_string_default(json, "created_by", ""));
    reputation->ranks = list_create(false);

    reputation->vnum = json_get_int_default(json, "vnum", 0);
    reputation->flags = json_get_int_default(json, "flags", 0);
    reputation->initial_rank = json_get_int_default(json, "initial_rank", 1);
    reputation->initial_reputation = json_get_int_default(json, "initial_reputation", 0);

    json_t *token = json_object_get(json, "token");
    if (token)
    {
        if (json_is_string(token))
        {
            WNUM_LOAD load;
            if (parse_widevnum_load(json_string_value(token), &load))
            {
                reputation->token_load = load;
            }
        }
        else if (json_is_integer(token))
        {
            reputation->token_load.auid = area->uid;
            reputation->token_load.vnum = json_integer_value(token);
        }
    }

    json_t *ranks = json_object_get(json, "ranks");
    if (ranks && json_is_array(ranks))
    {
        size_t index;
        json_t *rank_json;
        json_array_foreach(ranks, index, rank_json)
        {
            REPUTATION_INDEX_RANK_DATA *rank = alloc_perm(sizeof(REPUTATION_INDEX_RANK_DATA));
            if (!rank)
                continue;

            memset(rank, 0, sizeof(*rank));
            rank->valid = true;
            rank->uid = json_get_int_default(rank_json, "uid", index + 1);
            rank->ordinal = json_get_int_default(rank_json, "ordinal", list_size(reputation->ranks) + 1);
            rank->name = str_dup(json_get_string_default(rank_json, "name", ""));
            rank->description = str_dup(json_get_string_default(rank_json, "description", ""));
            rank->comments = str_dup(json_get_string_default(rank_json, "comments", ""));
            rank->capacity = json_get_int_default(rank_json, "capacity", 1);
            rank->flags = json_get_int_default(rank_json, "flags", 0);
            rank->set = json_get_int_default(rank_json, "set", 0);

            const char *color = json_get_string_default(rank_json, "color", "Y");
            rank->color = (color && color[0]) ? color[0] : 'Y';

            if (rank->uid > reputation->top_rank_uid)
                reputation->top_rank_uid = rank->uid;

            list_appendlink(reputation->ranks, rank);
        }
    }

    return reputation;
}

static json_t *json_area_serialize_quest_v2(QUEST_INDEX_V2_DATA *quest_index_v2, AREA_DATA *area)
{
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *pool_entry;
    QUEST_REWARD_INDEX_V2_DATA *reward;
    json_t *json;
    json_t *stages;
    json_t *rewards;

    if (!quest_index_v2 || !area || quest_index_v2->area != area)
        return NULL;

    json = json_object();
    if (!json)
        return NULL;

    json_object_set_new(json, "vnum", json_integer(quest_index_v2->vnum));
    json_object_set_new(json, "name", json_string_safe(quest_index_v2->name));
    json_object_set_new(json, "description", json_string_safe(quest_index_v2->description));
    json_object_set_new(json, "quest_class", json_integer(quest_index_v2->quest_class));
    json_object_set_new(json, "quest_type", json_integer(quest_index_v2->quest_type));
    json_object_set_new(json, "category", json_integer(quest_index_v2->category));
    json_object_set_new(json, "target_scope", json_integer(quest_index_v2->target_scope));
    json_object_set_new(json, "flags", json_integer(quest_index_v2->flags));
    json_object_set_new(json, "repeat_policy", json_integer(quest_index_v2->repeat_policy));
    json_object_set_new(json, "allowance_cost", json_integer(quest_index_v2->allowance_cost));
    json_object_set_new(json, "entry_stage_id", json_integer(quest_index_v2->entry_stage_id));
    json_object_set_new(json, "seed_policy", json_integer(quest_index_v2->seed_policy));
    json_object_set_new(json, "fixed_seed", json_integer((json_int_t)quest_index_v2->fixed_seed));
    json_object_set_new(json, "enabled", quest_index_v2->enabled ? json_true() : json_false());
    if (quest_index_v2->progs)
    {
        json_t *progs_json = json_area_serialize_progs(quest_index_v2->progs, area);
        if (progs_json)
            json_object_set_new(json, "progs", progs_json);
    }
    if (quest_index_v2->index_vars)
    {
        json_t *index_vars = json_area_serialize_index_vars(quest_index_v2->index_vars, area);
        if (index_vars)
            json_object_set_new(json, "index_vars", index_vars);
    }

    stages = json_array();
    for (stage = quest_index_v2->stages; stage != NULL; stage = stage->next)
    {
        json_t *stage_json = json_object();
        json_t *objectives = json_array();

        json_object_set_new(stage_json, "id", json_integer(stage->id));
        json_object_set_new(stage_json, "name", json_string_safe(stage->name));
        json_object_set_new(stage_json, "description", json_string_safe(stage->description));
        json_object_set_new(stage_json, "completion_mode", json_integer(stage->completion_mode));
        json_object_set_new(stage_json, "stage_source", json_integer(stage->stage_source));
        json_object_set_new(stage_json, "auto_commence", stage->auto_commence ? json_true() : json_false());
        json_object_set_new(stage_json, "next_stage_id", json_integer(stage->next_stage_id));
        json_object_set_new(stage_json, "generator_profile", json_string_safe(stage->generator_profile));
        json_object_set_new(stage_json, "generator_salt", json_integer((json_int_t)stage->generator_salt));
        json_object_set_new(stage_json, "on_enter_script", json_string_safe(stage->on_enter_script));
        json_object_set_new(stage_json, "on_exit_script", json_string_safe(stage->on_exit_script));

        for (objective = stage->objectives; objective != NULL; objective = objective->next)
        {
            json_t *objective_json = json_object();
            json_t *pool_entries = json_array();
            json_t *target_load = json_object();
            json_t *destination_load = json_object();
            json_t *target_token_load = json_object();
            json_t *destination_token_load = json_object();

            json_object_set_new(objective_json, "id", json_integer(objective->id));
            json_object_set_new(objective_json, "objective_type", json_integer(objective->objective_type));
            json_object_set_new(objective_json, "quantity", json_integer(objective->quantity));
            json_object_set_new(objective_json, "required_count", json_integer(objective->required_count));

            json_object_set_new(target_load, "auid", json_integer(objective->target_load.auid));
            json_object_set_new(target_load, "vnum", json_integer(objective->target_load.vnum));
            json_object_set_new(objective_json, "target_load", target_load);

            json_object_set_new(destination_load, "auid", json_integer(objective->destination_load.auid));
            json_object_set_new(destination_load, "vnum", json_integer(objective->destination_load.vnum));
            json_object_set_new(objective_json, "destination_load", destination_load);

            json_object_set_new(target_token_load, "auid", json_integer(objective->target_token_load.auid));
            json_object_set_new(target_token_load, "vnum", json_integer(objective->target_token_load.vnum));
            json_object_set_new(objective_json, "target_token_load", target_token_load);

            json_object_set_new(destination_token_load, "auid", json_integer(objective->destination_token_load.auid));
            json_object_set_new(destination_token_load, "vnum", json_integer(objective->destination_token_load.vnum));
            json_object_set_new(objective_json, "destination_token_load", destination_token_load);

            json_object_set_new(objective_json, "target_ref_stage_id", json_integer(objective->target_ref_stage_id));
            json_object_set_new(objective_json, "target_ref_objective_id", json_integer(objective->target_ref_objective_id));
            json_object_set_new(objective_json, "target_ref_name", json_string_safe(objective->target_ref_name));
            json_object_set_new(objective_json, "target_variable_name", json_string_safe(objective->target_variable_name));
            json_object_set_new(objective_json, "target_token_ref_name", json_string_safe(objective->target_token_ref_name));
            json_object_set_new(objective_json, "target_token_variable_name", json_string_safe(objective->target_token_variable_name));
            json_object_set_new(objective_json, "target_mode", json_integer(objective->target_mode));
            json_object_set_new(objective_json, "destination_ref_name", json_string_safe(objective->destination_ref_name));
            json_object_set_new(objective_json, "destination_variable_name", json_string_safe(objective->destination_variable_name));
            json_object_set_new(objective_json, "destination_token_ref_name", json_string_safe(objective->destination_token_ref_name));
            json_object_set_new(objective_json, "destination_token_variable_name", json_string_safe(objective->destination_token_variable_name));
            json_object_set_new(objective_json, "target_tag", json_string_safe(objective->target_tag));
            json_object_set_new(objective_json, "description", json_string_safe(objective->description));
            json_object_set_new(objective_json, "optional", objective->optional ? json_true() : json_false());
            json_object_set_new(objective_json, "strict_target", objective->strict_target ? json_true() : json_false());

            for (pool_entry = objective->pool_entries; pool_entry != NULL; pool_entry = pool_entry->next)
            {
                json_t *pool_json = json_object();
                json_t *pool_target_load = json_object();

                json_object_set_new(pool_json, "id", json_integer(pool_entry->id));
                json_object_set_new(pool_json, "weight", json_integer(pool_entry->weight));
                json_object_set_new(pool_target_load, "auid", json_integer(pool_entry->target_load.auid));
                json_object_set_new(pool_target_load, "vnum", json_integer(pool_entry->target_load.vnum));
                json_object_set_new(pool_json, "target_load", pool_target_load);
                json_array_append_new(pool_entries, pool_json);
            }

            json_object_set_new(objective_json, "pool_entries", pool_entries);
            json_array_append_new(objectives, objective_json);
        }

        json_object_set_new(stage_json, "objectives", objectives);
        json_array_append_new(stages, stage_json);
    }

    rewards = json_array();
    for (reward = quest_index_v2->rewards; reward != NULL; reward = reward->next)
    {
        json_t *reward_json = json_object();
        json_t *reward_target_load = json_object();

        json_object_set_new(reward_json, "reward_type", json_integer(reward->reward_type));
        json_object_set_new(reward_json, "amount", json_integer(reward->amount));
        json_object_set_new(reward_target_load, "auid", json_integer(reward->target_load.auid));
        json_object_set_new(reward_target_load, "vnum", json_integer(reward->target_load.vnum));
        json_object_set_new(reward_json, "target_load", reward_target_load);
        json_object_set_new(reward_json, "currency", json_string_safe(reward->currency));
        json_object_set_new(reward_json, "script", json_string_safe(reward->script));

        json_array_append_new(rewards, reward_json);
    }

    json_object_set_new(json, "stages", stages);
    json_object_set_new(json, "rewards", rewards);

    return json;
}

static QUEST_INDEX_V2_DATA *json_area_deserialize_quest_v2(json_t *json, AREA_DATA *area)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    json_t *stages;
    json_t *rewards;
    size_t stage_index;
    size_t reward_index;
    json_t *stage_json;
    json_t *reward_json;

    if (!json || !area)
        return NULL;

    quest_index_v2 = new_quest_index_v2();
    if (!quest_index_v2)
        return NULL;

    quest_index_v2->area = area;
    quest_index_v2->vnum = json_get_int_default(json, "vnum", 0);
    if (quest_index_v2->vnum < 1)
    {
        free_quest_index_v2(quest_index_v2);
        return NULL;
    }

    free_string(quest_index_v2->name);
    quest_index_v2->name = str_dup(json_get_string_default(json, "name", "unnamed quest"));

    free_string(quest_index_v2->description);
    quest_index_v2->description = str_dup(json_get_string_default(json, "description", ""));

    quest_index_v2->quest_class = json_get_int_default(json, "quest_class", quest_index_v2->quest_class);
    quest_index_v2->quest_type = json_get_int_default(json, "quest_type", quest_index_v2->quest_type);
    quest_index_v2->category = json_get_int_default(json, "category", quest_index_v2->category);
    quest_index_v2->target_scope = json_get_int_default(json, "target_scope", quest_index_v2->target_scope);
    quest_index_v2->flags = json_get_int_default(json, "flags", quest_index_v2->flags);
    quest_index_v2->repeat_policy = json_get_int_default(json, "repeat_policy", quest_index_v2->repeat_policy);
    quest_index_v2->allowance_cost = json_get_int_default(json, "allowance_cost", quest_index_v2->allowance_cost);
    quest_index_v2->entry_stage_id = json_get_int_default(json, "entry_stage_id", quest_index_v2->entry_stage_id);
    quest_index_v2->seed_policy = json_get_int_default(json, "seed_policy", quest_index_v2->seed_policy);
    quest_index_v2->fixed_seed = (unsigned long long)json_get_int_default(json, "fixed_seed", 0);
    quest_index_v2->enabled = json_get_bool_default(json, "enabled", true);
    quest_index_v2->progs = json_area_deserialize_progs(json_object_get(json, "progs"), area, PRG_QPROG);
    quest_index_v2->index_vars = json_area_deserialize_index_vars(json_object_get(json, "index_vars"), area);

    stages = json_object_get(json, "stages");
    if (stages && json_is_array(stages))
    {
        QUEST_STAGE_INDEX_V2_DATA *last_stage = NULL;

        json_array_foreach(stages, stage_index, stage_json)
        {
            QUEST_STAGE_INDEX_V2_DATA *stage = new_quest_stage_index_v2();
            json_t *objectives;
            size_t objective_index;
            json_t *objective_json;
            QUEST_OBJECTIVE_INDEX_V2_DATA *last_objective = NULL;

            stage->id = json_get_int_default(stage_json, "id", stage_index + 1);

            free_string(stage->name);
            stage->name = str_dup(json_get_string_default(stage_json, "name", ""));

            free_string(stage->description);
            stage->description = str_dup(json_get_string_default(stage_json, "description", ""));

            stage->completion_mode = json_get_int_default(stage_json, "completion_mode", stage->completion_mode);
            stage->stage_source = json_get_int_default(stage_json, "stage_source", stage->stage_source);
            stage->auto_commence = json_get_bool_default(stage_json, "auto_commence", stage->auto_commence);
            stage->next_stage_id = json_get_int_default(stage_json, "next_stage_id", stage->next_stage_id);

            free_string(stage->generator_profile);
            stage->generator_profile = str_dup(json_get_string_default(stage_json, "generator_profile", ""));
            stage->generator_salt = (unsigned long long)json_get_int_default(stage_json, "generator_salt", 0);

            free_string(stage->on_enter_script);
            stage->on_enter_script = str_dup(json_get_string_default(stage_json, "on_enter_script", ""));

            free_string(stage->on_exit_script);
            stage->on_exit_script = str_dup(json_get_string_default(stage_json, "on_exit_script", ""));

            objectives = json_object_get(stage_json, "objectives");
            if (objectives && json_is_array(objectives))
            {
                json_array_foreach(objectives, objective_index, objective_json)
                {
                    QUEST_OBJECTIVE_INDEX_V2_DATA *objective = new_quest_objective_index_v2();
                    json_t *target_load;
                    json_t *destination_load;
                    json_t *target_token_load;
                    json_t *destination_token_load;
                    json_t *pool_entries;
                    size_t pool_index;
                    json_t *pool_json;
                    QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *last_pool = NULL;

                    objective->id = json_get_int_default(objective_json, "id", objective_index + 1);
                    objective->objective_type = json_get_int_default(objective_json, "objective_type", objective->objective_type);
                    objective->quantity = json_get_int_default(objective_json, "quantity", objective->quantity);
                    objective->required_count = json_get_int_default(objective_json, "required_count", objective->required_count);
                    objective->target_ref_stage_id = json_get_int_default(objective_json, "target_ref_stage_id", 0);
                    objective->target_ref_objective_id = json_get_int_default(objective_json, "target_ref_objective_id", 0);

                    free_string(objective->target_ref_name);
                    objective->target_ref_name = str_dup(json_get_string_default(objective_json, "target_ref_name", ""));

                    free_string(objective->target_variable_name);
                    objective->target_variable_name = str_dup(json_get_string_default(objective_json, "target_variable_name", ""));

                    free_string(objective->target_token_ref_name);
                    objective->target_token_ref_name = str_dup(json_get_string_default(objective_json, "target_token_ref_name", ""));

                    free_string(objective->target_token_variable_name);
                    objective->target_token_variable_name = str_dup(json_get_string_default(objective_json, "target_token_variable_name", ""));

                    objective->target_mode = json_get_int_default(objective_json, "target_mode", objective->target_mode);

                    free_string(objective->destination_ref_name);
                    objective->destination_ref_name = str_dup(json_get_string_default(objective_json, "destination_ref_name", ""));

                    free_string(objective->destination_variable_name);
                    objective->destination_variable_name = str_dup(json_get_string_default(objective_json, "destination_variable_name", ""));

                    free_string(objective->destination_token_ref_name);
                    objective->destination_token_ref_name = str_dup(json_get_string_default(objective_json, "destination_token_ref_name", ""));

                    free_string(objective->destination_token_variable_name);
                    objective->destination_token_variable_name = str_dup(json_get_string_default(objective_json, "destination_token_variable_name", ""));

                    free_string(objective->target_tag);
                    objective->target_tag = str_dup(json_get_string_default(objective_json, "target_tag", ""));

                    free_string(objective->description);
                    objective->description = str_dup(json_get_string_default(objective_json, "description", ""));

                    objective->optional = json_get_bool_default(objective_json, "optional", objective->optional);
                    objective->strict_target = json_get_bool_default(objective_json, "strict_target", objective->strict_target);

                    target_load = json_object_get(objective_json, "target_load");
                    if (target_load && json_is_object(target_load))
                    {
                        objective->target_load.auid = json_get_int_default(target_load, "auid", 0);
                        objective->target_load.vnum = json_get_int_default(target_load, "vnum", 0);
                    }

                    destination_load = json_object_get(objective_json, "destination_load");
                    if (destination_load && json_is_object(destination_load))
                    {
                        objective->destination_load.auid = json_get_int_default(destination_load, "auid", 0);
                        objective->destination_load.vnum = json_get_int_default(destination_load, "vnum", 0);
                    }

                    target_token_load = json_object_get(objective_json, "target_token_load");
                    if (target_token_load && json_is_object(target_token_load))
                    {
                        objective->target_token_load.auid = json_get_int_default(target_token_load, "auid", 0);
                        objective->target_token_load.vnum = json_get_int_default(target_token_load, "vnum", 0);
                    }

                    destination_token_load = json_object_get(objective_json, "destination_token_load");
                    if (destination_token_load && json_is_object(destination_token_load))
                    {
                        objective->destination_token_load.auid = json_get_int_default(destination_token_load, "auid", 0);
                        objective->destination_token_load.vnum = json_get_int_default(destination_token_load, "vnum", 0);
                    }

                    pool_entries = json_object_get(objective_json, "pool_entries");
                    if (pool_entries && json_is_array(pool_entries))
                    {
                        json_array_foreach(pool_entries, pool_index, pool_json)
                        {
                            QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *pool_entry = new_quest_objective_pool_entry_v2();
                            json_t *pool_target_load = json_object_get(pool_json, "target_load");

                            pool_entry->id = json_get_int_default(pool_json, "id", pool_index + 1);
                            pool_entry->weight = json_get_int_default(pool_json, "weight", pool_entry->weight);

                            if (pool_target_load && json_is_object(pool_target_load))
                            {
                                pool_entry->target_load.auid = json_get_int_default(pool_target_load, "auid", 0);
                                pool_entry->target_load.vnum = json_get_int_default(pool_target_load, "vnum", 0);
                            }

                            pool_entry->next = NULL;
                            if (!objective->pool_entries)
                                objective->pool_entries = pool_entry;
                            else
                                last_pool->next = pool_entry;
                            last_pool = pool_entry;
                        }
                    }

                    objective->next = NULL;
                    if (!stage->objectives)
                        stage->objectives = objective;
                    else
                        last_objective->next = objective;
                    last_objective = objective;
                }
            }

            stage->next = NULL;
            if (!quest_index_v2->stages)
                quest_index_v2->stages = stage;
            else
                last_stage->next = stage;
            last_stage = stage;
        }
    }

    rewards = json_object_get(json, "rewards");
    if (rewards && json_is_array(rewards))
    {
        QUEST_REWARD_INDEX_V2_DATA *last_reward = NULL;

        json_array_foreach(rewards, reward_index, reward_json)
        {
            QUEST_REWARD_INDEX_V2_DATA *reward = new_quest_reward_index_v2();
            json_t *target_load = json_object_get(reward_json, "target_load");

            reward->reward_type = json_get_int_default(reward_json, "reward_type", reward->reward_type);
            reward->amount = json_get_int_default(reward_json, "amount", reward->amount);

            free_string(reward->currency);
            reward->currency = str_dup(json_get_string_default(reward_json, "currency", ""));

            free_string(reward->script);
            reward->script = str_dup(json_get_string_default(reward_json, "script", ""));

            if (target_load && json_is_object(target_load))
            {
                reward->target_load.auid = json_get_int_default(target_load, "auid", 0);
                reward->target_load.vnum = json_get_int_default(target_load, "vnum", 0);
            }

            reward->next = NULL;
            if (!quest_index_v2->rewards)
                quest_index_v2->rewards = reward;
            else
                last_reward->next = reward;
            last_reward = reward;
        }
    }

    if (!quest_index_v2_register(quest_index_v2))
    {
        free_quest_index_v2(quest_index_v2);
        return NULL;
    }

    return quest_index_v2;
}

/*
 * Deserialize a dungeon index from JSON
 */
DUNGEON_INDEX_DATA *json_area_deserialize_dungeon(json_t *json, AREA_DATA *area)
{
    if (!json || !area) return NULL;
    
    DUNGEON_INDEX_DATA *dungeon = alloc_perm(sizeof(DUNGEON_INDEX_DATA));
    if (!dungeon) return NULL;
    
    dungeon->area = area;
    dungeon->valid = true;
    
    /* Initialize string and list fields to safe defaults */
    dungeon->name = &str_empty[0];
    dungeon->description = &str_empty[0];
    dungeon->comments = &str_empty[0];
    dungeon->zone_out = &str_empty[0];
    dungeon->zone_out_portal = &str_empty[0];
    dungeon->zone_out_mount = &str_empty[0];
    dungeon->floors = list_create(false);
    
    // Basic info
    dungeon->vnum = json_get_int_default(json, "vnum", 0);
    dungeon->name = str_dup(json_get_string_default(json, "name", "Unnamed Dungeon"));
    dungeon->description = str_dup(json_get_string_default(json, "description", ""));
    
    const char *comments = json_get_string_default(json, "comments", "");
    if (comments && comments[0] != '\0')
        dungeon->comments = str_dup(comments);
    
    // Properties
    dungeon->area_who = json_get_int_default(json, "area_who", 0);
    dungeon->repop = json_get_int_default(json, "repop", 0);
    dungeon->flags = json_get_int_default(json, "flags", 0);
    
    // Group management
    dungeon->min_group = json_get_int_default(json, "min_group", 0);
    dungeon->max_group = json_get_int_default(json, "max_group", 0);
    dungeon->max_players = json_get_int_default(json, "max_players", 0);
    dungeon->death_release = json_get_int_default(json, "death_release", 0);
    dungeon->idle_timeout = json_get_int_default(json, "idle_timeout", 0);
    
    // Zone out strings
    const char *zone_out = json_get_string_default(json, "zone_out", "");
    if (zone_out && zone_out[0] != '\0')
        dungeon->zone_out = str_dup(zone_out);
    
    const char *portal_out = json_get_string_default(json, "portal_out", "");
    if (portal_out && portal_out[0] != '\0')
        dungeon->zone_out_portal = str_dup(portal_out);
    
    const char *mount_out = json_get_string_default(json, "mount_out", "");
    if (mount_out && mount_out[0] != '\0')
        dungeon->zone_out_mount = str_dup(mount_out);
    
    // Entry room - store as WNUM_LOAD for later resolution
    // Handle widevnum string or legacy object with area_uid/vnum
    json_t *entry_room = json_object_get(json, "entry_room");
    if (entry_room) {
        if (json_is_string(entry_room)) {
            WNUM_LOAD wload;
            if (parse_widevnum_load(json_string_value(entry_room), &wload)) {
                dungeon->entry_ref.load.auid = wload.auid;
                dungeon->entry_ref.load.vnum = wload.vnum;
            }
        } else if (json_is_object(entry_room)) {
            dungeon->entry_ref.load.auid = json_get_int_default(entry_room, "area_uid", 0);
            dungeon->entry_ref.load.vnum = json_get_int_default(entry_room, "vnum", 0);
        } else if (json_is_integer(entry_room)) {
            dungeon->entry_ref.load.auid = 0;
            dungeon->entry_ref.load.vnum = json_integer_value(entry_room);
        }
    }
    dungeon->entry_room = NULL;  // Will be resolved in fix pass

    // Exit room - store as WNUM_LOAD for later resolution
    json_t *exit_room = json_object_get(json, "exit_room");
    if (exit_room) {
        if (json_is_string(exit_room)) {
            WNUM_LOAD wload;
            if (parse_widevnum_load(json_string_value(exit_room), &wload)) {
                dungeon->exit_ref.load.auid = wload.auid;
                dungeon->exit_ref.load.vnum = wload.vnum;
            }
        } else if (json_is_object(exit_room)) {
            dungeon->exit_ref.load.auid = json_get_int_default(exit_room, "area_uid", 0);
            dungeon->exit_ref.load.vnum = json_get_int_default(exit_room, "vnum", 0);
        } else if (json_is_integer(exit_room)) {
            dungeon->exit_ref.load.auid = 0;
            dungeon->exit_ref.load.vnum = json_integer_value(exit_room);
        }
    }
    dungeon->exit_room = NULL;  // Will be resolved in fix pass
    
    // Floors - list of blueprint references (store as WNUM_LOAD, will be resolved to pointers later)
    json_t *floors = json_object_get(json, "floors");
    if (floors && json_is_array(floors)) {
        size_t index;
        json_t *floor_val;
        
        json_array_foreach(floors, index, floor_val) {
            WNUM_LOAD *wload = alloc_perm(sizeof(WNUM_LOAD));
            if (json_is_string(floor_val)) {
                /* Widevnum string format: "auid#vnum" */
                if (!parse_widevnum_load(json_string_value(floor_val), wload)) {
                    continue;
                }
            } else if (json_is_integer(floor_val)) {
                /* Legacy: bare vnum, assume same area */
                wload->auid = area ? area->uid : 0;
                wload->vnum = json_integer_value(floor_val);
            } else {
                continue;
            }
            list_appendlink(dungeon->floors, wload);
        }
    }
    
    // Levels
    json_t *levels = json_object_get(json, "levels");
    if (levels && json_is_array(levels)) {
        dungeon->levels = list_create(false);
        size_t index;
        json_t *level_json;
        
        json_array_foreach(levels, index, level_json) {
            DUNGEON_INDEX_LEVEL_DATA *level = alloc_perm(sizeof(DUNGEON_INDEX_LEVEL_DATA));
            level->valid = true;
            level->mode = json_get_int_default(level_json, "mode", 0);
            level->floor = json_get_int_default(level_json, "floor", 0);
            
            // Weighted floors
            json_t *weighted_floors = json_object_get(level_json, "weighted_floors");
            if (weighted_floors && json_is_array(weighted_floors)) {
                level->weighted_floors = list_create(false);
                size_t wf_index;
                json_t *wf_json;
                
                json_array_foreach(weighted_floors, wf_index, wf_json) {
                    DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *wf = alloc_perm(sizeof(DUNGEON_INDEX_WEIGHTED_FLOOR_DATA));
                    wf->weight = json_get_int_default(wf_json, "weight", 0);
                    wf->floor = json_get_int_default(wf_json, "floor", 0);
                    list_appendlink(level->weighted_floors, wf);
                    level->total_weight += wf->weight;
                }
            }
            
            list_appendlink(dungeon->levels, level);
        }
    } else {
        dungeon->levels = list_create(false);
    }
    
    // Special rooms - handle widevnum strings or legacy integers
    json_t *special_rooms = json_object_get(json, "special_rooms");
    if (special_rooms && json_is_array(special_rooms)) {
        dungeon->special_rooms = list_create(false);
        size_t index;
        json_t *special_json;

        json_array_foreach(special_rooms, index, special_json) {
            DUNGEON_INDEX_SPECIAL_ROOM *special = alloc_perm(sizeof(DUNGEON_INDEX_SPECIAL_ROOM));
            special->valid = true;
            special->name = str_dup(json_get_string_default(special_json, "name", ""));
            special->level = json_get_int_default(special_json, "level", 0);
            /* Handle widevnum string or legacy integer */
            json_t *room_json = json_object_get(special_json, "room");
            if (room_json && json_is_string(room_json)) {
                WNUM_LOAD wload;
                if (parse_widevnum_load(json_string_value(room_json), &wload)) {
                    special->room = wload.vnum;
                }
            } else {
                special->room = json_get_int_default(special_json, "room", 0);
            }
            list_appendlink(dungeon->special_rooms, special);
        }
    } else {
        dungeon->special_rooms = list_create(false);
    }
    
    // Special exits - simplified, just create empty list for now since structure is complex
    dungeon->special_exits = list_create(false);
    
    // Dungeon progs
    dungeon->progs = json_area_deserialize_progs(json_object_get(json, "dungeon_progs"), area, PRG_DPROG);
    
    // Index vars
    dungeon->index_vars = NULL;
    json_t *index_vars = json_object_get(json, "index_vars");
    if (index_vars) {
        dungeon->index_vars = json_area_deserialize_index_vars(index_vars, area);
    }
    
    return dungeon;
}

/*
 * Deserialize a ship index from JSON
 */
SHIP_INDEX_DATA *json_area_deserialize_ship(json_t *json, AREA_DATA *area)
{
    if (!json || !area) return NULL;
    
    SHIP_INDEX_DATA *ship = alloc_perm(sizeof(SHIP_INDEX_DATA));
    if (!ship) return NULL;
    
    ship->area = area;
    
    // Basic info
    ship->vnum = json_get_int_default(json, "vnum", 0);
    ship->name = str_dup(json_get_string_default(json, "name", ""));
    ship->description = str_dup(json_get_string_default(json, "description", ""));
    ship->ship_class = json_get_int_default(json, "ship_class", 0);
    ship->flags = json_get_int_default(json, "flags", 0);
    
    // Blueprint reference - use parse_widevnum for consistency
    json_t *blueprint_ref = json_object_get(json, "blueprint");
    if (blueprint_ref) {
        if (json_is_string(blueprint_ref)) {
            WNUM_LOAD wload;
            if (parse_widevnum_load(json_string_value(blueprint_ref), &wload)) {
                ship->blueprint_ref.load.auid = wload.auid;
                ship->blueprint_ref.load.vnum = wload.vnum;
            }
        } else if (json_is_integer(blueprint_ref)) {
            /* Bare vnum - assume same area */
            ship->blueprint_ref.load.auid = area->uid;
            ship->blueprint_ref.load.vnum = json_integer_value(blueprint_ref);
        }
    }
    ship->blueprint = NULL;  // Will be resolved in fix pass

    // Ship object reference - use parse_widevnum for consistency
    json_t *ship_object_ref = json_object_get(json, "ship_object");
    if (ship_object_ref) {
        if (json_is_string(ship_object_ref)) {
            WNUM_LOAD wload;
            if (parse_widevnum_load(json_string_value(ship_object_ref), &wload)) {
                ship->ship_object_ref.load.auid = wload.auid;
                ship->ship_object_ref.load.vnum = wload.vnum;
            }
        } else if (json_is_integer(ship_object_ref)) {
            /* Bare vnum - assume same area */
            ship->ship_object_ref.load.auid = area->uid;
            ship->ship_object_ref.load.vnum = json_integer_value(ship_object_ref);
        }
    }
    ship->ship_object = NULL;  // Will be resolved in fix pass
    
    // Stats
    ship->hit = json_get_int_default(json, "hit", 100);
    ship->guns = json_get_int_default(json, "guns", 0);
    ship->min_crew = json_get_int_default(json, "min_crew", 0);
    ship->max_crew = json_get_int_default(json, "max_crew", 0);
    ship->move_delay = json_get_int_default(json, "move_delay", 12);
    ship->move_steps = json_get_int_default(json, "move_steps", 5);
    ship->turning = json_get_int_default(json, "turning", 5);
    ship->weight = json_get_int_default(json, "weight", 100);
    ship->capacity = json_get_int_default(json, "capacity", 100);
    ship->armor = json_get_int_default(json, "armor", 0);
    ship->oars = json_get_int_default(json, "oars", 0);
    
    // Special keys - handle widevnum strings or legacy integers
    json_t *special_keys = json_object_get(json, "special_keys");
    if (special_keys && json_is_array(special_keys)) {
        ship->special_keys = list_create(false);
        size_t index;
        json_t *key_val;

        json_array_foreach(special_keys, index, key_val) {
            WNUM_LOAD wload;
            OBJ_INDEX_DATA *key = NULL;
            AREA_DATA *key_area = NULL;

            wload.auid = 0;
            wload.vnum = -1;

            if (json_is_string(key_val)) {
                if (!parse_widevnum_load(json_string_value(key_val), &wload)) {
                    continue;
                }
            } else if (json_is_integer(key_val)) {
                wload.auid = area->uid;
                wload.vnum = json_integer_value(key_val);
            } else {
                continue;
            }

            if (wload.vnum < 1) {
                continue;
            }

            key_area = wload.auid > 0 ? get_area_from_uid(wload.auid) : area;
            if (!key_area) {
                key_area = area;
            }

            key = get_obj_index(key_area, wload.vnum);
            if (!key) {
                key = get_obj_index_global(wload.vnum);
            }

            if (key) {
                list_appendlink(ship->special_keys, key);
            } else {
                log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                    "Ship '%s' (vnum %ld in %s): special key object %ld not found",
                    ship->name ? ship->name : "unnamed",
                    ship->vnum,
                    area->name,
                    wload.vnum);
            }
        }
    } else {
        ship->special_keys = list_create(false);
    }
    
    return ship;
}

/*
 * Deserialize a blueprint section from JSON
 */
BLUEPRINT_SECTION *json_area_deserialize_blueprint_section(json_t *json, AREA_DATA *area)
{
    if (!json || !area) return NULL;
    
    BLUEPRINT_SECTION *section = alloc_perm(sizeof(BLUEPRINT_SECTION));
    if (!section) return NULL;
    
    section->valid = true;
    section->area = area;
    
    pbugf(LOG_DEBUG, "[BPSECT LOAD] vnum=%ld area=%p (%s)", 
          json_get_int_default(json, "vnum", 0),
          (void*)area, 
          area ? area->name : "NULL");
    
    // Basic info
    section->vnum = json_get_int_default(json, "vnum", 0);
    section->name = str_dup(json_get_string_default(json, "name", ""));
    section->description = str_dup(json_get_string_default(json, "description", ""));
    
    const char *comments = json_get_string_default(json, "comments", "");
    if (comments && comments[0] != '\0')
        section->comments = str_dup(comments);
    
    section->type = json_get_int_default(json, "type", 0);
    section->flags = json_get_int_default(json, "flags", 0);
    
    // Maze data (only for BSTYPE_MAZE sections)
    section->maze_x = json_get_int_default(json, "maze_w", 0);
    section->maze_y = json_get_int_default(json, "maze_h", 0);
    section->total_maze_weight = 0;
    section->maze_templates = list_create(false);
    section->maze_fixed_rooms = list_create(false);
    
    json_t *maze_templates = json_object_get(json, "maze_templates");
    if (maze_templates && json_is_array(maze_templates)) {
        size_t mt_index;
        json_t *mt_json;
        json_array_foreach(maze_templates, mt_index, mt_json) {
            MAZE_WEIGHTED_ROOM *mwr = new_maze_weighted_room();
            mwr->weight = json_get_int_default(mt_json, "weight", 1);
            mwr->exit_count = json_get_int_default(mt_json, "exit_count", 0);
            json_t *room_ref = json_object_get(mt_json, "room");
            if (room_ref && json_is_string(room_ref)) {
                WNUM_LOAD wload;
                if (parse_widevnum_load(json_string_value(room_ref), &wload)) {
                    mwr->room_ref.load.auid = wload.auid;
                    mwr->room_ref.load.vnum = wload.vnum;
                }
            } else {
                mwr->room_ref.load.auid = area->uid;
                mwr->room_ref.load.vnum = json_get_int_default(mt_json, "room", 0);
            }
            mwr->room = NULL;  // Resolved in fix pass

            /* Exit template properties */
            json_t *et_json = json_object_get(mt_json, "exit_template");
            if (et_json && json_is_object(et_json)) {
                mwr->exit_template.flags = json_array_to_flags(json_object_get(et_json, "flags"), exit_flags);
                mwr->exit_template.keyword = str_dup(json_get_string_default(et_json, "keyword", "door"));
                mwr->exit_template.strength = (int16_t)json_get_int_default(et_json, "strength", 0);
                const char *mat = json_get_string_default(et_json, "material", "");
                mwr->exit_template.material = (mat[0] != '\0') ? str_dup(mat) : NULL;

                json_t *key_json = json_object_get(et_json, "key");
                if (key_json && json_is_string(key_json)) {
                    WNUM_LOAD wload;
                    if (parse_widevnum_load(json_string_value(key_json), &wload))
                        mwr->exit_template.lock.key_load = wload;
                } else {
                    mwr->exit_template.lock.key_load.auid = area->uid;
                    mwr->exit_template.lock.key_load.vnum = json_get_int_default(et_json, "key", 0);
                }
                mwr->exit_template.lock.pick_chance = json_get_int_default(et_json, "pick_chance", 0);
                json_t *lf_json = json_object_get(et_json, "lock_flags");
                if (lf_json && json_is_array(lf_json))
                    mwr->exit_template.lock.flags = json_array_to_flags(lf_json, lock_flags);
                else
                    mwr->exit_template.lock.flags = json_get_int_default(et_json, "lock_flags", 0);
            }

            section->total_maze_weight += mwr->weight;
            list_appendlink(section->maze_templates, mwr);
        }
    }
    
    json_t *maze_fixed = json_object_get(json, "maze_fixed_rooms");
    if (maze_fixed && json_is_array(maze_fixed)) {
        size_t mf_index;
        json_t *mf_json;
        json_array_foreach(maze_fixed, mf_index, mf_json) {
            MAZE_FIXED_ROOM *mfr = new_maze_fixed_room();
            mfr->x = json_get_int_default(mf_json, "x", 0);
            mfr->y = json_get_int_default(mf_json, "y", 0);
            mfr->connected = json_get_bool_default(mf_json, "connected", true);
            json_t *room_ref = json_object_get(mf_json, "room");
            if (room_ref && json_is_string(room_ref)) {
                WNUM_LOAD wload;
                if (parse_widevnum_load(json_string_value(room_ref), &wload)) {
                    mfr->room_ref.load.auid = wload.auid;
                    mfr->room_ref.load.vnum = wload.vnum;
                }
            } else {
                mfr->room_ref.load.auid = area->uid;
                mfr->room_ref.load.vnum = json_get_int_default(mf_json, "room", 0);
            }
            mfr->room = NULL;  // Resolved in fix pass
            list_appendlink(section->maze_fixed_rooms, mfr);
        }
    }
    
    // Maze map data
    json_t *map_data_json = json_object_get(json, "maze_map");
    if (map_data_json && json_is_object(map_data_json)) {
        MAZE_MAP_DATA *mmd = new_maze_map_data();

        json_t *obj_ref = json_object_get(map_data_json, "obj");
        if (obj_ref && json_is_string(obj_ref)) {
            WNUM_LOAD wload;
            if (parse_widevnum_load(json_string_value(obj_ref), &wload)) {
                mmd->obj_ref.load.auid = wload.auid;
                mmd->obj_ref.load.vnum = wload.vnum;
            }
        } else {
            mmd->obj_ref.load.auid = area->uid;
            mmd->obj_ref.load.vnum = json_get_int_default(map_data_json, "obj", 0);
        }

        json_t *mob_ref = json_object_get(map_data_json, "mob");
        if (mob_ref && json_is_string(mob_ref)) {
            WNUM_LOAD wload;
            if (parse_widevnum_load(json_string_value(mob_ref), &wload)) {
                mmd->mob_ref.load.auid = wload.auid;
                mmd->mob_ref.load.vnum = wload.vnum;
            }
        } else {
            mmd->mob_ref.load.auid = area->uid;
            mmd->mob_ref.load.vnum = json_get_int_default(map_data_json, "mob", 0);
        }

        mmd->solve = json_get_bool_default(map_data_json, "solve", false);
        mmd->obj = NULL;
        mmd->mob = NULL;

        section->map_data = mmd;
    }

    // Room range - can be integer vnum or WNUM string
    json_t *lower_vnum_json = json_object_get(json, "lower_vnum");
    if (lower_vnum_json) {
        if (json_is_integer(lower_vnum_json)) {
            section->lower_vnum_ref.load.auid = area->uid;
            section->lower_vnum_ref.load.vnum = json_integer_value(lower_vnum_json);
            section->lower_vnum = json_integer_value(lower_vnum_json);
        } else if (json_is_string(lower_vnum_json)) {
            const char *wnum_str = json_string_value(lower_vnum_json);
            unsigned long auid = 0;
            long vnum = 0;
            if (sscanf(wnum_str, "%lu#%ld", &auid, &vnum) == 2) {
                section->lower_vnum_ref.load.auid = auid;
                section->lower_vnum_ref.load.vnum = vnum;
                section->lower_vnum = vnum;
            } else {
                section->lower_vnum_ref.load.auid = area->uid;
                section->lower_vnum_ref.load.vnum = atol(wnum_str);
                section->lower_vnum = atol(wnum_str);
            }
        }
    }
    
    json_t *upper_vnum_json = json_object_get(json, "upper_vnum");
    if (upper_vnum_json) {
        if (json_is_integer(upper_vnum_json)) {
            section->upper_vnum_ref.load.auid = area->uid;
            section->upper_vnum_ref.load.vnum = json_integer_value(upper_vnum_json);
            section->upper_vnum = json_integer_value(upper_vnum_json);
        } else if (json_is_string(upper_vnum_json)) {
            const char *wnum_str = json_string_value(upper_vnum_json);
            unsigned long auid = 0;
            long vnum = 0;
            if (sscanf(wnum_str, "%lu#%ld", &auid, &vnum) == 2) {
                section->upper_vnum_ref.load.auid = auid;
                section->upper_vnum_ref.load.vnum = vnum;
                section->upper_vnum = vnum;
            } else {
                section->upper_vnum_ref.load.auid = area->uid;
                section->upper_vnum_ref.load.vnum = atol(wnum_str);
                section->upper_vnum = atol(wnum_str);
            }
        }
    }
    section->rooms_area = NULL;  // Will be resolved in fix pass
    
    // Recall room - store as WNUM_LOAD for later resolution
    // Handle new "recall_room" widevnum string or legacy "recall" object
    json_t *recall_json = json_object_get(json, "recall_room");
    if (recall_json && json_is_string(recall_json)) {
        WNUM_LOAD wload;
        if (parse_widevnum_load(json_string_value(recall_json), &wload)) {
            section->recall_ref.load.auid = wload.auid;
            section->recall_ref.load.vnum = wload.vnum;
        }
    } else {
        /* Legacy format: "recall" object with area_uid and vnum */
        json_t *recall = json_object_get(json, "recall");
        if (recall && json_is_object(recall)) {
            section->recall_ref.load.auid = json_get_int_default(recall, "area_uid", 0);
            section->recall_ref.load.vnum = json_get_int_default(recall, "vnum", 0);
        } else {
            section->recall_ref.load.auid = 0;
            section->recall_ref.load.vnum = 0;
        }
    }
    section->recall_room = NULL;  // Will be resolved in fix pass
    
    // Links
    json_t *links = json_object_get(json, "links");
    if (links && json_is_array(links)) {
        size_t index;
        json_t *link_json;
        
        json_array_foreach(links, index, link_json) {
            BLUEPRINT_LINK *link = alloc_perm(sizeof(BLUEPRINT_LINK));
            link->valid = true;
            link->name = str_dup(json_get_string_default(link_json, "name", ""));
            link->door = json_get_int_default(link_json, "door", 0);
            link->used = false;
            link->next = NULL;
            
            // Room reference - handle widevnum string or legacy integer
            json_t *room_json = json_object_get(link_json, "room");
            if (room_json && json_is_string(room_json)) {
                WNUM_LOAD wload;
                if (parse_widevnum_load(json_string_value(room_json), &wload)) {
                    link->room_ref.load.auid = wload.auid;
                    link->room_ref.load.vnum = wload.vnum;
                }
            } else {
                link->room_ref.load.auid = area->uid;
                link->room_ref.load.vnum = json_get_int_default(link_json, "room", 0);
            }
            link->room = NULL;  // Will be resolved in fix pass
            link->ex = NULL;
            
            // Add to section's link list preserving JSON order
            if (!section->links) {
                section->links = link;
            } else {
                BLUEPRINT_LINK *tail = section->links;
                while (tail->next)
                    tail = tail->next;
                tail->next = link;
            }
        }
    }
    
    return section;
}

/*
 * Deserialize a blueprint from JSON
 */
BLUEPRINT *json_area_deserialize_blueprint(json_t *json, AREA_DATA *area)
{
    if (!json || !area) return NULL;
    
    BLUEPRINT *blueprint = alloc_perm(sizeof(BLUEPRINT));
    if (!blueprint) return NULL;
    
    blueprint->valid = true;
    blueprint->area = area;
    
    /* Initialize list fields so they're always valid, even if JSON lacks them */
    blueprint->name = &str_empty[0];
    blueprint->description = &str_empty[0];
    blueprint->comments = &str_empty[0];
    blueprint->sections = list_create(false);
    blueprint->special_rooms = list_createx(false, NULL, NULL);
    blueprint->_static.layout = NULL;
    blueprint->_static.recall = -1;
    blueprint->_static.entries = list_createx(false, NULL, NULL);
    blueprint->_static.exits = list_createx(false, NULL, NULL);
    
    // Basic info
    blueprint->vnum = json_get_int_default(json, "vnum", 0);
    blueprint->name = str_dup(json_get_string_default(json, "name", ""));
    blueprint->description = str_dup(json_get_string_default(json, "description", ""));
    
    const char *comments = json_get_string_default(json, "comments", "");
    if (comments && comments[0] != '\0')
        blueprint->comments = str_dup(comments);
    
    blueprint->area_who = json_get_int_default(json, "area_who", 0);
    blueprint->repop = json_get_int_default(json, "repop", 0);
    blueprint->flags = json_get_int_default(json, "flags", 0);
    blueprint->mode = json_get_int_default(json, "mode", 0);
    
    // Sections - list of section references with WNUM_LOAD
    json_t *sections = json_object_get(json, "sections");
    if (sections && json_is_array(sections)) {
        blueprint->sections = list_create(false);
        size_t index;
        json_t *section_vnum_json;
        
        json_array_foreach(sections, index, section_vnum_json) {
            if (json_is_integer(section_vnum_json)) {
                BLUEPRINT_SECTION_REF *ref = alloc_perm(sizeof(BLUEPRINT_SECTION_REF));
                ref->section_ref.load.auid = area->uid;
                ref->section_ref.load.vnum = json_integer_value(section_vnum_json);
                ref->section = NULL;  // Will be resolved in fix pass
                list_appendlink(blueprint->sections, ref);
            }
        }
    }
    
    // Special rooms
    json_t *special_rooms = json_object_get(json, "special_rooms");
    if (special_rooms && json_is_array(special_rooms)) {
        blueprint->special_rooms = list_create(false);
        size_t index;
        json_t *special_json;
        
        json_array_foreach(special_rooms, index, special_json) {
            BLUEPRINT_SPECIAL_ROOM *special = alloc_perm(sizeof(BLUEPRINT_SPECIAL_ROOM));
            special->valid = true;
            special->name = str_dup(json_get_string_default(special_json, "name", ""));
            special->section = json_get_int_default(special_json, "section", 0);

            // Room vnum - handle widevnum string or legacy integer
            json_t *room_json = json_object_get(special_json, "room");
            if (room_json && json_is_string(room_json)) {
                WNUM_LOAD wload;
                if (parse_widevnum_load(json_string_value(room_json), &wload)) {
                    special->room_ref.load.auid = wload.auid;
                    special->room_ref.load.vnum = wload.vnum;
                }
            } else {
                special->room_ref.load.auid = area->uid;
                special->room_ref.load.vnum = json_get_int_default(special_json, "room", 0);
            }
            special->room = NULL;  // Will be resolved in fix pass

            list_appendlink(blueprint->special_rooms, special);
        }
    }
    
    // Static mode fields - all in 'static' object
    json_t *static_data = json_object_get(json, "static");
    if (static_data && json_is_object(static_data)) {
        blueprint->_static.recall = json_get_int_default(static_data, "recall", 0);
        
        // Static entries
        json_t *static_entries = json_object_get(static_data, "entries");
        if (static_entries && json_is_array(static_entries)) {
            blueprint->_static.entries = list_create(false);
            size_t index;
            json_t *entry_json;
            
            json_array_foreach(static_entries, index, entry_json) {
                BLUEPRINT_EXIT_DATA *entry = alloc_perm(sizeof(BLUEPRINT_EXIT_DATA));
                entry->name = str_dup(json_get_string_default(entry_json, "name", ""));
                entry->section = json_get_int_default(entry_json, "section", 0);
                entry->link = json_get_int_default(entry_json, "link", 0);
                list_appendlink(blueprint->_static.entries, entry);
            }
        }
        
        // Static exits
        json_t *static_exits = json_object_get(static_data, "exits");
        if (static_exits && json_is_array(static_exits)) {
            blueprint->_static.exits = list_create(false);
            size_t index;
            json_t *exit_json;
            
            json_array_foreach(static_exits, index, exit_json) {
                BLUEPRINT_EXIT_DATA *exit_data = alloc_perm(sizeof(BLUEPRINT_EXIT_DATA));
                exit_data->name = str_dup(json_get_string_default(exit_json, "name", ""));
                exit_data->section = json_get_int_default(exit_json, "section", 0);
                exit_data->link = json_get_int_default(exit_json, "link", 0);
                list_appendlink(blueprint->_static.exits, exit_data);
            }
        }
        
        // Static layout
        json_t *static_layout = json_object_get(static_data, "layout");
        if (static_layout && json_is_array(static_layout)) {
            size_t index;
            json_t *link_json;
            
            json_array_foreach(static_layout, index, link_json) {
                STATIC_BLUEPRINT_LINK *link = alloc_perm(sizeof(STATIC_BLUEPRINT_LINK));
                link->valid = true;
                link->blueprint = blueprint;
                link->section1 = json_get_int_default(link_json, "section1", 0);
                link->link1 = json_get_int_default(link_json, "link1", 0);
                link->section2 = json_get_int_default(link_json, "section2", 0);
                link->link2 = json_get_int_default(link_json, "link2", 0);
                
                // Add to layout list
                link->next = blueprint->_static.layout;
                blueprint->_static.layout = link;
            }
        }
    }
    
    // Blueprint progs
    blueprint->progs = json_area_deserialize_progs(json_object_get(json, "progs"), area, PRG_IPROG);
    
    // Index vars
    blueprint->index_vars = NULL;
    json_t *index_vars = json_object_get(json, "index_vars");
    if (index_vars) {
        blueprint->index_vars = json_area_deserialize_index_vars(index_vars, area);
    }
    
    return blueprint;
}

/***************************************************************************
 * Main Load/Save Functions                                                *
 ***************************************************************************/

AREA_DATA *json_area_load(const char *filename)
{
    char area_dir_buf[MAX_INPUT_LENGTH];
    const char *area_dir = resolve_game_path(AREA_DIR, area_dir_buf, sizeof(area_dir_buf));
    char path[512];
    json_t *root, *area_obj;
    json_error_t error;
    AREA_DATA *area;
    
    /* Build full path */
    snprintf(path, sizeof(path), "%s%s", area_dir, filename);
    
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
            if (token && token->vnum) {
                /* Add to area's token hash table */
                int hash = token->vnum % MAX_KEY_HASH;
                token->next = area->token_index_hash[hash];
                area->token_index_hash[hash] = token;
            }
        }
    }

    /* Deserialize reputations */
    json_t *reputations = json_object_get(root, "reputations");
    if (reputations && json_is_array(reputations)) {
        size_t index;
        json_t *reputation_json;
        json_array_foreach(reputations, index, reputation_json) {
            REPUTATION_INDEX_DATA *reputation = json_area_deserialize_reputation(reputation_json, area);
            if (reputation && reputation->vnum) {
                int hash = reputation->vnum % MAX_KEY_HASH;
                reputation->next = area->reputation_index_hash[hash];
                area->reputation_index_hash[hash] = reputation;
            }
        }
    }

    /* Deserialize quest indices v2 */
    json_t *quests_v2 = json_object_get(root, "quests_v2");
    if (quests_v2 && json_is_array(quests_v2)) {
        size_t index;
        json_t *quest_json;
        json_array_foreach(quests_v2, index, quest_json) {
            (void)json_area_deserialize_quest_v2(quest_json, area);
        }
    }
    
    /* Deserialize dungeons */
    json_t *dungeons = json_object_get(root, "dungeons");
    if (dungeons && json_is_array(dungeons)) {
        size_t index;
        json_t *dungeon_json;
        json_array_foreach(dungeons, index, dungeon_json) {
            DUNGEON_INDEX_DATA *dungeon = json_area_deserialize_dungeon(dungeon_json, area);
            if (dungeon && dungeon->vnum) {
                /* Add to area's dungeon hash table */
                int hash = dungeon->vnum % MAX_KEY_HASH;
                dungeon->next = area->dungeon_index_hash[hash];
                area->dungeon_index_hash[hash] = dungeon;
            }
        }
    }
    
    /* Deserialize blueprint sections */
    json_t *sections = json_object_get(root, "blueprint_sections");
    if (sections && json_is_array(sections)) {
        size_t index;
        json_t *section_json;
        json_array_foreach(sections, index, section_json) {
            BLUEPRINT_SECTION *section = json_area_deserialize_blueprint_section(section_json, area);
            if (section && section->vnum) {
                /* Add to area's blueprint section hash table */
                int hash = section->vnum % MAX_KEY_HASH;
                section->next = area->blueprint_section_hash[hash];
                area->blueprint_section_hash[hash] = section;
            }
        }
    }
    
    /* Deserialize blueprints */
    json_t *blueprints = json_object_get(root, "blueprints");
    if (blueprints && json_is_array(blueprints)) {
        size_t index;
        json_t *blueprint_json;
        json_array_foreach(blueprints, index, blueprint_json) {
            BLUEPRINT *blueprint = json_area_deserialize_blueprint(blueprint_json, area);
            if (blueprint && blueprint->vnum) {
                /* Add to area's blueprint hash table */
                int hash = blueprint->vnum % MAX_KEY_HASH;
                blueprint->next = area->blueprint_hash[hash];
                area->blueprint_hash[hash] = blueprint;
            }
        }
    }
    
    /* Deserialize ships */
    json_t *ships = json_object_get(root, "ships");
    if (ships && json_is_array(ships)) {
        size_t index;
        json_t *ship_json;
        json_array_foreach(ships, index, ship_json) {
            SHIP_INDEX_DATA *ship = json_area_deserialize_ship(ship_json, area);
            if (ship && ship->vnum) {
                /* Add to area's ship hash table */
                int hash = ship->vnum % MAX_KEY_HASH;
                ship->next = area->ship_index_hash[hash];
                area->ship_index_hash[hash] = ship;
            }
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
                if (get_script_index(area, script->vnum, PRG_MPROG)) {
                    free_script(script);
                    continue;
                }
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
                if (get_script_index(area, script->vnum, PRG_OPROG)) {
                    free_script(script);
                    continue;
                }
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
                if (get_script_index(area, script->vnum, PRG_RPROG)) {
                    free_script(script);
                    continue;
                }
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
                if (get_script_index(area, script->vnum, PRG_TPROG)) {
                    free_script(script);
                    continue;
                }
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
                if (get_script_index(area, script->vnum, PRG_APROG)) {
                    free_script(script);
                    continue;
                }
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
                if (get_script_index(area, script->vnum, PRG_IPROG)) {
                    free_script(script);
                    continue;
                }
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
                if (get_script_index(area, script->vnum, PRG_DPROG)) {
                    free_script(script);
                    continue;
                }
                script->next = area->dprog_list;
                area->dprog_list = script;
            }
        }
    }

    /* Quest progs */
    scripts = json_object_get(root, "qprogs");
    if (scripts && json_is_array(scripts)) {
        json_array_foreach(scripts, script_index, script_json) {
            script = json_area_deserialize_script(script_json, area, IFC_Q);
            if (script) {
                script->type = PRG_QPROG;
                if (get_script_index(area, script->vnum, PRG_QPROG)) {
                    free_script(script);
                    continue;
                }
                script->next = area->qprog_list;
                area->qprog_list = script;
            }
        }
    }
    
    json_decref(root);
    
    log_stringf("json_area_load: Loaded area '%s' (UID %ld)", area->name, area->uid);
    
    return area;
}

bool json_area_save_to(AREA_DATA *area, const char *filename)
{
    char path[512];
    json_t *root;
    
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
    // Always iterate through all hash buckets to catch widevnum entities
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (ROOM_INDEX_DATA *room = area->room_index_hash[j]; room; room = room->next) {
            if (room->vnum && room->area == area) {
                json_t *room_json = json_area_serialize_room(room);
                if (room_json) {
                    json_array_append_new(rooms, room_json);
                }
            }
        }

    }
    json_object_set_new(root, "rooms", rooms);
    
    /* Serialize mobiles */
    json_t *mobiles = json_array();
    // Always iterate through all hash buckets to catch widevnum entities
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (MOB_INDEX_DATA *mob = area->mob_index_hash[j]; mob; mob = mob->next) {
            if (mob->vnum && mob->area == area) {
                json_t *mob_json = json_area_serialize_mobile(mob);
                if (mob_json) {
                    json_array_append_new(mobiles, mob_json);
                }
            }
        }

    }
    json_object_set_new(root, "mobiles", mobiles);
    
    /* Serialize objects */
    json_t *objects = json_array();
    // Always iterate through all hash buckets to catch widevnum entities
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (OBJ_INDEX_DATA *obj = area->obj_index_hash[j]; obj; obj = obj->next) {
            if (obj->vnum && obj->area == area) {
                json_t *obj_json = json_area_serialize_object(obj);
                if (obj_json) {
                    json_array_append_new(objects, obj_json);
                }
            }
        }

    }
    json_object_set_new(root, "objects", objects);
    
    /* Serialize shops - shops are linked to mobiles via mob->pShop */
    json_t *shops = json_array();
    // Always iterate through all hash buckets to catch widevnum entities
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (MOB_INDEX_DATA *mob = area->mob_index_hash[j]; mob; mob = mob->next) {
            if (mob->vnum && mob->area == area && mob->pShop) {
                json_t *shop_json = json_area_serialize_shop(mob->pShop, area);
                if (shop_json) {
                    json_array_append_new(shops, shop_json);
                }
            }
        }

    }
    if (json_array_size(shops) > 0) {
        json_object_set_new(root, "shops", shops);
    } else {
        json_decref(shops);
    }
    
    /* Serialize tokens */
    json_t *tokens = json_array();
    // Always iterate through all hash buckets to catch widevnum entities
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (TOKEN_INDEX_DATA *token = area->token_index_hash[j]; token; token = token->next) {
            if (token->vnum && token->area == area) {
                json_t *token_json = json_area_serialize_token(token);
                if (token_json) {
                    json_array_append_new(tokens, token_json);
                }
            }
        }

    }
    if (json_array_size(tokens) > 0) {
        json_object_set_new(root, "tokens", tokens);
    } else {
        json_decref(tokens);
    }

    /* Serialize reputations */
    json_t *reputations = json_array();
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (REPUTATION_INDEX_DATA *reputation = area->reputation_index_hash[j]; reputation; reputation = reputation->next) {
            if (reputation->vnum && reputation->area == area) {
                json_t *reputation_json = json_area_serialize_reputation(reputation, area);
                if (reputation_json) {
                    json_array_append_new(reputations, reputation_json);
                }
            }
        }
    }
    if (json_array_size(reputations) > 0) {
        json_object_set_new(root, "reputations", reputations);
    } else {
        json_decref(reputations);
    }

    /* Serialize quest indices v2 */
    json_t *quests_v2 = json_array();
    for (QUEST_INDEX_V2_DATA *quest_index_v2 = quest_index_v2_list; quest_index_v2; quest_index_v2 = quest_index_v2->next) {
        if (quest_index_v2->vnum && quest_index_v2->area == area) {
            json_t *quest_json = json_area_serialize_quest_v2(quest_index_v2, area);
            if (quest_json) {
                json_array_append_new(quests_v2, quest_json);
            }
        }
    }
    if (json_array_size(quests_v2) > 0) {
        json_object_set_new(root, "quests_v2", quests_v2);
    } else {
        json_decref(quests_v2);
    }
    
    /* Write to file with pretty printing */
    if (!json_file_save(root, path, "json_area_save_to", JSON_INDENT(2) | JSON_PRESERVE_ORDER)) {
        return false;
    }
    
    log_stringf("json_area_save_to: Saved area '%s' (UID %ld) to %s", area->name, area->uid, path);
    
    return true;
}

bool json_area_save(AREA_DATA *area)
{
    char area_dir_buf[MAX_INPUT_LENGTH];
    const char *area_dir = resolve_game_path(AREA_DIR, area_dir_buf, sizeof(area_dir_buf));
    char path[512];
    json_t *root;
    
    if (!area || !area->file_name) {
        log_string("json_area_save: Invalid area or filename");
        return false;
    }
    
    /* Build full path */
    snprintf(path, sizeof(path), "%s%s", area_dir, area->file_name);
    
    /* Create root JSON object */
    root = json_object();
    json_object_set_new(root, "schema_version", json_string(JSON_AREA_SCHEMA_VERSION));
    
    /* Serialize area metadata */
    json_object_set_new(root, "area", json_area_serialize_metadata(area));
    
    /* Serialize rooms */
    json_t *rooms = json_array();
    // Always iterate through all hash buckets to catch widevnum entities
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (ROOM_INDEX_DATA *room = area->room_index_hash[j]; room; room = room->next) {
            if (room->vnum && room->area == area) {
                json_t *room_json = json_area_serialize_room(room);
                if (room_json) {
                    json_array_append_new(rooms, room_json);
                }
            }
        }

    }
    json_object_set_new(root, "rooms", rooms);
    
    /* Serialize mobiles */
    json_t *mobiles = json_array();
    // Always iterate through all hash buckets to catch widevnum entities
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (MOB_INDEX_DATA *mob = area->mob_index_hash[j]; mob; mob = mob->next) {
            if (mob->vnum && mob->area == area) {
                json_t *mob_json = json_area_serialize_mobile(mob);
                if (mob_json) {
                    json_array_append_new(mobiles, mob_json);
                }
            }
        }

    }
    json_object_set_new(root, "mobiles", mobiles);
    
    /* Serialize objects */
    json_t *objects = json_array();
    // Always iterate through all hash buckets to catch widevnum entities
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (OBJ_INDEX_DATA *obj = area->obj_index_hash[j]; obj; obj = obj->next) {
            if (obj->vnum && obj->area == area) {
                json_t *obj_json = json_area_serialize_object(obj);
                if (obj_json) {
                    json_array_append_new(objects, obj_json);
                }
            }
        }

    }
    json_object_set_new(root, "objects", objects);
    
    /* Serialize tokens */
    json_t *tokens = json_array();
    // Always iterate through all hash buckets to catch widevnum entities
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (TOKEN_INDEX_DATA *token = area->token_index_hash[j]; token; token = token->next) {
            if (token->vnum && token->area == area) {
                json_t *token_json = json_area_serialize_token(token);
                if (token_json) {
                    json_array_append_new(tokens, token_json);
                }
            }
        }

    }
    json_object_set_new(root, "tokens", tokens);

    /* Serialize reputations */
    json_t *reputations = json_array();
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (REPUTATION_INDEX_DATA *reputation = area->reputation_index_hash[j]; reputation; reputation = reputation->next) {
            if (reputation->vnum && reputation->area == area) {
                json_t *reputation_json = json_area_serialize_reputation(reputation, area);
                if (reputation_json) {
                    json_array_append_new(reputations, reputation_json);
                }
            }
        }

    }
    if (json_array_size(reputations) > 0)
        json_object_set_new(root, "reputations", reputations);
    else
        json_decref(reputations);

    /* Serialize quest indices v2 */
    json_t *quests_v2 = json_array();
    for (QUEST_INDEX_V2_DATA *quest_index_v2 = quest_index_v2_list; quest_index_v2; quest_index_v2 = quest_index_v2->next) {
        if (quest_index_v2->vnum && quest_index_v2->area == area) {
            json_t *quest_json = json_area_serialize_quest_v2(quest_index_v2, area);
            if (quest_json) {
                json_array_append_new(quests_v2, quest_json);
            }
        }
    }
    if (json_array_size(quests_v2) > 0)
        json_object_set_new(root, "quests_v2", quests_v2);
    else
        json_decref(quests_v2);
    
    /* Serialize blueprints */
    json_t *blueprints = json_array();
    // Always iterate through all hash buckets to catch widevnum entities
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (BLUEPRINT *blueprint = area->blueprint_hash[j]; blueprint; blueprint = blueprint->next) {
            if (blueprint->vnum && blueprint->area == area) {
                json_t *blueprint_json = json_area_serialize_blueprint(blueprint, area);
                if (blueprint_json) {
                    json_array_append_new(blueprints, blueprint_json);
                }
            }
        }

    }
    if (json_array_size(blueprints) > 0)
        json_object_set_new(root, "blueprints", blueprints);
    else
        json_decref(blueprints);
    
    /* Serialize blueprint sections */
    json_t *blueprint_sections = json_array();
    // Always iterate through all hash buckets to catch widevnum entities
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (BLUEPRINT_SECTION *section = area->blueprint_section_hash[j]; section; section = section->next) {
            if (section->vnum && section->area == area) {
                json_t *section_json = json_area_serialize_blueprint_section(section, area);
                if (section_json) {
                    json_array_append_new(blueprint_sections, section_json);
                }
            }
        }

    }
    if (json_array_size(blueprint_sections) > 0)
        json_object_set_new(root, "blueprint_sections", blueprint_sections);
    else
        json_decref(blueprint_sections);
    
    /* Serialize dungeons */
    json_t *dungeons = json_array();
    // Always iterate through all hash buckets to catch widevnum entities
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (DUNGEON_INDEX_DATA *dungeon = area->dungeon_index_hash[j]; dungeon; dungeon = dungeon->next) {
            if (dungeon->vnum && dungeon->area == area) {
                json_t *dungeon_json = json_area_serialize_dungeon(dungeon, area);
                if (dungeon_json) {
                    json_array_append_new(dungeons, dungeon_json);
                }
            }
        }

    }
    if (json_array_size(dungeons) > 0)
        json_object_set_new(root, "dungeons", dungeons);
    else
        json_decref(dungeons);
    
    /* Serialize ships */
    json_t *ships = json_array();
    // Always iterate through all hash buckets to catch widevnum entities
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (SHIP_INDEX_DATA *ship = area->ship_index_hash[j]; ship; ship = ship->next) {
            if (ship->vnum && ship->area == area) {
                json_t *ship_json = json_area_serialize_ship(ship, area);
                if (ship_json) {
                    json_array_append_new(ships, ship_json);
                }
            }
        }

    }
    if (json_array_size(ships) > 0)
        json_object_set_new(root, "ships", ships);
    else
        json_decref(ships);
    
    /* Serialize scripts */
    /* Save all 8 types of scripts: mobprogs, oprogs, rprogs, tprogs, aprogs, iprogs, dprogs, qprogs */
    json_t *mobprogs = json_array();
    json_t *oprogs = json_array();
    json_t *rprogs = json_array();
    json_t *tprogs = json_array();
    json_t *aprogs = json_array();
    json_t *iprogs = json_array();
    json_t *dprogs = json_array();
    json_t *qprogs = json_array();
    
    /* Iterate through script linked lists directly */
    SCRIPT_DATA *script;
    json_t *script_json;
    
    // MOBprogs
    for (script = area->mprog_list; script; script = script->next) {
        if (json_script_array_has_vnum(mobprogs, script->vnum))
            continue;
        script_json = json_area_serialize_script(script);
        if (script_json) json_array_append_new(mobprogs, script_json);
    }
    
    // OBJprogs
    for (script = area->oprog_list; script; script = script->next) {
        if (json_script_array_has_vnum(oprogs, script->vnum))
            continue;
        script_json = json_area_serialize_script(script);
        if (script_json) json_array_append_new(oprogs, script_json);
    }
    
    // ROOMprogs
    for (script = area->rprog_list; script; script = script->next) {
        if (json_script_array_has_vnum(rprogs, script->vnum))
            continue;
        script_json = json_area_serialize_script(script);
        if (script_json) json_array_append_new(rprogs, script_json);
    }
    
    // TOKENprogs
    for (script = area->tprog_list; script; script = script->next) {
        if (json_script_array_has_vnum(tprogs, script->vnum))
            continue;
        script_json = json_area_serialize_script(script);
        if (script_json) json_array_append_new(tprogs, script_json);
    }
    
    // AREAprogs
    for (script = area->aprog_list; script; script = script->next) {
        if (json_script_array_has_vnum(aprogs, script->vnum))
            continue;
        script_json = json_area_serialize_script(script);
        if (script_json) json_array_append_new(aprogs, script_json);
    }
    
    // INSTANCEprogs
    for (script = area->iprog_list; script; script = script->next) {
        if (json_script_array_has_vnum(iprogs, script->vnum))
            continue;
        script_json = json_area_serialize_script(script);
        if (script_json) json_array_append_new(iprogs, script_json);
    }
    
    // DUNGEONprogs
    for (script = area->dprog_list; script; script = script->next) {
        if (json_script_array_has_vnum(dprogs, script->vnum))
            continue;
        script_json = json_area_serialize_script(script);
        if (script_json) json_array_append_new(dprogs, script_json);
    }

    // QUESTprogs
    for (script = area->qprog_list; script; script = script->next) {
        if (json_script_array_has_vnum(qprogs, script->vnum))
            continue;
        script_json = json_area_serialize_script(script);
        if (script_json) json_array_append_new(qprogs, script_json);
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

    if (json_array_size(qprogs) > 0)
        json_object_set_new(root, "qprogs", qprogs);
    else
        json_decref(qprogs);
    
    /* Cache in Redis immediately (if available) */
    if (redis_is_available() && area->file_name && area->file_name[0]) {
        char *json_str = json_dumps(root, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
        if (json_str) {
            if (redis_cache_area_state(area->file_name, json_str)) {
                log_stringf("json_area_save: Cached area '%s' in Redis", area->name);
            }
            free(json_str);
        }
    }

    /* Write to file with pretty printing */
    if (!json_file_save(root, path, "json_area_save", JSON_INDENT(2) | JSON_PRESERVE_ORDER)) {
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
    
    // Always iterate through all hash buckets to catch widevnum entities
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (ROOM_INDEX_DATA *room = area->room_index_hash[j]; room; room = room->next) {
            if (room->vnum && room->area == area) {
                json_t *room_json = json_area_serialize_room(room);
                if (room_json) json_array_append_new(rooms, room_json);
            }
        }
    }
    json_object_set_new(root, "rooms", rooms);

    /* Serialize mobiles */
    json_t *mobiles = json_array();
    
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (MOB_INDEX_DATA *mob = area->mob_index_hash[j]; mob; mob = mob->next) {
            if (mob->vnum && mob->area == area) {
                json_t *mob_json = json_area_serialize_mobile(mob);
                if (mob_json) json_array_append_new(mobiles, mob_json);
            }
        }
    }
    json_object_set_new(root, "mobiles", mobiles);

    /* Serialize objects */
    json_t *objects = json_array();
    
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (OBJ_INDEX_DATA *obj = area->obj_index_hash[j]; obj; obj = obj->next) {
            if (obj->vnum && obj->area == area) {
                json_t *obj_json = json_area_serialize_object(obj);
                if (obj_json) json_array_append_new(objects, obj_json);
            }
        }
    }
    json_object_set_new(root, "objects", objects);

    /* Serialize tokens */
    json_t *tokens = json_array();
    
    for (int j = 0; j < MAX_KEY_HASH; j++) {
        for (TOKEN_INDEX_DATA *token = area->token_index_hash[j]; token; token = token->next) {
            if (token->vnum && token->area == area) {
                json_t *token_json = json_area_serialize_token(token);
                if (token_json) json_array_append_new(tokens, token_json);
            }
        }
    }
    if (json_array_size(tokens) > 0) {
        json_object_set_new(root, "tokens", tokens);
    } else {
        json_decref(tokens);
    }

    /* Convert to pretty-printed JSON string for readability in Redis */
    result = json_dumps(root, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
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

    if (room->region && room->region != &room->area->region)
        json_object_set_new(json, "region_uid", json_integer(room->region->uid));
    
    if (room->home_owner && room->home_owner[0] != '\0')
        json_object_set_new(json, "home_owner", json_string(room->home_owner));
    
    // Flags and sector
    json_object_set_new(json, "flags", flags_to_json_array(room->rs_room_flag[0], room_flags));
    json_object_set_new(json, "flags2", flags_to_json_array(room->rs_room_flag[1], room2_flags));
    json_object_set_new(json, "sector", json_string(sector_name(room_rs_sector_type(room))));
    
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
            /* Use widevnum format for room recall */
            ROOM_INDEX_DATA *recall_room = get_room_index_global(room->rs_recall.id[0]);
            if (recall_room) {
                json_object_set_new(recall, "vnum", json_string(widevnum_string_room(recall_room, NULL)));
            } else {
                json_object_set_new(recall, "vnum", json_integer(room->rs_recall.id[0]));
            }
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

    area_region_add_room(&area->region, room);

    {
        long region_uid = json_get_int_default(json, "region_uid", 0);
        if (region_uid > 0) {
            AREA_REGION *region = get_area_region_by_uid(area, region_uid);
            if (region)
                area_region_add_room(region, room);
        }
    }
    
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
    
    {
        json_t *sector_json = json_object_get(json, "sector");
        int sector_index = SECT_INSIDE;

        if (sector_json && json_is_string(sector_json)) {
            const char *sector_value = json_string_value(sector_json);
            int lookup = sector_lookup(sector_value);
            if (lookup != NO_FLAG)
                sector_index = lookup;
        } else {
            sector_index = json_get_int_default(json, "sector", SECT_INSIDE);
        }

        room_set_rs_sector_type(room, sector_index);
    }
    
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
            /* Handle widevnum string or legacy integer */
            json_t *vnum_json = json_object_get(recall, "vnum");
            if (vnum_json && json_is_string(vnum_json)) {
                WNUM_LOAD wload;
                if (parse_widevnum_load(json_string_value(vnum_json), &wload)) {
                    room->rs_recall.id[0] = wload.vnum;
                }
            } else {
                room->rs_recall.id[0] = json_get_int_default(recall, "vnum", 0);
            }
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
    if (mob->corpse_load.vnum)
        json_object_set_new(json, "corpse_vnum", json_integer(mob->corpse_load.vnum));
    if (mob->zombie_load.vnum)
        json_object_set_new(json, "zombie_vnum", json_integer(mob->zombie_load.vnum));
    
    if (mob->boss)
        json_object_set_new(json, "boss", json_true());

    if (mob->mob_reputations)
    {
        json_t *reputation_rewards = json_array();
        MOB_REPUTATION_DATA *rep;

        for (rep = mob->mob_reputations; rep; rep = rep->next)
        {
            if (!IS_VALID(rep->reputation))
                continue;

            json_t *entry = json_object();
            json_object_set_new(entry,
                                "reputation",
                                json_string(widevnum_string(rep->reputation->area,
                                                            rep->reputation->vnum,
                                                            mob->area)));
            json_object_set_new(entry, "minimum_rank", json_integer(rep->minimum_rank));
            json_object_set_new(entry, "maximum_rank", json_integer(rep->maximum_rank));
            json_object_set_new(entry, "points", json_integer(rep->points));
            json_array_append_new(reputation_rewards, entry);
        }

        if (json_array_size(reputation_rewards) > 0)
            json_object_set_new(json, "reputation_rewards", reputation_rewards);
        else
            json_decref(reputation_rewards);
    }
    
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
    
    /* Questor */
    if (mob->pQuestor) {
        json_t *questor = json_object();
        json_object_set_new(questor, "scroll", json_integer(mob->pQuestor->scroll));
        if (mob->pQuestor->keywords && mob->pQuestor->keywords[0] != '\0')
            json_object_set_new(questor, "keywords", json_string(mob->pQuestor->keywords));
        if (mob->pQuestor->short_descr && mob->pQuestor->short_descr[0] != '\0')
            json_object_set_new(questor, "short_descr", json_string(mob->pQuestor->short_descr));
        if (mob->pQuestor->long_descr && mob->pQuestor->long_descr[0] != '\0')
            json_object_set_new(questor, "long_descr", json_string(mob->pQuestor->long_descr));
        if (mob->pQuestor->header && mob->pQuestor->header[0] != '\0')
            json_object_set_new(questor, "header", json_string(mob->pQuestor->header));
        if (mob->pQuestor->footer && mob->pQuestor->footer[0] != '\0')
            json_object_set_new(questor, "footer", json_string(mob->pQuestor->footer));
        if (mob->pQuestor->prefix && mob->pQuestor->prefix[0] != '\0')
            json_object_set_new(questor, "prefix", json_string(mob->pQuestor->prefix));
        if (mob->pQuestor->suffix && mob->pQuestor->suffix[0] != '\0')
            json_object_set_new(questor, "suffix", json_string(mob->pQuestor->suffix));
        if (mob->pQuestor->line_width != 70)
            json_object_set_new(questor, "line_width", json_integer(mob->pQuestor->line_width));
        json_object_set_new(json, "questor", questor);
    }

    /* Trainer */
    if (mob->pTrainer) {
        json_t *trainer = json_object();
        if (mob->pTrainer->greeting && mob->pTrainer->greeting[0] != '\0')
            json_object_set_new(trainer, "greeting", json_string(mob->pTrainer->greeting));
        if (mob->pTrainer->flags)
            json_object_set_new(trainer, "flags", json_integer(mob->pTrainer->flags));

        json_t *entries = json_array();
        TRAINER_ENTRY *entry;
        for (entry = mob->pTrainer->entries; entry; entry = entry->next) {
            if (!IS_VALID(entry)) continue;
            json_t *jentry = json_object();
            json_object_set_new(jentry, "skill", json_string(entry->skill_name));
            if (IS_VALID(entry->reputation))
                json_object_set_new(jentry,
                                    "reputation",
                                    json_string(widevnum_string(entry->reputation->area,
                                                                entry->reputation->vnum,
                                                                mob->area)));
            if (entry->min_reputation_rank > 0)
                json_object_set_new(jentry, "min_reputation_rank", json_integer(entry->min_reputation_rank));
            if (entry->max_reputation_rank > 0)
                json_object_set_new(jentry, "max_reputation_rank", json_integer(entry->max_reputation_rank));
            if (entry->max_rating)
                json_object_set_new(jentry, "max_rating", json_integer(entry->max_rating));
            if (entry->cost_gold)
                json_object_set_new(jentry, "cost_gold", json_integer(entry->cost_gold));
            if (entry->cost_trains)
                json_object_set_new(jentry, "cost_trains", json_integer(entry->cost_trains));
            if (entry->check_script)
                json_object_set_new(jentry, "check_script", json_string(entry->check_script));
            json_array_append_new(entries, jentry);
        }
        json_object_set_new(trainer, "entries", entries);
        json_object_set_new(json, "trainer", trainer);
    }

    /* Crew */
    if (IS_VALID(mob->pCrew)) {
        json_t *crew = json_object();
        json_object_set_new(crew, "min_rank", json_integer(mob->pCrew->min_rank));
        json_object_set_new(crew, "scouting", json_integer(mob->pCrew->scouting));
        json_object_set_new(crew, "gunning", json_integer(mob->pCrew->gunning));
        json_object_set_new(crew, "oarring", json_integer(mob->pCrew->oarring));
        json_object_set_new(crew, "mechanics", json_integer(mob->pCrew->mechanics));
        json_object_set_new(crew, "navigation", json_integer(mob->pCrew->navigation));
        json_object_set_new(crew, "leadership", json_integer(mob->pCrew->leadership));
        json_object_set_new(json, "crew", crew);
    }
    
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
    mob->corpse_load.vnum = json_get_int_default(json, "corpse_vnum", 0);
    mob->zombie_load.vnum = json_get_int_default(json, "zombie_vnum", 0);
    mob->boss = json_get_bool_default(json, "boss", false);

    {
        json_t *reputation_rewards = json_object_get(json, "reputation_rewards");
        if (reputation_rewards && json_is_array(reputation_rewards))
        {
            MOB_REPUTATION_DATA *last = NULL;
            size_t idx;
            json_t *entry;

            json_array_foreach(reputation_rewards, idx, entry)
            {
                if (!json_is_object(entry))
                    continue;

                const char *rep_ref = json_get_string_default(entry, "reputation", NULL);
                if (!rep_ref || rep_ref[0] == '\0')
                    continue;

                WNUM_LOAD rep_load;
                if (!parse_widevnum_load(rep_ref, &rep_load))
                    continue;

                MOB_REPUTATION_DATA *new_rep = new_mob_reputation_data();
                new_rep->reputation_load = rep_load;
                new_rep->minimum_rank = json_get_int_default(entry, "minimum_rank", 0);
                new_rep->maximum_rank = json_get_int_default(entry, "maximum_rank", 0);
                new_rep->points = json_get_int_default(entry, "points", 0);
                new_rep->next = NULL;

                if (last)
                    last->next = new_rep;
                else
                    mob->mob_reputations = new_rep;

                last = new_rep;
            }
        }
    }
    
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
    
    /* Questor */
    {
        json_t *questor = json_object_get(json, "questor");
        if (questor && json_is_object(questor)) {
            mob->pQuestor = new_questor_data();
            mob->pQuestor->scroll = json_get_int_default(questor, "scroll", mob->pQuestor->scroll);
            const char *qstr;
            qstr = json_get_string_default(questor, "keywords", NULL);
            if (qstr) { free_string(mob->pQuestor->keywords); mob->pQuestor->keywords = str_dup(qstr); }
            qstr = json_get_string_default(questor, "short_descr", NULL);
            if (qstr) { free_string(mob->pQuestor->short_descr); mob->pQuestor->short_descr = str_dup(qstr); }
            qstr = json_get_string_default(questor, "long_descr", NULL);
            if (qstr) { free_string(mob->pQuestor->long_descr); mob->pQuestor->long_descr = str_dup(qstr); }
            qstr = json_get_string_default(questor, "header", NULL);
            if (qstr) { free_string(mob->pQuestor->header); mob->pQuestor->header = str_dup(qstr); }
            qstr = json_get_string_default(questor, "footer", NULL);
            if (qstr) { free_string(mob->pQuestor->footer); mob->pQuestor->footer = str_dup(qstr); }
            qstr = json_get_string_default(questor, "prefix", NULL);
            if (qstr) { free_string(mob->pQuestor->prefix); mob->pQuestor->prefix = str_dup(qstr); }
            qstr = json_get_string_default(questor, "suffix", NULL);
            if (qstr) { free_string(mob->pQuestor->suffix); mob->pQuestor->suffix = str_dup(qstr); }
            mob->pQuestor->line_width = json_get_int_default(questor, "line_width", 70);
        }
    }

    /* Trainer */
    {
        json_t *trainer = json_object_get(json, "trainer");
        if (trainer && json_is_object(trainer)) {
            mob->pTrainer = new_trainer_data();
            const char *greeting = json_get_string_default(trainer, "greeting", NULL);
            if (greeting)
                mob->pTrainer->greeting = str_dup(greeting);
            mob->pTrainer->flags = json_get_int_default(trainer, "flags", 0);

            json_t *entries = json_object_get(trainer, "entries");
            if (entries && json_is_array(entries)) {
                TRAINER_ENTRY *last = NULL;
                size_t idx;
                json_t *jentry;
                json_array_foreach(entries, idx, jentry) {
                    const char *sname = json_get_string_default(jentry, "skill", NULL);
                    if (!sname) continue;
                    TRAINER_ENTRY *entry = new_trainer_entry();
                    free_string(entry->skill_name);
                    entry->skill_name = str_dup(sname);
                    entry->max_rating = json_get_int_default(jentry, "max_rating", 0);
                    entry->cost_gold = json_get_int_default(jentry, "cost_gold", 0);
                    entry->cost_trains = json_get_int_default(jentry, "cost_trains", 0);
                    {
                        const char *rep_ref = json_get_string_default(jentry, "reputation", "");
                        if (rep_ref && rep_ref[0] != '\0')
                            parse_widevnum_load(rep_ref, &entry->reputation_load);
                        entry->min_reputation_rank = json_get_int_default(jentry, "min_reputation_rank", 0);
                        entry->max_reputation_rank = json_get_int_default(jentry, "max_reputation_rank", 0);
                    }
                    const char *script = json_get_string_default(jentry, "check_script", NULL);
                    if (script)
                        entry->check_script = str_dup(script);
                    JSON_APPEND_LINK(mob->pTrainer->entries, last, entry);
                }
            }
        }
    }

    /* Crew */
    {
        json_t *crew = json_object_get(json, "crew");
        if (crew && json_is_object(crew)) {
            mob->pCrew = new_ship_crew_index();
            mob->pCrew->min_rank = json_get_int_default(crew, "min_rank", 0);
            mob->pCrew->scouting = json_get_int_default(crew, "scouting", 0);
            mob->pCrew->gunning = json_get_int_default(crew, "gunning", 0);
            mob->pCrew->oarring = json_get_int_default(crew, "oarring", 0);
            mob->pCrew->mechanics = json_get_int_default(crew, "mechanics", 0);
            mob->pCrew->navigation = json_get_int_default(crew, "navigation", 0);
            mob->pCrew->leadership = json_get_int_default(crew, "leadership", 0);
        }
    }

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
    
    // Legacy values[] — retained as transitional backup until Phase 5 is complete.
    // Allows recovery from .are files for objects that lost data during
    // premature Phase 4 value[] removal.
    {
        json_t *vals = json_array();
        for (int i = 0; i < 8; i++)
            json_array_append_new(vals, json_integer(obj->value[i]));
        json_object_set_new(json, "values", vals);
    }

    // Type-specific data (canonical structured representation)
    {
        json_t *td = obj_index_type_data_to_json(obj);
        if (td) {
            json_object_set_new(json, "type_data", td);
        }
    }

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
    for (CATALYST_DATA *cat = obj->catalyst; cat; cat = cat->next) {
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
        /* Use widevnum format for key */
        if (obj->lock->key_load.vnum > 0) {
            OBJ_INDEX_DATA *key_obj = obj->lock->key_wnum.pArea ?
                get_obj_index(obj->lock->key_wnum.pArea, obj->lock->key_wnum.vnum) :
                get_obj_index_global(obj->lock->key_load.vnum);
            if (key_obj) {
                json_object_set_new(lock, "key_vnum", json_string(widevnum_string_object(key_obj, NULL)));
            } else {
                json_object_set_new(lock, "key_vnum", json_integer(obj->lock->key_load.vnum));
            }
        }
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
    
    /* Spells */
    if (obj->spells) {
        json_t *spell_array = json_array();
        SPELL_DATA *spell;
        for (spell = obj->spells; spell; spell = spell->next) {
            json_t *sp = json_object();
            json_object_set_new(sp, "name", json_string(skill_table[spell->sn].name));
            json_object_set_new(sp, "level", json_integer(spell->level));
            json_object_set_new(sp, "repop", json_integer(spell->repop));
            json_array_append_new(spell_array, sp);
        }
        if (json_array_size(spell_array) > 0) {
            json_object_set_new(json, "spells", spell_array);
        } else {
            json_decref(spell_array);
        }
    }
    
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
    // Portal destination area UID (value[4]) resolved post-boot by fix_portal_destinations()

    // Type-specific data (canonical structured representation)
    {
        json_t *td = json_object_get(json, "type_data");
        if (td && json_is_object(td)) {
            obj_index_type_data_from_json(obj, td);
        } else if (values && json_is_array(values)) {
            // No type_data in JSON — migrate from legacy values[]
            obj_index_migrate_values_to_types(obj);
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
            CATALYST_DATA *cat = json_area_deserialize_catalyst(catalyst_json);
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
            JSON_APPEND_LINK(obj->extra_descr, ed_last, ed);
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
        /* Handle widevnum string or legacy integer for key_vnum */
        json_t *key_json = json_object_get(lock, "key_vnum");
        if (key_json && json_is_string(key_json)) {
            WNUM_LOAD wload;
            if (parse_widevnum_load(json_string_value(key_json), &wload)) {
                obj->lock->key_load = wload;
            }
        } else {
            obj->lock->key_load.vnum = json_get_int_default(lock, "key_vnum", 0);
        }
        
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
    
    /* Spells - load from explicit spell array */
    {
        json_t *spell_array = json_object_get(json, "spells");
        if (spell_array && json_is_array(spell_array)) {
            SPELL_DATA *last_spell = NULL;
            size_t sidx;
            json_t *selem;
            json_array_foreach(spell_array, sidx, selem) {
                const char *spell_name = json_get_string_default(selem, "name", NULL);
                if (!spell_name) continue;
                int sn = skill_lookup(spell_name);
                if (sn < 0) continue;
                if (!str_cmp(skill_table[sn].name, "reserved")
                ||  !str_cmp(skill_table[sn].name, "none"))
                    continue;
                SPELL_DATA *spell = new_spell();
                spell->sn = sn;
                spell->level = json_get_int_default(selem, "level", 0);
                spell->repop = json_get_int_default(selem, "repop", 0);
                JSON_APPEND_LINK(obj->spells, last_spell, spell);
            }
        }
    }

    /* Migrate legacy value-based spells to obj->spells.
     * Old format stored spell data in value[] array:
     *   Armour/Weapon/Ranged: value[5]=level, value[6]=spell1, value[7]=spell2
     *   Light:                value[3]=level, value[4]=spell1, value[5]=spell2
     *   Artifact:             value[0]=level, value[1]=spell1, value[2]=spell2
     *   Scroll/Pill/Potion:   value[0]=level, value[1-4]=spells
     *   Wand/Staff:           value[0]=level, value[3]=spell
     * After migration, the value slots are zeroed to prevent double-migration. */
    if (obj->spells == NULL) {
        int legacy_values[8];
        for (int vi = 0; vi < 8; vi++)
            legacy_values[vi] = obj->value[vi];

        switch (obj->item_type) {
        case ITEM_ARMOUR:
        case ITEM_WEAPON:
        case ITEM_RANGED_WEAPON:
            if (legacy_values[5] > 0) {
                if (legacy_values[6] > 0 && legacy_values[6] < MAX_SKILL
                &&  skill_table[legacy_values[6]].spell_fun != spell_null) {
                    SPELL_DATA *sp = new_spell();
                    sp->sn = legacy_values[6];
                    sp->level = legacy_values[5];
                    sp->repop = 100;
                    sp->next = obj->spells;
                    obj->spells = sp;
                }
                if (legacy_values[7] > 0 && legacy_values[7] < MAX_SKILL
                &&  skill_table[legacy_values[7]].spell_fun != spell_null) {
                    SPELL_DATA *sp = new_spell();
                    sp->sn = legacy_values[7];
                    sp->level = legacy_values[5];
                    sp->repop = 100;
                    sp->next = obj->spells;
                    obj->spells = sp;
                }
                legacy_values[5] = 0;
                legacy_values[6] = 0;
                legacy_values[7] = 0;
            }
            break;

        case ITEM_LIGHT:
            if (legacy_values[3] > 0) {
                if (legacy_values[4] > 0 && legacy_values[4] < MAX_SKILL
                &&  skill_table[legacy_values[4]].spell_fun != spell_null) {
                    SPELL_DATA *sp = new_spell();
                    sp->sn = legacy_values[4];
                    sp->level = legacy_values[3];
                    sp->repop = 100;
                    sp->next = obj->spells;
                    obj->spells = sp;
                }
                if (legacy_values[5] > 0 && legacy_values[5] < MAX_SKILL
                &&  skill_table[legacy_values[5]].spell_fun != spell_null) {
                    SPELL_DATA *sp = new_spell();
                    sp->sn = legacy_values[5];
                    sp->level = legacy_values[3];
                    sp->repop = 100;
                    sp->next = obj->spells;
                    obj->spells = sp;
                }
                legacy_values[3] = 0;
                legacy_values[4] = 0;
                legacy_values[5] = 0;
            }
            break;

        case ITEM_ARTIFACT:
            if (legacy_values[0] > 0) {
                if (legacy_values[1] > 0 && legacy_values[1] < MAX_SKILL
                &&  skill_table[legacy_values[1]].spell_fun != spell_null) {
                    SPELL_DATA *sp = new_spell();
                    sp->sn = legacy_values[1];
                    sp->level = legacy_values[0];
                    sp->repop = 100;
                    sp->next = obj->spells;
                    obj->spells = sp;
                }
                if (legacy_values[2] > 0 && legacy_values[2] < MAX_SKILL
                &&  skill_table[legacy_values[2]].spell_fun != spell_null) {
                    SPELL_DATA *sp = new_spell();
                    sp->sn = legacy_values[2];
                    sp->level = legacy_values[0];
                    sp->repop = 100;
                    sp->next = obj->spells;
                    obj->spells = sp;
                }
                legacy_values[0] = 0;
                legacy_values[1] = 0;
                legacy_values[2] = 0;
            }
            break;

        case ITEM_SCROLL:
        case ITEM_PILL:
        case ITEM_POTION:
            if (legacy_values[0] > 0) {
                for (int vi = 1; vi <= 4; vi++) {
                    if (legacy_values[vi] > 0 && legacy_values[vi] < MAX_SKILL
                    &&  skill_table[legacy_values[vi]].spell_fun != spell_null) {
                        SPELL_DATA *sp = new_spell();
                        sp->sn = legacy_values[vi];
                        sp->level = legacy_values[0];
                        sp->repop = 100;
                        sp->next = obj->spells;
                        obj->spells = sp;
                    }
                }
                for (int vi = 0; vi <= 4; vi++)
                    legacy_values[vi] = 0;
            }
            break;

        case ITEM_WAND:
        case ITEM_STAFF:
            if (legacy_values[0] > 0) {
                if (legacy_values[3] > 0 && legacy_values[3] < MAX_SKILL
                &&  skill_table[legacy_values[3]].spell_fun != spell_null) {
                    SPELL_DATA *sp = new_spell();
                    sp->sn = legacy_values[3];
                    sp->level = legacy_values[0];
                    sp->repop = 100;
                    sp->next = obj->spells;
                    obj->spells = sp;
                }
                legacy_values[0] = 0;
                legacy_values[3] = 0;
            }
            break;

        default:
            break;
        }

        for (int vi = 0; vi < 8; vi++)
            obj->value[vi] = legacy_values[vi];
    }

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
        long value = flag_value(script_flags, (char *)flags_str);
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

static bool json_script_array_has_vnum(json_t *scripts, long vnum)
{
    if (!scripts || !json_is_array(scripts))
        return false;

    size_t index;
    json_t *entry;
    json_array_foreach(scripts, index, entry) {
        if (!json_is_object(entry))
            continue;
        if (json_get_int_default(entry, "vnum", 0) == vnum)
            return true;
    }

    return false;
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

/*
 * Get descriptive field name for arg1 based on reset command
 */
static const char *reset_arg1_field_name(char command)
{
    switch (command) {
        case 'M': return "mob_vnum";
        case 'O': return "object_vnum";
        case 'G': return "object_vnum";
        case 'E': return "object_vnum";
        case 'P': return "object_vnum";
        case 'D': return "room_vnum";
        case 'R': return "room_vnum";
        default:  return "arg1";  // Fallback for unknown commands
    }
}

/*
 * Get descriptive field name for arg2 based on reset command
 */
static const char *reset_arg2_field_name(char command)
{
    switch (command) {
        case 'M': return "limit";
        case 'O': return "limit";
        case 'G': return "limit";
        case 'E': return "limit";
        case 'P': return "limit";
        case 'D': return "exit_direction";
        case 'R': return "num_exits";
        default:  return "arg2";  // Fallback
    }
}

/*
 * Get descriptive field name for arg3 based on reset command
 */
static const char *reset_arg3_field_name(char command)
{
    switch (command) {
        case 'M': return "room_vnum";
        case 'O': return "room_vnum";
        case 'E': return "wear_location";
        case 'P': return "container_vnum";
        case 'D': return "door_state";
        default:  return "arg3";  // Fallback (G, R use arg3=0)
    }
}

/*
 * Get descriptive field name for arg4 based on reset command
 */
static const char *reset_arg4_field_name(char command)
{
    switch (command) {
        case 'M': return "chance";
        case 'O': return "chance";
        default:  return "arg4";  // Most commands don't use arg4
    }
}

json_t *json_area_serialize_reset(RESET_DATA *reset)
{
    if (!reset) return NULL;
    
    json_t *json = json_object();
    if (!json) return NULL;
    
    json_object_set_new(json, "command", json_string(reset_command_to_string(reset->command)));
    json_object_set_new(json, reset_arg2_field_name(reset->command), json_integer(reset->arg2));
    json_object_set_new(json, reset_arg4_field_name(reset->command), json_integer(reset->arg4));
    
    // Save arg1 and arg3 with descriptive names based on command type
    // Also save area UIDs for cross-area references (when pArea is not NULL)
    long arg1_val, arg3_val;
    const char *arg1_field = reset_arg1_field_name(reset->command);
    const char *arg3_field = reset_arg3_field_name(reset->command);
    char area_uid_field[64];
    
    switch (reset->command) {
        case 'M': // Mobile: arg1=mob(wnum), arg3=room(value)
        case 'O': // Object: arg1=obj(wnum), arg3=room(value)
        case 'G': // Give: arg1=obj(wnum), arg3=unused
        case 'E': // Equip: arg1=obj(wnum), arg3=wear_loc(value)
            arg1_val = reset->arg1.wnum.vnum;
            arg3_val = reset->arg3.value;
            // Save area UID for cross-area entity references
            if (reset->arg1.wnum.pArea) {
                snprintf(area_uid_field, sizeof(area_uid_field), "%s_area_uid", arg1_field);
                json_object_set_new(json, area_uid_field, json_integer(reset->arg1.wnum.pArea->uid));
            }
            break;
            
        case 'P': // Put: arg1=obj(wnum), arg3=container(wnum)
            arg1_val = reset->arg1.wnum.vnum;
            arg3_val = reset->arg3.wnum.vnum;
            // Save area UIDs for both object and container
            if (reset->arg1.wnum.pArea) {
                snprintf(area_uid_field, sizeof(area_uid_field), "%s_area_uid", arg1_field);
                json_object_set_new(json, area_uid_field, json_integer(reset->arg1.wnum.pArea->uid));
            }
            if (reset->arg3.wnum.pArea) {
                snprintf(area_uid_field, sizeof(area_uid_field), "%s_area_uid", arg3_field);
                json_object_set_new(json, area_uid_field, json_integer(reset->arg3.wnum.pArea->uid));
            }
            break;
            
        case 'D': // Door: arg1=room(value), arg3=state(value)
        case 'R': // Randomize: arg1=room(value), arg3=unused
            arg1_val = reset->arg1.value;
            arg3_val = reset->arg3.value;
            break;
            
        default:
            // Unknown command - try wnum
            arg1_val = reset->arg1.wnum.vnum;
            arg3_val = reset->arg3.wnum.vnum;
            break;
    }
    
    json_object_set_new(json, arg1_field, json_integer(arg1_val));
    json_object_set_new(json, arg3_field, json_integer(arg3_val));
    
    return json;
}

RESET_DATA *json_area_deserialize_reset(json_t *json, AREA_DATA *area)
{
    if (!json) return NULL;
    
    RESET_DATA *reset = alloc_perm(sizeof(*reset));
    if (!reset) return NULL;
    
    const char *cmd_str = json_get_string_default(json, "command", "stop");
    reset->command = reset_string_to_command(cmd_str);
    
    // Read descriptive field names with fallback to generic names for backwards compatibility
    const char *arg2_field = reset_arg2_field_name(reset->command);
    const char *arg4_field = reset_arg4_field_name(reset->command);
    
    // Try descriptive name first, fall back to generic "arg2"/"arg4"
    json_t *arg2_json = json_object_get(json, arg2_field);
    if (!arg2_json) arg2_json = json_object_get(json, "arg2");
    reset->arg2 = arg2_json ? json_integer_value(arg2_json) : 0;
    
    json_t *arg4_json = json_object_get(json, arg4_field);
    if (!arg4_json) arg4_json = json_object_get(json, "arg4");
    reset->arg4 = arg4_json ? json_integer_value(arg4_json) : 0;
    
    // Read descriptive field names with fallback to generic names for backwards compatibility
    const char *arg1_field = reset_arg1_field_name(reset->command);
    const char *arg3_field = reset_arg3_field_name(reset->command);
    char area_uid_field[64];
    
    // Try descriptive name first, fall back to generic "arg1"/"arg3"
    json_t *arg1_json = json_object_get(json, arg1_field);
    if (!arg1_json) arg1_json = json_object_get(json, "arg1");
    long arg1_val = arg1_json ? json_integer_value(arg1_json) : 0;
    
    json_t *arg3_json = json_object_get(json, arg3_field);
    if (!arg3_json) arg3_json = json_object_get(json, "arg3");
    long arg3_val = arg3_json ? json_integer_value(arg3_json) : 0;
    
    // Load area UIDs for cross-area references
    snprintf(area_uid_field, sizeof(area_uid_field), "%s_area_uid", arg1_field);
    json_t *arg1_auid_json = json_object_get(json, area_uid_field);
    if (!arg1_auid_json) arg1_auid_json = json_object_get(json, "arg1_area_uid");  // Fallback
    long arg1_area_uid = arg1_auid_json ? json_integer_value(arg1_auid_json) : 0;
    
    snprintf(area_uid_field, sizeof(area_uid_field), "%s_area_uid", arg3_field);
    json_t *arg3_auid_json = json_object_get(json, area_uid_field);
    if (!arg3_auid_json) arg3_auid_json = json_object_get(json, "arg3_area_uid");  // Fallback
    long arg3_area_uid = arg3_auid_json ? json_integer_value(arg3_auid_json) : 0;
    
    // Set union fields based on command type
    switch (reset->command) {
        case 'M': // Mobile: arg1=mob(wnum), arg3=room(value)
        case 'O': // Object: arg1=obj(wnum), arg3=room(value)
        case 'G': // Give: arg1=obj(wnum), arg3=unused
        case 'E': // Equip: arg1=obj(wnum), arg3=wear_loc(value)
            // Store as WNUM_LOAD for later resolution
            reset->arg1.load.auid = arg1_area_uid;
            reset->arg1.load.vnum = arg1_val;
            reset->arg3.value = arg3_val;
            break;
            
        case 'P': // Put: arg1=obj(wnum), arg3=container(wnum)
            // Store both as WNUM_LOAD for later resolution
            reset->arg1.load.auid = arg1_area_uid;
            reset->arg1.load.vnum = arg1_val;
            reset->arg3.load.auid = arg3_area_uid;
            reset->arg3.load.vnum = arg3_val;
            break;
            
        case 'D': // Door: arg1=room(value), arg3=state(value)
        case 'R': // Randomize: arg1=room(value), arg3=unused
            reset->arg1.value = arg1_val;
            reset->arg3.value = arg3_val;
            break;
            
        default:
            // Unknown command - use WNUM for safety
            reset->arg1.wnum.pArea = NULL;
            reset->arg1.wnum.vnum = arg1_val;
            reset->arg3.wnum.pArea = NULL;
            reset->arg3.wnum.vnum = arg3_val;
            break;
    }
    
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
    
    /* Stock type and vnum - use widevnum format */
    json_object_set_new(json, "type", json_integer(stock->type));

    switch (stock->type) {
        case STOCK_OBJECT: {
            AREA_DATA *stock_area = stock->entity.wnum.pArea;
            if (!stock_area && stock->obj && stock->obj->area)
                stock_area = stock->obj->area;
            json_object_set_new(json, "vnum", json_string(widevnum_string(stock_area, stock->entity.wnum.vnum, NULL)));
            break;
        }
        case STOCK_PET:
        case STOCK_MOUNT:
        case STOCK_GUARD:
        case STOCK_CREW: {
            AREA_DATA *stock_area = stock->entity.wnum.pArea;
            if (!stock_area && stock->mob && stock->mob->area)
                stock_area = stock->mob->area;
            json_object_set_new(json, "mob_vnum", json_string(widevnum_string(stock_area, stock->entity.wnum.vnum, NULL)));
            break;
        }
        case STOCK_SHIP: {
            AREA_DATA *stock_area = stock->entity.wnum.pArea;
            if (!stock_area && stock->ship && stock->ship->area)
                stock_area = stock->ship->area;
            json_object_set_new(json, "ship_vnum", json_string(widevnum_string(stock_area, stock->entity.wnum.vnum, NULL)));
            break;
        }
        case STOCK_CUSTOM:
            if (stock->custom_keyword && stock->custom_keyword[0] != '\0')
                json_object_set_new(json, "keyword", json_string(stock->custom_keyword));
            break;
        default:
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

    if (IS_VALID(stock->reputation))
    {
        json_object_set_new(json,
                            "reputation",
                            json_string(widevnum_string(stock->reputation->area,
                                                        stock->reputation->vnum,
                                                        area)));
        if (stock->min_reputation_rank > 0)
            json_object_set_new(json, "min_reputation_rank", json_integer(stock->min_reputation_rank));
        if (stock->max_reputation_rank > 0)
            json_object_set_new(json, "max_reputation_rank", json_integer(stock->max_reputation_rank));
        if (stock->min_show_rank > 0)
            json_object_set_new(json, "min_show_rank", json_integer(stock->min_show_rank));
        if (stock->max_show_rank > 0)
            json_object_set_new(json, "max_show_rank", json_integer(stock->max_show_rank));
    }
    
    return json;
}

SHOP_STOCK_DATA *json_area_deserialize_shop_stock(json_t *json, AREA_DATA *area)
{
    if (!json) return NULL;
    
    SHOP_STOCK_DATA *stock = new_shop_stock();
    if (!stock) return NULL;
    
    /* Stock type */
    stock->type = json_get_int_default(json, "type", STOCK_OBJECT);

    /* Helper macro to parse widevnum string or fall back to legacy separate fields */
    #define PARSE_STOCK_VNUM(vnum_field, auid_field) do { \
        json_t *vnum_json = json_object_get(json, vnum_field); \
        if (vnum_json && json_is_string(vnum_json)) { \
            WNUM_LOAD wload; \
            if (parse_widevnum_load(json_string_value(vnum_json), &wload)) { \
                stock->entity.load.vnum = wload.vnum; \
                stock->entity.load.auid = wload.auid; \
            } \
        } else { \
            stock->entity.load.vnum = json_get_int_default(json, vnum_field, 0); \
            stock->entity.load.auid = json_get_int_default(json, auid_field, 0); \
        } \
    } while(0)

    /* Vnum based on type */
    /* NOTE: Pointer fixup (obj/mob) deferred until after all entities loaded */
    switch (stock->type) {
        case STOCK_OBJECT:
            PARSE_STOCK_VNUM("vnum", "area_uid");
            stock->obj = NULL;  /* Will be fixed up later */
            break;
        case STOCK_PET:
        case STOCK_MOUNT:
        case STOCK_GUARD:
            PARSE_STOCK_VNUM("mob_vnum", "mob_area_uid");
            stock->mob = NULL;  /* Will be fixed up later */
            break;
        case STOCK_SHIP:
            PARSE_STOCK_VNUM("ship_vnum", "ship_area_uid");
            stock->ship = NULL;  /* Will be fixed up later */
            break;
        case STOCK_CREW:
            PARSE_STOCK_VNUM("crew_vnum", "crew_area_uid");
            break;
        case STOCK_CUSTOM:
            stock->custom_keyword = str_dup(json_get_string_default(json, "keyword", ""));
            break;
    }

    #undef PARSE_STOCK_VNUM
    
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

    {
        const char *rep_ref = json_get_string_default(json, "reputation", "");
        if (rep_ref && rep_ref[0] != '\0')
            parse_widevnum_load(rep_ref, &stock->reputation_load);
        stock->min_reputation_rank = json_get_int_default(json, "min_reputation_rank", 0);
        stock->max_reputation_rank = json_get_int_default(json, "max_reputation_rank", 0);
        stock->min_show_rank = json_get_int_default(json, "min_show_rank", 0);
        stock->max_show_rank = json_get_int_default(json, "max_show_rank", 0);
    }
    
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

    if (IS_VALID(shop->reputation))
    {
        json_object_set_new(json,
                            "reputation",
                            json_string(widevnum_string(shop->reputation->area,
                                                        shop->reputation->vnum,
                                                        ref_area)));
        if (shop->min_reputation_rank > 0)
            json_object_set_new(json, "min_reputation_rank", json_integer(shop->min_reputation_rank));
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

    {
        const char *rep_ref = json_get_string_default(json, "reputation", "");
        if (rep_ref && rep_ref[0] != '\0')
            parse_widevnum_load(rep_ref, &shop->reputation_load);
        shop->min_reputation_rank = json_get_int_default(json, "min_reputation_rank", 0);
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

json_t *json_area_serialize_catalyst(CATALYST_DATA *cat)
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

CATALYST_DATA *json_area_deserialize_catalyst(json_t *json)
{
    if (!json) return NULL;
    
    CATALYST_DATA *cat = new_catalyst();
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
    cat->duration = (cat->modifier > 0) ? (cat->level * cat->modifier) : -1;
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
    
    SCRIPT_DATA *group_scripts[MAX_PROG_GROUPS];
    json_t *group_objects[MAX_PROG_GROUPS];
    json_t *group_triggers[MAX_PROG_GROUPS];
    int group_count = 0;
    int slot;
    ITERATOR it;
    PROG_LIST *trigger;
    
    for (slot = 0; slot < TRIGSLOT_MAX; slot++) {
        if (!progs[slot] || list_size(progs[slot]) == 0)
            continue;
            
        iterator_start(&it, progs[slot]);
        while ((trigger = (PROG_LIST *)iterator_nextdata(&it))) {
            const char *trig_name = (trigger->trig_type >= 0 && trigger->trig_type < trigger_table_size) 
                ? trigger_table[trigger->trig_type].name : "unknown";
            const char *phrase = trigger->trig_phrase ? trigger->trig_phrase : "";
            
            json_t *trig_json = json_object();
            if (!trig_json) continue;
            json_object_set_new(trig_json, "type", json_string(trig_name));
            json_object_set_new(trig_json, "phrase", json_string(phrase));
            
            int gi;
            bool found = false;
            for (gi = 0; gi < group_count; gi++) {
                if (group_scripts[gi] == trigger->script) {
                    json_array_append_new(group_triggers[gi], trig_json);
                    found = true;
                    break;
                }
            }
            
            if (!found && group_count < MAX_PROG_GROUPS) {
                json_t *prog_json = json_object();
                if (!prog_json) { json_decref(trig_json); continue; }
                
                json_t *triggers_arr = json_array();
                if (trigger->script && trigger->script->area)
                    json_object_set_new(prog_json, "vnum", json_string(widevnum_string_script(trigger->script, NULL)));
                else
                    json_object_set_new(prog_json, "vnum", json_integer(trigger->vnum));
                json_object_set_new(prog_json, "triggers", triggers_arr);
                json_array_append_new(triggers_arr, trig_json);
                
                group_scripts[group_count] = trigger->script;
                group_objects[group_count] = prog_json;
                group_triggers[group_count] = triggers_arr;
                group_count++;
            }
        }
        iterator_stop(&it);
    }
    
    if (group_count == 0) {
        json_decref(array);
        return NULL;
    }
    
    int gi;
    for (gi = 0; gi < group_count; gi++) {
        json_array_append_new(array, group_objects[gi]);
    }
    
    return array;
}

static LLIST **json_area_deserialize_progs_flat(json_t *json, AREA_DATA *area, int prog_type)
{
    LLIST **progs = new_prog_bank();
    if (!progs) return NULL;
    
    size_t index;
    json_t *prog_json;
    
    json_array_foreach(json, index, prog_json) {
        long vnum = json_get_int_default(prog_json, "vnum", 0);
        if (vnum == 0) continue;
        
        const char *trig_name = json_get_string_default(prog_json, "trigger", "");
        const char *phrase = json_get_string_default(prog_json, "phrase", "");
        
        int tindex = trigger_index((char *)trig_name, prog_type);
        if (tindex < 0) {
            log_stringf("json_area_deserialize_progs: unknown trigger type '%s'", trig_name);
            continue;
        }
        
        int slot = trigger_table[tindex].slot;
        if (slot < 0 || slot >= TRIGSLOT_MAX) {
            log_stringf("json_area_deserialize_progs: invalid slot %d for trigger '%s'", slot, trig_name);
            continue;
        }
        
        PROG_LIST *trigger = new_trigger();
        trigger->vnum = vnum;
        trigger->trig_type = tindex;
        trigger->trig_phrase = str_dup(phrase);
        if (is_widevnum_format(phrase)) {
            trigger->numeric = true;
            trigger->trig_is_widevnum = true;
            parse_widevnum_load(phrase, &trigger->trig_load);
            trigger->trig_number = (int)trigger->trig_load.vnum;
        } else {
            trigger->numeric = json_get_bool_default(prog_json, "numeric", false);
            trigger->trig_number = trigger->numeric ?
                json_get_int_default(prog_json, "number", 0) :
                atoi(phrase);
        }
        
        trigger->script = NULL;
        list_appendlink(progs[slot], trigger);
    }
    
    return progs;
}

static LLIST **json_area_deserialize_progs_grouped(json_t *json, AREA_DATA *area, int prog_type)
{
    LLIST **progs = new_prog_bank();
    if (!progs) return NULL;
    
    size_t gindex;
    json_t *group_json;
    
    json_array_foreach(json, gindex, group_json) {
        json_t *vnum_json = json_object_get(group_json, "vnum");
        if (!vnum_json) continue;
        
        WNUM_LOAD script_load = {0, 0};
        long bare_vnum = 0;
        bool has_widevnum = false;
        
        if (json_is_string(vnum_json)) {
            const char *vnum_str = json_string_value(vnum_json);
            if (is_widevnum_format(vnum_str)) {
                parse_widevnum_load(vnum_str, &script_load);
                bare_vnum = script_load.vnum;
                has_widevnum = true;
            } else {
                bare_vnum = atol(vnum_str);
            }
        } else {
            bare_vnum = json_integer_value(vnum_json);
        }
        if (bare_vnum == 0) continue;
        
        json_t *triggers_arr = json_object_get(group_json, "triggers");
        if (!triggers_arr || !json_is_array(triggers_arr)) continue;
        
        size_t tindex_arr;
        json_t *trig_json;
        
        json_array_foreach(triggers_arr, tindex_arr, trig_json) {
            const char *trig_name = json_get_string_default(trig_json, "type", "");
            const char *phrase = json_get_string_default(trig_json, "phrase", "");
            
            int tindex = trigger_index((char *)trig_name, prog_type);
            if (tindex < 0) {
                log_stringf("json_area_deserialize_progs: unknown trigger type '%s'", trig_name);
                continue;
            }
            
            int slot = trigger_table[tindex].slot;
            if (slot < 0 || slot >= TRIGSLOT_MAX) {
                log_stringf("json_area_deserialize_progs: invalid slot %d for trigger '%s'", slot, trig_name);
                continue;
            }
            
            PROG_LIST *trigger = new_trigger();
            trigger->vnum = bare_vnum;
            trigger->script_is_widevnum = has_widevnum;
            trigger->script_load = script_load;
            trigger->trig_type = tindex;
            trigger->trig_phrase = str_dup(phrase);
            if (is_widevnum_format(phrase)) {
                trigger->numeric = true;
                trigger->trig_is_widevnum = true;
                parse_widevnum_load(phrase, &trigger->trig_load);
                trigger->trig_number = (int)trigger->trig_load.vnum;
            } else {
                trigger->numeric = is_number((char *)phrase);
                trigger->trig_number = atoi(phrase);
            }
            
            trigger->script = NULL;
            list_appendlink(progs[slot], trigger);
        }
    }
    
    return progs;
}

LLIST **json_area_deserialize_progs(json_t *json, AREA_DATA *area, int prog_type)
{
    if (!json || !json_is_array(json))
        return NULL;
    
    json_t *first = json_array_get(json, 0);
    if (!first)
        return NULL;
    
    if (json_object_get(first, "triggers"))
        return json_area_deserialize_progs_grouped(json, area, prog_type);
    else
        return json_area_deserialize_progs_flat(json, area, prog_type);
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
                    json_object_set_new(var_json, "vnum", json_string(widevnum_string_room(var->_.r, NULL)));
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
            /* Handle widevnum string or legacy integer */
            long vnum = 0;
            json_t *vnum_json = json_object_get(var_json, "vnum");
            if (vnum_json && json_is_string(vnum_json)) {
                WNUM_LOAD wload;
                if (parse_widevnum_load(json_string_value(vnum_json), &wload)) {
                    vnum = wload.vnum;
                }
            } else {
                vnum = json_get_int_default(var_json, "vnum", 0);
            }
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
        
        /* Object vnum - use widevnum format */
        if (trade->obj_load.vnum > 0) {
            OBJ_INDEX_DATA *trade_obj_idx = get_obj_index_global(trade->obj_load.vnum);
            if (trade_obj_idx) {
                json_object_set_new(trade_obj, "obj_vnum", json_string(widevnum_string_object(trade_obj_idx, NULL)));
            } else {
                json_object_set_new(trade_obj, "obj_vnum", json_integer(trade->obj_load.vnum));
            }
        }
        
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
 * Serialize a blueprint section to JSON
 */
json_t *json_area_serialize_blueprint_section(BLUEPRINT_SECTION *section, AREA_DATA *area)
{
    if (!section) return NULL;
    
    json_t *json = json_object();
    if (!json) return NULL;
    
    /* Basic info */
    json_object_set_new(json, "vnum", json_integer(section->vnum));
    json_object_set_new(json, "name", json_string(section->name ? section->name : ""));
    json_object_set_new(json, "description", json_string(section->description ? section->description : ""));
    
    if (section->comments && section->comments[0] != '\0')
        json_object_set_new(json, "comments", json_string(section->comments));
    
    /* Type and flags */
    json_object_set_new(json, "type", json_integer(section->type));
    json_object_set_new(json, "flags", json_integer(section->flags));
    
    /* Room range - always save as widevnum string for cross-area support */
    if (section->rooms_area) {
        json_object_set_new(json, "lower_vnum", json_string(widevnum_string(section->rooms_area, section->lower_vnum, area)));
        json_object_set_new(json, "upper_vnum", json_string(widevnum_string(section->rooms_area, section->upper_vnum, area)));
    } else {
        json_object_set_new(json, "lower_vnum", json_integer(section->lower_vnum));
        json_object_set_new(json, "upper_vnum", json_integer(section->upper_vnum));
    }
    
    /* Maze data (only for BSTYPE_MAZE sections) */
    if (section->type == BSTYPE_MAZE) {
        json_object_set_new(json, "maze_w", json_integer(section->maze_x));
        json_object_set_new(json, "maze_h", json_integer(section->maze_y));
        
        if (section->maze_templates && list_size(section->maze_templates) > 0) {
            json_t *mt_array = json_array();
            ITERATOR it;
            MAZE_WEIGHTED_ROOM *mwr;
            iterator_start(&it, section->maze_templates);
            while ((mwr = (MAZE_WEIGHTED_ROOM *)iterator_nextdata(&it))) {
                json_t *mt_json = json_object();
                json_object_set_new(mt_json, "weight", json_integer(mwr->weight));
                if (mwr->exit_count > 0)
                    json_object_set_new(mt_json, "exit_count", json_integer(mwr->exit_count));
                if (mwr->room) {
                    json_object_set_new(mt_json, "room", json_string(widevnum_string_room(mwr->room, NULL)));
                } else {
                    json_object_set_new(mt_json, "room", json_integer(mwr->room_ref.load.vnum));
                }

                /* Exit template properties */
                if (mwr->exit_template.flags & EX_ISDOOR) {
                    json_t *et_json = json_object();
                    json_object_set_new(et_json, "flags", flags_to_json_array(mwr->exit_template.flags, exit_flags));
                    if (mwr->exit_template.keyword && mwr->exit_template.keyword[0] != '\0')
                        json_object_set_new(et_json, "keyword", json_string(mwr->exit_template.keyword));
                    if (mwr->exit_template.strength > 0)
                        json_object_set_new(et_json, "strength", json_integer(mwr->exit_template.strength));
                    if (mwr->exit_template.material && mwr->exit_template.material[0] != '\0')
                        json_object_set_new(et_json, "material", json_string(mwr->exit_template.material));
                    if (mwr->exit_template.lock.key_wnum.pArea && mwr->exit_template.lock.key_wnum.vnum > 0) {
                        OBJ_INDEX_DATA *key_obj = get_obj_index(mwr->exit_template.lock.key_wnum.pArea,
                                                                 mwr->exit_template.lock.key_wnum.vnum);
                        if (key_obj)
                            json_object_set_new(et_json, "key", json_string(widevnum_string_object(key_obj, NULL)));
                        else
                            json_object_set_new(et_json, "key", json_integer(mwr->exit_template.lock.key_wnum.vnum));
                    }
                    if (mwr->exit_template.lock.pick_chance > 0)
                        json_object_set_new(et_json, "pick_chance", json_integer(mwr->exit_template.lock.pick_chance));
                    if (mwr->exit_template.lock.flags)
                        json_object_set_new(et_json, "lock_flags", flags_to_json_array(mwr->exit_template.lock.flags, lock_flags));
                    json_object_set_new(mt_json, "exit_template", et_json);
                }

                json_array_append_new(mt_array, mt_json);
            }
            iterator_stop(&it);
            json_object_set_new(json, "maze_templates", mt_array);
        }
        
        if (section->maze_fixed_rooms && list_size(section->maze_fixed_rooms) > 0) {
            json_t *mf_array = json_array();
            ITERATOR it;
            MAZE_FIXED_ROOM *mfr;
            iterator_start(&it, section->maze_fixed_rooms);
            while ((mfr = (MAZE_FIXED_ROOM *)iterator_nextdata(&it))) {
                json_t *mf_json = json_object();
                json_object_set_new(mf_json, "x", json_integer(mfr->x));
                json_object_set_new(mf_json, "y", json_integer(mfr->y));
                json_object_set_new(mf_json, "connected", mfr->connected ? json_true() : json_false());
                if (mfr->room) {
                    json_object_set_new(mf_json, "room", json_string(widevnum_string_room(mfr->room, NULL)));
                } else {
                    json_object_set_new(mf_json, "room", json_integer(mfr->room_ref.load.vnum));
                }
                json_array_append_new(mf_array, mf_json);
            }
            iterator_stop(&it);
            json_object_set_new(json, "maze_fixed_rooms", mf_array);
        }

        // Map data
        if (section->map_data) {
            MAZE_MAP_DATA *mmd = section->map_data;
            json_t *map_json = json_object();

            if (mmd->obj) {
                json_object_set_new(map_json, "obj", json_string(widevnum_string_object(mmd->obj, NULL)));
            } else if (mmd->obj_ref.load.vnum > 0) {
                json_object_set_new(map_json, "obj", json_integer(mmd->obj_ref.load.vnum));
            }

            if (mmd->mob) {
                json_object_set_new(map_json, "mob", json_string(widevnum_string_mobile(mmd->mob, NULL)));
            } else if (mmd->mob_ref.load.vnum > 0) {
                json_object_set_new(map_json, "mob", json_integer(mmd->mob_ref.load.vnum));
            }

            if (mmd->solve)
                json_object_set_new(map_json, "solve", json_true());

            json_object_set_new(json, "maze_map", map_json);
        }
    }
    
    /* Recall room - use widevnum format */
    if (section->recall_room) {
        json_object_set_new(json, "recall_room", json_string(widevnum_string_room(section->recall_room, NULL)));
    }

    /* Links */
    if (section->links) {
        json_t *links_array = json_array();
        for (BLUEPRINT_LINK *bl = section->links; bl; bl = bl->next) {
            json_t *link_json = json_object();
            json_object_set_new(link_json, "name", json_string(bl->name ? bl->name : ""));
            if (bl->room) {
                json_object_set_new(link_json, "room", json_string(widevnum_string_room(bl->room, NULL)));
            }
            json_object_set_new(link_json, "door", json_integer(bl->door));
            json_array_append_new(links_array, link_json);
        }
        if (json_array_size(links_array) > 0) {
            json_object_set_new(json, "links", links_array);
        } else {
            json_decref(links_array);
        }
    }
    
    return json;
}

/*
 * Serialize a blueprint to JSON
 */
json_t *json_area_serialize_blueprint(BLUEPRINT *blueprint, AREA_DATA *area)
{
    if (!blueprint) return NULL;
    
    json_t *json = json_object();
    if (!json) return NULL;
    
    /* Basic info */
    json_object_set_new(json, "vnum", json_integer(blueprint->vnum));
    json_object_set_new(json, "name", json_string(blueprint->name ? blueprint->name : ""));
    json_object_set_new(json, "description", json_string(blueprint->description ? blueprint->description : ""));
    
    if (blueprint->comments && blueprint->comments[0] != '\0')
        json_object_set_new(json, "comments", json_string(blueprint->comments));
    
    /* Flags and mode */
    json_object_set_new(json, "flags", json_integer(blueprint->flags));
    json_object_set_new(json, "mode", json_integer(blueprint->mode));
    json_object_set_new(json, "repop", json_integer(blueprint->repop));
    json_object_set_new(json, "area_who", json_integer(blueprint->area_who));
    
    /* Sections array - store section vnums */
    if (blueprint->sections && list_size(blueprint->sections) > 0) {
        json_t *sections_array = json_array();
        ITERATOR it;
        iterator_start(&it, blueprint->sections);
        while(iterator_nextdata(&it)) {
            BLUEPRINT_SECTION_REF *ref = (BLUEPRINT_SECTION_REF*)iterator_currentdata(&it);
            if (ref && ref->section) {
                json_array_append_new(sections_array, json_integer(ref->section->vnum));
            }
        }
        iterator_stop(&it);
        if (json_array_size(sections_array) > 0) {
            json_object_set_new(json, "sections", sections_array);
        } else {
            json_decref(sections_array);
        }
    }
    
    /* Static mode data */
    if (blueprint->mode == BLUEPRINT_MODE_STATIC) {
        json_t *static_data = json_object();
        
        /* Recall section */
        if (blueprint->_static.recall > 0) {
            json_object_set_new(static_data, "recall", json_integer(blueprint->_static.recall));
        }
        
        /* Static entries */
        if (blueprint->_static.entries && list_size(blueprint->_static.entries) > 0) {
            json_t *entries_array = json_array();
            ITERATOR it;
            iterator_start(&it, blueprint->_static.entries);
            while(iterator_nextdata(&it)) {
                BLUEPRINT_EXIT_DATA *ex = (BLUEPRINT_EXIT_DATA*)iterator_currentdata(&it);
                if (ex) {
                    json_t *entry_json = json_object();
                    json_object_set_new(entry_json, "name", json_string(ex->name ? ex->name : ""));
                    json_object_set_new(entry_json, "section", json_integer(ex->section));
                    json_object_set_new(entry_json, "link", json_integer(ex->link));
                    json_array_append_new(entries_array, entry_json);
                }
            }
            iterator_stop(&it);
            if (json_array_size(entries_array) > 0) {
                json_object_set_new(static_data, "entries", entries_array);
            } else {
                json_decref(entries_array);
            }
        }
        
        /* Static exits */
        if (blueprint->_static.exits && list_size(blueprint->_static.exits) > 0) {
            json_t *exits_array = json_array();
            ITERATOR it;
            iterator_start(&it, blueprint->_static.exits);
            while(iterator_nextdata(&it)) {
                BLUEPRINT_EXIT_DATA *ex = (BLUEPRINT_EXIT_DATA*)iterator_currentdata(&it);
                if (ex) {
                    json_t *exit_json = json_object();
                    json_object_set_new(exit_json, "name", json_string(ex->name ? ex->name : ""));
                    json_object_set_new(exit_json, "section", json_integer(ex->section));
                    json_object_set_new(exit_json, "link", json_integer(ex->link));
                    json_array_append_new(exits_array, exit_json);
                }
            }
            iterator_stop(&it);
            if (json_array_size(exits_array) > 0) {
                json_object_set_new(static_data, "exits", exits_array);
            } else {
                json_decref(exits_array);
            }
        }
        
        /* Static links */
        if (blueprint->_static.layout) {
            json_t *links_array = json_array();
            for (STATIC_BLUEPRINT_LINK *sbl = blueprint->_static.layout; sbl; sbl = sbl->next) {
                json_t *link_json = json_object();
                json_object_set_new(link_json, "section1", json_integer(sbl->section1));
                json_object_set_new(link_json, "link1", json_integer(sbl->link1));
                json_object_set_new(link_json, "section2", json_integer(sbl->section2));
                json_object_set_new(link_json, "link2", json_integer(sbl->link2));
                json_array_append_new(links_array, link_json);
            }
            if (json_array_size(links_array) > 0) {
                json_object_set_new(static_data, "layout", links_array);
            } else {
                json_decref(links_array);
            }
        }
        
        if (json_object_size(static_data) > 0) {
            json_object_set_new(json, "static", static_data);
        } else {
            json_decref(static_data);
        }
    }
    
    /* Special rooms - use widevnum format */
    if (blueprint->special_rooms && list_size(blueprint->special_rooms) > 0) {
        json_t *special_rooms_array = json_array();
        ITERATOR it;
        iterator_start(&it, blueprint->special_rooms);
        while(iterator_nextdata(&it)) {
            BLUEPRINT_SPECIAL_ROOM *special = (BLUEPRINT_SPECIAL_ROOM*)iterator_currentdata(&it);
            if (special) {
                json_t *special_json = json_object();
                json_object_set_new(special_json, "name", json_string(special->name ? special->name : ""));
                json_object_set_new(special_json, "section", json_integer(special->section));
                if (special->room) {
                    json_object_set_new(special_json, "room", json_string(widevnum_string_room(special->room, NULL)));
                }
                json_array_append_new(special_rooms_array, special_json);
            }
        }
        iterator_stop(&it);
        if (json_array_size(special_rooms_array) > 0) {
            json_object_set_new(json, "special_rooms", special_rooms_array);
        } else {
            json_decref(special_rooms_array);
        }
    }
    
    /* Progs */
    if (blueprint->progs) {
        json_t *progs_json = json_area_serialize_progs(blueprint->progs, area);
        if (progs_json) {
            json_object_set_new(json, "progs", progs_json);
        }
    }
    
    /* Index vars */
    if (blueprint->index_vars) {
        json_t *vars_json = json_area_serialize_index_vars(blueprint->index_vars, area);
        if (vars_json) {
            json_object_set_new(json, "index_vars", vars_json);
        }
    }
    
    return json;
}

/*
 * Serialize a dungeon index to JSON
 */
json_t *json_area_serialize_dungeon(DUNGEON_INDEX_DATA *dungeon, AREA_DATA *area)
{
    if (!dungeon) return NULL;
    
    json_t *json = json_object();
    if (!json) return NULL;
    
    /* Basic info */
    json_object_set_new(json, "vnum", json_integer(dungeon->vnum));
    json_object_set_new(json, "name", json_string(dungeon->name ? dungeon->name : ""));
    json_object_set_new(json, "description", json_string(dungeon->description ? dungeon->description : ""));
    
    if (dungeon->comments && dungeon->comments[0] != '\0')
        json_object_set_new(json, "comments", json_string(dungeon->comments));
    
    /* Properties */
    json_object_set_new(json, "area_who", json_integer(dungeon->area_who));
    json_object_set_new(json, "repop", json_integer(dungeon->repop));
    json_object_set_new(json, "flags", json_integer(dungeon->flags));
    
    /* Group management */
    if (dungeon->min_group > 0)
        json_object_set_new(json, "min_group", json_integer(dungeon->min_group));
    if (dungeon->max_group > 0)
        json_object_set_new(json, "max_group", json_integer(dungeon->max_group));
    if (dungeon->max_players > 0)
        json_object_set_new(json, "max_players", json_integer(dungeon->max_players));
    if (dungeon->death_release > 0)
        json_object_set_new(json, "death_release", json_integer(dungeon->death_release));
    if (dungeon->idle_timeout > 0)
        json_object_set_new(json, "idle_timeout", json_integer(dungeon->idle_timeout));
    
    /* Zone out strings */
    if (dungeon->zone_out && dungeon->zone_out[0] != '\0')
        json_object_set_new(json, "zone_out", json_string(dungeon->zone_out));
    if (dungeon->zone_out_portal && dungeon->zone_out_portal[0] != '\0')
        json_object_set_new(json, "portal_out", json_string(dungeon->zone_out_portal));
    if (dungeon->zone_out_mount && dungeon->zone_out_mount[0] != '\0')
        json_object_set_new(json, "mount_out", json_string(dungeon->zone_out_mount));
    
    /* Entry/exit rooms - use widevnum format */
    if (dungeon->entry_room) {
        json_object_set_new(json, "entry_room", json_string(widevnum_string_room(dungeon->entry_room, NULL)));
    }
    if (dungeon->exit_room) {
        json_object_set_new(json, "exit_room", json_string(widevnum_string_room(dungeon->exit_room, NULL)));
    }
    
    /* Floors - list of blueprint widevnums */
    if (dungeon->floors && list_size(dungeon->floors) > 0) {
        json_t *floors_array = json_array();
        ITERATOR it;
        iterator_start(&it, dungeon->floors);
        while(iterator_nextdata(&it)) {
            BLUEPRINT *bp = (BLUEPRINT*)iterator_currentdata(&it);
            if (bp) {
                json_array_append_new(floors_array, json_string(widevnum_string_blueprint(bp, NULL)));
            }
        }
        iterator_stop(&it);
        if (json_array_size(floors_array) > 0) {
            json_object_set_new(json, "floors", floors_array);
        } else {
            json_decref(floors_array);
        }
    }
    
    /* Levels (only if not scripted) */
    if (!IS_SET(dungeon->flags, DUNGEON_SCRIPTED_LEVELS) && dungeon->levels && list_size(dungeon->levels) > 0) {
        json_t *levels_array = json_array();
        ITERATOR it;
        iterator_start(&it, dungeon->levels);
        while(iterator_nextdata(&it)) {
            DUNGEON_INDEX_LEVEL_DATA *level = (DUNGEON_INDEX_LEVEL_DATA*)iterator_currentdata(&it);
            if (level) {
                json_t *level_json = json_object();
                json_object_set_new(level_json, "mode", json_integer(level->mode));
                json_object_set_new(level_json, "floor", json_integer(level->floor));
                
                /* Weighted floors */
                if (level->weighted_floors && list_size(level->weighted_floors) > 0) {
                    json_t *weighted_array = json_array();
                    ITERATOR wit;
                    iterator_start(&wit, level->weighted_floors);
                    while(iterator_nextdata(&wit)) {
                        DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *wf = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA*)iterator_currentdata(&wit);
                        if (wf) {
                            json_t *wf_json = json_object();
                            json_object_set_new(wf_json, "weight", json_integer(wf->weight));
                            json_object_set_new(wf_json, "floor", json_integer(wf->floor));
                            json_array_append_new(weighted_array, wf_json);
                        }
                    }
                    iterator_stop(&wit);
                    if (json_array_size(weighted_array) > 0) {
                        json_object_set_new(level_json, "weighted_floors", weighted_array);
                    } else {
                        json_decref(weighted_array);
                    }
                }
                
                json_array_append_new(levels_array, level_json);
            }
        }
        iterator_stop(&it);
        if (json_array_size(levels_array) > 0) {
            json_object_set_new(json, "levels", levels_array);
        } else {
            json_decref(levels_array);
        }
    }
    
    /* Special rooms */
    if (dungeon->special_rooms && list_size(dungeon->special_rooms) > 0) {
        json_t *special_rooms_array = json_array();
        ITERATOR it;
        iterator_start(&it, dungeon->special_rooms);
        while(iterator_nextdata(&it)) {
            DUNGEON_INDEX_SPECIAL_ROOM *special = (DUNGEON_INDEX_SPECIAL_ROOM*)iterator_currentdata(&it);
            if (special) {
                json_t *special_json = json_object();
                json_object_set_new(special_json, "name", json_string(special->name ? special->name : ""));
                json_object_set_new(special_json, "level", json_integer(special->level));
                json_object_set_new(special_json, "room", json_integer(special->room));
                json_array_append_new(special_rooms_array, special_json);
            }
        }
        iterator_stop(&it);
        if (json_array_size(special_rooms_array) > 0) {
            json_object_set_new(json, "special_rooms", special_rooms_array);
        } else {
            json_decref(special_rooms_array);
        }
    }
    
    /* Special exits - complex structure, serialize basic info only */
    if (dungeon->special_exits && list_size(dungeon->special_exits) > 0) {
        json_t *special_exits_array = json_array();
        ITERATOR it;
        iterator_start(&it, dungeon->special_exits);
        while(iterator_nextdata(&it)) {
            DUNGEON_INDEX_SPECIAL_EXIT *ex = (DUNGEON_INDEX_SPECIAL_EXIT*)iterator_currentdata(&it);
            if (ex) {
                json_t *ex_json = json_object();
                json_object_set_new(ex_json, "name", json_string(ex->name ? ex->name : ""));
                json_object_set_new(ex_json, "mode", json_integer(ex->mode));
                json_object_set_new(ex_json, "create_if_exists", ex->create_if_exists ? json_true() : json_false());
                /* Note: from/to lists contain complex DUNGEON_INDEX_WEIGHTED_EXIT_DATA */
                /* These are fully serialized in the .dat format but simplified here */
                json_array_append_new(special_exits_array, ex_json);
            }
        }
        iterator_stop(&it);
        if (json_array_size(special_exits_array) > 0) {
            json_object_set_new(json, "special_exits", special_exits_array);
        } else {
            json_decref(special_exits_array);
        }
    }
    
    /* Progs */
    if (dungeon->progs) {
        json_t *progs_json = json_area_serialize_progs(dungeon->progs, area);
        if (progs_json) {
            json_object_set_new(json, "dungeon_progs", progs_json);
        }
    }
    
    /* Index vars */
    if (dungeon->index_vars) {
        json_t *vars_json = json_area_serialize_index_vars(dungeon->index_vars, area);
        if (vars_json) {
            json_object_set_new(json, "index_vars", vars_json);
        }
    }
    
    return json;
}

/*
 * Serialize a ship index to JSON
 */
json_t *json_area_serialize_ship(SHIP_INDEX_DATA *ship, AREA_DATA *area)
{
    if (!ship) return NULL;
    
    json_t *json = json_object();
    if (!json) return NULL;
    
    /* Basic info */
    json_object_set_new(json, "vnum", json_integer(ship->vnum));
    json_object_set_new(json, "name", json_string(ship->name ? ship->name : ""));
    json_object_set_new(json, "description", json_string(ship->description ? ship->description : ""));
    json_object_set_new(json, "ship_class", json_integer(ship->ship_class));
    json_object_set_new(json, "flags", json_integer(ship->flags));
    
    /* Blueprint reference - always use widevnum format */
    if (IS_VALID(ship->blueprint)) {
        json_object_set_new(json, "blueprint", json_string(widevnum_string(ship->blueprint->area, ship->blueprint->vnum, NULL)));
    }

    /* Ship object reference - always use widevnum format */
    if (ship->ship_object) {
        json_object_set_new(json, "ship_object", json_string(widevnum_string_object(ship->ship_object, NULL)));
    }
    
    /* Stats */
    json_object_set_new(json, "hit", json_integer(ship->hit));
    json_object_set_new(json, "guns", json_integer(ship->guns));
    json_object_set_new(json, "min_crew", json_integer(ship->min_crew));
    json_object_set_new(json, "max_crew", json_integer(ship->max_crew));
    json_object_set_new(json, "move_delay", json_integer(ship->move_delay));
    json_object_set_new(json, "move_steps", json_integer(ship->move_steps));
    json_object_set_new(json, "turning", json_integer(ship->turning));
    json_object_set_new(json, "weight", json_integer(ship->weight));
    json_object_set_new(json, "capacity", json_integer(ship->capacity));
    json_object_set_new(json, "armor", json_integer(ship->armor));
    json_object_set_new(json, "oars", json_integer(ship->oars));
    
    /* Special keys - use widevnum format */
    if (ship->special_keys && list_size(ship->special_keys) > 0) {
        json_t *keys_array = json_array();
        ITERATOR it;
        iterator_start(&it, ship->special_keys);
        while (iterator_nextdata(&it)) {
            OBJ_INDEX_DATA *key_obj = (OBJ_INDEX_DATA *)iterator_currentdata(&it);
            if (key_obj) {
                json_array_append_new(keys_array, json_string(widevnum_string_object(key_obj, NULL)));
            }
        }
        iterator_stop(&it);
        if (json_array_size(keys_array) > 0) {
            json_object_set_new(json, "special_keys", keys_array);
        } else {
            json_decref(keys_array);
        }
    }
    
    return json;
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

        /* Handle widevnum string or legacy integer for obj_vnum */
        long obj_vnum = 0;
        WNUM_LOAD obj_wload = { 0, 0 };
        json_t *obj_vnum_json = json_object_get(trade_json, "obj_vnum");
        if (obj_vnum_json && json_is_string(obj_vnum_json)) {
            if (parse_widevnum_load(json_string_value(obj_vnum_json), &obj_wload)) {
                obj_vnum = obj_wload.vnum;
            }
        } else {
            obj_vnum = json_get_int_default(trade_json, "obj_vnum", 0);
            obj_wload.vnum = obj_vnum;
        }

        /* Create the trade item */
        new_trade_item(area, trade_type, replenish_time, replenish_amount,
                      max_qty, min_price, max_price, obj_vnum);

        /* Store the full WNUM_LOAD on the newly created trade item (it's at head of list) */
        if (area->trade_list) {
            area->trade_list->obj_load.auid = obj_wload.auid;
            area->trade_list->obj_load.vnum = obj_wload.vnum;
        }
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
        for (int j = 0; j < MAX_KEY_HASH; j++) {
            for (MOB_INDEX_DATA *mob = area->mob_index_hash[j]; mob; mob = mob->next) {
                if (mob->vnum && mob->pShop) {
                    for (SHOP_STOCK_DATA *stock = mob->pShop->stock; stock; stock = stock->next) {
                        switch (stock->type) {
                            case STOCK_OBJECT:
                                if (stock->entity.load.vnum > 0) {
                                    long auid = stock->entity.load.auid;
                                    long vnum = stock->entity.load.vnum;
                                    AREA_DATA *target_area = NULL;
                                    
                                    if (auid) {
                                        target_area = get_area_index(auid);
                                        if (!target_area) {
                                            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                                "fix_shops: Area UID %ld not found for object vnum %ld (mob %ld in %s)",
                                                auid, vnum, mob->vnum, area->file_name);
                                        }
                                    }
                                    
                                    /* Resolve pointer - use target area if specified, otherwise global */
                                    stock->obj = target_area ? get_obj_index(target_area, vnum) : get_obj_index_global(vnum);

                                    /* Backfill target area from resolved object if uid was omitted */
                                    if (!target_area && stock->obj && stock->obj->area)
                                        target_area = stock->obj->area;
                                    
                                    /* Convert to WNUM format */
                                    stock->entity.wnum.pArea = target_area;
                                    stock->entity.wnum.vnum = vnum;

                                    if (target_area && stock->entity.load.auid == 0)
                                        stock->entity.load.auid = target_area->uid;
                                    
                                    if (!stock->obj) {
                                        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                            "fix_shops: Shop stock object vnum %ld not found (mob %ld in %s)",
                                            vnum, mob->vnum, area->file_name);
                                    }
                                }
                                break;
                            case STOCK_PET:
                            case STOCK_MOUNT:
                            case STOCK_GUARD:
                            case STOCK_CREW:
                                if (stock->entity.load.vnum > 0) {
                                    long auid = stock->entity.load.auid;
                                    long vnum = stock->entity.load.vnum;
                                    AREA_DATA *target_area = NULL;
                                    
                                    if (auid) {
                                        target_area = get_area_index(auid);
                                        if (!target_area) {
                                            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                                "fix_shops: Area UID %ld not found for mob vnum %ld (mob %ld in %s)",
                                                auid, vnum, mob->vnum, area->file_name);
                                        }
                                    }
                                    
                                    /* Resolve pointer - use target area if specified, otherwise global */
                                    stock->mob = target_area ? get_mob_index(target_area, vnum) : get_mob_index_global(vnum);

                                    /* Backfill target area from resolved mob if uid was omitted */
                                    if (!target_area && stock->mob && stock->mob->area)
                                        target_area = stock->mob->area;
                                    
                                    /* Convert to WNUM format */
                                    stock->entity.wnum.pArea = target_area;
                                    stock->entity.wnum.vnum = vnum;

                                    if (target_area && stock->entity.load.auid == 0)
                                        stock->entity.load.auid = target_area->uid;
                                    
                                    if (!stock->mob) {
                                        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                            "fix_shops: Shop stock mob vnum %ld not found (mob %ld in %s)",
                                            vnum, mob->vnum, area->file_name);
                                    }
                                }
                                break;
                            case STOCK_SHIP:
                                if (stock->entity.load.vnum > 0) {
                                    long auid = stock->entity.load.auid;
                                    long vnum = stock->entity.load.vnum;
                                    AREA_DATA *target_area = NULL;
                                    
                                    if (auid) {
                                        target_area = get_area_index(auid);
                                        if (!target_area) {
                                            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                                "fix_shops: Area UID %ld not found for ship vnum %ld (mob %ld in %s)",
                                                auid, vnum, mob->vnum, area->file_name);
                                        }
                                    }
                                    
                                    /* Use area-scoped lookup if area is known, fall back to global */
                                    stock->ship = target_area ? 
                                        get_ship_index_for_area(target_area, vnum) :
                                        get_ship_index(vnum);

                                    /* Backfill target area from resolved ship if uid was omitted */
                                    if (!target_area && stock->ship && stock->ship->area)
                                        target_area = stock->ship->area;
                                    
                                    /* Convert to WNUM format */
                                    stock->entity.wnum.pArea = target_area;
                                    stock->entity.wnum.vnum = vnum;

                                    if (target_area && stock->entity.load.auid == 0)
                                        stock->entity.load.auid = target_area->uid;
                                    
                                    if (!stock->ship) {
                                        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                            "fix_shops: Shop ship vnum %ld not found (mob %ld in %s)",
                                            vnum, mob->vnum, area->file_name);
                                    }
                                }
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
            

        }
    }
}
