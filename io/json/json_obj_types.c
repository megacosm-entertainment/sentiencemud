/***************************************************************************
 *  Sentience MUD                                                          *
 *  JSON serialization/deserialization for type-specific object data.       *
 *                                                                         *
 *  Each type struct has a pair of static to_json/from_json functions.      *
 *  The public API wraps these into a single JSON object keyed by type      *
 *  name, making it easy to embed in any save format.                      *
 ***************************************************************************/

#include "../../merc.h"
#include "../../item_types.h"
#include "../../tables.h"
#include "json_obj_types.h"
#include "json_common.h"
#include <jansson.h>
#include <string.h>

/*
 * ============================================================================
 *  HELPER MACROS
 * ============================================================================
 */

/* Set an integer field in a JSON object */
#define JSET_INT(j, key, val)  json_object_set_new((j), (key), json_integer((json_int_t)(val)))
/* Set a long field */
#define JSET_LONG(j, key, val) json_object_set_new((j), (key), json_integer((json_int_t)(val)))
/* Set a string field (handles NULL by skipping) */
#define JSET_STR(j, key, val)  do { if ((val) && (val)[0]) json_object_set_new((j), (key), json_string(val)); } while(0)

/* Get an integer from JSON, with default */
#define JGET_INT(j, key, def)  ((int)json_integer_value(json_object_get((j), (key))))
#define JGET_LONG(j, key, def) ((long)json_integer_value(json_object_get((j), (key))))
#define JGET_INT16(j, key)     ((int16_t)json_integer_value(json_object_get((j), (key))))
#define JGET_CHAR(j, key)      ((char)json_integer_value(json_object_get((j), (key))))

/* Get a string from JSON, returns empty string if not found */
static inline const char *jget_str(json_t *j, const char *key) {
    json_t *v = json_object_get(j, key);
    return v && json_is_string(v) ? json_string_value(v) : NULL;
}

/*
 * ============================================================================
 *  FLAG / ENUM HELPERS  (backward-compatible read & human-readable write)
 * ============================================================================
 */

/* Write a bitfield as a JSON array of flag-name strings. */
static void jset_flags(json_t *j, const char *key, long val,
                       const struct flag_type *table)
{
    json_object_set_new(j, key, json_flags_serialize(val, table));
}

/* Read a bitfield — accepts both string-array (new) and integer (legacy). */
static long jget_flags(json_t *j, const char *key,
                       const struct flag_type *table)
{
    json_t *v = json_object_get(j, key);
    if (!v) return 0;
    if (json_is_array(v))   return json_flags_deserialize(v, table);
    if (json_is_integer(v)) return (long)json_integer_value(v);
    return 0;
}

/* Write an enum — delegates to json_common. */
static void jset_enum(json_t *j, const char *key, long val,
                      const struct flag_type *table)
{
    json_enum_serialize(j, key, val, table);
}

/* Read an enum — delegates to json_common. */
static long jget_enum(json_t *j, const char *key,
                      const struct flag_type *table)
{
    return json_enum_deserialize(j, key, table);
}

/* Write attack_table damage type as its string name. */
static void jset_attack_type(json_t *j, const char *key, int val)
{
    if (val >= 0 && val < MAX_DAMAGE_MESSAGE && attack_table[val].name)
        json_object_set_new(j, key, json_string(attack_table[val].name));
    else
        json_object_set_new(j, key, json_integer((json_int_t)val));
}

/* Read attack_table damage type — string name or integer. */
static int jget_attack_type(json_t *j, const char *key)
{
    json_t *v = json_object_get(j, key);
    if (!v) return 0;
    if (json_is_string(v)) {
        const char *name = json_string_value(v);
        for (int i = 0; i < MAX_DAMAGE_MESSAGE; i++) {
            if (attack_table[i].name && !str_cmp(attack_table[i].name, name))
                return i;
        }
        return 0;
    }
    return (int)json_integer_value(v);
}

/* Write liquid type as its runtime string name. */
static void jset_liquid(json_t *j, const char *key, int val)
{
    if (val >= 0 && val < liquid_count())
        json_object_set_new(j, key, json_string(liquid_name(val)));
    else
        json_object_set_new(j, key, json_integer((json_int_t)val));
}

/* Read liquid type — string name or integer. */
static int jget_liquid(json_t *j, const char *key)
{
    json_t *v = json_object_get(j, key);
    if (!v) return 0;
    if (json_is_string(v)) {
        int liq = liq_lookup(json_string_value(v));
        return (liq >= 0) ? liq : 0;
    }
    return (int)json_integer_value(v);
}

/* Write trade type as its string name from trade_table. */
static void jset_trade_type(json_t *j, const char *key, int val)
{
    for (int i = 0; trade_table[i].trade_type != -1; i++) {
        if (trade_table[i].trade_type == val && trade_table[i].name[0]) {
            json_object_set_new(j, key, json_string(trade_table[i].name));
            return;
        }
    }
    json_object_set_new(j, key, json_integer((json_int_t)val));
}

/* Read trade type — string name or integer. */
static int jget_trade_type(json_t *j, const char *key)
{
    json_t *v = json_object_get(j, key);
    if (!v) return 0;
    if (json_is_string(v)) {
        const char *name = json_string_value(v);
        for (int i = 0; trade_table[i].trade_type != -1; i++) {
            if (!str_cmp(trade_table[i].name, name))
                return trade_table[i].trade_type;
        }
        return 0;
    }
    return (int)json_integer_value(v);
}

/* Write herb type as its string name from herb_table. */
static void jset_herb_type(json_t *j, const char *key, int val)
{
    if (val >= 0 && val < MAX_HERB && herb_table[val].name)
        json_object_set_new(j, key, json_string(herb_table[val].name));
    else
        json_object_set_new(j, key, json_integer((json_int_t)val));
}

/* Read herb type — string name or integer. */
static int jget_herb_type(json_t *j, const char *key)
{
    json_t *v = json_object_get(j, key);
    if (!v) return 0;
    if (json_is_string(v)) {
        const char *name = json_string_value(v);
        for (int i = 0; i < MAX_HERB; i++) {
            if (herb_table[i].name && !str_cmp(herb_table[i].name, name))
                return i;
        }
        return 0;
    }
    return (int)json_integer_value(v);
}


