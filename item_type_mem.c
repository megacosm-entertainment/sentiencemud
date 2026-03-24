/***************************************************************************
 *  Sentience MUD                                                          *
 *  Item Type Memory Management                                            *
 *                                                                         *
 *  Provides new_*, copy_*, and free_* functions for all type-specific      *
 *  data structs. Uses the standard free-list pattern with VALIDATE/        *
 *  INVALIDATE for lifecycle tracking.                                      *
 ***************************************************************************/

#include "merc.h"
#include "recycle.h"
#include "item_types.h"

/*
 * Free-list heads for each type data struct.
 */
static ARMOR_DATA *             armor_data_free;
static BODY_PART_DATA *         body_part_data_free;
static BOOK_PAGE *              book_page_free;
static BOOK_DATA *              book_data_free;
static CART_DATA *              cart_data_free;
static COMPASS_DATA *           compass_data_free;
static CONTAINER_DATA *         container_data_free;
static CORPSE_DATA *            corpse_data_free;
static FLUID_CONTAINER_DATA *   fluid_container_data_free;
static FOOD_BUFF_DATA *         food_buff_data_free;
static FOOD_DATA *              food_data_free;
static FURNITURE_DATA *         furniture_data_free;
static HERB_DATA *              herb_data_free;
static INK_DATA *               ink_data_free;
static INSTRUMENT_DATA *        instrument_data_free;
static ITEM_SHIP_DATA *         item_ship_data_free;
static SHIP_MODULE_DATA *       ship_module_data_free;
static JEWELRY_DATA *           jewelry_data_free;
static LIGHT_DATA *             light_data_free;
static MAP_DATA *               map_data_free;
static MIST_DATA *              mist_data_free;
static MONEY_DATA *             money_data_free;
static PAGE_DATA *              page_data_free;
static PORTAL_DATA *            portal_data_free;
static SCROLL_DATA *            scroll_data_free;
static SEED_DATA *              seed_data_free;
static SEXTANT_DATA *           sextant_data_free;
static TATTOO_DATA *            tattoo_data_free;
static TELESCOPE_DATA *         telescope_data_free;
static TOOL_DATA *              tool_data_free;
static TRADE_DATA *             trade_data_free;
static WAND_DATA *              wand_data_free;
static WEAPON_CONTAINER_DATA *  weapon_container_data_free;
static WEAPON_DATA *            weapon_data_free;

/*
 * Helper: copy a linked list of SPELL_DATA.
 * Returns a new linked list (not LLIST, just ->next chain) with copies.
 */
static SPELL_DATA *copy_spell_chain(SPELL_DATA *src)
{
    SPELL_DATA *head = NULL;
    SPELL_DATA **tail = &head;

    for (SPELL_DATA *s = src; s != NULL; s = s->next)
    {
        SPELL_DATA *copy = new_spell();
        copy->sn    = s->sn;
        copy->level = s->level;
        copy->repop = s->repop;
        copy->next  = NULL;

        *tail = copy;
        tail  = &copy->next;
    }

    return head;
}

/*
 * Helper: free a linked list of SPELL_DATA.
 */
static void free_spell_chain(SPELL_DATA *chain)
{
    SPELL_DATA *s, *next;
    for (s = chain; s != NULL; s = next)
    {
        next = s->next;
        free_spell(s);
    }
}

/*
 * Helper: copy a LOCK_STATE (deep copy).
 */
static LOCK_STATE *copy_lock(LOCK_STATE *src)
{
    if (src == NULL) return NULL;

    LOCK_STATE *lock   = new_lock_state();
    lock->key_load     = src->key_load;
    lock->key_wnum     = src->key_wnum;
    lock->pick_chance  = src->pick_chance;
    lock->flags        = src->flags;
    /* special_keys is not owned by the lock state, so just copy the pointer */
    lock->special_keys = src->special_keys;

    return lock;
}


/* ========================================================================== */
/*  ARMOR                                                                     */
/* ========================================================================== */

ARMOR_DATA *new_armor_data(void)
{
    ARMOR_DATA *data;

    if (armor_data_free)
    {
        data = armor_data_free;
        armor_data_free = armor_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(ARMOR_DATA));
    }

    memset(data, 0, sizeof(ARMOR_DATA));
    VALIDATE(data);
    return data;
}

ARMOR_DATA *copy_armor_data(ARMOR_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    ARMOR_DATA *data = new_armor_data();
    data->armor_type     = src->armor_type;
    data->armor_strength = src->armor_strength;
    memcpy(data->protection, src->protection, sizeof(data->protection));
    return data;
}

void free_armor_data(ARMOR_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = armor_data_free;
    armor_data_free = data;
}


/* ========================================================================== */
/*  BOOK PAGE                                                                 */
/* ========================================================================== */

BOOK_PAGE *new_book_page(void)
{
    BOOK_PAGE *data;

    if (book_page_free)
    {
        data = book_page_free;
        book_page_free = book_page_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(BOOK_PAGE));
    }

    memset(data, 0, sizeof(BOOK_PAGE));
    VALIDATE(data);
    return data;
}

