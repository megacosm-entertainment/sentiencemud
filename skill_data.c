/***************************************************************************
 *  Skill Data System - Implementation                                     *
 *                                                                         *
 *  Data-driven skill/spell backend. Loads skill definitions from JSON     *
 *  files in data/skills/.                                                 *
 *                                                                         *
 *  Provides O(1) hash table lookup by name and UID index for fast access. *
 ***************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <jansson.h>
#include "io/json/json_common.h"
#include "merc.h"
#include "magic.h"
#include "skill_data.h"

/***************************************************************************
 * Globals                                                                 *
 ***************************************************************************/

static SKILL_DATA *     skill_list = NULL;      /* Alphabetically sorted linked list */
static int              skill_total = 0;        /* Total loaded skills */

/* Hash table for O(1) name lookup */
typedef struct skill_hash_entry {
    char *              key;
    SKILL_DATA *        value;
    struct skill_hash_entry *next;
} SKILL_HASH_ENTRY;

static SKILL_HASH_ENTRY *skill_hash_tbl[SKILL_HASH_SIZE];

/* UID index for O(1) lookup by UID */
static SKILL_DATA **    skill_uid_index = NULL;
static int              max_skill_uid = 0;

/***************************************************************************
 * Spell Function Name <-> Pointer Table                                   *
 *                                                                         *
 * Maps function name strings to SPELL_FUN pointers for JSON serialization.*
 * This gets populated from the extern declarations in merc.h.             *
 ***************************************************************************/

typedef struct spell_func_entry {
    const char *    name;
    SPELL_FUN *     func;
} SPELL_FUNC_ENTRY;

/* All spell functions declared in the codebase.
 * This table is used to resolve "spell_acid_blast" -> &spell_acid_blast
 * when loading from JSON, and the reverse when saving. */
