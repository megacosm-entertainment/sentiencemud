/***************************************************************************
 *  JSON Character Format - COMPLETE IMPLEMENTATION WITH ALL FIXES         *
 *                                                                          *
 *  This file contains comprehensive character serialization including:    *
 *  - All character flags (act, comm, config)                              *
 *  - Full class data with subclasses                                      *
 *  - Human-readable skill names                                           *
 *  - All inventory/equipment                                              *
 *  - Proper character initialization on load                              *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <jansson.h>
#include "../../merc.h"
#include "../../tables.h"
#include "../../recycle.h"
#include "json_common.h"
#include "json_char.h"
#include "json_persist.h"
#include "../../account/preferences.h"
#include "../cache/redis_cache.h"
#include "../../wilds.h"
#include "../../skill_data.h"
#include "../../traits.h"
#include "../../class_data.h"
#include "../../skill_group.h"
#include "../../account/unlock.h"
#include "../../song_data.h"
#include "json_obj_types.h"

/***************************************************************************
 * External Flag Tables                                                    *
 ***************************************************************************/

extern const struct flag_type act_flags[];
extern const struct flag_type act2_flags[];
extern const struct flag_type plr_flags[];
extern const struct flag_type plr2_flags[];
extern const struct flag_type affect_flags[];
extern const struct flag_type affect2_flags[];
extern const struct flag_type imm_flags[];
extern const struct flag_type res_flags[];
extern const struct flag_type vuln_flags[];
extern const struct flag_type comm_flags[];
extern const struct flag_type channel_flags[];
extern const struct flag_type part_flags[];

/***************************************************************************
 * Utility Functions                                                       *
 ***************************************************************************/

void json_get_char_path(const char *char_name, char *path_buf, size_t buf_size)
{
    // No .json extension - same path as old pfile
    // Format is auto-detected on load
    snprintf(path_buf, buf_size, "%s%c/%s",
             PLAYER_DIR, tolower(char_name[0]), char_name);
}

void json_get_pfile_path(const char *char_name, char *path_buf, size_t buf_size)
{
    // Same as json_get_char_path - unified filename
    snprintf(path_buf, buf_size, "%s%c/%s",
             PLAYER_DIR, tolower(char_name[0]), char_name);
}

void json_get_backup_path(const char *char_name, char *path_buf, size_t buf_size)
{
    // Backup old pfile before migration
    snprintf(path_buf, buf_size, "%s%c.old/%s",
             PLAYER_DIR, tolower(char_name[0]), char_name);
}

bool json_ensure_char_dir(const char *char_name)
{
    return json_ensure_dir(PLAYER_DIR, char_name);
}

bool json_is_json_file(const char *filename)
{
    return json_file_is_json(filename);
}

/***************************************************************************
 * CHAR_INFO_CACHE Serialization                                          *
 ***************************************************************************/

json_t *char_info_to_json(CHAR_INFO_CACHE *info)
{
    json_t *json, *classes_array;
    int i;

    if (!info) {
        return NULL;
    }

    json = json_object();

    // Basic info
    json_object_set_new(json, "name", json_string(info->name));
    json_object_set_new(json, "level", json_integer(info->level));
    json_object_set_new(json, "tot_level", json_integer(info->tot_level));
    json_object_set_new(json, "remorts", json_integer(info->remorts));
    json_object_set_new(json, "race", json_string(info->race));
    json_object_set_new(json, "title", json_string(info->title));

    // Classes
    classes_array = json_array();
    for (i = 0; i < info->num_classes; i++) {
        json_array_append_new(classes_array, json_string(info->classes[i]));
    }
    json_object_set_new(json, "classes", classes_array);

    // Stats
    json_object_set_new(json, "health_pct", json_integer(info->health_pct));
    json_object_set_new(json, "mana_pct", json_integer(info->mana_pct));
    json_object_set_new(json, "gold", json_integer(info->gold));
    json_object_set_new(json, "experience", json_integer(info->experience));

    // Metadata
    json_object_set_new(json, "last_played", json_integer(info->last_played));
    json_object_set_new(json, "is_active", json_boolean(info->is_active));

    return json;
}

CHAR_INFO_CACHE *json_to_char_info(json_t *json)
{
    CHAR_INFO_CACHE *info;
    json_t *classes_array, *value;
    size_t index;
    const char *str;

    if (!json) {
        return NULL;
    }

    info = (CHAR_INFO_CACHE *)calloc(1, sizeof(CHAR_INFO_CACHE));
    if (!info) {
        return NULL;
    }

    // Basic info
    str = json_string_value(json_object_get(json, "name"));
    info->name = str ? strdup(str) : strdup("Unknown");

    info->level = json_integer_value(json_object_get(json, "level"));
    info->tot_level = json_integer_value(json_object_get(json, "tot_level"));
    info->remorts = json_integer_value(json_object_get(json, "remorts"));

    str = json_string_value(json_object_get(json, "race"));
    info->race = str ? strdup(str) : strdup("human");

    str = json_string_value(json_object_get(json, "title"));
    info->title = str ? strdup(str) : strdup("");

    // Classes
    classes_array = json_object_get(json, "classes");
    if (classes_array && json_is_array(classes_array)) {
        info->num_classes = json_array_size(classes_array);
        if (info->num_classes > 0) {
            info->classes = (char **)calloc(info->num_classes, sizeof(char *));
            json_array_foreach(classes_array, index, value) {
                str = json_string_value(value);
                info->classes[index] = str ? strdup(str) : strdup("Unknown");
            }
        }
    }

    // Stats
    info->health_pct = json_integer_value(json_object_get(json, "health_pct"));
    info->mana_pct = json_integer_value(json_object_get(json, "mana_pct"));
    info->gold = json_integer_value(json_object_get(json, "gold"));
    info->experience = json_integer_value(json_object_get(json, "experience"));

    // Metadata
    info->last_played = json_integer_value(json_object_get(json, "last_played"));
    info->is_active = json_is_true(json_object_get(json, "is_active"));

    return info;
}

/***************************************************************************
 * Object Serialization                                                    *
 ***************************************************************************/

json_t *obj_to_json(OBJ_DATA *obj, int nest_level)
{
    json_t *json_obj, *contains_array, *affects_array, *extra_descr_array, *spells_array, *tokens_array;
    json_t *catalysts_array;
    AFFECT_DATA *paf;
    EXTRA_DESCR_DATA *ed;
    SPELL_DATA *spell;
    TOKEN_DATA *token;
    WAYPOINT_DATA *wp;
    ITERATOR it;
    int i;

    if (!obj) {
        return NULL;
    }

    json_obj = json_object();

    // Basic identification - use widevnum format for area-scoped persistence
    json_object_set_new(json_obj, "vnum", json_string(widevnum_string_object(obj->pIndexData, NULL)));
    json_object_set_new(json_obj, "nest_level", json_integer(nest_level));

    // Save unique object ID if set
    if (obj->id[0] || obj->id[1]) {
        json_t *id_array = json_array();
        json_array_append_new(id_array, json_integer(obj->id[0]));
        json_array_append_new(id_array, json_integer(obj->id[1]));
        json_object_set_new(json_obj, "id", id_array);
    }

    // Use POINTER comparison (not string comparison) to match save.c behavior
    // If the pointer differs from prototype, save it - even if content is identical
    if (obj->name != obj->pIndexData->name) {
        json_object_set_new(json_obj, "name", json_string(obj->name));
    }
    if (obj->short_descr != obj->pIndexData->short_descr) {
        json_object_set_new(json_obj, "short_descr", json_string(obj->short_descr));
    }
    if (obj->description != obj->pIndexData->description) {
        json_object_set_new(json_obj, "description", json_string(obj->description));
    }
    if (obj->full_description != obj->pIndexData->full_description) {
        json_object_set_new(json_obj, "full_description", json_string(obj->full_description));
    }

    // Object state
    if (obj->item_type != obj->pIndexData->item_type) {
        json_object_set_new(json_obj, "item_type", json_integer(obj->item_type));
    }
    if (obj->wear_loc != WEAR_NONE) {
        json_object_set_new(json_obj, "wear_loc", json_integer(obj->wear_loc));
    }
    if (obj->level != obj->pIndexData->level) {
        json_object_set_new(json_obj, "level", json_integer(obj->level));
    }
    if (obj->weight != obj->pIndexData->weight) {
        json_object_set_new(json_obj, "weight", json_integer(obj->weight));
    }
    if (obj->condition != obj->pIndexData->condition) {
        json_object_set_new(json_obj, "condition", json_integer(obj->condition));
    }
    if (obj->timer) {
        json_object_set_new(json_obj, "timer", json_integer(obj->timer));
    }
    if (obj->cost != obj->pIndexData->cost) {
        json_object_set_new(json_obj, "cost", json_integer(obj->cost));
    }
    if (obj->fragility != obj->pIndexData->fragility) {
        json_object_set_new(json_obj, "fragility", json_integer(obj->fragility));
    }
    if (obj->times_allowed_fixed != obj->pIndexData->times_allowed_fixed) {
        json_object_set_new(json_obj, "times_allowed_fixed", json_integer(obj->times_allowed_fixed));
    }
    if (obj->times_fixed > 0) {
        json_object_set_new(json_obj, "times_fixed", json_integer(obj->times_fixed));
    }

    // Extra flags (only if different from prototype)
    for (i = 0; i < 4; i++) {
        if (obj->extra[i] != obj->pIndexData->extra[i]) {
            json_object_set_new(json_obj, i == 0 ? "extra_flags" :
                                          i == 1 ? "extra2_flags" :
                                          i == 2 ? "extra3_flags" : "extra4_flags",
                               json_integer(obj->extra[i]));
        }
    }

    // Type-specific data (canonical structured representation) — replaces legacy values[]
    {
        json_t *td = obj_type_data_to_json(obj);
        if (td) {
            json_object_set_new(json_obj, "type_data", td);
        }
    }

    // Owner
    if (obj->owner) {
        json_object_set_new(json_obj, "owner", json_string(obj->owner));
    }

    // Material
    if (obj->material && (!obj->pIndexData->material || str_cmp(obj->material, obj->pIndexData->material))) {
        json_object_set_new(json_obj, "material", json_string(obj->material));
    }

    // Version and persistence
    if (obj->version) {
        json_object_set_new(json_obj, "version", json_integer(obj->version));
    }
    if (obj->persist) {
        json_object_set_new(json_obj, "persist", json_boolean(obj->persist));
    }

    // Wear flags (if different from prototype)
    if (obj->wear_flags != obj->pIndexData->wear_flags) {
        json_object_set_new(json_obj, "wear_flags", json_integer(obj->wear_flags));
    }

    // Last wear location
    if (obj->last_wear_loc != WEAR_NONE) {
        json_object_set_new(json_obj, "last_wear_loc", json_integer(obj->last_wear_loc));
    }

    // Enchantment count
    if (obj->num_enchanted > 0) {
        json_object_set_new(json_obj, "num_enchanted", json_integer(obj->num_enchanted));
    }

    // Locker flag
    if (obj->locker) {
        json_object_set_new(json_obj, "locker", json_boolean(obj->locker));
    }

    // Old descriptions (pre-customize, for uncustomize command)
    if (obj->old_name) {
        json_object_set_new(json_obj, "old_name", json_string(obj->old_name));
    }
    if (obj->old_short_descr) {
        json_object_set_new(json_obj, "old_short_descr", json_string(obj->old_short_descr));
    }
    if (obj->old_description) {
        json_object_set_new(json_obj, "old_description", json_string(obj->old_description));
    }
    if (obj->old_full_description) {
        json_object_set_new(json_obj, "old_full_description", json_string(obj->old_full_description));
    }

    // Loaded by / corpse ownership
    if (obj->loaded_by) {
        json_object_set_new(json_obj, "loaded_by", json_string(obj->loaded_by));
    }
    if (obj->owner_name) {
        json_object_set_new(json_obj, "owner_name", json_string(obj->owner_name));
    }
    if (obj->owner_short) {
        json_object_set_new(json_obj, "owner_short", json_string(obj->owner_short));
    }

    // Permanent extra flags (for tracking enchant/customize changes)
    {
        bool has_perm_extra = false;
        for (i = 0; i < 4; i++) {
            if (obj->extra_perm[i]) { has_perm_extra = true; break; }
        }
        if (has_perm_extra) {
            json_t *perm_array = json_array();
            for (i = 0; i < 4; i++) {
                json_array_append_new(perm_array, json_integer(obj->extra_perm[i]));
            }
            json_object_set_new(json_obj, "extra_perm", perm_array);
        }
    }

    // Permanent weapon flags
    if (obj->item_type == ITEM_WEAPON && obj->weapon_flags_perm) {
        json_object_set_new(json_obj, "weapon_flags_perm", json_integer(obj->weapon_flags_perm));
    }

    // Script creation tracking
    if (obj->script_created) {
        json_object_set_new(json_obj, "script_created", json_boolean(obj->script_created));
        json_object_set_new(json_obj, "created_script_type", json_integer(obj->created_script_type));
        if (obj->created_script_load.vnum) {
            json_object_set_new(json_obj, "created_script_vnum", json_integer(obj->created_script_load.vnum));
        }
    }
    if (obj->creation_time) {
        json_object_set_new(json_obj, "creation_time", json_integer(obj->creation_time));
    }

    // Extra descriptions
    extra_descr_array = json_array();
    for (ed = obj->extra_descr; ed; ed = ed->next) {
        json_t *ed_obj = json_object();
        json_object_set_new(ed_obj, "keyword", json_string(ed->keyword));
        json_object_set_new(ed_obj, "description", json_string(ed->description));
        json_array_append_new(extra_descr_array, ed_obj);
    }
    if (json_array_size(extra_descr_array) > 0) {
        json_object_set_new(json_obj, "extra_descr", extra_descr_array);
    } else {
        json_decref(extra_descr_array);
    }

    // Affects - use comprehensive format (where, group, custom_name)
    affects_array = json_array();
    for (paf = obj->affected; paf; paf = paf->next) {
        json_t *aff = json_persist_affect_to_json(paf);
        if (aff) {
            json_array_append_new(affects_array, aff);
        }
    }
    if (json_array_size(affects_array) > 0) {
        json_object_set_new(json_obj, "affects", affects_array);
    } else {
        json_decref(affects_array);
    }

    // Catalysts
    catalysts_array = json_array();
    for (paf = obj->catalyst; paf; paf = paf->next) {
        json_t *cat = json_persist_affect_to_json(paf);
        if (cat) {
            json_object_set_new(cat, "catalyst_type",
                json_string(flag_string(catalyst_types, paf->type)));
            json_array_append_new(catalysts_array, cat);
        }
    }
    if (json_array_size(catalysts_array) > 0) {
        json_object_set_new(json_obj, "catalysts", catalysts_array);
    } else {
        json_decref(catalysts_array);
    }

    // Spells (for wands, staves, scrolls, potions)
    spells_array = json_array();
    for (spell = obj->spells; spell; spell = spell->next) {
        json_t *sp = json_object();
        json_object_set_new(sp, "sn", json_integer(spell->sn));
        json_object_set_new(sp, "level", json_integer(spell->level));
        json_object_set_new(sp, "repop", json_integer(spell->repop));
        json_array_append_new(spells_array, sp);
    }
    if (json_array_size(spells_array) > 0) {
        json_object_set_new(json_obj, "spells", spells_array);
    } else {
        json_decref(spells_array);
    }

    // Lock state
    if (obj->lock) {
        json_t *lock_json = json_persist_lock_to_json(obj->lock);
        if (lock_json) {
            json_object_set_new(json_obj, "lock", lock_json);
        }
    }

    // Waypoints
    if (obj->waypoints && IS_VALID(obj->waypoints)) {
        json_t *wp_array = json_array();
        iterator_start(&it, obj->waypoints);
        while ((wp = (WAYPOINT_DATA *)iterator_nextdata(&it))) {
            json_t *wp_json = json_object();
            json_object_set_new(wp_json, "w", json_integer(wp->w));
            json_object_set_new(wp_json, "x", json_integer(wp->x));
            json_object_set_new(wp_json, "y", json_integer(wp->y));
            if (wp->name) {
                json_object_set_new(wp_json, "name", json_string(wp->name));
            }
            json_array_append_new(wp_array, wp_json);
        }
        iterator_stop(&it);
        if (json_array_size(wp_array) > 0) {
            json_object_set_new(json_obj, "waypoints", wp_array);
        } else {
            json_decref(wp_array);
        }
    }

    // Object tokens
    if (obj->tokens) {
        tokens_array = json_array();
        for (token = obj->tokens; token; token = token->next) {
            json_t *tok_json = json_persist_token_to_json(token);
            if (tok_json) {
                json_array_append_new(tokens_array, tok_json);
            }
        }
        if (json_array_size(tokens_array) > 0) {
            json_object_set_new(json_obj, "obj_tokens", tokens_array);
        } else {
            json_decref(tokens_array);
        }
    }

    // Script variables
    if (obj->progs) {
        json_t *vars = json_persist_scriptdata_to_json(obj->progs);
        if (vars) {
            json_object_set_new(json_obj, "variables", vars);
        }
    }

    // Contains (nested objects)
    contains_array = json_array();
    for (OBJ_DATA *cont_obj = obj->contains; cont_obj; cont_obj = cont_obj->next_content) {
        json_t *contained = obj_to_json(cont_obj, nest_level + 1);
        if (contained) {
            json_array_append_new(contains_array, contained);
        }
    }
    if (json_array_size(contains_array) > 0) {
        json_object_set_new(json_obj, "contains", contains_array);
    } else {
        json_decref(contains_array);
    }

    return json_obj;
}

static json_t *inventory_to_json(CHAR_DATA *ch)
{
    json_t *inventory;
    OBJ_DATA *obj;
    ITERATOR it;
    int written_count = 0;

    inventory = json_array();

    // Use lcarrying (LIST structure) instead of carrying (deprecated linked list)
    if (ch->lcarrying && IS_VALID(ch->lcarrying)) {

        iterator_start(&it, ch->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            // Only write top-level inventory items (not equipped, not in locker, not in containers)
            if (!obj->locker && obj->in_obj == NULL && obj->wear_loc == WEAR_NONE) {
                json_t *json_obj = obj_to_json(obj, 0);
                if (json_obj) {
                    json_array_append_new(inventory, json_obj);
                    written_count++;
                }
            }
        }
        iterator_stop(&it);
    }

    return inventory;
}

static json_t *equipment_to_json(CHAR_DATA *ch)
{
    json_t *equipment;
    OBJ_DATA *obj;
    ITERATOR it;

    equipment = json_array();

    // Use lworn (LIST structure) instead of iterating carrying
    if (ch->lworn && IS_VALID(ch->lworn)) {
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            // Only write non-locker equipped items
            if (!obj->locker && obj->in_obj == NULL) {
                json_t *json_obj = obj_to_json(obj, 0);
                if (json_obj) {
                    json_array_append_new(equipment, json_obj);
                }
            }
        }
        iterator_stop(&it);
    }

    return equipment;
}

static json_t *locker_to_json(CHAR_DATA *ch)
{
    json_t *locker;
    OBJ_DATA *obj;
    ITERATOR it;

    locker = json_array();

    // Use llocker (LIST structure) instead of iterating carrying
    if (ch->llocker && IS_VALID(ch->llocker)) {
        iterator_start(&it, ch->llocker);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            // Only write top-level locker items (not in containers)
            if (obj->in_obj == NULL) {
                json_t *json_obj = obj_to_json(obj, 0);
                if (json_obj) {
                    json_array_append_new(locker, json_obj);
                }
            }
        }
        iterator_stop(&it);
    }

    return locker;
}

/* Flag helpers now provided by json_common.h */

/***************************************************************************
 * Skills Serialization - WITH HUMAN-READABLE NAMES                       *
 ***************************************************************************/

/**
 * json_parse_skill_source - Parse a skill source string from JSON
 *
 * @param str  Source string ("script", "script_perm", "affect") or NULL
 * @return     SKILLSRC_* constant, defaults to SKILLSRC_NORMAL
 */
static char json_parse_skill_source(const char *str)
{
    if (!str) return SKILLSRC_NORMAL;
    if (!strcmp(str, "script"))      return SKILLSRC_SCRIPT;
    if (!strcmp(str, "script_perm")) return SKILLSRC_SCRIPT_PERM;
    if (!strcmp(str, "affect"))      return SKILLSRC_AFFECT;
    return SKILLSRC_NORMAL;
}

