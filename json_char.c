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
#include "merc.h"
#include "tables.h"
#include "recycle.h"
#include "json_char.h"
#include "json_persist.h"
#include "redis_cache.h"
#include "wilds.h"

/***************************************************************************
 * External Flag Tables                                                    *
 ***************************************************************************/

extern const struct flag_type act_flags[];
extern const struct flag_type act2_flags[];
extern const struct flag_type plr_flags[];
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
    char dir_path[256];

    snprintf(dir_path, sizeof(dir_path), "%s%c",
             PLAYER_DIR, tolower(char_name[0]));

    // Try to create directory (will fail if exists, which is fine)
    mkdir(dir_path, 0755);
    return true;
}

bool json_is_json_file(const char *filename)
{
    FILE *fp;
    char first_char;
    bool is_json;

    fp = fopen(filename, "r");
    if (!fp) {
        return false;  // File doesn't exist
    }

    // JSON files start with '{'
    first_char = fgetc(fp);
    is_json = (first_char == '{');
    fclose(fp);

    return is_json;
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
    AFFECT_DATA *paf;
    EXTRA_DESCR_DATA *ed;
    SPELL_DATA *spell;
    TOKEN_DATA *token;
    int i;

    if (!obj) {
        return NULL;
    }

    json_obj = json_object();

    // Basic identification
    json_object_set_new(json_obj, "vnum", json_integer(obj->pIndexData->vnum));
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

    // Values (only if different from prototype)
    json_t *values_array = json_array();
    bool has_custom_values = false;
    for (i = 0; i < 8; i++) {
        if (obj->value[i] != obj->pIndexData->value[i]) {
            has_custom_values = true;
        }
        json_array_append_new(values_array, json_integer(obj->value[i]));
    }
    if (has_custom_values) {
        json_object_set_new(json_obj, "values", values_array);
    } else {
        json_decref(values_array);
    }

    // Owner
    if (obj->owner) {
        json_object_set_new(json_obj, "owner", json_string(obj->owner));
    }

    // Material
    if (obj->material && (!obj->pIndexData->material || str_cmp(obj->material, obj->pIndexData->material))) {
        json_object_set_new(json_obj, "material", json_string(obj->material));
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

    // Affects
    affects_array = json_array();
    for (paf = obj->affected; paf; paf = paf->next) {
        json_t *aff = json_object();
        json_object_set_new(aff, "type", json_integer(paf->type));
        json_object_set_new(aff, "level", json_integer(paf->level));
        json_object_set_new(aff, "duration", json_integer(paf->duration));
        json_object_set_new(aff, "location", json_integer(paf->location));
        json_object_set_new(aff, "modifier", json_integer(paf->modifier));
        json_object_set_new(aff, "bitvector", json_integer(paf->bitvector));
        if (paf->bitvector2) {
            json_object_set_new(aff, "bitvector2", json_integer(paf->bitvector2));
        }
        json_array_append_new(affects_array, aff);
    }
    if (json_array_size(affects_array) > 0) {
        json_object_set_new(json_obj, "affects", affects_array);
    } else {
        json_decref(affects_array);
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
    int total_in_list = 0;
    int written_count = 0;

    inventory = json_array();

    // Use lcarrying (LIST structure) instead of carrying (deprecated linked list)
    if (ch->lcarrying && IS_VALID(ch->lcarrying)) {
        total_in_list = list_size(ch->lcarrying);
        log_stringf("inventory_to_json: %s has %d items in lcarrying", ch->name, total_in_list);

        iterator_start(&it, ch->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            // Only write top-level inventory items (not equipped, not in locker, not in containers)
            if (!obj->locker && obj->in_obj == NULL && obj->wear_loc == WEAR_NONE) {
                json_t *json_obj = obj_to_json(obj, 0);
                if (json_obj) {
                    json_array_append_new(inventory, json_obj);
                    written_count++;
                    log_stringf("  inventory_to_json: wrote vnum=%ld name='%s'",
                               obj->pIndexData->vnum, obj->name ? obj->name : "(null)");
                }
            } else {
                log_stringf("  inventory_to_json: SKIPPED vnum=%ld name='%s' locker=%d in_obj=%s wear_loc=%d",
                           obj->pIndexData->vnum, obj->name ? obj->name : "(null)",
                           obj->locker, obj->in_obj ? "yes" : "no", obj->wear_loc);
            }
        }
        iterator_stop(&it);
        log_stringf("inventory_to_json: wrote %d top-level items for %s", written_count, ch->name);
    } else {
        log_stringf("inventory_to_json: %s has no lcarrying list!", ch->name);
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

/***************************************************************************
 * Flag Serialization Helpers - Convert flags to/from human-readable names *
 ***************************************************************************/

static json_t *flags_to_json_array(const struct flag_type *flag_table, long bits)
{
    json_t *flags_array;
    int i;

    if (!flag_table) {
        return json_array();
    }

    flags_array = json_array();

    // Convert each set bit to its flag name
    for (i = 0; flag_table[i].name != NULL; i++) {
        if (!is_stat(flag_table) && IS_SET(bits, flag_table[i].bit)) {
            json_array_append_new(flags_array, json_string(flag_table[i].name));
        } else if (flag_table[i].bit == bits) {
            json_array_append_new(flags_array, json_string(flag_table[i].name));
            break;
        }
    }

    return flags_array;
}

static long flags_from_json_array(const struct flag_type *flag_table, json_t *flags_array)
{
    long bits = 0;
    size_t index;
    json_t *value;
    const char *flag_name;

    if (!flag_table || !flags_array || !json_is_array(flags_array)) {
        return 0;
    }

    // Convert each flag name to its bit value
    json_array_foreach(flags_array, index, value) {
        flag_name = json_string_value(value);
        if (!flag_name) continue;

        // Look up flag by name
        for (int i = 0; flag_table[i].name != NULL; i++) {
            if (!str_cmp(flag_table[i].name, flag_name)) {
                SET_BIT(bits, flag_table[i].bit);
                break;
            }
        }
    }

    return bits;
}

/***************************************************************************
 * Skills Serialization - WITH HUMAN-READABLE NAMES                       *
 ***************************************************************************/

static json_t *skills_to_json(CHAR_DATA *ch)
{
    json_t *skills;
    int sn;

    if (!ch->pcdata) {
        return json_object();
    }

    skills = json_object();

    // Save skills with skill NAME as key (more robust than numeric ID)
    for (sn = 0; sn < MAX_SKILL; sn++) {
        if ((ch->pcdata->learned[sn] > 0 || ch->pcdata->mod_learned[sn] != 0) && skill_table[sn].name) {
            json_t *skill_data = json_object();

            if (ch->pcdata->learned[sn] > 0) {
                json_object_set_new(skill_data, "learned", json_integer(ch->pcdata->learned[sn]));
            }

            // Add skill modifiers (if any)
            if (ch->pcdata->mod_learned[sn] != 0) {
                json_object_set_new(skill_data, "mod_learned", json_integer(ch->pcdata->mod_learned[sn]));
            }

            // Use skill name as key (robust against ID changes)
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
        json_object_set_new(token_obj, "vnum", json_integer(token->pIndexData->vnum));
        json_object_set_new(token_obj, "id", json_pack("[i, i]",
            (int)token->id[0], (int)token->id[1]));
        json_object_set_new(token_obj, "timer", json_integer(token->timer));

        // Token values
        json_t *values = json_array();
        for (int i = 0; i < MAX_TOKEN_VALUES; i++) {
            json_array_append_new(values, json_integer(token->value[i]));
        }
        json_object_set_new(token_obj, "values", values);

        // TODO: Token variables (if needed)

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

    // Save learned skill groups with human-readable names
    for (gn = 0; gn < MAX_GROUP; gn++) {
        if (ch->pcdata->group_known[gn] && group_table[gn].name) {
            json_t *group_data = json_object();
            json_object_set_new(group_data, "id", json_integer(gn));
            json_object_set_new(group_data, "name", json_string(group_table[gn].name));
            json_array_append_new(groups, group_data);
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
        json_object_set_new(aff, "level", json_integer(paf->level));
        json_object_set_new(aff, "duration", json_integer(paf->duration));
        json_object_set_new(aff, "location", json_integer(paf->location));
        json_object_set_new(aff, "modifier", json_integer(paf->modifier));
        json_object_set_new(aff, "bitvector", json_integer(paf->bitvector));

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
    json_object_set_new(meta, "format_version", json_integer(2));

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
    json_object_set_new(basic, "sex", json_integer(ch->sex));
    json_object_set_new(basic, "body_type", json_integer(ch->body_type));

    // **FIX #1: Save ALL character flags as human-readable arrays**
    json_object_set_new(basic, "act_flags", flags_to_json_array(act_flags, ch->act[0]));
    json_object_set_new(basic, "act2_flags", flags_to_json_array(act2_flags, ch->act[1]));
    json_object_set_new(basic, "comm_flags", flags_to_json_array(comm_flags, ch->comm));
    if (ch->pcdata) {
        json_object_set_new(basic, "channel_flags", flags_to_json_array(channel_flags, ch->pcdata->channel_flags));
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
    json_object_set_new(basic, "affected_by_flags", flags_to_json_array(affect_flags, ch->affected_by[0]));
    json_object_set_new(basic, "affected_by2_flags", flags_to_json_array(affect2_flags, ch->affected_by[1]));
    json_object_set_new(basic, "affected_by_perm_flags", flags_to_json_array(affect_flags, ch->affected_by_perm[0]));
    json_object_set_new(basic, "affected_by_perm2_flags", flags_to_json_array(affect2_flags, ch->affected_by_perm[1]));

    // Also save numeric for backward compatibility
    json_object_set_new(basic, "affected_by", json_integer(ch->affected_by[0]));
    json_object_set_new(basic, "affected_by2", json_integer(ch->affected_by[1]));
    json_object_set_new(basic, "affected_by_perm", json_integer(ch->affected_by_perm[0]));
    json_object_set_new(basic, "affected_by_perm2", json_integer(ch->affected_by_perm[1]));

    // *** RESISTANCE/IMMUNITY/VULNERABILITY FLAGS (human-readable) ***
    json_object_set_new(basic, "imm_flags_names", flags_to_json_array(imm_flags, ch->imm_flags));
    json_object_set_new(basic, "imm_flags_perm_names", flags_to_json_array(imm_flags, ch->imm_flags_perm));
    json_object_set_new(basic, "res_flags_names", flags_to_json_array(res_flags, ch->res_flags));
    json_object_set_new(basic, "res_flags_perm_names", flags_to_json_array(res_flags, ch->res_flags_perm));
    json_object_set_new(basic, "vuln_flags_names", flags_to_json_array(vuln_flags, ch->vuln_flags));
    json_object_set_new(basic, "vuln_flags_perm_names", flags_to_json_array(vuln_flags, ch->vuln_flags_perm));

    // Also save numeric for backward compatibility
    json_object_set_new(basic, "imm_flags", json_integer(ch->imm_flags));
    json_object_set_new(basic, "imm_flags_perm", json_integer(ch->imm_flags_perm));
    json_object_set_new(basic, "res_flags", json_integer(ch->res_flags));
    json_object_set_new(basic, "res_flags_perm", json_integer(ch->res_flags_perm));
    json_object_set_new(basic, "vuln_flags", json_integer(ch->vuln_flags));
    json_object_set_new(basic, "vuln_flags_perm", json_integer(ch->vuln_flags_perm));

    // Lost body parts (human-readable)
    if (ch->lostparts != 0) {
        json_object_set_new(basic, "lostparts_names", flags_to_json_array(part_flags, ch->lostparts));
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
        json_object_set_new(basic, "played_hours", json_integer(ch->played + (int)(current_time - ch->logon) / 3600));
        json_object_set_new(basic, "last_login", json_integer(ch->pcdata->last_login));

        // *** BANK BALANCE - CRITICAL! ***
        json_object_set_new(basic, "bankbalance", json_integer(ch->pcdata->bankbalance));

        // *** PC_DATA FIELDS ***
        json_object_set_new(basic, "true_sex", json_integer(ch->pcdata->true_sex));
        json_object_set_new(basic, "last_level", json_integer(ch->pcdata->last_level));
        json_object_set_new(basic, "quests_completed", json_integer(ch->pcdata->quests_completed));
        json_object_set_new(basic, "security", json_integer(ch->pcdata->security));

        // *** USER PREFERENCES ***
        json_object_set_new(basic, "scroll_lines", json_integer(ch->lines)); // Page length
        json_object_set_new(basic, "prompt", json_string(ch->prompt ? ch->prompt : ""));
        json_object_set_new(basic, "verb_preference", json_integer(ch->verb_preference));

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
        // Character is in regular room
        json_object_set_new(position, "type", json_string("room"));
        json_object_set_new(position, "room_vnum", json_integer(ch->in_room->vnum));
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
        json_t *quest = json_object();
        json_object_set_new(quest, "questgiver_type", json_integer(ch->quest->questgiver_type));
        json_object_set_new(quest, "questgiver", json_integer(ch->quest->questgiver));
        json_object_set_new(quest, "questreceiver_type", json_integer(ch->quest->questreceiver_type));
        json_object_set_new(quest, "questreceiver", json_integer(ch->quest->questreceiver));
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
                json_object_set_new(part_obj, "obj", json_integer(part->obj));
                json_object_set_new(part_obj, "mob", json_integer(part->mob));
                json_object_set_new(part_obj, "room", json_integer(part->room));
                json_object_set_new(part_obj, "obj_sac", json_integer(part->obj_sac));
                json_object_set_new(part_obj, "mob_rescue", json_integer(part->mob_rescue));
                json_object_set_new(part_obj, "custom_task", json_boolean(part->custom_task));
                json_object_set_new(part_obj, "complete", json_boolean(part->complete));
                if (part->description) {
                    json_object_set_new(part_obj, "description", json_string(part->description));
                }
                // For pickup quests, save the object vnum and room vnum
                if (part->pObj && part->pObj->in_room && !part->complete) {
                    json_object_set_new(part_obj, "pobj_vnum", json_integer(part->pObj->pIndexData->vnum));
                    json_object_set_new(part_obj, "pobj_room", json_integer(part->pObj->in_room->vnum));
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

    // TODO: Add in future if needed:
    // - variables (script variables)
    // - other player-specific data

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

    // Get vnum
    vnum = json_integer_value(json_object_get(json_obj, "vnum"));
    pObjIndex = get_obj_index((find_area_by_vnum(vnum, NULL) ?: get_system_area_fallback()), vnum);
    if (!pObjIndex) {
        log_stringf("json_to_obj: bad vnum %ld", vnum);
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

    // Affects
    value = json_object_get(json_obj, "affects");
    if (value && json_is_array(value)) {
        json_array_foreach(value, index, array_elem) {
            AFFECT_DATA *paf = new_affect();
            paf->type = json_integer_value(json_object_get(array_elem, "type"));
            paf->level = json_integer_value(json_object_get(array_elem, "level"));
            paf->duration = json_integer_value(json_object_get(array_elem, "duration"));
            paf->location = json_integer_value(json_object_get(array_elem, "location"));
            paf->modifier = json_integer_value(json_object_get(array_elem, "modifier"));
            paf->bitvector = json_integer_value(json_object_get(array_elem, "bitvector"));

            json_t *bv2 = json_object_get(array_elem, "bitvector2");
            if (bv2) {
                paf->bitvector2 = json_integer_value(bv2);
            }

            paf->next = obj->affected;
            obj->affected = paf;
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

    // CRITICAL: Add object to loaded_objects and assign ID
    // This matches the behavior of fread_obj_new() in save.c
    // Without this, objects won't be:
    // - Found by owhere, get_obj_world, etc.
    // - Saved when character is saved
    // - Properly tracked by the game
    if (!list_haslink(loaded_objects, obj)) {
        list_appendlink(loaded_objects, obj);
        obj->pIndexData->count++;
        log_stringf("json_to_obj: Added vnum=%ld '%s' to loaded_objects (id=%lu/%lu)",
                   obj->pIndexData->vnum, obj->short_descr ? obj->short_descr : "(null)",
                   obj->id[0], obj->id[1]);
    }

    // Fix for scrolls/potions that have generic names - derive name from short_descr
    // This matches the VERSION_PLAYER_006 fix in the pfile loading code
    if (obj->pIndexData->vnum == get_reserved_vnum("obj_scroll")) {
        if (!strcmp(obj->name, "scroll")) {
            free_string(obj->name);
            obj->name = short_to_name(obj->short_descr);
            log_stringf("json_to_obj: Fixed scroll name from short_descr, now '%s'", obj->name);
        }
    }
    if (obj->pIndexData->vnum == get_reserved_vnum("obj_potion")) {
        if (!strcmp(obj->name, "potion")) {
            free_string(obj->name);
            obj->name = short_to_name(obj->short_descr);
            log_stringf("json_to_obj: Fixed potion name from short_descr, now '%s'", obj->name);
        }
    }

    // Assign a unique object ID if not already set
    get_obj_id(obj);

    // Apply object fixes (times_allowed_fixed, etc.)
    obj->times_allowed_fixed = obj->pIndexData->times_allowed_fixed;
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

    // Read metadata section
    metadata = json_object_get(root, "metadata");
    if (metadata) {
        value = json_object_get(metadata, "created");
        if (value) {
            ch->pcdata->creation_date = json_integer_value(value);
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

    ch->sex = json_integer_value(json_object_get(character, "sex"));
    ch->body_type = json_integer_value(json_object_get(character, "body_type"));

    // **FIX #1: Load ALL character flags (prefer human-readable arrays, fall back to numeric)**
    // Try loading from flag name arrays first
    value = json_object_get(character, "act_flags");
    if (value && json_is_array(value)) {
        ch->act[0] = flags_from_json_array(act_flags, value);
    } else {
        // Fall back to numeric format for backward compatibility
        value = json_object_get(character, "act");
        if (value) ch->act[0] = json_integer_value(value);
    }

    value = json_object_get(character, "act2_flags");
    if (value && json_is_array(value)) {
        ch->act[1] = flags_from_json_array(act2_flags, value);
    } else {
        value = json_object_get(character, "act2");
        if (value) ch->act[1] = json_integer_value(value);
    }

    value = json_object_get(character, "comm_flags");
    if (value && json_is_array(value)) {
        ch->comm = flags_from_json_array(comm_flags, value);
    } else {
        value = json_object_get(character, "comm");
        if (value) ch->comm = json_integer_value(value);
    }

    if (ch->pcdata) {
        value = json_object_get(character, "channel_flags");
        if (value && json_is_array(value)) {
            ch->pcdata->channel_flags = flags_from_json_array(channel_flags, value);
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

    ch->played = json_integer_value(json_object_get(character, "played_hours"));

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
        ch->affected_by[0] = flags_from_json_array(affect_flags, value);
    } else {
        value = json_object_get(character, "affected_by");
        if (value) ch->affected_by[0] = json_integer_value(value);
    }

    value = json_object_get(character, "affected_by2_flags");
    if (value && json_is_array(value)) {
        ch->affected_by[1] = flags_from_json_array(affect2_flags, value);
    } else {
        value = json_object_get(character, "affected_by2");
        if (value) ch->affected_by[1] = json_integer_value(value);
    }

    value = json_object_get(character, "affected_by_perm_flags");
    if (value && json_is_array(value)) {
        ch->affected_by_perm[0] = flags_from_json_array(affect_flags, value);
    } else {
        value = json_object_get(character, "affected_by_perm");
        if (value) ch->affected_by_perm[0] = json_integer_value(value);
    }

    value = json_object_get(character, "affected_by_perm2_flags");
    if (value && json_is_array(value)) {
        ch->affected_by_perm[1] = flags_from_json_array(affect2_flags, value);
    } else {
        value = json_object_get(character, "affected_by_perm2");
        if (value) ch->affected_by_perm[1] = json_integer_value(value);
    }

    // *** RESISTANCE/IMMUNITY/VULNERABILITY FLAGS (prefer arrays, fall back to numeric) ***
    value = json_object_get(character, "imm_flags_names");
    if (value && json_is_array(value)) {
        ch->imm_flags = flags_from_json_array(imm_flags, value);
    } else {
        value = json_object_get(character, "imm_flags");
        if (value) ch->imm_flags = json_integer_value(value);
    }

    value = json_object_get(character, "imm_flags_perm_names");
    if (value && json_is_array(value)) {
        ch->imm_flags_perm = flags_from_json_array(imm_flags, value);
    } else {
        value = json_object_get(character, "imm_flags_perm");
        if (value) ch->imm_flags_perm = json_integer_value(value);
    }

    value = json_object_get(character, "res_flags_names");
    if (value && json_is_array(value)) {
        ch->res_flags = flags_from_json_array(res_flags, value);
    } else {
        value = json_object_get(character, "res_flags");
        if (value) ch->res_flags = json_integer_value(value);
    }

    value = json_object_get(character, "res_flags_perm_names");
    if (value && json_is_array(value)) {
        ch->res_flags_perm = flags_from_json_array(res_flags, value);
    } else {
        value = json_object_get(character, "res_flags_perm");
        if (value) ch->res_flags_perm = json_integer_value(value);
    }

    value = json_object_get(character, "vuln_flags_names");
    if (value && json_is_array(value)) {
        ch->vuln_flags = flags_from_json_array(vuln_flags, value);
    } else {
        value = json_object_get(character, "vuln_flags");
        if (value) ch->vuln_flags = json_integer_value(value);
    }

    value = json_object_get(character, "vuln_flags_perm_names");
    if (value && json_is_array(value)) {
        ch->vuln_flags_perm = flags_from_json_array(vuln_flags, value);
    } else {
        value = json_object_get(character, "vuln_flags_perm");
        if (value) ch->vuln_flags_perm = json_integer_value(value);
    }

    // Lost body parts (prefer array, fall back to numeric)
    value = json_object_get(character, "lostparts_names");
    if (value && json_is_array(value)) {
        ch->lostparts = flags_from_json_array(part_flags, value);
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
            ch->quest->questgiver = 0;
            ch->quest->questreceiver_type = 0;
            ch->quest->questreceiver = 0;
            ch->quest->msg_complete = false;
            ch->quest->generating = false;
            ch->quest->scripted = false;
        }

        value = json_object_get(quest, "questgiver_type");
        if (value) ch->quest->questgiver_type = json_integer_value(value);
        value = json_object_get(quest, "questgiver");
        if (value) ch->quest->questgiver = json_integer_value(value);
        value = json_object_get(quest, "questreceiver_type");
        if (value) ch->quest->questreceiver_type = json_integer_value(value);
        value = json_object_get(quest, "questreceiver");
        if (value) ch->quest->questreceiver = json_integer_value(value);
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
                if (value) part->obj = json_integer_value(value);
                value = json_object_get(part_elem, "mob");
                if (value) part->mob = json_integer_value(value);
                value = json_object_get(part_elem, "room");
                if (value) part->room = json_integer_value(value);
                value = json_object_get(part_elem, "obj_sac");
                if (value) part->obj_sac = json_integer_value(value);
                value = json_object_get(part_elem, "mob_rescue");
                if (value) part->mob_rescue = json_integer_value(value);
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
            // Regular room position
            long room_vnum = json_integer_value(json_object_get(position, "room_vnum"));
            ROOM_INDEX_DATA *room = get_room_index_global(room_vnum);
            if (room) {
                ch->in_room = room;
            } else {
                // Fallback to default recall room if saved room doesn't exist
                plogf(LOG_WARN, "%s: room vnum %ld not found, using default recall",
                      ch->name, room_vnum);
                ch->in_room = get_room_index_global(11001);
            }
        }
    } else {
        // No position saved - use default recall
        ch->in_room = get_room_index_global(11001);
    }

    // Read inventory section (skip if not loading heavy data)
    if (load_heavy) {
        inventory = json_object_get(root, "inventory");
        if (inventory && json_is_array(inventory)) {
            json_array_foreach(inventory, index, array_elem) {
                OBJ_DATA *obj = json_to_obj(array_elem, ch);
                if (obj) {
                    obj_to_char(obj, ch);
                }
            }
        }
    }

    // Read equipment section (skip if not loading heavy data)
    if (load_heavy) {
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
    }

    // Read locker section (skip if not loading heavy data)
    if (load_heavy) {
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
    }

    // **FIX #4: Read skills section - uses skill NAME as key (robust against ID changes)**
    // (skip if not loading heavy data)
    if (load_heavy) {
    skills = json_object_get(root, "skills");
    if (skills && json_is_object(skills)) {
        const char *skill_key;
        json_t *skill_value;
        json_object_foreach(skills, skill_key, skill_value) {
            int sn;

            // Try to look up skill by name first (new format)
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
        }
    }

    // Read skill groups section
    json_t *skill_groups = json_object_get(root, "skill_groups");
    if (skill_groups && json_is_array(skill_groups)) {
        json_array_foreach(skill_groups, index, array_elem) {
            int gn = json_integer_value(json_object_get(array_elem, "id"));
            if (gn >= 0 && gn < MAX_GROUP) {
                ch->pcdata->group_known[gn] = true;
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
            paf->level = json_integer_value(json_object_get(array_elem, "level"));
            paf->duration = json_integer_value(json_object_get(array_elem, "duration"));
            paf->location = json_integer_value(json_object_get(array_elem, "location"));
            paf->modifier = json_integer_value(json_object_get(array_elem, "modifier"));
            paf->bitvector = json_integer_value(json_object_get(array_elem, "bitvector"));

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
            long vnum = json_integer_value(json_object_get(array_elem, "vnum"));
            TOKEN_INDEX_DATA *pTokenIndex = get_token_index((find_area_by_vnum(vnum, NULL) ?: get_system_area_fallback()), vnum);
            if (!pTokenIndex) {
                log_stringf("json_read_char_internal: bad token vnum %ld", vnum);
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
    } // End if (load_heavy) - close the block that started at skills section

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
    log_stringf("PERFORMANCE json_read_char_internal: %s with %d objects (%s) - total: %ldms",
               ch->name, obj_count, load_heavy ? "full" : "basic", total_ms);

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
        json_object_foreach(skills, skill_key, skill_value) {
            int sn;

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
        }
    }

    // Read skill groups section
    json_t *skill_groups = json_object_get(root, "skill_groups");
    if (skill_groups && json_is_array(skill_groups)) {
        json_array_foreach(skill_groups, index, array_elem) {
            int gn = json_integer_value(json_object_get(array_elem, "id"));
            if (gn >= 0 && gn < MAX_GROUP) {
                ch->pcdata->group_known[gn] = true;
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
            paf->level = json_integer_value(json_object_get(array_elem, "level"));
            paf->duration = json_integer_value(json_object_get(array_elem, "duration"));
            paf->location = json_integer_value(json_object_get(array_elem, "location"));
            paf->modifier = json_integer_value(json_object_get(array_elem, "modifier"));
            paf->bitvector = json_integer_value(json_object_get(array_elem, "bitvector"));

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
            long vnum = json_integer_value(json_object_get(array_elem, "vnum"));
            TOKEN_INDEX_DATA *pTokenIndex = get_token_index((find_area_by_vnum(vnum, NULL) ?: get_system_area_fallback()), vnum);
            if (!pTokenIndex) {
                log_stringf("json_read_char_remaining: bad token vnum %ld", vnum);
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

    // Mark as fully loaded
    ch->pcdata->fully_loaded = true;

    // Performance logging
    gettimeofday(&end_time, NULL);
    total_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
              (end_time.tv_usec - start_time.tv_usec) / 1000;
    int obj_count = (ch->lcarrying ? list_size(ch->lcarrying) : 0) +
                   (ch->llocker ? list_size(ch->llocker) : 0) +
                   (ch->lworn ? list_size(ch->lworn) : 0);
    log_stringf("PERFORMANCE json_read_char_remaining: %s with %d objects - total: %ldms",
               ch->name, obj_count, total_ms);

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
