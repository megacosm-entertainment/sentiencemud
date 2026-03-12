/***************************************************************************
 *  JSON Persistence Layer - Phase 1 Implementation                        *
 *                                                                          *
 *  Handles JSON serialization/deserialization for persistent world state: *
 *  - Objects (items in rooms, containers, etc.)                           *
 *  - Mobiles (NPCs)                                                       *
 *  - Rooms (static, wilderness, cloned)                                   *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <jansson.h>
#include "../../merc.h"
#include "../../tables.h"
#include "../../scripts.h"
#include "../../wilds.h"
#include "../../recycle.h"
#include "json_persist.h"
#include "../../traits.h"
#include "../cache/redis_cache.h"
#include "../../skill_data.h"
#include "json_obj_types.h"
#include "json_common.h"
#include "json_olc.h"

#define OLC_HISTORY_DIR "data/history"

static void emit_json_persist_event(event_severity_t severity,
                                    const char *category,
                                    const char *message,
                                    const char *action,
                                    const char *target)
{
    log_context_t ctx = {
        .actor_type = "system",
        .actor_name = "json_persist",
        .action = action,
        .target_type = target ? "storage" : NULL,
        .target_name = target,
    };
    log_event_t ev = {
        .severity = severity,
        .category = category ? category : LOG_ERROR,
        .plain_message = message,
        .context = &ctx,
        .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__,
    };
    log_emit_event(&ev, NULL);
}

/***************************************************************************
 * External References                                                     *
 ***************************************************************************/

extern LLIST *persist_objs;
extern LLIST *persist_mobs;
extern LLIST *persist_rooms;

extern const struct flag_type lock_flags[];
extern const struct flag_type catalyst_types[];
extern const struct flag_type affgroup_mobile_flags[];
extern const struct toxin_type toxin_table[];

/***************************************************************************
 * Forward Declarations                                                    *
 ***************************************************************************/

static json_t *spell_to_json(SPELL_DATA *spell);
static SPELL_DATA *json_to_spell(json_t *json);
static json_t *waypoint_to_json(WAYPOINT_DATA *wp);
static WAYPOINT_DATA *json_to_waypoint(json_t *json);
static json_t *variable_to_json(pVARIABLE var);
static bool json_load_variable(json_t *json, pVARIABLE *vars);

static const char *persist_json_dir_path(char *buf, size_t bufsize)
{
    return resolve_game_path(PERSIST_JSON_DIR, buf, bufsize);
}

static const char *persist_json_rooms_path(char *buf, size_t bufsize)
{
    return resolve_game_path(PERSIST_JSON_ROOMS, buf, bufsize);
}

static const char *persist_json_mobiles_path(char *buf, size_t bufsize)
{
    return resolve_game_path(PERSIST_JSON_MOBILES, buf, bufsize);
}

static const char *persist_json_objects_path(char *buf, size_t bufsize)
{
    return resolve_game_path(PERSIST_JSON_OBJECTS, buf, bufsize);
}

static const char *persist_dat_path(char *buf, size_t bufsize)
{
    return resolve_game_path(PERSIST_FILE, buf, bufsize);
}

/***************************************************************************
 * Initialization                                                          *
 ***************************************************************************/

bool json_persist_init(void)
{
    int ret;
    char persist_dir_buf[MAX_INPUT_LENGTH];
    char rooms_dir_buf[MAX_INPUT_LENGTH];
    char mobiles_dir_buf[MAX_INPUT_LENGTH];
    char objects_dir_buf[MAX_INPUT_LENGTH];
    const char *persist_dir = persist_json_dir_path(persist_dir_buf, sizeof(persist_dir_buf));
    const char *rooms_dir = persist_json_rooms_path(rooms_dir_buf, sizeof(rooms_dir_buf));
    const char *mobiles_dir = persist_json_mobiles_path(mobiles_dir_buf, sizeof(mobiles_dir_buf));
    const char *objects_dir = persist_json_objects_path(objects_dir_buf, sizeof(objects_dir_buf));

    /* Create main persist directory */
    ret = mkdir(persist_dir, 0755);
    if (ret != 0 && errno != EEXIST) {
        char msg[MSL];
        snprintf(msg, sizeof(msg), "json_persist_init: Failed to create %s: %s",
                 persist_dir, strerror(errno));
        emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                "persist_init_mkdir_failed", persist_dir);
        return false;
    }

    /* Create subdirectories */
    ret = mkdir(rooms_dir, 0755);
    if (ret != 0 && errno != EEXIST) {
        char msg[MSL];
        snprintf(msg, sizeof(msg), "json_persist_init: Failed to create %s: %s",
                 rooms_dir, strerror(errno));
        emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                "persist_init_mkdir_failed", rooms_dir);
        return false;
    }

    ret = mkdir(mobiles_dir, 0755);
    if (ret != 0 && errno != EEXIST) {
        char msg[MSL];
        snprintf(msg, sizeof(msg), "json_persist_init: Failed to create %s: %s",
                 mobiles_dir, strerror(errno));
        emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                "persist_init_mkdir_failed", mobiles_dir);
        return false;
    }

    ret = mkdir(objects_dir, 0755);
    if (ret != 0 && errno != EEXIST) {
        char msg[MSL];
        snprintf(msg, sizeof(msg), "json_persist_init: Failed to create %s: %s",
                 objects_dir, strerror(errno));
        emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                "persist_init_mkdir_failed", objects_dir);
        return false;
    }

    log_string("json_persist_init: Directory structure created.");
    return true;
}

/***************************************************************************
 * Path Generation                                                         *
 ***************************************************************************/

void json_persist_object_path(unsigned long id0, unsigned long id1, char *buf, size_t bufsize)
{
    char objects_dir_buf[MAX_INPUT_LENGTH];
    const char *objects_dir = persist_json_objects_path(objects_dir_buf, sizeof(objects_dir_buf));
    snprintf(buf, bufsize, "%s%lu_%lu.json", objects_dir, id0, id1);
}

void json_persist_mobile_path(unsigned long id0, unsigned long id1, char *buf, size_t bufsize)
{
    char mobiles_dir_buf[MAX_INPUT_LENGTH];
    const char *mobiles_dir = persist_json_mobiles_path(mobiles_dir_buf, sizeof(mobiles_dir_buf));
    snprintf(buf, bufsize, "%s%lu_%lu.json", mobiles_dir, id0, id1);
}

void json_persist_room_id(ROOM_INDEX_DATA *room, char *buf, size_t bufsize)
{
    if (!room) {
        snprintf(buf, bufsize, "invalid");
        return;
    }

    if (room->wilds) {
        /* Wilderness room: wilds_<uid>_<x>_<y>_<z> */
        snprintf(buf, bufsize, "wilds_%lu_%ld_%ld_%ld",
                room->wilds->uid, room->x, room->y, room->z);
    } else if (room->source) {
        /* Cloned room: clone_<source_vnum>_<id0>_<id1> */
        snprintf(buf, bufsize, "clone_%ld_%lu_%lu",
                room->source->vnum, room->id[0], room->id[1]);
    } else {
        /* Static room: vnum */
        snprintf(buf, bufsize, "%ld", room->vnum);
    }
}

void json_persist_room_path(ROOM_INDEX_DATA *room, char *buf, size_t bufsize)
{
    char rooms_dir_buf[MAX_INPUT_LENGTH];
    const char *rooms_dir = persist_json_rooms_path(rooms_dir_buf, sizeof(rooms_dir_buf));
    char room_id[256];
    json_persist_room_id(room, room_id, sizeof(room_id));
    snprintf(buf, bufsize, "%s%s.json", rooms_dir, room_id);
}

/***************************************************************************
 * Lock Serialization                                                      *
 ***************************************************************************/

json_t *json_persist_lock_to_json(LOCK_STATE *lock)
{
    json_t *json;

    if (!lock) return NULL;

    json = json_object();
    if (lock->key_load.auid > 0)
        json_object_set_new(json, "key_auid", json_integer(lock->key_load.auid));
    if (lock->key_load.vnum > 0)
        json_object_set_new(json, "key_vnum", json_integer(lock->key_load.vnum));
    json_object_set_new(json, "flags", json_string(flag_string(lock_flags, lock->flags)));
    json_object_set_new(json, "pick_chance", json_integer(lock->pick_chance));

    return json;
}

LOCK_STATE *json_persist_json_to_lock(json_t *json)
{
    LOCK_STATE *lock;
    json_t *value;

    if (!json) return NULL;

    lock = new_lock_state();

    value = json_object_get(json, "key_auid");
    if (value) lock->key_load.auid = json_integer_value(value);

    value = json_object_get(json, "key_vnum");
    if (value) lock->key_load.vnum = json_integer_value(value);

    value = json_object_get(json, "flags");
    if (value) lock->flags = flag_value(lock_flags, (char *)json_string_value(value));

    value = json_object_get(json, "pick_chance");
    if (value) lock->pick_chance = json_integer_value(value);

    return lock;
}

/***************************************************************************
 * Location Serialization                                                  *
 ***************************************************************************/

json_t *json_persist_location_to_json(LOCATION *loc)
{
    json_t *json;

    if (!loc) return NULL;

    json = json_object();
    json_object_set_new(json, "wuid", json_integer(loc->wuid));
    json_object_set_new(json, "id0", json_integer(loc->id[0]));
    json_object_set_new(json, "id1", json_integer(loc->id[1]));
    json_object_set_new(json, "id2", json_integer(loc->id[2]));

    return json;
}

bool json_persist_json_to_location(json_t *json, LOCATION *loc)
{
    json_t *value;

    if (!json || !loc) return false;

    memset(loc, 0, sizeof(LOCATION));

    value = json_object_get(json, "wuid");
    if (value) loc->wuid = json_integer_value(value);

    value = json_object_get(json, "id0");
    if (value) loc->id[0] = json_integer_value(value);

    value = json_object_get(json, "id1");
    if (value) loc->id[1] = json_integer_value(value);

    value = json_object_get(json, "id2");
    if (value) loc->id[2] = json_integer_value(value);

    return true;
}

/***************************************************************************
 * Spell Serialization (for objects)                                       *
 ***************************************************************************/

static json_t *spell_to_json(SPELL_DATA *spell)
{
    json_t *json;

    if (!spell) return NULL;

    json = json_object();
    json_object_set_new(json, "sn", json_integer(spell->sn));
    json_object_set_new(json, "level", json_integer(spell->level));
    json_object_set_new(json, "repop", json_integer(spell->repop));

    return json;
}

static SPELL_DATA *json_to_spell(json_t *json)
{
    SPELL_DATA *spell;
    json_t *value;

    if (!json) return NULL;

    spell = alloc_mem(sizeof(SPELL_DATA));
    memset(spell, 0, sizeof(SPELL_DATA));

    value = json_object_get(json, "sn");
    if (value) spell->sn = json_integer_value(value);

    value = json_object_get(json, "level");
    if (value) spell->level = json_integer_value(value);

    value = json_object_get(json, "repop");
    if (value) spell->repop = json_integer_value(value);

    return spell;
}

/***************************************************************************
 * Waypoint Serialization                                                  *
 ***************************************************************************/

static json_t *waypoint_to_json(WAYPOINT_DATA *wp)
{
    json_t *json;

    if (!wp) return NULL;

    json = json_object();
    json_object_set_new(json, "w", json_integer(wp->w));
    json_object_set_new(json, "x", json_integer(wp->x));
    json_object_set_new(json, "y", json_integer(wp->y));
    if (wp->name) {
        json_object_set_new(json, "name", json_string(wp->name));
    }

    return json;
}

static WAYPOINT_DATA *json_to_waypoint(json_t *json)
{
    WAYPOINT_DATA *wp;
    json_t *value;

    if (!json) return NULL;

    wp = new_waypoint();

    value = json_object_get(json, "w");
    if (value) wp->w = json_integer_value(value);

    value = json_object_get(json, "x");
    if (value) wp->x = json_integer_value(value);

    value = json_object_get(json, "y");
    if (value) wp->y = json_integer_value(value);

    value = json_object_get(json, "name");
    if (value) {
        wp->name = str_dup(json_string_value(value));
    }

    return wp;
}

/***************************************************************************
 * Variable Serialization                                                  *
 ***************************************************************************/

static json_t *variable_to_json(pVARIABLE var)
{
    json_t *json;

    if (!var || !var->save) return NULL;

    json = json_object();
    json_object_set_new(json, "name", json_string(var->name));
    json_object_set_new(json, "type", json_integer(var->type));

    switch (var->type) {
    case VAR_BOOLEAN:
        json_object_set_new(json, "value", json_boolean(var->_.boolean));
        break;
    case VAR_INTEGER:
        json_object_set_new(json, "value", json_integer(var->_.i));
        break;
    case VAR_STRING:
    case VAR_STRING_S:
        json_object_set_new(json, "value", json_string_safe(var->_.s));
        break;
    case VAR_ROOM:
        if (var->_.r) {
            json_t *room_ref = json_object();
            if (var->_.r->wilds) {
                json_object_set_new(room_ref, "type", json_string("wilds"));
                json_object_set_new(room_ref, "wuid", json_integer(var->_.r->wilds->uid));
                json_object_set_new(room_ref, "x", json_integer(var->_.r->x));
                json_object_set_new(room_ref, "y", json_integer(var->_.r->y));
            } else if (var->_.r->source) {
                json_object_set_new(room_ref, "type", json_string("clone"));
                json_object_set_new(room_ref, "vnum", json_string(widevnum_string_room(var->_.r->source, NULL)));
                json_object_set_new(room_ref, "id0", json_integer(var->_.r->id[0]));
                json_object_set_new(room_ref, "id1", json_integer(var->_.r->id[1]));
            } else {
                json_object_set_new(room_ref, "type", json_string("static"));
                json_object_set_new(room_ref, "vnum", json_string(widevnum_string_room(var->_.r, NULL)));
            }
            json_object_set_new(json, "value", room_ref);
        }
        break;
    default:
        /* For other types, store a placeholder */
        json_object_set_new(json, "value", json_null());
        break;
    }

    return json;
}

static bool json_load_variable(json_t *json, pVARIABLE *vars)
{
    json_t *value, *type_val, *name_val;
    int type;
    const char *name;

    if (!json || !vars) return false;

    name_val = json_object_get(json, "name");
    type_val = json_object_get(json, "type");
    value = json_object_get(json, "value");

    if (!name_val || !type_val) return false;

    name = json_string_value(name_val);
    type = json_integer_value(type_val);

    switch (type) {
    case VAR_BOOLEAN:
        return variables_setsave_boolean(vars, (char *)name, json_boolean_value(value), true);

    case VAR_INTEGER:
        return variables_setsave_integer(vars, (char *)name, json_integer_value(value), true);

    case VAR_STRING:
    case VAR_STRING_S:
        if (value && json_is_string(value)) {
            return variables_setsave_string(vars, (char *)name,
                (char *)json_string_value(value), true, true);
        }
        break;

    case VAR_ROOM:
        if (value && json_is_object(value)) {
            json_t *room_type = json_object_get(value, "type");
            const char *rtype = json_string_value(room_type);

            if (!str_cmp(rtype, "static")) {
                json_t *vnum_val = json_object_get(value, "vnum");
                ROOM_INDEX_DATA *room = NULL;
                if (json_is_string(vnum_val)) {
                    /* New widevnum format */
                    WNUM room_wnum;
                    if (parse_widevnum((char *)json_string_value(vnum_val), NULL, &room_wnum) && room_wnum.pArea) {
                        room = get_room_index(room_wnum.pArea, room_wnum.vnum);
                    }
                } else {
                    /* Legacy integer format */
                    long vnum = json_integer_value(vnum_val);
                    room = get_room_index_global(vnum);
                }
                if (room) {
                    return variables_setsave_room(vars, (char *)name, room, true);
                }
            } else if (!str_cmp(rtype, "clone")) {
                /* Clone room references need deferred resolution */
                json_t *src_vnum_val = json_object_get(value, "vnum");
                ROOM_INDEX_DATA *source = NULL;
                if (json_is_string(src_vnum_val)) {
                    /* New widevnum format */
                    WNUM room_wnum;
                    if (parse_widevnum((char *)json_string_value(src_vnum_val), NULL, &room_wnum) && room_wnum.pArea) {
                        source = get_room_index(room_wnum.pArea, room_wnum.vnum);
                    }
                } else {
                    /* Legacy integer format */
                    long src_vnum = json_integer_value(src_vnum_val);
                    source = get_room_index_global(src_vnum);
                }
                int id0 = json_integer_value(json_object_get(value, "id0"));
                int id1 = json_integer_value(json_object_get(value, "id1"));
                if (source) {
                    return variables_set_clone_room(vars, (char *)name, source, id0, id1, true);
                }
            } else if (!str_cmp(rtype, "wilds")) {
                /* Wilderness room references */
                int wuid = json_integer_value(json_object_get(value, "wuid"));
                int x = json_integer_value(json_object_get(value, "x"));
                int y = json_integer_value(json_object_get(value, "y"));
                return variables_set_wilds_room(vars, (char *)name, wuid, x, y, true);
            }
        }
        break;

    default:
        /* Other variable types not yet supported in JSON persistence */
        break;
    }

    return false;
}