/*
 * ============================================================================
 *  PER-TYPE SERIALIZATION
 * ============================================================================
 */

/* ==================== ARMOR ==================== */

static json_t *armor_to_json(ARMOR_DATA *d)
{
    json_t *j = json_object();
    jset_enum(j, "armor_type", d->armor_type, armor_types);
    JSET_INT(j, "armor_strength", d->armor_strength);

    json_t *prot = json_array();
    for (int i = 0; i < ARMOR_PROT_MAX; i++)
        json_array_append_new(prot, json_integer(d->protection[i]));
    json_object_set_new(j, "protection", prot);

    return j;
}

static ARMOR_DATA *armor_from_json(json_t *j)
{
    ARMOR_DATA *d = new_armor_data();
    d->armor_type     = (int16_t)jget_enum(j, "armor_type", armor_types);
    d->armor_strength = JGET_INT16(j, "armor_strength");

    json_t *prot = json_object_get(j, "protection");
    if (json_is_array(prot)) {
        for (int i = 0; i < ARMOR_PROT_MAX && i < (int)json_array_size(prot); i++)
            d->protection[i] = (int16_t)json_integer_value(json_array_get(prot, i));
    }
    return d;
}

/* ==================== BODY PART ==================== */

static json_t *body_part_to_json(BODY_PART_DATA *d)
{
    json_t *j = json_object();
    jset_flags(j, "parts", d->parts, part_flags);
    JSET_INT(j, "race_uid", d->race_uid);
    if (d->char_id[0] || d->char_id[1]) {
        json_t *id = json_array();
        json_array_append_new(id, json_integer(d->char_id[0]));
        json_array_append_new(id, json_integer(d->char_id[1]));
        json_object_set_new(j, "char_id", id);
    }
    return j;
}

static BODY_PART_DATA *body_part_from_json(json_t *j)
{
    BODY_PART_DATA *d = new_body_part_data();
    d->parts    = jget_flags(j, "parts", part_flags);
    d->race_uid = JGET_INT(j, "race_uid", 0);
    json_t *id = json_object_get(j, "char_id");
    if (json_is_array(id) && json_array_size(id) >= 2) {
        d->char_id[0] = json_integer_value(json_array_get(id, 0));
        d->char_id[1] = json_integer_value(json_array_get(id, 1));
    }
    return d;
}

/* ==================== BOOK ==================== */

static json_t *book_to_json(BOOK_DATA *d)
{
    json_t *j = json_object();
    JSET_STR(j, "name", d->name);
    JSET_STR(j, "short_descr", d->short_descr);
    JSET_LONG(j, "flags", d->flags);
    JSET_INT(j, "current_page", d->current_page);
    JSET_INT(j, "open_page", d->open_page);
    /* pages list and lock handled separately by the save system */
    return j;
}

static BOOK_DATA *book_from_json(json_t *j)
{
    BOOK_DATA *d = new_book_data();
    const char *s;
    if ((s = jget_str(j, "name")))          { free_string(d->name); d->name = str_dup(s); }
    if ((s = jget_str(j, "short_descr")))   { free_string(d->short_descr); d->short_descr = str_dup(s); }
    d->flags        = JGET_LONG(j, "flags", 0);
    d->current_page = JGET_INT(j, "current_page", 0);
    d->open_page    = JGET_INT(j, "open_page", 0);
    return d;
}

/* ==================== CART ==================== */

static json_t *cart_to_json(CART_DATA *d)
{
    json_t *j = json_object();
    jset_flags(j, "flags", d->flags, cart_flags);
    JSET_INT(j, "min_strength", d->min_strength);
    JSET_INT(j, "move_delay", d->move_delay);
    JSET_INT(j, "capacity", d->capacity);
    JSET_INT(j, "max_items", d->max_items);
    JSET_INT(j, "weight_multiplier", d->weight_multiplier);
    JSET_INT(j, "vanish_time", d->vanish_time);
    return j;
}

static CART_DATA *cart_from_json(json_t *j)
{
    CART_DATA *d = new_cart_data();
    d->flags             = jget_flags(j, "flags", cart_flags);
    d->min_strength      = JGET_INT16(j, "min_strength");
    d->move_delay        = JGET_INT16(j, "move_delay");
    d->capacity          = JGET_INT(j, "capacity", 0);
    d->max_items         = JGET_INT(j, "max_items", 0);
    d->weight_multiplier = JGET_INT(j, "weight_multiplier", 0);
    d->vanish_time       = JGET_INT(j, "vanish_time", 0);
    return d;
}

/* ==================== COMPASS ==================== */

static json_t *compass_to_json(COMPASS_DATA *d)
{
    json_t *j = json_object();
    JSET_INT(j, "accuracy", d->accuracy);
    JSET_LONG(j, "wuid", d->wuid);
    JSET_LONG(j, "x", d->x);
    JSET_LONG(j, "y", d->y);
    return j;
}

static COMPASS_DATA *compass_from_json(json_t *j)
{
    COMPASS_DATA *d = new_compass_data();
    d->accuracy = JGET_INT16(j, "accuracy");
    d->wuid     = JGET_LONG(j, "wuid", 0);
    d->x        = JGET_LONG(j, "x", 0);
    d->y        = JGET_LONG(j, "y", 0);
    return d;
}

/* ==================== CONTAINER ==================== */

static json_t *container_to_json(CONTAINER_DATA *d)
{
    json_t *j = json_object();
    JSET_STR(j, "name", d->name);
    JSET_STR(j, "short_descr", d->short_descr);
    jset_flags(j, "flags", d->flags, container_flags);
    JSET_INT(j, "max_weight", d->max_weight);
    JSET_INT(j, "weight_multiplier", d->weight_multiplier);
    JSET_INT(j, "max_volume", d->max_volume);
    JSET_INT(j, "max_items", d->max_items);
    /* whitelist/blacklist/lock handled separately */
    return j;
}

