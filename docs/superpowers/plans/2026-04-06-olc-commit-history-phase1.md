# OLC Commit History & Scalar Staging — Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add commit-level history with revert capability to the OLC staged changeset system, plus convert remaining simple scalar editor commands to use staging.

**Architecture:** A new `olc_commit_history` module stores per-entity commit records (capped, newest-first). On every `commit`/`commit group`, the changeset's changes are archived into a commit record before being cleared. History is lazy-loaded from disk, cached in Redis. New `history`/`history revert` commands replace the old `OLC_CHANGE_HISTORY` system for staged editors.

**Tech Stack:** C23, Jansson (JSON), Redis cache (`io/cache/redis_cache.h`), LLIST linked lists, game settings (`tables.h`/`merc.h`)

**Design Spec:** `docs/superpowers/specs/2026-04-06-olc-commit-history-and-full-staging-design.md`

---

## File Structure

### New Files

| File | Responsibility |
|------|---------------|
| `editors/common/olc_commit_history.h` | Commit history data model: `olc_committed_change_t`, `olc_commit_record_t`, `olc_commit_history_t` structs + full CRUD/serialization API |
| `editors/common/olc_commit_history.c` | Implementation: create/destroy, archive, lookup, eviction, serialize/deserialize, disk save/load, Redis cache |
| `tests/unit/olc_commit_history_tests.c` | Unit tests: data model CRUD, serialization roundtrip, eviction, history revert logic |
| `tests/data/unit/olc_commit_history_tests.json` | JSON test definitions for history tests |

### Modified Files

| File | Changes |
|------|---------|
| `editors/common/olc_staged.c` | Hook `olc_commit_history_archive()` into `olc_staged_cmd_commit()` and `olc_staged_cmd_commit_group()`. Add history command implementations: `olc_staged_cmd_history()`, `olc_staged_cmd_history_detail()`, `olc_staged_cmd_history_revert()` |
| `editors/common/olc_staged.h` | Declare new history command functions |
| `editors/common/olc_editor.c` | Route `history` and `view` through staged system when `change_mode == OLC_CHANGE_STAGED` (before falling through to old system). Add save-history-on-area-save hook. |
| `editors/objects/oedit.c` | Convert ~5 unstaged scalar commands to `olc_cmd_*` + add field handlers |
| `editors/mobiles/medit.c` | Convert ~6 unstaged scalar commands to `olc_cmd_*` + add field handlers |
| `editors/rooms/redit.c` | Convert ~1 unstaged scalar command to `olc_cmd_*` + add field handler |
| `editors/areas/aedit.c` | Convert ~4 unstaged scalar commands to `olc_cmd_*` + add field handlers |
| `merc.h` | Add `olc_history_limit` to `game_settings_data` struct |
| `tables.c` | Register `olc_history_limit` in `game_settings_table[]`, add `SETTING_CAT_OLC` |
| `merc.h` | Add `SETTING_CAT_OLC` constant (alongside existing `SETTING_CAT_*` at lines 342-357) |
| `CMakeLists.txt` | Add new `.c` files to source lists and BUILD_TESTS section |
| `Makefile` | Add new `.c` files to source lists and BUILD_TESTS section |
| `tests/framework/test_dispatcher.c` | Register `olchist_` handler in `handler_table` |
| `tests/framework/test_modules.h` | Declare `run_olc_commit_history_test_case()` |
| `tests/data/test_config.json` | Add `olc_commit_history_tests` suite |

---

## Task 1: Game Setting for History Limit

Add the configurable `olc_history_limit` game setting before any history code references it.

**Files:**
- Modify: `merc.h` (inside `struct game_settings_data`, after `mission_history_limit` ~line 1390)
- Modify: `merc.h` (update `SETTING_CAT_MAX` and add `SETTING_CAT_OLC` — note: SETTING_CAT_* constants are in `merc.h` lines 342-357, NOT `tables.h`)
- Modify: `tables.c` (add entry to `game_settings_table[]` and category name)

- [ ] **Step 1: Add setting field to merc.h**

In `merc.h`, inside `struct game_settings_data`, after the mission settings block, add:

```c
    /* OLC Settings */
    int olc_history_limit;  // Max commit history records per entity (default 20, 0 = unlimited)
```

- [ ] **Step 2: Add SETTING_CAT_OLC to merc.h**

In `merc.h`, after `SETTING_CAT_DEBUG` (line 352), add:

```c
#define SETTING_CAT_OLC 12
```

Then change the existing `SETTING_CAT_MAX 12` (line 357) to:

```c
#define SETTING_CAT_MAX 13 /* Number of setting categories */
```

- [ ] **Step 3: Register in tables.c**

In `tables.c`, in the `game_settings_table[]` array, add before the sentinel:

```c
    /* OLC Settings */
    { "olc_history_limit", &game_settings.olc_history_limit, SETTING_TYPE_INT, SETTING_CAT_OLC, "Max commit history records per OLC entity (0 = unlimited, default 20)", true, false, false, NULL },
```

Also in the `setting_category_names[]` array, add `"OLC"` at index 12 (before the closing `}`).

- [ ] **Step 4: Build and verify**

```bash
cd /sentience/src && ./build tests
```

Expected: Clean compile.

- [ ] **Step 5: Commit**

```bash
git add merc.h tables.c
git commit -m "feat(olc): add olc_history_limit game setting

Configurable limit for OLC commit history records per entity.
Default 20, 0 = unlimited. Category: OLC.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 2: Commit History Data Model

Create the core data structures and CRUD functions.

**Files:**
- Create: `editors/common/olc_commit_history.h`
- Create: `editors/common/olc_commit_history.c`
- Modify: `CMakeLists.txt` (add `editors/common/olc_commit_history.c`)
- Modify: `Makefile` (add `editors/common/olc_commit_history.c`)

- [ ] **Step 1: Create olc_commit_history.h**

Create `editors/common/olc_commit_history.h` with:

```c
/**
 * @file olc_commit_history.h
 * @brief Commit-level history for OLC staged editors.
 *
 * Tracks committed changesets as revertable records. Each entity has its own
 * history, capped at a configurable limit (game setting: olc_history_limit).
 * History is lazy-loaded from disk, cached in Redis.
 */

#ifndef __OLC_COMMIT_HISTORY_H__
#define __OLC_COMMIT_HISTORY_H__

#include "olc_changeset.h"

/* Default history limit if game setting is 0 or unset */
#define OLC_DEFAULT_HISTORY_LIMIT  20

/**
 * A single field change within a committed record.
 * Mirrors olc_pending_change_t but is immutable once archived.
 */
typedef struct olc_committed_change {
    char            *field_path;
    olc_field_type_t field_type;
    json_t          *old_value;
    json_t          *new_value;
} olc_committed_change_t;

/**
 * A single commit record — an archived changeset.
 */
typedef struct olc_commit_record {
    int              id;             /* Sequential per-entity, starts at 1 */
    int              group_id;       /* Shared across entities in group commit; 0 = solo */
    char            *author;
    char            *comment;        /* Optional commit message */
    time_t           timestamp;
    LLIST           *changes;        /* List of olc_committed_change_t* */
} olc_commit_record_t;

/**
 * Per-entity commit history.
 */
typedef struct olc_commit_history {
    int              editor_type;    /* ED_* constant */
    WNUM_LOAD        entity_wnum;
    LLIST           *records;        /* List of olc_commit_record_t*, newest first */
    int              next_id;        /* Next sequential ID to assign */
    int              max_records;    /* From game setting, cached at creation */
    bool             is_dirty;       /* Needs disk save */
} olc_commit_history_t;

/* --- Global state --- */
extern int olc_next_group_id;        /* Global group ID counter */

/* --- Committed change lifecycle --- */
olc_committed_change_t *olc_committed_change_create(const char *field_path,
    olc_field_type_t type, json_t *old_value, json_t *new_value);
void olc_committed_change_destroy(olc_committed_change_t *change);

/* --- Commit record lifecycle --- */
olc_commit_record_t *olc_commit_record_create(const char *author, int group_id);
void olc_commit_record_destroy(olc_commit_record_t *record);

/* --- History lifecycle --- */
olc_commit_history_t *olc_commit_history_create(int editor_type, WNUM_LOAD wnum);
void olc_commit_history_destroy(olc_commit_history_t *history);

/* --- History operations --- */

/** Archive a changeset into a new commit record. Returns the new record, or NULL on error. */
olc_commit_record_t *olc_commit_history_archive(olc_commit_history_t *history,
    olc_changeset_t *cs, int group_id, const char *comment);

/** Find a record by ID. Returns NULL if not found. */
olc_commit_record_t *olc_commit_history_find(olc_commit_history_t *history, int id);

/** Get the number of records. */
int olc_commit_history_count(olc_commit_history_t *history);

/** Get the effective max records limit (from game settings or default). */
int olc_commit_history_get_limit(void);

