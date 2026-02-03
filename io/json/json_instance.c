/***************************************************************************
 *  JSON Instance Format - Instance/Ship/Dungeon Serialization             *
 *                                                                          *
 *  This file contains serialization for runtime instance data:            *
 *  - Instances (standalone and object-owned)                              *
 *  - Ships (contain an instance)                                          *
 *  - Dungeons (contain multiple instances)                                *
 *  - Uses WNUM format for area-scoped references                          *
 *  - Embeds complete room state (NPCs, objects) within parent entity      *
 *                                                                          *
 *  Storage Model:                                                          *
 *  - Ships: persist/ships/ship_<uid>.json (self-contained)                *
 *  - Dungeons: persist/dungeons/dungeon_<uid>.json (self-contained)       *
 *  - Instances: persist/instances/instance_<uid>.json (self-contained)    *
 *  - Loose entities: persist/rooms/, persist/mobiles/, persist/objects/   *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <jansson.h>
#include "../../merc.h"
#include "../../tables.h"
#include "../../recycle.h"
#include "../../scripts.h"
#include "json_instance.h"
#include "../../wilds.h"
#include "../cache/redis_cache.h"

/***************************************************************************
 * External References                                                     *
 ***************************************************************************/

extern LLIST *loaded_instances;
extern LLIST *loaded_ships;
extern LLIST *loaded_dungeons;

/* Instance helper functions */
extern void instance_section_tallyentities(INSTANCE_SECTION *section);

/* JSON persist functions for complete room serialization */
extern json_t *json_persist_room_to_json(ROOM_INDEX_DATA *room);
extern ROOM_INDEX_DATA *json_persist_json_to_room(json_t *json);

/* Existing persist functions for objects/mobiles */
extern void persist_save_object(FILE *fp, OBJ_DATA *obj);
extern OBJ_DATA *persist_load_object(FILE *fp);
extern void persist_save_mobile(FILE *fp, CHAR_DATA *mob);
extern CHAR_DATA *persist_load_mobile(FILE *fp);

/***************************************************************************
 * Constants                                                               *
 ***************************************************************************/

/***************************************************************************
 * Helper Functions                                                        *
 ***************************************************************************/

/**
 * Create a WNUM reference as a JSON string
 * Format: "area_uid#vnum"
 */
