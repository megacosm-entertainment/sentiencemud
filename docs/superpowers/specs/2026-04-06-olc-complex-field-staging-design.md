# OLC Complex Field Staging — Phase 3

## Problem

Phase 2 converted ~48 simple/scalar OLC editor functions across oedit, medit,
aedit, and redit to staged changesets. However, the majority of editor
functionality (~90 functions, ~8000 lines) still bypasses the changeset system:

1. **List operations** — adding/removing affects, spells, catalysts, quests,
   scripts, immunities, reputations, trades, resets, extra descriptions,
   conditional descriptions.
2. **Complex nested structs** — shops (1042 lines), trainers, questors, crews,
   locks, waypoints, regions, builders.
3. **Type-specific sub-fields** — 32 item types in `oedit_types.c` (weapon,
   armor, container, portal, etc.) with their own field sets.
4. **Room exits** — 10 direction commands + change_exit + dislink.
5. **Room flags** — bitmatrix-based flag toggling.

The GMCP Editor protocol already defines field types for these operations
(`list_add`, `list_remove`, `list_update`, `type_data`, `embedded`, `exit`)
but the server-side infrastructure to handle them does not exist.

### Scope

This phase prototypes with **oedit end-to-end** (all 51 complex functions
including all 32 type-specific commands), validating the design before rolling
out to medit, aedit, and redit in a future phase.

## Approach

Build three infrastructure layers, then convert all oedit complex fields:

1. **List operation staging** — sequential operation entries in changesets
2. **Embedded struct snapshots** — JSON serialize/deserialize for complex structs
3. **Type-specific field dispatch** — single wildcard handler with internal routing
4. **GMCP Editor.Set extension** — path-based detection for new field types
5. **OEdit conversions** — all 51 functions using the new infrastructure

## Design

### 1. List Operation Infrastructure

#### Path Format

List operations use unique sequential paths within the changeset:

```
{list_name}/{op}:{sequence}
```

| Operation | Path Pattern | Field Type |
|-----------|-------------|------------|
| Add item | `"affects/add:0"` | `OLC_FIELD_LIST_ADD` |
| Remove item | `"affects/rm:0"` | `OLC_FIELD_LIST_REMOVE` |
| Update item | `"affects/upd:0"` | `OLC_FIELD_LIST_UPDATE` |

The sequence number auto-increments per changeset per list prefix. Each
operation is its own change entry in the changeset's `changes` list, with a
unique `field_path`. This works with the existing `olc_changeset_add_change()`
API without modification — each path is unique, so no collapsing occurs.

#### New API Functions

```c
/* In olc_changeset.h */

/**
 * Get the next available sequence number for a given path prefix.
 * Scans existing changes matching "{prefix}:" and returns max + 1.
 */
int olc_changeset_next_seq(olc_changeset_t *cs, const char *prefix);

/**
 * Revert all changes matching a field path prefix.
 * E.g., prefix "affects" removes "affects/add:0", "affects/rm:0", etc.
 * Returns the number of changes removed.
 */
int olc_changeset_revert_prefix(olc_changeset_t *cs, const char *prefix);
```

#### Staging Helper

```c
/* In olc_commands.h */

/**
 * Stage a list add operation. Builds the path "list_name/add:N" automatically.
 * Returns the pending change, or NULL on failure.
 */
olc_pending_change_t *olc_stage_list_add(
    olc_changeset_t *cs, const char *list_name, json_t *value);

/**
 * Stage a list remove operation. Stores the index being removed.
 * The value should contain {"index": N} where N is the live list index.
 */
olc_pending_change_t *olc_stage_list_remove(
    olc_changeset_t *cs, const char *list_name, int index,
    json_t *old_value);
```

#### JSON Value Format

Each list type defines its own JSON schema for item values. Examples:

**Affect (AFFECT_DATA):**
```json
{
  "where": "TO_OBJECT",
  "location": "APPLY_STR",
  "modifier": 5,
  "type": -1,
  "duration": -1,
  "bitvector": 0
}
```

**Spell (SPELL_DATA):**
```json
{
  "spell": "fireball",
  "level": 20,
  "repop": 100
}
```

**Script attachment (PROG_LIST):**
```json
{
  "vnum": "5#100",
  "trigger": "greet_prog",
  "phrase": "100"
}
```

**List remove:**
```json
{"index": 2}
```

#### Apply Semantics

On commit, the apply handler for a list uses a **two-pass algorithm**:

1. Collect all `{list_name}/*` changes from the changeset.
2. Sort by sequence number (ascending).
3. **Pass 1 — Removes:** Extract all `rm` operations. Sort by index
   **descending**. Apply removes from highest index to lowest. This avoids
   index shifting — removing index 5 before index 2 means index 2 is still
   valid. Adds do not affect remove indices because removes reference the
   **live list state at staging time**, before any adds.
4. **Pass 2 — Adds:** Apply all `add` operations in sequence order (appending
   to the list). Since removes happened first, adds always append to the
   post-removal list.

This two-pass approach is deterministic regardless of interleaving at staging
time. The invariant is: **remove indices always reference the original live
list, add operations always append to the end.**

The handler table entry uses a wildcard match:

```c
{ "affects/*", OLC_FIELD_LIST_ADD, NULL, oedit_apply_affect_ops, NULL },
```

The `field_path_matches()` function already supports `*` suffix matching
(used for `var/*` in Phase 2), so this works without changes.

### 2. Complex Struct Snapshots

Large, deeply nested structs with many interrelated sub-commands are staged
as embedded JSON snapshots using `OLC_FIELD_EMBEDDED`.

#### Affected Structs

| Editor | Struct | Path | Est. Lines |
|--------|--------|------|-----------|
| oedit | LOCK_STATE | `"lock"` | 168 |
| oedit | WAYPOINT_DATA list | `"waypoints"` | 180 |

> **Future phases:** medit (shop, questor, trainer, crew), aedit (regions),
> and redit (exits) will use the same snapshot infrastructure. Shown below for
> completeness; conversion is deferred to Phase 4.

| Editor | Struct | Path | Est. Lines |
|--------|--------|------|-----------|
| medit | SHOP_DATA | `"shop"` | 1042 |
| medit | questor data | `"questor"` | 187 |
| medit | trainer data | `"trainer"` | 306 |
| medit | SHIP_CREW_INDEX | `"crew"` | 216 |
| aedit | AREA_REGION list | `"regions"` | 361 |

#### Staged Mode Behavior

Sub-commands in staged mode operate on the staged JSON snapshot, not the live
struct. This is consistent with how scalar fields work — changes accumulate
in the changeset and are not visible in the live game until committed.

**Flow for sub-commands (e.g., `lock key 5#100`):**

1. Check if a pending change for `"lock"` already exists.
2. If not, serialize the live struct to JSON → `old_value`. Copy → working
   `new_value`.
3. Modify the `new_value` JSON to reflect the sub-command (set key field).
4. Store/update the change: `olc_changeset_add_change(cs, "lock",
   OLC_FIELD_EMBEDDED, old_value, new_value)`. The existing collapsing logic
   replaces `new_value` on the existing entry while preserving `old_value`.
5. Display commands (`show`) read from staged JSON when a pending change exists.

**On commit:** The apply function deserializes `new_value` JSON and replaces
the live struct entirely.

**On revert:** The pending change is removed. The live struct is untouched
(it was never modified).

#### Serialization API

Each complex struct needs a serialize/deserialize pair:

```c
/* Serialize live struct to JSON */
json_t *oedit_serialize_lock(const OBJ_INDEX_DATA *pObj);
json_t *medit_serialize_shop(const MOB_INDEX_DATA *pMob);

/* Deserialize JSON back to live struct (on commit) */
bool oedit_deserialize_lock(OBJ_INDEX_DATA *pObj, json_t *data);
bool medit_deserialize_shop(MOB_INDEX_DATA *pMob, json_t *data);
```