/* --- Serialization --- */
json_t *olc_commit_history_serialize(olc_commit_history_t *history);
olc_commit_history_t *olc_commit_history_deserialize(json_t *json);
json_t *olc_commit_record_serialize(olc_commit_record_t *record);

/* --- Persistence (disk + Redis cache) --- */

/** Load history for an entity. Checks Redis first, then disk. Returns NULL if none. */
olc_commit_history_t *olc_commit_history_load(int editor_type, WNUM_LOAD wnum);

/** Save history to disk and update Redis cache. Returns true on success. */
bool olc_commit_history_save(olc_commit_history_t *history);

/** Delete history from disk and Redis. */
void olc_commit_history_delete(int editor_type, WNUM_LOAD wnum);

/** Save all dirty histories. Called on area save / shutdown. */
void olc_commit_history_save_all_dirty(void);

/** Get or load history for an entity (lazy load). */
olc_commit_history_t *olc_commit_history_get_or_load(int editor_type, WNUM_LOAD wnum);

#endif /* __OLC_COMMIT_HISTORY_H__ */
```

- [ ] **Step 2: Create olc_commit_history.c — lifecycle functions**

Create `editors/common/olc_commit_history.c` with includes and the lifecycle functions:

```c
/**
 * @file olc_commit_history.c
 * @brief Commit-level history for OLC staged editors — implementation.
 */

#include "olc_commit_history.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../log.h"
#include "../../io/cache/redis_cache.h"
#include <string.h>
#include <sys/stat.h>

int olc_next_group_id = 1;

/* In-memory cache of loaded histories — avoids repeated disk/Redis reads */
static LLIST *loaded_histories = NULL;

static void ensure_loaded_histories(void)
{
    if (!loaded_histories)
        loaded_histories = list_create(false);
}

/* --- Committed change lifecycle --- */

olc_committed_change_t *olc_committed_change_create(const char *field_path,
    olc_field_type_t type, json_t *old_value, json_t *new_value)
{
    olc_committed_change_t *change = alloc_mem(sizeof(*change));
    change->field_path = str_dup(field_path ? field_path : "");
    change->field_type = type;
    change->old_value = old_value ? json_incref(old_value) : NULL;
    change->new_value = new_value ? json_incref(new_value) : NULL;
    return change;
}

void olc_committed_change_destroy(olc_committed_change_t *change)
{
    if (!change) return;
    free_string(change->field_path);
    if (change->old_value) json_decref(change->old_value);
    if (change->new_value) json_decref(change->new_value);
    free_mem(change, sizeof(*change));
}

/* --- Commit record lifecycle --- */

olc_commit_record_t *olc_commit_record_create(const char *author, int group_id)
{
    olc_commit_record_t *record = alloc_mem(sizeof(*record));
    record->id = 0;
    record->group_id = group_id;
    record->author = str_dup(author ? author : "");
    record->comment = str_dup("");
    record->timestamp = time(NULL);
    record->changes = list_create(false);
    return record;
}

void olc_commit_record_destroy(olc_commit_record_t *record)
{
    if (!record) return;
    free_string(record->author);
    free_string(record->comment);
    if (record->changes) {
        ITERATOR it;
        iterator_start(&it, record->changes);
        olc_committed_change_t *change;
        while ((change = iterator_nextdata(&it)) != NULL)
            olc_committed_change_destroy(change);
        iterator_stop(&it);
        list_destroy(record->changes);
    }
    free_mem(record, sizeof(*record));
}

/* --- History lifecycle --- */

olc_commit_history_t *olc_commit_history_create(int editor_type, WNUM_LOAD wnum)
{
    olc_commit_history_t *history = alloc_mem(sizeof(*history));
    history->editor_type = editor_type;
    history->entity_wnum = wnum;
    history->records = list_create(false);
    history->next_id = 1;
    history->max_records = olc_commit_history_get_limit();
    history->is_dirty = false;
    return history;
}

void olc_commit_history_destroy(olc_commit_history_t *history)
{
    if (!history) return;
    if (history->records) {
        ITERATOR it;
        iterator_start(&it, history->records);
        olc_commit_record_t *record;
        while ((record = iterator_nextdata(&it)) != NULL)
            olc_commit_record_destroy(record);
        iterator_stop(&it);
        list_destroy(history->records);
    }
    free_mem(history, sizeof(*history));
}

int olc_commit_history_get_limit(void)
{
    int limit = game_settings.olc_history_limit;
    return (limit > 0) ? limit : OLC_DEFAULT_HISTORY_LIMIT;
}
```

- [ ] **Step 3: Add archive and lookup functions to olc_commit_history.c**

Append to `olc_commit_history.c`:

```c
/* --- History operations --- */

olc_commit_record_t *olc_commit_history_archive(olc_commit_history_t *history,
    olc_changeset_t *cs, int group_id, const char *comment)
{
    if (!history || !cs) return NULL;
    if (olc_changeset_count(cs) == 0) return NULL;

    olc_commit_record_t *record = olc_commit_record_create(cs->author, group_id);
    record->id = history->next_id++;

    if (comment && comment[0] != '\0') {
        free_string(record->comment);
        record->comment = str_dup(comment);
    }

    /* Copy changes from changeset into committed record */
    ITERATOR it;
    iterator_start(&it, cs->changes);
    olc_pending_change_t *pending;
    while ((pending = iterator_nextdata(&it)) != NULL) {
        olc_committed_change_t *committed = olc_committed_change_create(
            pending->field_path, pending->field_type,
            pending->old_value, pending->new_value);
        list_addlink(record->changes, committed);
    }
    iterator_stop(&it);

    /* Prepend (newest first) */
    list_addlink(history->records, record);

    /* Evict oldest if over limit */
    int limit = history->max_records;
    while (limit > 0 && list_size(history->records) > limit) {
        /* Remove from tail (oldest) */
        olc_commit_record_t *oldest = list_nthdata(history->records,
            list_size(history->records));
        if (oldest) {
            list_remlink(history->records, oldest, false);
            olc_commit_record_destroy(oldest);
        }
    }

    history->is_dirty = true;
    return record;
}

olc_commit_record_t *olc_commit_history_find(olc_commit_history_t *history, int id)
{
    if (!history || !history->records) return NULL;

    ITERATOR it;
    iterator_start(&it, history->records);
    olc_commit_record_t *record;
    while ((record = iterator_nextdata(&it)) != NULL) {
        if (record->id == id) {
            iterator_stop(&it);
            return record;
        }
    }
    iterator_stop(&it);
    return NULL;
}

int olc_commit_history_count(olc_commit_history_t *history)
{
    return (history && history->records) ? list_size(history->records) : 0;
}
```

- [ ] **Step 4: Add to build systems**

In `CMakeLists.txt`, add `editors/common/olc_commit_history.c` to the main source list (near the other `editors/common/olc_*.c` entries).

In `Makefile`, add `editors/common/olc_commit_history.c` to `C_FILES` (near the other `editors/common/olc_*.c` entries).

- [ ] **Step 5: Build and verify**

```bash
cd /sentience/src && ./build tests
```

Expected: Clean compile. The new module compiles but nothing calls it yet.

- [ ] **Step 6: Commit**

```bash
git add editors/common/olc_commit_history.h editors/common/olc_commit_history.c CMakeLists.txt Makefile
git commit -m "feat(olc): add commit history data model and CRUD

New olc_commit_history module with:
- olc_committed_change_t, olc_commit_record_t, olc_commit_history_t
- Create/destroy lifecycle for all three levels
- Archive function to copy changeset changes into a commit record
- Lookup by ID, count, eviction of oldest records
- Uses game_settings.olc_history_limit for configurable cap

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 3: History Serialization

Add JSON serialize/deserialize for commit history persistence.

**Files:**
- Modify: `editors/common/olc_commit_history.c`

- [ ] **Step 1: Add serialization functions**

Append serialization code to `olc_commit_history.c`:

