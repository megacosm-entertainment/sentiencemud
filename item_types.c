/***************************************************************************
 *  Sentience MUD                                                          *
 *  Item Type System Implementation                                        *
 *                                                                         *
 *  Data-driven multi-typing compatibility system and helper functions.     *
 ***************************************************************************/

#include "merc.h"
#include "item_types.h"

/*
 * ============================================================================
 *  COMPATIBILITY TABLE
 * ============================================================================
 *
 *  Initialized by item_types_init() at boot time.
 *
 *  Each type's composites bitset declares which OTHER types it can coexist
 *  with. When adding type X to an object that already has types {A, B}, the
 *  system checks:
 *    1. ALL of {A, B} are in X's composites (X is compatible with them)
 *    2. X is in A's composites AND X is in B's composites (they accept X)
 *
 *  This is a single O(1) bitwise check per direction.
 */

ITEM_TYPE_INFO item_type_info[ITEM__MAX];

/*
 * Helper: make types A and B mutually compatible.
 * Sets the bit for B in A's composites, and A in B's composites.
 */
static void compose(int type_a, int type_b)
{
    if (type_a < 0 || type_a >= ITEM__MAX) return;
    if (type_b < 0 || type_b >= ITEM__MAX) return;

    TBIT_SET(item_type_info[type_a].composites, type_b);
    TBIT_SET(item_type_info[type_b].composites, type_a);
}

/*
 * Helper: allow type_a to accept type_b as secondary (one-way only).
 * The reverse is NOT set - type_b does not necessarily accept type_a.
 */
static void compose_oneway(int type_a, int type_b)
{
    if (type_a < 0 || type_a >= ITEM__MAX) return;
    if (type_b < 0 || type_b >= ITEM__MAX) return;

    TBIT_SET(item_type_info[type_a].composites, type_b);
}

