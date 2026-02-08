# Plan: JSON I/O Consolidation

## Overview

The `src/io/json/` directory contains 17 `.c` files for serializing different game subsystems. Significant code duplication exists across these files. This plan identifies the repeated patterns and proposes a shared utility layer (`json_utils.c/h`) to consolidate them.

## Current State

| File | Lines (approx) | Purpose |
|------|------|---------|
| json_account.c | 756 | Account auth, characters, vault |
| json_area.c | 1600+ | Area/room/mob/obj/script index data |
| json_ban.c | 157 | Ban list |
| json_changesets.c | 198 | Game settings changeset history |
| json_char.c | 1700+ | Player characters, objects |
| json_chat.c | 393 | Chat rooms |
| json_church.c | 906 | Player churches/guilds |
| json_commands.c | 178 | Commands table |
| json_game_settings.c | 768 | Game configuration |
| json_gq.c | 387 | Global quest data |
| json_instance.c | 1279 | Instances/ships/dungeons |
| json_mail.c | 374 | In-game mail |
| json_note.c | 342 | Notes/news/changes |
| json_persist.c | 1500+ | Persistent world state |
| json_projects.c | 334 | Builder projects |
| json_race.c | 496 | Race definitions |
| json_reserved.c | 428 | Reserved vnum entities |
| json_socials.c | 299 | Social emote commands |
| json_staff.c | 201 | Immortal/staff data |

Existing shared utilities in `io/common.c` deal with legacy text file headers/footers and are **not used** by any JSON code.

---

## Duplicated Patterns

### 1. Flag Serialization — `flags_to_json_array` / `flags_from_json_array`

**Four separate implementations** with slightly different signatures:

| File | Function | Signature |
|------|----------|-----------|
| `json_account.c` | `flags_to_json_array` | `(const struct flag_type *table, long bits)` |
| `json_account.c` | `flags_from_json_array` | `(const struct flag_type *table, json_t *array)` |
| `json_area.h` (declared) | `flags_to_json_array` | `(long flags, const struct flag_type *table)` — **args reversed** |
| `json_area.h` (declared) | `json_array_to_flags` | `(json_t *array, const struct flag_type *table)` — **different name** |
| `json_race.c` | `flags_from_json_array` | `(const struct flag_type *table, json_t *array)` — **static duplicate** |
| `json_church.c` | uses `json_area.h` versions | via `#include "json_area.h"` |
| `json_commands.c` | uses `json_area.h` versions | via `#include "json_area.h"` |

**Problems:**
- Two different parameter orderings (`flags, table` vs `table, flags`)
- Two different names (`flags_from_json_array` vs `json_array_to_flags`)
- `json_account.c` has its own `static` copy that also handles stat-type flags differently via `is_stat()`
- `json_race.c` has yet another `static` copy
- Several files include `json_area.h` solely to access the flag helpers

**Proposed unification:**
```c
// json_utils.h
json_t *json_flags_to_array(long bits, const struct flag_type *table);
long    json_flags_from_array(json_t *array, const struct flag_type *table);
```

### 2. WNUM Serialization — `wnum_to_json` / `parse_wnum_from_json`

**Three separate implementations:**

| File | Serialize | Deserialize |
|------|-----------|-------------|
| `json_gq.c` | `wnum_to_json_str(long area_uid, long vnum)` | `parse_wnum_from_json(json_t*, long*, long*)` |
| `json_mail.c` | `wnum_to_json_str(long area_uid, long vnum)` | `parse_wnum_from_json(json_t*, long*, long*)` |
| `json_instance.c` | `wnum_to_json(AREA_DATA*, long vnum)` | `parse_wnum_from_json(json_t*) → WNUM` |

`json_gq.c` and `json_mail.c` are **identical copy-paste** implementations. `json_instance.c` uses a slightly different API returning a `WNUM` struct.

Meanwhile, `json_area.c` uses `widevnum_string_room()` / `parse_widevnum()` directly, and `json_char.c` uses `widevnum_string_object()` etc. These are fine — they use the game's existing widevnum API.

**Proposed unification:**
```c
// json_utils.h
json_t *json_wnum_to_string(long area_uid, long vnum);
json_t *json_wnum_from_area(AREA_DATA *area, long vnum);
void    json_wnum_parse(json_t *json, long *area_uid, long *vnum);
WNUM    json_wnum_parse_resolve(json_t *json);
```

### 3. JSON Format Detection

**Three near-identical implementations:**

| File | Function | Logic |
|------|----------|-------|
| `json_account.c` | `json_is_account_json()` | fopen → fgetc → check for `{` |
| `json_char.c` | `json_is_json_file()` | fopen → fgetc → check for `{` |
| `json_game_settings.c` | `json_is_game_settings_json()` | fopen → fgetc → check for `{` |

All three are byte-for-byte identical in logic.

**Proposed unification:**
```c
// json_utils.h
bool json_file_is_json(const char *filename);
```

### 4. Common Save Pattern (json_dump_file + error handling)