static json_t *skills_to_json(CHAR_DATA *ch)
{
    json_t *skills;
    SKILL_ENTRY *entry;

    if (!ch->pcdata) {
        return json_object();
    }

    skills = json_object();

    // Save skills from sorted_skills list — preserves source and flags
    for (entry = ch->sorted_skills; entry; entry = entry->next) {
        // Skip token-based entries (they're saved in the tokens section)
        if (IS_VALID(entry->token))
            continue;

        int sn = entry->sn;
        if (sn <= 0 || sn >= MAX_SKILL || !skill_table[sn].name)
            continue;

        // Sync entry rating from learned[] to capture any runtime changes
        entry->rating = ch->pcdata->learned[sn];
        entry->mod_rating = ch->pcdata->mod_learned[sn];

        json_t *skill_data = json_object();

        if (entry->rating > 0) {
            json_object_set_new(skill_data, "learned", json_integer(entry->rating));
        }

        if (entry->mod_rating != 0) {
            json_object_set_new(skill_data, "mod_learned", json_integer(entry->mod_rating));
        }

        // Save source if non-default
        if (entry->source != SKILLSRC_NORMAL) {
            const char *source_name = NULL;
            switch (entry->source) {
                case SKILLSRC_SCRIPT:      source_name = "script"; break;
                case SKILLSRC_SCRIPT_PERM: source_name = "script_perm"; break;
                case SKILLSRC_AFFECT:      source_name = "affect"; break;
            }
            if (source_name)
                json_object_set_new(skill_data, "source", json_string(source_name));
        }

        // Save flags if non-default (strip SKILL_SPELL — determined at load time)
        long save_flags = entry->flags & ~SKILL_SPELL;
        if (save_flags != SKILL_AUTOMATIC) {
            json_object_set_new(skill_data, "flags",
                json_string(flag_string(skill_flags, entry->flags & ~SKILL_SPELL)));
        }

        // Save class sources list
        if (entry->sources) {
            json_t *sources_arr = json_array();
            SKILL_SOURCE *src;
            for (src = entry->sources; src; src = src->next) {
                if (src->clazz && src->clazz->name) {
                    json_t *src_obj = json_object();
                    json_object_set_new(src_obj, "class", json_string(src->clazz->name));
                    json_object_set_new(src_obj, "scope", json_integer(src->scope));
                    json_array_append_new(sources_arr, src_obj);
                }
            }
            if (json_array_size(sources_arr) > 0)
                json_object_set_new(skill_data, "class_sources", sources_arr);
            else
                json_decref(sources_arr);
        }

        json_object_set_new(skills, skill_table[sn].name, skill_data);
    }

    // Safety net: catch any skills in learned[] not represented in sorted_skills
    // This handles edge cases during migration from old format
    for (int sn = 0; sn < MAX_SKILL; sn++) {
        if ((ch->pcdata->learned[sn] > 0 || ch->pcdata->mod_learned[sn] != 0)
            && skill_table[sn].name
            && !json_object_get(skills, skill_table[sn].name)) {

            json_t *skill_data = json_object();
            if (ch->pcdata->learned[sn] > 0)
                json_object_set_new(skill_data, "learned", json_integer(ch->pcdata->learned[sn]));
            if (ch->pcdata->mod_learned[sn] != 0)
                json_object_set_new(skill_data, "mod_learned", json_integer(ch->pcdata->mod_learned[sn]));
            json_object_set_new(skills, skill_table[sn].name, skill_data);
        }
    }

    return skills;
}

/***************************************************************************
 * Token Serialization                                                     *
 ***************************************************************************/

static json_t *tokens_to_json(CHAR_DATA *ch)
{
    json_t *tokens;
    TOKEN_DATA *token;

    if (!ch->tokens) {
        return json_array();
    }

    tokens = json_array();

    for (token = ch->tokens; token; token = token->next) {
        // Skip skill tokens (they're saved with skills)
        if (token->skill) {
            continue;
        }

        json_t *token_obj = json_object();
        json_object_set_new(token_obj, "vnum", json_string(widevnum_string(token->pIndexData->area, token->pIndexData->vnum, NULL)));
        json_object_set_new(token_obj, "id", json_pack("[i, i]",
            (int)token->id[0], (int)token->id[1]));
        json_object_set_new(token_obj, "timer", json_integer(token->timer));

        // Token values
        json_t *values = json_array();
        for (int i = 0; i < MAX_TOKEN_VALUES; i++) {
            json_array_append_new(values, json_integer(token->value[i]));
        }
        json_object_set_new(token_obj, "values", values);

        // Script variables
        if (token->progs) {
            json_t *vars = json_persist_scriptdata_to_json(token->progs);
            if (vars) {
                json_object_set_new(token_obj, "variables", vars);
            }
        }

        json_array_append_new(tokens, token_obj);
    }

    return tokens;
}

/***************************************************************************
 * Aliases Serialization                                                   *
 ***************************************************************************/

static json_t *aliases_to_json(CHAR_DATA *ch)
{
    json_t *aliases;
    int pos;

    if (!ch->pcdata) {
        return json_array();
    }

    aliases = json_array();

    for (pos = 0; pos < MAX_ALIAS; pos++) {
        if (ch->pcdata->alias[pos] == NULL || ch->pcdata->alias_sub[pos] == NULL) {
            break;
        }

        json_t *alias_obj = json_object();
        json_object_set_new(alias_obj, "alias", json_string(ch->pcdata->alias[pos]));
        json_object_set_new(alias_obj, "substitution", json_string(ch->pcdata->alias_sub[pos]));
        json_array_append_new(aliases, alias_obj);
    }

    return aliases;
}

/***************************************************************************
 * Skill Groups Serialization                                             *
 ***************************************************************************/

static json_t *groups_to_json(CHAR_DATA *ch)
{
    json_t *groups;
    int gn;

    if (!ch->pcdata) {
        return json_array();
    }

    groups = json_array();

    // Save known skill groups from the new LLIST if populated, otherwise fall
    // back to the legacy bool array so existing characters still serialize.
    if (ch->pcdata->known_groups && list_size(ch->pcdata->known_groups) > 0) {
        ITERATOR sg_it;
        SKILL_GROUP *sg;
        iterator_start(&sg_it, ch->pcdata->known_groups);
        while ((sg = (SKILL_GROUP *)iterator_nextdata(&sg_it))) {
            if (sg->name) {
                json_t *group_data = json_object();
                int gn = group_lookup(sg->name);
                json_object_set_new(group_data, "id", json_integer(gn >= 0 ? gn : -1));
                json_object_set_new(group_data, "name", json_string(sg->name));
                json_array_append_new(groups, group_data);
            }
        }
        iterator_stop(&sg_it);
    } else {
        for (gn = 0; gn < MAX_GROUP; gn++) {
            if (ch->pcdata->group_known[gn] && group_table[gn].name) {
                json_t *group_data = json_object();
                json_object_set_new(group_data, "id", json_integer(gn));
                json_object_set_new(group_data, "name", json_string(group_table[gn].name));
                json_array_append_new(groups, group_data);
            }
        }
    }

    return groups;
}

/***************************************************************************
 * Affects Serialization                                                   *
 ***************************************************************************/

static json_t *affects_to_json(CHAR_DATA *ch)
{
    json_t *affects;
    AFFECT_DATA *paf;

    affects = json_array();

    for (paf = ch->affected; paf; paf = paf->next) {
        json_t *aff = json_object();

        if (paf->group) {
            json_object_set_new(aff, "group", json_integer(paf->group));
        }
        json_object_set_new(aff, "where", json_integer(paf->where));
        json_object_set_new(aff, "type", json_integer(paf->type));

        // Save skill name alongside numeric type for resilient loading
        if (paf->type > 0) {
            SKILL_DATA *sk = skill_from_sn(paf->type);
            if (sk) {
                json_object_set_new(aff, "type_name", json_string(sk->name));
            }
        }

        json_object_set_new(aff, "level", json_integer(paf->level));
        json_object_set_new(aff, "duration", json_integer(paf->duration));
        json_object_set_new(aff, "location", json_integer(paf->location));
        json_object_set_new(aff, "modifier", json_integer(paf->modifier));
        json_object_set_new(aff, "bitvector", json_integer(paf->bitvector));
        json_object_set_new(aff, "slot", json_integer(paf->slot));

        if (paf->bitvector2) {
            json_object_set_new(aff, "bitvector2", json_integer(paf->bitvector2));
        }
        if (paf->custom_name) {
            json_object_set_new(aff, "custom_name", json_string(paf->custom_name));
        }

        json_array_append_new(affects, aff);
    }

    return affects;
}

/***************************************************************************
 * Full Character Serialization - WITH ALL FLAGS AND COMPLETE DATA        *
 ***************************************************************************/

static json_t *char_metadata_to_json(CHAR_DATA *ch)
{
    json_t *meta;

    meta = json_object();

    // Format version
    json_object_set_new(meta, "format_version", json_integer(3));

    // Player data version (for migration tracking)
    json_object_set_new(meta, "version", json_integer(VERSION_PLAYER));

    // Character unique ID
    if (ch->id[0] || ch->id[1]) {
        json_t *char_id = json_array();
        json_array_append_new(char_id, json_integer(ch->id[0]));
        json_array_append_new(char_id, json_integer(ch->id[1]));
        json_object_set_new(meta, "character_id", char_id);
    }

    // Timestamps
    if (ch->pcdata) {
        json_object_set_new(meta, "created", json_integer(ch->pcdata->creation_date));
    }
    json_object_set_new(meta, "last_saved", json_integer(current_time));

    // Account linkage
    if (ch->pcdata && ch->pcdata->account_name) {
        json_t *account = json_object();
        json_object_set_new(account, "name", json_string(ch->pcdata->account_name));

        json_t *account_id = json_array();
        json_array_append_new(account_id, json_integer(ch->pcdata->account_id[0]));
        json_array_append_new(account_id, json_integer(ch->pcdata->account_id[1]));
        json_object_set_new(account, "id", account_id);

        json_object_set_new(meta, "account", account);
    }

    return meta;
}

