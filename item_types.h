/***************************************************************************
 *  Sentience MUD                                                          *
 *  Item Type Data Structures and Multi-Typing System                      *
 *                                                                         *
 *  Backported from the 2.0 development branch with improvements.          *
 *  This module provides type-specific data structs for objects,            *
 *  replacing the legacy value[] array approach with strongly-typed         *
 *  data accessible through accessor macros (e.g. WEAPON(obj),             *
 *  IS_WEAPON(obj)).                                                       *
 *                                                                         *
 *  The compatibility system uses a data-driven bitset approach             *
 *  instead of the 2.0 branch's switch statement, allowing O(1)            *
 *  validation of multi-type combinations against ALL existing types.       *
 ***************************************************************************/

#ifndef ITEM_TYPES_H
#define ITEM_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Forward declarations using struct tags to avoid typedef conflicts
 * with merc.h. These are only needed as pointer types here.
 * When included from merc.h (after its own typedef block), the full
 * typedefs are already available.
 */
struct obj_data;
struct obj_index_data;
struct spell_data;
struct dice_data;
struct lock_state_data;
struct list_type;

/*
 * Maximum item type number + 1.
 * Used for array sizing and bitset calculations.
 * Current highest is ITEM_BODY_PART (84), but use 96 for headroom.
 */
#define ITEM__MAX 96

/*
 * Unified corpse type (replaces ITEM_CORPSE_NPC and ITEM_CORPSE_PC).
 * The individual NPC/PC distinction is handled by the corpse_data.player field.
 * Currently only used for the compatibility system; legacy types still exist.
 */
#ifndef ITEM_CORPSE
#define ITEM_CORPSE 85
#endif

#ifndef ITEM_FLUID_CONTAINER
#define ITEM_FLUID_CONTAINER 86
#endif

#ifndef ITEM_PAGE
#define ITEM_PAGE 87
#endif

/*
 * ============================================================================
 *  TYPE BITSET
 * ============================================================================
 *
 *  A compact bitset for tracking which item types are present on an object,
 *  and for declaring type compatibility rules. With ITEM__MAX at 96,
 *  we need 2 x uint64_t (128 bits of capacity).
 */

#define TYPE_BITSET_WORDS 2

typedef struct type_bitset {
    uint64_t bits[TYPE_BITSET_WORDS];
} TYPE_BITSET;

/* Set bit for item type n */
#define TBIT_SET(bs, n)   ((bs).bits[(n) / 64] |=  (1ULL << ((n) % 64)))
/* Clear bit for item type n */
#define TBIT_CLR(bs, n)   ((bs).bits[(n) / 64] &= ~(1ULL << ((n) % 64)))
/* Test bit for item type n */
#define TBIT_TST(bs, n)   (((bs).bits[(n) / 64] &  (1ULL << ((n) % 64))) != 0)
/* Test if bitset is all zeros */
#define TBIT_EMPTY(bs)    ((bs).bits[0] == 0 && (bs).bits[1] == 0)
/* Initialize to zero */
#define TBIT_ZERO(bs)     do { (bs).bits[0] = 0; (bs).bits[1] = 0; } while(0)

/*
 * Check if ALL bits in 'test' are clear in 'mask'.
 * Equivalent to: (test & ~mask) == 0
 * Used for compatibility: "are all set types within the allowed set?"
 */
static inline bool tbit_subset_of(TYPE_BITSET test, TYPE_BITSET mask)
{
    return ((test.bits[0] & ~mask.bits[0]) == 0)
        && ((test.bits[1] & ~mask.bits[1]) == 0);
}

/*
 * ============================================================================
 *  ITEM TYPE METADATA TABLE
 * ============================================================================
 *
 *  Each item type has an entry describing what other types it can coexist with.
 *  This replaces the 2.0 branch's switch statement with a data-driven approach.
 *
 *  Compatibility is BIDIRECTIONAL:
 *    To add type X to an object with types {A, B}:
 *      1. X must list A and B in its composites (X can live with A and B)
 *      2. A and B must each list X in their composites (A and B can live with X)
 *
 *  The check is O(1):
 *    can_add = tbit_subset_of(obj->type_flags, item_type_info[X].composites)
 *           && tbit_subset_of(X_bit, item_type_info[A].composites)
 *           && tbit_subset_of(X_bit, item_type_info[B].composites);
 *
 *  Simplified with the helper function below.
 */