void item_types_init(void)
{
    memset(item_type_info, 0, sizeof(item_type_info));

    /* Set up basic metadata for all known item types */
    item_type_info[ITEM_LIGHT].type          = ITEM_LIGHT;
    item_type_info[ITEM_LIGHT].name          = "light";
    item_type_info[ITEM_LIGHT].has_typed_data = true;

    item_type_info[ITEM_SCROLL].type         = ITEM_SCROLL;
    item_type_info[ITEM_SCROLL].name         = "scroll";
    item_type_info[ITEM_SCROLL].has_typed_data = true;

    item_type_info[ITEM_WAND].type           = ITEM_WAND;
    item_type_info[ITEM_WAND].name           = "wand";
    item_type_info[ITEM_WAND].has_typed_data = true;

    item_type_info[ITEM_STAFF].type          = ITEM_STAFF;
    item_type_info[ITEM_STAFF].name          = "staff";

    item_type_info[ITEM_WEAPON].type         = ITEM_WEAPON;
    item_type_info[ITEM_WEAPON].name         = "weapon";
    item_type_info[ITEM_WEAPON].has_typed_data = true;

    item_type_info[ITEM_TREASURE].type       = ITEM_TREASURE;
    item_type_info[ITEM_TREASURE].name       = "treasure";

    item_type_info[ITEM_ARMOUR].type         = ITEM_ARMOUR;
    item_type_info[ITEM_ARMOUR].name         = "armour";
    item_type_info[ITEM_ARMOUR].has_typed_data = true;

    item_type_info[ITEM_POTION].type         = ITEM_POTION;
    item_type_info[ITEM_POTION].name         = "potion";

    item_type_info[ITEM_CLOTHING].type       = ITEM_CLOTHING;
    item_type_info[ITEM_CLOTHING].name       = "clothing";

    item_type_info[ITEM_FURNITURE].type      = ITEM_FURNITURE;
    item_type_info[ITEM_FURNITURE].name      = "furniture";
    item_type_info[ITEM_FURNITURE].has_typed_data = true;

    item_type_info[ITEM_TRASH].type          = ITEM_TRASH;
    item_type_info[ITEM_TRASH].name          = "trash";

    item_type_info[ITEM_CONTAINER].type      = ITEM_CONTAINER;
    item_type_info[ITEM_CONTAINER].name      = "container";
    item_type_info[ITEM_CONTAINER].has_typed_data = true;

    item_type_info[ITEM_DRINK_CON].type      = ITEM_DRINK_CON;
    item_type_info[ITEM_DRINK_CON].name      = "drink container";

    item_type_info[ITEM_KEY].type            = ITEM_KEY;
    item_type_info[ITEM_KEY].name            = "key";

    item_type_info[ITEM_FOOD].type           = ITEM_FOOD;
    item_type_info[ITEM_FOOD].name           = "food";
    item_type_info[ITEM_FOOD].has_typed_data = true;

    item_type_info[ITEM_MONEY].type          = ITEM_MONEY;
    item_type_info[ITEM_MONEY].name          = "money";
    item_type_info[ITEM_MONEY].has_typed_data = true;

    item_type_info[ITEM_BOAT].type           = ITEM_BOAT;
    item_type_info[ITEM_BOAT].name           = "boat";

    item_type_info[ITEM_CORPSE_NPC].type     = ITEM_CORPSE_NPC;
    item_type_info[ITEM_CORPSE_NPC].name     = "npc corpse";

    item_type_info[ITEM_CORPSE_PC].type      = ITEM_CORPSE_PC;
    item_type_info[ITEM_CORPSE_PC].name      = "pc corpse";

    item_type_info[ITEM_FOUNTAIN].type       = ITEM_FOUNTAIN;
    item_type_info[ITEM_FOUNTAIN].name       = "fountain";

    item_type_info[ITEM_PILL].type           = ITEM_PILL;
    item_type_info[ITEM_PILL].name           = "pill";

    item_type_info[ITEM_PROTECT].type        = ITEM_PROTECT;
    item_type_info[ITEM_PROTECT].name        = "protect";

    item_type_info[ITEM_MAP].type            = ITEM_MAP;
    item_type_info[ITEM_MAP].name            = "map";
    item_type_info[ITEM_MAP].has_typed_data  = true;

    item_type_info[ITEM_PORTAL].type         = ITEM_PORTAL;
    item_type_info[ITEM_PORTAL].name         = "portal";
    item_type_info[ITEM_PORTAL].has_typed_data = true;

    item_type_info[ITEM_CATALYST].type       = ITEM_CATALYST;
    item_type_info[ITEM_CATALYST].name       = "catalyst";

    item_type_info[ITEM_ROOM_KEY].type       = ITEM_ROOM_KEY;
    item_type_info[ITEM_ROOM_KEY].name       = "room key";

    item_type_info[ITEM_GEM].type            = ITEM_GEM;
    item_type_info[ITEM_GEM].name            = "gem";

    item_type_info[ITEM_JEWELRY].type        = ITEM_JEWELRY;
    item_type_info[ITEM_JEWELRY].name        = "jewelry";
    item_type_info[ITEM_JEWELRY].has_typed_data = true;

    item_type_info[ITEM_JUKEBOX].type        = ITEM_JUKEBOX;
    item_type_info[ITEM_JUKEBOX].name        = "jukebox";

    item_type_info[ITEM_ARTIFACT].type       = ITEM_ARTIFACT;
    item_type_info[ITEM_ARTIFACT].name       = "artifact";

    item_type_info[ITEM_SHARECERT].type      = ITEM_SHARECERT;
    item_type_info[ITEM_SHARECERT].name      = "share certificate";

    item_type_info[ITEM_ROOM_FLAME].type     = ITEM_ROOM_FLAME;
    item_type_info[ITEM_ROOM_FLAME].name     = "room flame";

    item_type_info[ITEM_INSTRUMENT].type     = ITEM_INSTRUMENT;
    item_type_info[ITEM_INSTRUMENT].name     = "instrument";
    item_type_info[ITEM_INSTRUMENT].has_typed_data = true;

    item_type_info[ITEM_SEED].type           = ITEM_SEED;
    item_type_info[ITEM_SEED].name           = "seed";

    item_type_info[ITEM_CART].type           = ITEM_CART;
    item_type_info[ITEM_CART].name           = "cart";
    item_type_info[ITEM_CART].has_typed_data = true;

    item_type_info[ITEM_SHIP].type           = ITEM_SHIP;
    item_type_info[ITEM_SHIP].name           = "ship";

    item_type_info[ITEM_ROOM_DARKNESS].type  = ITEM_ROOM_DARKNESS;
    item_type_info[ITEM_ROOM_DARKNESS].name  = "room darkness";

    item_type_info[ITEM_RANGED_WEAPON].type  = ITEM_RANGED_WEAPON;
    item_type_info[ITEM_RANGED_WEAPON].name  = "ranged weapon";

    item_type_info[ITEM_SEXTANT].type        = ITEM_SEXTANT;
    item_type_info[ITEM_SEXTANT].name        = "sextant";
    item_type_info[ITEM_SEXTANT].has_typed_data = true;

    item_type_info[ITEM_WEAPON_CONTAINER].type = ITEM_WEAPON_CONTAINER;
    item_type_info[ITEM_WEAPON_CONTAINER].name = "weapon container";

    item_type_info[ITEM_ROOM_ROOMSHIELD].type = ITEM_ROOM_ROOMSHIELD;
    item_type_info[ITEM_ROOM_ROOMSHIELD].name = "room shield";

    item_type_info[ITEM_BOOK].type           = ITEM_BOOK;
    item_type_info[ITEM_BOOK].name           = "book";
    item_type_info[ITEM_BOOK].has_typed_data = true;

    item_type_info[ITEM_SMOKE_BOMB].type     = ITEM_SMOKE_BOMB;
    item_type_info[ITEM_SMOKE_BOMB].name     = "smoke bomb";

    item_type_info[ITEM_STINKING_CLOUD].type = ITEM_STINKING_CLOUD;
    item_type_info[ITEM_STINKING_CLOUD].name = "stinking cloud";

    item_type_info[ITEM_HERB].type           = ITEM_HERB;
    item_type_info[ITEM_HERB].name           = "herb";

    item_type_info[ITEM_SPELL_TRAP].type     = ITEM_SPELL_TRAP;
    item_type_info[ITEM_SPELL_TRAP].name     = "spell trap";

    item_type_info[ITEM_WITHERING_CLOUD].type = ITEM_WITHERING_CLOUD;
    item_type_info[ITEM_WITHERING_CLOUD].name = "withering cloud";

    item_type_info[ITEM_BANK].type           = ITEM_BANK;
    item_type_info[ITEM_BANK].name           = "bank";

    item_type_info[ITEM_KEYRING].type        = ITEM_KEYRING;
    item_type_info[ITEM_KEYRING].name        = "keyring";

    item_type_info[ITEM_TRADE_TYPE].type     = ITEM_TRADE_TYPE;
    item_type_info[ITEM_TRADE_TYPE].name     = "trade type";

    item_type_info[ITEM_ICE_STORM].type      = ITEM_ICE_STORM;
    item_type_info[ITEM_ICE_STORM].name      = "ice storm";

    item_type_info[ITEM_FLOWER].type         = ITEM_FLOWER;
    item_type_info[ITEM_FLOWER].name         = "flower";

    item_type_info[ITEM_EMPTY_VIAL].type     = ITEM_EMPTY_VIAL;
    item_type_info[ITEM_EMPTY_VIAL].name     = "empty vial";

    item_type_info[ITEM_BLANK_SCROLL].type   = ITEM_BLANK_SCROLL;
    item_type_info[ITEM_BLANK_SCROLL].name   = "blank scroll";

    item_type_info[ITEM_MIST].type           = ITEM_MIST;
    item_type_info[ITEM_MIST].name           = "mist";
    item_type_info[ITEM_MIST].has_typed_data = true;

    item_type_info[ITEM_SHRINE].type         = ITEM_SHRINE;
    item_type_info[ITEM_SHRINE].name         = "shrine";

    item_type_info[ITEM_WHISTLE].type        = ITEM_WHISTLE;
    item_type_info[ITEM_WHISTLE].name        = "whistle";

    item_type_info[ITEM_SHOVEL].type         = ITEM_SHOVEL;
    item_type_info[ITEM_SHOVEL].name         = "shovel";

    item_type_info[ITEM_TOOL].type           = ITEM_TOOL;
    item_type_info[ITEM_TOOL].name           = "tool";
    item_type_info[ITEM_TOOL].has_typed_data = true;

    item_type_info[ITEM_PIPE].type           = ITEM_PIPE;
    item_type_info[ITEM_PIPE].name           = "pipe";

    item_type_info[ITEM_TATTOO].type         = ITEM_TATTOO;
    item_type_info[ITEM_TATTOO].name         = "tattoo";
    item_type_info[ITEM_TATTOO].has_typed_data = true;

    item_type_info[ITEM_INK].type            = ITEM_INK;
    item_type_info[ITEM_INK].name            = "ink";
    item_type_info[ITEM_INK].has_typed_data  = true;

    item_type_info[ITEM_PART].type           = ITEM_PART;
    item_type_info[ITEM_PART].name           = "part";

    item_type_info[ITEM_COMMODITY].type      = ITEM_COMMODITY;
    item_type_info[ITEM_COMMODITY].name      = "commodity";

    item_type_info[ITEM_TELESCOPE].type      = ITEM_TELESCOPE;
    item_type_info[ITEM_TELESCOPE].name      = "telescope";
    item_type_info[ITEM_TELESCOPE].has_typed_data = true;

    item_type_info[ITEM_COMPASS].type        = ITEM_COMPASS;
    item_type_info[ITEM_COMPASS].name        = "compass";
    item_type_info[ITEM_COMPASS].has_typed_data = true;

    item_type_info[ITEM_WHETSTONE].type      = ITEM_WHETSTONE;
    item_type_info[ITEM_WHETSTONE].name      = "whetstone";

    item_type_info[ITEM_CHISEL].type         = ITEM_CHISEL;
    item_type_info[ITEM_CHISEL].name         = "chisel";

    item_type_info[ITEM_PICK].type           = ITEM_PICK;
    item_type_info[ITEM_PICK].name           = "pick";

    item_type_info[ITEM_TINDERBOX].type      = ITEM_TINDERBOX;
    item_type_info[ITEM_TINDERBOX].name      = "tinderbox";

    item_type_info[ITEM_DRYING_CLOTH].type   = ITEM_DRYING_CLOTH;
    item_type_info[ITEM_DRYING_CLOTH].name   = "drying cloth";

    item_type_info[ITEM_NEEDLE].type         = ITEM_NEEDLE;
    item_type_info[ITEM_NEEDLE].name         = "needle";

    item_type_info[ITEM_BODY_PART].type      = ITEM_BODY_PART;
    item_type_info[ITEM_BODY_PART].name      = "body part";

    /*
     * New unified types (not yet in the main item type list).
     * These will be used as the codebase migrates.
     */
    item_type_info[ITEM_CORPSE].type         = ITEM_CORPSE;
    item_type_info[ITEM_CORPSE].name         = "corpse";

    item_type_info[ITEM_FLUID_CONTAINER].type      = ITEM_FLUID_CONTAINER;
    item_type_info[ITEM_FLUID_CONTAINER].name      = "fluid container";
    item_type_info[ITEM_FLUID_CONTAINER].has_typed_data = true;

    item_type_info[ITEM_PAGE].type           = ITEM_PAGE;
    item_type_info[ITEM_PAGE].name           = "page";
    item_type_info[ITEM_PAGE].has_typed_data = true;

    /*
     * ================================================================
     *  COMPATIBILITY RULES
     * ================================================================
     *
     *  Bidirectional (compose): A can have B, B can have A.
     *  One-way (compose_oneway): A can accept B as secondary,
     *     but B cannot accept A.
     *
     *  These mirror the 2.0 branch's switch statement rules,
     *  now expressed declaratively.
     */

    /* CONTAINER composites */
    compose(ITEM_CONTAINER,  ITEM_ARMOUR);
    compose(ITEM_CONTAINER,  ITEM_CART);
    compose(ITEM_CONTAINER,  ITEM_FLUID_CONTAINER);
    compose(ITEM_CONTAINER,  ITEM_FURNITURE);
    compose(ITEM_CONTAINER,  ITEM_JEWELRY);
    compose(ITEM_CONTAINER,  ITEM_LIGHT);
    compose(ITEM_CONTAINER,  ITEM_PORTAL);

    /* JEWELRY composites (beyond container, already set above) */
    compose(ITEM_JEWELRY,    ITEM_FLUID_CONTAINER);
    compose(ITEM_JEWELRY,    ITEM_LIGHT);
    compose(ITEM_JEWELRY,    ITEM_WAND);

    /* CART composites (container already set) */
    compose(ITEM_CART,       ITEM_FURNITURE);
    compose(ITEM_CART,       ITEM_LIGHT);
    compose(ITEM_CART,       ITEM_PORTAL);
    compose(ITEM_CART,       ITEM_FLUID_CONTAINER);

    /* CORPSE - one-way only: corpses can accept cart, not vice versa */
    compose_oneway(ITEM_CORPSE,      ITEM_CART);
    compose_oneway(ITEM_CORPSE_NPC,  ITEM_CART);
    compose_oneway(ITEM_CORPSE_PC,   ITEM_CART);

    /* FURNITURE composites (container, cart already set) */
    compose(ITEM_FURNITURE,  ITEM_FLUID_CONTAINER);
    compose(ITEM_FURNITURE,  ITEM_LIGHT);
    compose(ITEM_FURNITURE,  ITEM_PORTAL);

    /* ARMOUR composites (container already set) */
    compose(ITEM_ARMOUR,     ITEM_FLUID_CONTAINER);
    compose(ITEM_ARMOUR,     ITEM_LIGHT);

    /* FLUID_CONTAINER composites (container, furniture, armour, cart, jewelry already set) */
    compose(ITEM_FLUID_CONTAINER, ITEM_LIGHT);
    compose(ITEM_FLUID_CONTAINER, ITEM_PORTAL);
    compose(ITEM_FLUID_CONTAINER, ITEM_WEAPON);

    /* LIGHT composites (most already set above) */
    compose(ITEM_LIGHT,      ITEM_WEAPON);
    compose(ITEM_LIGHT,      ITEM_WAND);
    compose(ITEM_LIGHT,      ITEM_PORTAL);

    /* PORTAL composites (container, furniture, light, fluid_container, cart already set) */
    /* All portal composites are already set via mutual compose() calls above */

    /* WAND composites (jewelry, light already set) */
    compose(ITEM_WAND,       ITEM_WEAPON);

    /* WEAPON composites (light, wand, fluid_container already set) */
    /* All weapon composites are already set via mutual compose() calls above */
}

