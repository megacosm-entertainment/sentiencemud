# Plan: Redis Caching Layer for Area Data

**Status:** IMPLEMENTED
**Date:** 2026-01-29
**Related:** `/sentience/docs/json_area_implementation.md`

## Overview

Implement Redis caching for area JSON data following the same pattern as player files. Redis serves as the fast read/write layer, with async disk writes for durability.

## Goals

1. **Async Cache Warming:** After boot, warm Redis cache non-blocking
2. **Immediate Cache Updates:** OLC edits and saves update Redis immediately
3. **Async Disk Writes:** Area saves queue dirty keys for background disk flush
4. **Reduced Disk I/O:** Normal operation only hits Redis, disk writes are batched

## Architecture

```
BOOT SEQUENCE:
┌──────────┐     ┌──────────┐     ┌──────────┐
│   Disk   │────>│  Memory  │────>│  Redis   │  (async warm)
│  (.json) │     │(AREA_DATA)│     │ (cache)  │
└──────────┘     └──────────┘     └──────────┘

EDIT/SAVE SEQUENCE:
┌──────────┐     ┌──────────┐     ┌──────────┐
│  Memory  │────>│  Redis   │────>│   Disk   │  (async via dirty queue)
│(AREA_DATA)│     │ (cache)  │     │  (.json) │
└──────────┘     └──────────┘     └──────────┘
                      │
                      v
               ┌─────────────┐
               │ Dirty Queue │──> Background Writer
               │(persist:dirty)│
               └─────────────┘
```

## Data Flow

### Boot (Cold Start)
1. Load area from disk (JSON or .are fallback)
2. Deserialize into memory (AREA_DATA)
3. Queue async job to serialize and cache in Redis
4. Continue boot without waiting

### OLC Editing
1. Builder makes changes in memory
2. Changes immediately serialized to Redis cache
3. No disk write yet (dirty flag set on area)

### Area Save (asave command)
1. Serialize AREA_DATA to JSON
2. Store in Redis immediately
3. Queue dirty key for async disk write
4. Background writer pops from queue and writes to disk

## Key Schema

Following existing patterns from `redis_cache.h`:

```
area:{uid}:full     - Full area JSON (primary cache)
area:{uid}:meta     - Lightweight metadata for area listing
area:{uid}:version  - Version hash for change detection
area:{uid}:loaded   - Timestamp when area was last loaded
```

## TTL Strategy

```c
#define REDIS_TTL_AREA_FULL     (7 * 24 * 3600)  // 7 days - areas rarely change
#define REDIS_TTL_AREA_META     (24 * 3600)      // 24 hours - for area listings
#define REDIS_TTL_AREA_LOADED   (24 * 3600)      // 24 hours - load tracking
```

## Implementation Plan

### Phase 1: Core Caching Functions

Add to `redis_cache.h`:
```c
// Area caching functions
bool redis_cache_area_full(AREA_DATA *area, json_t *area_json);
json_t *redis_get_area_full(long area_uid);
bool redis_invalidate_area(long area_uid);

// Area metadata for listings
typedef struct area_info_cache {
    long uid;
    char *name;
    char *filename;
    int min_level;
    int max_level;
    int room_count;
    int mob_count;
    int obj_count;
    long last_modified;
} AREA_INFO_CACHE;

AREA_INFO_CACHE *redis_get_area_info(long area_uid);
void free_area_info_cache(AREA_INFO_CACHE *info);

// Bulk operations
void redis_warm_area_cache(void);
void redis_invalidate_all_areas(void);
```

Add to `redis_cache.c`:
```c
bool redis_cache_area_full(AREA_DATA *area, json_t *area_json)
{
    char *key, *json_str;
    bool result;

    if (!redis_is_available() || !area || !area_json) {
        return false;
    }

    // Create key: area:{uid}:full
    char uid_str[32];
    snprintf(uid_str, sizeof(uid_str), "%ld", area->uid);
    key = redis_key("area", uid_str, "full");

    // Serialize JSON to string
    json_str = json_dumps(area_json, JSON_COMPACT);
    if (!json_str) {
        return false;
    }

    result = redis_cache_persist_data(key, json_str);
    free(json_str);

    return result;
}

json_t *redis_get_area_full(long area_uid)
{
    char *key, *json_str;
    json_t *root;
    json_error_t error;

    if (!redis_is_available()) {
        return NULL;
    }

    char uid_str[32];
    snprintf(uid_str, sizeof(uid_str), "%ld", area_uid);
    key = redis_key("area", uid_str, "full");

    json_str = redis_get_persist_data(key);
    if (!json_str) {
        stats.misses++;
        return NULL;
    }

    root = json_loads(json_str, 0, &error);
    free(json_str);

    if (!root) {
        log_stringf("Redis: Failed to parse area JSON: %s", error.text);
        return NULL;
    }

    stats.hits++;
    return root;
}
```