typedef struct item_type_info {
    int         type;           /* ITEM_* constant */
    const char *name;           /* Display name */
    TYPE_BITSET composites;     /* Bitset of types this can coexist with */
    bool        has_typed_data; /* Whether this type has a type-specific struct */
} ITEM_TYPE_INFO;

/* Global metadata table, indexed by ITEM_* constant */
extern ITEM_TYPE_INFO item_type_info[ITEM__MAX];

/*
 * ============================================================================
 *  COMPATIBILITY API
 * ============================================================================
 */

/* Initialize the item_type_info table. Call once at boot. */
void            item_types_init(void);

/* Can item type 'add_type' be added to an object with primary type 'primary'
 * and current type_flags? Checks bidirectional compatibility. */
bool            obj_can_add_item_type(int primary, TYPE_BITSET current_flags, int add_type);

/* Simplified: can this type be added to this obj_index? */
bool            obj_index_can_add_item_type(OBJ_INDEX_DATA *pObjIndex, int item_type);

/* Fill 'assigned' with all types currently present on the object index. */
void            obj_index_assess_item_types(OBJ_INDEX_DATA *pObjIndex, bool assigned[ITEM__MAX]);

/*
 * ============================================================================
 *  ACCESSOR MACROS
 * ============================================================================
 *
 *  TYPE(obj)     - Returns the type-specific data pointer (may be NULL).
 *  IS_TYPE(obj)  - Returns true if the object has valid type-specific data.
 *
 *  These work on both OBJ_DATA and OBJ_INDEX_DATA since both structs have
 *  the same _type pointer fields.
 */

#define ARMOR(obj)       ((obj)->_armor)
#define IS_ARMOR(obj)    (ARMOR(obj) != NULL && (obj)->_armor->valid)

#define BODY_PART(obj)   ((obj)->_body_part)
#define IS_BODY_PART(obj)(BODY_PART(obj) != NULL && (obj)->_body_part->valid)

#define BOOK(obj)        ((obj)->_book)
#define IS_BOOK(obj)     (BOOK(obj) != NULL && (obj)->_book->valid)

#define CART(obj)        ((obj)->_cart)
#define IS_CART(obj)     (CART(obj) != NULL && (obj)->_cart->valid)

#define COMPASS(obj)     ((obj)->_compass)
#define IS_COMPASS(obj)  (COMPASS(obj) != NULL && (obj)->_compass->valid)

#define CONTAINER(obj)   ((obj)->_container)
#define IS_CONTAINER(obj)(CONTAINER(obj) != NULL && (obj)->_container->valid)

#define CORPSE(obj)      ((obj)->_corpse)
#define IS_CORPSE(obj)   (CORPSE(obj) != NULL && (obj)->_corpse->valid)

#define FLUID_CON(obj)   ((obj)->_fluid_container)
#define IS_FLUID_CON(obj)(FLUID_CON(obj) != NULL && (obj)->_fluid_container->valid)

#define FOOD(obj)        ((obj)->_food)
#define IS_FOOD(obj)     (FOOD(obj) != NULL && (obj)->_food->valid)

#define FURNITURE(obj)   ((obj)->_furniture)
#define IS_FURNITURE(obj)(FURNITURE(obj) != NULL && (obj)->_furniture->valid)

#define HERB(obj)        ((obj)->_herb)
#define IS_HERB(obj)     (HERB(obj) != NULL && (obj)->_herb->valid)

#define INK(obj)         ((obj)->_ink)
#define IS_INK(obj)      (INK(obj) != NULL && (obj)->_ink->valid)

#define INSTRUMENT(obj)  ((obj)->_instrument)
#define IS_INSTRUMENT(obj)(INSTRUMENT(obj) != NULL && (obj)->_instrument->valid)

#define SHIP_TYPE(obj)   ((obj)->_item_ship)
#define IS_SHIP_TYPE(obj)(SHIP_TYPE(obj) != NULL && (obj)->_item_ship->valid)

#define JEWELRY(obj)     ((obj)->_jewelry)
#define IS_JEWELRY(obj)  (JEWELRY(obj) != NULL && (obj)->_jewelry->valid)

#define LIGHT(obj)       ((obj)->_light)
#define IS_LIGHT(obj)    (LIGHT(obj) != NULL && (obj)->_light->valid)

#define MAP(obj)         ((obj)->_map)
#define IS_MAP(obj)      (MAP(obj) != NULL && (obj)->_map->valid)