/*
 * ============================================================================
 *  COMPATIBILITY CHECKING
 * ============================================================================
 */

/*
 * Check if 'add_type' can be added to an object with the given primary type
 * and current type flags.
 *
 * Returns true if:
 *   1. add_type considers ALL currently set types as compatible
 *   2. ALL currently set types consider add_type as compatible
 */
bool obj_can_add_item_type(int primary, TYPE_BITSET current_flags, int add_type)
{
    if (add_type <= 0 || add_type >= ITEM__MAX)
        return false;

    /* Can't add a type the object already has */
    if (TBIT_TST(current_flags, add_type))
        return false;

    /* The new type must be compatible with ALL existing types */
    if (!tbit_subset_of(current_flags, item_type_info[add_type].composites))
        return false;

    /*
     * ALL existing types must also accept the new type.
     * Build a single-bit set for the new type and check each existing type.
     */
    TYPE_BITSET new_bit;
    TBIT_ZERO(new_bit);
    TBIT_SET(new_bit, add_type);

    for (int i = 0; i < ITEM__MAX; i++)
    {
        if (TBIT_TST(current_flags, i))
        {
            if (!tbit_subset_of(new_bit, item_type_info[i].composites))
                return false;
        }
    }

    return true;
}

/*
 * Convenience wrapper for obj_index.
 * Builds type_flags from the object's current state and calls
 * obj_can_add_item_type().
 */
bool obj_index_can_add_item_type(OBJ_INDEX_DATA *pObjIndex, int item_type)
{
    if (pObjIndex == NULL)
        return false;

    /* Use the stored type_flags bitset */
    return obj_can_add_item_type(pObjIndex->item_type, pObjIndex->type_flags, item_type);
}

/*
 * Populate a boolean array indicating which types are active on an object index.
 */
void obj_index_assess_item_types(OBJ_INDEX_DATA *pObjIndex, bool assigned[ITEM__MAX])
{
    for (int i = 0; i < ITEM__MAX; i++)
        assigned[i] = false;

    if (pObjIndex == NULL)
        return;

    /* Primary type is always set */
    if (pObjIndex->item_type > 0 && pObjIndex->item_type < ITEM__MAX)
        assigned[pObjIndex->item_type] = true;

    /* Check for secondary types via IS_* macros */
    if (IS_ARMOR(pObjIndex))      assigned[ITEM_ARMOUR] = true;
    if (IS_BOOK(pObjIndex))       assigned[ITEM_BOOK] = true;
    if (IS_CART(pObjIndex))       assigned[ITEM_CART] = true;
    if (IS_COMPASS(pObjIndex))    assigned[ITEM_COMPASS] = true;
    if (IS_CONTAINER(pObjIndex))  assigned[ITEM_CONTAINER] = true;
    if (IS_FLUID_CON(pObjIndex))  assigned[ITEM_FLUID_CONTAINER] = true;
    if (IS_FOOD(pObjIndex))       assigned[ITEM_FOOD] = true;
    if (IS_FURNITURE(pObjIndex))  assigned[ITEM_FURNITURE] = true;
    if (IS_INK(pObjIndex))        assigned[ITEM_INK] = true;
    if (IS_INSTRUMENT(pObjIndex)) assigned[ITEM_INSTRUMENT] = true;
    if (IS_JEWELRY(pObjIndex))    assigned[ITEM_JEWELRY] = true;
    if (IS_LIGHT(pObjIndex))      assigned[ITEM_LIGHT] = true;
    if (IS_MAP(pObjIndex))        assigned[ITEM_MAP] = true;
    if (IS_MIST(pObjIndex))       assigned[ITEM_MIST] = true;
    if (IS_MONEY(pObjIndex))      assigned[ITEM_MONEY] = true;
    if (IS_PAGE(pObjIndex))       assigned[ITEM_PAGE] = true;
    if (IS_PORTAL(pObjIndex))     assigned[ITEM_PORTAL] = true;
    if (IS_SCROLL(pObjIndex))     assigned[ITEM_SCROLL] = true;
    if (IS_SEXTANT(pObjIndex))    assigned[ITEM_SEXTANT] = true;
    if (IS_TATTOO(pObjIndex))     assigned[ITEM_TATTOO] = true;
    if (IS_TELESCOPE(pObjIndex))  assigned[ITEM_TELESCOPE] = true;
    if (IS_TOOL(pObjIndex))       assigned[ITEM_TOOL] = true;
    if (IS_WAND(pObjIndex))       assigned[ITEM_WAND] = true;
    if (IS_WEAPON(pObjIndex))     assigned[ITEM_WEAPON] = true;
}

/*
 * ============================================================================
 *  WEIGHT CALCULATION
 * ============================================================================
 */

int obj_get_weight(OBJ_DATA *obj)
{
    int weight;

    if (IS_MONEY(obj))
        return get_weight_coins(MONEY(obj)->silver, MONEY(obj)->gold);

    weight = obj->weight;

    /* Factor in container weight reduction */
    if (IS_CONTAINER(obj))
    {
        int contents = container_get_content_weight(obj, NULL);
        if (contents > 0)
        {
            weight += CONTAINER(obj)->weight_multiplier * contents / 100;
        }
    }

    return weight;
}

/*
 * ============================================================================
 *  SUBTYPE
 * ============================================================================
 */

int obj_get_subtype(OBJ_DATA *obj)
{
    if (IS_VALID(obj))
    {
        if (IS_WEAPON(obj))
            return WEAPON(obj)->weapon_class;
    }

    return -1;
}

int objindex_get_subtype(OBJ_INDEX_DATA *pObjIndex)
{
    if (pObjIndex)
    {
        if (IS_WEAPON(pObjIndex))
            return WEAPON(pObjIndex)->weapon_class;
    }

    return -1;
}

/*
 * ============================================================================
 *  CONTAINER HELPERS
 * ============================================================================
 */

static bool __container_is_listed(LLIST *list, int item_type, int sub_type)
{
    ITERATOR it;
    CONTAINER_FILTER *filter;

    iterator_start(&it, list);
    while ((filter = (CONTAINER_FILTER *)iterator_nextdata(&it)))
    {
        if (filter->item_type == item_type
            && (sub_type < 0 || filter->sub_type < 0 || filter->sub_type == sub_type))
        {
            break;
        }
    }
    iterator_stop(&it);

    return filter != NULL;
}

bool container_is_valid_item_type(OBJ_DATA *container, int item_type, int subtype)
{
    return IS_VALID(container) && IS_CONTAINER(container)
        && ((list_size(CONTAINER(container)->whitelist) < 1)
            || __container_is_listed(CONTAINER(container)->whitelist, item_type, subtype))
        && ((list_size(CONTAINER(container)->blacklist) < 1)
            || !__container_is_listed(CONTAINER(container)->blacklist, item_type, subtype));
}