### 3. Type-Specific Field Dispatch

#### Path Format

Type-specific fields use namespaced paths:

```
typedata/{type_name}/{field_name}
```

Examples:
- `"typedata/weapon/class"` — weapon class (sword, mace, etc.)
- `"typedata/weapon/dice"` — weapon damage dice
- `"typedata/armor/pierce"` — armor AC pierce value
- `"typedata/portal/destination"` — portal destination

#### Single Wildcard Handler

One handler table entry with internal dispatch:

```c
/* In oedit.c handler table: */
{ "typedata/*", OLC_FIELD_TYPE_DATA, oedit_serialize_typedata, oedit_apply_typedata, NULL },
```

The `oedit_apply_typedata()` function parses the path to extract type name
and field name, then dispatches to a per-type handler:

```c
static const oedit_type_handler_t type_dispatch[] = {
    { "weapon",    weapon_apply_field,    weapon_serialize },
    { "armor",     armor_apply_field,     armor_serialize  },
    { "container", container_apply_field, container_serialize },
    { "portal",    portal_apply_field,    portal_serialize },
    // ... all 32 types
    { NULL, NULL, NULL }
};
```

Each per-type handler receives the field name suffix and the JSON value:

```c
typedef bool (*type_field_apply_fn)(void *entity, const char *field_name,
                                     olc_pending_change_t *change);
typedef json_t *(*type_field_serialize_fn)(void *entity, const char *type_name);
```

#### addtype / removetype

Type addition and removal use special paths with the same `OLC_FIELD_TYPE_DATA`
type (since dispatch is **path-only**, not path+type):

- Add type: path `"typedata/+{type_name}"`, type `OLC_FIELD_TYPE_DATA`
- Remove type: path `"typedata/-{type_name}"`, type `OLC_FIELD_TYPE_DATA`

The `oedit_apply_typedata()` handler detects the `+`/`-` prefix in the path
and calls `obj_index_alloc_type_data()` or `obj_index_free_type_data()`
accordingly. The field handler dispatch in `olc_find_field_handler()` matches
on **path pattern only** — the `type` parameter is used for auto-detection in
GMCP but does not filter handler table lookups.

### 4. GMCP Editor.Set Extension

#### Path-Based Operation Detection

The existing `handle_editor_set()` function is extended to detect operation
type from the field path prefix:

| Field Path Pattern | Detected Type | Action |
|-------------------|---------------|--------|
| `*/add` | `OLC_FIELD_LIST_ADD` | Auto-assign sequence, stage list add |
| `*/rm` | `OLC_FIELD_LIST_REMOVE` | Auto-assign sequence, stage list remove |
| `*/upd` | `OLC_FIELD_LIST_UPDATE` | Auto-assign sequence, stage list update |

> **Note:** `list_update` and `*/upd` detection are forward-looking
> infrastructure. No oedit function currently uses list update operations.
> The infrastructure is included so Phase 4 editors (medit shop stock, etc.)
> can use it without framework changes. No staging helper or test coverage
> is required for this phase.
| `typedata/*` | `OLC_FIELD_TYPE_DATA` | Route to type dispatch |
| Known embedded names | `OLC_FIELD_EMBEDDED` | Snapshot-based staging |
| Everything else | Auto-detect (existing) | Scalar handling |

The client sends the same `Editor.Set` message format — only the field path
changes:

```json
{"entity_id": "obj:5#100", "field": "affects/add",
 "value": {"location": "APPLY_STR", "modifier": 5, "where": "TO_OBJECT"}}
```

The server strips the `/add` suffix, assigns a sequence number, and stages
as `"affects/add:0"`.

#### Editor.Field Response

