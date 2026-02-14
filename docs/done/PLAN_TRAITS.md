# Trait System Plan

## Goal

Replace all hardcoded `IS_RACE()` macros in `merc.h` with a data-driven trait system.
Traits are defined as JSON, assigned to races in their race JSON with per-race values,
and queried at runtime via a C API. No game behavior should be hardcoded to any specific
race name.

## Trait Types

Each trait has a type that determines how it stores and returns values:

| Type | C Return | Example |
|------|----------|---------|
| `boolean` | `bool` | `sunlight_vulnerability` → true/false |
| `integer` | `int` | `breath_damage_bonus` → 25 (percent) |
| `string` | `const char *` | `shift_form` → "werewolf" |

## Trait Definitions

Stored in a single file: `data/traits/traits.json`

One file is preferred over per-trait files because trait definitions are small (just
metadata), there will be a manageable number of them, and it simplifies loading. The
actual per-race *values* live in each race's JSON file.

### Format

```json
{
  "_version": "1.0",
  "_format": "trait_definitions",
  "traits": [
    {
      "id": "sunlight_vulnerability",
      "name": "Sunlight Vulnerability",
      "description": "Race takes damage when outdoors during daytime.",
      "type": "boolean",
      "default": false,
      "category": "survival"
    },
    {
      "id": "breath_damage_bonus",
      "name": "Breath Damage Bonus",
      "description": "Percentage bonus to breath-type spell damage.",
      "type": "integer",
      "default": 0,
      "category": "combat"
    },
    {
      "id": "shift_form",
      "name": "Shift Form",
      "description": "Alternate form name when using the shift ability.",
      "type": "string",
      "default": null,
      "category": "ability"
    }
  ]
}
```

## Race JSON Integration

Races declare trait values in a `"traits"` object within their race JSON file.
Only traits that differ from their default need to be listed.

### Example: vampire.json (excerpt)

```json
{
  "id": "vampire",
  "traits": {
    "sunlight_vulnerability": true,
    "blood_feeding": true,
    "stakeable": true,
    "can_shapeshift": true,
    "shift_form": "werewolf"
  }
}
```

### Example: dragon.json (excerpt)

```json
{
  "id": "dragon",
  "traits": {
    "breath_damage_bonus": 25
  }
}
```

### Example: elf.json (excerpt)

```json
{
  "id": "elf",
  "traits": {
    "mana_regen_multiplier": 2,
    "ranged_accuracy_bonus": 5
  }
}
```

## Complete Trait Catalog

Derived from analyzing every `IS_*` macro usage in the codebase (~48 call sites):

### Combat Traits

| Trait ID | Type | Default | Replaces | Description |
|----------|------|---------|----------|-------------|
| `breath_damage_bonus` | integer | 0 | `IS_DRAGON` | % bonus to breath spell damage |
| `bash_damage_multiplier` | integer | 1 | `IS_MINOTAUR` | Multiplier on bash damage |
| `holy_damage_bonus` | integer | 0 | `IS_SLAYER` | % bonus damage vs evil-aligned targets |
| `stakeable` | boolean | false | `IS_VAMPIRE` | Can be staked while sleeping |

### Survival Traits

| Trait ID | Type | Default | Replaces | Description |
|----------|------|---------|----------|-------------|
| `sunlight_vulnerability` | boolean | false | `IS_VAMPIRE` | Takes damage outdoors in daytime |
| `blood_feeding` | boolean | false | `IS_VAMPIRE` | Nourished by blood instead of food |

### Resource Traits

| Trait ID | Type | Default | Replaces | Description |
|----------|------|---------|----------|-------------|
| `toxin_system` | boolean | false | `IS_SITH` | Has toxin resource pool (save/regen/inject) |
| `mana_regen_multiplier` | integer | 1 | `IS_ELF` | Multiplier on mana regeneration rate |

### Ability Traits

| Trait ID | Type | Default | Replaces | Description |
|----------|------|---------|----------|-------------|
| `can_shapeshift` | boolean | false | `IS_VAMPIRE`/`IS_SLAYER` | Access to shift command |
| `shift_form` | string | null | `IS_VAMPIRE`/`IS_SLAYER` | Which shifted form to use |
| `scent_tracking` | boolean | false | `IS_SITH` | 100% hunt success via scent |
| `scent_track_flavor` | string | null | `IS_SITH` | Hunt flavor text (e.g. "forked tongue tastes the air") |
| `ranged_accuracy_bonus` | integer | 0 | `IS_ELF` | Flat % bonus to ranged weapon accuracy |
| `cosmic_projection` | boolean | false | `IS_DEMON`/`IS_ANGEL` | Can project between planes |
| `classless_skills` | boolean | false | `IS_ANGEL`/`IS_MYSTIC`/`IS_DEMON` | Can learn skills from any class |