static const SPELL_FUNC_ENTRY spell_func_table[] =
{
    { "spell_null",                 spell_null },
    { "spell_acid_blast",           spell_acid_blast },
    { "spell_acid_breath",          spell_acid_breath },
    { "spell_afterburn",            spell_afterburn },
    { "spell_animate_dead",         spell_animate_dead },
    { "spell_armour",               spell_armour },
    { "spell_avatar_shield",        spell_avatar_shield },
    { "spell_bless",                spell_bless },
    { "spell_blindness",            spell_blindness },
    { "spell_burning_hands",        spell_burning_hands },
    { "spell_call_familiar",        spell_call_familiar },
    { "spell_call_lightning",       spell_call_lightning },
    { "spell_calm",                 spell_calm },
    { "spell_cancellation",         spell_cancellation },
    { "spell_cause_critical",       spell_cause_critical },
    { "spell_cause_light",          spell_cause_light },
    { "spell_cause_serious",        spell_cause_serious },
    { "spell_chain_lightning",      spell_chain_lightning },
    { "spell_channel",              spell_channel },
    { "spell_charm_person",         spell_charm_person },
    { "spell_chill_touch",          spell_chill_touch },
    { "spell_cloak_of_guile",       spell_cloak_of_guile },
    { "spell_colour_spray",         spell_colour_spray },
    { "spell_continual_light",      spell_continual_light },
    { "spell_control_weather",      spell_control_weather },
    { "spell_cosmic_blast",         spell_cosmic_blast },
    { "spell_counter_spell",        spell_counter_spell },
    { "spell_create_food",          spell_create_food },
    { "spell_create_rose",          spell_create_rose },
    { "spell_create_spring",        spell_create_spring },
    { "spell_create_water",         spell_create_water },
    { "spell_cure_blindness",       spell_cure_blindness },
    { "spell_cure_critical",        spell_cure_critical },
    { "spell_cure_disease",         spell_cure_disease },
    { "spell_cure_light",           spell_cure_light },
    { "spell_cure_poison",          spell_cure_poison },
    { "spell_cure_serious",         spell_cure_serious },
    { "spell_cure_toxic",           spell_cure_toxic },
    { "spell_curse",                spell_curse },
    { "spell_dark_shroud",          spell_dark_shroud },
    { "spell_deathbarbs",           spell_deathbarbs },
    { "spell_death_grip",           spell_death_grip },
    { "spell_deathsight",           spell_deathsight },
    { "spell_demonfire",            spell_demonfire },
    { "spell_destruction",          spell_destruction },
    { "spell_detect_hidden",        spell_detect_hidden },
    { "spell_detect_invis",         spell_detect_invis },
    { "spell_detect_magic",         spell_detect_magic },
    { "spell_discharge",            spell_discharge },
    { "spell_dispel_evil",          spell_dispel_evil },
    { "spell_dispel_good",          spell_dispel_good },
    { "spell_dispel_magic",         spell_dispel_magic },
    { "spell_dispel_room",          spell_dispel_room },
    { "spell_eagle_eye",            spell_eagle_eye },
    { "spell_earth_walk",           spell_earth_walk },
    { "spell_earthquake",           spell_earthquake },
    { "spell_electrical_barrier",   spell_electrical_barrier },
    { "spell_enchant_armour",       spell_enchant_armour },
    { "spell_enchant_weapon",       spell_enchant_weapon },
    { "spell_energy_drain",         spell_energy_drain },
    { "spell_energy_field",         spell_energy_field },
    { "spell_ensnare",              spell_ensnare },
    { "spell_entrap",               spell_entrap },
    { "spell_exorcism",             spell_exorcism },
    { "spell_faerie_fire",          spell_faerie_fire },
    { "spell_faerie_fog",           spell_faerie_fog },
    { "spell_fatigue",              spell_fatigue },
    { "spell_fire_barrier",         spell_fire_barrier },
    { "spell_fire_breath",          spell_fire_breath },
    { "spell_fire_cloud",           spell_fire_cloud },
    { "spell_fireball",             spell_fireball },
    { "spell_fireproof",            spell_fireproof },
    { "spell_flamestrike",          spell_flamestrike },
    { "spell_flash",                spell_flash },
    { "spell_fly",                  spell_fly },
    { "spell_frenzy",               spell_frenzy },
    { "spell_frost_barrier",        spell_frost_barrier },
    { "spell_frost_breath",         spell_frost_breath },
    { "spell_gas_breath",           spell_gas_breath },
    { "spell_gate",                 spell_gate },
    { "spell_giant_strength",       spell_giant_strength },
    { "spell_glacial_wave",         spell_glacial_wave },
    { "spell_glorious_bolt",        spell_glorious_bolt },
    { "spell_harm",                 spell_harm },
    { "spell_haste",                spell_haste },
    { "spell_heal",                 spell_heal },
    { "spell_healing_aura",         spell_healing_aura },
    { "spell_holy_shield",          spell_holy_shield },
    { "spell_holy_sword",           spell_holy_sword },
    { "spell_holy_word",            spell_holy_word },
    { "spell_ice_shards",           spell_ice_shards },
    { "spell_ice_storm",            spell_ice_storm },
    { "spell_identify",             spell_identify },
    { "spell_improved_invisibility", spell_improved_invisibility },
    { "spell_inferno",              spell_inferno },
    { "spell_infravision",          spell_infravision },
    { "spell_invis",                spell_invis },
    { "spell_kill",                 spell_kill },
    { "spell_light_shroud",         spell_light_shroud },
    { "spell_lightning_bolt",       spell_lightning_bolt },
    { "spell_lightning_breath",     spell_lightning_breath },
    { "spell_locate_object",        spell_locate_object },
    { "spell_magic_missile",        spell_magic_missile },
    { "spell_mass_healing",         spell_mass_healing },
    { "spell_mass_invis",           spell_mass_invis },
    { "spell_master_weather",       spell_master_weather },
    { "spell_maze",                 spell_maze },
    { "spell_momentary_darkness",   spell_momentary_darkness },
    { "spell_morphlock",            spell_morphlock },
    { "spell_nexus",                spell_nexus },
    { "spell_paralysis",            spell_paralysis },
    { "spell_pass_door",            spell_pass_door },
    { "spell_plague",               spell_plague },
    { "spell_poison",               spell_poison },
    { "spell_raise_dead",           spell_raise_dead },
    { "spell_recharge",             spell_recharge },
    { "spell_reflection",           spell_reflection },
    { "spell_refresh",              spell_refresh },
    { "spell_regeneration",         spell_regeneration },
    { "spell_remove_curse",         spell_remove_curse },
    { "spell_room_shield",          spell_room_shield },
    { "spell_sanctuary",            spell_sanctuary },
    { "spell_shield",               spell_shield },
    { "spell_shocking_grasp",       spell_shocking_grasp },
    { "spell_shriek",               spell_shriek },
    { "spell_silence",              spell_silence },
    { "spell_sleep",                spell_sleep },
    { "spell_slow",                 spell_slow },
    { "spell_soul_essence",         spell_soul_essence },
    { "spell_spell_deflection",     spell_spell_deflection },
    { "spell_spell_shield",         spell_spell_shield },
    { "spell_spell_trap",           spell_spell_trap },
    { "spell_starflare",            spell_starflare },
    { "spell_stinking_cloud",       spell_stinking_cloud },
    { "spell_stone_skin",           spell_stone_skin },
    { "spell_stone_spikes",         spell_stone_spikes },
    { "spell_stone_touch",          spell_stone_touch },
    { "spell_summon",               spell_summon },
    { "spell_third_eye",            spell_third_eye },
    { "spell_toxic_fumes",          spell_toxic_fumes },
    { "spell_toxin_neurotoxin",     spell_toxin_neurotoxin },
    { "spell_toxin_paralysis",      spell_toxin_paralysis },
    { "spell_toxin_venom",          spell_toxin_venom },
    { "spell_toxin_weakness",       spell_toxin_weakness },
    { "spell_underwater_breathing", spell_underwater_breathing },
    { "spell_vision",               spell_vision },
    { "spell_vocalize",             spell_vocalize },
    { "spell_weaken",               spell_weaken },
    { "spell_web",                  spell_web },
    { "spell_wind_of_confusion",    spell_wind_of_confusion },
    { "spell_withering_cloud",      spell_withering_cloud },
    { "spell_word_of_recall",       spell_word_of_recall },
    { NULL, NULL }
};