/***************************************************************************
 * Affect Serialization                                                    *
 ***************************************************************************/

json_t *json_persist_affect_to_json(AFFECT_DATA *paf)
{
    json_t *json;

    if (!paf) return NULL;

    json = json_object();

    json_object_set_new(json, "type", json_integer(paf->type));
    json_object_set_new(json, "where", json_integer(paf->where));
    json_object_set_new(json, "group", json_integer(paf->group));
    json_object_set_new(json, "level", json_integer(paf->level));
    json_object_set_new(json, "duration", json_integer(paf->duration));
    json_object_set_new(json, "modifier", json_integer(paf->modifier));
    json_object_set_new(json, "location", json_integer(paf->location));
    json_object_set_new(json, "bitvector", json_integer(paf->bitvector));
    json_object_set_new(json, "bitvector2", json_integer(paf->bitvector2));

    if (paf->custom_name) {
        json_object_set_new(json, "custom_name", json_string(paf->custom_name));
    }

    /* For skill-based locations, store skill name for readability */
    if (paf->location >= APPLY_SKILL && paf->location < APPLY_SKILL_MAX) {
        int skill_index = paf->location - APPLY_SKILL;
        if (skill_table[skill_index].name) {
            json_object_set_new(json, "skill_name", json_string(skill_table[skill_index].name));
        }
    }

    /* For skill-based types, store skill name */
    if (paf->type >= 0 && paf->type < MAX_SKILL && skill_table[paf->type].name) {
        json_object_set_new(json, "type_name", json_string(skill_table[paf->type].name));
    }

    return json;
}

AFFECT_DATA *json_persist_json_to_affect(json_t *json)
{
    AFFECT_DATA *paf;
    json_t *value;

    if (!json) return NULL;

    paf = new_affect();

    value = json_object_get(json, "type");
    if (value) paf->type = json_integer_value(value);
    paf->skill = skill_find_uid(paf->type);

    value = json_object_get(json, "where");
    if (value) paf->where = json_integer_value(value);

    value = json_object_get(json, "group");
    if (value) paf->group = json_integer_value(value);

    value = json_object_get(json, "level");
    if (value) paf->level = json_integer_value(value);

    value = json_object_get(json, "duration");
    if (value) paf->duration = json_integer_value(value);

    value = json_object_get(json, "modifier");
    if (value) paf->modifier = json_integer_value(value);

    value = json_object_get(json, "location");
    if (value) paf->location = json_integer_value(value);

    value = json_object_get(json, "bitvector");
    if (value) paf->bitvector = json_integer_value(value);

    value = json_object_get(json, "bitvector2");
    if (value) paf->bitvector2 = json_integer_value(value);

    value = json_object_get(json, "custom_name");
    if (value) {
        paf->custom_name = create_affect_cname((char *)json_string_value(value));
    }

    return paf;
}

/***************************************************************************
 * Script Data Serialization                                               *
 ***************************************************************************/

json_t *json_persist_scriptdata_to_json(PROG_DATA *progs)
{
    json_t *vars_array;
    pVARIABLE var;

    if (!progs || !progs->vars) return NULL;

    vars_array = json_array();

    for (var = progs->vars; var; var = var->next) {
        if (var->save) {
            json_t *var_json = variable_to_json(var);
            if (var_json) {
                json_array_append_new(vars_array, var_json);
            }
        }
    }

    if (json_array_size(vars_array) == 0) {
        json_decref(vars_array);
        return NULL;
    }

    return vars_array;
}

bool json_persist_json_to_scriptdata(json_t *json, PROG_DATA **progs)
{
    size_t index;
    json_t *elem;

    if (!json || !json_is_array(json) || !progs) return false;

    if (!*progs) {
        *progs = new_prog_data();
    }

    json_array_foreach(json, index, elem) {
        json_load_variable(elem, &(*progs)->vars);
    }

    return true;
}

/***************************************************************************
 * Token Serialization                                                     *
 ***************************************************************************/

json_t *json_persist_token_to_json(TOKEN_DATA *token)
{
    json_t *json, *values_array, *scriptdata;
    int i;

    if (!token || !token->pIndexData) return NULL;

    json = json_object();

    /* Use widevnum format for area-scoped persistence */
    json_object_set_new(json, "vnum", json_string(widevnum_string(token->pIndexData->area, token->pIndexData->vnum, NULL)));
    json_object_set_new(json, "id0", json_integer(token->id[0]));
    json_object_set_new(json, "id1", json_integer(token->id[1]));
    json_object_set_new(json, "timer", json_integer(token->timer));

    /* Values array */
    values_array = json_array();
    for (i = 0; i < MAX_TOKEN_VALUES; i++) {
        json_array_append_new(values_array, json_integer(token->value[i]));
    }
    json_object_set_new(json, "values", values_array);

    /* Script variables */
    if (token->progs) {
        scriptdata = json_persist_scriptdata_to_json(token->progs);
        if (scriptdata) {
            json_object_set_new(json, "variables", scriptdata);
        }
    }

    return json;
}

TOKEN_DATA *json_persist_json_to_token(json_t *json)
{
    TOKEN_DATA *token;
    TOKEN_INDEX_DATA *pTokenIndex;
    json_t *value, *values_array, *scriptdata;
    WNUM wnum;
    int i;

    if (!json) return NULL;

    /* Parse vnum - supports both widevnum string and legacy integer */
    value = json_object_get(json, "vnum");
    if (json_is_string(value)) {
        /* New widevnum format: "auid#vnum" */
        if (!parse_widevnum((char *)json_string_value(value), NULL, &wnum) || !wnum.pArea) {
            log_stringf("json_persist_json_to_token: bad widevnum '%s'", json_string_value(value));
            return NULL;
        }
        pTokenIndex = get_token_index(wnum.pArea, wnum.vnum);
    } else {
        /* Legacy integer format */
        long vnum = json_integer_value(value);
        AREA_DATA *pArea = NULL;
        WNUM wnum;
        if (resolve_widevnum(vnum, NULL, &wnum))
            pArea = wnum.pArea;
        if (!pArea) pArea = get_system_area_fallback();
        pTokenIndex = get_token_index(pArea, vnum);
    }
    if (!pTokenIndex) {
        log_stringf("json_persist_json_to_token: token index not found");
        return NULL;
    }

    token = create_token(pTokenIndex);
    if (!token) return NULL;

    value = json_object_get(json, "id0");
    if (value) token->id[0] = json_integer_value(value);

    value = json_object_get(json, "id1");
    if (value) token->id[1] = json_integer_value(value);

    value = json_object_get(json, "timer");
    if (value) token->timer = json_integer_value(value);

    values_array = json_object_get(json, "values");
    if (values_array && json_is_array(values_array)) {
        for (i = 0; i < MAX_TOKEN_VALUES && i < (int)json_array_size(values_array); i++) {
            token->value[i] = json_integer_value(json_array_get(values_array, i));
        }
    }

    scriptdata = json_object_get(json, "variables");
    if (scriptdata) {
        json_persist_json_to_scriptdata(scriptdata, &token->progs);
    }

    /* Assign a unique token ID if not already set (loaded from JSON) */
    get_token_id(token);

    return token;
}

/***************************************************************************
 * Object Serialization (Full State)                                       *
 ***************************************************************************/

