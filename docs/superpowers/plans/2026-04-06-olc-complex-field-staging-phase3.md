# OLC Complex Field Staging (Phase 3) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stage all complex oedit fields (list operations, embedded structs, 32 type-specific commands) through the changeset system, with GMCP support.

**Architecture:** Build three infrastructure layers (list ops, embedded snapshots, type dispatch), extend GMCP Editor.Set, then convert all 49 oedit data-modifying functions. Sequential operation entries for lists, JSON snapshots for complex structs, namespaced paths with wildcard dispatch for type-specific fields.

**Tech Stack:** C23, Jansson (JSON), custom OLC changeset framework in `editors/common/`

**Spec:** `docs/superpowers/specs/2026-04-06-olc-complex-field-staging-design.md`

**Build/Test:**
```bash
cd /sentience/src && ./build tests        # Build with test support
./install debug                           # Install
cd /sentience && ./sent -test             # Run all tests
./sent -test:olccs_                       # OLC changeset tests only
```

**Test baseline:** 604 total, 588 passed, 2 known failures, 14 skipped.

---

## File Map

### New Files
None — all changes go into existing files.

### Modified Files

| File | Responsibility | Tasks |
|------|---------------|-------|
| `editors/common/olc_changeset.h` | Declare `olc_changeset_next_seq`, `olc_changeset_revert_prefix` | 1 |
| `editors/common/olc_changeset.c` | Implement `olc_changeset_next_seq`, `olc_changeset_revert_prefix` | 1 |
| `editors/common/olc_commands.h` | Declare `olc_stage_list_add`, `olc_stage_list_remove` | 1 |
| `editors/common/olc_commands.c` | Implement `olc_stage_list_add`, `olc_stage_list_remove` | 1 |
| `editors/common/olc_field_handlers.c` | Add `/**` multi-level wildcard to `field_path_matches()` | 1 |
| `editors/common/olc_staged.h` | Declare `olc_staged_embedded`, `olc_staged_embedded_set` | 1 |
| `editors/common/olc_staged.c` | Implement `olc_staged_embedded`, `olc_staged_embedded_set` | 1 |
| `gmcp_editor.c` | Extend `handle_editor_set()` and `handle_editor_revert()` | 3 |
| `editors/common/olc_staged.c` | Grouped pending display for list ops (contains `olc_staged_cmd_pending`) | 3 |
| `editors/objects/oedit.c` | Handler table entries, apply functions, list op conversions | 4–7 |
| `editors/objects/oedit_types.c` | Type dispatch table, per-type apply/serialize, conversions | 7–10 |
| `tests/unit/olc_changeset_tests.c` | New unit tests | 2, 11 |
| `tests/data/unit/olc_changeset_tests.json` | New test definitions | 2, 11 |

---

## Existing Patterns Reference

### How scalar staging works (Phase 2 pattern — gold standard)

**In the command function** (e.g., `oedit_cost`):
```c
OEDIT(oedit_cost) {
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    return olc_cmd_long(ch, argument, "Cost", "Syntax: cost <amount>",
        &pObj->cost, 0, LONG_MAX, pObj, oedit_record_change);
}
```

**In the handler table** (oedit_field_handlers[]):
```c
{ "Cost", OLC_FIELD_LONG, NULL, oedit_apply_cost, NULL },
```

**The apply macro** (above the handler table):
```c
OLC_FIELD_APPLY_LONG(oedit_apply_cost, OBJ_INDEX_DATA, cost)
```

**Flow:** Command → `olc_cmd_long()` → validates → `olc_changeset_add_change(cs, "Cost", OLC_FIELD_LONG, old, new)` → `notify_field_change()`.
On commit: `olc_changeset_commit()` → finds handler → calls `oedit_apply_cost()` → `olc_apply_generic_long(&pObj->cost, change)`.

### Key infrastructure functions

- `olc_get_active_changeset(ch, edef)` — gets or creates changeset for current editor session
- `olc_changeset_add_change(cs, path, type, old, new)` — stages change (collapses same-path duplicates)
- `notify_field_change(cs, ch, label, value, type_str, staged)` — sends GMCP `Editor.Field` update
- `olc_changeset_find_change(cs, path)` — finds change by exact path match
- `field_path_matches(pattern, path)` — handler dispatch: exact + single-level wildcard (`var/*`)
- `olc_staged_json(cs, label)` — returns raw `change->new_value` JSON for a field

### How var/* wildcard works (Phase 2)

Handler entry: `{ "var/*", OLC_FIELD_STRING, NULL, oedit_apply_var, NULL }`
Commands use paths like `"var/mykeyname"`. The apply function extracts the key name from the path suffix.

---

## Task 1: Core Infrastructure — List Operations, Embedded Helpers, Multi-Level Wildcards

**Files:**
- Modify: `editors/common/olc_changeset.h` (add declarations)
- Modify: `editors/common/olc_changeset.c` (implement `next_seq`, `revert_prefix`, list-op sort in commit)
- Modify: `editors/common/olc_commands.h` (add declarations, make `notify_field_change` public)
- Modify: `editors/common/olc_commands.c` (implement `olc_stage_list_add`, `olc_stage_list_remove`, make `notify_field_change` non-static)
- Modify: `editors/common/olc_field_handlers.c` (extend `field_path_matches` for `/**`)
- Modify: `editors/common/olc_staged.h` (add declarations)
- Modify: `editors/common/olc_staged.c` (implement `olc_staged_embedded`, `olc_staged_embedded_set`)

### New Functions

- [ ] **Step 0: Make `notify_field_change()` public**

In `editors/common/olc_commands.c` at line 30, remove the `static` keyword:
```c
// BEFORE:
static void notify_field_change(olc_changeset_t *cs, CHAR_DATA *ch,
// AFTER:
void notify_field_change(olc_changeset_t *cs, CHAR_DATA *ch,
```

In `editors/common/olc_commands.h`, add the declaration:
```c
void notify_field_change(olc_changeset_t *cs, CHAR_DATA *ch,
    const char *label, json_t *value, const char *type_str, bool is_staged);
```

**Why:** This function is called from `oedit.c` in Tasks 4–10 to send GMCP field updates. It is currently `static` in `olc_commands.c`, making it inaccessible from other files.

- [ ] **Step 1: Add declarations to `olc_changeset.h`**

Add before the `#endif`:
```c
/* List operation helpers */
int  olc_changeset_next_seq(olc_changeset_t *cs, const char *prefix);
int  olc_changeset_revert_prefix(olc_changeset_t *cs, const char *prefix);
```

- [ ] **Step 2: Implement `olc_changeset_next_seq()` in `olc_changeset.c`**

Add after `olc_changeset_remove_change()`:
```c
/**
 * Get the next available sequence number for a path prefix.
 * Scans changes matching "{prefix}:" and returns max + 1.
 * E.g., for prefix "affects/add" with existing changes "affects/add:0"
 * and "affects/add:2", returns 3.
 */
int olc_changeset_next_seq(olc_changeset_t *cs, const char *prefix)
{
    if (!cs || !prefix)
        return 0;

    int max_seq = -1;
    size_t prefix_len = strlen(prefix);

    ITERATOR it;
    iterator_start(&it, cs->changes);
    olc_pending_change_t *change;
    while ((change = iterator_nextdata(&it)) != NULL) {
        if (strncmp(change->field_path, prefix, prefix_len) == 0
            && change->field_path[prefix_len] == ':') {
            int seq = atoi(change->field_path + prefix_len + 1);
            if (seq > max_seq)
                max_seq = seq;
        }
    }
    iterator_stop(&it);

    return max_seq + 1;
}
```

- [ ] **Step 3: Implement `olc_changeset_revert_prefix()` in `olc_changeset.c`**

Add after `olc_changeset_next_seq()`:
```c
/**
 * Remove all changes whose field_path starts with the given prefix followed
 * by '/'. Also removes an exact match on the prefix itself.
 * Returns the number of changes removed.
 */
int olc_changeset_revert_prefix(olc_changeset_t *cs, const char *prefix)
{
    if (!cs || !prefix)
        return 0;

    int removed = 0;
    size_t prefix_len = strlen(prefix);

    ITERATOR it;
    iterator_start(&it, cs->changes);
    olc_pending_change_t *change;
    while ((change = iterator_nextdata(&it)) != NULL) {
        bool match = (strcmp(change->field_path, prefix) == 0)
            || (strncmp(change->field_path, prefix, prefix_len) == 0
                && change->field_path[prefix_len] == '/');
        if (match) {
            iterator_remcurrent(&it);
            olc_pending_change_destroy(change);
            removed++;
        }
    }
    iterator_stop(&it);

    if (removed > 0)
        cs->is_dirty = true;

    return removed;
}
```

- [ ] **Step 4: Add staging helper declarations to `olc_commands.h`**

Add before `#endif`:
```c
/* List operation staging helpers */
olc_pending_change_t *olc_stage_list_add(olc_changeset_t *cs,
    const char *list_name, json_t *value);
olc_pending_change_t *olc_stage_list_remove(olc_changeset_t *cs,
    const char *list_name, int index, json_t *old_value);
```