### Death/Planar Traits

| Trait ID | Type | Default | Replaces | Description |
|----------|------|---------|----------|-------------|
| `death_plane_vnum_min` | integer | 0 | `IS_DEMON`/`IS_ANGEL` | Min vnum of alternate death room range |
| `death_plane_vnum_max` | integer | 0 | `IS_DEMON`/`IS_ANGEL` | Max vnum of alternate death room range |

### Race Assignments

| Race | Traits |
|------|--------|
| **vampire** (+ fiend) | `sunlight_vulnerability`, `blood_feeding`, `stakeable`, `can_shapeshift`, `shift_form`="werewolf" |
| **slayer** (+ changeling) | `holy_damage_bonus`=10, `can_shapeshift`, `shift_form`="slayer" |
| **sith** (+ naga) | `toxin_system`, `scent_tracking`, `scent_track_flavor`="Your forked tongue tastes the air" |
| **dragon** | `breath_damage_bonus`=25 |
| **draconian** (+ dragon) | (dragon gets breath bonus; draconian may get a smaller one) |
| **elf** (+ seraph) | `mana_regen_multiplier`=2, `ranged_accuracy_bonus`=5 |
| **minotaur** (+ hell baron) | `bash_damage_multiplier`=2 |
| **demon** (+ demon_mob) | `death_plane_vnum_min`=200050, `death_plane_vnum_max`=207050, `cosmic_projection`, `classless_skills` |
| **angel** | `death_plane_vnum_min`=300050, `death_plane_vnum_max`=307050, `cosmic_projection`, `classless_skills` |
| **mystic** | `classless_skills` |

## C Data Structures

### Trait Definition (loaded from traits.json)

```c
typedef enum {
    TRAIT_BOOLEAN,
    TRAIT_INTEGER,
    TRAIT_STRING
} trait_type_t;

typedef struct trait_def {
    struct trait_def *next;
    bool valid;
    char *id;
    char *name;
    char *description;
    char *category;
    trait_type_t type;

    /* Defaults */
    bool default_bool;
    int default_int;
    char *default_string;
} TRAIT_DEF;
```

### Trait Value (stored per-race)

```c
typedef struct trait_value {
    struct trait_value *next;
    TRAIT_DEF *def;          /* Points to the trait definition */
    bool bool_val;
    int int_val;
    char *string_val;
} TRAIT_VALUE;
```

### On RACE_DATA

```c
struct race_data {
    /* ... existing fields ... */
    LLIST *traits;           /* Linked list of TRAIT_VALUE */
};
```

### Lookup Performance

For fast lookup in combat loops, traits use a hash table on the race keyed by trait ID
string. With ~15-20 traits per race maximum, a simple linked list scan would also be
fast enough (sub-microsecond), but a small hash gives O(1) and is easy to implement
since we already have the pattern in the race hash table.

Alternatively, since trait definitions are global and numbered, we can assign each
trait definition a sequential index at load time, and store trait values on the race
in an array indexed by trait index. This gives true O(1) with zero hashing overhead:

```c
/* Global */
extern int trait_count;              /* Total trait definitions loaded */

/* On RACE_DATA */
TRAIT_VALUE *trait_values;           /* Array[trait_count], indexed by def->index */

/* Lookup */
#define race_get_trait(race, trait_def) (&(race)->trait_values[(trait_def)->index])
```

For string-based lookups (scripting, OLC), use `trait_def_lookup("id")` to get the
TRAIT_DEF first, then index into the race's array.

## C API

```c
/* Trait definition management */
void load_trait_definitions(void);
TRAIT_DEF *trait_def_lookup(const char *id);

/* Race trait queries */
bool race_has_trait(RACE_DATA *race, const char *trait_id);
bool race_get_trait_bool(RACE_DATA *race, const char *trait_id);
int race_get_trait_int(RACE_DATA *race, const char *trait_id);
const char *race_get_trait_string(RACE_DATA *race, const char *trait_id);

/* Convenience macros for hot paths (cache the TRAIT_DEF pointer) */
#define TRAIT_BOOL(race, cached_def) ((race)->trait_values[(cached_def)->index].bool_val)
#define TRAIT_INT(race, cached_def)  ((race)->trait_values[(cached_def)->index].int_val)
```

