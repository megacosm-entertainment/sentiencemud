# Widevnum Implementation Plan

**Started:** January 27, 2026
**Last Updated:** February 18, 2026
**Status:** Historical (implementation complete)
**Branch:** `feature/widevnum-migration`

> This plan is retained as an implementation record. Widevnum migration and follow-up cleanup are complete. For final status, see `WIDEVNUM_REMAINING_WORK.md`.

---

## Overview

Widevnums (wide virtual numbers) transition Sentience from global vnums to area-scoped vnums with cross-area references. Instead of every mob/obj/room having a globally unique vnum, each area maintains its own local vnum space, and entities are referenced as `{area, vnum}` pairs.

**Why:** Eliminates vnum namespace collisions, enables area isolation, allows vnum reuse across areas, and provides a modern architecture compatible with JSON persistence and Redis caching.

---

## Data Structures

### Runtime Representation

```c
// In merc.h
typedef struct wnum_data {
    AREA_DATA *pArea;    // Pointer to area (runtime only)
    long vnum;           // Local vnum within area
} WNUM;
```

Used during gameplay. The `pArea` pointer gives O(1) access to the area without needing to look it up.

### Persistent Representation

```c
// In merc.h
typedef struct wnum_load_data {
    long auid;           // Area UID (persistent identifier)
    long vnum;           // Local vnum
} WNUM_LOAD;
```

Used in save files and JSON. Area pointers can't be serialized, so the area's unique ID is stored instead. At boot time, `WNUM_LOAD` values are resolved to `WNUM` values via `get_area_from_uid()`.

### Union Pattern (Backward Compatibility)

Several structs use a union to support both representations during loading:

```c
union {
    WNUM      wnum;      // Runtime: area pointer + vnum
    WNUM_LOAD load;      // Loading: area UID + vnum
    long      vnum;      // Legacy: bare vnum
} entity;
```

Used in: `reset_data`, `shop_stock_data`, blueprint/dungeon/ship reference fields.

### String Formats

| Format | Example | Meaning |
|--------|---------|---------|
| `#1234` | Relative | Vnum 1234 in current area context |
| `923#1234` | Absolute (UID) | Vnum 1234 in area with UID 923 |
| `Midgaard#1234` | Absolute (name) | Vnum 1234 in area named "Midgaard" |
| `'Multi Word'#42` | Quoted name | Vnum 42 in area with spaces in name |
| `1234` | Legacy bare vnum | Resolved via `find_area_by_vnum()` |

### AREA_DATA Extensions

```c
struct area_data {
    // ... existing fields ...
    long uid;                                          // Unique area identifier
    MOB_INDEX_DATA *mob_index_hash[MAX_KEY_HASH];      // Per-area mob hash
    OBJ_INDEX_DATA *obj_index_hash[MAX_KEY_HASH];      // Per-area obj hash
    ROOM_INDEX_DATA *room_index_hash[MAX_KEY_HASH];    // Per-area room hash
    long bottom_mob_vnum, top_mob_vnum;                // Per-area vnum ranges
    long bottom_obj_vnum, top_obj_vnum;
    long bottom_room_vnum, top_room_vnum;
    BLUEPRINT_DATA *blueprint_hash[MAX_KEY_HASH];
    DUNGEON_INDEX_DATA *dungeon_index_hash[MAX_KEY_HASH];
    SHIP_INDEX_DATA *ship_index_hash[MAX_KEY_HASH];
};
```

---

## Core Functions (Implemented)

### Parsing & Formatting (handler.c)

| Function | Purpose |
|----------|---------|
| `parse_widevnum(argument, current_area, &wnum)` | Parse user input to WNUM. Supports all string formats. Legacy bare vnums resolved via `find_area_by_vnum()`. |
| `widevnum_string(pArea, vnum, pRefArea)` | Format area+vnum for display. Relative `#vnum` if same area as pRefArea, absolute `auid#vnum` otherwise. |
| `widevnum_string_wnum(wnum, pRefArea)` | Convenience wrapper for WNUM struct. |
| `widevnum_string_mobile(mob, pRefArea)` | Format mob index for display. |
| `widevnum_string_object(obj, pRefArea)` | Format obj index for display. |
| `widevnum_string_room(room, pRefArea)` | Format room index for display. |