- [ ] **Step 5: Implement staging helpers in `olc_commands.c`**

Add at end of file (before any `#endif` if present):
```c
/**
 * Stage a list add operation.
 * Builds path "list_name/add:N" with auto-incrementing sequence.
 */
olc_pending_change_t *olc_stage_list_add(olc_changeset_t *cs,
    const char *list_name, json_t *value)
{
    char prefix[MIL];
    snprintf(prefix, sizeof(prefix), "%s/add", list_name);
    int seq = olc_changeset_next_seq(cs, prefix);

    char path[MIL];
    snprintf(path, sizeof(path), "%s:%d", prefix, seq);

    return olc_changeset_add_change(cs, path, OLC_FIELD_LIST_ADD, NULL, value);
}

/**
 * Stage a list remove operation.
 * Builds path "list_name/rm:N" with auto-incrementing sequence.
 * old_value should be a JSON snapshot of the item being removed (for history).
 */
olc_pending_change_t *olc_stage_list_remove(olc_changeset_t *cs,
    const char *list_name, int index, json_t *old_value)
{
    char prefix[MIL];
    snprintf(prefix, sizeof(prefix), "%s/rm", list_name);
    int seq = olc_changeset_next_seq(cs, prefix);

    char path[MIL];
    snprintf(path, sizeof(path), "%s:%d", prefix, seq);

    json_t *val = json_pack("{s:i}", "index", index);
    olc_pending_change_t *result = olc_changeset_add_change(
        cs, path, OLC_FIELD_LIST_REMOVE, old_value, val);
    json_decref(val);
    return result;
}
```

- [ ] **Step 6: Extend `field_path_matches()` for multi-level wildcards**

In `editors/common/olc_field_handlers.c`, modify `field_path_matches()` to support `/**` (matches any depth):

```c
static bool field_path_matches(const char *pattern, const char *path)
{
    if (!pattern || !path)
        return false;

    /* Exact match */
    if (strcmp(pattern, path) == 0)
        return true;

    size_t plen = strlen(pattern);

    /* Multi-level wildcard: pattern ends with "/**" — matches any depth */
    if (plen >= 3 && pattern[plen - 1] == '*' && pattern[plen - 2] == '*'
        && pattern[plen - 3] == '/') {
        size_t prefix_len = plen - 2; /* include the '/' */
        if (strncmp(pattern, path, prefix_len) == 0 && path[prefix_len] != '\0')
            return true;
    }

    /* Single-level wildcard: pattern ends with "/*" — matches one segment */
    if (plen >= 2 && pattern[plen - 1] == '*' && pattern[plen - 2] == '/') {
        size_t prefix_len = plen - 1; /* include the '/' */
        if (strncmp(pattern, path, prefix_len) == 0 && path[prefix_len] != '\0') {
            if (strchr(path + prefix_len, '/') == NULL)
                return true;
        }
    }

    return false;
}
```

**Important:** The `/**` check MUST come before the `/*` check, because `/**` ends with `/*` and would match the single-level pattern first otherwise.

- [ ] **Step 7: Add embedded snapshot declarations to `olc_staged.h`**

Add before `#endif`:
```c
/* Embedded struct snapshot helpers */
json_t *olc_staged_embedded(olc_changeset_t *cs, const char *struct_name);
bool    olc_staged_embedded_set(olc_changeset_t *cs, const char *struct_name,
                                 const char *key, json_t *value);
```

- [ ] **Step 8: Implement embedded snapshot helpers in `olc_staged.c`**

```c
/**
 * Get the staged JSON snapshot for an embedded struct.
 * Returns the pending new_value JSON, or NULL if no change is staged.
 * The returned JSON is borrowed — do NOT decref.
 */
json_t *olc_staged_embedded(olc_changeset_t *cs, const char *struct_name)
{
    return olc_staged_json(cs, struct_name);
}

/**
 * Modify a key within a staged embedded snapshot.
 * If no snapshot is staged yet, this is a no-op and returns false.
 * If the snapshot exists, sets key=value in the new_value JSON object.
 */
bool olc_staged_embedded_set(olc_changeset_t *cs, const char *struct_name,
                              const char *key, json_t *value)
{
    if (!cs) return false;

    olc_pending_change_t *change = olc_changeset_find_change(cs, struct_name);
    if (!change || !change->new_value) return false;

    json_object_set(change->new_value, key, value);
    cs->is_dirty = true;
    return true;
}
```

**Note:** Both functions take `olc_changeset_t *cs` — the caller obtains the changeset via `olc_get_active_changeset(ch, edef)` as usual. This matches the pattern of `olc_staged_json()`.

- [ ] **Step 9: Add list-operation sort to `olc_changeset_commit()`**

In `editors/common/olc_field_handlers.c`, modify `olc_changeset_commit()` to sort list operations before applying. Add a sort step at the beginning of the commit function that reorders changes so that all `*/rm:*` entries come before `*/add:*` entries for each list prefix, with removes sorted by **descending** index:

```c
/**
 * Compare function for list operation ordering.
 * Removes (*/rm:*) sort before adds (*/add:*).
 * Within removes, sort by descending index.
 * Within adds, sort by ascending sequence.
 * Non-list changes sort to the front (applied first, preserving order).
 */
static int list_op_sort_compare(const void *a, const void *b)
{
    const olc_pending_change_t *ca = *(const olc_pending_change_t **)a;
    const olc_pending_change_t *cb = *(const olc_pending_change_t **)b;

    bool a_is_rm  = (strstr(ca->field_path, "/rm:") != NULL);
    bool b_is_rm  = (strstr(cb->field_path, "/rm:") != NULL);
    bool a_is_add = (strstr(ca->field_path, "/add:") != NULL);
    bool b_is_add = (strstr(cb->field_path, "/add:") != NULL);
    bool a_is_list = a_is_rm || a_is_add;
    bool b_is_list = b_is_rm || b_is_add;

    /* Non-list changes come first (preserve insertion order) */
    if (!a_is_list && !b_is_list) return 0;
    if (!a_is_list) return -1;
    if (!b_is_list) return 1;

    /* Removes come before adds */
    if (a_is_rm && b_is_add) return -1;
    if (a_is_add && b_is_rm) return 1;

    /* Within removes, sort by descending index */
    if (a_is_rm && b_is_rm) {
        const char *a_idx = strstr(ca->field_path, "/rm:") + 4;
        const char *b_idx = strstr(cb->field_path, "/rm:") + 4;
        /* These are sequence numbers, not the actual target indices.
         * The target index is in new_value.index */
        int a_target = (int)json_integer_value(
            json_object_get(ca->new_value, "index"));
        int b_target = (int)json_integer_value(
            json_object_get(cb->new_value, "index"));
        return b_target - a_target; /* descending */
    }

    /* Within adds, sort by ascending sequence */
    if (a_is_add && b_is_add) {
        const char *a_seq = strstr(ca->field_path, "/add:") + 5;
        const char *b_seq = strstr(cb->field_path, "/add:") + 5;
        return atoi(a_seq) - atoi(b_seq);
    }

    return 0;
}
```

Then, at the start of `olc_changeset_commit()`, before the main apply loop, extract the changes into an array, sort, and iterate the sorted array instead of the linked list:

```c
/* Collect changes into sortable array */
int count = list_size(cs->changes);
if (count > 0) {
    olc_pending_change_t **sorted = alloca(count * sizeof(*sorted));
    int idx = 0;
    ITERATOR it;
    iterator_start(&it, cs->changes);
    olc_pending_change_t *change;
    while ((change = iterator_nextdata(&it)) != NULL) {
        sorted[idx++] = change;
    }
    iterator_stop(&it);

    qsort(sorted, count, sizeof(*sorted), list_op_sort_compare);

    /* Apply changes in sorted order */
    for (int i = 0; i < count; i++) {
        /* existing apply dispatch logic, using sorted[i] */
    }
}
```

