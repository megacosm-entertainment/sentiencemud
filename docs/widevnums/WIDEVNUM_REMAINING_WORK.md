# Widevnum Migration: Remaining Work Analysis

**Date:** February 6, 2026
**Last Updated:** February 7, 2026
**Status:** Phases 1-6 complete. Phase 7 in progress (Cat 1 mostly done, utility functions backported). Phase 8 pending.

## Executive Summary

The core widevnum infrastructure is solid: `WNUM`/`WNUM_LOAD` types, `parse_widevnum()` for input, `widevnum_string_*()` for display, per-area hash tables, and JSON serialization are all working. What remains is systematically updating the ~1,458 `->vnum` references and related code across the codebase to be widevnum-aware.

This document categorizes every remaining change needed, organized by priority and risk.

**Key principle:** `parse_widevnum()` handles legacy bare vnum lookups via `find_area_by_vnum()`, so user-facing *input* paths that already use it are covered. The remaining gaps are:
1. **Display** - showing vnums to users without area context
2. **Storage** - struct fields storing bare `long vnum` instead of `WNUM`/`WNUM_LOAD`
3. **Comparison** - runtime `->vnum == stored_vnum` checks without verifying area match
4. **Commands** - immortal commands that don't accept/display widevnum format

---

## Category 1: Immortal Commands (Phase 7)

These are the most visible changes. Immortals interact with vnums constantly through these commands. All are in `act_wiz.c` unless noted.

### 1A: Find Commands (8 commands) - COMPLETE

All eight use `widevnum_string_*()` for display and support area filtering for input.

| Command | Function | Status |
|---------|----------|--------|
| `mfind` | `do_mfind` | DONE - display + area scoping |
| `ofind` | `do_ofind` | DONE - display + area scoping |
| `tfind` | `do_tfind` | DONE - display + area scoping |
| `rfind` | `do_rfind` | DONE - display + area scoping |
| `bpfind` | `do_bpfind` | DONE - display + area scoping |
| `bsfind` | `do_bsfind` | DONE - display + area scoping |
| `dngfind` | `do_dngfind` | DONE - display + area scoping |
| `shfind` | `do_shfind` | DONE - display + area scoping |

### 1B: Stat Commands (3 commands + sub-dispatchers) - COMPLETE

| Command | Function | Status |
|---------|----------|--------|
| `rstat` | `do_rstat` | DONE - uses `widevnum_string_room()` |
| `ostat` | `do_ostat` | DONE - uses `widevnum_string_object()` |
| `mstat` | `do_mstat` | DONE - uses `widevnum_string_room()` |
| `astat` | `do_astat` | N/A - area-level display, uses area uid directly |

### 1C: Where Commands (3 commands) - MOSTLY COMPLETE

| Command | Function | Status |
|---------|----------|--------|
| `rwhere` | `do_rwhere` | DONE - uses `widevnum_string_room()` |
| `owhere` | `do_owhere` | DONE - uses `widevnum_string_object/mobile/room()` |
| `mwhere` | `do_mwhere` | PARTIAL - room display uses `widevnum_string_room()`, but mob vnum still raw `%ld` in "nowhere" and name-search paths (lines ~4850, ~4879) |

### 1D: Load Commands (3 commands) - MOSTLY COMPLETE

Input parsing uses `parse_widevnum()`. Confirmation/log messages still show raw `%ld` for vnums.

| Command | Function | Status |
|---------|----------|--------|
| `mload` | `do_mload` | Input: DONE. Display: raw `%ld` in confirmation (lines ~5637, 5663) and log (line ~5618) |
| `oload` | `do_oload` | Input: DONE. Display: likely same pattern as mload |
| `sload` | `do_sload` | DISABLED (`#if 0`) |

### 1E: Navigation Commands (3 commands) - COMPLETE

All use `find_location()` which already supports widevnum format (`uid#vnum`, `#vnum`, bare vnum, area name).

| Command | Function | Status |
|---------|----------|--------|
| `goto` | `do_goto` | DONE via `find_location()` |
| `transfer` | `do_transfer` | DONE via `find_location()` |
| `at` | `do_at` | DONE via `find_location()` |

### 1F: Other Commands - PARTIALLY COMPLETE

