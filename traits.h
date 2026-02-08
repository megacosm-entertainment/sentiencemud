/**
 * traits.h - Trait system API for data-driven race behavior
 *
 * Replaces hardcoded IS_RACE() macros with traits defined in JSON,
 * assigned to races, and queried at runtime via indexed array lookups.
 */

#ifndef TRAITS_H
#define TRAITS_H

#include <stdbool.h>
#include <stdint.h>

/* Forward declarations */
typedef struct race_data RACE_DATA;

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
 * Trait Value - stored per-race in an indexed array                       *
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

/***************************************************************************
 * Race Trait Query API                                                    *
 ***************************************************************************/

bool		race_has_trait(RACE_DATA *race, const char *trait_id);
bool		race_get_trait_bool(RACE_DATA *race, const char *trait_id);
int		race_get_trait_int(RACE_DATA *race, const char *trait_id);
const char *	race_get_trait_string(RACE_DATA *race, const char *trait_id);

/***************************************************************************
 * Race Trait Loading (called from json_race.c)                            *
 ***************************************************************************/

void		race_init_traits(RACE_DATA *race);
void		race_load_traits_json(RACE_DATA *race, void *traits_obj);
void *		race_save_traits_json(RACE_DATA *race);

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