**Why:** The spec mandates two-pass apply: removes in descending index order (so earlier indices aren't shifted before later ones are processed), then adds in sequence order. Without this sort, removing index 2 then index 5 would corrupt the list (index 5 shifts to 4 after the first remove).

- [ ] **Step 10: Add `olc_staged_flags_or()` helper to `olc_staged.h` / `olc_staged.c`**

In `olc_staged.h`, declare:
```c
long olc_staged_flags_or(olc_changeset_t *cs, const char *field, long live_value);
```

In `olc_staged.c`, implement:
```c
/**
 * Get the staged flag value for a field, or the live value if not staged.
 * Used by type commands to read the current effective flags before toggling.
 */
long olc_staged_flags_or(olc_changeset_t *cs, const char *field, long live_value)
{
    json_t *staged = olc_staged_json(cs, field);
    if (staged && json_is_integer(staged))
        return (long)json_integer_value(staged);
    return live_value;
}
```

**Why:** Type commands that toggle flags need to read the current effective value (staged if a change exists, live otherwise). Without this, toggling a flag twice in staged mode would read the live value both times instead of correctly toggling back.

- [ ] **Step 11: Build and verify compilation**

```bash
cd /sentience/src && ./build tests
```

Expected: Clean compile, no new warnings.

- [ ] **Step 12: Commit**

```bash
git add editors/common/olc_changeset.h editors/common/olc_changeset.c \
        editors/common/olc_commands.h editors/common/olc_commands.c \
        editors/common/olc_field_handlers.c \
        editors/common/olc_staged.h editors/common/olc_staged.c
git commit -m "feat(olc): add list operation and embedded snapshot infrastructure

Add olc_changeset_next_seq() and olc_changeset_revert_prefix() for
sequential list operation paths and bulk prefix-based revert.

Add olc_stage_list_add() and olc_stage_list_remove() helpers that
auto-generate sequential paths (list/add:N, list/rm:N).

Add olc_staged_embedded() and olc_staged_embedded_set() for reading
and modifying staged JSON snapshots of complex structs.

Extend field_path_matches() to support /** multi-level wildcards
for type-specific field dispatch (typedata/weapon/class).

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 2: Infrastructure Unit Tests

**Files:**
- Modify: `tests/unit/olc_changeset_tests.c`
- Modify: `tests/data/unit/olc_changeset_tests.json`
- Modify: `tests/data/test_config.json` (if new suite entries needed)

**Existing tests:** The test file already has 3 tests (`olccs_apply_long`, `olccs_apply_dice`, `olccs_long_roundtrip`). Test type prefix is `olccs_`.

- [ ] **Step 1: Add test for `olc_changeset_next_seq`**

In `tests/unit/olc_changeset_tests.c`, add:
```c
static test_result_t test_olccs_next_seq(test_context_t *ctx)
{
    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT,
        (WNUM_LOAD){.auid = 1, .vnum = 100}, "Test Object", "TestBuilder");

    /* Empty changeset should return 0 */
    TEST_ASSERT_INT_EQUALS(0, olc_changeset_next_seq(cs, "affects/add"));

    /* Add a change and verify sequence increments */
    json_t *val = json_pack("{s:s}", "location", "APPLY_STR");
    olc_changeset_add_change(cs, "affects/add:0", OLC_FIELD_LIST_ADD, NULL, val);
    json_decref(val);
    TEST_ASSERT_INT_EQUALS(1, olc_changeset_next_seq(cs, "affects/add"));

    /* Add non-sequential and verify it finds the max */
    json_t *val2 = json_pack("{s:s}", "location", "APPLY_DEX");
    olc_changeset_add_change(cs, "affects/add:5", OLC_FIELD_LIST_ADD, NULL, val2);
    json_decref(val2);
    TEST_ASSERT_INT_EQUALS(6, olc_changeset_next_seq(cs, "affects/add"));

    /* Different prefix should still return 0 */
    TEST_ASSERT_INT_EQUALS(0, olc_changeset_next_seq(cs, "affects/rm"));

    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}
```

- [ ] **Step 2: Add test for `olc_changeset_revert_prefix`**

```c
static test_result_t test_olccs_revert_prefix(test_context_t *ctx)
{
    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT,
        (WNUM_LOAD){.auid = 1, .vnum = 100}, "Test Object", "TestBuilder");

    /* Stage some list operations */
    json_t *v1 = json_pack("{s:s}", "location", "APPLY_STR");
    json_t *v2 = json_pack("{s:s}", "location", "APPLY_DEX");
    json_t *v3 = json_pack("{s:i}", "index", 2);
    json_t *v4 = json_string("some value");

    olc_changeset_add_change(cs, "affects/add:0", OLC_FIELD_LIST_ADD, NULL, v1);
    olc_changeset_add_change(cs, "affects/add:1", OLC_FIELD_LIST_ADD, NULL, v2);
    olc_changeset_add_change(cs, "affects/rm:0", OLC_FIELD_LIST_REMOVE, NULL, v3);
    olc_changeset_add_change(cs, "Name", OLC_FIELD_STRING, NULL, v4);
    json_decref(v1); json_decref(v2); json_decref(v3); json_decref(v4);

    TEST_ASSERT_INT_EQUALS(4, olc_changeset_count(cs));

    /* Revert affects prefix should remove 3, leave Name */
    int removed = olc_changeset_revert_prefix(cs, "affects");
    TEST_ASSERT_INT_EQUALS(3, removed);
    TEST_ASSERT_INT_EQUALS(1, olc_changeset_count(cs));
    TEST_ASSERT_NOT_NULL(olc_changeset_find_change(cs, "Name"));

    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}
```

- [ ] **Step 3: Add test for multi-level wildcard matching**

```c
static test_result_t test_olccs_multilevel_wildcard(test_context_t *ctx)
{
    /* Test the field_path_matches function indirectly via handler lookup.
     * Create a handler table with a ** pattern and verify it matches
     * multi-level paths. */
    static const olc_field_handler_t handlers[] = {
        { "typedata/**", OLC_FIELD_TYPE_DATA, NULL, NULL, NULL },
        { "var/*",       OLC_FIELD_STRING,    NULL, NULL, NULL },
        { NULL, 0, NULL, NULL, NULL }
    };

    /* Multi-level should match */
    const olc_field_handler_t *h;
    h = olc_find_field_handler(handlers, "typedata/weapon/class", OLC_FIELD_TYPE_DATA);
    TEST_ASSERT_NOT_NULL(h);

    h = olc_find_field_handler(handlers, "typedata/+armor", OLC_FIELD_TYPE_DATA);
    TEST_ASSERT_NOT_NULL(h);

    /* Single-level should still work */
    h = olc_find_field_handler(handlers, "var/mykey", OLC_FIELD_STRING);
    TEST_ASSERT_NOT_NULL(h);

    /* Single-level should NOT match multi-level */
    h = olc_find_field_handler(handlers, "var/nested/key", OLC_FIELD_STRING);
    TEST_ASSERT_NULL(h);

    return TEST_SUCCESS;
}
```

- [ ] **Step 4: Add test for `olc_stage_list_add` and `olc_stage_list_remove`**

```c
static test_result_t test_olccs_stage_list_ops(test_context_t *ctx)
{
    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT,
        (WNUM_LOAD){.auid = 1, .vnum = 100}, "Test Object", "TestBuilder");

    /* Stage two adds */
    json_t *v1 = json_pack("{s:s, s:i}", "location", "APPLY_STR", "modifier", 5);
    json_t *v2 = json_pack("{s:s, s:i}", "location", "APPLY_DEX", "modifier", 3);
    olc_pending_change_t *c1 = olc_stage_list_add(cs, "affects", v1);
    olc_pending_change_t *c2 = olc_stage_list_add(cs, "affects", v2);
    json_decref(v1); json_decref(v2);

    TEST_ASSERT_NOT_NULL(c1);
    TEST_ASSERT_NOT_NULL(c2);
    TEST_ASSERT_STR_EQUALS("affects/add:0", c1->field_path);
    TEST_ASSERT_STR_EQUALS("affects/add:1", c2->field_path);

    /* Stage a remove */
    json_t *old = json_pack("{s:s}", "location", "APPLY_CON");
    olc_pending_change_t *c3 = olc_stage_list_remove(cs, "affects", 2, old);
    json_decref(old);

    TEST_ASSERT_NOT_NULL(c3);
    TEST_ASSERT_STR_EQUALS("affects/rm:0", c3->field_path);
    TEST_ASSERT_INT_EQUALS(2, json_integer_value(json_object_get(c3->new_value, "index")));

    TEST_ASSERT_INT_EQUALS(3, olc_changeset_count(cs));

    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}
```

- [ ] **Step 5: Register tests in dispatcher and module header**

Add to `tests/framework/test_dispatcher.c` handler_table:
```c
{ "olccs_next_seq",           test_olccs_next_seq },
{ "olccs_revert_prefix",      test_olccs_revert_prefix },
{ "olccs_multilevel_wildcard", test_olccs_multilevel_wildcard },
{ "olccs_stage_list_ops",     test_olccs_stage_list_ops },
```

Declare in `tests/framework/test_modules.h`:
```c
test_result_t test_olccs_next_seq(test_context_t *ctx);
test_result_t test_olccs_revert_prefix(test_context_t *ctx);
test_result_t test_olccs_multilevel_wildcard(test_context_t *ctx);
test_result_t test_olccs_stage_list_ops(test_context_t *ctx);
```

Add JSON test definitions to `tests/data/unit/olc_changeset_tests.json`.

- [ ] **Step 6: Build and run tests**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test:olccs_
```

Expected: All olccs_ tests pass (3 existing + 4 new = 7 total).

- [ ] **Step 7: Commit**