bool container_is_valid_item(OBJ_DATA *container, OBJ_DATA *obj)
{
    int subtype = obj_get_subtype(obj);

    return IS_VALID(container) && IS_CONTAINER(container) && IS_VALID(obj)
        && ((list_size(CONTAINER(container)->whitelist) < 1)
            || __container_is_listed(CONTAINER(container)->whitelist, obj->item_type, subtype))
        && ((list_size(CONTAINER(container)->blacklist) < 1)
            || !__container_is_listed(CONTAINER(container)->blacklist, obj->item_type, subtype));
}

int container_get_content_weight(OBJ_DATA *container, OBJ_DATA *obj)
{
    if (!IS_VALID(container))   return -1;
    if (!IS_CONTAINER(container)) return -1;

    int weight = 0;
    OBJ_DATA *o;

    for (o = container->contains; o; o = o->next_content)
    {
        weight += obj_get_weight(o);
    }

    if (IS_VALID(obj))
    {
        weight += obj_get_weight(obj);
    }

    return weight;
}

bool container_can_fit_weight(OBJ_DATA *container, OBJ_DATA *obj)
{
    if (CONTAINER(container)->max_weight < 0) return true;

    int weight = container_get_content_weight(container, obj);
    if (weight < 0) return false;

    return (weight <= CONTAINER(container)->max_weight);
}

int container_get_content_volume(OBJ_DATA *container, OBJ_DATA *obj)
{
    if (!IS_VALID(container))   return -1;
    if (!IS_CONTAINER(container)) return -1;

    int volume = 0;
    OBJ_DATA *o;

    for (o = container->contains; o; o = o->next_content)
    {
        volume += get_obj_number(o);
    }

    if (IS_VALID(obj))
    {
        volume += get_obj_number(obj);
    }

    return volume;
}

bool container_can_fit_volume(OBJ_DATA *container, OBJ_DATA *obj)
{
    if (CONTAINER(container)->max_volume < 0) return true;

    int volume = container_get_content_volume(container, obj);
    if (volume < 0) return false;

    return (volume <= CONTAINER(container)->max_volume);
}

bool container_check_duplicate(OBJ_DATA *container, OBJ_DATA *obj)
{
    if (!IS_CONTAINER(container)) return false;

    OBJ_DATA *o;

    for (o = container->contains; o; o = o->next_content)
    {
        if (o->pIndexData == obj->pIndexData)
            return false;
    }

    return true;
}

/*
 * ============================================================================
 *  TYPE DATA LIFECYCLE (free/copy all)
 * ============================================================================
 */

void obj_free_type_data(OBJ_DATA *obj)
{
    if (obj == NULL) return;

    if (obj->_armor)           { free_armor_data(obj->_armor);           obj->_armor = NULL; }
    if (obj->_body_part)       { free_body_part_data(obj->_body_part);   obj->_body_part = NULL; }
    if (obj->_book)            { free_book_data(obj->_book);             obj->_book = NULL; }
    if (obj->_cart)            { free_cart_data(obj->_cart);              obj->_cart = NULL; }
    if (obj->_compass)         { free_compass_data(obj->_compass);       obj->_compass = NULL; }
    if (obj->_container)       { free_container_data(obj->_container);   obj->_container = NULL; }
    if (obj->_corpse)          { free_corpse_data(obj->_corpse);         obj->_corpse = NULL; }
    if (obj->_fluid_container) { free_fluid_container_data(obj->_fluid_container); obj->_fluid_container = NULL; }
    if (obj->_food)            { free_food_data(obj->_food);             obj->_food = NULL; }
    if (obj->_furniture)       { free_furniture_data(obj->_furniture);   obj->_furniture = NULL; }
    if (obj->_herb)            { free_herb_data(obj->_herb);             obj->_herb = NULL; }
    if (obj->_ink)             { free_ink_data(obj->_ink);               obj->_ink = NULL; }
    if (obj->_instrument)      { free_instrument_data(obj->_instrument); obj->_instrument = NULL; }
    if (obj->_item_ship)       { free_item_ship_data(obj->_item_ship);   obj->_item_ship = NULL; }
    if (obj->_jewelry)         { free_jewelry_data(obj->_jewelry);       obj->_jewelry = NULL; }
    if (obj->_light)           { free_light_data(obj->_light);           obj->_light = NULL; }
    if (obj->_map)             { free_map_data(obj->_map);               obj->_map = NULL; }
    if (obj->_mist)            { free_mist_data(obj->_mist);             obj->_mist = NULL; }
    if (obj->_money)           { free_money_data(obj->_money);           obj->_money = NULL; }
    if (obj->_page)            { free_page_data(obj->_page);             obj->_page = NULL; }
    if (obj->_portal)          { free_portal_data(obj->_portal);         obj->_portal = NULL; }
    if (obj->_scroll)          { free_scroll_data(obj->_scroll);         obj->_scroll = NULL; }
    if (obj->_seed)            { free_seed_data(obj->_seed);             obj->_seed = NULL; }
    if (obj->_sextant)         { free_sextant_data(obj->_sextant);       obj->_sextant = NULL; }
    if (obj->_tattoo)          { free_tattoo_data(obj->_tattoo);         obj->_tattoo = NULL; }
    if (obj->_telescope)       { free_telescope_data(obj->_telescope);   obj->_telescope = NULL; }
    if (obj->_tool)            { free_tool_data(obj->_tool);             obj->_tool = NULL; }
    if (obj->_trade)           { free_trade_data(obj->_trade);           obj->_trade = NULL; }
    if (obj->_wand)            { free_wand_data(obj->_wand);             obj->_wand = NULL; }
    if (obj->_weapon_container){ free_weapon_container_data(obj->_weapon_container); obj->_weapon_container = NULL; }
    if (obj->_weapon)          { free_weapon_data(obj->_weapon);         obj->_weapon = NULL; }

    TBIT_ZERO(obj->type_flags);
}

void obj_index_free_type_data(OBJ_INDEX_DATA *pObj)
{
    if (pObj == NULL) return;

    if (pObj->_armor)           { free_armor_data(pObj->_armor);           pObj->_armor = NULL; }
    if (pObj->_body_part)       { free_body_part_data(pObj->_body_part);   pObj->_body_part = NULL; }
    if (pObj->_book)            { free_book_data(pObj->_book);             pObj->_book = NULL; }
    if (pObj->_cart)            { free_cart_data(pObj->_cart);              pObj->_cart = NULL; }
    if (pObj->_compass)         { free_compass_data(pObj->_compass);       pObj->_compass = NULL; }
    if (pObj->_container)       { free_container_data(pObj->_container);   pObj->_container = NULL; }
    if (pObj->_corpse)          { free_corpse_data(pObj->_corpse);         pObj->_corpse = NULL; }
    if (pObj->_fluid_container) { free_fluid_container_data(pObj->_fluid_container); pObj->_fluid_container = NULL; }
    if (pObj->_food)            { free_food_data(pObj->_food);             pObj->_food = NULL; }
    if (pObj->_furniture)       { free_furniture_data(pObj->_furniture);   pObj->_furniture = NULL; }
    if (pObj->_herb)            { free_herb_data(pObj->_herb);             pObj->_herb = NULL; }
    if (pObj->_ink)             { free_ink_data(pObj->_ink);               pObj->_ink = NULL; }
    if (pObj->_instrument)      { free_instrument_data(pObj->_instrument); pObj->_instrument = NULL; }
    if (pObj->_item_ship)       { free_item_ship_data(pObj->_item_ship);   pObj->_item_ship = NULL; }
    if (pObj->_jewelry)         { free_jewelry_data(pObj->_jewelry);       pObj->_jewelry = NULL; }
    if (pObj->_light)           { free_light_data(pObj->_light);           pObj->_light = NULL; }
    if (pObj->_map)             { free_map_data(pObj->_map);               pObj->_map = NULL; }
    if (pObj->_mist)            { free_mist_data(pObj->_mist);             pObj->_mist = NULL; }
    if (pObj->_money)           { free_money_data(pObj->_money);           pObj->_money = NULL; }
    if (pObj->_page)            { free_page_data(pObj->_page);             pObj->_page = NULL; }
    if (pObj->_portal)          { free_portal_data(pObj->_portal);         pObj->_portal = NULL; }
    if (pObj->_scroll)          { free_scroll_data(pObj->_scroll);         pObj->_scroll = NULL; }
    if (pObj->_seed)            { free_seed_data(pObj->_seed);             pObj->_seed = NULL; }
    if (pObj->_sextant)         { free_sextant_data(pObj->_sextant);       pObj->_sextant = NULL; }
    if (pObj->_tattoo)          { free_tattoo_data(pObj->_tattoo);         pObj->_tattoo = NULL; }
    if (pObj->_telescope)       { free_telescope_data(pObj->_telescope);   pObj->_telescope = NULL; }
    if (pObj->_tool)            { free_tool_data(pObj->_tool);             pObj->_tool = NULL; }
    if (pObj->_trade)           { free_trade_data(pObj->_trade);           pObj->_trade = NULL; }
    if (pObj->_wand)            { free_wand_data(pObj->_wand);             pObj->_wand = NULL; }
    if (pObj->_weapon_container){ free_weapon_container_data(pObj->_weapon_container); pObj->_weapon_container = NULL; }
    if (pObj->_weapon)          { free_weapon_data(pObj->_weapon);         pObj->_weapon = NULL; }

    TBIT_ZERO(pObj->type_flags);
}