json_t *json_persist_object_to_json(OBJ_DATA *obj)
{
    json_t *json, *array, *stache_array;
    AFFECT_DATA *paf;
    EXTRA_DESCR_DATA *ed;
    SPELL_DATA *spell;
    TOKEN_DATA *token;
    WAYPOINT_DATA *wp;
    OBJ_DATA *cont_obj;
    ITERATOR it;
    int i;

    if (!obj || !obj->pIndexData) return NULL;

    json = json_object();

    /* Metadata */
    json_object_set_new(json, "json_version", json_integer(JSON_PERSIST_VERSION_OBJECT));

    /* Identification - use widevnum format for area-scoped persistence */
    json_object_set_new(json, "vnum", json_string(widevnum_string_object(obj->pIndexData, NULL)));
    json_object_set_new(json, "id0", json_integer(obj->id[0]));
    json_object_set_new(json, "id1", json_integer(obj->id[1]));
    json_object_set_new(json, "persist", json_boolean(obj->persist));
    json_object_set_new(json, "version", json_integer(obj->version));

    /* Descriptions */
    json_object_set_new(json, "name", json_string_safe(obj->name));
    json_object_set_new(json, "short_descr", json_string_safe(obj->short_descr));
    json_object_set_new(json, "description", json_string_safe(obj->description));
    if (obj->full_description) {
        json_object_set_new(json, "full_description", json_string(obj->full_description));
    }

    /* Old descriptions (for restoration) */
    if (obj->old_name) json_object_set_new(json, "old_name", json_string(obj->old_name));
    if (obj->old_short_descr) json_object_set_new(json, "old_short_descr", json_string(obj->old_short_descr));
    if (obj->old_description) json_object_set_new(json, "old_description", json_string(obj->old_description));
    if (obj->old_full_description) json_object_set_new(json, "old_full_description", json_string(obj->old_full_description));

    /* Ownership */
    if (obj->owner) json_object_set_new(json, "owner", json_string(obj->owner));
    if (obj->loaded_by) json_object_set_new(json, "loaded_by", json_string(obj->loaded_by));
    if (obj->owner_name) json_object_set_new(json, "owner_name", json_string(obj->owner_name));
    if (obj->owner_short) json_object_set_new(json, "owner_short", json_string(obj->owner_short));

    /* Item properties */
    json_object_set_new(json, "item_type", json_integer(obj->item_type));
    json_object_set_new(json, "wear_flags", json_integer(obj->wear_flags));
    json_object_set_new(json, "wear_loc", json_integer(obj->wear_loc));
    json_object_set_new(json, "last_wear_loc", json_integer(obj->last_wear_loc));
    json_object_set_new(json, "level", json_integer(obj->level));
    json_object_set_new(json, "weight", json_integer(obj->weight));
    json_object_set_new(json, "cost", json_integer(obj->cost));
    json_object_set_new(json, "timer", json_integer(obj->timer));
    json_object_set_new(json, "condition", json_integer(obj->condition));
    json_object_set_new(json, "num_enchanted", json_integer(obj->num_enchanted));
    json_object_set_new(json, "fragility", json_integer(obj->fragility));
    json_object_set_new(json, "times_allowed_fixed", json_integer(obj->times_allowed_fixed));
    json_object_set_new(json, "times_fixed", json_integer(obj->times_fixed));
    json_object_set_new(json, "locker", json_boolean(obj->locker));
    json_object_set_new(json, "stached", json_boolean(obj->stached));

    /* Extra flags */
    array = json_array();
    for (i = 0; i < 4; i++) {
        json_array_append_new(array, json_integer(obj->extra[i]));
    }
    json_object_set_new(json, "extra", array);

    /* Permanent extra flags */
    array = json_array();
    for (i = 0; i < 4; i++) {
        json_array_append_new(array, json_integer(obj->extra_perm[i]));
    }
    json_object_set_new(json, "extra_perm", array);

    /* Weapon flags (if applicable) */
    if (obj->item_type == ITEM_WEAPON) {
        json_object_set_new(json, "weapon_flags_perm", json_integer(obj->weapon_flags_perm));
    }

    /* Type-specific data (canonical, replaces legacy values[]) */
    {
        json_t *td = obj_type_data_to_json(obj);
        if (td) {
            json_object_set_new(json, "type_data", td);
        }
    }

    /* Location - use widevnum format for room references */
    if (obj->in_room) {
        json_t *loc = json_object();
        if (obj->in_room->wilds) {
            json_object_set_new(loc, "type", json_string("wilds"));
            json_object_set_new(loc, "wuid", json_integer(obj->in_room->wilds->uid));
            json_object_set_new(loc, "x", json_integer(obj->in_room->x));
            json_object_set_new(loc, "y", json_integer(obj->in_room->y));
        } else if (obj->in_room->source) {
            json_object_set_new(loc, "type", json_string("clone"));
            json_object_set_new(loc, "vnum", json_string(widevnum_string_room(obj->in_room->source, NULL)));
            json_object_set_new(loc, "id0", json_integer(obj->in_room->id[0]));
            json_object_set_new(loc, "id1", json_integer(obj->in_room->id[1]));
        } else {
            json_object_set_new(loc, "type", json_string("static"));
            json_object_set_new(loc, "vnum", json_string(widevnum_string_room(obj->in_room, NULL)));
        }
        json_object_set_new(json, "location", loc);
    }

    /* Lock */
    if (obj->lock) {
        json_t *lock_json = json_persist_lock_to_json(obj->lock);
        if (lock_json) {
            json_object_set_new(json, "lock", lock_json);
        }
    }

    /* Waypoints */
    if (obj->waypoints && IS_VALID(obj->waypoints)) {
        array = json_array();
        iterator_start(&it, obj->waypoints);
        while ((wp = (WAYPOINT_DATA *)iterator_nextdata(&it))) {
            json_t *wp_json = waypoint_to_json(wp);
            if (wp_json) {
                json_array_append_new(array, wp_json);
            }
        }
        iterator_stop(&it);
        if (json_array_size(array) > 0) {
            json_object_set_new(json, "waypoints", array);
        } else {
            json_decref(array);
        }
    }

    /* Spells */
    if (obj->spells) {
        array = json_array();
        for (spell = obj->spells; spell; spell = spell->next) {
            json_t *spell_json = spell_to_json(spell);
            if (spell_json) {
                json_array_append_new(array, spell_json);
            }
        }
        if (json_array_size(array) > 0) {
            json_object_set_new(json, "spells", array);
        } else {
            json_decref(array);
        }
    }

    /* Affects */
    if (obj->affected) {
        array = json_array();
        for (paf = obj->affected; paf; paf = paf->next) {
            json_t *aff_json = json_persist_affect_to_json(paf);
            if (aff_json) {
                json_array_append_new(array, aff_json);
            }
        }
        if (json_array_size(array) > 0) {
            json_object_set_new(json, "affects", array);
        } else {
            json_decref(array);
        }
    }

    /* Catalysts */
    if (obj->catalyst) {
        array = json_array();
        for (CATALYST_DATA *cat = obj->catalyst; cat; cat = cat->next) {
            json_t *cat_json = json_object();
            json_object_set_new(cat_json, "where", json_integer(cat->where));
            json_object_set_new(cat_json, "level", json_integer(cat->level));
            json_object_set_new(cat_json, "duration", json_integer(cat->duration));
            json_object_set_new(cat_json, "modifier", json_integer(cat->modifier));
            json_object_set_new(cat_json, "random", json_integer(cat->random));
            if (!IS_NULLSTR(cat->custom_name))
                json_object_set_new(cat_json, "custom_name", json_string(cat->custom_name));
            json_object_set_new(cat_json, "catalyst_type",
                json_string(flag_string(catalyst_types, cat->type)));
            json_array_append_new(array, cat_json);
        }
        if (json_array_size(array) > 0) {
            json_object_set_new(json, "catalysts", array);
        } else {
            json_decref(array);
        }
    }

    /* Extra descriptions */
    if (obj->extra_descr) {
        array = json_array();
        for (ed = obj->extra_descr; ed; ed = ed->next) {
            json_t *ed_json = json_object();
            json_object_set_new(ed_json, "keyword", json_string_safe(ed->keyword));
            json_object_set_new(ed_json, "description", json_string_safe(ed->description));
            json_array_append_new(array, ed_json);
        }
        if (json_array_size(array) > 0) {
            json_object_set_new(json, "extra_descr", array);
        } else {
            json_decref(array);
        }
    }

    /* Script variables */
    if (obj->progs) {
        json_t *vars = json_persist_scriptdata_to_json(obj->progs);
        if (vars) {
            json_object_set_new(json, "variables", vars);
        }
    }

    /* Tokens */
    if (obj->tokens) {
        array = json_array();
        for (token = obj->tokens; token; token = token->next) {
            json_t *tok_json = json_persist_token_to_json(token);
            if (tok_json) {
                json_array_append_new(array, tok_json);
            }
        }
        if (json_array_size(array) > 0) {
            json_object_set_new(json, "tokens", array);
        } else {
            json_decref(array);
        }
    }

    /* Contained objects (recursive) */
    if (obj->contains) {
        array = json_array();
        for (cont_obj = obj->contains; cont_obj; cont_obj = cont_obj->next_content) {
            json_t *cont_json = json_persist_object_to_json(cont_obj);
            if (cont_json) {
                json_array_append_new(array, cont_json);
            }
        }
        if (json_array_size(array) > 0) {
            json_object_set_new(json, "contains", array);
        } else {
            json_decref(array);
        }
    }

    /* Stached objects (recursive) */
    stache_array = json_array();
    if (obj->lstache && IS_VALID(obj->lstache)) {
        OBJ_DATA *stached_obj;
        iterator_start(&it, obj->lstache);
        while ((stached_obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            json_t *stached_json = json_persist_object_to_json(stached_obj);
            if (stached_json) {
                json_array_append_new(stache_array, stached_json);
            }
        }
        iterator_stop(&it);
    }
    if (json_array_size(stache_array) > 0) {
        json_object_set_new(json, "stache", stache_array);
    } else {
        json_decref(stache_array);
    }

    return json;
}

/***************************************************************************
 * Object Deserialization                                                  *
 ***************************************************************************/

OBJ_DATA *json_persist_json_to_object(json_t *json)
{
    OBJ_DATA *obj;
    OBJ_INDEX_DATA *pObjIndex;
    json_t *value, *array, *elem;
    WNUM wnum;
    size_t index;
    int i;

    if (!json) return NULL;

    /* Parse vnum - supports both widevnum string and legacy integer */
    value = json_object_get(json, "vnum");
    if (json_is_string(value)) {
        /* New widevnum format: "auid#vnum" */
        if (!parse_widevnum((char *)json_string_value(value), NULL, &wnum) || !wnum.pArea) {
            log_stringf("json_persist_json_to_object: bad widevnum '%s'", json_string_value(value));
            return NULL;
        }
        pObjIndex = get_obj_index(wnum.pArea, wnum.vnum);
    } else {
        /* Legacy integer format */
        long vnum = json_integer_value(value);
        AREA_DATA *pArea = NULL;
        WNUM wnum;
        if (resolve_widevnum(vnum, NULL, &wnum))
            pArea = wnum.pArea;
        if (!pArea) pArea = get_system_area_fallback();
        pObjIndex = get_obj_index(pArea, vnum);
    }
    if (!pObjIndex) {
        log_stringf("json_persist_json_to_object: object index not found");
        return NULL;
    }

    obj = create_object_noid(pObjIndex, -1, false, false);
    if (!obj) return NULL;

    /* IDs */
    value = json_object_get(json, "id0");
    if (value) obj->id[0] = json_integer_value(value);
    value = json_object_get(json, "id1");
    if (value) obj->id[1] = json_integer_value(value);

    /* O(1) deduplication check */
    if (obj->id[0] || obj->id[1]) {
        OBJ_DATA *existing = loaded_obj_hash_find(obj->id[0], obj->id[1]);
        if (existing) {
            log_stringf("json_persist_json_to_object: DUPLICATE object vnum=%ld '%s' (id=%lu/%lu) - already loaded as '%s', skipping",
                       obj->pIndexData ? obj->pIndexData->vnum : 0,
                       obj->short_descr ? obj->short_descr : "(null)",
                       obj->id[0], obj->id[1],
                       existing->short_descr ? existing->short_descr : "(null)");
            free_obj(obj);
            return NULL;
        }
    }

    value = json_object_get(json, "persist");
    if (value) obj->persist = json_boolean_value(value);

    value = json_object_get(json, "version");
    if (value) obj->version = json_integer_value(value);

    /* Descriptions */
    value = json_object_get(json, "name");
    if (value) {
        free_string(obj->name);
        obj->name = str_dup(json_string_value(value));
    }
    value = json_object_get(json, "short_descr");
    if (value) {
        free_string(obj->short_descr);
        obj->short_descr = str_dup(json_string_value(value));
    }
    value = json_object_get(json, "description");
    if (value) {
        free_string(obj->description);
        obj->description = str_dup(json_string_value(value));
    }
    value = json_object_get(json, "full_description");
    if (value) {
        free_string(obj->full_description);
        obj->full_description = str_dup(json_string_value(value));
    }

    /* Old descriptions */
    value = json_object_get(json, "old_name");
    if (value) obj->old_name = str_dup(json_string_value(value));
    value = json_object_get(json, "old_short_descr");
    if (value) obj->old_short_descr = str_dup(json_string_value(value));
    value = json_object_get(json, "old_description");
    if (value) obj->old_description = str_dup(json_string_value(value));
    value = json_object_get(json, "old_full_description");
    if (value) obj->old_full_description = str_dup(json_string_value(value));

    /* Ownership */
    value = json_object_get(json, "owner");
    if (value) {
        free_string(obj->owner);
        obj->owner = str_dup(json_string_value(value));
    }
    value = json_object_get(json, "loaded_by");
    if (value) obj->loaded_by = str_dup(json_string_value(value));
    value = json_object_get(json, "owner_name");
    if (value) obj->owner_name = str_dup(json_string_value(value));
    value = json_object_get(json, "owner_short");
    if (value) obj->owner_short = str_dup(json_string_value(value));

    /* Item properties */
    value = json_object_get(json, "item_type");
    if (value) obj->item_type = json_integer_value(value);
    value = json_object_get(json, "wear_flags");
    if (value) obj->wear_flags = json_integer_value(value);
    value = json_object_get(json, "wear_loc");
    if (value) obj->wear_loc = json_integer_value(value);
    value = json_object_get(json, "last_wear_loc");
    if (value) obj->last_wear_loc = json_integer_value(value);
    value = json_object_get(json, "level");
    if (value) obj->level = json_integer_value(value);
    value = json_object_get(json, "weight");
    if (value) obj->weight = json_integer_value(value);
    value = json_object_get(json, "cost");
    if (value) obj->cost = json_integer_value(value);
    value = json_object_get(json, "timer");
    if (value) obj->timer = json_integer_value(value);
    value = json_object_get(json, "condition");
    if (value) obj->condition = json_integer_value(value);
    value = json_object_get(json, "num_enchanted");
    if (value) obj->num_enchanted = json_integer_value(value);
    value = json_object_get(json, "fragility");
    if (value) obj->fragility = json_integer_value(value);
    value = json_object_get(json, "times_allowed_fixed");
    if (value) obj->times_allowed_fixed = json_integer_value(value);
    value = json_object_get(json, "times_fixed");
    if (value) obj->times_fixed = json_integer_value(value);
    value = json_object_get(json, "locker");
    if (value) obj->locker = json_boolean_value(value);
    value = json_object_get(json, "stached");
    if (value) obj->stached = json_boolean_value(value);

    /* Extra flags */
    array = json_object_get(json, "extra");
    if (array && json_is_array(array)) {
        for (i = 0; i < 4 && i < (int)json_array_size(array); i++) {
            obj->extra[i] = json_integer_value(json_array_get(array, i));
        }
    }

    /* Permanent extra flags */
    array = json_object_get(json, "extra_perm");
    if (array && json_is_array(array)) {
        for (i = 0; i < 4 && i < (int)json_array_size(array); i++) {
            obj->extra_perm[i] = json_integer_value(json_array_get(array, i));
        }
    }

    /* Weapon flags */
    value = json_object_get(json, "weapon_flags_perm");
    if (value) obj->weapon_flags_perm = json_integer_value(value);

    /* Values */
    array = json_object_get(json, "values");
    if (array && json_is_array(array)) {
        for (i = 0; i < 8 && i < (int)json_array_size(array); i++) {
            obj->value[i] = json_integer_value(json_array_get(array, i));
        }
    }

    /* Type-specific data (canonical structured representation) */
    value = json_object_get(json, "type_data");
    if (value && json_is_object(value)) {
        obj_type_data_from_json(obj, value);
    }

    /* Location - stored for later resolution */
    value = json_object_get(json, "location");
    if (value && json_is_object(value)) {
        json_t *loc_type = json_object_get(value, "type");
        const char *type = json_string_value(loc_type);

        if (!str_cmp(type, "static")) {
            json_t *vnum_val = json_object_get(value, "vnum");
            if (json_is_string(vnum_val)) {
                /* New widevnum format */
                WNUM room_wnum;
                if (parse_widevnum((char *)json_string_value(vnum_val), NULL, &room_wnum) && room_wnum.pArea) {
                    obj->in_room = get_room_index(room_wnum.pArea, room_wnum.vnum);
                }
            } else {
                /* Legacy integer format */
                long room_vnum = json_integer_value(vnum_val);
                obj->in_room = get_room_index_global(room_vnum);
            }
        } else if (!str_cmp(type, "clone")) {
            /* Clone room - needs deferred resolution */
            obj->in_room = NULL;
        } else if (!str_cmp(type, "wilds")) {
            /* Wilderness room - needs deferred resolution */
            obj->in_room = NULL;
        }
    }

    /* Lock */
    value = json_object_get(json, "lock");
    if (value) {
        obj->lock = json_persist_json_to_lock(value);
    }

    /* Waypoints */
    array = json_object_get(json, "waypoints");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            WAYPOINT_DATA *wp = json_to_waypoint(elem);
            if (wp) {
                if (!obj->waypoints) {
                    obj->waypoints = list_create(false);
                }
                list_appendlink(obj->waypoints, wp);
            }
        }
    }

    /* Spells */
    array = json_object_get(json, "spells");
    if (array && json_is_array(array)) {
        SPELL_DATA *last = NULL;
        json_array_foreach(array, index, elem) {
            SPELL_DATA *spell = json_to_spell(elem);
            if (spell) {
                if (!obj->spells) {
                    obj->spells = spell;
                } else {
                    last->next = spell;
                }
                last = spell;
            }
        }
    }

    /* Affects */
    array = json_object_get(json, "affects");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            AFFECT_DATA *paf = json_persist_json_to_affect(elem);
            if (paf) {
                paf->next = obj->affected;
                obj->affected = paf;
            }
        }
    }

    /* Catalysts */
    array = json_object_get(json, "catalysts");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            CATALYST_DATA *cat = new_catalyst();
            if (cat) {
                json_t *value;

                value = json_object_get(elem, "where");
                if (value) cat->where = json_integer_value(value);

                value = json_object_get(elem, "level");
                if (value) cat->level = json_integer_value(value);

                value = json_object_get(elem, "duration");
                if (value) cat->duration = json_integer_value(value);

                value = json_object_get(elem, "modifier");
                if (value) cat->modifier = json_integer_value(value);

                value = json_object_get(elem, "random");
                if (value) cat->random = json_integer_value(value);

                value = json_object_get(elem, "custom_name");
                if (value && json_is_string(value))
                    cat->custom_name = str_dup(json_string_value(value));

                value = json_object_get(elem, "catalyst_type");
                if (value && json_is_string(value))
                    cat->type = flag_value(catalyst_types, (char *)json_string_value(value));

                cat->next = obj->catalyst;
                obj->catalyst = cat;
            }
        }
    }

    /* Extra descriptions */
    array = json_object_get(json, "extra_descr");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            EXTRA_DESCR_DATA *ed = new_extra_descr();
            ed->keyword = str_dup(json_get_string(elem, "keyword", ""));
            ed->description = str_dup(json_get_string(elem, "description", ""));
            ed->next = obj->extra_descr;
            obj->extra_descr = ed;
        }
    }

    /* Script variables */
    value = json_object_get(json, "variables");
    if (value) {
        json_persist_json_to_scriptdata(value, &obj->progs);
    }

    /* Tokens */
    array = json_object_get(json, "tokens");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            TOKEN_DATA *token = json_persist_json_to_token(elem);
            if (token) {
                token_to_obj(token, obj);
            }
        }
    }

    /* Contained objects (recursive) */
    array = json_object_get(json, "contains");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            OBJ_DATA *cont_obj = json_persist_json_to_object(elem);
            if (cont_obj) {
                obj_to_obj(cont_obj, obj);
            }
        }
    }

    /* Stached objects (recursive) */
    array = json_object_get(json, "stache");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            OBJ_DATA *stached_obj = json_persist_json_to_object(elem);
            if (stached_obj) {
                stached_obj->next_content = NULL;
                stached_obj->stached = true;
                list_appendlink(obj->lstache, stached_obj);
            }
        }
    }

    /* Add object to loaded_objects tracking list.
     * The object was just created by create_object_noid with add_to_loaded_objs=false,
     * so it is guaranteed to not be in the list yet (no need for list_haslink scan). */
    list_appendlink(loaded_objects, obj);
    loaded_obj_hash_add(obj);
    obj->pIndexData->count++;

    /* Assign a unique object ID if not already set (loaded from JSON) */
    get_obj_id(obj);

    /* Apply object fixes */
    obj->times_allowed_fixed = obj->pIndexData->times_allowed_fixed;
    fix_object(obj);

    return obj;
}

/***************************************************************************
 * Object File Operations                                                  *
 ***************************************************************************/

bool json_persist_save_object(OBJ_DATA *obj)
{
    char path[256];
    json_t *json;

    if (!obj || !obj->id[0]) return false;

    json_persist_object_path(obj->id[0], obj->id[1], path, sizeof(path));

    json = json_persist_object_to_json(obj);
    if (!json) {
        char msg[MSL];
        snprintf(msg, sizeof(msg),
                 "json_persist_save_object: failed to serialize object %lu_%lu",
                 obj->id[0], obj->id[1]);
        emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                "persist_save_object_serialize_failed", path);
        return false;
    }

    if (!json_file_save(json, path, "json_persist_save_object", JSON_INDENT(2))) {
        return false;
    }

    return true;
}

OBJ_DATA *json_persist_load_object(unsigned long id0, unsigned long id1)
{
    char path[256];
    json_t *json;
    json_error_t error;
    OBJ_DATA *obj;

    json_persist_object_path(id0, id1, path, sizeof(path));

    json = json_load_file(path, 0, &error);
    if (!json) {
        /* File doesn't exist or is invalid - not necessarily an error */
        return NULL;
    }

    obj = json_persist_json_to_object(json);
    json_decref(json);

    return obj;
}

/***************************************************************************
 * Mobile Serialization                                                    *
 ***************************************************************************/