static json_t *wnum_to_json(AREA_DATA *area, long vnum)
{
    char buf[256];
    long area_uid = area ? area->uid : 0;
    
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
 */
static WNUM parse_wnum_from_json(json_t *json)
{
    WNUM result = {NULL, 0};
    const char *str;
    long vnum;
    
    if (!json || !json_is_string(json)) {
        return result;
    }
    
    str = json_string_value(json);
    if (!str) {
        return result;
    }
    
    /* Parse as "auid#vnum" or bare "vnum" */
    char temp[MSL];
    strncpy(temp, str, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    if (parse_widevnum(temp, NULL, &result)) {
        return result;
    }
    
    /* Fall back to bare vnum */
    if (sscanf(str, "%ld", &vnum) == 1) {
        result.vnum = vnum;
        result.pArea = NULL;  /* Will need area-scoped lookup */
    }
    
    return result;
}

/**
 * Save room reference (vnum + clone IDs)
 */
static json_t *room_ref_to_json(ROOM_INDEX_DATA *room)
{
    json_t *json;
    
    if (!room) {
        return json_null();
    }
    
    json = json_object();
    json_object_set_new(json, "vnum", json_string(widevnum_string_room(room, NULL)));
    json_object_set_new(json, "id0", json_integer(room->id[0]));
    json_object_set_new(json, "id1", json_integer(room->id[1]));

    return json;
}

/**
 * Load room reference
 */
static ROOM_INDEX_DATA *json_to_room_ref(json_t *json)
{
    unsigned long id0, id1;
    json_t *value;
    ROOM_INDEX_DATA *source_room = NULL;

    if (!json || json_is_null(json)) {
        return NULL;
    }

    /* Parse vnum - supports both widevnum string and legacy integer */
    value = json_object_get(json, "vnum");
    if (json_is_string(value)) {
        WNUM wnum;
        if (parse_widevnum((char *)json_string_value(value), NULL, &wnum) && wnum.pArea) {
            source_room = get_room_index(wnum.pArea, wnum.vnum);
        }
    } else {
        long vnum = value ? json_integer_value(value) : 0;
        source_room = get_room_index_global(vnum);
    }

    value = json_object_get(json, "id0");
    id0 = value ? json_integer_value(value) : 0;

    value = json_object_get(json, "id1");
    id1 = value ? json_integer_value(value) : 0;

    if ((id0 || id1) && source_room) {
        /* Clone room - need to find it */
        return get_clone_room(source_room, id0, id1);
    }

    return NULL;
}

/**
 * Save UID pair
 */
static json_t *uid_to_json(unsigned long id0, unsigned long id1)
{
    json_t *json = json_array();
    json_array_append_new(json, json_integer(id0));
    json_array_append_new(json, json_integer(id1));
    return json;
}

/**
 * Load UID pair
 */
static void json_to_uid(json_t *json, unsigned long *id0, unsigned long *id1)
{
    if (!json || !json_is_array(json) || json_array_size(json) < 2) {
        *id0 = 0;
        *id1 = 0;
        return;
    }
    
    *id0 = json_integer_value(json_array_get(json, 0));
    *id1 = json_integer_value(json_array_get(json, 1));
}

/***************************************************************************
 * Instance Section Serialization                                          *
 ***************************************************************************/

json_t *instance_section_to_json(INSTANCE_SECTION *section)
{
    json_t *json, *rooms_array;
    ITERATOR it;
    ROOM_INDEX_DATA *room;
    
    if (!section || !IS_VALID(section)) {
        return json_null();
    }
    
    json = json_object();
    
    /* Section reference (WNUM) */
    /* Log area pointer for debugging corruption */
    if (IS_VALID(section->section)) {
        pbugf(LOG_DEBUG, "[INSTSECT SAVE] section_vnum=%ld bp_area=%p value=0x%lx (%s)",
              section->section->vnum,
              (void*)section->section->area,
              (unsigned long)section->section->area,
              section->section->area && (unsigned long)section->section->area > 0x10000 ? 
                  section->section->area->name : "CORRUPT");
    }
    
    /* Check if section->section->area is a valid pointer (not NULL and not garbage) */
    if (IS_VALID(section->section) && 
        section->section->area != NULL && 
        (unsigned long)section->section->area > 0x10000) {
        /* Valid area pointer - use it for WNUM */
        json_object_set_new(json, "section_wnum", 
            wnum_to_json(section->section->area, section->section->vnum));
    } else if (IS_VALID(section->section)) {
        /* No area or invalid/garbage area pointer - use global vnum */
        json_object_set_new(json, "section_wnum", 
            wnum_to_json(NULL, section->section->vnum));
    } else {
        /* Invalid section pointer - skip */
        json_object_set_new(json, "section_wnum", json_null());
    }
    
    /* Embed complete room data (self-contained) */
    rooms_array = json_array();
    iterator_start(&it, section->rooms);
    while ((room = (ROOM_INDEX_DATA *)iterator_nextdata(&it))) {
        /* Use json_persist to serialize complete room state */
        json_t *room_json = json_persist_room_to_json(room);
        if (room_json) {
            json_array_append_new(rooms_array, room_json);
        }
    }
    iterator_stop(&it);
    json_object_set_new(json, "rooms", rooms_array);
    
    return json;
}

INSTANCE_SECTION *json_to_instance_section(json_t *json)
{
    INSTANCE_SECTION *section;
    json_t *value, *rooms_array;
    WNUM wnum;
    size_t index;
    json_t *room_json;
    
    if (!json || json_is_null(json)) {
        return NULL;
    }
    
    section = new_instance_section();
    
    /* Load section reference */
    value = json_object_get(json, "section_wnum");
    wnum = parse_wnum_from_json(value);
    
    pbugf(LOG_DEBUG, "[INSTSECT LOAD] Parsed WNUM: pArea=%p vnum=%ld (json=%s)",
          (void*)wnum.pArea,
          wnum.vnum,
          value ? json_string_value(value) : "NULL");
    
    section->section = get_blueprint_section_for_area(wnum.pArea, wnum.vnum);
    
    pbugf(LOG_DEBUG, "[INSTSECT LOAD] get_blueprint_section_for_area returned: %p valid=%s",
          (void*)section->section,
          section->section ? (section->section->valid ? "true" : "FALSE") : "NULL");
    
    if (IS_VALID(section->section)) {
        pbugf(LOG_DEBUG, "[INSTSECT LOAD] section_vnum=%ld bp_area=%p (%s)",
              section->section->vnum,
              (void*)section->section->area,
              section->section->area && (unsigned long)section->section->area > 0x10000 ? 
                  section->section->area->name : "CORRUPT");
    }
    
    if (!IS_VALID(section->section)) {
        free_instance_section(section);
        return NULL;
    }
    
    /* Load embedded rooms with full state */
    rooms_array = json_object_get(json, "rooms");
    if (rooms_array && json_is_array(rooms_array)) {
        json_array_foreach(rooms_array, index, room_json) {
            ROOM_INDEX_DATA *room = json_persist_json_to_room(room_json);
            if (room) {
                room->instance_section = section;
                variable_dynamic_fix_clone_room(room);
                list_appendlink(section->rooms, room);
            }
        }
    }
    
    return section;
}

/***************************************************************************
 * Instance Serialization                                                  *
 ***************************************************************************/

json_t *instance_to_json(INSTANCE *instance)
{
    json_t *json, *sections_array, *players_array;
    ITERATOR it;
    INSTANCE_SECTION *section;
    LLIST_UID_DATA *luid;
    
    if (!instance || !IS_VALID(instance)) {
        return json_null();
    }
    
    json = json_object();
    
    /* Instance type - determine from ownership */
    const char *instance_type = "standalone";
    if (IS_VALID(instance->ship)) {
        instance_type = "ship";
    } else if (IS_VALID(instance->dungeon)) {
        instance_type = "dungeon";
    }
    json_object_set_new(json, "instance_type", json_string(instance_type));
    
    /* Blueprint reference (WNUM) */
    AREA_DATA *bp_area = instance->blueprint->area ? 
        instance->blueprint->area : get_system_area_fallback();
    json_object_set_new(json, "blueprint_wnum", 
        wnum_to_json(bp_area, instance->blueprint->vnum));
    
    /* Basic properties */
    json_object_set_new(json, "floor", json_integer(instance->floor));
    json_object_set_new(json, "flags", json_integer(instance->flags));
    
    /* Object owner */
    if (instance->object_uid[0] || instance->object_uid[1]) {
        json_object_set_new(json, "object_uid", 
            uid_to_json(instance->object_uid[0], instance->object_uid[1]));
    }
    
    /* Player owners */
    players_array = json_array();
    iterator_start(&it, instance->player_owners);
    while ((luid = (LLIST_UID_DATA *)iterator_nextdata(&it))) {
        json_array_append_new(players_array, uid_to_json(luid->id[0], luid->id[1]));
    }
    iterator_stop(&it);
    if (json_array_size(players_array) > 0) {
        json_object_set_new(json, "player_owners", players_array);
    } else {
        json_decref(players_array);
    }
    
    /* Sections */
    sections_array = json_array();
    iterator_start(&it, instance->sections);
    while ((section = (INSTANCE_SECTION *)iterator_nextdata(&it))) {
        json_array_append_new(sections_array, instance_section_to_json(section));
    }
    iterator_stop(&it);
    json_object_set_new(json, "sections", sections_array);
    
    /* Special room references */
    json_object_set_new(json, "recall", room_ref_to_json(instance->recall));
    json_object_set_new(json, "entrance", room_ref_to_json(instance->entrance));
    json_object_set_new(json, "exit", room_ref_to_json(instance->exit));
    
    return json;
}

INSTANCE *json_to_instance(json_t *json)
{
    INSTANCE *instance;
    json_t *value, *array;
    WNUM wnum;
    size_t index;
    json_t *elem;
    
    if (!json || json_is_null(json)) {
        return NULL;
    }
    
    instance = new_instance();
    
    /* Load blueprint reference */
    value = json_object_get(json, "blueprint_wnum");
    wnum = parse_wnum_from_json(value);
    
    /* Try area-scoped lookup first, fall back to global */
    if (wnum.pArea) {
        instance->blueprint = get_blueprint_for_area(wnum.pArea, wnum.vnum);
    }
    
    if (!IS_VALID(instance->blueprint)) {
        instance->blueprint = get_blueprint(wnum.vnum);
    }
    
    if (!IS_VALID(instance->blueprint)) {
        log_stringf("json_to_instance: Blueprint %ld not found", wnum.vnum);
        free_instance(instance);
        return NULL;
    }
    
    /* Initialize progs */
    instance->progs = new_prog_data();
    instance->progs->progs = instance->blueprint->progs;
    variable_copylist(&instance->blueprint->index_vars, &instance->progs->vars, false);
    
    /* Load properties */
    value = json_object_get(json, "floor");
    if (value) instance->floor = json_integer_value(value);
    
    value = json_object_get(json, "flags");
    if (value) instance->flags = json_integer_value(value);
    
    /* Object owner */
    value = json_object_get(json, "object_uid");
    if (value) {
        json_to_uid(value, &instance->object_uid[0], &instance->object_uid[1]);
    }
    
    /* Player owners */
    array = json_object_get(json, "player_owners");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            unsigned long id0, id1;
            json_to_uid(elem, &id0, &id1);
            instance_addowner_playerid(instance, id0, id1);
        }
    }
    
    /* Sections */
    array = json_object_get(json, "sections");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            INSTANCE_SECTION *section = json_to_instance_section(elem);
            if (section) {
                section->instance = instance;
                section->blueprint = instance->blueprint;
                list_appendlink(instance->sections, section);
                
                /* Tally entities in the loaded section */
                instance_section_tallyentities(section);
            }
        }
    }
    
    /* Special rooms */
    value = json_object_get(json, "recall");
    instance->recall = json_to_room_ref(value);
    
    value = json_object_get(json, "entrance");
    instance->entrance = json_to_room_ref(value);
    
    value = json_object_get(json, "exit");
    instance->exit = json_to_room_ref(value);
    
    return instance;
}

