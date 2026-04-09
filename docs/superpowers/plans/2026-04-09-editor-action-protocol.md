# Editor Action Protocol Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a unified GMCP action protocol for add/remove operations across all 4 OLC editors, with shared staging between GMCP and telnet paths.

**Architecture:** A central action handler registry maps action names (e.g., "addoprog") to form-builder and stage functions. GMCP sends `Editor.Action` → server replies with `Editor.Action.Form` (self-describing form with live data) → client submits `Editor.Action.Submit` → server stages via shared `stage_fn` and replies with `Editor.Action.Result`. Telnet add commands are refactored as thin wrappers that parse text, build JSON, and call the same `stage_fn`. Each editor module defines its own action handler table. An action session tracker on `olc_edit_state_t` prevents stale/duplicate submissions.

**Tech Stack:** C23, Jansson (JSON), existing OLC changeset infrastructure (`olc_stage_list_add/remove`), GMCP protocol layer.

**Spec:** `docs/superpowers/specs/2026-04-08-editor-action-protocol-design.md`

---

## File Map

### New Files

| File | Responsibility |
|------|---------------|
| `editors/common/olc_actions.h` | Action handler types (`olc_action_handler_t`, `olc_action_session_t`), registry API (`olc_find_action`, `olc_register_actions`), session management API (`olc_action_session_create/find/clear`) |
| `editors/common/olc_actions.c` | Registry implementation (static table aggregation), session lifecycle, GMCP dispatch helpers (`olc_action_build_form`, `olc_action_build_result`) |
| `editors/objects/oedit_actions.c` | Oedit action form/stage functions + handler table for ~10 actions (addaffect, addoprog, addtype, removetype, addspell, addskill, addimmune, addquest, addcatalyst, addwaypoint) |
| `editors/mobiles/medit_actions.c` | Medit action form/stage functions + handler table for 3 actions (addmprog, addquest, addreputation) |
| `editors/rooms/redit_actions.c` | Redit action form/stage functions + handler table for 5 actions (mreset, oreset, delreset, addrprog, addcdesc) |
| `editors/areas/aedit_actions.c` | Aedit action form/stage functions + handler table for 2 actions (addaprog, addtrade) |
| `tests/unit/olc_action_tests.c` | Test module for action form/stage/validation tests |
| `tests/data/unit/olc_action_tests.json` | JSON test case definitions |

### Modified Files

| File | Changes |
|------|---------|
| `gmcp_editor.c` | Add `handle_editor_action()` and `handle_editor_action_submit()` GMCP handlers |
| `gmcp_editor.h` | Declare new GMCP message builder/sender functions |
| `protocol.h` | Add `GMCP_SENTIENCE_EDITOR_ACTION` and `GMCP_SENTIENCE_EDITOR_ACTION_SUBMIT` enum values |
| `protocol.c` | Add GMCP message name mappings for Action and Action.Submit |
| `editors/common/olc_changeset.h` | Add `olc_action_session_t` to `olc_edit_state_t` |
| `editors/objects/oedit.c` | Refactor `oedit_addoprog`/`oedit_addskill` to call stage_fn; register action table |
| `editors/mobiles/medit.c` | Refactor `medit_addmprog`/`medit_addquest`/`medit_addreputation` to call stage_fn; add apply handlers to field table; register action table |
| `editors/rooms/redit.c` | Refactor `redit_mreset`/`redit_oreset`/`redit_addrprog`/`redit_addcdesc` to call stage_fn; add `redit_delreset`; add apply handlers to field table; register action table |
| `editors/areas/aedit.c` | Refactor `aedit_addaprog`/`aedit_addtrade` to call stage_fn; add apply handlers to field table; register action table |
| `CMakeLists.txt` | Add all new .c files to source lists (main + BUILD_TESTS) |
| `Makefile` | Add all new .c files to source lists (main + BUILD_TESTS) |
| `docs/GMCP_WEB_CLIENT_REFERENCE.md` | Document Editor.Action messages, conditional fields, action lifecycle |

---

## Task Decomposition

### Task 1: Action Infrastructure — Types, Registry, Session Management

**Files:**
- Create: `editors/common/olc_actions.h`
- Create: `editors/common/olc_actions.c`
- Modify: `editors/common/olc_changeset.h` (~line 93, add action session to `olc_edit_state_t`)
- Modify: `CMakeLists.txt` (add `olc_actions.c`)
- Modify: `Makefile` (add `olc_actions.c`)

**Context:** The `olc_edit_state_t` struct (in `olc_changeset.h:93-97`) already tracks `active_changesets`, `string_edit_sessions`, and `next_string_session_id`. We add action session tracking here using the same pattern.

The action handler struct is a static const table pattern — same as `oedit_field_handlers[]` in `oedit.c:766` and `oedit_table[]` in `oedit.c:79`.

- [ ] **Step 1: Create `olc_actions.h` with type definitions**