/* NOTE: This table will be incomplete initially. As we discover missing entries
 * during bootstrap, they'll be logged and can be added. The bootstrap process
 * also stores spell_fun_name by comparing pointers, so even unlisted functions
 * will work functionally — they just won't have a JSON-serializable name. */

/***************************************************************************
 * Hash Table Implementation                                               *
 ***************************************************************************/

/**
 * skill_hash - FNV-1a hash of a skill name (case-insensitive)
 */
static unsigned int skill_hash(const char *name)
{
    unsigned int hash = 2166136261u;
    while (*name) {
        hash ^= (unsigned char)LOWER(*name);
        name++;
        hash *= 16777619u;
    }
    return hash % SKILL_HASH_SIZE;
}

/**
 * skill_hash_insert - Insert a SKILL_DATA into the hash table
 */
static void skill_hash_insert(SKILL_DATA *skill)
{
    unsigned int idx;
    SKILL_HASH_ENTRY *entry;

    if (!skill || !skill->name)
        return;

    idx = skill_hash(skill->name);
    entry = (SKILL_HASH_ENTRY *)alloc_perm(sizeof(SKILL_HASH_ENTRY));
    entry->key = skill->name;
    entry->value = skill;
    entry->next = skill_hash_tbl[idx];
    skill_hash_tbl[idx] = entry;
}

/***************************************************************************
 * Memory Management                                                       *
 ***************************************************************************/

/**
 * new_skill_data - Allocate and initialize a new SKILL_DATA
 */
SKILL_DATA *new_skill_data(void)
{
    SKILL_DATA *skill;

    skill = (SKILL_DATA *)alloc_perm(sizeof(SKILL_DATA));
    memset(skill, 0, sizeof(SKILL_DATA));

    skill->valid = true;
    skill->uid = -1;
    skill->difficulty = 1;
    skill->spell_fun = spell_null;

    /* Initialize legacy class arrays to "not available" */
    for (int i = 0; i < MAX_CLASS; i++) {
        skill->skill_level[i] = 31;
        skill->rating[i] = 1;
    }

    return skill;
}

/***************************************************************************
 * Lookup Functions                                                        *
 ***************************************************************************/

/**
 * skill_find - Exact match lookup by name (case-insensitive, O(1) average)
 */
SKILL_DATA *skill_find(const char *name)
{
    unsigned int idx;
    SKILL_HASH_ENTRY *entry;

    if (!name || !name[0])
        return NULL;

    idx = skill_hash(name);
    entry = skill_hash_tbl[idx];

    while (entry) {
        if (!str_cmp(entry->key, name))
            return entry->value;
        entry = entry->next;
    }

    return NULL;
}