BOOK_PAGE *copy_book_page(BOOK_PAGE *src)
{
    if (!IS_VALID(src)) return NULL;

    BOOK_PAGE *data = new_book_page();
    data->page_no = src->page_no;
    data->title   = str_dup(src->title ? src->title : "");
    data->text    = str_dup(src->text ? src->text : "");
    return data;
}

void free_book_page(BOOK_PAGE *data)
{
    if (!IS_VALID(data)) return;

    if (data->title) free_string(data->title);
    if (data->text)  free_string(data->text);

    INVALIDATE(data);
    data->next = book_page_free;
    book_page_free = data;
}


/* ========================================================================== */
/*  BOOK                                                                      */
/* ========================================================================== */

BOOK_DATA *new_book_data(void)
{
    BOOK_DATA *data;

    if (book_data_free)
    {
        data = book_data_free;
        book_data_free = book_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(BOOK_DATA));
    }

    memset(data, 0, sizeof(BOOK_DATA));
    data->pages = list_create(false);
    VALIDATE(data);
    return data;
}

BOOK_DATA *copy_book_data(BOOK_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    BOOK_DATA *data = new_book_data();
    data->name         = str_dup(src->name ? src->name : "");
    data->short_descr  = str_dup(src->short_descr ? src->short_descr : "");
    data->flags        = src->flags;
    data->current_page = src->current_page;
    data->open_page    = src->open_page;
    data->lock         = copy_lock(src->lock);

    /* Copy pages */
    if (src->pages)
    {
        ITERATOR it;
        BOOK_PAGE *page;
        iterator_start(&it, src->pages);
        while ((page = (BOOK_PAGE *)iterator_nextdata(&it)))
        {
            BOOK_PAGE *pcopy = copy_book_page(page);
            if (pcopy)
                list_appendlink(data->pages, pcopy);
        }
        iterator_stop(&it);
    }

    return data;
}

void free_book_data(BOOK_DATA *data)
{
    if (!IS_VALID(data)) return;

    if (data->name)        free_string(data->name);
    if (data->short_descr) free_string(data->short_descr);
    if (data->lock)        free_lock_state(data->lock);

    if (data->pages)
    {
        /* Free each page in the list */
        ITERATOR it;
        BOOK_PAGE *page;
        iterator_start(&it, data->pages);
        while ((page = (BOOK_PAGE *)iterator_nextdata(&it)))
        {
            free_book_page(page);
        }
        iterator_stop(&it);
        list_destroy(data->pages);
    }

    INVALIDATE(data);
    data->next = book_data_free;
    book_data_free = data;
}


/* ========================================================================== */
/*  CART                                                                      */
/* ========================================================================== */

CART_DATA *new_cart_data(void)
{
    CART_DATA *data;

    if (cart_data_free)
    {
        data = cart_data_free;
        cart_data_free = cart_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(CART_DATA));
    }

    memset(data, 0, sizeof(CART_DATA));
    VALIDATE(data);
    return data;
}

CART_DATA *copy_cart_data(CART_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    CART_DATA *data = new_cart_data();
    data->flags            = src->flags;
    data->min_strength     = src->min_strength;
    data->move_delay       = src->move_delay;
    data->capacity         = src->capacity;
    data->weight_multiplier = src->weight_multiplier;
    data->vanish_time      = src->vanish_time;
    return data;
}

void free_cart_data(CART_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = cart_data_free;
    cart_data_free = data;
}


/* ========================================================================== */
/*  COMPASS                                                                   */
/* ========================================================================== */

COMPASS_DATA *new_compass_data(void)
{
    COMPASS_DATA *data;

    if (compass_data_free)
    {
        data = compass_data_free;
        compass_data_free = compass_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(COMPASS_DATA));
    }

    memset(data, 0, sizeof(COMPASS_DATA));
    VALIDATE(data);
    return data;
}

COMPASS_DATA *copy_compass_data(COMPASS_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    COMPASS_DATA *data = new_compass_data();
    data->accuracy = src->accuracy;
    data->wuid     = src->wuid;
    data->x        = src->x;
    data->y        = src->y;
    return data;
}

void free_compass_data(COMPASS_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = compass_data_free;
    compass_data_free = data;
}


/* ========================================================================== */
/*  CONTAINER                                                                 */
/* ========================================================================== */

CONTAINER_DATA *new_container_data(void)
{
    CONTAINER_DATA *data;

    if (container_data_free)
    {
        data = container_data_free;
        container_data_free = container_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(CONTAINER_DATA));
    }

    memset(data, 0, sizeof(CONTAINER_DATA));
    data->whitelist         = list_create(false);
    data->blacklist         = list_create(false);
    data->weight_multiplier = 100;
    data->max_weight        = -1;   /* -1 = unlimited */
    data->max_volume        = -1;
    VALIDATE(data);
    return data;
}

