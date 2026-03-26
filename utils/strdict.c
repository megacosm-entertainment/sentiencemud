#include "strdict.h"

static inline uint64_t _strdict_mix(uint64_t a, uint64_t b)
{
    __uint128_t r = (__uint128_t)a * b;
    return (uint64_t)(r ^ (r >> 64));
}

#define HASH_A  (UINT64_C(0x517cc1b727220a95))
#define HASH_B  (UINT64_C(0x9e3779b97f4a7c15))
static uint32_t _strdict_hash(const char *key)
{
    uint64_t h = HASH_A;
    while (*key) {
        h = _strdict_mix(h ^ (unsigned char)*key, HASH_B);
        ++key;
    }
    h ^= h >> 32;
    /* never return 0 — we use 0 as a fast "hash not computed" sentinel */
    return (uint32_t)h | (uint32_t)!h;
}

static inline size_t _strdict_next_pow2(size_t n)
{
    if (n < SD_MIN_CAP) return SD_MIN_CAP;
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}
 
static char *_strdict_strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char  *d = (char *)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* Find the slot for key.  Returns the live slot if found, else the first
 * EMPTY/DEAD slot usable for insertion (first_dead), or NULL if table is full
 * (should not happen if load-factor is respected). */
static SLOT *_strdict_find(SLOT *slots, size_t cap,
                      const char *key, uint32_t h,
                      SLOT **first_dead_out)
{
    size_t mask = cap - 1;
    size_t idx  = h & mask;
    SLOT  *first_dead = NULL;
 
    for (size_t probe = 0; probe < cap; ++probe) {
        SLOT *s = slots + ((idx + probe) & mask);
 
        if (s->state == SLOT_EMPTY) {
            if (first_dead_out) *first_dead_out = first_dead ? first_dead : s;
            return NULL;
        }
        if (s->state == SLOT_DEAD) {
            if (!first_dead) first_dead = s;
            continue;
        }
        /* SLOT_LIVE */
        if (s->hash == h && strcmp(s->key, key) == 0)
            return s;
    }
    if (first_dead_out) *first_dead_out = first_dead;
    return NULL;
}

/* Rehash into a fresh slot array of new_cap. */
static bool _strdict_rehash(STRING_DICT *d, size_t new_cap)
{
    SLOT *ns = (SLOT *)calloc(new_cap, sizeof(SLOT));
    if (!ns) return false;
 
    for (size_t i = 0; i < d->cap; ++i) {
        SLOT *s = d->slots + i;
        if (s->state != SLOT_LIVE) continue;
 
        size_t mask = new_cap - 1;
        size_t idx  = s->hash & mask;
        for (size_t probe = 0; probe < new_cap; ++probe) {
            SLOT *t = ns + ((idx + probe) & mask);
            if (t->state == SLOT_EMPTY) {
                *t = *s;
                break;
            }
        }
    }
    free(d->slots);
    d->slots = ns;
    d->cap   = new_cap;
    d->dead  = 0;
    return true;
}


STRING_DICT *strdict_new(size_t initial_cap)
{
    STRING_DICT *d = (STRING_DICT *)malloc(sizeof *d);
    if (!d) return NULL;
    d->cap   = _strdict_next_pow2(initial_cap);
    d->count = 0;
    d->dead  = 0;
    d->slots = (SLOT *)calloc(d->cap, sizeof(SLOT));
    if (!d->slots) { free(d); return NULL; }
    return d;
}
 
void strdict_free(STRING_DICT *d)
{
    if (!d) return;
    for (size_t i = 0; i < d->cap; ++i) {
        if (d->slots[i].state == SLOT_LIVE) {
            free(d->slots[i].key);
            free(d->slots[i].val);
        }
    }
    free(d->slots);
    free(d);
}

bool strdict_set(STRING_DICT *d, const char *key, const char *val)
{
    if (!d || !key || !val) return false;
 
    /* Grow if load factor exceeded (count + dead against cap). */
    if ((double)(d->count + d->dead + 1) / (double)d->cap > SD_MAX_LOAD) {
        size_t new_cap = (d->count + 1 > d->cap * SD_MAX_LOAD / 2)
                         ? d->cap * 2 : d->cap;   /* shrink tombstones only? */
        if (!_strdict_rehash(d, new_cap)) return false;
    }
 
    uint32_t h = _strdict_hash(key);
    SLOT *first_dead = NULL;
    SLOT *live = _strdict_find(d->slots, d->cap, key, h, &first_dead);
 
    if (live) {
        /* Update existing key. */
        char *nv = _strdict_strdup(val);
        if (!nv) return false;
        free(live->val);
        live->val = nv;
        return true;
    }
 
    /* Insert into first_dead (tombstone) or empty slot. */
    SLOT *dest = first_dead;
    if (!dest) return false;   /* table full — should never happen */
 
    char *nk = _strdict_strdup(key);
    char *nv = _strdict_strdup(val);
    if (!nk || !nv) { free(nk); free(nv); return false; }
 
    if (dest->state == SLOT_DEAD) d->dead--;
    dest->key   = nk;
    dest->val   = nv;
    dest->hash  = h;
    dest->state = SLOT_LIVE;
    d->count++;
    return true;
}

const char *strdict_get(const STRING_DICT *d, const char *key)
{
    if (!d || !key) return NULL;
    if (!d->count) return NULL;
    uint32_t h    = _strdict_hash(key);
    SLOT    *live = _strdict_find(d->slots, d->cap, key, h, NULL);
    return live ? live->val : NULL;
}
 
bool strdict_delete(STRING_DICT *d, const char *key)
{
    if (!d || !key) return false;
    if (!d->count) return false;
    uint32_t h    = _strdict_hash(key);
    SLOT    *live = _strdict_find(d->slots, d->cap, key, h, NULL);
    if (!live) return false;
    free(live->key); live->key = NULL;
    free(live->val); live->val = NULL;
    live->state = SLOT_DEAD;
    d->count--;
    d->dead++;
    return true;
}
 
size_t strdict_count(const STRING_DICT *d) { return d ? d->count : 0; }

void strdict_clear(STRING_DICT *d)
{
    if (!d) return;
    for (size_t i = 0; i < d->cap; ++i) {
        SLOT *s = d->slots + i;
        if (s->state == SLOT_LIVE) { free(s->key); free(s->val); }
        *s = (SLOT){NULL};
    }
    d->count = d->dead = 0;
}
 
STRING_DICT_ITER strdict_iter(const STRING_DICT *d) { return (STRING_DICT_ITER){ ._d = d, ._i = 0 }; }
 
bool strdict_next(STRING_DICT_ITER *it, const char **key, const char **val)
{
    const STRING_DICT *d = it->_d;
    while (it->_i < d->cap) {
        SLOT *s = d->slots + it->_i++;
        if (s->state == SLOT_LIVE) {
            if (key) *key = s->key;
            if (val) *val = s->val;
            return true;
        }
    }
    return false;
}
