/**
 * traits.h - Multi-layer trait system API
 *
 * Traits can be set at three levels, queried in priority order:
 *   1. Personal (PC_DATA.trait_values) — per-character overrides
 *   2. Class (CLASS_DATA.trait_values) — class-inherent traits
 *   3. Race (RACE_DATA.trait_values)   — racial defaults
 *
 * Boolean traits use OR semantics across layers (true at any layer = true).
 * Integer traits use the highest-priority layer that has the trait set.
 * String traits use the highest-priority layer that has the trait set.
 *
 * Originally replaced hardcoded IS_RACE() macros; now generalizes to
 * class and character-level traits as well.
 */

#ifndef TRAITS_H
#define TRAITS_H

#include <stdbool.h>
#include <stdint.h>

/* Forward declarations */
typedef struct race_data RACE_DATA;
typedef struct class_data CLASS_DATA;
typedef struct char_data CHAR_DATA;

/***************************************************************************
 * Trait Types                                                             *
 ***************************************************************************/

typedef enum {
    TRAIT_BOOLEAN,
    TRAIT_INTEGER,
    TRAIT_STRING
} trait_type_t;

/***************************************************************************
 * Trait Definition - loaded from data/traits/traits.json                  *
 ***************************************************************************/

typedef struct trait_def TRAIT_DEF;

struct trait_def {
    TRAIT_DEF *	next;
    bool	valid;
    int		index;		/* Sequential index for array-based lookup */

    /* Identity */
    char *	id;		/* Unique string identifier */
    char *	name;		/* Display name */
    char *	description;	/* Long description */
    char *	category;	/* Grouping category */

    /* Type and defaults */
    trait_type_t	type;
    bool		default_bool;
    int		default_int;
    char *		default_string;
};

/***************************************************************************
 * Trait Value - stored per-race/class/character in an indexed array       *
 ***************************************************************************/

typedef struct trait_value TRAIT_VALUE;

struct trait_value {
    bool	set;		/* Was this explicitly set (vs default)? */
    bool	bool_val;
    int		int_val;
    char *	string_val;
};

/***************************************************************************
 * Globals                                                                 *
 ***************************************************************************/

extern TRAIT_DEF *	trait_def_list;
extern int		trait_def_count;

/***************************************************************************
 * Trait Definition API                                                    *
 ***************************************************************************/

void		load_trait_definitions(void);
TRAIT_DEF *	trait_def_lookup(const char *id);
TRAIT_DEF *	trait_def_lookup_name(const char *name);

/***************************************************************************
 * Race Trait Query API                                                    *
 ***************************************************************************/

bool		race_has_trait(RACE_DATA *race, const char *trait_id);
bool		race_get_trait_bool(RACE_DATA *race, const char *trait_id);
int		race_get_trait_int(RACE_DATA *race, const char *trait_id);
const char *	race_get_trait_string(RACE_DATA *race, const char *trait_id);

/* Set traits on a race definition (runtime modification) */
bool		race_set_trait_bool(RACE_DATA *race, const char *trait_id, bool value);
bool		race_set_trait_int(RACE_DATA *race, const char *trait_id, int value);
bool		race_set_trait_string(RACE_DATA *race, const char *trait_id, const char *value);
bool		race_clear_trait(RACE_DATA *race, const char *trait_id);

/***************************************************************************
 * Race Trait Loading (called from json_race.c)                            *
 ***************************************************************************/

void		race_init_traits(RACE_DATA *race);
void		race_load_traits_json(RACE_DATA *race, void *traits_obj);
void *		race_save_traits_json(RACE_DATA *race);

/***************************************************************************
 * Class Trait API                                                         *
 ***************************************************************************/

/* Query traits on a class definition */
bool		class_has_trait(CLASS_DATA *clazz, const char *trait_id);
bool		class_get_trait_bool(CLASS_DATA *clazz, const char *trait_id);
int		class_get_trait_int(CLASS_DATA *clazz, const char *trait_id);
const char *	class_get_trait_string(CLASS_DATA *clazz, const char *trait_id);

/* Set traits on a class definition (runtime modification) */
bool		class_set_trait_bool(CLASS_DATA *clazz, const char *trait_id, bool value);
bool		class_set_trait_int(CLASS_DATA *clazz, const char *trait_id, int value);
bool		class_set_trait_string(CLASS_DATA *clazz, const char *trait_id, const char *value);
bool		class_clear_trait(CLASS_DATA *clazz, const char *trait_id);

/* Class trait init/load/save (mirrors race pattern) */
void		class_init_traits(CLASS_DATA *clazz);
void		class_load_traits_json(CLASS_DATA *clazz, void *traits_obj);
void *		class_save_traits_json(CLASS_DATA *clazz);

/***************************************************************************
 * Personal (Character) Trait API                                          *
 ***************************************************************************/

/* Init/load/save personal trait overrides on PC_DATA */
void		char_init_traits(CHAR_DATA *ch);
void		char_load_traits_json(CHAR_DATA *ch, void *traits_obj);
void *		char_save_traits_json(CHAR_DATA *ch);

/***************************************************************************
 * Unified Character Trait Query API (layered: personal > class > race)    *
 *                                                                         *
 * These check all three trait layers in priority order and return the      *
 * first explicitly-set value found.                                       *
 ***************************************************************************/

bool		ch_has_trait(CHAR_DATA *ch, const char *trait_id);
bool		ch_get_trait_bool(CHAR_DATA *ch, const char *trait_id);
int		ch_get_trait_int(CHAR_DATA *ch, const char *trait_id);
const char *	ch_get_trait_string(CHAR_DATA *ch, const char *trait_id);

/* Set personal trait overrides on a character (PC only) */
bool		ch_set_trait_bool(CHAR_DATA *ch, const char *trait_id, bool value);
bool		ch_set_trait_int(CHAR_DATA *ch, const char *trait_id, int value);
bool		ch_set_trait_string(CHAR_DATA *ch, const char *trait_id, const char *value);
bool		ch_clear_trait(CHAR_DATA *ch, const char *trait_id);

/***************************************************************************
 * Trait Value Helpers (for direct array access)                           *
 ***************************************************************************/

/* Allocate a trait_values array sized to trait_def_count, filled with defaults */
TRAIT_VALUE *	trait_values_alloc(void);

/* Load/save a generic trait_values array from/to JSON */
void		trait_values_load_json(TRAIT_VALUE *tv, void *traits_obj);
void *		trait_values_save_json(TRAIT_VALUE *tv);

/***************************************************************************
 * Fast Lookup Macros (use cached TRAIT_DEF pointer for hot paths)         *
 ***************************************************************************/

#define TRAIT_BOOL(race, tdef) \
    ((tdef) && (race)->trait_values ? (race)->trait_values[(tdef)->index].bool_val : false)

#define TRAIT_INT(race, tdef) \
    ((tdef) && (race)->trait_values ? (race)->trait_values[(tdef)->index].int_val : 0)

#define TRAIT_STR(race, tdef) \
    ((tdef) && (race)->trait_values ? (race)->trait_values[(tdef)->index].string_val : NULL)

#endif /* TRAITS_H */
