# OLC Changeset Framework & GMCP Editor Protocol — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the changeset/commit pattern from gameedit to core OLC editors (redit, medit, oedit, aedit) with field-level overlays, bidirectional GMCP protocol for web client editing, and non-blocking string editing for WebSocket clients.

**Architecture:** Field-level overlay using Jansson `json_t*` for old/new values. Pending changes stored per-builder, per-entity in `olc_changeset_t`. Changes are invisible to others until committed. GMCP messages sent immediately (event-driven, not dirty-flag polled). Incoming GMCP parsed with Jansson `json_loads()`. Non-blocking string editor bypasses modal `string_append()` for WebSocket clients.

**Tech Stack:** C23, Jansson (JSON library), custom OLC editor framework (`editors/common/`), GMCP protocol layer (`protocol.c`), LLIST linked lists.

---

## Reference

- **Design spec:** `docs/superpowers/specs/2026-03-26-olc-changeset-gmcp-design.md` (source of truth for all data structures, GMCP message formats, and behavioral rules)
- **Build:** `cd /sentience/src && ./build tests`
- **Test:** `cd /sentience && ./sent -test`
- **Test subset:** `cd /sentience && ./sent -test:olccs` (pattern match)
- **Baseline:** 432 tests, 418 pass, 1 known failure, 13 skipped — any new failures are regressions
- **C standard:** C23 (mid-block declarations, modern features allowed)
- **Build sync:** Both `CMakeLists.txt` AND `Makefile` must be updated for any new `.c` files

## File Map

### New Files

| File | Responsibility |
|------|---------------|
| `editors/common/olc_changeset.h` | Changeset types (`olc_pending_change_t`, `olc_changeset_t`, `olc_changeset_group_t`, `olc_edit_state_t`), field type enum, CRUD API, preview API, commit/revert API, draft API, safety limit constants |
| `editors/common/olc_changeset.c` | Changeset lifecycle (create/destroy), change CRUD (add/find/remove/clear), list operation collapsing, commit flow, revert flow, draft serialization/deserialization |
| `editors/common/olc_staged.h` | Preview helper signatures (`olc_staged_string`, `olc_staged_int`, etc.), staged command utilities |
| `editors/common/olc_staged.c` | Preview helper implementations, staged-mode wrappers for `olc_cmd_*` flow, pending marker formatting |
| `editors/common/olc_field_handlers.h` | `olc_field_handler_t` struct, handler registration API, generic handler declarations |
| `editors/common/olc_field_handlers.c` | Generic scalar handlers (string, int, bool, flags), handler table lookup, apply dispatch |
| `gmcp_editor.h` | GMCP Editor message builder declarations, incoming handler declarations |
| `gmcp_editor.c` | Outgoing message builders (Open, Field, State, Close, Error, CommitResult, StringEdit), incoming handlers (Set, Commit, Revert, Request, StringEdit.Save/Cancel, Draft.Save/Load), `sentience_handle_editor()` dispatcher |
| `tests/unit/olc_changeset_tests.c` | Unit tests for changeset CRUD, list collapsing, preview helpers, draft serialization |
| `tests/data/unit/olc_changeset_tests.json` | JSON test definitions for changeset unit tests |
| `tests/integration/gmcp_editor_tests.c` | Integration tests for GMCP message building, staged command flow |
| `tests/data/integration/gmcp_editor_tests.json` | JSON test definitions for GMCP editor tests |

### Modified Files

| File | Location | Changes |
|------|----------|---------|
| `editors/common/olc_editor.h` | Lines 160-172 | Append `OLC_CHANGE_STAGED = 4` to `olc_change_mode_t` enum |
| `editors/common/olc_editor.h` | Lines 285-328 | Add `field_handlers` and `staged_config` fields to `OLC_EDITOR_DEF` |
| `editors/common/olc_editor.c` | Lines 344-374 | Add `OLC_CHANGE_STAGED` case to `olc_mark_changed()` |
| `editors/common/olc_editor.c` | Lines 800-966 | Wire `commit`/`revert`/`pending`/`savedraft`/`loaddraft`/`discardraft` into `olc_editor_interp()` |
| `editors/common/olc_commands.c` | Lines 31-329 | Add staging code path to each `olc_cmd_*` helper |
| `editors/common/olc_display.c` | Display functions | Add pending-change visual markers (`{Y*{x` prefix) |
| `protocol.h` | Lines 306-319 | Add 8 `GMCP_SENTIENCE_EDITOR_*` enum values before `GMCP_RECEIVE_MAX` |
| `protocol.c` | Lines 3364-3376 | Add 8 entries to `GMCPReceiveTable[]` |
| `protocol.c` | Lines 3809+ | Add 8 case statements to `ParseGMCP()` switch |
| `gmcp_sentience.h` | Lines 25-40 | Add `SENTIENCE_DIRTY_EDITOR = (1 << 14)` flag |
| `merc.h` | Lines 2020-2027 | Add `olc_edit_state_t *olc_state` to `DESCRIPTOR_DATA` |
| `mem.c` | Lines 380-393 | Add auto-draft hook before `INVALIDATE(d)` in `free_descriptor()` |
| `editors/rooms/redit.c` | Lines 112-135 | Change `.change_mode` to `OLC_CHANGE_STAGED`, add `.field_handlers` |
| `editors/mobiles/medit.c` | Editor def | Same migration as redit |
| `editors/objects/oedit.c` | Editor def | Same migration as redit |
| `editors/areas/aedit.c` | Editor def | Same migration as redit |
| `CMakeLists.txt` | Lines 139-175, 317-362 | Add 6 new source files + 2 test files |
| `Makefile` | Lines 122-158, 299-342 | Add 6 new source files + 2 test files (keep synchronized) |
| `tests/framework/test_dispatcher.c` | Lines 57-116 | Add `olccs_` and `gmcped_` handler entries |
| `tests/framework/test_modules.h` | Lines 14-56 | Add handler declarations |
| `tests/data/test_config.json` | Lines 42-80 | Add test suites to `full_test_suites` |

---

## Phase 1: Core Changeset Framework

### Task 1: Changeset Types, CRUD & Build Infrastructure

**Goal:** Create the core data structures and basic CRUD operations for changesets. Establish test infrastructure. This is the foundation everything else builds on.

**Files:**
- Create: `editors/common/olc_changeset.h`
- Create: `editors/common/olc_changeset.c`
- Create: `tests/unit/olc_changeset_tests.c`
- Create: `tests/data/unit/olc_changeset_tests.json`
- Modify: `CMakeLists.txt:139-175` (source files section)
- Modify: `CMakeLists.txt:317-362` (test files section)
- Modify: `Makefile:122-158` (source files section)
- Modify: `Makefile:299-342` (test files section)
- Modify: `tests/framework/test_dispatcher.c:57-116` (handler table)
- Modify: `tests/framework/test_modules.h:14-56` (handler declaration)
- Modify: `tests/data/test_config.json:42-80` (test suite registration)

- [ ] **Step 1: Create the changeset header with all type definitions**

Create `editors/common/olc_changeset.h`:

```c
/**
 * @file olc_changeset.h
 * @brief OLC Changeset Framework — Types and API
 *
 * Provides field-level overlay changesets for staged OLC editing.
 * Changes are accumulated in memory as pending modifications, then
 * committed atomically to the live entity.
 *
 * @see docs/superpowers/specs/2026-03-26-olc-changeset-gmcp-design.md
 */

#ifndef __OLC_CHANGESET_H__
#define __OLC_CHANGESET_H__

#include "../../merc.h"
#include <jansson.h>

/* =========================================================================
 * Constants
 * ========================================================================= */

/** Maximum pending changes per entity */
#define OLC_MAX_PENDING_PER_ENTITY   100

/** Maximum total pending changes across all entities per builder */
#define OLC_MAX_PENDING_PER_BUILDER  500

/** Maximum draft file size in bytes */
#define OLC_MAX_DRAFT_SIZE           (64 * 1024)

/* =========================================================================
 * Field Type Enum
 * ========================================================================= */

/**
 * Types of fields that can be staged in a changeset.
 * Simple scalars use generic framework handlers; complex types need
 * editor-specific handlers registered in OLC_EDITOR_DEF.
 */
typedef enum {
    OLC_FIELD_STRING,       /**< char* fields (name, short_descr, etc.) */
    OLC_FIELD_INT,          /**< int fields (level, alignment, etc.) */
    OLC_FIELD_INT16,        /**< int16_t fields */
    OLC_FIELD_BOOL,         /**< bool fields */
    OLC_FIELD_FLAGS,        /**< long bitfield (act flags, room flags, etc.) */
    OLC_FIELD_WIDEVNUM,     /**< WNUM/WNUM_LOAD references */
    OLC_FIELD_EXIT,         /**< EXIT_DATA (direction + destination + lock) */
    OLC_FIELD_EMBEDDED,     /**< Embedded structs (dice, location) */
    OLC_FIELD_LIST_ADD,     /**< Add item to a linked list */
    OLC_FIELD_LIST_REMOVE,  /**< Remove item from a linked list */
    OLC_FIELD_LIST_UPDATE,  /**< Update item in a linked list */
    OLC_FIELD_MULTILINE,    /**< Multi-line string (description, etc.) */
    OLC_FIELD_TYPE_DATA,    /**< Polymorphic type-specific data (item types) */
} olc_field_type_t;

/* =========================================================================
 * Core Data Structures
 * ========================================================================= */

/**
 * A single field modification pending commit.
 * Data-only — application is dispatched through olc_field_handler_t lookups.
 */
typedef struct olc_pending_change {
    char               *field_path;     /**< e.g., "name", "exits/north/destination" */
    olc_field_type_t    field_type;
    json_t             *old_value;      /**< Jansson JSON — original value snapshot */
    json_t             *new_value;      /**< Jansson JSON — staged replacement value */
} olc_pending_change_t;

/**
 * Per-entity pending changes for one builder.
 * One changeset per entity currently open in an editor.
 */
typedef struct olc_changeset {
    int             editor_type;        /**< ED_ROOM, ED_MOBILE, etc. */
    WNUM_LOAD       entity_wnum;        /**< Area UID + vnum (persistent identifier) */
    char           *entity_label;       /**< Human-readable label */
    char           *author;             /**< Builder character name */
    LLIST          *changes;            /**< List of olc_pending_change_t* */
    time_t          created_at;
    time_t          updated_at;
    bool            is_dirty;           /**< Has unsaved changes since last draft */
} olc_changeset_t;

/**
 * Optional multi-entity commit group.
 * Ties multiple changesets committed together for audit purposes.
 */
typedef struct olc_changeset_group {
    int             group_id;           /**< Sequential ID for history */
    char           *comment;            /**< Commit message */
    char           *author;             /**< Builder who committed */
    time_t          committed_at;
    LLIST          *changesets;         /**< List of olc_changeset_t* (committed copies) */
} olc_changeset_group_t;

/**
 * Per-builder editing state, stored on descriptor.
 */
typedef struct olc_edit_state {
    LLIST          *active_changesets;   /**< List of olc_changeset_t* (one per open entity) */
    LLIST          *string_edit_sessions;/**< List of active non-blocking string edits */
    int             next_string_session_id;
} olc_edit_state_t;

/* =========================================================================
 * Changeset Lifecycle API
 * ========================================================================= */

/** Create a new empty changeset for an entity. */
olc_changeset_t *olc_changeset_create(int editor_type, WNUM_LOAD entity_wnum,
                                       const char *entity_label, const char *author);

/** Destroy a changeset and all its pending changes. */
void olc_changeset_destroy(olc_changeset_t *cs);

/* =========================================================================
 * Change CRUD API
 * ========================================================================= */

/**
 * Add or update a pending change. If a change for field_path already exists,
 * it is updated in place (old_value preserved, new_value replaced). If the
 * new_value equals old_value, the change is removed (no-op detection).
 * For list operations, collapsing rules apply (see spec).
 *
 * @return The change entry (new or updated), or NULL if collapsed to no-op.
 */
olc_pending_change_t *olc_changeset_add_change(olc_changeset_t *cs,
    const char *field_path, olc_field_type_t type,
    json_t *old_value, json_t *new_value);

/** Find a pending change by field path. Returns NULL if not found. */
olc_pending_change_t *olc_changeset_find_change(olc_changeset_t *cs,
    const char *field_path);

/** Remove a pending change by field path. Returns true if found and removed. */
bool olc_changeset_remove_change(olc_changeset_t *cs, const char *field_path);

/** Remove all pending changes. */
void olc_changeset_clear(olc_changeset_t *cs);

/** Count of pending changes. */
int olc_changeset_count(olc_changeset_t *cs);

/* =========================================================================
 * Edit State API
 * ========================================================================= */

/** Create a new edit state (called when first staged editor opens). */
olc_edit_state_t *olc_edit_state_create(void);

/** Destroy edit state and all owned changesets. */
void olc_edit_state_destroy(olc_edit_state_t *state);

/** Find changeset for a specific entity in the builder's edit state. */
olc_changeset_t *olc_edit_state_find_changeset(olc_edit_state_t *state,
    int editor_type, WNUM_LOAD entity_wnum);

/** Total pending changes across all active changesets. */
int olc_edit_state_total_pending(olc_edit_state_t *state);

/* =========================================================================
 * Pending Change Lifecycle (internal helpers, exposed for testing)
 * ========================================================================= */

/** Allocate a new pending change. Caller owns the returned pointer. */
olc_pending_change_t *olc_pending_change_create(const char *field_path,
    olc_field_type_t type, json_t *old_value, json_t *new_value);

/** Free a pending change and its JSON values. */
void olc_pending_change_destroy(olc_pending_change_t *change);

#endif /* !def __OLC_CHANGESET_H__ */
```

- [ ] **Step 2: Create minimal changeset implementation (CRUD only)**

Create `editors/common/olc_changeset.c`:

```c
/**
 * @file olc_changeset.c
 * @brief OLC Changeset Framework — Implementation
 */

#include "olc_changeset.h"
#include "../../db.h"
#include <string.h>
#include <time.h>

/* =========================================================================
 * Pending Change Lifecycle
 * ========================================================================= */

olc_pending_change_t *olc_pending_change_create(const char *field_path,
    olc_field_type_t type, json_t *old_value, json_t *new_value)
{
    olc_pending_change_t *change = alloc_mem(sizeof(*change));
    change->field_path = str_dup(field_path);
    change->field_type = type;
    change->old_value  = old_value ? json_incref(old_value) : NULL;
    change->new_value  = new_value ? json_incref(new_value) : NULL;
    return change;
}

void olc_pending_change_destroy(olc_pending_change_t *change)
{
    if (!change) return;
    free_string(change->field_path);
    if (change->old_value) json_decref(change->old_value);
    if (change->new_value) json_decref(change->new_value);
    free_mem(change, sizeof(*change));
}

/* =========================================================================
 * Changeset Lifecycle
 * ========================================================================= */

olc_changeset_t *olc_changeset_create(int editor_type, WNUM_LOAD entity_wnum,
                                       const char *entity_label, const char *author)
{
    olc_changeset_t *cs = alloc_mem(sizeof(*cs));
    cs->editor_type  = editor_type;
    cs->entity_wnum  = entity_wnum;
    cs->entity_label = str_dup(entity_label ? entity_label : "");
    cs->author       = str_dup(author ? author : "");
    cs->changes      = new_llist();
    cs->created_at   = current_time;
    cs->updated_at   = current_time;
    cs->is_dirty     = false;
    return cs;
}

void olc_changeset_destroy(olc_changeset_t *cs)
{
    if (!cs) return;
    olc_changeset_clear(cs);
    del_llist(cs->changes);
    free_string(cs->entity_label);
    free_string(cs->author);
    free_mem(cs, sizeof(*cs));
}

/* =========================================================================
 * Change CRUD
 * ========================================================================= */

olc_pending_change_t *olc_changeset_find_change(olc_changeset_t *cs,
    const char *field_path)
{
    if (!cs || !field_path) return NULL;

    LLIST_ITERATOR *it = llist_iterator(cs->changes);
    olc_pending_change_t *change;
    while ((change = llist_next(it)) != NULL) {
        if (!strcmp(change->field_path, field_path)) {
            llist_iterator_delete(it);
            return change;
        }
    }
    llist_iterator_delete(it);
    return NULL;
}

olc_pending_change_t *olc_changeset_add_change(olc_changeset_t *cs,
    const char *field_path, olc_field_type_t type,
    json_t *old_value, json_t *new_value)
{
    if (!cs || !field_path) return NULL;

    /* No-op detection: if new_value equals old_value, remove any existing change */
    if (json_equal(old_value, new_value)) {
        olc_changeset_remove_change(cs, field_path);
        return NULL;
    }

    /* Check if change already exists for this field */
    olc_pending_change_t *existing = olc_changeset_find_change(cs, field_path);
    if (existing) {
        /* Update in place: preserve original old_value, replace new_value */
        if (existing->new_value) json_decref(existing->new_value);
        existing->new_value = new_value ? json_incref(new_value) : NULL;
        existing->field_type = type;
        cs->updated_at = current_time;
        cs->is_dirty = true;

        /* No-op check after update: if new equals original old, remove */
        if (json_equal(existing->old_value, existing->new_value)) {
            olc_changeset_remove_change(cs, field_path);
            return NULL;
        }
        return existing;
    }

    /* Safety limit check */
    if (olc_changeset_count(cs) >= OLC_MAX_PENDING_PER_ENTITY)
        return NULL;

    /* Create new change entry */
    olc_pending_change_t *change = olc_pending_change_create(
        field_path, type, old_value, new_value);
    llist_add(cs->changes, change);
    cs->updated_at = current_time;
    cs->is_dirty = true;
    return change;
}

bool olc_changeset_remove_change(olc_changeset_t *cs, const char *field_path)
{
    if (!cs || !field_path) return false;

    LLIST_ITERATOR *it = llist_iterator(cs->changes);
    olc_pending_change_t *change;
    while ((change = llist_next(it)) != NULL) {
        if (!strcmp(change->field_path, field_path)) {
            llist_remove(cs->changes, change);
            llist_iterator_delete(it);
            olc_pending_change_destroy(change);
            cs->updated_at = current_time;
            return true;
        }
    }
    llist_iterator_delete(it);
    return false;
}

void olc_changeset_clear(olc_changeset_t *cs)
{
    if (!cs) return;

    LLIST_ITERATOR *it = llist_iterator(cs->changes);
    olc_pending_change_t *change;
    while ((change = llist_next(it)) != NULL) {
        llist_remove(cs->changes, change);
        olc_pending_change_destroy(change);
    }
    llist_iterator_delete(it);
    cs->updated_at = current_time;
    cs->is_dirty = false;
}

int olc_changeset_count(olc_changeset_t *cs)
{
    return (cs && cs->changes) ? llist_count(cs->changes) : 0;
}

/* =========================================================================
 * Edit State
 * ========================================================================= */

olc_edit_state_t *olc_edit_state_create(void)
{
    olc_edit_state_t *state = alloc_mem(sizeof(*state));
    state->active_changesets = new_llist();
    state->string_edit_sessions = new_llist();
    state->next_string_session_id = 1;
    return state;
}

void olc_edit_state_destroy(olc_edit_state_t *state)
{
    if (!state) return;

    LLIST_ITERATOR *it = llist_iterator(state->active_changesets);
    olc_changeset_t *cs;
    while ((cs = llist_next(it)) != NULL) {
        llist_remove(state->active_changesets, cs);
        olc_changeset_destroy(cs);
    }
    llist_iterator_delete(it);
    del_llist(state->active_changesets);

    /* String edit sessions cleaned up in Phase 5 */
    del_llist(state->string_edit_sessions);

    free_mem(state, sizeof(*state));
}

olc_changeset_t *olc_edit_state_find_changeset(olc_edit_state_t *state,
    int editor_type, WNUM_LOAD entity_wnum)
{
    if (!state) return NULL;

    LLIST_ITERATOR *it = llist_iterator(state->active_changesets);
    olc_changeset_t *cs;
    while ((cs = llist_next(it)) != NULL) {
        if (cs->editor_type == editor_type
            && cs->entity_wnum.auid == entity_wnum.auid
            && cs->entity_wnum.vnum == entity_wnum.vnum) {
            llist_iterator_delete(it);
            return cs;
        }
    }
    llist_iterator_delete(it);
    return NULL;
}

int olc_edit_state_total_pending(olc_edit_state_t *state)
{
    if (!state) return 0;

    int total = 0;
    LLIST_ITERATOR *it = llist_iterator(state->active_changesets);
    olc_changeset_t *cs;
    while ((cs = llist_next(it)) != NULL) {
        total += olc_changeset_count(cs);
    }
    llist_iterator_delete(it);
    return total;
}
```

- [ ] **Step 3: Create test handler and JSON test definitions**

Create `tests/unit/olc_changeset_tests.c`:

```c
/**
 * OLC Changeset Unit Tests
 *
 * Tests changeset CRUD operations, list collapsing, and preview helpers.
 * These are pure data-structure tests with no MUD environment dependency.
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../framework/test_framework.h"
#include "../../editors/common/olc_changeset.h"
#include <jansson.h>
#include <string.h>

/* =========================================================================
 * Test Scenarios
 * ========================================================================= */

static test_result_t test_changeset_create_destroy(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test Room", "Builder");

    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_INT_EQ(ED_ROOM, cs->editor_type);
    TEST_ASSERT_INT_EQ(5, (int)cs->entity_wnum.auid);
    TEST_ASSERT_INT_EQ(3001, (int)cs->entity_wnum.vnum);
    TEST_ASSERT_STR_EQ("Test Room", cs->entity_label);
    TEST_ASSERT_STR_EQ("Builder", cs->author);
    TEST_ASSERT_INT_EQ(0, olc_changeset_count(cs));
    TEST_ASSERT_FALSE(cs->is_dirty);

    olc_changeset_destroy(cs);
    return TEST_PASS;
}

static test_result_t test_change_add_find_remove(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test Room", "Builder");

    json_t *old_val = json_string("Old Name");
    json_t *new_val = json_string("New Name");

    /* Add a change */
    olc_pending_change_t *change = olc_changeset_add_change(
        cs, "name", OLC_FIELD_STRING, old_val, new_val);
    TEST_ASSERT_NOT_NULL(change);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));
    TEST_ASSERT_TRUE(cs->is_dirty);
    TEST_ASSERT_STR_EQ("name", change->field_path);

    /* Find it */
    olc_pending_change_t *found = olc_changeset_find_change(cs, "name");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_STR_EQ("New Name", json_string_value(found->new_value));

    /* Find non-existent */
    TEST_ASSERT_NULL(olc_changeset_find_change(cs, "nonexistent"));

    /* Remove it */
    TEST_ASSERT_TRUE(olc_changeset_remove_change(cs, "name"));
    TEST_ASSERT_INT_EQ(0, olc_changeset_count(cs));

    /* Remove non-existent */
    TEST_ASSERT_FALSE(olc_changeset_remove_change(cs, "name"));

    json_decref(old_val);
    json_decref(new_val);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}

static test_result_t test_change_update_in_place(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *original = json_string("Original");
    json_t *first    = json_string("First Edit");
    json_t *second   = json_string("Second Edit");

    /* Add initial change */
    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, original, first);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));

    /* Update same field — old_value should stay as "Original" */
    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, first, second);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));

    olc_pending_change_t *found = olc_changeset_find_change(cs, "name");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_STR_EQ("Original", json_string_value(found->old_value));
    TEST_ASSERT_STR_EQ("Second Edit", json_string_value(found->new_value));

    json_decref(original);
    json_decref(first);
    json_decref(second);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}

static test_result_t test_noop_detection(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *val = json_string("Same Value");

    /* Adding change where old == new should be a no-op */
    olc_pending_change_t *result = olc_changeset_add_change(
        cs, "name", OLC_FIELD_STRING, val, val);
    TEST_ASSERT_NULL(result);
    TEST_ASSERT_INT_EQ(0, olc_changeset_count(cs));

    /* Add a real change, then "revert" it by setting new = original old */
    json_t *old_val = json_string("Old");
    json_t *new_val = json_string("New");
    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, old_val, new_val);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));

    /* Update back to original — should collapse to no-op */
    result = olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, new_val, old_val);
    TEST_ASSERT_NULL(result);
    TEST_ASSERT_INT_EQ(0, olc_changeset_count(cs));

    json_decref(val);
    json_decref(old_val);
    json_decref(new_val);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}

static test_result_t test_changeset_clear(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *old_v = json_string("old");
    json_t *new_v = json_string("new");

    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, old_v, new_v);
    olc_changeset_add_change(cs, "desc", OLC_FIELD_STRING, old_v, new_v);
    TEST_ASSERT_INT_EQ(2, olc_changeset_count(cs));

    olc_changeset_clear(cs);
    TEST_ASSERT_INT_EQ(0, olc_changeset_count(cs));
    TEST_ASSERT_FALSE(cs->is_dirty);

    json_decref(old_v);
    json_decref(new_v);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}

static test_result_t test_safety_limit(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *old_v = json_string("old");
    json_t *new_v = json_string("new");

    /* Fill to the limit */
    char field[32];
    for (int i = 0; i < OLC_MAX_PENDING_PER_ENTITY; i++) {
        snprintf(field, sizeof(field), "field_%d", i);
        olc_pending_change_t *c = olc_changeset_add_change(
            cs, field, OLC_FIELD_STRING, old_v, new_v);
        TEST_ASSERT_NOT_NULL(c);
    }
    TEST_ASSERT_INT_EQ(OLC_MAX_PENDING_PER_ENTITY, olc_changeset_count(cs));

    /* Next addition should fail */
    olc_pending_change_t *overflow = olc_changeset_add_change(
        cs, "overflow_field", OLC_FIELD_STRING, old_v, new_v);
    TEST_ASSERT_NULL(overflow);
    TEST_ASSERT_INT_EQ(OLC_MAX_PENDING_PER_ENTITY, olc_changeset_count(cs));

    json_decref(old_v);
    json_decref(new_v);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}

static test_result_t test_edit_state_lifecycle(test_case_t *test)
{
    olc_edit_state_t *state = olc_edit_state_create();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_INT_EQ(0, olc_edit_state_total_pending(state));

    /* Add a changeset */
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Room", "Builder");
    llist_add(state->active_changesets, cs);

    json_t *old_v = json_string("old");
    json_t *new_v = json_string("new");
    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, old_v, new_v);

    TEST_ASSERT_INT_EQ(1, olc_edit_state_total_pending(state));

    /* Find it */
    olc_changeset_t *found = olc_edit_state_find_changeset(state, ED_ROOM, wnum);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_STR_EQ("Room", found->entity_label);

    /* Find non-existent */
    WNUM_LOAD other = { .auid = 5, .vnum = 9999 };
    TEST_ASSERT_NULL(olc_edit_state_find_changeset(state, ED_ROOM, other));

    json_decref(old_v);
    json_decref(new_v);
    olc_edit_state_destroy(state);
    return TEST_PASS;
}

static test_result_t test_numeric_change_types(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    /* Integer change */
    json_t *old_int = json_integer(100);
    json_t *new_int = json_integer(200);
    olc_pending_change_t *c = olc_changeset_add_change(
        cs, "heal_rate", OLC_FIELD_INT, old_int, new_int);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_INT_EQ(OLC_FIELD_INT, c->field_type);
    TEST_ASSERT_INT_EQ(100, (int)json_integer_value(c->old_value));
    TEST_ASSERT_INT_EQ(200, (int)json_integer_value(c->new_value));

    /* Boolean change */
    json_t *old_bool = json_false();
    json_t *new_bool = json_true();
    c = olc_changeset_add_change(cs, "is_dark", OLC_FIELD_BOOL, old_bool, new_bool);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_INT_EQ(OLC_FIELD_BOOL, c->field_type);

    /* Flags change (as integer bitmask) */
    json_t *old_flags = json_integer(0x0003);
    json_t *new_flags = json_integer(0x000F);
    c = olc_changeset_add_change(cs, "room_flags", OLC_FIELD_FLAGS, old_flags, new_flags);
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_INT_EQ(3, olc_changeset_count(cs));

    json_decref(old_int); json_decref(new_int);
    json_decref(old_bool); json_decref(new_bool);
    json_decref(old_flags); json_decref(new_flags);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}

/* =========================================================================
 * Dispatch
 * ========================================================================= */

test_result_t run_olc_changeset_test_case(test_case_t *test)
{
    if (!test || !test->test_type) return TEST_ERROR;

    const char *type = test->test_type;

    if (!strcmp(type, "olccs_create_destroy"))       return test_changeset_create_destroy(test);
    if (!strcmp(type, "olccs_add_find_remove"))       return test_change_add_find_remove(test);
    if (!strcmp(type, "olccs_update_in_place"))       return test_change_update_in_place(test);
    if (!strcmp(type, "olccs_noop_detection"))        return test_noop_detection(test);
    if (!strcmp(type, "olccs_clear"))                 return test_changeset_clear(test);
    if (!strcmp(type, "olccs_safety_limit"))          return test_safety_limit(test);
    if (!strcmp(type, "olccs_edit_state_lifecycle"))  return test_edit_state_lifecycle(test);
    if (!strcmp(type, "olccs_numeric_types"))         return test_numeric_change_types(test);

    return TEST_SKIP;
}

#endif /* BUILD_TESTS */
```

Create `tests/data/unit/olc_changeset_tests.json`:

```json
{
  "type": "test_suite",
  "test_suite": "olc_changeset_unit_tests",
  "description": "Unit tests for OLC changeset CRUD operations, no-op detection, safety limits, and edit state lifecycle",
  "version": "1.0",
  "requires_mud_environment": false,
  "dependencies": [],
  "timeout_seconds": 30,
  "tests": [
    {
      "name": "olccs_create_destroy",
      "description": "Create a changeset and verify all fields initialized correctly",
      "test_type": "olccs_create_destroy",
      "input": {},
      "expected_output": { "success": true }
    },
    {
      "name": "olccs_add_find_remove",
      "description": "Add a pending change, find it, remove it, verify counts",
      "test_type": "olccs_add_find_remove",
      "input": {},
      "expected_output": { "success": true }
    },
    {
      "name": "olccs_update_in_place",
      "description": "Updating same field preserves original old_value, replaces new_value",
      "test_type": "olccs_update_in_place",
      "input": {},
      "expected_output": { "success": true }
    },
    {
      "name": "olccs_noop_detection",
      "description": "Change where old==new is a no-op; reverting to original collapses to no-op",
      "test_type": "olccs_noop_detection",
      "input": {},
      "expected_output": { "success": true }
    },
    {
      "name": "olccs_clear",
      "description": "Clear removes all pending changes and resets dirty flag",
      "test_type": "olccs_clear",
      "input": {},
      "expected_output": { "success": true }
    },
    {
      "name": "olccs_safety_limit",
      "description": "Cannot exceed OLC_MAX_PENDING_PER_ENTITY changes",
      "test_type": "olccs_safety_limit",
      "input": {},
      "expected_output": { "success": true }
    },
    {
      "name": "olccs_edit_state_lifecycle",
      "description": "Edit state create, add changeset, find, total pending, destroy",
      "test_type": "olccs_edit_state_lifecycle",
      "input": {},
      "expected_output": { "success": true }
    },
    {
      "name": "olccs_numeric_types",
      "description": "Changeset handles int, bool, and flags field types correctly",
      "test_type": "olccs_numeric_types",
      "input": {},
      "expected_output": { "success": true }
    }
  ]
}
```