| Command | Function | File | Status |
|---------|----------|------|--------|
| `token` | `do_token` | `act_wiz.c` | Input: DONE (uses `parse_widevnum()`). Display: not checked. |
| `tset` | `do_tset` | `act_wiz.c` | Not widevnum-related (time setting). |
| `boost` | `do_boost` | `act_wiz.c` | Not widevnum-related (boost setting). |
| `vnum` | `do_vnum` | `act_info.c` | Not checked. |
| `mset` | `do_mset` | `act_wiz.c` | Not checked. |

### Remaining Display Polish

The following are minor display-only gaps where confirmation/log messages still use raw `%ld` for vnums instead of `widevnum_string_*()`:

- `do_mwhere`: mob vnum in "nowhere" and name-search output
- `do_mload`: confirmation messages and permission-denied log
- `do_oload`: confirmation messages (likely same as mload)

---

## Category 2: Cross-Area Comparison Bugs (Critical)

These are runtime logic errors where two vnums are compared without verifying they belong to the same area. With widevnums, different areas can have entities with the same local vnum, making bare vnum comparisons incorrect.

### 2A: Lock/Key System (4 instances, handler.c)

```c
// handler.c:~10114, 10122, 10136, 10144
if (obj->pIndexData->vnum == lock->key_vnum)
```

**Problem:** `lock->key_vnum` is a bare `long`. Two objects from different areas with the same vnum would both unlock the same door.

**Fix:** Change `LOCK_STATE.key_vnum` (merc.h) to `WNUM_LOAD key` or add `long key_area_uid` alongside `key_vnum`. Compare both area and vnum in the check:
```c
if (obj->pIndexData->vnum == lock->key_vnum
    && obj->pIndexData->area == lock->key_area)
```

**Risk:** HIGH - Keys unlocking wrong doors is a game-breaking bug.

### 2B: Global Quest System (5+ instances, handler.c / db.c / gq.c)

```c
// handler.c:3176, 4961 - mob checking
if (ch->pIndexData->vnum == gq_mob->vnum)

// db.c:3337, 3661 - index counting
if (pMobIndex->vnum == gq_mob->vnum)

// gq.c:317 - duplicate check
if (gq_mob->vnum == vnum)
```

**Problem:** The `GQ_MOB_DATA` and `GQ_OBJ_DATA` structures store bare `long vnum` for quest targets. Two mobs from different areas with the same vnum would both count as quest targets.

**Fix:** Add area tracking to GQ mob/obj entries. Store `WNUM_LOAD` or `{area_uid, vnum}` pair. Update all comparisons to check both fields.

**Risk:** HIGH - Wrong mobs counting for quests breaks gameplay.

### 2C: Quest System (6 instances, quest.c)

```c
// quest.c:480, 490, 501
if (IS_NPC(mob) && mob->pIndexData->vnum == ch->quest->questgiver)

// quest.c:606, 619, 631
if (IS_NPC(mob) && mob->pIndexData->vnum == ch->quest->questreceiver)
```

**Problem:** `quest->questgiver` and `quest->questreceiver` are bare `long` vnums. Mobs with the same vnum in different areas would incorrectly match.

**Fix:** Add area_uid fields to quest data (`questgiver_auid`, `questreceiver_auid`) or change to `WNUM_LOAD`. Update comparisons.

**Risk:** HIGH - Quest givers/receivers misidentified breaks quest system.

### 2D: Object Duplicate Check (1 instance, act_obj.c)

```c
// act_obj.c:1045
if (obj->pIndexData->vnum == key->pIndexData->vnum)
```

**Problem:** Comparing two different object vnums without area context.

**Fix:** Add area comparison: `&& obj->pIndexData->area == key->pIndexData->area`.

**Risk:** MEDIUM - Affects container logic.

### 2E: Script Interface (5+ instances, script_ifc.c)

```c
// script_ifc.c:857, 968, 1011, 1059, 1079
ARG_MOB(0)->fighting->pIndexData->vnum == ARG_NUM(1)
```

**Problem:** Script `ARG_NUM` passes bare vnums for mob comparisons. Scripts could match wrong mobs across areas.

**Fix:** Add script functions that compare by WNUM, or require scripts to use widevnum format. Consider adding `ARG_WNUM()` macro.

**Risk:** HIGH - Scripts are the backbone of game logic.

---

## Category 3: Struct Fields Storing Bare Vnums

These are `long` fields in structs that store a vnum without area context. They need migration to `WNUM_LOAD` (for persistence) or `WNUM` (for runtime) to be fully widevnum-safe.

### Tier 1: Critical (Cross-area scenarios are common)