### Index Lookups (db.c)

| Function | Purpose |
|----------|---------|
| `get_mob_index(pArea, vnum)` | Look up mob in specific area's hash table. |
| `get_obj_index(pArea, vnum)` | Look up obj in specific area's hash table. |
| `get_room_index(pArea, vnum)` | Look up room in specific area's hash table. |
| `get_mob_index_global(vnum)` | Search all areas for mob (legacy convenience). |
| `get_obj_index_global(vnum)` | Search all areas for obj (legacy convenience). |
| `get_room_index_global(vnum)` | Search all areas for room (legacy convenience). |

### Area Lookup (db.c)

| Function | Purpose |
|----------|---------|
| `get_area_from_uid(uid)` | Find area by unique ID. |
| `find_area_by_vnum(vnum, context)` | Find which area contains a bare vnum (legacy support). |

---

## Phase Status

| Phase | Description | Status | Completed |
|-------|-------------|--------|-----------|
| 1 | Core Infrastructure | ✅ Complete | Jan 27, 2026 |
| 2 | Index System Migration | ✅ Complete | Jan 28, 2026 |
| -- | JSON Area Implementation | ✅ Complete | Jan 29, 2026 |
| -- | Redis Caching Layer | ✅ Complete | Jan 30, 2026 |
| 3 | Database Loading | ✅ Complete | Jan 29, 2026 |
| 4 | Subsystems (churches, chat, instances, blueprints) | ✅ Complete | Feb 1-2, 2026 |
| 5 | Scripting Engine | ✅ Complete | Feb 2, 2026 |
| 6 | OLC Editors | ✅ Complete | Feb 2, 2026 |
| 7 | Commands & Display | ✅ Complete | Feb 2026 |
| 8 | Polish & Cross-Area Fixes | ✅ Complete | Feb 2026 |

### Phase 1: Core Infrastructure ✅

- `WNUM`/`WNUM_LOAD` structures in `merc.h`
- `parse_widevnum()` with context-aware parsing in `handler.c`
- `widevnum_string()` family in `handler.c`
- Area UID system with `get_area_from_uid()`
- Per-area hash tables for mob/obj/room indices
- Reserved entity system (`get_reserved_*_index` helpers)

### Phase 2: Index System Migration ✅

- All `get_*_index()` functions take area parameter
- Global lookup wrappers for backward compatibility
- 314 compilation errors fixed across 85 files
- Reset system stores WNUM instead of bare vnums
- Shop stock system supports cross-area references

### Phase 3: Database Loading ✅

- Complete JSON serialization/deserialization in `json_area.c` (~2476 lines)
- WNUM format for persistent storage: `"area_uid#vnum"`
- Automatic `.are` fallback loading for backward compatibility
- JSON saves via `asave` command
- Cross-area reference serialization complete

### Phase 4: Subsystems ✅

- **Chat rooms:** JSON format with `area_uid` + `vnum` fields (`json_chat.c`)
- **Reserved entities:** `WNUM_LOAD` structure for persistent storage (`json_reserved.c`)
- **Churches:** JSON format with all church data preserved (`json_church.c`)
- **Instances:** WNUM format for instances/ships/dungeons (`json_instance.c`)
- **Blueprints/Dungeons/Ships:** Area-scoped hash tables with lookup functions

### Phase 5: Scripting Engine ✅

- Added `ENT_WIDEVNUM` entity type to `scripts.h`
- Updated `SCRIPT_PARAM` union to use `WNUM` typedef
- Unified `script_getlocation()` function (~600 lines -> 180 lines)
- All script commands updated: `MLOAD`, `OLOAD`, `GOTO`, `TRANSFER`, `GTRANSFER`
- All commands use `parse_widevnum()` with area context
- Backward compatible with legacy bare vnums

### Phase 6: OLC Editors ✅

Updated 29 functions across 10 editors with context-aware parsing:

- **Core editors:** redit (6), medit (7), oedit (4), aedit (3)
- **Specialized editors:** tedit (1), shedit (3), bpedit (3), dngedit (1), wedit (1)
- **OLC entry commands:** do_redit, do_oedit, do_medit, do_tedit in `olc.c`
- Auto-vnum support in create commands
- All prog commands use area-scoped `get_script_index()`

