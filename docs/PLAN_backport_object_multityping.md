# Backport: Object Multityping & Type-Specific Data

**Status:** In Progress / Planned (verified 2026-02-26 docs audit)


This document tracks the backport of the Object Multityping system from the `src_20_dev` branch into the main `src` codebase, and the ongoing deprecation of the legacy `value[]` array.

---

## 1. System Overview

The object multityping system moves object-specific data out of the overloaded `long value[8]` integer array and into dedicated, type-specific structures (e.g., `WEAPON_DATA`, `ARMOR_DATA`, `PORTAL_DATA`). This makes the code self-documenting, type-safe, and extensible with fields that go beyond simple integers (strings, sub-structs, lists, etc.).

**Example:**
```c
// Legacy (deprecated):
obj->value[1] = 10;  // dice number? charge count? who knows?

// New (canonical):
OBJ_WEAPON(obj)->damage.number = 10;  // clearly weapon dice count
```

---

## 2. Implementation Status

### Phase 1: Data Structures — COMPLETE ✓

**Files created:**
| File | Lines | Purpose |
|------|------:|---------|
| `item_types.h` | 970 | 32 type structs, accessor macros (`OBJ_WEAPON()`, `IS_WEAPON()`), bitset compatibility system, API declarations |
| `item_types.c` | 1,127 | Compatibility table, lifecycle functions (`obj_free_type_data`, `obj_copy_type_data`), value[] → struct migration |
| `item_type_mem.c` | 1,710 | Memory management with 32 free-list pools (`new_*_data()`, `free_*_data()`, `copy_*_data()`) |

**Type structs defined (32 total):**
`ARMOR_DATA`, `BODY_PART_DATA`, `BOOK_DATA`, `CART_DATA`, `COMPASS_DATA`, `CONTAINER_DATA`, `CORPSE_DATA`, `FLUID_CONTAINER_DATA`, `FOOD_DATA`, `FURNITURE_DATA`, `HERB_DATA`, `INK_DATA`, `INSTRUMENT_DATA`, `ITEM_SHIP_DATA`, `JEWELRY_DATA`, `LIGHT_DATA`, `MAP_DATA`, `MIST_DATA`, `MONEY_DATA`, `PAGE_DATA`, `PORTAL_DATA`, `SCROLL_DATA`, `SEED_DATA`, `SEXTANT_DATA`, `TATTOO_DATA`, `TELESCOPE_DATA`, `TOOL_DATA`, `TRADE_DATA`, `WAND_DATA`, `WEAPON_CONTAINER_DATA`, `WEAPON_DATA`, plus supporting `BOOK_PAGE`, `CONTAINER_FILTER`, `INSTRUMENT_RESERVOIR_DATA`.

**Accessor macros:** Each type has `OBJ_<TYPE>(obj)` (returns pointer, asserts non-NULL) and `IS_<TYPE>(obj)` (NULL-safe bool check). Both work on `OBJ_DATA` and `OBJ_INDEX_DATA` via the shared `_<type>` pointer fields added to both structs in `merc.h`.

**Multi-typing:** Data-driven bitset compatibility system allows objects to hold multiple type structs simultaneously (e.g., a weapon that is also a container). Compatibility rules are declared in `init_item_type_compat()`.

### Phase 2: Versioning & Migration — COMPLETE ✓

**Object version:** `VERSION_OBJECT_005` (`0x01000004`) added to `merc.h`.

**Migration functions** (in `item_types.c`):
- `obj_migrate_values_to_types(OBJ_DATA *obj)` — for instances
- `obj_index_migrate_values_to_types(OBJ_INDEX_DATA *obj)` — for templates

These read from the legacy `value[]` array and populate the corresponding type struct. Uses a shared `MIGRATE_VALUES_BODY` macro to avoid duplicating the large switch for both struct types. Covers all 29 item types that use value[] slots.

**Migration hooks:**
- `fix_object()` in `save.c` — migrates instances when `obj->version < VERSION_OBJECT_005`
- `read_object_new()` in `olc_save.c` — migrates templates when `area->version_object < VERSION_OBJECT_005`

### Phase 3: Serialization — COMPLETE ✓

**Files created:**
| File | Lines | Purpose |
|------|------:|---------|
| `io/json/json_obj_types.h` | 33 | Declares 4 public serialization functions |
| `io/json/json_obj_types.c` | 975 | Per-type `*_to_json()` / `*_from_json()` static functions for all 31 types, plus aggregate functions |

**Public API:**
- `obj_type_data_to_json(OBJ_DATA *)` → `json_t *`
- `obj_index_type_data_to_json(OBJ_INDEX_DATA *)` → `json_t *`
- `obj_type_data_from_json(OBJ_DATA *, json_t *)` — deserialize
- `obj_index_type_data_from_json(OBJ_INDEX_DATA *, json_t *)` — deserialize