```c
/* --- Serialization --- */

static json_t *committed_change_serialize(olc_committed_change_t *change)
{
    json_t *obj = json_object();
    json_object_set_new(obj, "field_path", json_string(change->field_path ? change->field_path : ""));
    json_object_set_new(obj, "field_type", json_integer(change->field_type));
    if (change->old_value)
        json_object_set(obj, "old_value", change->old_value);
    if (change->new_value)
        json_object_set(obj, "new_value", change->new_value);
    return obj;
}

static olc_committed_change_t *committed_change_deserialize(json_t *json)
{
    if (!json_is_object(json)) return NULL;

    json_t *j_fp = json_object_get(json, "field_path");
    json_t *j_ft = json_object_get(json, "field_type");
    if (!json_is_string(j_fp) || !json_is_integer(j_ft))
        return NULL;

    return olc_committed_change_create(
        json_string_value(j_fp),
        (olc_field_type_t)json_integer_value(j_ft),
        json_object_get(json, "old_value"),
        json_object_get(json, "new_value"));
}

json_t *olc_commit_record_serialize(olc_commit_record_t *record)
{
    if (!record) return NULL;

    json_t *obj = json_object();
    json_object_set_new(obj, "id", json_integer(record->id));
    json_object_set_new(obj, "group_id", json_integer(record->group_id));
    json_object_set_new(obj, "author", json_string(record->author ? record->author : ""));
    json_object_set_new(obj, "comment", json_string(record->comment ? record->comment : ""));
    json_object_set_new(obj, "timestamp", json_integer((json_int_t)record->timestamp));

    json_t *changes_arr = json_array();
    if (record->changes) {
        ITERATOR it;
        iterator_start(&it, record->changes);
        olc_committed_change_t *change;
        while ((change = iterator_nextdata(&it)) != NULL)
            json_array_append_new(changes_arr, committed_change_serialize(change));
        iterator_stop(&it);
    }
    json_object_set_new(obj, "changes", changes_arr);
    return obj;
}

static olc_commit_record_t *commit_record_deserialize(json_t *json)
{
    if (!json_is_object(json)) return NULL;

    json_t *j_id = json_object_get(json, "id");
    json_t *j_group = json_object_get(json, "group_id");
    json_t *j_author = json_object_get(json, "author");
    if (!json_is_integer(j_id) || !json_is_integer(j_group) || !json_is_string(j_author))
        return NULL;

    olc_commit_record_t *record = olc_commit_record_create(
        json_string_value(j_author), (int)json_integer_value(j_group));
    record->id = (int)json_integer_value(j_id);

    json_t *j_comment = json_object_get(json, "comment");
    if (json_is_string(j_comment)) {
        free_string(record->comment);
        record->comment = str_dup(json_string_value(j_comment));
    }

    json_t *j_ts = json_object_get(json, "timestamp");
    if (json_is_integer(j_ts))
        record->timestamp = (time_t)json_integer_value(j_ts);

    json_t *changes_arr = json_object_get(json, "changes");
    if (json_is_array(changes_arr)) {
        size_t idx;
        json_t *entry;
        json_array_foreach(changes_arr, idx, entry) {
            olc_committed_change_t *change = committed_change_deserialize(entry);
            if (change)
                list_appendlink(record->changes, change);
        }
    }

    return record;
}

json_t *olc_commit_history_serialize(olc_commit_history_t *history)
{
    if (!history) return NULL;

    json_t *root = json_object();
    json_object_set_new(root, "editor_type", json_integer(history->editor_type));
    json_object_set_new(root, "entity_wnum_auid", json_integer(history->entity_wnum.auid));
    json_object_set_new(root, "entity_wnum_vnum", json_integer(history->entity_wnum.vnum));
    json_object_set_new(root, "next_id", json_integer(history->next_id));

    json_t *records_arr = json_array();
    if (history->records) {
        ITERATOR it;
        iterator_start(&it, history->records);
        olc_commit_record_t *record;
        while ((record = iterator_nextdata(&it)) != NULL)
            json_array_append_new(records_arr, olc_commit_record_serialize(record));
        iterator_stop(&it);
    }
    json_object_set_new(root, "records", records_arr);
    return root;
}

olc_commit_history_t *olc_commit_history_deserialize(json_t *json)
{
    if (!json_is_object(json)) return NULL;

    json_t *j_type = json_object_get(json, "editor_type");
    json_t *j_auid = json_object_get(json, "entity_wnum_auid");
    json_t *j_vnum = json_object_get(json, "entity_wnum_vnum");
    if (!json_is_integer(j_type) || !json_is_integer(j_auid) || !json_is_integer(j_vnum))
        return NULL;

    WNUM_LOAD wnum = {
        .auid = json_integer_value(j_auid),
        .vnum = json_integer_value(j_vnum)
    };
    olc_commit_history_t *history = olc_commit_history_create(
        (int)json_integer_value(j_type), wnum);

    json_t *j_next = json_object_get(json, "next_id");
    if (json_is_integer(j_next))
        history->next_id = (int)json_integer_value(j_next);

    json_t *records_arr = json_object_get(json, "records");
    if (json_is_array(records_arr)) {
        size_t idx;
        json_t *entry;
        json_array_foreach(records_arr, idx, entry) {
            olc_commit_record_t *record = commit_record_deserialize(entry);
            if (record)
                list_appendlink(history->records, record);
        }
    }

    history->is_dirty = false;
    return history;
}
```

- [ ] **Step 2: Build and verify**

```bash
cd /sentience/src && ./build tests
```

- [ ] **Step 3: Commit**

```bash
git add editors/common/olc_commit_history.c
git commit -m "feat(olc): add commit history serialization

JSON serialize/deserialize for commit records and full history.
Follows the same conventions as olc_changeset serialization.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 4: Disk & Redis Persistence

Add file I/O and Redis caching for commit history.

**Files:**
- Modify: `editors/common/olc_commit_history.c`

- [ ] **Step 1: Add disk persistence functions**

Append to `olc_commit_history.c`:

```c
/* --- Persistence helpers --- */

static const char *editor_type_dir_name(int editor_type)
{
    switch (editor_type) {
        case ED_AREA:   return "area";
        case ED_ROOM:   return "room";
        case ED_OBJECT: return "object";
        case ED_MOBILE: return "mobile";
        default:        return "unknown";
    }
}

static void ensure_history_dir(int editor_type)
{
    char path[256];
    snprintf(path, sizeof(path), "%shistory", DATA_DIR);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%shistory/%s", DATA_DIR, editor_type_dir_name(editor_type));
    mkdir(path, 0755);
}

static const char *history_disk_path(int editor_type, WNUM_LOAD wnum)
{
    static char path[256];
    snprintf(path, sizeof(path), "%shistory/%s/%ld_%ld.json",
        DATA_DIR, editor_type_dir_name(editor_type), wnum.auid, wnum.vnum);
    return path;
}

static const char *history_redis_key(int editor_type, WNUM_LOAD wnum)
{
    static char key[128];
    snprintf(key, sizeof(key), "olc:history:%s:%ld:%ld",
        editor_type_dir_name(editor_type), wnum.auid, wnum.vnum);
    return key;
}

/* --- Disk I/O --- */

static bool history_save_to_disk(olc_commit_history_t *history)
{
    if (!history) return false;

    ensure_history_dir(history->editor_type);

    json_t *json = olc_commit_history_serialize(history);
    if (!json) return false;

    const char *path = history_disk_path(history->editor_type, history->entity_wnum);
    int result = json_dump_file(json, path, JSON_INDENT(2) | JSON_SORT_KEYS);
    json_decref(json);

    return (result == 0);
}

static olc_commit_history_t *history_load_from_disk(int editor_type, WNUM_LOAD wnum)
{
    const char *path = history_disk_path(editor_type, wnum);

    json_error_t error;
    json_t *json = json_load_file(path, 0, &error);
    if (!json) return NULL;

    olc_commit_history_t *history = olc_commit_history_deserialize(json);
    json_decref(json);
    return history;
}

/* --- Redis cache --- */

static void history_cache_to_redis(olc_commit_history_t *history)
{
    if (!history || !redis_is_available()) return;

    json_t *json = olc_commit_history_serialize(history);
    if (!json) return;

    char *json_str = json_dumps(json, JSON_COMPACT);
    json_decref(json);
    if (!json_str) return;

    redis_cache_persist_data(
        history_redis_key(history->editor_type, history->entity_wnum),
        json_str);
    free(json_str);
}

static olc_commit_history_t *history_load_from_redis(int editor_type, WNUM_LOAD wnum)
{
    if (!redis_is_available()) return NULL;

    char *json_str = redis_get_persist_data(history_redis_key(editor_type, wnum));
    if (!json_str) return NULL;

    json_error_t error;
    json_t *json = json_loads(json_str, 0, &error);
    free(json_str);
    if (!json) return NULL;

    olc_commit_history_t *history = olc_commit_history_deserialize(json);
    json_decref(json);
    return history;
}

/* --- Public persistence API --- */

olc_commit_history_t *olc_commit_history_load(int editor_type, WNUM_LOAD wnum)
{
    /* Try Redis first */
    olc_commit_history_t *history = history_load_from_redis(editor_type, wnum);
    if (history) return history;

    /* Fall back to disk */
    history = history_load_from_disk(editor_type, wnum);
    if (history) {
        /* Populate Redis cache */
        history_cache_to_redis(history);
    }
    return history;
}

bool olc_commit_history_save(olc_commit_history_t *history)
{
    if (!history) return false;

    bool ok = history_save_to_disk(history);
    if (ok) {
        history_cache_to_redis(history);
        history->is_dirty = false;
    }
    return ok;
}

void olc_commit_history_delete(int editor_type, WNUM_LOAD wnum)
{
    const char *path = history_disk_path(editor_type, wnum);
    unlink(path);

    if (redis_is_available())
        redis_delete_persist_data(history_redis_key(editor_type, wnum));
}