/***************************************************************************
 * Ship Serialization                                                      *
 ***************************************************************************/

json_t *ship_to_json(SHIP_DATA *ship)
{
    json_t *json;
    
    if (!ship || !IS_VALID(ship)) {
        return json_null();
    }
    
    json = json_object();
    
    /* Ship index reference (WNUM) */
    AREA_DATA *ship_area = ship->index->area ? 
        ship->index->area : get_system_area_fallback();
    json_object_set_new(json, "index_wnum", 
        wnum_to_json(ship_area, ship->index->vnum));
    
    /* Ship UID */
    json_object_set_new(json, "uid", uid_to_json(ship->id[0], ship->id[1]));
    
    /* Ship properties */
    json_object_set_new(json, "name", json_string(ship->ship_name));
    json_object_set_new(json, "ship_type", json_integer(ship->ship_type));
    json_object_set_new(json, "owner_uid", uid_to_json(ship->owner_uid[0], ship->owner_uid[1]));
    if (ship->owner && !IS_NPC(ship->owner)) {
        json_object_set_new(json, "owner_name", json_string(ship->owner->name));
    }
    json_object_set_new(json, "flag", json_string(ship->flag ? ship->flag : ""));
    json_object_set_new(json, "ship_power", json_integer(ship->ship_power));
    json_object_set_new(json, "hit", json_integer(ship->hit));
    json_object_set_new(json, "armor", json_integer(ship->armor));
    json_object_set_new(json, "ship_flags", json_integer(ship->ship_flags));
    json_object_set_new(json, "cannons", json_integer(ship->cannons));
    json_object_set_new(json, "min_crew", json_integer(ship->min_crew));
    json_object_set_new(json, "max_crew", json_integer(ship->max_crew));
    json_object_set_new(json, "oars", json_integer(ship->oars));
    
    /* Instance */
    if (IS_VALID(ship->instance)) {
        json_object_set_new(json, "instance", instance_to_json(ship->instance));
    }
    
    /* Ship object location */
    if (IS_VALID(ship->ship)) {
        json_t *location = json_object();
        ROOM_INDEX_DATA *room = obj_room(ship->ship);
        
        if (room && IS_WILDERNESS(room)) {
            json_object_set_new(location, "type", json_string("wilderness"));
            json_object_set_new(location, "x", json_integer(room->x));
            json_object_set_new(location, "y", json_integer(room->y));
            json_object_set_new(location, "area_uid", json_integer(room->wilds->pArea->uid));
            json_object_set_new(location, "wilds_uid", json_integer(room->wilds->uid));
        } else if (room) {
            json_object_set_new(location, "type", json_string("room"));
            json_object_set_new(location, "room_vnum", json_string(widevnum_string_room(room, NULL)));
        }
        
        if (json_object_size(location) > 0) {
            json_object_set_new(json, "location", location);
        } else {
            json_decref(location);
        }
    }
    
    /* TODO: Add steering, crew, objects, etc. */
    
    return json;
}

