/***************************************************************************
 *  Sentience MUD                                                          *
 *  OLC Object Editor — Type-specific subcommands.                         *
 *                                                                         *
 *  Each item type that carries type data gets a dedicated oedit command    *
 *  (e.g. "weapon", "portal", "container") so builders can interact with   *
 *  multityped objects by name instead of the legacy v0-v7 indices.        *
 ***************************************************************************/

#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../item_types.h"
#include "../../recycle.h"
#include <string.h>
#include "../../skill_data.h"
#include "../common.h"
#include "../common/olc_changeset.h"
#include "../common/olc_commands.h"
#include "../common/olc_staged.h"
#include "../common/olc_editor.h"
#include "../common/olc_field_handlers.h"
#include "../common/olc_display.h"
#include <jansson.h>

/* Forward declarations for helpers used by type commands */
extern void set_weapon_dice(OBJ_INDEX_DATA *objIndex);
extern void set_armour(OBJ_INDEX_DATA *objIndex);
extern int  get_armour_strength(char *argument);

/*
 * Type Dispatch Infrastructure
 *
 * Per-type apply/serialize function pairs called from the typedata/**
 * wildcard handler in oedit.c.
 */

/**
 * Per-type field apply function.
 * Receives the entity, the field name (after the type prefix), and the change.
 * E.g., for path "typedata/weapon/class", field_name = "class".
 */
typedef bool (*oedit_type_apply_fn)(OBJ_INDEX_DATA *pObj,
    const char *field_name, olc_pending_change_t *change);

/**
 * Per-type serialization function.
 * Returns a JSON object with all fields for this type.
 */
typedef json_t *(*oedit_type_serialize_fn)(const OBJ_INDEX_DATA *pObj);

/**
 * Per-type schema function.
 * Returns a JSON array of field descriptors for this type.
 */
typedef json_t *(*oedit_type_schema_fn)(const OBJ_INDEX_DATA *pObj);

typedef struct {
    const char             *type_name;
    oedit_type_apply_fn     apply_fn;
    oedit_type_serialize_fn serialize_fn;
    oedit_type_schema_fn    schema_fn;
} oedit_type_dispatch_t;

/* Stubs — implemented in Tasks 8-10 */
static bool armor_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_ARMOR(pObj)) return false;
    if (strcmp(field, "pierce") == 0) { ARMOR(pObj)->protection[0] = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "bash") == 0) { ARMOR(pObj)->protection[1] = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "slash") == 0) { ARMOR(pObj)->protection[2] = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "exotic") == 0) { ARMOR(pObj)->protection[3] = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "strength") == 0) { ARMOR(pObj)->armor_strength = (int)json_integer_value(change->new_value); set_armour(pObj); return true; }
    if (strcmp(field, "type") == 0) { ARMOR(pObj)->armor_type = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *armor_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_ARMOR(pObj)) return json_null();
    return json_pack("{s:i, s:i, s:i, s:i, s:i, s:i}",
        "pierce", (int)ARMOR(pObj)->protection[0],
        "bash", (int)ARMOR(pObj)->protection[1],
        "slash", (int)ARMOR(pObj)->protection[2],
        "exotic", (int)ARMOR(pObj)->protection[3],
        "strength", (int)ARMOR(pObj)->armor_strength,
        "type", (int)ARMOR(pObj)->armor_type);
}

static json_t *armor_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_ARMOR(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Type:", "command", "typedata/armor/type",
        "type", "enum", "value", flag_string(armor_types, ARMOR(pObj)->armor_type),
        "options", olc_flag_options_json(armor_types));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Strength:", "command", "typedata/armor/strength",
        "type", "enum", "value", flag_string(armour_strength_table, ARMOR(pObj)->armor_strength),
        "options", olc_flag_options_json(armour_strength_table));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Pierce:", "command", "typedata/armor/pierce",
        "type", "int", "value", (int)ARMOR(pObj)->protection[0]);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Bash:", "command", "typedata/armor/bash",
        "type", "int", "value", (int)ARMOR(pObj)->protection[1]);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Slash:", "command", "typedata/armor/slash",
        "type", "int", "value", (int)ARMOR(pObj)->protection[2]);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Exotic:", "command", "typedata/armor/exotic",
        "type", "int", "value", (int)ARMOR(pObj)->protection[3]);
    json_array_append_new(arr, f);
    return arr;
}

static bool bodypart_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_BODY_PART(pObj)) return false;
    if (strcmp(field, "parts") == 0) { BODY_PART(pObj)->parts = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "race") == 0) { BODY_PART(pObj)->race_uid = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *bodypart_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_BODY_PART(pObj)) return json_null();
    return json_pack("{s:I, s:i}", "parts", (json_int_t)BODY_PART(pObj)->parts, "race", BODY_PART(pObj)->race_uid);
}

static json_t *bodypart_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_BODY_PART(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Parts:", "command", "typedata/bodypart/parts",
        "type", "flags", "value", flag_string(part_flags, BODY_PART(pObj)->parts),
        "options", olc_flag_options_json(part_flags));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Race:", "command", "typedata/bodypart/race",
        "type", "int", "value", BODY_PART(pObj)->race_uid);
    json_array_append_new(arr, f);
    return arr;
}

static bool book_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_BOOK(pObj)) return false;
    if (strcmp(field, "flags") == 0) { BOOK(pObj)->flags = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *book_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_BOOK(pObj)) return json_null();
    return json_pack("{s:i}", "flags", BOOK(pObj)->flags);
}

static json_t *book_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_BOOK(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Flags:", "command", "typedata/book/flags",
        "type", "flags", "value", flag_string(container_flags, BOOK(pObj)->flags),
        "options", olc_flag_options_json(container_flags));
    json_array_append_new(arr, f);
    return arr;
}

static bool cart_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_CART(pObj)) return false;
    if (strcmp(field, "capacity") == 0) { CART(pObj)->capacity = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "delay") == 0) { CART(pObj)->move_delay = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "strength") == 0) { CART(pObj)->min_strength = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "items") == 0) { CART(pObj)->max_items = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "weightmult") == 0) { CART(pObj)->weight_multiplier = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "flags") == 0) { CART(pObj)->flags = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "vanish") == 0) { CART(pObj)->vanish_time = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *cart_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_CART(pObj)) return json_null();
    return json_pack("{s:i, s:i, s:i, s:i, s:i, s:I, s:i}",
        "capacity", CART(pObj)->capacity,
        "delay", (int)CART(pObj)->move_delay,
        "strength", (int)CART(pObj)->min_strength,
        "items", CART(pObj)->max_items,
        "weightmult", CART(pObj)->weight_multiplier,
        "flags", (json_int_t)CART(pObj)->flags,
        "vanish", CART(pObj)->vanish_time);
}

static json_t *cart_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_CART(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Capacity:", "command", "typedata/cart/capacity",
        "type", "int", "value", CART(pObj)->capacity);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Delay:", "command", "typedata/cart/delay",
        "type", "int", "value", (int)CART(pObj)->move_delay);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Strength:", "command", "typedata/cart/strength",
        "type", "int", "value", (int)CART(pObj)->min_strength);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Items:", "command", "typedata/cart/items",
        "type", "int", "value", CART(pObj)->max_items);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Weight Mult:", "command", "typedata/cart/weightmult",
        "type", "int", "value", CART(pObj)->weight_multiplier);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Flags:", "command", "typedata/cart/flags",
        "type", "flags", "value", flag_string(cart_flags, (long)CART(pObj)->flags),
        "options", olc_flag_options_json(cart_flags));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Vanish:", "command", "typedata/cart/vanish",
        "type", "int", "value", CART(pObj)->vanish_time);
    json_array_append_new(arr, f);
    return arr;
}

static bool compass_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_COMPASS(pObj)) return false;
    if (strcmp(field, "accuracy") == 0) { COMPASS(pObj)->accuracy = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *compass_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_COMPASS(pObj)) return json_null();
    return json_pack("{s:i}", "accuracy", COMPASS(pObj)->accuracy);
}

static json_t *compass_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_COMPASS(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Accuracy:", "command", "typedata/compass/accuracy",
        "type", "int", "value", COMPASS(pObj)->accuracy);
    json_array_append_new(arr, f);
    return arr;
}

static bool container_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_CONTAINER(pObj)) return false;
    if (strcmp(field, "weight") == 0) { CONTAINER(pObj)->max_weight = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "flags") == 0) { CONTAINER(pObj)->flags = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "items") == 0) { CONTAINER(pObj)->max_items = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "weightmult") == 0) { CONTAINER(pObj)->weight_multiplier = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *container_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_CONTAINER(pObj)) return json_null();
    return json_pack("{s:i, s:I, s:i, s:i}",
        "weight", CONTAINER(pObj)->max_weight,
        "flags", (json_int_t)CONTAINER(pObj)->flags,
        "items", CONTAINER(pObj)->max_items,
        "weightmult", CONTAINER(pObj)->weight_multiplier);
}

static json_t *container_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_CONTAINER(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Weight:", "command", "typedata/container/weight",
        "type", "int", "value", CONTAINER(pObj)->max_weight);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Flags:", "command", "typedata/container/flags",
        "type", "flags", "value", flag_string(container_flags, (long)CONTAINER(pObj)->flags),
        "options", olc_flag_options_json(container_flags));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Items:", "command", "typedata/container/items",
        "type", "int", "value", CONTAINER(pObj)->max_items);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Weight Mult:", "command", "typedata/container/weightmult",
        "type", "int", "value", CONTAINER(pObj)->weight_multiplier);
    json_array_append_new(arr, f);
    return arr;
}

static bool corpse_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_CORPSE(pObj)) return false;
    if (strcmp(field, "type") == 0) { CORPSE(pObj)->corpse_type = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "resurrection") == 0) { CORPSE(pObj)->resurrection = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "animation") == 0) { CORPSE(pObj)->animation = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "parts") == 0) { CORPSE(pObj)->body_parts = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "mobile") == 0) {
        CORPSE(pObj)->mobile_area_uid = (long)json_integer_value(json_object_get(change->new_value, "mobile_auid"));
        CORPSE(pObj)->mobile_vnum = (long)json_integer_value(json_object_get(change->new_value, "mobile_vnum"));
        return true;
    }
    return false;
}
static json_t *corpse_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_CORPSE(pObj)) return json_null();
    return json_pack("{s:i, s:i, s:i, s:I, s:{s:I, s:I}}",
        "type", CORPSE(pObj)->corpse_type,
        "resurrection", CORPSE(pObj)->resurrection,
        "animation", CORPSE(pObj)->animation,
        "parts", (json_int_t)CORPSE(pObj)->body_parts,
        "mobile",
            "mobile_auid", (json_int_t)CORPSE(pObj)->mobile_area_uid,
            "mobile_vnum", (json_int_t)CORPSE(pObj)->mobile_vnum);
}

static json_t *corpse_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_CORPSE(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Type:", "command", "typedata/corpse/type",
        "type", "enum", "value", flag_string(corpse_types, CORPSE(pObj)->corpse_type),
        "options", olc_flag_options_json(corpse_types));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Resurrection:", "command", "typedata/corpse/resurrection",
        "type", "int", "value", CORPSE(pObj)->resurrection);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Animation:", "command", "typedata/corpse/animation",
        "type", "int", "value", CORPSE(pObj)->animation);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Parts:", "command", "typedata/corpse/parts",
        "type", "flags", "value", flag_string(part_flags, (long)CORPSE(pObj)->body_parts),
        "options", olc_flag_options_json(part_flags));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s}",
        "label", "Mobile:", "command", "typedata/corpse/mobile",
        "type", "widevnum",
        "value", widevnum_string(
            CORPSE(pObj)->mobile_area_uid > 0 ? get_area_index(CORPSE(pObj)->mobile_area_uid) : NULL,
            CORPSE(pObj)->mobile_vnum, pObj->area));
    json_array_append_new(arr, f);
    return arr;
}

static bool drink_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_FLUID_CON(pObj)) return false;
    if (strcmp(field, "capacity") == 0) { FLUID_CON(pObj)->capacity = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "amount") == 0) { FLUID_CON(pObj)->amount = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "liquid") == 0) { FLUID_CON(pObj)->liquid = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "poison") == 0) { FLUID_CON(pObj)->poison = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "refill") == 0) { FLUID_CON(pObj)->refill_rate = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *drink_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_FLUID_CON(pObj)) return json_null();
    return json_pack("{s:i, s:i, s:i, s:i, s:i}",
        "capacity", (int)FLUID_CON(pObj)->capacity,
        "amount", (int)FLUID_CON(pObj)->amount,
        "liquid", FLUID_CON(pObj)->liquid,
        "poison", (int)FLUID_CON(pObj)->poison,
        "refill", (int)FLUID_CON(pObj)->refill_rate);
}

static json_t *drink_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_FLUID_CON(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Capacity:", "command", "typedata/drink/capacity",
        "type", "int", "value", (int)FLUID_CON(pObj)->capacity);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Amount:", "command", "typedata/drink/amount",
        "type", "int", "value", (int)FLUID_CON(pObj)->amount);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Liquid:", "command", "typedata/drink/liquid",
        "type", "int", "value", FLUID_CON(pObj)->liquid);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Poison:", "command", "typedata/drink/poison",
        "type", "int", "value", (int)FLUID_CON(pObj)->poison);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Refill:", "command", "typedata/drink/refill",
        "type", "int", "value", (int)FLUID_CON(pObj)->refill_rate);
    json_array_append_new(arr, f);
    return arr;
}

static bool food_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_FOOD(pObj)) return false;
    int val = (int)json_integer_value(change->new_value);
    if (strcmp(field, "hunger") == 0) { FOOD(pObj)->hunger = val; return true; }
    if (strcmp(field, "full") == 0) { FOOD(pObj)->full = val; return true; }
    if (strcmp(field, "poison") == 0) { FOOD(pObj)->poison = val; return true; }
    if (strcmp(field, "timer") == 0) { FOOD(pObj)->timer = val; return true; }
    return false;
}
static json_t *food_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_FOOD(pObj)) return json_null();
    return json_pack("{s:i, s:i, s:i, s:i}",
        "hunger", FOOD(pObj)->hunger, "full", FOOD(pObj)->full,
        "poison", FOOD(pObj)->poison, "timer", FOOD(pObj)->timer);
}

static json_t *food_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_FOOD(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Hunger:", "command", "typedata/food/hunger",
        "type", "int", "value", FOOD(pObj)->hunger);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Full:", "command", "typedata/food/full",
        "type", "int", "value", FOOD(pObj)->full);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Poison:", "command", "typedata/food/poison",
        "type", "int", "value", FOOD(pObj)->poison);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Timer:", "command", "typedata/food/timer",
        "type", "int", "value", FOOD(pObj)->timer);
    json_array_append_new(arr, f);
    return arr;
}

static bool furniture_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_FURNITURE(pObj)) return false;
    if (strcmp(field, "people") == 0) { FURNITURE(pObj)->max_people = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "weight") == 0) { FURNITURE(pObj)->max_weight = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "flags") == 0) { FURNITURE(pObj)->flags = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "heal") == 0) { FURNITURE(pObj)->heal_rate = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "mana") == 0) { FURNITURE(pObj)->mana_rate = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "move") == 0) { FURNITURE(pObj)->move_rate = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *furniture_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_FURNITURE(pObj)) return json_null();
    return json_pack("{s:i, s:i, s:I, s:i, s:i, s:i}",
        "people", FURNITURE(pObj)->max_people,
        "weight", FURNITURE(pObj)->max_weight,
        "flags", (json_int_t)FURNITURE(pObj)->flags,
        "heal", FURNITURE(pObj)->heal_rate,
        "mana", FURNITURE(pObj)->mana_rate,
        "move", FURNITURE(pObj)->move_rate);
}

static json_t *furniture_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_FURNITURE(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "People:", "command", "typedata/furniture/people",
        "type", "int", "value", FURNITURE(pObj)->max_people);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Weight:", "command", "typedata/furniture/weight",
        "type", "int", "value", FURNITURE(pObj)->max_weight);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Flags:", "command", "typedata/furniture/flags",
        "type", "flags", "value", flag_string(furniture_flags, (long)FURNITURE(pObj)->flags),
        "options", olc_flag_options_json(furniture_flags));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Heal:", "command", "typedata/furniture/heal",
        "type", "int", "value", FURNITURE(pObj)->heal_rate);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Mana:", "command", "typedata/furniture/mana",
        "type", "int", "value", FURNITURE(pObj)->mana_rate);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Move:", "command", "typedata/furniture/move",
        "type", "int", "value", FURNITURE(pObj)->move_rate);
    json_array_append_new(arr, f);
    return arr;
}

static bool herb_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_HERB(pObj)) return false;
    if (strcmp(field, "type") == 0) { HERB(pObj)->type = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "healing") == 0) { HERB(pObj)->healing = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "regen") == 0) { HERB(pObj)->regenerative = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "refresh") == 0) { HERB(pObj)->refreshing = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "immunity") == 0) { HERB(pObj)->immunity = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "resistance") == 0) { HERB(pObj)->resistance = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "vulnerability") == 0) { HERB(pObj)->vulnerability = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "spell") == 0) { HERB(pObj)->spell = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *herb_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_HERB(pObj)) return json_null();
    return json_pack("{s:i, s:i, s:i, s:i, s:I, s:I, s:I, s:i}",
        "type", HERB(pObj)->type,
        "healing", HERB(pObj)->healing,
        "regen", HERB(pObj)->regenerative,
        "refresh", HERB(pObj)->refreshing,
        "immunity", (json_int_t)HERB(pObj)->immunity,
        "resistance", (json_int_t)HERB(pObj)->resistance,
        "vulnerability", (json_int_t)HERB(pObj)->vulnerability,
        "spell", HERB(pObj)->spell);
}

static json_t *herb_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_HERB(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Type:", "command", "typedata/herb/type",
        "type", "int", "value", HERB(pObj)->type);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Healing:", "command", "typedata/herb/healing",
        "type", "int", "value", HERB(pObj)->healing);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Regen:", "command", "typedata/herb/regen",
        "type", "int", "value", HERB(pObj)->regenerative);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Refresh:", "command", "typedata/herb/refresh",
        "type", "int", "value", HERB(pObj)->refreshing);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Immunity:", "command", "typedata/herb/immunity",
        "type", "flags", "value", flag_string(imm_flags, (long)HERB(pObj)->immunity),
        "options", olc_flag_options_json(imm_flags));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Resistance:", "command", "typedata/herb/resistance",
        "type", "flags", "value", flag_string(imm_flags, (long)HERB(pObj)->resistance),
        "options", olc_flag_options_json(imm_flags));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Vulnerability:", "command", "typedata/herb/vulnerability",
        "type", "flags", "value", flag_string(imm_flags, (long)HERB(pObj)->vulnerability),
        "options", olc_flag_options_json(imm_flags));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Spell:", "command", "typedata/herb/spell",
        "type", "int", "value", HERB(pObj)->spell);
    json_array_append_new(arr, f);
    return arr;
}

static bool ink_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_INK(pObj)) return false;
    int val = (int)json_integer_value(change->new_value);
    if (strcmp(field, "type1") == 0) { INK(pObj)->types[0] = val; return true; }
    if (strcmp(field, "type2") == 0) { INK(pObj)->types[1] = val; return true; }
    if (strcmp(field, "type3") == 0) { INK(pObj)->types[2] = val; return true; }
    return false;
}
static json_t *ink_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_INK(pObj)) return json_null();
    return json_pack("{s:i, s:i, s:i}",
        "type1", INK(pObj)->types[0], "type2", INK(pObj)->types[1],
        "type3", INK(pObj)->types[2]);
}

static json_t *ink_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_INK(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Type1:", "command", "typedata/ink/type1",
        "type", "int", "value", INK(pObj)->types[0]);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Type2:", "command", "typedata/ink/type2",
        "type", "int", "value", INK(pObj)->types[1]);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Type3:", "command", "typedata/ink/type3",
        "type", "int", "value", INK(pObj)->types[2]);
    json_array_append_new(arr, f);
    return arr;
}

static bool instrument_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_INSTRUMENT(pObj)) return false;
    if (strcmp(field, "type") == 0) { INSTRUMENT(pObj)->type = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "flags") == 0) { INSTRUMENT(pObj)->flags = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "beatsmin") == 0) { INSTRUMENT(pObj)->beats_min = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "beatsmax") == 0) { INSTRUMENT(pObj)->beats_max = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *instrument_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_INSTRUMENT(pObj)) return json_null();
    return json_pack("{s:i, s:I, s:i, s:i}",
        "type", INSTRUMENT(pObj)->type,
        "flags", (json_int_t)INSTRUMENT(pObj)->flags,
        "beatsmin", INSTRUMENT(pObj)->beats_min,
        "beatsmax", INSTRUMENT(pObj)->beats_max);
}

static json_t *instrument_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_INSTRUMENT(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Type:", "command", "typedata/instrument/type",
        "type", "enum", "value", flag_string(instrument_types, INSTRUMENT(pObj)->type),
        "options", olc_flag_options_json(instrument_types));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Flags:", "command", "typedata/instrument/flags",
        "type", "int", "value", (int)(long)INSTRUMENT(pObj)->flags);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Beats Min:", "command", "typedata/instrument/beatsmin",
        "type", "int", "value", INSTRUMENT(pObj)->beats_min);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Beats Max:", "command", "typedata/instrument/beatsmax",
        "type", "int", "value", INSTRUMENT(pObj)->beats_max);
    json_array_append_new(arr, f);
    return arr;
}

static bool jewelry_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_JEWELRY(pObj)) return false;
    if (strcmp(field, "mana") == 0) { JEWELRY(pObj)->max_mana = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *jewelry_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_JEWELRY(pObj)) return json_null();
    return json_pack("{s:i}", "mana", JEWELRY(pObj)->max_mana);
}

static json_t *jewelry_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_JEWELRY(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Mana:", "command", "typedata/jewelry/mana",
        "type", "int", "value", JEWELRY(pObj)->max_mana);
    json_array_append_new(arr, f);
    return arr;
}

static bool light_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_LIGHT(pObj)) return false;
    if (strcmp(field, "duration") == 0) { LIGHT(pObj)->duration = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "flags") == 0) { LIGHT(pObj)->flags = (long)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *light_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_LIGHT(pObj)) return json_null();
    return json_pack("{s:i, s:I}", "duration", LIGHT(pObj)->duration, "flags", (json_int_t)LIGHT(pObj)->flags);
}

static json_t *light_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_LIGHT(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Duration:", "command", "typedata/light/duration",
        "type", "int", "value", LIGHT(pObj)->duration);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Flags:", "command", "typedata/light/flags",
        "type", "flags", "value", flag_string(light_flags, (long)LIGHT(pObj)->flags),
        "options", olc_flag_options_json(light_flags));
    json_array_append_new(arr, f);
    return arr;
}

static bool map_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_MAP(pObj)) return false;
    if (strcmp(field, "wuid") == 0) { MAP(pObj)->wuid = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "x") == 0) { MAP(pObj)->x = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "y") == 0) { MAP(pObj)->y = (long)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *map_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_MAP(pObj)) return json_null();
    return json_pack("{s:I, s:I, s:I}",
        "wuid", (json_int_t)MAP(pObj)->wuid,
        "x", (json_int_t)MAP(pObj)->x,
        "y", (json_int_t)MAP(pObj)->y);
}