/*
 * Copy type data between two OBJ_DATA pointers.
 */
void obj_copy_type_data(OBJ_DATA *dst, OBJ_DATA *src)
{
    if (dst == NULL || src == NULL) return;

    dst->type_flags = src->type_flags;

    dst->_armor           = copy_armor_data(src->_armor);
    dst->_body_part       = copy_body_part_data(src->_body_part);
    dst->_book            = copy_book_data(src->_book);
    dst->_cart            = copy_cart_data(src->_cart);
    dst->_compass         = copy_compass_data(src->_compass);
    dst->_container       = copy_container_data(src->_container);
    dst->_corpse          = copy_corpse_data(src->_corpse);
    dst->_fluid_container = copy_fluid_container_data(src->_fluid_container);
    dst->_food            = copy_food_data(src->_food);
    dst->_furniture       = copy_furniture_data(src->_furniture);
    dst->_herb            = copy_herb_data(src->_herb);
    dst->_ink             = copy_ink_data(src->_ink);
    dst->_instrument      = copy_instrument_data(src->_instrument);
    dst->_item_ship       = copy_item_ship_data(src->_item_ship);
    dst->_jewelry         = copy_jewelry_data(src->_jewelry);
    dst->_light           = copy_light_data(src->_light);
    dst->_map             = copy_map_data(src->_map);
    dst->_mist            = copy_mist_data(src->_mist);
    dst->_money           = copy_money_data(src->_money);
    dst->_page            = copy_page_data(src->_page);
    dst->_portal          = copy_portal_data(src->_portal);
    dst->_scroll          = copy_scroll_data(src->_scroll);
    dst->_seed            = copy_seed_data(src->_seed);
    dst->_sextant         = copy_sextant_data(src->_sextant);
    dst->_tattoo          = copy_tattoo_data(src->_tattoo);
    dst->_telescope       = copy_telescope_data(src->_telescope);
    dst->_tool            = copy_tool_data(src->_tool);
    dst->_trade           = copy_trade_data(src->_trade);
    dst->_wand            = copy_wand_data(src->_wand);
    dst->_weapon_container= copy_weapon_container_data(src->_weapon_container);
    dst->_weapon          = copy_weapon_data(src->_weapon);
}

/*
 * Copy type data from an OBJ_INDEX_DATA template to an OBJ_DATA instance.
 * Used by create_object_noid() to propagate type structs from template to instance.
 * The _<type> pointer fields are identical between both struct types.
 */
void obj_copy_type_data_from_index(OBJ_DATA *dst, OBJ_INDEX_DATA *src)
{
    if (dst == NULL || src == NULL) return;

    dst->type_flags = src->type_flags;

    dst->_armor           = copy_armor_data(src->_armor);
    dst->_body_part       = copy_body_part_data(src->_body_part);
    dst->_book            = copy_book_data(src->_book);
    dst->_cart            = copy_cart_data(src->_cart);
    dst->_compass         = copy_compass_data(src->_compass);
    dst->_container       = copy_container_data(src->_container);
    dst->_corpse          = copy_corpse_data(src->_corpse);
    dst->_fluid_container = copy_fluid_container_data(src->_fluid_container);
    dst->_food            = copy_food_data(src->_food);
    dst->_furniture       = copy_furniture_data(src->_furniture);
    dst->_herb            = copy_herb_data(src->_herb);
    dst->_ink             = copy_ink_data(src->_ink);
    dst->_instrument      = copy_instrument_data(src->_instrument);
    dst->_item_ship       = copy_item_ship_data(src->_item_ship);
    dst->_jewelry         = copy_jewelry_data(src->_jewelry);
    dst->_light           = copy_light_data(src->_light);
    dst->_map             = copy_map_data(src->_map);
    dst->_mist            = copy_mist_data(src->_mist);
    dst->_money           = copy_money_data(src->_money);
    dst->_page            = copy_page_data(src->_page);
    dst->_portal          = copy_portal_data(src->_portal);
    dst->_scroll          = copy_scroll_data(src->_scroll);
    dst->_seed            = copy_seed_data(src->_seed);
    dst->_sextant         = copy_sextant_data(src->_sextant);
    dst->_tattoo          = copy_tattoo_data(src->_tattoo);
    dst->_telescope       = copy_telescope_data(src->_telescope);
    dst->_tool            = copy_tool_data(src->_tool);
    dst->_trade           = copy_trade_data(src->_trade);
    dst->_wand            = copy_wand_data(src->_wand);
    dst->_weapon_container= copy_weapon_container_data(src->_weapon_container);
    dst->_weapon          = copy_weapon_data(src->_weapon);
}

/*
 * ============================================================================
 *  VALUE[] → TYPE STRUCT MIGRATION
 * ============================================================================
 *
 *  These functions populate type-specific data structs from the legacy value[]
 *  array based on item_type. Called during VERSION_OBJECT_005 migration in
 *  fix_object() for instances and read_object_new() for templates.
 *
 *  After migration, the type structs are the CANONICAL and ONLY data source.
 *  value[] is no longer written on save — it exists solely as a read path
 *  for migrating old data.  New saves use TypeData (JSON) exclusively.
 *
 *  Uses a macro to avoid duplicating the large switch body for both
 *  OBJ_DATA and OBJ_INDEX_DATA (which have identical field layouts).
 */

/* Allocate a type struct if not already present, and set the type flag bit */
#define ALLOC_TYPE(O, field, alloc_fn, type_id) \
    do { \
        if (!(O)->field) { \
            (O)->field = alloc_fn(); \
            TBIT_SET((O)->type_flags, type_id); \
        } \
    } while(0)