```c
/* editors/common/olc_actions.h */
#ifndef __OLC_ACTIONS_H__
#define __OLC_ACTIONS_H__

#include "olc_changeset.h"
#include <jansson.h>

/* Forward declarations */
typedef struct char_data CHAR_DATA;

/**
 * Action form builder — returns JSON array of field descriptors.
 * @param entity  The entity being edited (OBJ_INDEX_DATA*, MOB_INDEX_DATA*, etc.)
 * @param ch      The editing character (for context like area access)
 * @return json_t* array of field objects (caller owns reference), or NULL on error
 */
typedef json_t *(*olc_action_form_fn)(void *entity, CHAR_DATA *ch);

/**
 * Action stage function — validates values and stages the change.
 * @param entity  The entity being edited
 * @param cs      Active changeset to stage into
 * @param values  JSON object of field values from the form
 * @param errbuf  Buffer for error message on failure
 * @param errlen  Size of errbuf
 * @return true on success, false on validation failure (message in errbuf)
 */
typedef bool (*olc_action_stage_fn)(void *entity, olc_changeset_t *cs,
                                     json_t *values,
                                     char *errbuf, size_t errlen);

/**
 * Action handler entry — one per action (e.g., "addoprog", "mreset").
 * Editor modules define static arrays of these, NULL-terminated.
 */
typedef struct olc_action_handler {
    const char          *action_name;   /* "addoprog", "mreset", etc. */
    int                  editor_type;   /* ED_OBJECT, ED_MOBILE, ED_ROOM, ED_AREA */
    const char          *display_hint;  /* "inline" or "dialog" */
    const char          *title;         /* "Add Object Program" */
    const char          *list_name;     /* "oprogs", "resets", etc. */
    olc_action_form_fn   form_fn;
    olc_action_stage_fn  stage_fn;
} olc_action_handler_t;

/**
 * Active action session — tracks one pending action form per editor session.
 * Stored on olc_edit_state_t. Only one active at a time per descriptor.
 */
typedef struct olc_action_session {
    char    session_id[32];     /* "act_1", "act_2", etc. */
    char    action_name[64];    /* "addoprog" */
    int     editor_type;        /* ED_OBJECT */
    time_t  created_at;
} olc_action_session_t;

/* Registry — editors call this during init to register their action tables */
void olc_register_actions(const olc_action_handler_t *table);

/* Lookup — find handler by editor type + action name */
const olc_action_handler_t *olc_find_action(int editor_type, const char *action_name);

/* Session management */
olc_action_session_t *olc_action_session_create(olc_edit_state_t *state,
    int editor_type, const char *action_name);
olc_action_session_t *olc_action_session_find(olc_edit_state_t *state,
    const char *session_id);
void olc_action_session_clear(olc_edit_state_t *state);

/* GMCP message builders */
json_t *olc_action_build_form(const char *entity_id,
    const olc_action_handler_t *handler,
    const olc_action_session_t *session,
    json_t *fields);

json_t *olc_action_build_result(const char *entity_id,
    const char *session_id,
    bool success, const char *message);

#endif /* __OLC_ACTIONS_H__ */
```

- [ ] **Step 2: Create `olc_actions.c` with implementation**

```c
/* editors/common/olc_actions.c */
#include "olc_actions.h"
#include "../../merc.h"

#include <string.h>
#include <time.h>

/* Registry — up to 8 editor modules can register tables */
#define MAX_ACTION_TABLES 8
static const olc_action_handler_t *action_tables[MAX_ACTION_TABLES];
static int action_table_count = 0;

void olc_register_actions(const olc_action_handler_t *table)
{
    if (action_table_count < MAX_ACTION_TABLES)
        action_tables[action_table_count++] = table;
}

const olc_action_handler_t *olc_find_action(int editor_type, const char *action_name)
{
    for (int t = 0; t < action_table_count; t++) {
        for (const olc_action_handler_t *h = action_tables[t]; h->action_name; h++) {
            if (h->editor_type == editor_type
                && !str_cmp(h->action_name, action_name))
                return h;
        }
    }
    return NULL;
}

olc_action_session_t *olc_action_session_create(olc_edit_state_t *state,
    int editor_type, const char *action_name)
{
    /* Clear any existing session (max 1 at a time) */
    olc_action_session_clear(state);

    olc_action_session_t *session = &state->action_session;
    snprintf(session->session_id, sizeof(session->session_id),
             "act_%d", state->next_action_session_id++);
    strlcpy(session->action_name, action_name, sizeof(session->action_name));
    session->editor_type = editor_type;
    session->created_at = time(NULL);
    state->has_action_session = true;
    return session;
}

olc_action_session_t *olc_action_session_find(olc_edit_state_t *state,
    const char *session_id)
{
    if (!state->has_action_session)
        return NULL;
    if (str_cmp(state->action_session.session_id, session_id))
        return NULL;
    return &state->action_session;
}

void olc_action_session_clear(olc_edit_state_t *state)
{
    memset(&state->action_session, 0, sizeof(state->action_session));
    state->has_action_session = false;
}

json_t *olc_action_build_form(const char *entity_id,
    const olc_action_handler_t *handler,
    const olc_action_session_t *session,
    json_t *fields)
{
    return json_pack("{s:s, s:s, s:s, s:s, s:s, s:O, s:i}",
        "entity_id",  entity_id,
        "action",     handler->action_name,
        "session_id", session->session_id,
        "display",    handler->display_hint,
        "title",      handler->title,
        "fields",     fields,
        "_v",         1);
}

json_t *olc_action_build_result(const char *entity_id,
    const char *session_id,
    bool success, const char *message)
{
    json_t *result = json_pack("{s:s, s:s, s:s, s:i}",
        "entity_id",  entity_id,
        "session_id", session_id,
        "status",     success ? "success" : "error",
        "_v",         1);

    if (!success && message)
        json_object_set_new(result, "message", json_string(message));

    return result;
}
```

- [ ] **Step 3: Add `olc_action_session_t` fields to `olc_edit_state_t`**

In `editors/common/olc_changeset.h`, add to `olc_edit_state_t` (currently at line 93-97):

```c
/* Before olc_edit_state_t, add forward declaration: */
typedef struct olc_action_session olc_action_session_t;

/* Extend olc_edit_state_t: */
typedef struct olc_edit_state {
    LLIST           *active_changesets;
    LLIST           *string_edit_sessions;
    int              next_string_session_id;
    /* Action session tracking (Phase 5) */
    olc_action_session_t action_session;
    int              next_action_session_id;
    bool             has_action_session;
} olc_edit_state_t;
```