json_t *json_persist_mobile_to_json(CHAR_DATA *ch)
{
    json_t *json, *array;
    AFFECT_DATA *paf;
    TOKEN_DATA *token;
    OBJ_DATA *obj;
    ITERATOR it;
    int i;

    if (!ch || !ch->pIndexData) return NULL;
    if (!IS_NPC(ch)) return NULL;  /* Only mobiles, not players */

    json = json_object();

    /* Metadata */
    json_object_set_new(json, "json_version", json_integer(JSON_PERSIST_VERSION_MOBILE));

    /* Identification - use widevnum format for area-scoped persistence */
    json_object_set_new(json, "vnum", json_string(widevnum_string_mobile(ch->pIndexData, NULL)));
    json_object_set_new(json, "id0", json_integer(ch->id[0]));
    json_object_set_new(json, "id1", json_integer(ch->id[1]));
    json_object_set_new(json, "persist", json_boolean(ch->persist));
    json_object_set_new(json, "version", json_integer(ch->version));

    /* Death state */
    if (ch->dead) {
        json_object_set_new(json, "dead", json_true());
        json_object_set_new(json, "death_time_left", json_integer(ch->time_left_death));
    }

    /* Recall/Repop location */
    if (location_isset(&ch->recall)) {
        json_t *recall_loc = json_persist_location_to_json(&ch->recall);
        if (recall_loc) {
            json_object_set_new(json, "recall", recall_loc);
        }
    }

    /* Basic info */
    json_object_set_new(json, "name", json_string_safe(ch->name));
    json_object_set_new(json, "owner", json_string_safe(ch->owner));
    json_object_set_new(json, "short_descr", json_string_safe(ch->short_descr));
    json_object_set_new(json, "long_descr", json_string_safe(ch->long_descr));
    json_object_set_new(json, "description", json_string_safe(ch->description));

    /* Race and basic stats */
    if (ch->race) {
        json_object_set_new(json, "race", json_string(ch->race->name));
    }
    json_object_set_new(json, "sex", json_integer(ch->sex));
    json_object_set_new(json, "level", json_integer(ch->level));
    json_object_set_new(json, "tot_level", json_integer(ch->tot_level));

    /* Current location - use widevnum format for room references */
    if (ch->in_room) {
        json_t *loc = json_object();
        if (ch->in_room->wilds) {
            json_object_set_new(loc, "type", json_string("wilds"));
            json_object_set_new(loc, "wuid", json_integer(ch->in_room->wilds->uid));
            json_object_set_new(loc, "x", json_integer(ch->in_room->x));
            json_object_set_new(loc, "y", json_integer(ch->in_room->y));
        } else if (ch->in_room->source) {
            json_object_set_new(loc, "type", json_string("clone"));
            json_object_set_new(loc, "vnum", json_string(widevnum_string_room(ch->in_room->source, NULL)));
            json_object_set_new(loc, "id0", json_integer(ch->in_room->id[0]));
            json_object_set_new(loc, "id1", json_integer(ch->in_room->id[1]));
        } else {
            json_object_set_new(loc, "type", json_string("static"));
            json_object_set_new(loc, "vnum", json_string(widevnum_string_room(ch->in_room, NULL)));
        }
        json_object_set_new(json, "location", loc);
    }

    /* Toxins (for Sith/Naga races) */
    if (race_has_trait(ch->race, "toxin_system")) {
        array = json_array();
        for (i = 0; i < MAX_TOXIN; i++) {
            json_t *tox = json_object();
            json_object_set_new(tox, "name", json_string(toxin_table[i].name));
            json_object_set_new(tox, "value", json_integer(ch->toxin[i]));
            json_array_append_new(array, tox);
        }
        json_object_set_new(json, "toxins", array);
    }

    /* Health/Mana/Move */
    json_object_set_new(json, "hit", json_integer(ch->hit));
    json_object_set_new(json, "max_hit", json_integer(ch->max_hit));
    json_object_set_new(json, "mana", json_integer(ch->mana));
    json_object_set_new(json, "max_mana", json_integer(ch->max_mana));
    json_object_set_new(json, "move", json_integer(ch->move));
    json_object_set_new(json, "max_move", json_integer(ch->max_move));
    json_object_set_new(json, "manastore", json_integer(ch->manastore));

    /* Currency and points */
    json_object_set_new(json, "gold", json_integer(UMAX(0, ch->gold)));
    json_object_set_new(json, "silver", json_integer(UMAX(0, ch->silver)));
    json_object_set_new(json, "pneuma", json_integer(ch->pneuma));
    json_object_set_new(json, "home", json_integer(ch->home));
    json_object_set_new(json, "questpoints", json_integer(ch->questpoints));
    json_object_set_new(json, "deitypoints", json_integer(ch->deitypoints));
    json_object_set_new(json, "exp", json_integer(ch->exp));

    /* Flags */
    json_object_set_new(json, "act0", json_integer(ch->act[0]));
    json_object_set_new(json, "act1", json_integer(ch->act[1]));
    json_object_set_new(json, "affected_by0", json_integer(ch->affected_by[0]));
    json_object_set_new(json, "affected_by1", json_integer(ch->affected_by[1]));
    json_object_set_new(json, "off_flags", json_integer(ch->off_flags));
    json_object_set_new(json, "imm_flags", json_integer(ch->imm_flags));
    json_object_set_new(json, "imm_flags_perm", json_integer(ch->imm_flags_perm));
    json_object_set_new(json, "res_flags", json_integer(ch->res_flags));
    json_object_set_new(json, "res_flags_perm", json_integer(ch->res_flags_perm));
    json_object_set_new(json, "vuln_flags", json_integer(ch->vuln_flags));
    json_object_set_new(json, "vuln_flags_perm", json_integer(ch->vuln_flags_perm));

    /* Positions */
    json_object_set_new(json, "start_pos", json_integer(ch->start_pos));
    json_object_set_new(json, "default_pos", json_integer(ch->default_pos));
    json_object_set_new(json, "position", json_integer(
        ch->position == POS_FIGHTING ? POS_STANDING : ch->position));

    /* Physical attributes */
    json_object_set_new(json, "parts", json_integer(ch->parts));
    json_object_set_new(json, "size", json_integer(ch->size));
    json_object_set_new(json, "material", json_string(ch->material[0] ? ch->material : "unknown"));
    if (ch->corpse_type) {
        json_object_set_new(json, "corpse_type", json_integer(ch->corpse_type));
    }
    if (ch->corpse_load.vnum) {
        json_object_set_new(json, "corpse_vnum", json_integer(ch->corpse_load.vnum));
    }

    /* Communication and misc */
    json_object_set_new(json, "comm", json_integer(ch->comm));
    json_object_set_new(json, "practice", json_integer(UMAX(0, ch->practice)));
    json_object_set_new(json, "train", json_integer(UMAX(0, ch->train)));
    json_object_set_new(json, "saving_throw", json_integer(ch->saving_throw));
    json_object_set_new(json, "alignment", json_integer(ch->alignment));
    json_object_set_new(json, "hitroll", json_integer(ch->hitroll));
    json_object_set_new(json, "damroll", json_integer(ch->damroll));
    json_object_set_new(json, "wimpy", json_integer(UMAX(0, ch->wimpy)));

    /* Armor class */
    array = json_array();
    for (i = 0; i < 4; i++) {
        json_array_append_new(array, json_integer(ch->armour[i]));
    }
    json_object_set_new(json, "armour", array);

    /* Stats */
    array = json_array();
    for (i = 0; i < MAX_STATS; i++) {
        json_array_append_new(array, json_integer(ch->perm_stat[i]));
    }
    json_object_set_new(json, "perm_stat", array);

    array = json_array();
    for (i = 0; i < MAX_STATS; i++) {
        json_array_append_new(array, json_integer(ch->mod_stat[i]));
    }
    json_object_set_new(json, "mod_stat", array);

    json_object_set_new(json, "lostparts", json_integer(ch->lostparts));

    /* Affects */
    if (ch->affected) {
        array = json_array();
        for (paf = ch->affected; paf; paf = paf->next) {
            if (!paf->custom_name && (paf->type < 0 || paf->type >= MAX_SKILL))
                continue;

            json_t *aff = json_object();
            if (paf->custom_name) {
                json_object_set_new(aff, "custom_name", json_string(paf->custom_name));
            } else {
                json_object_set_new(aff, "skill_name", json_string(skill_table[paf->type].name));
            }
            json_object_set_new(aff, "group", json_string(flag_string(affgroup_mobile_flags, paf->group)));
            json_object_set_new(aff, "where", json_integer(paf->where));
            json_object_set_new(aff, "level", json_integer(paf->level));
            json_object_set_new(aff, "duration", json_integer(paf->duration));
            json_object_set_new(aff, "modifier", json_integer(paf->modifier));
            json_object_set_new(aff, "location", json_integer(paf->location));
            json_object_set_new(aff, "bitvector", json_integer(paf->bitvector));
            json_object_set_new(aff, "bitvector2", json_integer(paf->bitvector2));
            json_object_set_new(aff, "slot", json_integer(paf->slot));
            json_array_append_new(array, aff);
        }
        if (json_array_size(array) > 0) {
            json_object_set_new(json, "affects", array);
        } else {
            json_decref(array);
        }
    }

    /* Shop data - store as reference for later loading */
    if (ch->shop) {
        json_object_set_new(json, "has_shop", json_true());
        /* Shop data is tied to the mob index, so we just need to flag it */
    }

    /* Crew data - store as reference */
    if (ch->crew) {
        json_object_set_new(json, "has_crew", json_true());
        /* Crew data is tied to the mob index */
    }

    /* Script variables */
    if (ch->progs) {
        json_t *vars = json_persist_scriptdata_to_json(ch->progs);
        if (vars) {
            json_object_set_new(json, "variables", vars);
        }
    }

    /* Tokens */
    if (ch->tokens) {
        array = json_array();
        for (token = ch->tokens; token; token = token->next) {
            json_t *tok = json_persist_token_to_json(token);
            if (tok) {
                json_array_append_new(array, tok);
            }
        }
        if (json_array_size(array) > 0) {
            json_object_set_new(json, "tokens", array);
        } else {
            json_decref(array);
        }
    }

    /* Carried objects */
    if (ch->lcarrying && list_size(ch->lcarrying) > 0) {
        array = json_array();
        iterator_start(&it, ch->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            json_t *obj_json = json_persist_object_to_json(obj);
            if (obj_json) {
                json_array_append_new(array, obj_json);
            }
        }
        iterator_stop(&it);
        if (json_array_size(array) > 0) {
            json_object_set_new(json, "carrying", array);
        } else {
            json_decref(array);
        }
    }

    /* Worn objects */
    if (ch->lworn && list_size(ch->lworn) > 0) {
        array = json_array();
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            json_t *obj_json = json_persist_object_to_json(obj);
            if (obj_json) {
                json_array_append_new(array, obj_json);
            }
        }
        iterator_stop(&it);
        if (json_array_size(array) > 0) {
            json_object_set_new(json, "worn", array);
        } else {
            json_decref(array);
        }
    }

    return json;
}

