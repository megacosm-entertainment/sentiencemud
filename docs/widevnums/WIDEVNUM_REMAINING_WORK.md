# Widevnum Migration: Remaining Work Analysis

**Date:** February 6, 2026
**Last Updated:** February 18, 2026
**Status:** **MIGRATION COMPLETE.** Phases 1-8 done. All 8 categories finished: commands, cross-area comparisons, struct fields, hardcoded constants, display consistency, function signatures, subsystem cleanup, scripting system. Post-completion cleanup/regression fixes also complete.

## Executive Summary

The core widevnum infrastructure is solid: `WNUM`/`WNUM_LOAD` types, `parse_widevnum()` for input, `widevnum_string_*()` for display, per-area hash tables, and JSON serialization are all working. This document records the completed migration of the ~1,458 `->vnum` reference patterns and related widevnum hardening across the codebase.

This document now serves as a completion ledger of that migration work, organized by category and risk, including post-completion fixes.

## Post-Completion Fixes (2026-02-18) — COMPLETE

- Script parsing/context hardening pass completed: context area now only applies for `#vnum` relative input, avoiding accidental cross-area reinterpretation.
- JSON shop stock regression fixed: cross-area stock references now preserve/derive valid area identity during fixup and serialization, preventing `0#<vnum>` output for valid items.

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

### 1C: Where Commands (3 commands) - COMPLETE

| Command | Function | Status |
|---------|----------|--------|
| `rwhere` | `do_rwhere` | DONE - uses `widevnum_string_room()` |
| `owhere` | `do_owhere` | DONE - uses `widevnum_string_object/mobile/room()` |
| `mwhere` | `do_mwhere` | DONE - uses `widevnum_string_room()` and `widevnum_string_mobile()` |

### 1D: Load Commands (3 commands) - COMPLETE

| Command | Function | Status |
|---------|----------|--------|
| `mload` | `do_mload` | DONE - uses `widevnum_string_mobile()` for confirmation, log, and permission messages |
| `oload` | `do_oload` | DONE - uses `widevnum_string_object()` |
| `sload` | `do_sload` | DISABLED (`#if 0`) |

### 1E: Navigation Commands (3 commands) - COMPLETE

All use `find_location()` which already supports widevnum format (`uid#vnum`, `#vnum`, bare vnum, area name).

| Command | Function | Status |
|---------|----------|--------|
| `goto` | `do_goto` | DONE via `find_location()` |
| `transfer` | `do_transfer` | DONE via `find_location()` |
| `at` | `do_at` | DONE via `find_location()` |

### 1F: Other Commands - COMPLETE

| Command | Function | File | Status |
|---------|----------|------|--------|
| `token` | `do_token` | `act_wiz.c` | DONE - input uses `parse_widevnum()`, display uses `widevnum_string()` (Cat 5) |
| `tset` | `do_tset` | `act_wiz.c` | N/A - not widevnum-related (time setting) |
| `boost` | `do_boost` | `act_wiz.c` | N/A - not widevnum-related (boost setting) |
| `vnum` | `do_vnum` | `act_info.c` | DONE - dispatcher to do_mfind/do_ofind/do_tfind (all completed) |
| `mset` | `do_mset` | `act_wiz.c` | N/A - modifies stat/resource fields, no vnum display |

---

## Category 2: Cross-Area Comparison Bugs (Critical)

These are runtime logic errors where two vnums are compared without verifying they belong to the same area. With widevnums, different areas can have entities with the same local vnum, making bare vnum comparisons incorrect.

### 2A: Lock/Key System (4 instances, handler.c) - COMPLETE

**Status:** DONE (February 9, 2026). Full lockstate system backported from src_20_dev:
- `LOCK_STATE` struct: `long key_vnum` → `WNUM_LOAD key_load` + `WNUM key_wnum`, `keys` → `special_keys`
- `SPECIAL_KEY_DATA`: `long key_vnum` → `WNUM key_wnum`
- All 4 bare vnum comparisons replaced with `wnum_match_obj()`
- New `lockstate_iskey()` predicate added
- New lock flags: `LOCK_FREE_KEYS`, `LOCK_CHECK_BOTH`, `LOCK_FINAL`, `LOCK_NOMAGIC`, `LOCK_NOSCRIPT`
- Boot-time resolution: `fix_rooms()` resolves exit lock keys, new `fix_object_locks()` resolves obj lock keys
- JSON serialization updated with backward compat (reads legacy bare vnum, writes auid+vnum)
- All script, OLC, save/load, and display code updated across ~15 files

