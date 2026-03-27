# Editor.Open Schema Capture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Send `Sentience.Editor.Open` GMCP messages with tab-grouped field schemas when builders enter OLC editors, enabling the web client to render dynamic editor forms.

**Architecture:** Add `capture_mode` to `OLC_LAYOUT_CTX` so existing `olc_display_*` functions record field descriptors as JSON instead of rendering text. An orchestrator runs each editor tab's show function in capture mode, collects fields into a tab array, merges constraint annotations, and builds the GMCP message. This replaces the current `Editor.State` sent on editor entry.

**Tech Stack:** C23, Jansson (JSON), existing OLC display/editor framework, GMCP protocol layer.

**Spec:** `docs/superpowers/specs/2026-03-27-editor-open-schema-design.md`

**Branch:** `tieryo/gmcp_editors` (active)

**Build/Test commands:**
```bash
cd /sentience/src && ./build tests          # Build with test support
cd /sentience && ./sent -test:olcsc_        # Run schema capture tests
cd /sentience && ./sent -test:gmcped_       # Run GMCP editor tests
cd /sentience && ./sent -test:olccs_        # Run changeset tests (regression)
```

**Test baseline:** 459 pass, 0 fail. Any new failures = regressions.

---

## File Map

### Modified files (no new source files, only new test files)

| File | Responsibility |
|------|---------------|
| `editors/common.h` | Add 3 fields to `OLC_LAYOUT_CTX`: `capture_mode`, `captured_fields`, `current_section` |
| `editors/common/olc_display.h` | Declare `olc_field_annotation_t`, `olc_schema_capture()`, flag-option helper |
| `editors/common/olc_display.c` | Capture-mode branches in ~14 display functions + `olc_schema_capture()` orchestrator |
| `editors/common/olc_editor.h` | Add `const olc_field_annotation_t *annotations` to `OLC_EDITOR_DEF` |
| `editors/common/olc_editor.c` | Replace `gmcp_editor_send_state()` with `gmcp_editor_send_open()` in `olc_editor_enter()` |
| `editors/rooms/redit.c` | Add `redit_annotations[]`, wire into `redit_def` |
| `editors/mobiles/medit.c` | Add `medit_annotations[]`, wire into `medit_def` |
| `editors/objects/oedit.c` | Add `oedit_annotations[]`, wire into `oedit_def` |
| `editors/areas/aedit.c` | Add `aedit_annotations[]`, wire into `aedit_def` |
| `gmcp_editor.c` | Add `gmcp_editor_build_open()`, `gmcp_editor_send_open()`, editor type mapper |
| `gmcp_editor.h` | Declare new functions |
| `docs/GMCP_WEB_CLIENT_REFERENCE.md` | Document `Editor.Open` message |

### New test files

| File | Responsibility |
|------|---------------|
| `tests/unit/olc_schema_capture_tests.c` | Test handler for `olcsc_` tests |
| `tests/data/unit/olc_schema_capture_tests.json` | Test case definitions |

### Build system updates (for new test files only)

| File | Change |
|------|--------|
| `CMakeLists.txt` | Add `tests/unit/olc_schema_capture_tests.c` to BUILD_TESTS section |
| `Makefile` | Add `tests/unit/olc_schema_capture_tests.c` to BUILD_TESTS section |
| `tests/framework/test_modules.h` | Declare `run_olc_schema_capture_test_case()` |
| `tests/framework/test_dispatcher.c` | Add `olcsc_` entry to handler_table |
| `tests/data/test_config.json` | Add `olc_schema_capture_tests` to suite lists |

---

## Task 1: Foundation — Struct Changes and Type Declarations

**Files:**
- Modify: `editors/common.h` (OLC_LAYOUT_CTX struct)
- Modify: `editors/common/olc_display.h` (new types and declarations)
- Modify: `editors/common/olc_editor.h` (OLC_EDITOR_DEF struct)
- Modify: `editors/common.c` (olc_layout_new / olc_layout_free)

- [ ] **Step 1: Add capture fields to OLC_LAYOUT_CTX**

In `editors/common.h`, add three fields to the `OLC_LAYOUT_CTX` struct after the existing `changeset` field:

```c
    struct olc_changeset *changeset;  /* existing field */
    /* Schema capture mode */
    bool         capture_mode;      /* When true, record field descriptors instead of rendering */
    json_t      *captured_fields;   /* JSON array of captured field descriptors (capture_mode only) */
    const char  *current_section;   /* Current section title from olc_display_section() */
```

This requires `#include <jansson.h>` at the top of `editors/common.h` if not already present. Check first — it may already be included transitively via `merc.h`.

- [ ] **Step 2: Add annotation type and schema capture declarations**

There is a circular include between `olc_editor.h` and `olc_display.h` (`olc_display.h` includes `olc_editor.h`). To add the annotation type to both, use this approach:

**In `editors/common/olc_editor.h`**, add a forward typedef before the `OLC_EDITOR_DEF` struct:

```c
/* Forward declaration — full struct definition in olc_display.h */
typedef struct olc_field_annotation olc_field_annotation_t;
```

Then add the `annotations` field inside `struct olc_editor_def`, after `field_handlers`:

```c
    const struct olc_field_handler *field_handlers;  /* existing */
    const olc_field_annotation_t   *annotations;     /* Field constraint annotations (NULL-terminated, may be NULL) */
```

**In `editors/common/olc_display.h`**, add after the existing includes/types.
Note: Do NOT repeat the typedef — it was forward-declared in `olc_editor.h` which is included above. Only define the struct body:

```c
#include <limits.h>  /* for INT_MIN, INT_MAX */

/**
 * Field constraint annotation for schema capture.
 * Matched by command name after capture to add min/max/max_length.
 * Terminate arrays with { NULL }.
 *
 * typedef is forward-declared in olc_editor.h to break circular include.
 */
struct olc_field_annotation {
    const char *command;     /* Field command name to match */
    int         min;         /* Minimum value (INT_MIN = unconstrained) */
    int         max;         /* Maximum value (INT_MAX = unconstrained) */
    int         max_length;  /* Max string length (0 = no limit) */
};

/**
 * Capture field schema from an editor's tab show functions.
 *
 * Runs each tab's show_fn in capture mode, collecting field descriptors.
 * Returns a JSON array of tab objects: [{"name","short_name","fields":[...]}]
 * Caller must json_decref() the result.
 *
 * @param ch       Character context (used for changeset lookup; may have NULL desc for tests)
 * @param def      Editor definition with tabs and optional annotations
 * @param entity   The entity being edited (passed to show_fn)
 * @param cs       Active changeset (for staged value display, may be NULL)
 * @return         JSON array of tab objects, or NULL on failure
 */
json_t *olc_schema_capture(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
                           void *entity, struct olc_changeset *cs);

/**
 * Build a JSON array of option name strings from a flag_type table.
 * Only includes settable flags. Caller must json_decref().
 */
json_t *olc_flag_options_json(const struct flag_type *table);
```

- [ ] **Step 4: Initialize new fields in olc_layout_new() and clean up in olc_layout_free()**

In `editors/common.c`, `olc_layout_new()`:
After the existing `ctx->changeset = NULL;` block, add:

```c
    ctx->capture_mode = false;
    ctx->captured_fields = NULL;
    ctx->current_section = NULL;
```

In `olc_layout_free()`:
Before `free_mem(ctx, sizeof(OLC_LAYOUT_CTX));`, add:

```c
    if (ctx->captured_fields) {
        json_decref(ctx->captured_fields);
        ctx->captured_fields = NULL;
    }
```