SHIP_DATA *json_to_ship(json_t *json)
{
    SHIP_DATA *ship;
    json_t *value;
    WNUM wnum;
    SHIP_INDEX_DATA *index;
    
    if (!json || json_is_null(json)) {
        return NULL;
    }
    
    /* Load ship index reference */
    value = json_object_get(json, "index_wnum");
    wnum = parse_wnum_from_json(value);
    
    /* Try area-scoped lookup first, fall back to global */
    if (wnum.pArea) {
        index = get_ship_index_for_area(wnum.pArea, wnum.vnum);
    }
    
    if (!index) {
        index = get_ship_index(wnum.vnum);
    }
    
    if (!index) {
        log_stringf("json_to_ship: Ship index %ld not found", wnum.vnum);
        return NULL;
    }
    
    ship = new_ship();
    ship->index = index;
    
    /* Load UID */
    value = json_object_get(json, "uid");
    if (value) {
        json_to_uid(value, &ship->id[0], &ship->id[1]);
    }
    
    /* Load properties */
    value = json_object_get(json, "name");
    if (value) ship->ship_name = str_dup(json_string_value(value));
    
    value = json_object_get(json, "ship_type");
    if (value) ship->ship_type = json_integer_value(value);
    
    value = json_object_get(json, "owner_uid");
    if (value) {
        json_to_uid(value, &ship->owner_uid[0], &ship->owner_uid[1]);
    }
    
    value = json_object_get(json, "flag");
    if (value) ship->flag = str_dup(json_string_value(value));
    
    value = json_object_get(json, "ship_power");
    if (value) ship->ship_power = json_integer_value(value);
    
    value = json_object_get(json, "hit");
    if (value) ship->hit = json_integer_value(value);
    
    value = json_object_get(json, "armor");
    if (value) ship->armor = json_integer_value(value);
    
    value = json_object_get(json, "ship_flags");
    if (value) ship->ship_flags = json_integer_value(value);
    
    value = json_object_get(json, "cannons");
    if (value) ship->cannons = json_integer_value(value);
    
    value = json_object_get(json, "min_crew");
    if (value) ship->min_crew = json_integer_value(value);
    
    value = json_object_get(json, "max_crew");
    if (value) ship->max_crew = json_integer_value(value);
    
    value = json_object_get(json, "oars");
    if (value) ship->oars = json_integer_value(value);
    
    /* Load instance with full room state (NPCs, objects, etc.) */
    value = json_object_get(json, "instance");
    if (value && !json_is_null(value)) {
        ship->instance = json_to_instance(value);
        if (IS_VALID(ship->instance)) {
            ship->instance->ship = ship;
        }
    }
    
    /* Place ship object in world */
    value = json_object_get(json, "location");
    if (value && !json_is_null(value)) {
        json_t *type_obj = json_object_get(value, "type");
        const char *type = type_obj ? json_string_value(type_obj) : "room";
        
        if (!strcmp(type, "wilderness")) {
            int x = json_integer_value(json_object_get(value, "x"));
            int y = json_integer_value(json_object_get(value, "y"));
            long area_uid = json_integer_value(json_object_get(value, "area_uid"));
            long wilds_uid = json_integer_value(json_object_get(value, "wilds_uid"));
            
            AREA_DATA *pArea = get_area_from_uid(area_uid);
            if (pArea) {
                WILDS_DATA *wilds = get_wilds_from_uid(pArea, wilds_uid);
                if (wilds) {
                    ROOM_INDEX_DATA *room = get_wilds_vroom(wilds, x, y);
                    if (!room) {
                        room = create_wilds_vroom(wilds, x, y);
                    }
                    if (room && IS_VALID(ship->ship)) {
                        obj_to_room(ship->ship, room);
                        
                        /* Sync ship instance entrance to ship object location */
                        if (IS_VALID(ship->instance) && ship->instance->entrance) {
                            ship->instance->entrance->wilds = room->wilds;
                            ship->instance->entrance->x = room->x;
                            ship->instance->entrance->y = room->y;
                        }
                    }
                }
            }
        } else {
            json_t *room_vnum_val = json_object_get(value, "room_vnum");
            ROOM_INDEX_DATA *room = NULL;
            if (json_is_string(room_vnum_val)) {
                WNUM wnum;
                if (parse_widevnum((char *)json_string_value(room_vnum_val), NULL, &wnum) && wnum.pArea) {
                    room = get_room_index(wnum.pArea, wnum.vnum);
                }
            } else {
                long room_vnum = json_integer_value(room_vnum_val);
                room = get_room_index_global(room_vnum);
            }
            if (room && IS_VALID(ship->ship)) {
                obj_to_room(ship->ship, room);

                /* Sync ship instance entrance to ship object location for wilderness rooms */
                if (IS_VALID(ship->instance) && ship->instance->entrance && room->wilds) {
                    ship->instance->entrance->wilds = room->wilds;
                    ship->instance->entrance->x = room->x;
                    ship->instance->entrance->y = room->y;
                }
            }
        }
    }
    
    return ship;
}