```bash
git commit -m "test(olc): add unit tests for list ops and wildcard infrastructure

Tests: olccs_next_seq, olccs_revert_prefix, olccs_multilevel_wildcard,
olccs_stage_list_ops.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 3: GMCP Editor.Set Extensions & Pending Display

**Files:**
- Modify: `gmcp_editor.c` (~lines 471-523 `handle_editor_set`, ~lines 581-606 `handle_editor_revert`)
- Modify: `editors/common/olc_staged.c` (~line 208 `olc_staged_cmd_pending`)

- [ ] **Step 1: Extend `handle_editor_set()` for list and type operations**

In `gmcp_editor.c`, modify the field type detection block (currently lines 493-497) to also detect list operations and type data from the field path:

```c
/* Determine field type from field path and JSON value type */
olc_field_type_t ftype = OLC_FIELD_STRING;
const char *effective_field = field;
bool is_list_op = false;

/* Check for list operation suffix */
size_t flen = strlen(field);
if (flen > 4 && strcmp(field + flen - 4, "/add") == 0) {
    ftype = OLC_FIELD_LIST_ADD;
    is_list_op = true;
} else if (flen > 3 && strcmp(field + flen - 3, "/rm") == 0) {
    ftype = OLC_FIELD_LIST_REMOVE;
    is_list_op = true;
} else if (flen > 4 && strcmp(field + flen - 4, "/upd") == 0) {
    ftype = OLC_FIELD_LIST_UPDATE;
    is_list_op = true;
} else if (strncmp(field, "typedata/", 9) == 0) {
    ftype = OLC_FIELD_TYPE_DATA;
} else if (json_is_integer(value)) {
    ftype = OLC_FIELD_INT;
} else if (json_is_boolean(value)) {
    ftype = OLC_FIELD_BOOL;
} else if (json_is_array(value)) {
    ftype = OLC_FIELD_MULTIFLAGS;
} else if (json_is_object(value)) {
    /* Only classify as embedded if it's a known embedded field.
     * This prevents arbitrary JSON objects from being misclassified. */
    static const char *embedded_fields[] = { "lock", "waypoints", NULL };
    bool is_embedded = false;
    for (int i = 0; embedded_fields[i]; i++) {
        if (strcmp(field, embedded_fields[i]) == 0) {
            is_embedded = true;
            break;
        }
    }
    if (is_embedded)
        ftype = OLC_FIELD_EMBEDDED;
}
```

For list operations, use the staging helpers instead of direct `add_change`:

```c
if (is_list_op) {
    olc_pending_change_t *result = NULL;
    /* Strip the operation suffix to get the list name */
    char list_name[MIL];
    strlcpy(list_name, field, sizeof(list_name));
    char *slash = strrchr(list_name, '/');
    if (slash) *slash = '\0';

    if (ftype == OLC_FIELD_LIST_ADD) {
        result = olc_stage_list_add(cs, list_name, value);
    } else if (ftype == OLC_FIELD_LIST_REMOVE) {
        int index = (int)json_integer_value(json_object_get(value, "index"));
        result = olc_stage_list_remove(cs, list_name, index, NULL);
    }

    if (result) {
        const char *type_str = (ftype == OLC_FIELD_LIST_ADD) ? "list_add" : "list_remove";
        gmcp_editor_send_field(d, entity_id, result->field_path,
            result->new_value, type_str, true);
    }
    return;
}
```

The rest of the function (scalar handling) stays the same.

- [ ] **Step 2: Extend `handle_editor_revert()` for prefix-based revert**

In the revert handler, after the single-field revert attempt, add prefix revert fallback:

```c
if (field) {
    /* Try exact field revert first */
    bool reverted = olc_changeset_revert_field(cs, field);
    if (!reverted) {
        /* Try prefix-based revert (e.g., "affects" reverts affects/*) */
        int prefix_removed = olc_changeset_revert_prefix(cs, field);
        if (prefix_removed == 0) {
            gmcp_editor_send_error(d, entity_id, field,
                "field_not_found", "No pending change for this field.");
            return;
        }
    }
}
```

- [ ] **Step 3: Update pending display for grouped list operations**

In `editors/common/olc_staged.c`, modify `olc_staged_cmd_pending()` (at ~line 208) to group list operations by prefix. The current display shows each change as a separate line. For list operations, group them:

```c
/* Inside the change iteration loop of olc_staged_cmd_pending(),
 * detect list operations and group them by prefix.
 *
 * Pseudocode for the grouping logic: */
static void format_list_group(BUFFER *buffer, const char *prefix,
    olc_changeset_t *cs)
{
    int add_count = 0, rm_count = 0;
    char add_prefix[MIL], rm_prefix[MIL];
    snprintf(add_prefix, sizeof(add_prefix), "%s/add:", prefix);
    snprintf(rm_prefix, sizeof(rm_prefix), "%s/rm:", prefix);
    size_t add_len = strlen(add_prefix);
    size_t rm_len = strlen(rm_prefix);

    ITERATOR it;
    iterator_start(&it, cs->changes);
    olc_pending_change_t *change;
    while ((change = iterator_nextdata(&it)) != NULL) {
        if (strncmp(change->field_path, add_prefix, add_len) == 0)
            add_count++;
        else if (strncmp(change->field_path, rm_prefix, rm_len) == 0)
            rm_count++;
    }
    iterator_stop(&it);

    if (add_count > 0)
        bprintf(buffer, "  {Y%s{x: {G+%d added{x\n\r", prefix, add_count);
    if (rm_count > 0)
        bprintf(buffer, "  {Y%s{x: {R-%d removed{x\n\r", prefix, rm_count);
}
```

The implementer should:
1. Detect list operation paths (contains `/add:` or `/rm:`)
2. Track which prefixes have already been displayed (avoid duplicates)
3. For each new prefix, call the grouping function
4. Skip individual list ops in the main display loop

Keep the existing flat display for non-list changes (scalars, embedded, typedata).

- [ ] **Step 4: Build and verify**

```bash
cd /sentience/src && ./build tests
```

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(olc): extend GMCP Editor.Set for list ops and prefix revert

Extend handle_editor_set() to detect list operation suffixes (/add,
/rm, /upd) and type data prefix (typedata/) from field paths.
Extend handle_editor_revert() with prefix-based bulk revert fallback.
Update pending display to group list operations by prefix.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 4: OEdit List Operations — Affects & Immunities

**Files:**
- Modify: `editors/objects/oedit.c` (lines 917-1172 for functions, ~340-365 handler table)

**Functions to convert (5):**
- `oedit_addaffect` (line 917) — adds AFFECT_DATA with TO_OBJECT
- `oedit_delaffect` (line 1977) — removes affect by index
- `oedit_addimmune` (line 1051) — adds AFFECT_DATA with TO_IMMUNE/RESIST/VULN
- `oedit_delimmune` (line 2037) — removes immunity by index
- `oedit_addskill` (line 1281) — adds AFFECT_DATA with APPLY_SKILL

### Conversion Pattern for List Add Functions

The pattern for converting `oedit_addXXX` to staged mode:

1. **Keep all existing validation** (argument parsing, permission checks, range checks)
2. **In staged mode:** Instead of creating the struct and appending to list, serialize the validated values to JSON and call `olc_stage_list_add()`
3. **In non-staged mode (fallback):** Keep existing direct mutation code

```c
/* After all validation, where the function currently does:
 *   AFFECT_DATA *pAf = new_affect();
 *   pAf->where = TO_OBJECT;
 *   pAf->location = loc;
 *   ... etc ...
 *   pAf->next = pObj->affected;
 *   pObj->affected = pAf;
 *
 * Replace with staged version:
 */
const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ED_OBJECT);
olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
    ? olc_get_active_changeset(ch, edef) : NULL;