### 2B: Global Quest System - COMPLETE

**Status:** DONE (February 9, 2026).
- `GQ_MOB_DATA`: `long vnum` → `WNUM_LOAD vnum_load` + `WNUM vnum_wnum`; `long obj` → `WNUM_LOAD obj_load` + `WNUM obj_wnum`
- `GQ_OBJ_DATA`: `long vnum` → `WNUM_LOAD vnum_load` + `WNUM vnum_wnum`
- All comparisons use `wnum_match()` / `wnum_match_mob()` / `wnum_match_obj()`
- New `gq_resolve_wnum_load()` and `gq_set_load_from_wnum()` helpers
- JSON serialization updated with backward compat (reads legacy bare vnum, writes auid+vnum)
- GQ add/remove commands accept widevnum input via `parse_widevnum()`
- Files: merc.h, gq.c, handler.c, db.c, db2.c, act_obj2.c, json_gq.c, mem.c

### 2C: Quest System - COMPLETE

**Status:** DONE (February 9, 2026).
- `QUEST_DATA`: `long questgiver/questreceiver` → `WNUM_LOAD *_load` + `WNUM *_wnum`
- `QUEST_PART_DATA`: `long obj/mob/room/obj_sac/mob_rescue` → `WNUM_LOAD *_load` + `WNUM *_wnum` pairs
- All comparisons use `wnum_match_mob()` / `wnum_match_obj()` / `wnum_match_room()`
- New `quest_set_wnum()` and `quest_part_resolve()` helpers
- Player save/load updated: writes `QuestGiverW`/`QuestReceiverW` widevnum strings, reads both legacy bare vnum and new format
- JSON char serialization updated with backward compat
- Script quest commands updated to populate WNUM fields from runtime entities
- Files: merc.h, quest.c, save.c, json_char.c, script_commands.c, mem.c

### 2D: Object Duplicate Check - COMPLETE

**Status:** DONE (February 9, 2026).
- `act_obj.c`: Added `&& obj->pIndexData->area == key->pIndexData->area` to the container duplicate check

### 2E: Script Interface - COMPLETE

**Status:** DONE (February 9, 2026).
- New `resolve_legacy_vnum()` helper resolves bare vnum → WNUM via `find_area_by_vnum()`
- New `script_match_npc_vnum()` uses `resolve_legacy_vnum()` + `wnum_match_mob()` for `ARG_NUM` comparisons
- String arguments use `parse_widevnum()` then check both `pArea` and `vnum`
- Applied to: `ifc_isfighting`, `ifc_ison`, `ifc_pulling`, `ifc_hasrider`, `ifc_ismount`
- Files: script_ifc.c

---

## Category 3: Struct Fields Storing Bare Vnums

These are `long` fields in structs that store a vnum without area context. They need migration to `WNUM_LOAD` (for persistence) or `WNUM` (for runtime) to be fully widevnum-safe.

### Tier 1: Critical (Cross-area scenarios are common) - COMPLETE