/* --- In-memory cache and lazy loading --- */

olc_commit_history_t *olc_commit_history_get_or_load(int editor_type, WNUM_LOAD wnum)
{
    ensure_loaded_histories();

    /* Check in-memory cache */
    ITERATOR it;
    iterator_start(&it, loaded_histories);
    olc_commit_history_t *history;
    while ((history = iterator_nextdata(&it)) != NULL) {
        if (history->editor_type == editor_type
            && history->entity_wnum.auid == wnum.auid
            && history->entity_wnum.vnum == wnum.vnum) {
            iterator_stop(&it);
            return history;
        }
    }
    iterator_stop(&it);

    /* Try loading from Redis/disk */
    history = olc_commit_history_load(editor_type, wnum);
    if (!history) {
        /* Create fresh */
        history = olc_commit_history_create(editor_type, wnum);
    }

    list_addlink(loaded_histories, history);
    return history;
}

void olc_commit_history_save_all_dirty(void)
{
    if (!loaded_histories) return;

    ITERATOR it;
    iterator_start(&it, loaded_histories);
    olc_commit_history_t *history;
    while ((history = iterator_nextdata(&it)) != NULL) {
        if (history->is_dirty)
            olc_commit_history_save(history);
    }
    iterator_stop(&it);
}
```

- [ ] **Step 2: Build and verify**

```bash
cd /sentience/src && ./build tests
```

- [ ] **Step 3: Commit**

```bash
git add editors/common/olc_commit_history.c
git commit -m "feat(olc): add commit history disk/Redis persistence

Lazy-loaded from disk, cached in Redis. Cache-through pattern:
Redis first, disk fallback, populate cache on miss.
Save on area save / shutdown via olc_commit_history_save_all_dirty().

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 5: Unit Tests for History Model

Write tests for the core data model, serialization, and eviction.

**Files:**
- Create: `tests/unit/olc_commit_history_tests.c`
- Create: `tests/data/unit/olc_commit_history_tests.json`
- Modify: `tests/framework/test_dispatcher.c` (register `olchist_` handler)
- Modify: `tests/framework/test_modules.h` (declare handler)
- Modify: `tests/data/test_config.json` (add suite)
- Modify: `CMakeLists.txt` (BUILD_TESTS section)
- Modify: `Makefile` (BUILD_TESTS section)

- [ ] **Step 1: Create test file with lifecycle and archive tests**

Create `tests/unit/olc_commit_history_tests.c`:

```c
#ifdef BUILD_TESTS

#include <string.h>
#include "../framework/test_framework.h"
#include "../../merc.h"
#include "../../editors/common/olc_commit_history.h"
#include "../../editors/common/olc_changeset.h"

/* --- olchist_create_destroy --- */
static test_result_t test_olchist_create_destroy(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 1, .vnum = 100 };
    olc_commit_history_t *history = olc_commit_history_create(ED_OBJECT, wnum);

    TEST_ASSERT_NOT_NULL(history);
    TEST_ASSERT_INT_EQ(ED_OBJECT, history->editor_type);
    TEST_ASSERT_INT_EQ(1, (int)history->entity_wnum.auid);
    TEST_ASSERT_INT_EQ(100, (int)history->entity_wnum.vnum);
    TEST_ASSERT_INT_EQ(0, olc_commit_history_count(history));
    TEST_ASSERT_INT_EQ(1, history->next_id);
    TEST_ASSERT_FALSE(history->is_dirty);

    olc_commit_history_destroy(history);
    return TEST_SUCCESS;
}

/* --- olchist_archive --- */
static test_result_t test_olchist_archive(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 1, .vnum = 200 };
    olc_commit_history_t *history = olc_commit_history_create(ED_OBJECT, wnum);
    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT, wnum, "Test Obj", "Builder");

    olc_changeset_add_change(cs, "Name", OLC_FIELD_STRING,
        json_string("old name"), json_string("new name"));
    olc_changeset_add_change(cs, "Weight", OLC_FIELD_INT16,
        json_integer(10), json_integer(20));

    olc_commit_record_t *record = olc_commit_history_archive(history, cs, 0, "test commit");

    TEST_ASSERT_NOT_NULL(record);
    TEST_ASSERT_INT_EQ(1, record->id);
    TEST_ASSERT_INT_EQ(0, record->group_id);
    TEST_ASSERT_STR_EQ("Builder", record->author);
    TEST_ASSERT_STR_EQ("test commit", record->comment);
    TEST_ASSERT_INT_EQ(2, list_size(record->changes));
    TEST_ASSERT_INT_EQ(1, olc_commit_history_count(history));
    TEST_ASSERT_TRUE(history->is_dirty);
    TEST_ASSERT_INT_EQ(2, history->next_id);

    /* Verify lookup */
    olc_commit_record_t *found = olc_commit_history_find(history, 1);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_INT_EQ(1, found->id);

    TEST_ASSERT_NULL(olc_commit_history_find(history, 999));

    olc_changeset_destroy(cs);
    olc_commit_history_destroy(history);
    return TEST_SUCCESS;
}

/* --- olchist_eviction --- */
static test_result_t test_olchist_eviction(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 1, .vnum = 300 };
    olc_commit_history_t *history = olc_commit_history_create(ED_ROOM, wnum);
    history->max_records = 3;

    /* Archive 5 changesets — only newest 3 should survive */
    for (int i = 0; i < 5; i++) {
        olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Room", "Builder");
        char field[32];
        snprintf(field, sizeof(field), "Field_%d", i);
        olc_changeset_add_change(cs, field, OLC_FIELD_INT,
            json_integer(i), json_integer(i + 10));
        olc_commit_history_archive(history, cs, 0, NULL);
        olc_changeset_destroy(cs);
    }

    TEST_ASSERT_INT_EQ(3, olc_commit_history_count(history));

    /* Newest should be ID 5, oldest should be ID 3 */
    TEST_ASSERT_NOT_NULL(olc_commit_history_find(history, 5));
    TEST_ASSERT_NOT_NULL(olc_commit_history_find(history, 4));
    TEST_ASSERT_NOT_NULL(olc_commit_history_find(history, 3));
    TEST_ASSERT_NULL(olc_commit_history_find(history, 2));
    TEST_ASSERT_NULL(olc_commit_history_find(history, 1));

    olc_commit_history_destroy(history);
    return TEST_SUCCESS;
}

/* --- olchist_serialize_roundtrip --- */
static test_result_t test_olchist_serialize_roundtrip(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 42 };
    olc_commit_history_t *history = olc_commit_history_create(ED_MOBILE, wnum);

    olc_changeset_t *cs = olc_changeset_create(ED_MOBILE, wnum, "Mob", "Tester");
    olc_changeset_add_change(cs, "Name", OLC_FIELD_STRING,
        json_string("goblin"), json_string("cave goblin"));
    olc_changeset_add_change(cs, "Level", OLC_FIELD_INT,
        json_integer(5), json_integer(10));
    olc_commit_history_archive(history, cs, 42, "first edit");
    olc_changeset_destroy(cs);

    cs = olc_changeset_create(ED_MOBILE, wnum, "Mob", "Tester2");
    olc_changeset_add_change(cs, "Hitroll", OLC_FIELD_INT16,
        json_integer(3), json_integer(7));
    olc_commit_history_archive(history, cs, 0, NULL);
    olc_changeset_destroy(cs);

    /* Serialize */
    json_t *json = olc_commit_history_serialize(history);
    TEST_ASSERT_NOT_NULL(json);

    /* Deserialize */
    olc_commit_history_t *restored = olc_commit_history_deserialize(json);
    json_decref(json);
    TEST_ASSERT_NOT_NULL(restored);

    TEST_ASSERT_INT_EQ(ED_MOBILE, restored->editor_type);
    TEST_ASSERT_INT_EQ(5, (int)restored->entity_wnum.auid);
    TEST_ASSERT_INT_EQ(42, (int)restored->entity_wnum.vnum);
    TEST_ASSERT_INT_EQ(3, restored->next_id);
    TEST_ASSERT_INT_EQ(2, olc_commit_history_count(restored));

    /* Check first record */
    olc_commit_record_t *r1 = olc_commit_history_find(restored, 1);
    TEST_ASSERT_NOT_NULL(r1);
    TEST_ASSERT_STR_EQ("Tester", r1->author);
    TEST_ASSERT_STR_EQ("first edit", r1->comment);
    TEST_ASSERT_INT_EQ(42, r1->group_id);
    TEST_ASSERT_INT_EQ(2, list_size(r1->changes));

    /* Check change values survived roundtrip */
    olc_committed_change_t *c1 = list_nthdata(r1->changes, 1);
    TEST_ASSERT_NOT_NULL(c1);
    TEST_ASSERT_STR_EQ("Name", c1->field_path);
    TEST_ASSERT_INT_EQ(OLC_FIELD_STRING, c1->field_type);
    TEST_ASSERT_STR_EQ("goblin", json_string_value(c1->old_value));
    TEST_ASSERT_STR_EQ("cave goblin", json_string_value(c1->new_value));

    /* Verify list ordering: records should be newest-first after roundtrip.
     * Record 2 (id=2) was added second, so it should appear first in iteration. */
    ITERATOR it;
    iterator_start(&it, restored->records);
    olc_commit_record_t *first = iterator_nextdata(&it);
    olc_commit_record_t *second = iterator_nextdata(&it);
    iterator_stop(&it);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_INT_EQ(2, first->id);  /* newest first */
    TEST_ASSERT_INT_EQ(1, second->id); /* oldest last */

    olc_commit_history_destroy(history);
    olc_commit_history_destroy(restored);
    return TEST_SUCCESS;
}

/* --- olchist_group_id --- */
static test_result_t test_olchist_group_id(test_case_t *test)
{
    WNUM_LOAD wnum1 = { .auid = 1, .vnum = 10 };
    WNUM_LOAD wnum2 = { .auid = 1, .vnum = 20 };

    olc_commit_history_t *h1 = olc_commit_history_create(ED_OBJECT, wnum1);
    olc_commit_history_t *h2 = olc_commit_history_create(ED_OBJECT, wnum2);

    int gid = olc_next_group_id++;

    olc_changeset_t *cs1 = olc_changeset_create(ED_OBJECT, wnum1, "Obj1", "Builder");
    olc_changeset_add_change(cs1, "Name", OLC_FIELD_STRING,
        json_string("a"), json_string("b"));
    olc_commit_record_t *r1 = olc_commit_history_archive(h1, cs1, gid, NULL);

    olc_changeset_t *cs2 = olc_changeset_create(ED_OBJECT, wnum2, "Obj2", "Builder");
    olc_changeset_add_change(cs2, "Name", OLC_FIELD_STRING,
        json_string("c"), json_string("d"));
    olc_commit_record_t *r2 = olc_commit_history_archive(h2, cs2, gid, NULL);

    TEST_ASSERT_INT_EQ(gid, r1->group_id);
    TEST_ASSERT_INT_EQ(gid, r2->group_id);
    TEST_ASSERT_INT_EQ(r1->group_id, r2->group_id);

    olc_changeset_destroy(cs1);
    olc_changeset_destroy(cs2);
    olc_commit_history_destroy(h1);
    olc_commit_history_destroy(h2);
    return TEST_SUCCESS;
}

/* --- olchist_empty_changeset --- */
static test_result_t test_olchist_empty_changeset(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 1, .vnum = 50 };
    olc_commit_history_t *history = olc_commit_history_create(ED_AREA, wnum);
    olc_changeset_t *cs = olc_changeset_create(ED_AREA, wnum, "Area", "Builder");

    /* Empty changeset should not archive */
    olc_commit_record_t *record = olc_commit_history_archive(history, cs, 0, NULL);
    TEST_ASSERT_NULL(record);
    TEST_ASSERT_INT_EQ(0, olc_commit_history_count(history));
    TEST_ASSERT_FALSE(history->is_dirty);

    olc_changeset_destroy(cs);
    olc_commit_history_destroy(history);
    return TEST_SUCCESS;
}

/* --- Dispatcher --- */

typedef struct {
    const char *test_type;
    test_result_t (*handler)(test_case_t *);
} olchist_handler_entry_t;

static const olchist_handler_entry_t olchist_handlers[] = {
    { "olchist_create_destroy",     test_olchist_create_destroy },
    { "olchist_archive",            test_olchist_archive },
    { "olchist_eviction",           test_olchist_eviction },
    { "olchist_serialize_roundtrip", test_olchist_serialize_roundtrip },
    { "olchist_group_id",           test_olchist_group_id },
    { "olchist_empty_changeset",    test_olchist_empty_changeset },
    { NULL, NULL }
};

test_result_t run_olc_commit_history_test_case(test_case_t *test)
{
    if (!test || !test->test_type) return TEST_ERROR;

    for (int i = 0; olchist_handlers[i].test_type; i++) {
        if (!strcmp(test->test_type, olchist_handlers[i].test_type))
            return olchist_handlers[i].handler(test);
    }

    log_test(test, "Unknown olchist test type: %s", test->test_type);
    return TEST_ERROR;
}

#endif /* BUILD_TESTS */
```