static json_t *map_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_MAP(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "WUID:", "command", "typedata/map/wuid",
        "type", "int", "value", (int)(long)MAP(pObj)->wuid);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "X:", "command", "typedata/map/x",
        "type", "int", "value", (int)(long)MAP(pObj)->x);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Y:", "command", "typedata/map/y",
        "type", "int", "value", (int)(long)MAP(pObj)->y);
    json_array_append_new(arr, f);
    return arr;
}

static bool mist_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_MIST(pObj)) return false;
    int val = (int)json_integer_value(change->new_value);
    if (strcmp(field, "objects") == 0) { MIST(pObj)->obscure_objs = val; return true; }
    if (strcmp(field, "characters") == 0) { MIST(pObj)->obscure_mobs = val; return true; }
    if (strcmp(field, "room") == 0) { MIST(pObj)->obscure_room = val; return true; }
    return false;
}
static json_t *mist_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_MIST(pObj)) return json_null();
    return json_pack("{s:i, s:i, s:i}",
        "objects", MIST(pObj)->obscure_objs,
        "characters", MIST(pObj)->obscure_mobs,
        "room", MIST(pObj)->obscure_room);
}

static json_t *mist_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_MIST(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Objects:", "command", "typedata/mist/objects",
        "type", "int", "value", MIST(pObj)->obscure_objs);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Characters:", "command", "typedata/mist/characters",
        "type", "int", "value", MIST(pObj)->obscure_mobs);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Room:", "command", "typedata/mist/room",
        "type", "int", "value", MIST(pObj)->obscure_room);
    json_array_append_new(arr, f);
    return arr;
}

static bool money_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_MONEY(pObj)) return false;
    int val = (int)json_integer_value(change->new_value);
    if (strcmp(field, "silver") == 0) { MONEY(pObj)->silver = val; return true; }
    if (strcmp(field, "gold") == 0) { MONEY(pObj)->gold = val; return true; }
    return false;
}
static json_t *money_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_MONEY(pObj)) return json_null();
    return json_pack("{s:i, s:i}", "silver", MONEY(pObj)->silver, "gold", MONEY(pObj)->gold);
}

static json_t *money_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_MONEY(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Silver:", "command", "typedata/money/silver",
        "type", "int", "value", MONEY(pObj)->silver);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Gold:", "command", "typedata/money/gold",
        "type", "int", "value", MONEY(pObj)->gold);
    json_array_append_new(arr, f);
    return arr;
}

static bool page_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_PAGE(pObj)) return false;
    if (strcmp(field, "number") == 0) { PAGE(pObj)->page_no = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "title") == 0) {
        const char *s = json_string_value(change->new_value);
        free_string(PAGE(pObj)->title);
        PAGE(pObj)->title = str_dup(s ? s : "");
        return true;
    }
    return false;
}
static json_t *page_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_PAGE(pObj)) return json_null();
    return json_pack("{s:i, s:s}", "number", PAGE(pObj)->page_no,
        "title", PAGE(pObj)->title ? PAGE(pObj)->title : "");
}

static json_t *page_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_PAGE(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Number:", "command", "typedata/page/number",
        "type", "int", "value", PAGE(pObj)->page_no);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s}",
        "label", "Title:", "command", "typedata/page/title",
        "type", "string", "value", PAGE(pObj)->title ? PAGE(pObj)->title : "");
    json_array_append_new(arr, f);
    return arr;
}

static bool portal_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_PORTAL(pObj)) return false;
    if (strcmp(field, "charges") == 0) { PORTAL(pObj)->charges = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "exit") == 0) { PORTAL(pObj)->exit = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "flags") == 0) { PORTAL(pObj)->flags = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "param0") == 0) { PORTAL(pObj)->params[0] = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "param1") == 0) { PORTAL(pObj)->params[1] = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "param2") == 0) { PORTAL(pObj)->params[2] = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "param3") == 0) { PORTAL(pObj)->params[3] = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "param4") == 0) { PORTAL(pObj)->params[4] = (long)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *portal_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_PORTAL(pObj)) return json_null();
    return json_pack("{s:i, s:I, s:I, s:I, s:I, s:I, s:I, s:I}",
        "charges", PORTAL(pObj)->charges,
        "exit", (json_int_t)PORTAL(pObj)->exit,
        "flags", (json_int_t)PORTAL(pObj)->flags,
        "param0", (json_int_t)PORTAL(pObj)->params[0],
        "param1", (json_int_t)PORTAL(pObj)->params[1],
        "param2", (json_int_t)PORTAL(pObj)->params[2],
        "param3", (json_int_t)PORTAL(pObj)->params[3],
        "param4", (json_int_t)PORTAL(pObj)->params[4]);
}

static json_t *portal_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_PORTAL(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Charges:", "command", "typedata/portal/charges",
        "type", "int", "value", PORTAL(pObj)->charges);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Exit:", "command", "typedata/portal/exit",
        "type", "flags", "value", flag_string(portal_exit_flags, (long)PORTAL(pObj)->exit),
        "options", olc_flag_options_json(portal_exit_flags));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Flags:", "command", "typedata/portal/flags",
        "type", "flags", "value", flag_string(portal_flags, (long)PORTAL(pObj)->flags),
        "options", olc_flag_options_json(portal_flags));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Param0:", "command", "typedata/portal/param0",
        "type", "int", "value", (int)PORTAL(pObj)->params[0]);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Param1:", "command", "typedata/portal/param1",
        "type", "int", "value", (int)PORTAL(pObj)->params[1]);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Param2:", "command", "typedata/portal/param2",
        "type", "int", "value", (int)PORTAL(pObj)->params[2]);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Param3:", "command", "typedata/portal/param3",
        "type", "int", "value", (int)PORTAL(pObj)->params[3]);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Param4:", "command", "typedata/portal/param4",
        "type", "int", "value", (int)PORTAL(pObj)->params[4]);
    json_array_append_new(arr, f);
    return arr;
}

static bool scroll_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_SCROLL(pObj)) return false;
    if (strcmp(field, "mana") == 0) { SCROLL(pObj)->max_mana = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "flags") == 0) { SCROLL(pObj)->flags = (long)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *scroll_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_SCROLL(pObj)) return json_null();
    return json_pack("{s:i, s:I}", "mana", SCROLL(pObj)->max_mana, "flags", (json_int_t)SCROLL(pObj)->flags);
}

static json_t *scroll_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_SCROLL(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Mana:", "command", "typedata/scroll/mana",
        "type", "int", "value", SCROLL(pObj)->max_mana);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Flags:", "command", "typedata/scroll/flags",
        "type", "int", "value", (int)(long)SCROLL(pObj)->flags);
    json_array_append_new(arr, f);
    return arr;
}

static bool seed_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_SEED(pObj)) return false;
    if (strcmp(field, "time") == 0) { SEED(pObj)->growth_time = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "object") == 0) {
        long auid = (long)json_integer_value(json_object_get(change->new_value, "auid"));
        long vnum = (long)json_integer_value(json_object_get(change->new_value, "vnum"));
        SEED(pObj)->object_vnum = vnum;
        SEED(pObj)->object_area_uid = auid;
        return true;
    }
    return false;
}
static json_t *seed_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_SEED(pObj)) return json_null();
    return json_pack("{s:i, s:{s:I, s:I}}",
        "time", SEED(pObj)->growth_time,
        "object",
            "auid", (json_int_t)SEED(pObj)->object_area_uid,
            "vnum", (json_int_t)SEED(pObj)->object_vnum);
}

static json_t *seed_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_SEED(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Time:", "command", "typedata/seed/time",
        "type", "int", "value", SEED(pObj)->growth_time);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s}",
        "label", "Object:", "command", "typedata/seed/object",
        "type", "widevnum",
        "value", widevnum_string(
            SEED(pObj)->object_area_uid > 0 ? get_area_index(SEED(pObj)->object_area_uid) : NULL,
            SEED(pObj)->object_vnum, pObj->area));
    json_array_append_new(arr, f);
    return arr;
}

static bool sextant_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_SEXTANT(pObj)) return false;
    if (strcmp(field, "accuracy") == 0) { SEXTANT(pObj)->accuracy = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *sextant_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_SEXTANT(pObj)) return json_null();
    return json_pack("{s:i}", "accuracy", SEXTANT(pObj)->accuracy);
}

static json_t *sextant_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_SEXTANT(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Accuracy:", "command", "typedata/sextant/accuracy",
        "type", "int", "value", SEXTANT(pObj)->accuracy);
    json_array_append_new(arr, f);
    return arr;
}

static bool ship_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_SHIP_TYPE(pObj)) return false;
    if (strcmp(field, "weight") == 0) { SHIP_TYPE(pObj)->weight = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "delay") == 0) { SHIP_TYPE(pObj)->move_delay = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "mincrew") == 0) { SHIP_TYPE(pObj)->min_crew = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "capacity") == 0) { SHIP_TYPE(pObj)->capacity = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "maxcrew") == 0) { SHIP_TYPE(pObj)->max_crew = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "room") == 0) {
        long auid = (long)json_integer_value(json_object_get(change->new_value, "auid"));
        long vnum = (long)json_integer_value(json_object_get(change->new_value, "vnum"));
        SHIP_TYPE(pObj)->first_room = vnum;
        SHIP_TYPE(pObj)->first_room_area_uid = auid;
        return true;
    }
    if (strcmp(field, "hitpoints") == 0) { SHIP_TYPE(pObj)->hit_points = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "guns") == 0) { SHIP_TYPE(pObj)->max_guns = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *ship_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_SHIP_TYPE(pObj)) return json_null();
    return json_pack("{s:i, s:i, s:i, s:i, s:i, s:{s:I, s:I}, s:i, s:i}",
        "weight", SHIP_TYPE(pObj)->weight,
        "delay", SHIP_TYPE(pObj)->move_delay,
        "mincrew", SHIP_TYPE(pObj)->min_crew,
        "capacity", SHIP_TYPE(pObj)->capacity,
        "maxcrew", SHIP_TYPE(pObj)->max_crew,
        "room",
            "auid", (json_int_t)SHIP_TYPE(pObj)->first_room_area_uid,
            "vnum", (json_int_t)SHIP_TYPE(pObj)->first_room,
        "hitpoints", SHIP_TYPE(pObj)->hit_points,
        "guns", SHIP_TYPE(pObj)->max_guns);
}

static json_t *ship_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_SHIP_TYPE(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Weight:", "command", "typedata/ship/weight",
        "type", "int", "value", SHIP_TYPE(pObj)->weight);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Delay:", "command", "typedata/ship/delay",
        "type", "int", "value", SHIP_TYPE(pObj)->move_delay);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Min Crew:", "command", "typedata/ship/mincrew",
        "type", "int", "value", SHIP_TYPE(pObj)->min_crew);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Capacity:", "command", "typedata/ship/capacity",
        "type", "int", "value", SHIP_TYPE(pObj)->capacity);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Max Crew:", "command", "typedata/ship/maxcrew",
        "type", "int", "value", SHIP_TYPE(pObj)->max_crew);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s}",
        "label", "Room:", "command", "typedata/ship/room",
        "type", "widevnum",
        "value", widevnum_string(
            SHIP_TYPE(pObj)->first_room_area_uid > 0 ? get_area_index(SHIP_TYPE(pObj)->first_room_area_uid) : NULL,
            SHIP_TYPE(pObj)->first_room, pObj->area));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Hit Points:", "command", "typedata/ship/hitpoints",
        "type", "int", "value", SHIP_TYPE(pObj)->hit_points);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Guns:", "command", "typedata/ship/guns",
        "type", "int", "value", SHIP_TYPE(pObj)->max_guns);
    json_array_append_new(arr, f);
    return arr;
}

static bool shipmodule_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_SHIP_MODULE(pObj)) return false;
    SHIP_MODULE_DATA *d = SHIP_MODULE_TYPE(pObj);
    if (strcmp(field, "type") == 0) { d->type = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "size") == 0) { d->size = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "weight") == 0) { d->weight = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "domain") == 0) { d->domain_flags = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "hitbonus") == 0) { d->hit_bonus = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "armorbonus") == 0) { d->armor_bonus = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "speedbonus") == 0) { d->speed_bonus = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "turningbonus") == 0) { d->turning_bonus = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "cargoweight") == 0) { d->cargo_weight_bonus = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "cargocapacity") == 0) { d->cargo_capacity_bonus = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "crewbonus") == 0) { d->crew_bonus = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "damage") == 0) { d->damage = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "range") == 0) { d->range = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "reload") == 0) { d->reload_time = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "damagetype") == 0) { d->damage_type = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "weapflags") == 0) { d->weapon_flags = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "operators") == 0) { d->operators = (int16_t)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "gunning") == 0) { d->req_gunning = (int16_t)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "mechanics") == 0) { d->req_mechanics = (int16_t)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "scouting") == 0) { d->req_scouting = (int16_t)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "navigation") == 0) { d->req_navigation = (int16_t)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "oarring") == 0) { d->req_oarring = (int16_t)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "leadership") == 0) { d->req_leadership = (int16_t)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "ammo") == 0) {
        long auid = (long)json_integer_value(json_object_get(change->new_value, "auid"));
        long vnum = (long)json_integer_value(json_object_get(change->new_value, "vnum"));
        d->ammo_ref.load.vnum = vnum;
        d->ammo_ref.load.auid = auid;
        if (vnum > 0) {
            AREA_DATA *area = auid > 0 ? get_area_index(auid) : NULL;
            d->ammo = area ? get_obj_index(area, vnum) : get_obj_index_global(vnum);
        } else {
            d->ammo = NULL;
        }
        return true;
    }
    if (strcmp(field, "ammoshot") == 0) { d->ammo_per_shot = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "flags") == 0) { d->flags = (long)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *shipmodule_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_SHIP_MODULE(pObj)) return json_null();
    SHIP_MODULE_DATA *d = SHIP_MODULE_TYPE(pObj);
    return json_pack("{s:i, s:i, s:i, s:I, s:i, s:i, s:i, s:i, s:i, s:i, s:i,"
                     " s:i, s:i, s:i, s:i, s:I, s:i, s:i, s:i, s:i, s:i, s:i, s:i,"
                     " s:{s:I, s:I}, s:i, s:I}",
        "type", d->type,
        "size", d->size,
        "weight", d->weight,
        "domain", (json_int_t)d->domain_flags,
        "hitbonus", d->hit_bonus,
        "armorbonus", d->armor_bonus,
        "speedbonus", d->speed_bonus,
        "turningbonus", d->turning_bonus,
        "cargoweight", d->cargo_weight_bonus,
        "cargocapacity", d->cargo_capacity_bonus,
        "crewbonus", d->crew_bonus,
        "damage", d->damage,
        "range", d->range,
        "reload", d->reload_time,
        "damagetype", d->damage_type,
        "weapflags", (json_int_t)d->weapon_flags,
        "operators", (int)d->operators,
        "gunning", (int)d->req_gunning,
        "mechanics", (int)d->req_mechanics,
        "scouting", (int)d->req_scouting,
        "navigation", (int)d->req_navigation,
        "oarring", (int)d->req_oarring,
        "leadership", (int)d->req_leadership,
        "ammo",
            "auid", (json_int_t)d->ammo_ref.load.auid,
            "vnum", (json_int_t)d->ammo_ref.load.vnum,
        "ammoshot", d->ammo_per_shot,
        "flags", (json_int_t)d->flags);
}

static json_t *shipmodule_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_SHIP_MODULE(pObj)) return json_array();
    SHIP_MODULE_DATA *d = SHIP_MODULE_TYPE(pObj);
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Type:", "command", "typedata/shipmodule/type",
        "type", "int", "value", d->type);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Size:", "command", "typedata/shipmodule/size",
        "type", "int", "value", d->size);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Weight:", "command", "typedata/shipmodule/weight",
        "type", "int", "value", d->weight);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Domain:", "command", "typedata/shipmodule/domain",
        "type", "int", "value", (int)(long)d->domain_flags);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Hit Bonus:", "command", "typedata/shipmodule/hitbonus",
        "type", "int", "value", d->hit_bonus);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Armor Bonus:", "command", "typedata/shipmodule/armorbonus",
        "type", "int", "value", d->armor_bonus);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Speed Bonus:", "command", "typedata/shipmodule/speedbonus",
        "type", "int", "value", d->speed_bonus);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Turning Bonus:", "command", "typedata/shipmodule/turningbonus",
        "type", "int", "value", d->turning_bonus);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Cargo Weight:", "command", "typedata/shipmodule/cargoweight",
        "type", "int", "value", d->cargo_weight_bonus);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Cargo Capacity:", "command", "typedata/shipmodule/cargocapacity",
        "type", "int", "value", d->cargo_capacity_bonus);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Crew Bonus:", "command", "typedata/shipmodule/crewbonus",
        "type", "int", "value", d->crew_bonus);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Damage:", "command", "typedata/shipmodule/damage",
        "type", "int", "value", d->damage);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Range:", "command", "typedata/shipmodule/range",
        "type", "int", "value", d->range);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Reload:", "command", "typedata/shipmodule/reload",
        "type", "int", "value", d->reload_time);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Damage Type:", "command", "typedata/shipmodule/damagetype",
        "type", "int", "value", d->damage_type);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Weap Flags:", "command", "typedata/shipmodule/weapflags",
        "type", "int", "value", (int)(long)d->weapon_flags);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Operators:", "command", "typedata/shipmodule/operators",
        "type", "int", "value", (int)d->operators);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Gunning:", "command", "typedata/shipmodule/gunning",
        "type", "int", "value", (int)d->req_gunning);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Mechanics:", "command", "typedata/shipmodule/mechanics",
        "type", "int", "value", (int)d->req_mechanics);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Scouting:", "command", "typedata/shipmodule/scouting",
        "type", "int", "value", (int)d->req_scouting);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Navigation:", "command", "typedata/shipmodule/navigation",
        "type", "int", "value", (int)d->req_navigation);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Oarring:", "command", "typedata/shipmodule/oarring",
        "type", "int", "value", (int)d->req_oarring);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Leadership:", "command", "typedata/shipmodule/leadership",
        "type", "int", "value", (int)d->req_leadership);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s}",
        "label", "Ammo:", "command", "typedata/shipmodule/ammo",
        "type", "widevnum",
        "value", widevnum_string(
            d->ammo_ref.load.auid > 0 ? get_area_index(d->ammo_ref.load.auid) : NULL,
            d->ammo_ref.load.vnum, pObj->area));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Ammo/Shot:", "command", "typedata/shipmodule/ammoshot",
        "type", "int", "value", d->ammo_per_shot);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Flags:", "command", "typedata/shipmodule/flags",
        "type", "int", "value", (int)(long)d->flags);
    json_array_append_new(arr, f);
    return arr;
}

static bool tattoo_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_TATTOO(pObj)) return false;
    int val = (int)json_integer_value(change->new_value);
    if (strcmp(field, "touches") == 0) { TATTOO(pObj)->touches = val; return true; }
    if (strcmp(field, "fading") == 0) { TATTOO(pObj)->fading_chance = val; return true; }
    if (strcmp(field, "faderate") == 0) { TATTOO(pObj)->fading_rate = val; return true; }
    return false;
}
static json_t *tattoo_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_TATTOO(pObj)) return json_null();
    return json_pack("{s:i, s:i, s:i}",
        "touches", TATTOO(pObj)->touches,
        "fading", TATTOO(pObj)->fading_chance,
        "faderate", TATTOO(pObj)->fading_rate);
}

static json_t *tattoo_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_TATTOO(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Touches:", "command", "typedata/tattoo/touches",
        "type", "int", "value", TATTOO(pObj)->touches);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Fading:", "command", "typedata/tattoo/fading",
        "type", "int", "value", TATTOO(pObj)->fading_chance);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Fade Rate:", "command", "typedata/tattoo/faderate",
        "type", "int", "value", TATTOO(pObj)->fading_rate);
    json_array_append_new(arr, f);
    return arr;
}

static bool telescope_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_TELESCOPE(pObj)) return false;
    int val = (int)json_integer_value(change->new_value);
    if (strcmp(field, "distance") == 0) { TELESCOPE(pObj)->distance = (int16_t)val; return true; }
    if (strcmp(field, "mindist") == 0) { TELESCOPE(pObj)->min_distance = (int16_t)val; return true; }
    if (strcmp(field, "maxdist") == 0) { TELESCOPE(pObj)->max_distance = (int16_t)val; return true; }
    if (strcmp(field, "bonus") == 0) { TELESCOPE(pObj)->bonus_view = (int16_t)val; return true; }
    if (strcmp(field, "heading") == 0) { TELESCOPE(pObj)->heading = (int16_t)val; return true; }
    return false;
}
static json_t *telescope_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_TELESCOPE(pObj)) return json_null();
    return json_pack("{s:i, s:i, s:i, s:i, s:i}",
        "distance", (int)TELESCOPE(pObj)->distance,
        "mindist", (int)TELESCOPE(pObj)->min_distance,
        "maxdist", (int)TELESCOPE(pObj)->max_distance,
        "bonus", (int)TELESCOPE(pObj)->bonus_view,
        "heading", (int)TELESCOPE(pObj)->heading);
}

static json_t *telescope_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_TELESCOPE(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Distance:", "command", "typedata/telescope/distance",
        "type", "int", "value", (int)TELESCOPE(pObj)->distance);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Min Dist:", "command", "typedata/telescope/mindist",
        "type", "int", "value", (int)TELESCOPE(pObj)->min_distance);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Max Dist:", "command", "typedata/telescope/maxdist",
        "type", "int", "value", (int)TELESCOPE(pObj)->max_distance);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Bonus:", "command", "typedata/telescope/bonus",
        "type", "int", "value", (int)TELESCOPE(pObj)->bonus_view);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Heading:", "command", "typedata/telescope/heading",
        "type", "int", "value", (int)TELESCOPE(pObj)->heading);
    json_array_append_new(arr, f);
    return arr;
}

static bool tool_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_TOOL(pObj)) return false;
    int val = (int)json_integer_value(change->new_value);
    if (strcmp(field, "type") == 0) { TOOL(pObj)->type = val; return true; }
    if (strcmp(field, "tier") == 0) { TOOL(pObj)->tier = val; return true; }
    return false;
}
static json_t *tool_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_TOOL(pObj)) return json_null();
    return json_pack("{s:i, s:i}", "type", TOOL(pObj)->type, "tier", TOOL(pObj)->tier);
}

static json_t *tool_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_TOOL(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Type:", "command", "typedata/tool/type",
        "type", "enum", "value", flag_string(tool_types, TOOL(pObj)->type),
        "options", olc_flag_options_json(tool_types));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Tier:", "command", "typedata/tool/tier",
        "type", "int", "value", TOOL(pObj)->tier);
    json_array_append_new(arr, f);
    return arr;
}

static bool trade_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_TRADE(pObj)) return false;
    if (strcmp(field, "type") == 0) { TRADE(pObj)->trade_type = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *trade_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_TRADE(pObj)) return json_null();
    return json_pack("{s:i}", "type", TRADE(pObj)->trade_type);
}

static json_t *trade_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_TRADE(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Type:", "command", "typedata/trade/type",
        "type", "int", "value", TRADE(pObj)->trade_type);
    json_array_append_new(arr, f);
    return arr;
}