static json_t *char_basic_to_json(CHAR_DATA *ch)
{
    json_t *basic, *classes_obj, *stats, *vitals;

    basic = json_object();

    // Basic identification
    json_object_set_new(basic, "name", json_string(ch->name));
    json_object_set_new(basic, "level", json_integer(ch->level));
    json_object_set_new(basic, "tot_level", json_integer(ch->tot_level));
    json_object_set_new(basic, "race", json_string(ch->race ? ch->race->id : "human"));
    if (ch->orace)
        json_object_set_new(basic, "original_race", json_string(ch->orace->id));
    json_object_set_new(basic, "sex", json_integer(ch->sex));
    json_object_set_new(basic, "body_type", json_integer(ch->body_type));

    // Save act/plr flags as human-readable arrays using the correct table
    if (ch->pcdata) {
        json_object_set_new(basic, "plr_flags", json_flags_serialize(ch->act[0], plr_flags));
        json_object_set_new(basic, "plr2_flags", json_flags_serialize(ch->act[1], plr2_flags));
    } else {
        json_object_set_new(basic, "act_flags", json_flags_serialize(ch->act[0], act_flags));
        json_object_set_new(basic, "act2_flags", json_flags_serialize(ch->act[1], act2_flags));
    }
    json_object_set_new(basic, "comm_flags", json_flags_serialize(ch->comm, comm_flags));
    if (ch->pcdata) {
        json_object_set_new(basic, "channel_flags", json_flags_serialize(ch->pcdata->channel_flags, channel_flags));
    }

    // Also save numeric for backward compatibility during transition
    json_object_set_new(basic, "act", json_integer(ch->act[0]));
    json_object_set_new(basic, "act2", json_integer(ch->act[1]));
    json_object_set_new(basic, "comm", json_integer(ch->comm));
    if (ch->pcdata) {
        json_object_set_new(basic, "channel_flags_numeric", json_integer(ch->pcdata->channel_flags));
    }

    // **FIX #2: Save complete class data with all levels**
    classes_obj = json_object();
    if (ch->pcdata) {
        if (ch->pcdata->class_mage > 0 || ch->pcdata->second_class_mage > 0) {
            json_t *mage = json_object();
            json_object_set_new(mage, "primary", json_integer(ch->pcdata->class_mage));
            if (ch->pcdata->second_class_mage > 0) {
                json_object_set_new(mage, "secondary", json_integer(ch->pcdata->second_class_mage));
            }
            if (ch->pcdata->sub_class_mage > 0) {
                json_object_set_new(mage, "sub", json_integer(ch->pcdata->sub_class_mage));
            }
            if (ch->pcdata->second_sub_class_mage > 0) {
                json_object_set_new(mage, "second_sub", json_integer(ch->pcdata->second_sub_class_mage));
            }
            json_object_set_new(classes_obj, "mage", mage);
        }
        if (ch->pcdata->class_cleric > 0 || ch->pcdata->second_class_cleric > 0) {
            json_t *cleric = json_object();
            json_object_set_new(cleric, "primary", json_integer(ch->pcdata->class_cleric));
            if (ch->pcdata->second_class_cleric > 0) {
                json_object_set_new(cleric, "secondary", json_integer(ch->pcdata->second_class_cleric));
            }
            if (ch->pcdata->sub_class_cleric > 0) {
                json_object_set_new(cleric, "sub", json_integer(ch->pcdata->sub_class_cleric));
            }
            if (ch->pcdata->second_sub_class_cleric > 0) {
                json_object_set_new(cleric, "second_sub", json_integer(ch->pcdata->second_sub_class_cleric));
            }
            json_object_set_new(classes_obj, "cleric", cleric);
        }
        if (ch->pcdata->class_thief > 0 || ch->pcdata->second_class_thief > 0) {
            json_t *thief = json_object();
            json_object_set_new(thief, "primary", json_integer(ch->pcdata->class_thief));
            if (ch->pcdata->second_class_thief > 0) {
                json_object_set_new(thief, "secondary", json_integer(ch->pcdata->second_class_thief));
            }
            if (ch->pcdata->sub_class_thief > 0) {
                json_object_set_new(thief, "sub", json_integer(ch->pcdata->sub_class_thief));
            }
            if (ch->pcdata->second_sub_class_thief > 0) {
                json_object_set_new(thief, "second_sub", json_integer(ch->pcdata->second_sub_class_thief));
            }
            json_object_set_new(classes_obj, "thief", thief);
        }
        if (ch->pcdata->class_warrior > 0 || ch->pcdata->second_class_warrior > 0) {
            json_t *warrior = json_object();
            json_object_set_new(warrior, "primary", json_integer(ch->pcdata->class_warrior));
            if (ch->pcdata->second_class_warrior > 0) {
                json_object_set_new(warrior, "secondary", json_integer(ch->pcdata->second_class_warrior));
            }
            if (ch->pcdata->sub_class_warrior > 0) {
                json_object_set_new(warrior, "sub", json_integer(ch->pcdata->sub_class_warrior));
            }
            if (ch->pcdata->second_sub_class_warrior > 0) {
                json_object_set_new(warrior, "second_sub", json_integer(ch->pcdata->second_sub_class_warrior));
            }
            json_object_set_new(classes_obj, "warrior", warrior);
        }
    }
    json_object_set_new(basic, "classes", classes_obj);

    // Stats
    stats = json_object();
    json_object_set_new(stats, "perm", json_pack("{s:i, s:i, s:i, s:i, s:i}",
        "str", ch->perm_stat[STAT_STR],
        "int", ch->perm_stat[STAT_INT],
        "wis", ch->perm_stat[STAT_WIS],
        "dex", ch->perm_stat[STAT_DEX],
        "con", ch->perm_stat[STAT_CON]));
    json_object_set_new(stats, "mod", json_pack("{s:i, s:i, s:i, s:i, s:i}",
        "str", ch->mod_stat[STAT_STR],
        "int", ch->mod_stat[STAT_INT],
        "wis", ch->mod_stat[STAT_WIS],
        "dex", ch->mod_stat[STAT_DEX],
        "con", ch->mod_stat[STAT_CON]));
    json_object_set_new(basic, "stats", stats);

    // Vitals
    vitals = json_object();
    json_object_set_new(vitals, "health", json_pack("{s:i, s:i}",
        "current", ch->hit, "max", ch->max_hit));
    json_object_set_new(vitals, "mana", json_pack("{s:i, s:i}",
        "current", ch->mana, "max", ch->max_mana));
    json_object_set_new(vitals, "move", json_pack("{s:i, s:i}",
        "current", ch->move, "max", ch->max_move));
    json_object_set_new(basic, "vitals", vitals);

    // Other basic data
    json_object_set_new(basic, "alignment", json_integer(ch->alignment));
    json_object_set_new(basic, "gold", json_integer(ch->gold));
    json_object_set_new(basic, "silver", json_integer(ch->silver));
    json_object_set_new(basic, "experience", json_integer(ch->exp));

    // *** CRITICAL MISSING FIELDS - Combat Stats ***
    json_object_set_new(basic, "hitroll", json_integer(ch->hitroll));
    json_object_set_new(basic, "damroll", json_integer(ch->damroll));
    json_object_set_new(basic, "saving_throw", json_integer(ch->saving_throw));

    // Armor class (AC) for pierce/bash/slash/magic
    json_t *armor = json_array();
    json_array_append_new(armor, json_integer(ch->armour[0])); // pierce
    json_array_append_new(armor, json_integer(ch->armour[1])); // bash
    json_array_append_new(armor, json_integer(ch->armour[2])); // slash
    json_array_append_new(armor, json_integer(ch->armour[3])); // magic
    json_object_set_new(basic, "armor", armor);

    // Practice and train sessions
    json_object_set_new(basic, "practice", json_integer(ch->practice));
    json_object_set_new(basic, "train", json_integer(ch->train));
    json_object_set_new(basic, "wimpy", json_integer(ch->wimpy));

    // Position state (standing, sitting, sleeping, etc.)
    json_object_set_new(basic, "position_state", json_integer(ch->position == POS_FIGHTING ? POS_STANDING : ch->position));

    // *** AFFECT FLAGS - Both current and permanent (human-readable) ***
    json_object_set_new(basic, "affected_by_flags", json_flags_serialize(ch->affected_by[0], affect_flags));
    json_object_set_new(basic, "affected_by2_flags", json_flags_serialize(ch->affected_by[1], affect2_flags));
    json_object_set_new(basic, "affected_by_perm_flags", json_flags_serialize(ch->affected_by_perm[0], affect_flags));
    json_object_set_new(basic, "affected_by_perm2_flags", json_flags_serialize(ch->affected_by_perm[1], affect2_flags));

    // Also save numeric for backward compatibility
    json_object_set_new(basic, "affected_by", json_integer(ch->affected_by[0]));
    json_object_set_new(basic, "affected_by2", json_integer(ch->affected_by[1]));
    json_object_set_new(basic, "affected_by_perm", json_integer(ch->affected_by_perm[0]));
    json_object_set_new(basic, "affected_by_perm2", json_integer(ch->affected_by_perm[1]));

    // *** RESISTANCE/IMMUNITY/VULNERABILITY FLAGS (human-readable) ***
    json_object_set_new(basic, "imm_flags_names", json_flags_serialize(ch->imm_flags, imm_flags));
    json_object_set_new(basic, "imm_flags_perm_names", json_flags_serialize(ch->imm_flags_perm, imm_flags));
    json_object_set_new(basic, "res_flags_names", json_flags_serialize(ch->res_flags, res_flags));
    json_object_set_new(basic, "res_flags_perm_names", json_flags_serialize(ch->res_flags_perm, res_flags));
    json_object_set_new(basic, "vuln_flags_names", json_flags_serialize(ch->vuln_flags, vuln_flags));
    json_object_set_new(basic, "vuln_flags_perm_names", json_flags_serialize(ch->vuln_flags_perm, vuln_flags));

    // Also save numeric for backward compatibility
    json_object_set_new(basic, "imm_flags", json_integer(ch->imm_flags));
    json_object_set_new(basic, "imm_flags_perm", json_integer(ch->imm_flags_perm));
    json_object_set_new(basic, "res_flags", json_integer(ch->res_flags));
    json_object_set_new(basic, "res_flags_perm", json_integer(ch->res_flags_perm));
    json_object_set_new(basic, "vuln_flags", json_integer(ch->vuln_flags));
    json_object_set_new(basic, "vuln_flags_perm", json_integer(ch->vuln_flags_perm));

    // Lost body parts (human-readable)
    if (ch->lostparts != 0) {
        json_object_set_new(basic, "lostparts_names", json_flags_serialize(ch->lostparts, part_flags));
        json_object_set_new(basic, "lostparts", json_integer(ch->lostparts)); // numeric for backward compatibility
    }

    // *** COUNTERS - Deaths, Kills, Quest Points ***
    json_object_set_new(basic, "deaths", json_integer(ch->deaths));
    json_object_set_new(basic, "arena_deaths", json_integer(ch->arena_deaths));
    json_object_set_new(basic, "player_deaths", json_integer(ch->player_deaths));
    json_object_set_new(basic, "cpk_deaths", json_integer(ch->cpk_deaths));
    json_object_set_new(basic, "wars_won", json_integer(ch->wars_won));
    json_object_set_new(basic, "arena_kills", json_integer(ch->arena_kills));
    json_object_set_new(basic, "player_kills", json_integer(ch->player_kills));
    json_object_set_new(basic, "cpk_kills", json_integer(ch->cpk_kills));
    json_object_set_new(basic, "monster_kills", json_integer(ch->monster_kills));

    // Quest data
    json_object_set_new(basic, "questpoints", json_integer(ch->questpoints));
    json_object_set_new(basic, "nextquest", json_integer(ch->nextquest));
    json_object_set_new(basic, "deitypoints", json_integer(ch->deitypoints));
    json_object_set_new(basic, "pneuma", json_integer(ch->pneuma));
    json_object_set_new(basic, "home", json_integer(ch->home));
    json_object_set_new(basic, "manastore", json_integer(ch->manastore));
    json_object_set_new(basic, "locker_tier", json_integer(ch->locker_tier));

    if (ch->pcdata) {
        json_object_set_new(basic, "title", json_string(ch->pcdata->title ? ch->pcdata->title : ""));
        json_object_set_new(basic, "description", json_string(ch->description ? ch->description : ""));
        // Save playtime in seconds (raw value, not divided by 3600)
        json_object_set_new(basic, "played", json_integer(ch->played + (int)(current_time - ch->logon)));
        json_object_set_new(basic, "last_login", json_integer(ch->pcdata->last_login));
        json_object_set_new(basic, "last_logoff", json_integer(ch->pcdata->last_logoff));

        // *** BANK BALANCE - CRITICAL! ***
        json_object_set_new(basic, "bankbalance", json_integer(ch->pcdata->bankbalance));

        // *** PC_DATA FIELDS ***
        json_object_set_new(basic, "true_sex", json_integer(ch->pcdata->true_sex));
        json_object_set_new(basic, "last_level", json_integer(ch->pcdata->last_level));
        json_object_set_new(basic, "quests_completed", json_integer(ch->pcdata->quests_completed));
        json_object_set_new(basic, "security", json_integer(ch->pcdata->security));
        json_object_set_new(basic, "staff_rank", json_integer(ch->pcdata->staff_rank));
        json_object_set_new(basic, "class_current", json_integer(ch->pcdata->class_current));
        json_object_set_new(basic, "sub_class_current", json_integer(ch->pcdata->sub_class_current));
        json_object_set_new(basic, "challenge_delay", json_integer(ch->pcdata->challenge_delay));
        json_object_set_new(basic, "need_change_pw", json_integer(ch->pcdata->need_change_pw));
        json_object_set_new(basic, "danger_range", json_integer(ch->pcdata->danger_range));

        // Note read timestamps
        json_object_set_new(basic, "last_note", json_integer(ch->pcdata->last_note));
        json_object_set_new(basic, "last_idea", json_integer(ch->pcdata->last_idea));
        json_object_set_new(basic, "last_penalty", json_integer(ch->pcdata->last_penalty));
        json_object_set_new(basic, "last_news", json_integer(ch->pcdata->last_news));
        json_object_set_new(basic, "last_changes", json_integer(ch->pcdata->last_changes));
        json_object_set_new(basic, "last_project_inquiry", json_integer(ch->pcdata->last_project_inquiry));

        // Pre-level vitals snapshot
        json_object_set_new(basic, "hit_before", json_integer(ch->pcdata->hit_before));
        json_object_set_new(basic, "mana_before", json_integer(ch->pcdata->mana_before));
        json_object_set_new(basic, "move_before", json_integer(ch->pcdata->move_before));

        // Last area string
        if (ch->pcdata->last_area && ch->pcdata->last_area[0] != '\0') {
            json_object_set_new(basic, "last_area", json_string(ch->pcdata->last_area));
        }

        // AFK message
        if (IS_SET(ch->comm, COMM_AFK) && ch->pcdata->afk_message != NULL) {
            json_object_set_new(basic, "afk_message", json_string(ch->pcdata->afk_message));
        }

        // Player flag
        if (ch->pcdata->flag && ch->pcdata->flag[0] != '\0') {
            json_object_set_new(basic, "player_flag", json_string(ch->pcdata->flag));
        }

        // *** USER PREFERENCES ***
        json_object_set_new(basic, "scroll_lines", json_integer(ch->lines)); // Page length
        json_object_set_new(basic, "prompt", json_string(ch->prompt ? ch->prompt : ""));
        json_object_set_new(basic, "verb_preference", json_integer(ch->verb_preference));

        // Character preference overrides
        if (ch->pcdata->preferences) {
            json_object_set_new(basic, "preference_overrides",
                prefs_to_json(ch->pcdata->preferences));
        }

        // Pronouns
        if (ch->pronoun_he_she && ch->pronoun_he_she[0] != '\0') {
            json_object_set_new(basic, "pronoun_he_she", json_string(ch->pronoun_he_she));
        }
        if (ch->pronoun_him_her && ch->pronoun_him_her[0] != '\0') {
            json_object_set_new(basic, "pronoun_him_her", json_string(ch->pronoun_him_her));
        }
        if (ch->pronoun_his_her && ch->pronoun_his_her[0] != '\0') {
            json_object_set_new(basic, "pronoun_his_her", json_string(ch->pronoun_his_her));
        }
        if (ch->pronoun_his_hers && ch->pronoun_his_hers[0] != '\0') {
            json_object_set_new(basic, "pronoun_his_hers", json_string(ch->pronoun_his_hers));
        }
        if (ch->pronoun_himself_herself && ch->pronoun_himself_herself[0] != '\0') {
            json_object_set_new(basic, "pronoun_himself_herself", json_string(ch->pronoun_himself_herself));
        }

        // Deletion status
        if (ch->deleted) {
            json_object_set_new(basic, "deleted", json_boolean(ch->deleted));
            json_object_set_new(basic, "delete_time", json_integer(ch->delete_time));
        }

        // Locker rent
        json_object_set_new(basic, "locker_rent", json_integer(ch->locker_rent));

        // Permanent vitals
        json_object_set_new(basic, "perm_hit", json_integer(ch->pcdata->perm_hit));
        json_object_set_new(basic, "perm_mana", json_integer(ch->pcdata->perm_mana));
        json_object_set_new(basic, "perm_move", json_integer(ch->pcdata->perm_move));

        // Condition (hunger, thirst, drunk, food)
        json_t *condition = json_array();
        for (int i = 0; i < 4; i++) {
            json_array_append_new(condition, json_integer(ch->pcdata->condition[i]));
        }
        json_object_set_new(basic, "condition", condition);

        // Toxins - use toxin NAME as key (robust against ID changes)
        json_t *toxins = json_object();
        for (int i = 0; i < MAX_TOXIN; i++) {
            if (ch->toxin[i] > 0 && toxin_table[i].name) {
                json_t *toxin_data = json_object();
                json_object_set_new(toxin_data, "level", json_integer(ch->toxin[i]));
                // Use toxin name as key (robust against ID changes)
                json_object_set_new(toxins, toxin_table[i].name, toxin_data);
            }
        }
        if (json_object_size(toxins) > 0) {
            json_object_set_new(basic, "toxins", toxins);
        } else {
            json_decref(toxins);
        }

        // Immortal-specific fields
        if (ch->invis_level > 0) {
            json_object_set_new(basic, "invis_level", json_integer(ch->invis_level));
        }
        if (ch->incog_level > 0) {
            json_object_set_new(basic, "incog_level", json_integer(ch->incog_level));
        }
        if (ch->wiznet != 0) {
            json_object_set_new(basic, "wiznet", json_integer(ch->wiznet));
        }

        // Church membership
        if (ch->church) {
            json_object_set_new(basic, "church", json_string(ch->church->name));
        }

        // Immortal imm_flag (custom who-tag)
        if (ch->pcdata->immortal && ch->pcdata->immortal->imm_flag != NULL &&
            str_cmp(ch->pcdata->immortal->imm_flag, "none")) {
            json_object_set_new(basic, "imm_flag", json_string(ch->pcdata->immortal->imm_flag));
        }

        // Character-level auth data (for unlinked characters or mid-migration)
        {
            bool is_unlinked = IS_NULLSTR(ch->pcdata->account_name);
            bool has_auth_data = !IS_NULLSTR(ch->pcdata->pwd) || ch->pcdata->mfa_enabled;

            if (is_unlinked || has_auth_data) {
                if (!IS_NULLSTR(ch->pcdata->pwd)) {
                    json_object_set_new(basic, "char_password", json_string(ch->pcdata->pwd));
                    json_object_set_new(basic, "char_password_version", json_integer(ch->pcdata->pwd_vers));
                }
                if (!IS_NULLSTR(ch->pcdata->old_pwd)) {
                    json_object_set_new(basic, "char_old_password", json_string(ch->pcdata->old_pwd));
                }
                if (!IS_NULLSTR(ch->pcdata->reset_code)) {
                    json_object_set_new(basic, "char_reset_code", json_string(ch->pcdata->reset_code));
                    json_object_set_new(basic, "char_reset_time", json_integer(ch->pcdata->reset_time));
                    json_object_set_new(basic, "char_reset_state", json_integer(ch->pcdata->reset_state));
                }
                if (!IS_NULLSTR(ch->pcdata->mfa_key)) {
                    json_object_set_new(basic, "char_mfa_key", json_string(ch->pcdata->mfa_key));
                }
                if (ch->pcdata->mfa_enabled) {
                    json_object_set_new(basic, "char_mfa_enabled", json_boolean(true));
                    if (!IS_NULLSTR(ch->pcdata->mfa_pending_key)) {
                        json_object_set_new(basic, "char_mfa_pending_key", json_string(ch->pcdata->mfa_pending_key));
                    }
                    json_object_set_new(basic, "char_mfa_pending", json_boolean(ch->pcdata->mfa_pending));

                    json_t *recovery_arr = json_array();
                    for (int i = 0; i < MFA_RECOVERY_CODES; i++) {
                        if (ch->pcdata->recovery_codes[i]) {
                            json_t *rc = json_object();
                            json_object_set_new(rc, "code", json_string(ch->pcdata->recovery_codes[i]));
                            json_object_set_new(rc, "used", json_boolean(ch->pcdata->recovery_used[i]));
                            json_array_append_new(recovery_arr, rc);
                        }
                    }
                    if (json_array_size(recovery_arr) > 0) {
                        json_object_set_new(basic, "char_recovery_codes", recovery_arr);
                    } else {
                        json_decref(recovery_arr);
                    }
                }
            }
        }

        // Character-level email fields (for unlinked characters or mid-migration)
        if (!IS_NULLSTR(ch->pcdata->email)) {
            json_object_set_new(basic, "char_email", json_string(ch->pcdata->email));
            json_object_set_new(basic, "char_email_verified", json_boolean(ch->pcdata->email_verified));
        }
        if (!IS_NULLSTR(ch->pcdata->pending_email)) {
            json_object_set_new(basic, "char_pending_email", json_string(ch->pcdata->pending_email));
        }
        if (!IS_NULLSTR(ch->pcdata->email_verification_code)) {
            json_object_set_new(basic, "char_email_verification_code", json_string(ch->pcdata->email_verification_code));
            json_object_set_new(basic, "char_email_verification_time", json_integer(ch->pcdata->email_verification_time));
        }
        if (ch->pcdata->email_verification_last_sent > 0) {
            json_object_set_new(basic, "char_email_verification_last_sent", json_integer(ch->pcdata->email_verification_last_sent));
        }

        // Ignore list
        if (ch->pcdata->ignoring) {
            json_t *ignore_arr = json_array();
            IGNORE_DATA *ignore;
            for (ignore = ch->pcdata->ignoring; ignore; ignore = ignore->next) {
                json_t *ignore_obj = json_object();
                json_object_set_new(ignore_obj, "name", json_string(ignore->name));
                if (ignore->reason && ignore->reason[0] != '\0') {
                    json_object_set_new(ignore_obj, "reason", json_string(ignore->reason));
                }
                json_array_append_new(ignore_arr, ignore_obj);
            }
            json_object_set_new(basic, "ignoring", ignore_arr);
        }

        // Vis-to list (selective visibility)
        if (ch->pcdata->vis_to_people) {
            json_t *visto_arr = json_array();
            STRING_DATA *string;
            for (string = ch->pcdata->vis_to_people; string; string = string->next) {
                json_array_append_new(visto_arr, json_string(string->string));
            }
            json_object_set_new(basic, "vis_to", visto_arr);
        }

        // Quiet-to list
        if (ch->pcdata->quiet_people) {
            json_t *quiet_arr = json_array();
            STRING_DATA *string;
            for (string = ch->pcdata->quiet_people; string; string = string->next) {
                json_array_append_new(quiet_arr, json_string(string->string));
            }
            json_object_set_new(basic, "quiet_to", quiet_arr);
        }

        // Room before arena
        if (location_isset(&ch->pcdata->room_before_arena)) {
            json_t *rba = json_object();
            json_object_set_new(rba, "wuid", json_integer(ch->pcdata->room_before_arena.wuid));
            json_t *rba_id = json_array();
            json_array_append_new(rba_id, json_integer(ch->pcdata->room_before_arena.id[0]));
            json_array_append_new(rba_id, json_integer(ch->pcdata->room_before_arena.id[1]));
            json_array_append_new(rba_id, json_integer(ch->pcdata->room_before_arena.id[2]));
            json_object_set_new(rba, "id", rba_id);
            json_object_set_new(basic, "room_before_arena", rba);
        }

        // Granted commands
        if (ch->pcdata->commands) {
            json_t *cmd_arr = json_array();
            COMMAND_DATA *cmd;
            for (cmd = ch->pcdata->commands; cmd; cmd = cmd->next) {
                json_array_append_new(cmd_arr, json_string(cmd->name));
            }
            json_object_set_new(basic, "granted_commands", cmd_arr);
        }
    }

    // Death state
    if (ch->dead) {
        json_object_set_new(basic, "dead", json_boolean(true));
        json_object_set_new(basic, "death_time_left", json_integer(ch->time_left_death));
    }

    // Shifted form (werewolf/slayer)
    if (ch->shifted != SHIFTED_NONE) {
        json_object_set_new(basic, "shifted", json_integer(ch->shifted));
    }

    // Before-social room marker
    if (location_isset(&ch->before_social)) {
        json_t *bs = json_object();
        json_object_set_new(bs, "wuid", json_integer(ch->before_social.wuid));
        json_t *bs_id = json_array();
        json_array_append_new(bs_id, json_integer(ch->before_social.id[0]));
        json_array_append_new(bs_id, json_integer(ch->before_social.id[1]));
        json_array_append_new(bs_id, json_integer(ch->before_social.id[2]));
        json_object_set_new(bs, "id", bs_id);
        json_object_set_new(basic, "before_social", bs);
    }

    // Position (room location or wilderness)
    json_t *position = json_object();
    
    if (ch->in_wilds) {
        // Character is in wilderness - save coordinates
        json_object_set_new(position, "type", json_string("wilderness"));
        json_object_set_new(position, "x", json_integer(ch->in_room ? ch->in_room->x : ch->at_wilds_x));
        json_object_set_new(position, "y", json_integer(ch->in_room ? ch->in_room->y : ch->at_wilds_y));
        json_object_set_new(position, "area_uid", json_integer(ch->in_wilds->pArea->uid));
        json_object_set_new(position, "wilds_uid", json_integer(ch->in_wilds->uid));
    } else if (ch->was_in_wilds) {
        // Fallback to was_in_wilds if in_wilds not set
        json_object_set_new(position, "type", json_string("wilderness"));
        json_object_set_new(position, "x", json_integer(ch->was_at_wilds_x));
        json_object_set_new(position, "y", json_integer(ch->was_at_wilds_y));
        json_object_set_new(position, "area_uid", json_integer(ch->was_in_wilds->pArea->uid));
        json_object_set_new(position, "wilds_uid", json_integer(ch->was_in_wilds->uid));
    } else if (ch->in_room) {
        // Character is in regular room - use widevnum format
        json_object_set_new(position, "type", json_string("room"));

        // If player is in a dungeon/instance room, save the dungeon entry room
        // instead of the instanced clone room (which won't exist on reload)
        ROOM_INDEX_DATA *save_room = ch->in_room;
        if (IS_VALID(ch->in_room->instance_section)
            && IS_VALID(ch->in_room->instance_section->instance))
        {
            INSTANCE *inst = ch->in_room->instance_section->instance;
            if (inst->dungeon && inst->dungeon->entry_room)
                save_room = inst->dungeon->entry_room;
            else if (inst->entrance)
                save_room = inst->entrance;
        }
        json_object_set_new(position, "room_vnum", json_string(widevnum_string_room(save_room, NULL)));
    } else {
        // No position set - will use recall
        json_decref(position);
        position = NULL;
    }
    
    if (position) {
        json_object_set_new(basic, "position", position);
    }

    // Recall location
    if (ch->recall.id[0] != 0 || ch->recall.wuid != 0) {
        json_t *recall = json_object();
        if (ch->recall.wuid) {
            json_object_set_new(recall, "wuid", json_integer(ch->recall.wuid));
        }
        json_t *recall_id = json_array();
        json_array_append_new(recall_id, json_integer(ch->recall.id[0]));
        json_array_append_new(recall_id, json_integer(ch->recall.id[1]));
        json_array_append_new(recall_id, json_integer(ch->recall.id[2]));
        json_object_set_new(recall, "id", recall_id);
        json_object_set_new(basic, "recall", recall);
    }

    // Quest data (if currently questing)
    if (IS_QUESTING(ch) && ch->quest) {
        WNUM questgiver_wnum = ch->quest->questgiver_wnum;
        WNUM questreceiver_wnum = ch->quest->questreceiver_wnum;

        if (!questgiver_wnum.pArea && ch->quest->questgiver_load.vnum > 0) {
            AREA_DATA *fallback = NULL;
            WNUM wnum;
            if (resolve_widevnum(ch->quest->questgiver_load.vnum, NULL, &wnum))
                fallback = wnum.pArea;
            if (!fallback) fallback = get_system_area_fallback();
            resolve_wnum_load(&ch->quest->questgiver_load, &questgiver_wnum, fallback);
        }

        if (!questreceiver_wnum.pArea && ch->quest->questreceiver_load.vnum > 0) {
            AREA_DATA *fallback = NULL;
            WNUM wnum;
            if (resolve_widevnum(ch->quest->questreceiver_load.vnum, NULL, &wnum))
                fallback = wnum.pArea;
            if (!fallback) fallback = get_system_area_fallback();
            resolve_wnum_load(&ch->quest->questreceiver_load, &questreceiver_wnum, fallback);
        }

        json_t *quest = json_object();
        json_object_set_new(quest, "questgiver_type", json_integer(ch->quest->questgiver_type));
        json_object_set_new(quest, "questgiver", json_string(widevnum_string_wnum(questgiver_wnum, NULL)));
        json_object_set_new(quest, "questreceiver_type", json_integer(ch->quest->questreceiver_type));
        json_object_set_new(quest, "questreceiver", json_string(widevnum_string_wnum(questreceiver_wnum, NULL)));
        json_object_set_new(quest, "countdown", json_integer(ch->countdown));
        json_object_set_new(quest, "msg_complete", json_boolean(ch->quest->msg_complete));
        json_object_set_new(quest, "scripted", json_boolean(ch->quest->scripted));

        // Quest parts
        if (ch->quest->parts) {
            json_t *parts_array = json_array();
            QUEST_PART_DATA *part;
            for (part = ch->quest->parts; part; part = part->next) {
                json_t *part_obj = json_object();
                json_object_set_new(part_obj, "index", json_integer(part->index));
                json_object_set_new(part_obj, "minutes", json_integer(part->minutes));
                if (part->obj_load.vnum > 0 && !part->obj_wnum.pArea) {
                    AREA_DATA *fallback = NULL;
                    WNUM wnum;
                    if (resolve_widevnum(part->obj_load.vnum, NULL, &wnum))
                        fallback = wnum.pArea;
                    if (!fallback) fallback = get_system_area_fallback();
                    resolve_wnum_load(&part->obj_load, &part->obj_wnum, fallback);
                }
                if (part->mob_load.vnum > 0 && !part->mob_wnum.pArea) {
                    AREA_DATA *fallback = NULL;
                    WNUM wnum;
                    if (resolve_widevnum(part->mob_load.vnum, NULL, &wnum))
                        fallback = wnum.pArea;
                    if (!fallback) fallback = get_system_area_fallback();
                    resolve_wnum_load(&part->mob_load, &part->mob_wnum, fallback);
                }
                if (part->room_load.vnum > 0 && !part->room_wnum.pArea) {
                    AREA_DATA *fallback = NULL;
                    WNUM wnum;
                    if (resolve_widevnum(part->room_load.vnum, NULL, &wnum))
                        fallback = wnum.pArea;
                    if (!fallback) fallback = get_system_area_fallback();
                    resolve_wnum_load(&part->room_load, &part->room_wnum, fallback);
                }
                if (part->obj_sac_load.vnum > 0 && !part->obj_sac_wnum.pArea) {
                    AREA_DATA *fallback = NULL;
                    WNUM wnum;
                    if (resolve_widevnum(part->obj_sac_load.vnum, NULL, &wnum))
                        fallback = wnum.pArea;
                    if (!fallback) fallback = get_system_area_fallback();
                    resolve_wnum_load(&part->obj_sac_load, &part->obj_sac_wnum, fallback);
                }
                if (part->mob_rescue_load.vnum > 0 && !part->mob_rescue_wnum.pArea) {
                    AREA_DATA *fallback = NULL;
                    WNUM wnum;
                    if (resolve_widevnum(part->mob_rescue_load.vnum, NULL, &wnum))
                        fallback = wnum.pArea;
                    if (!fallback) fallback = get_system_area_fallback();
                    resolve_wnum_load(&part->mob_rescue_load, &part->mob_rescue_wnum, fallback);
                }

                if (part->obj_load.vnum != -1)
                    json_object_set_new(part_obj, "obj", json_string(widevnum_string_wnum(part->obj_wnum, NULL)));
                else
                    json_object_set_new(part_obj, "obj", json_integer(-1));
                if (part->mob_load.vnum != -1)
                    json_object_set_new(part_obj, "mob", json_string(widevnum_string_wnum(part->mob_wnum, NULL)));
                else
                    json_object_set_new(part_obj, "mob", json_integer(-1));
                if (part->room_load.vnum != -1)
                    json_object_set_new(part_obj, "room", json_string(widevnum_string_wnum(part->room_wnum, NULL)));
                else
                    json_object_set_new(part_obj, "room", json_integer(-1));
                if (part->obj_sac_load.vnum != -1)
                    json_object_set_new(part_obj, "obj_sac", json_string(widevnum_string_wnum(part->obj_sac_wnum, NULL)));
                else
                    json_object_set_new(part_obj, "obj_sac", json_integer(-1));
                if (part->mob_rescue_load.vnum != -1)
                    json_object_set_new(part_obj, "mob_rescue", json_string(widevnum_string_wnum(part->mob_rescue_wnum, NULL)));
                else
                    json_object_set_new(part_obj, "mob_rescue", json_integer(-1));
                json_object_set_new(part_obj, "custom_task", json_boolean(part->custom_task));
                json_object_set_new(part_obj, "complete", json_boolean(part->complete));
                if (part->description) {
                    json_object_set_new(part_obj, "description", json_string(part->description));
                }
                // For pickup quests, save the object vnum and room vnum - use widevnum format
                if (part->pObj && part->pObj->in_room && !part->complete) {
                    json_object_set_new(part_obj, "pobj_vnum", json_string(widevnum_string_object(part->pObj->pIndexData, NULL)));
                    json_object_set_new(part_obj, "pobj_room", json_string(widevnum_string_room(part->pObj->in_room, NULL)));
                }
                json_array_append_new(parts_array, part_obj);
            }
            json_object_set_new(quest, "parts", parts_array);
        }

        json_object_set_new(basic, "quest", quest);
    }

    return basic;
}