#define MIST(obj)        ((obj)->_mist)
#define IS_MIST(obj)     (MIST(obj) != NULL && (obj)->_mist->valid)

#define MONEY(obj)       ((obj)->_money)
#define IS_MONEY(obj)    (MONEY(obj) != NULL && (obj)->_money->valid)

#define PAGE(obj)        ((obj)->_page)
#define IS_PAGE(obj)     (PAGE(obj) != NULL && (obj)->_page->valid)

#define PORTAL(obj)      ((obj)->_portal)
#define IS_PORTAL(obj)   (PORTAL(obj) != NULL && (obj)->_portal->valid)

#define SCROLL(obj)      ((obj)->_scroll)
#define IS_SCROLL(obj)   (SCROLL(obj) != NULL && (obj)->_scroll->valid)

#define SEED(obj)        ((obj)->_seed)
#define IS_SEED(obj)     (SEED(obj) != NULL && (obj)->_seed->valid)

#define SEXTANT(obj)     ((obj)->_sextant)
#define IS_SEXTANT(obj)  (SEXTANT(obj) != NULL && (obj)->_sextant->valid)

#define TATTOO(obj)      ((obj)->_tattoo)
#define IS_TATTOO(obj)   (TATTOO(obj) != NULL && (obj)->_tattoo->valid)

#define TELESCOPE(obj)   ((obj)->_telescope)
#define IS_TELESCOPE(obj)(TELESCOPE(obj) != NULL && (obj)->_telescope->valid)

#define TOOL(obj)        ((obj)->_tool)
#define IS_TOOL(obj)     (TOOL(obj) != NULL && (obj)->_tool->valid)

#define TRADE(obj)       ((obj)->_trade)
#define IS_TRADE(obj)    (TRADE(obj) != NULL && (obj)->_trade->valid)

#define WAND(obj)        ((obj)->_wand)
#define IS_WAND(obj)     (WAND(obj) != NULL && (obj)->_wand->valid)

#define WEAPON(obj)      ((obj)->_weapon)
#define IS_WEAPON(obj)   (WEAPON(obj) != NULL && (obj)->_weapon->valid)

#define WEAPON_CON(obj)  ((obj)->_weapon_container)
#define IS_WEAPON_CON(obj)(WEAPON_CON(obj) != NULL && (obj)->_weapon_container->valid)

/*
 * ============================================================================
 *  TYPE-SPECIFIC DATA STRUCTURES
 * ============================================================================
 *
 *  Each struct follows the pattern:
 *    - TYPE *next;     (free-list linkage)
 *    - bool valid;     (validation flag, used by IS_VALID/VALIDATE/INVALIDATE)
 *    - type-specific fields
 */

/* ==================== ARMOR ==================== */

#define ARMOR_TYPE_NONE   0
#define ARMOR_TYPE_CLOTH  1
#define ARMOR_TYPE_LEATHER 2
#define ARMOR_TYPE_MAIL   3
#define ARMOR_TYPE_PLATE  4

/* Armor protection array uses AC_PIERCE/AC_BASH/AC_SLASH/AC_EXOTIC (4 slots) */
#define ARMOR_PROT_MAX    4

struct obj_armor_data
{
    struct obj_armor_data *next;
    bool valid;

    int16_t armor_type;
    int16_t armor_strength;
    int16_t protection[ARMOR_PROT_MAX];
};

/* ==================== BOOK ==================== */

struct book_page_data
{
    struct book_page_data *next;
    bool valid;

    int   page_no;
    char *title;
    char *text;
};

struct obj_book_data
{
    struct obj_book_data *next;
    bool valid;

    char *name;
    char *short_descr;

    long  flags;         /* Container flags for open/close/lock */
    int   current_page;
    LLIST *pages;        /* BOOK_PAGE * elements */
    int   open_page;     /* Initial page on open */

    LOCK_STATE *lock;
};

/* ==================== CART ==================== */

#define CART_MOUNT_ONLY       (A)
#define CART_TEAM_ANIMAL_ONLY (B)

struct obj_cart_data
{
    struct obj_cart_data *next;
    bool valid;

    long    flags;
    int16_t min_strength;
    int16_t move_delay;
    int     capacity;        /* Max weight of contents */
    int     max_items;       /* Maximum number of items */
    int     weight_multiplier; /* Weight mult for contents (%) */
    int     vanish_time;     /* Timer before cart vanishes */
};