Almost every file follows this pattern for saving:
```c
root = json_object();
json_object_set_new(root, "version", json_integer(N));
// ... build content ...
json_object_set_new(root, "items", array);

ret = json_dump_file(root, path, JSON_INDENT(2));
json_decref(root);
if (ret != 0) {
    log_stringf("...: Failed to write %s", path);
    return false;
}
return true;
```

Files using this exact pattern: `json_ban.c`, `json_changesets.c`, `json_chat.c`, `json_commands.c`, `json_gq.c`, `json_mail.c`, `json_note.c`, `json_projects.c`, `json_staff.c`.

Some files (`json_socials.c`, `json_reserved.c`) use `json_dumps()` + manual `fopen`/`fprintf`/`fclose` instead of `json_dump_file()`, adding unnecessary complexity.

**Proposed utility:**
```c
// json_utils.h
bool json_write_file(json_t *root, const char *path, const char *context);
```
Handles `json_dump_file()`, error logging, and `json_decref()`.

### 5. Common Load Pattern (json_load_file + validation)

Nearly every loader follows:
```c
root = json_load_file(path, 0, &error);
if (!root) {
    log_stringf("...: parse error on line %d: %s", error.line, error.text);
    return false;
}
array = json_object_get(root, "items");
if (!array || !json_is_array(array)) {
    log_stringf("...: missing 'items' array");
    json_decref(root);
    return false;
}
// ... iterate ...
json_decref(root);
```

**Proposed utility:**
```c
// json_utils.h
json_t *json_load_validated(const char *path, const char *array_key, 
                            json_t **out_array, const char *context);
```
Returns root (caller must `json_decref`) and sets `*out_array` if `array_key` is non-NULL.

### 6. String Deserialization Pattern

This pattern appears **hundreds of times** across all files:
```c
str = json_string_value(json_object_get(obj, "key"));
if (str) field = str_dup(str);
```

Or the defensive variant:
```c
value = json_object_get(obj, "key");
if (value && json_is_string(value))
    field = str_dup(json_string_value(value));
```

`json_area.h` already declares `json_get_string_default()`, `json_get_int_default()`, and `json_get_bool_default()` — but these are defined in `json_area.c` and other files can only use them by including `json_area.h`, creating a false dependency.

**Proposed:** Move `json_get_string_default`, `json_get_int_default`, `json_get_bool_default` into `json_utils.c/h` and add:
```c
// json_utils.h
// Already exist in json_area.c, move here:
const char *json_get_string_default(json_t *obj, const char *key, const char *default_val);
long        json_get_int_default(json_t *obj, const char *key, long default_val);
bool        json_get_bool_default(json_t *obj, const char *key, bool default_val);

// New: str_dup wrapper that handles NULL
char       *json_get_str_dup(json_t *obj, const char *key, const char *default_val);
```

### 7. Linked List Deserialization Loop

Many files iterate a JSON array and build a singly-linked list:
```c
json_array_foreach(array, index, value) {
    THING *item = deserialize(value);
    if (item) {
        item->next = NULL;
        if (list_head == NULL)
            list_head = item;
        else
            last->next = item;
        last = item;
    }
}
```

Files: `json_ban.c`, `json_chat.c`, `json_gq.c`, `json_mail.c`, `json_note.c`, `json_staff.c`.

This is structural and hard to abstract without macros. Lower priority, but could use a macro:
```c
#define JSON_APPEND_LINK(head, last, item) do { \
    (item)->next = NULL;                        \
    if (!(head)) (head) = (item);               \
    else (last)->next = (item);                  \
    (last) = (item);                            \
} while(0)
```

### 8. UID Pair Serialization

Two implementations:
- `json_instance.c`: `uid_to_json()` / `json_to_uid()` 
- `json_account.c`: inline `json_array` construction for `account_id`

**Proposed:**
```c
json_t *json_uid_pair_to_array(unsigned long id0, unsigned long id1);
void    json_uid_pair_from_array(json_t *json, unsigned long *id0, unsigned long *id1);
```

### 9. Path Generation

Several files have path generation functions that follow `snprintf(buf, size, "%sX/%s", DIR, tolower(...), name)`:
- `json_account.c`: `json_get_account_path`, `json_get_account_backup_path`, `json_ensure_account_dir`
- `json_char.c`: `json_get_char_path`, `json_get_pfile_path`, `json_get_backup_path`, `json_ensure_char_dir`

These are domain-specific enough to remain in their files, but `json_ensure_char_dir` and `json_ensure_account_dir` are identical in logic and could share:
```c
bool json_ensure_dir_for_name(const char *base_dir, const char *name);
```

---

## Proposed New File: `json_utils.c/h`

Location: `src/io/json/json_utils.c` and `src/io/json/json_utils.h`

### API Summary

