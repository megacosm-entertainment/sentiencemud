# Worklog: JSON Cache Refactor - Eliminate Temporary .cache Files

## Date: 2026-01-23

## Summary

Refactored the JSON character loading functions to accept `json_t*` directly, eliminating the wasteful temporary file workaround when loading from Redis cache.

## Problem

When loading a character from Redis cache, the code was:
1. Getting `json_t*` from Redis via `redis_get_char_full()`
2. Writing it to a temp `.cache` file with `json_dump_file()`
3. Reading it back with `json_read_char()` which calls `json_load_file()`
4. Deleting the temp file with `unlink()`

This was wasteful - we already had the parsed JSON in memory.

## Solution

Created new `_from_json()` variants that accept `json_t*` directly, then had the file-based versions call these internally.

## Files Modified

### json_char.h
Added new function declarations:
```c
bool json_read_char_from_json(CHAR_DATA *ch, json_t *root);
bool json_read_char_basic_from_json(CHAR_DATA *ch, json_t *root);
bool json_read_char_remaining_from_json(CHAR_DATA *ch, json_t *root);
```

### json_char.c
- Created `json_read_char_internal_from_json()` - core implementation that processes `json_t*` directly
- Refactored `json_read_char_internal()` to load file then delegate to `_from_json` version
- Added public wrappers: `json_read_char_from_json()`, `json_read_char_basic_from_json()`
- Created `json_read_char_remaining_from_json()` for deferred heavy data loading
- Refactored `json_read_char_remaining()` to delegate to `_from_json` version

### save.c (lines 1128-1147)
Simplified Redis cache loading path:
- Before: `Redis -> json_dump_file -> temp.cache -> json_load_file -> process -> unlink`
- After: `Redis -> json_read_char_from_json -> done`

### nanny.c (lines 4027-4059)
Updated `proceed_to_game()` to try Redis cache first before falling back to disk when loading remaining character data.

## Benefits

1. **No more `.cache` temp files** - eliminates disk I/O when data is already in memory
2. **Cleaner architecture** - JSON processing is now separated from file loading
3. **Faster Redis cache hits** - skips the wasteful write-to-disk-read-from-disk round trip
4. **Consistent pattern** - both initial load and remaining load now benefit from Redis caching

## API Notes

The `_from_json()` functions do NOT call `json_decref()` on the root object - the caller retains ownership and is responsible for cleanup. This follows standard jansson conventions.

## Testing

- Build completed with no new warnings
- Existing file-based functions continue to work unchanged (backward compatible)
- Redis cache path now uses direct JSON object loading