if (cs) {
    json_t *val = json_pack("{s:s, s:s, s:i, s:i, s:i, s:i}",
        "where", flag_string(apply_types, TO_OBJECT),
        "location", flag_string(apply_flags, loc),
        "modifier", mod,
        "type", -1,
        "duration", -1,
        "bitvector", 0);
    olc_stage_list_add(cs, "affects", val);
    notify_field_change(cs, ch, "affects", val, "list_add", true);
    json_decref(val);
} else {
    /* existing direct mutation code */
    AFFECT_DATA *pAf = new_affect();
    /* ... */
}
```

### Conversion Pattern for List Remove Functions

**Important:** The remove JSON MUST include a `"where"` filter to distinguish TO_OBJECT affects from TO_IMMUNE/TO_RESIST/TO_VULN entries. Both `oedit_delaffect` and `oedit_delimmune` share the `affects/*` handler, but they index into different subsets of the same linked list. Without a `where` filter, removing immunity index 2 would incorrectly target the 2nd TO_OBJECT entry.

```c
/* After validation, where the function currently unlinks from list: */
if (cs) {
    /* Serialize the item being removed for history.
     * Include "where_filter" so the apply handler can find the right subset. */
    json_t *old_val = json_pack("{s:s, s:s, s:i}",
        "where", flag_string(apply_types, pAf->where),
        "location", flag_string(apply_flags, pAf->location),
        "modifier", pAf->modifier);

    /* For delaffect: where_filter = "TO_OBJECT"
     * For delimmune: where_filter = "TO_IMMUNE" or "TO_RESIST" or "TO_VULN" */
    json_t *rm_val = json_pack("{s:i, s:s}",
        "index", affect_index,
        "where_filter", flag_string(apply_types, pAf->where));
    /* Note: olc_stage_list_remove stores rm_val as new_value, old_val as old_value */
    olc_stage_list_remove(cs, "affects", affect_index, old_val);
    /* Patch the where_filter into the stored change's new_value */
    olc_pending_change_t *last = olc_changeset_find_last(cs, "affects/rm");
    if (last && last->new_value)
        json_object_set_new(last->new_value, "where_filter",
            json_string(flag_string(apply_types, pAf->where)));
    notify_field_change(cs, ch, "affects", rm_val,
        "list_remove", true);
    json_decref(old_val);
    json_decref(rm_val);
} else {
    /* existing unlink + free_affect code */
}
```

**Alternative simpler approach:** Instead of patching after the fact, modify `olc_stage_list_remove()` to accept a JSON `new_value` directly (instead of building `{"index": N}` internally). Then the caller builds the full JSON including `where_filter`. The implementer should choose whichever is cleaner.

### Apply Handler

- [ ] **Step 1: Write the apply handler for affects**

Add to oedit.c, near the other apply functions:

```c
/**
 * Apply a single staged affect operation (add or remove).
 * Called once per change by olc_changeset_commit() (which sorts list ops:
 * removes in descending index before adds in sequence order — see Task 1 Step 9).
 *
 * Remove operations use "where_filter" to select the correct subset
 * of the affects list (TO_OBJECT vs TO_IMMUNE/TO_RESIST/TO_VULN).
 */
static bool oedit_apply_affect_ops(void *entity, olc_pending_change_t *change)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;

    if (change->field_type == OLC_FIELD_LIST_REMOVE) {
        int index = (int)json_integer_value(
            json_object_get(change->new_value, "index"));

        /* Determine which subset of affects to index into */
        const char *where_str = json_string_value(
            json_object_get(change->new_value, "where_filter"));
        int where_filter = where_str ? flag_value(apply_types, where_str) : TO_OBJECT;

        /* For immunity/resist/vuln, match any of TO_IMMUNE|TO_RESIST|TO_VULN */
        bool is_immunity = (where_filter == TO_IMMUNE
            || where_filter == TO_RESIST || where_filter == TO_VULN);

        AFFECT_DATA *prev = NULL;
        int count = 0;
        for (AFFECT_DATA *pAf = pObj->affected; pAf; pAf = pAf->next) {
            bool in_subset;
            if (is_immunity)
                in_subset = (pAf->where == TO_IMMUNE || pAf->where == TO_RESIST
                    || pAf->where == TO_VULN);
            else
                in_subset = (pAf->where == where_filter);

            if (!in_subset) { prev = pAf; continue; }
            if (count == index) {
                if (prev) prev->next = pAf->next;
                else pObj->affected = pAf->next;
                free_affect(pAf);
                return true;
            }
            prev = pAf;
            count++;
        }
        return false; /* index out of range */
    }

    if (change->field_type == OLC_FIELD_LIST_ADD) {
        AFFECT_DATA *pAf = new_affect();
        pAf->where = flag_value(apply_types,
            json_string_value(json_object_get(change->new_value, "where")));
        pAf->location = flag_value(apply_flags,
            json_string_value(json_object_get(change->new_value, "location")));
        pAf->modifier = (int)json_integer_value(
            json_object_get(change->new_value, "modifier"));
        pAf->type = (int)json_integer_value(
            json_object_get(change->new_value, "type"));
        pAf->duration = (int)json_integer_value(
            json_object_get(change->new_value, "duration"));
        pAf->bitvector = (long)json_integer_value(
            json_object_get(change->new_value, "bitvector"));
        pAf->level = 0;

        pAf->next = pObj->affected;
        pObj->affected = pAf;
        return true;
    }

    return false;
}
```

- [ ] **Step 2: Add handler table entry**

Add to `oedit_field_handlers[]` array (before the NULL terminator):
```c
{ "affects/*", OLC_FIELD_LIST_ADD, NULL, oedit_apply_affect_ops, NULL },
```

- [ ] **Step 3: Convert all 5 functions**

Apply the staging patterns shown above to each function. Keep all existing validation intact. The staged path stages JSON; the non-staged fallback keeps existing direct mutation.

For `oedit_addimmune`, the JSON format includes the `where` field to distinguish TO_IMMUNE/TO_RESIST/TO_VULN:
```json
{"where": "TO_IMMUNE", "location": "APPLY_NONE", "modifier": 0,
 "bitvector": 4096, "type": -1, "duration": -1}
```

For `oedit_addskill`, include the skill UID for reliable deserialization:
```json
{"where": "TO_OBJECT", "location": "APPLY_SKILL",
 "modifier": 25, "skill_uid": 42, "type": -1, "duration": -1, "bitvector": 0}
```

- [ ] **Step 4: Build and test**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test
```

Expected: 604+ total, no regressions.

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(olc): convert oedit affect list operations to staged mode

Convert oedit_addaffect, oedit_delaffect, oedit_addimmune,
oedit_delimmune, oedit_addskill to use olc_stage_list_add/remove.
Add oedit_apply_affect_ops handler for affects/* changes.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 5: OEdit List Operations — Spells, Catalysts, Quests, Scripts

**Files:**
- Modify: `editors/objects/oedit.c`

**Functions to convert (8):**
- `oedit_addspell` (line 1175) / `oedit_delspell` (line 1451) — list: `spells`
- `oedit_addcatalyst` (line 1356) / `oedit_delcatalyst` (line 1500) — list: `catalysts`
- `oedit_addquest` (line 3533) / `oedit_delquest` (line 3581) — list: `quests`
- `oedit_addoprog` (line 3263) / `oedit_deloprog` (line 3372) — list: `oprogs`

### JSON Schemas

**Spell:**
```json
{"spell_uid": 42, "spell_name": "fireball", "level": 20, "repop": 100}
```

**Catalyst:**
```json
{"type": "fire", "strength": 5, "charges": 10, "chance": 80,
 "where": "TO_CATALYST_ACTIVE", "custom_name": ""}
```
Note: `oedit_addcatalyst` has merge logic (if duplicate type/where/strength/chance, increments charges). In staged mode, stage the add as normal — the apply function handles merging.

**Quest:**
```json
{"auid": 5, "vnum": 100}
```

**OProg:**
```json
{"script_auid": 5, "script_vnum": 100, "trigger": "greet_prog", "phrase": "100"}
```

**OProg remove** — note `oedit_deloprog` supports removing an entire script group OR a specific trigger within a group. The JSON should capture this:
```json
{"group_index": 0}                          /* remove entire group */
{"group_index": 0, "trigger_index": 1}      /* remove specific trigger */
```

### Apply Handlers

Each list needs its own apply handler following the same pattern as `oedit_apply_affect_ops()`:

```c
static bool oedit_apply_spell_ops(void *entity, olc_pending_change_t *change);
static bool oedit_apply_catalyst_ops(void *entity, olc_pending_change_t *change);
static bool oedit_apply_quest_ops(void *entity, olc_pending_change_t *change);
static bool oedit_apply_oprog_ops(void *entity, olc_pending_change_t *change);
```

### Handler Table Entries

```c
{ "spells/*",    OLC_FIELD_LIST_ADD, NULL, oedit_apply_spell_ops,    NULL },
{ "catalysts/*", OLC_FIELD_LIST_ADD, NULL, oedit_apply_catalyst_ops, NULL },
{ "quests/*",    OLC_FIELD_LIST_ADD, NULL, oedit_apply_quest_ops,    NULL },
{ "oprogs/*",    OLC_FIELD_LIST_ADD, NULL, oedit_apply_oprog_ops,    NULL },
```

- [ ] **Step 1: Write apply handlers for all 4 lists**
- [ ] **Step 2: Add handler table entries**
- [ ] **Step 3: Convert `oedit_addspell` and `oedit_delspell`**
- [ ] **Step 4: Convert `oedit_addcatalyst` and `oedit_delcatalyst`**

Special: catalyst add has merge logic. In staged mode, always stage as a new add. The apply handler should check for duplicates and merge if needed (same logic as the current function).

- [ ] **Step 5: Convert `oedit_addquest` and `oedit_delquest`**

Note: `parse_widevnum()` takes `char *` (mutable). The conversion must work with the existing argument parsing.

- [ ] **Step 6: Convert `oedit_addoprog` and `oedit_deloprog`**

Note: `oedit_deloprog` uses `prog_build_groups()` which returns a temporary group array. In staged mode, the JSON stores group_index and optional trigger_index. The apply handler must call `prog_build_groups()` at apply time to resolve the indices.

- [ ] **Step 7: Build and test**

```bash
cd /sentience/src && ./build tests && ./install debug && cd /sentience && ./sent -test
```

- [ ] **Step 8: Commit**

```bash
git commit -m "feat(olc): convert oedit spell/catalyst/quest/script lists to staged mode