CHAR_DATA *json_persist_json_to_mobile(json_t *json)
{
    CHAR_DATA *ch;
    MOB_INDEX_DATA *pMobIndex;
    json_t *value, *array, *elem;
    WNUM wnum;
    size_t index;
    int i;

    if (!json) return NULL;

    /* Parse vnum - supports both widevnum string and legacy integer */
    value = json_object_get(json, "vnum");
    if (json_is_string(value)) {
        /* New widevnum format: "auid#vnum" */
        if (!parse_widevnum((char *)json_string_value(value), NULL, &wnum) || !wnum.pArea) {
            log_stringf("json_persist_json_to_mobile: bad widevnum '%s'", json_string_value(value));
            return NULL;
        }
        pMobIndex = get_mob_index(wnum.pArea, wnum.vnum);
    } else {
        /* Legacy integer format */
        long vnum = json_integer_value(value);
        AREA_DATA *area = NULL;
        WNUM wnum;
        if (resolve_widevnum(vnum, NULL, &wnum))
            area = wnum.pArea;
        if (!area) area = get_system_area_fallback();
        pMobIndex = get_mob_index(area, vnum);
    }
    if (!pMobIndex) {
        log_stringf("json_persist_json_to_mobile: mobile index not found");
        return NULL;
    }

    ch = create_mobile(pMobIndex, true);  /* persistLoad=true to avoid adding to global lists yet */
    if (!ch) return NULL;

    /* IDs */
    value = json_object_get(json, "id0");
    if (value) ch->id[0] = json_integer_value(value);
    value = json_object_get(json, "id1");
    if (value) ch->id[1] = json_integer_value(value);

    value = json_object_get(json, "persist");
    if (value) ch->persist = json_boolean_value(value);

    value = json_object_get(json, "version");
    if (value) ch->version = json_integer_value(value);

    /* Death state */
    value = json_object_get(json, "dead");
    if (value && json_boolean_value(value)) {
        ch->dead = true;
        value = json_object_get(json, "death_time_left");
        if (value) ch->time_left_death = json_integer_value(value);
    }

    /* Recall/Repop location */
    value = json_object_get(json, "recall");
    if (value) {
        json_persist_json_to_location(value, &ch->recall);
    }

    /* Basic info */
    value = json_object_get(json, "name");
    if (value) {
        free_string(ch->name);
        ch->name = str_dup(json_string_value(value));
    }
    value = json_object_get(json, "owner");
    if (value) {
        free_string(ch->owner);
        ch->owner = str_dup(json_string_value(value));
    }
    value = json_object_get(json, "short_descr");
    if (value) {
        free_string(ch->short_descr);
        ch->short_descr = str_dup(json_string_value(value));
    }
    value = json_object_get(json, "long_descr");
    if (value) {
        free_string(ch->long_descr);
        ch->long_descr = str_dup(json_string_value(value));
    }
    value = json_object_get(json, "description");
    if (value) {
        free_string(ch->description);
        ch->description = str_dup(json_string_value(value));
    }

    /* Race */
    value = json_object_get(json, "race");
    if (value) {
        RACE_DATA *race = race_lookup_name(json_string_value(value));
        if (race) {
            ch->race = race;
        }
    }

    /* Basic stats */
    value = json_object_get(json, "sex");
    if (value) ch->sex = json_integer_value(value);
    value = json_object_get(json, "level");
    if (value) ch->level = json_integer_value(value);
    value = json_object_get(json, "tot_level");
    if (value) ch->tot_level = json_integer_value(value);

    /* Location - stored for later resolution */
    value = json_object_get(json, "location");
    if (value && json_is_object(value)) {
        json_t *loc_type = json_object_get(value, "type");
        const char *type = json_string_value(loc_type);

        if (!str_cmp(type, "static")) {
            json_t *vnum_val = json_object_get(value, "vnum");
            if (json_is_string(vnum_val)) {
                /* New widevnum format */
                WNUM room_wnum;
                if (parse_widevnum((char *)json_string_value(vnum_val), NULL, &room_wnum) && room_wnum.pArea) {
                    ch->in_room = get_room_index(room_wnum.pArea, room_wnum.vnum);
                }
            } else {
                /* Legacy integer format */
                long room_vnum = json_integer_value(vnum_val);
                ch->in_room = get_room_index_global(room_vnum);
            }
        } else if (!str_cmp(type, "clone") || !str_cmp(type, "wilds")) {
            /* Clone/wilderness room - needs deferred resolution */
            ch->in_room = NULL;
        }
    }

    /* Toxins */
    array = json_object_get(json, "toxins");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            const char *tname = json_get_string(elem, "name", "");
            int tval = json_integer_value(json_object_get(elem, "value"));
            if (tname) {
                for (i = 0; i < MAX_TOXIN; i++) {
                    if (!str_cmp(tname, toxin_table[i].name)) {
                        ch->toxin[i] = tval;
                        break;
                    }
                }
            }
        }
    }

    /* Health/Mana/Move */
    value = json_object_get(json, "hit");
    if (value) ch->hit = json_integer_value(value);
    value = json_object_get(json, "max_hit");
    if (value) ch->max_hit = json_integer_value(value);
    value = json_object_get(json, "mana");
    if (value) ch->mana = json_integer_value(value);
    value = json_object_get(json, "max_mana");
    if (value) ch->max_mana = json_integer_value(value);
    value = json_object_get(json, "move");
    if (value) ch->move = json_integer_value(value);
    value = json_object_get(json, "max_move");
    if (value) ch->max_move = json_integer_value(value);
    value = json_object_get(json, "manastore");
    if (value) ch->manastore = json_integer_value(value);

    /* Currency and points */
    value = json_object_get(json, "gold");
    if (value) ch->gold = json_integer_value(value);
    value = json_object_get(json, "silver");
    if (value) ch->silver = json_integer_value(value);
    value = json_object_get(json, "pneuma");
    if (value) ch->pneuma = json_integer_value(value);
    value = json_object_get(json, "home");
    if (value) ch->home = json_integer_value(value);
    value = json_object_get(json, "questpoints");
    if (value) ch->questpoints = json_integer_value(value);
    value = json_object_get(json, "deitypoints");
    if (value) ch->deitypoints = json_integer_value(value);
    value = json_object_get(json, "exp");
    if (value) ch->exp = json_integer_value(value);

    /* Flags */
    value = json_object_get(json, "act0");
    if (value) ch->act[0] = json_integer_value(value);
    value = json_object_get(json, "act1");
    if (value) ch->act[1] = json_integer_value(value);
    value = json_object_get(json, "affected_by0");
    if (value) ch->affected_by[0] = json_integer_value(value);
    value = json_object_get(json, "affected_by1");
    if (value) ch->affected_by[1] = json_integer_value(value);
    value = json_object_get(json, "off_flags");
    if (value) ch->off_flags = json_integer_value(value);
    value = json_object_get(json, "imm_flags");
    if (value) ch->imm_flags = json_integer_value(value);
    value = json_object_get(json, "imm_flags_perm");
    if (value) ch->imm_flags_perm = json_integer_value(value);
    value = json_object_get(json, "res_flags");
    if (value) ch->res_flags = json_integer_value(value);
    value = json_object_get(json, "res_flags_perm");
    if (value) ch->res_flags_perm = json_integer_value(value);
    value = json_object_get(json, "vuln_flags");
    if (value) ch->vuln_flags = json_integer_value(value);
    value = json_object_get(json, "vuln_flags_perm");
    if (value) ch->vuln_flags_perm = json_integer_value(value);

    /* Positions */
    value = json_object_get(json, "start_pos");
    if (value) ch->start_pos = json_integer_value(value);
    value = json_object_get(json, "default_pos");
    if (value) ch->default_pos = json_integer_value(value);
    value = json_object_get(json, "position");
    if (value) ch->position = json_integer_value(value);

    /* Physical attributes */
    value = json_object_get(json, "parts");
    if (value) ch->parts = json_integer_value(value);
    value = json_object_get(json, "size");
    if (value) ch->size = json_integer_value(value);
    value = json_object_get(json, "material");
    if (value) {
        strncpy(ch->material, json_string_value(value), sizeof(ch->material) - 1);
        ch->material[sizeof(ch->material) - 1] = '\0';
    }
    value = json_object_get(json, "corpse_type");
    if (value) ch->corpse_type = json_integer_value(value);
    value = json_object_get(json, "corpse_vnum");
    if (value) ch->corpse_load.vnum = json_integer_value(value);

    /* Communication and misc */
    value = json_object_get(json, "comm");
    if (value) ch->comm = json_integer_value(value);
    value = json_object_get(json, "practice");
    if (value) ch->practice = json_integer_value(value);
    value = json_object_get(json, "train");
    if (value) ch->train = json_integer_value(value);
    value = json_object_get(json, "saving_throw");
    if (value) ch->saving_throw = json_integer_value(value);
    value = json_object_get(json, "alignment");
    if (value) ch->alignment = json_integer_value(value);
    value = json_object_get(json, "hitroll");
    if (value) ch->hitroll = json_integer_value(value);
    value = json_object_get(json, "damroll");
    if (value) ch->damroll = json_integer_value(value);
    value = json_object_get(json, "wimpy");
    if (value) ch->wimpy = json_integer_value(value);

    /* Armor class */
    array = json_object_get(json, "armour");
    if (array && json_is_array(array)) {
        for (i = 0; i < 4 && i < (int)json_array_size(array); i++) {
            ch->armour[i] = json_integer_value(json_array_get(array, i));
        }
    }

    /* Stats */
    array = json_object_get(json, "perm_stat");
    if (array && json_is_array(array)) {
        for (i = 0; i < MAX_STATS && i < (int)json_array_size(array); i++) {
            ch->perm_stat[i] = json_integer_value(json_array_get(array, i));
        }
    }

    array = json_object_get(json, "mod_stat");
    if (array && json_is_array(array)) {
        for (i = 0; i < MAX_STATS && i < (int)json_array_size(array); i++) {
            ch->mod_stat[i] = json_integer_value(json_array_get(array, i));
        }
    }

    value = json_object_get(json, "lostparts");
    if (value) ch->lostparts = json_integer_value(value);

    /* Affects */
    array = json_object_get(json, "affects");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            AFFECT_DATA *paf = new_affect();
            json_t *aff_val;

            aff_val = json_object_get(elem, "custom_name");
            if (aff_val) {
                paf->custom_name = create_affect_cname((char *)json_string_value(aff_val));
                paf->type = -1;  /* Custom name, no skill */
                paf->skill = NULL;
            } else {
                aff_val = json_object_get(elem, "skill_name");
                if (aff_val) {
                    paf->type = skill_lookup((char *)json_string_value(aff_val));
                    paf->skill = skill_find_uid(paf->type);
                }
            }

            aff_val = json_object_get(elem, "group");
            if (aff_val) {
                paf->group = flag_value(affgroup_mobile_flags, (char *)json_string_value(aff_val));
            }

            aff_val = json_object_get(elem, "where");
            if (aff_val) paf->where = json_integer_value(aff_val);
            aff_val = json_object_get(elem, "level");
            if (aff_val) paf->level = json_integer_value(aff_val);
            aff_val = json_object_get(elem, "duration");
            if (aff_val) paf->duration = json_integer_value(aff_val);
            aff_val = json_object_get(elem, "modifier");
            if (aff_val) paf->modifier = json_integer_value(aff_val);
            aff_val = json_object_get(elem, "location");
            if (aff_val) paf->location = json_integer_value(aff_val);
            aff_val = json_object_get(elem, "bitvector");
            if (aff_val) paf->bitvector = json_integer_value(aff_val);
            aff_val = json_object_get(elem, "bitvector2");
            if (aff_val) paf->bitvector2 = json_integer_value(aff_val);
            aff_val = json_object_get(elem, "slot");
            if (aff_val) paf->slot = json_integer_value(aff_val);

            paf->next = ch->affected;
            ch->affected = paf;
        }
    }

    /* Shop and Crew - tied to mob index, just needs flag awareness */
    value = json_object_get(json, "has_shop");
    if (value && json_boolean_value(value) && pMobIndex->pShop) {
        ch->shop = pMobIndex->pShop;
    }
    value = json_object_get(json, "has_crew");
    if (value && json_boolean_value(value) && pMobIndex->pCrew) {
        /* Crew linking would be handled here if needed */
    }

    /* Script variables */
    value = json_object_get(json, "variables");
    if (value) {
        json_persist_json_to_scriptdata(value, &ch->progs);
    }

    /* Tokens */
    array = json_object_get(json, "tokens");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            TOKEN_DATA *token = json_persist_json_to_token(elem);
            if (token) {
                token_to_char(token, ch);
            }
        }
    }

    /* Carried objects */
    array = json_object_get(json, "carrying");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            OBJ_DATA *obj = json_persist_json_to_object(elem);
            if (obj) {
                obj_to_char(obj, ch);
            }
        }
    }

    /* Worn objects */
    array = json_object_get(json, "worn");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            OBJ_DATA *obj = json_persist_json_to_object(elem);
            if (obj) {
                /* Load with wear location preserved */
                int wear_loc = obj->wear_loc;
                obj_to_char(obj, ch);
                if (wear_loc != WEAR_NONE) {
                    equip_char(ch, obj, wear_loc);
                }
            }
        }
    }

    /* Add mobile to loaded_chars and assign ID if needed */
    if (!list_haslink(loaded_chars, ch)) {
        list_appendlink(loaded_chars, ch);
    }

    /* Assign a unique mobile ID if not already set (loaded from JSON) */
    get_mob_id(ch);

    return ch;
}

bool json_persist_save_mobile(CHAR_DATA *ch)
{
    char path[256];
    json_t *json;

    if (!ch || !ch->id[0]) return false;
    if (!IS_NPC(ch)) return false;  /* Only mobiles, not players */

    json_persist_mobile_path(ch->id[0], ch->id[1], path, sizeof(path));

    json = json_persist_mobile_to_json(ch);
    if (!json) {
        char msg[MSL];
        snprintf(msg, sizeof(msg),
                 "json_persist_save_mobile: failed to serialize mobile %lu_%lu",
                 ch->id[0], ch->id[1]);
        emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                "persist_save_mobile_serialize_failed", path);
        return false;
    }

    if (!json_file_save(json, path, "json_persist_save_mobile", JSON_INDENT(2))) {
        return false;
    }

    return true;
}

CHAR_DATA *json_persist_load_mobile(unsigned long id0, unsigned long id1)
{
    char path[256];
    json_t *json;
    json_error_t error;
    CHAR_DATA *ch;

    json_persist_mobile_path(id0, id1, path, sizeof(path));

    json = json_load_file(path, 0, &error);
    if (!json) {
        /* File doesn't exist or is invalid - not necessarily an error */
        return NULL;
    }

    ch = json_persist_json_to_mobile(json);
    json_decref(json);

    return ch;
}

/***************************************************************************
 * Room Serialization                                                      *
 ***************************************************************************/

extern const struct flag_type room_flags[];

/* Helper to serialize room environment (for clone rooms) */
static json_t *room_environ_to_json(ROOM_INDEX_DATA *room)
{
    json_t *env;
    ROOM_INDEX_DATA *env_room;

    if (!room || !room_is_clone(room)) return NULL;

    env = json_object();

    if (room->environ_type == ENVIRON_ROOM && room->environ.room) {
        env_room = room->environ.room;
        json_object_set_new(env, "type", json_string("room"));
        if (env_room->wilds) {
            json_object_set_new(env, "wuid", json_integer(env_room->wilds->uid));
            json_object_set_new(env, "x", json_integer(env_room->x));
            json_object_set_new(env, "y", json_integer(env_room->y));
            json_object_set_new(env, "z", json_integer(env_room->z));
        } else if (env_room->source) {
            json_object_set_new(env, "source_vnum", json_string(widevnum_string_room(env_room->source, NULL)));
            json_object_set_new(env, "id0", json_integer(env_room->id[0]));
            json_object_set_new(env, "id1", json_integer(env_room->id[1]));
        } else {
            json_object_set_new(env, "vnum", json_string(widevnum_string_room(env_room, NULL)));
        }
    } else if (room->environ_type == ENVIRON_MOBILE && room->environ.mob) {
        json_object_set_new(env, "type", json_string("mobile"));
        json_object_set_new(env, "id0", json_integer(room->environ.mob->id[0]));
        json_object_set_new(env, "id1", json_integer(room->environ.mob->id[1]));
    } else if (room->environ_type == ENVIRON_OBJECT && room->environ.obj) {
        json_object_set_new(env, "type", json_string("object"));
        json_object_set_new(env, "id0", json_integer(room->environ.obj->id[0]));
        json_object_set_new(env, "id1", json_integer(room->environ.obj->id[1]));
    } else if (room->environ_type == ENVIRON_TOKEN && room->environ.token) {
        json_object_set_new(env, "type", json_string("token"));
        json_object_set_new(env, "id0", json_integer(room->environ.token->id[0]));
        json_object_set_new(env, "id1", json_integer(room->environ.token->id[1]));
    } else if (room->environ_type == -ENVIRON_ROOM && room->environ.clone.source) {
        json_object_set_new(env, "type", json_string("clone_room_ref"));
        json_object_set_new(env, "source_vnum", json_string(widevnum_string_room(room->environ.clone.source, NULL)));
        json_object_set_new(env, "id0", json_integer(room->environ.clone.id[0]));
        json_object_set_new(env, "id1", json_integer(room->environ.clone.id[1]));
    } else if (room->environ_type == -ENVIRON_MOBILE) {
        json_object_set_new(env, "type", json_string("mobile_ref"));
        json_object_set_new(env, "id0", json_integer(room->environ.clone.id[0]));
        json_object_set_new(env, "id1", json_integer(room->environ.clone.id[1]));
    } else if (room->environ_type == -ENVIRON_OBJECT) {
        json_object_set_new(env, "type", json_string("object_ref"));
        json_object_set_new(env, "id0", json_integer(room->environ.clone.id[0]));
        json_object_set_new(env, "id1", json_integer(room->environ.clone.id[1]));
    } else if (room->environ_type == -ENVIRON_TOKEN) {
        json_object_set_new(env, "type", json_string("token_ref"));
        json_object_set_new(env, "id0", json_integer(room->environ.clone.id[0]));
        json_object_set_new(env, "id1", json_integer(room->environ.clone.id[1]));
    } else {
        json_decref(env);
        return NULL;
    }

    return env;
}

json_t *json_persist_room_to_json(ROOM_INDEX_DATA *room)
{
    json_t *json, *array;
    CHAR_DATA *ch;
    OBJ_DATA *obj;
    TOKEN_DATA *token;
    int i;

    if (!room) return NULL;

    json = json_object();

    /* Metadata */
    json_object_set_new(json, "json_version", json_integer(JSON_PERSIST_VERSION_ROOM));

    /* Room type and identification - use widevnum format */
    if (room->source) {
        /* Clone room */
        json_object_set_new(json, "room_type", json_string("clone"));
        json_object_set_new(json, "source_vnum", json_string(widevnum_string_room(room->source, NULL)));
        json_object_set_new(json, "id0", json_integer(room->id[0]));
        json_object_set_new(json, "id1", json_integer(room->id[1]));

        /* Environment for clone rooms */
        json_t *env = room_environ_to_json(room);
        if (env) {
            json_object_set_new(json, "environment", env);
        }
    } else if (room->wilds) {
        /* Wilderness room */
        json_object_set_new(json, "room_type", json_string("wilds"));
        json_object_set_new(json, "wilds_uid", json_integer(room->wilds->uid));
    } else {
        /* Static room */
        json_object_set_new(json, "room_type", json_string("static"));
        json_object_set_new(json, "vnum", json_string(widevnum_string_room(room, NULL)));
    }

    /* Coordinates */
    json_object_set_new(json, "x", json_integer(room->x));
    json_object_set_new(json, "y", json_integer(room->y));
    json_object_set_new(json, "z", json_integer(room->z));

    /* View wilds reference */
    if (room->viewwilds) {
        json_object_set_new(json, "viewwilds_uid", json_integer(room->viewwilds->uid));
    }

    /* Basic info */
    json_object_set_new(json, "name", json_string_safe(room->name));
    json_object_set_new(json, "description", json_string_safe(room->description));
    if (room->owner && room->owner[0]) {
        json_object_set_new(json, "owner", json_string(room->owner));
    }

    json_object_set_new(json, "persist", json_boolean(room->persist));
    json_object_set_new(json, "locale", json_integer(room->locale));

    /* Flags */
    json_object_set_new(json, "room_flags", json_integer(room->room_flag[0]));
    json_object_set_new(json, "room_flags2", json_integer(room->room_flag[1]));
    json_object_set_new(json, "sector_type", json_integer(room_sector_type(room)));

    /* Regeneration rates */
    if (room->heal_rate != 100) {
        json_object_set_new(json, "heal_rate", json_integer(room->heal_rate));
    }
    if (room->mana_rate != 100) {
        json_object_set_new(json, "mana_rate", json_integer(room->mana_rate));
    }
    if (room->move_rate != 100) {
        json_object_set_new(json, "move_rate", json_integer(room->move_rate));
    }

    /* Room recall */
    if (location_isset(&room->recall)) {
        json_t *recall_loc = json_persist_location_to_json(&room->recall);
        if (recall_loc) {
            json_object_set_new(json, "recall", recall_loc);
        }
    }

    /* Exits */
    array = json_array();
    for (i = 0; i < MAX_DIR; i++) {
        if (room->exit[i]) {
            json_t *exit_json = json_persist_exit_to_json(room->exit[i], i);
            if (exit_json) {
                json_array_append_new(array, exit_json);
            }
        }
    }
    if (json_array_size(array) > 0) {
        json_object_set_new(json, "exits", array);
    } else {
        json_decref(array);
    }

    /* Script variables */
    if (room->progs) {
        json_t *vars = json_persist_scriptdata_to_json(room->progs);
        if (vars) {
            json_object_set_new(json, "variables", vars);
        }
    }

    /* Tokens */
    if (room->tokens) {
        array = json_array();
        for (token = room->tokens; token; token = token->next) {
            json_t *tok = json_persist_token_to_json(token);
            if (tok) {
                json_array_append_new(array, tok);
            }
        }
        if (json_array_size(array) > 0) {
            json_object_set_new(json, "tokens", array);
        } else {
            json_decref(array);
        }
    }

    /* Contents (objects) */
    if (room->contents) {
        array = json_array();
        for (obj = room->contents; obj; obj = obj->next_content) {
            json_t *obj_json = json_persist_object_to_json(obj);
            if (obj_json) {
                json_array_append_new(array, obj_json);
            }
        }
        if (json_array_size(array) > 0) {
            json_object_set_new(json, "objects", array);
        } else {
            json_decref(array);
        }
    }

    /* People (NPCs only) */
    if (room->people) {
        array = json_array();
        for (ch = room->people; ch; ch = ch->next_in_room) {
            if (IS_NPC(ch)) {
                json_t *mob_json = json_persist_mobile_to_json(ch);
                if (mob_json) {
                    json_array_append_new(array, mob_json);
                }
            }
        }
        if (json_array_size(array) > 0) {
            json_object_set_new(json, "mobiles", array);
        } else {
            json_decref(array);
        }
    }

    return json;
}