- [ ] **Step 4: Register test handler in test framework**

In `tests/framework/test_modules.h`, add after the last handler declaration (around line 56):
```c
test_result_t run_olc_changeset_test_case(test_case_t *test);
```

In `tests/framework/test_dispatcher.c`, add to `handler_table[]` (before the sentinel):
```c
    { "olccs_",                          run_olc_changeset_test_case,       MATCH_SUBSTR },
```

In `tests/data/test_config.json`, add `"olc_changeset_unit_tests"` to the `full_test_suites` array.

- [ ] **Step 5: Update build system — add source and test files**

In `CMakeLists.txt`, add to `SOURCE_FILES` (after `editors/common/olc_display.c` around line 175):
```cmake
    editors/common/olc_changeset.c
```

In `CMakeLists.txt`, add to the `BUILD_TESTS` section (after `tests/integration/olc_framework_tests.c`):
```cmake
        tests/unit/olc_changeset_tests.c
```

In `Makefile`, add to `C_FILES` (after `editors/common/olc_display.c` around line 158):
```makefile
    editors/common/olc_changeset.c \
```

In `Makefile`, add to the `BUILD_TESTS` section (after `tests/integration/olc_framework_tests.c`):
```makefile
               tests/unit/olc_changeset_tests.c \
```

- [ ] **Step 6: Build and verify tests pass**

Run: `cd /sentience/src && ./build tests`
Expected: Build succeeds with no errors.

Run: `cd /sentience && ./sent -test:olccs`
Expected: All 8 `olccs_*` tests PASS.

Run: `cd /sentience && ./sent -test`
Expected: Baseline + 8 new tests pass. No regressions.

- [ ] **Step 7: Commit**

```bash
cd /sentience/src
git add editors/common/olc_changeset.h editors/common/olc_changeset.c \
        tests/unit/olc_changeset_tests.c tests/data/unit/olc_changeset_tests.json \
        tests/framework/test_dispatcher.c tests/framework/test_modules.h \
        tests/data/test_config.json CMakeLists.txt Makefile
git commit -m "feat(olc): add changeset data structures, CRUD operations, and unit tests

Introduce olc_changeset_t for field-level overlay editing with:
- olc_pending_change_t with Jansson JSON old/new values
- Changeset create/destroy lifecycle
- Change add (with update-in-place), find, remove, clear
- No-op detection (new==old collapses to removal)
- Safety limit (OLC_MAX_PENDING_PER_ENTITY)
- olc_edit_state_t for per-builder changeset management
- 8 unit tests covering all CRUD operations

Part of: OLC changeset framework & GMCP editor protocol"
```


### Task 2: List Operation Collapsing

**Goal:** Implement the collapsing rules for list operations (extra_descr, resets, affects) as specified in the design document. This ensures redundant operations cancel out.

**Files:**
- Modify: `editors/common/olc_changeset.c` (add collapsing logic to `olc_changeset_add_change`)
- Modify: `tests/unit/olc_changeset_tests.c` (add collapsing tests)
- Modify: `tests/data/unit/olc_changeset_tests.json` (add test definitions)

- [ ] **Step 1: Write failing tests for list operation collapsing**

Add to `tests/unit/olc_changeset_tests.c`:

```c
static test_result_t test_list_add_then_remove(test_case_t *test)
{
    /* ADD then REMOVE for same item → collapse to no-op, purge both */
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *item = json_object();
    json_object_set_new(item, "keyword", json_string("statue"));
    json_object_set_new(item, "description", json_string("A stone statue."));

    /* ADD */
    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_ADD, NULL, item);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));

    /* REMOVE same → should collapse to no-op */
    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_REMOVE, item, NULL);
    TEST_ASSERT_INT_EQ(0, olc_changeset_count(cs));

    json_decref(item);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}

static test_result_t test_list_add_then_update(test_case_t *test)
{
    /* ADD then UPDATE for same item → collapse to ADD with updated value */
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *item_v1 = json_string("A stone statue.");
    json_t *item_v2 = json_string("A marble statue.");

    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_ADD, NULL, item_v1);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));

    /* UPDATE same → should remain ADD with updated new_value */
    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_UPDATE, item_v1, item_v2);

    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));
    olc_pending_change_t *found = olc_changeset_find_change(cs, "extra_descr/statue");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_INT_EQ(OLC_FIELD_LIST_ADD, found->field_type);
    TEST_ASSERT_NULL(found->old_value);  /* ADD has null old_value */
    TEST_ASSERT_STR_EQ("A marble statue.", json_string_value(found->new_value));

    json_decref(item_v1);
    json_decref(item_v2);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}

static test_result_t test_list_remove_then_add(test_case_t *test)
{
    /* REMOVE then ADD for same key → collapse to UPDATE */
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *old_item = json_string("Old description.");
    json_t *new_item = json_string("New description.");

    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_REMOVE, old_item, NULL);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));

    /* ADD back → should collapse to UPDATE */
    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_ADD, NULL, new_item);

    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));
    olc_pending_change_t *found = olc_changeset_find_change(cs, "extra_descr/statue");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_INT_EQ(OLC_FIELD_LIST_UPDATE, found->field_type);
    TEST_ASSERT_STR_EQ("Old description.", json_string_value(found->old_value));
    TEST_ASSERT_STR_EQ("New description.", json_string_value(found->new_value));

    json_decref(old_item);
    json_decref(new_item);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}

static test_result_t test_list_update_then_update(test_case_t *test)
{
    /* UPDATE then UPDATE → keep original old_value, update new_value */
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *original = json_string("Original");
    json_t *first    = json_string("First Edit");
    json_t *second   = json_string("Second Edit");

    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_UPDATE, original, first);
    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_UPDATE, first, second);

    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));
    olc_pending_change_t *found = olc_changeset_find_change(cs, "extra_descr/statue");
    TEST_ASSERT_STR_EQ("Original", json_string_value(found->old_value));
    TEST_ASSERT_STR_EQ("Second Edit", json_string_value(found->new_value));

    json_decref(original);
    json_decref(first);
    json_decref(second);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}

static test_result_t test_list_update_then_remove(test_case_t *test)
{
    /* UPDATE then REMOVE → collapse to REMOVE with original's old_value */
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *original  = json_string("Original");
    json_t *edited    = json_string("Edited");

    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_UPDATE, original, edited);
    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_REMOVE, edited, NULL);

    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));
    olc_pending_change_t *found = olc_changeset_find_change(cs, "extra_descr/statue");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_INT_EQ(OLC_FIELD_LIST_REMOVE, found->field_type);
    TEST_ASSERT_STR_EQ("Original", json_string_value(found->old_value));
    TEST_ASSERT_NULL(found->new_value);

    json_decref(original);
    json_decref(edited);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}
```

Add corresponding dispatch entries in `run_olc_changeset_test_case()`:
```c
    if (!strcmp(type, "olccs_list_add_remove"))       return test_list_add_then_remove(test);
    if (!strcmp(type, "olccs_list_add_update"))       return test_list_add_then_update(test);
    if (!strcmp(type, "olccs_list_remove_add"))       return test_list_remove_then_add(test);
    if (!strcmp(type, "olccs_list_update_update"))    return test_list_update_then_update(test);
    if (!strcmp(type, "olccs_list_update_remove"))    return test_list_update_then_remove(test);
```

Add 5 test entries to `tests/data/unit/olc_changeset_tests.json`:
```json
    {
      "name": "olccs_list_add_remove",
      "description": "ADD then REMOVE for same list item collapses to no-op",
      "test_type": "olccs_list_add_remove",
      "input": {},
      "expected_output": { "success": true }
    },
    {
      "name": "olccs_list_add_update",
      "description": "ADD then UPDATE for same item collapses to ADD with updated value",
      "test_type": "olccs_list_add_update",
      "input": {},
      "expected_output": { "success": true }
    },
    {
      "name": "olccs_list_remove_add",
      "description": "REMOVE then ADD for same key collapses to UPDATE",
      "test_type": "olccs_list_remove_add",
      "input": {},
      "expected_output": { "success": true }
    },
    {
      "name": "olccs_list_update_update",
      "description": "UPDATE then UPDATE keeps original old_value, updates new_value",
      "test_type": "olccs_list_update_update",
      "input": {},
      "expected_output": { "success": true }
    },
    {
      "name": "olccs_list_update_remove",
      "description": "UPDATE then REMOVE collapses to REMOVE with original old_value",
      "test_type": "olccs_list_update_remove",
      "input": {},
      "expected_output": { "success": true }
    }
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:olccs_list`
Expected: All 5 new list tests FAIL (collapsing logic not yet implemented).

- [ ] **Step 3: Implement list operation collapsing in olc_changeset_add_change**

In `editors/common/olc_changeset.c`, modify `olc_changeset_add_change()` to add collapsing logic when the existing change is a list operation. Add a helper function:

```c
/**
 * Check if a field type is a list operation.
 */
static bool is_list_operation(olc_field_type_t type)
{
    return type == OLC_FIELD_LIST_ADD
        || type == OLC_FIELD_LIST_REMOVE
        || type == OLC_FIELD_LIST_UPDATE;
}

/**
 * Handle list operation collapsing when a change already exists for the field.
 * Returns true if the operation was handled (caller should not proceed with
 * normal update-in-place logic). Sets *out to the resulting change or NULL
 * if collapsed to no-op.
 */
static bool collapse_list_operation(olc_changeset_t *cs,
    olc_pending_change_t *existing, olc_field_type_t new_type,
    json_t *old_value, json_t *new_value, olc_pending_change_t **out)
{
    *out = NULL;

    /* ADD + REMOVE → no-op */
    if (existing->field_type == OLC_FIELD_LIST_ADD && new_type == OLC_FIELD_LIST_REMOVE) {
        olc_changeset_remove_change(cs, existing->field_path);
        return true;
    }

    /* ADD + UPDATE → ADD with updated new_value */
    if (existing->field_type == OLC_FIELD_LIST_ADD && new_type == OLC_FIELD_LIST_UPDATE) {
        if (existing->new_value) json_decref(existing->new_value);
        existing->new_value = new_value ? json_incref(new_value) : NULL;
        /* Type stays as LIST_ADD, old_value stays NULL */
        cs->updated_at = current_time;
        cs->is_dirty = true;
        *out = existing;
        return true;
    }

    /* REMOVE + ADD → UPDATE (old from REMOVE, new from ADD) */
    if (existing->field_type == OLC_FIELD_LIST_REMOVE && new_type == OLC_FIELD_LIST_ADD) {
        existing->field_type = OLC_FIELD_LIST_UPDATE;
        /* old_value stays as REMOVE's old_value */
        if (existing->new_value) json_decref(existing->new_value);
        existing->new_value = new_value ? json_incref(new_value) : NULL;
        cs->updated_at = current_time;
        cs->is_dirty = true;
        *out = existing;
        return true;
    }

    /* UPDATE + UPDATE → keep original old_value, update new_value */
    if (existing->field_type == OLC_FIELD_LIST_UPDATE && new_type == OLC_FIELD_LIST_UPDATE) {
        if (existing->new_value) json_decref(existing->new_value);
        existing->new_value = new_value ? json_incref(new_value) : NULL;
        cs->updated_at = current_time;
        cs->is_dirty = true;
        *out = existing;
        return true;
    }

    /* UPDATE + REMOVE → REMOVE with original old_value */
    if (existing->field_type == OLC_FIELD_LIST_UPDATE && new_type == OLC_FIELD_LIST_REMOVE) {
        existing->field_type = OLC_FIELD_LIST_REMOVE;
        /* old_value stays from original UPDATE */
        if (existing->new_value) json_decref(existing->new_value);
        existing->new_value = NULL;
        cs->updated_at = current_time;
        cs->is_dirty = true;
        *out = existing;
        return true;
    }

    return false;  /* Not a recognized collapsing pair */
}
```

Then in `olc_changeset_add_change()`, after finding `existing`, before the normal update-in-place:

```c
    if (existing) {
        /* List operation collapsing */
        if (is_list_operation(existing->field_type) || is_list_operation(type)) {
            olc_pending_change_t *result;
            if (collapse_list_operation(cs, existing, type, old_value, new_value, &result))
                return result;
        }

        /* Normal update-in-place for non-list fields */
        // ... existing code ...
    }
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:olccs`
Expected: All 13 tests (8 original + 5 collapsing) PASS.

- [ ] **Step 5: Commit**

```bash
git add editors/common/olc_changeset.c tests/unit/olc_changeset_tests.c \
        tests/data/unit/olc_changeset_tests.json
git commit -m "feat(olc): implement list operation collapsing rules

Add 5 collapsing rules for list field operations:
- ADD+REMOVE → no-op (purge both)
- ADD+UPDATE → ADD with updated value
- REMOVE+ADD → UPDATE
- UPDATE+UPDATE → keep original old_value, update new_value
- UPDATE+REMOVE → REMOVE with original old_value

5 new unit tests covering all collapsing scenarios."
```


### Task 3: Preview Helpers

**Goal:** Implement functions that return the "effective" value of a field — the staged value if a pending change exists, otherwise the live value. These are used by display functions to show preview state.

**Files:**
- Create: `editors/common/olc_staged.h`
- Create: `editors/common/olc_staged.c`
- Modify: `tests/unit/olc_changeset_tests.c` (add preview tests)
- Modify: `tests/data/unit/olc_changeset_tests.json` (add test definitions)
- Modify: `CMakeLists.txt` (add olc_staged.c)
- Modify: `Makefile` (add olc_staged.c)

- [ ] **Step 1: Write failing tests for preview helpers**

Add to `tests/unit/olc_changeset_tests.c`:

```c
#include "../../editors/common/olc_staged.h"

static test_result_t test_staged_string(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    /* No pending change → returns live value */
    const char *result = olc_staged_string(cs, "name", "Live Name");
    TEST_ASSERT_STR_EQ("Live Name", result);

    /* Add pending change → returns staged value */
    json_t *old_v = json_string("Live Name");
    json_t *new_v = json_string("Staged Name");
    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, old_v, new_v);

    result = olc_staged_string(cs, "name", "Live Name");
    TEST_ASSERT_STR_EQ("Staged Name", result);

    /* NULL changeset → returns live value */
    result = olc_staged_string(NULL, "name", "Live Name");
    TEST_ASSERT_STR_EQ("Live Name", result);

    json_decref(old_v);
    json_decref(new_v);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}

static test_result_t test_staged_int(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    /* No change → live */
    TEST_ASSERT_INT_EQ(100, olc_staged_int(cs, "heal_rate", 100));

    /* With pending change */
    json_t *old_v = json_integer(100);
    json_t *new_v = json_integer(200);
    olc_changeset_add_change(cs, "heal_rate", OLC_FIELD_INT, old_v, new_v);

    TEST_ASSERT_INT_EQ(200, olc_staged_int(cs, "heal_rate", 100));

    json_decref(old_v);
    json_decref(new_v);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}

static test_result_t test_staged_flags(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *old_v = json_integer(0x03);
    json_t *new_v = json_integer(0x0F);
    olc_changeset_add_change(cs, "room_flags", OLC_FIELD_FLAGS, old_v, new_v);

    long result = olc_staged_flags(cs, "room_flags", 0x03);
    TEST_ASSERT_INT_EQ(0x0F, (int)result);

    json_decref(old_v);
    json_decref(new_v);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}

static test_result_t test_staged_bool(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    TEST_ASSERT_FALSE(olc_staged_bool(cs, "no_recall", false));

    json_t *old_v = json_false();
    json_t *new_v = json_true();
    olc_changeset_add_change(cs, "no_recall", OLC_FIELD_BOOL, old_v, new_v);

    TEST_ASSERT_TRUE(olc_staged_bool(cs, "no_recall", false));

    json_decref(old_v);
    json_decref(new_v);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}

static test_result_t test_is_field_staged(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    TEST_ASSERT_FALSE(olc_is_field_staged(cs, "name"));

    json_t *old_v = json_string("Old");
    json_t *new_v = json_string("New");
    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, old_v, new_v);

    TEST_ASSERT_TRUE(olc_is_field_staged(cs, "name"));
    TEST_ASSERT_FALSE(olc_is_field_staged(cs, "description"));

    json_decref(old_v);
    json_decref(new_v);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}
```