| Struct | Field | File | Notes |
|--------|-------|------|-------|
| `MAIL_DATA` | `from_location` | merc.h:~4122 | Mail origin room - always cross-area |
| `MAIL_DATA` | `to_location` | merc.h:~4123 | Mail destination room - always cross-area |
| `AREA_DATA` | `post_office` | merc.h:~5718 | Area post office room vnum |
| `AREA_DATA` | `airship_land_spot` | merc.h:~5691 | Airship landing room - can be cross-area |
| `CHAR_DATA` | `vnum_of_boat_before_logoff` | merc.h:~5203 | Player's boat vnum saved across sessions |
| `GQ_MOB_DATA` | `vnum` | (gq structures) | Global quest target mob |
| `GQ_OBJ_DATA` | `vnum` | (gq structures) | Global quest target object |
| `quest data` | `questgiver` | (quest structures) | Quest giver mob vnum |
| `quest data` | `questreceiver` | (quest structures) | Quest receiver mob vnum |

### Tier 2: High Priority (Cross-area possible)

| Struct | Field | File | Notes |
|--------|-------|------|-------|
| `MOB_INDEX_DATA` | `corpse` | merc.h:~4085 | Corpse object vnum - could be from another area |
| `MOB_INDEX_DATA` | `zombie` | merc.h:~4086 | Animated corpse vnum |
| `OBJ_DATA` | `created_script_vnum` | merc.h:~5434 | Script that created object |
| `OBJ_DATA` | `orig_vnum` | merc.h:~5456 | Original object vnum before modification |
| `EXIT_DATA` | `u1.vnum` | merc.h:~5530 | Exit destination (during loading only) |
| `LOCK_STATE` | `key_vnum` | merc.h:~5301 | Lock key object vnum |

### Tier 3: Medium Priority

| Struct | Field | File | Notes |
|--------|-------|------|-------|
| `TRADE_ITEM` | `obj_vnum` | merc.h:~5822 | Trade item - usually area-local |
| `newbie_eq_type` | `vnum` | merc.h:~2143 | Starting equipment - static |
| `weapon_type` | `vnum` | merc.h:~2163 | Starting weapons - static |
| `map_exit_type` | `room_vnum` | merc.h:~2157 | Wilderness map exits |
| `tunneler_place_type` | `vnum` | merc.h:~6760 | Tunneler destinations |

### Tier 4: Low Priority (Already area-local or legacy unions)

Fields already using the `union { WNUM wnum; WNUM_LOAD load; long vnum; }` pattern are partially migrated. The `long vnum` union member provides backward compatibility during loading.

---

## Category 4: Hardcoded Vnum Constants

These use `#define` constants like `OBJ_VNUM_*`, `MOB_VNUM_*`, `ROOM_VNUM_*` that assume global vnum uniqueness.

### Current Pattern (Fragile)

```c
// Scattered across multiple files
if (obj->pIndexData->vnum == OBJ_VNUM_ABYSS_PORTAL)    // db.c:1977
if (container->pIndexData->vnum == OBJ_VNUM_CURSED_ORB) // act_obj.c:1070
if (obj->pIndexData->vnum == OBJ_VNUM_GOLD_WHISTLE)     // mount.c:197
if (obj->pIndexData->vnum == OBJ_VNUM_RELIC_EXTRA_DAMAGE) // act_obj.c:8633
if (obj->pIndexData->vnum == 100035)                     // save.c:3800 (magic number!)
if (obj->pIndexData->vnum == 152533)                     // act_info.c:2629 (magic number!)
```

**Strategy:** These should migrate to the **reserved entity system** which already exists and uses WNUM internally. Reserved entities are looked up by *name* (e.g., `get_reserved_obj_index("abyss_portal")`), making them area-independent.

### Migration Steps

1. Register each hardcoded vnum as a reserved entity
2. Replace `OBJ_VNUM_CONSTANT` checks with `get_reserved_obj_index("name")` lookups
3. Replace magic number checks (`100035`, `152533`) with named reserved entities
4. Remove `#define OBJ_VNUM_*` constants from merc.h
5. Document all reserved entities in a configuration file

### Relic System (Special Case)

The relic system in `db.c` sets global pointers during boot:
```c
// db.c:3643-3655
if (pObjIndex->vnum == OBJ_VNUM_RELIC_EXTRA_DAMAGE) relic_extra_damage = pObjIndex;
if (pObjIndex->vnum == OBJ_VNUM_RELIC_EXTRA_HP)     relic_extra_hp = pObjIndex;
// ... etc
```