| Struct | Field | File | Status |
|--------|-------|------|--------|
| `MAIL_DATA` | `from_location_load`/`from_location_wnum` | merc.h | DONE - fixed data loss bug (area_uid was parsed but discarded) |
| `MAIL_DATA` | `to_location_load`/`to_location_wnum` | merc.h | DONE - fixed data loss bug |
| `AREA_DATA` | `post_office_load`/`post_office_wnum` | merc.h | DONE - boot-time resolution in fix_area_fields() |
| `AREA_DATA` | `airship_land_load`/`airship_land_wnum` | merc.h | DONE - boot-time resolution in fix_area_fields() |
| `CHAR_DATA` | `boat_logoff_load`/`boat_logoff_wnum` | merc.h | DONE - save/load updated |
| `GQ_MOB_DATA` | `vnum_load`/`vnum_wnum` | merc.h | DONE - migrated |
| `GQ_MOB_DATA` | `obj_load`/`obj_wnum` | merc.h | DONE - migrated |
| `GQ_OBJ_DATA` | `vnum_load`/`vnum_wnum` | merc.h | DONE - migrated |
| `QUEST_DATA` | `questgiver_load`/`questgiver_wnum` | merc.h | DONE - migrated |
| `QUEST_DATA` | `questreceiver_load`/`questreceiver_wnum` | merc.h | DONE - migrated |
| `QUEST_PART_DATA` | all 5 vnum fields | merc.h | DONE - migrated to WNUM_LOAD + WNUM pairs |

### Tier 2: High Priority (Cross-area possible) - COMPLETE

| Struct | Field | File | Status |
|--------|-------|------|--------|
| `MOB_INDEX_DATA` | `corpse_load`/`corpse_wnum` | merc.h | DONE - boot-time resolution in fix_area_fields() |
| `MOB_INDEX_DATA` | `zombie_load`/`zombie_wnum` | merc.h | DONE - boot-time resolution in fix_area_fields() |
| `CHAR_DATA` | `corpse_load`/`corpse_wnum` | merc.h | DONE - runtime tracking updated |
| `OBJ_DATA` | `created_script_load`/`created_script_wnum` | merc.h | DONE - save/load updated |
| `OBJ_DATA` | `orig_wnum` | merc.h | DONE - runtime WNUM (no WNUM_LOAD needed), set at mob death with area context |
| Corpse `value[5]`/`value[6]` | `CORPSE_MOBILE`/`CORPSE_MOBILE_AUID` | merc.h | DONE - added value[6] for area UID, magic_death.c updated |
| `EXIT_DATA` | `u1.vnum` | merc.h | N/A - loading-time only, resolved by fix_rooms() |
| `LOCK_STATE` | `key_load`/`key_wnum` | merc.h | DONE - migrated |

### Tier 3: Medium Priority - COMPLETE

| Struct | Field | File | Status |
|--------|-------|------|--------|
| `TRADE_ITEM` | `obj_load`/`obj_wnum` | merc.h | DONE - fixed data loss bug, boot-time resolution in fix_area_fields() |
| `newbie_eq_type` | `vnum` | merc.h | DEFERRED - uses reserved entity names in db.c; bare vnum field unused for lookups |
| `weapon_type` | `vnum` | merc.h | DEFERRED - uses reserved entity names in db.c; bare vnum field unused for lookups |
| `map_exit_type` | `room_vnum` | merc.h | SKIPPED - dead code (table entirely commented out in const.c) |
| `tunneler_place_type` | `vnum` | merc.h | FIXED - tunneler bug: get_room_index() called with 1 arg → get_room_index_global() |

### Tier 4: Low Priority (Already area-local or legacy unions)

Fields already using the `union { WNUM wnum; WNUM_LOAD load; long vnum; }` pattern are partially migrated. The `long vnum` union member provides backward compatibility during loading.

---

## Category 4: Hardcoded Vnum Constants - COMPLETE

**Status:** DONE (February 9, 2026).

All `#define OBJ_VNUM_*`, `MOB_VNUM_*`, `ROOM_VNUM_*` constants have been removed from merc.h. All runtime lookups now use the reserved entity system via `get_reserved_obj_index("name")`, `get_reserved_mob_index("name")`, `get_reserved_room_index("name")`.

**What Changed:**
- All hardcoded vnum `#define` constants removed from merc.h
- All comparison sites migrated to `get_reserved_*_index()` lookups (227 call sites across 50 files)
- Relic system uses reserved entity lookups at boot time
- Bootstrap `bootstrap_reserved.c` updated with all reserved entity registrations
- Newbie equipment, boat harbours, abyss portal, glass hammer, cursed orb, gold whistle, etc. all use reserved entities
- Magic number checks (e.g., `100035`, `152533`) replaced with named reserved entities

---

## Category 5: Display Consistency - COMPLETE

