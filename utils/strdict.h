#ifndef __STRDICT_H__
#define __STRDICT_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define SD_MIN_CAP      8       /* must be a power of two */
#define SD_MAX_LOAD     0.70    /* resize when count/cap exceeds this */

typedef struct string_dictionary_type STRING_DICT;
typedef struct string_dictionary_iter STRING_DICT_ITER;
typedef struct string_dictionary_slot SLOT;
typedef enum { SLOT_EMPTY = 0, SLOT_LIVE, SLOT_DEAD} SLOT_STATE;

struct string_dictionary_iter {
    const STRING_DICT * _d;
    size_t              _i;
};

struct string_dictionary_slot {
    char       *key;
    char       *val;
    uint32_t    hash;
    SLOT_STATE  state;
};


struct string_dictionary_type {
    SLOT *slots;
    size_t cap;
    size_t count;
    size_t dead;
};

STRING_DICT *strdict_new(size_t initial_cap);
void strdict_free(STRING_DICT *d);
bool strdict_set(STRING_DICT *d, const char *key, const char *val);
const char *strdict_get(const STRING_DICT *d, const char *key);
bool strdict_delete(STRING_DICT *d, const char *key);
size_t strdict_count(const STRING_DICT *d);
void strdict_clear(STRING_DICT *d);
STRING_DICT_ITER strdict_iter(const STRING_DICT *d);
bool strdict_next(STRING_DICT_ITER *it, const char **key, const char **val);


#endif  // __STRDICT_H__