/* ==================== COMPASS ==================== */

struct obj_compass_data
{
    struct obj_compass_data *next;
    bool valid;

    int16_t accuracy;
    long    wuid;
    long    x;
    long    y;
};

/* ==================== CONTAINER ==================== */

struct container_filter_data
{
    int item_type;
    int sub_type;
};

struct obj_container_data
{
    struct obj_container_data *next;
    bool valid;

    char *name;
    char *short_descr;

    long  flags;
    int   max_weight;
    int   weight_multiplier;
    int   max_volume;
    int   max_items;         /* Maximum number of items */

    LLIST *whitelist;    /* CONTAINER_FILTER * */
    LLIST *blacklist;    /* CONTAINER_FILTER * */

    LOCK_STATE *lock;
};

/* ==================== FLUID CONTAINER ==================== */
/* Combines DRINK_CON, FOUNTAIN, and POTION into one type */

struct obj_fluid_container_data
{
    struct obj_fluid_container_data *next;
    bool valid;

    char *name;
    char *short_descr;

    long    flags;       /* Container flags */
    int     liquid;      /* Index into runtime liquid list */
    int16_t capacity;
    int16_t amount;
    int16_t refill_rate; /* Positive = fountain behavior */
    int16_t poison;      /* 0-100 range */

    LOCK_STATE *lock;

    SPELL_DATA *spells;  /* For potion-type fluids */
};

/* ==================== FOOD ==================== */

struct obj_food_data
{
    struct obj_food_data *next;
    bool valid;

    LLIST *buffs;          /* FOOD_BUFF_DATA * elements */
    int hunger;
    int full;
    int poison;          /* 0 (none) to 100 (permanent) */
    int timer;           /* Timer before food disappears */
};

struct food_buff_data
{
    struct food_buff_data *next;
    bool valid;

    int16_t where;
    int16_t location;
    int16_t modifier;
    long bitvector;
    long bitvector2;
};

/* ==================== FURNITURE ==================== */

#define FURNITURE_ON    (A)
#define FURNITURE_IN    (B)
#define FURNITURE_AT    (C)
#define FURNITURE_ABOVE (D)
#define FURNITURE_UNDER (E)

struct obj_furniture_data
{
    struct obj_furniture_data *next;
    bool valid;

    long  flags;
    int   max_people;
    int   max_weight;
    int   heal_rate;
    int   mana_rate;
    int   move_rate;

    /* Positional flags: which positions are available */
    long  standing;
    long  sitting;
    long  resting;
    long  sleeping;
};

/* ==================== INK ==================== */

#define MAX_INK_TYPES 3

struct obj_ink_data
{
    struct obj_ink_data *next;
    bool valid;

    int16_t types[MAX_INK_TYPES];
    int16_t amounts[MAX_INK_TYPES];
};

/* ==================== INSTRUMENT ==================== */

#define INSTRUMENT_MAX_CATALYSTS 3

typedef struct instrument_reservoir_data
{
    int16_t type;
    int16_t amount;
    int16_t capacity;
} INSTRUMENT_CATALYST;

struct obj_instrument_data
{
    struct obj_instrument_data *next;
    bool valid;

    int  type;
    long flags;

    int  mana_min;
    int  mana_max;
    int  beats_min;
    int  beats_max;

    INSTRUMENT_CATALYST reservoirs[INSTRUMENT_MAX_CATALYSTS];
};

/* ==================== JEWELRY ==================== */

struct obj_jewelry_data
{
    struct obj_jewelry_data *next;
    bool valid;

    int max_mana;
    SPELL_DATA *spells;
};

/* ==================== LIGHT ==================== */

#define LIGHT_IS_ACTIVE            (A)
#define LIGHT_REMOVE_ON_EXTINGUISH (B)
#define LIGHT_NO_EXTINGUISH        (C)

struct obj_light_data
{
    struct obj_light_data *next;
    bool valid;

    long flags;
    int  duration;       /* Ticks remaining. Negative = infinite */
};

/* ==================== MAP ==================== */

struct obj_map_data
{
    struct obj_map_data *next;
    bool valid;

    long  wuid;
    long  x;
    long  y;
    LLIST *waypoints;
};

/* ==================== MIST ==================== */

struct obj_mist_data
{
    struct obj_mist_data *next;
    bool valid;

    char obscure_mobs;
    char obscure_objs;
    char obscure_room;