Define `olc_action_session_t` directly in `olc_changeset.h` (it's just char arrays and ints, no jansson dependency). Then `olc_actions.h` includes `olc_changeset.h` and uses it. Remove the duplicate `olc_action_session_t` typedef from `olc_actions.h` — it lives only in `olc_changeset.h`.

- [ ] **Step 4: Add new files to both build systems**

In `CMakeLists.txt`, add `editors/common/olc_actions.c` to the source list (near the other `editors/common/olc_*.c` entries).

In `Makefile`, add `editors/common/olc_actions.c` to `C_FILES` (near the other `editors/common/olc_*.c` entries).

- [ ] **Step 5: Write tests for registry and session management**

Create `tests/unit/olc_action_tests.c` with test functions (prefix: `olcact_`):

```c
static test_result_t test_olcact_session_create(test_case_t *test)
{
    olc_edit_state_t *state = olc_edit_state_create();
    olc_action_session_t *session = olc_action_session_create(state, ED_OBJECT, "addoprog");

    TEST_ASSERT_NOT_NULL(session);
    TEST_ASSERT_STR_EQ("act_0", session->session_id);
    TEST_ASSERT_STR_EQ("addoprog", session->action_name);
    TEST_ASSERT_INT_EQ(ED_OBJECT, session->editor_type);
    TEST_ASSERT_TRUE(state->has_action_session);

    olc_edit_state_destroy(state);
    return TEST_SUCCESS;
}

static test_result_t test_olcact_session_find(test_case_t *test)
{
    olc_edit_state_t *state = olc_edit_state_create();
    olc_action_session_create(state, ED_OBJECT, "addoprog");

    TEST_ASSERT_NOT_NULL(olc_action_session_find(state, "act_0"));
    TEST_ASSERT_NULL(olc_action_session_find(state, "act_999"));

    olc_edit_state_destroy(state);
    return TEST_SUCCESS;
}

static test_result_t test_olcact_session_clear(test_case_t *test)
{
    olc_edit_state_t *state = olc_edit_state_create();
    olc_action_session_create(state, ED_OBJECT, "addoprog");
    olc_action_session_clear(state);

    TEST_ASSERT_FALSE(state->has_action_session);
    TEST_ASSERT_NULL(olc_action_session_find(state, "act_0"));

    olc_edit_state_destroy(state);
    return TEST_SUCCESS;
}

static test_result_t test_olcact_session_replace(test_case_t *test)
{
    olc_edit_state_t *state = olc_edit_state_create();
    olc_action_session_create(state, ED_OBJECT, "addoprog");
    olc_action_session_t *s2 = olc_action_session_create(state, ED_OBJECT, "addaffect");

    /* First session replaced */
    TEST_ASSERT_NULL(olc_action_session_find(state, "act_0"));
    TEST_ASSERT_NOT_NULL(olc_action_session_find(state, "act_1"));
    TEST_ASSERT_STR_EQ("addaffect", s2->action_name);

    olc_edit_state_destroy(state);
    return TEST_SUCCESS;
}

static test_result_t test_olcact_build_form(test_case_t *test)
{
    /* Build a form and verify JSON structure */
    olc_action_handler_t handler = {
        .action_name = "addoprog", .editor_type = ED_OBJECT,
        .display_hint = "inline", .title = "Add Object Program",
        .list_name = "oprogs"
    };
    olc_action_session_t session = { .session_id = "act_0" };
    json_t *fields = json_array();
    json_array_append_new(fields, json_pack("{s:s, s:s}", "field", "script", "type", "widevnum"));

    json_t *form = olc_action_build_form("obj:5#3010", &handler, &session, fields);
    TEST_ASSERT_NOT_NULL(form);
    TEST_ASSERT_STR_EQ("obj:5#3010", json_string_value(json_object_get(form, "entity_id")));
    TEST_ASSERT_STR_EQ("addoprog", json_string_value(json_object_get(form, "action")));
    TEST_ASSERT_STR_EQ("act_0", json_string_value(json_object_get(form, "session_id")));
    TEST_ASSERT_STR_EQ("inline", json_string_value(json_object_get(form, "display")));
    TEST_ASSERT_INT_EQ(1, json_integer_value(json_object_get(form, "_v")));

    json_decref(form);
    json_decref(fields);
    return TEST_SUCCESS;
}

static test_result_t test_olcact_build_result_success(test_case_t *test)
{
    json_t *result = olc_action_build_result("obj:5#3010", "act_0", true, NULL);
    TEST_ASSERT_STR_EQ("success", json_string_value(json_object_get(result, "status")));
    TEST_ASSERT_NULL(json_object_get(result, "message"));
    json_decref(result);
    return TEST_SUCCESS;
}

static test_result_t test_olcact_build_result_error(test_case_t *test)
{
    json_t *result = olc_action_build_result("obj:5#3010", "act_0", false, "Script not found");
    TEST_ASSERT_STR_EQ("error", json_string_value(json_object_get(result, "status")));
    TEST_ASSERT_STR_EQ("Script not found", json_string_value(json_object_get(result, "message")));
    json_decref(result);
    return TEST_SUCCESS;
}
```

Register in `test_dispatcher.c` handler table and `test_modules.h`. Create `tests/data/unit/olc_action_tests.json` with test suite and test cases. Add suite to `test_config.json`.

- [ ] **Step 6: Build and run tests**

```bash
cd /sentience/src && ./build tests
./install debug
cd /sentience && ./sent -test:olcact
```

Expected: All 7 new tests pass.

- [ ] **Step 7: Commit**

```bash
git add -A && git commit -m "feat(olc): action infrastructure — types, registry, session management

New files: olc_actions.h/.c with action handler types, registry,
session tracking, and GMCP message builders. Extends olc_edit_state_t
with action session support. Includes 7 unit tests."
```

---

### Task 2: GMCP Protocol Integration — Action + Action.Submit Handlers

**Files:**
- Modify: `protocol.h` (~line 325, add enum values before `GMCP_RECEIVE_MAX`)
- Modify: `protocol.c` (~line 3382, add message name mappings)
- Modify: `gmcp_editor.c` (~line 955, add switch cases; add handler functions)
- Modify: `gmcp_editor.h` (add declarations for send helpers)

**Context:** The GMCP receive enum is at `protocol.h:318-326`. Message name mappings are at `protocol.c:3375-3382`. The dispatch switch is in `gmcp_editor.c:928-952` inside `sentience_handle_editor()`. Follow the exact same pattern as existing handlers.

- [ ] **Step 1: Add GMCP enum values in `protocol.h`**

Add before `GMCP_RECEIVE_MAX` (line 326):

```c
   GMCP_SENTIENCE_EDITOR_DRAFT_LOAD,
   GMCP_SENTIENCE_EDITOR_ACTION,         /* NEW */
   GMCP_SENTIENCE_EDITOR_ACTION_SUBMIT,  /* NEW */
   GMCP_RECEIVE_MAX
```

- [ ] **Step 2: Add message name mappings in `protocol.c`**

Add after the `EDITOR_DRAFT_LOAD` entry (~line 3382):

```c
   { GMCP_SENTIENCE_EDITOR_ACTION,       "Sentience.Editor.Action"           },
   { GMCP_SENTIENCE_EDITOR_ACTION_SUBMIT,"Sentience.Editor.Action.Submit"    },
```

- [ ] **Step 3: Add dispatch cases in `gmcp_editor.c` and routing in `protocol.c`**

In `sentience_handle_editor()`, add cases after `GMCP_SENTIENCE_EDITOR_DRAFT_LOAD`:

```c
        case GMCP_SENTIENCE_EDITOR_ACTION:
            handle_editor_action(d, payload);
            break;
        case GMCP_SENTIENCE_EDITOR_ACTION_SUBMIT:
            handle_editor_action_submit(d, payload);
            break;
```

**Also in `protocol.c`** (~line 4257), add the new enum values to the GMCP dispatch routing switch so messages are actually forwarded to `sentience_handle_editor()`:

```c
       case GMCP_SENTIENCE_EDITOR_ACTION:
       case GMCP_SENTIENCE_EDITOR_ACTION_SUBMIT:
```

Add these alongside the existing `GMCP_SENTIENCE_EDITOR_*` cases (before the `{` block at line 4258).

- [ ] **Step 4: Implement `handle_editor_action` in `gmcp_editor.c`**

Uses `sentience_send_package()` (not `SendGMCPRaw`) for all GMCP sends — matching the pattern used by every other handler in this file (see gmcp_editor.c:322-450). Error responses use `olc_action_build_result()` sent via `sentience_send_package()` as `Sentience.Editor.Action.Result`.

```c
static void handle_editor_action(descriptor_t *d, json_t *payload)
{
    /* 1. Parse entity_id and action from payload */
    const char *entity_id_str = json_string_value(json_object_get(payload, "entity_id"));
    const char *action_name   = json_string_value(json_object_get(payload, "action"));
    if (!entity_id_str || !action_name) {
        gmcp_editor_send_error(d, entity_id_str ? entity_id_str : "",
            "action", "invalid_request", "Missing entity_id or action");
        return;
    }

    /* 2. Validate entity_id matches active editor */
    int editor_type;
    WNUM_LOAD wnum;
    if (!gmcp_editor_parse_entity_id(entity_id_str, &editor_type, &wnum)) {
        gmcp_editor_send_error(d, entity_id_str, "action", "invalid_entity", "Invalid entity_id");
        return;
    }

    if (d->editor != editor_type) {
        gmcp_editor_send_error(d, entity_id_str, "action", "type_mismatch", "Editor type mismatch");
        return;
    }

    /* 3. Find active changeset */
    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(editor_type);
    olc_changeset_t *cs = olc_edit_state_find_changeset(d->olc_state,
        editor_type, wnum);
    if (!cs || !edef) {
        gmcp_editor_send_error(d, entity_id_str, "action", "no_changeset", "No active changeset");
        return;
    }

    /* 4. Look up action handler */
    const olc_action_handler_t *handler = olc_find_action(editor_type, action_name);
    if (!handler) {
        json_t *result = olc_action_build_result(entity_id_str, "", false, "Unknown action");
        sentience_send_package(d, "Sentience.Editor.Action.Result", result);
        return;
    }

    /* 5. Call form_fn to build form fields */
    json_t *fields = handler->form_fn(d->pEdit, d->character);
    if (!fields) {
        json_t *result = olc_action_build_result(entity_id_str, "", false,
            "Failed to generate form");
        sentience_send_package(d, "Sentience.Editor.Action.Result", result);
        return;
    }

    /* 6. Create action session */
    olc_action_session_t *session = olc_action_session_create(
        d->olc_state, editor_type, action_name);

    /* 7. Build and send Action.Form */
    json_t *form = olc_action_build_form(entity_id_str, handler, session, fields);
    sentience_send_package(d, "Sentience.Editor.Action.Form", form);
    json_decref(fields);
}
```

- [ ] **Step 5: Implement `handle_editor_action_submit` in `gmcp_editor.c`**

Same pattern — `sentience_send_package()` for all sends. For post-stage updates, the field notification and schema refresh logic is nontrivial — delegate to a helper that determines the affected tab and rebuilds its fields. Reference the commit handler at gmcp_editor.c:670-725 which already handles schema refresh after list changes.

```c
static void handle_editor_action_submit(descriptor_t *d, json_t *payload)
{
    const char *entity_id_str = json_string_value(json_object_get(payload, "entity_id"));
    const char *session_id    = json_string_value(json_object_get(payload, "session_id"));
    json_t *values            = json_object_get(payload, "values");

    if (!entity_id_str || !session_id || !values) {
        gmcp_editor_send_error(d, entity_id_str ? entity_id_str : "",
            "action", "invalid_request", "Missing required fields");
        return;
    }

    /* Validate session */
    olc_action_session_t *session = olc_action_session_find(d->olc_state, session_id);
    if (!session) {
        json_t *result = olc_action_build_result(entity_id_str, session_id,
            false, "Invalid or expired session");
        sentience_send_package(d, "Sentience.Editor.Action.Result", result);
        return;
    }

    /* Find handler and changeset */
    int editor_type;
    WNUM_LOAD wnum;
    gmcp_editor_parse_entity_id(entity_id_str, &editor_type, &wnum);

    const olc_action_handler_t *handler = olc_find_action(
        session->editor_type, session->action_name);
    olc_changeset_t *cs = olc_edit_state_find_changeset(
        d->olc_state, editor_type, wnum);

    if (!handler || !cs) {
        json_t *result = olc_action_build_result(entity_id_str, session_id,
            false, "Action handler or changeset not found");
        sentience_send_package(d, "Sentience.Editor.Action.Result", result);
        olc_action_session_clear(d->olc_state);
        return;
    }

    /* Check staging limits */
    if (!olc_check_staging_limits(d->character, cs)) {
        json_t *result = olc_action_build_result(entity_id_str, session_id,
            false, "Changeset limit reached");
        sentience_send_package(d, "Sentience.Editor.Action.Result", result);
        olc_action_session_clear(d->olc_state);
        return;
    }

    /* Call stage_fn */
    char errbuf[MAX_STRING_LENGTH];
    errbuf[0] = '\0';
    bool ok = handler->stage_fn(d->pEdit, cs, values, errbuf, sizeof(errbuf));

    /* Send result */
    json_t *result = olc_action_build_result(entity_id_str, session_id,
        ok, ok ? NULL : errbuf);
    sentience_send_package(d, "Sentience.Editor.Action.Result", result);

    if (ok) {
        /*
         * Post-stage GMCP updates:
         * 1. Send Editor.Field for the list_name with is_pending=true
         *    Signature: gmcp_editor_send_field(d, entity_id, field, value, type_str, is_pending)
         *    For list staging, value=NULL and type_str="list" signals the field is pending.
         */
        gmcp_editor_send_field(d, entity_id_str, handler->list_name,
            NULL, "list", true);

        /*
         * 2. Send Schema.Update if tab structure changed.
         *    This requires determining the affected tab and rebuilding its fields JSON.
         *    Use the same schema capture approach as handle_editor_commit (gmcp_editor.c:670-725):
         *    - Get the OLC_EDITOR_DEF
         *    - Find which tab contains this list_name
         *    - Run the tab's show_fn in capture mode to get updated fields
         *    - Call gmcp_editor_send_schema_update(d, entity_id, tab_name, fields)
         *
         *    Extract this into a reusable helper: gmcp_editor_refresh_tab_for_list()
         *    that both handle_editor_action_submit and handle_editor_commit can call.
         */
        gmcp_editor_refresh_tab_for_list(d, entity_id_str, handler->list_name);
    }

    /* Clear session regardless of success/failure */
    olc_action_session_clear(d->olc_state);
}
```

The `gmcp_editor_refresh_tab_for_list()` helper needs to:
1. Find which tab contains the given list_name (from the editor def's tab config)
2. Run that tab's show_fn in capture mode to get the fields JSON
3. Call `gmcp_editor_send_schema_update(d, entity_id, tab_name, captured_fields)`

This is the same logic already used in `handle_editor_commit` for Schema.Update after type changes — extract it into a shared helper.

- [ ] **Step 6: Add declarations to `gmcp_editor.h`**

```c
/* Action protocol (Phase 5) */
void gmcp_editor_refresh_tab_for_list(descriptor_t *d,
    const char *entity_id, const char *list_name);
```

Note: The `handle_editor_action` and `handle_editor_action_submit` functions are `static` in `gmcp_editor.c` (not exposed in the header) — same pattern as all other handlers. The `gmcp_editor_refresh_tab_for_list` helper is exposed because it's useful for the commit path too.

- [ ] **Step 7: Build and verify compilation**

```bash
cd /sentience/src && ./build tests
```

Expected: Clean build. No new test failures.

- [ ] **Step 8: Commit**

```bash
git add -A && git commit -m "feat(gmcp): Editor.Action and Editor.Action.Submit handlers

Add GMCP protocol support for action forms. Editor.Action triggers
form generation via registered handler. Editor.Action.Submit validates
session, calls stage_fn, sends result + field/schema updates."
```

---

### Task 3: Oedit Action Handlers — Form and Stage Functions

**Files:**
- Create: `editors/objects/oedit_actions.c`
- Modify: `editors/objects/oedit.c` (register action table, refactor addoprog/addskill)
- Modify: `CMakeLists.txt` (add `oedit_actions.c`)
- Modify: `Makefile` (add `oedit_actions.c`)

**Context:** Oedit has the most actions (~10). Some already stage (addaffect, addspell, addcatalyst, addquest, addtype, addwaypoint) — these need form_fn + stage_fn wrappers. Two apply immediately (addoprog, addskill) — these need conversion. Each form_fn returns a `json_t*` array of field descriptors. Each stage_fn validates values and calls `olc_stage_list_add()`.

Reference the existing staged `oedit_addaffect` pattern at `oedit.c:1597-1700` — the stage_fn extracts the same data but from JSON `values` instead of text parsing.

Reference the apply handler `oedit_apply_affect_ops` at `oedit.c:415-491` — on commit, LIST_ADD changes are applied by creating the runtime struct from JSON.

The form_fn should return field descriptors matching the spec's form field definitions. For enum fields, include the `options` array with valid values. Use existing flag/lookup tables (`apply_flags`, `apply_types`, `oprog_triggers`, etc.) to populate options.

**Important patterns:**
- `flag_string(apply_flags, ...)` returns the string name for a flag value
- Script triggers are in `oprog_flags` table (for oprogs), `mprog_flags` (mprogs), etc.
- Available spell list: iterate `skill_table` where `skill_table[sn].spell_fun != spell_null`
- Available skill list: iterate `skill_table` where applicable
- Object types: `type_flag[]` table, or use `ITEM_*` constants

The oedit_actions.c file defines:
1. One `form_fn` + one `stage_fn` per action
2. The `oedit_actions[]` handler table
3. An init function to register with the action registry

For actions that already stage (addaffect, addspell, etc.), the existing telnet command functions can stay as-is but also get a stage_fn for GMCP use. Alternatively, refactor the telnet command to call the shared stage_fn. Prefer the refactor — extract the staging logic into the stage_fn, have the telnet command parse text → build JSON → call stage_fn.

For addoprog (currently immediate), convert to staged: the telnet command now parses text → builds JSON → calls `oedit_action_oprog_stage()` which calls `olc_stage_list_add()`. Add the matching apply handler `oedit_apply_oprog_ops` to the field handler table (it's already there at oedit.c:794).

- [ ] **Step 1: Create `oedit_actions.c` with form and stage functions for all 10 actions**

Each form_fn follows this pattern:
```c
json_t *oedit_action_oprog_form(void *entity, CHAR_DATA *ch)
{
    json_t *fields = json_array();
    /* Script field */
    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b}",
        "field", "script", "label", "Script", "type", "widevnum", "required", true));
    /* Trigger enum field with options from oprog_flags */
    json_t *options = json_array();
    /* Populate from oprog_flags table */
    for (int i = 0; oprog_flags[i].name; i++)
        json_array_append_new(options, json_string(oprog_flags[i].name));
    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:o}",
        "field", "trigger", "label", "Trigger", "type", "enum",
        "required", true, "options", options));
    /* Phrase field */
    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:s}",
        "field", "phrase", "label", "Phrase", "type", "string",
        "required", true, "default", "*"));
    return fields;
}
```

Each stage_fn follows this pattern:
```c
bool oedit_action_oprog_stage(void *entity, olc_changeset_t *cs,
    json_t *values, char *errbuf, size_t errlen)
{
    const char *script_str = json_string_value(json_object_get(values, "script"));
    const char *trigger_str = json_string_value(json_object_get(values, "trigger"));
    const char *phrase = json_string_value(json_object_get(values, "phrase"));

    /* Validate inputs */
    if (!script_str || !trigger_str) {
        snprintf(errbuf, errlen, "Missing required fields");
        return false;
    }

    /* Parse and validate widevnum */
    WNUM_LOAD script_wnum;
    if (!parse_widevnum(script_str, &script_wnum)) {
        snprintf(errbuf, errlen, "Invalid script vnum: %s", script_str);
        return false;
    }

    /* Validate trigger */
    int trig = flag_value(oprog_flags, trigger_str);
    if (trig == NO_FLAG) {
        snprintf(errbuf, errlen, "Unknown trigger: %s", trigger_str);
        return false;
    }

    /* Build staged JSON value */
    json_t *val = json_pack("{s:s, s:s, s:s}",
        "script", script_str,
        "trigger", trigger_str,
        "phrase", phrase ? phrase : "*");

    olc_stage_list_add(cs, "oprogs", val);
    json_decref(val);
    return true;
}
```

Include all 10 actions: addaffect, addimmune, addspell, addskill, addoprog, addquest, addcatalyst, addwaypoint, addtype, removetype.

End with handler table:
```c
const olc_action_handler_t oedit_actions[] = {
    { "addaffect",    ED_OBJECT, "inline", "Add Affect",           "affects",    oedit_action_affect_form,    oedit_action_affect_stage },
    { "addimmune",    ED_OBJECT, "inline", "Add Immunity",         "affects",    oedit_action_immune_form,    oedit_action_immune_stage },
    { "addspell",     ED_OBJECT, "inline", "Add Spell",            "spells",     oedit_action_spell_form,     oedit_action_spell_stage },
    { "addskill",     ED_OBJECT, "inline", "Add Skill",            "affects",    oedit_action_skill_form,     oedit_action_skill_stage },
    { "addoprog",     ED_OBJECT, "inline", "Add Object Program",   "oprogs",     oedit_action_oprog_form,     oedit_action_oprog_stage },
    { "addquest",     ED_OBJECT, "inline", "Add Quest",            "quests",     oedit_action_quest_form,     oedit_action_quest_stage },
    { "addcatalyst",  ED_OBJECT, "inline", "Add Catalyst",         "catalysts",  oedit_action_catalyst_form,  oedit_action_catalyst_stage },
    { "addwaypoint",  ED_OBJECT, "inline", "Add Waypoint",         "waypoints",  oedit_action_waypoint_form,  oedit_action_waypoint_stage },
    { "addtype",      ED_OBJECT, "inline", "Add Type",             "typedata",   oedit_action_addtype_form,   oedit_action_addtype_stage },
    { "removetype",   ED_OBJECT, "inline", "Remove Type",          "typedata",   oedit_action_removetype_form, oedit_action_removetype_stage },
    { NULL }
};
```

- [ ] **Step 2: Refactor `oedit_addoprog` in `oedit.c` to use stage_fn**

Replace the direct-apply code path with:
```c
/* Parse text arguments into values... */
json_t *values = json_pack("{s:s, s:s, s:s}",
    "script", widevnum_string, "trigger", trigger_name, "phrase", phrase);

char errbuf[MAX_STRING_LENGTH];
if (!oedit_action_oprog_stage(pObj, cs, values, errbuf, sizeof(errbuf))) {
    send_to_char(errbuf, ch);
    json_decref(values);
    return false;
}
json_decref(values);
printf_to_char(ch, "{G[STAGED]{x Object program added.\n\r");
```

Do the same for `oedit_addskill` (which has partial staging — complete the conversion).

- [ ] **Step 3: Register oedit action table**

In `oedit.c`, call `olc_register_actions(oedit_actions)` during editor registration (e.g., in the init section or alongside `olc_register_editor(&oedit_def)`).

- [ ] **Step 4: Add to build systems**

Add `editors/objects/oedit_actions.c` to both `CMakeLists.txt` and `Makefile`.

- [ ] **Step 5: Write tests for oedit form and stage functions**

Add to `olc_action_tests.c`:
- `test_olcact_oprog_form` — verify form fields and trigger options
- `test_olcact_oprog_stage` — create changeset, call stage_fn, verify pending change
- `test_olcact_oprog_stage_invalid` — bad vnum → error message
- `test_olcact_affect_stage` — verify affect staging via stage_fn
- `test_olcact_addtype_form` — verify type enum options

- [ ] **Step 6: Build and run tests**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test:olcact
```

Expected: All new tests pass. No regressions.

- [ ] **Step 7: Commit**

```bash
git add -A && git commit -m "feat(oedit): action handlers for all 10 oedit list operations

Form and stage functions for addaffect, addimmune, addspell, addskill,
addoprog, addquest, addcatalyst, addwaypoint, addtype, removetype.
Refactors addoprog and addskill to stage through changeset."
```

---

### Task 4: Medit Action Handlers + Apply Handlers

**Files:**
- Create: `editors/mobiles/medit_actions.c`
- Modify: `editors/mobiles/medit.c` (refactor addmprog/addquest/addreputation to use stage_fn; add list apply handlers to field_handlers table; register action table)
- Modify: `CMakeLists.txt` (add `medit_actions.c`)
- Modify: `Makefile` (add `medit_actions.c`)

**Context:** Medit currently has NO list staging at all. All three add commands (addmprog, addquest, addreputation) apply immediately via direct struct manipulation. This task must:

1. Create form_fn + stage_fn for each action in `medit_actions.c`
2. Add apply handlers (`medit_apply_mprog_ops`, `medit_apply_quest_ops`, `medit_apply_reputation_ops`) in `medit.c`
3. Register those apply handlers in `medit_field_handlers[]` (currently at medit.c:403-456)
4. Refactor the telnet commands to call the shared stage_fn
5. Register the medit action table

The apply handlers follow the same pattern as `oedit_apply_affect_ops` (oedit.c:415-491): switch on `change->field_type` for `OLC_FIELD_LIST_ADD` and `OLC_FIELD_LIST_REMOVE`, create/remove runtime structs from JSON.

**Important:** `medit_addmprog` (medit.c:3506-3613) creates `PROG_LIST *list = new_trigger()` and populates fields. The apply handler must do the same from JSON. The del commands (delmprog, delquest, delreputation) also need conversion — they currently remove items directly.

Actions: addmprog, addquest, addreputation (3 actions, each with add and corresponding delete).

- [ ] **Step 1: Create `medit_actions.c` with form and stage functions**

Three form_fn/stage_fn pairs + handler table. Follow the oprog pattern from Task 3. Trigger flags for mprogs use `mprog_flags` table.

- [ ] **Step 2: Add apply handlers in `medit.c`**

```c
bool medit_apply_mprog_ops(void *entity, olc_pending_change_t *change)
{
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;

    if (change->field_type == OLC_FIELD_LIST_ADD) {
        /* Create PROG_LIST from JSON, add to pMob->progs */
    }
    if (change->field_type == OLC_FIELD_LIST_REMOVE) {
        /* Remove by index from pMob->progs */
    }
    return false;
}
```

Similarly for quest_ops and reputation_ops.

- [ ] **Step 3: Register apply handlers in `medit_field_handlers[]`**

Add entries at end of table (before NULL terminator):
```c
    { "mprogs/**",      OLC_FIELD_LIST_ADD,  NULL, medit_apply_mprog_ops,      NULL },
    { "quests/**",      OLC_FIELD_LIST_ADD,  NULL, medit_apply_quest_ops,      NULL },
    { "reputations/**", OLC_FIELD_LIST_ADD,  NULL, medit_apply_reputation_ops, NULL },
```

- [ ] **Step 4: Refactor telnet add commands to call stage_fn**

Convert `medit_addmprog`, `medit_addquest`, `medit_addreputation` to parse text → build JSON → call stage_fn. Also convert the corresponding delete commands (delmprog, delquest, delreputation) to use `olc_stage_list_remove()`.

- [ ] **Step 5: Register medit action table**

In `medit.c`, call `olc_register_actions(medit_actions)` alongside editor registration.

- [ ] **Step 6: Add to build systems and write tests**

Add `editors/mobiles/medit_actions.c` to both build files. Add tests:
- `test_olcact_mprog_form` — verify form fields
- `test_olcact_mprog_stage` — verify staging
- `test_olcact_medit_quest_stage` — verify quest staging
- `test_olcact_medit_reputation_stage` — verify reputation staging

- [ ] **Step 7: Build and run tests**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test:olcact
```

- [ ] **Step 8: Commit**

```bash
git add -A && git commit -m "feat(medit): action handlers and apply handlers for list operations

Adds form/stage functions for addmprog, addquest, addreputation.
Adds apply handlers for commit. Converts all telnet add/del commands
to stage through changesets."
```

---

### Task 5: Redit Action Handlers + Apply Handlers + delreset

**Files:**
- Create: `editors/rooms/redit_actions.c`
- Modify: `editors/rooms/redit.c` (refactor mreset/oreset/addrprog/addcdesc; add delreset command; add apply handlers; register action table)
- Modify: `CMakeLists.txt` (add `redit_actions.c`)
- Modify: `Makefile` (add `redit_actions.c`)

**Context:** Redit has 5 actions, the most complex being mreset and oreset (dialog display hint with conditional fields). Also adds `delreset` which is entirely new — no telnet command exists for it yet.

**Critical details:**
- `redit_mreset` (redit.c:1615-1683) creates `RESET_DATA` with `command = 'M'`, args for mob wnum, max, min, room vnum. Also calls `create_mobile()` to instantiate the mob in the room.
- `redit_oreset` (redit.c:1685-1750+) creates `RESET_DATA` with `command = 'O'`, `'P'` (put in container), or `'E'` (equip on mob) depending on target. Has conditional fields.
- `add_reset()` adds to the room's reset list — the apply handler must replicate this.
- The staged mode should NOT instantiate mobs/objects — that only happens on full area reset.
- `delreset` removes a reset by index from `pRoom->reset_first` linked list.
- Conditional fields: oreset's `target_name` and `wear_loc` use `visible_when` in the form JSON.

Actions: mreset (dialog), oreset (dialog), delreset (inline, new), addrprog (inline), addcdesc (inline).

- [ ] **Step 1: Create `redit_actions.c` with form and stage functions**

Five form_fn/stage_fn pairs. The oreset form_fn includes `visible_when` on conditional fields:
```c
json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:b, s:{s:[s,s]}}",
    "field", "target_name", "label", "Target Name", "type", "string",
    "required", false,
    "visible_when", "target", "container", "mob"));
```

- [ ] **Step 2: Add `redit_delreset` telnet command**

Add to `redit_table[]` and implement:
```c
REDIT(redit_delreset)
{
    /* Parse index argument, validate, stage removal */
}
```

- [ ] **Step 3: Add apply handlers in `redit.c`**

```c
bool redit_apply_reset_ops(void *entity, olc_pending_change_t *change);
bool redit_apply_rprog_ops(void *entity, olc_pending_change_t *change);
bool redit_apply_cdesc_ops(void *entity, olc_pending_change_t *change);
```

- [ ] **Step 4: Register apply handlers in `redit_field_handlers[]`**

Add entries:
```c
    { "resets/**",  OLC_FIELD_LIST_ADD,  NULL, redit_apply_reset_ops,  NULL },
    { "rprogs/**",  OLC_FIELD_LIST_ADD,  NULL, redit_apply_rprog_ops,  NULL },
    { "cdescs/**",  OLC_FIELD_LIST_ADD,  NULL, redit_apply_cdesc_ops,  NULL },
```

- [ ] **Step 5: Refactor telnet commands to call stage_fn**

Convert `redit_mreset`, `redit_oreset`, `redit_addrprog`, `redit_addcdesc` and their delete counterparts.

- [ ] **Step 6: Register redit action table, add to build systems**

- [ ] **Step 7: Write tests**

- `test_olcact_mreset_form` — verify dialog display hint, form fields
- `test_olcact_mreset_stage` — verify reset staging
- `test_olcact_oreset_form` — verify conditional fields with visible_when
- `test_olcact_delreset_stage` — verify reset deletion staging
- `test_olcact_rprog_stage` — verify rprog staging

- [ ] **Step 8: Build and run tests**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test:olcact
```

- [ ] **Step 9: Commit**

```bash
git add -A && git commit -m "feat(redit): action handlers, apply handlers, and delreset

Adds form/stage for mreset, oreset, delreset (new), addrprog, addcdesc.
Adds apply handlers for commit. Converts all telnet reset/prog/cdesc
commands to stage. oreset form uses visible_when for conditional fields."
```

---

### Task 6: Aedit Action Handlers + Apply Handlers

**Files:**
- Create: `editors/areas/aedit_actions.c`
- Modify: `editors/areas/aedit.c` (refactor addaprog/addtrade; add apply handlers; register action table)
- Modify: `CMakeLists.txt` (add `aedit_actions.c`)
- Modify: `Makefile` (add `aedit_actions.c`)

**Context:** Aedit has the fewest actions (2: addaprog, addtrade). Both currently apply immediately.

- `aedit_addaprog` (aedit.c:2074-2147) follows the same prog pattern as medit/oedit. The form_fn uses `aprog_flags` for trigger options.
- `aedit_add_trade` (aedit.c:864-922) creates trade items via `new_trade_item()`. The stage_fn packs obj_vnum, replenish_time, replenish_amount, max_qty, min_price, max_price into JSON.

Actions: addaprog (inline), addtrade (inline).

- [ ] **Step 1: Create `aedit_actions.c` with form and stage functions**

Two form_fn/stage_fn pairs + handler table.

- [ ] **Step 2: Add apply handlers in `aedit.c`**

```c
bool aedit_apply_aprog_ops(void *entity, olc_pending_change_t *change);
bool aedit_apply_trade_ops(void *entity, olc_pending_change_t *change);
```

- [ ] **Step 3: Register apply handlers in `aedit_field_handlers[]`**

```c
    { "aprogs/**",  OLC_FIELD_LIST_ADD,  NULL, aedit_apply_aprog_ops,  NULL },
    { "trades/**",  OLC_FIELD_LIST_ADD,  NULL, aedit_apply_trade_ops,  NULL },
```

- [ ] **Step 4: Refactor telnet commands, register action table, add to build systems**

- [ ] **Step 5: Write tests**

- `test_olcact_aprog_form` — verify form fields
- `test_olcact_aprog_stage` — verify staging
- `test_olcact_trade_form` — verify trade form with 6 fields
- `test_olcact_trade_stage` — verify trade staging

- [ ] **Step 6: Build and run tests**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test:olcact
```

- [ ] **Step 7: Commit**

```bash
git add -A && git commit -m "feat(aedit): action handlers and apply handlers for list operations

Adds form/stage for addaprog and addtrade. Adds apply handlers for
commit. Converts telnet commands to stage through changesets."
```

---

### Task 7: GMCP Reference Documentation

**Files:**
- Modify: `docs/GMCP_WEB_CLIENT_REFERENCE.md`

**Context:** The reference doc already has Editor.Open, Editor.Set, Editor.Field, Editor.Commit, Editor.Revert, Editor.Schema.Update, StringEdit, Draft sections. Add the new Action messages following the same format.

- [ ] **Step 1: Add TOC entries**

Add to the table of contents (under Editor section):
```
- Editor.Action (Client → Server)
- Editor.Action.Form (Server → Client)
- Editor.Action.Submit (Client → Server)
- Editor.Action.Result (Server → Client)
```

- [ ] **Step 2: Add Editor.Action section**

Document all four messages with JSON examples, field tables, display hints explanation, and conditional fields (`visible_when`).

- [ ] **Step 3: Update Package Summary table**

Add action messages to the package/message summary.

- [ ] **Step 4: Update Editor lifecycle section**

Add action flow to the lifecycle diagram/description.

- [ ] **Step 5: Add Client Implementation Guide for action forms**

Document how clients should render inline vs dialog forms, handle `visible_when` field visibility, and manage action session lifecycle.

- [ ] **Step 6: Commit**

```bash
git add docs/GMCP_WEB_CLIENT_REFERENCE.md
git commit -m "docs: Editor.Action protocol in GMCP reference

Documents Editor.Action, Action.Form, Action.Submit, Action.Result.
Includes conditional fields, display hints, action lifecycle, and
client implementation guidance."
```

---

### Task 8: Integration Tests + Full Test Suite Validation

**Files:**
- Modify: `tests/unit/olc_action_tests.c` (add integration tests)
- Modify: `tests/data/unit/olc_action_tests.json` (add integration test cases)

**Context:** Tasks 1-6 added unit tests for individual components. This task adds end-to-end integration tests that verify the full round-trip: registry lookup → form generation → stage → commit → apply → runtime state verification.

- [ ] **Step 1: Write integration test for oedit oprog round-trip**

```c
static test_result_t test_olcact_oprog_roundtrip(test_case_t *test)
{
    /* Create OBJ_INDEX_DATA, changeset */
    /* Look up "addoprog" handler via olc_find_action(ED_OBJECT, "addoprog") */
    /* Call form_fn, verify fields */
    /* Call stage_fn with values, verify change staged */
    /* Call olc_changeset_commit with oedit_field_handlers */
    /* Verify pObj->progs has new entry */
    /* Cleanup */
}
```

- [ ] **Step 2: Write integration test for redit reset round-trip**

- [ ] **Step 3: Write integration test for medit mprog round-trip**

- [ ] **Step 4: Run full test suite**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test
```

Expected: All existing tests pass + all new tests pass. Document final count.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "test: integration tests for action protocol round-trips

End-to-end tests verifying registry → form → stage → commit → apply
for oedit oprogs, redit resets, and medit mprogs."
```

---

### Task 9: Implementation Plan Commit

**Files:**
- Create: `docs/superpowers/plans/2026-04-09-editor-action-protocol.md`

- [ ] **Step 1: Commit the plan document**

```bash
git add docs/superpowers/plans/2026-04-09-editor-action-protocol.md
git commit -m "docs: Phase 5 editor action protocol implementation plan"
```

---

## Build and Test Commands

```bash
# Build with tests
cd /sentience/src && ./build tests

# Install
./install debug

# Run all tests
cd /sentience && ./sent -test

# Run action tests only
cd /sentience && ./sent -test:olcact

# Expected baseline before Phase 5: 615 tests, 599 pass, 2 known failures, 14 skipped
```

## Dependencies

Tasks 3-6 all depend on Task 1 (infrastructure) and Task 2 (GMCP handlers).
Tasks 3-6 are independent of each other and could run in parallel.
Task 7 (docs) is independent of implementation tasks.
Task 8 (integration tests) depends on Tasks 3-6.
Task 9 (plan commit) has no dependencies.