```c
#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <jansson.h>
#include "../../merc.h"

/*--- Value Extraction (move from json_area.c) ---*/
const char *json_get_string_default(json_t *obj, const char *key, const char *def);
long        json_get_int_default(json_t *obj, const char *key, long def);
bool        json_get_bool_default(json_t *obj, const char *key, bool def);
char       *json_get_str_dup(json_t *obj, const char *key, const char *def);

/*--- Flag Serialization ---*/
json_t *json_flags_to_array(long bits, const struct flag_type *table);
long    json_flags_from_array(json_t *array, const struct flag_type *table);

/*--- WNUM Serialization ---*/
json_t *json_wnum_to_string(long area_uid, long vnum);
json_t *json_wnum_from_area(AREA_DATA *area, long vnum);
void    json_wnum_parse(json_t *json, long *area_uid, long *vnum);

/*--- UID Pair ---*/
json_t *json_uid_pair_to_array(unsigned long id0, unsigned long id1);
void    json_uid_pair_from_array(json_t *json, unsigned long *id0, unsigned long *id1);

/*--- File I/O Helpers ---*/
bool    json_file_is_json(const char *filename);
bool    json_write_file(json_t *root, const char *path, const char *context);
json_t *json_load_validated(const char *path, const char *array_key,
                            json_t **out_array, const char *context);

/*--- Directory Helpers ---*/
bool    json_ensure_dir_for_name(const char *base_dir, const char *name);

/*--- Linked List Append Macro ---*/
#define JSON_APPEND_LINK(head, last, item) do { \
    (item)->next = NULL;                        \
    if (!(head)) (head) = (item);               \
    else (last)->next = (item);                  \
    (last) = (item);                            \
} while(0)

#endif /* JSON_UTILS_H */
```

---

## Migration Plan

### Phase 1: Create `json_utils.c/h` with all common functions
- Move `json_get_string_default`, `json_get_int_default`, `json_get_bool_default` from `json_area.c`
- Implement unified `json_flags_to_array` / `json_flags_from_array`
- Implement unified WNUM helpers
- Implement `json_file_is_json`, `json_write_file`, `json_load_validated`
- Implement `json_uid_pair_*` and `json_ensure_dir_for_name`
- Update CMakeLists.txt and Makefile

### Phase 2: Migrate simpler files first
Order by least complexity / risk:
1. `json_ban.c` — simplest file, good proving ground
2. `json_staff.c` — simple linked list pattern
3. `json_note.c` — simple, uses standard save/load pattern
4. `json_socials.c` — replace manual fopen/fprintf with `json_write_file`
5. `json_reserved.c` — replace manual fopen/fprintf with `json_write_file`
6. `json_chat.c` — standard pattern

### Phase 3: Migrate WNUM-dependent files
7. `json_gq.c` — replace local `wnum_to_json_str` / `parse_wnum_from_json`
8. `json_mail.c` — same duplicate WNUM code as json_gq.c
9. `json_projects.c` — straightforward
10. `json_changesets.c` — straightforward
11. `json_commands.c` — remove `json_area.h` dependency (only needed for helpers)

### Phase 4: Migrate flag-heavy files
12. `json_account.c` — replace local `flags_to_json_array` / `flags_from_json_array`
13. `json_race.c` — replace local static `flags_from_json_array`
14. `json_church.c` — already uses `json_area.h` helpers, switch to `json_utils.h`
15. `json_game_settings.c` — replace `json_is_game_settings_json` with shared version

### Phase 5: Update json_area.h/c
16. `json_area.h` — remove utility declarations (now in `json_utils.h`)
17. `json_area.c` — remove utility implementations, include `json_utils.h`
18. Files that included `json_area.h` for utilities switch to `json_utils.h`

### Phase 6: Clean up complex files
19. `json_char.c` — replace `json_is_json_file`, use `json_get_str_dup`
20. `json_instance.c` — replace local WNUM/UID helpers
21. `json_persist.c` — largest, use helpers where applicable

### Phase 7: Evaluate `io/common.c`
- The file header/footer functions are for a text format, not JSON
- If nothing uses them, consider removing or documenting their purpose
- If legacy loaders use them, leave as-is

---

## Estimated Impact

| Metric | Before | After (est.) |
|--------|--------|------|
| Duplicate flag helper implementations | 4 | 1 |
| Duplicate WNUM helpers | 3 | 1 |
| Duplicate format detection functions | 3 | 1 |
| Files including json_area.h just for helpers | 3 | 0 |
| Total lines saved (estimated) | — | ~300-400 |

## Risks

- **Parameter order change**: `json_area.h` uses `(long flags, const struct flag_type*)` while `json_account.c` uses `(const struct flag_type*, long bits)`. The unified version must pick one and update all callers.
- **`is_stat()` in json_account.c flag helper**: The account version has special stat-flag handling. The unified version needs to handle both bitfield and stat-type flag tables, or the account code keeps a wrapper.
- **Build system**: Both `CMakeLists.txt` and `Makefile` must be updated for the new file.
- **json_area.h is widely included**: Removing helpers from it requires checking all includers.

## Testing

After each phase:
1. `./build tests && cd /sentience && ./sent -test`
2. Verify JSON save/load roundtrips for affected subsystems
3. Spot-check a generated JSON file for correct formatting