/**
 * skill_search - Prefix match lookup (like legacy skill_lookup)
 *
 * Returns the first skill whose name starts with the given prefix.
 * Linear scan of the sorted list — use skill_find for exact matches.
 */
SKILL_DATA *skill_search(const char *prefix)
{
    SKILL_DATA *skill;

    if (!prefix || !prefix[0])
        return NULL;

    for (skill = skill_list; skill; skill = skill->next) {
        if (LOWER(prefix[0]) == LOWER(skill->name[0])
        && !str_prefix(prefix, skill->name))
            return skill;
    }

    return NULL;
}

/**
 * skill_find_uid - Lookup by UID (O(1) via index array)
 */
SKILL_DATA *skill_find_uid(int16_t uid)
{
    if (uid < 0 || uid > max_skill_uid || !skill_uid_index)
        return NULL;

    return skill_uid_index[uid];
}

/**
 * skill_name - Get the name of a skill, or "none" if NULL
 */
const char *skill_name(SKILL_DATA *skill)
{
    return skill ? skill->name : "none";
}

/**
 * skill_name_by_uid - Get a skill name by UID, or "" if not found
 *
 * Used by the SKILL_NAME() macro for backward compatibility with code
 * that references skills by numeric index.
 */
const char *skill_name_by_uid(int16_t uid)
{
    SKILL_DATA *sk = skill_find_uid(uid);
    return sk ? sk->name : "";
}

/**
 * skill_first - Get the first skill in the global sorted list
 */
SKILL_DATA *skill_first(void)
{
    return skill_list;
}

/**
 * skill_count - Get the total number of loaded skills
 */
int skill_count(void)
{
    return skill_total;
}

/**
 * skill_sn - Get the UID of a skill (matches legacy sn after bootstrap)
 */
int16_t skill_sn(SKILL_DATA *skill)
{
    return skill ? skill->uid : -1;
}

/**
 * skill_resolve_gsn - Resolve a skill name to its UID
 *
 * Convenience function that replaces gsn_* global usage patterns.
 * Returns the UID (which matches the legacy sn index after bootstrap),
 * or -1 if the skill is not found.
 *
 * @param name   Skill name to look up (exact match, case-insensitive)
 * @return       Skill UID, or -1 if not found
 */
int16_t skill_resolve_gsn(const char *name)
{
    SKILL_DATA *sk = skill_find(name);
    return sk ? sk->uid : -1;
}

/***************************************************************************
 * Spell Function Resolution                                               *
 ***************************************************************************/

/**
 * spell_fun_lookup - Resolve a function name string to a SPELL_FUN pointer
 */
SPELL_FUN *spell_fun_lookup(const char *name)
{
    if (!name || !name[0])
        return NULL;

    for (int i = 0; spell_func_table[i].name != NULL; i++) {
        if (!str_cmp(name, spell_func_table[i].name))
            return spell_func_table[i].func;
    }

    return NULL;
}

/**
 * spell_fun_name - Resolve a SPELL_FUN pointer to its function name string
 */
const char *spell_fun_name(SPELL_FUN *fun)
{
    if (!fun || fun == spell_null)
        return NULL;

    for (int i = 0; spell_func_table[i].name != NULL; i++) {
        if (spell_func_table[i].func == fun)
            return spell_func_table[i].name;
    }

    return NULL;
}

/***************************************************************************
 * Sorted List Insertion                                                   *
 ***************************************************************************/

/**
 * skill_insert_sorted - Insert a skill into the global list alphabetically
 */
static void skill_insert_sorted(SKILL_DATA *skill)
{
    SKILL_DATA *prev, *curr;

    if (!skill_list || str_cmp(skill->name, skill_list->name) < 0) {
        skill->next = skill_list;
        skill_list = skill;
        return;
    }

    for (prev = skill_list, curr = skill_list->next;
         curr != NULL;
         prev = curr, curr = curr->next) {
        if (str_cmp(skill->name, curr->name) < 0)
            break;
    }

    skill->next = curr;
    prev->next = skill;
}

/***************************************************************************
 * JSON Loading                                                            *
 ***************************************************************************/

/**
 * skill_load_json - Load a single SKILL_DATA from a JSON file
 */