#define MIGRATE_VALUES_BODY(O) \
    do { \
        switch ((O)->item_type) { \
        case ITEM_LIGHT: \
            ALLOC_TYPE(O, _light, new_light_data, ITEM_LIGHT); \
            (O)->_light->duration = (int)(O)->value[2]; \
            break; \
        \
        case ITEM_SCROLL: \
            ALLOC_TYPE(O, _scroll, new_scroll_data, ITEM_SCROLL); \
            /* Spells were migrated to spell list in version 1. */ \
            /* max_mana not in value[], leave at default. */ \
            break; \
        \
        case ITEM_WAND: \
            ALLOC_TYPE(O, _wand, new_wand_data, ITEM_WAND); \
            (O)->_wand->max_charges = (int)(O)->value[1]; \
            (O)->_wand->charges     = (int)(O)->value[2]; \
            /* v0=level and v3=spell_sn migrated to spell list in version 1 */ \
            break; \
        \
        case ITEM_STAFF: \
            ALLOC_TYPE(O, _wand, new_wand_data, ITEM_STAFF); \
            (O)->_wand->max_charges = (int)(O)->value[1]; \
            (O)->_wand->charges     = (int)(O)->value[2]; \
            break; \
        \
        case ITEM_WEAPON: \
            ALLOC_TYPE(O, _weapon, new_weapon_data, ITEM_WEAPON); \
            (O)->_weapon->weapon_class = (int16_t)(O)->value[0]; \
            (O)->_weapon->damage.number = (int)(O)->value[1]; \
            (O)->_weapon->damage.size   = (int)(O)->value[2]; \
            (O)->_weapon->damage_type   = (int)(O)->value[3]; \
            (O)->_weapon->flags         = (O)->value[4]; \
            break; \
        \
        case ITEM_ARMOUR: \
            ALLOC_TYPE(O, _armor, new_armor_data, ITEM_ARMOUR); \
            (O)->_armor->protection[0]  = (int16_t)(O)->value[0]; \
            (O)->_armor->protection[1]  = (int16_t)(O)->value[1]; \
            (O)->_armor->protection[2]  = (int16_t)(O)->value[2]; \
            (O)->_armor->protection[3]  = (int16_t)(O)->value[3]; \
            (O)->_armor->armor_strength = (int16_t)(O)->value[4]; \
            break; \
        \
        case ITEM_POTION: \
            ALLOC_TYPE(O, _fluid_container, new_fluid_container_data, ITEM_POTION); \
            (O)->_fluid_container->capacity = (int16_t)(O)->value[5]; \
            (O)->_fluid_container->amount   = (int16_t)(O)->value[5]; \
            /* Spells migrated to spell list in version 1 */ \
            break; \
        \
        case ITEM_FURNITURE: \
            ALLOC_TYPE(O, _furniture, new_furniture_data, ITEM_FURNITURE); \
            (O)->_furniture->max_people = (int)(O)->value[0]; \
            (O)->_furniture->max_weight = (int)(O)->value[1]; \
            (O)->_furniture->flags      = (O)->value[2]; \
            (O)->_furniture->heal_rate  = (int)(O)->value[3]; \
            (O)->_furniture->mana_rate  = (int)(O)->value[4]; \
            (O)->_furniture->move_rate  = (int)(O)->value[5]; \
            break; \
        \
        case ITEM_CONTAINER: \
            ALLOC_TYPE(O, _container, new_container_data, ITEM_CONTAINER); \
            (O)->_container->max_weight       = (int)(O)->value[0]; \
            (O)->_container->flags            = (O)->value[1]; \
            /* v2=key migrated to lock in VERSION_OBJECT_004 */ \
            (O)->_container->max_items        = (int)(O)->value[3]; \
            (O)->_container->weight_multiplier= (int)(O)->value[4]; \
            break; \
        \
        case ITEM_DRINK_CON: \
            ALLOC_TYPE(O, _fluid_container, new_fluid_container_data, ITEM_DRINK_CON); \
            (O)->_fluid_container->capacity = (int16_t)(O)->value[0]; \
            (O)->_fluid_container->amount   = (int16_t)(O)->value[1]; \
            (O)->_fluid_container->liquid   = (int)(O)->value[2]; \
            (O)->_fluid_container->poison   = (int16_t)(O)->value[3]; \
            break; \
        \
        case ITEM_FOOD: \
            ALLOC_TYPE(O, _food, new_food_data, ITEM_FOOD); \
            (O)->_food->hunger = (int)(O)->value[0]; \
            (O)->_food->full   = (int)(O)->value[1]; \
            (O)->_food->poison = (int)(O)->value[3]; \
            (O)->_food->timer  = (int)(O)->value[4]; \
            break; \
        \
        case ITEM_MONEY: \
            ALLOC_TYPE(O, _money, new_money_data, ITEM_MONEY); \
            (O)->_money->silver = (int)(O)->value[0]; \
            (O)->_money->gold   = (int)(O)->value[1]; \
            break; \
        \
        case ITEM_CORPSE_NPC: \
            ALLOC_TYPE(O, _corpse, new_corpse_data, ITEM_CORPSE_NPC); \
            (O)->_corpse->corpse_type  = (int)(O)->value[0]; \
            (O)->_corpse->resurrection = (int)(O)->value[1]; \
            (O)->_corpse->animation    = (int)(O)->value[2]; \
            (O)->_corpse->body_parts   = (O)->value[3]; \
            (O)->_corpse->mobile_vnum  = (O)->value[5]; \
            break; \
        \
        case ITEM_CORPSE_PC: \
            ALLOC_TYPE(O, _corpse, new_corpse_data, ITEM_CORPSE_PC); \
            (O)->_corpse->corpse_type  = (int)(O)->value[0]; \
            (O)->_corpse->resurrection = (int)(O)->value[1]; \
            (O)->_corpse->animation    = (int)(O)->value[2]; \
            (O)->_corpse->body_parts   = (O)->value[3]; \
            (O)->_corpse->mobile_vnum  = (O)->value[5]; \
            break; \
        \
        case ITEM_FOUNTAIN: \
            ALLOC_TYPE(O, _fluid_container, new_fluid_container_data, ITEM_FOUNTAIN); \
            (O)->_fluid_container->capacity    = (int16_t)(O)->value[0]; \
            (O)->_fluid_container->amount      = (int16_t)(O)->value[1]; \
            (O)->_fluid_container->liquid      = (int)(O)->value[2]; \
            (O)->_fluid_container->refill_rate = 1; /* fountains refill */ \
            break; \
        \
        case ITEM_PILL: \
            /* Spells migrated to spell list in version 1. */ \
            /* No type struct currently — pills use obj->spells. */ \
            break; \
        \
        case ITEM_PORTAL: \
            ALLOC_TYPE(O, _portal, new_portal_data, ITEM_PORTAL); \
            (O)->_portal->charges   = (int)(O)->value[0]; \
            (O)->_portal->exit      = (O)->value[1]; \
            (O)->_portal->flags     = (O)->value[2]; \
            (O)->_portal->params[0] = (O)->value[3]; /* destination */ \
            (O)->_portal->params[4] = (O)->value[4]; /* destination area uid (legacy key migrated in V004) */ \
            (O)->_portal->params[1] = (O)->value[5]; /* area/floor */ \
            (O)->_portal->params[2] = (O)->value[6]; /* map_x */ \
            (O)->_portal->params[3] = (O)->value[7]; /* map_y */ \
            break; \
        \
        case ITEM_INSTRUMENT: \
            ALLOC_TYPE(O, _instrument, new_instrument_data, ITEM_INSTRUMENT); \
            (O)->_instrument->type       = (int)(O)->value[0]; \
            (O)->_instrument->flags      = (O)->value[1]; \
            (O)->_instrument->beats_min  = (int)(O)->value[2]; \
            (O)->_instrument->beats_max  = (int)(O)->value[3]; \
            break; \
        \
        case ITEM_SEED: \
            ALLOC_TYPE(O, _seed, new_seed_data, ITEM_SEED); \
            (O)->_seed->growth_time  = (int)(O)->value[0]; \
            (O)->_seed->object_vnum  = (O)->value[1]; \
            break; \
        \
        case ITEM_CART: \
            ALLOC_TYPE(O, _cart, new_cart_data, ITEM_CART); \
            (O)->_cart->capacity          = (int)(O)->value[0]; \
            (O)->_cart->move_delay        = (int16_t)(O)->value[1]; \
            (O)->_cart->min_strength      = (int16_t)(O)->value[2]; \
            (O)->_cart->max_items         = (int)(O)->value[3]; \
            (O)->_cart->weight_multiplier = (int)(O)->value[4]; \
            (O)->_cart->vanish_time       = (int)(O)->value[5]; \
            break; \
        \
        case ITEM_SHIP: \
            ALLOC_TYPE(O, _item_ship, new_item_ship_data, ITEM_SHIP); \
            (O)->_item_ship->weight     = (int)(O)->value[0]; \
            (O)->_item_ship->move_delay = (int)(O)->value[1]; \
            (O)->_item_ship->min_crew   = (int)(O)->value[2]; \
            (O)->_item_ship->capacity   = (int)(O)->value[3]; \
            (O)->_item_ship->max_crew   = (int)(O)->value[4]; \
            (O)->_item_ship->first_room = (O)->value[5]; \
            (O)->_item_ship->hit_points = (int)(O)->value[6]; \
            (O)->_item_ship->max_guns   = (int)(O)->value[7]; \
            break; \
        \
        case ITEM_RANGED_WEAPON: \
            ALLOC_TYPE(O, _weapon, new_weapon_data, ITEM_RANGED_WEAPON); \
            (O)->_weapon->weapon_class  = (int16_t)(O)->value[0]; \
            (O)->_weapon->damage.number = (int)(O)->value[1]; \
            (O)->_weapon->damage.size   = (int)(O)->value[2]; \
            (O)->_weapon->range         = (int)(O)->value[3]; \
            break; \
        \
        case ITEM_SEXTANT: \
            ALLOC_TYPE(O, _sextant, new_sextant_data, ITEM_SEXTANT); \
            (O)->_sextant->accuracy = (int16_t)(O)->value[0]; \
            break; \
        \
        case ITEM_WEAPON_CONTAINER: \
            ALLOC_TYPE(O, _weapon_container, new_weapon_container_data, ITEM_WEAPON_CONTAINER); \
            (O)->_weapon_container->max_weight       = (int)(O)->value[0]; \
            (O)->_weapon_container->weapon_type       = (int)(O)->value[1]; \
            (O)->_weapon_container->max_items         = (int)(O)->value[3]; \
            (O)->_weapon_container->weight_multiplier = (int)(O)->value[4]; \
            break; \
        \
        case ITEM_BOOK: \
            ALLOC_TYPE(O, _book, new_book_data, ITEM_BOOK); \
            (O)->_book->flags = (O)->value[1]; \
            /* v2=key migrated to lock in VERSION_OBJECT_004 */ \
            break; \
        \
        case ITEM_HERB: \
            ALLOC_TYPE(O, _herb, new_herb_data, ITEM_HERB); \
            (O)->_herb->type          = (int)(O)->value[0]; \
            (O)->_herb->healing       = (int)(O)->value[1]; \
            (O)->_herb->regenerative  = (int)(O)->value[2]; \
            (O)->_herb->refreshing    = (int)(O)->value[3]; \
            (O)->_herb->immunity      = (O)->value[4]; \
            (O)->_herb->resistance    = (O)->value[5]; \
            (O)->_herb->vulnerability = (O)->value[6]; \
            (O)->_herb->spell         = (int)(O)->value[7]; \
            break; \
        \
        case ITEM_MIST: \
            ALLOC_TYPE(O, _mist, new_mist_data, ITEM_MIST); \
            (O)->_mist->obscure_objs = (char)(O)->value[0]; \
            (O)->_mist->obscure_mobs = (char)(O)->value[1]; \
            break; \
        \
        case ITEM_TRADE_TYPE: \
            ALLOC_TYPE(O, _trade, new_trade_data, ITEM_TRADE_TYPE); \
            (O)->_trade->trade_type = (int)(O)->value[0]; \
            break; \
        \
        case ITEM_TATTOO: \
            ALLOC_TYPE(O, _tattoo, new_tattoo_data, ITEM_TATTOO); \
            (O)->_tattoo->touches       = (int)(O)->value[0]; \
            (O)->_tattoo->fading_chance  = (int)(O)->value[1]; \
            break; \
        \
        case ITEM_INK: \
            ALLOC_TYPE(O, _ink, new_ink_data, ITEM_INK); \
            (O)->_ink->types[0] = (int16_t)(O)->value[0]; \
            (O)->_ink->types[1] = (int16_t)(O)->value[1]; \
            (O)->_ink->types[2] = (int16_t)(O)->value[2]; \
            break; \
        \
        case ITEM_TELESCOPE: \
            ALLOC_TYPE(O, _telescope, new_telescope_data, ITEM_TELESCOPE); \
            (O)->_telescope->distance     = (int16_t)(O)->value[0]; \
            (O)->_telescope->min_distance = (int16_t)(O)->value[1]; \
            (O)->_telescope->max_distance = (int16_t)(O)->value[2]; \
            (O)->_telescope->bonus_view   = (int16_t)(O)->value[3]; \
            (O)->_telescope->heading      = (int16_t)(O)->value[4]; \
            break; \
        \
        case ITEM_COMPASS: \
            ALLOC_TYPE(O, _compass, new_compass_data, ITEM_COMPASS); \
            (O)->_compass->accuracy = (int16_t)(O)->value[0]; \
            (O)->_compass->wuid     = (O)->value[1]; \
            (O)->_compass->x        = (O)->value[2]; \
            (O)->_compass->y        = (O)->value[3]; \
            break; \
        \
        case ITEM_BODY_PART: \
            ALLOC_TYPE(O, _body_part, new_body_part_data, ITEM_BODY_PART); \
            (O)->_body_part->parts       = (O)->value[0]; \
            (O)->_body_part->race_uid    = (int)(O)->value[1]; \
            (O)->_body_part->char_id[0]  = (O)->value[2]; \
            (O)->_body_part->char_id[1]  = (O)->value[3]; \
            break; \
        \
        default: \
            break; \
        } \
    } while(0)