    char icy;
    char fiery;
    char acidic;
    char stink;
    char wither;
    char toxic;
    char shock;
    char fog;
    char sleep;
};

/* ==================== MONEY ==================== */

struct obj_money_data
{
    struct obj_money_data *next;
    bool valid;

    int silver;
    int gold;
};

/* ==================== PAGE ==================== */

struct obj_page_data
{
    struct obj_page_data *next;
    bool valid;

    int   page_no;
    char *title;
    char *text;
};

/* ==================== PORTAL ==================== */

#define MAX_PORTAL_VALUES 5

struct obj_portal_data
{
    struct obj_portal_data *next;
    bool valid;

    char *name;
    char *short_descr;

    long  exit;          /* Exit flags */
    long  flags;         /* Portal flags */
    int   charges;       /* < 0 = infinite */
    int   type;          /* Gate type */
    long  params[MAX_PORTAL_VALUES];

    LOCK_STATE *lock;
};

/* ==================== SCROLL ==================== */

#define SCROLL_DESTROY_ON_RECITE (A)

struct obj_scroll_data
{
    struct obj_scroll_data *next;
    bool valid;

    int   max_mana;
    long  flags;
    SPELL_DATA *spells;
};

/* ==================== SEXTANT ==================== */

struct obj_sextant_data
{
    struct obj_sextant_data *next;
    bool valid;

    int16_t accuracy;
};

/* ==================== TATTOO ==================== */

struct obj_tattoo_data
{
    struct obj_tattoo_data *next;
    bool valid;

    int touches;
    int fading_chance;
    int fading_rate;

    SPELL_DATA *spells;
};

/* ==================== TELESCOPE ==================== */

struct obj_telescope_data
{
    struct obj_telescope_data *next;
    bool valid;

    int16_t distance;
    int16_t min_distance;
    int16_t max_distance;
    int16_t bonus_view;
    int16_t heading;
};

/* ==================== TOOL ==================== */

#define TOOL_NONE          0
#define TOOL_PICKAXE       1
#define TOOL_SLEDGEHAMMER  2
#define TOOL_HATCHET       3
#define TOOL_SCYTHE        4
#define TOOL_FISHING_ROD   5
#define TOOL_FISHING_SPEAR 6
#define TOOL_SKINNING_KNIFE 7

struct obj_tool_data
{
    struct obj_tool_data *next;
    bool valid;

    int16_t type;
    int16_t tier;
};

/* ==================== WAND ==================== */

struct obj_wand_data
{
    struct obj_wand_data *next;
    bool valid;

    int max_mana;
    int charges;
    int max_charges;
    int cooldown;
    int recharge_time;

    SPELL_DATA *spells;
};

/* ==================== WEAPON ==================== */

struct obj_weapon_data
{
    struct obj_weapon_data *next;
    bool valid;

    int16_t weapon_class;
    int     damage_type;
    long    flags;
    DICE_DATA damage;

    int   range;         /* Number of rooms for ranged weapons */
    int   max_mana;      /* Maximum mana for imbuing */

    int   charges;
    int   max_charges;
    int   cooldown;
    int   recharge_time;

    SPELL_DATA *spells;
};

/* ==================== BODY PART ==================== */

struct obj_body_part_data
{
    struct obj_body_part_data *next;
    bool valid;

    long parts;          /* part_flags bitfield */
    int  race_uid;
    long char_id[2];     /* Owner character ID pair */
};

/* ==================== CORPSE ==================== */

struct obj_corpse_data
{
    struct obj_corpse_data *next;
    bool valid;

    int  corpse_type;    /* corpse_types flag value */
    int  resurrection;   /* Resurrection chance (0-100%) */
    int  animation;      /* Animation chance (0-100%) */
    long body_parts;     /* part_flags */
    long mobile_vnum;    /* Source mobile vnum */
    long mobile_area_uid;/* Source mobile area UID (0 = resolve globally) */
};

/* ==================== HERB ==================== */

struct obj_herb_data
{
    struct obj_herb_data *next;
    bool valid;

    int  type;           /* herb_table index */
    int  healing;        /* Healing percentage */
    int  regenerative;   /* Regenerative percentage */
    int  refreshing;     /* Refreshing percentage */
    long immunity;       /* imm_flags */
    long resistance;     /* res_flags */
    long vulnerability;  /* vuln_flags */
    int  spell;          /* skill_table index */
};