CONTAINER_DATA *copy_container_data(CONTAINER_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    CONTAINER_DATA *data = new_container_data();
    data->name              = str_dup(src->name ? src->name : "");
    data->short_descr       = str_dup(src->short_descr ? src->short_descr : "");
    data->flags             = src->flags;
    data->max_weight        = src->max_weight;
    data->weight_multiplier = src->weight_multiplier;
    data->max_volume        = src->max_volume;
    data->max_items         = src->max_items;
    data->lock              = copy_lock(src->lock);

    /* Copy filter lists */
    if (src->whitelist)
    {
        ITERATOR it;
        CONTAINER_FILTER *f;
        iterator_start(&it, src->whitelist);
        while ((f = (CONTAINER_FILTER *)iterator_nextdata(&it)))
        {
            CONTAINER_FILTER *fc = alloc_mem(sizeof(CONTAINER_FILTER));
            fc->item_type = f->item_type;
            fc->sub_type  = f->sub_type;
            list_appendlink(data->whitelist, fc);
        }
        iterator_stop(&it);
    }

    if (src->blacklist)
    {
        ITERATOR it;
        CONTAINER_FILTER *f;
        iterator_start(&it, src->blacklist);
        while ((f = (CONTAINER_FILTER *)iterator_nextdata(&it)))
        {
            CONTAINER_FILTER *fc = alloc_mem(sizeof(CONTAINER_FILTER));
            fc->item_type = f->item_type;
            fc->sub_type  = f->sub_type;
            list_appendlink(data->blacklist, fc);
        }
        iterator_stop(&it);
    }

    return data;
}

void free_container_data(CONTAINER_DATA *data)
{
    if (!IS_VALID(data)) return;

    if (data->name)        free_string(data->name);
    if (data->short_descr) free_string(data->short_descr);
    if (data->lock)        free_lock_state(data->lock);

    if (data->whitelist)
    {
        ITERATOR it;
        CONTAINER_FILTER *f;
        iterator_start(&it, data->whitelist);
        while ((f = (CONTAINER_FILTER *)iterator_nextdata(&it)))
        {
            free_mem(f, sizeof(CONTAINER_FILTER));
        }
        iterator_stop(&it);
        list_destroy(data->whitelist);
    }

    if (data->blacklist)
    {
        ITERATOR it;
        CONTAINER_FILTER *f;
        iterator_start(&it, data->blacklist);
        while ((f = (CONTAINER_FILTER *)iterator_nextdata(&it)))
        {
            free_mem(f, sizeof(CONTAINER_FILTER));
        }
        iterator_stop(&it);
        list_destroy(data->blacklist);
    }

    INVALIDATE(data);
    data->next = container_data_free;
    container_data_free = data;
}


/* ========================================================================== */
/*  FLUID CONTAINER                                                           */
/* ========================================================================== */

FLUID_CONTAINER_DATA *new_fluid_container_data(void)
{
    FLUID_CONTAINER_DATA *data;

    if (fluid_container_data_free)
    {
        data = fluid_container_data_free;
        fluid_container_data_free = fluid_container_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(FLUID_CONTAINER_DATA));
    }

    memset(data, 0, sizeof(FLUID_CONTAINER_DATA));
    VALIDATE(data);
    return data;
}

FLUID_CONTAINER_DATA *copy_fluid_container_data(FLUID_CONTAINER_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    FLUID_CONTAINER_DATA *data = new_fluid_container_data();
    data->name         = str_dup(src->name ? src->name : "");
    data->short_descr  = str_dup(src->short_descr ? src->short_descr : "");
    data->flags        = src->flags;
    data->liquid       = src->liquid;
    data->capacity     = src->capacity;
    data->amount       = src->amount;
    data->refill_rate  = src->refill_rate;
    data->poison       = src->poison;
    data->lock         = copy_lock(src->lock);
    data->spells       = copy_spell_chain(src->spells);
    return data;
}

void free_fluid_container_data(FLUID_CONTAINER_DATA *data)
{
    if (!IS_VALID(data)) return;

    if (data->name)        free_string(data->name);
    if (data->short_descr) free_string(data->short_descr);
    if (data->lock)        free_lock_state(data->lock);
    free_spell_chain(data->spells);

    INVALIDATE(data);
    data->next = fluid_container_data_free;
    fluid_container_data_free = data;
}


/* ========================================================================== */
/*  FOOD BUFF                                                                 */
/* ========================================================================== */

FOOD_BUFF_DATA *new_food_buff_data(void)
{
    FOOD_BUFF_DATA *data;

    if (food_buff_data_free)
    {
        data = food_buff_data_free;
        food_buff_data_free = food_buff_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(FOOD_BUFF_DATA));
    }

    memset(data, 0, sizeof(FOOD_BUFF_DATA));
    VALIDATE(data);
    return data;
}

FOOD_BUFF_DATA *copy_food_buff_data(FOOD_BUFF_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    FOOD_BUFF_DATA *data = new_food_buff_data();
    data->where     = src->where;
    data->location  = src->location;
    data->modifier  = src->modifier;
    data->bitvector = src->bitvector;
    data->bitvector2 = src->bitvector2;
    return data;
}

void free_food_buff_data(FOOD_BUFF_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = food_buff_data_free;
    food_buff_data_free = data;
}


/* ========================================================================== */
/*  FOOD                                                                      */
/* ========================================================================== */

FOOD_DATA *new_food_data(void)
{
    FOOD_DATA *data;

    if (food_data_free)
    {
        data = food_data_free;
        food_data_free = food_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(FOOD_DATA));
    }

    memset(data, 0, sizeof(FOOD_DATA));
    VALIDATE(data);
    return data;
}