**Status:** DONE (February 9, 2026). ~51 display sites updated across 13 files.

### Updated Files

**High traffic (immortal commands):**
- `act_wiz.c` (15 sites): do_ostat (4), do_mstat clone (1), do_restore (1), do_token (8), do_church (1)
- `act_info.c` (5 sites): do_look room header (2), do_exits (3)
- `olc.c` (12 sites): olc_ed_vnum (9), rlist (1), mlist (1), do_dislink (1)

**Medium traffic (OLC editors):**
- `editors/rooms/redit.c` (5 sites): mreset/oreset confirmation messages
- `editors/mobiles/medit.c` (6 sites): shop stock display - replaced manual cross-area formatting with `widevnum_string_wnum()`
- `editors/areas/aedit.c` (1 site): post office setting
- `editors/dungeons/dngedit.c` (2 sites): entry/exit room display
- `editors/blueprints/bpedit.c` (4 sites): special rooms, entries/exits, variables
- `editors/blueprints/bsedit.c` (1 site): recall room

**Low traffic:**
- `boat.c` (2 sites): boat_get_name_room display
- `church.c` (1 site): church room list
- `handler.c` (1 site): visit debug output
- `comm.c` (1 site): %R prompt variable (immortal room display)

### Not Changed (by design)

- Save/load file format writes (save.c, olc_save.c, db.c, db2.c) - file format compat
- Internal logging (log_string, bug, pbugf) - low visibility, separate pass if desired
- Script error messages (script_comp.c) - low visibility
- Area min/max vnum ranges (do_astat) - legacy zone metadata, not entity vnums

### Display Function Selection Guide

| Entity Type | Function to Use |
|-------------|----------------|
| Room | `widevnum_string_room(room, pRefArea)` |
| Mobile index | `widevnum_string_mobile(mob_index, pRefArea)` |
| Object index | `widevnum_string_object(obj_index, pRefArea)` |
| Token index | `widevnum_string_token(token, pRefArea)` |
| Script | `widevnum_string_script(script, pRefArea)` |
| Ship index | `widevnum_string_ship(ship, pRefArea)` |
| Blueprint | `widevnum_string_blueprint(bp, pRefArea)` |
| Blueprint section | `widevnum_string_blueprint_section(bs, pRefArea)` |
| Dungeon index | `widevnum_string_dungeon(dng, pRefArea)` |
| WNUM struct | `widevnum_string_wnum(wnum, pRefArea)` |
| Raw area+vnum | `widevnum_string(pArea, vnum, pRefArea)` |

**`pRefArea` usage:**
- `NULL` = always show absolute format (`923#1234`)
- `ch->in_room->area` = show relative `#1234` for same-area, absolute for cross-area
- In OLC: use the area being edited as pRefArea

---

## Category 6: Function Signatures - COMPLETE

**Status:** DONE (February 9, 2026). All functions reviewed - no changes needed.

### Global Lookup Functions (Keep as convenience wrappers)

These intentionally take bare vnums and iterate all areas. Used by `parse_widevnum()` for legacy resolution:

- `get_mob_index_global(long vnum)` - Searches all areas
- `get_obj_index_global(long vnum)` - Searches all areas
- `get_room_index_global(long vnum)` - Searches all areas
- `get_script_index_global(long vnum, int type)` - Searches all areas
- `find_area_by_vnum(long vnum, AREA_DATA *context)` - Finds area containing vnum

### Functions Reviewed

| Function | File | Verdict | Reasoning |
|----------|------|---------|-----------|
| `find_path` | hunt.c | SAFE | Already resolves areas internally via `find_area_by_vnum()`; all callers have room pointers with area context. WNUM variant deferred as optimization-only. |
| `get_char_world_vnum` | merc.h | DEAD CODE | Declaration only, no implementation or call sites. Can be removed in cleanup. |
| `create_invasion_quest` | invasion.c | SAFE | Already receives `AREA_DATA *pArea` parameter; vnums are contextual to that area. |
| `generate_quest_scroll` | quest.c | SAFE | Already resolves area internally via `find_area_by_vnum()`. |
| `get_npc_ship_index` | db.c | DISABLED | Entire function is `#if 0` (returns NULL). Non-functional stub. |