/* ==================== ITEM SHIP ==================== */
/* Note: SHIP_DATA is already used in merc.h for runtime ship instances.
 * This struct holds the template properties from value[] on ITEM_SHIP objects. */

struct obj_item_ship_data
{
    struct obj_item_ship_data *next;
    bool valid;

    int  weight;
    int  move_delay;
    int  min_crew;
    int  capacity;
    int  max_crew;
    long first_room;          /* Room vnum */
    long first_room_area_uid; /* Room area UID (0 = resolve globally) */
    int  hit_points;
    int  max_guns;
};

/* ==================== SEED ==================== */

struct obj_seed_data
{
    struct obj_seed_data *next;
    bool valid;

    int  growth_time;         /* Ticks before growth */
    long object_vnum;         /* Vnum of object it turns into */
    long object_area_uid;     /* Area UID of object (0 = resolve globally) */
};

/* ==================== TRADE ==================== */

struct obj_trade_data
{
    struct obj_trade_data *next;
    bool valid;

    int trade_type;      /* trade_table index */
};

/* ==================== WEAPON CONTAINER ==================== */

struct obj_weapon_container_data
{
    struct obj_weapon_container_data *next;
    bool valid;

    int  max_weight;
    int  weapon_type;    /* weapon_class flag value */
    int  max_items;      /* Maximum number of items */
    int  weight_multiplier; /* Weight multiplier (%) */
};

/*
 * ============================================================================
 *  TYPEDEFS (aliases matching the 2.0 branch convention)
 * ============================================================================
 */

typedef struct obj_armor_data           ARMOR_DATA;
typedef struct obj_body_part_data       BODY_PART_DATA;
typedef struct book_page_data           BOOK_PAGE;
typedef struct obj_book_data            BOOK_DATA;
typedef struct obj_cart_data             CART_DATA;
typedef struct obj_compass_data         COMPASS_DATA;
typedef struct container_filter_data    CONTAINER_FILTER;
typedef struct obj_container_data       CONTAINER_DATA;
typedef struct obj_corpse_data          CORPSE_DATA;
typedef struct obj_fluid_container_data FLUID_CONTAINER_DATA;
typedef struct food_buff_data           FOOD_BUFF_DATA;
typedef struct obj_food_data            FOOD_DATA;
typedef struct obj_furniture_data       FURNITURE_DATA;
typedef struct obj_herb_data            HERB_DATA;
typedef struct obj_ink_data             INK_DATA;
typedef struct obj_instrument_data      INSTRUMENT_DATA;
typedef struct obj_item_ship_data       ITEM_SHIP_DATA;
typedef struct obj_jewelry_data         JEWELRY_DATA;
typedef struct obj_light_data           LIGHT_DATA;
typedef struct obj_map_data             MAP_DATA;
typedef struct obj_mist_data            MIST_DATA;
typedef struct obj_money_data           MONEY_DATA;
typedef struct obj_page_data            PAGE_DATA;
typedef struct obj_portal_data          PORTAL_DATA;
typedef struct obj_scroll_data          SCROLL_DATA;
typedef struct obj_seed_data            SEED_DATA;
typedef struct obj_sextant_data         SEXTANT_DATA;
typedef struct obj_tattoo_data          TATTOO_DATA;
typedef struct obj_telescope_data       TELESCOPE_DATA;
typedef struct obj_tool_data            TOOL_DATA;
typedef struct obj_trade_data           TRADE_DATA;
typedef struct obj_wand_data            WAND_DATA;
typedef struct obj_weapon_container_data WEAPON_CONTAINER_DATA;
typedef struct obj_weapon_data          WEAPON_DATA;

/*
 * ============================================================================
 *  MEMORY MANAGEMENT
 * ============================================================================
 */