### Usage Examples

**Before:**
```c
if (IS_VAMPIRE(ch))
    hurt_vampires(ch);
```

**After:**
```c
if (ch->race && race_get_trait_bool(ch->race, "sunlight_vulnerability"))
    hurt_vampires(ch);
```

**Hot path with cached def (initialized once at boot):**
```c
/* Global, set during load_trait_definitions() */
static TRAIT_DEF *td_sunlight_vulnerability = NULL;

/* In init code */
td_sunlight_vulnerability = trait_def_lookup("sunlight_vulnerability");

/* In combat loop */
if (ch->race && TRAIT_BOOL(ch->race, td_sunlight_vulnerability))
    hurt_vampires(ch);
```

## Scripting Engine Integration

Replace the hardcoded `isangel()`, `isdemon()`, `ismystic()` script conditionals with
a generic `hastrait()` function:

**Before:**
```
if isangel($n)
```

**After:**
```
if hastrait($n, 'cosmic_projection')
```

The old `isangel()`/`isdemon()`/`ismystic()` functions can remain as deprecated
aliases that internally check the appropriate trait, to avoid breaking existing scripts.

## OLC Integration

Add trait editing to the race editor (`raceedit`):

```
raceedit traits                           - List all traits and current values
raceedit trait <trait_id>                  - Toggle boolean / show current
raceedit trait <trait_id> <value>          - Set value
raceedit trait <trait_id> remove           - Reset to default
```

Add a trait definition editor (`traitedit`) for creating/modifying trait definitions
without code changes.

## Load Order

1. `load_trait_definitions()` — parse `data/traits/traits.json`, assign indices
2. `load_races()` — parse each race JSON, including its `"traits"` object;
   for each trait key, look up the TRAIT_DEF, populate the race's trait_values array
3. Traits with no race-specific value get the definition's default

## File Organization

```
data/
  traits/
    traits.json              # All trait definitions
  races/
    human.json               # Races include "traits": { ... }
    vampire.json
    ...
```

## Source Files

| File | Purpose |
|------|---------|
| `src/traits.c` | Trait definition loading, race trait API, OLC |
| `src/traits.h` | Trait structs, API declarations, convenience macros |
| `src/io/json/json_race.c` | Extended to parse/save `"traits"` on race load/save |

## Implementation Phases

### Phase 1: Core Infrastructure
1. Create `traits.h` and `traits.c` with data structures
2. Implement `load_trait_definitions()` from JSON
3. Add `traits` field to `RACE_DATA`
4. Extend `json_race.c` to load/save trait values on races
5. Implement query API (`race_get_trait_bool`, etc.)
6. Create initial `data/traits/traits.json` with all traits listed above
7. Update `Makefile` and `CMakeLists.txt`

### Phase 2: Populate Race Data
1. Add `"traits": { ... }` to each race JSON file that needs traits
2. Verify all traits load correctly at boot

### Phase 3: Replace Macros
1. Replace each `IS_*` macro call site with the appropriate trait query
2. Remove (or deprecate) the `IS_*` macros from `merc.h`
3. For hot paths (fight.c combat loop), use cached TRAIT_DEF pointers

### Phase 4: Scripting & OLC
1. Add `hastrait()` script conditional
2. Deprecate `isangel()`/`isdemon()`/`ismystic()` (alias to trait checks)
3. Add `traitedit` OLC editor
4. Add trait commands to `raceedit`

## Per-Character Trait Overrides

For now, traits are race-level only. If per-character overrides are needed in the
future (e.g., a quest grants a temporary trait), the system can be extended by adding
a `LLIST *trait_overrides` on `CHAR_DATA` that takes precedence over race traits.
The query API would check character overrides first, then fall back to race traits.
This is out of scope for the initial implementation.

## Notes

- The remort relationship (e.g., vampire→fiend, sith→naga) means remort races
  should generally inherit or extend the base race's traits. This is handled naturally
  by each race having its own trait assignments in JSON.
- `IS_REMORT(ch)` is already handled by `race_is_remort()` and doesn't need a trait.
- `IS_SAGE(ch)` checks class, not race, and is out of scope for this system.