FOOD_DATA *copy_food_data(FOOD_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    FOOD_DATA *data = new_food_data();
    ITERATOR it;
    FOOD_BUFF_DATA *buff;

    data->hunger = src->hunger;
    data->full   = src->full;
    data->poison = src->poison;
    data->timer  = src->timer;

    if (src->buffs)
    {
        data->buffs = list_createx(false, NULL, NULL);
        iterator_start(&it, src->buffs);
        while ((buff = (FOOD_BUFF_DATA *)iterator_nextdata(&it)))
        {
            FOOD_BUFF_DATA *copy = copy_food_buff_data(buff);
            if (copy)
                list_appendlink(data->buffs, copy);
        }
        iterator_stop(&it);
    }

    return data;
}

void free_food_data(FOOD_DATA *data)
{
    if (!IS_VALID(data)) return;

    if (data->buffs)
    {
        ITERATOR it;
        FOOD_BUFF_DATA *buff;

        iterator_start(&it, data->buffs);
        while ((buff = (FOOD_BUFF_DATA *)iterator_nextdata(&it)))
            free_food_buff_data(buff);
        iterator_stop(&it);

        list_destroy(data->buffs);
        data->buffs = NULL;
    }

    INVALIDATE(data);
    data->next = food_data_free;
    food_data_free = data;
}


/* ========================================================================== */
/*  FURNITURE                                                                 */
/* ========================================================================== */

FURNITURE_DATA *new_furniture_data(void)
{
    FURNITURE_DATA *data;

    if (furniture_data_free)
    {
        data = furniture_data_free;
        furniture_data_free = furniture_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(FURNITURE_DATA));
    }

    memset(data, 0, sizeof(FURNITURE_DATA));
    VALIDATE(data);
    return data;
}

FURNITURE_DATA *copy_furniture_data(FURNITURE_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    FURNITURE_DATA *data = new_furniture_data();
    data->flags      = src->flags;
    data->max_people = src->max_people;
    data->max_weight = src->max_weight;
    data->heal_rate  = src->heal_rate;
    data->mana_rate  = src->mana_rate;
    data->move_rate  = src->move_rate;
    data->standing   = src->standing;
    data->sitting    = src->sitting;
    data->resting    = src->resting;
    data->sleeping   = src->sleeping;
    return data;
}

void free_furniture_data(FURNITURE_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = furniture_data_free;
    furniture_data_free = data;
}


/* ========================================================================== */
/*  INK                                                                       */
/* ========================================================================== */

INK_DATA *new_ink_data(void)
{
    INK_DATA *data;

    if (ink_data_free)
    {
        data = ink_data_free;
        ink_data_free = ink_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(INK_DATA));
    }

    memset(data, 0, sizeof(INK_DATA));
    VALIDATE(data);
    return data;
}

INK_DATA *copy_ink_data(INK_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    INK_DATA *data = new_ink_data();
    memcpy(data->types, src->types, sizeof(data->types));
    memcpy(data->amounts, src->amounts, sizeof(data->amounts));
    return data;
}

void free_ink_data(INK_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = ink_data_free;
    ink_data_free = data;
}


/* ========================================================================== */
/*  INSTRUMENT                                                                */
/* ========================================================================== */

INSTRUMENT_DATA *new_instrument_data(void)
{
    INSTRUMENT_DATA *data;

    if (instrument_data_free)
    {
        data = instrument_data_free;
        instrument_data_free = instrument_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(INSTRUMENT_DATA));
    }

    memset(data, 0, sizeof(INSTRUMENT_DATA));
    VALIDATE(data);
    return data;
}

INSTRUMENT_DATA *copy_instrument_data(INSTRUMENT_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    INSTRUMENT_DATA *data = new_instrument_data();
    data->type      = src->type;
    data->flags     = src->flags;
    data->mana_min  = src->mana_min;
    data->mana_max  = src->mana_max;
    data->beats_min = src->beats_min;
    data->beats_max = src->beats_max;
    memcpy(data->reservoirs, src->reservoirs, sizeof(data->reservoirs));
    return data;
}

void free_instrument_data(INSTRUMENT_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = instrument_data_free;
    instrument_data_free = data;
}


/* ========================================================================== */
/*  JEWELRY                                                                   */
/* ========================================================================== */

JEWELRY_DATA *new_jewelry_data(void)
{
    JEWELRY_DATA *data;

    if (jewelry_data_free)
    {
        data = jewelry_data_free;
        jewelry_data_free = jewelry_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(JEWELRY_DATA));
    }

    memset(data, 0, sizeof(JEWELRY_DATA));
    VALIDATE(data);
    return data;
}

JEWELRY_DATA *copy_jewelry_data(JEWELRY_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    JEWELRY_DATA *data = new_jewelry_data();
    data->max_mana = src->max_mana;
    data->spells   = copy_spell_chain(src->spells);
    return data;
}

void free_jewelry_data(JEWELRY_DATA *data)
{
    if (!IS_VALID(data)) return;

    free_spell_chain(data->spells);

    INVALIDATE(data);
    data->next = jewelry_data_free;
    jewelry_data_free = data;
}


