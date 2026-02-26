# Persistence Refactor Worklog

## Overview
Tracking progress on migrating from monolithic `persist.dat` to individual JSON files.

---

## 2026-01-23: Phase 1 Implementation Start

### Files Created

#### `/sentience/src/json_persist.h`
- Header file for JSON persistence module
- Defines directory constants: `PERSIST_JSON_DIR`, `PERSIST_JSON_ROOMS`, `PERSIST_JSON_MOBILES`, `PERSIST_JSON_OBJECTS`
- Declares serialization functions for objects, mobiles, rooms, tokens, affects, locks, locations, exits, script data

#### `/sentience/src/json_persist.c`
- Core implementation of JSON persistence
- **Fully implemented:**
  - `json_persist_init()` - Creates directory structure
  - Object serialization (`json_persist_object_to_json`, `json_persist_json_to_object`)
  - Token serialization (`json_persist_token_to_json`, `json_persist_json_to_token`)
  - Affect serialization (`json_persist_affect_to_json`, `json_persist_json_to_affect`)
  - Lock serialization (`json_persist_lock_to_json`, `json_persist_json_to_lock`)
  - Location serialization (`json_persist_location_to_json`, `json_persist_json_to_location`)
  - Script variable serialization (`json_persist_scriptdata_to_json`, `json_persist_json_to_scriptdata`)
  - Helper functions: `spell_to_json`, `json_to_spell`, `waypoint_to_json`, `json_to_waypoint`, `variable_to_json`, `json_load_variable`
  - Path generation functions for objects, mobiles, rooms

- **Stubbed (not yet implemented):**
  - Mobile serialization (`json_persist_mobile_to_json`, `json_persist_json_to_mobile`)
  - Room serialization (`json_persist_room_to_json`, `json_persist_json_to_room`)
  - Exit serialization (`json_persist_exit_to_json`, `json_persist_json_to_exit`)
  - File I/O (`json_persist_save_object`, `json_persist_load_object`, etc.)
  - Batch operations (`json_persist_save_all`, `json_persist_load_all`)
  - Migration utilities (`json_persist_migrate_from_dat`, `json_persist_needs_migration`)

#### `/sentience/docs/CLAUDE_PERSISTENCE.md`
- Technical documentation of the persistence system
- Documents current `persist.dat` format
- Lists all persisted fields for objects, mobiles, rooms
- Documents existing infrastructure (jansson, Redis)
- Phase 1 target directory structure

### Build System Updates

#### `/sentience/src/Makefile`
- Added dependency paths for `.deps/` libraries (zlog, jansson, libcotp, libquickmail)
- Added include paths: `-I$(ZLOG_DIR)/src -I$(JANSSON_DIR)/src` etc.
- Added library paths: `-L$(ZLOG_DIR)/lib -L$(JANSSON_DIR)/src/.libs` etc.
- Added `-lzlog` to LIBS
- Added `json_persist.c` and `log.c` to C_FILES
- Configured parallel builds to use `nproc/2` cores by default

#### `/sentience/src/CMakeLists.txt`
- Added parallel build configuration using `ProcessorCount`
- Set `CMAKE_BUILD_PARALLEL_LEVEL` to nproc/2

#### `/sentience/src/.deps/build_deps.sh`
- Created script to build all dependencies from source
- Supports: `all`, `zlog`, `jansson`, `libcotp`, `libquickmail`, `clean`, `check`
- Checks if libraries are already built to avoid redundant work

### Compilation Fixes Applied
1. Changed `LOCK_DATA` to `LOCK_STATE` (correct type name)
2. Added `#include "wilds.h"` for `WILDS_DATA` type
3. Renamed `json_array` variable to `vars_array` to avoid shadowing jansson function
4. Added casts for `json_string_value()` returns to avoid const warnings

### Build System Notes
- GNU Makefile and CMake are both supported
- GNU Make: run `make` from `/sentience/src`
- CMake: run from `/sentience/src/.build` (out-of-source build)
  ```bash
  mkdir -p .build && cd .build && cmake .. && make
  ```
- CMake artifacts are confined to `.build/` directory

---

## 2026-01-23: Phase 1 Implementation Continued

### Mobile Serialization - Complete

#### `json_persist_mobile_to_json(CHAR_DATA *ch)`
Serializes all mobile data to JSON:
- Version, vnum, UID, persist flag
- Death state and respawn timer
- Recall/repop location
- Basic info (name, owner, descriptions)
- Race, sex, level, tot_level
- Location (static, wilderness, or clone room)
- Toxins (for Sith/Naga races)
- HMV (hit/mana/move with current and max)
- Currency (gold, silver, pneuma, etc.)
- All flags (act, affected_by, off, imm, res, vuln)
- Positions, physical attributes
- Stats (perm and mod), armor, combat bonuses
- Affects (with custom name support)
- Shop/crew references
- Script variables, tokens
- Carried and worn objects (recursive)

