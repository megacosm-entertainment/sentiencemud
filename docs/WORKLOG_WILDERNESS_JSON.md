# Worklog: Wilderness JSON Loading Fixes

## Date: 2026-01-29

## Summary

Fixed critical issues with loading wilderness (wilds) data from JSON area files. The JSON-loaded wilderness was displaying incorrectly (all tiles as yellow 'O') and causing segfaults when ships with invalid rooms were rendered.

## Issues Fixed

### Issue 1: All Wilderness Tiles Displaying as Yellow 'O'

**Symptoms:**
- Stepping into wilderness loaded from JSON showed all tiles as yellow 'O' characters
- Map display was completely broken
- The same area loaded from `.are` format worked correctly

**Root Cause:**
The `json_to_wilds()` function in `json_area.c` had two problems:

1. **Missing `wilds->map` allocation**: The `.are` loader allocates both `staticmap` (permanent copy) and `map` (working copy used for display), but the JSON loader only set `staticmap` via `str_dup()`. The `map` pointer was left as `str_empty[0]` from `new_wilds()`. When display code accessed `pWilds->map[index]`, it read garbage/empty memory, causing terrain lookups to fail and fall back to yellow 'O'.

2. **Improper buffer allocation**: The JSON loader wasn't using `allocate_wildsmap()` which properly allocates `map_size_x * map_size_y + 1` bytes.

**Fix (json_area.c lines 146-169):**
```c
// Allocate staticmap and map buffers like the .are loader does
const char *json_map = json_get_string_default(json, "staticmap", "");
int map_total = wilds->map_size_x * wilds->map_size_y;
if (map_total > 0) {
    wilds->staticmap = allocate_wildsmap(wilds->map_size_x, wilds->map_size_y);
    wilds->map = allocate_wildsmap(wilds->map_size_x, wilds->map_size_y);

    // Copy map data directly - newlines in the JSON ARE terrain data
    memcpy(wilds->staticmap, json_map, map_total);
    // Copy staticmap to map (working copy)
    memcpy(wilds->map, wilds->staticmap, map_total);
} else {
    wilds->staticmap = str_dup("");
    wilds->map = str_dup("");
}
```

**Files Modified:**
- `json_area.c`: Fixed `json_to_wilds()` buffer allocation
- `wilds.h`: Added declaration for `allocate_wildsmap()`

### Issue 2: Segfault in show_map_to_char_wyx

**Symptoms:**
- Segmentation fault when entering wilderness
- Crash at `wilds.c:1709` accessing `ship->ship->in_room->wilds`

**Root Cause:**
Ships that failed to load properly (due to missing "Uid" field support in `ship_load`) had `in_room = NULL`. The map display code iterated through `loaded_ships` without checking for NULL `in_room`.

**Fix (wilds.c line 1706-1711):**
```c
iterator_start(&it, loaded_ships);
while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
{
    // Skip ships without a valid room (can happen if ship failed to load properly)
    if (!ship->ship || !ship->ship->in_room)
        continue;

    if( ship->ship->in_room->wilds == pWilds &&
```

**Files Modified:**
- `wilds.c`: Added NULL check for ship rooms in `show_map_to_char_wyx()`

## Technical Notes

### Wilderness Data Structure
The wilderness system uses two map buffers:
- `staticmap`: The permanent terrain data loaded from file
- `map`: A working copy that gets modified at runtime (e.g., vlinks marked as '0')

Both must be properly allocated with `allocate_wildsmap()` which uses `calloc()` to allocate `map_size_x * map_size_y + 1` bytes.

### JSON Staticmap Format
The JSON staticmap string contains the raw terrain data as a continuous string. Newline characters in the string are valid terrain data (representing empty/null terrain at the top of maps), not row separators. The `.are` format stores rows with trailing newlines consumed by `fread_to_eol()`, but embedded newlines within rows are preserved.

### Ship Loading Issue
The ship persistence format doesn't recognize the "Uid" field, causing ships to fail loading:
```
BUG: ship_load: no match for word Uid
persist_load_object: could not resolve room for object vnum 157001
ship_load: loaded ship object with NULL in_room
```

This is a separate issue related to the widevnum migration that needs to be addressed in `boat.c`.

## Testing

- Verified wilderness loads correctly from JSON
- Verified terrain displays properly
- Verified vlinks appear at correct positions
- Verified no segfaults when ships fail to load

## Related Documentation

- `/sentience/docs/json_area_implementation.md` - JSON area format specification
- `/sentience/src/wilds.c` - Wilderness loading and display code
- `/sentience/src/json_area.c` - JSON area serialization/deserialization