These should use the reserved entity system to look up relics by name at boot time.

---

## Category 5: Display Consistency

~53 files format vnums for display. Many use raw `%ld` format for `->vnum` fields. These should use `widevnum_string_*()` functions for consistent widevnum display.

### Files Needing Display Updates

**High traffic (immortals see these constantly):**
- `act_wiz.c` - stat, find, where, load commands (covered in Category 1)
- `act_info.c` - vnum display, player info commands
- `olc.c` - OLC entry/list displays

**Medium traffic (builders see these):**
- `editors/rooms/redit.c` - Room display/list formatting
- `editors/mobiles/medit.c` - Mobile display/list formatting
- `editors/objects/oedit.c` - Object display/list formatting
- `editors/areas/aedit.c` - Area display
- `editors/blueprints/bpedit.c` - Blueprint display
- `editors/blueprints/bsedit.c` - Blueprint section display

**Low traffic (logging, debugging):**
- `io/json/json_char.c` - Character save/load logging
- `log.c` - Error logging with vnum context
- Various `bug()` and `log_string()` calls throughout

### Display Function Selection Guide

| Entity Type | Function to Use |
|-------------|----------------|
| Room | `widevnum_string_room(room, pRefArea)` |
| Mobile index | `widevnum_string_mobile(mob_index, pRefArea)` |
| Object index | `widevnum_string_object(obj_index, pRefArea)` |
| WNUM struct | `widevnum_string_wnum(wnum, pRefArea)` |
| Raw area+vnum | `widevnum_string(pArea, vnum, pRefArea)` |

**`pRefArea` usage:**
- `NULL` = always show absolute format (`923#1234`)
- `ch->in_room->area` = show relative `#1234` for same-area, absolute for cross-area
- In OLC: use the area being edited as pRefArea

---

## Category 6: Function Signatures

Several functions take bare `long vnum` parameters where they should accept `WNUM` or `WNUM *`.

### Global Lookup Functions (Keep as convenience wrappers)

These intentionally take bare vnums and iterate all areas. They should remain but be documented as legacy/convenience:

- `get_mob_index_global(long vnum)` - Searches all areas
- `get_obj_index_global(long vnum)` - Searches all areas
- `get_room_index_global(long vnum)` - Searches all areas
- `get_script_index_global(long vnum, int type)` - Searches all areas
- `find_area_by_vnum(long vnum, AREA_DATA *context)` - Finds area containing vnum

These are used by `parse_widevnum()` for legacy bare vnum resolution and should not be removed.

### Functions Needing WNUM Variants

| Function | File | Current Sig | Suggested Change |
|----------|------|-------------|-----------------|
| `find_path` | pathfinding | `(long in_room_vnum, long out_room_vnum, ...)` | Add `find_path_wnum(WNUM from, WNUM to, ...)` |
| `get_char_world_vnum` | handler.c | `(CHAR_DATA *ch, long vnum)` | Add area parameter or WNUM variant |
| `create_invasion_quest` | handler.c | `(AREA_DATA*, int, long, long)` | Change mob vnums to WNUM |
| `generate_quest_scroll` | quest.c | `(... long vnum, ...)` | Change to WNUM |
| `get_npc_ship_index` | handler.c | `(long vnum)` | Add area parameter |

### Token Lookup Functions

These currently take bare `long vnum`:
- `get_token_list(LLIST *tokens, long vnum, int count)`
- `get_token_char(CHAR_DATA *ch, long vnum, int count)`
- `get_token_obj(OBJ_DATA *obj, long vnum, int count)`
- `get_token_room(ROOM_INDEX_DATA *room, long vnum, int count)`

**Assessment:** Tokens are looked up on their *owner* (char/obj/room), so the token's vnum is being compared against tokens already attached to that owner. Since token attachment is area-scoped at creation time, these comparisons are **likely safe** but should be verified. Consider adding `_wnum` variants for new code.

---

## Category 7: Subsystem-Specific Issues (Phase 4)

### 7A: Blueprint/Instance System