static SKILL_DATA *skill_load_json(const char *filename)
{
    json_t *root, *obj, *arr, *val;
    json_error_t error;
    SKILL_DATA *skill;
    const char *str;
    size_t index;

    root = json_load_file(filename, 0, &error);
    if (!root) {
        log_stringf("skill_load_json: Error loading %s: %s", filename, error.text);
        return NULL;
    }

    /* Validate format */
    str = json_get_string(root, "_format", "");
    if (!str || str_cmp(str, "skill_data")) {
        log_stringf("skill_load_json: Invalid format in %s", filename);
        json_decref(root);
        return NULL;
    }

    skill = new_skill_data();

    /* Identity */
    str = json_get_string(root, "name", "");
    skill->name = str_dup(str ? str : "unknown");

    val = json_object_get(root, "uid");
    skill->uid = val ? (int16_t)json_integer_value(val) : -1;

    str = json_get_string(root, "type", "");
    skill->isspell = (str && !str_cmp(str, "spell"));

    str = json_get_string(root, "display", "");
    skill->display = str_dup(str ? str : skill->name);

    str = json_get_string(root, "summary", "");
    skill->summary = str ? str_dup(str) : NULL;

    str = json_get_string(root, "description", "");
    skill->description = str ? str_dup(str) : NULL;

    str = json_get_string(root, "comments", "");
    skill->comments = str ? str_dup(str) : NULL;

    str = json_get_string(root, "help_keyword", "");
    skill->help_keyword = str ? str_dup(str) : NULL;

    /* Flags */
    arr = json_object_get(root, "flags");
    if (arr && json_is_array(arr)) {
        json_array_foreach(arr, index, val) {
            str = json_string_value(val);
            if (!str) continue;
            if (!str_cmp(str, "racial"))        SET_BIT(skill->flags, SKILLFLAG_RACIAL);
            else if (!str_cmp(str, "remort"))    SET_BIT(skill->flags, SKILLFLAG_REMORT);
            else if (!str_cmp(str, "no_practice")) SET_BIT(skill->flags, SKILLFLAG_NO_PRACTICE);
            else if (!str_cmp(str, "no_improve"))  SET_BIT(skill->flags, SKILLFLAG_NO_IMPROVE);
            else if (!str_cmp(str, "passive"))     SET_BIT(skill->flags, SKILLFLAG_PASSIVE);
            else if (!str_cmp(str, "token_driven")) SET_BIT(skill->flags, SKILLFLAG_TOKEN_DRIVEN);
        }
    }

    /* Invocation */
    str = json_get_string(root, "spell_function", "");
    if (str && str[0]) {
        skill->spell_fun_name = str_dup(str);
        skill->spell_fun = spell_fun_lookup(str);
        if (!skill->spell_fun && str_cmp(str, "spell_null") && str_cmp(str, "none")) {
            log_stringf("skill_load_json: Unknown spell function '%s' for skill '%s'", str, skill->name);
        }
    }

    /* Target type */
    str = json_get_string(root, "target", "");
    if (str) {
        if (!str_cmp(str, "ignore"))              skill->target = TAR_IGNORE;
        else if (!str_cmp(str, "offensive"))      skill->target = TAR_CHAR_OFFENSIVE;
        else if (!str_cmp(str, "defensive"))      skill->target = TAR_CHAR_DEFENSIVE;
        else if (!str_cmp(str, "self"))           skill->target = TAR_CHAR_SELF;
        else if (!str_cmp(str, "obj_inv"))        skill->target = TAR_OBJ_INV;
        else if (!str_cmp(str, "obj_char_def"))   skill->target = TAR_OBJ_CHAR_DEF;
        else if (!str_cmp(str, "obj_char_off"))   skill->target = TAR_OBJ_CHAR_OFF;
    }

    /* Position */
    str = json_get_string(root, "minimum_position", "");
    if (str) {
        if (!str_cmp(str, "dead"))           skill->minimum_position = POS_DEAD;
        else if (!str_cmp(str, "mortal"))    skill->minimum_position = POS_MORTAL;
        else if (!str_cmp(str, "incap"))     skill->minimum_position = POS_INCAP;
        else if (!str_cmp(str, "stunned"))   skill->minimum_position = POS_STUNNED;
        else if (!str_cmp(str, "sleeping"))  skill->minimum_position = POS_SLEEPING;
        else if (!str_cmp(str, "resting"))   skill->minimum_position = POS_RESTING;
        else if (!str_cmp(str, "sitting"))   skill->minimum_position = POS_SITTING;
        else if (!str_cmp(str, "fighting"))  skill->minimum_position = POS_FIGHTING;
        else if (!str_cmp(str, "standing"))  skill->minimum_position = POS_STANDING;
        else                                 skill->minimum_position = POS_STANDING;
    }

    /* Numeric fields */
    val = json_object_get(root, "min_mana");
    skill->min_mana = val ? (int16_t)json_integer_value(val) : 0;

    val = json_object_get(root, "beats");
    skill->beats = val ? (int16_t)json_integer_value(val) : 0;

    /* Messages */
    str = json_get_string(root, "noun_damage", "");
    skill->noun_damage = str_dup(str ? str : "");

    str = json_get_string(root, "msg_off", "");
    skill->msg_off = str_dup(str ? str : "");

    str = json_get_string(root, "msg_obj", "");
    skill->msg_obj = str_dup(str ? str : "");

    str = json_get_string(root, "msg_disp", "");
    skill->msg_disp = str_dup(str ? str : "");

    /* Race restriction */
    str = json_get_string(root, "race", "");
    if (str && str[0]) {
        skill->race_name = str_dup(str);
        skill->race = race_lookup(str);
    }

    /* Inks */
    arr = json_object_get(root, "inks");
    if (arr && json_is_array(arr)) {
        for (size_t i = 0; i < 3 && i < json_array_size(arr); i++) {
            json_t *pair = json_array_get(arr, i);
            if (pair && json_is_array(pair) && json_array_size(pair) == 2) {
                skill->inks[i][0] = (int)json_integer_value(json_array_get(pair, 0));
                skill->inks[i][1] = (int)json_integer_value(json_array_get(pair, 1));
            }
        }
    }

    /* Token widevnum */
    obj = json_object_get(root, "token_wnum");
    if (obj && json_is_object(obj)) {
        val = json_object_get(obj, "auid");
        skill->token_wnum.auid = val ? (long)json_integer_value(val) : 0;
        val = json_object_get(obj, "vnum");
        skill->token_wnum.vnum = val ? (long)json_integer_value(val) : 0;
    }

    /* Generic values */
    obj = json_object_get(root, "values");
    if (obj && json_is_object(obj)) {
        const char *key;
        json_t *jval;
        int vi = 0;
        json_object_foreach(obj, key, jval) {
            if (vi >= MAX_SKILL_VALUES) break;
            skill->value_names[vi] = str_dup(key);
            skill->values[vi] = (int)json_integer_value(jval);
            vi++;
        }
    }

    json_decref(root);
    return skill;
}