Convert oedit_addspell, oedit_delspell, oedit_addcatalyst,
oedit_delcatalyst, oedit_addquest, oedit_delquest, oedit_addoprog,
oedit_deloprog to staged list operations.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 6: OEdit Complex Structs — Lock & Waypoints

**Files:**
- Modify: `editors/objects/oedit.c` (lines 1579-1757 waypoints, lines 1759-1926 lock)

**Functions to convert (2):**
- `oedit_lock` (line 1759) — LOCK_STATE embedded struct
- `oedit_waypoints` (line 1579) — WAYPOINT_DATA list (managed as embedded snapshot)

### Lock Serialization

**Note:** `LOCK_STATE` also has a `special_keys` (LLIST) field, but it is NOT editable via the lock sub-commands in oedit — it is managed by the instance/area system. We deliberately exclude it from serialization. The apply handler does not touch `special_keys`.

```c
static json_t *oedit_serialize_lock(void *entity, const char *field_path)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    if (!pObj->lock) return json_null();

    return json_pack("{s:i, s:i, s:i, s:i}",
        "key_auid", (int)pObj->lock->key_load.auid,
        "key_vnum", (int)pObj->lock->key_load.vnum,
        "flags", (int)pObj->lock->flags,
        "pick_chance", pObj->lock->pick_chance);
}
```

### Lock Staged Mode Pattern

Each sub-command (`add`, `remove`, `key`, `flags`, `pick`) works on the staged JSON snapshot:

```c
/* In oedit_lock, for the "key" sub-command: */
if (cs) {
    json_t *snapshot = olc_staged_embedded(ch, "lock");
    if (!snapshot) {
        /* First edit: serialize live struct */
        json_t *old_val = oedit_serialize_lock(pObj, "lock");
        json_t *new_val = json_deep_copy(old_val);
        json_object_set_new(new_val, "key_auid", json_integer(key_auid));
        json_object_set_new(new_val, "key_vnum", json_integer(key_vnum));
        olc_changeset_add_change(cs, "lock", OLC_FIELD_EMBEDDED, old_val, new_val);
        json_decref(old_val);
        json_decref(new_val);
    } else {
        /* Update existing snapshot */
        olc_staged_embedded_set(ch, "lock", "key_auid", json_integer(key_auid));
        olc_staged_embedded_set(ch, "lock", "key_vnum", json_integer(key_vnum));
    }
    notify_field_change(cs, ch, "lock", json_null(), "embedded", true);
} else {
    /* existing direct mutation */
}
```

### Lock Apply Handler

```c
static bool oedit_apply_lock(void *entity, olc_pending_change_t *change)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    json_t *data = change->new_value;

    if (json_is_null(data)) {
        /* Lock was removed */
        if (pObj->lock) {
            free_lock_state(pObj->lock);
            pObj->lock = NULL;
        }
        return true;
    }

    /* Create lock if needed */
    if (!pObj->lock)
        pObj->lock = new_lock_state();

    pObj->lock->key_load.auid = json_integer_value(json_object_get(data, "key_auid"));
    pObj->lock->key_load.vnum = json_integer_value(json_object_get(data, "key_vnum"));
    pObj->lock->key_wnum = resolve_wnum(pObj->lock->key_load);
    pObj->lock->flags = (int)json_integer_value(json_object_get(data, "flags"));
    pObj->lock->pick_chance = (int)json_integer_value(json_object_get(data, "pick_chance"));

    return true;
}
```

### Waypoints — same embedded pattern

Waypoints are a list managed as a single embedded snapshot (the entire waypoint list is serialized/deserialized as one JSON array). Sub-commands (`add`, `delete`) modify the staged JSON array.

### Handler Table Entries

```c
{ "lock",      OLC_FIELD_EMBEDDED, oedit_serialize_lock,      oedit_apply_lock,      NULL },
{ "waypoints", OLC_FIELD_EMBEDDED, oedit_serialize_waypoints, oedit_apply_waypoints, NULL },
```

- [ ] **Step 1: Write lock serialize/apply functions**
- [ ] **Step 2: Convert `oedit_lock` sub-commands to staged mode**
- [ ] **Step 3: Write waypoints serialize/apply functions**
- [ ] **Step 4: Convert `oedit_waypoints` sub-commands to staged mode**
- [ ] **Step 5: Add handler table entries**
- [ ] **Step 6: Build and test**
- [ ] **Step 7: Commit**

```bash
git commit -m "feat(olc): convert oedit lock and waypoints to staged embedded mode

Serialize LOCK_STATE and WAYPOINT_DATA list as JSON snapshots.
Sub-commands modify staged JSON; apply functions deserialize on commit.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 7: Type Dispatch Framework + addtype/removetype

**Files:**
- Modify: `editors/objects/oedit.c` (handler table, dispatch functions)
- Modify: `editors/objects/oedit_types.c` (type handler table, per-type apply stubs)

### Type Handler Infrastructure

- [ ] **Step 1: Define type handler typedef and dispatch table**

In `oedit_types.c`, add near the top (after includes):

```c
/**
 * Per-type field apply function.
 * Receives the entity, the field name (after the type prefix), and the change.
 * E.g., for path "typedata/weapon/class", field_name = "class".
 */
typedef bool (*oedit_type_apply_fn)(OBJ_INDEX_DATA *pObj,
    const char *field_name, olc_pending_change_t *change);

/**
 * Per-type serialization function.
 * Returns a JSON object with all fields for this type.
 */
typedef json_t *(*oedit_type_serialize_fn)(const OBJ_INDEX_DATA *pObj);

typedef struct {
    const char             *type_name;
    oedit_type_apply_fn     apply_fn;
    oedit_type_serialize_fn serialize_fn;
} oedit_type_dispatch_t;
```

- [ ] **Step 2: Create the dispatch table with stubs**

```c
/* Forward declarations — implemented in Tasks 8-10 */
static bool weapon_apply_field(OBJ_INDEX_DATA *pObj, const char *field, olc_pending_change_t *change);
static json_t *weapon_serialize(const OBJ_INDEX_DATA *pObj);
/* ... one pair per type ... */

static const oedit_type_dispatch_t type_dispatch[] = {
    { "armor",          armor_apply_field,          armor_serialize },
    { "bodypart",       bodypart_apply_field,       bodypart_serialize },
    { "book",           book_apply_field,           book_serialize },
    /* ... all 32 types ... */
    { "weaponcontainer", weaponcontainer_apply_field, weaponcontainer_serialize },
    { NULL, NULL, NULL }
};
```

- [ ] **Step 3: Implement dispatch functions in oedit.c (or oedit_types.c)**

```c
/**
 * Apply a typedata/** change by dispatching to the per-type handler.
 * Path format: "typedata/{type_name}/{field_name}" or "typedata/+{type}" / "typedata/-{type}"
 */
static bool oedit_apply_typedata(void *entity, olc_pending_change_t *change)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    const char *path = change->field_path;

    /* Skip "typedata/" prefix */
    const char *rest = path + 9; /* strlen("typedata/") */

    /* Check for addtype/removetype */
    if (rest[0] == '+') {
        const char *type_name = rest + 1;
        int type_flag = flag_value(type_flags, type_name);
        if (type_flag == NO_FLAG) return false;
        obj_index_alloc_type_data(pObj, type_flag);
        return true;
    }
    if (rest[0] == '-') {
        const char *type_name = rest + 1;
        int type_flag = flag_value(type_flags, type_name);
        if (type_flag == NO_FLAG) return false;
        obj_index_remove_type(pObj, type_flag);
        return true;
    }

    /* Regular type field: rest = "weapon/class" */
    char type_name[MIL];
    const char *slash = strchr(rest, '/');
    if (!slash) return false;

    size_t tlen = slash - rest;
    if (tlen >= sizeof(type_name)) return false;
    memcpy(type_name, rest, tlen);
    type_name[tlen] = '\0';

    const char *field_name = slash + 1;

    /* Find type handler */
    for (int i = 0; type_dispatch[i].type_name; i++) {
        if (strcmp(type_dispatch[i].type_name, type_name) == 0) {
            return type_dispatch[i].apply_fn(pObj, field_name, change);
        }
    }

    return false;
}