Add dispatch entries and JSON test definitions for `olccs_staged_string`, `olccs_staged_int`, `olccs_staged_flags`, `olccs_staged_bool`, `olccs_is_field_staged`.

- [ ] **Step 2: Create olc_staged.h and olc_staged.c**

Create `editors/common/olc_staged.h`:

```c
/**
 * @file olc_staged.h
 * @brief Preview helpers and staging utilities for OLC changeset mode.
 *
 * Provides functions to read the "effective" value of a field:
 * the staged value if a pending change exists, otherwise the live value.
 */

#ifndef __OLC_STAGED_H__
#define __OLC_STAGED_H__

#include "olc_changeset.h"

/* =========================================================================
 * Preview Helpers
 * ========================================================================= */

/** Returns staged string if pending, otherwise live value. */
const char *olc_staged_string(olc_changeset_t *cs, const char *field, const char *live);

/** Returns staged int if pending, otherwise live value. */
int olc_staged_int(olc_changeset_t *cs, const char *field, int live);

/** Returns staged flags if pending, otherwise live value. */
long olc_staged_flags(olc_changeset_t *cs, const char *field, long live);

/** Returns staged bool if pending, otherwise live value. */
bool olc_staged_bool(olc_changeset_t *cs, const char *field, bool live);

/** Returns raw JSON value if pending, otherwise NULL. */
json_t *olc_staged_json(olc_changeset_t *cs, const char *field);

/** Check if a field has a pending change. */
bool olc_is_field_staged(olc_changeset_t *cs, const char *field);

/* =========================================================================
 * Display Formatting
 * ========================================================================= */

/** Pending marker prefix: "{Y*{x " if field is staged, "" if not. */
const char *olc_staged_marker(olc_changeset_t *cs, const char *field);

#endif /* !def __OLC_STAGED_H__ */
```

Create `editors/common/olc_staged.c`:

```c
/**
 * @file olc_staged.c
 * @brief Preview helpers and staging utilities — implementation.
 */

#include "olc_staged.h"
#include <string.h>

const char *olc_staged_string(olc_changeset_t *cs, const char *field, const char *live)
{
    if (!cs || !field) return live;

    olc_pending_change_t *change = olc_changeset_find_change(cs, field);
    if (!change || !change->new_value) return live;

    const char *val = json_string_value(change->new_value);
    return val ? val : live;
}

int olc_staged_int(olc_changeset_t *cs, const char *field, int live)
{
    if (!cs || !field) return live;

    olc_pending_change_t *change = olc_changeset_find_change(cs, field);
    if (!change || !change->new_value || !json_is_integer(change->new_value))
        return live;

    return (int)json_integer_value(change->new_value);
}

long olc_staged_flags(olc_changeset_t *cs, const char *field, long live)
{
    if (!cs || !field) return live;

    olc_pending_change_t *change = olc_changeset_find_change(cs, field);
    if (!change || !change->new_value || !json_is_integer(change->new_value))
        return live;

    return (long)json_integer_value(change->new_value);
}

bool olc_staged_bool(olc_changeset_t *cs, const char *field, bool live)
{
    if (!cs || !field) return live;

    olc_pending_change_t *change = olc_changeset_find_change(cs, field);
    if (!change || !change->new_value) return live;

    if (json_is_true(change->new_value)) return true;
    if (json_is_false(change->new_value)) return false;
    return live;
}

json_t *olc_staged_json(olc_changeset_t *cs, const char *field)
{
    if (!cs || !field) return NULL;

    olc_pending_change_t *change = olc_changeset_find_change(cs, field);
    return change ? change->new_value : NULL;
}

bool olc_is_field_staged(olc_changeset_t *cs, const char *field)
{
    return olc_changeset_find_change(cs, field) != NULL;
}

const char *olc_staged_marker(olc_changeset_t *cs, const char *field)
{
    return olc_is_field_staged(cs, field) ? "{Y*{x " : "";
}
```

- [ ] **Step 3: Update build system**

Add `editors/common/olc_staged.c` to both `CMakeLists.txt` and `Makefile` (after `olc_changeset.c`).

- [ ] **Step 4: Build and run tests**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:olccs`
Expected: All 18 tests PASS (13 + 5 preview tests).

- [ ] **Step 5: Commit**

```bash
git add editors/common/olc_staged.h editors/common/olc_staged.c \
        tests/unit/olc_changeset_tests.c tests/data/unit/olc_changeset_tests.json \
        CMakeLists.txt Makefile
git commit -m "feat(olc): add preview helpers for staged field values

Implement olc_staged_string/int/flags/bool for reading effective field
values through the changeset overlay. olc_is_field_staged() and
olc_staged_marker() support display-time pending indicators.

5 new unit tests for preview helpers."
```


## Phase 2: Field Handlers, Commit & Revert

### Task 4: Field Handler Framework

**Goal:** Create the field handler registration system and generic handlers for scalar types. This is the dispatch mechanism that applies pending changes to live entities during commit.

**Files:**
- Create: `editors/common/olc_field_handlers.h`
- Create: `editors/common/olc_field_handlers.c`
- Modify: `tests/unit/olc_changeset_tests.c` (add handler tests)
- Modify: `tests/data/unit/olc_changeset_tests.json` (add test definitions)
- Modify: `CMakeLists.txt` (add olc_field_handlers.c)
- Modify: `Makefile` (add olc_field_handlers.c)

- [ ] **Step 1: Write failing tests for field handler registration and generic apply**

Add tests to `tests/unit/olc_changeset_tests.c` that:
- Register a handler, look it up by field path
- Test wildcard matching (`exits/*` matches `exits/north`)
- Test generic string apply (sets `char*` via `free_string`/`str_dup`)
- Test generic int apply (sets `int` field)
- Test generic bool apply (sets `bool` field)
- Test generic flags apply (sets `long` field)

- [ ] **Step 2: Create olc_field_handlers.h**

```c
/**
 * @file olc_field_handlers.h
 * @brief Field handler registration and dispatch for OLC changeset commit.
 *
 * Simple scalar fields (string, int, bool, flags) use generic handlers
 * provided by the framework. Complex fields (exits, lists, embedded structs)
 * use editor-specific handlers registered in OLC_EDITOR_DEF.
 */

#ifndef __OLC_FIELD_HANDLERS_H__
#define __OLC_FIELD_HANDLERS_H__

#include "olc_changeset.h"

/** Maximum registered field handlers per editor */
#define OLC_MAX_FIELD_HANDLERS  64

/**
 * Handler for applying a pending change to a live entity.
 * Editor-specific handlers are registered for complex field types.
 */
typedef struct olc_field_handler {
    const char         *field_path;     /**< e.g., "exits/*", "extra_descr/*" */
    olc_field_type_t    type;

    /** Serialize current live value to JSON (for changeset creation). */
    json_t *(*serialize_fn)(void *entity, const char *field_path);

    /** Apply a pending change to the live entity. Returns true on success. */
    bool (*apply_fn)(void *entity, olc_pending_change_t *change);

    /** Generate display string for preview. Caller must not free. */
    const char *(*display_fn)(json_t *value);
} olc_field_handler_t;

/* =========================================================================
 * Handler Lookup
 * ========================================================================= */

/**
 * Find the handler for a field path. Checks editor-specific handlers first,
 * then falls back to generic scalar handlers.
 *
 * @param handlers   Editor-specific handler table (NULL-terminated, may be NULL)
 * @param field_path The field being looked up
 * @param type       The field type (used for generic handler fallback)
 * @return Handler pointer, or NULL if no handler found
 */
const olc_field_handler_t *olc_find_field_handler(
    const olc_field_handler_t *handlers,
    const char *field_path, olc_field_type_t type);

/* =========================================================================
 * Generic Apply Functions (for scalar types)
 * ========================================================================= */

/**
 * Apply a string change: free_string old, str_dup new.
 * @param field_ptr  Pointer to the char* field on the entity
 * @param change     The pending change with new_value as json_string
 */
bool olc_apply_generic_string(char **field_ptr, olc_pending_change_t *change);

/** Apply an int change. */
bool olc_apply_generic_int(int *field_ptr, olc_pending_change_t *change);

/** Apply an int16_t change. */
bool olc_apply_generic_int16(int16_t *field_ptr, olc_pending_change_t *change);

/** Apply a bool change. */
bool olc_apply_generic_bool(bool *field_ptr, olc_pending_change_t *change);

/** Apply a flags (long) change. */
bool olc_apply_generic_flags(long *field_ptr, olc_pending_change_t *change);

/* =========================================================================
 * Commit API
 * ========================================================================= */

/**
 * Apply all pending changes in a changeset to the live entity.
 * Uses handler lookup for each change. Returns the number of changes applied.
 * On validation failure, sets *error_field to the failing field path.
 *
 * @param cs          Changeset to commit
 * @param entity      Live entity pointer
 * @param handlers    Editor-specific handler table (NULL-terminated, may be NULL)
 * @param error_field Set to failing field path on error (caller does not free)
 * @return Number of changes applied, or -1 on error
 */
int olc_changeset_commit(olc_changeset_t *cs, void *entity,
    const olc_field_handler_t *handlers, const char **error_field);

/**
 * Revert all pending changes (clear the changeset).
 */
void olc_changeset_revert(olc_changeset_t *cs);

/**
 * Revert a single field's pending change.
 * @return true if the field had a pending change that was removed
 */
bool olc_changeset_revert_field(olc_changeset_t *cs, const char *field_path);

#endif /* !def __OLC_FIELD_HANDLERS_H__ */
```

- [ ] **Step 3: Implement field handlers and commit/revert**

Create `editors/common/olc_field_handlers.c` with:

1. **Wildcard matching** — `field_path_matches(pattern, path)`:
   - `"exits/*"` matches `"exits/north"`, `"exits/south"`
   - Exact match takes priority over wildcard

2. **`olc_find_field_handler()`** — searches editor-specific table first, then returns NULL (generic scalars are applied directly by the commit function based on `field_type`)

3. **Generic apply functions** — each reads `change->new_value` and writes to the field pointer:
   ```c
   bool olc_apply_generic_string(char **field_ptr, olc_pending_change_t *change) {
       if (!field_ptr || !change || !change->new_value) return false;
       const char *val = json_string_value(change->new_value);
       if (!val) return false;
       free_string(*field_ptr);
       *field_ptr = str_dup(val);
       return true;
   }
   ```

4. **`olc_changeset_commit()`** — iterates pending changes, looks up handler or uses generic apply based on type, applies each one. On success clears the changeset. **Important:** If a handler is found but has `apply_fn == NULL`, log a warning and skip that change (mark as skipped in the result). This allows editors to register field patterns for display/serialization before the apply logic is written, without crashing on commit.

5. **`olc_changeset_revert()`** — calls `olc_changeset_clear(cs)`

6. **`olc_changeset_revert_field()`** — calls `olc_changeset_remove_change(cs, field_path)`

- [ ] **Step 4: Update build system and run tests**

Add `editors/common/olc_field_handlers.c` to both `CMakeLists.txt` and `Makefile`.

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:olccs`
Expected: All tests PASS including new handler/commit tests.

- [ ] **Step 5: Commit**

```bash
git add editors/common/olc_field_handlers.h editors/common/olc_field_handlers.c \
        tests/unit/olc_changeset_tests.c tests/data/unit/olc_changeset_tests.json \
        CMakeLists.txt Makefile
git commit -m "feat(olc): add field handler framework with generic scalar apply and commit/revert

Implement olc_field_handler_t for editor-specific complex field dispatch.
Generic handlers for string, int, int16, bool, flags. Wildcard field path
matching. olc_changeset_commit() applies all pending changes.
olc_changeset_revert() and revert_field() for discarding changes."
```


## Phase 3: Editor Integration

### Task 5: OLC_CHANGE_STAGED Enum & Edit State on Descriptor

**Goal:** Extend the OLC framework to recognize staged mode. Add the edit state struct to the descriptor so changesets persist across editor commands.

**Files:**
- Modify: `editors/common/olc_editor.h:160-172` (add enum value)
- Modify: `editors/common/olc_editor.h:285-328` (add field_handlers to OLC_EDITOR_DEF)
- Modify: `merc.h:2020-2027` (add olc_edit_state_t* to DESCRIPTOR_DATA)
- Modify: `mem.c:380-393` (clean up edit state in free_descriptor)
- Modify: `editors/common/olc_editor.c:344-374` (add STAGED case to olc_mark_changed)
- Modify: `editors/common/olc_editor.c:420-447` (init edit state in olc_editor_enter)

- [ ] **Step 1: Add OLC_CHANGE_STAGED to the enum**

In `editors/common/olc_editor.h`, at line 171 (before the closing `}`), add:

```c
    /** Field-level overlay with commit semantics. Changes are staged
     *  in memory and only applied to the live entity on commit. */
    OLC_CHANGE_STAGED,
```

- [ ] **Step 2: Add field_handlers to OLC_EDITOR_DEF**

In `editors/common/olc_editor.h`, in the `olc_editor_def` struct (after `get_history_fn`), add:

```c
    /* --- Staged mode configuration --- */
    const struct olc_field_handler *field_handlers;  /**< Complex field handlers (NULL-terminated, may be NULL) */
```

Add the include for field handler types at the top of the file:
```c
#include "olc_field_handlers.h"
```

Note: `olc_changeset.h` is already included transitively through `olc_field_handlers.h`.

- [ ] **Step 3: Add olc_edit_state_t to DESCRIPTOR_DATA**

In `merc.h`, in the `DESCRIPTOR_DATA` struct, after the existing OLC fields (around line 2027, after `void *editor_ptr;`), add:

```c
    struct olc_edit_state *olc_state;          /**< Staged editing state (changesets, string sessions) */
```

Add forward declaration near the top of `merc.h` (with other struct forward declarations):
```c
typedef struct olc_edit_state olc_edit_state_t;
```

- [ ] **Step 4: Clean up edit state in free_descriptor**

In `mem.c:free_descriptor()`, before `INVALIDATE(d)` (around line 389), add:

```c
    if (d->olc_state) {
        /* Auto-draft is handled by the caller before reaching here (Phase 5).
         * This is a safety net to prevent memory leaks. */
        olc_edit_state_destroy(d->olc_state);
        d->olc_state = NULL;
    }
```

Add `#include "editors/common/olc_changeset.h"` at the top of `mem.c`.

- [ ] **Step 5: Add STAGED case to olc_mark_changed**