static CONTAINER_DATA *container_from_json(json_t *j)
{
    CONTAINER_DATA *d = new_container_data();
    const char *s;
    if ((s = jget_str(j, "name")))          { free_string(d->name); d->name = str_dup(s); }
    if ((s = jget_str(j, "short_descr")))   { free_string(d->short_descr); d->short_descr = str_dup(s); }
    d->flags             = jget_flags(j, "flags", container_flags);
    d->max_weight        = JGET_INT(j, "max_weight", 0);
    d->weight_multiplier = JGET_INT(j, "weight_multiplier", 0);
    d->max_volume        = JGET_INT(j, "max_volume", 0);
    d->max_items         = JGET_INT(j, "max_items", 0);
    return d;
}

/* ==================== CORPSE ==================== */

static json_t *corpse_to_json(CORPSE_DATA *d)
{
    json_t *j = json_object();
    jset_enum(j, "corpse_type", d->corpse_type, corpse_types);
    JSET_INT(j, "resurrection", d->resurrection);
    JSET_INT(j, "animation", d->animation);
    jset_flags(j, "body_parts", d->body_parts, part_flags);
    JSET_LONG(j, "mobile_vnum", d->mobile_vnum);
    JSET_LONG(j, "mobile_area_uid", d->mobile_area_uid);
    return j;
}

static CORPSE_DATA *corpse_from_json(json_t *j)
{
    CORPSE_DATA *d = new_corpse_data();
    d->corpse_type  = (int)jget_enum(j, "corpse_type", corpse_types);
    d->resurrection = JGET_INT(j, "resurrection", 0);
    d->animation    = JGET_INT(j, "animation", 0);
    d->body_parts   = jget_flags(j, "body_parts", part_flags);
    d->mobile_vnum  = JGET_LONG(j, "mobile_vnum", 0);
    d->mobile_area_uid = JGET_LONG(j, "mobile_area_uid", 0);
    return d;
}

/* ==================== FLUID CONTAINER ==================== */

static json_t *fluid_container_to_json(FLUID_CONTAINER_DATA *d)
{
    json_t *j = json_object();
    long uid = liquid_uid(d->liquid);
    JSET_STR(j, "name", d->name);
    JSET_STR(j, "short_descr", d->short_descr);
    JSET_LONG(j, "flags", d->flags);
    JSET_LONG(j, "liquid_uid", uid);
    jset_liquid(j, "liquid", d->liquid);
    JSET_INT(j, "capacity", d->capacity);
    JSET_INT(j, "amount", d->amount);
    JSET_INT(j, "refill_rate", d->refill_rate);
    JSET_INT(j, "poison", d->poison);
    /* lock and spells handled separately */
    return j;
}

static FLUID_CONTAINER_DATA *fluid_container_from_json(json_t *j)
{
    FLUID_CONTAINER_DATA *d = new_fluid_container_data();
    const char *s;
    long uid;
    int index;

    if ((s = jget_str(j, "name")))          { free_string(d->name); d->name = str_dup(s); }
    if ((s = jget_str(j, "short_descr")))   { free_string(d->short_descr); d->short_descr = str_dup(s); }
    d->flags       = JGET_LONG(j, "flags", 0);

    uid = JGET_LONG(j, "liquid_uid", 0);
    index = (uid > 0) ? liquid_index_from_uid(uid) : -1;
    if (index >= 0)
        d->liquid = (int16_t)index;
    else
        d->liquid = (int16_t)jget_liquid(j, "liquid");

    d->capacity    = JGET_INT16(j, "capacity");
    d->amount      = JGET_INT16(j, "amount");
    d->refill_rate = JGET_INT16(j, "refill_rate");
    d->poison      = JGET_INT16(j, "poison");
    return d;
}

/* ==================== FOOD ==================== */

static json_t *food_to_json(FOOD_DATA *d)
{
    json_t *j = json_object();
    JSET_INT(j, "hunger", d->hunger);
    JSET_INT(j, "full", d->full);
    JSET_INT(j, "poison", d->poison);
    JSET_INT(j, "timer", d->timer);
    return j;
}

static FOOD_DATA *food_from_json(json_t *j)
{
    FOOD_DATA *d = new_food_data();
    d->hunger = JGET_INT(j, "hunger", 0);
    d->full   = JGET_INT(j, "full", 0);
    d->poison = JGET_INT(j, "poison", 0);
    d->timer  = JGET_INT(j, "timer", 0);
    return d;
}

/* ==================== FURNITURE ==================== */

static json_t *furniture_to_json(FURNITURE_DATA *d)
{
    json_t *j = json_object();
    jset_flags(j, "flags", d->flags, furniture_flags);
    JSET_INT(j, "max_people", d->max_people);
    JSET_INT(j, "max_weight", d->max_weight);
    JSET_INT(j, "heal_rate", d->heal_rate);
    JSET_INT(j, "mana_rate", d->mana_rate);
    JSET_INT(j, "move_rate", d->move_rate);
    jset_enum(j, "standing", d->standing, position_flags);
    jset_enum(j, "sitting", d->sitting, position_flags);
    jset_enum(j, "resting", d->resting, position_flags);
    jset_enum(j, "sleeping", d->sleeping, position_flags);
    return j;
}

static FURNITURE_DATA *furniture_from_json(json_t *j)
{
    FURNITURE_DATA *d = new_furniture_data();
    d->flags      = jget_flags(j, "flags", furniture_flags);
    d->max_people = JGET_INT(j, "max_people", 0);
    d->max_weight = JGET_INT(j, "max_weight", 0);
    d->heal_rate  = JGET_INT(j, "heal_rate", 0);
    d->mana_rate  = JGET_INT(j, "mana_rate", 0);
    d->move_rate  = JGET_INT(j, "move_rate", 0);
    d->standing   = jget_enum(j, "standing", position_flags);
    d->sitting    = jget_enum(j, "sitting", position_flags);
    d->resting    = jget_enum(j, "resting", position_flags);
    d->sleeping   = jget_enum(j, "sleeping", position_flags);
    return d;
}

/* ==================== HERB ==================== */

static json_t *herb_to_json(HERB_DATA *d)
{
    json_t *j = json_object();
    jset_herb_type(j, "type", d->type);
    JSET_INT(j, "healing", d->healing);
    JSET_INT(j, "regenerative", d->regenerative);
    JSET_INT(j, "refreshing", d->refreshing);
    jset_flags(j, "immunity", d->immunity, imm_flags);
    jset_flags(j, "resistance", d->resistance, res_flags);
    jset_flags(j, "vulnerability", d->vulnerability, vuln_flags);
    JSET_INT(j, "spell", d->spell);
    return j;
}