- [ ] **Step 2: Create JSON test definitions**

Create `tests/data/unit/olc_commit_history_tests.json`:

```json
{
    "suite_name": "olc_commit_history_tests",
    "description": "Unit tests for OLC commit history data model",
    "tests": [
        {
            "name": "Create and destroy history",
            "test_type": "olchist_create_destroy",
            "expected_result": "PASS"
        },
        {
            "name": "Archive changeset into history",
            "test_type": "olchist_archive",
            "expected_result": "PASS"
        },
        {
            "name": "Eviction of oldest records",
            "test_type": "olchist_eviction",
            "expected_result": "PASS"
        },
        {
            "name": "Serialize and deserialize roundtrip",
            "test_type": "olchist_serialize_roundtrip",
            "expected_result": "PASS"
        },
        {
            "name": "Group ID shared across entities",
            "test_type": "olchist_group_id",
            "expected_result": "PASS"
        },
        {
            "name": "Empty changeset not archived",
            "test_type": "olchist_empty_changeset",
            "expected_result": "PASS"
        }
    ]
}
```

- [ ] **Step 3: Register in test infrastructure**

In `tests/framework/test_modules.h`, add:
```c
test_result_t run_olc_commit_history_test_case(test_case_t *test);
```

In `tests/framework/test_dispatcher.c`, add to `handler_table`:
```c
    { "olchist_",                        run_olc_commit_history_test_case,  MATCH_SUBSTR },
```

In `tests/data/test_config.json`, add `"olc_commit_history_tests"` to both the `"suites"` array and the unit tests `"suites"` list.

- [ ] **Step 4: Add test file to build systems**

In `CMakeLists.txt` BUILD_TESTS section, add:
```
tests/unit/olc_commit_history_tests.c
```

In `Makefile` BUILD_TESTS section, add:
```
tests/unit/olc_commit_history_tests.c
```

- [ ] **Step 5: Build and run tests**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test:olchist_
```

Expected: 6 tests, all PASS.

- [ ] **Step 6: Run full test suite to check for regressions**

```bash
cd /sentience && ./sent -test
```

Expected: All existing tests still pass. New total includes 6 olchist_ tests.

- [ ] **Step 7: Commit**

```bash
git add tests/unit/olc_commit_history_tests.c tests/data/unit/olc_commit_history_tests.json \
    tests/framework/test_dispatcher.c tests/framework/test_modules.h tests/data/test_config.json \
    CMakeLists.txt Makefile
git commit -m "test(olc): add commit history unit tests

6 tests covering: create/destroy, archive, eviction, serialization
roundtrip, group ID sharing, empty changeset rejection.
Prefix: olchist_

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 6: Hook History Archival into Commit

Wire the history module into the existing commit flow.

**Files:**
- Modify: `editors/common/olc_staged.c`
- Modify: `editors/common/olc_staged.h`

- [ ] **Step 1: Add include and helper to olc_staged.c**

At the top of `olc_staged.c`, add:
```c
#include "olc_commit_history.h"
```

Add a helper function before `olc_staged_cmd_commit()`:

```c
static void archive_changeset_to_history(olc_changeset_t *cs, int group_id, const char *comment)
{
    if (!cs || olc_changeset_count(cs) == 0) return;

    olc_commit_history_t *history = olc_commit_history_get_or_load(
        cs->editor_type, cs->entity_wnum);
    if (!history) return;

    olc_commit_history_archive(history, cs, group_id, comment);
}
```

- [ ] **Step 2: Hook into olc_staged_cmd_commit()**

**CRITICAL:** `olc_changeset_commit()` calls `olc_changeset_clear(cs)` on success (line 202 of `olc_field_handlers.c`), which empties the changes list. Therefore, the archive call MUST come BEFORE the commit call.

In `olc_staged_cmd_commit()`, insert the archive call BEFORE the existing `olc_changeset_commit()` call:

```c
    /* Archive to history BEFORE commit (commit clears the changeset) */
    archive_changeset_to_history(cs, 0, argument);

    const char *error_field = NULL;
    int applied = olc_changeset_commit(cs, pEdit,
        def->field_handlers, &error_field);

    if (applied < 0) {
        /* Commit failed — but we already archived. This is acceptable:
         * the archive records the attempt. The user will see the error
         * and can fix and re-commit. */
```

The existing code after the commit check remains unchanged.

- [ ] **Step 3: Hook into olc_staged_cmd_commit_group()**

In `olc_staged_cmd_commit_group()`, before the iterator loop, allocate a group ID:

```c
    int group_id = olc_next_group_id++;
```