ROOM_INDEX_DATA *json_persist_json_to_room(json_t *json)
{
    ROOM_INDEX_DATA *room = NULL;
    json_t *value, *array, *elem;
    const char *room_type;
    size_t index;

    if (!json) return NULL;

    value = json_object_get(json, "room_type");
    if (!value) return NULL;
    room_type = json_string_value(value);

    /* Create or find room based on type */
    if (!str_cmp(room_type, "static")) {
        bool persisted_snapshot = false;

        value = json_object_get(json, "persist");
        if (value)
            persisted_snapshot = json_boolean_value(value);

        json_t *vnum_val = json_object_get(json, "vnum");
        if (json_is_string(vnum_val)) {
            /* New widevnum format */
            WNUM room_wnum;
            if (parse_widevnum((char *)json_string_value(vnum_val), NULL, &room_wnum) && room_wnum.pArea) {
                room = get_room_index(room_wnum.pArea, room_wnum.vnum);
            }
        } else {
            /* Legacy integer format */
            long vnum = json_integer_value(vnum_val);
            room = get_room_index_global(vnum);
        }
        if (!room) {
            log_string("json_persist_json_to_room: bad vnum");
            return NULL;
        }

        /* Static room overlays are only valid for rooms currently marked persistent.
         * This prevents stale persisted snapshots from clobbering normal area rooms. */
        if (!room->persist) {
            if (persisted_snapshot) {
                log_stringf("json_persist_json_to_room: skipping stale persistent snapshot for non-persistent room %s",
                            widevnum_string_room(room, NULL));
            }
            return room;
        }

        /* Static rooms exist - apply persistent changes */
    } else if (!str_cmp(room_type, "clone")) {
        json_t *src_vnum_val = json_object_get(json, "source_vnum");
        ROOM_INDEX_DATA *source = NULL;
        if (json_is_string(src_vnum_val)) {
            /* New widevnum format */
            WNUM room_wnum;
            if (parse_widevnum((char *)json_string_value(src_vnum_val), NULL, &room_wnum) && room_wnum.pArea) {
                source = get_room_index(room_wnum.pArea, room_wnum.vnum);
            }
        } else {
            /* Legacy integer format */
            long source_vnum = json_integer_value(src_vnum_val);
            source = get_room_index_global(source_vnum);
        }
        if (!source) {
            log_string("json_persist_json_to_room: bad source vnum");
            return NULL;
        }
        /* Create clone room */
        room = create_virtual_room(source, false, false);
        if (!room) return NULL;

        value = json_object_get(json, "id0");
        if (value) room->id[0] = json_integer_value(value);
        value = json_object_get(json, "id1");
        if (value) room->id[1] = json_integer_value(value);

        /* Environment - deferred resolution may be needed */
        /* TODO: Handle environment restoration */
    } else if (!str_cmp(room_type, "wilds")) {
        /* Wilderness rooms are handled differently - they're generated on demand */
        /* For now, return NULL - wilderness persistence handled elsewhere */
        log_string("json_persist_json_to_room: wilds rooms not yet supported");
        return NULL;
    } else {
        log_stringf("json_persist_json_to_room: unknown room type '%s'", room_type);
        return NULL;
    }

    /* Coordinates */
    value = json_object_get(json, "x");
    if (value) room->x = json_integer_value(value);
    value = json_object_get(json, "y");
    if (value) room->y = json_integer_value(value);
    value = json_object_get(json, "z");
    if (value) room->z = json_integer_value(value);

    /* View wilds */
    value = json_object_get(json, "viewwilds_uid");
    if (value) {
        long wuid = json_integer_value(value);
        room->viewwilds = get_wilds_from_uid(NULL, wuid);
    }

    /* Basic info */
    value = json_object_get(json, "name");
    if (value) {
        free_string(room->name);
        room->name = str_dup(json_string_value(value));
    }
    value = json_object_get(json, "description");
    if (value) {
        free_string(room->description);
        room->description = str_dup(json_string_value(value));
    }
    value = json_object_get(json, "owner");
    if (value) {
        free_string(room->owner);
        room->owner = str_dup(json_string_value(value));
    }

    value = json_object_get(json, "persist");
    if (value) room->persist = json_boolean_value(value);
    value = json_object_get(json, "locale");
    if (value) room->locale = json_integer_value(value);

    /* Flags */
    value = json_object_get(json, "room_flags");
    if (value) room->room_flag[0] = json_integer_value(value);
    value = json_object_get(json, "room_flags2");
    if (value) room->room_flag[1] = json_integer_value(value);
    value = json_object_get(json, "sector_type");
    if (value) room_set_sector_type(room, json_integer_value(value));

    /* Regeneration rates */
    value = json_object_get(json, "heal_rate");
    if (value) room->heal_rate = json_integer_value(value);
    value = json_object_get(json, "mana_rate");
    if (value) room->mana_rate = json_integer_value(value);
    value = json_object_get(json, "move_rate");
    if (value) room->move_rate = json_integer_value(value);

    /* Room recall */
    value = json_object_get(json, "recall");
    if (value) {
        json_persist_json_to_location(value, &room->recall);
    }

    /* Exits */
    array = json_object_get(json, "exits");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            EXIT_DATA *pexit = json_persist_json_to_exit(elem, room);
            if (pexit) {
                int dir = pexit->orig_door;
                if (dir >= 0 && dir < MAX_DIR) {
                    if (room->exit[dir]) {
                        free_exit(room->exit[dir]);
                    }
                    room->exit[dir] = pexit;
                }
            }
        }
    }

    /* Script variables */
    value = json_object_get(json, "variables");
    if (value) {
        json_persist_json_to_scriptdata(value, &room->progs);
    }

    /* Tokens */
    array = json_object_get(json, "tokens");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            TOKEN_DATA *token = json_persist_json_to_token(elem);
            if (token) {
                token_to_room(token, room);
            }
        }
    }

    /* Objects */
    array = json_object_get(json, "objects");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            OBJ_DATA *obj = json_persist_json_to_object(elem);
            if (obj) {
                obj_to_room(obj, room);
            }
        }
    }

    /* Mobiles */
    array = json_object_get(json, "mobiles");
    if (array && json_is_array(array)) {
        json_array_foreach(array, index, elem) {
            CHAR_DATA *ch = json_persist_json_to_mobile(elem);
            if (ch) {
                char_to_room(ch, room);
            }
        }
    }

    /* Assign a unique vroom ID if this is a virtual room without an ID */
    get_vroom_id(room);

    return room;
}

bool json_persist_save_room(ROOM_INDEX_DATA *room)
{
    char path[256];
    json_t *json;

    if (!room) return false;

    json_persist_room_path(room, path, sizeof(path));

    json = json_persist_room_to_json(room);
    if (!json) {
        char room_id[256];
        char msg[MSL];
        json_persist_room_id(room, room_id, sizeof(room_id));
        snprintf(msg, sizeof(msg), "json_persist_save_room: failed to serialize room %s", room_id);
        emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                "persist_save_room_serialize_failed", path);
        return false;
    }

    if (!json_file_save(json, path, "json_persist_save_room", JSON_INDENT(2))) {
        return false;
    }

    return true;
}

ROOM_INDEX_DATA *json_persist_load_room(const char *room_id)
{
    char rooms_dir_buf[MAX_INPUT_LENGTH];
    const char *rooms_dir = persist_json_rooms_path(rooms_dir_buf, sizeof(rooms_dir_buf));
    char path[512];
    json_t *json;
    json_error_t error;
    ROOM_INDEX_DATA *room;

    if (!room_id || !room_id[0]) return NULL;

    snprintf(path, sizeof(path), "%s%s.json", rooms_dir, room_id);

    json = json_load_file(path, 0, &error);
    if (!json) {
        /* File doesn't exist or is invalid - not necessarily an error */
        return NULL;
    }

    room = json_persist_json_to_room(json);
    json_decref(json);

    return room;
}

/***************************************************************************
 * Exit Serialization                                                      *
 ***************************************************************************/

extern const struct flag_type exit_flags[];

json_t *json_persist_exit_to_json(EXIT_DATA *pexit, int dir)
{
    json_t *json;
    LOCATION loc;

    if (!pexit) return NULL;

    /* Skip wilderness exits - they are dynamically generated */
    if (IS_SET(pexit->exit_info, EX_VLINK))
        return NULL;

    json = json_object();

    /* Direction */
    json_object_set_new(json, "direction", json_string(dir_name[dir]));
    json_object_set_new(json, "orig_door", json_integer(pexit->orig_door));

    /* Destination */
    if (pexit->u1.to_room) {
        location_from_room(&loc, pexit->u1.to_room);
        if (location_isset(&loc)) {
            json_t *dest = json_persist_location_to_json(&loc);
            if (dest) {
                json_object_set_new(json, "destination", dest);
            }
        }
    } else if (pexit->wilds.wilds_uid > 0) {
        json_t *dest = json_object();
        json_object_set_new(dest, "wuid", json_integer(pexit->wilds.wilds_uid));
        json_object_set_new(dest, "x", json_integer(pexit->wilds.x));
        json_object_set_new(dest, "y", json_integer(pexit->wilds.y));
        json_object_set_new(json, "wilds_destination", dest);
    }

    /* Descriptions */
    if (pexit->keyword && pexit->keyword[0]) {
        json_object_set_new(json, "keyword", json_string(pexit->keyword));
    }
    if (pexit->short_desc && pexit->short_desc[0]) {
        json_object_set_new(json, "short_desc", json_string(pexit->short_desc));
    }
    if (pexit->long_desc && pexit->long_desc[0]) {
        json_object_set_new(json, "long_desc", json_string(pexit->long_desc));
    }

    /* Flags */
    json_object_set_new(json, "flags", json_string(flag_string(exit_flags, pexit->exit_info)));
    json_object_set_new(json, "reset_flags", json_string(flag_string(exit_flags, pexit->rs_flags)));

    /* Door/Lock data */
    json_t *door = json_object();
    json_object_set_new(door, "strength", json_integer(pexit->door.strength));

    /* Current lock */
    json_t *lock_json = json_object();
    if (pexit->door.lock.key_load.auid > 0)
        json_object_set_new(lock_json, "key_auid", json_integer(pexit->door.lock.key_load.auid));
    if (pexit->door.lock.key_load.vnum > 0)
        json_object_set_new(lock_json, "key_vnum", json_integer(pexit->door.lock.key_load.vnum));
    json_object_set_new(lock_json, "flags", json_string(flag_string(lock_flags, pexit->door.lock.flags)));
    json_object_set_new(lock_json, "pick_chance", json_integer(pexit->door.lock.pick_chance));
    json_object_set_new(door, "lock", lock_json);

    /* Reset lock */
    json_t *rs_lock = json_object();
    if (pexit->door.rs_lock.key_load.auid > 0)
        json_object_set_new(rs_lock, "key_auid", json_integer(pexit->door.rs_lock.key_load.auid));
    if (pexit->door.rs_lock.key_load.vnum > 0)
        json_object_set_new(rs_lock, "key_vnum", json_integer(pexit->door.rs_lock.key_load.vnum));
    json_object_set_new(rs_lock, "flags", json_string(flag_string(lock_flags, pexit->door.rs_lock.flags)));
    json_object_set_new(rs_lock, "pick_chance", json_integer(pexit->door.rs_lock.pick_chance));
    json_object_set_new(door, "reset_lock", rs_lock);

    if (pexit->door.material && pexit->door.material[0]) {
        json_object_set_new(door, "material", json_string(pexit->door.material));
    }

    json_object_set_new(json, "door", door);

    return json;
}

EXIT_DATA *json_persist_json_to_exit(json_t *json, ROOM_INDEX_DATA *room)
{
    EXIT_DATA *pexit;
    json_t *value, *door_json, *lock_json;
    int dir;

    if (!json || !room) return NULL;

    pexit = new_exit();
    if (!pexit) return NULL;

    /* Direction */
    value = json_object_get(json, "orig_door");
    if (value) {
        dir = json_integer_value(value);
        pexit->orig_door = dir;
    } else {
        free_exit(pexit);
        return NULL;
    }

    /* Destination - deferred resolution may be needed */
    value = json_object_get(json, "destination");
    if (value) {
        LOCATION loc;
        if (json_persist_json_to_location(value, &loc)) {
            /* Try to resolve the room */
            ROOM_INDEX_DATA *to_room = location_to_room(&loc);
            if (to_room) {
                pexit->u1.to_room = to_room;
            }
            /* Store location for deferred resolution */
            pexit->wilds.wilds_uid = loc.wuid;
        }
    }

    value = json_object_get(json, "wilds_destination");
    if (value) {
        pexit->wilds.wilds_uid = json_integer_value(json_object_get(value, "wuid"));
        pexit->wilds.x = json_integer_value(json_object_get(value, "x"));
        pexit->wilds.y = json_integer_value(json_object_get(value, "y"));
    }

    /* Descriptions */
    value = json_object_get(json, "keyword");
    if (value) {
        free_string(pexit->keyword);
        pexit->keyword = str_dup(json_string_value(value));
    }
    value = json_object_get(json, "short_desc");
    if (value) {
        free_string(pexit->short_desc);
        pexit->short_desc = str_dup(json_string_value(value));
    }
    value = json_object_get(json, "long_desc");
    if (value) {
        free_string(pexit->long_desc);
        pexit->long_desc = str_dup(json_string_value(value));
    }

    /* Flags */
    value = json_object_get(json, "flags");
    if (value) {
        pexit->exit_info = flag_value(exit_flags, (char *)json_string_value(value));
    }
    value = json_object_get(json, "reset_flags");
    if (value) {
        pexit->rs_flags = flag_value(exit_flags, (char *)json_string_value(value));
    }

    /* Door data */
    door_json = json_object_get(json, "door");
    if (door_json) {
        value = json_object_get(door_json, "strength");
        if (value) pexit->door.strength = json_integer_value(value);

        /* Current lock */
        lock_json = json_object_get(door_json, "lock");
        if (lock_json) {
            value = json_object_get(lock_json, "key_auid");
            if (value) pexit->door.lock.key_load.auid = json_integer_value(value);
            value = json_object_get(lock_json, "key_vnum");
            if (value) pexit->door.lock.key_load.vnum = json_integer_value(value);
            value = json_object_get(lock_json, "flags");
            if (value) pexit->door.lock.flags = flag_value(lock_flags, (char *)json_string_value(value));
            value = json_object_get(lock_json, "pick_chance");
            if (value) pexit->door.lock.pick_chance = json_integer_value(value);
        }

        /* Reset lock */
        lock_json = json_object_get(door_json, "reset_lock");
        if (lock_json) {
            value = json_object_get(lock_json, "key_auid");
            if (value) pexit->door.rs_lock.key_load.auid = json_integer_value(value);
            value = json_object_get(lock_json, "key_vnum");
            if (value) pexit->door.rs_lock.key_load.vnum = json_integer_value(value);
            value = json_object_get(lock_json, "flags");
            if (value) pexit->door.rs_lock.flags = flag_value(lock_flags, (char *)json_string_value(value));
            value = json_object_get(lock_json, "pick_chance");
            if (value) pexit->door.rs_lock.pick_chance = json_integer_value(value);
        }

        value = json_object_get(door_json, "material");
        if (value) {
            free_string(pexit->door.material);
            pexit->door.material = str_dup(json_string_value(value));
        }
    }

    return pexit;
}