static HERB_DATA *herb_from_json(json_t *j)
{
    HERB_DATA *d = new_herb_data();
    d->type          = jget_herb_type(j, "type");
    d->healing       = JGET_INT(j, "healing", 0);
    d->regenerative  = JGET_INT(j, "regenerative", 0);
    d->refreshing    = JGET_INT(j, "refreshing", 0);
    d->immunity      = jget_flags(j, "immunity", imm_flags);
    d->resistance    = jget_flags(j, "resistance", res_flags);
    d->vulnerability = jget_flags(j, "vulnerability", vuln_flags);
    d->spell         = JGET_INT(j, "spell", 0);
    return d;
}

/* ==================== INK ==================== */

static json_t *ink_to_json(INK_DATA *d)
{
    json_t *j = json_object();
    json_t *types = json_array();
    for (int i = 0; i < MAX_INK_TYPES; i++)
        json_array_append_new(types, json_integer(d->types[i]));
    json_object_set_new(j, "types", types);

    json_t *amounts = json_array();
    for (int i = 0; i < MAX_INK_TYPES; i++)
        json_array_append_new(amounts, json_integer(d->amounts[i]));
    json_object_set_new(j, "amounts", amounts);
    return j;
}

static INK_DATA *ink_from_json(json_t *j)
{
    INK_DATA *d = new_ink_data();
    json_t *types = json_object_get(j, "types");
    if (json_is_array(types)) {
        for (int i = 0; i < MAX_INK_TYPES && i < (int)json_array_size(types); i++)
            d->types[i] = (int16_t)json_integer_value(json_array_get(types, i));
    }
    json_t *amounts = json_object_get(j, "amounts");
    if (json_is_array(amounts)) {
        for (int i = 0; i < MAX_INK_TYPES && i < (int)json_array_size(amounts); i++)
            d->amounts[i] = (int16_t)json_integer_value(json_array_get(amounts, i));
    }
    return d;
}

/* ==================== INSTRUMENT ==================== */

static json_t *instrument_to_json(INSTRUMENT_DATA *d)
{
    json_t *j = json_object();
    jset_enum(j, "type", d->type, instrument_types);
    jset_flags(j, "flags", d->flags, instrument_flags);
    JSET_INT(j, "mana_min", d->mana_min);
    JSET_INT(j, "mana_max", d->mana_max);
    JSET_INT(j, "beats_min", d->beats_min);
    JSET_INT(j, "beats_max", d->beats_max);

    json_t *reservoirs = json_array();
    for (int i = 0; i < INSTRUMENT_MAX_CATALYSTS; i++) {
        json_t *r = json_object();
        jset_enum(r, "type", d->reservoirs[i].type, catalyst_types);
        JSET_INT(r, "amount", d->reservoirs[i].amount);
        JSET_INT(r, "capacity", d->reservoirs[i].capacity);
        json_array_append_new(reservoirs, r);
    }
    json_object_set_new(j, "reservoirs", reservoirs);
    return j;
}

static INSTRUMENT_DATA *instrument_from_json(json_t *j)
{
    INSTRUMENT_DATA *d = new_instrument_data();
    d->type      = (int)jget_enum(j, "type", instrument_types);
    d->flags     = jget_flags(j, "flags", instrument_flags);
    d->mana_min  = JGET_INT(j, "mana_min", 0);
    d->mana_max  = JGET_INT(j, "mana_max", 0);
    d->beats_min = JGET_INT(j, "beats_min", 0);
    d->beats_max = JGET_INT(j, "beats_max", 0);

    json_t *reservoirs = json_object_get(j, "reservoirs");
    if (json_is_array(reservoirs)) {
        for (int i = 0; i < INSTRUMENT_MAX_CATALYSTS && i < (int)json_array_size(reservoirs); i++) {
            json_t *r = json_array_get(reservoirs, i);
            d->reservoirs[i].type     = (int16_t)jget_enum(r, "type", catalyst_types);
            d->reservoirs[i].amount   = JGET_INT16(r, "amount");
            d->reservoirs[i].capacity = JGET_INT16(r, "capacity");
        }
    }
    return d;
}

/* ==================== ITEM SHIP ==================== */

static json_t *item_ship_to_json(ITEM_SHIP_DATA *d)
{
    json_t *j = json_object();
    JSET_INT(j, "weight", d->weight);
    JSET_INT(j, "move_delay", d->move_delay);
    JSET_INT(j, "min_crew", d->min_crew);
    JSET_INT(j, "capacity", d->capacity);
    JSET_INT(j, "max_crew", d->max_crew);
    JSET_LONG(j, "first_room", d->first_room);
    JSET_LONG(j, "first_room_area_uid", d->first_room_area_uid);
    JSET_INT(j, "hit_points", d->hit_points);
    JSET_INT(j, "max_guns", d->max_guns);
    return j;
}

static ITEM_SHIP_DATA *item_ship_from_json(json_t *j)
{
    ITEM_SHIP_DATA *d = new_item_ship_data();
    d->weight     = JGET_INT(j, "weight", 0);
    d->move_delay = JGET_INT(j, "move_delay", 0);
    d->min_crew   = JGET_INT(j, "min_crew", 0);
    d->capacity   = JGET_INT(j, "capacity", 0);
    d->max_crew   = JGET_INT(j, "max_crew", 0);
    d->first_room = JGET_LONG(j, "first_room", 0);
    d->first_room_area_uid = JGET_LONG(j, "first_room_area_uid", 0);
    d->hit_points = JGET_INT(j, "hit_points", 0);
    d->max_guns   = JGET_INT(j, "max_guns", 0);
    return d;
}

/* ==================== JEWELRY ==================== */

static json_t *jewelry_to_json(JEWELRY_DATA *d)
{
    json_t *j = json_object();
    JSET_INT(j, "max_mana", d->max_mana);
    /* spells handled separately */
    return j;
}

static JEWELRY_DATA *jewelry_from_json(json_t *j)
{
    JEWELRY_DATA *d = new_jewelry_data();
    d->max_mana = JGET_INT(j, "max_mana", 0);
    return d;
}