/* ========================================================================== */
/*  LIGHT                                                                     */
/* ========================================================================== */

LIGHT_DATA *new_light_data(void)
{
    LIGHT_DATA *data;

    if (light_data_free)
    {
        data = light_data_free;
        light_data_free = light_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(LIGHT_DATA));
    }

    memset(data, 0, sizeof(LIGHT_DATA));
    data->duration = -1; /* Infinite by default */
    VALIDATE(data);
    return data;
}

LIGHT_DATA *copy_light_data(LIGHT_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    LIGHT_DATA *data = new_light_data();
    data->flags    = src->flags;
    data->duration = src->duration;
    return data;
}

void free_light_data(LIGHT_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = light_data_free;
    light_data_free = data;
}


/* ========================================================================== */
/*  MAP                                                                       */
/* ========================================================================== */

MAP_DATA *new_map_data(void)
{
    MAP_DATA *data;

    if (map_data_free)
    {
        data = map_data_free;
        map_data_free = map_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(MAP_DATA));
    }

    memset(data, 0, sizeof(MAP_DATA));
    VALIDATE(data);
    return data;
}

MAP_DATA *copy_map_data(MAP_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    MAP_DATA *data = new_map_data();
    data->wuid = src->wuid;
    data->x    = src->x;
    data->y    = src->y;
    /* Waypoints list is not deep-copied for now (shared references or NULL) */
    data->waypoints = NULL;
    return data;
}

void free_map_data(MAP_DATA *data)
{
    if (!IS_VALID(data)) return;

    if (data->waypoints)
    {
        list_destroy(data->waypoints);
        data->waypoints = NULL;
    }

    INVALIDATE(data);
    data->next = map_data_free;
    map_data_free = data;
}


/* ========================================================================== */
/*  MIST                                                                      */
/* ========================================================================== */

MIST_DATA *new_mist_data(void)
{
    MIST_DATA *data;

    if (mist_data_free)
    {
        data = mist_data_free;
        mist_data_free = mist_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(MIST_DATA));
    }

    memset(data, 0, sizeof(MIST_DATA));
    VALIDATE(data);
    return data;
}

MIST_DATA *copy_mist_data(MIST_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    MIST_DATA *data = new_mist_data();
    data->obscure_mobs = src->obscure_mobs;
    data->obscure_objs = src->obscure_objs;
    data->obscure_room = src->obscure_room;
    data->icy     = src->icy;
    data->fiery   = src->fiery;
    data->acidic  = src->acidic;
    data->stink   = src->stink;
    data->wither  = src->wither;
    data->toxic   = src->toxic;
    data->shock   = src->shock;
    data->fog     = src->fog;
    data->sleep   = src->sleep;
    return data;
}

void free_mist_data(MIST_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = mist_data_free;
    mist_data_free = data;
}


/* ========================================================================== */
/*  MONEY                                                                     */
/* ========================================================================== */

MONEY_DATA *new_money_data(void)
{
    MONEY_DATA *data;

    if (money_data_free)
    {
        data = money_data_free;
        money_data_free = money_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(MONEY_DATA));
    }

    memset(data, 0, sizeof(MONEY_DATA));
    VALIDATE(data);
    return data;
}

MONEY_DATA *copy_money_data(MONEY_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    MONEY_DATA *data = new_money_data();
    data->silver = src->silver;
    data->gold   = src->gold;
    return data;
}

void free_money_data(MONEY_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = money_data_free;
    money_data_free = data;
}


/* ========================================================================== */
/*  PAGE                                                                      */
/* ========================================================================== */

PAGE_DATA *new_page_data(void)
{
    PAGE_DATA *data;

    if (page_data_free)
    {
        data = page_data_free;
        page_data_free = page_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(PAGE_DATA));
    }

    memset(data, 0, sizeof(PAGE_DATA));
    VALIDATE(data);
    return data;
}

PAGE_DATA *copy_page_data(PAGE_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    PAGE_DATA *data = new_page_data();
    data->page_no = src->page_no;
    data->title   = str_dup(src->title ? src->title : "");
    data->text    = str_dup(src->text ? src->text : "");
    return data;
}

void free_page_data(PAGE_DATA *data)
{
    if (!IS_VALID(data)) return;

    if (data->title) free_string(data->title);
    if (data->text)  free_string(data->text);

    INVALIDATE(data);
    data->next = page_data_free;
    page_data_free = data;
}


/* ========================================================================== */
/*  PORTAL                                                                    */
/* ========================================================================== */

PORTAL_DATA *new_portal_data(void)
{
    PORTAL_DATA *data;

    if (portal_data_free)
    {
        data = portal_data_free;
        portal_data_free = portal_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(PORTAL_DATA));
    }

    memset(data, 0, sizeof(PORTAL_DATA));
    data->charges = -1; /* Infinite by default */
    VALIDATE(data);
    return data;
}