/***************************************************************************
 * JSON Saving                                                             *
 ***************************************************************************/

/**
 * target_name - Convert TAR_* constant to string for JSON
 */
static const char *target_name(int16_t target)
{
    switch (target) {
        case TAR_IGNORE:            return "ignore";
        case TAR_CHAR_OFFENSIVE:    return "offensive";
        case TAR_CHAR_DEFENSIVE:    return "defensive";
        case TAR_CHAR_SELF:         return "self";
        case TAR_OBJ_INV:           return "obj_inv";
        case TAR_OBJ_CHAR_DEF:     return "obj_char_def";
        case TAR_OBJ_CHAR_OFF:     return "obj_char_off";
        default:                    return "ignore";
    }
}

/**
 * position_name - Convert POS_* constant to string for JSON
 */
static const char *position_name(int16_t pos)
{
    switch (pos) {
        case POS_DEAD:      return "dead";
        case POS_MORTAL:    return "mortal";
        case POS_INCAP:     return "incap";
        case POS_STUNNED:   return "stunned";
        case POS_SLEEPING:  return "sleeping";
        case POS_RESTING:   return "resting";
        case POS_SITTING:   return "sitting";
        case POS_FIGHTING:  return "fighting";
        case POS_STANDING:  return "standing";
        default:            return "standing";
    }
}

/**
 * skill_save_json - Save a single SKILL_DATA to its JSON file
 */