### Token Lookup Functions - FIXED (Phase 9A)

- `get_token_list`, `get_token_char`, `get_token_obj`, `get_token_room`
- **Original assessment was wrong:** Tokens CAN be cross-area (given to entities from other areas). If two areas share the same token vnum, bare vnum comparison could return the wrong token.
- **Fix:** Added `AREA_DATA *area` parameter to all 4 functions. NULL = match any area (backward compatible). Non-NULL = filter by token's source area.
- **Wizard commands** (do_token give/junk) now pass `token_wnum.pArea` from parsed widevnum input.
- **Script callers** pass NULL for now (scripts use bare vnums; will gain widevnum support later).

---

## Category 7: Subsystem-Specific Issues - COMPLETE

**Status:** DONE (February 9, 2026). All subsystems reviewed - already widevnum-safe.

### 7A: Blueprint/Instance System - ALREADY DONE

Union structure (`WNUM wnum` / `WNUM_LOAD load` / `long vnum`) is in place for all blueprint reference fields. JSON loading populates `.load` members; boot-time resolution fills `.wnum` pointers. Legacy `.are` path uses bare `.vnum` as fallback only - can be removed when all legacy zones are converted.

Fields: `room_ref`, `entry_ref`, `exit_ref`, `ship_object_ref`, `blueprint_ref`

### 7B: Ship/Boat System - ALREADY DONE

All hardcoded `ROOM_VNUM_*` constants were removed in Category 4 (reserved entity migration). Boat system now uses `get_reserved_room_index("room_plith_harbour")` etc.

### 7C: Portal System - ALREADY DONE

Portal destinations already store both vnum and area context:
- `value[3]` = destination room vnum
- `value[4]` = destination area uid (set at `act_enter.c:461`)
- Resolution: `get_area_index(portal->value[4])` then `get_room_index(dest_area, portal->value[3])`

Cross-area portal support is complete.

---

## Recommended Implementation Order

### Completed Work (as of February 9, 2026)

- Phase 7A/7B: Command display and input updates (Categories 1A-1E) - MOSTLY DONE
- Phase 7C: Cross-area comparison fixes (Category 2) - ALL DONE (locks, GQ, quests, scripts, obj dupe)
- Phase 7D: Struct field migration (Category 3) - ALL DONE (locks, GQ, quest, mail, area, mob, char, obj, trade)
- Phase 7E: Hardcoded constant migration (Category 4) - DONE (all `#define` constants removed, reserved entity system used everywhere)
- Phase 7F: Display consistency (Category 5) - DONE (~51 sites across 13 files)
- Phase 7G: Function signatures (Category 6) - DONE (all reviewed, already safe or dead code)
- Phase 7H: Subsystem cleanup (Category 7) - DONE (blueprints, boats, portals all already widevnum-safe)

**Category 3 details:** 10 fields migrated to WNUM_LOAD + WNUM pairs across merc.h, json_mail.c, json_area.c, json_persist.c, save.c, fight.c, magic_death.c, boat.c, act_obj.c, act_info.c, act_wiz.c, aedit.c, medit.c, mail.c, mem.c, script_expand.c, scripts.c, olc_save.c. Boot-time resolution via new `fix_area_fields()` in db.c. Fixed 3 data loss bugs (mail from/to location, trade item obj_vnum) and 1 tunneler bug.

**Category 5 details:** Updated all user-facing vnum display sites to use `widevnum_string_*()` functions. High-traffic files (act_wiz.c, act_info.c, olc.c), all OLC editors (redit, medit, aedit, dngedit, bpedit, bsedit), and low-priority files (boat.c, church.c, handler.c, comm.c). Internal logging and save/load file format writes intentionally left as raw `%ld`.

**Category 6 details:** All flagged functions reviewed - `create_invasion_quest` already receives area parameter, `generate_quest_scroll` and `find_path` handle cross-area internally, `get_char_world_vnum` is dead code, `get_npc_ship_index` is disabled. Token lookups were initially assessed as safe but later corrected in Phase 9A.