static bool wand_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_WAND(pObj)) return false;
    if (strcmp(field, "mana") == 0) { WAND(pObj)->max_mana = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "charges") == 0) { WAND(pObj)->charges = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "maxcharges") == 0) { WAND(pObj)->max_charges = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "cooldown") == 0) { WAND(pObj)->cooldown = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "recharge") == 0) { WAND(pObj)->recharge_time = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *wand_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_WAND(pObj)) return json_null();
    return json_pack("{s:i, s:i, s:i, s:i, s:i}",
        "mana", WAND(pObj)->max_mana,
        "charges", WAND(pObj)->charges,
        "maxcharges", WAND(pObj)->max_charges,
        "cooldown", WAND(pObj)->cooldown,
        "recharge", WAND(pObj)->recharge_time);
}

static json_t *wand_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_WAND(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Mana:", "command", "typedata/wand/mana",
        "type", "int", "value", WAND(pObj)->max_mana);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Charges:", "command", "typedata/wand/charges",
        "type", "int", "value", WAND(pObj)->charges);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Max Charges:", "command", "typedata/wand/maxcharges",
        "type", "int", "value", WAND(pObj)->max_charges);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Cooldown:", "command", "typedata/wand/cooldown",
        "type", "int", "value", WAND(pObj)->cooldown);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Recharge:", "command", "typedata/wand/recharge",
        "type", "int", "value", WAND(pObj)->recharge_time);
    json_array_append_new(arr, f);
    return arr;
}

static bool weapon_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_WEAPON(pObj)) return false;
    if (strcmp(field, "class") == 0) { WEAPON(pObj)->weapon_class = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "dice") == 0) {
        WEAPON(pObj)->damage.number = (int)json_integer_value(json_object_get(change->new_value, "number"));
        WEAPON(pObj)->damage.size = (int)json_integer_value(json_object_get(change->new_value, "size"));
        WEAPON(pObj)->damage.bonus = (int)json_integer_value(json_object_get(change->new_value, "bonus"));
        set_weapon_dice(pObj);
        return true;
    }
    if (strcmp(field, "damtype") == 0) { WEAPON(pObj)->damage_type = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "flags") == 0) { WEAPON(pObj)->flags = (long)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "range") == 0) { WEAPON(pObj)->range = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "mana") == 0) { WEAPON(pObj)->max_mana = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "charges") == 0) { WEAPON(pObj)->charges = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "maxcharges") == 0) { WEAPON(pObj)->max_charges = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "cooldown") == 0) { WEAPON(pObj)->cooldown = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "recharge") == 0) { WEAPON(pObj)->recharge_time = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *weapon_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_WEAPON(pObj)) return json_null();
    return json_pack("{s:i, s:{s:i, s:i, s:i}, s:i, s:I, s:i, s:i, s:i, s:i, s:i, s:i}",
        "class", (int)WEAPON(pObj)->weapon_class,
        "dice",
            "number", WEAPON(pObj)->damage.number,
            "size", WEAPON(pObj)->damage.size,
            "bonus", WEAPON(pObj)->damage.bonus,
        "damtype", WEAPON(pObj)->damage_type,
        "flags", (json_int_t)WEAPON(pObj)->flags,
        "range", WEAPON(pObj)->range,
        "mana", WEAPON(pObj)->max_mana,
        "charges", WEAPON(pObj)->charges,
        "maxcharges", WEAPON(pObj)->max_charges,
        "cooldown", WEAPON(pObj)->cooldown,
        "recharge", WEAPON(pObj)->recharge_time);
}

static json_t *weapon_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_WEAPON(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Class:", "command", "typedata/weapon/class",
        "type", "enum", "value", flag_string(weapon_class, WEAPON(pObj)->weapon_class),
        "options", olc_flag_options_json(weapon_class));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s}",
        "label", "Dice:", "command", "typedata/weapon/dice",
        "type", "dice",
        "value", formatf("%dd%d+%d", WEAPON(pObj)->damage.number,
            WEAPON(pObj)->damage.size, WEAPON(pObj)->damage.bonus));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Dam Type:", "command", "typedata/weapon/damtype",
        "type", "int", "value", WEAPON(pObj)->damage_type);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Flags:", "command", "typedata/weapon/flags",
        "type", "flags", "value", flag_string(weapon_type2, (long)WEAPON(pObj)->flags),
        "options", olc_flag_options_json(weapon_type2));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Range:", "command", "typedata/weapon/range",
        "type", "int", "value", WEAPON(pObj)->range);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Mana:", "command", "typedata/weapon/mana",
        "type", "int", "value", WEAPON(pObj)->max_mana);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Charges:", "command", "typedata/weapon/charges",
        "type", "int", "value", WEAPON(pObj)->charges);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Max Charges:", "command", "typedata/weapon/maxcharges",
        "type", "int", "value", WEAPON(pObj)->max_charges);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Cooldown:", "command", "typedata/weapon/cooldown",
        "type", "int", "value", WEAPON(pObj)->cooldown);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Recharge:", "command", "typedata/weapon/recharge",
        "type", "int", "value", WEAPON(pObj)->recharge_time);
    json_array_append_new(arr, f);
    return arr;
}

static bool weaponcon_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change)
{
    if (!IS_WEAPON_CON(pObj)) return false;
    if (strcmp(field, "weight") == 0) { WEAPON_CON(pObj)->max_weight = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "weapontype") == 0) { WEAPON_CON(pObj)->weapon_type = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "items") == 0) { WEAPON_CON(pObj)->max_items = (int)json_integer_value(change->new_value); return true; }
    if (strcmp(field, "weightmult") == 0) { WEAPON_CON(pObj)->weight_multiplier = (int)json_integer_value(change->new_value); return true; }
    return false;
}
static json_t *weaponcon_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_WEAPON_CON(pObj)) return json_null();
    return json_pack("{s:i, s:i, s:i, s:i}",
        "weight", WEAPON_CON(pObj)->max_weight,
        "weapontype", WEAPON_CON(pObj)->weapon_type,
        "items", WEAPON_CON(pObj)->max_items,
        "weightmult", WEAPON_CON(pObj)->weight_multiplier);
}

static json_t *weaponcon_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_WEAPON_CON(pObj)) return json_array();
    json_t *arr = json_array();
    json_t *f;
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Weight:", "command", "typedata/weaponcon/weight",
        "type", "int", "value", WEAPON_CON(pObj)->max_weight);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Weapon Type:", "command", "typedata/weaponcon/weapontype",
        "type", "enum", "value", flag_string(weapon_class, WEAPON_CON(pObj)->weapon_type),
        "options", olc_flag_options_json(weapon_class));
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Items:", "command", "typedata/weaponcon/items",
        "type", "int", "value", WEAPON_CON(pObj)->max_items);
    json_array_append_new(arr, f);
    f = json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Weight Mult:", "command", "typedata/weaponcon/weightmult",
        "type", "int", "value", WEAPON_CON(pObj)->weight_multiplier);
    json_array_append_new(arr, f);
    return arr;
}

static const oedit_type_dispatch_t type_dispatch[] = {
    { "armor",       armor_apply_field,       armor_serialize,       armor_schema },
    { "bodypart",    bodypart_apply_field,    bodypart_serialize,    bodypart_schema },
    { "book",        book_apply_field,        book_serialize,        book_schema },
    { "cart",        cart_apply_field,        cart_serialize,        cart_schema },
    { "compass",     compass_apply_field,     compass_serialize,     compass_schema },
    { "container",   container_apply_field,   container_serialize,   container_schema },
    { "corpse",      corpse_apply_field,      corpse_serialize,      corpse_schema },
    { "drink",       drink_apply_field,       drink_serialize,       drink_schema },
    { "food",        food_apply_field,        food_serialize,        food_schema },
    { "furniture",   furniture_apply_field,   furniture_serialize,   furniture_schema },
    { "herb",        herb_apply_field,        herb_serialize,        herb_schema },
    { "ink",         ink_apply_field,         ink_serialize,         ink_schema },
    { "instrument",  instrument_apply_field,  instrument_serialize,  instrument_schema },
    { "jewelry",     jewelry_apply_field,     jewelry_serialize,     jewelry_schema },
    { "light",       light_apply_field,       light_serialize,       light_schema },
    { "map",         map_apply_field,         map_serialize,         map_schema },
    { "mist",        mist_apply_field,        mist_serialize,        mist_schema },
    { "money",       money_apply_field,       money_serialize,       money_schema },
    { "page",        page_apply_field,        page_serialize,        page_schema },
    { "portal",      portal_apply_field,      portal_serialize,      portal_schema },
    { "scroll",      scroll_apply_field,      scroll_serialize,      scroll_schema },
    { "seed",        seed_apply_field,        seed_serialize,        seed_schema },
    { "sextant",     sextant_apply_field,     sextant_serialize,     sextant_schema },
    { "ship",        ship_apply_field,        ship_serialize,        ship_schema },
    { "shipmodule",  shipmodule_apply_field,  shipmodule_serialize,  shipmodule_schema },
    { "tattoo",      tattoo_apply_field,      tattoo_serialize,      tattoo_schema },
    { "telescope",   telescope_apply_field,   telescope_serialize,   telescope_schema },
    { "tool",        tool_apply_field,        tool_serialize,        tool_schema },
    { "trade",       trade_apply_field,       trade_serialize,       trade_schema },
    { "wand",        wand_apply_field,        wand_serialize,        wand_schema },
    { "weapon",      weapon_apply_field,      weapon_serialize,      weapon_schema },
    { "weaponcon",   weaponcon_apply_field,   weaponcon_serialize,   weaponcon_schema },
    { NULL, NULL, NULL, NULL }
};

bool oedit_apply_typedata(void *entity, olc_pending_change_t *change)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    const char *path = change->field_path;

    /* Skip "typedata/" prefix */
    const char *rest = path + 9; /* strlen("typedata/") */

    /* Check for addtype/removetype: +type or -type */
    if (rest[0] == '+') {
        const char *type_name = rest + 1;
        int type_flag = flag_value(type_flags, type_name);
        if (type_flag == NO_FLAG) return false;
        obj_index_alloc_type_data(pObj, type_flag);
        return true;
    }
    if (rest[0] == '-') {
        const char *type_name = rest + 1;
        int type_flag = flag_value(type_flags, type_name);
        if (type_flag == NO_FLAG) return false;
        obj_index_remove_type(pObj, type_flag);
        return true;
    }

    /* Regular type field: rest = "weapon/class" */
    char type_buf[MIL];
    const char *slash = strchr(rest, '/');
    if (!slash) return false;

    size_t tlen = slash - rest;
    if (tlen >= sizeof(type_buf)) return false;
    memcpy(type_buf, rest, tlen);
    type_buf[tlen] = '\0';

    const char *field_name = slash + 1;

    /* Find type handler */
    for (int i = 0; type_dispatch[i].type_name; i++) {
        if (strcmp(type_dispatch[i].type_name, type_buf) == 0) {
            return type_dispatch[i].apply_fn(pObj, field_name, change);
        }
    }

    return false;
}

json_t *oedit_serialize_typedata(void *entity, const char *field_path)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    const char *rest = field_path + 9;

    char type_buf[MIL];
    const char *slash = strchr(rest, '/');
    if (!slash) {
        strlcpy(type_buf, rest, sizeof(type_buf));
    } else {
        size_t tlen = slash - rest;
        if (tlen >= sizeof(type_buf)) return json_null();
        memcpy(type_buf, rest, tlen);
        type_buf[tlen] = '\0';
    }

    for (int i = 0; type_dispatch[i].type_name; i++) {
        if (strcmp(type_dispatch[i].type_name, type_buf) == 0
            && type_dispatch[i].serialize_fn) {
            return type_dispatch[i].serialize_fn(pObj);
        }
    }

    return json_null();
}

json_t *oedit_type_schema(const OBJ_INDEX_DATA *pObj)
{
    json_t *fields = json_array();
    if (!pObj) return fields;

    for (int i = 0; type_dispatch[i].type_name; i++) {
        if (!type_dispatch[i].schema_fn) continue;
        json_t *type_fields = type_dispatch[i].schema_fn(pObj);
        if (!type_fields || json_is_null(type_fields)) {
            json_decref(type_fields);
            continue;
        }
        if (json_array_size(type_fields) == 0) {
            json_decref(type_fields);
            continue;
        }
        /* Add section marker before this type's fields */
        json_t *section = json_pack("{s:s, s:s}",
            "type", "section",
            "label", type_dispatch[i].type_name);
        json_array_append_new(fields, section);
        /* Append all field descriptors from this type */
        for (size_t j = 0; j < json_array_size(type_fields); j++) {
            json_array_append(fields, json_array_get(type_fields, j));
        }
        json_decref(type_fields);
    }

    return fields;
}

/* ============================================================================
 *  ARMOR
 * ============================================================================ */