void obj_migrate_values_to_types(OBJ_DATA *obj)
{
    if (obj == NULL) return;
    MIGRATE_VALUES_BODY(obj);
}

void obj_index_migrate_values_to_types(OBJ_INDEX_DATA *obj)
{
    if (obj == NULL) return;
    MIGRATE_VALUES_BODY(obj);
}

#undef ALLOC_TYPE
#undef MIGRATE_VALUES_BODY

/*
 * ============================================================================
 *  TYPE DATA ALLOCATION
 * ============================================================================
 *
 *  Allocate a default (zeroed) type struct for a given item type.
 *  Used by oedit when setting primary type or adding secondary types.
 *  Does not overwrite an existing struct.
 */

bool obj_index_alloc_type_data(OBJ_INDEX_DATA *pObj, int item_type)
{
    if (pObj == NULL) return false;
    if (item_type <= 0 || item_type >= ITEM__MAX) return false;

    switch (item_type) {
    case ITEM_LIGHT:
        if (!pObj->_light) pObj->_light = new_light_data();
        break;
    case ITEM_SCROLL:
        if (!pObj->_scroll) pObj->_scroll = new_scroll_data();
        break;
    case ITEM_WAND:
    case ITEM_STAFF:
        if (!pObj->_wand) pObj->_wand = new_wand_data();
        break;
    case ITEM_WEAPON:
    case ITEM_RANGED_WEAPON:
        if (!pObj->_weapon) pObj->_weapon = new_weapon_data();
        break;
    case ITEM_ARMOUR:
        if (!pObj->_armor) pObj->_armor = new_armor_data();
        break;
    case ITEM_POTION:
    case ITEM_DRINK_CON:
    case ITEM_FOUNTAIN:
    case ITEM_FLUID_CONTAINER:
        if (!pObj->_fluid_container) pObj->_fluid_container = new_fluid_container_data();
        break;
    case ITEM_FURNITURE:
        if (!pObj->_furniture) pObj->_furniture = new_furniture_data();
        break;
    case ITEM_CONTAINER:
        if (!pObj->_container) pObj->_container = new_container_data();
        break;
    case ITEM_FOOD:
        if (!pObj->_food) pObj->_food = new_food_data();
        break;
    case ITEM_MONEY:
        if (!pObj->_money) pObj->_money = new_money_data();
        break;
    case ITEM_CORPSE_NPC:
    case ITEM_CORPSE_PC:
    case ITEM_CORPSE:
        if (!pObj->_corpse) pObj->_corpse = new_corpse_data();
        break;
    case ITEM_PORTAL:
        if (!pObj->_portal) pObj->_portal = new_portal_data();
        break;
    case ITEM_MAP:
        if (!pObj->_map) pObj->_map = new_map_data();
        break;
    case ITEM_JEWELRY:
        if (!pObj->_jewelry) pObj->_jewelry = new_jewelry_data();
        break;
    case ITEM_INSTRUMENT:
        if (!pObj->_instrument) pObj->_instrument = new_instrument_data();
        break;
    case ITEM_SEED:
        if (!pObj->_seed) pObj->_seed = new_seed_data();
        break;
    case ITEM_CART:
        if (!pObj->_cart) pObj->_cart = new_cart_data();
        break;
    case ITEM_SHIP:
        if (!pObj->_item_ship) pObj->_item_ship = new_item_ship_data();
        break;
    case ITEM_SEXTANT:
        if (!pObj->_sextant) pObj->_sextant = new_sextant_data();
        break;
    case ITEM_WEAPON_CONTAINER:
        if (!pObj->_weapon_container) pObj->_weapon_container = new_weapon_container_data();
        break;
    case ITEM_BOOK:
        if (!pObj->_book) pObj->_book = new_book_data();
        break;
    case ITEM_HERB:
        if (!pObj->_herb) pObj->_herb = new_herb_data();
        break;
    case ITEM_MIST:
        if (!pObj->_mist) pObj->_mist = new_mist_data();
        break;
    case ITEM_TRADE_TYPE:
        if (!pObj->_trade) pObj->_trade = new_trade_data();
        break;
    case ITEM_TATTOO:
        if (!pObj->_tattoo) pObj->_tattoo = new_tattoo_data();
        break;
    case ITEM_INK:
        if (!pObj->_ink) pObj->_ink = new_ink_data();
        break;
    case ITEM_TELESCOPE:
        if (!pObj->_telescope) pObj->_telescope = new_telescope_data();
        break;
    case ITEM_COMPASS:
        if (!pObj->_compass) pObj->_compass = new_compass_data();
        break;
    case ITEM_BODY_PART:
        if (!pObj->_body_part) pObj->_body_part = new_body_part_data();
        break;
    case ITEM_TOOL:
        if (!pObj->_tool) pObj->_tool = new_tool_data();
        break;
    case ITEM_PAGE:
        if (!pObj->_page) pObj->_page = new_page_data();
        break;
    default:
        return false;
    }

    TBIT_SET(pObj->type_flags, item_type);
    return true;
}