For list operations, the response includes the assigned path with sequence:

```json
{"entity_id": "obj:5#100", "field": "affects/add:0",
 "type": "list_add", "value": {...}, "is_pending": true}
```

This lets the client track individual operations for selective revert.

### 5. Revert Enhancements

#### Individual Operation Revert

Already works via `olc_changeset_revert_field(cs, "affects/add:1")`.

#### Bulk List Revert

New `olc_changeset_revert_prefix()` removes all changes matching a prefix:

```c
// Telnet: "revert affects" removes all affects/* changes
// GMCP: {"entity_id": "obj:5#100", "field": "affects"}
int removed = olc_changeset_revert_prefix(cs, "affects");
```

#### GMCP Revert Extension

The existing `handle_editor_revert()` detects whether the field path is an
exact match or a prefix. If exact match fails, it tries prefix-based revert.

#### Telnet Pending Display

The `pending` command groups list operations for readability:

```
[Pending Changes]
 Affects (3 operations):
   1. affects/add:0  - Add: STR +5 (TO_OBJECT)
   2. affects/add:1  - Add: DEX +3 (TO_OBJECT)
   3. affects/rm:0   - Remove: index 2
 Type Data (2 changes):
   4. typedata/weapon/class  - "sword" (was "mace")
   5. typedata/weapon/dice   - 2d6+3 (was 1d8+1)
 Lock:
   6. lock  - Modified (key: 5#100, flags: pickproof)
```

### 6. OEdit Function Inventory

All 51 complex functions to convert in the oedit prototype:

#### List Add/Remove Operations (11 functions)

| Function | List | Lines | JSON Fields |
|----------|------|-------|-------------|
| `oedit_addaffect` | affects | 133 | where, location, modifier, type, duration, bitvector |
| `oedit_delaffect` | affects | 59 | index |
| `oedit_addimmune` | affects | 124 | where (TO_IMMUNE/RESIST/VULN), imm_flags, bitvector |
| `oedit_delimmune` | affects | 59 | index |
| `oedit_addspell` | spells | 105 | spell, level, repop |
| `oedit_delspell` | spells | 47 | index |
| `oedit_addskill` | affects | 74 | where (TO_OBJECT), location (APPLY_SKILL), modifier, skill_name |
| `oedit_addcatalyst` | catalysts | 93 | type, strength, charges, chance, active |
| `oedit_delcatalyst` | catalysts | 47 | index |
| `oedit_addquest` | quests | 47 | vnum |
| `oedit_delquest` | quests | 44 | index |

#### Script Operations (2 functions)

| Function | List | Lines | JSON Fields |
|----------|------|-------|-------------|
| `oedit_addoprog` | oprogs | 108 | vnum, trigger, phrase |
| `oedit_deloprog` | oprogs | 69 | group_index, trigger_index (optional) |

#### Complex Struct Operations (4 functions)

| Function | Struct | Lines | Sub-commands |
|----------|--------|-------|-------------|
| `oedit_lock` | LOCK_STATE | 168 | add, remove, key, flags, pick |
| `oedit_waypoints` | WAYPOINT_DATA list | 180 | list, add, delete |
| `oedit_addtype` | type system | 42 | — |
| `oedit_removetype` | type system | 30 | — |

#### Type-Specific Commands (32 functions in oedit_types.c)