Inside the loop, insert the archive BEFORE each `olc_changeset_commit()`:

```c
        archive_changeset_to_history(cs, group_id, argument);

        const char *error_field = NULL;
        int applied = olc_changeset_commit(cs, ch->desc->pEdit,
            edef->field_handlers, &error_field);
```

- [ ] **Step 4: Build and verify**

```bash
cd /sentience/src && ./build tests
```

- [ ] **Step 5: Test manually** (if possible)

Start the MUD, enter oedit, make a change, commit. The change should be archived to history. Check `data/history/object/` for the JSON file.

- [ ] **Step 6: Commit**

```bash
git add editors/common/olc_staged.c
git commit -m "feat(olc): archive changesets to history on commit

Every commit/commit group now archives the changeset into the
per-entity commit history before changes are applied/cleared.
Group commits share a group_id for cross-entity linking.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 7: History Commands

Implement the `history`, `history <id>`, and `history revert` commands.

**Files:**
- Modify: `editors/common/olc_staged.c` (add command implementations)
- Modify: `editors/common/olc_staged.h` (declare new functions)
- Modify: `editors/common/olc_editor.c` (update dispatch)

- [ ] **Step 1: Declare history command functions in olc_staged.h**

Add to `olc_staged.h`:

```c
/* History commands */
void olc_staged_cmd_history(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
    void *pEdit, char *argument);
```

- [ ] **Step 2: Implement history list command**

Add to `olc_staged.c`:

```c
/* =========================================================================
 * History Commands
 * ========================================================================= */

/* NOTE: Do NOT add #include "olc_commit_history.h" here — it was already
 * added in Task 6 Step 1. Also, do NOT define get_entity_wnum_from_edit() —
 * use the existing olc_get_entity_wnum(def, pEdit) from olc_editor.h. */

static void show_history_list(CHAR_DATA *ch, olc_commit_history_t *history,
    const char *argument)
{
    int limit = olc_commit_history_count(history);
    if (!IS_NULLSTR(argument) && is_number(argument))
        limit = UMIN(atoi(argument), limit);

    if (limit == 0) {
        send_to_char("No commit history for this entity.\n\r", ch);
        return;
    }

    BUFFER *buffer = new_buf();
    char buf[MSL];

    snprintf(buf, sizeof(buf),
        "{Y+------+--------------------+-------+-------------------------------+{x\n\r");
    add_buf(buffer, buf);
    snprintf(buf, sizeof(buf),
        "{Y| {WID{x   | {WAuthor{x             | {W# Chg{x | {WDate & Time{x                  |{x\n\r");
    add_buf(buffer, buf);
    snprintf(buf, sizeof(buf),
        "{Y+------+--------------------+-------+-------------------------------+{x\n\r");
    add_buf(buffer, buf);

    int shown = 0;
    ITERATOR it;
    iterator_start(&it, history->records);
    olc_commit_record_t *record;
    while ((record = iterator_nextdata(&it)) != NULL && shown < limit) {
        char time_buf[64];
        struct tm *tm_info = localtime(&record->timestamp);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

        snprintf(buf, sizeof(buf),
            "{Y| {W%-4d{x | %-18.18s | {W%-5d{x | %-29s |{x%s\n\r",
            record->id, record->author,
            list_size(record->changes), time_buf,
            record->group_id > 0 ? " {D[group]{x" : "");
        add_buf(buffer, buf);
        shown++;
    }
    iterator_stop(&it);

    snprintf(buf, sizeof(buf),
        "{Y+------+--------------------+-------+-------------------------------+{x\n\r");
    add_buf(buffer, buf);

    snprintf(buf, sizeof(buf),
        "\n\rUse '{Whistory <id>{x' for details. '{Whistory revert <id>{x' to undo.\n\r");
    add_buf(buffer, buf);

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}

static void show_history_detail(CHAR_DATA *ch, olc_commit_history_t *history,
    int id)
{
    olc_commit_record_t *record = olc_commit_history_find(history, id);
    if (!record) {
        printf_to_char(ch, "No commit record with ID %d.\n\r", id);
        return;
    }

    char time_buf[64];
    struct tm *tm_info = localtime(&record->timestamp);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    printf_to_char(ch, "{WCommit #%d{x by %s on %s%s\n\r",
        record->id, record->author, time_buf,
        record->group_id > 0 ? formatf(" {D[group %d]{x", record->group_id) : "");

    if (!IS_NULLSTR(record->comment))
        printf_to_char(ch, "{DComment:{x %s\n\r", record->comment);

    printf_to_char(ch, "\n\r{D%-25s %-8s %-20s %-20s{x\n\r",
        "Field", "Type", "Old", "New");
    printf_to_char(ch, "{D%.73s{x\n\r",
        "-------------------------------------------------------------------------");

    ITERATOR it;
    iterator_start(&it, record->changes);
    olc_committed_change_t *change;
    while ((change = iterator_nextdata(&it)) != NULL) {
        char old_buf[64], new_buf[64];
        format_json_brief(old_buf, sizeof(old_buf), change->old_value);
        format_json_brief(new_buf, sizeof(new_buf), change->new_value);

        printf_to_char(ch, " {Y%-24s{x %-8s %-20s {W%-20s{x\n\r",
            change->field_path,
            field_type_label(change->field_type),
            old_buf, new_buf);
    }
    iterator_stop(&it);

    printf_to_char(ch, "\n\r{x%d change%s. Use '{Whistory revert %d{x' to undo.\n\r",
        list_size(record->changes),
        list_size(record->changes) == 1 ? "" : "s",
        record->id);
}
```

Note: `format_json_brief` and `field_type_label` are already static in `olc_staged.c`. If they need to be used by the history detail view, either move them above or make them non-static. Since the history commands are in the same file, they are already accessible.

- [ ] **Step 3: Implement history revert command**

Add to `olc_staged.c`:

```c
static void do_history_revert(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
    void *pEdit, olc_commit_history_t *history, char *argument)
{
    char arg_id[MIL], arg_confirm[MIL];
    argument = one_argument(argument, arg_id);
    argument = one_argument(argument, arg_confirm);

    if (!is_number(arg_id)) {
        send_to_char("Syntax: history revert <id> [confirm]\n\r", ch);
        return;
    }

    int id = atoi(arg_id);
    olc_commit_record_t *record = olc_commit_history_find(history, id);
    if (!record) {
        printf_to_char(ch, "No commit record with ID %d.\n\r", id);
        return;
    }

    /* Preview mode: show what would change */
    if (str_cmp(arg_confirm, "confirm")) {
        printf_to_char(ch, "{WRevert preview for commit #%d{x by %s:\n\r\n\r",
            record->id, record->author);

        printf_to_char(ch, "{D%-25s %-20s %-20s{x\n\r",
            "Field", "Current (new)", "Revert to (old)");
        printf_to_char(ch, "{D%.65s{x\n\r",
            "-----------------------------------------------------------------");

        int revertable = 0;
        int skipped = 0;
        ITERATOR it;
        iterator_start(&it, record->changes);
        olc_committed_change_t *change;
        while ((change = iterator_nextdata(&it)) != NULL) {
            /* Phase 1: only scalar types are revertable */
            bool can_revert = (change->field_type <= OLC_FIELD_MULTIFLAGS
                || change->field_type == OLC_FIELD_MULTILINE);
            char cur_buf[64], old_buf[64];
            format_json_brief(cur_buf, sizeof(cur_buf), change->new_value);
            format_json_brief(old_buf, sizeof(old_buf), change->old_value);

            if (can_revert) {
                printf_to_char(ch, " {Y%-24s{x %-20s → {W%-20s{x\n\r",
                    change->field_path, cur_buf, old_buf);
                revertable++;
            } else {
                printf_to_char(ch, " {D%-24s %-20s   (skip: %s){x\n\r",
                    change->field_path, cur_buf,
                    field_type_label(change->field_type));
                skipped++;
            }
        }
        iterator_stop(&it);

        printf_to_char(ch, "\n\r%d field%s will be reverted",
            revertable, revertable == 1 ? "" : "s");
        if (skipped > 0)
            printf_to_char(ch, ", %d skipped (non-scalar)", skipped);
        printf_to_char(ch, ".\n\rType '{Whistory revert %d confirm{x' to proceed.\n\r", id);
        return;
    }

    /* Confirmed: stage reverse changes, then let olc_staged_cmd_commit handle
     * everything (archival, commit, GMCP, area flag). This avoids duplicating
     * logic and guarantees the revert is recorded in history. */
    olc_changeset_t *cs = olc_get_active_changeset(ch, def);
    if (!cs) {
        send_to_char("No active changeset — cannot revert.\n\r", ch);
        return;
    }

    if (olc_changeset_count(cs) > 0) {
        send_to_char("{RYou have pending changes.{x Commit or revert them first.\n\r", ch);
        return;
    }

    int reverted = 0;
    int skipped = 0;
    ITERATOR it;
    iterator_start(&it, record->changes);
    olc_committed_change_t *change;
    while ((change = iterator_nextdata(&it)) != NULL) {
        bool can_revert = (change->field_type <= OLC_FIELD_MULTIFLAGS
            || change->field_type == OLC_FIELD_MULTILINE);
        if (!can_revert) {
            skipped++;
            continue;
        }

        /* Stage the reverse: new_value becomes old, old_value becomes new */
        olc_changeset_add_change(cs, change->field_path, change->field_type,
            change->new_value, change->old_value);
        reverted++;
    }
    iterator_stop(&it);

    if (reverted == 0) {
        send_to_char("No revertable fields in this commit.\n\r", ch);
        return;
    }

    /* Delegate to olc_staged_cmd_commit() which handles:
     * archive → commit → area flag → GMCP notification */
    char revert_comment[MIL];
    snprintf(revert_comment, sizeof(revert_comment), "Revert of commit #%d", id);
    olc_staged_cmd_commit(ch, def, pEdit, revert_comment);

    if (skipped > 0)
        printf_to_char(ch, "{D(%d non-scalar field%s skipped){x\n\r",
            skipped, skipped == 1 ? "" : "s");
}
```

- [ ] **Step 4: Implement the main dispatch function**

Add to `olc_staged.c`:

```c
void olc_staged_cmd_history(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
    void *pEdit, char *argument)
{
    WNUM_LOAD wnum = olc_get_entity_wnum(def, pEdit);
    olc_commit_history_t *history = olc_commit_history_get_or_load(
        def->editor_type, wnum);

    if (!history || olc_commit_history_count(history) == 0) {
        if (IS_NULLSTR(argument) || is_number(argument)) {
            send_to_char("No commit history for this entity.\n\r", ch);
            return;
        }
    }

    char arg1[MIL];
    char *rest = one_argument(argument, arg1);

    /* "history revert <id> [confirm]" */
    if (!str_cmp(arg1, "revert")) {
        do_history_revert(ch, def, pEdit, history, rest);
        return;
    }

    /* "history <id>" — detail view */
    if (is_number(arg1)) {
        show_history_detail(ch, history, atoi(arg1));
        return;
    }

    /* "history" or "history <count>" — list view */
    show_history_list(ch, history, argument);
}
```

- [ ] **Step 5: Update dispatch in olc_editor.c**

In `olc_editor.c`, in the staged-mode command dispatch block (around line 954-984), add a `history` handler INSIDE the `if (def->change_mode == OLC_CHANGE_STAGED)` block, BEFORE the existing `history` handler outside it:

```c
        if (!str_cmp(command, "history")) {
            olc_staged_cmd_history(ch, def, ch->desc->pEdit, rest);
            return;
        }
        if (!str_cmp(command, "view")) {
            /* "view <id>" is alias for "history <id>" in staged mode */
            if (!IS_NULLSTR(rest) && is_number(rest)) {
                char hist_arg[MIL];
                snprintf(hist_arg, sizeof(hist_arg), "%s", rest);
                olc_staged_cmd_history(ch, def, ch->desc->pEdit, hist_arg);
            } else {
                send_to_char("Syntax: view <commit_id>  (alias for: history <id>)\n\r", ch);
            }
            return;
        }