void obj_index_set_primary_type(OBJ_INDEX_DATA *pObj, int item_type)
{
    if (pObj == NULL) return;

    /* Free all existing type data and clear type_flags */
    obj_index_free_type_data(pObj);

    /* Clear legacy value array */
    for (int i = 0; i < 8; i++)
        pObj->value[i] = 0;

    /* Set the new primary type */
    pObj->item_type = item_type;

    /* Allocate default type struct */
    obj_index_alloc_type_data(pObj, item_type);
}

bool obj_index_remove_type(OBJ_INDEX_DATA *pObj, int item_type)
{
    if (pObj == NULL) return false;

    /* Cannot remove the primary type */
    if (pObj->item_type == item_type)
        return false;

    /* Must actually have this type */
    if (!TBIT_TST(pObj->type_flags, item_type))
        return false;

    /* Free the specific type struct */
    switch (item_type) {
    case ITEM_LIGHT:
        if (pObj->_light) { free_light_data(pObj->_light); pObj->_light = NULL; }
        break;
    case ITEM_SCROLL:
        if (pObj->_scroll) { free_scroll_data(pObj->_scroll); pObj->_scroll = NULL; }
        break;
    case ITEM_WAND:
    case ITEM_STAFF:
        if (pObj->_wand) { free_wand_data(pObj->_wand); pObj->_wand = NULL; }
        break;
    case ITEM_WEAPON:
    case ITEM_RANGED_WEAPON:
        if (pObj->_weapon) { free_weapon_data(pObj->_weapon); pObj->_weapon = NULL; }
        break;
    case ITEM_ARMOUR:
        if (pObj->_armor) { free_armor_data(pObj->_armor); pObj->_armor = NULL; }
        break;
    case ITEM_POTION:
    case ITEM_DRINK_CON:
    case ITEM_FOUNTAIN:
    case ITEM_FLUID_CONTAINER:
        if (pObj->_fluid_container) { free_fluid_container_data(pObj->_fluid_container); pObj->_fluid_container = NULL; }
        break;
    case ITEM_FURNITURE:
        if (pObj->_furniture) { free_furniture_data(pObj->_furniture); pObj->_furniture = NULL; }
        break;
    case ITEM_CONTAINER:
        if (pObj->_container) { free_container_data(pObj->_container); pObj->_container = NULL; }
        break;
    case ITEM_FOOD:
        if (pObj->_food) { free_food_data(pObj->_food); pObj->_food = NULL; }
        break;
    case ITEM_MONEY:
        if (pObj->_money) { free_money_data(pObj->_money); pObj->_money = NULL; }
        break;
    case ITEM_CORPSE_NPC:
    case ITEM_CORPSE_PC:
    case ITEM_CORPSE:
        if (pObj->_corpse) { free_corpse_data(pObj->_corpse); pObj->_corpse = NULL; }
        break;
    case ITEM_PORTAL:
        if (pObj->_portal) { free_portal_data(pObj->_portal); pObj->_portal = NULL; }
        break;
    case ITEM_MAP:
        if (pObj->_map) { free_map_data(pObj->_map); pObj->_map = NULL; }
        break;
    case ITEM_JEWELRY:
        if (pObj->_jewelry) { free_jewelry_data(pObj->_jewelry); pObj->_jewelry = NULL; }
        break;
    case ITEM_INSTRUMENT:
        if (pObj->_instrument) { free_instrument_data(pObj->_instrument); pObj->_instrument = NULL; }
        break;
    case ITEM_SEED:
        if (pObj->_seed) { free_seed_data(pObj->_seed); pObj->_seed = NULL; }
        break;
    case ITEM_CART:
        if (pObj->_cart) { free_cart_data(pObj->_cart); pObj->_cart = NULL; }
        break;
    case ITEM_SHIP:
        if (pObj->_item_ship) { free_item_ship_data(pObj->_item_ship); pObj->_item_ship = NULL; }
        break;
    case ITEM_SEXTANT:
        if (pObj->_sextant) { free_sextant_data(pObj->_sextant); pObj->_sextant = NULL; }
        break;
    case ITEM_WEAPON_CONTAINER:
        if (pObj->_weapon_container) { free_weapon_container_data(pObj->_weapon_container); pObj->_weapon_container = NULL; }
        break;
    case ITEM_BOOK:
        if (pObj->_book) { free_book_data(pObj->_book); pObj->_book = NULL; }
        break;
    case ITEM_HERB:
        if (pObj->_herb) { free_herb_data(pObj->_herb); pObj->_herb = NULL; }
        break;
    case ITEM_MIST:
        if (pObj->_mist) { free_mist_data(pObj->_mist); pObj->_mist = NULL; }
        break;
    case ITEM_TRADE_TYPE:
        if (pObj->_trade) { free_trade_data(pObj->_trade); pObj->_trade = NULL; }
        break;
    case ITEM_TATTOO:
        if (pObj->_tattoo) { free_tattoo_data(pObj->_tattoo); pObj->_tattoo = NULL; }
        break;
    case ITEM_INK:
        if (pObj->_ink) { free_ink_data(pObj->_ink); pObj->_ink = NULL; }
        break;
    case ITEM_TELESCOPE:
        if (pObj->_telescope) { free_telescope_data(pObj->_telescope); pObj->_telescope = NULL; }
        break;
    case ITEM_COMPASS:
        if (pObj->_compass) { free_compass_data(pObj->_compass); pObj->_compass = NULL; }
        break;
    case ITEM_BODY_PART:
        if (pObj->_body_part) { free_body_part_data(pObj->_body_part); pObj->_body_part = NULL; }
        break;
    case ITEM_TOOL:
        if (pObj->_tool) { free_tool_data(pObj->_tool); pObj->_tool = NULL; }
        break;
    case ITEM_PAGE:
        if (pObj->_page) { free_page_data(pObj->_page); pObj->_page = NULL; }
        break;
    default:
        return false;
    }

    TBIT_CLR(pObj->type_flags, item_type);
    return true;
}

void obj_index_copy_type_data(OBJ_INDEX_DATA *dst, OBJ_INDEX_DATA *src)
{
    if (dst == NULL || src == NULL) return;

    dst->type_flags = src->type_flags;

    dst->_armor           = copy_armor_data(src->_armor);
    dst->_body_part       = copy_body_part_data(src->_body_part);
    dst->_book            = copy_book_data(src->_book);
    dst->_cart            = copy_cart_data(src->_cart);
    dst->_compass         = copy_compass_data(src->_compass);
    dst->_container       = copy_container_data(src->_container);
    dst->_corpse          = copy_corpse_data(src->_corpse);
    dst->_fluid_container = copy_fluid_container_data(src->_fluid_container);
    dst->_food            = copy_food_data(src->_food);
    dst->_furniture       = copy_furniture_data(src->_furniture);
    dst->_herb            = copy_herb_data(src->_herb);
    dst->_ink             = copy_ink_data(src->_ink);
    dst->_instrument      = copy_instrument_data(src->_instrument);
    dst->_item_ship       = copy_item_ship_data(src->_item_ship);
    dst->_jewelry         = copy_jewelry_data(src->_jewelry);
    dst->_light           = copy_light_data(src->_light);
    dst->_map             = copy_map_data(src->_map);
    dst->_mist            = copy_mist_data(src->_mist);
    dst->_money           = copy_money_data(src->_money);
    dst->_page            = copy_page_data(src->_page);
    dst->_portal          = copy_portal_data(src->_portal);
    dst->_scroll          = copy_scroll_data(src->_scroll);
    dst->_seed            = copy_seed_data(src->_seed);
    dst->_sextant         = copy_sextant_data(src->_sextant);
    dst->_tattoo          = copy_tattoo_data(src->_tattoo);
    dst->_telescope       = copy_telescope_data(src->_telescope);
    dst->_tool            = copy_tool_data(src->_tool);
    dst->_trade           = copy_trade_data(src->_trade);
    dst->_wand            = copy_wand_data(src->_wand);
    dst->_weapon_container= copy_weapon_container_data(src->_weapon_container);
    dst->_weapon          = copy_weapon_data(src->_weapon);
}