/***************************************************************************
 * Dungeon Serialization                                                   *
 ***************************************************************************/

json_t *dungeon_to_json(DUNGEON *dungeon)
{
    json_t *json, *floors_array, *players_array;
    ITERATOR it;
    INSTANCE *instance;
    LLIST_UID_DATA *luid;
    
    if (!dungeon || !IS_VALID(dungeon)) {
        return json_null();
    }
    
    json = json_object();
    
    /* Dungeon index reference (WNUM) */
    AREA_DATA *dng_area = dungeon->index->area ? 
        dungeon->index->area : get_system_area_fallback();
    json_object_set_new(json, "index_wnum", 
        wnum_to_json(dng_area, dungeon->index->vnum));
    
    /* Dungeon UID */
    json_object_set_new(json, "uid", uid_to_json(dungeon->uid[0], dungeon->uid[1]));
    
    /* Properties */
    json_object_set_new(json, "flags", json_integer(dungeon->flags));
    
    if (dungeon->idle_timer > 0) {
        json_object_set_new(json, "idle_timer", json_integer(dungeon->idle_timer));
    }
    
    /* Player owners */
    players_array = json_array();
    iterator_start(&it, dungeon->player_owners);
    while ((luid = (LLIST_UID_DATA *)iterator_nextdata(&it))) {
        json_array_append_new(players_array, uid_to_json(luid->id[0], luid->id[1]));
    }
    iterator_stop(&it);
    if (json_array_size(players_array) > 0) {
        json_object_set_new(json, "player_owners", players_array);
    } else {
        json_decref(players_array);
    }
    
    /* Floor instances */
    floors_array = json_array();
    iterator_start(&it, dungeon->floors);
    while ((instance = (INSTANCE *)iterator_nextdata(&it))) {
        json_array_append_new(floors_array, instance_to_json(instance));
    }
    iterator_stop(&it);
    json_object_set_new(json, "floors", floors_array);
    
    return json;
}