- [ ] **Step 5: Build to verify no compilation errors**

```bash
cd /sentience/src && ./build tests
```

Expected: Clean build. All existing editors get `annotations = NULL` by default (C designated initializers zero-fill unspecified fields).

- [ ] **Step 6: Run existing tests to verify no regressions**

```bash
cd /sentience && ./sent -test:olccs_ && ./sent -test:gmcped_ && ./sent -test:olcfw_
```

Expected: All 51 tests pass (30 olccs + 15 gmcped + 6 olcfw).

- [ ] **Step 7: Commit**

```bash
cd /sentience/src && git add editors/common.h editors/common/olc_display.h \
    editors/common/olc_editor.h editors/common.c
git commit -m "Add capture_mode fields to OLC_LAYOUT_CTX and annotation types

Foundation for Editor.Open schema capture:
- OLC_LAYOUT_CTX: capture_mode, captured_fields, current_section
- olc_field_annotation_t: per-field constraint metadata (min/max/max_length)
- OLC_EDITOR_DEF: annotations pointer for per-editor constraints
- olc_schema_capture() and olc_flag_options_json() declarations

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 2: Test Infrastructure Setup

**Files:**
- Create: `tests/unit/olc_schema_capture_tests.c`
- Create: `tests/data/unit/olc_schema_capture_tests.json`
- Modify: `tests/framework/test_modules.h`
- Modify: `tests/framework/test_dispatcher.c`
- Modify: `tests/data/test_config.json`
- Modify: `CMakeLists.txt`
- Modify: `Makefile`

- [ ] **Step 1: Create test JSON file with all test cases**

Create `tests/data/unit/olc_schema_capture_tests.json`:

```json
{
    "type": "test_suite",
    "test_suite": "olc_schema_capture_tests",
    "description": "OLC schema capture mode and Editor.Open message builder tests",
    "version": "1.0",
    "requires_mud_environment": false,
    "test_level": "unit",
    "tests": [
        {
            "name": "olcsc_capture_string",
            "test_type": "olcsc_capture_string",
            "description": "Capture mode records string field descriptor",
            "input": {},
            "expected_output": { "success": true }
        },
        {
            "name": "olcsc_capture_number",
            "test_type": "olcsc_capture_number",
            "description": "Capture mode records integer field descriptor",
            "input": {},
            "expected_output": { "success": true }
        },
        {
            "name": "olcsc_capture_bool",
            "test_type": "olcsc_capture_bool",
            "description": "Capture mode records boolean field descriptor",
            "input": {},
            "expected_output": { "success": true }
        },
        {
            "name": "olcsc_capture_text",
            "test_type": "olcsc_capture_text",
            "description": "Capture mode records multiline text field descriptor",
            "input": {},
            "expected_output": { "success": true }
        },
        {
            "name": "olcsc_capture_type",
            "test_type": "olcsc_capture_type",
            "description": "Capture mode records enum field with options",
            "input": {},
            "expected_output": { "success": true }
        },
        {
            "name": "olcsc_capture_flags",
            "test_type": "olcsc_capture_flags",
            "description": "Capture mode records flags field with options",
            "input": {},
            "expected_output": { "success": true }
        },
        {
            "name": "olcsc_capture_readonly",
            "test_type": "olcsc_capture_readonly",
            "description": "NULL command produces readonly field",
            "input": {},
            "expected_output": { "success": true }
        },
        {
            "name": "olcsc_capture_section",
            "test_type": "olcsc_capture_section",
            "description": "Section markers appear in captured fields",
            "input": {},
            "expected_output": { "success": true }
        },
        {
            "name": "olcsc_capture_pair",
            "test_type": "olcsc_capture_pair",
            "description": "Pair display produces two separate field entries",
            "input": {},
            "expected_output": { "success": true }
        },
        {
            "name": "olcsc_capture_skip_hr",
            "test_type": "olcsc_capture_skip_hr",
            "description": "HR, blank, header, footer produce no captured fields",
            "input": {},
            "expected_output": { "success": true }
        },
        {
            "name": "olcsc_flag_options_json",
            "test_type": "olcsc_flag_options_json",
            "description": "Flag table to JSON option array conversion",
            "input": {},
            "expected_output": { "success": true }
        },
        {
            "name": "olcsc_annotation_merge",
            "test_type": "olcsc_annotation_merge",
            "description": "Annotations add min/max/max_length to matching fields",
            "input": {},
            "expected_output": { "success": true }
        },
        {
            "name": "olcsc_build_open",
            "test_type": "olcsc_build_open",
            "description": "Build Editor.Open message with tabs and state",
            "input": {},
            "expected_output": { "success": true }
        },
        {
            "name": "olcsc_build_open_editor_type",
            "test_type": "olcsc_build_open_editor_type",
            "description": "Editor type mapping for known and unknown ED_* values",
            "input": {},
            "expected_output": { "success": true }
        }
    ]
}
```

- [ ] **Step 2: Create test handler C file skeleton**

Create `tests/unit/olc_schema_capture_tests.c`:

```c
/**
 * OLC Schema Capture Tests
 *
 * Tests for capture_mode in olc_display_* functions, the
 * olc_schema_capture() orchestrator, and Editor.Open message building.
 */
#ifdef BUILD_TESTS

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <jansson.h>
#include "../framework/test_framework.h"
#include "../../merc.h"
#include "../../olc.h"
#include "../../gmcp_editor.h"
#include "../../editors/common.h"
#include "../../editors/common/olc_display.h"
#include "../../editors/common/olc_editor.h"
#include "../../editors/common/olc_changeset.h"

/*
 * Helper: create a minimal capture-mode layout context.
 * Does not need a real CHAR_DATA — capture mode skips rendering.
 */
static OLC_LAYOUT_CTX *create_capture_ctx(void)
{
    OLC_LAYOUT_CTX *ctx = alloc_mem(sizeof(OLC_LAYOUT_CTX));
    memset(ctx, 0, sizeof(OLC_LAYOUT_CTX));
    ctx->capture_mode = true;
    ctx->captured_fields = json_array();
    ctx->screen_width = 80;
    ctx->label_width = 16;
    ctx->value_width = 58;
    return ctx;
}

static void free_capture_ctx(OLC_LAYOUT_CTX *ctx)
{
    if (!ctx) return;
    if (ctx->captured_fields)
        json_decref(ctx->captured_fields);
    free_mem(ctx, sizeof(OLC_LAYOUT_CTX));
}

/* ---- Stub tests — each will be filled as capture branches are implemented ---- */

static test_result_t test_capture_string(test_case_t *test)
{
    (void)test;
    /* Will test olc_display_string in capture mode */
    return TEST_SKIP;  /* Replaced when Task 3 implements capture */
}