OEDIT(oedit_armor)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_ARMOR(pObj)) {
        send_to_char("Object lacks the armour type. Use '{Waddtype armour{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "pierce")) {
            if (argument[0] == '\0') { send_to_char("Syntax: armor pierce <value>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(ARMOR(pObj)->protection[0]);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/armor/pierce", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/armor/pierce", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x AC pierce set.\n\r", ch);
            } else {
                ARMOR(pObj)->protection[0] = val;
                send_to_char("AC pierce set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "bash")) {
            if (argument[0] == '\0') { send_to_char("Syntax: armor bash <value>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(ARMOR(pObj)->protection[1]);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/armor/bash", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/armor/bash", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x AC bash set.\n\r", ch);
            } else {
                ARMOR(pObj)->protection[1] = val;
                send_to_char("AC bash set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "slash")) {
            if (argument[0] == '\0') { send_to_char("Syntax: armor slash <value>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(ARMOR(pObj)->protection[2]);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/armor/slash", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/armor/slash", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x AC slash set.\n\r", ch);
            } else {
                ARMOR(pObj)->protection[2] = val;
                send_to_char("AC slash set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "exotic")) {
            if (argument[0] == '\0') { send_to_char("Syntax: armor exotic <value>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(ARMOR(pObj)->protection[3]);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/armor/exotic", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/armor/exotic", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x AC exotic set.\n\r", ch);
            } else {
                ARMOR(pObj)->protection[3] = val;
                send_to_char("AC exotic set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "strength")) {
            if (argument[0] == '\0') { send_to_char("Syntax: armor strength <none|light|medium|strong|heavy>\n\r", ch); return false; }
            int val = get_armour_strength(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(ARMOR(pObj)->armor_strength);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/armor/strength", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/armor/strength", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Armour strength set.\n\r", ch);
            } else {
                ARMOR(pObj)->armor_strength = val;
                set_armour(pObj);
                send_to_char("Armour strength set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "type")) {
            if (argument[0] == '\0') { send_to_char("Syntax: armor type <none|cloth|leather|mail|plate>\n\r", ch); return false; }
            int val = flag_value(armor_types, argument);
            if (val == NO_FLAG) { send_to_char("Invalid armor type.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(ARMOR(pObj)->armor_type);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/armor/type", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/armor/type", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Armour type set.\n\r", ch);
            } else {
                ARMOR(pObj)->armor_type = val;
                send_to_char("Armour type set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: pierce, bash, slash, exotic, strength, type\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WArmour:{x\n\r"
        "  {Gtype          {x %s\n\r"
        "  {Gstrength      {x %s\n\r"
        "  {Gpierce        {x [%d]\n\r"
        "  {Gbash          {x [%d]\n\r"
        "  {Gslash         {x [%d]\n\r"
        "  {Gexotic        {x [%d]\n\r",
        flag_string(armor_types, ARMOR(pObj)->armor_type),
        armour_strength_table[ARMOR(pObj)->armor_strength].name,
        ARMOR(pObj)->protection[0],
        ARMOR(pObj)->protection[1],
        ARMOR(pObj)->protection[2],
        ARMOR(pObj)->protection[3]);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  BODY PART
 * ============================================================================ */
OEDIT(oedit_bodypart)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_BODY_PART(pObj)) {
        send_to_char("Object lacks the body_part type. Use '{Waddtype body_part{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "parts")) {
            if (argument[0] == '\0') { send_to_char("Syntax: bodypart parts <flags>\n\r", ch); return false; }
            long toggle = flag_value(part_flags, argument);
            if (toggle == NO_FLAG) { send_to_char("Invalid part flag.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                long current = olc_staged_flags_or(cs, "typedata/bodypart/parts", BODY_PART(pObj)->parts);
                long result = current ^ toggle;
                json_t *jold = json_integer((json_int_t)BODY_PART(pObj)->parts);
                json_t *jnew = json_integer((json_int_t)result);
                olc_changeset_add_change(cs, "typedata/bodypart/parts", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/bodypart/parts", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Body parts toggled.\n\r", ch);
            } else {
                BODY_PART(pObj)->parts ^= toggle;
                send_to_char("Body parts toggled.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "race")) {
            if (argument[0] == '\0') { send_to_char("Syntax: bodypart race <uid>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(BODY_PART(pObj)->race_uid);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/bodypart/race", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/bodypart/race", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Race UID set.\n\r", ch);
            } else {
                BODY_PART(pObj)->race_uid = val;
                send_to_char("Race UID set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: parts, race\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WBody Part:{x\n\r"
        "  {Gparts         {x %s\n\r"
        "  {Grace           {x [%d]\n\r",
        flag_string(part_flags, BODY_PART(pObj)->parts),
        BODY_PART(pObj)->race_uid);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  BOOK
 * ============================================================================ */
OEDIT(oedit_book)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_BOOK(pObj)) {
        send_to_char("Object lacks the book type. Use '{Waddtype book{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: book flags <flag>\n\r", ch); return false; }
            int toggle = flag_value(container_flags, argument);
            if (toggle == NO_FLAG) { send_to_char("Invalid flag.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                long current = olc_staged_flags_or(cs, "typedata/book/flags", BOOK(pObj)->flags);
                long result = current ^ toggle;
                json_t *jold = json_integer(BOOK(pObj)->flags);
                json_t *jnew = json_integer(result);
                olc_changeset_add_change(cs, "typedata/book/flags", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/book/flags", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Book flags toggled.\n\r", ch);
            } else {
                TOGGLE_BIT(BOOK(pObj)->flags, toggle);
                send_to_char("Book flags toggled.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: flags\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WBook:{x\n\r"
        "  {Gflags         {x [%s]\n\r",
        flag_string(container_flags, BOOK(pObj)->flags));
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  CART
 * ============================================================================ */
OEDIT(oedit_cart)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_CART(pObj)) {
        send_to_char("Object lacks the cart type. Use '{Waddtype cart{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "capacity")) {
            if (argument[0] == '\0') { send_to_char("Syntax: cart capacity <weight>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(CART(pObj)->capacity);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/cart/capacity", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/cart/capacity", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Cart capacity set.\n\r", ch);
            } else {
                CART(pObj)->capacity = val;
                send_to_char("Cart capacity set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "delay")) {
            if (argument[0] == '\0') { send_to_char("Syntax: cart delay <ticks>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(CART(pObj)->move_delay);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/cart/delay", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/cart/delay", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Cart move delay set.\n\r", ch);
            } else {
                CART(pObj)->move_delay = val;
                send_to_char("Cart move delay set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "strength")) {
            if (argument[0] == '\0') { send_to_char("Syntax: cart strength <min_str>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(CART(pObj)->min_strength);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/cart/strength", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/cart/strength", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Cart min strength set.\n\r", ch);
            } else {
                CART(pObj)->min_strength = val;
                send_to_char("Cart min strength set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "items")) {
            if (argument[0] == '\0') { send_to_char("Syntax: cart items <max>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(CART(pObj)->max_items);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/cart/items", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/cart/items", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Cart max items set.\n\r", ch);
            } else {
                CART(pObj)->max_items = val;
                send_to_char("Cart max items set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "weightmult")) {
            if (argument[0] == '\0') { send_to_char("Syntax: cart weightmult <multiplier>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(CART(pObj)->weight_multiplier);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/cart/weightmult", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/cart/weightmult", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Cart weight multiplier set.\n\r", ch);
            } else {
                CART(pObj)->weight_multiplier = val;
                send_to_char("Cart weight multiplier set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: cart flags <flag>\n\r", ch); return false; }
            long val = flag_value(cart_flags, argument);
            if (val != NO_FLAG) {
                if (cs) {
                    if (!olc_check_staging_limits(ch, cs)) return false;
                    long current = olc_staged_flags_or(cs, "typedata/cart/flags", CART(pObj)->flags);
                    long result = current ^ val;
                    json_t *jold = json_integer((json_int_t)CART(pObj)->flags);
                    json_t *jnew = json_integer((json_int_t)result);
                    olc_changeset_add_change(cs, "typedata/cart/flags", OLC_FIELD_TYPE_DATA, jold, jnew);
                    notify_field_change(cs, ch, "typedata/cart/flags", jnew, "type_data", true);
                    json_decref(jold); json_decref(jnew);
                    send_to_char("{G[STAGED]{x Cart flags toggled.\n\r", ch);
                } else {
                    CART(pObj)->flags ^= val;
                    send_to_char("Cart flags toggled.\n\r", ch);
                }
            }
            return true;
        }
        if (!str_prefix(field, "vanish")) {
            if (argument[0] == '\0') { send_to_char("Syntax: cart vanish <time>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(CART(pObj)->vanish_time);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/cart/vanish", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/cart/vanish", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Cart vanish time set.\n\r", ch);
            } else {
                CART(pObj)->vanish_time = val;
                send_to_char("Cart vanish time set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: capacity, delay, strength, items, weightmult, flags, vanish\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WCart:{x\n\r"
        "  {Gcapacity      {x [%d]\n\r"
        "  {Gdelay         {x [%d]\n\r"
        "  {Gstrength      {x [%d]\n\r"
        "  {Gitems         {x [%d]\n\r"
        "  {Gweightmult    {x [%d]\n\r"
        "  {Gflags         {x [%s]\n\r"
        "  {Gvanish        {x [%d]\n\r",
        CART(pObj)->capacity,
        CART(pObj)->move_delay,
        CART(pObj)->min_strength,
        CART(pObj)->max_items,
        CART(pObj)->weight_multiplier,
        flag_string(cart_flags, CART(pObj)->flags),
        CART(pObj)->vanish_time);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  COMPASS
 * ============================================================================ */
OEDIT(oedit_compass)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_COMPASS(pObj)) {
        send_to_char("Object lacks the compass type. Use '{Waddtype compass{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "accuracy")) {
            if (argument[0] == '\0') { send_to_char("Syntax: compass accuracy <percent>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(COMPASS(pObj)->accuracy);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/compass/accuracy", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/compass/accuracy", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Compass accuracy set.\n\r", ch);
            } else {
                COMPASS(pObj)->accuracy = val;
                send_to_char("Compass accuracy set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: accuracy\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WCompass:{x\n\r"
        "  {Gaccuracy      {x [%d%%]\n\r",
        COMPASS(pObj)->accuracy);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  CONTAINER
 * ============================================================================ */
OEDIT(oedit_container)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_CONTAINER(pObj)) {
        send_to_char("Object lacks the container type. Use '{Waddtype container{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "weight")) {
            if (argument[0] == '\0') { send_to_char("Syntax: container weight <max_kg>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(CONTAINER(pObj)->max_weight);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/container/weight", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/container/weight", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Container max weight set.\n\r", ch);
            } else {
                CONTAINER(pObj)->max_weight = val;
                send_to_char("Container max weight set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: container flags <flag>\n\r", ch); return false; }
            int val = flag_value(container_flags, argument);
            if (val == NO_FLAG) { send_to_char("Invalid container flag.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                long current = olc_staged_flags_or(cs, "typedata/container/flags", CONTAINER(pObj)->flags);
                long result = current ^ val;
                json_t *jold = json_integer((json_int_t)CONTAINER(pObj)->flags);
                json_t *jnew = json_integer((json_int_t)result);
                olc_changeset_add_change(cs, "typedata/container/flags", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/container/flags", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Container flags toggled.\n\r", ch);
            } else {
                TOGGLE_BIT(CONTAINER(pObj)->flags, val);
                send_to_char("Container flags toggled.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "items")) {
            if (argument[0] == '\0') { send_to_char("Syntax: container items <max>\n\r", ch); return false; }
            int val = atoi(argument);
            if (val > 225 && ch->tot_level < MAX_LEVEL) {
                send_to_char("Sorry, that value is out of range.\n\r", ch);
                return false;
            }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(CONTAINER(pObj)->max_items);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/container/items", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/container/items", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Container max items set.\n\r", ch);
            } else {
                CONTAINER(pObj)->max_items = val;
                send_to_char("Container max items set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "weightmult")) {
            if (argument[0] == '\0') { send_to_char("Syntax: container weightmult <1-1000>\n\r", ch); return false; }
            int val = atoi(argument);
            if (val <= 0 || val > 1000) {
                send_to_char("Weight multiplier must be between 1 and 1000.\n\r", ch);
                return false;
            }
            if (val < 1000 && !has_imp_sig(NULL, pObj) && ch->tot_level < MAX_LEVEL) {
                send_to_char("An imp sig is required to set the weight multiplier below 100%.\n\r", ch);
                return false;
            }
            if (has_imp_sig(NULL, pObj))
                use_imp_sig(NULL, pObj);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(CONTAINER(pObj)->weight_multiplier);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/container/weightmult", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/container/weightmult", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Container weight multiplier set.\n\r", ch);
            } else {
                CONTAINER(pObj)->weight_multiplier = val;
                send_to_char("Container weight multiplier set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: weight, flags, items, weightmult\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WContainer:{x\n\r"
        "  {Gweight        {x [%d kg]\n\r"
        "  {Gflags         {x [%s]\n\r"
        "  {Gitems         {x [%d]\n\r"
        "  {Gweightmult    {x [%d%%]\n\r",
        CONTAINER(pObj)->max_weight,
        flag_string(container_flags, CONTAINER(pObj)->flags),
        CONTAINER(pObj)->max_items,
        CONTAINER(pObj)->weight_multiplier);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  CORPSE
 * ============================================================================ */
OEDIT(oedit_corpse)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_CORPSE(pObj)) {
        send_to_char("Object lacks the corpse type. Use '{Waddtype npccorpse{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "type")) {
            if (argument[0] == '\0') { send_to_char("Syntax: corpse type <corpse_type>\n\r", ch); return false; }
            int val = flag_value(corpse_types, argument);
            if (val == NO_FLAG) { send_to_char("Invalid corpse type.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(CORPSE(pObj)->corpse_type);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/corpse/type", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/corpse/type", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Corpse type set.\n\r", ch);
            } else {
                CORPSE(pObj)->corpse_type = val;
                send_to_char("Corpse type set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "resurrection")) {
            if (argument[0] == '\0') { send_to_char("Syntax: corpse resurrection <percent>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(CORPSE(pObj)->resurrection);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/corpse/resurrection", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/corpse/resurrection", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Resurrection chance set.\n\r", ch);
            } else {
                CORPSE(pObj)->resurrection = val;
                send_to_char("Resurrection chance set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "animation")) {
            if (argument[0] == '\0') { send_to_char("Syntax: corpse animation <percent>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(CORPSE(pObj)->animation);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/corpse/animation", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/corpse/animation", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Animation chance set.\n\r", ch);
            } else {
                CORPSE(pObj)->animation = val;
                send_to_char("Animation chance set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "parts")) {
            if (argument[0] == '\0') { send_to_char("Syntax: corpse parts <body_part_flags>\n\r", ch); return false; }
            long val = flag_value(part_flags, argument);
            if (val == NO_FLAG) { send_to_char("Invalid body part flags.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer((json_int_t)CORPSE(pObj)->body_parts);
                json_t *jnew = json_integer((json_int_t)val);
                olc_changeset_add_change(cs, "typedata/corpse/parts", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/corpse/parts", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Body parts set.\n\r", ch);
            } else {
                CORPSE(pObj)->body_parts = val;
                send_to_char("Body parts set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "mobile")) {
            if (argument[0] == '\0') { send_to_char("Syntax: corpse mobile <vnum|0>\n\r", ch); return false; }
            if (atol(argument) != 0) {
                WNUM key_wnum = { NULL, 0 };
                MOB_INDEX_DATA *key_mob;
                parse_widevnum(argument, ch->in_room ? ch->in_room->area : NULL, &key_wnum);
                key_mob = key_wnum.pArea ? get_mob_index(key_wnum.pArea, key_wnum.vnum) : get_mob_index_global(key_wnum.vnum);
                if (!key_mob) {
                    send_to_char("No such mobile exists.\n\r", ch);
                    return false;
                }
                if (cs) {
                    if (!olc_check_staging_limits(ch, cs)) return false;
                    json_t *jold = json_pack("{s:I, s:I}",
                        "mobile_auid", (json_int_t)CORPSE(pObj)->mobile_area_uid,
                        "mobile_vnum", (json_int_t)CORPSE(pObj)->mobile_vnum);
                    json_t *jnew = json_pack("{s:I, s:I}",
                        "mobile_auid", (json_int_t)(key_mob->area ? key_mob->area->uid : 0),
                        "mobile_vnum", (json_int_t)key_wnum.vnum);
                    olc_changeset_add_change(cs, "typedata/corpse/mobile", OLC_FIELD_TYPE_DATA, jold, jnew);
                    notify_field_change(cs, ch, "typedata/corpse/mobile", jnew, "type_data", true);
                    json_decref(jold); json_decref(jnew);
                    send_to_char("{G[STAGED]{x Mobile set.\n\r", ch);
                } else {
                    CORPSE(pObj)->mobile_vnum = key_wnum.vnum;
                    CORPSE(pObj)->mobile_area_uid = key_mob->area ? key_mob->area->uid : 0;
                    send_to_char("Mobile set.\n\r", ch);
                }
            } else {
                if (cs) {
                    if (!olc_check_staging_limits(ch, cs)) return false;
                    json_t *jold = json_pack("{s:I, s:I}",
                        "mobile_auid", (json_int_t)CORPSE(pObj)->mobile_area_uid,
                        "mobile_vnum", (json_int_t)CORPSE(pObj)->mobile_vnum);
                    json_t *jnew = json_pack("{s:I, s:I}",
                        "mobile_auid", (json_int_t)0,
                        "mobile_vnum", (json_int_t)0);
                    olc_changeset_add_change(cs, "typedata/corpse/mobile", OLC_FIELD_TYPE_DATA, jold, jnew);
                    notify_field_change(cs, ch, "typedata/corpse/mobile", jnew, "type_data", true);
                    json_decref(jold); json_decref(jnew);
                    send_to_char("{G[STAGED]{x Mobile set.\n\r", ch);
                } else {
                    CORPSE(pObj)->mobile_vnum = 0;
                    CORPSE(pObj)->mobile_area_uid = 0;
                    send_to_char("Mobile set.\n\r", ch);
                }
            }
            return true;
        }
        send_to_char("Valid fields: type, resurrection, animation, parts, mobile\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WCorpse:{x\n\r"
        "  {Gtype          {x %s\n\r"
        "  {Gresurrection  {x %d%%\n\r"
        "  {Ganimation     {x %d%%\n\r"
        "  {Gparts         {x %s\n\r"
        "  {Gmobile        {x %s\n\r",
        flag_string(corpse_types, CORPSE(pObj)->corpse_type),
        CORPSE(pObj)->resurrection,
        CORPSE(pObj)->animation,
        flag_string(part_flags, CORPSE(pObj)->body_parts),
        widevnum_string(CORPSE(pObj)->mobile_area_uid > 0
            ? get_area_index(CORPSE(pObj)->mobile_area_uid) : NULL,
            CORPSE(pObj)->mobile_vnum, pObj->area));
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  DRINK / FLUID CONTAINER
 * ============================================================================ */
OEDIT(oedit_drink)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_FLUID_CON(pObj)) {
        send_to_char("Object lacks the fluid container type. Use '{Waddtype drinkcontainer{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "capacity")) {
            if (argument[0] == '\0') { send_to_char("Syntax: drink capacity <amount>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(FLUID_CON(pObj)->capacity);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/drink/capacity", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/drink/capacity", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Liquid capacity set.\n\r", ch);
            } else {
                FLUID_CON(pObj)->capacity = val;
                send_to_char("Liquid capacity set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "amount")) {
            if (argument[0] == '\0') { send_to_char("Syntax: drink amount <current>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(FLUID_CON(pObj)->amount);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/drink/amount", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/drink/amount", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Liquid amount set.\n\r", ch);
            } else {
                FLUID_CON(pObj)->amount = val;
                send_to_char("Liquid amount set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "liquid")) {
            if (argument[0] == '\0') { send_to_char("Syntax: drink liquid <liquid_name>\n\r", ch); return false; }
            int liq = liq_lookup(argument);
            int val = (liq != -1) ? liq : 0;
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(FLUID_CON(pObj)->liquid);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/drink/liquid", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/drink/liquid", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Liquid type set.\n\r", ch);
            } else {
                FLUID_CON(pObj)->liquid = val;
                send_to_char("Liquid type set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "poison")) {
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                int cur = FLUID_CON(pObj)->poison;
                olc_pending_change_t *existing = olc_changeset_find_change(cs, "typedata/drink/poison");
                if (existing) cur = (int)json_integer_value(existing->new_value);
                int val = (cur == 0) ? 1 : 0;
                json_t *jold = json_integer(FLUID_CON(pObj)->poison);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/drink/poison", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/drink/poison", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Poison toggled.\n\r", ch);
            } else {
                FLUID_CON(pObj)->poison = (FLUID_CON(pObj)->poison == 0) ? 1 : 0;
                send_to_char("Poison toggled.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "refill")) {
            if (argument[0] == '\0') { send_to_char("Syntax: drink refill <rate>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(FLUID_CON(pObj)->refill_rate);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/drink/refill", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/drink/refill", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Refill rate set.\n\r", ch);
            } else {
                FLUID_CON(pObj)->refill_rate = val;
                send_to_char("Refill rate set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: capacity, amount, liquid, poison, refill\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WFluid Container:{x\n\r"
        "  {Gcapacity      {x [%d]\n\r"
        "  {Gamount        {x [%d]\n\r"
        "  {Gliquid        {x %s\n\r"
        "  {Gpoison        {x %s\n\r"
        "  {Grefill        {x [%d]\n\r",
        FLUID_CON(pObj)->capacity,
        FLUID_CON(pObj)->amount,
        liquid_name(FLUID_CON(pObj)->liquid),
        FLUID_CON(pObj)->poison != 0 ? "Yes" : "No",
        FLUID_CON(pObj)->refill_rate);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  FOOD
 * ============================================================================ */
OEDIT(oedit_food)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_FOOD(pObj)) {
        send_to_char("Object lacks the food type. Use '{Waddtype food{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "hunger")) {
            if (argument[0] == '\0') { send_to_char("Syntax: food hunger <hours>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(FOOD(pObj)->hunger);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/food/hunger", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/food/hunger", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Food hunger hours set.\n\r", ch);
            } else {
                FOOD(pObj)->hunger = val;
                send_to_char("Food hunger hours set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "full")) {
            if (argument[0] == '\0') { send_to_char("Syntax: food full <hours>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(FOOD(pObj)->full);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/food/full", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/food/full", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Food full hours set.\n\r", ch);
            } else {
                FOOD(pObj)->full = val;
                send_to_char("Food full hours set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "poison")) {
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                int cur = FOOD(pObj)->poison;
                olc_pending_change_t *existing = olc_changeset_find_change(cs, "typedata/food/poison");
                if (existing) cur = (int)json_integer_value(existing->new_value);
                int val = (cur == 0) ? 1 : 0;
                json_t *jold = json_integer(FOOD(pObj)->poison);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/food/poison", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/food/poison", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Poison toggled.\n\r", ch);
            } else {
                FOOD(pObj)->poison = (FOOD(pObj)->poison == 0) ? 1 : 0;
                send_to_char("Poison toggled.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "timer")) {
            if (argument[0] == '\0') { send_to_char("Syntax: food timer <ticks>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(FOOD(pObj)->timer);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/food/timer", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/food/timer", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Food timer set.\n\r", ch);
            } else {
                FOOD(pObj)->timer = val;
                send_to_char("Food timer set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: hunger, full, poison, timer\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WFood:{x\n\r"
        "  {Ghunger        {x [%d]\n\r"
        "  {Gfull          {x [%d]\n\r"
        "  {Gpoison        {x %s\n\r"
        "  {Gtimer         {x [%d]\n\r",
        FOOD(pObj)->hunger,
        FOOD(pObj)->full,
        FOOD(pObj)->poison != 0 ? "Yes" : "No",
        FOOD(pObj)->timer);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  FURNITURE
 * ============================================================================ */
OEDIT(oedit_furniture)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_FURNITURE(pObj)) {
        send_to_char("Object lacks the furniture type. Use '{Waddtype furniture{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "people")) {
            if (argument[0] == '\0') { send_to_char("Syntax: furniture people <max>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(FURNITURE(pObj)->max_people);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/furniture/people", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/furniture/people", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Max people set.\n\r", ch);
            } else {
                FURNITURE(pObj)->max_people = val;
                send_to_char("Max people set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "weight")) {
            if (argument[0] == '\0') { send_to_char("Syntax: furniture weight <max_kg>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(FURNITURE(pObj)->max_weight);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/furniture/weight", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/furniture/weight", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Max weight set.\n\r", ch);
            } else {
                FURNITURE(pObj)->max_weight = val;
                send_to_char("Max weight set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: furniture flags <flag>\n\r", ch); return false; }
            long val = flag_value(furniture_flags, argument);
            if (val != NO_FLAG) {
                if (cs) {
                    if (!olc_check_staging_limits(ch, cs)) return false;
                    long current = olc_staged_flags_or(cs, "typedata/furniture/flags", FURNITURE(pObj)->flags);
                    long result = current ^ val;
                    json_t *jold = json_integer((json_int_t)FURNITURE(pObj)->flags);
                    json_t *jnew = json_integer((json_int_t)result);
                    olc_changeset_add_change(cs, "typedata/furniture/flags", OLC_FIELD_TYPE_DATA, jold, jnew);
                    notify_field_change(cs, ch, "typedata/furniture/flags", jnew, "type_data", true);
                    json_decref(jold); json_decref(jnew);
                    send_to_char("{G[STAGED]{x Furniture flags toggled.\n\r", ch);
                } else {
                    FURNITURE(pObj)->flags ^= val;
                    send_to_char("Furniture flags toggled.\n\r", ch);
                }
            }
            return true;
        }
        if (!str_prefix(field, "heal")) {
            if (argument[0] == '\0') { send_to_char("Syntax: furniture heal <bonus>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(FURNITURE(pObj)->heal_rate);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/furniture/heal", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/furniture/heal", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Heal bonus set.\n\r", ch);
            } else {
                FURNITURE(pObj)->heal_rate = val;
                send_to_char("Heal bonus set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "mana")) {
            if (argument[0] == '\0') { send_to_char("Syntax: furniture mana <bonus>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(FURNITURE(pObj)->mana_rate);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/furniture/mana", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/furniture/mana", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Mana bonus set.\n\r", ch);
            } else {
                FURNITURE(pObj)->mana_rate = val;
                send_to_char("Mana bonus set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "move")) {
            if (argument[0] == '\0') { send_to_char("Syntax: furniture move <bonus>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(FURNITURE(pObj)->move_rate);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/furniture/move", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/furniture/move", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Move bonus set.\n\r", ch);
            } else {
                FURNITURE(pObj)->move_rate = val;
                send_to_char("Move bonus set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: people, weight, flags, heal, mana, move\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WFurniture:{x\n\r"
        "  {Gpeople        {x [%d]\n\r"
        "  {Gweight        {x [%d]\n\r"
        "  {Gflags         {x %s\n\r"
        "  {Gheal          {x [%d]\n\r"
        "  {Gmana          {x [%d]\n\r"
        "  {Gmove          {x [%d]\n\r",
        FURNITURE(pObj)->max_people,
        FURNITURE(pObj)->max_weight,
        flag_string(furniture_flags, FURNITURE(pObj)->flags),
        FURNITURE(pObj)->heal_rate,
        FURNITURE(pObj)->mana_rate,
        FURNITURE(pObj)->move_rate);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  HERB
 * ============================================================================ */
OEDIT(oedit_herb)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_HERB(pObj)) {
        send_to_char("Object lacks the herb type. Use '{Waddtype herb{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "type")) {
            if (argument[0] == '\0') { send_to_char("Syntax: herb type <herb_name>\n\r", ch); return false; }
            int i;
            for (i = 0; i < MAX_HERB; i++) {
                if (!str_prefix(argument, herb_table[i].name))
                    break;
            }
            if (i >= MAX_HERB) { send_to_char("Invalid herb type.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(HERB(pObj)->type);
                json_t *jnew = json_integer(i);
                olc_changeset_add_change(cs, "typedata/herb/type", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/herb/type", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Herb type set.\n\r", ch);
            } else {
                HERB(pObj)->type = i;
                send_to_char("Herb type set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "healing")) {
            if (argument[0] == '\0') { send_to_char("Syntax: herb healing <percent>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(HERB(pObj)->healing);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/herb/healing", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/herb/healing", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Healing rate set.\n\r", ch);
            } else {
                HERB(pObj)->healing = val;
                send_to_char("Healing rate set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "regen")) {
            if (argument[0] == '\0') { send_to_char("Syntax: herb regen <percent>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(HERB(pObj)->regenerative);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/herb/regen", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/herb/regen", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Regenerative rate set.\n\r", ch);
            } else {
                HERB(pObj)->regenerative = val;
                send_to_char("Regenerative rate set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "refresh")) {
            if (argument[0] == '\0') { send_to_char("Syntax: herb refresh <percent>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(HERB(pObj)->refreshing);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/herb/refresh", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/herb/refresh", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Refreshing rate set.\n\r", ch);
            } else {
                HERB(pObj)->refreshing = val;
                send_to_char("Refreshing rate set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "immunity")) {
            if (argument[0] == '\0') { send_to_char("Syntax: herb immunity <flags>\n\r", ch); return false; }
            long toggle = flag_value(imm_flags, argument);
            if (toggle == NO_FLAG) { send_to_char("Invalid immunity flag.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                long current = olc_staged_flags_or(cs, "typedata/herb/immunity", HERB(pObj)->immunity);
                long result = current ^ toggle;
                json_t *jold = json_integer((json_int_t)HERB(pObj)->immunity);
                json_t *jnew = json_integer((json_int_t)result);
                olc_changeset_add_change(cs, "typedata/herb/immunity", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/herb/immunity", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Immunity toggled.\n\r", ch);
            } else {
                HERB(pObj)->immunity ^= toggle;
                send_to_char("Immunity toggled.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "resistance")) {
            if (argument[0] == '\0') { send_to_char("Syntax: herb resistance <flags>\n\r", ch); return false; }
            long toggle = flag_value(res_flags, argument);
            if (toggle == NO_FLAG) { send_to_char("Invalid resistance flag.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                long current = olc_staged_flags_or(cs, "typedata/herb/resistance", HERB(pObj)->resistance);
                long result = current ^ toggle;
                json_t *jold = json_integer((json_int_t)HERB(pObj)->resistance);
                json_t *jnew = json_integer((json_int_t)result);
                olc_changeset_add_change(cs, "typedata/herb/resistance", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/herb/resistance", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Resistance toggled.\n\r", ch);
            } else {
                HERB(pObj)->resistance ^= toggle;
                send_to_char("Resistance toggled.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "vulnerability")) {
            if (argument[0] == '\0') { send_to_char("Syntax: herb vulnerability <flags>\n\r", ch); return false; }
            long toggle = flag_value(vuln_flags, argument);
            if (toggle == NO_FLAG) { send_to_char("Invalid vulnerability flag.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                long current = olc_staged_flags_or(cs, "typedata/herb/vulnerability", HERB(pObj)->vulnerability);
                long result = current ^ toggle;
                json_t *jold = json_integer((json_int_t)HERB(pObj)->vulnerability);
                json_t *jnew = json_integer((json_int_t)result);
                olc_changeset_add_change(cs, "typedata/herb/vulnerability", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/herb/vulnerability", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Vulnerability toggled.\n\r", ch);
            } else {
                HERB(pObj)->vulnerability ^= toggle;
                send_to_char("Vulnerability toggled.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "spell")) {
            if (argument[0] == '\0') { send_to_char("Syntax: herb spell <spell_name>\n\r", ch); return false; }
            int sn = skill_lookup(argument);
            SKILL_DATA *spell_ref = skill_find_uid(sn);
            int new_spell = -1;
            if (sn > 0 && spell_ref && spell_ref->spell_fun != spell_null) {
                new_spell = sn;
            } else if (sn == 0 || !str_cmp(argument, "none")) {
                new_spell = 0;
            }
            if (new_spell < 0) { send_to_char("Invalid spell.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(HERB(pObj)->spell);
                json_t *jnew = json_integer(new_spell);
                olc_changeset_add_change(cs, "typedata/herb/spell", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/herb/spell", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char(new_spell > 0 ? "{G[STAGED]{x Spell set.\n\r" : "{G[STAGED]{x Spell cleared.\n\r", ch);
            } else {
                HERB(pObj)->spell = new_spell;
                send_to_char(new_spell > 0 ? "Spell set.\n\r" : "Spell cleared.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: type, healing, regen, refresh, immunity, resistance, vulnerability, spell\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WHerb:{x\n\r"
        "  {Gtype          {x %s\n\r"
        "  {Ghealing       {x [%d%%]\n\r"
        "  {Gregen         {x [%d%%]\n\r"
        "  {Grefresh       {x [%d%%]\n\r"
        "  {Gimmunity      {x %s\n\r"
        "  {Gresistance    {x %s\n\r"
        "  {Gvulnerability {x %s\n\r"
        "  {Gspell         {x %s\n\r",
        herb_table[HERB(pObj)->type].name,
        HERB(pObj)->healing,
        HERB(pObj)->regenerative,
        HERB(pObj)->refreshing,
        flag_string(imm_flags, HERB(pObj)->immunity),
        flag_string(res_flags, HERB(pObj)->resistance),
        flag_string(vuln_flags, HERB(pObj)->vulnerability),
        HERB(pObj)->spell > 0 ? (skill_find_uid(HERB(pObj)->spell) ? skill_find_uid(HERB(pObj)->spell)->name : "unknown") : "none");
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  INK
 * ============================================================================ */
OEDIT(oedit_ink)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_INK(pObj)) {
        send_to_char("Object lacks the ink type. Use '{Waddtype ink{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "type1")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ink type1 <catalyst_type>\n\r", ch); return false; }
            int val = flag_lookup(argument, catalyst_types);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(INK(pObj)->types[0]);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/ink/type1", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/ink/type1", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Ink type 1 set.\n\r", ch);
            } else {
                INK(pObj)->types[0] = val;
                send_to_char("Ink type 1 set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "type2")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ink type2 <catalyst_type>\n\r", ch); return false; }
            int val = flag_lookup(argument, catalyst_types);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(INK(pObj)->types[1]);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/ink/type2", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/ink/type2", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Ink type 2 set.\n\r", ch);
            } else {
                INK(pObj)->types[1] = val;
                send_to_char("Ink type 2 set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "type3")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ink type3 <catalyst_type>\n\r", ch); return false; }
            int val = flag_lookup(argument, catalyst_types);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(INK(pObj)->types[2]);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/ink/type3", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/ink/type3", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Ink type 3 set.\n\r", ch);
            } else {
                INK(pObj)->types[2] = val;
                send_to_char("Ink type 3 set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: type1, type2, type3\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WInk:{x\n\r"
        "  {Gtype1         {x %s\n\r"
        "  {Gtype2         {x %s\n\r"
        "  {Gtype3         {x %s\n\r",
        flag_string(catalyst_types, INK(pObj)->types[0]),
        flag_string(catalyst_types, INK(pObj)->types[1]),
        flag_string(catalyst_types, INK(pObj)->types[2]));
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  INSTRUMENT
 * ============================================================================ */
OEDIT(oedit_instrument)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_INSTRUMENT(pObj)) {
        send_to_char("Object lacks the instrument type. Use '{Waddtype instrument{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "type")) {
            if (argument[0] == '\0') { send_to_char("Syntax: instrument type <instrument_type>\n\r", ch); return false; }
            int val = flag_value(instrument_types, argument);
            if (val == NO_FLAG) { send_to_char("Invalid instrument type.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(INSTRUMENT(pObj)->type);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/instrument/type", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/instrument/type", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Instrument type set.\n\r", ch);
            } else {
                INSTRUMENT(pObj)->type = val;
                send_to_char("Instrument type set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: instrument flags <flag>\n\r", ch); return false; }
            int val = flag_value(instrument_flags, argument);
            if (val == NO_FLAG) { send_to_char("Invalid flag.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                long current = olc_staged_flags_or(cs, "typedata/instrument/flags", INSTRUMENT(pObj)->flags);
                long result = current ^ val;
                json_t *jold = json_integer((json_int_t)INSTRUMENT(pObj)->flags);
                json_t *jnew = json_integer((json_int_t)result);
                olc_changeset_add_change(cs, "typedata/instrument/flags", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/instrument/flags", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Instrument flags toggled.\n\r", ch);
            } else {
                INSTRUMENT(pObj)->flags ^= val;
                send_to_char("Instrument flags toggled.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "beatsmin")) {
            if (argument[0] == '\0') { send_to_char("Syntax: instrument beatsmin <percent>\n\r", ch); return false; }
            int val = atoi(argument);
            if (val < 1 || val > 5000) {
                send_to_char("Min scale factor must be between 1%% and 5000%%.\n\r", ch);
                return false;
            }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(INSTRUMENT(pObj)->beats_min);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/instrument/beatsmin", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/instrument/beatsmin", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Min playtime scale factor set.\n\r", ch);
            } else {
                INSTRUMENT(pObj)->beats_min = val;
                send_to_char("Min playtime scale factor set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "beatsmax")) {
            if (argument[0] == '\0') { send_to_char("Syntax: instrument beatsmax <percent>\n\r", ch); return false; }
            int val = atoi(argument);
            if (val < 1 || val > 5000) {
                send_to_char("Max scale factor must be between 1%% and 5000%%.\n\r", ch);
                return false;
            }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(INSTRUMENT(pObj)->beats_max);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/instrument/beatsmax", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/instrument/beatsmax", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Max playtime scale factor set.\n\r", ch);
            } else {
                INSTRUMENT(pObj)->beats_max = val;
                send_to_char("Max playtime scale factor set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: type, flags, beatsmin, beatsmax\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WInstrument:{x\n\r"
        "  {Gtype          {x %s\n\r"
        "  {Gflags         {x %s\n\r"
        "  {Gbeatsmin      {x [%d%%]\n\r"
        "  {Gbeatsmax      {x [%d%%]\n\r",
        flag_string(instrument_types, INSTRUMENT(pObj)->type),
        flag_string(instrument_flags, INSTRUMENT(pObj)->flags),
        INSTRUMENT(pObj)->beats_min,
        INSTRUMENT(pObj)->beats_max);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  JEWELRY
 * ============================================================================ */
OEDIT(oedit_jewelry)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_JEWELRY(pObj)) {
        send_to_char("Object lacks the jewelry type. Use '{Waddtype jewelry{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "mana")) {
            if (argument[0] == '\0') { send_to_char("Syntax: jewelry mana <max_mana>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(JEWELRY(pObj)->max_mana);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/jewelry/mana", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/jewelry/mana", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Max mana set.\n\r", ch);
            } else {
                JEWELRY(pObj)->max_mana = val;
                send_to_char("Max mana set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: mana\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WJewelry:{x\n\r"
        "  {Gmana          {x [%d]\n\r",
        JEWELRY(pObj)->max_mana);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  LIGHT
 * ============================================================================ */
OEDIT(oedit_light)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_LIGHT(pObj)) {
        send_to_char("Object lacks the light type. Use '{Waddtype light{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "duration")) {
            if (argument[0] == '\0') { send_to_char("Syntax: light duration <hours|-1 for infinite>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(LIGHT(pObj)->duration);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/light/duration", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/light/duration", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Light duration set.\n\r", ch);
            } else {
                LIGHT(pObj)->duration = val;
                send_to_char("Light duration set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: light flags <flag>\n\r", ch); return false; }
            long toggle = flag_value(light_flags, argument);
            if (toggle == NO_FLAG) { send_to_char("Invalid light flag.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                long current = olc_staged_flags_or(cs, "typedata/light/flags", LIGHT(pObj)->flags);
                long result = current ^ toggle;
                json_t *jold = json_integer((json_int_t)LIGHT(pObj)->flags);
                json_t *jnew = json_integer((json_int_t)result);
                olc_changeset_add_change(cs, "typedata/light/flags", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/light/flags", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Light flags toggled.\n\r", ch);
            } else {
                LIGHT(pObj)->flags ^= toggle;
                send_to_char("Light flags toggled.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: duration, flags\n\r", ch);
        return false;
    }

    if (LIGHT(pObj)->duration == -1) {
        sprintf(buf,
            "{WLight:{x\n\r"
            "  {Gduration      {x Infinite [-1]\n\r"
            "  {Gflags         {x [%s]\n\r",
            flag_string(light_flags, LIGHT(pObj)->flags));
    } else {
        sprintf(buf,
            "{WLight:{x\n\r"
            "  {Gduration      {x [%d]\n\r"
            "  {Gflags         {x [%s]\n\r",
            LIGHT(pObj)->duration,
            flag_string(light_flags, LIGHT(pObj)->flags));
    }
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  MAP
 * ============================================================================ */
OEDIT(oedit_map)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_MAP(pObj)) {
        send_to_char("Object lacks the map type. Use '{Waddtype map{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "wuid")) {
            if (argument[0] == '\0') { send_to_char("Syntax: map wuid <wilderness_uid>\n\r", ch); return false; }
            long val = atol(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer((json_int_t)MAP(pObj)->wuid);
                json_t *jnew = json_integer((json_int_t)val);
                olc_changeset_add_change(cs, "typedata/map/wuid", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/map/wuid", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Map wilderness UID set.\n\r", ch);
            } else {
                MAP(pObj)->wuid = val;
                send_to_char("Map wilderness UID set.\n\r", ch);
            }
            return true;
        }
        if (!str_cmp(field, "x")) {
            if (argument[0] == '\0') { send_to_char("Syntax: map x <coordinate>\n\r", ch); return false; }
            long val = atol(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer((json_int_t)MAP(pObj)->x);
                json_t *jnew = json_integer((json_int_t)val);
                olc_changeset_add_change(cs, "typedata/map/x", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/map/x", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Map X coordinate set.\n\r", ch);
            } else {
                MAP(pObj)->x = val;
                send_to_char("Map X coordinate set.\n\r", ch);
            }
            return true;
        }
        if (!str_cmp(field, "y")) {
            if (argument[0] == '\0') { send_to_char("Syntax: map y <coordinate>\n\r", ch); return false; }
            long val = atol(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer((json_int_t)MAP(pObj)->y);
                json_t *jnew = json_integer((json_int_t)val);
                olc_changeset_add_change(cs, "typedata/map/y", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/map/y", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Map Y coordinate set.\n\r", ch);
            } else {
                MAP(pObj)->y = val;
                send_to_char("Map Y coordinate set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: wuid, x, y\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WMap:{x\n\r"
        "  {Gwuid          {x [%ld]\n\r"
        "  {Gx             {x [%ld]\n\r"
        "  {Gy             {x [%ld]\n\r",
        MAP(pObj)->wuid,
        MAP(pObj)->x,
        MAP(pObj)->y);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  MIST
 * ============================================================================ */
OEDIT(oedit_mist)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_MIST(pObj)) {
        send_to_char("Object lacks the mist type. Use '{Waddtype mist{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "objects")) {
            if (argument[0] == '\0') { send_to_char("Syntax: mist objects <percent>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(MIST(pObj)->obscure_objs);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/mist/objects", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/mist/objects", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Object obscurity set.\n\r", ch);
            } else {
                MIST(pObj)->obscure_objs = val;
                send_to_char("Object obscurity set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "characters")) {
            if (argument[0] == '\0') { send_to_char("Syntax: mist characters <percent>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(MIST(pObj)->obscure_mobs);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/mist/characters", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/mist/characters", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Character obscurity set.\n\r", ch);
            } else {
                MIST(pObj)->obscure_mobs = val;
                send_to_char("Character obscurity set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "room")) {
            if (argument[0] == '\0') { send_to_char("Syntax: mist room <percent>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(MIST(pObj)->obscure_room);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/mist/room", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/mist/room", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Room obscurity set.\n\r", ch);
            } else {
                MIST(pObj)->obscure_room = val;
                send_to_char("Room obscurity set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: objects, characters, room\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WMist:{x\n\r"
        "  {Gobjects       {x [%d%%]\n\r"
        "  {Gcharacters    {x [%d%%]\n\r"
        "  {Groom          {x [%d%%]\n\r",
        MIST(pObj)->obscure_objs,
        MIST(pObj)->obscure_mobs,
        MIST(pObj)->obscure_room);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  MONEY
 * ============================================================================ */
OEDIT(oedit_money)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_MONEY(pObj)) {
        send_to_char("Object lacks the money type. Use '{Waddtype money{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "silver")) {
            if (argument[0] == '\0') { send_to_char("Syntax: money silver <amount>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(MONEY(pObj)->silver);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/money/silver", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/money/silver", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Silver amount set.\n\r", ch);
            } else {
                MONEY(pObj)->silver = val;
                send_to_char("Silver amount set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "gold")) {
            if (argument[0] == '\0') { send_to_char("Syntax: money gold <amount>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(MONEY(pObj)->gold);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/money/gold", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/money/gold", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Gold amount set.\n\r", ch);
            } else {
                MONEY(pObj)->gold = val;
                send_to_char("Gold amount set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: silver, gold\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WMoney:{x\n\r"
        "  {Gsilver        {x [%d]\n\r"
        "  {Ggold          {x [%d]\n\r",
        MONEY(pObj)->silver,
        MONEY(pObj)->gold);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  PAGE
 * ============================================================================ */
OEDIT(oedit_page)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_PAGE(pObj)) {
        send_to_char("Object lacks the page type. Use '{Waddtype part{x' (page).\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "number")) {
            if (argument[0] == '\0') { send_to_char("Syntax: page number <page_no>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(PAGE(pObj)->page_no);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/page/number", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/page/number", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Page number set.\n\r", ch);
            } else {
                PAGE(pObj)->page_no = val;
                send_to_char("Page number set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "title")) {
            if (argument[0] == '\0') { send_to_char("Syntax: page title <text>\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_string(PAGE(pObj)->title ? PAGE(pObj)->title : "");
                json_t *jnew = json_string(argument);
                olc_changeset_add_change(cs, "typedata/page/title", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/page/title", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Page title set.\n\r", ch);
            } else {
                free_string(PAGE(pObj)->title);
                PAGE(pObj)->title = str_dup(argument);
                send_to_char("Page title set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: number, title\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WPage:{x\n\r"
        "  {Gnumber        {x [%d]\n\r"
        "  {Gtitle         {x %s\n\r",
        PAGE(pObj)->page_no,
        PAGE(pObj)->title ? PAGE(pObj)->title : "(none)");
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  PORTAL
 * ============================================================================ */
OEDIT(oedit_portal)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_PORTAL(pObj)) {
        send_to_char("Object lacks the portal type. Use '{Waddtype portal{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "charges")) {
            if (argument[0] == '\0') { send_to_char("Syntax: portal charges <num>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(PORTAL(pObj)->charges);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/portal/charges", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/portal/charges", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Portal charges set.\n\r", ch);
            } else {
                PORTAL(pObj)->charges = val;
                send_to_char("Portal charges set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "exit")) {
            if (argument[0] == '\0') { send_to_char("Syntax: portal exit <exit_flags>\n\r", ch); return false; }
            long toggle = flag_value(portal_exit_flags, argument);
            if (toggle == NO_FLAG) { send_to_char("Invalid exit flag.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                long current = olc_staged_flags_or(cs, "typedata/portal/exit", PORTAL(pObj)->exit);
                long result = current ^ toggle;
                json_t *jold = json_integer((json_int_t)PORTAL(pObj)->exit);
                json_t *jnew = json_integer((json_int_t)result);
                olc_changeset_add_change(cs, "typedata/portal/exit", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/portal/exit", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Exit flags toggled.\n\r", ch);
            } else {
                PORTAL(pObj)->exit ^= toggle;
                send_to_char("Exit flags toggled.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: portal flags <portal_flags>\n\r", ch); return false; }
            long toggle = flag_value(portal_flags, argument);
            if (toggle == NO_FLAG) { send_to_char("Invalid portal flag.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                long current_flags = olc_staged_flags_or(cs, "typedata/portal/flags", PORTAL(pObj)->flags);
                long new_flags = current_flags ^ toggle;
                if (IS_SET(new_flags, GATE_DUNGEON))
                    REMOVE_BIT(new_flags, GATE_AREARANDOM);
                json_t *jold_f = json_integer((json_int_t)PORTAL(pObj)->flags);
                json_t *jnew_f = json_integer((json_int_t)new_flags);
                olc_changeset_add_change(cs, "typedata/portal/flags", OLC_FIELD_TYPE_DATA, jold_f, jnew_f);
                notify_field_change(cs, ch, "typedata/portal/flags", jnew_f, "type_data", true);
                json_decref(jold_f); json_decref(jnew_f);
                /* If DUNGEON just turned on, clear all params */
                if (IS_SET(toggle, GATE_DUNGEON) && IS_SET(new_flags, GATE_DUNGEON)) {
                    for (int pi = 0; pi < MAX_PORTAL_VALUES; pi++) {
                        char ppath[MSL];
                        sprintf(ppath, "typedata/portal/param%d", pi);
                        json_t *jpold = json_integer((json_int_t)PORTAL(pObj)->params[pi]);
                        json_t *jpnew = json_integer(0);
                        olc_changeset_add_change(cs, ppath, OLC_FIELD_TYPE_DATA, jpold, jpnew);
                        json_decref(jpold); json_decref(jpnew);
                    }
                }
                send_to_char("{G[STAGED]{x Portal flags toggled.\n\r", ch);
            } else {
                PORTAL(pObj)->flags ^= toggle;
                if (IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON))
                    REMOVE_BIT(PORTAL(pObj)->flags, GATE_AREARANDOM);
                if (IS_SET(toggle, GATE_DUNGEON) && IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON)) {
                    PORTAL(pObj)->params[0] = 0;
                    PORTAL(pObj)->params[1] = 0;
                    PORTAL(pObj)->params[2] = 0;
                    PORTAL(pObj)->params[3] = 0;
                    PORTAL(pObj)->params[4] = 0;
                }
                send_to_char("Portal flags toggled.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "destination")) {
            if (argument[0] == '\0') { send_to_char("Syntax: portal destination <dungeon_vnum|area#vnum|widevnum|-1>\n\r", ch); return false; }
            if (IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON)) {
                DUNGEON_INDEX_DATA *dng = NULL;
                WNUM dng_wnum = { NULL, 0 };
                AREA_DATA *context = ch->in_room ? ch->in_room->area : pObj->area;

                if (parse_widevnum(argument, context, &dng_wnum) && dng_wnum.pArea != NULL)
                    dng = get_dungeon_index_for_area(dng_wnum.pArea, dng_wnum.vnum);
                else if (is_number(argument))
                    dng = get_dungeon_index(atol(argument));

                if (!dng) {
                    send_to_char("There is no such dungeon.\n\r", ch);
                    return false;
                }

                long new_p0 = dng->vnum;
                long new_p4 = dng->area ? dng->area->uid : 0;
                long new_p1 = PORTAL(pObj)->params[1] < 1 ? 1 : PORTAL(pObj)->params[1];

                if (cs) {
                    if (!olc_check_staging_limits(ch, cs)) return false;
                    json_t *jo0 = json_integer((json_int_t)PORTAL(pObj)->params[0]);
                    json_t *jn0 = json_integer((json_int_t)new_p0);
                    olc_changeset_add_change(cs, "typedata/portal/param0", OLC_FIELD_TYPE_DATA, jo0, jn0);
                    json_decref(jo0); json_decref(jn0);

                    json_t *jo4 = json_integer((json_int_t)PORTAL(pObj)->params[4]);
                    json_t *jn4 = json_integer((json_int_t)new_p4);
                    olc_changeset_add_change(cs, "typedata/portal/param4", OLC_FIELD_TYPE_DATA, jo4, jn4);
                    json_decref(jo4); json_decref(jn4);

                    json_t *jo1 = json_integer((json_int_t)PORTAL(pObj)->params[1]);
                    json_t *jn1 = json_integer((json_int_t)new_p1);
                    olc_changeset_add_change(cs, "typedata/portal/param1", OLC_FIELD_TYPE_DATA, jo1, jn1);
                    notify_field_change(cs, ch, "typedata/portal/param0", jn0, "type_data", true);
                    json_decref(jo1); json_decref(jn1);

                    send_to_char("{G[STAGED]{x Dungeon destination set.\n\r", ch);
                } else {
                    PORTAL(pObj)->params[0] = new_p0;
                    PORTAL(pObj)->params[4] = new_p4;
                    PORTAL(pObj)->params[1] = new_p1;
                    send_to_char("Dungeon destination set.\n\r", ch);
                }
                return true;
            }

            if (!str_cmp(argument, "-1")) {
                if (cs) {
                    if (!olc_check_staging_limits(ch, cs)) return false;
                    json_t *jo0 = json_integer((json_int_t)PORTAL(pObj)->params[0]);
                    json_t *jn0 = json_integer((json_int_t)-1);
                    olc_changeset_add_change(cs, "typedata/portal/param0", OLC_FIELD_TYPE_DATA, jo0, jn0);
                    json_decref(jo0); json_decref(jn0);

                    json_t *jo4 = json_integer((json_int_t)PORTAL(pObj)->params[4]);
                    json_t *jn4 = json_integer(0);
                    olc_changeset_add_change(cs, "typedata/portal/param4", OLC_FIELD_TYPE_DATA, jo4, jn4);
                    notify_field_change(cs, ch, "typedata/portal/param0", jn0, "type_data", true);
                    json_decref(jo4); json_decref(jn4);

                    send_to_char("{G[STAGED]{x Destination mode set to area random.\n\r", ch);
                } else {
                    PORTAL(pObj)->params[0] = -1;
                    PORTAL(pObj)->params[4] = 0;
                    send_to_char("Destination mode set to area random.\n\r", ch);
                }
                return true;
            }

            {
                AREA_DATA *context = ch->in_room ? ch->in_room->area : NULL;
                WNUM dest_wnum = { NULL, 0 };

                if (!parse_widevnum(argument, context, &dest_wnum) || dest_wnum.pArea == NULL
                    || get_room_index(dest_wnum.pArea, dest_wnum.vnum) == NULL) {
                    send_to_char("No such destination room.\n\r", ch);
                    return false;
                }

                long new_p0 = dest_wnum.vnum;
                long new_p4 = dest_wnum.pArea->uid;

                if (cs) {
                    if (!olc_check_staging_limits(ch, cs)) return false;
                    json_t *jo0 = json_integer((json_int_t)PORTAL(pObj)->params[0]);
                    json_t *jn0 = json_integer((json_int_t)new_p0);
                    olc_changeset_add_change(cs, "typedata/portal/param0", OLC_FIELD_TYPE_DATA, jo0, jn0);
                    json_decref(jo0); json_decref(jn0);

                    json_t *jo1 = json_integer((json_int_t)PORTAL(pObj)->params[1]);
                    json_t *jn1 = json_integer(0);
                    olc_changeset_add_change(cs, "typedata/portal/param1", OLC_FIELD_TYPE_DATA, jo1, jn1);
                    json_decref(jo1); json_decref(jn1);

                    json_t *jo4 = json_integer((json_int_t)PORTAL(pObj)->params[4]);
                    json_t *jn4 = json_integer((json_int_t)new_p4);
                    olc_changeset_add_change(cs, "typedata/portal/param4", OLC_FIELD_TYPE_DATA, jo4, jn4);
                    notify_field_change(cs, ch, "typedata/portal/param0", jn0, "type_data", true);
                    json_decref(jo4); json_decref(jn4);

                    send_to_char("{G[STAGED]{x Destination room set.\n\r", ch);
                } else {
                    PORTAL(pObj)->params[0] = new_p0;
                    PORTAL(pObj)->params[1] = 0;
                    PORTAL(pObj)->params[4] = new_p4;
                    send_to_char("Destination room set.\n\r", ch);
                }
            }
            return true;
        }
        if (!str_prefix(field, "param1")) {
            if (argument[0] == '\0') { send_to_char("Syntax: portal param1 <value> (floor/area/map_uid)\n\r", ch); return false; }
            long val = atol(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer((json_int_t)PORTAL(pObj)->params[1]);
                json_t *jnew = json_integer((json_int_t)val);
                olc_changeset_add_change(cs, "typedata/portal/param1", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/portal/param1", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                if (IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON))
                    send_to_char("{G[STAGED]{x Dungeon floor set.\n\r", ch);
                else if (IS_SET(PORTAL(pObj)->flags, GATE_AREARANDOM) || PORTAL(pObj)->params[0] == -1)
                    send_to_char("{G[STAGED]{x Area ID set.\n\r", ch);
                else
                    send_to_char("{G[STAGED]{x Wilderness map UID set.\n\r", ch);
            } else {
                PORTAL(pObj)->params[1] = val;
                if (IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON))
                    send_to_char("Dungeon floor set.\n\r", ch);
                else if (IS_SET(PORTAL(pObj)->flags, GATE_AREARANDOM) || PORTAL(pObj)->params[0] == -1)
                    send_to_char("Area ID set.\n\r", ch);
                else
                    send_to_char("Wilderness map UID set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "param2")) {
            if (argument[0] == '\0') { send_to_char("Syntax: portal param2 <value> (map_x)\n\r", ch); return false; }
            if (!IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON) && !IS_SET(PORTAL(pObj)->flags, GATE_AREARANDOM)
                && PORTAL(pObj)->params[0] <= 0) {
                long val = atol(argument);
                if (cs) {
                    if (!olc_check_staging_limits(ch, cs)) return false;
                    json_t *jold = json_integer((json_int_t)PORTAL(pObj)->params[2]);
                    json_t *jnew = json_integer((json_int_t)val);
                    olc_changeset_add_change(cs, "typedata/portal/param2", OLC_FIELD_TYPE_DATA, jold, jnew);
                    notify_field_change(cs, ch, "typedata/portal/param2", jnew, "type_data", true);
                    json_decref(jold); json_decref(jnew);
                    send_to_char("{G[STAGED]{x Wilderness map X set.\n\r", ch);
                } else {
                    PORTAL(pObj)->params[2] = val;
                    send_to_char("Wilderness map X set.\n\r", ch);
                }
                return true;
            }
            send_to_char("This field only applies to wilderness portals.\n\r", ch);
            return false;
        }
        if (!str_prefix(field, "param3")) {
            if (argument[0] == '\0') { send_to_char("Syntax: portal param3 <value> (map_y)\n\r", ch); return false; }
            if (!IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON) && !IS_SET(PORTAL(pObj)->flags, GATE_AREARANDOM)
                && PORTAL(pObj)->params[0] <= 0) {
                long val = atol(argument);
                if (cs) {
                    if (!olc_check_staging_limits(ch, cs)) return false;
                    json_t *jold = json_integer((json_int_t)PORTAL(pObj)->params[3]);
                    json_t *jnew = json_integer((json_int_t)val);
                    olc_changeset_add_change(cs, "typedata/portal/param3", OLC_FIELD_TYPE_DATA, jold, jnew);
                    notify_field_change(cs, ch, "typedata/portal/param3", jnew, "type_data", true);
                    json_decref(jold); json_decref(jnew);
                    send_to_char("{G[STAGED]{x Wilderness map Y set.\n\r", ch);
                } else {
                    PORTAL(pObj)->params[3] = val;
                    send_to_char("Wilderness map Y set.\n\r", ch);
                }
                return true;
            }
            send_to_char("This field only applies to wilderness portals.\n\r", ch);
            return false;
        }
        send_to_char("Valid fields: charges, exit, flags, destination, param1, param2, param3\n\r", ch);
        return false;
    }

    /* Contextual display based on portal flags */
    if (IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON)) {
        AREA_DATA *dng_area = PORTAL(pObj)->params[4] > 0 ? get_area_index(PORTAL(pObj)->params[4]) : NULL;
        const char *dng_dest = (PORTAL(pObj)->params[0] > 0)
            ? widevnum_string(dng_area, PORTAL(pObj)->params[0], pObj->area)
            : "none";

        sprintf(buf,
            "{WPortal (Dungeon):{x\n\r"
            "  {Gcharges      {x [%d]\n\r"
            "  {Gexit         {x %s\n\r"
            "  {Gflags        {x %s\n\r"
            "  {Gdestination  {x [%s] (dungeon)\n\r"
            "  {Gparam1       {x [%ld] (floor)\n\r",
            PORTAL(pObj)->charges,
            flag_string(portal_exit_flags, PORTAL(pObj)->exit),
            flag_string(portal_flags, PORTAL(pObj)->flags),
            dng_dest,
            PORTAL(pObj)->params[1]);
    } else if (IS_SET(PORTAL(pObj)->flags, GATE_AREARANDOM) || PORTAL(pObj)->params[0] == -1) {
        sprintf(buf,
            "{WPortal (Area Random):{x\n\r"
            "  {Gcharges      {x [%d]\n\r"
            "  {Gexit         {x %s\n\r"
            "  {Gflags        {x %s\n\r"
            "  {Gparam1       {x [%ld] (area id)\n\r",
            PORTAL(pObj)->charges,
            flag_string(portal_exit_flags, PORTAL(pObj)->exit),
            flag_string(portal_flags, PORTAL(pObj)->flags),
            PORTAL(pObj)->params[1]);
    } else if (PORTAL(pObj)->params[0] > 0) {
        sprintf(buf,
            "{WPortal (Static):{x\n\r"
            "  {Gcharges      {x [%d]\n\r"
            "  {Gexit         {x %s\n\r"
            "  {Gflags        {x %s\n\r"
            "  {Gdestination  {x [%s]\n\r",
            PORTAL(pObj)->charges,
            flag_string(portal_exit_flags, PORTAL(pObj)->exit),
            flag_string(portal_flags, PORTAL(pObj)->flags),
            widevnum_string(
                PORTAL(pObj)->params[4] > 0 ? get_area_index(PORTAL(pObj)->params[4]) : NULL,
                PORTAL(pObj)->params[0], pObj->area));
    } else {
        sprintf(buf,
            "{WPortal (Wilderness):{x\n\r"
            "  {Gcharges      {x [%d]\n\r"
            "  {Gexit         {x %s\n\r"
            "  {Gflags        {x %s\n\r"
            "  {Gparam1       {x [%ld] (map uid)\n\r"
            "  {Gparam2       {x [%ld] (map x)\n\r"
            "  {Gparam3       {x [%ld] (map y)\n\r",
            PORTAL(pObj)->charges,
            flag_string(portal_exit_flags, PORTAL(pObj)->exit),
            flag_string(portal_flags, PORTAL(pObj)->flags),
            PORTAL(pObj)->params[1],
            PORTAL(pObj)->params[2],
            PORTAL(pObj)->params[3]);
    }
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  SCROLL
 * ============================================================================ */
OEDIT(oedit_scroll)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_SCROLL(pObj)) {
        send_to_char("Object lacks the scroll type. Use '{Waddtype scroll{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "mana")) {
            if (argument[0] == '\0') { send_to_char("Syntax: scroll mana <max_mana>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(SCROLL(pObj)->max_mana);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/scroll/mana", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/scroll/mana", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Scroll max mana set.\n\r", ch);
            } else {
                SCROLL(pObj)->max_mana = val;
                send_to_char("Scroll max mana set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: scroll flags <flag>\n\r", ch); return false; }
            long toggle = flag_value(scroll_flags, argument);
            if (toggle == NO_FLAG) { send_to_char("Invalid scroll flag.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                long current = olc_staged_flags_or(cs, "typedata/scroll/flags", SCROLL(pObj)->flags);
                long result = current ^ toggle;
                json_t *jold = json_integer((json_int_t)SCROLL(pObj)->flags);
                json_t *jnew = json_integer((json_int_t)result);
                olc_changeset_add_change(cs, "typedata/scroll/flags", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/scroll/flags", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Scroll flags toggled.\n\r", ch);
            } else {
                SCROLL(pObj)->flags ^= toggle;
                send_to_char("Scroll flags toggled.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: mana, flags\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WScroll:{x\n\r"
        "  {Gmana          {x [%d]\n\r"
        "  {Gflags         {x [%s]\n\r",
        SCROLL(pObj)->max_mana,
        flag_string(scroll_flags, SCROLL(pObj)->flags));
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  SEED
 * ============================================================================ */
OEDIT(oedit_seed)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_SEED(pObj)) {
        send_to_char("Object lacks the seed type. Use '{Waddtype seed{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "time")) {
            if (argument[0] == '\0') { send_to_char("Syntax: seed time <growth_time>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(SEED(pObj)->growth_time);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/seed/time", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/seed/time", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Growth time set.\n\r", ch);
            } else {
                SEED(pObj)->growth_time = val;
                send_to_char("Growth time set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "object")) {
            if (argument[0] == '\0') { send_to_char("Syntax: seed object <vnum|0>\n\r", ch); return false; }
            long new_vnum = 0;
            long new_auid = 0;
            if (atoi(argument) != 0) {
                WNUM key_wnum = { NULL, 0 };
                OBJ_INDEX_DATA *key_obj;
                parse_widevnum(argument, ch->in_room ? ch->in_room->area : NULL, &key_wnum);
                key_obj = key_wnum.pArea ? get_obj_index(key_wnum.pArea, key_wnum.vnum) : get_obj_index_global(key_wnum.vnum);
                if (!key_obj) {
                    send_to_char("No such object exists.\n\r", ch);
                    return false;
                }
                new_vnum = key_wnum.vnum;
                new_auid = key_obj->area ? key_obj->area->uid : 0;
            }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_pack("{s:I, s:I}",
                    "auid", (json_int_t)SEED(pObj)->object_area_uid,
                    "vnum", (json_int_t)SEED(pObj)->object_vnum);
                json_t *jnew = json_pack("{s:I, s:I}",
                    "auid", (json_int_t)new_auid,
                    "vnum", (json_int_t)new_vnum);
                olc_changeset_add_change(cs, "typedata/seed/object", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/seed/object", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Seed object set.\n\r", ch);
            } else {
                SEED(pObj)->object_vnum = new_vnum;
                SEED(pObj)->object_area_uid = new_auid;
                send_to_char("Seed object set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: time, object\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WSeed:{x\n\r"
        "  {Gtime          {x [%d]\n\r"
        "  {Gobject        {x %s\n\r",
        SEED(pObj)->growth_time,
        widevnum_string(SEED(pObj)->object_area_uid > 0
            ? get_area_index(SEED(pObj)->object_area_uid) : NULL,
            SEED(pObj)->object_vnum, pObj->area));
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  SEXTANT
 * ============================================================================ */
OEDIT(oedit_sextant)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_SEXTANT(pObj)) {
        send_to_char("Object lacks the sextant type. Use '{Waddtype sextant{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "accuracy")) {
            if (argument[0] == '\0') { send_to_char("Syntax: sextant accuracy <percent>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(SEXTANT(pObj)->accuracy);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/sextant/accuracy", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/sextant/accuracy", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Sextant accuracy set.\n\r", ch);
            } else {
                SEXTANT(pObj)->accuracy = val;
                send_to_char("Sextant accuracy set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: accuracy\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WSextant:{x\n\r"
        "  {Gaccuracy      {x [%d%%]\n\r",
        SEXTANT(pObj)->accuracy);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  SHIP
 * ============================================================================ */
OEDIT(oedit_ship)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_SHIP_TYPE(pObj)) {
        send_to_char("Object lacks the ship type. Use '{Waddtype ship{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "weight")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ship weight <kg>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(SHIP_TYPE(pObj)->weight);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/ship/weight", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/ship/weight", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Ship weight set.\n\r", ch);
            } else {
                SHIP_TYPE(pObj)->weight = val;
                send_to_char("Ship weight set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "delay")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ship delay <ticks>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(SHIP_TYPE(pObj)->move_delay);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/ship/delay", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/ship/delay", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Ship move delay set.\n\r", ch);
            } else {
                SHIP_TYPE(pObj)->move_delay = val;
                send_to_char("Ship move delay set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "mincrew")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ship mincrew <count>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(SHIP_TYPE(pObj)->min_crew);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/ship/mincrew", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/ship/mincrew", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Ship min crew set.\n\r", ch);
            } else {
                SHIP_TYPE(pObj)->min_crew = val;
                send_to_char("Ship min crew set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "capacity")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ship capacity <value>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(SHIP_TYPE(pObj)->capacity);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/ship/capacity", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/ship/capacity", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Ship capacity set.\n\r", ch);
            } else {
                SHIP_TYPE(pObj)->capacity = val;
                send_to_char("Ship capacity set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "maxcrew")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ship maxcrew <count>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(SHIP_TYPE(pObj)->max_crew);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/ship/maxcrew", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/ship/maxcrew", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Ship max crew set.\n\r", ch);
            } else {
                SHIP_TYPE(pObj)->max_crew = val;
                send_to_char("Ship max crew set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "room")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ship room <room_vnum|0>\n\r", ch); return false; }
            long new_vnum = 0;
            long new_auid = 0;
            if (atol(argument) != 0) {
                WNUM key_wnum = { NULL, 0 };
                ROOM_INDEX_DATA *key_room;
                parse_widevnum(argument, ch->in_room ? ch->in_room->area : NULL, &key_wnum);
                key_room = key_wnum.pArea ? get_room_index(key_wnum.pArea, key_wnum.vnum) : get_room_index_global(key_wnum.vnum);
                if (!key_room) {
                    send_to_char("No such room exists.\n\r", ch);
                    return false;
                }
                new_vnum = key_wnum.vnum;
                new_auid = key_room->area ? key_room->area->uid : 0;
            }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_pack("{s:I, s:I}",
                    "auid", (json_int_t)SHIP_TYPE(pObj)->first_room_area_uid,
                    "vnum", (json_int_t)SHIP_TYPE(pObj)->first_room);
                json_t *jnew = json_pack("{s:I, s:I}",
                    "auid", (json_int_t)new_auid,
                    "vnum", (json_int_t)new_vnum);
                olc_changeset_add_change(cs, "typedata/ship/room", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/ship/room", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Ship first room set.\n\r", ch);
            } else {
                SHIP_TYPE(pObj)->first_room = new_vnum;
                SHIP_TYPE(pObj)->first_room_area_uid = new_auid;
                send_to_char("Ship first room set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "hitpoints")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ship hitpoints <hp>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(SHIP_TYPE(pObj)->hit_points);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/ship/hitpoints", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/ship/hitpoints", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Ship hit points set.\n\r", ch);
            } else {
                SHIP_TYPE(pObj)->hit_points = val;
                send_to_char("Ship hit points set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "guns")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ship guns <max>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(SHIP_TYPE(pObj)->max_guns);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/ship/guns", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/ship/guns", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Ship max guns set.\n\r", ch);
            } else {
                SHIP_TYPE(pObj)->max_guns = val;
                send_to_char("Ship max guns set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: weight, delay, mincrew, capacity, maxcrew, room, hitpoints, guns\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WShip:{x\n\r"
        "  {Gweight        {x [%d kg]\n\r"
        "  {Gdelay         {x [%d]\n\r"
        "  {Gmincrew       {x [%d]\n\r"
        "  {Gcapacity      {x [%d]\n\r"
        "  {Gmaxcrew       {x [%d]\n\r"
        "  {Groom          {x %s\n\r"
        "  {Ghitpoints     {x [%d]\n\r"
        "  {Gguns          {x [%d]\n\r",
        SHIP_TYPE(pObj)->weight,
        SHIP_TYPE(pObj)->move_delay,
        SHIP_TYPE(pObj)->min_crew,
        SHIP_TYPE(pObj)->capacity,
        SHIP_TYPE(pObj)->max_crew,
        widevnum_string(SHIP_TYPE(pObj)->first_room_area_uid > 0
            ? get_area_index(SHIP_TYPE(pObj)->first_room_area_uid) : NULL,
            SHIP_TYPE(pObj)->first_room, pObj->area),
        SHIP_TYPE(pObj)->hit_points,
        SHIP_TYPE(pObj)->max_guns);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  SHIP MODULE
 * ============================================================================ */
OEDIT(oedit_shipmodule)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_SHIP_MODULE(pObj)) {
        send_to_char("Object lacks the ship module type. Use '{Waddtype shipmodule{x'.\n\r", ch);
        return false;
    }

    SHIP_MODULE_DATA *d = SHIP_MODULE_TYPE(pObj);

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "type")) {
            if (argument[0] == '\0') { send_to_char("Syntax: shipmodule type <weapon|defense|utility|propulsion>\n\r", ch); return false; }
            int val = flag_value(hardpoint_types, argument);
            if (val == NO_FLAG) { send_to_char("Invalid hardpoint type.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(d->type);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/shipmodule/type", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/shipmodule/type", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Module type set.\n\r", ch);
            } else {
                d->type = val;
                send_to_char("Module type set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "size")) {
            if (argument[0] == '\0') { send_to_char("Syntax: shipmodule size <small|medium|large>\n\r", ch); return false; }
            int val = flag_value(hardpoint_sizes, argument);
            if (val == NO_FLAG) { send_to_char("Invalid hardpoint size.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(d->size);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/shipmodule/size", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/shipmodule/size", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Module size set.\n\r", ch);
            } else {
                d->size = val;
                send_to_char("Module size set.\n\r", ch);
            }
            return true;
        }

/* Macro for simple int fields on shipmodule */
#define SM_INT_FIELD(prefix, member, label) \
        if (!str_prefix(field, prefix)) { \
            if (argument[0] == '\0') { send_to_char("Syntax: shipmodule " prefix " <value>\n\r", ch); return false; } \
            int val = atoi(argument); \
            if (cs) { \
                if (!olc_check_staging_limits(ch, cs)) return false; \
                json_t *jold = json_integer(d->member); \
                json_t *jnew = json_integer(val); \
                olc_changeset_add_change(cs, "typedata/shipmodule/" prefix, OLC_FIELD_TYPE_DATA, jold, jnew); \
                notify_field_change(cs, ch, "typedata/shipmodule/" prefix, jnew, "type_data", true); \
                json_decref(jold); json_decref(jnew); \
                send_to_char("{G[STAGED]{x " label " set.\n\r", ch); \
            } else { \
                d->member = val; \
                send_to_char(label " set.\n\r", ch); \
            } \
            return true; \
        }

        SM_INT_FIELD("weight",        weight,              "Module weight")
        SM_INT_FIELD("hitbonus",      hit_bonus,           "Hit bonus")
        SM_INT_FIELD("armorbonus",    armor_bonus,         "Armor bonus")
        SM_INT_FIELD("speedbonus",    speed_bonus,         "Speed bonus")
        SM_INT_FIELD("turningbonus",  turning_bonus,       "Turning bonus")
        SM_INT_FIELD("cargoweight",   cargo_weight_bonus,  "Cargo weight bonus")
        SM_INT_FIELD("cargocapacity", cargo_capacity_bonus,"Cargo capacity bonus")
        SM_INT_FIELD("crewbonus",     crew_bonus,          "Crew bonus")
        SM_INT_FIELD("damage",        damage,              "Damage")
        SM_INT_FIELD("range",         range,               "Range")
        SM_INT_FIELD("reload",        reload_time,         "Reload time")
        SM_INT_FIELD("damagetype",    damage_type,         "Damage type")
        SM_INT_FIELD("ammoshot",      ammo_per_shot,       "Ammo per shot")

/* Macro for int16_t fields on shipmodule */
#define SM_I16_FIELD(prefix, member, label) \
        if (!str_prefix(field, prefix)) { \
            if (argument[0] == '\0') { send_to_char("Syntax: shipmodule " prefix " <value>\n\r", ch); return false; } \
            int val = atoi(argument); \
            if (cs) { \
                if (!olc_check_staging_limits(ch, cs)) return false; \
                json_t *jold = json_integer((int)d->member); \
                json_t *jnew = json_integer(val); \
                olc_changeset_add_change(cs, "typedata/shipmodule/" prefix, OLC_FIELD_TYPE_DATA, jold, jnew); \
                notify_field_change(cs, ch, "typedata/shipmodule/" prefix, jnew, "type_data", true); \
                json_decref(jold); json_decref(jnew); \
                send_to_char("{G[STAGED]{x " label " set.\n\r", ch); \
            } else { \
                d->member = (int16_t)val; \
                send_to_char(label " set.\n\r", ch); \
            } \
            return true; \
        }

        SM_I16_FIELD("operators",  operators,      "Operators")
        SM_I16_FIELD("gunning",    req_gunning,    "Required gunning")
        SM_I16_FIELD("mechanics",  req_mechanics,  "Required mechanics")
        SM_I16_FIELD("scouting",   req_scouting,   "Required scouting")
        SM_I16_FIELD("navigation", req_navigation, "Required navigation")
        SM_I16_FIELD("oarring",    req_oarring,    "Required oarring")
        SM_I16_FIELD("leadership", req_leadership, "Required leadership")

#undef SM_INT_FIELD
#undef SM_I16_FIELD

        if (!str_prefix(field, "domain")) {
            if (argument[0] == '\0') { send_to_char("Syntax: shipmodule domain <aquatic|aerial|terrestrial>\n\r", ch); return false; }
            int toggle = flag_value(domain_flags, argument);
            if (toggle == NO_FLAG) { send_to_char("Invalid domain flag.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                long current = olc_staged_flags_or(cs, "typedata/shipmodule/domain", d->domain_flags);
                long result = current ^ toggle;
                json_t *jold = json_integer((json_int_t)d->domain_flags);
                json_t *jnew = json_integer((json_int_t)result);
                olc_changeset_add_change(cs, "typedata/shipmodule/domain", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/shipmodule/domain", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Module domain flags toggled.\n\r", ch);
            } else {
                TOGGLE_BIT(d->domain_flags, toggle);
                send_to_char("Module domain flags toggled.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "weapflags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: shipmodule weapflags <flag>\n\r", ch); return false; }
            int toggle = flag_value(weapon_module_flags, argument);
            if (toggle == NO_FLAG) { send_to_char("Invalid weapon module flag.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                long current = olc_staged_flags_or(cs, "typedata/shipmodule/weapflags", d->weapon_flags);
                long result = current ^ toggle;
                json_t *jold = json_integer((json_int_t)d->weapon_flags);
                json_t *jnew = json_integer((json_int_t)result);
                olc_changeset_add_change(cs, "typedata/shipmodule/weapflags", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/shipmodule/weapflags", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Weapon flags toggled.\n\r", ch);
            } else {
                TOGGLE_BIT(d->weapon_flags, toggle);
                send_to_char("Weapon flags toggled.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: shipmodule flags <flag>\n\r", ch); return false; }
            int toggle = flag_value(module_flags, argument);
            if (toggle == NO_FLAG) { send_to_char("Invalid module flag.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                long current = olc_staged_flags_or(cs, "typedata/shipmodule/flags", d->flags);
                long result = current ^ toggle;
                json_t *jold = json_integer((json_int_t)d->flags);
                json_t *jnew = json_integer((json_int_t)result);
                olc_changeset_add_change(cs, "typedata/shipmodule/flags", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/shipmodule/flags", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Module flags toggled.\n\r", ch);
            } else {
                TOGGLE_BIT(d->flags, toggle);
                send_to_char("Module flags toggled.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "ammo")) {
            if (argument[0] == '\0') { send_to_char("Syntax: shipmodule ammo <obj_vnum|0 for none>\n\r", ch); return false; }
            long new_vnum = 0;
            long new_auid = 0;
            long vnum = atol(argument);
            if (vnum != 0) {
                WNUM key_wnum = { NULL, 0 };
                parse_widevnum(argument, pObj->area, &key_wnum);
                OBJ_INDEX_DATA *ammo_obj = key_wnum.pArea
                    ? get_obj_index(key_wnum.pArea, key_wnum.vnum)
                    : get_obj_index_global(key_wnum.vnum);
                if (!ammo_obj) {
                    send_to_char("No such object exists.\n\r", ch);
                    return false;
                }
                new_vnum = key_wnum.vnum;
                new_auid = key_wnum.pArea ? key_wnum.pArea->uid : 0;
            }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_pack("{s:I, s:I}",
                    "auid", (json_int_t)d->ammo_ref.load.auid,
                    "vnum", (json_int_t)d->ammo_ref.load.vnum);
                json_t *jnew = json_pack("{s:I, s:I}",
                    "auid", (json_int_t)new_auid,
                    "vnum", (json_int_t)new_vnum);
                olc_changeset_add_change(cs, "typedata/shipmodule/ammo", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/shipmodule/ammo", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char(new_vnum > 0 ? "{G[STAGED]{x Ammo object set.\n\r" : "{G[STAGED]{x Ammo cleared.\n\r", ch);
            } else {
                if (new_vnum > 0) {
                    d->ammo_ref.load.vnum = new_vnum;
                    d->ammo_ref.load.auid = new_auid;
                    AREA_DATA *area = new_auid > 0 ? get_area_index(new_auid) : NULL;
                    d->ammo = area ? get_obj_index(area, new_vnum) : get_obj_index_global(new_vnum);
                } else {
                    d->ammo_ref.load.vnum = 0;
                    d->ammo_ref.load.auid = 0;
                    d->ammo = NULL;
                }
                send_to_char(new_vnum > 0 ? "Ammo object set.\n\r" : "Ammo cleared.\n\r", ch);
            }
            return true;
        }
        send_to_char(
            "Valid fields: type, size, weight, domain, hitbonus, armorbonus,\n\r"
            "  speedbonus, turningbonus, cargoweight, cargocapacity, crewbonus,\n\r"
            "  damage, range, reload, damagetype, weapflags, operators,\n\r"
            "  gunning, mechanics, scouting, navigation, oarring, leadership,\n\r"
            "  ammo, ammoshot, flags\n\r", ch);
        return false;
    }

    /* Show mode */
    sprintf(buf,
        "{WShip Module:{x\n\r"
        "  {Gtype          {x [%s]\n\r"
        "  {Gsize          {x [%s]\n\r"
        "  {Gweight        {x [%d]\n\r"
        "  {Gdomain        {x [%s]\n\r"
        "  {Gflags         {x [%s]\n\r",
        flag_string(hardpoint_types, d->type),
        flag_string(hardpoint_sizes, d->size),
        d->weight,
        flag_string(domain_flags, d->domain_flags),
        flag_string(module_flags, d->flags));
    send_to_char(buf, ch);

    sprintf(buf,
        "  {GBonuses:{x\n\r"
        "    {Ghitbonus      {x [%d]  {Garmorbonus   {x [%d]\n\r"
        "    {Gspeedbonus    {x [%d%%] {Gturningbonus {x [%d]\n\r"
        "    {Gcargoweight   {x [%d]  {Gcargocapacity{x [%d]\n\r"
        "    {Gcrewbonus     {x [%d]\n\r",
        d->hit_bonus, d->armor_bonus,
        d->speed_bonus, d->turning_bonus,
        d->cargo_weight_bonus, d->cargo_capacity_bonus,
        d->crew_bonus);
    send_to_char(buf, ch);

    sprintf(buf,
        "  {GWeapon:{x\n\r"
        "    {Gdamage       {x [%d]  {Grange        {x [%d]\n\r"
        "    {Greload       {x [%d]  {Gdamagetype   {x [%d]\n\r"
        "    {Gweapflags    {x [%s]\n\r",
        d->damage, d->range,
        d->reload_time, d->damage_type,
        flag_string(weapon_module_flags, d->weapon_flags));
    send_to_char(buf, ch);

    sprintf(buf,
        "  {GCrew Requirements:{x\n\r"
        "    {Goperators    {x [%d]\n\r"
        "    {Ggunning      {x [%d]  {Gmechanics    {x [%d]\n\r"
        "    {Gscouting     {x [%d]  {Gnavigation   {x [%d]\n\r"
        "    {Goarring      {x [%d]  {Gleadership   {x [%d]\n\r",
        d->operators,
        d->req_gunning, d->req_mechanics,
        d->req_scouting, d->req_navigation,
        d->req_oarring, d->req_leadership);
    send_to_char(buf, ch);

    sprintf(buf,
        "  {GAmmo:{x\n\r"
        "    {Gammo         {x [%s]\n\r"
        "    {Gammoshot     {x [%d]\n\r",
        d->ammo ? d->ammo->short_descr : "(none)",
        d->ammo_per_shot);
    send_to_char(buf, ch);

    return false;
}

/* ============================================================================
 *  TATTOO
 * ============================================================================ */
OEDIT(oedit_tattoo)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_TATTOO(pObj)) {
        send_to_char("Object lacks the tattoo type. Use '{Waddtype tattoo{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "touches")) {
            if (argument[0] == '\0') { send_to_char("Syntax: tattoo touches <count>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(TATTOO(pObj)->touches);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/tattoo/touches", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/tattoo/touches", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Tattoo touches set.\n\r", ch);
            } else {
                TATTOO(pObj)->touches = val;
                send_to_char("Tattoo touches set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "fading")) {
            if (argument[0] == '\0') { send_to_char("Syntax: tattoo fading <percent>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(TATTOO(pObj)->fading_chance);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/tattoo/fading", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/tattoo/fading", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Tattoo fading chance set.\n\r", ch);
            } else {
                TATTOO(pObj)->fading_chance = val;
                send_to_char("Tattoo fading chance set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "faderate")) {
            if (argument[0] == '\0') { send_to_char("Syntax: tattoo faderate <rate>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(TATTOO(pObj)->fading_rate);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/tattoo/faderate", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/tattoo/faderate", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Tattoo fading rate set.\n\r", ch);
            } else {
                TATTOO(pObj)->fading_rate = val;
                send_to_char("Tattoo fading rate set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: touches, fading, faderate\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WTattoo:{x\n\r"
        "  {Gtouches       {x [%d]\n\r"
        "  {Gfading        {x [%d%%]\n\r"
        "  {Gfaderate      {x [%d]\n\r",
        TATTOO(pObj)->touches,
        TATTOO(pObj)->fading_chance,
        TATTOO(pObj)->fading_rate);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  TELESCOPE
 * ============================================================================ */
OEDIT(oedit_telescope)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_TELESCOPE(pObj)) {
        send_to_char("Object lacks the telescope type. Use '{Waddtype telescope{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "distance")) {
            if (argument[0] == '\0') { send_to_char("Syntax: telescope distance <val>\n\r", ch); return false; }
            int val = atoi(argument);
            if (val < 0 || (val > 0 && val < TELESCOPE(pObj)->min_distance) || val > TELESCOPE(pObj)->max_distance) {
                char msg[MSL];
                sprintf(msg, "Distance must be 0 (collapsed), or from %d to %d.\n\r",
                    TELESCOPE(pObj)->min_distance, TELESCOPE(pObj)->max_distance);
                send_to_char(msg, ch);
                return false;
            }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer((int)TELESCOPE(pObj)->distance);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/telescope/distance", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/telescope/distance", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Telescope distance set.\n\r", ch);
            } else {
                TELESCOPE(pObj)->distance = val;
                send_to_char("Telescope distance set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "mindist")) {
            if (argument[0] == '\0') { send_to_char("Syntax: telescope mindist <val>\n\r", ch); return false; }
            int val = atoi(argument);
            if (val <= 0) { send_to_char("Minimum distance must be greater than zero.\n\r", ch); return false; }
            if (val > TELESCOPE(pObj)->max_distance) {
                send_to_char("Must be less than or equal to max distance.\n\r", ch);
                return false;
            }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer((int)TELESCOPE(pObj)->min_distance);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/telescope/mindist", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/telescope/mindist", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Telescope min distance set.\n\r", ch);
            } else {
                TELESCOPE(pObj)->min_distance = val;
                send_to_char("Telescope min distance set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "maxdist")) {
            if (argument[0] == '\0') { send_to_char("Syntax: telescope maxdist <val>\n\r", ch); return false; }
            int val = atoi(argument);
            if (val <= 0) { send_to_char("Maximum distance must be greater than zero.\n\r", ch); return false; }
            if (val < TELESCOPE(pObj)->min_distance) {
                send_to_char("Must be greater than or equal to min distance.\n\r", ch);
                return false;
            }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer((int)TELESCOPE(pObj)->max_distance);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/telescope/maxdist", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/telescope/maxdist", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Telescope max distance set.\n\r", ch);
            } else {
                TELESCOPE(pObj)->max_distance = val;
                send_to_char("Telescope max distance set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "bonus")) {
            if (argument[0] == '\0') { send_to_char("Syntax: telescope bonus <view_size>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer((int)TELESCOPE(pObj)->bonus_view);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/telescope/bonus", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/telescope/bonus", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Telescope bonus view set.\n\r", ch);
            } else {
                TELESCOPE(pObj)->bonus_view = val;
                send_to_char("Telescope bonus view set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "heading")) {
            if (argument[0] == '\0') { send_to_char("Syntax: telescope heading <dir|-1 for none>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer((int)TELESCOPE(pObj)->heading);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/telescope/heading", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/telescope/heading", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Telescope heading set.\n\r", ch);
            } else {
                TELESCOPE(pObj)->heading = val;
                send_to_char("Telescope heading set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: distance, mindist, maxdist, bonus, heading\n\r", ch);
        return false;
    }

    if (TELESCOPE(pObj)->heading >= 0) {
        sprintf(buf,
            "{WTelescope:{x\n\r"
            "  {Gdistance      {x [%d]\n\r"
            "  {Gmindist       {x [%d]\n\r"
            "  {Gmaxdist       {x [%d]\n\r"
            "  {Gbonus         {x [%d]\n\r"
            "  {Gheading       {x [%d]\n\r",
            TELESCOPE(pObj)->distance,
            TELESCOPE(pObj)->min_distance,
            TELESCOPE(pObj)->max_distance,
            TELESCOPE(pObj)->bonus_view,
            TELESCOPE(pObj)->heading);
    } else {
        sprintf(buf,
            "{WTelescope:{x\n\r"
            "  {Gdistance      {x [%d]\n\r"
            "  {Gmindist       {x [%d]\n\r"
            "  {Gmaxdist       {x [%d]\n\r"
            "  {Gbonus         {x [%d]\n\r"
            "  {Gheading       {x [none]\n\r",
            TELESCOPE(pObj)->distance,
            TELESCOPE(pObj)->min_distance,
            TELESCOPE(pObj)->max_distance,
            TELESCOPE(pObj)->bonus_view);
    }
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  TOOL
 * ============================================================================ */
OEDIT(oedit_tool)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_TOOL(pObj)) {
        send_to_char("Object lacks the tool type. Use '{Waddtype whetstone{x' (or chisel, pick, etc).\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "type")) {
            if (argument[0] == '\0') { send_to_char("Syntax: tool type <tool_type>\n\r", ch); return false; }
            int val = flag_value(tool_types, argument);
            if (val == NO_FLAG) { send_to_char("Invalid tool type.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(TOOL(pObj)->type);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/tool/type", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/tool/type", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Tool type set.\n\r", ch);
            } else {
                TOOL(pObj)->type = val;
                send_to_char("Tool type set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "tier")) {
            if (argument[0] == '\0') { send_to_char("Syntax: tool tier <level>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(TOOL(pObj)->tier);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/tool/tier", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/tool/tier", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Tool tier set.\n\r", ch);
            } else {
                TOOL(pObj)->tier = val;
                send_to_char("Tool tier set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: type, tier\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WTool:{x\n\r"
        "  {Gtype          {x %s\n\r"
        "  {Gtier          {x [%d]\n\r",
        flag_string(tool_types, TOOL(pObj)->type),
        TOOL(pObj)->tier);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  TRADE
 * ============================================================================ */
OEDIT(oedit_trade)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_TRADE(pObj)) {
        send_to_char("Object lacks the trade type. Use '{Waddtype trade_type{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "type")) {
            if (argument[0] == '\0') {
                int i = 0;
                send_to_char("Trade types:\n\r", ch);
                while (trade_table[i].trade_type != -1) {
                    send_to_char(trade_table[i].name, ch);
                    send_to_char("\n\r", ch);
                    i++;
                }
                return false;
            }
            int val = get_trade_item(argument);
            if (val == 0 && str_cmp(argument, "none")) {
                send_to_char("Invalid trade type. Use 'trade type' to see list.\n\r", ch);
                return false;
            }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(TRADE(pObj)->trade_type);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/trade/type", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/trade/type", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Trade type set.\n\r", ch);
            } else {
                TRADE(pObj)->trade_type = val;
                send_to_char("Trade type set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: type\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WTrade:{x\n\r"
        "  {Gtype          {x %s\n\r",
        trade_table[TRADE(pObj)->trade_type].name);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  WAND (also staff)
 * ============================================================================ */
OEDIT(oedit_wand)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_WAND(pObj)) {
        send_to_char("Object lacks the wand type. Use '{Waddtype wand{x' or '{Waddtype staff{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "mana")) {
            if (argument[0] == '\0') { send_to_char("Syntax: wand mana <max_mana>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WAND(pObj)->max_mana);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/wand/mana", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/wand/mana", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Max mana set.\n\r", ch);
            } else {
                WAND(pObj)->max_mana = val;
                send_to_char("Max mana set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "charges")) {
            if (argument[0] == '\0') { send_to_char("Syntax: wand charges <current>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WAND(pObj)->charges);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/wand/charges", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/wand/charges", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Current charges set.\n\r", ch);
            } else {
                WAND(pObj)->charges = val;
                send_to_char("Current charges set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "maxcharges")) {
            if (argument[0] == '\0') { send_to_char("Syntax: wand maxcharges <max>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WAND(pObj)->max_charges);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/wand/maxcharges", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/wand/maxcharges", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Max charges set.\n\r", ch);
            } else {
                WAND(pObj)->max_charges = val;
                send_to_char("Max charges set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "cooldown")) {
            if (argument[0] == '\0') { send_to_char("Syntax: wand cooldown <ticks>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WAND(pObj)->cooldown);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/wand/cooldown", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/wand/cooldown", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Cooldown set.\n\r", ch);
            } else {
                WAND(pObj)->cooldown = val;
                send_to_char("Cooldown set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "recharge")) {
            if (argument[0] == '\0') { send_to_char("Syntax: wand recharge <ticks>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WAND(pObj)->recharge_time);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/wand/recharge", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/wand/recharge", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Recharge time set.\n\r", ch);
            } else {
                WAND(pObj)->recharge_time = val;
                send_to_char("Recharge time set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: mana, charges, maxcharges, cooldown, recharge\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WWand/Staff:{x\n\r"
        "  {Gmana          {x [%d]\n\r"
        "  {Gcharges       {x [%d]\n\r"
        "  {Gmaxcharges    {x [%d]\n\r"
        "  {Gcooldown      {x [%d]\n\r"
        "  {Grecharge      {x [%d]\n\r",
        WAND(pObj)->max_mana,
        WAND(pObj)->charges,
        WAND(pObj)->max_charges,
        WAND(pObj)->cooldown,
        WAND(pObj)->recharge_time);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  WEAPON
 * ============================================================================ */
OEDIT(oedit_weapon)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_WEAPON(pObj)) {
        send_to_char("Object lacks the weapon type. Use '{Waddtype weapon{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "class")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon class <weapon_class>\n\r", ch); return false; }
            int val = flag_value(weapon_class, argument);
            if (val == NO_FLAG) { send_to_char("Invalid weapon class.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WEAPON(pObj)->weapon_class);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/weapon/class", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/weapon/class", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Weapon class set.\n\r", ch);
            } else {
                WEAPON(pObj)->weapon_class = val;
                send_to_char("Weapon class set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "dice")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon dice <number> <size> [bonus]\n\r", ch); return false; }
            char snum[MIL], ssize[MIL];
            argument = one_argument(argument, snum);
            argument = one_argument(argument, ssize);
            if (snum[0] == '\0' || ssize[0] == '\0') {
                send_to_char("Syntax: weapon dice <number> <size> [bonus]\n\r", ch);
                return false;
            }
            int num = atoi(snum);
            int size = atoi(ssize);
            int bonus = (argument[0] != '\0') ? atoi(argument) : WEAPON(pObj)->damage.bonus;
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_pack("{s:i, s:i, s:i}",
                    "number", WEAPON(pObj)->damage.number,
                    "size", WEAPON(pObj)->damage.size,
                    "bonus", WEAPON(pObj)->damage.bonus);
                json_t *jnew = json_pack("{s:i, s:i, s:i}",
                    "number", num, "size", size, "bonus", bonus);
                olc_changeset_add_change(cs, "typedata/weapon/dice", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/weapon/dice", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Weapon damage dice set.\n\r", ch);
            } else {
                WEAPON(pObj)->damage.number = num;
                WEAPON(pObj)->damage.size = size;
                WEAPON(pObj)->damage.bonus = bonus;
                set_weapon_dice(pObj);
                send_to_char("Weapon damage dice set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "damtype")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon damtype <attack_type>\n\r", ch); return false; }
            int val = attack_lookup(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WEAPON(pObj)->damage_type);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/weapon/damtype", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/weapon/damtype", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Weapon damage type set.\n\r", ch);
            } else {
                WEAPON(pObj)->damage_type = val;
                send_to_char("Weapon damage type set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon flags <weapon_flag>\n\r", ch); return false; }
            long val = flag_value(weapon_type2, argument);
            if (val != NO_FLAG) {
                if (cs) {
                    if (!olc_check_staging_limits(ch, cs)) return false;
                    long current = olc_staged_flags_or(cs, "typedata/weapon/flags", WEAPON(pObj)->flags);
                    long result = current ^ val;
                    json_t *jold = json_integer((json_int_t)WEAPON(pObj)->flags);
                    json_t *jnew = json_integer((json_int_t)result);
                    olc_changeset_add_change(cs, "typedata/weapon/flags", OLC_FIELD_TYPE_DATA, jold, jnew);
                    notify_field_change(cs, ch, "typedata/weapon/flags", jnew, "type_data", true);
                    json_decref(jold); json_decref(jnew);
                    send_to_char("{G[STAGED]{x Weapon flags toggled.\n\r", ch);
                } else {
                    WEAPON(pObj)->flags ^= val;
                    send_to_char("Weapon flags toggled.\n\r", ch);
                }
            }
            return true;
        }
        if (!str_prefix(field, "range")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon range <distance>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WEAPON(pObj)->range);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/weapon/range", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/weapon/range", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Weapon range set.\n\r", ch);
            } else {
                WEAPON(pObj)->range = val;
                send_to_char("Weapon range set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "mana")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon mana <max_mana>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WEAPON(pObj)->max_mana);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/weapon/mana", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/weapon/mana", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Weapon max mana set.\n\r", ch);
            } else {
                WEAPON(pObj)->max_mana = val;
                send_to_char("Weapon max mana set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "charges")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon charges <current>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WEAPON(pObj)->charges);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/weapon/charges", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/weapon/charges", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Weapon charges set.\n\r", ch);
            } else {
                WEAPON(pObj)->charges = val;
                send_to_char("Weapon charges set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "maxcharges")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon maxcharges <max>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WEAPON(pObj)->max_charges);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/weapon/maxcharges", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/weapon/maxcharges", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Weapon max charges set.\n\r", ch);
            } else {
                WEAPON(pObj)->max_charges = val;
                send_to_char("Weapon max charges set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "cooldown")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon cooldown <ticks>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WEAPON(pObj)->cooldown);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/weapon/cooldown", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/weapon/cooldown", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Weapon cooldown set.\n\r", ch);
            } else {
                WEAPON(pObj)->cooldown = val;
                send_to_char("Weapon cooldown set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "recharge")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon recharge <ticks>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WEAPON(pObj)->recharge_time);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/weapon/recharge", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/weapon/recharge", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Weapon recharge time set.\n\r", ch);
            } else {
                WEAPON(pObj)->recharge_time = val;
                send_to_char("Weapon recharge time set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: class, dice, damtype, flags, range, mana, charges, maxcharges, cooldown, recharge\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WWeapon:{x\n\r"
        "  {Gclass         {x %s\n\r"
        "  {Gdice          {x %dd%d+%d\n\r"
        "  {Gdamtype       {x %s\n\r"
        "  {Gflags         {x %s\n\r"
        "  {Grange         {x [%d]\n\r",
        flag_string(weapon_class, WEAPON(pObj)->weapon_class),
        WEAPON(pObj)->damage.number, WEAPON(pObj)->damage.size, WEAPON(pObj)->damage.bonus,
        attack_table[WEAPON(pObj)->damage_type].name,
        flag_string(weapon_type2, WEAPON(pObj)->flags),
        WEAPON(pObj)->range);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  WEAPON CONTAINER
 * ============================================================================ */
OEDIT(oedit_weaponcon)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_WEAPON_CON(pObj)) {
        send_to_char("Object lacks the weapon_container type. Use '{Waddtype weapon_container{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (field[0] != '\0') {
        if (!str_prefix(field, "weight")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weaponcon weight <max_kg>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WEAPON_CON(pObj)->max_weight);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/weaponcon/weight", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/weaponcon/weight", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Max weight set.\n\r", ch);
            } else {
                WEAPON_CON(pObj)->max_weight = val;
                send_to_char("Max weight set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "weapontype")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weaponcon weapontype <weapon_class>\n\r", ch); return false; }
            int val = flag_value(weapon_class, argument);
            if (val == NO_FLAG) { send_to_char("Invalid weapon class.\n\r", ch); return false; }
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WEAPON_CON(pObj)->weapon_type);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/weaponcon/weapontype", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/weaponcon/weapontype", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Weapon type set.\n\r", ch);
            } else {
                WEAPON_CON(pObj)->weapon_type = val;
                send_to_char("Weapon type set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "items")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weaponcon items <max>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WEAPON_CON(pObj)->max_items);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/weaponcon/items", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/weaponcon/items", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Max items set.\n\r", ch);
            } else {
                WEAPON_CON(pObj)->max_items = val;
                send_to_char("Max items set.\n\r", ch);
            }
            return true;
        }
        if (!str_prefix(field, "weightmult")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weaponcon weightmult <multiplier>\n\r", ch); return false; }
            int val = atoi(argument);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *jold = json_integer(WEAPON_CON(pObj)->weight_multiplier);
                json_t *jnew = json_integer(val);
                olc_changeset_add_change(cs, "typedata/weaponcon/weightmult", OLC_FIELD_TYPE_DATA, jold, jnew);
                notify_field_change(cs, ch, "typedata/weaponcon/weightmult", jnew, "type_data", true);
                json_decref(jold); json_decref(jnew);
                send_to_char("{G[STAGED]{x Weight multiplier set.\n\r", ch);
            } else {
                WEAPON_CON(pObj)->weight_multiplier = val;
                send_to_char("Weight multiplier set.\n\r", ch);
            }
            return true;
        }
        send_to_char("Valid fields: weight, weapontype, items, weightmult\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WWeapon Container:{x\n\r"
        "  {Gweight        {x [%d kg]\n\r"
        "  {Gweapontype    {x %s\n\r"
        "  {Gitems         {x [%d]\n\r"
        "  {Gweightmult    {x [%d%%]\n\r",
        WEAPON_CON(pObj)->max_weight,
        flag_string(weapon_class, WEAPON_CON(pObj)->weapon_type),
        WEAPON_CON(pObj)->max_items,
        WEAPON_CON(pObj)->weight_multiplier);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  SHOW ALL TYPE DATA
 * ============================================================================
 *
 * Writes human-readable type data for ALL active types on an object to a
 * BUFFER.  Called by oedit_show so the editor displays every type that is
 * present, not just the primary one.
 */
void oedit_show_type_data(OBJ_INDEX_DATA *pObj, BUFFER *buffer)
{
    char buf[MSL];

    if (IS_ARMOR(pObj)) {
        sprintf(buf,
            "\n\r{WArmour:{x\n\r"
            "  {Gtype          {x %s\n\r"
            "  {Gstrength      {x %s\n\r"
            "  {Gpierce        {x [%d]\n\r"
            "  {Gbash          {x [%d]\n\r"
            "  {Gslash         {x [%d]\n\r"
            "  {Gexotic        {x [%d]\n\r",
            flag_string(armor_types, ARMOR(pObj)->armor_type),
            armour_strength_table[ARMOR(pObj)->armor_strength].name,
            ARMOR(pObj)->protection[0],
            ARMOR(pObj)->protection[1],
            ARMOR(pObj)->protection[2],
            ARMOR(pObj)->protection[3]);
        add_buf(buffer, buf);
    }

    if (IS_BODY_PART(pObj)) {
        RACE_DATA *part_race = race_lookup_uid((int16_t)BODY_PART(pObj)->race_uid);
        sprintf(buf,
            "\n\r{WBody Part:{x\n\r"
            "  {Gparts         {x %s\n\r"
            "  {Grace           {x %s\n\r",
            flag_string(part_flags, BODY_PART(pObj)->parts),
            part_race ? part_race->name : "unknown");
        add_buf(buffer, buf);
    }

    if (IS_BOOK(pObj)) {
        sprintf(buf,
            "\n\r{WBook:{x\n\r"
            "  {Gflags         {x [%s]\n\r",
            flag_string(container_flags, BOOK(pObj)->flags));
        add_buf(buffer, buf);
    }

    if (IS_CART(pObj)) {
        sprintf(buf,
            "\n\r{WCart:{x\n\r"
            "  {Gcapacity      {x [%d]\n\r"
            "  {Gdelay         {x [%d]\n\r"
            "  {Gstrength      {x [%d]\n\r"
            "  {Gitems         {x [%d]\n\r"
            "  {Gweightmult    {x [%d]\n\r"
            "  {Gflags         {x [%s]\n\r"
            "  {Gvanish        {x [%d]\n\r",
            CART(pObj)->capacity,
            CART(pObj)->move_delay,
            CART(pObj)->min_strength,
            CART(pObj)->max_items,
            CART(pObj)->weight_multiplier,
            flag_string(cart_flags, CART(pObj)->flags),
            CART(pObj)->vanish_time);
        add_buf(buffer, buf);
    }

    if (IS_COMPASS(pObj)) {
        sprintf(buf,
            "\n\r{WCompass:{x\n\r"
            "  {Gaccuracy      {x [%d%%]\n\r",
            COMPASS(pObj)->accuracy);
        add_buf(buffer, buf);
    }

    if (IS_CONTAINER(pObj)) {
        sprintf(buf,
            "\n\r{WContainer:{x\n\r"
            "  {Gweight        {x [%d kg]\n\r"
            "  {Gflags         {x [%s]\n\r"
            "  {Gitems         {x [%d]\n\r"
            "  {Gweightmult    {x [%d%%]\n\r",
            CONTAINER(pObj)->max_weight,
            flag_string(container_flags, CONTAINER(pObj)->flags),
            CONTAINER(pObj)->max_items,
            CONTAINER(pObj)->weight_multiplier);
        add_buf(buffer, buf);
    }

    if (IS_CORPSE(pObj)) {
        sprintf(buf,
            "\n\r{WCorpse:{x\n\r"
            "  {Gtype          {x %s\n\r"
            "  {Gresurrection  {x %d%%\n\r"
            "  {Ganimation     {x %d%%\n\r"
            "  {Gparts         {x %s\n\r"
            "  {Gmobile        {x [%s]\n\r",
            flag_string(corpse_types, CORPSE(pObj)->corpse_type),
            CORPSE(pObj)->resurrection,
            CORPSE(pObj)->animation,
            flag_string(part_flags, CORPSE(pObj)->body_parts),
            widevnum_string(
                CORPSE(pObj)->mobile_area_uid > 0 ? get_area_index(CORPSE(pObj)->mobile_area_uid) : NULL,
                CORPSE(pObj)->mobile_vnum, pObj->area));
        add_buf(buffer, buf);
    }

    if (IS_FLUID_CON(pObj)) {
        sprintf(buf,
            "\n\r{WFluid Container:{x\n\r"
            "  {Gcapacity      {x [%d]\n\r"
            "  {Gamount        {x [%d]\n\r"
            "  {Gliquid        {x %s\n\r"
            "  {Gpoison        {x %s\n\r"
            "  {Grefill        {x [%d]\n\r",
            FLUID_CON(pObj)->capacity,
            FLUID_CON(pObj)->amount,
            liquid_name(FLUID_CON(pObj)->liquid),
            FLUID_CON(pObj)->poison != 0 ? "Yes" : "No",
            FLUID_CON(pObj)->refill_rate);
        add_buf(buffer, buf);
    }

    if (IS_FOOD(pObj)) {
        sprintf(buf,
            "\n\r{WFood:{x\n\r"
            "  {Ghunger        {x [%d]\n\r"
            "  {Gfull          {x [%d]\n\r"
            "  {Gpoison        {x %s\n\r"
            "  {Gtimer         {x [%d]\n\r",
            FOOD(pObj)->hunger,
            FOOD(pObj)->full,
            FOOD(pObj)->poison != 0 ? "Yes" : "No",
            FOOD(pObj)->timer);
        add_buf(buffer, buf);
    }

    if (IS_FURNITURE(pObj)) {
        sprintf(buf,
            "\n\r{WFurniture:{x\n\r"
            "  {Gpeople        {x [%d]\n\r"
            "  {Gweight        {x [%d]\n\r"
            "  {Gflags         {x %s\n\r"
            "  {Gheal          {x [%d]\n\r"
            "  {Gmana          {x [%d]\n\r"
            "  {Gmove          {x [%d]\n\r",
            FURNITURE(pObj)->max_people,
            FURNITURE(pObj)->max_weight,
            flag_string(furniture_flags, FURNITURE(pObj)->flags),
            FURNITURE(pObj)->heal_rate,
            FURNITURE(pObj)->mana_rate,
            FURNITURE(pObj)->move_rate);
        add_buf(buffer, buf);
    }

    if (IS_HERB(pObj)) {
        sprintf(buf,
            "\n\r{WHerb:{x\n\r"
            "  {Gtype          {x %s\n\r"
            "  {Ghealing       {x [%d%%]\n\r"
            "  {Gregen         {x [%d%%]\n\r"
            "  {Grefresh       {x [%d%%]\n\r"
            "  {Gimmunity      {x %s\n\r"
            "  {Gresistance    {x %s\n\r"
            "  {Gvulnerability {x %s\n\r"
            "  {Gspell         {x %s\n\r",
            herb_table[HERB(pObj)->type].name,
            HERB(pObj)->healing,
            HERB(pObj)->regenerative,
            HERB(pObj)->refreshing,
            flag_string(imm_flags, HERB(pObj)->immunity),
            flag_string(res_flags, HERB(pObj)->resistance),
            flag_string(vuln_flags, HERB(pObj)->vulnerability),
            HERB(pObj)->spell > 0 ? (skill_find_uid(HERB(pObj)->spell) ? skill_find_uid(HERB(pObj)->spell)->name : "unknown") : "none");
        add_buf(buffer, buf);
    }

    if (IS_INK(pObj)) {
        sprintf(buf,
            "\n\r{WInk:{x\n\r"
            "  {Gtype1         {x %s\n\r"
            "  {Gtype2         {x %s\n\r"
            "  {Gtype3         {x %s\n\r",
            flag_string(catalyst_types, INK(pObj)->types[0]),
            flag_string(catalyst_types, INK(pObj)->types[1]),
            flag_string(catalyst_types, INK(pObj)->types[2]));
        add_buf(buffer, buf);
    }

    if (IS_INSTRUMENT(pObj)) {
        sprintf(buf,
            "\n\r{WInstrument:{x\n\r"
            "  {Gtype          {x %s\n\r"
            "  {Gflags         {x %s\n\r"
            "  {Gbeatsmin      {x [%d%%]\n\r"
            "  {Gbeatsmax      {x [%d%%]\n\r",
            flag_string(instrument_types, INSTRUMENT(pObj)->type),
            flag_string(instrument_flags, INSTRUMENT(pObj)->flags),
            INSTRUMENT(pObj)->beats_min,
            INSTRUMENT(pObj)->beats_max);
        add_buf(buffer, buf);
    }

    if (IS_JEWELRY(pObj)) {
        sprintf(buf,
            "\n\r{WJewelry:{x\n\r"
            "  {Gmana          {x [%d]\n\r",
            JEWELRY(pObj)->max_mana);
        add_buf(buffer, buf);
    }

    if (IS_LIGHT(pObj)) {
        if (LIGHT(pObj)->duration == -1) {
            sprintf(buf,
                "\n\r{WLight:{x\n\r"
                "  {Gduration      {x Infinite [-1]\n\r"
                "  {Gflags         {x [%s]\n\r",
                flag_string(light_flags, LIGHT(pObj)->flags));
        } else {
            sprintf(buf,
                "\n\r{WLight:{x\n\r"
                "  {Gduration      {x [%d]\n\r"
                "  {Gflags         {x [%s]\n\r",
                LIGHT(pObj)->duration,
                flag_string(light_flags, LIGHT(pObj)->flags));
        }
        add_buf(buffer, buf);
    }

    if (IS_MAP(pObj)) {
        sprintf(buf,
            "\n\r{WMap:{x\n\r"
            "  {Gwuid          {x [%ld]\n\r"
            "  {Gx             {x [%ld]\n\r"
            "  {Gy             {x [%ld]\n\r",
            MAP(pObj)->wuid,
            MAP(pObj)->x,
            MAP(pObj)->y);
        add_buf(buffer, buf);
    }

    if (IS_MIST(pObj)) {
        sprintf(buf,
            "\n\r{WMist:{x\n\r"
            "  {Gobjects       {x [%d%%]\n\r"
            "  {Gcharacters    {x [%d%%]\n\r"
            "  {Groom          {x [%d%%]\n\r",
            MIST(pObj)->obscure_objs,
            MIST(pObj)->obscure_mobs,
            MIST(pObj)->obscure_room);
        add_buf(buffer, buf);
    }

    if (IS_MONEY(pObj)) {
        sprintf(buf,
            "\n\r{WMoney:{x\n\r"
            "  {Gsilver        {x [%d]\n\r"
            "  {Ggold          {x [%d]\n\r",
            MONEY(pObj)->silver,
            MONEY(pObj)->gold);
        add_buf(buffer, buf);
    }

    if (IS_PAGE(pObj)) {
        sprintf(buf,
            "\n\r{WPage:{x\n\r"
            "  {Gnumber        {x [%d]\n\r"
            "  {Gtitle         {x %s\n\r",
            PAGE(pObj)->page_no,
            PAGE(pObj)->title ? PAGE(pObj)->title : "(none)");
        add_buf(buffer, buf);
    }

    if (IS_PORTAL(pObj)) {
        if (IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON)) {
            AREA_DATA *dng_area = PORTAL(pObj)->params[4] > 0 ? get_area_index(PORTAL(pObj)->params[4]) : NULL;
            const char *dng_dest = (PORTAL(pObj)->params[0] > 0)
                ? widevnum_string(dng_area, PORTAL(pObj)->params[0], pObj->area)
                : "none";

            sprintf(buf,
                "\n\r{WPortal (Dungeon):{x\n\r"
                "  {Gcharges      {x [%d]\n\r"
                "  {Gexit         {x %s\n\r"
                "  {Gflags        {x %s\n\r"
                "  {Gdestination  {x [%s] (dungeon)\n\r"
                "  {Gparam1       {x [%ld] (floor)\n\r",
                PORTAL(pObj)->charges,
                flag_string(portal_exit_flags, PORTAL(pObj)->exit),
                flag_string(portal_flags, PORTAL(pObj)->flags),
                dng_dest,
                PORTAL(pObj)->params[1]);
        } else if (IS_SET(PORTAL(pObj)->flags, GATE_AREARANDOM) || PORTAL(pObj)->params[0] == -1) {
            sprintf(buf,
                "\n\r{WPortal (Area Random):{x\n\r"
                "  {Gcharges      {x [%d]\n\r"
                "  {Gexit         {x %s\n\r"
                "  {Gflags        {x %s\n\r"
                "  {Gparam1       {x [%ld] (area id)\n\r",
                PORTAL(pObj)->charges,
                flag_string(portal_exit_flags, PORTAL(pObj)->exit),
                flag_string(portal_flags, PORTAL(pObj)->flags),
                PORTAL(pObj)->params[1]);
        } else if (PORTAL(pObj)->params[0] > 0) {
            sprintf(buf,
                "\n\r{WPortal (Static):{x\n\r"
                "  {Gcharges      {x [%d]\n\r"
                "  {Gexit         {x %s\n\r"
                "  {Gflags        {x %s\n\r"
                "  {Gdestination  {x [%s]\n\r",
                PORTAL(pObj)->charges,
                flag_string(portal_exit_flags, PORTAL(pObj)->exit),
                flag_string(portal_flags, PORTAL(pObj)->flags),
                widevnum_string(
                    PORTAL(pObj)->params[4] > 0 ? get_area_index(PORTAL(pObj)->params[4]) : NULL,
                    PORTAL(pObj)->params[0], pObj->area));
        } else {
            sprintf(buf,
                "\n\r{WPortal (Wilderness):{x\n\r"
                "  {Gcharges      {x [%d]\n\r"
                "  {Gexit         {x %s\n\r"
                "  {Gflags        {x %s\n\r"
                "  {Gparam1       {x [%ld] (map uid)\n\r"
                "  {Gparam2       {x [%ld] (map x)\n\r"
                "  {Gparam3       {x [%ld] (map y)\n\r",
                PORTAL(pObj)->charges,
                flag_string(portal_exit_flags, PORTAL(pObj)->exit),
                flag_string(portal_flags, PORTAL(pObj)->flags),
                PORTAL(pObj)->params[1],
                PORTAL(pObj)->params[2],
                PORTAL(pObj)->params[3]);
        }
        add_buf(buffer, buf);
    }

    if (IS_SCROLL(pObj)) {
        sprintf(buf,
            "\n\r{WScroll:{x\n\r"
            "  {Gmana          {x [%d]\n\r"
            "  {Gflags         {x [%s]\n\r",
            SCROLL(pObj)->max_mana,
            flag_string(scroll_flags, SCROLL(pObj)->flags));
        add_buf(buffer, buf);
    }

    if (IS_SEED(pObj)) {
        sprintf(buf,
            "\n\r{WSeed:{x\n\r"
            "  {Gtime          {x [%d]\n\r"
            "  {Gobject        {x [%s]\n\r",
            SEED(pObj)->growth_time,
            widevnum_string(
                SEED(pObj)->object_area_uid > 0 ? get_area_index(SEED(pObj)->object_area_uid) : NULL,
                SEED(pObj)->object_vnum, pObj->area));
        add_buf(buffer, buf);
    }

    if (IS_SEXTANT(pObj)) {
        sprintf(buf,
            "\n\r{WSextant:{x\n\r"
            "  {Gaccuracy      {x [%d%%]\n\r",
            SEXTANT(pObj)->accuracy);
        add_buf(buffer, buf);
    }

    if (IS_SHIP_TYPE(pObj)) {
        sprintf(buf,
            "\n\r{WShip:{x\n\r"
            "  {Gweight        {x [%d kg]\n\r"
            "  {Gdelay         {x [%d]\n\r"
            "  {Gmincrew       {x [%d]\n\r"
            "  {Gcapacity      {x [%d]\n\r"
            "  {Gmaxcrew       {x [%d]\n\r"
            "  {Groom          {x [%s]\n\r"
            "  {Ghitpoints     {x [%d]\n\r"
            "  {Gguns          {x [%d]\n\r",
            SHIP_TYPE(pObj)->weight,
            SHIP_TYPE(pObj)->move_delay,
            SHIP_TYPE(pObj)->min_crew,
            SHIP_TYPE(pObj)->capacity,
            SHIP_TYPE(pObj)->max_crew,
            widevnum_string(
                SHIP_TYPE(pObj)->first_room_area_uid > 0 ? get_area_index(SHIP_TYPE(pObj)->first_room_area_uid) : NULL,
                SHIP_TYPE(pObj)->first_room, pObj->area),
            SHIP_TYPE(pObj)->hit_points,
            SHIP_TYPE(pObj)->max_guns);
        add_buf(buffer, buf);
    }

    if (IS_SHIP_MODULE(pObj)) {
        SHIP_MODULE_DATA *d = SHIP_MODULE_TYPE(pObj);
        sprintf(buf,
            "\n\r{WShip Module:{x\n\r"
            "  {Gtype          {x [%s]\n\r"
            "  {Gsize          {x [%s]\n\r"
            "  {Gweight        {x [%d]\n\r"
            "  {Gdomain        {x [%s]\n\r"
            "  {Gflags         {x [%s]\n\r",
            flag_string(hardpoint_types, d->type),
            flag_string(hardpoint_sizes, d->size),
            d->weight,
            flag_string(domain_flags, d->domain_flags),
            flag_string(module_flags, d->flags));
        add_buf(buffer, buf);

        sprintf(buf,
            "  {GBonuses:{x\n\r"
            "    {Ghitbonus      {x [%d]  {Garmorbonus   {x [%d]\n\r"
            "    {Gspeedbonus    {x [%d%%] {Gturningbonus {x [%d]\n\r"
            "    {Gcargoweight   {x [%d]  {Gcargocapacity{x [%d]\n\r"
            "    {Gcrewbonus     {x [%d]\n\r",
            d->hit_bonus, d->armor_bonus,
            d->speed_bonus, d->turning_bonus,
            d->cargo_weight_bonus, d->cargo_capacity_bonus,
            d->crew_bonus);
        add_buf(buffer, buf);

        sprintf(buf,
            "  {GWeapon:{x\n\r"
            "    {Gdamage       {x [%d]  {Grange        {x [%d]\n\r"
            "    {Greload       {x [%d]  {Gdamagetype   {x [%d]\n\r"
            "    {Gweapflags    {x [%s]\n\r",
            d->damage, d->range,
            d->reload_time, d->damage_type,
            flag_string(weapon_module_flags, d->weapon_flags));
        add_buf(buffer, buf);

        sprintf(buf,
            "  {GCrew Requirements:{x\n\r"
            "    {Goperators    {x [%d]\n\r"
            "    {Ggunning      {x [%d]  {Gmechanics    {x [%d]\n\r"
            "    {Gscouting     {x [%d]  {Gnavigation   {x [%d]\n\r"
            "    {Goarring      {x [%d]  {Gleadership   {x [%d]\n\r",
            d->operators,
            d->req_gunning, d->req_mechanics,
            d->req_scouting, d->req_navigation,
            d->req_oarring, d->req_leadership);
        add_buf(buffer, buf);

        sprintf(buf,
            "  {GAmmo:{x\n\r"
            "    {Gammo         {x [%s]\n\r"
            "    {Gammoshot     {x [%d]\n\r",
            d->ammo ? d->ammo->short_descr : "(none)",
            d->ammo_per_shot);
        add_buf(buffer, buf);
    }

    if (IS_TATTOO(pObj)) {
        sprintf(buf,
            "\n\r{WTattoo:{x\n\r"
            "  {Gtouches       {x [%d]\n\r"
            "  {Gfading        {x [%d%%]\n\r"
            "  {Gfaderate      {x [%d]\n\r",
            TATTOO(pObj)->touches,
            TATTOO(pObj)->fading_chance,
            TATTOO(pObj)->fading_rate);
        add_buf(buffer, buf);
    }

    if (IS_TELESCOPE(pObj)) {
        if (TELESCOPE(pObj)->heading >= 0) {
            sprintf(buf,
                "\n\r{WTelescope:{x\n\r"
                "  {Gdistance      {x [%d]\n\r"
                "  {Gmindist       {x [%d]\n\r"
                "  {Gmaxdist       {x [%d]\n\r"
                "  {Gbonus         {x [%d]\n\r"
                "  {Gheading       {x [%d]\n\r",
                TELESCOPE(pObj)->distance,
                TELESCOPE(pObj)->min_distance,
                TELESCOPE(pObj)->max_distance,
                TELESCOPE(pObj)->bonus_view,
                TELESCOPE(pObj)->heading);
        } else {
            sprintf(buf,
                "\n\r{WTelescope:{x\n\r"
                "  {Gdistance      {x [%d]\n\r"
                "  {Gmindist       {x [%d]\n\r"
                "  {Gmaxdist       {x [%d]\n\r"
                "  {Gbonus         {x [%d]\n\r"
                "  {Gheading       {x [none]\n\r",
                TELESCOPE(pObj)->distance,
                TELESCOPE(pObj)->min_distance,
                TELESCOPE(pObj)->max_distance,
                TELESCOPE(pObj)->bonus_view);
        }
        add_buf(buffer, buf);
    }

    if (IS_TOOL(pObj)) {
        sprintf(buf,
            "\n\r{WTool:{x\n\r"
            "  {Gtype          {x %s\n\r"
            "  {Gtier          {x [%d]\n\r",
            flag_string(tool_types, TOOL(pObj)->type),
            TOOL(pObj)->tier);
        add_buf(buffer, buf);
    }

    if (IS_TRADE(pObj)) {
        sprintf(buf,
            "\n\r{WTrade:{x\n\r"
            "  {Gtype          {x %s\n\r",
            trade_table[TRADE(pObj)->trade_type].name);
        add_buf(buffer, buf);
    }

    if (IS_WAND(pObj)) {
        sprintf(buf,
            "\n\r{WWand/Staff:{x\n\r"
            "  {Gmana          {x [%d]\n\r"
            "  {Gcharges       {x [%d]\n\r"
            "  {Gmaxcharges    {x [%d]\n\r"
            "  {Gcooldown      {x [%d]\n\r"
            "  {Grecharge      {x [%d]\n\r",
            WAND(pObj)->max_mana,
            WAND(pObj)->charges,
            WAND(pObj)->max_charges,
            WAND(pObj)->cooldown,
            WAND(pObj)->recharge_time);
        add_buf(buffer, buf);
    }

    if (IS_WEAPON(pObj)) {
        sprintf(buf,
            "\n\r{WWeapon:{x\n\r"
            "  {Gclass         {x %s\n\r"
            "  {Gdice          {x %dd%d+%d\n\r"
            "  {Gdamtype       {x %s\n\r"
            "  {Gflags         {x %s\n\r"
            "  {Grange         {x [%d]\n\r",
            flag_string(weapon_class, WEAPON(pObj)->weapon_class),
            WEAPON(pObj)->damage.number, WEAPON(pObj)->damage.size, WEAPON(pObj)->damage.bonus,
            attack_table[WEAPON(pObj)->damage_type].name,
            flag_string(weapon_type2, WEAPON(pObj)->flags),
            WEAPON(pObj)->range);
        add_buf(buffer, buf);
    }

    if (IS_WEAPON_CON(pObj)) {
        sprintf(buf,
            "\n\r{WWeapon Container:{x\n\r"
            "  {Gweight        {x [%d kg]\n\r"
            "  {Gweapontype    {x %s\n\r"
            "  {Gitems         {x [%d]\n\r"
            "  {Gweightmult    {x [%d%%]\n\r",
            WEAPON_CON(pObj)->max_weight,
            flag_string(weapon_class, WEAPON_CON(pObj)->weapon_type),
            WEAPON_CON(pObj)->max_items,
            WEAPON_CON(pObj)->weight_multiplier);
        add_buf(buffer, buf);
    }
}
