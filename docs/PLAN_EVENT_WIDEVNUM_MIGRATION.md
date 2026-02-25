# Plan: Migrate Event System from Global UIDs to Area-Scoped Widevnums

## Context

The event system (dynamic events / FATEs) currently stores all event definitions globally in `data/system/events.json` with auto-incremented UIDs. Other similar systems (dungeons, blueprints, ships, quests) have already been migrated to area-scoped storage using widevnums. This migration aligns events with the established pattern: each event definition lives inside an area, identified by `area + vnum` (WNUM), and is persisted as part of the area's JSON file. Global events are simply events stored in whichever area the builder chooses (e.g., a system area).

This is a fresh start - no data migration from the existing `events.json` is needed.

In the same migration, we introduce two script spaces:
- **`eprogs`** (event progs): scripts attached to an event definition, where `$(self)` is the event.
- **`sprogs`** (system progs): global/system scripts, with optional area/zone placement for locality.

## Reference Pattern: qedit / quest system

The quest system (`editors/quests/qedit.c`, `quest.c`, `io/json/json_area.c`) is the cleanest model:
- `QUEST_INDEX_V2_DATA` has `AREA_DATA *area` + `long vnum`
- Hashed into `area->quest_index_v2_hash[MAX_KEY_HASH]`
- Created via `qedit create [wnum]` with auto-vnum or explicit wnum
- Registered via `quest_index_v2_register()` (hashes into area table)
- Serialized/deserialized as part of area JSON in `json_area.c`
- Editor uses `OLC_CHANGE_AREA_FLAG` change mode with `get_area_fn`

Key qedit functions to mirror:
- `qedit_parse_index_ref()` - flexible wnum input parsing (`<auid>#<vnum>`, `#<vnum>`, bare `<vnum>`)
- `qedit_next_vnum_in_area()` - auto-vnum allocation
- `qedit_get_area()` - returns owning area from edit pointer
- `quest_index_v2_register()` - validates and hashes into area table

## Files to Modify

### Core Changes

1. **`merc.h`** - Struct and type changes
   - Add `EVENT_INDEX_DATA *event_index_hash[MAX_KEY_HASH]` to `AREA_DATA`
   - Move `EVENT_INDEX_DATA` struct from `evtedit.c` to `merc.h` (needed for area hash table and cross-file references)
   - Replace `long uid` with `AREA_DATA *area` + `long vnum` in `EVENT_INDEX_DATA`
   - Change `event_source_uid` on `CHAR_DATA` and `OBJ_DATA` from `long` to `WNUM event_source`
   - Move `EVENT_INSTANCE`, `EVENT_PART`, `EVT_ROSTER_ENTRY`, related enums/constants from `evtedit.c` to `merc.h`

2. **`editors/events/evtedit.c`** - Editor overhaul
   - Remove standalone JSON load/save (`evtedit_load_from_json`, `evtedit_save_to_json`, `EVTEDIT_JSON_FILE`)
   - Remove global linked list (`evtedit_list_head/tail`, `evtedit_next_uid`)
   - Remove `evtedit_ensure_loaded()` bootstrap
   - Move `event_system_enabled` toggle persistence to game settings (`io/json/json_game_settings.c`)
   - Rewrite `evtedit_create` to follow qedit pattern: parse wnum, validate area, hash into area table
   - Rewrite `evtedit_find_*` lookups to use area hash tables
   - Add `evtedit_get_area()` returning `evt->area`
   - Change editor def: `change_mode = OLC_CHANGE_AREA_FLAG`, add `get_area_fn = evtedit_get_area`
   - Add `evtedit_parse_index_ref()` (like qedit) for flexible wnum input
   - Add `evtedit_next_vnum_in_area()` for auto-vnum
   - Update `evtedit_list` to show area-scoped listing
   - Convert roster entry refs to widevnums (mob/obj references)
   - Convert `reward_success_script` / `reward_failure_script` to widevnums
   - Remove `scope_area_uid` field (owning area is default scope anchor)

3. **`io/json/json_area.c`** - Area persistence
   - Add `json_area_serialize_event()`
   - Add `json_area_deserialize_event()`
   - Add event serialization loop in area save (iterate `area->event_index_hash`)
   - Add event deserialization in area load (parse `"events"` array, hash into area)
   - Use widevnum strings for cross-references (roster refs, reward scripts, scope references)

4. **`event_types.h`** - Public API updates
   - Change `event_tag_mobile_spawn()` / `event_tag_object_spawn()` signatures from `long event_uid` to WNUM identification
   - Update getter signatures similarly
   - Add `get_event_index()` and `get_event_index_for_area()` lookup functions
   - Add `event_index_register()` function (like `quest_index_v2_register`)
   - Add `widevnum_string_event()` convenience function

5. **`event_types.c`** - Implementation updates
   - Implement new lookup and registration functions
   - Update tagging functions to use WNUM-based event references