The blueprint system already uses union patterns with WNUM/WNUM_LOAD/long. Several fields are marked as "Legacy: bare vnum" in the unions:
- `BLUEPRINT_SPECIAL_ROOM_DATA.room_ref.vnum`
- `DUNGEON_INDEX_DATA.entry_ref.vnum`
- `DUNGEON_INDEX_DATA.exit_ref.vnum`
- `SHIP_INDEX_DATA.ship_object_ref.vnum`
- `SHIP_INDEX_DATA.blueprint_ref.vnum`

**Status:** These have the union structure ready. The WNUM/WNUM_LOAD members are populated during JSON loading. The bare `long vnum` member is used during legacy .are loading as a fallback. This pattern is functional but the legacy path should eventually be removed.

### 7B: Ship/Boat System

`boat.c` has hardcoded room vnum comparisons:
```c
if (ship->ship->in_room->vnum == ROOM_VNUM_SEA_PLITH_HARBOUR || ...)
```

These should use reserved room entities or WNUM comparisons.

### 7C: Portal System

`act_enter.c` stores portal destination room vnums in object value fields (`obj->value[3]`). These bare longs need area context for cross-area portals.

**Note:** Object `value[]` fields are generic `long` arrays. Adding area context here requires either:
- A convention (e.g., `value[3]` = room vnum, `value[4]` = area uid)
- Or using the extended value system if one exists

---

## Recommended Implementation Order

### Phase 7A: Command Display Updates (Low Risk, High Visibility)

Update all immortal commands to **display** widevnums correctly using `widevnum_string_*()`. This doesn't change any game logic, just how vnums appear in output.

**Files:** `act_wiz.c`, `act_info.c`
**Scope:** ~60 sprintf changes
**Risk:** LOW - display-only changes

### Phase 7B: Command Input Updates (Low Risk)

Update immortal commands to **accept** widevnum input via `parse_widevnum()`.

**Files:** `act_wiz.c` (goto, mload, oload, transfer, at, sload)
**Scope:** ~10 input parsing changes
**Risk:** LOW - parse_widevnum handles legacy bare vnums

### Phase 7C: Cross-Area Comparison Fixes (High Risk, Critical)

Fix runtime comparison bugs in priority order:

1. **Lock/key system** - `handler.c` (4 comparisons)
2. **Quest system** - `quest.c` (6 comparisons)
3. **Global quest system** - `handler.c`, `db.c`, `gq.c` (5+ comparisons)
4. **Script interface** - `script_ifc.c` (5+ comparisons)
5. **Object duplicate check** - `act_obj.c` (1 comparison)

**Scope:** ~20 comparison fixes + struct field additions
**Risk:** HIGH - changes game logic, needs thorough testing

### Phase 7D: Struct Field Migration (Medium Risk)

Migrate bare `long vnum` fields to `WNUM_LOAD` in persistence structures:

1. **Tier 1:** Mail, GQ, quest data (cross-area by nature)
2. **Tier 2:** Corpse/zombie vnums, key vnums, script vnums
3. **Tier 3:** Static data (newbie eq, weapons, map exits)

**Risk:** MEDIUM - requires updating serialization code

### Phase 7E: Hardcoded Constant Migration (Low Risk)

Migrate `OBJ_VNUM_*` / `ROOM_VNUM_*` / `MOB_VNUM_*` constants to reserved entity system.

**Risk:** LOW - reserved entity system already exists and works

### Phase 8: Polish

1. Display consistency pass across all files
2. Remove `_global()` function calls where area context is available
3. Documentation and builder guide
4. Test suite expansion for cross-area scenarios
5. Performance validation

---

## Metrics

| Category | Items | Priority | Risk |
|----------|-------|----------|------|
| Command display updates | ~60 sprintf | HIGH | LOW |
| Command input updates | ~10 parse | HIGH | LOW |
| Cross-area comparison fixes | ~20 comparisons | CRITICAL | HIGH |
| Struct field migrations | ~15 fields | MEDIUM | MEDIUM |
| Hardcoded constant migration | ~15 constants | LOW | LOW |
| Display consistency (other files) | ~50 files | LOW | LOW |
| Function signature updates | ~10 functions | LOW | MEDIUM |

**Total estimated changes:** ~180 discrete modifications across ~30 files.

---

## Functions Backported from src_20_dev

**Status: COMPLETE** (February 7, 2026)

All needed utility functions have been backported. Functions not needed have been assessed and excluded.

### Backported (in handler.c unless noted)