static json_t *oedit_serialize_typedata(void *entity, const char *field_path)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    const char *rest = field_path + 9;

    char type_name[MIL];
    const char *slash = strchr(rest, '/');
    if (!slash) {
        /* Serialize entire type */
        strlcpy(type_name, rest, sizeof(type_name));
    } else {
        size_t tlen = slash - rest;
        if (tlen >= sizeof(type_name)) return json_null();
        memcpy(type_name, rest, tlen);
        type_name[tlen] = '\0';
    }

    for (int i = 0; type_dispatch[i].type_name; i++) {
        if (strcmp(type_dispatch[i].type_name, type_name) == 0
            && type_dispatch[i].serialize_fn) {
            return type_dispatch[i].serialize_fn(pObj);
        }
    }

    return json_null();
}
```

- [ ] **Step 4: Add handler table entry in oedit.c**

```c
{ "typedata/**", OLC_FIELD_TYPE_DATA, oedit_serialize_typedata, oedit_apply_typedata, NULL },
```

- [ ] **Step 5: Convert `oedit_addtype` and `oedit_removetype`**

```c
/* In oedit_addtype, after validation: */
if (cs) {
    char path[MIL];
    snprintf(path, sizeof(path), "typedata/+%s",
        flag_string(type_flags, type_flag));
    json_t *val = json_true(); /* marker value */
    olc_changeset_add_change(cs, path, OLC_FIELD_TYPE_DATA, NULL, val);
    notify_field_change(cs, ch, path, val, "type_data", true);
    json_decref(val);
} else {
    obj_index_alloc_type_data(pObj, type_flag);
}
```

- [ ] **Step 6: Build and test**
- [ ] **Step 7: Commit**

```bash
git commit -m "feat(olc): add type dispatch framework and convert addtype/removetype

Single typedata/** wildcard handler with internal dispatch table.
Per-type apply/serialize function pairs (stubs for Tasks 8-10).
Convert oedit_addtype and oedit_removetype to staged mode.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 8: Type Conversions — Simple Types (16 types)

**Files:**
- Modify: `editors/objects/oedit_types.c`

**Types (sorted by line number):**
book, bodypart, compass, food, ink, jewelry, light, map, mist, money, page, scroll, sextant, tattoo, tool, trade

These types have only simple fields (int, flag, string — no widevnum, no side effects, no list manipulation).

### Conversion Pattern for Type Commands

Each type command currently has this pattern:
```c
OEDIT(oedit_compass) {
    /* ... */
    argument = one_argument(argument, field);
    if (!str_prefix(field, "bearing")) {
        COMPASS(pObj)->accuracy = atoi(argument);
        send_to_char("Bearing accuracy set.\n\r", ch);
        return true;
    }
}
```

Convert to staged mode:
```c
OEDIT(oedit_compass) {
    /* ... */
    argument = one_argument(argument, field);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ED_OBJECT);
    olc_changeset_t *cs = (edef && edef->change_mode == OLC_CHANGE_STAGED)
        ? olc_get_active_changeset(ch, edef) : NULL;

    if (!str_prefix(field, "bearing")) {
        int val = atoi(argument);
        if (cs) {
            char path[MIL];
            snprintf(path, sizeof(path), "typedata/compass/accuracy");
            json_t *old_val = json_integer(COMPASS(pObj)->accuracy);
            json_t *new_val = json_integer(val);
            olc_changeset_add_change(cs, path, OLC_FIELD_TYPE_DATA, old_val, new_val);
            notify_field_change(cs, ch, path, new_val, "type_data", true);
            json_decref(old_val);
            json_decref(new_val);
        } else {
            COMPASS(pObj)->accuracy = val;
        }
        send_to_char("Bearing accuracy set.\n\r", ch);
        return true;
    }
}
```

### Per-Type Apply Functions

Each type needs an apply function that maps field names to struct member assignments:

```c
static bool compass_apply_field(OBJ_INDEX_DATA *pObj,
    const char *field, olc_pending_change_t *change)
{
    if (!IS_COMPASS(pObj)) return false;
    int val = (int)json_integer_value(change->new_value);

    if (strcmp(field, "accuracy") == 0) {
        COMPASS(pObj)->accuracy = val;
        return true;
    }

    return false;
}
```

For flag fields (XOR toggle), the staged value should be the **result** after toggling, not the toggle mask. This way the apply function does a simple assignment:

```c
/* In the command function, for a flag field: */
if (cs) {
    long current = olc_staged_flags_or(cs, path, FURNITURE(pObj)->flags);
    long new_flags = current ^ toggle;
    /* Stage the result, not the toggle */
    json_t *old_val = json_integer(FURNITURE(pObj)->flags);
    json_t *new_val = json_integer(new_flags);
    olc_changeset_add_change(cs, path, OLC_FIELD_TYPE_DATA, old_val, new_val);
    /* ... */
}
```

### Per-Type Serialize Functions

```c
static json_t *compass_serialize(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_COMPASS(pObj)) return json_null();
    return json_pack("{s:i}", "accuracy", COMPASS(pObj)->accuracy);
}
```

- [ ] **Step 1: Implement apply + serialize for all 16 simple types**
- [ ] **Step 2: Convert all 16 type command functions to staged mode**
- [ ] **Step 3: Verify dispatch table entries match function names**
- [ ] **Step 4: Build and test**
- [ ] **Step 5: Commit**

```bash
git commit -m "feat(olc): convert 16 simple oedit type commands to staged mode

Types: book, bodypart, compass, food, ink, jewelry, light, map, mist,
money, page, scroll, sextant, tattoo, tool, trade.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 9: Type Conversions — Medium Types (10 types)

**Files:**
- Modify: `editors/objects/oedit_types.c`

**Types:** armor, cart, container, corpse, drink, furniture, instrument, wand, weapon, weaponcontainer

These have more fields and some have special considerations:
- **armor**: calls `set_armour(pObj)` after strength change — apply must call it too
- **weapon**: calls `set_weapon_dice(pObj)` after dice change — apply must call it too
- **container**: imp_sig validation for weight_multiplier < 100%
- **corpse**: widevnum parsing for mobile_vnum (store as auid+vnum in JSON)
- **drink**: liquid lookup via `liq_lookup()` — store as string name in JSON

### Side Effects in Apply Functions

```c
static bool armor_apply_field(OBJ_INDEX_DATA *pObj,
    const char *field, olc_pending_change_t *change)
{
    if (!IS_ARMOR(pObj)) return false;

    if (strcmp(field, "strength") == 0) {
        ARMOR(pObj)->armor_strength = (int)json_integer_value(change->new_value);
        set_armour(pObj);  /* MUST call side effect */
        return true;
    }
    /* ... other fields ... */
}
```

### Widevnum Fields

For corpse mobile_vnum, seed object_vnum, etc., store both auid and vnum:
```json
{"mobile_auid": 5, "mobile_vnum": 3001}
```

The command function parses the widevnum and stores both values. The apply function sets both struct members.

- [ ] **Step 1: Implement apply + serialize for all 10 medium types**
- [ ] **Step 2: Convert all 10 type command functions to staged mode**
- [ ] **Step 3: Ensure side effects (set_armour, set_weapon_dice) are called in apply functions**
- [ ] **Step 4: Build and test**
- [ ] **Step 5: Commit**

```bash
git commit -m "feat(olc): convert 10 medium oedit type commands to staged mode

Types: armor, cart, container, corpse, drink, furniture, instrument,
wand, weapon, weaponcontainer. Side effects preserved in apply handlers.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 10: Type Conversions — Complex Types (6 types)

**Files:**
- Modify: `editors/objects/oedit_types.c`

**Types:** herb, portal, seed, ship, shipmodule, telescope

These have significant complexity:
- **portal**: 8+ fields with conditional logic based on GATE_DUNGEON/GATE_AREARANDOM flags. Flag toggles clear other fields (setting DUNGEON clears params). Widevnum parsing for destinations.
- **shipmodule**: 28+ fields including ammo widevnum reference. Largest type command (275 lines).
- **herb**: 8 fields including spell lookup and imm/res/vuln flag toggles.
- **telescope**: Cross-field validation (min_distance ≤ max_distance).
- **ship**: 9 fields including widevnum for first_room.
- **seed**: 4 fields including widevnum for object.

### Portal Special Handling

Portal flag changes have side effects (clearing params). In staged mode, the command function must:
1. Read current staged flags (or live if no change)
2. Apply the toggle
3. Apply side effects (clear params if DUNGEON toggled on)
4. Stage all affected fields

This may require staging multiple typedata paths in one command:
```c
/* Setting DUNGEON flag on */
olc_changeset_add_change(cs, "typedata/portal/flags", ...);
olc_changeset_add_change(cs, "typedata/portal/params0", json_integer(0), json_integer(-1));
/* etc. */
```

### Ship Module — Batch Approach

Given 28+ fields, the apply function should use a lookup table approach:
```c
typedef struct {
    const char *name;
    size_t      offset;
    enum { SMF_INT, SMF_INT16, SMF_FLAGS } type;
} shipmodule_field_desc_t;