void save_skill_data(SKILL_DATA *skill)
{
    json_t *root, *arr, *obj;
    char path[512];
    char safe_name[256];

    if (!skill || !skill->name)
        return;

    /* Create directory if needed */
    mkdir(SKILLS_DIR, 0755);

    /* Build safe filename: replace spaces with underscores */
    snprintf(safe_name, sizeof(safe_name), "%s", skill->name);
    for (char *p = safe_name; *p; p++) {
        if (*p == ' ') *p = '_';
        else *p = LOWER(*p);
    }

    snprintf(path, sizeof(path), "%s%s.json", SKILLS_DIR, safe_name);

    root = json_object();
    json_object_set_new(root, "_format", json_string("skill_data"));
    json_object_set_new(root, "_version", json_integer(1));

    /* Identity */
    json_object_set_new(root, "name", json_string(skill->name));
    json_object_set_new(root, "uid", json_integer(skill->uid));
    json_object_set_new(root, "type", json_string(skill->isspell ? "spell" : "skill"));

    if (skill->display && str_cmp(skill->display, skill->name))
        json_object_set_new(root, "display", json_string(skill->display));

    if (skill->summary)
        json_object_set_new(root, "summary", json_string(skill->summary));

    if (skill->description)
        json_object_set_new(root, "description", json_string(skill->description));

    if (skill->comments)
        json_object_set_new(root, "comments", json_string(skill->comments));

    if (skill->help_keyword)
        json_object_set_new(root, "help_keyword", json_string(skill->help_keyword));

    /* Flags */
    arr = json_array();
    if (IS_SET(skill->flags, SKILLFLAG_RACIAL))      json_array_append_new(arr, json_string("racial"));
    if (IS_SET(skill->flags, SKILLFLAG_REMORT))       json_array_append_new(arr, json_string("remort"));
    if (IS_SET(skill->flags, SKILLFLAG_NO_PRACTICE))  json_array_append_new(arr, json_string("no_practice"));
    if (IS_SET(skill->flags, SKILLFLAG_NO_IMPROVE))   json_array_append_new(arr, json_string("no_improve"));
    if (IS_SET(skill->flags, SKILLFLAG_PASSIVE))      json_array_append_new(arr, json_string("passive"));
    if (IS_SET(skill->flags, SKILLFLAG_TOKEN_DRIVEN)) json_array_append_new(arr, json_string("token_driven"));
    if (json_array_size(arr) > 0)
        json_object_set_new(root, "flags", arr);
    else
        json_decref(arr);

    /* Spell function */
    if (skill->spell_fun_name && skill->spell_fun_name[0])
        json_object_set_new(root, "spell_function", json_string(skill->spell_fun_name));
    else if (skill->spell_fun && skill->spell_fun != spell_null) {
        const char *fname = spell_fun_name(skill->spell_fun);
        if (fname)
            json_object_set_new(root, "spell_function", json_string(fname));
    }

    /* Invocation properties */
    json_object_set_new(root, "target", json_string(target_name(skill->target)));
    json_object_set_new(root, "minimum_position", json_string(position_name(skill->minimum_position)));
    json_object_set_new(root, "min_mana", json_integer(skill->min_mana));
    json_object_set_new(root, "beats", json_integer(skill->beats));

    /* Messages */
    json_object_set_new(root, "noun_damage", json_string(skill->noun_damage ? skill->noun_damage : ""));
    json_object_set_new(root, "msg_off", json_string(skill->msg_off ? skill->msg_off : ""));
    json_object_set_new(root, "msg_obj", json_string(skill->msg_obj ? skill->msg_obj : ""));
    json_object_set_new(root, "msg_disp", json_string(skill->msg_disp ? skill->msg_disp : ""));

    /* Race restriction */
    if (skill->race_name && skill->race_name[0])
        json_object_set_new(root, "race", json_string(skill->race_name));
    else
        json_object_set_new(root, "race", json_null());

    /* Inks */
    arr = json_array();
    for (int i = 0; i < 3; i++) {
        json_t *pair = json_array();
        json_array_append_new(pair, json_integer(skill->inks[i][0]));
        json_array_append_new(pair, json_integer(skill->inks[i][1]));
        json_array_append_new(arr, pair);
    }
    json_object_set_new(root, "inks", arr);

    /* Token widevnum */
    if (skill->token_wnum.auid > 0 || skill->token_wnum.vnum > 0) {
        obj = json_object();
        json_object_set_new(obj, "auid", json_integer(skill->token_wnum.auid));
        json_object_set_new(obj, "vnum", json_integer(skill->token_wnum.vnum));
        json_object_set_new(root, "token_wnum", obj);
    } else {
        json_object_set_new(root, "token_wnum", json_null());
    }

    /* Generic values */
    obj = json_object();
    for (int i = 0; i < MAX_SKILL_VALUES; i++) {
        if (skill->values[i] != 0 || (skill->value_names[i] && skill->value_names[i][0])) {
            const char *vname = (skill->value_names[i] && skill->value_names[i][0])
                ? skill->value_names[i] : "unnamed";
            json_object_set_new(obj, vname, json_integer(skill->values[i]));
        }
    }
    if (json_object_size(obj) > 0)
        json_object_set_new(root, "values", obj);
    else
        json_decref(obj);

    /* Write file */
    if (json_dump_file(root, path, JSON_INDENT(4) | JSON_SORT_KEYS) != 0) {
        log_stringf("save_skill_data: Failed to write %s", path);
    }

    json_decref(root);
}