json_t *char_to_json(CHAR_DATA *ch)
{
    json_t *root, *inventory, *equipment, *locker, *skills, *groups, *affects, *tokens, *aliases;

    if (!ch || IS_NPC(ch)) {
        return NULL;
    }

    root = json_object();

    // Metadata section
    json_object_set_new(root, "metadata", char_metadata_to_json(ch));

    // Character basic info section
    json_object_set_new(root, "character", char_basic_to_json(ch));

    // Inventory section
    inventory = inventory_to_json(ch);
    if (json_array_size(inventory) > 0) {
        json_object_set_new(root, "inventory", inventory);
    } else {
        json_decref(inventory);
    }

    // Equipment section
    equipment = equipment_to_json(ch);
    if (json_array_size(equipment) > 0) {
        json_object_set_new(root, "equipment", equipment);
    } else {
        json_decref(equipment);
    }

    // Locker section
    locker = locker_to_json(ch);
    if (json_array_size(locker) > 0) {
        json_object_set_new(root, "locker", locker);
    } else {
        json_decref(locker);
    }

    // Skills section
    skills = skills_to_json(ch);
    if (json_object_size(skills) > 0) {
        json_object_set_new(root, "skills", skills);
    } else {
        json_decref(skills);
    }

    // Skill groups section
    groups = groups_to_json(ch);
    if (json_array_size(groups) > 0) {
        json_object_set_new(root, "skill_groups", groups);
    } else {
        json_decref(groups);
    }

    // Tokens section
    tokens = tokens_to_json(ch);
    if (json_array_size(tokens) > 0) {
        json_object_set_new(root, "tokens", tokens);
    } else {
        json_decref(tokens);
    }

    // Aliases section
    aliases = aliases_to_json(ch);
    if (json_array_size(aliases) > 0) {
        json_object_set_new(root, "aliases", aliases);
    } else {
        json_decref(aliases);
    }

    // Affects section
    affects = affects_to_json(ch);
    if (json_array_size(affects) > 0) {
        json_object_set_new(root, "affects", affects);
    } else {
        json_decref(affects);
    }

    // Songs section (bard songs learned)
    if (ch->pcdata) {
        json_t *songs = json_array();
        for (int sn = 0; sn < MAX_SONGS; sn++) {
            if (ch->pcdata->songs_learned[sn]) {
                SONG_DATA *sd = song_lookup_uid(sn);
                if (sd) {
                    json_array_append_new(songs, json_string(sd->name));
                }
            }
        }
        if (json_array_size(songs) > 0) {
            json_object_set_new(root, "songs", songs);
        } else {
            json_decref(songs);
        }
    }

    // Ships section (ship ownership by ID pairs)
    if (ch->pcdata && ch->pcdata->ships && list_size(ch->pcdata->ships) > 0) {
        json_t *ships_arr = json_array();
        ITERATOR sit;
        SHIP_DATA *ship;
        iterator_start(&sit, ch->pcdata->ships);
        while ((ship = (SHIP_DATA *)iterator_nextdata(&sit))) {
            json_t *ship_id = json_array();
            json_array_append_new(ship_id, json_integer(ship->id[0]));
            json_array_append_new(ship_id, json_integer(ship->id[1]));
            json_array_append_new(ships_arr, ship_id);
        }
        iterator_stop(&sit);
        json_object_set_new(root, "ships", ships_arr);
    }

    // Unlocked areas section (area UIDs)
    if (ch->pcdata && ch->pcdata->unlocked_areas && list_size(ch->pcdata->unlocked_areas) > 0) {
        json_t *unlocked = json_array();
        ITERATOR uait;
        AREA_DATA *unlocked_area;
        iterator_start(&uait, ch->pcdata->unlocked_areas);
        while ((unlocked_area = (AREA_DATA *)iterator_nextdata(&uait))) {
            json_array_append_new(unlocked, json_integer(unlocked_area->uid));
        }
        iterator_stop(&uait);
        json_object_set_new(root, "unlocked_areas", unlocked);
    }

    // Personal trait overrides (only saves explicitly set traits)
    if (ch->pcdata) {
        json_t *traits = (json_t *)char_save_traits_json(ch);
        if (traits)
            json_object_set_new(root, "traits", traits);
    }

    // New class system class levels
    if (ch->pcdata && ch->pcdata->classes && list_size(ch->pcdata->classes) > 0) {
        json_t *class_levels = json_array();
        ITERATOR clit;
        CLASS_LEVEL *cl;
        iterator_start(&clit, ch->pcdata->classes);
        while ((cl = (CLASS_LEVEL *)iterator_nextdata(&clit))) {
            if (cl->clazz) {
                json_t *entry = json_object();
                json_object_set_new(entry, "uid", json_integer(cl->clazz->uid));
                json_object_set_new(entry, "name", json_string(cl->clazz->name));
                json_object_set_new(entry, "level", json_integer(cl->level));
                json_object_set_new(entry, "xp", json_integer(cl->xp));
                if (cl->active_title)
                    json_object_set_new(entry, "active_title", json_string(cl->active_title));
                if (cl->custom_data)
                    json_object_set_new(entry, "custom_data", json_deep_copy(cl->custom_data));
                // Mark current class
                if (ch->pcdata->current_class == cl)
                    json_object_set_new(entry, "current", json_true());
                json_array_append_new(class_levels, entry);
            }
        }
        iterator_stop(&clit);
        json_object_set_new(root, "class_levels", class_levels);

        if (ch->pcdata->pending_free_levels > 0)
            json_object_set_new(root, "pending_free_levels",
                                json_integer(ch->pcdata->pending_free_levels));
    }

    return root;
}

/***************************************************************************
 * File I/O                                                                *
 ***************************************************************************/

static bool backup_old_pfile(const char *filename, const char *char_name)
{
    char backup_path[512];
    char backup_dir[256];
    FILE *src, *dst;
    char buffer[4096];
    size_t bytes;

    // Check if file exists and is old format
    if (!json_is_json_file(filename)) {
        // It's an old pfile, back it up
        json_get_backup_path(char_name, backup_path, sizeof(backup_path));

        // Create backup directory
        snprintf(backup_dir, sizeof(backup_dir), "%s%c.old",
                PLAYER_DIR, tolower(char_name[0]));
        mkdir(backup_dir, 0755);

        // Copy file to backup
        src = fopen(filename, "r");
        if (!src) {
            return false; // File doesn't exist, no backup needed
        }

        dst = fopen(backup_path, "w");
        if (!dst) {
            fclose(src);
            log_stringf("json_write_char: Failed to create backup %s", backup_path);
            return false;
        }

        while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            fwrite(buffer, 1, bytes, dst);
        }

        fclose(src);
        fclose(dst);

        log_stringf("JSON: Backed up old pfile %s → %s", filename, backup_path);
    }

    return true;
}