| Function | Purpose |
|----------|---------|
| `wnum_match(WNUM, AREA_DATA*, long)` | Raw area+vnum comparison |
| `wnum_match_room(WNUM, ROOM_INDEX_DATA*)` | Room comparison (rejects clone rooms) |
| `wnum_match_obj(WNUM, OBJ_DATA*)` | Object instance comparison |
| `wnum_match_mob(WNUM, CHAR_DATA*)` | Mobile instance comparison (NPC only) |
| `wnum_match_token(WNUM, TOKEN_DATA*)` | Token instance comparison |
| `get_room_wnum(ROOM_INDEX_DATA*, WNUM*)` | Extract WNUM from room |
| `get_mob_wnum(MOB_INDEX_DATA*, WNUM*)` | Extract WNUM from mob index |
| `get_obj_wnum(OBJ_INDEX_DATA*, WNUM*)` | Extract WNUM from obj index |
| `get_token_wnum(TOKEN_INDEX_DATA*, WNUM*)` | Extract WNUM from token index |
| `widevnum_string_script(SCRIPT_DATA*, AREA_DATA*)` | Script display string |
| `wnum_zero` (db.c) | Zero-initialized WNUM constant |
| `resolve_wnum_load()` | WNUM_LOAD → WNUM boot resolution (backported earlier) |

**Already existed:** `widevnum_string_token()`, `get_token_index_wnum()`

### Not Needed

| Function | Why Skip |
|----------|----------|
| `get_*_index_wnum()` (mob/obj/room) | Trivial one-liners; callers can inline `get_*_index(wnum.pArea, wnum.vnum)`. Add later if pattern becomes repetitive. |
| `get_*_index_auid()` (3 functions) | Same reasoning - resolve area UID first, then call existing functions. |
| `RESERVED_WNUM` + `resolve_reserved*` | Our existing `RESERVED_DATA` system with `get_reserved_*_index(name)` already does this with `WNUM_LOAD` internally. More flexible (no C code changes to add items). |
| `fread_widevnum()` / `fread_widevnumptr()` | Legacy `.are` file readers. We use JSON. |
| `convert_vnum_to_widevnum()` | Already have `find_area_by_vnum()` (same logic, different name). |

---

## LOCK_STATE System: src vs src_20_dev Comparison

The lock/key system is the clearest example of what needs to change. Here is the exact diff between the two codebases:

### Current (`src`) - Bare vnum

```c
// merc.h
typedef struct lock_state_data {
    long key_vnum;       // Bare vnum - no area context
    int pick_chance;
    int flags;
    LLIST *keys;
} LOCK_STATE;

// handler.c - bare vnum comparison
if (obj->pIndexData->vnum == lock->key_vnum)
```

### Target (`src_20_dev`) - WNUM-aware

```c
// merc.h - new sub-struct for special keys
typedef struct lock_state_key_data LOCK_STATE_KEY;
struct lock_state_key_data {
    WNUM_LOAD load;      // Persistent: area_uid + vnum
    WNUM wnum;           // Runtime: area pointer + vnum
};

// merc.h - updated lock state
typedef struct lock_state_data {
    WNUM_LOAD key_load;  // Persistent key reference
    WNUM key_wnum;       // Runtime key reference (area + vnum)
    int pick_chance;
    int flags;
    LLIST *special_keys; // Renamed from 'keys'
} LOCK_STATE;

// handler.c - area-aware comparison via wnum_match_obj()
if (wnum_match_obj(lock->key_wnum, obj))
```

### What Changes

1. `long key_vnum` -> `WNUM key_wnum` + `WNUM_LOAD key_load`
2. `SPECIAL_KEY_DATA.key_vnum` -> `SPECIAL_KEY_DATA.key_wnum`
3. `keys` list renamed to `special_keys`
4. All 4 bare `== lock->key_vnum` comparisons -> `wnum_match_obj(lock->key_wnum, obj)`
5. `lockstate_functional()` checks `key_wnum.pArea && key_wnum.vnum > 0` instead of `key_vnum > 0`
6. New `lockstate_iskey(lock, obj)` predicate added (checks special keys, then falls back to `wnum_match_obj`)
7. Boot-time resolution: `fix_rooms()` resolves `key_load` -> `key_wnum` via `get_area_from_uid()`
8. Serialization in `json_persist.c` saves/loads `key_load` (area_uid + vnum)

This pattern (dual `WNUM_LOAD` for persistence + `WNUM` for runtime, resolved at boot, compared via `wnum_match_*`) is the template for all other struct field migrations in Category 3.