/* ==================== LIGHT ==================== */

static json_t *light_to_json(LIGHT_DATA *d)
{
    json_t *j = json_object();
    jset_flags(j, "flags", d->flags, light_flags);
    JSET_INT(j, "duration", d->duration);
    return j;
}

static LIGHT_DATA *light_from_json(json_t *j)
{
    LIGHT_DATA *d = new_light_data();
    d->flags    = jget_flags(j, "flags", light_flags);
    d->duration = JGET_INT(j, "duration", 0);
    return d;
}

/* ==================== MAP ==================== */

static json_t *map_to_json(MAP_DATA *d)
{
    json_t *j = json_object();
    JSET_LONG(j, "wuid", d->wuid);
    JSET_LONG(j, "x", d->x);
    JSET_LONG(j, "y", d->y);
    /* waypoints handled separately by the save system */
    return j;
}

static MAP_DATA *map_from_json(json_t *j)
{
    MAP_DATA *d = new_map_data();
    d->wuid = JGET_LONG(j, "wuid", 0);
    d->x    = JGET_LONG(j, "x", 0);
    d->y    = JGET_LONG(j, "y", 0);
    return d;
}

/* ==================== MIST ==================== */

static json_t *mist_to_json(MIST_DATA *d)
{
    json_t *j = json_object();
    JSET_INT(j, "obscure_mobs", d->obscure_mobs);
    JSET_INT(j, "obscure_objs", d->obscure_objs);
    JSET_INT(j, "obscure_room", d->obscure_room);
    JSET_INT(j, "icy", d->icy);
    JSET_INT(j, "fiery", d->fiery);
    JSET_INT(j, "acidic", d->acidic);
    JSET_INT(j, "stink", d->stink);
    JSET_INT(j, "wither", d->wither);
    JSET_INT(j, "toxic", d->toxic);
    JSET_INT(j, "shock", d->shock);
    JSET_INT(j, "fog", d->fog);
    JSET_INT(j, "sleep", d->sleep);
    return j;
}

static MIST_DATA *mist_from_json(json_t *j)
{
    MIST_DATA *d = new_mist_data();
    d->obscure_mobs = JGET_CHAR(j, "obscure_mobs");
    d->obscure_objs = JGET_CHAR(j, "obscure_objs");
    d->obscure_room = JGET_CHAR(j, "obscure_room");
    d->icy          = JGET_CHAR(j, "icy");
    d->fiery        = JGET_CHAR(j, "fiery");
    d->acidic       = JGET_CHAR(j, "acidic");
    d->stink        = JGET_CHAR(j, "stink");
    d->wither       = JGET_CHAR(j, "wither");
    d->toxic        = JGET_CHAR(j, "toxic");
    d->shock        = JGET_CHAR(j, "shock");
    d->fog          = JGET_CHAR(j, "fog");
    d->sleep        = JGET_CHAR(j, "sleep");
    return d;
}

/* ==================== MONEY ==================== */

static json_t *money_to_json(MONEY_DATA *d)
{
    json_t *j = json_object();
    JSET_INT(j, "silver", d->silver);
    JSET_INT(j, "gold", d->gold);
    return j;
}

static MONEY_DATA *money_from_json(json_t *j)
{
    MONEY_DATA *d = new_money_data();
    d->silver = JGET_INT(j, "silver", 0);
    d->gold   = JGET_INT(j, "gold", 0);
    return d;
}

/* ==================== PAGE ==================== */

static json_t *page_to_json(PAGE_DATA *d)
{
    json_t *j = json_object();
    JSET_INT(j, "page_no", d->page_no);
    JSET_STR(j, "title", d->title);
    JSET_STR(j, "text", d->text);
    return j;
}

static PAGE_DATA *page_from_json(json_t *j)
{
    PAGE_DATA *d = new_page_data();
    const char *s;
    d->page_no = JGET_INT(j, "page_no", 0);
    if ((s = jget_str(j, "title"))) { free_string(d->title); d->title = str_dup(s); }
    if ((s = jget_str(j, "text")))  { free_string(d->text); d->text = str_dup(s); }
    return d;
}

/* ==================== PORTAL ==================== */

static json_t *portal_to_json(PORTAL_DATA *d)
{
    json_t *j = json_object();
    JSET_STR(j, "name", d->name);
    JSET_STR(j, "short_descr", d->short_descr);
    jset_flags(j, "exit", d->exit, portal_exit_flags);
    jset_flags(j, "flags", d->flags, portal_flags);
    JSET_INT(j, "charges", d->charges);
    JSET_INT(j, "type", d->type);

    json_t *params = json_array();
    for (int i = 0; i < MAX_PORTAL_VALUES; i++)
        json_array_append_new(params, json_integer(d->params[i]));
    json_object_set_new(j, "params", params);
    /* lock handled separately */
    return j;
}

static PORTAL_DATA *portal_from_json(json_t *j)
{
    PORTAL_DATA *d = new_portal_data();
    const char *s;
    if ((s = jget_str(j, "name")))          { free_string(d->name); d->name = str_dup(s); }
    if ((s = jget_str(j, "short_descr")))   { free_string(d->short_descr); d->short_descr = str_dup(s); }
    d->exit    = jget_flags(j, "exit", portal_exit_flags);
    d->flags   = jget_flags(j, "flags", portal_flags);
    d->charges = JGET_INT(j, "charges", 0);
    d->type    = JGET_INT(j, "type", 0);

    json_t *params = json_object_get(j, "params");
    if (json_is_array(params)) {
        for (int i = 0; i < MAX_PORTAL_VALUES && i < (int)json_array_size(params); i++)
            d->params[i] = (long)json_integer_value(json_array_get(params, i));
    }
    return d;
}

/* ==================== SCROLL ==================== */

static json_t *scroll_to_json(SCROLL_DATA *d)
{
    json_t *j = json_object();
    JSET_INT(j, "max_mana", d->max_mana);
    jset_flags(j, "flags", d->flags, scroll_flags);
    /* spells handled separately */
    return j;
}