In `editors/common/olc_editor.c:olc_mark_changed()`, add a case in the switch:

```c
        case OLC_CHANGE_STAGED:
            /* Staged mode: no automatic dirty marking. Changes accumulate in
             * the changeset and are only applied on commit. The SENTIENCE_DIRTY_EDITOR
             * flag (for prompt indicator) is set separately. */
            break;
```

- [ ] **Step 6: Initialize edit state in olc_editor_enter**

In `editors/common/olc_editor.c:olc_editor_enter()`, when the editor's `change_mode == OLC_CHANGE_STAGED`:

```c
    /* Create edit state if this is the first staged editor */
    if (def->change_mode == OLC_CHANGE_STAGED && !ch->desc->olc_state) {
        ch->desc->olc_state = olc_edit_state_create();
    }

    /* Create changeset for this entity if in staged mode */
    if (def->change_mode == OLC_CHANGE_STAGED && ch->desc->olc_state) {
        WNUM_LOAD wnum = /* extract from pEdit based on editor_type */;
        olc_changeset_t *cs = olc_edit_state_find_changeset(
            ch->desc->olc_state, def->editor_type, wnum);
        if (!cs) {
            cs = olc_changeset_create(def->editor_type, wnum,
                /* entity_label */, ch->name);
            llist_add(ch->desc->olc_state->active_changesets, cs);
        }
    }
```

The wnum extraction needs a helper — add to `olc_editor.c`:
```c
static WNUM_LOAD olc_get_entity_wnum(const OLC_EDITOR_DEF *def, void *pEdit)
{
    WNUM_LOAD wnum = { 0, 0 };
    if (!pEdit) return wnum;

    switch (def->editor_type) {
        case ED_ROOM: {
            ROOM_INDEX_DATA *r = (ROOM_INDEX_DATA *)pEdit;
            wnum.auid = r->area->uid;
            wnum.vnum = r->vnum;
            break;
        }
        case ED_MOBILE: {
            MOB_INDEX_DATA *m = (MOB_INDEX_DATA *)pEdit;
            wnum.auid = m->area->uid;
            wnum.vnum = m->vnum;
            break;
        }
        case ED_OBJECT: {
            OBJ_INDEX_DATA *o = (OBJ_INDEX_DATA *)pEdit;
            wnum.auid = o->area->uid;
            wnum.vnum = o->vnum;
            break;
        }
        case ED_AREA: {
            AREA_DATA *a = (AREA_DATA *)pEdit;
            wnum.auid = a->uid;
            wnum.vnum = 0;
            break;
        }
        default:
            break;
    }
    return wnum;
}
```

- [ ] **Step 7: Build and verify no regressions**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test`
Expected: Baseline tests still pass. New enum value doesn't break anything since no editor uses it yet.

- [ ] **Step 8: Commit**

```bash
git add editors/common/olc_editor.h editors/common/olc_editor.c \
        merc.h mem.c
git commit -m "feat(olc): add OLC_CHANGE_STAGED mode and edit state on descriptor

- Append OLC_CHANGE_STAGED (4) to olc_change_mode_t enum
- Add field_handlers to OLC_EDITOR_DEF for complex field dispatch
- Add olc_edit_state_t* to DESCRIPTOR_DATA for per-builder changesets
- Initialize edit state and changeset on staged editor open
- Clean up edit state in free_descriptor()
- Add STAGED case to olc_mark_changed() (no-op, handled by commit)"
```

### Task 6: Built-in Staged Commands & olc_cmd_* Staging Path

**Goal:** Wire the staged editor commands (commit, revert, pending, savedraft, loaddraft, discardraft) into the editor interpreter, and modify `olc_cmd_*` helpers to store changes in the overlay instead of writing directly when in staged mode.

**Files:**
- Modify: `editors/common/olc_editor.c:800-966` (add staged command dispatch)
- Modify: `editors/common/olc_commands.c:31-329` (add staging code path)
- Modify: `editors/common/olc_staged.c` (add staged command implementations)
- Modify: `editors/common/olc_staged.h` (add staged command declarations)

- [ ] **Step 1: Add staged command functions to olc_staged.h/c**

Add to `editors/common/olc_staged.h`:

```c
/* =========================================================================
 * Staged Editor Commands
 * ========================================================================= */

/** Handle 'commit [comment]' — apply all pending, save entity, record history. */
void olc_staged_cmd_commit(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
    void *pEdit, char *argument);

/** Handle 'commit group [comment]' — commit all open staged editors atomically. */
void olc_staged_cmd_commit_group(CHAR_DATA *ch, char *argument);

/** Handle 'revert [field]' — discard pending changes. */
void olc_staged_cmd_revert(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
    void *pEdit, char *argument);

/** Handle 'pending' — show table of pending changes. */
void olc_staged_cmd_pending(CHAR_DATA *ch, const OLC_EDITOR_DEF *def, void *pEdit);

/* =========================================================================
 * Staging Wrappers for olc_cmd_* Helpers
 * ========================================================================= */

/**
 * Get the active changeset for the current editor.
 * Returns NULL if not in staged mode or no changeset found.
 */
olc_changeset_t *olc_get_active_changeset(CHAR_DATA *ch, const OLC_EDITOR_DEF *def);
```

- [ ] **Step 2: Implement staged command handlers in olc_staged.c**

Key implementations:

**`olc_staged_cmd_commit()`:**
1. Get active changeset
2. Check for empty changeset (warn and return)
3. Check stale-commit warning (compare `created_at` with entity's latest history entry)
4. Call `olc_changeset_commit(cs, pEdit, def->field_handlers, &error_field)`
5. On success: record history, trigger area/entity save, send feedback
6. On failure: show error with field name

**`olc_staged_cmd_revert()`:**
1. If argument empty: revert all (`olc_changeset_revert(cs)`)
2. If argument provided: revert single field (`olc_changeset_revert_field(cs, field)`)
3. Send feedback showing what was reverted

**`olc_staged_cmd_pending()`:**
1. Get changeset, iterate changes
2. Display table: `Field | Type | Old → New`
3. Show count at bottom

- [ ] **Step 3: Wire staged commands into olc_editor_interp**

In `editors/common/olc_editor.c:olc_editor_interp()`, before the command table lookup (around line 904), add staged command dispatch:

```c
    /* Staged mode built-in commands */
    if (def->change_mode == OLC_CHANGE_STAGED) {
        if (!str_cmp(cmd, "commit")) {
            char first_arg[MAX_INPUT_LENGTH];
            char *rest_of_args = one_argument(argument, first_arg);
            if (!str_cmp(first_arg, "group"))
                olc_staged_cmd_commit_group(ch, rest_of_args);
            else
                olc_staged_cmd_commit(ch, def, ch->desc->pEdit, argument);
            return;
        }
        if (!str_cmp(cmd, "revert")) {
            olc_staged_cmd_revert(ch, def, ch->desc->pEdit, argument);
            return;
        }
        if (!str_cmp(cmd, "pending")) {
            olc_staged_cmd_pending(ch, def, ch->desc->pEdit);
            return;
        }
        if (!str_cmp(cmd, "savedraft")) {
            /* Phase 5: olc_staged_cmd_savedraft(ch, def, ch->desc->pEdit); */
            send_to_char("Draft saving not yet implemented.\n\r", ch);
            return;
        }
        if (!str_cmp(cmd, "loaddraft")) {
            send_to_char("Draft loading not yet implemented.\n\r", ch);
            return;
        }
        if (!str_cmp(cmd, "discardraft")) {
            send_to_char("Draft discarding not yet implemented.\n\r", ch);
            return;
        }
    }
```

Also modify the `"done"` handler to warn about pending changes:

```c
    if (!str_cmp(cmd, "done")) {
        if (def->change_mode == OLC_CHANGE_STAGED) {
            olc_changeset_t *cs = olc_get_active_changeset(ch, def);
            if (cs && olc_changeset_count(cs) > 0) {
                printf_to_char(ch,
                    "{YWarning:{x You have %d uncommitted change%s. "
                    "Use '{Wcommit{x' to save or '{Wrevert{x' to discard.\n\r",
                    olc_changeset_count(cs),
                    olc_changeset_count(cs) == 1 ? "" : "s");
                return;
            }
        }
        // ... existing done logic ...
    }
```

- [ ] **Step 4: Add staging code path to olc_cmd_* helpers**

In `editors/common/olc_commands.c`, modify each helper to check for staged mode. The pattern is the same for all helpers. Example for `olc_cmd_string()`:

In `olc_cmd_string()`, at the beginning after argument validation, add:

```c
    /* Check for staged mode — store in overlay instead of writing directly */
    const OLC_EDITOR_DEF *def = olc_get_current_editor_def(ch);
    if (def && def->change_mode == OLC_CHANGE_STAGED) {
        olc_changeset_t *cs = olc_get_active_changeset(ch, def);
        if (cs) {
            /* Enforce safety limits */
            if (olc_changeset_count(cs) >= OLC_MAX_PENDING_PER_ENTITY) {
                send_to_char("{RLimit:{x Too many pending changes. Commit or revert first.\n\r", ch);
                return false;
            }
            if (olc_edit_state_total_pending(ch->desc->olc_state)
                    >= OLC_MAX_PENDING_PER_BUILDER) {
                send_to_char("{RLimit:{x Builder-wide change limit reached. Commit or revert.\n\r", ch);
                return false;
            }

            json_t *old_val = json_string(*field_ptr ? *field_ptr : "");
            json_t *new_val = json_string(argument);

            olc_pending_change_t *result = olc_changeset_add_change(
                cs, label, OLC_FIELD_STRING, old_val, new_val);

            json_decref(old_val);
            json_decref(new_val);

            if (result) {
                printf_to_char(ch, "{G[STAGED]{x %s set to: %s\n\r", label, argument);
                cmd_record(record_fn, ctx, ch, label, *field_ptr, argument);
            } else {
                printf_to_char(ch, "%s reverted to original value.\n\r", label);
            }
            return result != NULL;
        }
    }
```

Apply the same pattern to `olc_cmd_number()`, `olc_cmd_number_i16()`, `olc_cmd_flag_toggle()`, `olc_cmd_type_set()`, `olc_cmd_type_set_i16()`, `olc_cmd_bool()`, adjusting the JSON value creation for each type:
- `olc_cmd_number`: `json_integer(*field_ptr)` / `json_integer(value)`
- `olc_cmd_flag_toggle`: `json_integer(*field_ptr)` / `json_integer(*field_ptr ^ value)`
- `olc_cmd_bool`: `json_boolean(*field_ptr)` / `json_boolean(new_val)`

Add `olc_get_current_editor_def()` helper that retrieves the current editor's def from the editor type stored on the descriptor.

- [ ] **Step 5: Build and run tests**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test`
Expected: All tests pass. No regressions.

- [ ] **Step 6: Commit**

```bash
git add editors/common/olc_editor.c editors/common/olc_commands.c \
        editors/common/olc_staged.h editors/common/olc_staged.c
git commit -m "feat(olc): wire staged commands and olc_cmd_* staging path

Built-in commands for staged editors: commit, revert, pending.
Draft commands stubbed for Phase 5. Done warns about uncommitted changes.
All olc_cmd_* helpers (string, number, flags, type, bool) now stage
changes in the overlay when change_mode == OLC_CHANGE_STAGED instead
of writing directly to entity fields."
```

### Task 7: Display Pending Markers

**Goal:** Modify the OLC display helpers to show a visual marker on fields that have pending staged changes, so builders can see at a glance what they've modified.

**Files:**
- Modify: `editors/common/olc_display.h` (add changeset parameter to display functions)
- Modify: `editors/common/olc_display.c` (render pending markers)
- Modify: `gmcp_sentience.h` (add SENTIENCE_DIRTY_EDITOR flag — if not already done in Task 8)
- Modify: `gmcp_sentience.c` (consume SENTIENCE_DIRTY_EDITOR in prompt handler)

- [ ] **Step 1: Add changeset parameter to display functions**

The existing display functions don't know about changesets. We need to add an optional changeset parameter. To avoid breaking all existing callers, add new `_staged` variants or pass the changeset through the `OLC_LAYOUT_CTX`:

The cleanest approach: add `olc_changeset_t *changeset` to `OLC_LAYOUT_CTX` (the context struct already passed to all display functions). This way, existing callers don't change — they just get `ctx->changeset == NULL`.

In `editors/common/olc_display.h`, add to the `OLC_LAYOUT_CTX` struct (look for `olc_layout_ctx`):
```c
    olc_changeset_t     *changeset;     /**< Active changeset for pending markers (may be NULL) */
```

- [ ] **Step 2: Modify display functions to show pending markers**

In `editors/common/olc_display.c`, modify `olc_display_string()` and similar functions. Before rendering the value, check if the field is staged:

```c
void olc_display_string(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                        const char *label, const char *command,
                        const char *value)
{
    /* ... existing label formatting ... */

    const char *marker = olc_staged_marker(ctx->changeset, command);
    const char *display_value = value;

    /* If staged, show the staged value instead */
    if (ctx->changeset && command) {
        const char *staged = olc_staged_string(ctx->changeset, command, value);
        if (staged != value) display_value = staged;
    }

    /* Render with marker prefix if staged */
    if (IS_NULLSTR(display_value)) {
        snprintf(buf, sizeof(buf), "%s%s%*s %s%s(unset){x\n\r",
                 marker, theme->label, label_buf, pad, "", theme->unset);
    } else {
        snprintf(buf, sizeof(buf), "%s%s%*s %s%s{x\n\r",
                 marker, theme->label, label_buf, pad, "", theme->value, display_value);
    }
    /* ... */
}
```

Apply the same pattern to `olc_display_number()`, `olc_display_bool()`, `olc_display_flags()`, `olc_display_type()`.

- [ ] **Step 3: Set changeset on ctx when entering staged editor's show function**

In `editors/common/olc_editor.c`, where the show function is called (in the tab display logic), set `ctx->changeset` before calling the tab's show_fn:

```c
    if (def->change_mode == OLC_CHANGE_STAGED) {
        ctx->changeset = olc_get_active_changeset(ch, def);
    }
```

- [ ] **Step 4: Set SENTIENCE_DIRTY_EDITOR flag for prompt indicator**