/* new_* - Allocate and initialize a type data struct */
ARMOR_DATA *            new_armor_data(void);
BODY_PART_DATA *        new_body_part_data(void);
BOOK_PAGE *             new_book_page(void);
BOOK_DATA *             new_book_data(void);
CART_DATA *             new_cart_data(void);
COMPASS_DATA *          new_compass_data(void);
CONTAINER_DATA *        new_container_data(void);
CORPSE_DATA *           new_corpse_data(void);
FLUID_CONTAINER_DATA *  new_fluid_container_data(void);
FOOD_BUFF_DATA *        new_food_buff_data(void);
FOOD_DATA *             new_food_data(void);
FURNITURE_DATA *        new_furniture_data(void);
HERB_DATA *             new_herb_data(void);
INK_DATA *              new_ink_data(void);
INSTRUMENT_DATA *       new_instrument_data(void);
ITEM_SHIP_DATA *        new_item_ship_data(void);
JEWELRY_DATA *          new_jewelry_data(void);
LIGHT_DATA *            new_light_data(void);
MAP_DATA *              new_map_data(void);
MIST_DATA *             new_mist_data(void);
MONEY_DATA *            new_money_data(void);
PAGE_DATA *             new_page_data(void);
PORTAL_DATA *           new_portal_data(void);
SCROLL_DATA *           new_scroll_data(void);
SEED_DATA *             new_seed_data(void);
SEXTANT_DATA *          new_sextant_data(void);
TATTOO_DATA *           new_tattoo_data(void);
TELESCOPE_DATA *        new_telescope_data(void);
TOOL_DATA *             new_tool_data(void);
TRADE_DATA *            new_trade_data(void);
WAND_DATA *             new_wand_data(void);
WEAPON_CONTAINER_DATA * new_weapon_container_data(void);
WEAPON_DATA *           new_weapon_data(void);

/* copy_* - Deep copy a type data struct (returns NULL if src is invalid) */
ARMOR_DATA *            copy_armor_data(ARMOR_DATA *src);
BODY_PART_DATA *        copy_body_part_data(BODY_PART_DATA *src);
BOOK_PAGE *             copy_book_page(BOOK_PAGE *src);
BOOK_DATA *             copy_book_data(BOOK_DATA *src);
CART_DATA *             copy_cart_data(CART_DATA *src);
COMPASS_DATA *          copy_compass_data(COMPASS_DATA *src);
CONTAINER_DATA *        copy_container_data(CONTAINER_DATA *src);
CORPSE_DATA *           copy_corpse_data(CORPSE_DATA *src);
FLUID_CONTAINER_DATA *  copy_fluid_container_data(FLUID_CONTAINER_DATA *src);
FOOD_BUFF_DATA *        copy_food_buff_data(FOOD_BUFF_DATA *src);
FOOD_DATA *             copy_food_data(FOOD_DATA *src);
FURNITURE_DATA *        copy_furniture_data(FURNITURE_DATA *src);
HERB_DATA *             copy_herb_data(HERB_DATA *src);
INK_DATA *              copy_ink_data(INK_DATA *src);
INSTRUMENT_DATA *       copy_instrument_data(INSTRUMENT_DATA *src);
ITEM_SHIP_DATA *        copy_item_ship_data(ITEM_SHIP_DATA *src);
JEWELRY_DATA *          copy_jewelry_data(JEWELRY_DATA *src);
LIGHT_DATA *            copy_light_data(LIGHT_DATA *src);
MAP_DATA *              copy_map_data(MAP_DATA *src);
MIST_DATA *             copy_mist_data(MIST_DATA *src);
MONEY_DATA *            copy_money_data(MONEY_DATA *src);
PAGE_DATA *             copy_page_data(PAGE_DATA *src);
PORTAL_DATA *           copy_portal_data(PORTAL_DATA *src);
SCROLL_DATA *           copy_scroll_data(SCROLL_DATA *src);
SEED_DATA *             copy_seed_data(SEED_DATA *src);
SEXTANT_DATA *          copy_sextant_data(SEXTANT_DATA *src);
TATTOO_DATA *           copy_tattoo_data(TATTOO_DATA *src);
TELESCOPE_DATA *        copy_telescope_data(TELESCOPE_DATA *src);
TOOL_DATA *             copy_tool_data(TOOL_DATA *src);
TRADE_DATA *            copy_trade_data(TRADE_DATA *src);
WAND_DATA *             copy_wand_data(WAND_DATA *src);
WEAPON_CONTAINER_DATA * copy_weapon_container_data(WEAPON_CONTAINER_DATA *src);
WEAPON_DATA *           copy_weapon_data(WEAPON_DATA *src);