static test_result_t test_capture_number(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_capture_bool(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_capture_text(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_capture_type(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_capture_flags(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_capture_readonly(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_capture_section(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_capture_pair(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_capture_skip_hr(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_flag_options_json(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_annotation_merge(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_build_open(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_build_open_editor_type(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

/* ---- Dispatcher ---- */

test_result_t run_olc_schema_capture_test_case(test_case_t *test)
{
    if (!test || !test->test_type) return TEST_ERROR;

    const char *type = test->test_type;
    if (!strcmp(type, "olcsc_capture_string"))        return test_capture_string(test);
    if (!strcmp(type, "olcsc_capture_number"))        return test_capture_number(test);
    if (!strcmp(type, "olcsc_capture_bool"))          return test_capture_bool(test);
    if (!strcmp(type, "olcsc_capture_text"))          return test_capture_text(test);
    if (!strcmp(type, "olcsc_capture_type"))          return test_capture_type(test);
    if (!strcmp(type, "olcsc_capture_flags"))         return test_capture_flags(test);
    if (!strcmp(type, "olcsc_capture_readonly"))      return test_capture_readonly(test);
    if (!strcmp(type, "olcsc_capture_section"))       return test_capture_section(test);
    if (!strcmp(type, "olcsc_capture_pair"))          return test_capture_pair(test);
    if (!strcmp(type, "olcsc_capture_skip_hr"))       return test_capture_skip_hr(test);
    if (!strcmp(type, "olcsc_flag_options_json"))     return test_flag_options_json(test);
    if (!strcmp(type, "olcsc_annotation_merge"))      return test_annotation_merge(test);
    if (!strcmp(type, "olcsc_build_open"))            return test_build_open(test);
    if (!strcmp(type, "olcsc_build_open_editor_type"))return test_build_open_editor_type(test);

    return TEST_SKIP;
}

#endif /* BUILD_TESTS */
```

- [ ] **Step 3: Register handler in test_modules.h**

Add after the `run_gmcp_editor_test_case` declaration:

```c
test_result_t run_olc_schema_capture_test_case(test_case_t *test);
```

- [ ] **Step 4: Add dispatcher entry in test_dispatcher.c**

Add to `handler_table[]` before the sentinel, near the other `olc*` entries:

```c
    { "olcsc_",                          run_olc_schema_capture_test_case,  MATCH_SUBSTR },
```

- [ ] **Step 5: Add test suite to test_config.json**

Add `"olc_schema_capture_tests"` to the following arrays in `test_config.json`:
- `default_test_suites`
- `unit_only_suites`
- Any profile that includes unit tests (e.g., `ci.suites`)

- [ ] **Step 6: Add test file to build systems**

In `CMakeLists.txt`, add in the `if(BUILD_TESTS)` block:
```cmake
        tests/unit/olc_schema_capture_tests.c
```

In `Makefile`, add in the `ifdef BUILD_TESTS` block:
```makefile
               tests/unit/olc_schema_capture_tests.c \
```

- [ ] **Step 7: Build and run skeleton tests**

```bash
cd /sentience/src && ./build tests
cd /sentience && ./sent -test:olcsc_
```

Expected: 14 tests, 0 pass, 0 fail, 14 skipped. The skeleton compiles and dispatches correctly.

- [ ] **Step 8: Commit**

```bash
cd /sentience/src && git add tests/unit/olc_schema_capture_tests.c \
    tests/data/unit/olc_schema_capture_tests.json \
    tests/framework/test_modules.h tests/framework/test_dispatcher.c \
    tests/data/test_config.json CMakeLists.txt Makefile
git commit -m "Add schema capture test infrastructure (14 skeleton tests)

Test suite olcsc_ with JSON data and C handler skeleton.
All tests skip until capture branches are implemented.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 3: Capture Branches in Display Functions

**Files:**
- Modify: `editors/common/olc_display.c` (add capture branches to ~14 functions)

Each `olc_display_*` function gets a capture-mode early-return branch. The pattern is:

```c
void olc_display_string(OLC_LAYOUT_CTX *ctx, ...) {
    /* Capture mode: record field descriptor */
    if (ctx && ctx->capture_mode && ctx->captured_fields) {
        json_t *field = json_object();
        // ... build descriptor ...
        json_array_append_new(ctx->captured_fields, field);
        return;
    }
    /* existing render code unchanged */
}
```

- [ ] **Step 1: Add static helper for building field descriptors**

At the top of `editors/common/olc_display.c` (after includes), add:

```c
#include <jansson.h>

/**
 * Build a JSON array of settable option names from a flag_type table.
 */
json_t *olc_flag_options_json(const struct flag_type *table)
{
    json_t *arr = json_array();
    if (!table) return arr;
    for (int i = 0; table[i].name != NULL; i++) {
        if (table[i].settable)
            json_array_append_new(arr, json_string(table[i].name));
    }
    return arr;
}

/**
 * Helper: Create base field descriptor with label, command, type, section.
 */
static json_t *capture_field_base(OLC_LAYOUT_CTX *ctx, const char *label,
                                   const char *command, const char *type)
{
    json_t *field = json_object();
    json_object_set_new(field, "label", json_string(label ? label : ""));
    if (command)
        json_object_set_new(field, "command", json_string(command));
    else
        json_object_set_new(field, "readonly", json_true());
    json_object_set_new(field, "type", json_string(type));
    if (ctx->current_section)
        json_object_set_new(field, "section", json_string(ctx->current_section));
    return field;
}
```

- [ ] **Step 2: Add capture branch to olc_display_string()**

At the start of `olc_display_string()`, before any rendering logic, add:

```c
    if (ctx && ctx->capture_mode && ctx->captured_fields) {
        json_t *field = capture_field_base(ctx, label, command, "string");
        json_object_set_new(field, "value",
            json_string(IS_NULLSTR(value) ? "" : value));
        json_array_append_new(ctx->captured_fields, field);
        return;
    }
```

- [ ] **Step 3: Add capture branch to olc_display_number()**

At the start of `olc_display_number()`, before the staged value lookup:

```c
    if (ctx && ctx->capture_mode && ctx->captured_fields) {
        json_t *field = capture_field_base(ctx, label, command, "int");
        json_object_set_new(field, "value", json_integer(value));
        json_array_append_new(ctx->captured_fields, field);
        return;
    }
```

- [ ] **Step 4: Add capture branch to olc_display_bool()**

```c
    if (ctx && ctx->capture_mode && ctx->captured_fields) {
        json_t *field = capture_field_base(ctx, label, command, "bool");
        json_object_set_new(field, "value", json_boolean(value));
        json_array_append_new(ctx->captured_fields, field);
        return;
    }
```

- [ ] **Step 5: Add capture branch to olc_display_text()**

```c
    if (ctx && ctx->capture_mode && ctx->captured_fields) {
        json_t *field = capture_field_base(ctx, label, command, "multiline");
        json_object_set_new(field, "value",
            json_string(IS_NULLSTR(text) ? "" : text));
        json_array_append_new(ctx->captured_fields, field);
        return;
    }
```

- [ ] **Step 6: Add capture branch to olc_display_type()**

```c
    if (ctx && ctx->capture_mode && ctx->captured_fields) {
        json_t *field = capture_field_base(ctx, label, command, "enum");
        const char *name = table ? flag_name(table, value) : "unknown";
        json_object_set_new(field, "value", json_string(name ? name : "unknown"));
        if (table)
            json_object_set_new(field, "options", olc_flag_options_json(table));
        json_array_append_new(ctx->captured_fields, field);
        return;
    }
```

- [ ] **Step 7: Add capture branch to olc_display_flags()**

```c
    if (ctx && ctx->capture_mode && ctx->captured_fields) {
        json_t *field = capture_field_base(ctx, label, command, "flags");
        json_object_set_new(field, "value",
            json_string(flag_string(table, value)));
        if (table)
            json_object_set_new(field, "options", olc_flag_options_json(table));
        json_array_append_new(ctx->captured_fields, field);
        return;
    }
```

- [ ] **Step 8: Add capture branches to remaining field functions**

**olc_display_dice():**
```c
    if (ctx && ctx->capture_mode && ctx->captured_fields) {
        json_t *field = capture_field_base(ctx, label, command, "dice");
        if (dice) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%dd%d+%d", dice->number, dice->size, dice->bonus);
            json_object_set_new(field, "value", json_string(buf));
        } else {
            json_object_set_new(field, "value", json_string(""));
        }
        json_array_append_new(ctx->captured_fields, field);
        return;
    }
```

**olc_display_vnum():**
```c
    if (ctx && ctx->capture_mode && ctx->captured_fields) {
        json_t *field = capture_field_base(ctx, label, command, "vnum");
        json_object_set_new(field, "value", json_integer(vnum));
        if (name && name[0] != '\0')
            json_object_set_new(field, "name", json_string(name));
        json_array_append_new(ctx->captured_fields, field);
        return;
    }
```

**olc_display_widevnum():**
```c
    if (ctx && ctx->capture_mode && ctx->captured_fields) {
        json_t *field = capture_field_base(ctx, label, command, "widevnum");
        json_object_set_new(field, "value",
            json_string(wnum_str ? wnum_str : ""));
        if (name && name[0] != '\0')
            json_object_set_new(field, "name", json_string(name));
        json_array_append_new(ctx->captured_fields, field);
        return;
    }
```

**olc_display_percent():**
```c
    if (ctx && ctx->capture_mode && ctx->captured_fields) {
        json_t *field = capture_field_base(ctx, label, command, "percent");
        json_object_set_new(field, "value", json_integer(value));
        json_object_set_new(field, "scale", json_integer(scale));
        json_array_append_new(ctx->captured_fields, field);
        return;
    }
```

- [ ] **Step 9: Add capture branches to structural/pair functions**

**olc_display_section():**
```c
    if (ctx && ctx->capture_mode) {
        ctx->current_section = (title && title[0] != '\0') ? title : NULL;
        if (ctx->captured_fields && title && title[0] != '\0') {
            json_t *field = json_object();
            json_object_set_new(field, "type", json_string("_section"));
            json_object_set_new(field, "title", json_string(title));
            json_array_append_new(ctx->captured_fields, field);
        }
        return;
    }
```

**olc_display_infof():** This is variadic. The existing function has `if (!ctx || !ctx->buffer || !fmt) return;` at the top — in capture mode `ctx->buffer` is NULL, so we must add the capture branch **before** that guard, with its own `va_start`/`vsnprintf`/`va_end`:
```c
    /* Capture mode: must come BEFORE the buffer null-check since capture ctx has no buffer */
    if (ctx && ctx->capture_mode && ctx->captured_fields && fmt) {
        char buf[MAX_STRING_LENGTH];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf) - 4, fmt, args);
        va_end(args);
        json_t *field = json_object();
        json_object_set_new(field, "type", json_string("_info"));
        json_object_set_new(field, "text", json_string(buf));
        if (ctx->current_section)
            json_object_set_new(field, "section", json_string(ctx->current_section));
        json_array_append_new(ctx->captured_fields, field);
        return;
    }
    /* existing: if (!ctx || !ctx->buffer || !fmt) return; */
```

**olc_display_pair():**
```c
    if (ctx && ctx->capture_mode && ctx->captured_fields) {
        /* Capture first half */
        json_t *f1 = capture_field_base(ctx, label1, cmd1, "string");
        json_object_set_new(f1, "value", json_string(IS_NULLSTR(value1) ? "" : value1));
        json_array_append_new(ctx->captured_fields, f1);
        /* Capture second half */
        json_t *f2 = capture_field_base(ctx, label2, cmd2, "string");
        json_object_set_new(f2, "value", json_string(IS_NULLSTR(value2) ? "" : value2));
        json_array_append_new(ctx->captured_fields, f2);
        return;
    }
```

**olc_display_header(), olc_display_footer(), olc_display_hr(), olc_display_blank():**
Add early return for each:
```c
    if (ctx && ctx->capture_mode) return;
```

**olc_display_table_begin(), olc_display_table_row(), olc_display_table_end(), olc_display_list(), olc_display_scripts(), olc_display_vars(), olc_display_entity_list():**
Add early return for each:
```c
    if (ctx && ctx->capture_mode) return;
```

Note: For `olc_display_entity_list()`, the check is `if (ch && /* some way to check ctx */)` — but this function takes `CHAR_DATA *ch` not `OLC_LAYOUT_CTX *ctx`, so it doesn't participate in capture mode. No change needed.

- [ ] **Step 10: Build and verify**

```bash
cd /sentience/src && ./build tests
cd /sentience && ./sent -test:olccs_ && ./sent -test:gmcped_ && ./sent -test:olcfw_
```

Expected: Clean build, all 51 existing tests pass.

- [ ] **Step 11: Commit**

```bash
cd /sentience/src && git add editors/common/olc_display.c
git commit -m "Add capture-mode branches to all olc_display_* functions

When OLC_LAYOUT_CTX.capture_mode is true, display functions append JSON
field descriptors to captured_fields instead of rendering text:
- string/number/bool/text: basic field descriptors
- type/flags: include options array from flag_type table
- dice/vnum/widevnum/percent: type-specific value formats
- section: _section markers with title
- infof: _info markers with text
- pair: two separate field entries
- header/footer/hr/blank/table/list/scripts/vars: skip (no capture)

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 4: Write Real Tests for Capture Branches

**Files:**
- Modify: `tests/unit/olc_schema_capture_tests.c` (replace stubs with real tests)

- [ ] **Step 1: Implement test_capture_string**

Replace the stub in `olc_schema_capture_tests.c`:

```c
static test_result_t test_capture_string(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_string(ctx, &olc_theme_default, "Name:", "name", "Test Room");

    TEST_ASSERT_INT_EQ(1, (int)json_array_size(ctx->captured_fields));
    json_t *field = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_STR_EQ("Name:", json_string_value(json_object_get(field, "label")));
    TEST_ASSERT_STR_EQ("name", json_string_value(json_object_get(field, "command")));
    TEST_ASSERT_STR_EQ("string", json_string_value(json_object_get(field, "type")));
    TEST_ASSERT_STR_EQ("Test Room", json_string_value(json_object_get(field, "value")));
    TEST_ASSERT_NULL(json_object_get(field, "readonly"));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}
```

- [ ] **Step 2: Implement test_capture_number**

```c
static test_result_t test_capture_number(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_number(ctx, &olc_theme_default, "Heal:", "heal", 150);

    TEST_ASSERT_INT_EQ(1, (int)json_array_size(ctx->captured_fields));
    json_t *field = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_STR_EQ("int", json_string_value(json_object_get(field, "type")));
    TEST_ASSERT_INT_EQ(150, (int)json_integer_value(json_object_get(field, "value")));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}
```

- [ ] **Step 3: Implement test_capture_bool**

```c
static test_result_t test_capture_bool(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_bool(ctx, &olc_theme_default, "Persist:", "persist", true);

    TEST_ASSERT_INT_EQ(1, (int)json_array_size(ctx->captured_fields));
    json_t *field = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_STR_EQ("bool", json_string_value(json_object_get(field, "type")));
    TEST_ASSERT_TRUE(json_is_true(json_object_get(field, "value")));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}
```

- [ ] **Step 4: Implement test_capture_text, test_capture_type, test_capture_flags**

**test_capture_text:**
```c
static test_result_t test_capture_text(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_text(ctx, &olc_theme_default, "Description:", "description",
        "A dark room.\n\rIt smells musty.\n\r");

    TEST_ASSERT_INT_EQ(1, (int)json_array_size(ctx->captured_fields));
    json_t *field = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_STR_EQ("multiline", json_string_value(json_object_get(field, "type")));
    TEST_ASSERT_NOT_NULL(json_object_get(field, "value"));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}
```

**test_capture_type:** Use a real flag table from the codebase (e.g., `sex_flags` or a small known table):
```c
static test_result_t test_capture_type(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    /* Use size_flags which is a known small enum table */
    extern const struct flag_type size_flags[];
    olc_display_type(ctx, &olc_theme_default, "Size:", "size", size_flags, 2);

    TEST_ASSERT_INT_EQ(1, (int)json_array_size(ctx->captured_fields));
    json_t *field = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_STR_EQ("enum", json_string_value(json_object_get(field, "type")));
    /* Value should be the flag name for value 2 */
    TEST_ASSERT_NOT_NULL(json_object_get(field, "value"));
    /* Options should be a non-empty array */
    json_t *opts = json_object_get(field, "options");
    TEST_ASSERT_NOT_NULL(opts);
    TEST_ASSERT_TRUE(json_is_array(opts));
    TEST_ASSERT_TRUE(json_array_size(opts) > 0);

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}
```

**test_capture_flags:** Use a known flag table:
```c
static test_result_t test_capture_flags(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    extern const struct flag_type exit_flags[];
    olc_display_flags(ctx, &olc_theme_default, "Exit flags:", "exit",
        exit_flags, 1 | 2);  /* Some flags set */

    TEST_ASSERT_INT_EQ(1, (int)json_array_size(ctx->captured_fields));
    json_t *field = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_STR_EQ("flags", json_string_value(json_object_get(field, "type")));
    TEST_ASSERT_NOT_NULL(json_object_get(field, "options"));
    TEST_ASSERT_TRUE(json_array_size(json_object_get(field, "options")) > 0);

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}
```

- [ ] **Step 5: Implement test_capture_readonly, test_capture_section, test_capture_pair, test_capture_skip_hr**

**test_capture_readonly:**
```c
static test_result_t test_capture_readonly(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_string(ctx, &olc_theme_default, "Area:", NULL, "Midgaard");

    TEST_ASSERT_INT_EQ(1, (int)json_array_size(ctx->captured_fields));
    json_t *field = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_NULL(json_object_get(field, "command"));
    TEST_ASSERT_TRUE(json_is_true(json_object_get(field, "readonly")));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}
```

**test_capture_section:**
```c
static test_result_t test_capture_section(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_section(ctx, &olc_theme_default, "Combat Stats");

    TEST_ASSERT_INT_EQ(1, (int)json_array_size(ctx->captured_fields));
    json_t *field = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_STR_EQ("_section", json_string_value(json_object_get(field, "type")));
    TEST_ASSERT_STR_EQ("Combat Stats", json_string_value(json_object_get(field, "title")));

    /* Verify section name is propagated to subsequent fields */
    olc_display_string(ctx, &olc_theme_default, "Level:", "level", "10");
    json_t *field2 = json_array_get(ctx->captured_fields, 1);
    TEST_ASSERT_STR_EQ("Combat Stats",
        json_string_value(json_object_get(field2, "section")));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}
```

**test_capture_pair:**
```c
static test_result_t test_capture_pair(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_pair(ctx, &olc_theme_default,
        "Race:", "race", "human",
        "Size:", "size", "medium");

    TEST_ASSERT_INT_EQ(2, (int)json_array_size(ctx->captured_fields));
    json_t *f1 = json_array_get(ctx->captured_fields, 0);
    json_t *f2 = json_array_get(ctx->captured_fields, 1);
    TEST_ASSERT_STR_EQ("race", json_string_value(json_object_get(f1, "command")));
    TEST_ASSERT_STR_EQ("size", json_string_value(json_object_get(f2, "command")));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}
```

**test_capture_skip_hr:**
```c
static test_result_t test_capture_skip_hr(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_hr(ctx, &olc_theme_default);
    olc_display_blank(ctx);

    TEST_ASSERT_INT_EQ(0, (int)json_array_size(ctx->captured_fields));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}
```

- [ ] **Step 6: Implement test_flag_options_json**

```c
static test_result_t test_flag_options_json(test_case_t *test)
{
    (void)test;
    extern const struct flag_type size_flags[];
    json_t *opts = olc_flag_options_json(size_flags);

    TEST_ASSERT_NOT_NULL(opts);
    TEST_ASSERT_TRUE(json_is_array(opts));
    TEST_ASSERT_TRUE(json_array_size(opts) > 0);

    /* Verify all entries are strings */
    for (size_t i = 0; i < json_array_size(opts); i++) {
        TEST_ASSERT_TRUE(json_is_string(json_array_get(opts, i)));
    }

    /* NULL table returns empty array */
    json_t *empty = olc_flag_options_json(NULL);
    TEST_ASSERT_INT_EQ(0, (int)json_array_size(empty));
    json_decref(empty);

    json_decref(opts);
    return TEST_SUCCESS;
}
```

- [ ] **Step 7: Build and run tests**

```bash
cd /sentience/src && ./build tests
cd /sentience && ./sent -test:olcsc_
```

Expected: 14 tests, 12 pass (capture + flag_options), 2 skip (annotation_merge and build_open — not yet implemented).

- [ ] **Step 8: Commit**

```bash
cd /sentience/src && git add tests/unit/olc_schema_capture_tests.c
git commit -m "Implement capture mode tests for all display functions

12 passing tests covering: string, number, bool, text, type, flags,
readonly, section markers, pair, hr/blank skip, and flag options.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 5: Schema Capture Orchestrator

**Files:**
- Modify: `editors/common/olc_display.c` (add `olc_schema_capture()`)

- [ ] **Step 1: Implement olc_schema_capture()**

Add at the end of `editors/common/olc_display.c` (before any `#endif`):

```c
/**
 * Merge constraint annotations into captured field descriptors.
 */
static void merge_annotations(json_t *fields,
                               const olc_field_annotation_t *annotations)
{
    if (!annotations || !fields) return;

    for (size_t i = 0; i < json_array_size(fields); i++) {
        json_t *field = json_array_get(fields, i);
        const char *cmd = json_string_value(json_object_get(field, "command"));
        if (!cmd) continue;

        for (const olc_field_annotation_t *a = annotations; a->command; a++) {
            if (strcmp(a->command, cmd) == 0) {
                if (a->min != INT_MIN)
                    json_object_set_new(field, "min", json_integer(a->min));
                if (a->max != INT_MAX)
                    json_object_set_new(field, "max", json_integer(a->max));
                if (a->max_length > 0)
                    json_object_set_new(field, "max_length", json_integer(a->max_length));
                break;
            }
        }
    }
}

json_t *olc_schema_capture(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
                           void *entity, struct olc_changeset *cs)
{
    if (!def) return NULL;

    json_t *tabs = json_array();
    if (!tabs) return NULL;

    int tab_count = def->tabs.count;

    /* No tabs: all current editors use tabs, so this is a defensive fallback.
     * The show_fn creates its own OLC_LAYOUT_CTX internally, so capture_mode
     * is not propagated — fields will be empty. This is acceptable since
     * all production editors have tabs. A future refactor could pass ctx. */
    if (tab_count == 0) {
        OLC_LAYOUT_CTX *ctx = alloc_mem(sizeof(OLC_LAYOUT_CTX));
        memset(ctx, 0, sizeof(OLC_LAYOUT_CTX));
        ctx->capture_mode = true;
        ctx->captured_fields = json_array();
        ctx->changeset = cs;
        ctx->screen_width = 80;
        ctx->label_width = 16;
        ctx->value_width = 58;

        if (def->show_fn && ch)
            def->show_fn(ch, "");

        if (def->annotations)
            merge_annotations(ctx->captured_fields, def->annotations);

        json_t *tab = json_object();
        json_object_set_new(tab, "name", json_string(def->name));
        json_object_set_new(tab, "short_name", json_string(def->name));
        json_object_set_new(tab, "fields", ctx->captured_fields);
        ctx->captured_fields = NULL;  /* Ownership transferred to tab */
        json_array_append_new(tabs, tab);

        free_mem(ctx, sizeof(OLC_LAYOUT_CTX));
        return tabs;
    }

    /* Multi-tab: run each tab's show_fn */
    for (int t = 0; t < tab_count; t++) {
        const OLC_EDITOR_TAB *tab_def = &def->tabs.tabs[t];
        if (!tab_def->show_fn) continue;

        OLC_LAYOUT_CTX *ctx = alloc_mem(sizeof(OLC_LAYOUT_CTX));
        memset(ctx, 0, sizeof(OLC_LAYOUT_CTX));
        ctx->capture_mode = true;
        ctx->captured_fields = json_array();
        ctx->changeset = cs;
        ctx->current_tab = t;
        ctx->ch = ch;
        ctx->screen_width = 80;
        ctx->label_width = 16;
        ctx->value_width = 58;

        tab_def->show_fn(ch, ctx, entity);

        if (def->annotations)
            merge_annotations(ctx->captured_fields, def->annotations);

        json_t *tab = json_object();
        json_object_set_new(tab, "name",
            json_string(tab_def->name ? tab_def->name : ""));
        json_object_set_new(tab, "short_name",
            json_string(tab_def->short_name ? tab_def->short_name : ""));
        json_object_set_new(tab, "fields", ctx->captured_fields);
        ctx->captured_fields = NULL;
        json_array_append_new(tabs, tab);

        free_mem(ctx, sizeof(OLC_LAYOUT_CTX));
    }

    return tabs;
}
```

- [ ] **Step 2: Implement test_annotation_merge**

In `tests/unit/olc_schema_capture_tests.c`, replace the stub. Since `merge_annotations()` is static, we test it indirectly via `olc_schema_capture()` using a mock editor def with a minimal tab show function:

```c
/* Mock show function that captures two fields for annotation merge testing */
static void mock_tab_show_for_annotations(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *entity)
{
    (void)ch; (void)entity;
    olc_display_number(ctx, &olc_theme_default, "Heal:", "heal", 100);
    olc_display_number(ctx, &olc_theme_default, "Mana:", "mana", 200);
    olc_display_string(ctx, &olc_theme_default, "Name:", "name", "Test");
}

static test_result_t test_annotation_merge(test_case_t *test)
{
    (void)test;

    static const olc_field_annotation_t test_annotations[] = {
        { "heal", .min = 0, .max = 10000 },
        { "name", .max_length = 80 },
        { NULL }
    };

    /* Build a minimal editor def with one tab */
    OLC_EDITOR_TAB mock_tab = {
        .name = "Test",
        .short_name = "T",
        .show_fn = mock_tab_show_for_annotations,
    };
    OLC_EDITOR_DEF mock_def = {
        .name = "MockEdit",
        .tabs = { .count = 1, .tabs = &mock_tab },
        .annotations = test_annotations,
    };

    json_t *tabs = olc_schema_capture(NULL, &mock_def, NULL, NULL);
    TEST_ASSERT_NOT_NULL(tabs);
    TEST_ASSERT_INT_EQ(1, (int)json_array_size(tabs));

    json_t *tab = json_array_get(tabs, 0);
    json_t *fields = json_object_get(tab, "fields");
    TEST_ASSERT_INT_EQ(3, (int)json_array_size(fields));

    /* "heal" should have min=0, max=10000 from annotation */
    json_t *heal = json_array_get(fields, 0);
    TEST_ASSERT_INT_EQ(0, (int)json_integer_value(json_object_get(heal, "min")));
    TEST_ASSERT_INT_EQ(10000, (int)json_integer_value(json_object_get(heal, "max")));

    /* "mana" has no annotation — should NOT have min/max keys */
    json_t *mana = json_array_get(fields, 1);
    TEST_ASSERT_NULL(json_object_get(mana, "min"));
    TEST_ASSERT_NULL(json_object_get(mana, "max"));

    /* "name" should have max_length=80 from annotation */
    json_t *name = json_array_get(fields, 2);
    TEST_ASSERT_INT_EQ(80, (int)json_integer_value(json_object_get(name, "max_length")));
    TEST_ASSERT_NULL(json_object_get(name, "min")); /* no min for string */

    json_decref(tabs);
    return TEST_SUCCESS;
}
```

- [ ] **Step 3: Build and run**

```bash
cd /sentience/src && ./build tests
cd /sentience && ./sent -test:olcsc_
```

Expected: 13 pass, 1 skip (build_open — not yet implemented).

- [ ] **Step 4: Commit**

```bash
cd /sentience/src && git add editors/common/olc_display.c tests/unit/olc_schema_capture_tests.c
git commit -m "Add olc_schema_capture() orchestrator with annotation merge

Iterates editor tabs in capture mode, collects field descriptors,
merges constraint annotations by command name. Handles both tabbed
and non-tabbed editors.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 6: Editor.Open GMCP Message Builder

**Files:**
- Modify: `gmcp_editor.c` (add build_open, send_open, editor_type_name)
- Modify: `gmcp_editor.h` (declare new functions)

**Important:** `gmcp_editor.c` must include `editors/common/olc_display.h` to access `olc_schema_capture()`. Add this include at the top of the file alongside existing editor includes.

- [ ] **Step 1: Add include and editor_type_name helper to gmcp_editor.c**

Add at top of `gmcp_editor.c`:
```c
#include "editors/common/olc_display.h"
```

Then add the helper function (non-static so it can be tested):

```c
/**
 * Map ED_* constant to a lowercase string for the GMCP editor_type field.
 */
const char *gmcp_editor_type_name(int editor_type, const char *fallback_name)
{
    switch (editor_type) {
        case ED_ROOM:   return "room";
        case ED_MOBILE: return "mobile";
        case ED_OBJECT: return "object";
        case ED_AREA:   return "area";
        default: {
            /* Use lowercase of editor name as fallback */
            static char buf[32];
            if (fallback_name) {
                int i;
                for (i = 0; fallback_name[i] && i < 31; i++)
                    buf[i] = tolower((unsigned char)fallback_name[i]);
                buf[i] = '\0';
                return buf;
            }
            return "unknown";
        }
    }
}
```

- [ ] **Step 2: Add gmcp_editor_build_open()**

```c
json_t *gmcp_editor_build_open(const char *entity_id, const char *editor_name,
    const char *editor_type, json_t *tabs, olc_changeset_t *cs,
    bool draft_restored)
{
    json_t *msg = json_object();
    if (!msg) return NULL;

    json_object_set_new(msg, "entity_id",
        json_string(entity_id ? entity_id : ""));
    json_object_set_new(msg, "editor",
        json_string(editor_name ? editor_name : ""));
    json_object_set_new(msg, "editor_type",
        json_string(editor_type ? editor_type : "unknown"));

    if (tabs)
        json_object_set(msg, "tabs", tabs);  /* borrowed ref */
    else
        json_object_set_new(msg, "tabs", json_array());

    /* Embed state */
    json_t *state = json_object();
    int count = cs ? olc_changeset_count(cs) : 0;
    json_object_set_new(state, "pending_count", json_integer(count));
    json_object_set_new(state, "draft_restored", json_boolean(draft_restored));

    json_t *changes = json_array();
    if (cs && cs->changes) {
        /* Reuse the same change serialization as gmcp_editor_build_state */
        ITERATOR it;
        iterator_start(&it, cs->changes);
        olc_pending_change_t *change;
        while ((change = (olc_pending_change_t *)iterator_nextdata(&it)) != NULL) {
            json_t *entry = json_object();
            json_object_set_new(entry, "field",
                json_string(change->field_path ? change->field_path : ""));
            /* Use existing field type name mapping */
            const char *type_name;
            switch (change->field_type) {
                case OLC_FIELD_STRING:      type_name = "string"; break;
                case OLC_FIELD_INT:         type_name = "int"; break;
                case OLC_FIELD_INT16:       type_name = "int16"; break;
                case OLC_FIELD_FLAGS:       type_name = "flags"; break;
                case OLC_FIELD_BOOL:        type_name = "bool"; break;
                case OLC_FIELD_WIDEVNUM:    type_name = "widevnum"; break;
                case OLC_FIELD_EXIT:        type_name = "exit"; break;
                case OLC_FIELD_EMBEDDED:    type_name = "embedded"; break;
                case OLC_FIELD_LIST_ADD:    type_name = "list_add"; break;
                case OLC_FIELD_LIST_REMOVE: type_name = "list_remove"; break;
                case OLC_FIELD_LIST_UPDATE: type_name = "list_update"; break;
                case OLC_FIELD_MULTILINE:   type_name = "multiline"; break;
                case OLC_FIELD_TYPE_DATA:   type_name = "type_data"; break;
                default:                    type_name = "unknown"; break;
            }
            json_object_set_new(entry, "type", json_string(type_name));
            if (change->new_value)
                json_object_set(entry, "value", change->new_value);
            else
                json_object_set_new(entry, "value", json_null());
            json_array_append_new(changes, entry);
        }
        iterator_stop(&it);
    }
    json_object_set_new(state, "changes", changes);
    json_object_set_new(msg, "state", state);
    json_object_set_new(msg, "_v", json_integer(1));

    return msg;
}
```

Note: The change serialization duplicates logic from `gmcp_editor_build_state()`. Consider extracting a shared helper if the implementing agent sees the opportunity. This is not required but would be a clean DRY improvement.

- [ ] **Step 3: Add gmcp_editor_send_open()**

```c
void gmcp_editor_send_open(descriptor_t *d, const OLC_EDITOR_DEF *def,
    void *entity, const char *entity_id, olc_changeset_t *cs,
    bool draft_restored)
{
    if (!can_send_gmcp(d) || !def) return;

    json_t *tabs = olc_schema_capture(d->character, def, entity, cs);
    json_t *msg = gmcp_editor_build_open(entity_id, def->name,
        gmcp_editor_type_name(def->editor_type, def->name),
        tabs, cs, draft_restored);

    if (tabs) json_decref(tabs);
    if (msg)
        sentience_send_package(d, "Sentience.Editor.Open", msg);
}
```

- [ ] **Step 4: Add declarations to gmcp_editor.h**

```c
const char *gmcp_editor_type_name(int editor_type, const char *fallback_name);

json_t *gmcp_editor_build_open(const char *entity_id, const char *editor_name,
    const char *editor_type, json_t *tabs, olc_changeset_t *cs,
    bool draft_restored);

void gmcp_editor_send_open(descriptor_t *d, const OLC_EDITOR_DEF *def,
    void *entity, const char *entity_id, olc_changeset_t *cs,
    bool draft_restored);
```

Ensure `gmcp_editor.h` has a forward declaration for `OLC_EDITOR_DEF`:
```c
typedef struct olc_editor_def OLC_EDITOR_DEF;
```

- [ ] **Step 5: Implement test_build_open and test_build_open_editor_type**

In `tests/unit/olc_schema_capture_tests.c`, replace the stubs:

```c
static test_result_t test_build_open(test_case_t *test)
{
    (void)test;

    /* Build a mock tabs array */
    json_t *tabs = json_array();
    json_t *tab = json_object();
    json_object_set_new(tab, "name", json_string("General"));
    json_object_set_new(tab, "short_name", json_string("Gen"));
    json_t *fields = json_array();
    json_t *f = json_object();
    json_object_set_new(f, "label", json_string("Name:"));
    json_object_set_new(f, "command", json_string("name"));
    json_object_set_new(f, "type", json_string("string"));
    json_object_set_new(f, "value", json_string("Test Room"));
    json_array_append_new(fields, f);
    json_object_set_new(tab, "fields", fields);
    json_array_append_new(tabs, tab);

    json_t *msg = gmcp_editor_build_open("room:1#100", "REdit", "room",
        tabs, NULL, false);

    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_STR_EQ("room:1#100",
        json_string_value(json_object_get(msg, "entity_id")));
    TEST_ASSERT_STR_EQ("REdit",
        json_string_value(json_object_get(msg, "editor")));
    TEST_ASSERT_STR_EQ("room",
        json_string_value(json_object_get(msg, "editor_type")));
    TEST_ASSERT_INT_EQ(1, (int)json_array_size(json_object_get(msg, "tabs")));

    json_t *state = json_object_get(msg, "state");
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_INT_EQ(0,
        (int)json_integer_value(json_object_get(state, "pending_count")));
    TEST_ASSERT_FALSE(json_is_true(json_object_get(state, "draft_restored")));

    TEST_ASSERT_INT_EQ(1,
        (int)json_integer_value(json_object_get(msg, "_v")));

    json_decref(msg);
    json_decref(tabs);
    return TEST_SUCCESS;
}

static test_result_t test_build_open_editor_type(test_case_t *test)
{
    (void)test;

    /* Test known ED_* mappings via gmcp_editor_type_name() */
    TEST_ASSERT_STR_EQ("room", gmcp_editor_type_name(ED_ROOM, "REdit"));
    TEST_ASSERT_STR_EQ("mobile", gmcp_editor_type_name(ED_MOBILE, "MEdit"));
    TEST_ASSERT_STR_EQ("object", gmcp_editor_type_name(ED_OBJECT, "OEdit"));
    TEST_ASSERT_STR_EQ("area", gmcp_editor_type_name(ED_AREA, "AEdit"));

    /* Unknown ED_* falls back to lowercase name */
    TEST_ASSERT_STR_EQ("myeditor", gmcp_editor_type_name(9999, "MyEditor"));

    /* NULL fallback returns "unknown" */
    TEST_ASSERT_STR_EQ("unknown", gmcp_editor_type_name(9999, NULL));

    return TEST_SUCCESS;
}
```

- [ ] **Step 6: Build and run all tests**

```bash
cd /sentience/src && ./build tests
cd /sentience && ./sent -test:olcsc_ && ./sent -test:gmcped_
```

Expected: 14 olcsc_ pass, 15 gmcped_ pass.

- [ ] **Step 7: Commit**

```bash
cd /sentience/src && git add gmcp_editor.c gmcp_editor.h tests/unit/olc_schema_capture_tests.c
git commit -m "Add Editor.Open GMCP message builder and send function

gmcp_editor_build_open() assembles the full message with entity_id,
editor name/type, tab-grouped field schema, and embedded changeset state.
gmcp_editor_send_open() orchestrates schema capture and sends the message.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 7: Integration Wiring — Replace State with Open

**Files:**
- Modify: `editors/common/olc_editor.c`

- [ ] **Step 1: Replace gmcp_editor_send_state() with gmcp_editor_send_open() in olc_editor_enter()**

In `editors/common/olc_editor.c`, find the GMCP notification block (around line 530-540):

```c
    /* Notify web client of editor session */
    if (def->change_mode == OLC_CHANGE_STAGED && ch->desc->olc_state) {
        WNUM_LOAD wnum = olc_get_entity_wnum(def, pEdit);
        olc_changeset_t *cs = olc_edit_state_find_changeset(
            ch->desc->olc_state, def->editor_type, wnum);
        if (cs) {
            const char *eid = gmcp_editor_entity_id(cs->editor_type, cs->entity_wnum);
            bool draft = (olc_changeset_count(cs) > 0);
            gmcp_editor_send_state(ch->desc, eid, cs, draft);
        }
    }
```

Replace with:

```c
    /* Notify web client of editor session with full field schema */
    if (def->change_mode == OLC_CHANGE_STAGED && ch->desc->olc_state) {
        WNUM_LOAD wnum = olc_get_entity_wnum(def, pEdit);
        olc_changeset_t *cs = olc_edit_state_find_changeset(
            ch->desc->olc_state, def->editor_type, wnum);
        if (cs) {
            const char *eid = gmcp_editor_entity_id(cs->editor_type, cs->entity_wnum);
            bool draft = (olc_changeset_count(cs) > 0);
            gmcp_editor_send_open(ch->desc, def, pEdit, eid, cs, draft);
        }
    }
```

No new includes needed in `olc_editor.c` — it already has `gmcp_editor.h` which declares `gmcp_editor_send_open()`. The `olc_schema_capture()` call happens inside `gmcp_editor.c` (which has the `olc_display.h` include added in Task 6).

- [ ] **Step 2: Build and run full test suite**

```bash
cd /sentience/src && ./build tests
cd /sentience && ./sent -test:olcsc_ && ./sent -test:gmcped_ && ./sent -test:olccs_ && ./sent -test:olcfw_
```

Expected: All tests pass (14 + 15 + 30 + 6 = 65).

- [ ] **Step 3: Commit**

```bash
cd /sentience/src && git add editors/common/olc_editor.c
git commit -m "Wire Editor.Open into olc_editor_enter(), replacing Editor.State

Web clients now receive Sentience.Editor.Open with full field schema
when entering an editor, instead of the bare Editor.State message.
Editor.State continues to be sent for incremental changeset updates.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 8: Editor Annotation Arrays

**Files:**
- Modify: `editors/rooms/redit.c`
- Modify: `editors/mobiles/medit.c`
- Modify: `editors/objects/oedit.c`
- Modify: `editors/areas/aedit.c`

- [ ] **Step 1: Add redit_annotations in redit.c**

Add before the `redit_def` definition:

```c
static const olc_field_annotation_t redit_annotations[] = {
    { "heal rate",  .min = INT_MIN, .max = INT_MAX },
    { "mana rate",  .min = INT_MIN, .max = INT_MAX },
    { "move rate",  .min = INT_MIN, .max = INT_MAX },
    { NULL }
};
```

Note: The actual redit handlers use INT_MIN/INT_MAX (unconstrained), so these annotations don't add constraints. Include them as documentation, or leave `redit_annotations` as an empty sentinel array (`{ { NULL } }`) if no useful constraints exist. The implementing agent should check each `olc_cmd_number` call in redit to find actual constraints.

In `redit_def`, add `.annotations = redit_annotations` (or `NULL` if empty).

- [ ] **Step 2: Add medit_annotations in medit.c**

```c
static const olc_field_annotation_t medit_annotations[] = {
    { "Alignment", .min = -1000, .max = 1000 },
    { NULL }
};
```

Note: medit_level uses manual validation (1 to MAX_MOB_SKILL_LEVEL=1000) rather than `olc_cmd_number`, so it doesn't get auto-staged and can't be annotated this way. The implementing agent should verify which fields use `olc_cmd_number`/`olc_cmd_number_i16` and add annotations only for those. Wire `.annotations = medit_annotations` into `medit_def`.

- [ ] **Step 3: Add oedit_annotations in oedit.c**

```c
static const olc_field_annotation_t oedit_annotations[] = {
    { "Weight",    .min = 0, .max = INT_MAX },
    { "Condition", .min = 0, .max = 100 },
    { NULL }
};
```

Wire `.annotations = oedit_annotations` into `oedit_def`.

- [ ] **Step 4: Add aedit_annotations in aedit.c**

Area editor constraints are dynamic (security depends on player's own security level). Use an empty annotation array:

```c
static const olc_field_annotation_t aedit_annotations[] = {
    { NULL }
};
```

Wire `.annotations = aedit_annotations` into `aedit_def`.

- [ ] **Step 5: Build and run**

```bash
cd /sentience/src && ./build tests
cd /sentience && ./sent -test:olcsc_ && ./sent -test:olccs_ && ./sent -test:olcfw_
```

Expected: All tests pass.

- [ ] **Step 6: Commit**

```bash
cd /sentience/src && git add editors/rooms/redit.c editors/mobiles/medit.c \
    editors/objects/oedit.c editors/areas/aedit.c
git commit -m "Add field constraint annotations to all four editors

Annotation arrays for redit, medit, oedit, aedit with min/max
constraints derived from olc_cmd_number handler parameters.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 9: Documentation — GMCP Web Client Reference

**Files:**
- Modify: `docs/GMCP_WEB_CLIENT_REFERENCE.md`

- [ ] **Step 1: Add Editor.Open documentation**

Find the note about Editor.Open near line 1682-1685 that says:
```
> **Note:** There is no `Editor.Open` message currently.
```

Replace it with full documentation of the `Sentience.Editor.Open` message:

Include:
- **Direction:** Server → Client
- **When sent:** When a builder enters an OLC editor (replaces initial `Editor.State`)
- **Full wire format example** (from the spec's Editor.Open Wire Format section)
- **Field table:** All top-level keys (entity_id, editor, editor_type, tabs, state, _v)
- **Tab object fields:** name, short_name, fields[]
- **Field descriptor keys:** label, command, type, value, options, readonly, section, min, max, max_length
- **Field type reference table:** All type strings with descriptions
- **Structural marker types:** `_section` (title), `_info` (text)
- **Client implementation guidance:** How to render each type as form controls

Also update:
- The **Editor lifecycle** section to show Editor.Open as the initial message
- The **Appendix summary table** to include Editor.Open
- The **TOC** if one exists

- [ ] **Step 2: Review documentation**

Read through the added section to verify accuracy against the implementation.

- [ ] **Step 3: Commit**

```bash
cd /sentience/src && git add docs/GMCP_WEB_CLIENT_REFERENCE.md
git commit -m "Document Sentience.Editor.Open in GMCP web client reference

Full protocol documentation: wire format, field types, tab structure,
constraint annotations, structural markers, and client rendering guide.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 10: Final Build, Test, and Regression Check

**Files:** None (verification only)

- [ ] **Step 1: Clean rebuild**

```bash
cd /sentience/src && ./build clean tests
```

Expected: Clean build with zero warnings from modified files.

- [ ] **Step 2: Run all new tests**

```bash
cd /sentience && ./sent -test:olcsc_
```

Expected: 14 tests, 14 pass, 0 fail, 0 skip.

- [ ] **Step 3: Run all related tests (regression)**

```bash
cd /sentience && ./sent -test:olccs_ && ./sent -test:gmcped_ && ./sent -test:olcfw_
```

Expected: 30 + 15 + 6 = 51 pass, 0 fail.

- [ ] **Step 4: Run full test suite**

```bash
cd /sentience && ./sent -test
```

Expected: ~473 pass (459 existing + 14 new), 0 fail (excluding known pre-existing crash in pure_function_tests).

- [ ] **Step 5: Verify git status is clean**

```bash
cd /sentience/src && git --no-pager status && git --no-pager log --oneline -10
```

Expected: Clean working tree, ~7-9 new commits on `tieryo/gmcp_editors`.