In `gmcp_sentience.h`, the `SENTIENCE_DIRTY_EDITOR` flag is defined in Task 8. In the staging code paths (Task 6's `olc_cmd_*` staging wrappers), set this flag after successfully staging a change so the OLC prompt shows the editor-has-changes indicator:

```c
    /* After successful olc_changeset_add_change(): */
    SET_BIT(ch->desc->pProtocol->sentience_dirty, SENTIENCE_DIRTY_EDITOR);
```

In `gmcp_sentience.c`, add an `update_editor_dirty()` function (or inline in the existing dirty-flag consumer) that sends the prompt indicator. The indicator should convey "you have N uncommitted changes" to the client:

```c
/* In the sentience_dirty handler for SENTIENCE_DIRTY_EDITOR: */
if (IS_SET(dirty, SENTIENCE_DIRTY_EDITOR)) {
    /* Prompt is handled by the OLC framework's prompt function.
     * Just clear the flag — the prompt will re-read from changeset. */
    REMOVE_BIT(d->pProtocol->sentience_dirty, SENTIENCE_DIRTY_EDITOR);
}
```

- [ ] **Step 5: Build and run tests**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test`
Expected: All tests pass. Display changes are visual-only, no new test failures.

- [ ] **Step 6: Commit**

```bash
git add editors/common/olc_display.h editors/common/olc_display.c \
        editors/common/olc_editor.c gmcp_sentience.h gmcp_sentience.c
git commit -m "feat(olc): add pending-change visual markers and dirty flag

Display functions now show {Y*{x prefix for fields with staged changes.
OLC_LAYOUT_CTX carries the active changeset. Preview values shown
through olc_staged_* helpers. SENTIENCE_DIRTY_EDITOR prompt indicator
set on staging, consumed in prompt handler. No visual change for
non-staged editors."
```


## Phase 4: GMCP Editor Protocol

### Task 8: Protocol Registration & Outgoing Messages

**Goal:** Register the Sentience.Editor.* GMCP messages in the protocol layer and implement all server→client message builders.

**Files:**
- Create: `gmcp_editor.h`
- Create: `gmcp_editor.c`
- Modify: `protocol.h:306-319` (add GMCP_SENTIENCE_EDITOR_* enum values)
- Modify: `protocol.c:3364-3376` (add GMCPReceiveTable entries)
- Modify: `protocol.c:3809+` (add ParseGMCP dispatch)
- Modify: `gmcp_sentience.h:25-40` (add SENTIENCE_DIRTY_EDITOR flag)
- Create: `tests/integration/gmcp_editor_tests.c`
- Create: `tests/data/integration/gmcp_editor_tests.json`
- Modify: `CMakeLists.txt` (add gmcp_editor.c, test file)
- Modify: `Makefile` (add gmcp_editor.c, test file)
- Modify: `tests/framework/test_dispatcher.c` (add handler)
- Modify: `tests/framework/test_modules.h` (add declaration)
- Modify: `tests/data/test_config.json` (add test suite)

- [ ] **Step 1: Write failing tests for GMCP message JSON building**

Create `tests/integration/gmcp_editor_tests.c`:

```c
/**
 * GMCP Editor Protocol Tests
 *
 * Tests JSON message building for Sentience.Editor.* messages.
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../framework/test_framework.h"
#include "../../gmcp_editor.h"
#include <jansson.h>
#include <string.h>

static test_result_t test_build_field_message(test_case_t *test)
{
    json_t *msg = gmcp_editor_build_field("room:5#3001", "name",
        json_string("A Glowing Cavern"), "string", true);

    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_STR_EQ("room:5#3001", json_string_value(json_object_get(msg, "entity_id")));
    TEST_ASSERT_STR_EQ("name", json_string_value(json_object_get(msg, "field")));
    TEST_ASSERT_STR_EQ("A Glowing Cavern", json_string_value(json_object_get(msg, "value")));
    TEST_ASSERT_STR_EQ("string", json_string_value(json_object_get(msg, "type")));
    TEST_ASSERT_TRUE(json_is_true(json_object_get(msg, "is_pending")));
    TEST_ASSERT_INT_EQ(1, (int)json_integer_value(json_object_get(msg, "_v")));

    json_decref(msg);
    return TEST_PASS;
}

static test_result_t test_build_close_message(test_case_t *test)
{
    json_t *msg = gmcp_editor_build_close("room:5#3001", "done");

    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_STR_EQ("room:5#3001", json_string_value(json_object_get(msg, "entity_id")));
    TEST_ASSERT_STR_EQ("done", json_string_value(json_object_get(msg, "reason")));
    TEST_ASSERT_INT_EQ(1, (int)json_integer_value(json_object_get(msg, "_v")));

    json_decref(msg);
    return TEST_PASS;
}

static test_result_t test_build_error_message(test_case_t *test)
{
    json_t *msg = gmcp_editor_build_error("room:5#3001", "level",
        "out_of_range", "Level must be between 1 and 200.");

    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_STR_EQ("out_of_range", json_string_value(json_object_get(msg, "error")));
    TEST_ASSERT_STR_EQ("Level must be between 1 and 200.",
        json_string_value(json_object_get(msg, "message")));

    json_decref(msg);
    return TEST_PASS;
}

static test_result_t test_build_commit_result(test_case_t *test)
{
    json_t *msg = gmcp_editor_build_commit_result("room:5#3001", "success", 3);

    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_STR_EQ("success", json_string_value(json_object_get(msg, "status")));
    TEST_ASSERT_INT_EQ(3, (int)json_integer_value(json_object_get(msg, "changes_applied")));

    json_decref(msg);
    return TEST_PASS;
}

static test_result_t test_build_state_message(test_case_t *test)
{
    /* Build a state message with some pending changes */
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *old_v = json_string("Old Name");
    json_t *new_v = json_string("New Name");
    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, old_v, new_v);

    json_t *msg = gmcp_editor_build_state("room:5#3001", cs, false);

    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_STR_EQ("room:5#3001", json_string_value(json_object_get(msg, "entity_id")));
    TEST_ASSERT_INT_EQ(1, (int)json_integer_value(json_object_get(msg, "pending_count")));

    json_t *changes = json_object_get(msg, "changes");
    TEST_ASSERT_NOT_NULL(changes);
    TEST_ASSERT_TRUE(json_is_array(changes));
    TEST_ASSERT_INT_EQ(1, (int)json_array_size(changes));

    json_decref(old_v);
    json_decref(new_v);
    json_decref(msg);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}

/* Dispatch */
test_result_t run_gmcp_editor_test_case(test_case_t *test)
{
    if (!test || !test->test_type) return TEST_ERROR;

    const char *type = test->test_type;
    if (!strcmp(type, "gmcped_build_field"))         return test_build_field_message(test);
    if (!strcmp(type, "gmcped_build_close"))         return test_build_close_message(test);
    if (!strcmp(type, "gmcped_build_error"))         return test_build_error_message(test);
    if (!strcmp(type, "gmcped_build_commit_result")) return test_build_commit_result(test);
    if (!strcmp(type, "gmcped_build_state"))         return test_build_state_message(test);

    return TEST_SKIP;
}

#endif /* BUILD_TESTS */
```

Create `tests/data/integration/gmcp_editor_tests.json` with 5 test entries following the pattern from Task 1.

Register `gmcped_` handler in `test_dispatcher.c` and `test_modules.h`. Add test suite to `test_config.json`.

- [ ] **Step 2: Create gmcp_editor.h**

```c
/**
 * @file gmcp_editor.h
 * @brief GMCP Editor Protocol — Sentience.Editor.* messages.
 *
 * Handles bidirectional GMCP communication for OLC editors.
 * Server→client: Open, Field, State, Close, Error, CommitResult, StringEdit.*
 * Client→server: Set, Commit, Revert, Request, StringEdit.Save/Cancel, Draft.*
 *
 * @see docs/superpowers/specs/2026-03-26-olc-changeset-gmcp-design.md
 */

#ifndef __GMCP_EDITOR_H__
#define __GMCP_EDITOR_H__

#include "merc.h"
#include "editors/common/olc_changeset.h"
#include <jansson.h>

/* =========================================================================
 * Outgoing Message Builders (Server → Client)
 * ========================================================================= */

/**
 * Build Sentience.Editor.Open JSON.
 * Caller must json_decref() the result.
 */
json_t *gmcp_editor_build_open(const struct olc_editor_def *def,
    const char *entity_id, const char *entity_name,
    void *pEdit, olc_changeset_t *cs, bool has_draft);

/** Build Sentience.Editor.Field JSON. */
json_t *gmcp_editor_build_field(const char *entity_id, const char *field,
    json_t *value, const char *type_str, bool is_pending);

/** Build Sentience.Editor.State JSON from a changeset. */
json_t *gmcp_editor_build_state(const char *entity_id,
    olc_changeset_t *cs, bool draft_restored);

/** Build Sentience.Editor.Close JSON. reason: "done", "forced", "disconnect" */
json_t *gmcp_editor_build_close(const char *entity_id, const char *reason);

/** Build Sentience.Editor.Error JSON. */
json_t *gmcp_editor_build_error(const char *entity_id, const char *field,
    const char *error_code, const char *message);

/** Build Sentience.Editor.CommitResult JSON (single entity). */
json_t *gmcp_editor_build_commit_result(const char *entity_id,
    const char *status, int changes_applied);

/** Build Sentience.Editor.CommitResult JSON (group commit). */
json_t *gmcp_editor_build_group_commit_result(int group_id,
    json_t *results_array, const char *comment);

/* =========================================================================
 * Send Helpers (combines build + sentience_send_package)
 * ========================================================================= */

/** Send Editor.Field to descriptor. */
void gmcp_editor_send_field(descriptor_t *d, const char *entity_id,
    const char *field, json_t *value, const char *type_str, bool is_pending);

/** Send Editor.State to descriptor. */
void gmcp_editor_send_state(descriptor_t *d, const char *entity_id,
    olc_changeset_t *cs, bool draft_restored);

/** Send Editor.Close to descriptor. */
void gmcp_editor_send_close(descriptor_t *d, const char *entity_id,
    const char *reason);

/** Send Editor.Error to descriptor. */
void gmcp_editor_send_error(descriptor_t *d, const char *entity_id,
    const char *field, const char *error_code, const char *message);

/** Send Editor.CommitResult to descriptor. */
void gmcp_editor_send_commit_result(descriptor_t *d, const char *entity_id,
    const char *status, int changes_applied);

/* =========================================================================
 * Incoming Message Handler (Client → Server)
 * ========================================================================= */

/**
 * Dispatch incoming Sentience.Editor.* GMCP message.
 * Called from ParseGMCP() with the raw JSON payload string.
 *
 * @param d          Descriptor that sent the message
 * @param module     Which GMCP_SENTIENCE_EDITOR_* enum value
 * @param json_str   Raw JSON string (starts after the module name)
 */
void sentience_handle_editor(descriptor_t *d, int module, const char *json_str);

/* =========================================================================
 * Entity ID Formatting
 * ========================================================================= */

/** Format entity ID string: "type:auid#vnum" or "area:auid". Thread-safe (static buf). */
const char *gmcp_editor_entity_id(int editor_type, WNUM_LOAD wnum);

/** Parse entity ID string back to editor_type and wnum. Returns true on success. */
bool gmcp_editor_parse_entity_id(const char *entity_id,
    int *editor_type, WNUM_LOAD *wnum);

#endif /* !def __GMCP_EDITOR_H__ */
```

- [ ] **Step 3: Implement gmcp_editor.c — message builders**

Create `gmcp_editor.c` with all message builder implementations. Each builder creates a Jansson JSON object matching the spec format. The send helpers use `sentience_send_package()` from `gmcp_sentience.c`.

Key patterns:
```c
json_t *gmcp_editor_build_field(const char *entity_id, const char *field,
    json_t *value, const char *type_str, bool is_pending)
{
    json_t *msg = json_object();
    json_object_set_new(msg, "entity_id", json_string(entity_id));
    json_object_set_new(msg, "field", json_string(field));
    json_object_set(msg, "value", value);  /* borrowed ref */
    json_object_set_new(msg, "type", json_string(type_str));
    json_object_set_new(msg, "is_pending", json_boolean(is_pending));
    json_object_set_new(msg, "_v", json_integer(1));
    return msg;
}

void gmcp_editor_send_field(descriptor_t *d, const char *entity_id,
    const char *field, json_t *value, const char *type_str, bool is_pending)
{
    if (!d || !d->pProtocol || !d->pProtocol->bGMCP) return;

    json_t *msg = gmcp_editor_build_field(entity_id, field, value, type_str, is_pending);
    sentience_send_package(d, "Sentience.Editor.Field", msg);
    /* sentience_send_package handles json_decref */
}
```

Entity ID formatting:
```c
const char *gmcp_editor_entity_id(int editor_type, WNUM_LOAD wnum)
{
    static char buf[64];
    const char *type_name;
    switch (editor_type) {
        case ED_ROOM:   type_name = "room"; break;
        case ED_MOBILE: type_name = "mob";  break;
        case ED_OBJECT: type_name = "obj";  break;
        case ED_AREA:   type_name = "area"; break;
        default:        type_name = "unknown"; break;
    }
    if (editor_type == ED_AREA)
        snprintf(buf, sizeof(buf), "%s:%ld", type_name, wnum.auid);
    else
        snprintf(buf, sizeof(buf), "%s:%ld#%ld", type_name, wnum.auid, wnum.vnum);
    return buf;
}
```

- [ ] **Step 4: Add GMCP enum values and receive table entries**

In `protocol.h`, before `GMCP_RECEIVE_MAX` in the enum (line ~317):
```c
   GMCP_SENTIENCE_EDITOR_SET,             // 8
   GMCP_SENTIENCE_EDITOR_COMMIT,          // 9
   GMCP_SENTIENCE_EDITOR_REVERT,          // 10
   GMCP_SENTIENCE_EDITOR_REQUEST,         // 11
   GMCP_SENTIENCE_EDITOR_STRING_SAVE,     // 12
   GMCP_SENTIENCE_EDITOR_STRING_CANCEL,   // 13
   GMCP_SENTIENCE_EDITOR_DRAFT_SAVE,      // 14
   GMCP_SENTIENCE_EDITOR_DRAFT_LOAD,      // 15
   GMCP_RECEIVE_MAX                       // 16
```

In `protocol.c` `GMCPReceiveTable[]`, before the sentinel:
```c
   { GMCP_SENTIENCE_EDITOR_SET,           "Sentience.Editor.Set"             },
   { GMCP_SENTIENCE_EDITOR_COMMIT,        "Sentience.Editor.Commit"          },
   { GMCP_SENTIENCE_EDITOR_REVERT,        "Sentience.Editor.Revert"          },
   { GMCP_SENTIENCE_EDITOR_REQUEST,       "Sentience.Editor.Request"         },
   { GMCP_SENTIENCE_EDITOR_STRING_SAVE,   "Sentience.Editor.StringEdit.Save" },
   { GMCP_SENTIENCE_EDITOR_STRING_CANCEL, "Sentience.Editor.StringEdit.Cancel" },
   { GMCP_SENTIENCE_EDITOR_DRAFT_SAVE,    "Sentience.Editor.Draft.Save"      },
   { GMCP_SENTIENCE_EDITOR_DRAFT_LOAD,    "Sentience.Editor.Draft.Load"      },
```

In `protocol.c` `ParseGMCP()` switch, add:
```c
      case GMCP_SENTIENCE_EDITOR_SET:
      case GMCP_SENTIENCE_EDITOR_COMMIT:
      case GMCP_SENTIENCE_EDITOR_REVERT:
      case GMCP_SENTIENCE_EDITOR_REQUEST:
      case GMCP_SENTIENCE_EDITOR_STRING_SAVE:
      case GMCP_SENTIENCE_EDITOR_STRING_CANCEL:
      case GMCP_SENTIENCE_EDITOR_DRAFT_SAVE:
      case GMCP_SENTIENCE_EDITOR_DRAFT_LOAD:
          sentience_handle_editor(apDescriptor, GMCPReceiveTable[i].module,
                                  string + t[1].start);
          break;
```

- [ ] **Step 5: Add SENTIENCE_DIRTY_EDITOR flag**

In `gmcp_sentience.h`, add to the `sentience_dirty_t` enum:
```c
    SENTIENCE_DIRTY_EDITOR     = (1 << 14), // 0x4000
```

Update `SENTIENCE_DIRTY_ALL` to `0x7FFF`.

- [ ] **Step 6: Update build system and register tests**

Add `gmcp_editor.c` to both `CMakeLists.txt` and `Makefile` source lists.
Add `tests/integration/gmcp_editor_tests.c` to both test lists.
Register `gmcped_` handler in test framework.
Add `gmcp_editor_unit_tests` to `test_config.json`.

- [ ] **Step 7: Build and run tests**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:gmcped`
Expected: All 5 GMCP editor tests PASS.

Run: `cd /sentience && ./sent -test`
Expected: Baseline + new tests pass. No regressions.

- [ ] **Step 8: Commit**

```bash
git add gmcp_editor.h gmcp_editor.c protocol.h protocol.c \
        gmcp_sentience.h tests/integration/gmcp_editor_tests.c \
        tests/data/integration/gmcp_editor_tests.json \
        tests/framework/test_dispatcher.c tests/framework/test_modules.h \
        tests/data/test_config.json CMakeLists.txt Makefile
git commit -m "feat(gmcp): add Sentience.Editor.* protocol registration and message builders

Register 8 GMCP receive entries for Editor.Set/Commit/Revert/Request/
StringEdit.Save/Cancel/Draft.Save/Load. Implement outgoing message
builders for Field, State, Close, Error, CommitResult. Entity ID
formatting (type:auid#vnum). SENTIENCE_DIRTY_EDITOR flag for prompt.

5 unit tests for JSON message format validation."
```

### Task 9: GMCP Incoming Handlers

**Goal:** Implement the server-side handlers for client→server GMCP editor messages: Set, Commit, Revert, Request.

**Files:**
- Modify: `gmcp_editor.c` (implement sentience_handle_editor and sub-handlers)

- [ ] **Step 1: Implement sentience_handle_editor dispatcher**

```c
void sentience_handle_editor(descriptor_t *d, int module, const char *json_str)
{
    if (!d || !d->character || IS_NPC(d->character))
        return;

    /* Parse JSON payload using Jansson */
    json_error_t error;
    json_t *payload = json_loads(json_str, 0, &error);
    if (!payload) {
        log_string("GMCP Editor: JSON parse error: %s", error.text);
        return;
    }

    switch (module) {
        case GMCP_SENTIENCE_EDITOR_SET:
            handle_editor_set(d, payload);
            break;
        case GMCP_SENTIENCE_EDITOR_COMMIT:
            handle_editor_commit(d, payload);
            break;
        case GMCP_SENTIENCE_EDITOR_REVERT:
            handle_editor_revert(d, payload);
            break;
        case GMCP_SENTIENCE_EDITOR_REQUEST:
            handle_editor_request(d, payload);
            break;
        case GMCP_SENTIENCE_EDITOR_STRING_SAVE:
        case GMCP_SENTIENCE_EDITOR_STRING_CANCEL:
            /* Phase 5: handle_editor_string(d, module, payload); */
            break;
        case GMCP_SENTIENCE_EDITOR_DRAFT_SAVE:
        case GMCP_SENTIENCE_EDITOR_DRAFT_LOAD:
            /* Phase 5: handle_editor_draft(d, module, payload); */
            break;
    }

    json_decref(payload);
}
```

- [ ] **Step 2: Implement handle_editor_set**

```c
static void handle_editor_set(descriptor_t *d, json_t *payload)
{
    const char *entity_id = json_string_value(json_object_get(payload, "entity_id"));
    const char *field = json_string_value(json_object_get(payload, "field"));
    json_t *value = json_object_get(payload, "value");

    if (!entity_id || !field || !value) {
        gmcp_editor_send_error(d, entity_id ? entity_id : "",
            field ? field : "", "invalid_request", "Missing required fields.");
        return;
    }

    /* Validate: entity must be open in editor, character has permission */
    int editor_type;
    WNUM_LOAD wnum;
    if (!gmcp_editor_parse_entity_id(entity_id, &editor_type, &wnum)) {
        gmcp_editor_send_error(d, entity_id, field,
            "invalid_entity", "Invalid entity ID format.");
        return;
    }

    /* Find the active changeset */
    olc_changeset_t *cs = olc_edit_state_find_changeset(
        d->olc_state, editor_type, wnum);
    if (!cs) {
        gmcp_editor_send_error(d, entity_id, field,
            "not_editing", "Entity is not open for editing.");
        return;
    }

    /* Determine field type from the value JSON type */
    olc_field_type_t ftype = OLC_FIELD_STRING;  /* default */
    if (json_is_integer(value)) ftype = OLC_FIELD_INT;
    else if (json_is_boolean(value)) ftype = OLC_FIELD_BOOL;
    else if (json_is_array(value)) ftype = OLC_FIELD_FLAGS;

    /* Get current live value for old_value using field handler serialize_fn.
     * For fields with a registered handler, use it. For simple scalar fields
     * without a handler, the collapsing logic in olc_changeset_add_change()
     * tracks old_value from the first change — so pass NULL for old_value on
     * subsequent edits (the existing change's old_value is preserved). */
    json_t *old_value = NULL;
    olc_pending_change_t *existing = olc_changeset_find_change(cs, field);
    if (!existing) {
        /* First edit of this field — try to get live value via field handler */
        const olc_field_handler_t *handler = olc_field_handler_find(
            cs->editor_type, field);
        if (handler && handler->serialize_fn) {
            void *entity = olc_editor_get_entity(d);  /* pEdit from descriptor */
            old_value = handler->serialize_fn(entity, field);
        }
        /* For simple fields without a handler, old_value remains NULL.
         * The changeset stores new_value; on revert the original entity
         * field is unchanged since commit hasn't been called. */
    }

    /* Add to changeset (collapsing logic handles duplicates) */
    olc_pending_change_t *result = olc_changeset_add_change(
        cs, field, ftype, old_value, value);
    if (old_value) json_decref(old_value);

    const char *type_str = olc_field_type_name(ftype);
    if (result) {
        gmcp_editor_send_field(d, entity_id, field, value,
            type_str, true);
    } else {
        /* Collapsed to no-op — field reverted to original.
         * Re-serialize the live value to send current state. */
        const olc_field_handler_t *handler = olc_field_handler_find(
            cs->editor_type, field);
        json_t *live_value = NULL;
        if (handler && handler->serialize_fn) {
            void *entity = olc_editor_get_entity(d);
            live_value = handler->serialize_fn(entity, field);
        }
        gmcp_editor_send_field(d, entity_id, field,
            live_value ? live_value : json_null(), type_str, false);
        if (live_value) json_decref(live_value);
    }
}
```

- [ ] **Step 3: Implement handle_editor_commit**

Follows same pattern as `olc_staged_cmd_commit()` but reads from JSON payload instead of command arguments. Sends `Editor.CommitResult` instead of `send_to_char`.

Handles both single-entity and group commit based on presence of `"group"` array in payload.

- [ ] **Step 4: Implement handle_editor_revert**

Reads `entity_id` and optional `field` from payload. Calls `olc_changeset_revert()` or `olc_changeset_revert_field()`. Sends `Editor.State` with updated pending list.

- [ ] **Step 5: Implement handle_editor_request**

Reads `entity_id` and optional `type` (history/state). Sends full `Editor.State` or `Editor.Open` response.

- [ ] **Step 6: Wire GMCP sends into staged command handlers**

In `olc_staged.c`, after each successful command operation, also send GMCP:

```c
/* In olc_staged_cmd_commit, after successful commit: */
gmcp_editor_send_commit_result(ch->desc, entity_id, "success", count);

/* In olc_staged_cmd_revert, after revert: */
gmcp_editor_send_state(ch->desc, entity_id, cs, false);
```

In `olc_commands.c`, in the staging code path for each `olc_cmd_*`:
```c
/* After staging a change: */
gmcp_editor_send_field(ch->desc, entity_id, label, new_val, type_str, true);
```

- [ ] **Step 7: Build and run tests**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test`
Expected: All tests pass.

- [ ] **Step 8: Commit**

```bash
git add gmcp_editor.c editors/common/olc_staged.c editors/common/olc_commands.c
git commit -m "feat(gmcp): implement incoming Editor.Set/Commit/Revert/Request handlers

Bidirectional GMCP editing now functional. Incoming Set validates and
stores in overlay. Commit applies changes via field handlers. Revert
clears pending changes. Request returns full state.

All staged commands also send GMCP responses for web client sync."
```


## Phase 5: Draft Persistence & Non-Blocking String Editor

### Task 10: Draft Persistence

**Goal:** Implement save/load/discard for draft changesets and auto-draft on disconnect. Drafts are saved as JSON files so work-in-progress survives server restarts and disconnects.

**Files:**
- Modify: `editors/common/olc_changeset.h` (add draft API)
- Modify: `editors/common/olc_changeset.c` (implement draft serialization)
- Modify: `editors/common/olc_staged.c` (implement draft commands)
- Modify: `editors/common/olc_editor.c` (draft restore prompt on editor open)
- Modify: `mem.c` (auto-draft hook)
- Modify: `tests/unit/olc_changeset_tests.c` (add serialization tests)
- Modify: `tests/data/unit/olc_changeset_tests.json` (add test definitions)

- [ ] **Step 1: Add draft API to olc_changeset.h**

```c
/* =========================================================================
 * Draft Persistence API
 * ========================================================================= */

/**
 * Serialize a changeset to JSON for draft storage.
 * Caller must json_decref() the result.
 */
json_t *olc_changeset_serialize(olc_changeset_t *cs);

/**
 * Deserialize a changeset from JSON draft.
 * Returns NULL on invalid/corrupt JSON (graceful failure).
 */
olc_changeset_t *olc_changeset_deserialize(json_t *json);

/**
 * Save a changeset as a draft file.
 * Location: data/drafts/<author>/<editor_type>_<entity_id>.json
 * @return true on success
 */
bool olc_draft_save(olc_changeset_t *cs);

/**
 * Load a draft file for an entity, if one exists.
 * @return Deserialized changeset, or NULL if no draft found
 */
olc_changeset_t *olc_draft_load(const char *author, int editor_type,
    WNUM_LOAD entity_wnum);

/**
 * Delete a draft file for an entity.
 * @return true if a file was deleted
 */
bool olc_draft_discard(const char *author, int editor_type,
    WNUM_LOAD entity_wnum);

/**
 * Check if a draft exists for an entity.
 */
bool olc_draft_exists(const char *author, int editor_type,
    WNUM_LOAD entity_wnum);

/**
 * Auto-save all active changesets as drafts (called on disconnect).
 */
void olc_draft_auto_save(olc_edit_state_t *state);
```

- [ ] **Step 2: Write failing tests for serialization round-trip**

```c
static test_result_t test_changeset_serialize_roundtrip(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test Room", "Builder");

    json_t *old_v = json_string("Old Name");
    json_t *new_v = json_string("New Name");
    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, old_v, new_v);

    json_t *old_int = json_integer(100);
    json_t *new_int = json_integer(200);
    olc_changeset_add_change(cs, "heal_rate", OLC_FIELD_INT, old_int, new_int);

    /* Serialize */
    json_t *json = olc_changeset_serialize(cs);
    TEST_ASSERT_NOT_NULL(json);

    /* Deserialize */
    olc_changeset_t *restored = olc_changeset_deserialize(json);
    TEST_ASSERT_NOT_NULL(restored);

    /* Verify */
    TEST_ASSERT_INT_EQ(ED_ROOM, restored->editor_type);
    TEST_ASSERT_INT_EQ(5, (int)restored->entity_wnum.auid);
    TEST_ASSERT_INT_EQ(3001, (int)restored->entity_wnum.vnum);
    TEST_ASSERT_STR_EQ("Test Room", restored->entity_label);
    TEST_ASSERT_STR_EQ("Builder", restored->author);
    TEST_ASSERT_INT_EQ(2, olc_changeset_count(restored));

    olc_pending_change_t *name_change = olc_changeset_find_change(restored, "name");
    TEST_ASSERT_NOT_NULL(name_change);
    TEST_ASSERT_STR_EQ("New Name", json_string_value(name_change->new_value));

    json_decref(old_v); json_decref(new_v);
    json_decref(old_int); json_decref(new_int);
    json_decref(json);
    olc_changeset_destroy(cs);
    olc_changeset_destroy(restored);
    return TEST_PASS;
}