/* free_* - Return a type data struct to the free list */
void    free_armor_data(ARMOR_DATA *data);
void    free_body_part_data(BODY_PART_DATA *data);
void    free_book_page(BOOK_PAGE *data);
void    free_book_data(BOOK_DATA *data);
void    free_cart_data(CART_DATA *data);
void    free_compass_data(COMPASS_DATA *data);
void    free_container_data(CONTAINER_DATA *data);
void    free_corpse_data(CORPSE_DATA *data);
void    free_fluid_container_data(FLUID_CONTAINER_DATA *data);
void    free_food_buff_data(FOOD_BUFF_DATA *data);
void    free_food_data(FOOD_DATA *data);
void    free_furniture_data(FURNITURE_DATA *data);
void    free_herb_data(HERB_DATA *data);
void    free_ink_data(INK_DATA *data);
void    free_instrument_data(INSTRUMENT_DATA *data);
void    free_item_ship_data(ITEM_SHIP_DATA *data);
void    free_jewelry_data(JEWELRY_DATA *data);
void    free_light_data(LIGHT_DATA *data);
void    free_map_data(MAP_DATA *data);
void    free_mist_data(MIST_DATA *data);
void    free_money_data(MONEY_DATA *data);
void    free_page_data(PAGE_DATA *data);
void    free_portal_data(PORTAL_DATA *data);
void    free_scroll_data(SCROLL_DATA *data);
void    free_seed_data(SEED_DATA *data);
void    free_sextant_data(SEXTANT_DATA *data);
void    free_tattoo_data(TATTOO_DATA *data);
void    free_telescope_data(TELESCOPE_DATA *data);
void    free_tool_data(TOOL_DATA *data);
void    free_trade_data(TRADE_DATA *data);
void    free_wand_data(WAND_DATA *data);
void    free_weapon_container_data(WEAPON_CONTAINER_DATA *data);
void    free_weapon_data(WEAPON_DATA *data);

/*
 * ============================================================================
 *  UTILITY FUNCTIONS
 * ============================================================================
 */

/* Weight calculation that accounts for containers and money */
int     obj_get_weight(OBJ_DATA *obj);

/* Get the subtype for an object (e.g. weapon class) */
int     obj_get_subtype(OBJ_DATA *obj);
int     objindex_get_subtype(OBJ_INDEX_DATA *pObjIndex);

/* Container utilities */
bool    container_is_valid_item_type(OBJ_DATA *container, int item_type, int subtype);
bool    container_is_valid_item(OBJ_DATA *container, OBJ_DATA *obj);
int     container_get_content_weight(OBJ_DATA *container, OBJ_DATA *obj);
bool    container_can_fit_weight(OBJ_DATA *container, OBJ_DATA *obj);
int     container_get_content_volume(OBJ_DATA *container, OBJ_DATA *obj);
bool    container_can_fit_volume(OBJ_DATA *container, OBJ_DATA *obj);
bool    container_check_duplicate(OBJ_DATA *container, OBJ_DATA *obj);

/* Free all type-specific data on an object */
void    obj_free_type_data(OBJ_DATA *obj);
void    obj_index_free_type_data(OBJ_INDEX_DATA *pObj);

/* Copy all type-specific data from source to destination */
void    obj_copy_type_data(OBJ_DATA *dst, OBJ_DATA *src);
void    obj_copy_type_data_from_index(OBJ_DATA *dst, OBJ_INDEX_DATA *src);
void    obj_index_copy_type_data(OBJ_INDEX_DATA *dst, OBJ_INDEX_DATA *src);

/* Allocate type-specific data struct for a given item type.
 * If the struct already exists, does nothing.
 * Sets the corresponding bit in type_flags.
 * Returns true if the type was allocated (or already present). */
bool    obj_index_alloc_type_data(OBJ_INDEX_DATA *pObj, int item_type);

/* Free all type data, set new primary type, allocate its struct. */
void    obj_index_set_primary_type(OBJ_INDEX_DATA *pObj, int item_type);

/* Remove a secondary type from an object index.
 * Frees the type-specific struct and clears the type_flags bit.
 * Cannot remove the primary type. */
bool    obj_index_remove_type(OBJ_INDEX_DATA *pObj, int item_type);

/* Migrate value[] array to type-specific data structs.
 * Called during version migration (VERSION_OBJECT_005).
 * Populates the appropriate type struct based on item_type. */
void    obj_migrate_values_to_types(OBJ_DATA *obj);
void    obj_index_migrate_values_to_types(OBJ_INDEX_DATA *obj);

/* Legacy value[] compatibility helpers for runtime script paths.
 * These expose old slot semantics while reading/writing canonical
 * type-specific data when available. */
int     obj_get_legacy_value_slot(OBJ_DATA *obj, int slot);
bool    obj_set_legacy_value_slot(OBJ_DATA *obj, int slot, int value);

#endif /* ITEM_TYPES_H */
