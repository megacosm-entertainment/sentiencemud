# Plan: Migrate Event System from Global UIDs to Area-Scoped Widevnums

## Context

The event system (dynamic events / FATEs) currently stores all event definitions globally in `data/system/events.json` with auto-incremented UIDs. Other similar systems (dungeons, blueprints, ships, quests) have already been migrated to area-scoped storage using widevnums. This migration aligns events with the established pattern: each event definition lives inside an area, identified by `area + vnum` (WNUM), and is persisted as part of the area's JSON file. Global events are simply events stored in whichever area the builder chooses (e.g., a system area).

This is a fresh start - no data migration from the existing `events.json` is needed.

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
   - Move `EVENT_INDEX_DATA` struct from `evtedit.c` to `merc.h` (it needs to be visible for the area hash table and cross-file references)
   - Replace `long uid` with `AREA_DATA *area` + `long vnum` in `EVENT_INDEX_DATA`
   - Change `event_source_uid` on `CHAR_DATA` and `OBJ_DATA` from `long` to `WNUM event_source`
   - Move `EVENT_INSTANCE`, `EVENT_PART`, `EVT_ROSTER_ENTRY`, related enums/constants from `evtedit.c` to `merc.h`

2. **`editors/events/evtedit.c`** - Editor overhaul
   - Remove standalone JSON load/save (`evtedit_load_from_json`, `evtedit_save_to_json`, `EVTEDIT_JSON_FILE`)
   - Remove global linked list (`evtedit_list_head/tail`, `evtedit_next_uid`)
   - Remove `evtedit_ensure_loaded()` bootstrap
   - Move `event_system_enabled` toggle to game settings system (`io/json/json_game_settings.c`)
   - Rewrite `evtedit_create` to follow qedit pattern: parse wnum, validate area, hash into area table
   - Rewrite `evtedit_find_*` lookups to use area hash tables
   - Add `evtedit_get_area()` function returning `evt->area`
   - Change editor def: `change_mode = OLC_CHANGE_AREA_FLAG`, add `get_area_fn = evtedit_get_area`
   - Add `evtedit_parse_index_ref()` (like qedit's) for flexible wnum input
   - Add `evtedit_next_vnum_in_area()` for auto-vnum
   - Update `evtedit_list` to show area-scoped listing
   - Convert roster entry vnums to widevnums (mob/obj references)
   - Convert `reward_success_script` / `reward_failure_script` to widevnums
   - Remove `scope_area_uid` field (the owning area IS the scope anchor by default; scope can reference other areas via widevnum if needed)

3. **`io/json/json_area.c`** - Area persistence
   - Add `json_area_serialize_event()` function (serialize EVENT_INDEX_DATA)
   - Add `json_area_deserialize_event()` function
   - Add event serialization loop in area save (iterate `area->event_index_hash`)
   - Add event deserialization in area load (parse `"events"` array, hash into area)
   - Use widevnum strings for all cross-references (roster vnums, reward scripts, scope references)

4. **`event_types.h`** - Public API updates
   - Change `event_tag_mobile_spawn()` / `event_tag_object_spawn()` signatures from `long event_uid` to WNUM-based identification
   - Update getter signatures similarly
   - Add `get_event_index()` and `get_event_index_for_area()` lookup functions
   - Add `event_index_register()` function (like `quest_index_v2_register`)
   - Add `widevnum_string_event()` convenience function

5. **`event_types.c`** - Implementation updates
   - Implement the new lookup and registration functions
   - Update tagging functions to use WNUM-based event references

### Integration Point Updates

6. **`fight.c`** - Kill progress recording
   - `event_progress_record_kill()` - no signature change needed (it matches by instance, not by UID)
   - `event_progress_complete_invasion_leader()` - same

7. **`act_obj2.c`** - Collection turnin
   - `event_progress_record_collection_turnin()` - no signature change needed

8. **`handler.c`** - Widevnum utilities
   - Add `widevnum_string_event()` convenience function

9. **`Makefile` / `CMakeLists.txt`** - If `event_types.c` is new or files are added/removed

### Runtime System Changes

10. **Event instances** (`EVENT_INSTANCE`)
    - Replace `scope_area_uid` with `AREA_DATA *scope_area` (runtime pointer)
    - Instance tracking remains global (active events list) - this is correct since instances are transient runtime state
    - Instance IDs remain as-is (auto-incremented uint32_t per session)

11. **Entity tagging on CHAR_DATA / OBJ_DATA**
    - Change `long event_source_uid` to `WNUM event_source` (area pointer + vnum) to identify which event definition spawned it
    - Keep `uint32_t event_source_instance_id` - identifies which specific instance spawned it
    - Keep `int16_t event_source_bracket` as-is
    - The combination of WNUM + instance_id allows per-instance isolation: an event instance can be flagged so that entities spawned by one run cannot be used/counted in a different run of the same event definition
    - Add `EVT_FLAG_INSTANCE_EXCLUSIVE` flag to EVENT_INDEX_DATA to opt into this behavior

## Scope Simplification

Currently events have a `scope_type` (global/area/region/zones/battlefield) and a separate `scope_area_uid` anchor. With events now owned by areas:

- The owning area provides the default scope anchor
- `scope_type` remains (an event can still be global, area-scoped, etc.)
- `scope_area_uid` becomes unnecessary for area-scoped events (the owner area is the scope)
- For events that need to affect a DIFFERENT area than the one they're defined in, add a `WNUM scope_override` field
- Global events simply set `scope_type = EVT_SCOPE_GLOBAL` - no anchor needed

## `do_event` vs `do_evtedit` - Two Separate Commands

These are two distinct commands:

- **`do_evtedit`** - OLC editor for event *definitions* (index data). This is where the major structural changes happen: wnum-based create/find/list, area hash tables, etc.
- **`do_event`** - Runtime command for event *instances* (start, stop, join, leave, status, schedule, tick). This command works with active instances and needs lighter changes:
  - `event list` - iterate all areas' event hash tables instead of global linked list
  - `event info <wnum|name>` - look up definition by wnum instead of uid
  - `event start <wnum|name>` - same
  - `event join/leave/status` - work on active instances, minimal change needed
  - `event enable/disable` - system-wide toggle; no longer saves to `events.json`, needs alternative persistence (game settings)

## Implementation Order

1. Move structs to `merc.h` (EVENT_INDEX_DATA, EVENT_INSTANCE, EVENT_PART, EVT_ROSTER_ENTRY, enums)
2. Add `event_index_hash[MAX_KEY_HASH]` to `AREA_DATA`
3. Change `EVENT_INDEX_DATA`: replace `uid` with `area` + `vnum`
4. Change entity tagging fields on `CHAR_DATA` / `OBJ_DATA` to WNUM
5. Add lookup/registration functions in `event_types.c`
6. Update `event_types.h` public API
7. Add JSON serialization/deserialization in `json_area.c`
8. Rewrite `evtedit.c` editor (create, list, find, show, save flows)
9. Update `do_event` runtime command
10. Update runtime system (instances, progress, tagging, scope checking)
11. Update integration points (`fight.c`, `act_obj2.c`)
12. Add `widevnum_string_event()` to `handler.c`
13. Update build files if needed
14. Build and test

## Verification

1. `./build` - clean compilation with no warnings
2. `./build tests && cd /sentience && ./sent -test` - all tests pass
3. Manual in-game testing:
   - `evtedit create` - auto-vnums in current area
   - `evtedit 1#1` - open event by wnum
   - `evtedit list` - shows events grouped by area
   - `event start <wnum>` - starts an instance
   - `event join` / `event status` - participation works
   - Verify spawned mobs have correct event source WNUM tagging
   - Verify kill/collection progress tracking works
   - Verify instance-exclusive flag prevents cross-instance entity reuse