**Category 7 details:** Blueprint unions already have WNUM_LOAD members populated during JSON loading. Boat system uses reserved entities (no hardcoded vnums). Portal system already stores both destination vnum (value[3]) and area uid (value[4]).

### Phase 8: Scripting System - COMPLETE

Widevnum support is now first-class in the scripting system (February 9, 2026).

#### 8A: Trigger Phrase Widevnum Support - DONE

**Problem:** Trigger phrases like `give_prog ... 923#1234~` were broken because `is_number("923#1234")` returned false, causing widevnum triggers to be treated as name matches and fail silently.

**Solution:**
- Added `trig_is_widevnum`, `trig_load` (WNUM_LOAD), and `trig_wnum` (WNUM) fields to PROG_LIST struct
- Added `is_widevnum_format()` detection function in handler.c
- Updated trigger phrase parsing in all 11 files: json_area.c, olc_save.c, dungeon.c, blueprint.c, medit.c, oedit.c, redit.c, aedit.c, bpedit.c, tedit.c, dngedit.c
- Boot-time resolution: all 7 `fix_*progs()` functions in db.c now call `resolve_wnum_load()` for widevnum trigger phrases
- Runtime matching: `trigger_match_vnum()` helper does area+vnum match for widevnum triggers, bare vnum match for legacy triggers (backward compatible)
- `test_vnumname_trigger()` updated with `AREA_DATA *entity_area` parameter
- Wildcard triggers (trig_number=0) skip widevnum triggers (they're always specific)

#### 8B: Script Expansion Output - DONE

**Problem:** Script variable stringification produced bare vnums (`%d`), not widevnum format.

**Sites fixed (script_expand.c):**
- VAR_ROOM: now uses `widevnum_string_room(room, NULL)` instead of `sprintf(buf, "%d", room->vnum)`
- VAR_TOKEN: now uses `widevnum_string_token(token->pIndexData, NULL)`
- ESCAPE_LI (%i): room-prog and token-prog self-identity now produces widevnum format
- ENT_NUMBER: left as-is (generic number, not always a vnum)

#### 8C: Script _global() → Area-Aware Lookups - DONE

**Problem:** Script commands used `get_*_index_global()` for entity lookups, iterating all areas instead of using the script's own area context.

**Solution:**
- Added 5 area-aware helper functions in scripts.c: `get_script_from_info()`, `get_mob_index_from_info()`, `get_obj_index_from_info()`, `get_room_index_from_info()`, `get_token_index_from_info()`
- Each tries `get_*_index(script_area, vnum)` first, then falls back to `get_*_index_global(vnum)`
- Area context from `get_area_from_scriptinfo(info)` (script's home area)

**Files updated (~55 call sites):**
- script_mpcmds.c: 16 replacements (mp_getolocation, do_mpcall, do_mplink, do_mpinput, do_mpcloneroom, do_mpdestroyroom, do_mpxcall, do_mpscriptwait)
- script_opcmds.c: 9 replacements (do_opcall, do_opinput, do_opxcall, do_opscriptwait)
- script_rpcmds.c: 3 replacements (do_rpcall, do_rpinput, do_rpxcall)
- script_tpcmds.c: 9 replacements (do_tpadjust, do_tpcall, do_tpgive, do_tpinput, do_tpxcall, do_tpscriptwait)
- script_commands.c: 7 replacements (scriptcmd_call, scriptcmd_grantskill, scriptcmd_inputstring, scriptcmd_questcancel, scriptcmd_queststart, scriptcmd_revokeskill, scriptcmd_xcall)
- script_ifc.c: 10 replacements (ifc_mobexists, ifc_tokencount, ifc_tokenexists, ifc_mobclones, ifc_objclones, ifc_loaded)
- scripts.c: 2 replacements (script expansion vnum lookups)
- db.c: 7 fix_*progs() functions now use area-first then global fallback for script resolution

**Not changed (correct):** Player commands (`do_*stat`, `do_*dump`) that don't have script context, login trigger (system constant), and the _from_info helper fallback calls themselves.

#### 8D: ifcheck Area Context - DONE

**Problem:** `mobhere` and `objhere` ifchecks passed NULL for area context on numeric and string arguments.

**Fix (script_ifc.c):** Both now use `get_area_from_scriptinfo(info)` for area context in `get_mob_vnum_room()`/`get_obj_vnum_room()` calls and `parse_widevnum()` calls.

### Phase 9A: Token System Widevnum Support - COMPLETE

**Problem:** `get_token_char()`, `get_token_obj()`, `get_token_room()`, `get_token_list()` compared bare vnums without area context. If tokens from different areas share the same vnum, the wrong token could be returned.

**Solution:**
- Added `AREA_DATA *area` parameter to all 4 functions in handler.c
- Updated declarations in merc.h
- Updated all 39 call sites across 3 files:
  - script_tpcmds.c (12 calls): pass `NULL` (backward compatible, scripts use bare vnums)
  - script_ifc.c (17 calls): pass `NULL` (backward compatible, ifchecks use bare vnums)
  - act_wiz.c (10 calls): old code passes `NULL`, new do_token give/junk passes `token_wnum.pArea`

**Matching logic:** `(!area || token->pIndexData->area == area)` - NULL means match any, non-NULL means filter by area.

### Phase 9B: Polish (Deferred)

Minor items for future cleanup:
- Add WNUM variant for `find_path()` as performance optimization (not correctness)
- Remove legacy `.are` loading fallback from blueprint unions when all zones are JSON
- Internal log messages: optional pass to use `widevnum_string_*()` in `log_string`/`bug` calls
- Documentation and builder guide for widevnum format
- Test suite expansion for cross-area scenarios
- Performance validation

---

## Metrics

| Category | Items | Status |
|----------|-------|--------|
| Command display/input (Cat 1) | ~70 changes | DONE - all commands verified |
| Cross-area comparison fixes (Cat 2) | ~20 comparisons | DONE - all 5 subcategories complete |
| Struct field migrations (Cat 3) | ~15 fields | DONE - all migrated (3 data loss bugs fixed) |
| Hardcoded constant migration (Cat 4) | ~15 constants | DONE - all migrated to reserved entities |
| Display consistency (Cat 5) | ~51 sites | DONE - 13 files updated |
| Function signature updates (Cat 6) | ~10 functions | DONE - all reviewed, already safe or dead code |
| Subsystem cleanup (Cat 7) | ~5 items | DONE - blueprints, boats, portals all already widevnum-safe |
| Scripting system (Cat 8) | ~80 changes | DONE - triggers, expansion, _global(), ifchecks |
| Token system (Cat 9A) | ~43 changes | DONE - area-aware token lookup functions |

**Phases 7-9A COMPLETE.** Remaining items are Phase 9B polish (performance optimizations, internal log formatting, documentation).

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

## Migration Pattern Reference

The lock/key system was the first complete migration and established the pattern used for all subsequent work:

```
WNUM_LOAD field_load;   // Persistent: area_uid + vnum (saved to JSON/file)
WNUM field_wnum;        // Runtime: area pointer + vnum (resolved at boot)
```

**Boot resolution:** `resolve_wnum_load(&load, &wnum, fallback_area)` converts persistent → runtime.

**Comparison:** Use `wnum_match_mob()`, `wnum_match_obj()`, `wnum_match_room()`, or raw `wnum_match()`.

**Backward compat:** JSON readers check for both `key_auid` + `key_vnum` (new) and bare `key_vnum` (legacy). When `auid` is 0, resolution falls back to the owning area.

This pattern was applied consistently to: LOCK_STATE, GQ_MOB_DATA, GQ_OBJ_DATA, QUEST_DATA, QUEST_PART_DATA, MAIL_DATA, AREA_DATA, MOB_INDEX_DATA, CHAR_DATA, OBJ_DATA, TRADE_ITEM.

**Boot-time resolution functions:**
- `fix_rooms()` - resolves exit WNUM_LOAD fields
- `fix_object_locks()` - resolves object lock key WNUM_LOAD fields
- `fix_area_fields()` - resolves area-level (post_office, airship_land), mob index (corpse, zombie), and trade item WNUM_LOAD fields