#### `json_persist_json_to_mobile(json_t *json)`
Deserializes mobile from JSON, creating the mob from its index and applying all saved state.

#### `json_persist_save_mobile()` / `json_persist_load_mobile()`
File I/O operations for individual mobile JSON files.

### Exit Serialization - Complete

#### `json_persist_exit_to_json(EXIT_DATA *pexit, int dir)`
Serializes exit data:
- Direction, original door index
- Destination (room location or wilderness coords)
- Descriptions (keyword, short, long)
- Flags (exit_info, rs_flags)
- Door data (lock, reset lock, strength, material)
- Skips wilderness vlink exits (dynamically generated)

#### `json_persist_json_to_exit(json_t *json, ROOM_INDEX_DATA *room)`
Deserializes exit from JSON.

### Room Serialization - Complete

#### `json_persist_room_to_json(ROOM_INDEX_DATA *room)`
Serializes room data:
- Room type (static, wilderness, clone)
- Identification (vnum, wilds_uid, or clone source + IDs)
- Environment for clone rooms
- Coordinates, view wilds reference
- Basic info (name, description, owner)
- Flags, sector type, locale
- Regeneration rates
- Recall location
- All exits
- Script variables, tokens
- Contained objects (recursive)
- NPCs in room (recursive mobile serialization)

#### `json_persist_json_to_room(json_t *json)`
Deserializes room from JSON. Handles static rooms (applies state to existing) and clone rooms (creates new clone). Wilderness rooms noted as not yet supported (dynamically generated).

#### Room environment helper: `room_environ_to_json()`
Handles the complex environment references for clone rooms (room, mobile, object, token environments with both direct and deferred references).

### Batch Operations - Complete

#### `json_persist_save_all()`
- Saves all rooms from `persist_rooms` list first (they contain objects/mobs)
- Saves mobiles from `persist_mobs` list (skips those in persistent rooms)
- Saves objects from `persist_objs` list (skips those in persistent containers)
- Uses `in_persistent_environment()` helper to avoid double-saving nested entities

#### `json_persist_load_all()`
- Scans `data/persist/rooms/`, `data/persist/mobiles/`, `data/persist/objects/`
- Parses JSON files and recreates entities
- Adds loaded entities to persistence lists
- Places mobiles/objects in world

### External Dependencies Added
- `affgroup_mobile_flags` - for mobile affect groups
- `toxin_table` - for toxin serialization
- `exit_flags` - for exit flags
- `room_flags` - for room flags
- `dir_name` - for exit direction names (from merc.h, const)

### Compilation Fixes
1. Changed `create_mobile_noid` → `create_mobile(pMobIndex, true)` (persistLoad flag)
2. Changed `get_race_by_name` → `race_lookup_name`
3. Changed `clone_room` → `create_virtual_room(source, false, false)`
4. Fixed `get_wilds_from_uid` to take two args: `(NULL, wuid)` - NULL searches all areas
5. Removed redundant `extern char *dir_name[]` - already in merc.h as const

### Build Status
✅ Successfully compiles with cmake (tested 2026-01-23)

---

## 2026-01-23: Integration Complete

### Changes to `db.c`

#### Added includes
- `#include <dirent.h>` - for DIR/dirent types
- `#include "json_persist.h"` - JSON persist functions

#### Modified `persist_load()` (line ~8076)
New load sequence:
1. Calls `json_persist_init()` to create directory structure
2. Checks `json_persist_needs_migration()` to detect if persist.dat needs migration
3. If JSON files exist, calls `json_persist_load_all()` and returns
4. Falls back to loading from `persist.dat` (old format)
5. After loading from persist.dat, calls `json_persist_save_all()` to migrate to JSON

#### Modified `persist_save()` (line ~6229)
- After saving to `persist.dat`, also calls `json_persist_save_all()`
- This ensures both formats are kept in sync during transition

### Migration Flow
1. **First boot with persist.dat**: Detects migration needed, loads old format, saves to JSON
2. **Subsequent boots**: Loads directly from JSON files
3. **Ongoing saves**: Writes to both persist.dat and JSON (dual-write for safety)

### Build Status
✅ Successfully compiles with cmake (tested 2026-01-23)

### Testing Results
✅ **Phase 1 Complete** - Migration from persist.dat to JSON working correctly
- persist.dat loaded successfully
- JSON files created in `data/persist/` directories
- Subsequent boots load from JSON
- Dual-write keeps both formats in sync

---

## Phase 2: Async Write & Redis Caching

### Goals
1. **Async Write Operations** - Decouple file I/O from main game loop
2. **Redis Caching Layer** - Fast read-through cache
3. **Write-Back Queue** - Background worker handles disk writes

### Architecture (from PLAN_PERSISTENCE_REFACTOR.md)