PORTAL_DATA *copy_portal_data(PORTAL_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    PORTAL_DATA *data = new_portal_data();
    data->name        = str_dup(src->name ? src->name : "");
    data->short_descr = str_dup(src->short_descr ? src->short_descr : "");
    data->exit        = src->exit;
    data->flags       = src->flags;
    data->charges     = src->charges;
    data->type        = src->type;
    memcpy(data->params, src->params, sizeof(data->params));
    data->lock        = copy_lock(src->lock);
    return data;
}

void free_portal_data(PORTAL_DATA *data)
{
    if (!IS_VALID(data)) return;

    if (data->name)        free_string(data->name);
    if (data->short_descr) free_string(data->short_descr);
    if (data->lock)        free_lock_state(data->lock);

    INVALIDATE(data);
    data->next = portal_data_free;
    portal_data_free = data;
}


/* ========================================================================== */
/*  SCROLL                                                                    */
/* ========================================================================== */

SCROLL_DATA *new_scroll_data(void)
{
    SCROLL_DATA *data;

    if (scroll_data_free)
    {
        data = scroll_data_free;
        scroll_data_free = scroll_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(SCROLL_DATA));
    }

    memset(data, 0, sizeof(SCROLL_DATA));
    VALIDATE(data);
    return data;
}

SCROLL_DATA *copy_scroll_data(SCROLL_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    SCROLL_DATA *data = new_scroll_data();
    data->max_mana = src->max_mana;
    data->flags    = src->flags;
    data->spells   = copy_spell_chain(src->spells);
    return data;
}

void free_scroll_data(SCROLL_DATA *data)
{
    if (!IS_VALID(data)) return;

    free_spell_chain(data->spells);

    INVALIDATE(data);
    data->next = scroll_data_free;
    scroll_data_free = data;
}


/* ========================================================================== */
/*  SEXTANT                                                                   */
/* ========================================================================== */

SEXTANT_DATA *new_sextant_data(void)
{
    SEXTANT_DATA *data;

    if (sextant_data_free)
    {
        data = sextant_data_free;
        sextant_data_free = sextant_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(SEXTANT_DATA));
    }

    memset(data, 0, sizeof(SEXTANT_DATA));
    VALIDATE(data);
    return data;
}

SEXTANT_DATA *copy_sextant_data(SEXTANT_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    SEXTANT_DATA *data = new_sextant_data();
    data->accuracy = src->accuracy;
    return data;
}

void free_sextant_data(SEXTANT_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = sextant_data_free;
    sextant_data_free = data;
}


/* ========================================================================== */
/*  TATTOO                                                                    */
/* ========================================================================== */

TATTOO_DATA *new_tattoo_data(void)
{
    TATTOO_DATA *data;

    if (tattoo_data_free)
    {
        data = tattoo_data_free;
        tattoo_data_free = tattoo_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(TATTOO_DATA));
    }

    memset(data, 0, sizeof(TATTOO_DATA));
    VALIDATE(data);
    return data;
}

TATTOO_DATA *copy_tattoo_data(TATTOO_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    TATTOO_DATA *data = new_tattoo_data();
    data->touches       = src->touches;
    data->fading_chance = src->fading_chance;
    data->fading_rate   = src->fading_rate;
    data->spells        = copy_spell_chain(src->spells);
    return data;
}

void free_tattoo_data(TATTOO_DATA *data)
{
    if (!IS_VALID(data)) return;

    free_spell_chain(data->spells);

    INVALIDATE(data);
    data->next = tattoo_data_free;
    tattoo_data_free = data;
}


/* ========================================================================== */
/*  TELESCOPE                                                                 */
/* ========================================================================== */

TELESCOPE_DATA *new_telescope_data(void)
{
    TELESCOPE_DATA *data;

    if (telescope_data_free)
    {
        data = telescope_data_free;
        telescope_data_free = telescope_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(TELESCOPE_DATA));
    }

    memset(data, 0, sizeof(TELESCOPE_DATA));
    VALIDATE(data);
    return data;
}

TELESCOPE_DATA *copy_telescope_data(TELESCOPE_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    TELESCOPE_DATA *data = new_telescope_data();
    data->distance     = src->distance;
    data->min_distance = src->min_distance;
    data->max_distance = src->max_distance;
    data->bonus_view   = src->bonus_view;
    data->heading      = src->heading;
    return data;
}

void free_telescope_data(TELESCOPE_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = telescope_data_free;
    telescope_data_free = data;
}


/* ========================================================================== */
/*  TOOL                                                                      */
/* ========================================================================== */

TOOL_DATA *new_tool_data(void)
{
    TOOL_DATA *data;

    if (tool_data_free)
    {
        data = tool_data_free;
        tool_data_free = tool_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(TOOL_DATA));
    }

    memset(data, 0, sizeof(TOOL_DATA));
    VALIDATE(data);
    return data;
}

TOOL_DATA *copy_tool_data(TOOL_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    TOOL_DATA *data = new_tool_data();
    data->type = src->type;
    data->tier = src->tier;
    return data;
}

void free_tool_data(TOOL_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = tool_data_free;
    tool_data_free = data;
}


/* ========================================================================== */
/*  WAND                                                                      */
/* ========================================================================== */

WAND_DATA *new_wand_data(void)
{
    WAND_DATA *data;

    if (wand_data_free)
    {
        data = wand_data_free;
        wand_data_free = wand_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(WAND_DATA));
    }

    memset(data, 0, sizeof(WAND_DATA));
    VALIDATE(data);
    return data;
}