| Function | Type | Lines | Field Count |
|----------|------|-------|-------------|
| `oedit_armor` | ARMOR | 80 | 6 |
| `oedit_bodypart` | BODY_PART | 42 | 2 |
| `oedit_book` | BOOK | 35 | 1 |
| `oedit_cart` | CART | 86 | 7 |
| `oedit_compass` | COMPASS | 37 | 2 |
| `oedit_container` | CONTAINER | 78 | 5 |
| `oedit_corpse` | CORPSE | 89 | 11 |
| `oedit_drink` | DRINK | 69 | 5 |
| `oedit_food` | FOOD | 60 | 3 |
| `oedit_furniture` | FURNITURE | 78 | 8 |
| `oedit_herb` | HERB | 115 | 5 |
| `oedit_ink` | INK | 53 | 2 |
| `oedit_instrument` | INSTRUMENT | 75 | 4 + song list |
| `oedit_jewelry` | JEWELRY | 37 | 2 |
| `oedit_light` | LIGHT | 54 | 3 |
| `oedit_map` | MAP | 53 | 6 |
| `oedit_mist` | MIST | 53 | 4 |
| `oedit_money` | MONEY | 45 | 2 |
| `oedit_page` | PAGE | 46 | 3 |
| `oedit_portal` | PORTAL | 198 | 7+ (context-dependent) |
| `oedit_scroll` | SCROLL | 46 | 3 + spell list |
| `oedit_seed` | SEED | 61 | 4 |
| `oedit_sextant` | SEXTANT | 37 | 1 |
| `oedit_ship` | SHIP | 109 | 14 |
| `oedit_shipmodule` | SHIP_MODULE | 275 | 14+ |
| `oedit_tattoo` | TATTOO | 53 | 5 |
| `oedit_telescope` | TELESCOPE | 103 | 8 + object list |
| `oedit_tool` | TOOL | 47 | 3 |
| `oedit_trade` | TRADE | 51 | 8 |
| `oedit_wand` | WAND | 69 | 4 + spell list |
| `oedit_weapon` | WEAPON | 109 | 10 |
| `oedit_weaponcontainer` | WEAPON_CONTAINER | 59 | 4 |

#### Navigation (2 functions — not staged)

| Function | Lines | Notes |
|----------|-------|-------|
| `oedit_next` | 29 | Navigation only, no data change |
| `oedit_prev` | 28 | Navigation only, no data change |

Navigation commands do not modify entity data and are excluded from staging.

### 7. Infrastructure Shared Across All Editors

These additions to the common framework benefit all editors, not just oedit:

#### New Field Handler Macros

```c
/* For list operations — generates handler that collects and applies all
   operations with matching prefix */
#define OLC_FIELD_APPLY_LIST(func_name, entity_type, apply_add_fn, apply_rm_fn)

/* For embedded struct snapshots */
#define OLC_FIELD_APPLY_EMBEDDED(func_name, entity_type, deserialize_fn)
```

#### olc_staged.h Extensions

```c
/* Get the staged JSON value for a list (returns array of pending operations) */
json_t *olc_staged_list_ops(CHAR_DATA *ch, const char *list_name);

/* Get the staged JSON snapshot for an embedded struct */
json_t *olc_staged_embedded(CHAR_DATA *ch, const char *struct_name);

/* Helper to modify a staged embedded snapshot in place */
bool olc_staged_embedded_set(CHAR_DATA *ch, const char *struct_name,
                              const char *key, json_t *value);
```

### 8. Phasing Within This Phase

Given the scope (51 functions), the oedit prototype is divided into sub-tasks:

1. **Infrastructure** — `olc_changeset_next_seq`, `olc_changeset_revert_prefix`,
   `olc_stage_list_add`, `olc_stage_list_remove`, embedded snapshot helpers,
   `pending` display grouping, GMCP Editor.Set extension.
2. **List operations** — 13 oedit functions (affects, immunities, spells,
   skills, catalysts, quests, scripts).
3. **Complex structs** — 4 oedit functions (lock, waypoints, addtype,
   removetype).
4. **Type dispatch framework** — single wildcard handler with internal routing
   table.
5. **Type-specific conversions (batch 1)** — weapon, armor, container, portal,
   drink, food (common types).
6. **Type-specific conversions (batch 2)** — remaining 26 types.
7. **Tests** — unit tests for list staging, embedded snapshots, type dispatch,
   prefix revert.
8. **Final verification** — clean build + full test suite.