#### Write Path (Async)
1. Game logic generates JSON delta when entity changes
2. JSON written to Redis key (e.g., `persist:room:<id>`)
3. Key pushed to `dirty_keys` queue in Redis
4. Background worker pops from queue and writes to disk

#### Read Path (Cached)
1. Check Redis for `persist:<type>:<id>`
2. Cache hit → return immediately
3. Cache miss → read from disk, populate Redis with TTL

---

## 2026-01-23: Phase 2 Implementation Complete

### Redis World State Caching (`redis_cache.c`)

#### Low-level cache operations
- `redis_cache_persist_data(key, json_str)` - SET with TTL (7 days)
- `redis_get_persist_data(key)` - GET cached JSON
- `redis_delete_persist_data(key)` - DEL cache entry

#### Dirty queue operations
- `redis_queue_dirty_key(key)` - LPUSH to `persist:dirty_keys` queue
- `redis_pop_dirty_key(timeout_sec)` - BRPOP from queue (blocking)
- `redis_dirty_queue_size()` - LLEN of queue

#### High-level entity caching
- `redis_cache_room_state(room_id, json_str)` - cache + queue dirty
- `redis_cache_mobile_state(id0, id1, json_str)` - cache + queue dirty
- `redis_cache_object_state(id0, id1, json_str)` - cache + queue dirty
- `redis_get_room_state(room_id)` - retrieve cached room
- `redis_get_mobile_state(id0, id1)` - retrieve cached mobile
- `redis_get_object_state(id0, id1)` - retrieve cached object

### Background Worker Thread (`json_persist.c`)

#### Worker functions
- `persist_worker_func(void *arg)` - Background thread main loop
  - Uses BRPOP with 1-second timeout
  - Parses key format: `persist:room:<id>`, `persist:mobile:<id0>:<id1>`, `persist:object:<id0>:<id1>`
  - Retrieves JSON from Redis cache
  - Writes to disk using existing save functions
  - Handles errors gracefully, continues processing

#### Control functions
- `json_persist_worker_start()` - Spawns background thread
- `json_persist_worker_stop()` - Sets shutdown flag, joins thread
- `json_persist_worker_is_running()` - Check worker status

### Cached Save/Load Functions (`json_persist.c`)

#### Cached saves (async writes via Redis)
- `json_persist_save_object_cached(OBJ_DATA *obj)` - Serialize → cache → queue
- `json_persist_save_mobile_cached(CHAR_DATA *ch)` - Serialize → cache → queue
- `json_persist_save_room_cached(ROOM_INDEX_DATA *room)` - Serialize → cache → queue

#### Cached loads (read-through cache)
- `json_persist_load_object_cached(id0, id1)` - Cache hit returns fast, miss reads disk → caches
- `json_persist_load_mobile_cached(id0, id1)` - Cache hit returns fast, miss reads disk → caches
- `json_persist_load_room_cached(room_id)` - Cache hit returns fast, miss reads disk → caches

### Boot/Shutdown Integration (`comm.c`)

#### Startup sequence
1. `boot_db()` calls `persist_load()` (loads JSON or migrates from persist.dat)
2. `redis_init()` establishes Redis connection
3. `redis_warm_cache(100)` pre-caches active characters
4. `json_persist_worker_start()` spawns background writer (if Redis available)

#### Shutdown sequence
1. `async_cache_shutdown()` - waits for pending async operations
2. `json_persist_worker_stop()` - flushes dirty queue to disk
3. `redis_shutdown()` - closes Redis connection

### Build Status
✅ Successfully compiles with cmake (tested 2026-01-23)

### Next Steps
- Test Phase 2 with Redis enabled
- Consider converting persist_save() calls to use cached versions
- Monitor dirty queue size during gameplay
- Add admin commands for persist stats

---

## Reference

### Object Fields Serialized
- Version, UID (id[0], id[1]), persist flag
- Names: name, short_descr, description, full_description
- Flags: extra_flags[4], wear_flags, item_type, perm_extra[4]
- Stats: weight, condition, level, timer, cost, value[8]
- Location data (room reference or container)
- Lock state
- Waypoints, spells
- Affects (object, skill, mobile)
- Catalysts
- Extra descriptions
- Tokens
- Script variables
- Contained objects (recursive)

### Room Types
- Static: identified by vnum
- Wilderness: identified by wilds_uid + x,y,z coordinates
- Clone: identified by source_vnum + id[0],id[1]

---

## 2026-01-23: Redis & Character Loading Bug Fixes

### Issue 1: Redis Thread Safety

**Problem:** Segfaults occurring when multiple operations tried to use the Redis connection simultaneously. hiredis is not thread-safe.

**Solution:** Added `pthread_mutex_t redis_mutex` to `/sentience/src/redis_cache.c` and wrapped all `redisCommand()` calls with mutex lock/unlock.