WAND_DATA *copy_wand_data(WAND_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    WAND_DATA *data = new_wand_data();
    data->max_mana     = src->max_mana;
    data->charges      = src->charges;
    data->max_charges  = src->max_charges;
    data->cooldown     = src->cooldown;
    data->recharge_time = src->recharge_time;
    data->spells       = copy_spell_chain(src->spells);
    return data;
}

void free_wand_data(WAND_DATA *data)
{
    if (!IS_VALID(data)) return;

    free_spell_chain(data->spells);

    INVALIDATE(data);
    data->next = wand_data_free;
    wand_data_free = data;
}


/* ========================================================================== */
/*  WEAPON                                                                    */
/* ========================================================================== */

WEAPON_DATA *new_weapon_data(void)
{
    WEAPON_DATA *data;

    if (weapon_data_free)
    {
        data = weapon_data_free;
        weapon_data_free = weapon_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(WEAPON_DATA));
    }

    memset(data, 0, sizeof(WEAPON_DATA));
    VALIDATE(data);
    return data;
}

WEAPON_DATA *copy_weapon_data(WEAPON_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    WEAPON_DATA *data = new_weapon_data();
    data->weapon_class  = src->weapon_class;
    data->damage_type   = src->damage_type;
    data->flags         = src->flags;
    data->damage        = src->damage;
    data->range         = src->range;
    data->max_mana      = src->max_mana;
    data->charges       = src->charges;
    data->max_charges   = src->max_charges;
    data->cooldown      = src->cooldown;
    data->recharge_time = src->recharge_time;
    data->spells        = copy_spell_chain(src->spells);
    return data;
}

void free_weapon_data(WEAPON_DATA *data)
{
    if (!IS_VALID(data)) return;

    free_spell_chain(data->spells);

    INVALIDATE(data);
    data->next = weapon_data_free;
    weapon_data_free = data;
}


/* ========================================================================== */
/*  BODY PART                                                                 */
/* ========================================================================== */

BODY_PART_DATA *new_body_part_data(void)
{
    BODY_PART_DATA *data;

    if (body_part_data_free)
    {
        data = body_part_data_free;
        body_part_data_free = body_part_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(BODY_PART_DATA));
    }

    memset(data, 0, sizeof(BODY_PART_DATA));
    VALIDATE(data);
    return data;
}

BODY_PART_DATA *copy_body_part_data(BODY_PART_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    BODY_PART_DATA *data = new_body_part_data();
    data->parts    = src->parts;
    data->race_uid = src->race_uid;
    return data;
}

void free_body_part_data(BODY_PART_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = body_part_data_free;
    body_part_data_free = data;
}


/* ========================================================================== */
/*  CORPSE                                                                    */
/* ========================================================================== */

CORPSE_DATA *new_corpse_data(void)
{
    CORPSE_DATA *data;

    if (corpse_data_free)
    {
        data = corpse_data_free;
        corpse_data_free = corpse_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(CORPSE_DATA));
    }

    memset(data, 0, sizeof(CORPSE_DATA));
    VALIDATE(data);
    return data;
}

CORPSE_DATA *copy_corpse_data(CORPSE_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    CORPSE_DATA *data = new_corpse_data();
    data->corpse_type  = src->corpse_type;
    data->resurrection = src->resurrection;
    data->animation    = src->animation;
    data->body_parts   = src->body_parts;
    data->mobile_vnum  = src->mobile_vnum;
    data->mobile_area_uid = src->mobile_area_uid;
    return data;
}

void free_corpse_data(CORPSE_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = corpse_data_free;
    corpse_data_free = data;
}


/* ========================================================================== */
/*  HERB                                                                      */
/* ========================================================================== */

HERB_DATA *new_herb_data(void)
{
    HERB_DATA *data;

    if (herb_data_free)
    {
        data = herb_data_free;
        herb_data_free = herb_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(HERB_DATA));
    }

    memset(data, 0, sizeof(HERB_DATA));
    VALIDATE(data);
    return data;
}

HERB_DATA *copy_herb_data(HERB_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    HERB_DATA *data = new_herb_data();
    data->type          = src->type;
    data->healing       = src->healing;
    data->regenerative  = src->regenerative;
    data->refreshing    = src->refreshing;
    data->immunity      = src->immunity;
    data->resistance    = src->resistance;
    data->vulnerability = src->vulnerability;
    data->spell         = src->spell;
    return data;
}

void free_herb_data(HERB_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = herb_data_free;
    herb_data_free = data;
}


/* ========================================================================== */
/*  ITEM SHIP                                                                 */
/* ========================================================================== */

ITEM_SHIP_DATA *new_item_ship_data(void)
{
    ITEM_SHIP_DATA *data;

    if (item_ship_data_free)
    {
        data = item_ship_data_free;
        item_ship_data_free = item_ship_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(ITEM_SHIP_DATA));
    }

    memset(data, 0, sizeof(ITEM_SHIP_DATA));
    VALIDATE(data);
    return data;
}