6. **`scripts.h` / `script_const.c` / `script_expand.c` / `scripts.c`** - Script integration
   - Expose event definition as WNUM in script entities
   - Keep compatibility aliases where useful (`event_uid`, `source_uid`) while moving semantics to WNUM + instance
   - Ensure event context resolves `$(self)` properly for eprogs

7. **System prog namespace (`sprogs`)**
   - Add reserved/prog-type support for system scripts
   - Define persistence policy:
     - game-settings JSON for truly global sprogs, or
     - area/zone JSON arrays for local ownership
   - Add runtime dispatch points for system lifecycle hooks

### Integration Point Updates

8. **`fight.c`** - Kill progress recording
   - `event_progress_record_kill()` (instance-based; no external signature shift required)
   - `event_progress_complete_invasion_leader()` (same)

9. **`act_obj2.c`** - Collection turnin
   - `event_progress_record_collection_turnin()` (instance-based)

10. **`handler.c`** - Widevnum utilities
   - Add `widevnum_string_event()` convenience function

11. **`Makefile` / `CMakeLists.txt`**
   - Update only if files are added/removed

## Runtime System Changes

12. **Event instances** (`EVENT_INSTANCE`)
    - Replace `scope_area_uid` with `AREA_DATA *scope_area` (runtime pointer)
    - Keep global active-instance list (instances are transient runtime objects)
    - Keep session-scoped auto-incremented `instance_id`

13. **Entity tagging on `CHAR_DATA` / `OBJ_DATA`**
    - Use `WNUM event_source` (definition identity)
    - Keep `uint32_t event_source_instance_id`
    - Keep `int16_t event_source_bracket`
    - WNUM + instance_id enables per-instance isolation
    - Add `EVT_FLAG_INSTANCE_EXCLUSIVE` to enforce strict matching when desired

14. **Event progs (`eprogs`) lifecycle contract**
    - Hook points:
      - `on_start`
      - `on_phase_change`
      - `on_tick`
      - `on_complete`
      - `on_fail`
      - `on_stop`
    - `$(self)` is the event definition; runtime context exposes active instance data
    - Scope modes: `event_scope` and `global`
    - Missing script refs log warnings; runtime continues safely

15. **System progs (`sprogs`) runtime contract**
    - Global/system hooks independent of one event definition
    - Optional area/zone placement for locality and ownership
    - Same fail-safe behavior: never hard-fail core runtime tick

## Scope Simplification

Current model has `scope_type` + `scope_area_uid`. With area-owned events:

- Owning area is the default scope anchor
- `scope_type` remains (global/area/region/zones/battlefield)
- Explicit override is optional for cross-area behavior (`WNUM scope_override`)
- Global events use `EVT_SCOPE_GLOBAL`

## `do_event` vs `do_evtedit`

Two distinct commands:

- **`do_evtedit`** - OLC for event definitions
  - wnum-based create/find/list/edit
  - area-hash registration
  - `eprogs` CRUD (`eprog list/add/set/remove/clear`)

- **`do_event`** - Runtime command for active instances
  - `event list/info/start/stop` resolve definitions by wnum/name
  - `event join/leave/status` operate on active instances
  - system enable/disable stored in game settings (no `events.json`)

`**sprogs**` should use a separate editor/command surface (`spedit` or equivalent), not `evtedit`.

## Implementation Order

1. Move event structs/enums to `merc.h` (EVENT_INDEX_DATA, EVENT_INSTANCE, EVENT_PART, EVT_ROSTER_ENTRY)
2. Add `event_index_hash[MAX_KEY_HASH]` to `AREA_DATA`
3. Replace event definition UID with `area + vnum`
4. Convert `CHAR_DATA` / `OBJ_DATA` event source fields to WNUM
5. Add event lookup/registration in `event_types.c`
6. Update `event_types.h` public API
7. Add area JSON event serialization/deserialization in `json_area.c`
8. Rewrite `evtedit.c` (create/list/find/show/save flows)
9. Update `do_event` definition lookups
10. Update runtime scope/tagging checks and instance matching
11. Update integration points (`fight.c`, `act_obj2.c`)
12. Add `widevnum_string_event()` to `handler.c`
13. Add `eprogs` model + OLC + lifecycle dispatch points
14. Add `sprogs` namespace + persistence + dispatch
15. Update build files if needed
16. Build and test

## Verification

1. `./build` - compiles cleanly
2. `./build tests && cd /sentience && ./sent -test` - tests pass
3. Manual in-game checks:
   - `evtedit create` auto-vnums in current area
   - `evtedit 1#1` opens by wnum
   - `evtedit list` grouped/scoped by area
   - `event start <wnum>` starts instance
   - `event join` / `event status` work
   - spawned entities carry correct event-source WNUM tagging
   - kill/collection progress works
   - instance-exclusive behavior blocks cross-instance reuse
   - `eprog` hooks fire at expected lifecycle moments
   - `sprog` hooks fire from system-level dispatch points