DUNGEON *json_to_dungeon(json_t *json)
{
    DUNGEON *dungeon;
    json_t *value, *array;
    WNUM wnum;
    DUNGEON_INDEX_DATA *index;
    size_t idx;
    json_t *elem;
    
    if (!json || json_is_null(json)) {
        return NULL;
    }
    
    /* Load dungeon index reference */
    value = json_object_get(json, "index_wnum");
    wnum = parse_wnum_from_json(value);
    
    /* Try area-scoped lookup first, fall back to global */
    if (wnum.pArea) {
        index = get_dungeon_index_for_area(wnum.pArea, wnum.vnum);
    }
    
    if (!index) {
        index = get_dungeon_index(wnum.vnum);
    }
    
    if (!index) {
        log_stringf("json_to_dungeon: Dungeon index %ld not found", wnum.vnum);
        return NULL;
    }
    
    dungeon = new_dungeon();
    dungeon->index = index;
    
    /* Initialize progs */
    dungeon->progs = new_prog_data();
    dungeon->progs->progs = index->progs;
    variable_copylist(&index->index_vars, &dungeon->progs->vars, false);
    
    /* Use resolved room pointers from the index */
    dungeon->entry_room = index->entry_room;
    dungeon->exit_room = index->exit_room;
    
    /* Load UID */
    value = json_object_get(json, "uid");
    if (value) {
        json_to_uid(value, &dungeon->uid[0], &dungeon->uid[1]);
    }
    
    /* Load flags */
    value = json_object_get(json, "flags");
    if (value) dungeon->flags = json_integer_value(value);
    
    /* Load idle timer */
    value = json_object_get(json, "idle_timer");
    if (value) dungeon->idle_timer = json_integer_value(value);
    
    /* Player owners */
    array = json_object_get(json, "player_owners");
    if (array && json_is_array(array)) {
        json_array_foreach(array, idx, elem) {
            unsigned long id0, id1;
            json_to_uid(elem, &id0, &id1);
            dungeon_addowner_playerid(dungeon, id0, id1);
        }
    }
    
    /* Floor instances */
    array = json_object_get(json, "floors");
    if (array && json_is_array(array)) {
        json_array_foreach(array, idx, elem) {
            INSTANCE *instance = json_to_instance(elem);
            if (instance) {
                instance->dungeon = dungeon;
                list_appendlink(dungeon->floors, instance);
            }
        }
    }
    
    return dungeon;
}

/***************************************************************************
 * Individual Persist Functions                                            *
 ***************************************************************************/

/*
 * Save a single ship to its own persist file
 */