### Phase 2: Integration with json_area.c

Modify `json_area_load()`:
```c
AREA_DATA *json_area_load(const char *filename)
{
    AREA_DATA *area = NULL;
    json_t *root = NULL;
    long area_uid;

    // Try to determine area UID from filename or previous load
    area_uid = get_area_uid_from_filename(filename);

    // Phase 1: Try Redis cache first
    if (area_uid > 0) {
        root = redis_get_area_full(area_uid);
        if (root) {
            log_stringf("Loading area from Redis cache: %s (uid %ld)", filename, area_uid);
            area = json_area_deserialize(root);
            if (area) {
                json_decref(root);
                return area;
            }
            // Cache hit but deserialize failed - invalidate and fall through
            redis_invalidate_area(area_uid);
            json_decref(root);
        }
    }

    // Phase 2: Load from disk
    log_stringf("Loading area from disk: %s", filename);
    root = json_load_file(filename, 0, &error);
    if (!root) {
        return NULL;
    }

    area = json_area_deserialize(root);

    // Phase 3: Populate cache
    if (area && area->uid > 0) {
        redis_cache_area_full(area, root);
    }

    json_decref(root);
    return area;
}
```

Modify `json_area_save()`:
```c
bool json_area_save(AREA_DATA *area)
{
    json_t *root;
    bool result;

    // Serialize to JSON
    root = json_area_serialize(area);
    if (!root) {
        return false;
    }

    // Write to disk
    result = json_dump_file(root, filename, JSON_INDENT(2));

    // Update Redis cache (write-through)
    if (result && area->uid > 0) {
        redis_cache_area_full(area, root);
    }

    json_decref(root);
    return result;
}
```

### Phase 3: Cache Warming

Add to `boot_db()` or startup sequence:
```c
void redis_warm_area_cache(void)
{
    AREA_DATA *area;
    int cached = 0;

    if (!redis_is_available()) {
        return;
    }

    log_string("Redis: Warming area cache...");

    for (area = area_first; area; area = area->next) {
        json_t *root = json_area_serialize(area);
        if (root && area->uid > 0) {
            if (redis_cache_area_full(area, root)) {
                cached++;
            }
            json_decref(root);
        }
    }

    log_stringf("Redis: Cached %d areas", cached);
}
```

### Phase 4: Hot-Reload Support (Future)

```c
// Reload a single area from cache or disk
bool area_hot_reload(long area_uid)
{
    AREA_DATA *old_area, *new_area;
    json_t *root;

    // Find existing area
    old_area = get_area_by_uid(area_uid);
    if (!old_area) {
        return false;
    }

    // Invalidate cache to force disk read
    redis_invalidate_area(area_uid);

    // Load fresh copy
    new_area = json_area_load(old_area->file_name);
    if (!new_area) {
        return false;
    }

    // Swap data (complex - needs careful handling of active players/mobs)
    // ... implementation details ...

    return true;
}
```

## Performance Considerations

1. **Memory Usage:** Full area JSON can be large (1-10MB for complex areas)
   - Use JSON_COMPACT for Redis storage to reduce size
   - Consider compression for very large areas (zlib)

2. **Serialization Overhead:** `json_dumps()` and `json_loads()` have CPU cost
   - Cache serialized string if needed for repeated access
   - Use RedisJSON module for partial updates if available

3. **Cache Invalidation:** Must invalidate on:
   - `asave area` command
   - OLC changes that modify area
   - Area reset (if reset modifies index data)

4. **Startup Order:** Redis init must happen before area loading

## Testing Checklist

- [ ] Load area from empty cache (cache miss, populate from disk)
- [ ] Load area from populated cache (cache hit)
- [ ] Save area updates cache (write-through)
- [ ] Invalidate area removes from cache
- [ ] Cache warm populates all areas
- [ ] Handle Redis unavailable gracefully (fallback to disk)
- [ ] Handle corrupt cache data (invalidate and reload)
- [ ] Measure boot time improvement with warm cache

## Files to Modify

1. **redis_cache.h** - Add area caching function declarations
2. **redis_cache.c** - Implement area caching functions
3. **json_area.c** - Integrate cache into load/save
4. **db.c** - Add cache warming to boot sequence
5. **olc_save.c** - Ensure asave invalidates/updates cache

## Dependencies

- Existing Redis infrastructure (`redis_cache.c`)
- JSON area serialization (`json_area.c`)
- Jansson library

## Estimated Effort

- Phase 1 (Core Functions): ~100 lines
- Phase 2 (Integration): ~50 lines of modifications
- Phase 3 (Cache Warming): ~30 lines
- Phase 4 (Hot-Reload): ~100 lines (future)

**Total: ~280 lines of new/modified code**

## Notes

- The existing `redis_cache_persist_data()` can be reused for area caching
- Consider using the dirty queue pattern for async disk writes if needed
- Area UIDs are stable identifiers, making them ideal cache keys