static SCROLL_DATA *scroll_from_json(json_t *j)
{
    SCROLL_DATA *d = new_scroll_data();
    d->max_mana = JGET_INT(j, "max_mana", 0);
    d->flags    = jget_flags(j, "flags", scroll_flags);
    return d;
}

/* ==================== SEED ==================== */

static json_t *seed_to_json(SEED_DATA *d)
{
    json_t *j = json_object();
    JSET_INT(j, "growth_time", d->growth_time);
    JSET_LONG(j, "object_vnum", d->object_vnum);
    JSET_LONG(j, "object_area_uid", d->object_area_uid);
    return j;
}

static SEED_DATA *seed_from_json(json_t *j)
{
    SEED_DATA *d = new_seed_data();
    d->growth_time = JGET_INT(j, "growth_time", 0);
    d->object_vnum = JGET_LONG(j, "object_vnum", 0);
    d->object_area_uid = JGET_LONG(j, "object_area_uid", 0);
    return d;
}

/* ==================== SEXTANT ==================== */

static json_t *sextant_to_json(SEXTANT_DATA *d)
{
    json_t *j = json_object();
    JSET_INT(j, "accuracy", d->accuracy);
    return j;
}

static SEXTANT_DATA *sextant_from_json(json_t *j)
{
    SEXTANT_DATA *d = new_sextant_data();
    d->accuracy = JGET_INT16(j, "accuracy");
    return d;
}

/* ==================== TATTOO ==================== */

static json_t *tattoo_to_json(TATTOO_DATA *d)
{
    json_t *j = json_object();
    JSET_INT(j, "touches", d->touches);
    JSET_INT(j, "fading_chance", d->fading_chance);
    JSET_INT(j, "fading_rate", d->fading_rate);
    /* spells handled separately */
    return j;
}

static TATTOO_DATA *tattoo_from_json(json_t *j)
{
    TATTOO_DATA *d = new_tattoo_data();
    d->touches      = JGET_INT(j, "touches", 0);
    d->fading_chance = JGET_INT(j, "fading_chance", 0);
    d->fading_rate  = JGET_INT(j, "fading_rate", 0);
    return d;
}

/* ==================== TELESCOPE ==================== */

static json_t *telescope_to_json(TELESCOPE_DATA *d)
{
    json_t *j = json_object();
    JSET_INT(j, "distance", d->distance);
    JSET_INT(j, "min_distance", d->min_distance);
    JSET_INT(j, "max_distance", d->max_distance);
    JSET_INT(j, "bonus_view", d->bonus_view);
    JSET_INT(j, "heading", d->heading);
    return j;
}

static TELESCOPE_DATA *telescope_from_json(json_t *j)
{
    TELESCOPE_DATA *d = new_telescope_data();
    d->distance     = JGET_INT16(j, "distance");
    d->min_distance = JGET_INT16(j, "min_distance");
    d->max_distance = JGET_INT16(j, "max_distance");
    d->bonus_view   = JGET_INT16(j, "bonus_view");
    d->heading      = JGET_INT16(j, "heading");
    return d;
}

/* ==================== TOOL ==================== */

static json_t *tool_to_json(TOOL_DATA *d)
{
    json_t *j = json_object();
    jset_enum(j, "type", d->type, tool_types);
    JSET_INT(j, "tier", d->tier);
    return j;
}

static TOOL_DATA *tool_from_json(json_t *j)
{
    TOOL_DATA *d = new_tool_data();
    d->type = (int16_t)jget_enum(j, "type", tool_types);
    d->tier = JGET_INT16(j, "tier");
    return d;
}

/* ==================== TRADE ==================== */

static json_t *trade_to_json(TRADE_DATA *d)
{
    json_t *j = json_object();
    jset_trade_type(j, "trade_type", d->trade_type);
    return j;
}

static TRADE_DATA *trade_from_json(json_t *j)
{
    TRADE_DATA *d = new_trade_data();
    d->trade_type = jget_trade_type(j, "trade_type");
    return d;
}

/* ==================== WAND ==================== */

static json_t *wand_to_json(WAND_DATA *d)
{
    json_t *j = json_object();
    JSET_INT(j, "max_mana", d->max_mana);
    JSET_INT(j, "charges", d->charges);
    JSET_INT(j, "max_charges", d->max_charges);
    JSET_INT(j, "cooldown", d->cooldown);
    JSET_INT(j, "recharge_time", d->recharge_time);
    /* spells handled separately */
    return j;
}

static WAND_DATA *wand_from_json(json_t *j)
{
    WAND_DATA *d = new_wand_data();
    d->max_mana      = JGET_INT(j, "max_mana", 0);
    d->charges       = JGET_INT(j, "charges", 0);
    d->max_charges   = JGET_INT(j, "max_charges", 0);
    d->cooldown      = JGET_INT(j, "cooldown", 0);
    d->recharge_time = JGET_INT(j, "recharge_time", 0);
    return d;
}

/* ==================== WEAPON ==================== */

static json_t *weapon_to_json(WEAPON_DATA *d)
{
    json_t *j = json_object();
    jset_enum(j, "weapon_class", d->weapon_class, weapon_class);
    jset_attack_type(j, "damage_type", d->damage_type);
    jset_flags(j, "flags", d->flags, weapon_type2);
    JSET_INT(j, "damage_number", d->damage.number);
    JSET_INT(j, "damage_size", d->damage.size);
    JSET_INT(j, "damage_bonus", d->damage.bonus);
    JSET_INT(j, "range", d->range);
    JSET_INT(j, "max_mana", d->max_mana);
    JSET_INT(j, "charges", d->charges);
    JSET_INT(j, "max_charges", d->max_charges);
    JSET_INT(j, "cooldown", d->cooldown);
    JSET_INT(j, "recharge_time", d->recharge_time);
    /* spells handled separately */
    return j;
}

static WEAPON_DATA *weapon_from_json(json_t *j)
{
    WEAPON_DATA *d = new_weapon_data();
    d->weapon_class   = (int16_t)jget_enum(j, "weapon_class", weapon_class);
    d->damage_type    = jget_attack_type(j, "damage_type");
    d->flags          = jget_flags(j, "flags", weapon_type2);
    d->damage.number  = JGET_INT(j, "damage_number", 0);
    d->damage.size    = JGET_INT(j, "damage_size", 0);
    d->damage.bonus   = JGET_INT(j, "damage_bonus", 0);
    d->range          = JGET_INT(j, "range", 0);
    d->max_mana       = JGET_INT(j, "max_mana", 0);
    d->charges        = JGET_INT(j, "charges", 0);
    d->max_charges    = JGET_INT(j, "max_charges", 0);
    d->cooldown       = JGET_INT(j, "cooldown", 0);
    d->recharge_time  = JGET_INT(j, "recharge_time", 0);
    return d;
}