bool json_persist_save_ship(SHIP_DATA *ship)
{
    char path[512];
    json_t *json;
    int ret;
    
    if (!ship || !IS_VALID(ship)) {
        return false;
    }
    
    /* Create directory if it doesn't exist */
    mkdir(PERSIST_SHIPS_DIR, 0755);
    
    /* Generate filename: ship_<uid0>_<uid1>.json */
    snprintf(path, sizeof(path), "%sship_%lu_%lu.json",
             PERSIST_SHIPS_DIR, ship->id[0], ship->id[1]);
    
    json = ship_to_json(ship);
    if (!json) {
        log_stringf("json_persist_save_ship: Failed to serialize ship %lu_%lu",
                    ship->id[0], ship->id[1]);
        return false;
    }
    
    /* Add metadata flag for loading on boot */
    json_object_set_new(json, "load_on_boot", json_boolean(true));
    
    ret = json_dump_file(json, path, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
    json_decref(json);
    
    if (ret != 0) {
        log_stringf("json_persist_save_ship: Failed to write %s", path);
        return false;
    }
    
    return true;
}

/*
 * Load a single ship from its persist file
 */
SHIP_DATA *json_persist_load_ship(const char *filename)
{
    char path[512];
    json_t *root;
    json_error_t error;
    SHIP_DATA *ship;
    bool load_on_boot;
    
    snprintf(path, sizeof(path), "%s%s", PERSIST_SHIPS_DIR, filename);
    
    root = json_load_file(path, 0, &error);
    if (!root) {
        log_stringf("json_persist_load_ship: Failed to load %s: %s", path, error.text);
        return NULL;
    }
    
    /* Check load_on_boot flag */
    json_t *load_flag = json_object_get(root, "load_on_boot");
    load_on_boot = load_flag ? json_boolean_value(load_flag) : true;
    
    if (!load_on_boot) {
        json_decref(root);
        return NULL;
    }
    
    ship = json_to_ship(root);
    json_decref(root);
    
    return ship;
}

/*
 * Save a single dungeon to its own persist file
 */
bool json_persist_save_dungeon(DUNGEON *dungeon)
{
    char path[512];
    json_t *json;
    int ret;
    
    if (!dungeon || !IS_VALID(dungeon)) {
        return false;
    }
    
    if (IS_SET(dungeon->flags, (DUNGEON_NO_SAVE|DUNGEON_DESTROY))) {
        return true;  /* Don't save, but not an error */
    }
    
    /* Create directory if it doesn't exist */
    mkdir(PERSIST_DUNGEONS_DIR, 0755);
    
    /* Generate filename: dungeon_<uid0>_<uid1>.json */
    snprintf(path, sizeof(path), "%sdungeon_%lu_%lu.json",
             PERSIST_DUNGEONS_DIR, dungeon->uid[0], dungeon->uid[1]);
    
    json = dungeon_to_json(dungeon);
    if (!json) {
        log_stringf("json_persist_save_dungeon: Failed to serialize dungeon %lu_%lu",
                    dungeon->uid[0], dungeon->uid[1]);
        return false;
    }
    
    /* Add metadata flag for loading on boot */
    json_object_set_new(json, "load_on_boot", json_boolean(true));
    
    ret = json_dump_file(json, path, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
    json_decref(json);
    
    if (ret != 0) {
        log_stringf("json_persist_save_dungeon: Failed to write %s", path);
        return false;
    }
    
    return true;
}

/*
 * Load a single dungeon from its persist file
 */
DUNGEON *json_persist_load_dungeon(const char *filename)
{
    char path[512];
    json_t *root;
    json_error_t error;
    DUNGEON *dungeon;
    bool load_on_boot;
    
    snprintf(path, sizeof(path), "%s%s", PERSIST_DUNGEONS_DIR, filename);
    
    root = json_load_file(path, 0, &error);
    if (!root) {
        log_stringf("json_persist_load_dungeon: Failed to load %s: %s", path, error.text);
        return NULL;
    }
    
    /* Check load_on_boot flag */
    json_t *load_flag = json_object_get(root, "load_on_boot");
    load_on_boot = load_flag ? json_boolean_value(load_flag) : true;
    
    if (!load_on_boot) {
        json_decref(root);
        return NULL;
    }
    
    dungeon = json_to_dungeon(root);
    json_decref(root);
    
    return dungeon;
}

/*
 * Save a single standalone instance to its own persist file
 */
bool json_persist_save_instance(INSTANCE *instance)
{
    char path[512];
    json_t *json;
    int ret;
    
    if (!instance || !IS_VALID(instance)) {
        return false;
    }
    
    /* Don't save if it belongs to a ship or dungeon */
    if (IS_VALID(instance->ship) || IS_VALID(instance->dungeon)) {
        return true;
    }
    
    if (IS_SET(instance->flags, (INSTANCE_NO_SAVE|INSTANCE_DESTROY))) {
        return true;  /* Don't save, but not an error */
    }
    
    /* Create directory if it doesn't exist */
    mkdir(PERSIST_INSTANCES_DIR, 0755);
    
    /* Generate filename using object or player UID */
    if (instance->object_uid[0] || instance->object_uid[1]) {
        snprintf(path, sizeof(path), "%sinstance_obj_%lu_%lu.json",
                 PERSIST_INSTANCES_DIR, instance->object_uid[0], instance->object_uid[1]);
    } else if (instance->player_owners && instance->player_owners->size > 0) {
        /* Use first player UID */
        ITERATOR it;
        LLIST_UID_DATA *owner;
        iterator_start(&it, instance->player_owners);
        owner = (LLIST_UID_DATA *)iterator_nextdata(&it);
        iterator_stop(&it);
        
        if (owner) {
            snprintf(path, sizeof(path), "%sinstance_plr_%lu_%lu.json",
                     PERSIST_INSTANCES_DIR, owner->id[0], owner->id[1]);
        } else {
            snprintf(path, sizeof(path), "%sinstance_bp_%ld.json",
                     PERSIST_INSTANCES_DIR, instance->blueprint->vnum);
        }
    } else {
        /* Use blueprint vnum as fallback */
        snprintf(path, sizeof(path), "%sinstance_bp_%ld.json",
                 PERSIST_INSTANCES_DIR, instance->blueprint->vnum);
    }
    
    json = instance_to_json(instance);
    if (!json) {
        log_stringf("json_persist_save_instance: Failed to serialize instance");
        return false;
    }
    
    /* Add metadata flag for loading on boot */
    json_object_set_new(json, "load_on_boot", json_boolean(true));
    
    ret = json_dump_file(json, path, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
    json_decref(json);
    
    if (ret != 0) {
        log_stringf("json_persist_save_instance: Failed to write %s", path);
        return false;
    }
    
    return true;
}

/*
 * Load a single instance from its persist file
 */
INSTANCE *json_persist_load_instance(const char *filename)
{
    char path[512];
    json_t *root;
    json_error_t error;
    INSTANCE *instance;
    bool load_on_boot;
    
    snprintf(path, sizeof(path), "%s%s", PERSIST_INSTANCES_DIR, filename);
    
    root = json_load_file(path, 0, &error);
    if (!root) {
        log_stringf("json_persist_load_instance: Failed to load %s: %s", path, error.text);
        return NULL;
    }
    
    /* Check load_on_boot flag */
    json_t *load_flag = json_object_get(root, "load_on_boot");
    load_on_boot = load_flag ? json_boolean_value(load_flag) : true;
    
    if (!load_on_boot) {
        json_decref(root);
        return NULL;
    }
    
    instance = json_to_instance(root);
    json_decref(root);
    
    return instance;
}

/***************************************************************************
 * Directory Scanning Functions                                            *
 ***************************************************************************/

/*
 * Load all ships from persist directory
 */
int json_persist_load_all_ships(void)
{
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    
    dir = opendir(PERSIST_SHIPS_DIR);
    if (!dir) {
        /* Directory doesn't exist yet - not an error */
        return 0;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (entry->d_name[0] == '.') continue;
        
        /* Only load .json files */
        if (!strstr(entry->d_name, ".json")) continue;
        
        SHIP_DATA *ship = json_persist_load_ship(entry->d_name);
        if (ship) {
            list_appendlink(loaded_ships, ship);
            count++;
        }
    }
    
    closedir(dir);
    log_stringf("Loaded %d ships from persist directory", count);
    return count;
}

/*
 * Load all dungeons from persist directory
 */
int json_persist_load_all_dungeons(void)
{
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    
    dir = opendir(PERSIST_DUNGEONS_DIR);
    if (!dir) {
        /* Directory doesn't exist yet - not an error */
        return 0;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (entry->d_name[0] == '.') continue;
        
        /* Only load .json files */
        if (!strstr(entry->d_name, ".json")) continue;
        
        DUNGEON *dungeon = json_persist_load_dungeon(entry->d_name);
        if (dungeon) {
            list_appendlink(loaded_dungeons, dungeon);
            count++;
        }
    }
    
    closedir(dir);
    log_stringf("Loaded %d dungeons from persist directory", count);
    return count;
}

/*
 * Load all standalone instances from persist directory
 */
int json_persist_load_all_instances(void)
{
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    
    dir = opendir(PERSIST_INSTANCES_DIR);
    if (!dir) {
        /* Directory doesn't exist yet - not an error */
        return 0;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (entry->d_name[0] == '.') continue;
        
        /* Only load .json files */
        if (!strstr(entry->d_name, ".json")) continue;
        
        INSTANCE *instance = json_persist_load_instance(entry->d_name);
        if (instance) {
            list_appendlink(loaded_instances, instance);
            count++;
        }
    }
    
    closedir(dir);
    log_stringf("Loaded %d instances from persist directory", count);
    return count;
}

/***************************************************************************
 * Main Save Functions (Iterates and Saves Individually)                   *
 ***************************************************************************/

bool json_save_instances(void)
{
    ITERATOR it;
    DUNGEON *dungeon;
    SHIP_DATA *ship;
    INSTANCE *instance;
    int saved_dungeons = 0, saved_ships = 0, saved_instances = 0;
    
    /* Save each dungeon to its own file */
    iterator_start(&it, loaded_dungeons);
    while ((dungeon = (DUNGEON *)iterator_nextdata(&it))) {
        if (json_persist_save_dungeon(dungeon)) {
            saved_dungeons++;
        }
    }
    iterator_stop(&it);
    
    /* Save each ship to its own file */
    iterator_start(&it, loaded_ships);
    while ((ship = (SHIP_DATA *)iterator_nextdata(&it))) {
        if (json_persist_save_ship(ship)) {
            saved_ships++;
        }
    }
    iterator_stop(&it);
    
    /* Save each standalone instance to its own file */
    iterator_start(&it, loaded_instances);
    while ((instance = (INSTANCE *)iterator_nextdata(&it))) {
        if (json_persist_save_instance(instance)) {
            saved_instances++;
        }
    }
    iterator_stop(&it);
    
    log_stringf("json_save_instances: Saved %d dungeons, %d ships, %d instances to persist directories",
        saved_dungeons, saved_ships, saved_instances);
    
    return true;
}

bool json_load_instances(void)
{
    int dungeons_loaded, ships_loaded, instances_loaded;
    
    /* Load from individual persist files */
    dungeons_loaded = json_persist_load_all_dungeons();
    ships_loaded = json_persist_load_all_ships();
    instances_loaded = json_persist_load_all_instances();
    
    log_stringf("json_load_instances: Loaded %d dungeons, %d ships, %d instances from persist directories",
        dungeons_loaded, ships_loaded, instances_loaded);
    
    return true;
}

/***************************************************************************
 * Legacy Support (Old instances.json and .dat formats)                    *
 ***************************************************************************/

bool legacy_load_instances_dat(void)
{
    /* Just call the existing load_instances() function from db.c */
    extern void load_instances(void);
    load_instances();
    return true;
}