```

This ensures staged editors use the new commit-level history, while non-staged editors still fall through to the old `OLC_CHANGE_HISTORY` system.

- [ ] **Step 6: Build and verify**

```bash
cd /sentience/src && ./build tests && ./install debug
```

- [ ] **Step 7: Run tests**

```bash
cd /sentience && ./sent -test:olchist_ && ./sent -test:olccs_
```

Expected: All history and changeset tests pass.

- [ ] **Step 8: Commit**

```bash
git add editors/common/olc_staged.c editors/common/olc_staged.h editors/common/olc_editor.c
git commit -m "feat(olc): add history, history <id>, history revert commands

Staged editors now have commit-level history:
- 'history' shows table of past commits
- 'history <id>' shows field-by-field detail
- 'history revert <id>' previews, 'history revert <id> confirm' applies
- 'view <id>' aliases to 'history <id>' in staged mode
- Revert creates forward commit (auditable, never deletes history)
- Phase 1: only scalar/multiline fields are revertable

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 8: Save History on Area Save / Shutdown

Wire dirty history saves into the existing save triggers.

**Files:**
- Modify: `editors/common/olc_editor.c` (or appropriate save hook location)

- [ ] **Step 1: Find area save hook**

Search for where `AREA_CHANGED` flag triggers saves. Look in `olc_save.c` or `save.c` for the area save loop. The history save-all-dirty call should be added alongside area saves.

```bash
grep -rn "AREA_CHANGED\|save_area\|do_asave" olc_save.c save.c db.c
```

- [ ] **Step 2: Add save hook**

Add `olc_commit_history_save_all_dirty()` call at the appropriate location — typically:
1. In `do_asave` (manual area save command)
2. In the auto-save tick (if areas are auto-saved periodically)
3. In shutdown/copyover

Add `#include "editors/common/olc_commit_history.h"` to the file where the hook is added.

- [ ] **Step 3: Build and verify**

```bash
cd /sentience/src && ./build tests
```

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat(olc): save dirty commit histories on area save/shutdown

Calls olc_commit_history_save_all_dirty() alongside area saves
to persist commit history to disk and update Redis cache.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 9: Convert Simple Scalar Commands — oedit

Convert unstaged oedit commands to use `olc_cmd_*` helpers and add field handlers.

**Files:**
- Modify: `editors/objects/oedit.c`

### Target Commands

| Command | Current | Convert To | Field | Handler Macro |
|---------|---------|-----------|-------|---------------|
| `oedit_short` | `free_string + str_dup` | `olc_cmd_string` | short_descr (char*) | `OLC_FIELD_APPLY_STRING` |
| `oedit_long` | `free_string + str_dup` | `olc_cmd_string` | description (char*) | `OLC_FIELD_APPLY_STRING` |
| `oedit_material` | `free_string + str_dup` | `olc_cmd_string` | material (char*) | `OLC_FIELD_APPLY_STRING` |

**Deferred from Phase 1:** `oedit_cost` (`pObj->cost` is `long`, not `int` — needs `olc_cmd_number_long` or a local-var workaround), `oedit_fragility` (named option → int16 mapping, needs custom dispatch), `oedit_level` (recalculates points/armor/weapon dice), `oedit_type` (restructures type data), `oedit_parent` (multi-field widevnum), `oedit_lock` (multi-field), `oedit_wear` (constrained flags), `oedit_persist` (calls persist_addobj/removeobj), `oedit_sign` (sets to ch->name, not argument)

- [ ] **Step 1: Convert oedit_short**

The old function sets `pObj->short_descr` and optionally auto-generates name keywords. For staging:

```c
OEDIT(oedit_short)
{
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);

    if (!olc_cmd_string(ch, argument, "Short", NULL,
            &pObj->short_descr, 0, NULL, NULL))
        return false;

    /* Auto-set keywords from short description (if PLR_AUTOSETNAME) */
    if (IS_SET(ch->act[0], PLR_AUTOSETNAME)) {
        char *keywords = NULL;
        char *invalid = NULL;
        LOCALIZATION_ERROR err = localization_short_to_keywords(argument, &keywords, &invalid);
        if (err == LOC_OK && !IS_NULLSTR(keywords)) {
            olc_cmd_string(ch, keywords, "Name", NULL,
                &pObj->name, 0, NULL, NULL);
            free(keywords);
        } else {
            send_to_char("{RNone of the short description could be applied to the name.{x\n\r", ch);
            if (keywords) free(keywords);
        }
        if (invalid) {
            printf_to_char(ch, "{DSkipped noise words: %s{x\n\r", invalid);
            free(invalid);
        }
    }
    return true;
}
```

Add handler: `{ "Short", OLC_FIELD_STRING, NULL, oedit_apply_short, NULL }` with `OLC_FIELD_APPLY_STRING(oedit_apply_short, OBJ_INDEX_DATA, short_descr)`.

**Signature note:** `olc_cmd_string(ch, argument, label, syntax, field_ptr, str_flags, ctx, record_fn)` — 8 params total. Pass `0` for str_flags, `NULL` for ctx and record_fn.

- [ ] **Step 2: Convert oedit_long**

```c
OEDIT(oedit_long)
{
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);

    /* Pre-process: uppercase first letter */
    char processed[MSL];
    strlcpy(processed, argument, sizeof(processed));
    if (processed[0] != '\0')
        processed[0] = UPPER(processed[0]);

    return olc_cmd_string(ch, processed, "Long", NULL,
        &pObj->description, 0, NULL, NULL);
}
```

Add handler: `{ "Long", OLC_FIELD_STRING, NULL, oedit_apply_long, NULL }` with `OLC_FIELD_APPLY_STRING(oedit_apply_long, OBJ_INDEX_DATA, description)`.