/* ==================== WEAPON CONTAINER ==================== */

static json_t *weapon_container_to_json(WEAPON_CONTAINER_DATA *d)
{
    json_t *j = json_object();
    JSET_INT(j, "max_weight", d->max_weight);
    jset_enum(j, "weapon_type", d->weapon_type, weapon_class);
    JSET_INT(j, "max_items", d->max_items);
    JSET_INT(j, "weight_multiplier", d->weight_multiplier);
    return j;
}

static WEAPON_CONTAINER_DATA *weapon_container_from_json(json_t *j)
{
    WEAPON_CONTAINER_DATA *d = new_weapon_container_data();
    d->max_weight        = JGET_INT(j, "max_weight", 0);
    d->weapon_type       = (int)jget_enum(j, "weapon_type", weapon_class);
    d->max_items         = JGET_INT(j, "max_items", 0);
    d->weight_multiplier = JGET_INT(j, "weight_multiplier", 0);
    return d;
}


/*
 * ============================================================================
 *  PUBLIC API — Aggregate type data to/from JSON
 * ============================================================================
 *
 *  Uses a macro to avoid duplicating the serialization/deserialization
 *  dispatch for both OBJ_DATA and OBJ_INDEX_DATA.
 */

#define TYPE_DATA_TO_JSON_BODY(O) \
    json_t *j = json_object(); \
    bool has_data = false; \
    \
    if (IS_ARMOR(O))     { json_object_set_new(j, "armor",     armor_to_json(ARMOR(O)));     has_data = true; } \
    if (IS_BODY_PART(O)) { json_object_set_new(j, "body_part", body_part_to_json(BODY_PART(O))); has_data = true; } \
    if (IS_BOOK(O))      { json_object_set_new(j, "book",      book_to_json(BOOK(O)));       has_data = true; } \
    if (IS_CART(O))      { json_object_set_new(j, "cart",      cart_to_json(CART(O)));       has_data = true; } \
    if (IS_COMPASS(O))   { json_object_set_new(j, "compass",   compass_to_json(COMPASS(O))); has_data = true; } \
    if (IS_CONTAINER(O)) { json_object_set_new(j, "container", container_to_json(CONTAINER(O))); has_data = true; } \
    if (IS_CORPSE(O))    { json_object_set_new(j, "corpse",    corpse_to_json(CORPSE(O)));   has_data = true; } \
    if (IS_FLUID_CON(O)) { json_object_set_new(j, "fluid_container", fluid_container_to_json(FLUID_CON(O))); has_data = true; } \
    if (IS_FOOD(O))      { json_object_set_new(j, "food",      food_to_json(FOOD(O)));       has_data = true; } \
    if (IS_FURNITURE(O)) { json_object_set_new(j, "furniture", furniture_to_json(FURNITURE(O))); has_data = true; } \
    if (IS_HERB(O))      { json_object_set_new(j, "herb",      herb_to_json(HERB(O)));       has_data = true; } \
    if (IS_INK(O))       { json_object_set_new(j, "ink",       ink_to_json(INK(O)));         has_data = true; } \
    if (IS_INSTRUMENT(O)){ json_object_set_new(j, "instrument",instrument_to_json(INSTRUMENT(O))); has_data = true; } \
    if (IS_SHIP_TYPE(O)) { json_object_set_new(j, "ship",      item_ship_to_json(SHIP_TYPE(O))); has_data = true; } \
    if (IS_JEWELRY(O))   { json_object_set_new(j, "jewelry",   jewelry_to_json(JEWELRY(O))); has_data = true; } \
    if (IS_LIGHT(O))     { json_object_set_new(j, "light",     light_to_json(LIGHT(O)));     has_data = true; } \
    if (IS_MAP(O))       { json_object_set_new(j, "map",       map_to_json(MAP(O)));         has_data = true; } \
    if (IS_MIST(O))      { json_object_set_new(j, "mist",      mist_to_json(MIST(O)));       has_data = true; } \
    if (IS_MONEY(O))     { json_object_set_new(j, "money",     money_to_json(MONEY(O)));     has_data = true; } \
    if (IS_PAGE(O))      { json_object_set_new(j, "page",      page_to_json(PAGE(O)));       has_data = true; } \
    if (IS_PORTAL(O))    { json_object_set_new(j, "portal",    portal_to_json(PORTAL(O)));   has_data = true; } \
    if (IS_SCROLL(O))    { json_object_set_new(j, "scroll",    scroll_to_json(SCROLL(O)));   has_data = true; } \
    if (IS_SEED(O))      { json_object_set_new(j, "seed",      seed_to_json(SEED(O)));       has_data = true; } \
    if (IS_SEXTANT(O))   { json_object_set_new(j, "sextant",   sextant_to_json(SEXTANT(O))); has_data = true; } \
    if (IS_TATTOO(O))    { json_object_set_new(j, "tattoo",    tattoo_to_json(TATTOO(O)));   has_data = true; } \
    if (IS_TELESCOPE(O)) { json_object_set_new(j, "telescope", telescope_to_json(TELESCOPE(O))); has_data = true; } \
    if (IS_TOOL(O))      { json_object_set_new(j, "tool",      tool_to_json(TOOL(O)));       has_data = true; } \
    if (IS_TRADE(O))     { json_object_set_new(j, "trade",     trade_to_json(TRADE(O)));     has_data = true; } \
    if (IS_WAND(O))      { json_object_set_new(j, "wand",      wand_to_json(WAND(O)));       has_data = true; } \
    if (IS_WEAPON_CON(O)){ json_object_set_new(j, "weapon_container", weapon_container_to_json(WEAPON_CON(O))); has_data = true; } \
    if (IS_WEAPON(O))    { json_object_set_new(j, "weapon",    weapon_to_json(WEAPON(O)));   has_data = true; } \
    \
    if (!has_data) { json_decref(j); return NULL; } \
    return j;