static test_result_t test_changeset_deserialize_corrupt(test_case_t *test)
{
    /* Corrupt JSON should return NULL gracefully */
    json_t *bad = json_object();
    json_object_set_new(bad, "garbage", json_string("data"));

    olc_changeset_t *result = olc_changeset_deserialize(bad);
    TEST_ASSERT_NULL(result);

    json_decref(bad);
    return TEST_PASS;
}
```

- [ ] **Step 3: Implement serialization/deserialization**

In `olc_changeset.c`:

```c
json_t *olc_changeset_serialize(olc_changeset_t *cs)
{
    if (!cs) return NULL;

    json_t *root = json_object();
    json_object_set_new(root, "editor_type", json_integer(cs->editor_type));
    json_object_set_new(root, "entity_wnum_auid", json_integer(cs->entity_wnum.auid));
    json_object_set_new(root, "entity_wnum_vnum", json_integer(cs->entity_wnum.vnum));
    json_object_set_new(root, "entity_label", json_string(cs->entity_label));
    json_object_set_new(root, "author", json_string(cs->author));
    json_object_set_new(root, "created_at", json_integer((json_int_t)cs->created_at));

    json_t *changes_arr = json_array();
    LLIST_ITERATOR *it = llist_iterator(cs->changes);
    olc_pending_change_t *change;
    while ((change = llist_next(it)) != NULL) {
        json_t *entry = json_object();
        json_object_set_new(entry, "field_path", json_string(change->field_path));
        json_object_set_new(entry, "field_type", json_integer(change->field_type));
        if (change->old_value) json_object_set(entry, "old_value", change->old_value);
        if (change->new_value) json_object_set(entry, "new_value", change->new_value);
        json_array_append_new(changes_arr, entry);
    }
    llist_iterator_delete(it);
    json_object_set_new(root, "changes", changes_arr);

    return root;
}
```

Deserialization reverses the process with validation at each step. Returns NULL if any required field is missing or has wrong type.

- [ ] **Step 4: Implement draft file I/O**

```c
static const char *olc_draft_path(const char *author, int editor_type,
    WNUM_LOAD wnum)
{
    static char path[256];
    snprintf(path, sizeof(path), "%s%s/%s/%d_%ld_%ld.json",
        DATA_DIR, "drafts", author, editor_type, wnum.auid, wnum.vnum);
    return path;
}
```

`olc_draft_save()` serializes to JSON, checks size limit, writes atomically (write temp, rename).
`olc_draft_load()` reads file, `json_load_file()`, deserializes.
`olc_draft_discard()` removes the file.

- [ ] **Step 5: Wire draft commands in olc_staged.c**

Replace the stubs from Task 6:
```c
void olc_staged_cmd_savedraft(CHAR_DATA *ch, const OLC_EDITOR_DEF *def, void *pEdit) {
    olc_changeset_t *cs = olc_get_active_changeset(ch, def);
    if (!cs || olc_changeset_count(cs) == 0) {
        send_to_char("No pending changes to save.\n\r", ch);
        return;
    }
    if (olc_draft_save(cs)) {
        send_to_char("Draft saved.\n\r", ch);
        cs->is_dirty = false;
    } else {
        send_to_char("Error saving draft.\n\r", ch);
    }
}
```

- [ ] **Step 6: Add auto-draft in mem.c free_descriptor**

Enhance the cleanup in `free_descriptor()`:
```c
    if (d->olc_state) {
        olc_draft_auto_save(d->olc_state);
        olc_edit_state_destroy(d->olc_state);
        d->olc_state = NULL;
    }
```

`olc_draft_auto_save()` iterates all active changesets and saves each non-empty one as a draft.

- [ ] **Step 7: Add draft restore prompt on editor open**

In `olc_editor_enter()`, after creating the changeset for staged mode:
```c
    if (def->change_mode == OLC_CHANGE_STAGED) {
        if (olc_draft_exists(ch->name, def->editor_type, wnum)) {
            printf_to_char(ch,
                "{YYou have a saved draft. Use '{Wloaddraft{Y' to restore.{x\n\r");
        }
    }