/**
 * save_all_skill_data - Save all loaded skills to their JSON files
 */
void save_all_skill_data(void)
{
    SKILL_DATA *skill;

    log_string("Saving all skill data to JSON files...");

    for (skill = skill_list; skill; skill = skill->next) {
        save_skill_data(skill);
    }

    log_stringf("Saved %d skills.", skill_total);
}

/***************************************************************************
 * Boot Loading                                                            *
 ***************************************************************************/

/**
 * load_skill_data - Load all skills from JSON files
 *
 * Called from boot_db(). Skills are loaded from individual JSON files
 * in data/skills/. Requires JSON skill files to exist.
 */
void load_skill_data(void)
{
    DIR *dir;
    struct dirent *entry;
    char path[512];
    int i;

    log_string("Loading skill data...");

    /* Initialize hash table */
    for (i = 0; i < SKILL_HASH_SIZE; i++) {
        skill_hash_tbl[i] = NULL;
    }

    skill_list = NULL;
    skill_total = 0;
    max_skill_uid = 0;

    /* Try to open skills directory */
    dir = opendir(SKILLS_DIR);

    /* Check if directory has any JSON files */
    bool has_json = false;
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            size_t len = strlen(entry->d_name);
            if (len >= 6 && !str_cmp(entry->d_name + len - 5, ".json")) {
                has_json = true;
                break;
            }
        }
        closedir(dir);
    }

    if (!has_json) {
        log_string("FATAL: No skill JSON files found in " SKILLS_DIR);
        log_string("Skills must be defined as JSON files in data/skills/.");
        exit(1);
    }

    /* Load from JSON files */
    dir = opendir(SKILLS_DIR);
    if (!dir) {
        log_stringf("load_skill_data: Cannot open %s", SKILLS_DIR);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len < 6 || str_cmp(entry->d_name + len - 5, ".json"))
            continue;

        snprintf(path, sizeof(path), "%s%s", SKILLS_DIR, entry->d_name);

        SKILL_DATA *skill = skill_load_json(path);
        if (skill) {
            skill_insert_sorted(skill);

            if (skill->uid > max_skill_uid)
                max_skill_uid = skill->uid;

            skill_total++;
        }
    }
    closedir(dir);

    if (skill_total == 0) {
        log_stringf("WARNING: No skills loaded from %s!", SKILLS_DIR);
        return;
    }

    /* Build UID index */
    skill_uid_index = (SKILL_DATA **)alloc_perm(sizeof(SKILL_DATA *) * (max_skill_uid + 1));
    memset(skill_uid_index, 0, sizeof(SKILL_DATA *) * (max_skill_uid + 1));

    /* Populate hash table and UID index */
    for (SKILL_DATA *sk = skill_list; sk; sk = sk->next) {
        skill_hash_insert(sk);
        if (sk->uid >= 0 && sk->uid <= max_skill_uid)
            skill_uid_index[sk->uid] = sk;
    }

    /* Resolve race pointers */
    for (SKILL_DATA *sk = skill_list; sk; sk = sk->next) {
        if (sk->race_name && sk->race_name[0])
            sk->race = race_lookup(sk->race_name);
    }

    log_stringf("Loaded %d skills from JSON (max UID: %d).", skill_total, max_skill_uid);
}