bool json_write_char(CHAR_DATA *ch, const char *filename)
{
    json_t *root;
    char tmp_filename[512];
    int result;

    if (!ch || IS_NPC(ch)) {
        return false;
    }

    // Ensure directory exists
    json_ensure_char_dir(ch->name);

    // Backup old pfile before migrating to JSON
    backup_old_pfile(filename, ch->name);

    // Serialize to JSON
    root = char_to_json(ch);
    if (!root) {
        log_stringf("json_write_char: Failed to serialize %s", ch->name);
        return false;
    }

    // Write to temporary file first (atomic write)
    snprintf(tmp_filename, sizeof(tmp_filename), "%s.tmp", filename);
    result = json_dump_file(root, tmp_filename, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
    json_decref(root);

    if (result != 0) {
        log_stringf("json_write_char: Failed to write %s", tmp_filename);
        return false;
    }

    // Atomic rename
    if (rename(tmp_filename, filename) != 0) {
        log_stringf("json_write_char: Failed to rename %s to %s", tmp_filename, filename);
        unlink(tmp_filename);
        return false;
    }

    log_stringf("JSON: Saved character %s to %s", ch->name, filename);
    return true;
}

// Read ONLY the lightweight character info (for account menu display)
// Does NOT load inventory, equipment, skills, affects, etc.
// Returns CHAR_INFO_CACHE that must be freed with free_char_info_cache()
CHAR_INFO_CACHE *json_read_char_info_lightweight(const char *filename)
{
    json_t *root, *character, *metadata;
    json_error_t error;
    CHAR_INFO_CACHE *info;
    const char *str;
    json_t *value;

    // Load JSON file
    root = json_load_file(filename, 0, &error);
    if (!root) {
        log_stringf("json_read_char_info_lightweight: Failed to parse %s: %s", filename, error.text);
        return NULL;
    }

    // Create info cache structure
    info = (CHAR_INFO_CACHE *)calloc(1, sizeof(CHAR_INFO_CACHE));
    if (!info) {
        json_decref(root);
        return NULL;
    }

    // Read metadata
    metadata = json_object_get(root, "metadata");
    if (metadata) {
        value = json_object_get(metadata, "created");
        if (value) {
            info->last_played = json_integer_value(value);
        }
    }

    // Read character section
    character = json_object_get(root, "character");
    if (!character) {
        json_decref(root);
        free(info);
        return NULL;
    }

    // Basic identification
    str = json_string_value(json_object_get(character, "name"));
    info->name = str ? strdup(str) : strdup("Unknown");

    info->level = json_integer_value(json_object_get(character, "level"));
    info->tot_level = json_integer_value(json_object_get(character, "tot_level"));

    str = json_string_value(json_object_get(character, "race"));
    info->race = str ? strdup(str) : strdup("human");

    str = json_string_value(json_object_get(character, "title"));
    info->title = str ? strdup(str) : strdup("");

    // Quick stats
    info->gold = json_integer_value(json_object_get(character, "gold"));
    info->experience = json_integer_value(json_object_get(character, "experience"));

    // Calculate health/mana percentages
    json_t *vitals = json_object_get(character, "vitals");
    if (vitals) {
        json_t *health = json_object_get(vitals, "health");
        if (health) {
            int current = json_integer_value(json_object_get(health, "current"));
            int max = json_integer_value(json_object_get(health, "max"));
            info->health_pct = (max > 0) ? (current * 100 / max) : 100;
        }

        json_t *mana = json_object_get(vitals, "mana");
        if (mana) {
            int current = json_integer_value(json_object_get(mana, "current"));
            int max = json_integer_value(json_object_get(mana, "max"));
            info->mana_pct = (max > 0) ? (current * 100 / max) : 100;
        }
    }

    // Build class list
    json_t *classes_obj = json_object_get(character, "classes");
    if (classes_obj && json_is_object(classes_obj)) {
        // Count classes
        int num_classes = 0;
        if (json_object_get(classes_obj, "mage")) num_classes++;
        if (json_object_get(classes_obj, "cleric")) num_classes++;
        if (json_object_get(classes_obj, "thief")) num_classes++;
        if (json_object_get(classes_obj, "warrior")) num_classes++;

        if (num_classes > 0) {
            info->classes = (char **)calloc(num_classes, sizeof(char *));
            info->num_classes = num_classes;
            int idx = 0;

            if (json_object_get(classes_obj, "mage")) {
                info->classes[idx++] = strdup("mage");
            }
            if (json_object_get(classes_obj, "cleric")) {
                info->classes[idx++] = strdup("cleric");
            }
            if (json_object_get(classes_obj, "thief")) {
                info->classes[idx++] = strdup("thief");
            }
            if (json_object_get(classes_obj, "warrior")) {
                info->classes[idx++] = strdup("warrior");
            }
        }
    }

    json_decref(root);

    log_stringf("JSON: Read lightweight info for %s", info->name);
    return info;
}

bool json_read_char_info(CHAR_DATA *ch, const char *filename)
{
    // This function is deprecated - use json_read_char_info_lightweight instead
    // Keeping for backward compatibility
    CHAR_INFO_CACHE *info = json_read_char_info_lightweight(filename);
    if (info) {
        free_char_info_cache(info);
        return true;
    }
    return false;
}

/***************************************************************************
 * Object Deserialization                                                   *
 ***************************************************************************/

OBJ_DATA *json_to_obj(json_t *json_obj, CHAR_DATA *ch)
{
    OBJ_DATA *obj;
    OBJ_INDEX_DATA *pObjIndex;
    json_t *value, *array_elem;
    long vnum;
    int i;
    size_t index;

    if (!json_obj) {
        return NULL;
    }

    // Get vnum - supports both widevnum string and legacy integer
    value = json_object_get(json_obj, "vnum");
    if (json_is_string(value)) {
        WNUM wnum;
        if (!parse_widevnum((char *)json_string_value(value), NULL, &wnum) || !wnum.pArea) {
            log_stringf("json_to_obj: bad widevnum '%s'", json_string_value(value));
            return NULL;
        }
        pObjIndex = get_obj_index(wnum.pArea, wnum.vnum);
        vnum = wnum.vnum;
    } else {
        vnum = json_integer_value(value);
        AREA_DATA *pArea = NULL;
        WNUM wnum;
        if (resolve_widevnum(vnum, NULL, &wnum))
            pArea = wnum.pArea;
        if (!pArea) pArea = get_system_area_fallback();
        pObjIndex = get_obj_index(pArea, vnum);
    }
    if (!pObjIndex) {
        log_stringf("json_to_obj: object index not found for vnum %ld", vnum);
        return NULL;
    }

    // Create object from prototype
    obj = create_object_noid(pObjIndex, -1, false, false);
    if (!obj) {
        return NULL;
    }

    // Load unique object ID if present
    value = json_object_get(json_obj, "id");
    if (value && json_is_array(value) && json_array_size(value) >= 2) {
        obj->id[0] = json_integer_value(json_array_get(value, 0));
        obj->id[1] = json_integer_value(json_array_get(value, 1));
    }

    // O(1) deduplication check: if an object with this ID is already loaded, skip it
    if (obj->id[0] || obj->id[1]) {
        OBJ_DATA *existing = loaded_obj_hash_find(obj->id[0], obj->id[1]);
        if (existing) {
            log_stringf("json_to_obj: DUPLICATE object vnum=%ld '%s' (id=%lu/%lu) - already loaded as '%s', skipping",
                       vnum, obj->short_descr ? obj->short_descr : "(null)",
                       obj->id[0], obj->id[1],
                       existing->short_descr ? existing->short_descr : "(null)");
            free_obj(obj);
            return NULL;
        }
    }

    // Override fields if present in JSON
    value = json_object_get(json_obj, "name");
    if (value) {
        free_string(obj->name);
        obj->name = str_dup(json_string_value(value));
        log_stringf("json_to_obj: vnum=%ld loaded custom name='%s'", vnum, obj->name);
    } else {
        log_stringf("json_to_obj: vnum=%ld using prototype name='%s'", vnum, obj->name ? obj->name : "(null)");
    }

    value = json_object_get(json_obj, "short_descr");
    if (value) {
        free_string(obj->short_descr);
        obj->short_descr = str_dup(json_string_value(value));
    }

    value = json_object_get(json_obj, "description");
    if (value) {
        free_string(obj->description);
        obj->description = str_dup(json_string_value(value));
    }

    value = json_object_get(json_obj, "full_description");
    if (value) {
        free_string(obj->full_description);
        obj->full_description = str_dup(json_string_value(value));
    }

    // Object state
    value = json_object_get(json_obj, "item_type");
    if (value) {
        obj->item_type = json_integer_value(value);
    }

    value = json_object_get(json_obj, "wear_loc");
    if (value) {
        obj->wear_loc = json_integer_value(value);
    }

    value = json_object_get(json_obj, "level");
    if (value) {
        obj->level = json_integer_value(value);
    }

    value = json_object_get(json_obj, "weight");
    if (value) {
        obj->weight = json_integer_value(value);
    }

    value = json_object_get(json_obj, "condition");
    if (value) {
        obj->condition = json_integer_value(value);
    }

    value = json_object_get(json_obj, "timer");
    if (value) {
        obj->timer = json_integer_value(value);
    }

    value = json_object_get(json_obj, "cost");
    if (value) {
        obj->cost = json_integer_value(value);
    }

    value = json_object_get(json_obj, "fragility");
    if (value) {
        obj->fragility = json_integer_value(value);
    }

    value = json_object_get(json_obj, "times_allowed_fixed");
    if (value) {
        obj->times_allowed_fixed = json_integer_value(value);
    }

    value = json_object_get(json_obj, "times_fixed");
    if (value) {
        obj->times_fixed = json_integer_value(value);
    }

    // Extra flags
    value = json_object_get(json_obj, "extra_flags");
    if (value) obj->extra[0] = json_integer_value(value);
    value = json_object_get(json_obj, "extra2_flags");
    if (value) obj->extra[1] = json_integer_value(value);
    value = json_object_get(json_obj, "extra3_flags");
    if (value) obj->extra[2] = json_integer_value(value);
    value = json_object_get(json_obj, "extra4_flags");
    if (value) obj->extra[3] = json_integer_value(value);

    // Values array
    value = json_object_get(json_obj, "values");
    if (value && json_is_array(value)) {
        for (i = 0; i < 8 && i < (int)json_array_size(value); i++) {
            obj->value[i] = json_integer_value(json_array_get(value, i));
        }
    }

    // Type-specific data (canonical structured representation)
    value = json_object_get(json_obj, "type_data");
    if (value && json_is_object(value)) {
        obj_type_data_from_json(obj, value);
    }

    // Owner
    value = json_object_get(json_obj, "owner");
    if (value) {
        free_string(obj->owner);
        obj->owner = str_dup(json_string_value(value));
    }

    // Material
    value = json_object_get(json_obj, "material");
    if (value) {
        free_string(obj->material);
        obj->material = str_dup(json_string_value(value));
    }

    // Version and persistence
    value = json_object_get(json_obj, "version");
    if (value) obj->version = json_integer_value(value);
    value = json_object_get(json_obj, "persist");
    if (value) obj->persist = json_is_true(value);

    // Wear flags
    value = json_object_get(json_obj, "wear_flags");
    if (value) obj->wear_flags = json_integer_value(value);

    // Last wear location
    value = json_object_get(json_obj, "last_wear_loc");
    if (value) obj->last_wear_loc = json_integer_value(value);

    // Enchantment count
    value = json_object_get(json_obj, "num_enchanted");
    if (value) obj->num_enchanted = json_integer_value(value);

    // Locker flag
    value = json_object_get(json_obj, "locker");
    if (value) obj->locker = json_is_true(value);

    // Old descriptions (pre-customize)
    value = json_object_get(json_obj, "old_name");
    if (value) {
        free_string(obj->old_name);
        obj->old_name = str_dup(json_string_value(value));
    }
    value = json_object_get(json_obj, "old_short_descr");
    if (value) {
        free_string(obj->old_short_descr);
        obj->old_short_descr = str_dup(json_string_value(value));
    }
    value = json_object_get(json_obj, "old_description");
    if (value) {
        free_string(obj->old_description);
        obj->old_description = str_dup(json_string_value(value));
    }
    value = json_object_get(json_obj, "old_full_description");
    if (value) {
        free_string(obj->old_full_description);
        obj->old_full_description = str_dup(json_string_value(value));
    }

    // Loaded by / corpse ownership
    value = json_object_get(json_obj, "loaded_by");
    if (value) {
        free_string(obj->loaded_by);
        obj->loaded_by = str_dup(json_string_value(value));
    }
    value = json_object_get(json_obj, "owner_name");
    if (value) {
        free_string(obj->owner_name);
        obj->owner_name = str_dup(json_string_value(value));
    }
    value = json_object_get(json_obj, "owner_short");
    if (value) {
        free_string(obj->owner_short);
        obj->owner_short = str_dup(json_string_value(value));
    }

    // Permanent extra flags
    value = json_object_get(json_obj, "extra_perm");
    if (value && json_is_array(value)) {
        for (i = 0; i < 4 && i < (int)json_array_size(value); i++) {
            obj->extra_perm[i] = json_integer_value(json_array_get(value, i));
        }
    }

    // Permanent weapon flags
    value = json_object_get(json_obj, "weapon_flags_perm");
    if (value) obj->weapon_flags_perm = json_integer_value(value);

    // Script creation tracking
    value = json_object_get(json_obj, "script_created");
    if (value) obj->script_created = json_is_true(value);
    value = json_object_get(json_obj, "created_script_type");
    if (value) obj->created_script_type = json_integer_value(value);
    value = json_object_get(json_obj, "created_script_vnum");
    if (value) obj->created_script_load.vnum = json_integer_value(value);
    value = json_object_get(json_obj, "creation_time");
    if (value) obj->creation_time = json_integer_value(value);

    // Extra descriptions
    value = json_object_get(json_obj, "extra_descr");
    if (value && json_is_array(value)) {
        json_array_foreach(value, index, array_elem) {
            EXTRA_DESCR_DATA *ed = new_extra_descr();
            ed->keyword = str_dup(json_string_value(json_object_get(array_elem, "keyword")));
            ed->description = str_dup(json_string_value(json_object_get(array_elem, "description")));
            ed->next = obj->extra_descr;
            obj->extra_descr = ed;
        }
    }

    // Affects - use comprehensive format (where, group, custom_name)
    value = json_object_get(json_obj, "affects");
    if (value && json_is_array(value)) {
        json_array_foreach(value, index, array_elem) {
            AFFECT_DATA *paf = json_persist_json_to_affect(array_elem);
            if (paf) {
                paf->next = obj->affected;
                obj->affected = paf;
            }
        }
    }

    // Catalysts
    value = json_object_get(json_obj, "catalysts");
    if (value && json_is_array(value)) {
        json_array_foreach(value, index, array_elem) {
            AFFECT_DATA *paf = json_persist_json_to_affect(array_elem);
            if (paf) {
                paf->next = obj->catalyst;
                obj->catalyst = paf;
            }
        }
    }

    // Spells (for wands, staves, scrolls, potions)
    value = json_object_get(json_obj, "spells");
    if (value && json_is_array(value)) {
        json_array_foreach(value, index, array_elem) {
            SPELL_DATA *spell = new_spell();
            spell->sn = json_integer_value(json_object_get(array_elem, "sn"));
            spell->level = json_integer_value(json_object_get(array_elem, "level"));
            spell->repop = json_integer_value(json_object_get(array_elem, "repop"));
            spell->next = obj->spells;
            obj->spells = spell;
        }
    }

    // Lock state
    value = json_object_get(json_obj, "lock");
    if (value && json_is_object(value)) {
        obj->lock = json_persist_json_to_lock(value);
    }

    // Waypoints
    value = json_object_get(json_obj, "waypoints");
    if (value && json_is_array(value)) {
        if (!obj->waypoints) {
            obj->waypoints = list_create(false);
        }
        json_array_foreach(value, index, array_elem) {
            WAYPOINT_DATA *wp = new_waypoint();
            json_t *wp_val;
            wp_val = json_object_get(array_elem, "w");
            if (wp_val) wp->w = json_integer_value(wp_val);
            wp_val = json_object_get(array_elem, "x");
            if (wp_val) wp->x = json_integer_value(wp_val);
            wp_val = json_object_get(array_elem, "y");
            if (wp_val) wp->y = json_integer_value(wp_val);
            wp_val = json_object_get(array_elem, "name");
            if (wp_val) wp->name = str_dup(json_string_value(wp_val));
            list_appendlink(obj->waypoints, wp);
        }
    }

    // Object tokens
    value = json_object_get(json_obj, "obj_tokens");
    if (value && json_is_array(value)) {
        json_array_foreach(value, index, array_elem) {
            TOKEN_DATA *token = json_persist_json_to_token(array_elem);
            if (token) {
                token_to_obj(token, obj);
            }
        }
    }

    // Script variables
    value = json_object_get(json_obj, "variables");
    if (value && json_is_array(value)) {
        json_persist_json_to_scriptdata(value, &obj->progs);
    }

    // Contained objects (recursive)
    value = json_object_get(json_obj, "contains");
    if (value && json_is_array(value)) {
        json_array_foreach(value, index, array_elem) {
            OBJ_DATA *contained = json_to_obj(array_elem, ch);
            if (contained) {
                obj_to_obj(contained, obj);
            }
        }
    }

    // Add object to loaded_objects tracking list.
    // The object was just created by create_object_noid with add_to_loaded_objs=false,
    // so it is guaranteed to not be in the list yet (no need for list_haslink scan).
    list_appendlink(loaded_objects, obj);
    loaded_obj_hash_add(obj);
    obj->pIndexData->count++;

    // Fix for scrolls/potions that have generic names - derive name from short_descr
    // This matches the VERSION_PLAYER_006 fix in the pfile loading code
    // Cache the reserved index lookups to avoid repeated list traversals
    static OBJ_INDEX_DATA *scroll_index = NULL;
    static OBJ_INDEX_DATA *potion_index = NULL;
    static bool reserved_cached = false;
    if (!reserved_cached) {
        scroll_index = get_reserved_obj_index("obj_scroll");
        potion_index = get_reserved_obj_index("obj_potion");
        reserved_cached = true;
    }
    if (scroll_index && obj->pIndexData == scroll_index) {
        if (!strcmp(obj->name, "scroll")) {
            free_string(obj->name);
            obj->name = short_to_name(obj->short_descr);
            log_stringf("json_to_obj: Fixed scroll name from short_descr, now '%s'", obj->name);
        }
    }
    if (potion_index && obj->pIndexData == potion_index) {
        if (!strcmp(obj->name, "potion")) {
            free_string(obj->name);
            obj->name = short_to_name(obj->short_descr);
            log_stringf("json_to_obj: Fixed potion name from short_descr, now '%s'", obj->name);
        }
    }

    // Assign a unique object ID if not already set
    get_obj_id(obj);

    // Apply object fixes
    // Note: times_allowed_fixed is already correctly loaded from JSON above
    // (or defaults to prototype value from create_object_noid), do NOT overwrite it
    fix_object(obj);

    return obj;
}

/***************************************************************************
 * COMPLETE Character Deserialization - WITH PROPER INITIALIZATION         *
 ***************************************************************************/

// Forward declaration
static bool json_read_char_internal_from_json(CHAR_DATA *ch, json_t *root, bool load_heavy, const char *source_name);

// Internal implementation with load_heavy parameter - loads from file
static bool json_read_char_internal(CHAR_DATA *ch, const char *filename, bool load_heavy)
{
    json_t *root;
    json_error_t error;
    bool result;

    // Load JSON file
    root = json_load_file(filename, 0, &error);
    if (!root) {
        log_stringf("json_read_char: Failed to parse %s: %s", filename, error.text);
        return false;
    }

    // Delegate to the from_json implementation
    result = json_read_char_internal_from_json(ch, root, load_heavy, filename);

    json_decref(root);
    return result;
}

// Internal implementation that works directly on a json_t object
// source_name is used for logging (could be filename or "redis")
static bool json_read_char_internal_from_json(CHAR_DATA *ch, json_t *root, bool load_heavy, const char *source_name)
{
    json_t *metadata, *character, *inventory, *equipment, *locker, *skills, *affects, *classes_obj;
    json_t *value, *array_elem;
    const char *str;
    size_t index;
    struct timeval start_time, end_time;
    long total_ms;

    // Start timing
    gettimeofday(&start_time, NULL);
    struct timeval section_start, section_end;
    long section_ms;

    // Read metadata section
    metadata = json_object_get(root, "metadata");
    if (metadata) {
        value = json_object_get(metadata, "created");
        if (value) {
            ch->pcdata->creation_date = json_integer_value(value);
        }

        // Character unique ID
        json_t *char_id = json_object_get(metadata, "character_id");
        if (char_id && json_is_array(char_id)) {
            ch->id[0] = json_integer_value(json_array_get(char_id, 0));
            ch->id[1] = json_integer_value(json_array_get(char_id, 1));
        }

        // Player data version
        value = json_object_get(metadata, "version");
        if (value) {
            ch->version = json_integer_value(value);
        } else {
            // Pre-versioning JSON file: all legacy migrations are done,
            // but new version-gated migrations (010+) should still run.
            ch->version = VERSION_PLAYER_009;
        }

        // Account linkage
        json_t *account = json_object_get(metadata, "account");
        if (account) {
            str = json_string_value(json_object_get(account, "name"));
            if (str) {
                free_string(ch->pcdata->account_name);
                ch->pcdata->account_name = str_dup(str);
            }

            json_t *account_id = json_object_get(account, "id");
            if (account_id && json_is_array(account_id)) {
                ch->pcdata->account_id[0] = json_integer_value(json_array_get(account_id, 0));
                ch->pcdata->account_id[1] = json_integer_value(json_array_get(account_id, 1));
            }
        }
    }

    // Read character section
    character = json_object_get(root, "character");
    if (!character) {
        log_stringf("json_read_char: No character section in %s", source_name);
        return false;
    }

    // Basic info
    ch->level = json_integer_value(json_object_get(character, "level"));
    ch->tot_level = json_integer_value(json_object_get(character, "tot_level"));

    str = json_string_value(json_object_get(character, "race"));
    if (str) {
        ch->race = race_lookup(str);
    }

    str = json_string_value(json_object_get(character, "original_race"));
    if (str) {
        ch->orace = race_lookup(str);
    }

    /* Migration: auto-populate orace for existing remort/path characters */
    if (ch->race && !ch->orace && !IS_NPC(ch)) {
        if (race_is_remort(ch->race)) {
            RACE_DATA *prereq = race_get_prerequisite(ch->race);

            if (prereq && !race_is_path(prereq)) {
                /* Standard remort chain with known base race — auto-set */
                ch->orace = prereq;
            } else if (race_is_path(ch->race)) {
                /* Path/transformation race — must prompt player on login */
                ch->orace_question = true;
            }

            /* Grant account unlock for their current race so they can
             * create new characters with it. Skip deprecated races
             * (remorts of path races like changeling/fiend/wraith) since
             * those characters get downgraded to the path race. */
            if (ch->desc && ch->desc->account
                    && !(prereq && race_is_path(prereq))) {
                if (account_add_race_unlock(ch->desc->account, ch->race->id)) {
                    save_account(ch->desc->account);
                    log_stringf("orace migration: unlocked race '%s' on account '%s'",
                                ch->race->id, ch->desc->account->username);
                }
            }
        }
    }

    ch->sex = json_integer_value(json_object_get(character, "sex"));
    ch->body_type = json_integer_value(json_object_get(character, "body_type"));

    // Load act/plr flags using the correct table for PCs vs NPCs
    if (ch->pcdata) {
        // PC: prefer plr_flags/plr2_flags arrays, fall back to numeric
        value = json_object_get(character, "plr_flags");
        if (value && json_is_array(value)) {
            ch->act[0] = json_flags_deserialize(value, plr_flags);
        } else {
            // Fall back to numeric (handles legacy files and old act_flags arrays)
            value = json_object_get(character, "act");
            if (value) ch->act[0] = json_integer_value(value);
        }

        value = json_object_get(character, "plr2_flags");
        if (value && json_is_array(value)) {
            ch->act[1] = json_flags_deserialize(value, plr2_flags);
        } else {
            value = json_object_get(character, "act2");
            if (value) ch->act[1] = json_integer_value(value);
        }
    } else {
        // NPC: use act_flags/act2_flags arrays, fall back to numeric
        value = json_object_get(character, "act_flags");
        if (value && json_is_array(value)) {
            ch->act[0] = json_flags_deserialize(value, act_flags);
        } else {
            value = json_object_get(character, "act");
            if (value) ch->act[0] = json_integer_value(value);
        }

        value = json_object_get(character, "act2_flags");
        if (value && json_is_array(value)) {
            ch->act[1] = json_flags_deserialize(value, act2_flags);
        } else {
            value = json_object_get(character, "act2");
            if (value) ch->act[1] = json_integer_value(value);
        }
    }

    value = json_object_get(character, "comm_flags");
    if (value && json_is_array(value)) {
        ch->comm = json_flags_deserialize(value, comm_flags);
    } else {
        value = json_object_get(character, "comm");
        if (value) ch->comm = json_integer_value(value);
    }

    if (ch->pcdata) {
        value = json_object_get(character, "channel_flags");
        if (value && json_is_array(value)) {
            ch->pcdata->channel_flags = json_flags_deserialize(value, channel_flags);
        } else {
            // Try numeric format (named "channel_flags_numeric" in new format, or just "channel_flags" in old numeric format)
            value = json_object_get(character, "channel_flags_numeric");
            if (!value) value = json_object_get(character, "channel_flags");
            if (value && json_is_integer(value)) {
                ch->pcdata->channel_flags = json_integer_value(value);
            }
        }
    }

    // **FIX #2: Load complete class data**
    classes_obj = json_object_get(character, "classes");
    if (classes_obj && json_is_object(classes_obj)) {
        json_t *mage = json_object_get(classes_obj, "mage");
        if (mage) {
            ch->pcdata->class_mage = json_integer_value(json_object_get(mage, "primary"));
            value = json_object_get(mage, "secondary");
            if (value) ch->pcdata->second_class_mage = json_integer_value(value);
            value = json_object_get(mage, "sub");
            if (value) ch->pcdata->sub_class_mage = json_integer_value(value);
            value = json_object_get(mage, "second_sub");
            if (value) ch->pcdata->second_sub_class_mage = json_integer_value(value);
        }

        json_t *cleric = json_object_get(classes_obj, "cleric");
        if (cleric) {
            ch->pcdata->class_cleric = json_integer_value(json_object_get(cleric, "primary"));
            value = json_object_get(cleric, "secondary");
            if (value) ch->pcdata->second_class_cleric = json_integer_value(value);
            value = json_object_get(cleric, "sub");
            if (value) ch->pcdata->sub_class_cleric = json_integer_value(value);
            value = json_object_get(cleric, "second_sub");
            if (value) ch->pcdata->second_sub_class_cleric = json_integer_value(value);
        }

        json_t *thief = json_object_get(classes_obj, "thief");
        if (thief) {
            ch->pcdata->class_thief = json_integer_value(json_object_get(thief, "primary"));
            value = json_object_get(thief, "secondary");
            if (value) ch->pcdata->second_class_thief = json_integer_value(value);
            value = json_object_get(thief, "sub");
            if (value) ch->pcdata->sub_class_thief = json_integer_value(value);
            value = json_object_get(thief, "second_sub");
            if (value) ch->pcdata->second_sub_class_thief = json_integer_value(value);
        }

        json_t *warrior = json_object_get(classes_obj, "warrior");
        if (warrior) {
            ch->pcdata->class_warrior = json_integer_value(json_object_get(warrior, "primary"));
            value = json_object_get(warrior, "secondary");
            if (value) ch->pcdata->second_class_warrior = json_integer_value(value);
            value = json_object_get(warrior, "sub");
            if (value) ch->pcdata->sub_class_warrior = json_integer_value(value);
            value = json_object_get(warrior, "second_sub");
            if (value) ch->pcdata->second_sub_class_warrior = json_integer_value(value);
        }
    }

    // Stats
    json_t *stats = json_object_get(character, "stats");
    if (stats) {
        json_t *perm = json_object_get(stats, "perm");
        if (perm) {
            ch->perm_stat[STAT_STR] = json_integer_value(json_object_get(perm, "str"));
            ch->perm_stat[STAT_INT] = json_integer_value(json_object_get(perm, "int"));
            ch->perm_stat[STAT_WIS] = json_integer_value(json_object_get(perm, "wis"));
            ch->perm_stat[STAT_DEX] = json_integer_value(json_object_get(perm, "dex"));
            ch->perm_stat[STAT_CON] = json_integer_value(json_object_get(perm, "con"));
        }
        json_t *mod = json_object_get(stats, "mod");
        if (mod) {
            ch->mod_stat[STAT_STR] = json_integer_value(json_object_get(mod, "str"));
            ch->mod_stat[STAT_INT] = json_integer_value(json_object_get(mod, "int"));
            ch->mod_stat[STAT_WIS] = json_integer_value(json_object_get(mod, "wis"));
            ch->mod_stat[STAT_DEX] = json_integer_value(json_object_get(mod, "dex"));
            ch->mod_stat[STAT_CON] = json_integer_value(json_object_get(mod, "con"));
        }
    }

    // Vitals
    json_t *vitals = json_object_get(character, "vitals");
    if (vitals) {
        json_t *health = json_object_get(vitals, "health");
        if (health) {
            ch->hit = json_integer_value(json_object_get(health, "current"));
            ch->max_hit = json_integer_value(json_object_get(health, "max"));
        }
        json_t *mana = json_object_get(vitals, "mana");
        if (mana) {
            ch->mana = json_integer_value(json_object_get(mana, "current"));
            ch->max_mana = json_integer_value(json_object_get(mana, "max"));
        }
        json_t *move = json_object_get(vitals, "move");
        if (move) {
            ch->move = json_integer_value(json_object_get(move, "current"));
            ch->max_move = json_integer_value(json_object_get(move, "max"));
        }
    }

    // Other basic data
    ch->alignment = json_integer_value(json_object_get(character, "alignment"));
    ch->gold = json_integer_value(json_object_get(character, "gold"));
    ch->silver = json_integer_value(json_object_get(character, "silver"));
    ch->exp = json_integer_value(json_object_get(character, "experience"));

    str = json_string_value(json_object_get(character, "title"));
    if (str) {
        free_string(ch->pcdata->title);
        ch->pcdata->title = str_dup(str);
    }

    str = json_string_value(json_object_get(character, "description"));
    if (str) {
        free_string(ch->description);
        ch->description = str_dup(str);
    }

    // Load played time (seconds). New format uses "played" (raw seconds).
    // Backward compat: old format used "played_hours" (divided by 3600), so multiply back.
    value = json_object_get(character, "played");
    if (value) {
        ch->played = json_integer_value(value);
    } else {
        value = json_object_get(character, "played_hours");
        if (value) {
            ch->played = json_integer_value(value) * 3600;
        }
    }

    // Last logoff (for offline regen calculations)
    value = json_object_get(character, "last_logoff");
    if (value && ch->pcdata) ch->pcdata->last_logoff = json_integer_value(value);

    // *** LOAD CRITICAL MISSING FIELDS - Combat Stats ***
    value = json_object_get(character, "hitroll");
    if (value) ch->hitroll = json_integer_value(value);
    value = json_object_get(character, "damroll");
    if (value) ch->damroll = json_integer_value(value);
    value = json_object_get(character, "saving_throw");
    if (value) ch->saving_throw = json_integer_value(value);

    // Load armor class array
    json_t *armor = json_object_get(character, "armor");
    if (armor && json_is_array(armor)) {
        for (int ac = 0; ac < 4 && ac < json_array_size(armor); ac++) {
            ch->armour[ac] = json_integer_value(json_array_get(armor, ac));
        }
    }

    // Practice and train sessions
    value = json_object_get(character, "practice");
    if (value) ch->practice = json_integer_value(value);
    value = json_object_get(character, "train");
    if (value) ch->train = json_integer_value(value);
    value = json_object_get(character, "wimpy");
    if (value) ch->wimpy = json_integer_value(value);

    // Position state (standing, sitting, sleeping, etc.)
    value = json_object_get(character, "position_state");
    if (value) ch->position = json_integer_value(value);

    // *** AFFECT FLAGS - Both current and permanent (prefer arrays, fall back to numeric) ***
    value = json_object_get(character, "affected_by_flags");
    if (value && json_is_array(value)) {
        ch->affected_by[0] = json_flags_deserialize(value, affect_flags);
    } else {
        value = json_object_get(character, "affected_by");
        if (value) ch->affected_by[0] = json_integer_value(value);
    }

    value = json_object_get(character, "affected_by2_flags");
    if (value && json_is_array(value)) {
        ch->affected_by[1] = json_flags_deserialize(value, affect2_flags);
    } else {
        value = json_object_get(character, "affected_by2");
        if (value) ch->affected_by[1] = json_integer_value(value);
    }

    value = json_object_get(character, "affected_by_perm_flags");
    if (value && json_is_array(value)) {
        ch->affected_by_perm[0] = json_flags_deserialize(value, affect_flags);
    } else {
        value = json_object_get(character, "affected_by_perm");
        if (value) ch->affected_by_perm[0] = json_integer_value(value);
    }

    value = json_object_get(character, "affected_by_perm2_flags");
    if (value && json_is_array(value)) {
        ch->affected_by_perm[1] = json_flags_deserialize(value, affect2_flags);
    } else {
        value = json_object_get(character, "affected_by_perm2");
        if (value) ch->affected_by_perm[1] = json_integer_value(value);
    }

    // *** RESISTANCE/IMMUNITY/VULNERABILITY FLAGS (prefer arrays, fall back to numeric) ***
    value = json_object_get(character, "imm_flags_names");
    if (value && json_is_array(value)) {
        ch->imm_flags = json_flags_deserialize(value, imm_flags);
    } else {
        value = json_object_get(character, "imm_flags");
        if (value) ch->imm_flags = json_integer_value(value);
    }

    value = json_object_get(character, "imm_flags_perm_names");
    if (value && json_is_array(value)) {
        ch->imm_flags_perm = json_flags_deserialize(value, imm_flags);
    } else {
        value = json_object_get(character, "imm_flags_perm");
        if (value) ch->imm_flags_perm = json_integer_value(value);
    }

    value = json_object_get(character, "res_flags_names");
    if (value && json_is_array(value)) {
        ch->res_flags = json_flags_deserialize(value, res_flags);
    } else {
        value = json_object_get(character, "res_flags");
        if (value) ch->res_flags = json_integer_value(value);
    }

    value = json_object_get(character, "res_flags_perm_names");
    if (value && json_is_array(value)) {
        ch->res_flags_perm = json_flags_deserialize(value, res_flags);
    } else {
        value = json_object_get(character, "res_flags_perm");
        if (value) ch->res_flags_perm = json_integer_value(value);
    }

    value = json_object_get(character, "vuln_flags_names");
    if (value && json_is_array(value)) {
        ch->vuln_flags = json_flags_deserialize(value, vuln_flags);
    } else {
        value = json_object_get(character, "vuln_flags");
        if (value) ch->vuln_flags = json_integer_value(value);
    }

    value = json_object_get(character, "vuln_flags_perm_names");
    if (value && json_is_array(value)) {
        ch->vuln_flags_perm = json_flags_deserialize(value, vuln_flags);
    } else {
        value = json_object_get(character, "vuln_flags_perm");
        if (value) ch->vuln_flags_perm = json_integer_value(value);
    }

    // Lost body parts (prefer array, fall back to numeric)
    value = json_object_get(character, "lostparts_names");
    if (value && json_is_array(value)) {
        ch->lostparts = json_flags_deserialize(value, part_flags);
    } else {
        value = json_object_get(character, "lostparts");
        if (value) ch->lostparts = json_integer_value(value);
    }

    // *** COUNTERS - Deaths, Kills, Quest Points ***
    value = json_object_get(character, "deaths");
    if (value) ch->deaths = json_integer_value(value);
    value = json_object_get(character, "arena_deaths");
    if (value) ch->arena_deaths = json_integer_value(value);
    value = json_object_get(character, "player_deaths");
    if (value) ch->player_deaths = json_integer_value(value);
    value = json_object_get(character, "cpk_deaths");
    if (value) ch->cpk_deaths = json_integer_value(value);
    value = json_object_get(character, "wars_won");
    if (value) ch->wars_won = json_integer_value(value);
    value = json_object_get(character, "arena_kills");
    if (value) ch->arena_kills = json_integer_value(value);
    value = json_object_get(character, "player_kills");
    if (value) ch->player_kills = json_integer_value(value);
    value = json_object_get(character, "cpk_kills");
    if (value) ch->cpk_kills = json_integer_value(value);
    value = json_object_get(character, "monster_kills");
    if (value) ch->monster_kills = json_integer_value(value);

    // Quest data
    value = json_object_get(character, "questpoints");
    if (value) ch->questpoints = json_integer_value(value);
    value = json_object_get(character, "nextquest");
    if (value) ch->nextquest = json_integer_value(value);
    value = json_object_get(character, "deitypoints");
    if (value) ch->deitypoints = json_integer_value(value);
    value = json_object_get(character, "pneuma");
    if (value) ch->pneuma = json_integer_value(value);
    value = json_object_get(character, "home");
    if (value) ch->home = json_integer_value(value);
    value = json_object_get(character, "manastore");
    if (value) ch->manastore = json_integer_value(value);
    value = json_object_get(character, "locker_tier");
    if (value) ch->locker_tier = json_integer_value(value);

    // PC_DATA fields
    if (ch->pcdata) {
        value = json_object_get(character, "bankbalance");
        if (value) ch->pcdata->bankbalance = json_integer_value(value);

        value = json_object_get(character, "last_login");
        if (value) ch->pcdata->last_login = json_integer_value(value);

        value = json_object_get(character, "true_sex");
        if (value) ch->pcdata->true_sex = json_integer_value(value);
        value = json_object_get(character, "last_level");
        if (value) ch->pcdata->last_level = json_integer_value(value);
        value = json_object_get(character, "quests_completed");
        if (value) ch->pcdata->quests_completed = json_integer_value(value);
        value = json_object_get(character, "security");
        if (value) ch->pcdata->security = json_integer_value(value);

        // *** USER PREFERENCES ***
        value = json_object_get(character, "scroll_lines");
        if (value) ch->lines = json_integer_value(value);

        str = json_string_value(json_object_get(character, "prompt"));
        if (str) {
            free_string(ch->prompt);
            ch->prompt = str_dup(str);
        }

        value = json_object_get(character, "verb_preference");
        if (value) ch->verb_preference = json_integer_value(value);

        // Character preference overrides
        json_t *pref_overrides = json_object_get(character, "preference_overrides");
        if (pref_overrides && json_is_array(pref_overrides)) {
            json_to_prefs(pref_overrides, &ch->pcdata->preferences);
        }

        // Pronouns
        str = json_string_value(json_object_get(character, "pronoun_he_she"));
        if (str) {
            free_string(ch->pronoun_he_she);
            ch->pronoun_he_she = str_dup(str);
        }
        str = json_string_value(json_object_get(character, "pronoun_him_her"));
        if (str) {
            free_string(ch->pronoun_him_her);
            ch->pronoun_him_her = str_dup(str);
        }
        str = json_string_value(json_object_get(character, "pronoun_his_her"));
        if (str) {
            free_string(ch->pronoun_his_her);
            ch->pronoun_his_her = str_dup(str);
        }
        str = json_string_value(json_object_get(character, "pronoun_his_hers"));
        if (str) {
            free_string(ch->pronoun_his_hers);
            ch->pronoun_his_hers = str_dup(str);
        }
        str = json_string_value(json_object_get(character, "pronoun_himself_herself"));
        if (str) {
            free_string(ch->pronoun_himself_herself);
            ch->pronoun_himself_herself = str_dup(str);
        }

        // Deletion status
        value = json_object_get(character, "deleted");
        if (value) ch->deleted = json_is_true(value);
        value = json_object_get(character, "delete_time");
        if (value) ch->delete_time = json_integer_value(value);

        // Locker rent
        value = json_object_get(character, "locker_rent");
        if (value) ch->locker_rent = json_integer_value(value);

        // Permanent vitals
        value = json_object_get(character, "perm_hit");
        if (value) ch->pcdata->perm_hit = json_integer_value(value);
        value = json_object_get(character, "perm_mana");
        if (value) ch->pcdata->perm_mana = json_integer_value(value);
        value = json_object_get(character, "perm_move");
        if (value) ch->pcdata->perm_move = json_integer_value(value);

        // Condition (hunger, thirst, drunk, food)
        json_t *condition = json_object_get(character, "condition");
        if (condition && json_is_array(condition)) {
            for (int cond = 0; cond < 4 && cond < json_array_size(condition); cond++) {
                ch->pcdata->condition[cond] = json_integer_value(json_array_get(condition, cond));
            }
        }

        // Toxins - lookup by NAME (robust against ID changes)
        json_t *toxins = json_object_get(character, "toxins");
        if (toxins && json_is_object(toxins)) {
            const char *toxin_key;
            json_t *toxin_value;
            json_object_foreach(toxins, toxin_key, toxin_value) {
                int toxin_id = -1;

                // Look up toxin by name
                for (int i = 0; i < MAX_TOXIN; i++) {
                    if (toxin_table[i].name && !str_cmp(toxin_table[i].name, toxin_key)) {
                        toxin_id = i;
                        break;
                    }
                }

                // If not found by name, try as numeric ID (backward compatibility)
                if (toxin_id < 0) {
                    toxin_id = atoi(toxin_key);
                    if (toxin_id < 0 || toxin_id >= MAX_TOXIN) {
                        continue; // Skip invalid toxin
                    }
                }

                // Load toxin level
                if (json_is_object(toxin_value)) {
                    json_t *level = json_object_get(toxin_value, "level");
                    if (level) {
                        ch->toxin[toxin_id] = json_integer_value(level);
                    }
                } else {
                    // Very old format - just the level as integer
                    ch->toxin[toxin_id] = json_integer_value(toxin_value);
                }
            }
        }

        // Immortal-specific fields
        value = json_object_get(character, "invis_level");
        if (value) ch->invis_level = json_integer_value(value);
        value = json_object_get(character, "incog_level");
        if (value) ch->incog_level = json_integer_value(value);
        value = json_object_get(character, "wiznet");
        if (value) ch->wiznet = json_integer_value(value);

        // Church membership
        str = json_string_value(json_object_get(character, "church"));
        if (str) {
            ch->church = get_church_by_name(str);
        }

        // Immortal imm_flag (custom who-tag)
        str = json_string_value(json_object_get(character, "imm_flag"));
        if (str && ch->pcdata->immortal) {
            free_string(ch->pcdata->immortal->imm_flag);
            ch->pcdata->immortal->imm_flag = str_dup(str);
        }

        // Staff rank
        value = json_object_get(character, "staff_rank");
        if (value) ch->pcdata->staff_rank = json_integer_value(value);

        // Current class/subclass selection
        value = json_object_get(character, "class_current");
        if (value) ch->pcdata->class_current = json_integer_value(value);
        value = json_object_get(character, "sub_class_current");
        if (value) ch->pcdata->sub_class_current = json_integer_value(value);

        // Challenge delay, password change, danger range
        value = json_object_get(character, "challenge_delay");
        if (value) ch->pcdata->challenge_delay = json_integer_value(value);
        value = json_object_get(character, "need_change_pw");
        if (value) ch->pcdata->need_change_pw = json_integer_value(value);
        value = json_object_get(character, "danger_range");
        if (value) ch->pcdata->danger_range = json_integer_value(value);

        // Note read timestamps
        value = json_object_get(character, "last_note");
        if (value) ch->pcdata->last_note = json_integer_value(value);
        value = json_object_get(character, "last_idea");
        if (value) ch->pcdata->last_idea = json_integer_value(value);
        value = json_object_get(character, "last_penalty");
        if (value) ch->pcdata->last_penalty = json_integer_value(value);
        value = json_object_get(character, "last_news");
        if (value) ch->pcdata->last_news = json_integer_value(value);
        value = json_object_get(character, "last_changes");
        if (value) ch->pcdata->last_changes = json_integer_value(value);
        value = json_object_get(character, "last_project_inquiry");
        if (value) ch->pcdata->last_project_inquiry = json_integer_value(value);

        // Pre-level vitals snapshot
        value = json_object_get(character, "hit_before");
        if (value) ch->pcdata->hit_before = json_integer_value(value);
        value = json_object_get(character, "mana_before");
        if (value) ch->pcdata->mana_before = json_integer_value(value);
        value = json_object_get(character, "move_before");
        if (value) ch->pcdata->move_before = json_integer_value(value);

        // Last area string
        str = json_string_value(json_object_get(character, "last_area"));
        if (str) {
            free_string(ch->pcdata->last_area);
            ch->pcdata->last_area = str_dup(str);
        }

        // AFK message
        str = json_string_value(json_object_get(character, "afk_message"));
        if (str) {
            free_string(ch->pcdata->afk_message);
            ch->pcdata->afk_message = str_dup(str);
        }

        // Player flag
        str = json_string_value(json_object_get(character, "player_flag"));
        if (str) {
            free_string(ch->pcdata->flag);
            ch->pcdata->flag = str_dup(str);
        }

        // Character-level auth data (for unlinked characters or mid-migration)
        str = json_string_value(json_object_get(character, "char_password"));
        if (str) {
            free_string(ch->pcdata->pwd);
            ch->pcdata->pwd = str_dup(str);
        }
        value = json_object_get(character, "char_password_version");
        if (value) ch->pcdata->pwd_vers = json_integer_value(value);

        str = json_string_value(json_object_get(character, "char_old_password"));
        if (str) {
            free_string(ch->pcdata->old_pwd);
            ch->pcdata->old_pwd = str_dup(str);
        }

        str = json_string_value(json_object_get(character, "char_reset_code"));
        if (str) {
            free_string(ch->pcdata->reset_code);
            ch->pcdata->reset_code = str_dup(str);
        }
        value = json_object_get(character, "char_reset_time");
        if (value) ch->pcdata->reset_time = json_integer_value(value);
        value = json_object_get(character, "char_reset_state");
        if (value) ch->pcdata->reset_state = json_integer_value(value);

        str = json_string_value(json_object_get(character, "char_mfa_key"));
        if (str) {
            free_string(ch->pcdata->mfa_key);
            ch->pcdata->mfa_key = str_dup(str);
        }
        ch->pcdata->mfa_enabled = json_is_true(json_object_get(character, "char_mfa_enabled"));
        ch->pcdata->mfa_pending = json_is_true(json_object_get(character, "char_mfa_pending"));

        str = json_string_value(json_object_get(character, "char_mfa_pending_key"));
        if (str) {
            free_string(ch->pcdata->mfa_pending_key);
            ch->pcdata->mfa_pending_key = str_dup(str);
        }

        // Recovery codes
        {
            json_t *recovery = json_object_get(character, "char_recovery_codes");
            if (recovery && json_is_array(recovery)) {
                int ri = 0;
                size_t ridx;
                json_t *relem;
                json_array_foreach(recovery, ridx, relem) {
                    if (ri >= MFA_RECOVERY_CODES) break;
                    str = json_string_value(json_object_get(relem, "code"));
                    if (str) {
                        free_string(ch->pcdata->recovery_codes[ri]);
                        ch->pcdata->recovery_codes[ri] = str_dup(str);
                        ch->pcdata->recovery_used[ri] = json_is_true(json_object_get(relem, "used"));
                    }
                    ri++;
                }
            }
        }

        // Character-level email fields
        str = json_string_value(json_object_get(character, "char_email"));
        if (str) {
            free_string(ch->pcdata->email);
            ch->pcdata->email = str_dup(str);
        }
        ch->pcdata->email_verified = json_is_true(json_object_get(character, "char_email_verified"));

        str = json_string_value(json_object_get(character, "char_pending_email"));
        if (str) {
            free_string(ch->pcdata->pending_email);
            ch->pcdata->pending_email = str_dup(str);
        }
        str = json_string_value(json_object_get(character, "char_email_verification_code"));
        if (str) {
            free_string(ch->pcdata->email_verification_code);
            ch->pcdata->email_verification_code = str_dup(str);
        }
        value = json_object_get(character, "char_email_verification_time");
        if (value) ch->pcdata->email_verification_time = json_integer_value(value);
        value = json_object_get(character, "char_email_verification_last_sent");
        if (value) ch->pcdata->email_verification_last_sent = json_integer_value(value);

        // Ignore list
        json_t *ignoring = json_object_get(character, "ignoring");
        if (ignoring && json_is_array(ignoring)) {
            json_array_foreach(ignoring, index, array_elem) {
                const char *ignore_name = json_string_value(json_object_get(array_elem, "name"));
                if (ignore_name) {
                    IGNORE_DATA *ignore = new_ignore();
                    ignore->name = str_dup(ignore_name);
                    const char *ignore_reason = json_string_value(json_object_get(array_elem, "reason"));
                    if (ignore_reason) {
                        ignore->reason = str_dup(ignore_reason);
                    }
                    ignore->next = ch->pcdata->ignoring;
                    ch->pcdata->ignoring = ignore;
                }
            }
        }

        // Vis-to list (selective visibility)
        json_t *visto = json_object_get(character, "vis_to");
        if (visto && json_is_array(visto)) {
            json_array_foreach(visto, index, array_elem) {
                str = json_string_value(array_elem);
                if (str) {
                    STRING_DATA *string = new_string_data();
                    string->string = str_dup(str);
                    string->next = ch->pcdata->vis_to_people;
                    ch->pcdata->vis_to_people = string;
                }
            }
        }

        // Quiet-to list
        json_t *quietto = json_object_get(character, "quiet_to");
        if (quietto && json_is_array(quietto)) {
            json_array_foreach(quietto, index, array_elem) {
                str = json_string_value(array_elem);
                if (str) {
                    STRING_DATA *string = new_string_data();
                    string->string = str_dup(str);
                    string->next = ch->pcdata->quiet_people;
                    ch->pcdata->quiet_people = string;
                }
            }
        }

        // Room before arena (LOCATION)
        json_t *rba = json_object_get(character, "room_before_arena");
        if (rba && json_is_object(rba)) {
            value = json_object_get(rba, "wuid");
            if (value) ch->pcdata->room_before_arena.wuid = json_integer_value(value);
            json_t *rba_id = json_object_get(rba, "id");
            if (rba_id && json_is_array(rba_id)) {
                ch->pcdata->room_before_arena.id[0] = json_integer_value(json_array_get(rba_id, 0));
                ch->pcdata->room_before_arena.id[1] = json_integer_value(json_array_get(rba_id, 1));
                ch->pcdata->room_before_arena.id[2] = json_integer_value(json_array_get(rba_id, 2));
            }
        }

        // Granted commands
        json_t *granted = json_object_get(character, "granted_commands");
        if (granted && json_is_array(granted)) {
            json_array_foreach(granted, index, array_elem) {
                str = json_string_value(array_elem);
                if (str) {
                    COMMAND_DATA *cmd = new_command();
                    cmd->name = str_dup(str);
                    cmd->next = ch->pcdata->commands;
                    ch->pcdata->commands = cmd;
                }
            }
        }
    }

    // Death state
    value = json_object_get(character, "dead");
    if (value) ch->dead = json_is_true(value);
    value = json_object_get(character, "death_time_left");
    if (value) ch->time_left_death = json_integer_value(value);

    // Shifted form (werewolf/slayer)
    value = json_object_get(character, "shifted");
    if (value) ch->shifted = json_integer_value(value);

    // Before-social room marker (LOCATION)
    json_t *bs = json_object_get(character, "before_social");
    if (bs && json_is_object(bs)) {
        value = json_object_get(bs, "wuid");
        if (value) ch->before_social.wuid = json_integer_value(value);
        json_t *bs_id = json_object_get(bs, "id");
        if (bs_id && json_is_array(bs_id)) {
            ch->before_social.id[0] = json_integer_value(json_array_get(bs_id, 0));
            ch->before_social.id[1] = json_integer_value(json_array_get(bs_id, 1));
            ch->before_social.id[2] = json_integer_value(json_array_get(bs_id, 2));
        }
    }

    // Recall location
    json_t *recall = json_object_get(character, "recall");
    if (recall) {
        value = json_object_get(recall, "wuid");
        if (value) ch->recall.wuid = json_integer_value(value);

        json_t *recall_id = json_object_get(recall, "id");
        if (recall_id && json_is_array(recall_id)) {
            ch->recall.id[0] = json_integer_value(json_array_get(recall_id, 0));
            ch->recall.id[1] = json_integer_value(json_array_get(recall_id, 1));
            ch->recall.id[2] = json_integer_value(json_array_get(recall_id, 2));
        }
    }

    // Quest data (if currently questing)
    json_t *quest = json_object_get(character, "quest");
    if (quest && ch->pcdata) {
        // Allocate quest structure if needed
        if (!ch->quest) {
            ch->quest = (QUEST_DATA *)alloc_perm(sizeof(QUEST_DATA));
            // Initialize all fields to prevent garbage memory access
            ch->quest->next = NULL;
            ch->quest->parts = NULL;
            ch->quest->questgiver_type = 0;
            ch->quest->questgiver_load.auid = 0;
            ch->quest->questgiver_load.vnum = -1;
            ch->quest->questgiver_wnum = wnum_zero;
            ch->quest->questreceiver_type = 0;
            ch->quest->questreceiver_load.auid = 0;
            ch->quest->questreceiver_load.vnum = -1;
            ch->quest->questreceiver_wnum = wnum_zero;
            ch->quest->msg_complete = false;
            ch->quest->generating = false;
            ch->quest->scripted = false;
        }

        value = json_object_get(quest, "questgiver_type");
        if (value) ch->quest->questgiver_type = json_integer_value(value);
        value = json_object_get(quest, "questgiver");
        if (value) {
            if (json_is_string(value)) {
                if (parse_widevnum_load(json_string_value(value), &ch->quest->questgiver_load)) {
                    AREA_DATA *fallback = NULL;
                    WNUM wnum;
                    if (resolve_widevnum(ch->quest->questgiver_load.vnum, NULL, &wnum))
                        fallback = wnum.pArea;
                    if (!fallback) fallback = get_system_area_fallback();
                    resolve_wnum_load(&ch->quest->questgiver_load, &ch->quest->questgiver_wnum, fallback);
                }
            } else if (json_is_integer(value)) {
                ch->quest->questgiver_load.vnum = json_integer_value(value);
                ch->quest->questgiver_load.auid = 0;
                if (ch->quest->questgiver_load.vnum > 0) {
                    AREA_DATA *fallback = NULL;
                    WNUM wnum;
                    if (resolve_widevnum(ch->quest->questgiver_load.vnum, NULL, &wnum))
                        fallback = wnum.pArea;
                    if (!fallback) fallback = get_system_area_fallback();
                    resolve_wnum_load(&ch->quest->questgiver_load, &ch->quest->questgiver_wnum, fallback);
                }
            }
        }
        value = json_object_get(quest, "questreceiver_type");
        if (value) ch->quest->questreceiver_type = json_integer_value(value);
        value = json_object_get(quest, "questreceiver");
        if (value) {
            if (json_is_string(value)) {
                if (parse_widevnum_load(json_string_value(value), &ch->quest->questreceiver_load)) {
                    AREA_DATA *fallback = NULL;
                    WNUM wnum;
                    if (resolve_widevnum(ch->quest->questreceiver_load.vnum, NULL, &wnum))
                        fallback = wnum.pArea;
                    if (!fallback) fallback = get_system_area_fallback();
                    resolve_wnum_load(&ch->quest->questreceiver_load, &ch->quest->questreceiver_wnum, fallback);
                }
            } else if (json_is_integer(value)) {
                ch->quest->questreceiver_load.vnum = json_integer_value(value);
                ch->quest->questreceiver_load.auid = 0;
                if (ch->quest->questreceiver_load.vnum > 0) {
                    AREA_DATA *fallback = NULL;
                    WNUM wnum;
                    if (resolve_widevnum(ch->quest->questreceiver_load.vnum, NULL, &wnum))
                        fallback = wnum.pArea;
                    if (!fallback) fallback = get_system_area_fallback();
                    resolve_wnum_load(&ch->quest->questreceiver_load, &ch->quest->questreceiver_wnum, fallback);
                }
            }
        }
        value = json_object_get(quest, "countdown");
        if (value) ch->countdown = json_integer_value(value);

        // Additional quest flags
        value = json_object_get(quest, "msg_complete");
        if (value) ch->quest->msg_complete = json_boolean_value(value);
        value = json_object_get(quest, "scripted");
        if (value) ch->quest->scripted = json_boolean_value(value);

        // Quest parts
        json_t *parts_array = json_object_get(quest, "parts");
        if (parts_array && json_is_array(parts_array)) {
            QUEST_PART_DATA *last_part = NULL;
            size_t part_idx;
            json_t *part_elem;
            json_array_foreach(parts_array, part_idx, part_elem) {
                QUEST_PART_DATA *part = new_quest_part();

                value = json_object_get(part_elem, "index");
                if (value) part->index = json_integer_value(value);
                value = json_object_get(part_elem, "minutes");
                if (value) part->minutes = json_integer_value(value);
                value = json_object_get(part_elem, "obj");
                if (value) {
                    if (json_is_string(value) && parse_widevnum_load(json_string_value(value), &part->obj_load)) {
                        AREA_DATA *fallback = NULL;
                        WNUM wnum;
                        if (resolve_widevnum(part->obj_load.vnum, NULL, &wnum))
                            fallback = wnum.pArea;
                        if (!fallback) fallback = get_system_area_fallback();
                        resolve_wnum_load(&part->obj_load, &part->obj_wnum, fallback);
                    } else if (json_is_integer(value)) {
                        part->obj_load.auid = 0;
                        part->obj_load.vnum = json_integer_value(value);
                        if (part->obj_load.vnum > 0) {
                            AREA_DATA *fallback = NULL;
                            WNUM wnum;
                            if (resolve_widevnum(part->obj_load.vnum, NULL, &wnum))
                                fallback = wnum.pArea;
                            if (!fallback) fallback = get_system_area_fallback();
                            resolve_wnum_load(&part->obj_load, &part->obj_wnum, fallback);
                        }
                    }
                }
                value = json_object_get(part_elem, "mob");
                if (value) {
                    if (json_is_string(value) && parse_widevnum_load(json_string_value(value), &part->mob_load)) {
                        AREA_DATA *fallback = NULL;
                        WNUM wnum;
                        if (resolve_widevnum(part->mob_load.vnum, NULL, &wnum))
                            fallback = wnum.pArea;
                        if (!fallback) fallback = get_system_area_fallback();
                        resolve_wnum_load(&part->mob_load, &part->mob_wnum, fallback);
                    } else if (json_is_integer(value)) {
                        part->mob_load.auid = 0;
                        part->mob_load.vnum = json_integer_value(value);
                        if (part->mob_load.vnum > 0) {
                            AREA_DATA *fallback = NULL;
                            WNUM wnum;
                            if (resolve_widevnum(part->mob_load.vnum, NULL, &wnum))
                                fallback = wnum.pArea;
                            if (!fallback) fallback = get_system_area_fallback();
                            resolve_wnum_load(&part->mob_load, &part->mob_wnum, fallback);
                        }
                    }
                }
                value = json_object_get(part_elem, "room");
                if (value) {
                    if (json_is_string(value) && parse_widevnum_load(json_string_value(value), &part->room_load)) {
                        AREA_DATA *fallback = NULL;
                        WNUM wnum;
                        if (resolve_widevnum(part->room_load.vnum, NULL, &wnum))
                            fallback = wnum.pArea;
                        if (!fallback) fallback = get_system_area_fallback();
                        resolve_wnum_load(&part->room_load, &part->room_wnum, fallback);
                    } else if (json_is_integer(value)) {
                        part->room_load.auid = 0;
                        part->room_load.vnum = json_integer_value(value);
                        if (part->room_load.vnum > 0) {
                            AREA_DATA *fallback = NULL;
                            WNUM wnum;
                            if (resolve_widevnum(part->room_load.vnum, NULL, &wnum))
                                fallback = wnum.pArea;
                            if (!fallback) fallback = get_system_area_fallback();
                            resolve_wnum_load(&part->room_load, &part->room_wnum, fallback);
                        }
                    }
                }
                value = json_object_get(part_elem, "obj_sac");
                if (value) {
                    if (json_is_string(value) && parse_widevnum_load(json_string_value(value), &part->obj_sac_load)) {
                        AREA_DATA *fallback = NULL;
                        WNUM wnum;
                        if (resolve_widevnum(part->obj_sac_load.vnum, NULL, &wnum))
                            fallback = wnum.pArea;
                        if (!fallback) fallback = get_system_area_fallback();
                        resolve_wnum_load(&part->obj_sac_load, &part->obj_sac_wnum, fallback);
                    } else if (json_is_integer(value)) {
                        part->obj_sac_load.auid = 0;
                        part->obj_sac_load.vnum = json_integer_value(value);
                        if (part->obj_sac_load.vnum > 0) {
                            AREA_DATA *fallback = NULL;
                            WNUM wnum;
                            if (resolve_widevnum(part->obj_sac_load.vnum, NULL, &wnum))
                                fallback = wnum.pArea;
                            if (!fallback) fallback = get_system_area_fallback();
                            resolve_wnum_load(&part->obj_sac_load, &part->obj_sac_wnum, fallback);
                        }
                    }
                }
                value = json_object_get(part_elem, "mob_rescue");
                if (value) {
                    if (json_is_string(value) && parse_widevnum_load(json_string_value(value), &part->mob_rescue_load)) {
                        AREA_DATA *fallback = NULL;
                        WNUM wnum;
                        if (resolve_widevnum(part->mob_rescue_load.vnum, NULL, &wnum))
                            fallback = wnum.pArea;
                        if (!fallback) fallback = get_system_area_fallback();
                        resolve_wnum_load(&part->mob_rescue_load, &part->mob_rescue_wnum, fallback);
                    } else if (json_is_integer(value)) {
                        part->mob_rescue_load.auid = 0;
                        part->mob_rescue_load.vnum = json_integer_value(value);
                        if (part->mob_rescue_load.vnum > 0) {
                            AREA_DATA *fallback = NULL;
                            WNUM wnum;
                            if (resolve_widevnum(part->mob_rescue_load.vnum, NULL, &wnum))
                                fallback = wnum.pArea;
                            if (!fallback) fallback = get_system_area_fallback();
                            resolve_wnum_load(&part->mob_rescue_load, &part->mob_rescue_wnum, fallback);
                        }
                    }
                }
                value = json_object_get(part_elem, "custom_task");
                if (value) part->custom_task = json_boolean_value(value);
                value = json_object_get(part_elem, "complete");
                if (value) part->complete = json_boolean_value(value);
                value = json_object_get(part_elem, "description");
                if (value) part->description = str_dup(json_string_value(value));

                // Handle pickup quest objects
                // Note: pObj reconstruction would need to happen elsewhere after world is loaded
                // For now just leave it NULL - the quest system should handle missing objects

                // Link into parts list (preserve order)
                part->next = NULL;
                if (!ch->quest->parts) {
                    ch->quest->parts = part;
                } else if (last_part) {
                    last_part->next = part;
                }
                last_part = part;
            }
        }
    }

    // **FIX #3: Set room pointer directly WITHOUT calling char_to_room()**
    // The old pfile loading just sets ch->in_room pointer, NOT calling char_to_room()
    // char_to_room() will be called later during the login sequence
    json_t *position = json_object_get(character, "position");
    if (position) {
        json_t *type_obj = json_object_get(position, "type");
        const char *type = type_obj ? json_string_value(type_obj) : "room";
        
        if (!strcmp(type, "wilderness")) {
            // Wilderness position - set coordinates and in_wilds
            ch->at_wilds_x = json_integer_value(json_object_get(position, "x"));
            ch->at_wilds_y = json_integer_value(json_object_get(position, "y"));
            long area_uid = json_integer_value(json_object_get(position, "area_uid"));
            long wilds_uid = json_integer_value(json_object_get(position, "wilds_uid"));
            
            AREA_DATA *pArea = get_area_from_uid(area_uid);
            if (pArea) {
                ch->in_wilds = get_wilds_from_uid(pArea, wilds_uid);
                if (ch->in_wilds) {
                    // char_to_vroom() will be called later in nanny.c
                    ch->in_room = NULL;
                    plogf(LOG_INFO, "%s loaded at wilderness (%d, %d) in wilds uid %ld",
                          ch->name, ch->at_wilds_x, ch->at_wilds_y, wilds_uid);
                } else {
                    plogf(LOG_WARN, "%s: wilderness uid %ld not found, using default recall",
                          ch->name, wilds_uid);
                    ch->in_room = get_room_index_global(11001);
                }
            } else {
                plogf(LOG_WARN, "%s: area uid %ld not found, using default recall",
                      ch->name, area_uid);
                ch->in_room = get_room_index_global(11001);
            }
        } else {
            // Regular room position - supports both widevnum string and legacy integer
            json_t *room_vnum_val = json_object_get(position, "room_vnum");
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
            if (room) {
                ch->in_room = room;
            } else {
                // Fallback to default recall room if saved room doesn't exist
                plogf(LOG_WARN, "%s: room not found, using default recall", ch->name);
                ch->in_room = get_room_index_global(11001);
            }
        }
    } else {
        // No position saved - use default recall
        ch->in_room = get_room_index_global(11001);
    }

    // Read inventory section (skip if not loading heavy data)
    if (load_heavy) {
        gettimeofday(&section_start, NULL);
        inventory = json_object_get(root, "inventory");
        if (inventory && json_is_array(inventory)) {
            json_array_foreach(inventory, index, array_elem) {
                OBJ_DATA *obj = json_to_obj(array_elem, ch);
                if (obj) {
                    obj_to_char(obj, ch);
                }
            }
        }
        gettimeofday(&section_end, NULL);
        section_ms = (section_end.tv_sec - section_start.tv_sec) * 1000 +
                    (section_end.tv_usec - section_start.tv_usec) / 1000;
        if (section_ms > 100)
            log_stringf("PERFORMANCE %s: inventory load: %ldms", ch->name, section_ms);
    }

    // Read equipment section (skip if not loading heavy data)
    if (load_heavy) {
        gettimeofday(&section_start, NULL);
        equipment = json_object_get(root, "equipment");
        if (equipment && json_is_array(equipment)) {
            json_array_foreach(equipment, index, array_elem) {
                OBJ_DATA *obj = json_to_obj(array_elem, ch);
                if (obj) {
                    obj_to_char(obj, ch);
                    // Add to worn list if equipped (wear_loc is set in json_to_obj)
                    if (obj->wear_loc != WEAR_NONE) {
                        list_addlink(ch->lworn, obj);
                    }
                }
            }
        }
        gettimeofday(&section_end, NULL);
        section_ms = (section_end.tv_sec - section_start.tv_sec) * 1000 +
                    (section_end.tv_usec - section_start.tv_usec) / 1000;
        if (section_ms > 100)
            log_stringf("PERFORMANCE %s: equipment load: %ldms", ch->name, section_ms);
    }

    // Read locker section (skip if not loading heavy data)
    if (load_heavy) {
        gettimeofday(&section_start, NULL);
        locker = json_object_get(root, "locker");
        if (locker && json_is_array(locker)) {
            json_array_foreach(locker, index, array_elem) {
                OBJ_DATA *obj = json_to_obj(array_elem, ch);
                if (obj) {
                    // Use obj_to_locker() which properly adds to ch->llocker
                    obj_to_locker(obj, ch);
                }
            }
        }
        gettimeofday(&section_end, NULL);
        section_ms = (section_end.tv_sec - section_start.tv_sec) * 1000 +
                    (section_end.tv_usec - section_start.tv_usec) / 1000;
        if (section_ms > 100)
            log_stringf("PERFORMANCE %s: locker load: %ldms", ch->name, section_ms);
    }

    // **FIX #4: Read skills section - uses skill NAME as key (robust against ID changes)**
    // (skip if not loading heavy data)
    if (load_heavy) {
    gettimeofday(&section_start, NULL);
    skills = json_object_get(root, "skills");
    if (skills && json_is_object(skills)) {
        const char *skill_key;
        json_t *skill_value;
        SKILL_ENTRY *entry;
        json_object_foreach(skills, skill_key, skill_value) {
            int sn;

            // Use hash-based exact match first (O(1)), fall back to prefix search
            SKILL_DATA *sk = skill_find(skill_key);
            sn = sk ? sk->uid : -1;
            if (sn < 0)
                sn = skill_lookup(skill_key);

            // If not found by name, try as numeric ID (backward compatibility with old format)
            if (sn < 0) {
                sn = atoi(skill_key);
                if (sn < 0 || sn >= MAX_SKILL) {
                    continue; // Skip invalid skill
                }
            }

            // Now we have a valid skill number, load the data
            if (json_is_object(skill_value)) {
                // New format - object with learned/mod_learned fields
                json_t *learned = json_object_get(skill_value, "learned");
                if (learned) {
                    ch->pcdata->learned[sn] = json_integer_value(learned);
                }
                // Load skill modifiers
                json_t *mod_learned = json_object_get(skill_value, "mod_learned");
                if (mod_learned) {
                    ch->pcdata->mod_learned[sn] = json_integer_value(mod_learned);
                }
            } else {
                // Very old format - just the percentage as integer
                ch->pcdata->learned[sn] = json_integer_value(skill_value);
            }

            // Parse source and flags from JSON (defaults match old behavior)
            char source = SKILLSRC_NORMAL;
            long flags = SKILL_AUTOMATIC;
            if (json_is_object(skill_value)) {
                const char *source_str = json_string_value(json_object_get(skill_value, "source"));
                source = json_parse_skill_source(source_str);

                const char *flags_str = json_string_value(json_object_get(skill_value, "flags"));
                if (flags_str) {
                    flags = flag_value(skill_flags, (char *)flags_str);
                    if (flags == NO_FLAG) flags = SKILL_AUTOMATIC;
                }
            }

            // Add to sorted_skills list so 'skills'/'spells' commands work
            if (skill_table[sn].spell_fun == spell_null)
                skill_entry_addskill(ch, sn, NULL, source, flags);
            else
                skill_entry_addspell(ch, sn, NULL, source, flags);

            // Populate entry rating fields from loaded learned[] data
            entry = skill_entry_findsn(ch->sorted_skills, sn);
            if (entry) {
                entry->rating = ch->pcdata->learned[sn];
                entry->mod_rating = ch->pcdata->mod_learned[sn];

                // Load class sources list
                if (json_is_object(skill_value)) {
                    json_t *class_sources = json_object_get(skill_value, "class_sources");
                    if (class_sources && json_is_array(class_sources)) {
                        size_t si;
                        json_t *src_elem;
                        SKILL_SOURCE *tail = NULL;

                        json_array_foreach(class_sources, si, src_elem) {
                            const char *cls_name = json_string_value(
                                json_object_get(src_elem, "class"));
                            if (!cls_name || !cls_name[0])
                                continue;

                            CLASS_DATA *clazz = class_find_exact(cls_name);
                            if (!clazz)
                                continue;

                            SKILL_SOURCE *src = new_skill_source();
                            src->clazz = clazz;
                            src->scope = (int16_t)json_integer_value(
                                json_object_get(src_elem, "scope"));
                            src->next = NULL;

                            if (!entry->sources)
                                entry->sources = src;
                            else
                                tail->next = src;
                            tail = src;
                        }

                        // Refresh cached fields from loaded sources
                        if (entry->sources) {
                            entry->source_class = entry->sources->clazz;
                            entry->cross_class_scope = REWARD_SCOPE_CLASS;
                            SKILL_SOURCE *s;
                            for (s = entry->sources; s; s = s->next) {
                                if (s->scope > entry->cross_class_scope)
                                    entry->cross_class_scope = s->scope;
                            }
                        }
                    }
                }
            }
        }
    }

    // Read skill groups section
    json_t *skill_groups = json_object_get(root, "skill_groups");
    if (skill_groups && json_is_array(skill_groups)) {
        json_array_foreach(skill_groups, index, array_elem) {
            /* Try name-based resolution first, fall back to integer id */
            const char *gname = json_string_value(json_object_get(array_elem, "name"));
            int gn = -1;

            if (gname && gname[0])
                gn = group_lookup(gname);

            if (gn < 0)
                gn = json_integer_value(json_object_get(array_elem, "id"));

            if (gn >= 0 && gn < MAX_GROUP) {
                ch->pcdata->group_known[gn] = true;

                /* Also populate known_groups LLIST */
                if (group_table[gn].name) {
                    SKILL_GROUP *sg = skill_group_find(group_table[gn].name);
                    if (sg && !list_hasdata(ch->pcdata->known_groups, sg))
                        list_appendlink(ch->pcdata->known_groups, sg);
                }
            }
        }
    }

    // Read affects section
    affects = json_object_get(root, "affects");
    if (affects && json_is_array(affects)) {
        json_array_foreach(affects, index, array_elem) {
            AFFECT_DATA *paf = new_affect();

            value = json_object_get(array_elem, "group");
            if (value) {
                paf->group = json_integer_value(value);
            }

            paf->where = json_integer_value(json_object_get(array_elem, "where"));
            paf->type = json_integer_value(json_object_get(array_elem, "type"));

            // Resolve by name first (resilient to skill reordering)
            str = json_string_value(json_object_get(array_elem, "type_name"));
            if (str && str[0]) {
                SKILL_DATA *sk = skill_find(str);
                if (sk) {
                    if (sk->uid != paf->type) {
                        log_stringf("json_read_char: affect type_name '%s' uid %d != saved type %d for %s (auto-fixed)",
                                    str, sk->uid, paf->type, ch->name);
                        paf->type = sk->uid;
                    }
                    paf->skill = sk;
                } else {
                    log_stringf("json_read_char: affect type_name '%s' not found for %s, falling back to type %d",
                                str, ch->name, paf->type);
                    paf->skill = skill_from_sn(paf->type);
                }
            } else {
                paf->skill = skill_from_sn(paf->type);
            }

            paf->level = json_integer_value(json_object_get(array_elem, "level"));
            paf->duration = json_integer_value(json_object_get(array_elem, "duration"));
            paf->location = json_integer_value(json_object_get(array_elem, "location"));
            paf->modifier = json_integer_value(json_object_get(array_elem, "modifier"));
            paf->bitvector = json_integer_value(json_object_get(array_elem, "bitvector"));

            value = json_object_get(array_elem, "slot");
            if (value) paf->slot = json_integer_value(value);

            value = json_object_get(array_elem, "bitvector2");
            if (value) {
                paf->bitvector2 = json_integer_value(value);
            }

            str = json_string_value(json_object_get(array_elem, "custom_name"));
            if (str) {
                paf->custom_name = str_dup(str);
            }

            paf->next = ch->affected;
            ch->affected = paf;
        }
    }

    // Read tokens section
    json_t *tokens_array = json_object_get(root, "tokens");
    if (tokens_array && json_is_array(tokens_array)) {
        json_array_foreach(tokens_array, index, array_elem) {
            json_t *vnum_val = json_object_get(array_elem, "vnum");
            TOKEN_INDEX_DATA *pTokenIndex = NULL;
            if (json_is_string(vnum_val)) {
                WNUM wnum;
                if (parse_widevnum((char *)json_string_value(vnum_val), NULL, &wnum) && wnum.pArea) {
                    pTokenIndex = get_token_index(wnum.pArea, wnum.vnum);
                }
            } else {
                long vnum = json_integer_value(vnum_val);
                AREA_DATA *pArea = NULL;
                WNUM wnum;
                if (resolve_widevnum(vnum, NULL, &wnum))
                    pArea = wnum.pArea;
                if (!pArea) pArea = get_system_area_fallback();
                pTokenIndex = get_token_index(pArea, vnum);
            }
            if (!pTokenIndex) {
                log_string("json_read_char_internal: bad token vnum");
                continue;
            }

            TOKEN_DATA *token = new_token();
            token->pIndexData = pTokenIndex;

            // Load token ID
            json_t *token_id = json_object_get(array_elem, "id");
            if (token_id && json_is_array(token_id)) {
                token->id[0] = json_integer_value(json_array_get(token_id, 0));
                token->id[1] = json_integer_value(json_array_get(token_id, 1));
            }

            // Load timer
            value = json_object_get(array_elem, "timer");
            if (value) token->timer = json_integer_value(value);

            // Load values
            json_t *values = json_object_get(array_elem, "values");
            if (values && json_is_array(values)) {
                for (int i = 0; i < MAX_TOKEN_VALUES && i < json_array_size(values); i++) {
                    token->value[i] = json_integer_value(json_array_get(values, i));
                }
            }

            // Add to character's token list
            token->next = ch->tokens;
            ch->tokens = token;
        }
    }

    // Read aliases section
    json_t *aliases_array = json_object_get(root, "aliases");
    if (aliases_array && json_is_array(aliases_array) && ch->pcdata) {
        int pos = 0;
        json_array_foreach(aliases_array, index, array_elem) {
            if (pos >= MAX_ALIAS) {
                break;
            }

            str = json_string_value(json_object_get(array_elem, "alias"));
            if (str) {
                free_string(ch->pcdata->alias[pos]);
                ch->pcdata->alias[pos] = str_dup(str);
            }

            str = json_string_value(json_object_get(array_elem, "substitution"));
            if (str) {
                free_string(ch->pcdata->alias_sub[pos]);
                ch->pcdata->alias_sub[pos] = str_dup(str);
            }

            pos++;
        }
    }

    // Read songs section (bard songs learned by name)
    json_t *songs_array = json_object_get(root, "songs");
    if (songs_array && json_is_array(songs_array) && ch->pcdata) {
        json_array_foreach(songs_array, index, array_elem) {
            str = json_string_value(array_elem);
            if (str) {
                SONG_DATA *song = song_lookup((char *)str);
                if (song) {
                    ch->pcdata->songs_learned[song->uid] = true;
                    skill_entry_addsong(ch, song, NULL, SKILLSRC_NORMAL);
                } else {
                    log_stringf("json_read_char: unknown song '%s' for %s", str, ch->name);
                }
            }
        }
    }

    // Read ships section (ship ownership by ID pairs)
    json_t *ships_array = json_object_get(root, "ships");
    if (ships_array && json_is_array(ships_array) && ch->pcdata) {
        json_array_foreach(ships_array, index, array_elem) {
            if (json_is_array(array_elem) && json_array_size(array_elem) >= 2) {
                unsigned long id1 = json_integer_value(json_array_get(array_elem, 0));
                unsigned long id2 = json_integer_value(json_array_get(array_elem, 1));
                SHIP_DATA *ship = find_ship_uid(id1, id2);
                if (IS_VALID(ship)) {
                    list_appendlink(ch->pcdata->ships, ship);
                }
            }
        }
    }

    // Read unlocked areas section (area UIDs)
    json_t *unlocked_array = json_object_get(root, "unlocked_areas");
    if (unlocked_array && json_is_array(unlocked_array) && ch->pcdata) {
        json_array_foreach(unlocked_array, index, array_elem) {
            long uid = json_integer_value(array_elem);
            AREA_DATA *unlocked_area = get_area_from_uid(uid);
            if (unlocked_area) {
                player_unlock_area(ch, unlocked_area);
            }
        }
    }

    gettimeofday(&section_end, NULL);
    section_ms = (section_end.tv_sec - section_start.tv_sec) * 1000 +
                (section_end.tv_usec - section_start.tv_usec) / 1000;
    if (section_ms > 100)
        log_stringf("PERFORMANCE %s: skills/affects/tokens load: %ldms", ch->name, section_ms);

    } // End if (load_heavy) - close the block that started at skills section

    // Load personal trait overrides (lightweight, always loaded)
    if (ch->pcdata) {
        json_t *traits = json_object_get(root, "traits");
        if (traits && json_is_object(traits)) {
            char_init_traits(ch);
            char_load_traits_json(ch, (void *)traits);
        }
    }

    // Load new class system class levels (lightweight, always loaded)
    if (ch->pcdata) {
        json_t *class_levels_arr = json_object_get(root, "class_levels");
        if (class_levels_arr && json_is_array(class_levels_arr)) {
            json_array_foreach(class_levels_arr, index, array_elem) {
                if (!json_is_object(array_elem))
                    continue;

                /* Look up class by UID first, then by name as fallback */
                CLASS_DATA *clazz = NULL;
                value = json_object_get(array_elem, "uid");
                if (value)
                    clazz = class_find_uid((int16_t)json_integer_value(value));
                if (!clazz) {
                    str = json_string_value(json_object_get(array_elem, "name"));
                    if (str)
                        clazz = class_find_exact(str);
                }
                if (!clazz)
                    continue;

                int level = json_integer_value(json_object_get(array_elem, "level"));
                add_class_level(ch, clazz, level);

                /* Retrieve the just-added class level entry for extra data */
                CLASS_LEVEL *cl = get_class_level(ch, clazz);
                if (cl) {
                    value = json_object_get(array_elem, "xp");
                    if (value) cl->xp = json_integer_value(value);

                    str = json_string_value(json_object_get(array_elem, "active_title"));
                    if (str && str[0])
                        cl->active_title = str_dup(str);

                    json_t *custom = json_object_get(array_elem, "custom_data");
                    if (custom && json_is_object(custom))
                        cl->custom_data = json_deep_copy(custom);

                    /* Mark as current class if flagged */
                    value = json_object_get(array_elem, "current");
                    if (value && json_is_true(value))
                        ch->pcdata->current_class = cl;
                }
            }
        }

        /* Load pending free levels (overflow from class max_level changes) */
        value = json_object_get(root, "pending_free_levels");
        if (value)
            ch->pcdata->pending_free_levels = json_integer_value(value);
    }

    // Mark load state
    if (ch->pcdata) {
        ch->pcdata->fully_loaded = load_heavy;
    }

    // Performance logging
    gettimeofday(&end_time, NULL);
    total_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
              (end_time.tv_usec - start_time.tv_usec) / 1000;
    int obj_count = (ch->lcarrying ? list_size(ch->lcarrying) : 0) +
                   (ch->llocker ? list_size(ch->llocker) : 0) +
                   (ch->lworn ? list_size(ch->lworn) : 0);
    log_stringf("PERFORMANCE json_read_char_internal: %s with %d objects (%s) - total: %ldms [loaded_objects: %d]",
               ch->name, obj_count, load_heavy ? "full" : "basic", total_ms,
               loaded_objects ? list_size(loaded_objects) : 0);

    log_stringf("JSON: Loaded character %s from %s (%s)", ch->name, source_name,
                load_heavy ? "full" : "basic");
    return true;
}

// Public wrapper for full character load
bool json_read_char(CHAR_DATA *ch, const char *filename)
{
    return json_read_char_internal(ch, filename, true);
}

/***************************************************************************
 * Basic Character Loading (Defers Heavy Data)                             *
 ***************************************************************************/

// Load character WITHOUT inventory/equipment/skills/affects
// This is significantly faster and used for character menu display
bool json_read_char_basic(CHAR_DATA *ch, const char *filename)
{
    bool result = json_read_char_internal(ch, filename, false);
    if (result && ch->pcdata) {
        ch->pcdata->fully_loaded = false; // Mark as partially loaded
    }
    return result;
}

/***************************************************************************
 * Direct JSON Object Functions (for Redis cache)                          *
 ***************************************************************************/

// Read full character from JSON object (for Redis cache)
// Caller retains ownership of root - this function does not decref it
bool json_read_char_from_json(CHAR_DATA *ch, json_t *root)
{
    return json_read_char_internal_from_json(ch, root, true, "redis");
}

// Read basic character data from JSON object (for Redis cache)
// Caller retains ownership of root - this function does not decref it
bool json_read_char_basic_from_json(CHAR_DATA *ch, json_t *root)
{
    bool result = json_read_char_internal_from_json(ch, root, false, "redis");
    if (result && ch->pcdata) {
        ch->pcdata->fully_loaded = false; // Mark as partially loaded
    }
    return result;
}

// Load remaining character data from JSON object (for Redis cache)
// Caller retains ownership of root - this function does not decref it
bool json_read_char_remaining_from_json(CHAR_DATA *ch, json_t *root)
{
    json_t *inventory, *equipment, *locker, *skills, *affects;
    json_t *value, *array_elem;
    const char *str;
    size_t index;
    struct timeval start_time, end_time;
    long total_ms;

    if (!ch || !ch->pcdata) {
        return false;
    }

    // If already fully loaded, nothing to do
    if (ch->pcdata->fully_loaded) {
        return true;
    }

    // Start timing
    gettimeofday(&start_time, NULL);

    log_stringf("JSON: Loading remaining data for %s from redis (inventory/equipment/skills/affects)", ch->name);

    // Read inventory section
    inventory = json_object_get(root, "inventory");
    if (inventory && json_is_array(inventory)) {
        json_array_foreach(inventory, index, array_elem) {
            OBJ_DATA *obj = json_to_obj(array_elem, ch);
            if (obj) {
                obj_to_char(obj, ch);
            }
        }
    }

    // Read equipment section
    equipment = json_object_get(root, "equipment");
    if (equipment && json_is_array(equipment)) {
        json_array_foreach(equipment, index, array_elem) {
            OBJ_DATA *obj = json_to_obj(array_elem, ch);
            if (obj) {
                obj_to_char(obj, ch);
                // Add to worn list if equipped (wear_loc is set in json_to_obj)
                if (obj->wear_loc != WEAR_NONE) {
                    list_addlink(ch->lworn, obj);
                }
            }
        }
    }

    // Read locker section
    locker = json_object_get(root, "locker");
    if (locker && json_is_array(locker)) {
        json_array_foreach(locker, index, array_elem) {
            OBJ_DATA *obj = json_to_obj(array_elem, ch);
            if (obj) {
                // Use obj_to_locker() which properly adds to ch->llocker
                obj_to_locker(obj, ch);
            }
        }
    }

    // Read skills section
    skills = json_object_get(root, "skills");
    if (skills && json_is_object(skills)) {
        const char *skill_key;
        json_t *skill_value;
        SKILL_ENTRY *entry;
        json_object_foreach(skills, skill_key, skill_value) {
            int sn;

            // Use hash-based exact match first (O(1)), fall back to prefix search
            SKILL_DATA *sk_found = skill_find(skill_key);
            sn = sk_found ? sk_found->uid : -1;
            if (sn < 0)
                sn = skill_lookup(skill_key);
            if (sn < 0) {
                sn = atoi(skill_key);
                if (sn < 0 || sn >= MAX_SKILL) {
                    continue;
                }
            }

            if (json_is_object(skill_value)) {
                json_t *learned = json_object_get(skill_value, "learned");
                if (learned) {
                    ch->pcdata->learned[sn] = json_integer_value(learned);
                }
                json_t *mod_learned = json_object_get(skill_value, "mod_learned");
                if (mod_learned) {
                    ch->pcdata->mod_learned[sn] = json_integer_value(mod_learned);
                }
            } else {
                ch->pcdata->learned[sn] = json_integer_value(skill_value);
            }

            // Parse source and flags from JSON (defaults match old behavior)
            char source = SKILLSRC_NORMAL;
            long flags = SKILL_AUTOMATIC;
            if (json_is_object(skill_value)) {
                const char *source_str = json_string_value(json_object_get(skill_value, "source"));
                source = json_parse_skill_source(source_str);

                const char *flags_str = json_string_value(json_object_get(skill_value, "flags"));
                if (flags_str) {
                    flags = flag_value(skill_flags, (char *)flags_str);
                    if (flags == NO_FLAG) flags = SKILL_AUTOMATIC;
                }
            }

            // Add to sorted_skills list so 'skills'/'spells' commands work
            if (skill_table[sn].spell_fun == spell_null)
                skill_entry_addskill(ch, sn, NULL, source, flags);
            else
                skill_entry_addspell(ch, sn, NULL, source, flags);

            // Populate entry rating fields from loaded learned[] data
            entry = skill_entry_findsn(ch->sorted_skills, sn);
            if (entry) {
                entry->rating = ch->pcdata->learned[sn];
                entry->mod_rating = ch->pcdata->mod_learned[sn];

                // Load class sources list
                if (json_is_object(skill_value)) {
                    json_t *class_sources = json_object_get(skill_value, "class_sources");
                    if (class_sources && json_is_array(class_sources)) {
                        size_t si;
                        json_t *src_elem;
                        SKILL_SOURCE *tail = NULL;

                        json_array_foreach(class_sources, si, src_elem) {
                            const char *cls_name = json_string_value(
                                json_object_get(src_elem, "class"));
                            if (!cls_name || !cls_name[0])
                                continue;

                            CLASS_DATA *clazz = class_find_exact(cls_name);
                            if (!clazz)
                                continue;

                            SKILL_SOURCE *src = new_skill_source();
                            src->clazz = clazz;
                            src->scope = (int16_t)json_integer_value(
                                json_object_get(src_elem, "scope"));
                            src->next = NULL;

                            if (!entry->sources)
                                entry->sources = src;
                            else
                                tail->next = src;
                            tail = src;
                        }

                        // Refresh cached fields from loaded sources
                        if (entry->sources) {
                            entry->source_class = entry->sources->clazz;
                            entry->cross_class_scope = REWARD_SCOPE_CLASS;
                            SKILL_SOURCE *s;
                            for (s = entry->sources; s; s = s->next) {
                                if (s->scope > entry->cross_class_scope)
                                    entry->cross_class_scope = s->scope;
                            }
                        }
                    }
                }
            }
        }
    }

    // Read skill groups section
    json_t *skill_groups = json_object_get(root, "skill_groups");
    if (skill_groups && json_is_array(skill_groups)) {
        json_array_foreach(skill_groups, index, array_elem) {
            /* Try name-based resolution first, fall back to integer id */
            const char *gname = json_string_value(json_object_get(array_elem, "name"));
            int gn = -1;

            if (gname && gname[0])
                gn = group_lookup(gname);

            if (gn < 0)
                gn = json_integer_value(json_object_get(array_elem, "id"));

            if (gn >= 0 && gn < MAX_GROUP) {
                ch->pcdata->group_known[gn] = true;

                /* Also populate known_groups LLIST */
                if (group_table[gn].name) {
                    SKILL_GROUP *sg = skill_group_find(group_table[gn].name);
                    if (sg && !list_hasdata(ch->pcdata->known_groups, sg))
                        list_appendlink(ch->pcdata->known_groups, sg);
                }
            }
        }
    }

    // Read affects section
    affects = json_object_get(root, "affects");
    if (affects && json_is_array(affects)) {
        json_array_foreach(affects, index, array_elem) {
            AFFECT_DATA *paf = new_affect();

            value = json_object_get(array_elem, "group");
            if (value) {
                paf->group = json_integer_value(value);
            }

            paf->where = json_integer_value(json_object_get(array_elem, "where"));
            paf->type = json_integer_value(json_object_get(array_elem, "type"));

            // Resolve by name first (resilient to skill reordering)
            str = json_string_value(json_object_get(array_elem, "type_name"));
            if (str && str[0]) {
                SKILL_DATA *sk = skill_find(str);
                if (sk) {
                    if (sk->uid != paf->type) {
                        log_stringf("json_read_char: affect type_name '%s' uid %d != saved type %d for %s (auto-fixed)",
                                    str, sk->uid, paf->type, ch->name);
                        paf->type = sk->uid;
                    }
                    paf->skill = sk;
                } else {
                    log_stringf("json_read_char: affect type_name '%s' not found for %s, falling back to type %d",
                                str, ch->name, paf->type);
                    paf->skill = skill_from_sn(paf->type);
                }
            } else {
                paf->skill = skill_from_sn(paf->type);
            }

            paf->level = json_integer_value(json_object_get(array_elem, "level"));
            paf->duration = json_integer_value(json_object_get(array_elem, "duration"));
            paf->location = json_integer_value(json_object_get(array_elem, "location"));
            paf->modifier = json_integer_value(json_object_get(array_elem, "modifier"));
            paf->bitvector = json_integer_value(json_object_get(array_elem, "bitvector"));

            value = json_object_get(array_elem, "slot");
            if (value) paf->slot = json_integer_value(value);

            value = json_object_get(array_elem, "bitvector2");
            if (value) {
                paf->bitvector2 = json_integer_value(value);
            }

            str = json_string_value(json_object_get(array_elem, "custom_name"));
            if (str) {
                paf->custom_name = str_dup(str);
            }

            paf->next = ch->affected;
            ch->affected = paf;
        }
    }

    // Read tokens section
    json_t *tokens_array = json_object_get(root, "tokens");
    if (tokens_array && json_is_array(tokens_array)) {
        json_array_foreach(tokens_array, index, array_elem) {
            json_t *vnum_val = json_object_get(array_elem, "vnum");
            TOKEN_INDEX_DATA *pTokenIndex = NULL;
            if (json_is_string(vnum_val)) {
                WNUM wnum;
                if (parse_widevnum((char *)json_string_value(vnum_val), NULL, &wnum) && wnum.pArea) {
                    pTokenIndex = get_token_index(wnum.pArea, wnum.vnum);
                }
            } else {
                long vnum = json_integer_value(vnum_val);
                AREA_DATA *pArea = NULL;
                WNUM wnum;
                if (resolve_widevnum(vnum, NULL, &wnum))
                    pArea = wnum.pArea;
                if (!pArea) pArea = get_system_area_fallback();
                pTokenIndex = get_token_index(pArea, vnum);
            }
            if (!pTokenIndex) {
                log_string("json_read_char_remaining: bad token vnum");
                continue;
            }

            TOKEN_DATA *token = new_token();
            token->pIndexData = pTokenIndex;

            json_t *token_id = json_object_get(array_elem, "id");
            if (token_id && json_is_array(token_id)) {
                token->id[0] = json_integer_value(json_array_get(token_id, 0));
                token->id[1] = json_integer_value(json_array_get(token_id, 1));
            }

            value = json_object_get(array_elem, "timer");
            if (value) token->timer = json_integer_value(value);

            json_t *values = json_object_get(array_elem, "values");
            if (values && json_is_array(values)) {
                for (int i = 0; i < MAX_TOKEN_VALUES && i < json_array_size(values); i++) {
                    token->value[i] = json_integer_value(json_array_get(values, i));
                }
            }

            // Script variables
            value = json_object_get(array_elem, "variables");
            if (value && json_is_array(value)) {
                json_persist_json_to_scriptdata(value, &token->progs);
            }

            token->next = ch->tokens;
            ch->tokens = token;
        }
    }

    // Read aliases section
    json_t *aliases_array = json_object_get(root, "aliases");
    if (aliases_array && json_is_array(aliases_array) && ch->pcdata) {
        int pos = 0;
        json_array_foreach(aliases_array, index, array_elem) {
            if (pos >= MAX_ALIAS) {
                break;
            }

            str = json_string_value(json_object_get(array_elem, "alias"));
            if (str) {
                free_string(ch->pcdata->alias[pos]);
                ch->pcdata->alias[pos] = str_dup(str);
            }

            str = json_string_value(json_object_get(array_elem, "substitution"));
            if (str) {
                free_string(ch->pcdata->alias_sub[pos]);
                ch->pcdata->alias_sub[pos] = str_dup(str);
            }

            pos++;
        }
    }

    // Read songs section (bard songs learned by name)
    json_t *songs_array = json_object_get(root, "songs");
    if (songs_array && json_is_array(songs_array) && ch->pcdata) {
        json_array_foreach(songs_array, index, array_elem) {
            str = json_string_value(array_elem);
            if (str) {
                SONG_DATA *song = song_lookup((char *)str);
                if (song) {
                    ch->pcdata->songs_learned[song->uid] = true;
                    skill_entry_addsong(ch, song, NULL, SKILLSRC_NORMAL);
                } else {
                    log_stringf("json_read_char_remaining: unknown song '%s' for %s", str, ch->name);
                }
            }
        }
    }

    // Read ships section (ship ownership by ID pairs)
    json_t *ships_array = json_object_get(root, "ships");
    if (ships_array && json_is_array(ships_array) && ch->pcdata) {
        json_array_foreach(ships_array, index, array_elem) {
            if (json_is_array(array_elem) && json_array_size(array_elem) >= 2) {
                unsigned long id1 = json_integer_value(json_array_get(array_elem, 0));
                unsigned long id2 = json_integer_value(json_array_get(array_elem, 1));
                SHIP_DATA *ship = find_ship_uid(id1, id2);
                if (IS_VALID(ship)) {
                    list_appendlink(ch->pcdata->ships, ship);
                }
            }
        }
    }

    // Read unlocked areas section (area UIDs)
    json_t *unlocked_array = json_object_get(root, "unlocked_areas");
    if (unlocked_array && json_is_array(unlocked_array) && ch->pcdata) {
        json_array_foreach(unlocked_array, index, array_elem) {
            long uid = json_integer_value(array_elem);
            AREA_DATA *unlocked_area = get_area_from_uid(uid);
            if (unlocked_area) {
                player_unlock_area(ch, unlocked_area);
            }
        }
    }

    // Mark as fully loaded
    ch->pcdata->fully_loaded = true;

    // Performance logging
    gettimeofday(&end_time, NULL);
    total_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
              (end_time.tv_usec - start_time.tv_usec) / 1000;
    int obj_count = (ch->lcarrying ? list_size(ch->lcarrying) : 0) +
                   (ch->llocker ? list_size(ch->llocker) : 0) +
                   (ch->lworn ? list_size(ch->lworn) : 0);
    log_stringf("PERFORMANCE json_read_char_remaining: %s with %d objects - total: %ldms [loaded_objects: %d]",
               ch->name, obj_count, total_ms,
               loaded_objects ? list_size(loaded_objects) : 0);

    log_stringf("JSON: Completed loading remaining data for %s", ch->name);
    return true;
}

// Load remaining character data after json_read_char_basic() - file-based wrapper
// Call this when character actually enters the game
bool json_read_char_remaining(CHAR_DATA *ch, const char *filename)
{
    json_t *root;
    json_error_t error;
    bool result;

    if (!ch || !ch->pcdata) {
        return false;
    }

    // If already fully loaded, nothing to do
    if (ch->pcdata->fully_loaded) {
        return true;
    }

    // Load JSON file
    root = json_load_file(filename, 0, &error);
    if (!root) {
        log_stringf("json_read_char_remaining: Failed to parse %s: %s", filename, error.text);
        return false;
    }

    // Delegate to the from_json implementation
    result = json_read_char_remaining_from_json(ch, root);

    json_decref(root);
    return result;
}

bool json_read_char_inventory(CHAR_DATA *ch, json_t *root)
{
    // Inventory reading is now part of json_read_char()
    return true;
}

bool json_read_char_locker(CHAR_DATA *ch, json_t *root)
{
    // Locker reading is now part of json_read_char()
    return true;
}

bool json_read_char_equipment(CHAR_DATA *ch, json_t *root)
{
    // Equipment reading is now part of json_read_char()
    return true;
}
