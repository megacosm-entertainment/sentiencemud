# Plan: JSON I/O Consolidation

## Overview

The `src/io/json/` directory contains 20+ `.c` files for serializing different game
subsystems. This plan tracks the consolidation of duplicated patterns into a shared
utility layer (`json_common.c/h`).

---

## Phase 1 — COMPLETED

Phase 1 created `json_common.c/h` and migrated all duplicate function
implementations into the shared file. Backward-compatibility macros in
`json_common.h` preserve old names so callers can migrate gradually.

### What was consolidated

| Pattern | Instances removed | Shared function |
|---------|-------------------|-----------------|
| Flag serialize/deserialize | 4 static copies → 1 shared | `json_flags_serialize()` / `json_flags_deserialize()` |
| WNUM serialize/deserialize | 2 identical copies → 1 shared | `json_wnum_serialize()` / `json_wnum_deserialize()` |
| JSON format detection | 3 identical copies → 1 shared | `json_file_is_json()` |
| Ensure-directory helper | 2 identical copies → 1 shared | `json_ensure_dir()` |
| String/int/bool getters | Moved from json_area.c → shared | `json_get_string()` / `json_get_int()` / `json_get_bool()` |
| Enum serialize/deserialize | New (used by json_obj_types.c) | `json_enum_serialize()` / `json_enum_deserialize()` |

### Files modified

| File | Changes |
|------|---------|
| `json_common.c` | **Created** — ~250 lines, all shared implementations |
| `json_common.h` | Updated — full API docs, macros, backward compat aliases |
| `json_area.c` / `json_area.h` | Removed 5 getter + 2 flag implementations/declarations |
| `json_account.c` | Removed static flags + is_json + ensure_dir |
| `json_char.c` | Removed static flags + is_json + ensure_dir, 34 call sites updated |
| `json_race.c` | Removed both static flags functions, 20 call sites updated |
| `json_gq.c` | Removed static wnum functions, 6 call sites |
| `json_mail.c` | Removed static wnum functions, 4 call sites |
| `json_obj_types.c` | Simplified flag/enum wrappers to delegate |
| `json_game_settings.c` | Simplified is_json to one-liner |
| `json_commands.c` | Switched from json_area.h to json_common.h |
| `json_instance.c` | Simplified wnum_to_json to delegate |
| `CMakeLists.txt` / `Makefile` | Added json_common.c |

### Backward compatibility aliases (in json_common.h)

```c
#define flags_to_json_array(flags, table)    json_flags_serialize((flags), (table))
#define json_array_to_flags(array, table)    json_flags_deserialize((array), (table))
#define json_get_string_default(obj, key, d) json_get_string((obj), (key), (d))
#define json_get_int_default(obj, key, d)    json_get_int((obj), (key), (d))
#define json_get_bool_default(obj, key, d)   json_get_bool((obj), (key), (d))
```

---

## Phase 2 — COMPLETED

Phase 2 adopted the existing `json_common.c/h` utilities (`json_string_safe`,
`json_file_save`, `json_file_load`, `JSON_APPEND_LINK`) across all consumer
files. Clean build (206/206) passed.

### Summary of changes

| File | 2A string_safe | 2B file_save | 2C file_load | 2D APPEND_LINK | Notes |
|------|:-:|:-:|:-:|:-:|-------|
| `json_ban.c` | 1 | 1 | 1 | 1 | All 4 patterns |
| `json_staff.c` | 4 | 1 | 1 | — | |
| `json_note.c` | 5 | 1 | 1 | 1 | |
| `json_changesets.c` | 4 | 1 | 1 | — | Fixed dropped `next_changeset_id` |
| `json_chat.c` | 5 | 1 | 1 | 2 | Removed unused `json_error_t` |
| `json_commands.c` | 5 | 1 | 1 | — | Includes IS_NULLSTR variants |
| `json_projects.c` | 8 | 1 | 1 | 3 | Removed unused `ret`, `json_error_t` |
| `json_gq.c` | — | 1 | 1 | 2 | Removed unused vars |
| `json_mail.c` | — | 1 | 1 | 1 | Removed unused vars |
| `json_race.c` | 4 | skip | 1 | — | Save has custom error handling |
| `json_game_settings.c` | — | 1 | 2 | — | Both load sites + retry migration |
| `json_instance.c` | 1 | 3 | 4 | — | 3 save + 4 load (ship/dungeon/instance) |
| `json_persist.c` | 13 | 3 | skip | — | Loads intentionally suppress errors |
| `json_area.c` | 16 | 2 | skip | 4 | Load has custom line-number reporting |
| `json_church.c` | 11 | skip | skip | 2 | Load uses `pbugf` + line numbers |
| `json_char.c` | 3 | skip | 3 | — | Save uses tmp+rename (skip 2B) |
| `json_account.c` | — | skip | skip | — | No applicable patterns |
| **Totals** | **80** | **19** | **19** | **16** | |

### What was skipped (with rationale)

- **2B skips**: `json_race.c` (custom multi-line error logging), `json_church.c`
  (no save function), `json_char.c` / `json_account.c` (atomic write via
  tmp file + rename for crash safety)
- **2C skips**: `json_persist.c` (loads intentionally return NULL silently when
  file doesn't exist), `json_area.c` (logs `error.line` in addition to
  `error.text`), `json_church.c` (uses `pbugf` with line numbers),
  `json_account.c` (silent failure for old pfile fallback)

### Files that needed `#include "json_common.h"` added

- `json_ban.c`, `json_staff.c`, `json_note.c`, `json_changesets.c`,
  `json_chat.c`, `json_projects.c`, `json_persist.c`

(Other files already had it from Phase 1 or transitively via `json_area.h`)

---

## Phase 3 — Future opportunities (not yet planned)

These require new utilities or more invasive changes:

### 3A. `json_get_str_dup()` — str_dup with default

Hundreds of instances of:
```c
str = json_string_value(json_object_get(obj, "key"));
if (str) field = str_dup(str);
```

Could become:
```c
field = json_get_str_dup(obj, "key", "");
```

Requires adding a new function to `json_common.c`.

### 3B. Atomic file save (tmp + rename)

`json_account.c` and `json_char.c` use crash-safe write (temp file + rename).
Could add `json_file_save_atomic()` to handle this pattern.

### 3C. UID pair serialization

`json_instance.c` has `uid_to_json()` / `json_to_uid()`. Only two call sites.
Low priority.

### 3D. `json_socials.c` / `json_reserved.c` — manual fprintf

These files use `json_dumps()` + `fopen`/`fprintf`/`fclose` instead of
`json_dump_file()`. Could be migrated to `json_file_save()`.

---

## Testing

After each batch of changes:
1. `./build tests && cd /sentience && ./sent -test`
2. `./build clean` to verify no stale objects
3. Spot-check generated JSON files for correct formatting