- [ ] **Step 3: Convert oedit_material**

The old function does `material_lookup()` validation before assignment. Keep the validation, stage with `olc_cmd_string`:

```c
OEDIT(oedit_material)
{
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax:  material [material-name]\n\r", ch);
        return false;
    }

    int num = material_lookup(argument);
    if (num < 0) {
        send_to_char("Invalid material.\n\r", ch);
        return false;
    }

    const char *name = material_name(num);
    return olc_cmd_string(ch, (char *)name, "Material", NULL,
        &pObj->material, 0, NULL, NULL);
}
```

Add handler: `{ "Material", OLC_FIELD_STRING, NULL, oedit_apply_material, NULL }` with `OLC_FIELD_APPLY_STRING(oedit_apply_material, OBJ_INDEX_DATA, material)`.

- [ ] **Step 4: Build and run tests**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test:olccs_
```

Expected: All existing tests pass. The converted commands now stage changes.

- [ ] **Step 5: Commit**

```bash
git add editors/objects/oedit.c
git commit -m "feat(olc): convert oedit scalar commands to staged mode

Convert oedit_short, oedit_long, oedit_material to
use olc_cmd_* helpers. Add corresponding field handlers.
Side effects (auto-name from short, uppercase long) preserved.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 10: Convert Simple Scalar Commands — medit

Convert unstaged medit commands to use `olc_cmd_*` helpers.

**Files:**
- Modify: `editors/mobiles/medit.c`

### Target Commands

| Command | Current | Convert To | Field |
|---------|---------|-----------|-------|
| `medit_name` | `free_string + str_dup` | `olc_cmd_string` | player_name |
| `medit_short` | `free_string + str_dup` | `olc_cmd_string` | short_descr |
| `medit_long` | `free_string + str_dup` | `olc_cmd_string` | long_descr |
| `medit_skeywds` | `free_string + str_dup` | `olc_cmd_string` | skeywds |
| `medit_damtype` | `attack_lookup` | `olc_cmd_type_set` | dam_type |
| `medit_attacks` | `pMob->attacks = value` | `olc_cmd_number` | attacks |

**Deferred:** `medit_race` (10+ side effects), `medit_position` (two fields), `medit_ac` (4 values), `medit_hitdice/manadice/damdice` (3-component dice), `medit_affect/act` (dual-word multiflags), `medit_corpsevnum/zombievnum/parent` (widevnum), `medit_persist` (persist_addmob), `medit_spec` (function pointer), `medit_sign` (ch->name pattern)

- [ ] **Step 1: Convert medit_name**

Replace body with `olc_cmd_string` call. Keep existing validation (null check, length limits). Add handler.

**Signature reminder:** `olc_cmd_string(ch, argument, label, syntax, &field_ptr, str_flags, ctx, record_fn)` — use `0, NULL, NULL` for last 3 args.

- [ ] **Step 2: Convert medit_short**

Similar to oedit_short — stage the short desc, then auto-generate name if needed. Stage both as separate changes.

- [ ] **Step 3: Convert medit_long**

Uppercase first letter before staging. Add handler for `OLC_FIELD_APPLY_STRING(medit_apply_long, MOB_INDEX_DATA, long_descr)`.

- [ ] **Step 4: Convert medit_skeywds, medit_damtype, medit_attacks**

Straightforward conversions. `medit_damtype` uses `attack_lookup()` for validation before `olc_cmd_type_set`. `medit_attacks` uses `olc_cmd_number` — **9 params:** `olc_cmd_number(ch, argument, label, syntax, &field_ptr, min, max, ctx, record_fn)`, use `NULL, NULL` for last 2.

Add all field handlers.

- [ ] **Step 5: Build and run tests**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test:olccs_ && ./sent -test:olchist_
```

- [ ] **Step 6: Commit**

```bash
git add editors/mobiles/medit.c
git commit -m "feat(olc): convert medit scalar commands to staged mode

Convert medit_name, medit_short, medit_long, medit_skeywds,
medit_damtype, medit_attacks to olc_cmd_* helpers.
Add field handlers for all converted fields.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 11: Convert Simple Scalar Commands — aedit (and redit where possible)

**Files:**
- Modify: `editors/areas/aedit.c`

### redit Targets

| Command | Convert To | Field | Notes |
|---------|-----------|-------|-------|
| ~~`redit_locale`~~ | ~~`olc_cmd_number`~~ | ~~locale (long)~~ | **Deferred:** `pRoom->locale` is `long`, not `int`. Needs `olc_cmd_number_long` (Phase 2) |

**Deferred:** `redit_sector` (uses `room_set_rs_sector_type`), `redit_room` (dual-word multiflags with complex validation), `redit_persist` (persist_addroom/removeroom), `redit_recall/parent/coords/region` (complex), `redit_locale` (long type mismatch)

### aedit Targets

| Command | Convert To | Field |
|---------|-----------|-------|
| `aedit_security` | `olc_cmd_number` | security (int) |
| `aedit_topic` | `olc_cmd_string` | area_topic (char*) |
| `aedit_wilds` | `olc_cmd_number` | wilds_uid (int) |

**Deferred:** `aedit_builder` (string list), `aedit_vnum/levels` (two-value), `aedit_recall/airshipland/postoffice` (widevnum), `aedit_placetype/areawho` (type sets with special values), `aedit_regions` (very complex)

- [ ] **Step 1: Convert aedit_security**

Keep the permission check (must have sufficient security), then call `olc_cmd_number`:

**Signature:** `olc_cmd_number(ch, argument, label, syntax, &field_ptr, min, max, ctx, record_fn)` — 9 params total.

- [ ] **Step 2: Convert aedit_topic and aedit_wilds**

`aedit_topic` uses `olc_cmd_string` (8 params: `ch, argument, label, syntax, &field_ptr, str_flags, ctx, record_fn`).
`aedit_wilds` uses `olc_cmd_number` (9 params).

Add all field handlers.

- [ ] **Step 3: Build and run tests**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test
```

Expected: Full test suite passes with all new and existing tests.

- [ ] **Step 4: Commit**

```bash
git add editors/areas/aedit.c
git commit -m "feat(olc): convert aedit scalar commands to staged mode

Convert aedit_security, aedit_topic, aedit_wilds
to olc_cmd_* helpers. Add corresponding field handlers.
Note: redit_locale deferred — field is 'long' type, needs olc_cmd_number_long.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 12: Final Verification & Documentation

**Files:**
- Modify: `docs/superpowers/specs/2026-04-06-olc-commit-history-and-full-staging-design.md` (update Phase 1 status)

- [ ] **Step 1: Full test suite verification**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test
```

Record total test count and pass/fail. Expect: baseline + 6 olchist_ tests, all passing.

- [ ] **Step 2: Run OLC-specific tests**

```bash
cd /sentience && ./sent -test:olccs_ && ./sent -test:olchist_
```

All should pass.

- [ ] **Step 3: Update spec with Phase 1 completion status**

In the design spec, update the Phase 1 section to note completion:
- Commit history infrastructure: complete
- History commands: complete
- Persistence (disk + Redis): complete
- Game setting: complete
- Simple scalar conversions: X of Y completed
- List conversions deferred

- [ ] **Step 4: Final commit**

```bash
git add docs/superpowers/specs/2026-04-06-olc-commit-history-and-full-staging-design.md
git commit -m "docs: update OLC commit history spec with Phase 1 status

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Summary

| Task | Description | New Files | Key Changes |
|------|-------------|-----------|-------------|
| 1 | Game setting | — | `merc.h`, `tables.c` |
| 2 | Data model | `olc_commit_history.h`, `olc_commit_history.c` | structs, CRUD, eviction |
| 3 | Serialization | — | serialize/deserialize in `olc_commit_history.c` |
| 4 | Persistence | — | disk I/O, Redis cache in `olc_commit_history.c` |
| 5 | Unit tests | `olc_commit_history_tests.c`, `.json` | 6 tests, registered in dispatcher |
| 6 | Archival hook | — | `olc_staged.c` commit/group hooks |
| 7 | History commands | — | `olc_staged.c`, `olc_editor.c` dispatch |
| 8 | Save triggers | — | area save / shutdown hooks |
| 9 | oedit scalars | — | 4-5 commands converted |
| 10 | medit scalars | — | 6 commands converted |
| 11 | redit/aedit scalars | — | 4 commands converted |
| 12 | Final verification | — | full test suite, docs update |

**Staging coverage after Phase 1:**

| Editor | Before | After | Remaining (Phase 2+) |
|--------|--------|-------|----------------------|
| oedit | 11 staged | ~15 staged | ~52 (types, exits, complex) |
| medit | 24 staged | ~30 staged | ~29 (dice, AC, race, etc.) |
| redit | 8 staged | ~9 staged | ~28 (exits, sectors, flags) |
| aedit | 14 staged | ~17 staged | ~14 (builder, regions, etc.) |