**All 6 save/load paths wired:**

| Path | File | Format | Status |
|------|------|--------|--------|
| Text instance (pfiles) | `save.c` | `TypeData <json>~` | ✓ Read + Write |
| Text persist (world) | `db.c` | `TypeData <json>~` | ✓ Read + Write |
| Text template (areas) | `olc_save.c` | `TypeData <json>~` | ✓ Read + Write |
| JSON char (pfiles) | `io/json/json_char.c` | `"type_data": {...}` | ✓ Read + Write |
| JSON persist (world) | `io/json/json_persist.c` | `"type_data": {...}` | ✓ Read + Write |
| JSON area (templates) | `io/json/json_area.c` | `"type_data": {...}` | ✓ Read + Write |

**Object lifecycle propagation:**
- `create_object_noid()` in `db.c` — calls `obj_migrate_values_to_types()` after copying values from template
- `clone_object()` in `db.c` — calls `obj_free_type_data()` + `obj_copy_type_data()` after copying values from parent

### Phase 4: Deprecate value[] Writes — COMPLETE ✓

**Legacy `value[]` is no longer written on save.** All 6 save paths now write only `TypeData`/`type_data` (the JSON-serialized type structs). The `value[]` read paths are retained solely for migrating old data files that pre-date `VERSION_OBJECT_005`.

**Files modified:**
- `save.c` — Removed `Val` line write from `fwrite_obj_new()`
- `db.c` — Removed `Value` loop from `persist_save_object()`
- `olc_save.c` — Removed `Values` line from `save_object_new()`
- `io/json/json_char.c` — Removed `"values"` array from `obj_to_json()`
- `io/json/json_persist.c` — Removed `"values"` array from `json_persist_object_to_json()`
- `io/json/json_area.c` — Removed `"values"` array from `json_area_serialize_object()`

---

## 3. Remaining Work

### Phase 5: Convert Game Logic to Use Type Accessors — IN PROGRESS

~1,100+ lines across ~35 files still read `->value[]` directly for gameplay behavior. These must be converted to use the type struct accessors (e.g., `obj->value[1]` for a weapon → `OBJ_WEAPON(obj)->damage.number`).

**By priority / item type frequency:**

| Item Type | Key Files | Estimated Lines |
|-----------|-----------|----------------:|
| **Portal** (`value[0-7]`) | `act_enter.c` (47), `act_move.c` (71), `handler.c`, `magic_astral.c` (20), `magic_law.c` (34) | ~200+ |
| **Weapon** (`value[0-4]`) | `fight.c` (39), `fight2.c` (12), `shoot.c` (30), `act_obj.c`, `handler.c` | ~120+ |
| **Container** (`value[0-4]`) | `act_obj.c`, `act_obj2.c`, `handler.c` | ~60+ |
| **Furniture** (`value[0-5]`) | `act_move.c`, `act_info.c` | ~40+ |
| **Drink/Fountain/Food** | `act_obj.c`, `act_obj2.c` | ~50+ |
| **Light** (`value[2]`) | `handler.c`, `update.c` | ~15 |
| **Money** (`value[0-1]`) | `act_obj.c`, `handler.c` | ~15 |
| **Armor** (`value[0-3]`) | `handler.c`, `fight.c` | ~15 |
| **Other types** | Various | ~100+ |

**Scripting engine** (~120 lines): `script_ifc.c` (37), `script_opcmds.c` (12), `script_tpcmds.c` (24), `script_commands.c` (13), etc. — these expose value[] to builders' scripts and will need accessor functions or migration.

**OLC editors** (~325 lines): `olc_act.c` (297), `olc.c` (19), `editors/objects/oedit.c` (7) — these display/edit value[] and will eventually need a new field-based editing interface.

### Phase 6: OLC Refactor — NOT STARTED

Replace `value0`..`value7` commands in `oedit` with type-aware field commands (e.g., `oedit damage 10d4`, `oedit capacity 500`, `oedit portal_flags nocurse gowith`).

### Phase 7: Script Engine Accessors — NOT STARTED

Provide script-engine functions for reading/writing type struct fields, so builder scripts can use named fields instead of raw value indices.

### Phase 8: Remove value[] Field — NOT STARTED

Once all reads are converted and all old data has been migrated, remove `long value[8]` from `OBJ_DATA` and `OBJ_INDEX_DATA` entirely.

---

## 4. Build System

Both `CMakeLists.txt` and `Makefile` updated with:
- `item_types.c`
- `item_type_mem.c`
- `io/json/json_obj_types.c`

Current build: **clean** (no warnings from our code).