json_t *obj_type_data_to_json(OBJ_DATA *obj)
{
    if (!obj) return NULL;
    TYPE_DATA_TO_JSON_BODY(obj)
}

json_t *obj_index_type_data_to_json(OBJ_INDEX_DATA *obj)
{
    if (!obj) return NULL;
    TYPE_DATA_TO_JSON_BODY(obj)
}


/*
 * Deserialization: each type key maps to its from_json function.
 * Frees any existing type data before replacing it.
 */

#define LOAD_TYPE(O, j, key, field, from_fn, free_fn, type_id) \
    do { \
        json_t *_sub = json_object_get((j), (key)); \
        if (_sub && json_is_object(_sub)) { \
            if ((O)->field) free_fn((O)->field); \
            (O)->field = from_fn(_sub); \
            TBIT_SET((O)->type_flags, type_id); \
        } \
    } while(0)

#define TYPE_DATA_FROM_JSON_BODY(O, j) \
    LOAD_TYPE(O, j, "armor",           _armor,           armor_from_json,           free_armor_data,           ITEM_ARMOUR); \
    LOAD_TYPE(O, j, "body_part",       _body_part,       body_part_from_json,       free_body_part_data,       ITEM_BODY_PART); \
    LOAD_TYPE(O, j, "book",            _book,            book_from_json,            free_book_data,            ITEM_BOOK); \
    LOAD_TYPE(O, j, "cart",            _cart,            cart_from_json,            free_cart_data,            ITEM_CART); \
    LOAD_TYPE(O, j, "compass",         _compass,         compass_from_json,         free_compass_data,         ITEM_COMPASS); \
    LOAD_TYPE(O, j, "container",       _container,       container_from_json,       free_container_data,       ITEM_CONTAINER); \
    LOAD_TYPE(O, j, "corpse",          _corpse,          corpse_from_json,          free_corpse_data,          ITEM_CORPSE_NPC); \
    LOAD_TYPE(O, j, "fluid_container", _fluid_container, fluid_container_from_json, free_fluid_container_data, ITEM_DRINK_CON); \
    LOAD_TYPE(O, j, "food",            _food,            food_from_json,            free_food_data,            ITEM_FOOD); \
    LOAD_TYPE(O, j, "furniture",       _furniture,       furniture_from_json,       free_furniture_data,       ITEM_FURNITURE); \
    LOAD_TYPE(O, j, "herb",            _herb,            herb_from_json,            free_herb_data,            ITEM_HERB); \
    LOAD_TYPE(O, j, "ink",             _ink,             ink_from_json,             free_ink_data,             ITEM_INK); \
    LOAD_TYPE(O, j, "instrument",      _instrument,      instrument_from_json,      free_instrument_data,      ITEM_INSTRUMENT); \
    LOAD_TYPE(O, j, "ship",            _item_ship,       item_ship_from_json,       free_item_ship_data,       ITEM_SHIP); \
    LOAD_TYPE(O, j, "jewelry",         _jewelry,         jewelry_from_json,         free_jewelry_data,         ITEM_JEWELRY); \
    LOAD_TYPE(O, j, "light",           _light,           light_from_json,           free_light_data,           ITEM_LIGHT); \
    LOAD_TYPE(O, j, "map",             _map,             map_from_json,             free_map_data,             ITEM_MAP); \
    LOAD_TYPE(O, j, "mist",            _mist,            mist_from_json,            free_mist_data,            ITEM_MIST); \
    LOAD_TYPE(O, j, "money",           _money,           money_from_json,           free_money_data,           ITEM_MONEY); \
    LOAD_TYPE(O, j, "page",            _page,            page_from_json,            free_page_data,            ITEM_PAGE); \
    LOAD_TYPE(O, j, "portal",          _portal,          portal_from_json,          free_portal_data,          ITEM_PORTAL); \
    LOAD_TYPE(O, j, "scroll",          _scroll,          scroll_from_json,          free_scroll_data,          ITEM_SCROLL); \
    LOAD_TYPE(O, j, "seed",            _seed,            seed_from_json,            free_seed_data,            ITEM_SEED); \
    LOAD_TYPE(O, j, "sextant",         _sextant,         sextant_from_json,         free_sextant_data,         ITEM_SEXTANT); \
    LOAD_TYPE(O, j, "tattoo",          _tattoo,          tattoo_from_json,          free_tattoo_data,          ITEM_TATTOO); \
    LOAD_TYPE(O, j, "telescope",       _telescope,       telescope_from_json,       free_telescope_data,       ITEM_TELESCOPE); \
    LOAD_TYPE(O, j, "tool",            _tool,            tool_from_json,            free_tool_data,            ITEM_TOOL); \
    LOAD_TYPE(O, j, "trade",           _trade,           trade_from_json,           free_trade_data,           ITEM_TRADE_TYPE); \
    LOAD_TYPE(O, j, "wand",            _wand,            wand_from_json,            free_wand_data,            ITEM_WAND); \
    LOAD_TYPE(O, j, "weapon_container",_weapon_container, weapon_container_from_json, free_weapon_container_data, ITEM_WEAPON_CONTAINER); \
    LOAD_TYPE(O, j, "weapon",          _weapon,          weapon_from_json,          free_weapon_data,          ITEM_WEAPON);


void obj_type_data_from_json(OBJ_DATA *obj, json_t *json)
{
    if (!obj || !json || !json_is_object(json)) return;
    TYPE_DATA_FROM_JSON_BODY(obj, json)
}

void obj_index_type_data_from_json(OBJ_INDEX_DATA *obj, json_t *json)
{
    if (!obj || !json || !json_is_object(json)) return;
    TYPE_DATA_FROM_JSON_BODY(obj, json)
}

#undef TYPE_DATA_TO_JSON_BODY
#undef TYPE_DATA_FROM_JSON_BODY
#undef LOAD_TYPE
#undef ALLOC_TYPE
#undef JSET_INT
#undef JSET_LONG
#undef JSET_STR
#undef JGET_INT
#undef JGET_LONG
#undef JGET_INT16
#undef JGET_CHAR