### Phase 7: Commands & Display 🟡

See [WIDEVNUM_REMAINING_WORK.md](WIDEVNUM_REMAINING_WORK.md) for the complete breakdown.

Key areas:
- ~20 immortal commands need widevnum display/input updates (`act_wiz.c`)
- 8 find commands, 3 stat commands, 3 where commands, 3 load commands, 3 nav commands
- Functions to backport from `src_20_dev`: `wnum_match_*()` family, `resolve_wnum_load()`, convenience wrappers

### Phase 8: Polish & Cross-Area Fixes 🟡

See [WIDEVNUM_REMAINING_WORK.md](WIDEVNUM_REMAINING_WORK.md) for the complete breakdown.

Key areas:
- ~20 cross-area comparison bugs (lock/key, quests, global quests, script interface)
- ~15 struct fields storing bare `long vnum` that need `WNUM`/`WNUM_LOAD` migration
- ~15 hardcoded vnum constants to migrate to reserved entity system
- Display consistency pass across ~50 files

---

## Vnum Search Security

**CRITICAL:** Use hash table iteration for vnum searches, not vnum range iteration.

Iterating from `bottom_vnum` to `top_vnum` creates a denial-of-service vulnerability if a builder creates vnums at extreme ends of the range (e.g., vnum 1 and vnum 2000000000 would iterate 2 billion lookups).

**Secure pattern:**
```c
for (hash = 0; hash < MAX_KEY_HASH; hash++) {
    for (room = area->room_index_hash[hash]; room; room = room->next) {
        if (is_name(argument, room->name))
            found++;
    }
}
```

See [WIDEVNUM_BACKPORT_ANALYSIS.md](WIDEVNUM_BACKPORT_ANALYSIS.md) for detailed analysis.

---

## Performance Architecture

All game logic operates on **in-memory structures**. Redis is only for async disk writes.

```
Update in-memory structure (immediate)
    -> Push to Redis cache (async, fast)
        -> Background worker writes to disk (non-blocking)
```

- Area lookups use in-memory hash table: O(1) average
- Index lookups use per-area hash tables: O(1) average
- Redis is NOT consulted during gameplay
- No performance degradation vs. original global hash tables

---

## Key Files

| Component | File(s) | Lines |
|-----------|---------|-------|
| Core types | `merc.h` | WNUM/WNUM_LOAD typedefs, struct extensions |
| Parsing/formatting | `handler.c` | `parse_widevnum()`, `widevnum_string_*()` |
| Index lookups | `db.c` | `get_*_index()`, `get_*_index_global()` |
| JSON areas | `io/json/json_area.c` | ~2476 lines |
| JSON subsystems | `io/json/json_chat.c`, `json_church.c`, `json_instance.c`, `json_reserved.c` | ~1500 lines |
| Scripting | `scripts.c`, `script_*cmds.c` | ~1000 lines modified |
| OLC editors | `editors/*/` (10 files) + `olc.c` | ~2000 lines modified |
| Tests | `tests/integration/wnum_tests.c`, `tests/unit/test_wnum_standalone.c` | Test coverage |

**Total new/modified code:** ~8000 lines across 85+ files.

---

## Related Documents

- [WIDEVNUM_REMAINING_WORK.md](WIDEVNUM_REMAINING_WORK.md) - Detailed analysis of remaining changes needed (Phase 7-8)
- [WIDEVNUM_BACKPORT_ANALYSIS.md](WIDEVNUM_BACKPORT_ANALYSIS.md) - Detailed comparison of src vs src_20_dev implementations
- [WIDEVNUM_MASTER_PLAN.md](WIDEVNUM_MASTER_PLAN.md) - Original planning document with timeline and risk analysis
- [ARCHITECTURE_CHANGES_ANALYSIS.md](ARCHITECTURE_CHANGES_ANALYSIS.md) - Architectural analysis (index/runtime split, editor restructuring)

---

## Testing

```bash
# Build with test support
cd /sentience/src && ./build tests

# Run all tests
cd /sentience && ./sent -test

# Run widevnum-specific tests
./sent -test:wnum

# Run unit tests only
./sent -test:unit
```

**Test results (as of Feb 2, 2026):** 76 total tests, 48 passed (63% overall, 96% for new handlers). Zero compiler warnings.