```

- [ ] **Step 8: Build and run tests**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:olccs`
Expected: All tests pass including serialization round-trip.

- [ ] **Step 9: Commit**

```bash
git add editors/common/olc_changeset.h editors/common/olc_changeset.c \
        editors/common/olc_staged.c editors/common/olc_editor.c \
        mem.c tests/unit/olc_changeset_tests.c \
        tests/data/unit/olc_changeset_tests.json
git commit -m "feat(olc): implement draft persistence and auto-draft on disconnect

Changeset serialization/deserialization to JSON. Draft file I/O with
savedraft/loaddraft/discardraft commands. Auto-draft on free_descriptor()
prevents data loss from disconnects. Draft restore prompt on editor open.
Size limit enforced (OLC_MAX_DRAFT_SIZE).

Round-trip serialization tests and corrupt JSON handling."
```

### Task 11: Non-Blocking String Editor

**Goal:** For WebSocket/GMCP clients, bypass the modal `string_append()` and instead send a GMCP message that opens a text editing panel in the web client. The player can continue playing while editing a description in a side panel.

**Files:**
- Modify: `editors/common/olc_commands.c` (detect WebSocket in olc_cmd_string_append)
- Modify: `gmcp_editor.h` (add StringEdit types)
- Modify: `gmcp_editor.c` (add StringEdit message builders and handlers)
- Modify: `editors/common/olc_changeset.h` (add olc_string_edit_session_t)

- [ ] **Step 1: Define string edit session type**

In `editors/common/olc_changeset.h`:
```c
/**
 * Non-blocking string edit session for WebSocket clients.
 * Each open string edit gets its own session_id and panel in the web client.
 */
typedef struct olc_string_edit_session {
    int              session_id;     /**< Integer internally, "se_N" in GMCP */
    char            *entity_id;      /**< GMCP entity ID string */
    char            *field_path;     /**< Which field is being edited */
    char           **field_ptr;      /**< Direct pointer (for non-staged mode) */
    olc_changeset_t *changeset;      /**< For staged mode (mutually exclusive with field_ptr) */
} olc_string_edit_session_t;
```

- [ ] **Step 2: Add StringEdit GMCP message builders to gmcp_editor.h/c**

```c
/* In gmcp_editor.h */
json_t *gmcp_editor_build_string_open(const char *entity_id, const char *field,
    const char *current_value, int max_length, int session_id);

json_t *gmcp_editor_build_string_close(int session_id, const char *status);

void gmcp_editor_send_string_open(descriptor_t *d, const char *entity_id,
    const char *field, const char *current_value, int max_length, int session_id);

void gmcp_editor_send_string_close(descriptor_t *d, int session_id,
    const char *status);
```

- [ ] **Step 3: Detect WebSocket in olc_cmd_string_append**

In `editors/common/olc_commands.c:olc_cmd_string_append()`:

```c
bool olc_cmd_string_append(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, char **field_ptr,
    void *ctx, olc_cmd_record_fn record_fn)
{
    if (argument[0] != '\0') {
        /* ... existing argument handling ... */
    }

    /* Non-blocking for WebSocket clients with GMCP support */
    if (is_websocket_connection(ch->desc) && ch->desc->pProtocol->bGMCP) {
        const OLC_EDITOR_DEF *def = olc_get_current_editor_def(ch);
        if (def && ch->desc->olc_state) {
            olc_string_edit_session_t *session = olc_string_session_create(
                ch->desc->olc_state, /* entity_id */, label, field_ptr,
                def->change_mode == OLC_CHANGE_STAGED
                    ? olc_get_active_changeset(ch, def) : NULL);

            gmcp_editor_send_string_open(ch->desc, session->entity_id,
                label, *field_ptr ? *field_ptr : "",
                4096, session->session_id);

            printf_to_char(ch, "{GString editor opened in web client panel.{x\n\r");
            return true;
        }
    }

    /* Fallback: existing modal string_append() for telnet */
    string_append(ch, field_ptr);
    // ... existing code ...
}
```

- [ ] **Step 4: Implement StringEdit.Save/Cancel handlers**

In `gmcp_editor.c`:

```c
static void handle_editor_string_save(descriptor_t *d, json_t *payload)
{
    int session_id = /* parse from "se_N" format */;
    const char *value = json_string_value(json_object_get(payload, "value"));
    if (!value) return;

    olc_string_edit_session_t *session = /* find by session_id */;
    if (!session) return;

    if (session->changeset) {
        /* Staged mode — store in overlay */
        json_t *old_val = json_string(*(session->field_ptr) ? *(session->field_ptr) : "");
        json_t *new_val = json_string(value);
        olc_changeset_add_change(session->changeset, session->field_path,
            OLC_FIELD_MULTILINE, old_val, new_val);
        json_decref(old_val);
        json_decref(new_val);
    } else {
        /* Direct mode — write immediately */
        free_string(*(session->field_ptr));
        *(session->field_ptr) = str_dup(value);
    }

    gmcp_editor_send_string_close(d, session->session_id, "saved");
    /* Remove session from list */
}

static void handle_editor_string_cancel(descriptor_t *d, json_t *payload)
{
    int session_id = /* parse from "se_N" format */;
    olc_string_edit_session_t *session = /* find by session_id */;
    if (!session) return;

    gmcp_editor_send_string_close(d, session->session_id, "cancelled");
    /* Remove session from list */
}
```

- [ ] **Step 5: Build and run tests**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test`
Expected: All tests pass.

- [ ] **Step 6: Commit**

```bash
git add editors/common/olc_changeset.h editors/common/olc_commands.c \
        gmcp_editor.h gmcp_editor.c
git commit -m "feat(olc): add non-blocking string editor for WebSocket clients

WebSocket clients with GMCP bypass modal string_append() and get a
Sentience.Editor.StringEdit.Open message for panel-based editing.
StringEdit.Save/Cancel handlers apply changes (staged or direct).
Telnet clients continue using the existing modal editor."
```


## Phase 6: Editor Migrations

### Task 12: redit Migration (First Tier 1 Editor)

**Goal:** Migrate the room editor to use OLC_CHANGE_STAGED mode. This is the first real editor to use the framework, serving as the template for all subsequent migrations.

**Files:**
- Modify: `editors/rooms/redit.c` (change_mode, field handlers, command adjustments)
- Modify: `tests/integration/gmcp_editor_tests.c` (add redit integration test)
- Modify: `tests/data/integration/gmcp_editor_tests.json` (add test definition)

- [ ] **Step 1: Create redit field handlers for complex fields**

In `editors/rooms/redit.c`, add field handlers for non-scalar fields:

```c
/* Exits, extra descriptions, and resets need editor-specific handlers */
static json_t *redit_serialize_exit(void *entity, const char *field_path)
{
    ROOM_INDEX_DATA *room = (ROOM_INDEX_DATA *)entity;
    /* Parse direction from field_path (e.g., "exits/north" → DIR_NORTH) */
    int dir = /* parse direction */;
    EXIT_DATA *ex = room->exit[dir];
    if (!ex) return json_null();

    json_t *obj = json_object();
    json_object_set_new(obj, "destination",
        json_string(formatf("%ld#%ld", ex->u1.to_room ? ex->u1.to_room->area->uid : 0,
                            ex->u1.to_room ? ex->u1.to_room->vnum : 0)));
    json_object_set_new(obj, "keyword", json_string(ex->keyword ? ex->keyword : ""));
    json_object_set_new(obj, "description", json_string(ex->description ? ex->description : ""));
    json_object_set_new(obj, "exit_flags", json_integer(ex->exit_info));
    json_object_set_new(obj, "key_vnum", json_integer(ex->key));
    return obj;
}

static bool redit_apply_exit(void *entity, olc_pending_change_t *change)
{
    ROOM_INDEX_DATA *room = (ROOM_INDEX_DATA *)entity;
    /* Parse direction, apply new exit data from change->new_value JSON */
    /* ... implementation ... */
    return true;
}

static const char *redit_display_exit(json_t *value)
{
    static char buf[MIL];
    if (json_is_null(value)) return "(none)";
    const char *dest = json_string_value(json_object_get(value, "destination"));
    snprintf(buf, sizeof(buf), "→ %s", dest ? dest : "?");
    return buf;
}

/* Handler table — Complex fields need all three functions.
 * Simple scalar fields (name, description, sector, flags) don't need entries
 * here because the generic olc_cmd_* staging path handles them.
 * For extra_descr and resets: implement handlers when these complex list
 * operations are migrated. The commit flow gracefully skips changes whose
 * field handler has NULL apply_fn (logs a warning and marks as skipped). */
static const olc_field_handler_t redit_field_handlers[] = {
    { "exits/*",       OLC_FIELD_EXIT,        redit_serialize_exit,  redit_apply_exit,  redit_display_exit },
    { "extra_descr/*", OLC_FIELD_LIST_UPDATE,  NULL,                 NULL,              NULL },
    { "resets/*",      OLC_FIELD_LIST_UPDATE,  NULL,                 NULL,              NULL },
    { NULL, 0, NULL, NULL, NULL }
};
```

- [ ] **Step 2: Switch redit to OLC_CHANGE_STAGED**

In `editors/rooms/redit.c`, modify the `redit_def` static:

```c
static const OLC_EDITOR_DEF redit_def = {
    .name           = "REdit",
    .editor_type    = ED_ROOM,
    .cmd_table      = redit_table,
    .show_fn        = redit_show,
    .tabs           = { /* ... unchanged ... */ },
    .theme          = &olc_theme_world,
    .perm           = {
        .flags          = OLC_PERM_AREA_SECURITY,
    },
    .change_mode    = OLC_CHANGE_STAGED,        /* ← CHANGED from OLC_CHANGE_AREA_FLAG */
    .get_area_fn    = redit_get_area,
    .audit_changes  = true,
    .field_handlers = redit_field_handlers,      /* ← NEW */
};
```

- [ ] **Step 3: Update redit tab show functions to use preview helpers**

In the `redit_show_general_tab()` function, replace direct field access with staged preview where applicable. The display functions already handle this via `ctx->changeset` from Task 7, but verify:

```c
static void redit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    ROOM_INDEX_DATA *pRoom = (ROOM_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&redit_def);

    /* These now automatically show staged values if ctx->changeset is set */
    olc_display_string(ctx, theme, "Name:", "name", pRoom->name);
    /* ... rest of display calls ... */
}
```

- [ ] **Step 4: Write integration test for redit staged edit→commit cycle**

Add test to `tests/integration/gmcp_editor_tests.c`:

```c
static test_result_t test_redit_staged_cycle(test_case_t *test)
{
    /* This test verifies the end-to-end flow:
     * 1. Create a changeset for a room
     * 2. Add string and integer changes
     * 3. Verify preview shows staged values
     * 4. Commit and verify live entity updated
     */
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test Room", "Builder");

    /* Stage changes */
    json_t *old_name = json_string("Old Room");
    json_t *new_name = json_string("New Room");
    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, old_name, new_name);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));

    /* Verify preview */
    TEST_ASSERT_STR_EQ("New Room", olc_staged_string(cs, "name", "Old Room"));

    /* Commit (we can't easily test live entity update without a running room,
     * but we verify the changeset mechanics are sound) */
    olc_changeset_clear(cs);
    TEST_ASSERT_INT_EQ(0, olc_changeset_count(cs));

    json_decref(old_name);
    json_decref(new_name);
    olc_changeset_destroy(cs);
    return TEST_PASS;
}
```

- [ ] **Step 5: Build and run full test suite**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test`
Expected: All tests pass. redit now uses staged mode.

**Manual testing:** Start the MUD, create an immortal, `redit` a room, verify:
- `redit name New Name` shows `[STAGED] Name set to: New Name`
- `pending` shows the change
- `revert` clears it
- `commit` applies it
- `done` warns about uncommitted changes

- [ ] **Step 6: Commit**

```bash
git add editors/rooms/redit.c tests/integration/gmcp_editor_tests.c \
        tests/data/integration/gmcp_editor_tests.json
git commit -m "feat(olc): migrate redit to OLC_CHANGE_STAGED mode

First Tier 1 editor migration. Room editor now stages all field changes
in memory until committed. Exit field handlers for complex serialization.
Integration test for staged edit→commit cycle.

Builders see [STAGED] feedback, can use commit/revert/pending commands."
```

### Task 13: medit, oedit, aedit Migrations

**Goal:** Migrate the remaining Tier 1 editors to staged mode, following the same pattern as redit.

**Files:**
- Modify: `editors/mobiles/medit.c`
- Modify: `editors/objects/oedit.c`
- Modify: `editors/areas/aedit.c`

- [ ] **Step 1: Migrate medit**

1. Create `medit_field_handlers[]` for complex mob fields (shop data, special programs, etc.)
2. Change `.change_mode` to `OLC_CHANGE_STAGED` in `medit_def`
3. Add `.field_handlers = medit_field_handlers`
4. Build and test

- [ ] **Step 2: Migrate oedit**

1. Create `oedit_field_handlers[]` for complex object fields (item type data, affects, extra descriptions)
2. Change `.change_mode` to `OLC_CHANGE_STAGED` in `oedit_def`
3. Add `.field_handlers = oedit_field_handlers`
4. Build and test

- [ ] **Step 3: Migrate aedit**

1. Create `aedit_field_handlers[]` (area has fewer complex fields)
2. Change `.change_mode` to `OLC_CHANGE_STAGED` in `aedit_def`
3. Add `.field_handlers = aedit_field_handlers`
4. Build and test

- [ ] **Step 4: Full regression test**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test`
Expected: All tests pass. No regressions.

- [ ] **Step 5: Commit**

```bash
git add editors/mobiles/medit.c editors/objects/oedit.c editors/areas/aedit.c
git commit -m "feat(olc): migrate medit, oedit, aedit to OLC_CHANGE_STAGED mode

All Tier 1 editors now use staged editing with commit semantics.
Editor-specific field handlers for complex fields (shops, item types,
affects, etc.)."
```

---

## Summary

### Phase Dependency Graph

```
Phase 1 (Tasks 1-3): Core Framework
    ↓
Phase 2 (Task 4): Field Handlers & Commit
    ↓
Phase 3 (Tasks 5-7): Editor Integration
    ↓                          ↓
Phase 4 (Tasks 8-9):     Phase 5 (Tasks 10-11):
GMCP Protocol             Drafts & String Editor
    ↓                          ↓
Phase 6 (Tasks 12-13): Editor Migrations
```

### Commit Points

Each task ends with a commit. This gives 13 natural revert points if anything goes wrong. Key milestones:

1. **After Task 1:** Changeset CRUD works with unit tests ✓
2. **After Task 3:** Preview helpers work ✓
3. **After Task 4:** Commit/revert flow works ✓
4. **After Task 6:** Full staged editing works via commands ✓
5. **After Task 8:** GMCP protocol registered and outgoing messages work ✓
6. **After Task 9:** Bidirectional GMCP editing works ✓
7. **After Task 10:** Drafts persist across sessions ✓
8. **After Task 12:** redit fully migrated and tested ✓
9. **After Task 13:** All Tier 1 editors migrated ✓

### Test Count Projection

- Task 1: +8 unit tests (changeset CRUD)
- Task 2: +5 unit tests (list collapsing)
- Task 3: +5 unit tests (preview helpers)
- Task 4: +5 unit tests (field handlers, commit)
- Task 8: +5 integration tests (GMCP message building)
- Task 10: +2 unit tests (serialization)
- Task 12: +1 integration test (redit cycle)

**Total: ~31 new tests**, bringing the suite from 432 → ~463.