**Files Modified:**
- `redis_cache.c` - Added mutex initialization and lock/unlock around all Redis operations

### Issue 2: BRPOP Blocking Causing Lag

**Problem:** The dirty queue worker was using `BRPOP` (blocking pop) which held the mutex for up to 1 second, causing lag when other threads needed Redis access.

**Solution:** Changed `redis_pop_dirty_key()` from `BRPOP` (blocking) to `RPOP` (non-blocking). Added 100ms sleep in worker loop when queue is empty to prevent CPU spin.

**Files Modified:**
- `redis_cache.c` - Changed BRPOP to RPOP in `redis_pop_dirty_key()`
- `json_persist.c` - Added `usleep(100000)` when queue returns empty

### Issue 3: Redis Settings Not Loading

**Problem:** Redis showed as "not available" in-game even though the Redis server was running. The settings weren't being loaded from JSON.

**Root Cause:** The "redis" category was missing from the `category_names[]` array in `json_game_settings.c`, and Redis defaults weren't being initialized.

**Solution:**
1. Added "redis" to both `category_names[]` arrays
2. Changed hardcoded array size `8` to `SETTING_CAT_MAX`
3. Added Redis defaults to `init_game_settings_defaults()`

**Files Modified:**
- `json_game_settings.c` - Added "redis" category, fixed array sizing, added defaults

### Issue 4: Redis Init Block Commented Out

**Problem:** The entire Redis initialization block in `comm.c` was commented out with `/* ... */`.

**Solution:** Uncommented the Redis init block.

**Files Modified:**
- `comm.c` - Uncommented Redis initialization

### Issue 5: Character Load Function Using Brittle `goto`

**Problem:** `load_char_obj_internal()` used `goto load_success` to skip disk reading when loading from Redis cache. This caused:
1. `fpReserve` not being reopened (closed at start, never reopened on cache path)
2. All post-load processing being skipped (fix_character, immortal setup, etc.)
3. The `load_success:` label was inside an unrelated if block, creating confusing control flow

**Solution:** Refactored to use a `loaded_from_cache` boolean flag:
1. Added `bool loaded_from_cache = false;` at function start
2. Replaced `goto load_success;` with `loaded_from_cache = true;`
3. Wrapped disk loading paths in `if (!loaded_from_cache) { ... }`
4. Added safety check to reopen `fpReserve` if NULL after disk block
5. Removed the `load_success:` label entirely
6. Restructured so all post-load processing runs regardless of load source

**Files Modified:**
- `save.c` - Complete refactor of `load_char_obj_internal()`

### Issue 6: Double-Free Crash on Second Login

**Problem:** First login worked fine, but second login caused:
- Staff characters not recognized as staff
- Segfault on quit: `free(): double free detected in tcache 2`
- Backtrace showed `fclose()` in `save_char_obj()` line 319

**Root Cause:** After `fclose(fpReserve)`, the pointer was NOT set to NULL. It remained as a dangling pointer pointing to freed memory. The safety check `if (fpReserve == NULL)` never triggered because the pointer wasn't NULL - it was just invalid.

**Solution:** Added NULL checks before all `fclose(fpReserve)` calls, and set `fpReserve = NULL` after closing:

```c
if (fpReserve != NULL) {
    fclose(fpReserve);
    fpReserve = NULL;  // Mark as closed
}
```

**Files Modified:**
- `save.c` - Fixed 4 locations:
  - `load_char_obj_internal()` (~line 1108)
  - `save_char_obj()` (~line 319)
  - `load_account()` (~line 5578)
  - `save_account()` (~line 6271)

### Issue 7: plogf Macro Signature Change

**Problem:** Old `plogf()` calls in `fread_char()` used 1-2 arguments, but the macro was updated to require 3 arguments (category, format, ...).

**Solution:** Updated calls to use new signature:
```c
// Old:
plogf("save.c, fread_char(): Char is in a Vroom...");

// New:
plogf(LOG_INFO, "%s is in a Vroom...", ch->name);
```

**Files Modified:**
- `save.c` - Updated plogf calls in fread_char()

### Build Status
✅ All fixes compile successfully
✅ Login/logout cycle works without crashes
✅ Staff recognition works correctly
✅ Redis cache operational

### Lessons Learned

1. **Always set pointers to NULL after freeing/closing** - `fclose()` and `free()` don't modify the pointer, leaving dangling pointers that pass NULL checks.

2. **Avoid `goto` for control flow** - It's easy to accidentally skip important code. Use boolean flags and structured if/else instead.

3. **hiredis is not thread-safe** - Always protect Redis operations with a mutex in multi-threaded applications.

4. **Blocking operations hold locks** - BRPOP holds the mutex for its timeout duration. Use non-blocking alternatives (RPOP) with sleep loops instead.