ITEM_SHIP_DATA *copy_item_ship_data(ITEM_SHIP_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    ITEM_SHIP_DATA *data = new_item_ship_data();
    data->weight     = src->weight;
    data->move_delay = src->move_delay;
    data->min_crew   = src->min_crew;
    data->capacity   = src->capacity;
    data->max_crew   = src->max_crew;
    data->first_room = src->first_room;
    data->first_room_area_uid = src->first_room_area_uid;
    data->hit_points = src->hit_points;
    data->max_guns   = src->max_guns;
    return data;
}

void free_item_ship_data(ITEM_SHIP_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = item_ship_data_free;
    item_ship_data_free = data;
}


/* ========================================================================== */
/*  SHIP MODULE                                                               */
/* ========================================================================== */

SHIP_MODULE_DATA *new_ship_module_data(void)
{
    SHIP_MODULE_DATA *data;

    if (ship_module_data_free)
    {
        data = ship_module_data_free;
        ship_module_data_free = ship_module_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(SHIP_MODULE_DATA));
    }

    memset(data, 0, sizeof(SHIP_MODULE_DATA));
    VALIDATE(data);
    return data;
}

SHIP_MODULE_DATA *copy_ship_module_data(SHIP_MODULE_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    SHIP_MODULE_DATA *data = new_ship_module_data();
    data->type                = src->type;
    data->size                = src->size;
    data->weight              = src->weight;
    data->domain_flags        = src->domain_flags;
    data->hit_bonus           = src->hit_bonus;
    data->armor_bonus         = src->armor_bonus;
    data->speed_bonus         = src->speed_bonus;
    data->turning_bonus       = src->turning_bonus;
    data->cargo_weight_bonus  = src->cargo_weight_bonus;
    data->cargo_capacity_bonus = src->cargo_capacity_bonus;
    data->crew_bonus          = src->crew_bonus;
    data->damage              = src->damage;
    data->range               = src->range;
    data->reload_time         = src->reload_time;
    data->damage_type         = src->damage_type;
    data->weapon_flags        = src->weapon_flags;
    data->operators           = src->operators;
    data->req_gunning         = src->req_gunning;
    data->req_mechanics       = src->req_mechanics;
    data->req_scouting        = src->req_scouting;
    data->req_navigation      = src->req_navigation;
    data->req_oarring         = src->req_oarring;
    data->req_leadership      = src->req_leadership;
    data->ammo_ref            = src->ammo_ref;
    data->ammo                = src->ammo;
    data->ammo_per_shot       = src->ammo_per_shot;
    data->flags               = src->flags;
    return data;
}

void free_ship_module_data(SHIP_MODULE_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = ship_module_data_free;
    ship_module_data_free = data;
}


/* ========================================================================== */
/*  SEED                                                                      */
/* ========================================================================== */

SEED_DATA *new_seed_data(void)
{
    SEED_DATA *data;

    if (seed_data_free)
    {
        data = seed_data_free;
        seed_data_free = seed_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(SEED_DATA));
    }

    memset(data, 0, sizeof(SEED_DATA));
    VALIDATE(data);
    return data;
}

SEED_DATA *copy_seed_data(SEED_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    SEED_DATA *data = new_seed_data();
    data->growth_time  = src->growth_time;
    data->object_vnum  = src->object_vnum;
    data->object_area_uid = src->object_area_uid;
    return data;
}

void free_seed_data(SEED_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = seed_data_free;
    seed_data_free = data;
}


/* ========================================================================== */
/*  TRADE                                                                     */
/* ========================================================================== */

TRADE_DATA *new_trade_data(void)
{
    TRADE_DATA *data;

    if (trade_data_free)
    {
        data = trade_data_free;
        trade_data_free = trade_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(TRADE_DATA));
    }

    memset(data, 0, sizeof(TRADE_DATA));
    VALIDATE(data);
    return data;
}

TRADE_DATA *copy_trade_data(TRADE_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    TRADE_DATA *data = new_trade_data();
    data->trade_type = src->trade_type;
    return data;
}

void free_trade_data(TRADE_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = trade_data_free;
    trade_data_free = data;
}


/* ========================================================================== */
/*  WEAPON CONTAINER                                                          */
/* ========================================================================== */

WEAPON_CONTAINER_DATA *new_weapon_container_data(void)
{
    WEAPON_CONTAINER_DATA *data;

    if (weapon_container_data_free)
    {
        data = weapon_container_data_free;
        weapon_container_data_free = weapon_container_data_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(WEAPON_CONTAINER_DATA));
    }

    memset(data, 0, sizeof(WEAPON_CONTAINER_DATA));
    data->weight_multiplier = 100;
    VALIDATE(data);
    return data;
}

WEAPON_CONTAINER_DATA *copy_weapon_container_data(WEAPON_CONTAINER_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    WEAPON_CONTAINER_DATA *data = new_weapon_container_data();
    data->max_weight        = src->max_weight;
    data->weapon_type       = src->weapon_type;
    data->max_items         = src->max_items;
    data->weight_multiplier = src->weight_multiplier;
    return data;
}

void free_weapon_container_data(WEAPON_CONTAINER_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = weapon_container_data_free;
    weapon_container_data_free = data;
}