/***************************************************************************
 * Batch Operations                                                        *
 ***************************************************************************/

/*
 * Helper function to check if an entity is in a persistent environment.
 * Returns true if the entity should be skipped because it will be saved
 * as part of its container.
 */
static bool in_persistent_environment_depth(CHAR_DATA *ch, OBJ_DATA *obj, ROOM_INDEX_DATA *room, int depth)
{
    if (depth > 10) {
        log_string("in_persistent_environment: Recursion depth exceeded, possible circular reference");
        return false;  /* Assume not in persistent environment to avoid infinite loop */
    }
    
    if (ch) {
        if (!IS_NPC(ch)) return true;  /* Players handled separately */
        if (ch->in_room && ch->in_room->persist) return true;
        return in_persistent_environment_depth(NULL, NULL, ch->in_room, depth + 1);
    } else if (obj) {
        if (obj->locker) return true;  /* In a player locker */
        if (obj->in_obj) {
            if (obj->in_obj->persist) return true;
            return in_persistent_environment_depth(NULL, obj->in_obj, NULL, depth + 1);
        }
        if (obj->carried_by) {
            if (!IS_NPC(obj->carried_by)) return true;  /* On a player */
            if (obj->carried_by->persist) return true;
            return in_persistent_environment_depth(obj->carried_by, NULL, NULL, depth + 1);
        }
        if (obj->in_room) {
            if (obj->in_room->persist) return true;
            return in_persistent_environment_depth(NULL, NULL, obj->in_room, depth + 1);
        }
    }
    return false;
}

static bool in_persistent_environment(CHAR_DATA *ch, OBJ_DATA *obj, ROOM_INDEX_DATA *room)
{
    return in_persistent_environment_depth(ch, obj, room, 0);
}

bool json_persist_save_all(void)
{
    ROOM_INDEX_DATA *room;
    CHAR_DATA *mob;
    OBJ_DATA *obj;
    ITERATOR it;
    int saved_rooms = 0, saved_mobs = 0, saved_objs = 0;
    int failed_rooms = 0, failed_mobs = 0, failed_objs = 0;

    log_string("json_persist_save_all: Saving all persistent entities to cache...");

    /* Save all persistent rooms */
    if (persist_rooms && list_size(persist_rooms) > 0) {
        iterator_start(&it, persist_rooms);
        while ((room = (ROOM_INDEX_DATA *)iterator_nextdata(&it))) {
            if (json_persist_save_room_cached(room)) {
                saved_rooms++;
            } else {
                failed_rooms++;
                log_stringf("json_persist_save_all: Failed to save room %ld:%ld",
                    room->area ? room->area->uid : 0, room->vnum);
            }
        }
        iterator_stop(&it);
    }

    /* Save all persistent mobs */
    if (persist_mobs && list_size(persist_mobs) > 0) {
        iterator_start(&it, persist_mobs);
        while ((mob = (CHAR_DATA *)iterator_nextdata(&it))) {
            if (json_persist_save_mobile_cached(mob)) {
                saved_mobs++;
            } else {
                failed_mobs++;
                log_stringf("json_persist_save_all: Failed to save mob %ld",
                    mob->id);
            }
        }
        iterator_stop(&it);
    }

    /* Save all persistent objects */
    if (persist_objs && list_size(persist_objs) > 0) {
        iterator_start(&it, persist_objs);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (json_persist_save_object_cached(obj)) {
                saved_objs++;
            } else {
                failed_objs++;
                log_stringf("json_persist_save_all: Failed to save object %ld",
                    obj->id);
            }
        }
        iterator_stop(&it);
    }

    log_stringf("json_persist_save_all: Complete - saved %d rooms, %d mobs, %d objects (failed: %d/%d/%d)",
        saved_rooms, saved_mobs, saved_objs, failed_rooms, failed_mobs, failed_objs);

    return (failed_rooms == 0 && failed_mobs == 0 && failed_objs == 0);
}

bool json_persist_load_all(void)
{
    DIR *dir;
    struct dirent *entry;
    char rooms_dir_buf[MAX_INPUT_LENGTH];
    char mobiles_dir_buf[MAX_INPUT_LENGTH];
    char objects_dir_buf[MAX_INPUT_LENGTH];
    const char *rooms_dir = persist_json_rooms_path(rooms_dir_buf, sizeof(rooms_dir_buf));
    const char *mobiles_dir = persist_json_mobiles_path(mobiles_dir_buf, sizeof(mobiles_dir_buf));
    const char *objects_dir = persist_json_objects_path(objects_dir_buf, sizeof(objects_dir_buf));
    int loaded_objs = 0, loaded_mobs = 0, loaded_rooms = 0;
    int failed_objs = 0, failed_mobs = 0, failed_rooms = 0;

    log_string("json_persist_load_all: Starting JSON persistence load...");

    /* Ensure directory structure exists */
    if (!json_persist_init()) {
        log_string("json_persist_load_all: Failed to initialize directory structure");
        return false;
    }

    /* Load rooms first */
    dir = opendir(rooms_dir);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            if (!strstr(entry->d_name, ".json")) continue;

            /* Extract room ID from filename */
            char room_id[256];
            strncpy(room_id, entry->d_name, sizeof(room_id) - 1);
            room_id[sizeof(room_id) - 1] = '\0';
            char *dot = strstr(room_id, ".json");
            if (dot) *dot = '\0';

            ROOM_INDEX_DATA *room = json_persist_load_room(room_id);
            if (room) {
                if (room->persist) {
                    persist_addroom(room);
                }
                loaded_rooms++;
            } else {
                failed_rooms++;
            }
        }
        closedir(dir);
    }

    /* Load mobiles */
    dir = opendir(mobiles_dir);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            if (!strstr(entry->d_name, ".json")) continue;

            /* Parse ID from filename (format: id0_id1.json) */
            unsigned long id0, id1;
            if (sscanf(entry->d_name, "%lu_%lu.json", &id0, &id1) == 2) {
                CHAR_DATA *ch = json_persist_load_mobile(id0, id1);
                if (ch) {
                    /* Place mobile in world */
                    if (ch->in_room) {
                        char_to_room(ch, ch->in_room);
                    } else {
                        /* Default to limbo or some safe room */
                        ROOM_INDEX_DATA *safe_room = get_reserved_room_index("room_default");
                        if (safe_room) {
                            char_to_room(ch, safe_room);
                        }
                    }
                    if (ch->persist) {
                        persist_addmobile(ch);
                    }
                    loaded_mobs++;
                } else {
                    failed_mobs++;
                }
            }
        }
        closedir(dir);
    }

    /* Load objects */
    dir = opendir(objects_dir);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            if (!strstr(entry->d_name, ".json")) continue;

            /* Parse ID from filename (format: id0_id1.json) */
            unsigned long id0, id1;
            if (sscanf(entry->d_name, "%lu_%lu.json", &id0, &id1) == 2) {
                OBJ_DATA *obj = json_persist_load_object(id0, id1);
                if (obj) {
                    /* Place object in world */
                    if (obj->in_room) {
                        obj_to_room(obj, obj->in_room);
                    }
                    if (obj->persist) {
                        persist_addobject(obj);
                    }
                    loaded_objs++;
                } else {
                    failed_objs++;
                }
            }
        }
        closedir(dir);
    }

    log_stringf("json_persist_load_all: Loaded %d rooms (%d failed), %d mobiles (%d failed), %d objects (%d failed)",
               loaded_rooms, failed_rooms, loaded_mobs, failed_mobs, loaded_objs, failed_objs);

    return (failed_rooms == 0 && failed_mobs == 0 && failed_objs == 0);
}

/***************************************************************************
 * Migration Utilities                                                     *
 ***************************************************************************/

bool json_persist_needs_migration(void)
{
    FILE *fp;
    DIR *dir;
    char persist_file_buf[MAX_INPUT_LENGTH];
    char objects_dir_buf[MAX_INPUT_LENGTH];
    const char *persist_file = persist_dat_path(persist_file_buf, sizeof(persist_file_buf));
    const char *objects_dir = persist_json_objects_path(objects_dir_buf, sizeof(objects_dir_buf));

    /* Check if persist.dat exists */
    fp = fopen(persist_file, "r");
    if (!fp) {
        return false;  /* No old file to migrate */
    }
    fclose(fp);

    /* Check if JSON persist directory has content */
    dir = opendir(objects_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] != '.') {
                closedir(dir);
                return false;  /* Already has JSON files */
            }
        }
        closedir(dir);
    }

    return true;  /* Has persist.dat but no JSON files */
}

bool json_persist_migrate_from_dat(void)
{
    /* TODO: Implement migration from persist.dat */
    log_string("json_persist_migrate_from_dat: Not yet implemented");
    return false;
}

/***************************************************************************
 * Phase 2: Background Dirty Queue Worker                                  *
 ***************************************************************************/

static pthread_t persist_worker_thread;
static bool persist_worker_running = false;
static bool persist_worker_shutdown = false;

/*
 * Parse a dirty key to determine type and extract ID
 * Key format: persist:<type>:<id>
 * Returns: 0=room, 1=mobile, 2=object, 3=area, 4=olc_history_file, -1=error
 */
/*
 * Dirty key types:
 *   0 = room (persist:room:id)
 *   1 = mobile (persist:mobile:id)
 *   2 = object (persist:object:id)
 *   3 = area (persist:area:uid)
 *   4 = olc history file (persist:olc_history_file:<hist_type>)
 */
static int parse_dirty_key(const char *key, char *id_buf, size_t id_size)
{
    const char *type_start, *id_start;

    if (!key) {
        return -1;
    }

    /* Handle persist:* keys (rooms, mobiles, objects) */
    if (strncmp(key, "persist:", 8) == 0) {
        type_start = key + 8;

        if (strncmp(type_start, "room:", 5) == 0) {
            id_start = type_start + 5;
            strncpy(id_buf, id_start, id_size - 1);
            id_buf[id_size - 1] = '\0';
            return 0;
        } else if (strncmp(type_start, "mobile:", 7) == 0) {
            id_start = type_start + 7;
            strncpy(id_buf, id_start, id_size - 1);
            id_buf[id_size - 1] = '\0';
            return 1;
        } else if (strncmp(type_start, "object:", 7) == 0) {
            id_start = type_start + 7;
            strncpy(id_buf, id_start, id_size - 1);
            id_buf[id_size - 1] = '\0';
            return 2;
        } else if (strncmp(type_start, "olc_history_file:", 17) == 0) {
            id_start = type_start + 17;
            strncpy(id_buf, id_start, id_size - 1);
            id_buf[id_size - 1] = '\0';
            return 4;
        }
    }
    /* Handle area:full:* keys (areas use different namespace) */
    else if (strncmp(key, "area:full:", 10) == 0) {
        id_start = key + 10;
        strncpy(id_buf, id_start, id_size - 1);
        id_buf[id_size - 1] = '\0';
        return 3;
    }

    return -1;
}

/*
 * Validate cached JSON payload based on dirty key type before disk write.
 */
static bool validate_cached_payload_for_type(int type, const char *key, const char *json_str)
{
    json_error_t jerr;
    json_t *root;
    bool valid = false;

    if (!json_str)
        return false;

    root = json_loads(json_str, 0, &jerr);
    if (!root) {
        log_stringf("persist_worker: Invalid JSON payload for key %s (line %d): %s",
                    key ? key : "(null)", jerr.line, jerr.text);
        return false;
    }

    switch (type) {
    case 0: /* room */
    case 1: /* mobile */
    case 2: /* object */
        /* Persist entity payloads are expected to be JSON objects. */
        valid = json_is_object(root);
        if (!valid) {
            log_stringf("persist_worker: Invalid entity payload shape for key %s (expected object)",
                        key ? key : "(null)");
        }
        break;

    case 4: /* olc history file */
    {
        json_t *entries;
        valid = json_is_object(root);
        if (valid) {
            entries = json_object_get(root, "entries");
            valid = (entries && json_is_object(entries));
        }
        if (!valid) {
            log_stringf("persist_worker: Invalid OLC history payload shape for key %s", key ? key : "(null)");
        }
        break;
    }

    case 3: /* area */
        /* area jobs are intentionally ignored by this worker. */
        valid = true;
        break;

    default:
        valid = false;
        break;
    }

    json_decref(root);
    return valid;
}

/*
 * Write a dirty key's data from Redis to disk
 */