static const shipmodule_field_desc_t sm_fields[] = {
    { "type",          offsetof(SHIP_MODULE_DATA, type),          SMF_INT },
    { "size",          offsetof(SHIP_MODULE_DATA, size),          SMF_INT },
    { "weight",        offsetof(SHIP_MODULE_DATA, weight),        SMF_INT },
    /* ... all fields ... */
    { NULL, 0, 0 }
};
```

This avoids a 28-case switch statement.

- [ ] **Step 1: Implement apply + serialize for herb, telescope, seed**
- [ ] **Step 2: Implement apply + serialize for ship, shipmodule (table-driven for shipmodule)**
- [ ] **Step 3: Implement apply + serialize for portal (most complex — handle flag side effects)**
- [ ] **Step 4: Convert all 6 type command functions to staged mode**
- [ ] **Step 5: Build and test**
- [ ] **Step 6: Commit**

```bash
git commit -m "feat(olc): convert 6 complex oedit type commands to staged mode

Types: herb, portal, seed, ship, shipmodule, telescope.
Portal handles flag-dependent param clearing in staged mode.
Shipmodule uses table-driven field dispatch for 28+ fields.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 11: Integration Tests

**Files:**
- Modify: `tests/unit/olc_changeset_tests.c`
- Modify: `tests/data/unit/olc_changeset_tests.json`

Add integration-level tests that verify the full staging → commit → apply flow:

- [ ] **Step 1: Test list operation apply (affects add + remove)**

```c
static test_result_t test_olccs_apply_list_ops(test_context_t *ctx)
{
    /* Set up a test OBJ_INDEX_DATA with 3 existing TO_OBJECT affects */
    OBJ_INDEX_DATA *pObj = new_obj_index();
    for (int i = 0; i < 3; i++) {
        AFFECT_DATA *pAf = new_affect();
        pAf->where = TO_OBJECT;
        pAf->location = APPLY_STR + i; /* STR, DEX, INT */
        pAf->modifier = 10 + i;
        pAf->next = pObj->affected;
        pObj->affected = pAf;
    }

    /* Create changeset with 1 remove (index 1 = DEX) and 1 add */
    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT,
        (WNUM_LOAD){.auid = 1, .vnum = 100}, "Test Object", "TestBuilder");

    json_t *rm_val = json_pack("{s:i, s:s}", "index", 1, "where_filter", "TO_OBJECT");
    json_t *rm_old = json_pack("{s:s}", "location", "APPLY_DEX");
    olc_changeset_add_change(cs, "affects/rm:0", OLC_FIELD_LIST_REMOVE, rm_old, rm_val);
    json_decref(rm_val); json_decref(rm_old);

    json_t *add_val = json_pack("{s:s, s:s, s:i, s:i, s:i, s:i}",
        "where", "TO_OBJECT", "location", "APPLY_CON",
        "modifier", 5, "type", -1, "duration", -1, "bitvector", 0);
    olc_changeset_add_change(cs, "affects/add:0", OLC_FIELD_LIST_ADD, NULL, add_val);
    json_decref(add_val);

    /* Apply via oedit handler table (commit dispatches to oedit_apply_affect_ops).
     * The implementer must wire this to call olc_changeset_commit with the
     * oedit_field_handlers table and pObj as entity. */
    bool result = olc_changeset_commit(cs, oedit_field_handlers, pObj);
    TEST_ASSERT_TRUE(result);

    /* Verify: should have 3 affects (3 - 1 + 1), DEX removed, CON added */
    int count = 0;
    bool found_con = false;
    bool found_dex = false;
    for (AFFECT_DATA *pAf = pObj->affected; pAf; pAf = pAf->next) {
        if (pAf->where != TO_OBJECT) continue;
        count++;
        if (pAf->location == APPLY_CON) found_con = true;
        if (pAf->location == APPLY_DEX) found_dex = true;
    }
    TEST_ASSERT_INT_EQUALS(3, count);
    TEST_ASSERT_TRUE(found_con);
    TEST_ASSERT_FALSE(found_dex);

    olc_changeset_destroy(cs);
    /* clean up test object (implementer determines cleanup method) */
    return TEST_SUCCESS;
}
```

- [ ] **Step 2: Test embedded snapshot apply (lock)**

```c
static test_result_t test_olccs_apply_embedded(test_context_t *ctx)
{
    OBJ_INDEX_DATA *pObj = new_obj_index();
    /* Object starts with no lock */
    TEST_ASSERT_NULL(pObj->lock);

    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT,
        (WNUM_LOAD){.auid = 1, .vnum = 100}, "Test Object", "TestBuilder");

    /* Stage a lock creation */
    json_t *new_lock = json_pack("{s:i, s:i, s:i, s:i}",
        "key_auid", 5, "key_vnum", 3001, "flags", 1, "pick_chance", 50);
    olc_changeset_add_change(cs, "lock", OLC_FIELD_EMBEDDED, json_null(), new_lock);
    json_decref(new_lock);

    bool result = olc_changeset_commit(cs, oedit_field_handlers, pObj);
    TEST_ASSERT_TRUE(result);

    TEST_ASSERT_NOT_NULL(pObj->lock);
    TEST_ASSERT_INT_EQUALS(5, (int)pObj->lock->key_load.auid);
    TEST_ASSERT_INT_EQUALS(3001, (int)pObj->lock->key_load.vnum);
    TEST_ASSERT_INT_EQUALS(1, pObj->lock->flags);
    TEST_ASSERT_INT_EQUALS(50, pObj->lock->pick_chance);

    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}
```

- [ ] **Step 3: Test type dispatch apply**

```c
static test_result_t test_olccs_apply_typedata(test_context_t *ctx)
{
    OBJ_INDEX_DATA *pObj = new_obj_index();
    /* Allocate weapon type data on the object */
    obj_index_alloc_type_data(pObj, ITEM_WEAPON);
    TEST_ASSERT_TRUE(IS_WEAPON(pObj));

    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT,
        (WNUM_LOAD){.auid = 1, .vnum = 100}, "Test Object", "TestBuilder");

    /* Stage a weapon class change */
    json_t *old_val = json_integer(WEAPON(pObj)->weapon_class);
    json_t *new_val = json_integer(WEAPON_SWORD);
    olc_changeset_add_change(cs, "typedata/weapon/class", OLC_FIELD_TYPE_DATA,
        old_val, new_val);
    json_decref(old_val); json_decref(new_val);

    bool result = olc_changeset_commit(cs, oedit_field_handlers, pObj);
    TEST_ASSERT_TRUE(result);

    TEST_ASSERT_INT_EQUALS(WEAPON_SWORD, WEAPON(pObj)->weapon_class);

    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}
```

**Note:** These tests depend on Tasks 4, 6, and 7 respectively. The implementer may need to include the oedit handler table header or declare `extern` references to the handler tables. Adjust memory allocation/cleanup to match codebase patterns (check `new_obj_index()` availability or use test fixtures).

- [ ] **Step 4: Register tests, build, run**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test:olccs_
```

- [ ] **Step 5: Commit**

```bash
git commit -m "test(olc): add integration tests for list ops, embedded, and type dispatch

Tests: olccs_apply_list_ops, olccs_apply_embedded, olccs_apply_typedata.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 12: Final Verification

- [ ] **Step 1: Clean rebuild**

```bash
cd /sentience/src && ./build clean tests
```

Expected: Clean compile, no new warnings.

- [ ] **Step 2: Install and run full test suite**

```bash
./install debug && cd /sentience && ./sent -test
```

Expected: 604+ total, 588+ passed, 2 known failures, 14 skipped. No regressions.

- [ ] **Step 3: Run OLC-specific tests**

```bash
./sent -test:olccs_
```

Expected: All olccs_ tests pass (7 from Task 2 + 3 from Task 11 = 10+ total).

- [ ] **Step 4: Verify commit log**

```bash
cd /sentience/src && git --no-pager log --oneline -15
```

Verify all Phase 3 commits are present and well-ordered.

---

## Dependencies

```
Task 1 (Infrastructure) ─┬─→ Task 2 (Infra Tests)
                          ├─→ Task 3 (GMCP Extensions)
                          ├─→ Task 4 (Affects) ──→ Task 5 (Spells/Catalysts/Quests/Scripts)
                          ├─→ Task 6 (Lock/Waypoints)
                          └─→ Task 7 (Type Dispatch) ──→ Task 8 (Simple Types)
                                                      ──→ Task 9 (Medium Types)
                                                      ──→ Task 10 (Complex Types)
All Tasks 4-10 ──→ Task 11 (Integration Tests) ──→ Task 12 (Final Verify)
```

Tasks 2, 3, 4, 6, 7 can run in parallel after Task 1. Tasks 8-10 can run in parallel after Task 7. Task 5 depends on Task 4 (same file, same pattern — build on it).

**Shared file conflicts — sequential constraints:**
- **Tasks 4 & 6** both modify the `oedit_field_handlers[]` array in `oedit.c`. Run them sequentially (4 → 6) or coordinate handler table additions carefully.
- **Tasks 8, 9 & 10** all modify `oedit_types.c` (dispatch table + per-type functions). Run them sequentially (8 → 9 → 10) to avoid merge conflicts in the shared dispatch table.
- **In practice**, the recommended execution order is: 1 → (2, 3, 4 parallel) → 5 → 6 → 7 → 8 → 9 → 10 → 11 → 12.