static bool write_dirty_key_to_disk(const char *key, bool *retryable)
{
    char *json_str;
    char id_buf[256];
    char path[512];
    FILE *fp;
    int type;

    if (retryable) {
        *retryable = true;
    }

    /* Get the JSON data from Redis */
    json_str = redis_get_persist_data(key);
    if (!json_str) {
        log_stringf("persist_worker: No data in Redis for key %s", key);
        if (retryable) {
            *retryable = false;
        }
        return false;
    }

    /* Parse the key to determine file path */
    type = parse_dirty_key(key, id_buf, sizeof(id_buf));

    /* Validate cache payload before touching disk */
    if (!validate_cached_payload_for_type(type, key, json_str)) {
        free(json_str);
        if (retryable) {
            *retryable = false;
        }
        return false;
    }

    switch (type) {
    case 0: /* Room */
        {
            char rooms_dir_buf[MAX_INPUT_LENGTH];
            const char *rooms_dir = persist_json_rooms_path(rooms_dir_buf, sizeof(rooms_dir_buf));
            snprintf(path, sizeof(path), "%s%s.json", rooms_dir, id_buf);
        }
        break;
    case 1: /* Mobile */
        {
            char mobiles_dir_buf[MAX_INPUT_LENGTH];
            const char *mobiles_dir = persist_json_mobiles_path(mobiles_dir_buf, sizeof(mobiles_dir_buf));
            snprintf(path, sizeof(path), "%s%s.json", mobiles_dir, id_buf);
        }
        break;
    case 2: /* Object */
        {
            char objects_dir_buf[MAX_INPUT_LENGTH];
            const char *objects_dir = persist_json_objects_path(objects_dir_buf, sizeof(objects_dir_buf));
            snprintf(path, sizeof(path), "%s%s.json", objects_dir, id_buf);
        }
        break;
    case 3: /* Area */
        /*
         * Area files are persisted synchronously by area save code.
         * Do not persist area:full:* via this worker; discard stale
         * queued keys to avoid mass unintended area rewrites.
         */
        free(json_str);
        return true;
    case 4: /* OLC history file by hist type */
        {
            int hist_type = atoi(id_buf);

            /* Ensure directory exists */
            mkdir(OLC_HISTORY_DIR, 0755);

            switch (hist_type) {
            case OLC_HIST_SKILL:
                snprintf(path, sizeof(path), "%s/skills.json", OLC_HISTORY_DIR);
                break;
            case OLC_HIST_GROUP:
                snprintf(path, sizeof(path), "%s/groups.json", OLC_HISTORY_DIR);
                break;
            case OLC_HIST_SONG:
                snprintf(path, sizeof(path), "%s/songs.json", OLC_HISTORY_DIR);
                break;
            case OLC_HIST_RACE:
                snprintf(path, sizeof(path), "%s/races.json", OLC_HISTORY_DIR);
                break;
            case OLC_HIST_CLASS:
                snprintf(path, sizeof(path), "%s/classes.json", OLC_HISTORY_DIR);
                break;
            case OLC_HIST_TRAIT:
                snprintf(path, sizeof(path), "%s/traits.json", OLC_HISTORY_DIR);
                break;
            case OLC_HIST_AREA_EDITOR:
                snprintf(path, sizeof(path), "%s/area_editors.json", OLC_HISTORY_DIR);
                break;
            default:
                log_stringf("persist_worker: Unknown OLC history type %d for key %s",
                    hist_type, key);
                free(json_str);
                if (retryable) {
                    *retryable = false;
                }
                return false;
            }
        }
        break;
    default:
        log_stringf("persist_worker: Unknown key type: %s", key);
        free(json_str);
        if (retryable) {
            *retryable = false;
        }
        return false;
    }

    /* Write atomically: temp file in same directory, fsync, then rename */
    {
        char tmp_path[sizeof(path) + 64];
        size_t json_len = strlen(json_str);
        size_t written;

        snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%ld", path, (long)getpid());

        fp = fopen(tmp_path, "w");
        if (!fp) {
            log_stringf("persist_worker: Failed to open temp file %s for writing: %s", tmp_path, strerror(errno));
            {
                char msg[MSL];
                snprintf(msg, sizeof(msg),
                         "persist_worker: Failed to open temp file %s for writing: %s", tmp_path, strerror(errno));
                emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                        "persist_worker_open_temp_failed", tmp_path);
            }
            free(json_str);
            return false;
        }

        written = fwrite(json_str, 1, json_len, fp);
        if (written != json_len) {
            log_stringf("persist_worker: Short write to %s (%zu/%zu): %s", tmp_path, written, json_len, strerror(errno));
            {
                char msg[MSL];
                snprintf(msg, sizeof(msg),
                         "persist_worker: Short write to %s (%zu/%zu): %s", tmp_path, written, json_len, strerror(errno));
                emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                        "persist_worker_short_write", tmp_path);
            }
            fclose(fp);
            unlink(tmp_path);
            free(json_str);
            return false;
        }

        if (fflush(fp) != 0) {
            log_stringf("persist_worker: fflush failed for %s: %s", tmp_path, strerror(errno));
            {
                char msg[MSL];
                snprintf(msg, sizeof(msg),
                         "persist_worker: fflush failed for %s: %s", tmp_path, strerror(errno));
                emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                        "persist_worker_fflush_failed", tmp_path);
            }
            fclose(fp);
            unlink(tmp_path);
            free(json_str);
            return false;
        }

        if (fsync(fileno(fp)) != 0) {
            log_stringf("persist_worker: fsync failed for %s: %s", tmp_path, strerror(errno));
            {
                char msg[MSL];
                snprintf(msg, sizeof(msg),
                         "persist_worker: fsync failed for %s: %s", tmp_path, strerror(errno));
                emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                        "persist_worker_fsync_failed", tmp_path);
            }
            fclose(fp);
            unlink(tmp_path);
            free(json_str);
            return false;
        }

        if (fclose(fp) != 0) {
            log_stringf("persist_worker: fclose failed for %s: %s", tmp_path, strerror(errno));
            {
                char msg[MSL];
                snprintf(msg, sizeof(msg),
                         "persist_worker: fclose failed for %s: %s", tmp_path, strerror(errno));
                emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                        "persist_worker_fclose_failed", tmp_path);
            }
            unlink(tmp_path);
            free(json_str);
            return false;
        }

        if (rename(tmp_path, path) != 0) {
            log_stringf("persist_worker: rename(%s -> %s) failed: %s", tmp_path, path, strerror(errno));
            {
                char msg[MSL];
                snprintf(msg, sizeof(msg),
                         "persist_worker: rename(%s -> %s) failed: %s", tmp_path, path, strerror(errno));
                emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                        "persist_worker_rename_failed", path);
            }
            unlink(tmp_path);
            free(json_str);
            return false;
        }
    }

    free(json_str);

    return true;
}

/*
 * Background worker thread - processes dirty queue
 */
static void *persist_worker_func(void *arg)
{
    char *dirty_key;
    int processed = 0;
    int errors = 0;
    int idle_sleep_us = 100000;

    (void)arg;  /* Unused */

    log_string("persist_worker: Background worker started");

    while (!persist_worker_shutdown) {
        /* Non-blocking pop from dirty queue */
        dirty_key = redis_pop_dirty_key(0);

        if (dirty_key) {
            bool retryable = true;

            /* Queue active: poll aggressively */
            idle_sleep_us = 10000;

            if (write_dirty_key_to_disk(dirty_key, &retryable)) {
                processed++;
            } else {
                errors++;
                /* Re-queue only transient failures; drop corrupt/missing payload jobs */
                if (retryable) {
                    redis_queue_dirty_key(dirty_key);
                } else {
                    log_stringf("persist_worker: Dropping non-retryable dirty key %s", dirty_key);
                }
            }
            free(dirty_key);

            /* Log progress periodically */
            if (processed > 0 && processed % 100 == 0) {
                log_stringf("persist_worker: Processed %d keys (%d errors)", processed, errors);
            }
        } else {
            /* Queue empty - back off poll interval to reduce Redis chatter */
            usleep(idle_sleep_us);
            if (idle_sleep_us < 1000000) {
                idle_sleep_us *= 2;
                if (idle_sleep_us > 1000000)
                    idle_sleep_us = 1000000;
            }
        }
    }

    log_stringf("persist_worker: Shutdown - processed %d keys (%d errors)", processed, errors);
    return NULL;
}

/*
 * Start the background persist worker thread
 */
bool json_persist_worker_start(void)
{
    int ret;

    if (persist_worker_running) {
        log_string("persist_worker: Already running");
        return true;
    }

    if (!redis_is_available()) {
        log_string("persist_worker: Redis not available, worker not started");
        return false;
    }

    persist_worker_shutdown = false;

    ret = pthread_create(&persist_worker_thread, NULL, persist_worker_func, NULL);
    if (ret != 0) {
        log_stringf("persist_worker: Failed to create thread: %s", strerror(ret));
        return false;
    }

    persist_worker_running = true;
    log_string("persist_worker: Worker thread created");
    return true;
}

/*
 * Stop the background persist worker thread
 */
void json_persist_worker_stop(void)
{
    if (!persist_worker_running) {
        return;
    }

    log_string("persist_worker: Requesting shutdown...");
    persist_worker_shutdown = true;

    /* Wait for worker to finish */
    pthread_join(persist_worker_thread, NULL);

    persist_worker_running = false;
    log_string("persist_worker: Shutdown complete");
}

/*
 * Check if background worker is running
 */
bool json_persist_worker_is_running(void)
{
    return persist_worker_running;
}

/***************************************************************************
 * Phase 2: Redis-Cached Save Functions                                    *
 ***************************************************************************/

/*
 * Save object through Redis cache (async write to disk)
 */
bool json_persist_save_object_cached(OBJ_DATA *obj)
{
    json_t *json;
    char *json_str;
    bool result;

    if (!obj || !obj->id[0]) return false;
    if (!redis_is_available()) {
        /* Fall back to direct disk write */
        return json_persist_save_object(obj);
    }

    json = json_persist_object_to_json(obj);
    if (!json) {
        char msg[MSL];
        snprintf(msg, sizeof(msg),
                 "json_persist_save_object_cached: failed to serialize object %lu_%lu",
                 obj->id[0], obj->id[1]);
        emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                "persist_save_object_cached_serialize_failed", "redis");
        return false;
    }

    json_str = json_dumps(json, JSON_INDENT(2));
    json_decref(json);

    if (!json_str) {
        char msg[MSL];
        snprintf(msg, sizeof(msg),
                 "json_persist_save_object_cached: json_dumps failed for object %lu_%lu",
                 obj->id[0], obj->id[1]);
        emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                "persist_save_object_cached_dump_failed", "redis");
        return false;
    }

    result = redis_cache_object_state(obj->id[0], obj->id[1], json_str);
    free(json_str);

    return result;
}

/*
 * Save mobile through Redis cache (async write to disk)
 */
bool json_persist_save_mobile_cached(CHAR_DATA *ch)
{
    json_t *json;
    char *json_str;
    bool result;

    if (!ch || !ch->id[0]) return false;
    if (!IS_NPC(ch)) return false;
    if (!redis_is_available()) {
        /* Fall back to direct disk write */
        return json_persist_save_mobile(ch);
    }

    json = json_persist_mobile_to_json(ch);
    if (!json) {
        char msg[MSL];
        snprintf(msg, sizeof(msg),
                 "json_persist_save_mobile_cached: failed to serialize mobile %lu_%lu",
                 ch->id[0], ch->id[1]);
        emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                "persist_save_mobile_cached_serialize_failed", "redis");
        return false;
    }

    json_str = json_dumps(json, JSON_INDENT(2));
    json_decref(json);

    if (!json_str) {
        char msg[MSL];
        snprintf(msg, sizeof(msg),
                 "json_persist_save_mobile_cached: json_dumps failed for mobile %lu_%lu",
                 ch->id[0], ch->id[1]);
        emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                "persist_save_mobile_cached_dump_failed", "redis");
        return false;
    }

    result = redis_cache_mobile_state(ch->id[0], ch->id[1], json_str);
    free(json_str);

    return result;
}

/*
 * Save room through Redis cache (async write to disk)
 */
bool json_persist_save_room_cached(ROOM_INDEX_DATA *room)
{
    json_t *json;
    char *json_str;
    char room_id[256];
    bool result;

    if (!room) return false;
    if (!redis_is_available()) {
        /* Fall back to direct disk write */
        return json_persist_save_room(room);
    }

    json = json_persist_room_to_json(room);
    if (!json) {
        json_persist_room_id(room, room_id, sizeof(room_id));
        {
            char msg[MSL];
            snprintf(msg, sizeof(msg),
                     "json_persist_save_room_cached: failed to serialize room %s", room_id);
            emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                    "persist_save_room_cached_serialize_failed", "redis");
        }
        return false;
    }

    json_str = json_dumps(json, JSON_INDENT(2));
    json_decref(json);

    if (!json_str) {
        json_persist_room_id(room, room_id, sizeof(room_id));
        {
            char msg[MSL];
            snprintf(msg, sizeof(msg),
                     "json_persist_save_room_cached: json_dumps failed for room %s", room_id);
            emit_json_persist_event(EVENT_SEV_ERROR, LOG_ERROR, msg,
                                    "persist_save_room_cached_dump_failed", "redis");
        }
        return false;
    }

    json_persist_room_id(room, room_id, sizeof(room_id));
    result = redis_cache_room_state(room_id, json_str);
    free(json_str);

    return result;
}

/***************************************************************************
 * Phase 2: Redis-Cached Load Functions                                    *
 ***************************************************************************/

/*
 * Load object - check Redis cache first, then disk
 */
OBJ_DATA *json_persist_load_object_cached(unsigned long id0, unsigned long id1)
{
    char *json_str;
    json_t *json;
    json_error_t error;
    OBJ_DATA *obj;

    if (!redis_is_available()) {
        /* Fall back to direct disk load */
        return json_persist_load_object(id0, id1);
    }

    /* Try Redis first */
    json_str = redis_get_object_state(id0, id1);
    if (json_str) {
        json = json_loads(json_str, 0, &error);
        free(json_str);

        if (json) {
            obj = json_persist_json_to_object(json);
            json_decref(json);
            return obj;
        }
    }

    /* Fall back to disk */
    return json_persist_load_object(id0, id1);
}

/*
 * Load mobile - check Redis cache first, then disk
 */
CHAR_DATA *json_persist_load_mobile_cached(unsigned long id0, unsigned long id1)
{
    char *json_str;
    json_t *json;
    json_error_t error;
    CHAR_DATA *ch;

    if (!redis_is_available()) {
        /* Fall back to direct disk load */
        return json_persist_load_mobile(id0, id1);
    }

    /* Try Redis first */
    json_str = redis_get_mobile_state(id0, id1);
    if (json_str) {
        json = json_loads(json_str, 0, &error);
        free(json_str);

        if (json) {
            ch = json_persist_json_to_mobile(json);
            json_decref(json);
            return ch;
        }
    }

    /* Fall back to disk */
    return json_persist_load_mobile(id0, id1);
}

/*
 * Load room - check Redis cache first, then disk
 */
ROOM_INDEX_DATA *json_persist_load_room_cached(const char *room_id)
{
    char *json_str;
    json_t *json;
    json_error_t error;
    ROOM_INDEX_DATA *room;

    if (!room_id || !room_id[0]) return NULL;

    if (!redis_is_available()) {
        /* Fall back to direct disk load */
        return json_persist_load_room(room_id);
    }

    /* Try Redis first */
    json_str = redis_get_room_state(room_id);
    if (json_str) {
        json = json_loads(json_str, 0, &error);
        free(json_str);

        if (json) {
            room = json_persist_json_to_room(json);
            json_decref(json);
            return room;
        }
    }

    /* Fall back to disk */
    return json_persist_load_room(room_id);
}

/***************************************************************************
 * Phase 2: Cache Warming                                                   *
 ***************************************************************************/

/*
 * Warm Redis cache with all currently loaded persist entities.
 * Called after Redis is initialized and persist entities are loaded from disk.
 */
void json_persist_warm_cache(void)
{
    ITERATOR it;
    CHAR_DATA *ch;
    OBJ_DATA *obj;
    ROOM_INDEX_DATA *room;
    int cached_objs = 0, cached_mobs = 0, cached_rooms = 0;

    if (!redis_is_available()) {
        log_string("json_persist_warm_cache: Redis not available, skipping cache warm");
        return;
    }

    log_string("json_persist_warm_cache: Warming Redis cache with persist entities...");

    /* Cache rooms (they contain objects and mobiles so do first) */
    if (persist_rooms && list_size(persist_rooms) > 0) {
        iterator_start(&it, persist_rooms);
        while ((room = (ROOM_INDEX_DATA *)iterator_nextdata(&it))) {
            if (room->persist) {
                json_t *json = json_persist_room_to_json(room);
                if (json) {
                    char *json_str = json_dumps(json, JSON_INDENT(2));
                    json_decref(json);
                    if (json_str) {
                        char room_id[256];
                        char key[512];
                        json_persist_room_id(room, room_id, sizeof(room_id));
                        snprintf(key, sizeof(key), "persist:room:%s", room_id);
                        /* Cache only, don't queue as dirty (already on disk) */
                        redis_cache_persist_data(key, json_str);
                        free(json_str);
                        cached_rooms++;
                    }
                }
            }
        }
        iterator_stop(&it);
    }

    /* Cache mobiles not in persistent rooms */
    if (persist_mobs && list_size(persist_mobs) > 0) {
        iterator_start(&it, persist_mobs);
        while ((ch = (CHAR_DATA *)iterator_nextdata(&it))) {
            if (ch->persist && !in_persistent_environment(ch, NULL, NULL)) {
                json_t *json = json_persist_mobile_to_json(ch);
                if (json) {
                    char *json_str = json_dumps(json, JSON_INDENT(2));
                    json_decref(json);
                    if (json_str) {
                        char key[256];
                        snprintf(key, sizeof(key), "persist:mobile:%lu:%lu", ch->id[0], ch->id[1]);
                        redis_cache_persist_data(key, json_str);
                        free(json_str);
                        cached_mobs++;
                    }
                }
            }
        }
        iterator_stop(&it);
    }

    /* Cache objects not in persistent containers/rooms/mobs */
    if (persist_objs && list_size(persist_objs) > 0) {
        iterator_start(&it, persist_objs);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (obj->persist && !in_persistent_environment(NULL, obj, NULL)) {
                json_t *json = json_persist_object_to_json(obj);
                if (json) {
                    char *json_str = json_dumps(json, JSON_INDENT(2));
                    json_decref(json);
                    if (json_str) {
                        char key[256];
                        snprintf(key, sizeof(key), "persist:object:%lu:%lu", obj->id[0], obj->id[1]);
                        redis_cache_persist_data(key, json_str);
                        free(json_str);
                        cached_objs++;
                    }
                }
            }
        }
        iterator_stop(&it);
    }

    log_stringf("json_persist_warm_cache: Cached %d rooms, %d mobiles, %d objects",
               cached_rooms, cached_mobs, cached_objs);
}
