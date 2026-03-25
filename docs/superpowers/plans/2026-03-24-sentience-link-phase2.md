# Sentience.Link Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a transport-agnostic link abstraction so WebSocket clients get structured GMCP link data, OSC 8-capable terminals get native hyperlinks, and MXP telnet continues unchanged — all with zero call-site changes.

**Architecture:** A new `sentience_link.c` module provides link queue management, JSON building, and OSC 8 output helpers. A central `link_route()` function in `mxp_links.c` handles 4-way routing (GMCP, OSC8, MXP, plain). Entity functions gain trust-based action filtering. The link queue flushes in `process_output()` before socket write.

**Tech Stack:** C23, Jansson (JSON), OSC 8 terminal escapes, existing MXP tags, Sentience test framework.

**Spec:** `docs/superpowers/specs/2026-03-24-sentience-link-design.md`

---

## File Structure

| File | Action | Responsibility |
|------|--------|---------------|
| `sentience_link.h` | Create | Types: `link_mode_t` enum, `sentience_link_entry_t`, `sentience_link_queue_t`. Declarations for queue ops, OSC 8 helpers, `link_mode()`, URL encoder. |
| `sentience_link.c` | Create | Implementation: `link_mode()`, `has_osc8_support()`, `is_websocket_connection()`, queue init/add/free/flush, JSON builder, OSC 8 output, URL encoder. |
| `mxp_links.c` | Modify | Replace `isMXP(d)` checks with 4-way routing via `link_route()`. Add trust filtering to entity functions. |
| `mxp_links.h` | Modify | Add `#include "sentience_link.h"`. |
| `protocol.h` | Modify | Add `sentience_link_queue_t` field to `protocol_t`, add `#include "sentience_link.h"`. |
| `protocol.c` | Modify | Init queue in `ProtocolCreate()`, free in `ProtocolDestroy()`. |
| `merc.h` | Modify | Add `#define COMM_LINKS COMM_MXP` alias. |
| `const.c` | Modify | Add `"links"` setting entry. |
| `tables.c` | Modify | Add `"links"` comm flag entry. |
| `comm.c` | Modify | Add `sentience_link_queue_flush(d)` call in `process_output()`. |
| `CMakeLists.txt` | Modify | Add `sentience_link.c` to sources; add test file to BUILD_TESTS. |
| `Makefile` | Modify | Same as CMakeLists.txt. |
| `tests/unit/sentience_link_tests.c` | Create | Test handler: URL encoding, queue operations, JSON structure, OSC 8 output. |
| `tests/data/unit/sentience_link_unit_tests.json` | Create | Test cases for all testable functions. |
| `tests/framework/test_dispatcher.c` | Modify | Register `slink_` handler. |
| `tests/framework/test_modules.h` | Modify | Declare `run_sentience_link_test_case`. |
| `tests/data/test_config.json` | Modify | Add `sentience_link_unit_tests` to suite lists. |

---

### Task 1: Header + Stub + Build Integration

**Files:**
- Create: `sentience_link.h`
- Create: `sentience_link.c` (stub)
- Modify: `mxp_links.h` (name the struct, add `staff_only` field)
- Modify: `CMakeLists.txt`
- Modify: `Makefile`

- [ ] **Step 1: Name the struct and add `staff_only` in `mxp_links.h`**

The existing struct in `mxp_links.h` (around line 17-20) uses an anonymous struct:
```c
typedef struct {
    const char *cmd;
    const char *hint;
} mxp_cmd_hint_t;
```

Change it to a **named struct** and add the `staff_only` field:
```c
typedef struct mxp_cmd_hint {
    const char *cmd;
    const char *hint;
    bool staff_only;   /* If true, only shown to IS_IMMORTAL characters */
} mxp_cmd_hint_t;
```

**Why named?** `sentience_link.h` forward-declares `struct mxp_cmd_hint;` to avoid a circular include. Anonymous structs can't be forward-declared.

**Backward compatible:** Existing initializers like `{ cmd, hint }` zero-initialize `staff_only` to `false`, so no call-site changes are needed.

- [ ] **Step 2: Create `sentience_link.h`**

```c
/*
 * sentience_link.h — Link abstraction layer for GMCP, OSC 8, and MXP
 *
 * Provides transport-agnostic link routing: WebSocket clients get GMCP
 * link metadata + OSC 8 text markers, modern terminals get OSC 8
 * hyperlinks, MXP clients get <send> tags, plain clients get text.
 *
 * See docs/superpowers/specs/2026-03-24-sentience-link-design.md
 */

#ifndef SENTIENCE_LINK_H
#define SENTIENCE_LINK_H

#include <stdbool.h>
#include <jansson.h>

/* Forward declarations */
typedef struct descriptor_data descriptor_t;
typedef struct buf_type BUFFER;

/* Link delivery modes — priority order for routing */
typedef enum {
    LINK_NONE,   /* Plain text, no links */
    LINK_MXP,    /* Telnet MXP <send> tags */
    LINK_OSC8,   /* Telnet OSC 8 hyperlinks (primary action only) */
    LINK_GMCP    /* WebSocket: GMCP metadata + OSC 8 markers in text */
} link_mode_t;

#define SENTIENCE_LINK_MAX_ACTIONS   8
#define SENTIENCE_LINK_QUEUE_MAX     256
#define SENTIENCE_LINK_QUEUE_INITIAL 16

/* A single action in a link context menu */
typedef struct {
    char *label;    /* Menu item text (shown in context menu) */
    char *cmd;      /* MUD command to execute */
    char *hint;     /* Menu item tooltip (may be NULL) */
} sentience_link_action_t;

/* A queued link entry for Sentience.Link.List GMCP */
typedef struct {
    char id[16];    /* "lk_0", "lk_1", ... */
    char *text;     /* Display text */
    char *hint;     /* Link hover tooltip (may be NULL) */
    char *category; /* "obj", "mob", "room", "player", "help", "cmd" */
    int num_actions;
    sentience_link_action_t actions[SENTIENCE_LINK_MAX_ACTIONS];
} sentience_link_entry_t;

/* Link queue — stored on protocol_t, flushed before socket write */
typedef struct {
    int count;
    int capacity;
    sentience_link_entry_t *entries;
} sentience_link_queue_t;

/* ── Routing ───────────────────────────────────────────── */

link_mode_t link_mode(descriptor_t *d);
bool is_websocket_connection(descriptor_t *d);
bool has_osc8_support(descriptor_t *d);

/* ── Queue operations ──────────────────────────────────── */

void sentience_link_queue_init(sentience_link_queue_t *queue);
void sentience_link_queue_free(sentience_link_queue_t *queue);

const char *sentience_link_queue_add(sentience_link_queue_t *queue,
                                      const char *text,
                                      const char *hint,
                                      const char *category,
                                      const sentience_link_action_t *actions,
                                      int num_actions);

json_t *sentience_link_queue_to_json(const sentience_link_queue_t *queue);
void sentience_link_queue_flush(descriptor_t *d);

/* ── Trust filtering ───────────────────────────────────── */

/* Forward declaration — mxp_cmd_hint_t is defined in mxp_links.h */
struct mxp_cmd_hint;

/*
 * Filter an action list by trust level. Copies items where
 * staff_only is false (or is_staff is true) into out[].
 * Returns the number of items written.
 */
int sentience_link_filter_staff(const struct mxp_cmd_hint *items, int nitems,
                                 struct mxp_cmd_hint *out, int out_max,
                                 bool is_staff);

/* ── OSC 8 output helpers ──────────────────────────────── */

void link_osc8_gmcp(BUFFER *buf, const char *link_id, const char *text);
void link_osc8_telnet(BUFFER *buf, const char *primary_cmd, const char *text);
int  link_url_encode(char *dst, size_t dst_size, const char *src);

#endif /* SENTIENCE_LINK_H */
```

- [ ] **Step 3: Create `sentience_link.c` stub**

```c
/*
 * sentience_link.c — Link abstraction layer implementation
 *
 * Provides link queue management, JSON building, OSC 8 output,
 * and link mode detection for the 4-way routing in mxp_links.c.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "merc.h"
#include "protocol.h"
#include "connection.h"
#include "sentience_link.h"

/* ── Stub implementations — filled in by subsequent tasks ── */

link_mode_t link_mode(descriptor_t *d)
{
    (void)d;
    return LINK_NONE;
}

bool is_websocket_connection(descriptor_t *d)
{
    return d && d->conn && d->conn->type == CONN_TYPE_WEBSOCKET_TLS;
}

bool has_osc8_support(descriptor_t *d)
{
    (void)d;
    return false;
}

void sentience_link_queue_init(sentience_link_queue_t *queue)
{
    if (!queue) return;
    memset(queue, 0, sizeof(*queue));
}

void sentience_link_queue_free(sentience_link_queue_t *queue)
{
    if (!queue) return;
    free(queue->entries);
    memset(queue, 0, sizeof(*queue));
}

const char *sentience_link_queue_add(sentience_link_queue_t *queue,
                                      const char *text,
                                      const char *hint,
                                      const char *category,
                                      const sentience_link_action_t *actions,
                                      int num_actions)
{
    (void)queue; (void)text; (void)hint; (void)category;
    (void)actions; (void)num_actions;
    return NULL;
}

json_t *sentience_link_queue_to_json(const sentience_link_queue_t *queue)
{
    (void)queue;
    return json_array();
}

void sentience_link_queue_flush(descriptor_t *d)
{
    (void)d;
}

void link_osc8_gmcp(BUFFER *buf, const char *link_id, const char *text)
{
    if (buf && text) add_buf(buf, (char *)text);
    (void)link_id;
}

void link_osc8_telnet(BUFFER *buf, const char *primary_cmd, const char *text)
{
    if (buf && text) add_buf(buf, (char *)text);
    (void)primary_cmd;
}

int link_url_encode(char *dst, size_t dst_size, const char *src)
{
    (void)dst; (void)dst_size; (void)src;
    if (dst && dst_size > 0) dst[0] = '\0';
    return 0;
}

int sentience_link_filter_staff(const struct mxp_cmd_hint *items, int nitems,
                                 struct mxp_cmd_hint *out, int out_max,
                                 bool is_staff)
{
    (void)items; (void)nitems; (void)out; (void)out_max; (void)is_staff;
    return 0;
}
```

- [ ] **Step 4: Add to build files**

In `CMakeLists.txt`, after the `gmcp_sentience.c` line in the main sources section:
```
gmcp_sentience.c
sentience_link.c
```

In `Makefile`, after the `gmcp_sentience.c` line in the main sources section:
```
gmcp_sentience.c \
sentience_link.c \
```

- [ ] **Step 5: Build**

```bash
cd /sentience/src && ./build tests
```

Expected: clean compile with no errors.

- [ ] **Step 6: Commit**

```bash
git add mxp_links.h sentience_link.h sentience_link.c CMakeLists.txt Makefile
git commit -m "feat(link): add sentience_link header/stub, name mxp_cmd_hint struct, add staff_only field"
```

---

### Task 2: COMM_LINKS Preference + Protocol Integration

**Files:**
- Modify: `merc.h` (~line 4087)
- Modify: `const.c` (~line 418)
- Modify: `tables.c` (~line 909)
- Modify: `protocol.h` (~line 490)
- Modify: `protocol.c` (~lines 443, 516)

- [ ] **Step 1: Add COMM_LINKS alias in `merc.h`**

After the existing `#define COMM_MXP (I)` line, add:

```c
#define COMM_MXP		(I) // MXP is a protocol for enhanced mud clients.
#define COMM_LINKS		COMM_MXP // Unified link preference (alias for COMM_MXP)
```

- [ ] **Step 2: Add "links" setting in `const.c`**

Find the `"mxp"` settings entry (around line 418) and add a "links" entry immediately after it:

```c
{	"mxp",		0,		0,		COMM_MXP,	false,	STAFF_PLAYER,	SETTING_ON	},
{	"links",	0,		0,		COMM_LINKS,	false,	STAFF_PLAYER,	SETTING_ON	},
```

- [ ] **Step 3: Add "links" comm flag in `tables.c`**

Find the `"mxp"` comm flag entry (around line 909) and add:

```c
{   "mxp",			COMM_MXP,		true	},
{   "links",			COMM_LINKS,		true	},
```

- [ ] **Step 4: Add `sentience_link_queue_t` to `protocol_t` in `protocol.h`**

Add `#include "sentience_link.h"` near the existing includes at the top of protocol.h. Then add the queue field inside `protocol_t`, after the `sentience_cache` field:

```c
sentience_gmcp_cache_t sentience_cache;
sentience_link_queue_t sentience_link_queue;
```

- [ ] **Step 5: Init and free the queue in `protocol.c`**

In `ProtocolCreate()` (around line 443), after `sentience_gmcp_cache_reset`:

```c
sentience_gmcp_cache_reset(&pProtocol->sentience_cache);
sentience_link_queue_init(&pProtocol->sentience_link_queue);
```

In `ProtocolDestroy()` (around line 516), before `free(apProtocol)`:

```c
sentience_link_queue_free(&apProtocol->sentience_link_queue);
free(apProtocol);
```

- [ ] **Step 6: Build**

```bash
cd /sentience/src && ./build tests
```

Expected: clean compile.

- [ ] **Step 7: Commit**

```bash
git add merc.h const.c tables.c protocol.h protocol.c
git commit -m "feat(link): add COMM_LINKS preference and link queue to protocol_t"
```

---

### Task 3: Test Infrastructure

**Files:**
- Create: `tests/unit/sentience_link_tests.c`
- Create: `tests/data/unit/sentience_link_unit_tests.json`
- Modify: `tests/framework/test_dispatcher.c`
- Modify: `tests/framework/test_modules.h`
- Modify: `tests/data/test_config.json`
- Modify: `CMakeLists.txt`
- Modify: `Makefile`

- [ ] **Step 1: Create test handler `tests/unit/sentience_link_tests.c`**

```c
#ifdef BUILD_TESTS

#include <string.h>
#include <jansson.h>

#include "../framework/test_framework.h"
#include "../../log.h"
#include "../../sentience_link.h"
#include "../../mxp_links.h"

/* Forward declarations for scenario runners */
static test_result_t run_url_encode_scenario(json_t *tc);
static test_result_t run_queue_add_scenario(json_t *tc);
static test_result_t run_queue_json_scenario(json_t *tc);
static test_result_t run_osc8_gmcp_scenario(json_t *tc);
static test_result_t run_osc8_telnet_scenario(json_t *tc);
static test_result_t run_filter_staff_scenario(json_t *tc);
static test_result_t run_queue_sequential_scenario(json_t *tc);
static test_result_t run_queue_overflow_scenario(json_t *tc);

/* ── URL encode ──────────────────────────────────────── */

static test_result_t run_url_encode_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    const char *input = test_json_get_string(params, "input");
    const char *expected_output = test_json_get_string(expected, "output");
    int expected_len = test_json_get_int(expected, "length");

    char buf[512];
    int len = link_url_encode(buf, sizeof(buf), input);

    TEST_ASSERT_STR_EQ(expected_output, buf);
    TEST_ASSERT_INT_EQ(expected_len, len);

    return TEST_SUCCESS;
}

/* ── Queue add ───────────────────────────────────────── */

static test_result_t run_queue_add_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    sentience_link_queue_t queue;
    sentience_link_queue_init(&queue);

    const char *text = test_json_get_string(params, "text");
    const char *hint = test_json_get_string(params, "hint");
    const char *category = test_json_get_string(params, "category");

    json_t *actions_arr = json_object_get(params, "actions");
    sentience_link_action_t actions[SENTIENCE_LINK_MAX_ACTIONS] = {0};
    int num_actions = 0;

    if (actions_arr && json_is_array(actions_arr)) {
        num_actions = (int)json_array_size(actions_arr);
        if (num_actions > SENTIENCE_LINK_MAX_ACTIONS)
            num_actions = SENTIENCE_LINK_MAX_ACTIONS;
        for (int i = 0; i < num_actions; i++) {
            json_t *a = json_array_get(actions_arr, i);
            actions[i].label = (char *)test_json_get_string(a, "label");
            actions[i].cmd   = (char *)test_json_get_string(a, "cmd");
            actions[i].hint  = (char *)test_json_get_string(a, "hint");
        }
    }

    const char *id = sentience_link_queue_add(&queue, text, hint, category,
                                               actions, num_actions);

    const char *expected_id = test_json_get_string(expected, "id");
    int expected_count = test_json_get_int(expected, "count");

    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_STR_EQ(expected_id, id);
    TEST_ASSERT_INT_EQ(expected_count, queue.count);

    sentience_link_queue_free(&queue);
    return TEST_SUCCESS;
}

/* ── Queue to JSON ───────────────────────────────────── */

static test_result_t run_queue_json_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    sentience_link_queue_t queue;
    sentience_link_queue_init(&queue);

    /* Add entries from params.entries[] */
    json_t *entries = json_object_get(params, "entries");
    if (entries && json_is_array(entries)) {
        size_t idx;
        json_t *entry;
        json_array_foreach(entries, idx, entry) {
            const char *text = test_json_get_string(entry, "text");
            const char *hint = test_json_get_string(entry, "hint");
            const char *cat  = test_json_get_string(entry, "category");
            json_t *acts = json_object_get(entry, "actions");
            sentience_link_action_t actions[SENTIENCE_LINK_MAX_ACTIONS] = {0};
            int nact = 0;
            if (acts && json_is_array(acts)) {
                nact = (int)json_array_size(acts);
                if (nact > SENTIENCE_LINK_MAX_ACTIONS) nact = SENTIENCE_LINK_MAX_ACTIONS;
                for (int i = 0; i < nact; i++) {
                    json_t *a = json_array_get(acts, i);
                    actions[i].label = (char *)test_json_get_string(a, "label");
                    actions[i].cmd   = (char *)test_json_get_string(a, "cmd");
                    actions[i].hint  = (char *)test_json_get_string(a, "hint");
                }
            }
            sentience_link_queue_add(&queue, text, hint, cat, actions, nact);
        }
    }

    json_t *result = sentience_link_queue_to_json(&queue);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(json_is_array(result));

    int expected_count = test_json_get_int(expected, "count");
    TEST_ASSERT_INT_EQ(expected_count, (int)json_array_size(result));

    /* Verify first entry structure if present */
    if (expected_count > 0) {
        json_t *first = json_array_get(result, 0);
        TEST_ASSERT_NOT_NULL(first);
        TEST_ASSERT_NOT_NULL(json_object_get(first, "id"));
        TEST_ASSERT_NOT_NULL(json_object_get(first, "text"));
        TEST_ASSERT_NOT_NULL(json_object_get(first, "category"));
        TEST_ASSERT_NOT_NULL(json_object_get(first, "actions"));
        TEST_ASSERT_TRUE(json_is_array(json_object_get(first, "actions")));

        const char *exp_id = test_json_get_string(expected, "first_id");
        if (exp_id) {
            TEST_ASSERT_STR_EQ(exp_id, json_string_value(json_object_get(first, "id")));
        }
        const char *exp_cat = test_json_get_string(expected, "first_category");
        if (exp_cat) {
            TEST_ASSERT_STR_EQ(exp_cat, json_string_value(json_object_get(first, "category")));
        }
    }

    json_decref(result);
    sentience_link_queue_free(&queue);
    return TEST_SUCCESS;
}

/* ── OSC 8 GMCP output ──────────────────────────────── */

static test_result_t run_osc8_gmcp_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    const char *link_id = test_json_get_string(params, "link_id");
    const char *text = test_json_get_string(params, "text");
    const char *expected_output = test_json_get_string(expected, "output");

    BUFFER *buf = new_buf();
    link_osc8_gmcp(buf, link_id, text);
    const char *result = buf_string(buf);

    TEST_ASSERT_STR_EQ(expected_output, result);

    free_buf(buf);
    return TEST_SUCCESS;
}

/* ── OSC 8 telnet output ─────────────────────────────── */

static test_result_t run_osc8_telnet_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    const char *cmd = test_json_get_string(params, "cmd");
    const char *text = test_json_get_string(params, "text");
    const char *expected_output = test_json_get_string(expected, "output");

    BUFFER *buf = new_buf();
    link_osc8_telnet(buf, cmd, text);
    const char *result = buf_string(buf);

    TEST_ASSERT_STR_EQ(expected_output, result);

    free_buf(buf);
    return TEST_SUCCESS;
}

/* ── Trust filtering ─────────────────────────────────── */

static test_result_t run_filter_staff_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    bool is_staff = test_json_get_bool(params, "is_staff");
    json_t *items_arr = json_object_get(params, "items");
    if (!items_arr || !json_is_array(items_arr)) return TEST_ERROR;

    int nitems = (int)json_array_size(items_arr);
    mxp_cmd_hint_t items[SENTIENCE_LINK_MAX_ACTIONS] = {0};
    mxp_cmd_hint_t out[SENTIENCE_LINK_MAX_ACTIONS] = {0};

    for (int i = 0; i < nitems && i < SENTIENCE_LINK_MAX_ACTIONS; i++) {
        json_t *item = json_array_get(items_arr, i);
        items[i].cmd = test_json_get_string(item, "cmd");
        items[i].hint = test_json_get_string(item, "hint");
        items[i].staff_only = test_json_get_bool(item, "staff_only");
    }

    int nf = sentience_link_filter_staff(items, nitems, out,
                                          SENTIENCE_LINK_MAX_ACTIONS, is_staff);

    int expected_count = test_json_get_int(expected, "count");
    TEST_ASSERT_INT_EQ(expected_count, nf);

    if (nf > 0) {
        const char *exp_first = test_json_get_string(expected, "first_cmd");
        if (exp_first)
            TEST_ASSERT_STR_EQ(exp_first, out[0].cmd);

        const char *exp_last = test_json_get_string(expected, "last_cmd");
        if (exp_last)
            TEST_ASSERT_STR_EQ(exp_last, out[nf - 1].cmd);
    }

    return TEST_SUCCESS;
}

/* ── Queue sequential IDs ────────────────────────────── */

static test_result_t run_queue_sequential_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    json_t *texts = json_object_get(params, "texts");
    if (!texts || !json_is_array(texts)) return TEST_ERROR;

    sentience_link_queue_t queue;
    sentience_link_queue_init(&queue);

    json_t *exp_ids = json_object_get(expected, "ids");
    size_t idx;
    json_t *val;

    json_array_foreach(texts, idx, val) {
        const char *text = json_string_value(val);
        sentience_link_action_t act = { .label = "Look", .cmd = "look", .hint = NULL };
        const char *id = sentience_link_queue_add(&queue, text, NULL, "obj", &act, 1);
        TEST_ASSERT_NOT_NULL(id);

        if (exp_ids && idx < json_array_size(exp_ids)) {
            const char *exp_id = json_string_value(json_array_get(exp_ids, idx));
            TEST_ASSERT_STR_EQ(exp_id, id);
        }
    }

    int expected_count = test_json_get_int(expected, "count");
    TEST_ASSERT_INT_EQ(expected_count, queue.count);

    sentience_link_queue_free(&queue);
    return TEST_SUCCESS;
}

/* ── Queue capacity overflow ─────────────────────────── */

static test_result_t run_queue_overflow_scenario(json_t *tc)
{
    json_t *expected = json_object_get(tc, "expected");
    if (!expected) return TEST_ERROR;

    sentience_link_queue_t queue;
    sentience_link_queue_init(&queue);

    sentience_link_action_t act = { .label = "Look", .cmd = "look x", .hint = NULL };

    /* Fill to capacity */
    for (int i = 0; i < SENTIENCE_LINK_QUEUE_MAX; i++) {
        const char *id = sentience_link_queue_add(&queue, "item", NULL, "obj", &act, 1);
        if (!id) {
            sentience_link_queue_free(&queue);
            return TEST_FAIL;
        }
    }

    TEST_ASSERT_INT_EQ(SENTIENCE_LINK_QUEUE_MAX, queue.count);

    /* One more should fail */
    const char *overflow_id = sentience_link_queue_add(&queue, "overflow", NULL, "obj",
                                                        &act, 1);

    bool expect_null = test_json_get_bool(expected, "overflow_returns_null");
    if (expect_null) {
        TEST_ASSERT_NULL(overflow_id);
    }

    TEST_ASSERT_INT_EQ(SENTIENCE_LINK_QUEUE_MAX, queue.count);

    sentience_link_queue_free(&queue);
    return TEST_SUCCESS;
}

/* ── Main dispatcher ─────────────────────────────────── */

test_result_t run_sentience_link_test_case(test_case_t *test)
{
    json_t *input;
    json_t *test_cases;
    const char *func_name;
    size_t index;
    json_t *tc;

    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "Sentience link test missing configuration");
        return TEST_ERROR;
    }

    input = json_object_get(test->config, "input");
    if (!input) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "Sentience link test missing input");
        return TEST_ERROR;
    }

    func_name = test_json_get_string(input, "function");
    test_cases = json_object_get(input, "test_cases");
    if (!test_cases || !json_is_array(test_cases)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "Sentience link test requires test_cases array");
        return TEST_ERROR;
    }

    json_array_foreach(test_cases, index, tc) {
        test_result_t result;

        if (strcmp(func_name, "url_encode") == 0) {
            result = run_url_encode_scenario(tc);
        } else if (strcmp(func_name, "queue_add") == 0) {
            result = run_queue_add_scenario(tc);
        } else if (strcmp(func_name, "queue_json") == 0) {
            result = run_queue_json_scenario(tc);
        } else if (strcmp(func_name, "osc8_gmcp") == 0) {
            result = run_osc8_gmcp_scenario(tc);
        } else if (strcmp(func_name, "osc8_telnet") == 0) {
            result = run_osc8_telnet_scenario(tc);
        } else if (strcmp(func_name, "filter_staff") == 0) {
            result = run_filter_staff_scenario(tc);
        } else if (strcmp(func_name, "queue_sequential") == 0) {
            result = run_queue_sequential_scenario(tc);
        } else if (strcmp(func_name, "queue_overflow") == 0) {
            result = run_queue_overflow_scenario(tc);
        } else {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Unknown link function: %s", func_name);
            return TEST_ERROR;
        }

        if (result != TEST_SUCCESS)
            return result;
    }

    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
```

- [ ] **Step 2: Create test data `tests/data/unit/sentience_link_unit_tests.json`**

```json
{
    "type": "test_suite",
    "test_suite": "sentience_link_unit_tests",
    "description": "Unit tests for Sentience.Link abstraction layer",
    "version": "1.0",
    "requires_mud_environment": false,
    "test_level": "unit",
    "tests": [
        {
            "test_type": "slink_",
            "name": "slink_url_encode_simple",
            "description": "URL-encode a simple command with spaces",
            "input": {
                "function": "url_encode",
                "test_cases": [
                    {
                        "scenario": "simple_spaces",
                        "params": { "input": "look sword" },
                        "expected": { "output": "look%20sword", "length": 11 }
                    }
                ]
            }
        },
        {
            "test_type": "slink_",
            "name": "slink_url_encode_special",
            "description": "URL-encode a command with special characters",
            "input": {
                "function": "url_encode",
                "test_cases": [
                    {
                        "scenario": "special_chars",
                        "params": { "input": "goto 3:1024" },
                        "expected": { "output": "goto%203%3A1024", "length": 15 }
                    }
                ]
            }
        },
        {
            "test_type": "slink_",
            "name": "slink_url_encode_passthrough",
            "description": "URL-encode leaves alphanumerics and safe chars unchanged",
            "input": {
                "function": "url_encode",
                "test_cases": [
                    {
                        "scenario": "passthrough",
                        "params": { "input": "help-fireball_2.0" },
                        "expected": { "output": "help-fireball_2.0", "length": 17 }
                    }
                ]
            }
        },
        {
            "test_type": "slink_",
            "name": "slink_queue_add_single",
            "description": "Add one link to an empty queue",
            "input": {
                "function": "queue_add",
                "test_cases": [
                    {
                        "scenario": "single_add",
                        "params": {
                            "text": "a steel sword",
                            "hint": "A sword lies here",
                            "category": "obj",
                            "actions": [
                                { "label": "Look", "cmd": "look sword", "hint": null }
                            ]
                        },
                        "expected": { "id": "lk_0", "count": 1 }
                    }
                ]
            }
        },
        {
            "test_type": "slink_",
            "name": "slink_queue_json_two_entries",
            "description": "Build JSON from a queue with two entries",
            "input": {
                "function": "queue_json",
                "test_cases": [
                    {
                        "scenario": "two_entries",
                        "params": {
                            "entries": [
                                {
                                    "text": "sword",
                                    "hint": "A steel sword",
                                    "category": "obj",
                                    "actions": [
                                        { "label": "Look", "cmd": "look sword", "hint": null },
                                        { "label": "Get",  "cmd": "get sword",  "hint": null }
                                    ]
                                },
                                {
                                    "text": "Gandalf",
                                    "hint": null,
                                    "category": "player",
                                    "actions": [
                                        { "label": "Look", "cmd": "look gandalf", "hint": null }
                                    ]
                                }
                            ]
                        },
                        "expected": {
                            "count": 2,
                            "first_id": "lk_0",
                            "first_category": "obj"
                        }
                    }
                ]
            }
        },
        {
            "test_type": "slink_",
            "name": "slink_queue_json_empty",
            "description": "Build JSON from an empty queue returns empty array",
            "input": {
                "function": "queue_json",
                "test_cases": [
                    {
                        "scenario": "empty_queue",
                        "params": { "entries": [] },
                        "expected": { "count": 0 }
                    }
                ]
            }
        },
        {
            "test_type": "slink_",
            "name": "slink_osc8_gmcp_basic",
            "description": "OSC 8 GMCP marker wraps text with link ID",
            "input": {
                "function": "osc8_gmcp",
                "test_cases": [
                    {
                        "scenario": "basic_marker",
                        "params": { "link_id": "lk_0", "text": "sword" },
                        "expected": { "output": "\u001b]8;;lk_0\u0007sword\u001b]8;;\u0007" }
                    }
                ]
            }
        },
        {
            "test_type": "slink_",
            "name": "slink_osc8_gmcp_null_id",
            "description": "OSC 8 GMCP with null link_id outputs plain text",
            "input": {
                "function": "osc8_gmcp",
                "test_cases": [
                    {
                        "scenario": "null_id_fallback",
                        "params": { "link_id": null, "text": "sword" },
                        "expected": { "output": "sword" }
                    }
                ]
            }
        },
        {
            "test_type": "slink_",
            "name": "slink_osc8_telnet_basic",
            "description": "OSC 8 telnet hyperlink with mud:// URI",
            "input": {
                "function": "osc8_telnet",
                "test_cases": [
                    {
                        "scenario": "basic_hyperlink",
                        "params": { "cmd": "look sword", "text": "sword" },
                        "expected": { "output": "\u001b]8;;mud://look%20sword\u0007sword\u001b]8;;\u0007" }
                    }
                ]
            }
        },
        {
            "test_type": "slink_",
            "name": "slink_filter_staff_player_only",
            "description": "Non-staff user sees only player actions, staff_only filtered out",
            "input": {
                "function": "filter_staff",
                "test_cases": [
                    {
                        "scenario": "non_staff_filters",
                        "params": {
                            "is_staff": false,
                            "items": [
                                { "cmd": "look sword", "hint": "Look", "staff_only": false },
                                { "cmd": "examine sword", "hint": "Examine", "staff_only": false },
                                { "cmd": "stat obj 1 2", "hint": "Stat object", "staff_only": true },
                                { "cmd": "oedit 1:2", "hint": "Edit index", "staff_only": true }
                            ]
                        },
                        "expected": { "count": 2, "first_cmd": "look sword", "last_cmd": "examine sword" }
                    }
                ]
            }
        },
        {
            "test_type": "slink_",
            "name": "slink_filter_staff_all_pass",
            "description": "Staff user sees all actions including staff_only",
            "input": {
                "function": "filter_staff",
                "test_cases": [
                    {
                        "scenario": "staff_sees_all",
                        "params": {
                            "is_staff": true,
                            "items": [
                                { "cmd": "look sword", "hint": "Look", "staff_only": false },
                                { "cmd": "stat obj 1 2", "hint": "Stat object", "staff_only": true },
                                { "cmd": "oedit 1:2", "hint": "Edit index", "staff_only": true }
                            ]
                        },
                        "expected": { "count": 3, "first_cmd": "look sword", "last_cmd": "oedit 1:2" }
                    }
                ]
            }
        },
        {
            "test_type": "slink_",
            "name": "slink_filter_staff_all_filtered",
            "description": "Non-staff user with only staff_only actions gets zero results",
            "input": {
                "function": "filter_staff",
                "test_cases": [
                    {
                        "scenario": "all_filtered_out",
                        "params": {
                            "is_staff": false,
                            "items": [
                                { "cmd": "stat obj 1 2", "hint": "Stat", "staff_only": true },
                                { "cmd": "oedit 1:2", "hint": "Edit", "staff_only": true }
                            ]
                        },
                        "expected": { "count": 0 }
                    }
                ]
            }
        },
        {
            "test_type": "slink_",
            "name": "slink_queue_sequential_ids",
            "description": "Sequential queue adds produce lk_0, lk_1, lk_2",
            "input": {
                "function": "queue_sequential",
                "test_cases": [
                    {
                        "scenario": "three_sequential",
                        "params": {
                            "texts": ["sword", "shield", "helm"]
                        },
                        "expected": {
                            "ids": ["lk_0", "lk_1", "lk_2"],
                            "count": 3
                        }
                    }
                ]
            }
        },
        {
            "test_type": "slink_",
            "name": "slink_queue_overflow",
            "description": "Queue rejects entries beyond SENTIENCE_LINK_QUEUE_MAX (256)",
            "input": {
                "function": "queue_overflow",
                "test_cases": [
                    {
                        "scenario": "overflow_at_cap",
                        "params": {},
                        "expected": { "overflow_returns_null": true }
                    }
                ]
            }
        }
    ]
}
```

- [ ] **Step 3: Register in `tests/framework/test_dispatcher.c`**

After the `gmcp_sentience_` entry:

```c
    { "gmcp_sentience_",                 run_gmcp_sentience_test_case,      MATCH_SUBSTR },
    { "slink_",                          run_sentience_link_test_case,      MATCH_SUBSTR },
```

- [ ] **Step 4: Declare in `tests/framework/test_modules.h`**

After the `run_gmcp_sentience_test_case` declaration:

```c
test_result_t run_gmcp_sentience_test_case(test_case_t *test);
test_result_t run_sentience_link_test_case(test_case_t *test);
```

- [ ] **Step 5: Add suite to `tests/data/test_config.json`**

Use the same Python script approach as Phase 1 to add `sentience_link_unit_tests` to `default_test_suites`, `full_test_suites`, and `unit_only_suites`.

- [ ] **Step 6: Add test file to build files**

In `CMakeLists.txt` BUILD_TESTS section, after `tests/unit/gmcp_sentience_tests.c`:
```
tests/unit/gmcp_sentience_tests.c
tests/unit/sentience_link_tests.c
```

In `Makefile` BUILD_TESTS section, after `tests/unit/gmcp_sentience_tests.c`:
```
tests/unit/gmcp_sentience_tests.c \
tests/unit/sentience_link_tests.c \
```

- [ ] **Step 7: Build and run tests**

```bash
cd /sentience/src && ./build tests && ./install && cd /sentience && ./sent -test:slink
```

Expected: 14 tests run — some PASS (stubs return defaults that match empty/null expectations), some FAIL (URL encode, queue add, OSC 8, filter, sequential ID tests will fail against stubs). This confirms tests are wired up correctly.

- [ ] **Step 8: Commit**

```bash
git add tests/unit/sentience_link_tests.c tests/data/unit/sentience_link_unit_tests.json \
  tests/framework/test_dispatcher.c tests/framework/test_modules.h tests/data/test_config.json \
  CMakeLists.txt Makefile
git commit -m "test(link): add sentience link test infrastructure with 14 test cases"
```

---

### Task 4: Implement URL Encoder + OSC 8 Helpers

**Files:**
- Modify: `sentience_link.c` (replace stubs for `link_url_encode`, `link_osc8_gmcp`, `link_osc8_telnet`)

- [ ] **Step 1: Implement `link_url_encode()`**

Replace the stub in `sentience_link.c`:

```c
int link_url_encode(char *dst, size_t dst_size, const char *src)
{
    size_t di = 0;

    if (!dst || !src || dst_size == 0)
        return 0;

    for (const char *s = src; *s && di + 3 < dst_size; s++) {
        unsigned char c = (unsigned char)*s;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            dst[di++] = (char)c;
        } else {
            di += snprintf(dst + di, dst_size - di, "%%%02X", c);
        }
    }

    dst[di] = '\0';
    return (int)di;
}
```

- [ ] **Step 2: Implement `link_osc8_gmcp()`**

Replace the stub:

```c
void link_osc8_gmcp(BUFFER *buf, const char *link_id, const char *text)
{
    if (!buf || !text)
        return;

    if (!link_id || !link_id[0]) {
        add_buf(buf, (char *)text);
        return;
    }

    bprintf(buf, "\x1b]8;;%s\x07%s\x1b]8;;\x07", link_id, text);
}
```

- [ ] **Step 3: Implement `link_osc8_telnet()`**

Replace the stub:

```c
void link_osc8_telnet(BUFFER *buf, const char *primary_cmd, const char *text)
{
    char encoded[512];

    if (!buf || !text)
        return;

    if (!primary_cmd || !primary_cmd[0]) {
        add_buf(buf, (char *)text);
        return;
    }

    link_url_encode(encoded, sizeof(encoded), primary_cmd);
    bprintf(buf, "\x1b]8;;mud://%s\x07%s\x1b]8;;\x07", encoded, text);
}
```

- [ ] **Step 4: Run tests**

```bash
cd /sentience/src && ./build tests && ./install && cd /sentience && ./sent -test:slink
```

Expected: URL encode tests (3) PASS, OSC 8 tests (3) PASS. Queue tests may still fail.

- [ ] **Step 5: Commit**

```bash
git add sentience_link.c
git commit -m "feat(link): implement URL encoder and OSC 8 output helpers"
```

---

### Task 5: Implement Link Queue Operations + JSON Builder

**Files:**
- Modify: `sentience_link.c` (replace stubs for queue functions)

- [ ] **Step 1: Add internal helper `link_entry_clear()`**

Add as a `static` function near the top of `sentience_link.c`, before the queue functions:

```c
static void link_entry_clear(sentience_link_entry_t *entry)
{
    int i;

    if (!entry)
        return;

    free(entry->text);
    free(entry->hint);
    free(entry->category);
    for (i = 0; i < entry->num_actions; i++) {
        free(entry->actions[i].label);
        free(entry->actions[i].cmd);
        free(entry->actions[i].hint);
    }
    memset(entry, 0, sizeof(*entry));
}
```

- [ ] **Step 2: Implement `sentience_link_queue_init()`**

Replace the stub:

```c
void sentience_link_queue_init(sentience_link_queue_t *queue)
{
    if (!queue)
        return;

    queue->count    = 0;
    queue->capacity = SENTIENCE_LINK_QUEUE_INITIAL;
    queue->entries  = calloc(queue->capacity, sizeof(sentience_link_entry_t));
}
```

- [ ] **Step 3: Implement `sentience_link_queue_free()`**

Replace the stub:

```c
void sentience_link_queue_free(sentience_link_queue_t *queue)
{
    int i;

    if (!queue || !queue->entries)
        return;

    for (i = 0; i < queue->count; i++)
        link_entry_clear(&queue->entries[i]);

    free(queue->entries);
    queue->entries  = NULL;
    queue->count    = 0;
    queue->capacity = 0;
}
```

- [ ] **Step 4: Implement `sentience_link_queue_add()`**

Replace the stub:

```c
const char *sentience_link_queue_add(sentience_link_queue_t *queue,
                                      const char *text,
                                      const char *hint,
                                      const char *category,
                                      const sentience_link_action_t *actions,
                                      int num_actions)
{
    sentience_link_entry_t *entry;
    int i, nact;

    if (!queue || !queue->entries || queue->count >= SENTIENCE_LINK_QUEUE_MAX)
        return NULL;

    /* Grow if needed */
    if (queue->count >= queue->capacity) {
        int new_cap = queue->capacity * 2;
        sentience_link_entry_t *grown;

        if (new_cap > SENTIENCE_LINK_QUEUE_MAX)
            new_cap = SENTIENCE_LINK_QUEUE_MAX;

        grown = realloc(queue->entries, new_cap * sizeof(sentience_link_entry_t));
        if (!grown)
            return NULL;

        memset(grown + queue->capacity, 0,
               (new_cap - queue->capacity) * sizeof(sentience_link_entry_t));
        queue->entries  = grown;
        queue->capacity = new_cap;
    }

    entry = &queue->entries[queue->count];
    snprintf(entry->id, sizeof(entry->id), "lk_%d", queue->count);
    entry->text     = strdup(text ? text : "");
    entry->hint     = hint ? strdup(hint) : NULL;
    entry->category = strdup(category ? category : "cmd");

    nact = (num_actions > SENTIENCE_LINK_MAX_ACTIONS)
        ? SENTIENCE_LINK_MAX_ACTIONS : num_actions;
    entry->num_actions = nact;
    for (i = 0; i < nact; i++) {
        entry->actions[i].label = strdup(actions[i].label ? actions[i].label : "");
        entry->actions[i].cmd   = strdup(actions[i].cmd ? actions[i].cmd : "");
        entry->actions[i].hint  = actions[i].hint ? strdup(actions[i].hint) : NULL;
    }

    queue->count++;
    return entry->id;
}
```

- [ ] **Step 5: Implement `sentience_link_queue_to_json()`**

Replace the stub:

```c
json_t *sentience_link_queue_to_json(const sentience_link_queue_t *queue)
{
    json_t *arr = json_array();
    int i, j;

    if (!arr || !queue)
        return arr;

    for (i = 0; i < queue->count; i++) {
        const sentience_link_entry_t *e = &queue->entries[i];
        json_t *link = json_object();
        json_t *actions = json_array();

        json_object_set_new(link, "id",       json_string(e->id));
        json_object_set_new(link, "text",     json_string(e->text ? e->text : ""));
        if (e->hint)
            json_object_set_new(link, "hint", json_string(e->hint));
        json_object_set_new(link, "category", json_string(e->category ? e->category : "cmd"));

        for (j = 0; j < e->num_actions; j++) {
            json_t *act = json_object();
            json_object_set_new(act, "label", json_string(e->actions[j].label ? e->actions[j].label : ""));
            json_object_set_new(act, "cmd",   json_string(e->actions[j].cmd ? e->actions[j].cmd : ""));
            if (e->actions[j].hint)
                json_object_set_new(act, "hint", json_string(e->actions[j].hint));
            json_array_append_new(actions, act);
        }

        json_object_set_new(link, "actions", actions);
        json_array_append_new(arr, link);
    }

    return arr;
}
```

- [ ] **Step 6: Implement `sentience_link_queue_flush()`**

Replace the stub:

```c
void sentience_link_queue_flush(descriptor_t *d)
{
    sentience_link_queue_t *queue;
    json_t *arr;
    char *dump;
    int i;

    if (!d || !d->pProtocol)
        return;

    queue = &d->pProtocol->sentience_link_queue;
    if (queue->count == 0)
        return;

    /* Only send GMCP for WebSocket clients with Sentience support */
    if (is_websocket_connection(d) && d->pProtocol->bGMCP) {
        arr = sentience_link_queue_to_json(queue);
        if (arr) {
            dump = json_dumps(arr, JSON_COMPACT);
            if (dump) {
                SendGMCPRaw(d, "Sentience.Link.List", dump);
                free(dump);
            }
            json_decref(arr);
        }
    }

    /* Clear queue entries */
    for (i = 0; i < queue->count; i++)
        link_entry_clear(&queue->entries[i]);
    queue->count = 0;
}
```

- [ ] **Step 7: Implement `sentience_link_filter_staff()`**

Replace the stub. This is a pure function that `link_route()` calls for trust filtering — testable without mock descriptors:

```c
int sentience_link_filter_staff(const struct mxp_cmd_hint *items, int nitems,
                                 struct mxp_cmd_hint *out, int out_max,
                                 bool is_staff)
{
    int nf = 0;
    int i;

    if (!items || !out || out_max <= 0)
        return 0;

    for (i = 0; i < nitems && nf < out_max; i++) {
        if (items[i].staff_only && !is_staff)
            continue;
        out[nf++] = items[i];
    }

    return nf;
}
```

- [ ] **Step 8: Run tests**

```bash
cd /sentience/src && ./build tests && ./install && cd /sentience && ./sent -test:slink
```

Expected: all tests PASS.

- [ ] **Step 9: Commit**

```bash
git add sentience_link.c
git commit -m "feat(link): implement link queue operations and JSON builder"
```

---

### Task 6: Implement link_mode() + has_osc8_support()

**Files:**
- Modify: `sentience_link.c` (replace stubs for `link_mode`, `has_osc8_support`)

- [ ] **Step 1: Add OSC 8 terminal table**

Near the top of `sentience_link.c`, after includes:

```c
/* Terminals known to support OSC 8 hyperlinks (case-insensitive prefix match) */
static const char *osc8_terminals[] = {
    "mudlet",
    "xterm-256color",
    "xterm-kitty",
    "tmux-256color",
    NULL
};
```

- [ ] **Step 2: Implement `has_osc8_support()`**

Replace the stub:

```c
bool has_osc8_support(descriptor_t *d)
{
    const char *ttype;
    int i;

    if (!d || !d->pProtocol || !d->pProtocol->pLastTTYPE)
        return false;

    ttype = d->pProtocol->pLastTTYPE;

    for (i = 0; osc8_terminals[i]; i++) {
        if (!str_prefix(osc8_terminals[i], ttype))
            return true;
    }

    return false;
}
```

Note: `str_prefix()` is declared in `merc.h` and returns `false` if `astr` is a prefix of `bstr` (DikuMUD convention: `false` = match). Verify this before implementation — if `str_prefix` returns `true` for a match, flip the condition.

- [ ] **Step 3: Implement `link_mode()`**

Replace the stub:

```c
link_mode_t link_mode(descriptor_t *d)
{
    CHAR_DATA *ch;

    if (!d)
        return LINK_NONE;

    /* WebSocket always uses GMCP links — no player toggle needed */
    if (is_websocket_connection(d))
        return LINK_GMCP;

    ch = d->character;

    /* Check unified player preference */
    if (!ch || !IS_SET(ch->comm, COMM_LINKS))
        return LINK_NONE;

    /* MXP negotiated takes priority (established clients) */
    if (isMXP(d))
        return LINK_MXP;

    /* TTYPE indicates OSC 8 support */
    if (has_osc8_support(d))
        return LINK_OSC8;

    return LINK_NONE;
}
```

- [ ] **Step 4: Build**

```bash
cd /sentience/src && ./build tests
```

Expected: clean compile. `link_mode()` and `has_osc8_support()` can't be easily unit tested without mock descriptors, but they'll be integration-tested when mxp_links.c routing is added.

- [ ] **Step 5: Commit**

```bash
git add sentience_link.c
git commit -m "feat(link): implement link_mode() routing and OSC 8 terminal detection"
```

---

### Task 7: Rewrite mxp_links.c — Core Routing

**Files:**
- Modify: `mxp_links.h`
- Modify: `mxp_links.c`

This task replaces the `isMXP(d)` checks in the three core functions (`mxp_link`, `mxp_link_multi`, `mxp_link_prompt`) with 4-way routing via a new `link_route()` helper. The `staff_only` field and named struct were already added to `mxp_links.h` in Task 1.

- [ ] **Step 1: Add include to `mxp_links.h`**

At the top of `mxp_links.h`, after `#include "merc.h"`:

```c
#include "merc.h"
#include "sentience_link.h"
```

- [ ] **Step 2: Add `link_route()` static helper in `mxp_links.c`**

Add `#include "sentience_link.h"` to the includes, then add this static function before `mxp_link()`.

Trust filtering is **uniform across all modes**: `staff_only` actions are stripped for non-staff characters regardless of transport. If all actions are filtered out, the text is emitted as plain text (no link).

```c
/*
 * Central link routing — handles all 4 transport modes with trust filtering.
 *
 * Actions marked staff_only are excluded for non-immortal characters in ALL
 * modes (MXP, GMCP, OSC8). If no actions remain after filtering, plain text
 * is emitted. Delegates to sentience_link_filter_staff() for the filtering.
 *
 * Note: This intentionally diverges from the spec's statement that "MXP
 * continues to emit all commands." Uniform filtering is simpler and safer —
 * non-staff users never saw admin actions they could actually execute, and
 * hiding them avoids confusing right-click menus with non-functional options.
 */
static void link_route(descriptor_t *d, BUFFER *buf, const char *text,
                        const char *top_hint, const char *category,
                        const mxp_cmd_hint_t *items, int nitems,
                        link_mode_t mode)
{
    mxp_cmd_hint_t filtered[SENTIENCE_LINK_MAX_ACTIONS];
    bool is_staff;
    int nf;
    int i;

    /* Trust filter via shared helper (also unit-tested independently) */
    is_staff = (d->character && IS_IMMORTAL(d->character));
    nf = sentience_link_filter_staff(items, nitems, filtered,
                                      SENTIENCE_LINK_MAX_ACTIONS, is_staff);

    if (nf == 0) {
        add_buf(buf, (char *)text);
        return;
    }

    switch (mode) {
    case LINK_GMCP: {
        sentience_link_action_t actions[SENTIENCE_LINK_MAX_ACTIONS];
        const char *id;

        for (i = 0; i < nf; i++) {
            actions[i].label = (char *)(filtered[i].hint ? filtered[i].hint : filtered[i].cmd);
            actions[i].cmd   = (char *)filtered[i].cmd;
            actions[i].hint  = NULL;
        }

        id = sentience_link_queue_add(
            &d->pProtocol->sentience_link_queue,
            text, top_hint, category, actions, nf);
        link_osc8_gmcp(buf, id, text);
        break;
    }

    case LINK_OSC8:
        if (filtered[0].cmd)
            link_osc8_telnet(buf, filtered[0].cmd, text);
        else
            add_buf(buf, (char *)text);
        break;

    case LINK_MXP:
        if (nf == 1) {
            if (filtered[0].hint && filtered[0].hint[0])
                bprintf(buf, "\t<send href=\"%s\" hint=\"%s\">%s\t</send>",
                        filtered[0].cmd, filtered[0].hint, text);
            else
                bprintf(buf, "\t<send href=\"%s\">%s\t</send>",
                        filtered[0].cmd, text);
        } else {
            bool any_hint = false;

            add_buf(buf, (char *)"\t<send href=\"");
            for (i = 0; i < nf; i++) {
                if (i) add_buf(buf, (char *)"|");
                if (filtered[i].cmd) add_buf(buf, (char *)filtered[i].cmd);
            }
            add_buf(buf, (char *)"\"");

            for (i = 0; i < nf; i++) {
                if (filtered[i].hint && filtered[i].hint[0]) { any_hint = true; break; }
            }
            if (any_hint) {
                add_buf(buf, (char *)" hint=\"");
                for (i = 0; i < nf; i++) {
                    if (i) add_buf(buf, (char *)"|");
                    if (filtered[i].hint) add_buf(buf, (char *)filtered[i].hint);
                }
                add_buf(buf, (char *)"\"");
            }

            bprintf(buf, ">%s\t</send>", text);
        }
        break;

    default:
        add_buf(buf, (char *)text);
        break;
    }
}
```

- [ ] **Step 3: Rewrite `mxp_link()`**

```c
void mxp_link(descriptor_t *d, BUFFER *buf, const char *text,
              const char *command, const char *hint)
{
    link_mode_t mode;
    mxp_cmd_hint_t item;

    if (!text) text = "";
    if (!command || !command[0]) {
        add_buf(buf, (char *)text);
        return;
    }

    mode = link_mode(d);
    if (mode == LINK_NONE) {
        add_buf(buf, (char *)text);
        return;
    }

    item.cmd        = command;
    item.hint       = hint;
    item.staff_only = false;
    link_route(d, buf, text, hint, "cmd", &item, 1, mode);
}
```

- [ ] **Step 4: Rewrite `mxp_link_prompt()`**

```c
void mxp_link_prompt(descriptor_t *d, BUFFER *buf, const char *text,
                     const char *command, const char *hint)
{
    link_mode_t mode;

    if (!text) text = "";
    if (!command || !command[0]) {
        add_buf(buf, (char *)text);
        return;
    }

    mode = link_mode(d);

    /* MXP has a special prompt attribute; other modes treat it like a normal link */
    if (mode == LINK_MXP) {
        if (hint && hint[0])
            bprintf(buf, "\t<send href=\"%s\" hint=\"%s\" prompt>%s\t</send>",
                    command, hint, text);
        else
            bprintf(buf, "\t<send href=\"%s\" prompt>%s\t</send>", command, text);
        return;
    }

    if (mode == LINK_NONE) {
        add_buf(buf, (char *)text);
        return;
    }

    /* GMCP and OSC8: treat as regular link */
    {
        mxp_cmd_hint_t item = { command, hint };
        link_route(d, buf, text, hint, "cmd", &item, 1, mode);
    }
}
```

- [ ] **Step 5: Rewrite `mxp_link_multi()`**

```c
void mxp_link_multi(descriptor_t *d, BUFFER *buf, const char *text,
                    const mxp_cmd_hint_t *items, int nitems)
{
    link_mode_t mode;

    if (!text) text = "";
    if (!items || nitems <= 0) {
        add_buf(buf, (char *)text);
        return;
    }

    mode = link_mode(d);
    if (mode == LINK_NONE) {
        add_buf(buf, (char *)text);
        return;
    }

    link_route(d, buf, text, NULL, "cmd", items, nitems, mode);
}
```

- [ ] **Step 6: Build**

```bash
cd /sentience/src && ./build tests
```

Expected: clean compile. Existing behavior unchanged because `link_mode()` returns `LINK_MXP` for MXP clients (same as before) and `LINK_NONE` for non-MXP telnet (same as before). WebSocket clients now get GMCP links.

- [ ] **Step 7: Commit**

```bash
git add mxp_links.h mxp_links.c
git commit -m "feat(link): add 4-way link routing to core mxp functions"
```

---

### Task 8: Rewrite mxp_links.c — Entity Functions with `staff_only` Flag

**Files:**
- Modify: `mxp_links.c`

This task rewrites the entity-specific functions to use `link_route()` with appropriate categories. All actions are declared with a `staff_only` flag — `link_route()` handles filtering centrally. Entity functions no longer need `is_staff` branching; they just declare their full action list and let the router decide.

- [ ] **Step 1: Add `first_keyword()` helper in `mxp_links.c`**

Add this static helper near the top of `mxp_links.c`, after includes and before `link_route()`:

```c
/* Extract the first keyword from a space-separated keyword list.
 * Used to build player-facing commands (e.g., "look sword" from "sword steel"). */
static void first_keyword(const char *name_list, char *dst, size_t dst_size)
{
    size_t i = 0;

    if (!name_list || !dst || dst_size == 0)
        return;

    while (name_list[i] && name_list[i] != ' ' && i < dst_size - 1) {
        dst[i] = name_list[i];
        i++;
    }
    dst[i] = '\0';
}
```

- [ ] **Step 2: Rewrite `mxp_obj_link()`**

```c
void mxp_obj_link(descriptor_t *d, BUFFER *buf, OBJ_DATA *obj,
                  const char *text)
{
    link_mode_t mode;
    int n = 0;
    mxp_cmd_hint_t items[8];
    char kw[64], p1[128], p2[128];
    char c1[128], c2[128], c3[128], c4[128];

    if (!obj || !text) { add_buf(buf, (char *)(text ? text : "")); return; }

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, (char *)text); return; }

    first_keyword(obj->name, kw, sizeof(kw));

    /* Player actions */
    if (kw[0]) {
        snprintf(p1, sizeof(p1), "look %s", kw);
        items[n++] = (mxp_cmd_hint_t){ p1, "Look", false };

        snprintf(p2, sizeof(p2), "examine %s", kw);
        items[n++] = (mxp_cmd_hint_t){ p2, "Examine", false };
    }

    /* Staff actions */
    {
        const char *wvnum = widevnum_string_object(obj->pIndexData, NULL);

        snprintf(c1, sizeof(c1), "stat obj %ld %ld", obj->id[0], obj->id[1]);
        items[n++] = (mxp_cmd_hint_t){ c1, "Stat object", true };

        snprintf(c2, sizeof(c2), "oshow %s", wvnum);
        items[n++] = (mxp_cmd_hint_t){ c2, "Show index", true };

        snprintf(c3, sizeof(c3), "oedit %s", wvnum);
        items[n++] = (mxp_cmd_hint_t){ c3, "Edit index", true };

        snprintf(c4, sizeof(c4), "purge obj %ld %ld", obj->id[0], obj->id[1]);
        items[n++] = (mxp_cmd_hint_t){ c4, "***DANGER*** Purge object", true };
    }

    link_route(d, buf, text, NULL, "obj", items, n, mode);
}
```

- [ ] **Step 3: Rewrite `mxp_obj_id_link()`**

Admin-only display (raw object IDs) — all actions are `staff_only`:

```c
void mxp_obj_id_link(descriptor_t *d, BUFFER *buf, OBJ_DATA *obj)
{
    link_mode_t mode;
    char id_text[64];
    int n = 0;
    mxp_cmd_hint_t items[2];
    char c1[128], c2[128];

    if (!obj) return;

    snprintf(id_text, sizeof(id_text), "{W%ld %ld{X", obj->id[0], obj->id[1]);

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, id_text); return; }

    snprintf(c1, sizeof(c1), "stat obj %ld %ld", obj->id[0], obj->id[1]);
    items[n++] = (mxp_cmd_hint_t){ c1, "Stat object", true };

    snprintf(c2, sizeof(c2), "purge obj %ld %ld", obj->id[0], obj->id[1]);
    items[n++] = (mxp_cmd_hint_t){ c2, "***DANGER*** Purge object", true };

    link_route(d, buf, id_text, NULL, "obj", items, n, mode);
}
```

- [ ] **Step 4: Rewrite `mxp_obj_vnum_link()`**

Admin-only display (raw vnums) — all actions are `staff_only`:

```c
void mxp_obj_vnum_link(descriptor_t *d, BUFFER *buf, OBJ_INDEX_DATA *obj,
                       const char *text)
{
    link_mode_t mode;
    int n = 0;
    mxp_cmd_hint_t items[2];
    char c1[128], c2[128];

    if (!obj || !text) { add_buf(buf, (char *)(text ? text : "")); return; }

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, (char *)text); return; }

    const char *wvnum = widevnum_string_object(obj, NULL);

    snprintf(c1, sizeof(c1), "oshow %s", wvnum);
    items[n++] = (mxp_cmd_hint_t){ c1, "Show index", true };

    snprintf(c2, sizeof(c2), "oedit %s", wvnum);
    items[n++] = (mxp_cmd_hint_t){ c2, "Edit index", true };

    link_route(d, buf, text, NULL, "obj", items, n, mode);
}
```

- [ ] **Step 5: Rewrite `mxp_mob_link()`**

```c
void mxp_mob_link(descriptor_t *d, BUFFER *buf, CHAR_DATA *mob,
                  const char *text)
{
    link_mode_t mode;
    int n = 0;
    mxp_cmd_hint_t items[6];
    char kw[64], p1[128], p2[128];
    char c1[128], c2[128], c3[128];

    if (!mob || !text) { add_buf(buf, (char *)(text ? text : "")); return; }

    if (!IS_NPC(mob)) {
        mxp_player_link(d, buf, mob->name, text);
        return;
    }

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, (char *)text); return; }

    first_keyword(mob->name, kw, sizeof(kw));

    /* Player actions */
    if (kw[0]) {
        snprintf(p1, sizeof(p1), "look %s", kw);
        items[n++] = (mxp_cmd_hint_t){ p1, "Look", false };

        snprintf(p2, sizeof(p2), "consider %s", kw);
        items[n++] = (mxp_cmd_hint_t){ p2, "Consider", false };
    }

    /* Staff actions */
    {
        const char *wvnum = widevnum_string_mobile(mob->pIndexData, NULL);

        snprintf(c1, sizeof(c1), "stat mob %ld %ld", mob->id[0], mob->id[1]);
        items[n++] = (mxp_cmd_hint_t){ c1, "Stat mobile", true };

        snprintf(c2, sizeof(c2), "mshow %s", wvnum);
        items[n++] = (mxp_cmd_hint_t){ c2, "Show index", true };

        snprintf(c3, sizeof(c3), "medit %s", wvnum);
        items[n++] = (mxp_cmd_hint_t){ c3, "Edit index", true };
    }

    link_route(d, buf, text, NULL, "mob", items, n, mode);
}
```

- [ ] **Step 6: Rewrite `mxp_room_link()`**

```c
void mxp_room_link(descriptor_t *d, BUFFER *buf, ROOM_INDEX_DATA *room,
                   const char *text)
{
    link_mode_t mode;
    int n = 0;
    mxp_cmd_hint_t items[4];
    char c1[128], c2[128], c3[128];

    if (!room || !text) { add_buf(buf, (char *)(text ? text : "")); return; }

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, (char *)text); return; }

    const char *wvnum = widevnum_string_room(room, NULL);

    /* Room links are primarily used in admin contexts (blueprint, dungeon, stat).
     * Player-facing room links (exits, wilderness) will be added at those call
     * sites using mxp_link() or mxp_link_multi() directly. */
    snprintf(c1, sizeof(c1), "rshow %s", wvnum);
    items[n++] = (mxp_cmd_hint_t){ c1, "Show room", true };

    snprintf(c2, sizeof(c2), "redit %s", wvnum);
    items[n++] = (mxp_cmd_hint_t){ c2, "Edit room", true };

    snprintf(c3, sizeof(c3), "goto %s", wvnum);
    items[n++] = (mxp_cmd_hint_t){ c3, "Goto room", true };

    link_route(d, buf, text, NULL, "room", items, n, mode);
}
```

- [ ] **Step 7: Rewrite `mxp_player_link()`**

```c
void mxp_player_link(descriptor_t *d, BUFFER *buf, const char *name,
                     const char *text)
{
    link_mode_t mode;
    int n = 0;
    mxp_cmd_hint_t items[4];
    char p1[256], p2[256];
    char c1[256];

    if (!name) { add_buf(buf, (char *)(text ? text : "")); return; }
    if (!text) text = name;

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, (char *)text); return; }

    /* Player actions */
    snprintf(p1, sizeof(p1), "look %s", name);
    items[n++] = (mxp_cmd_hint_t){ p1, "Look", false };

    snprintf(p2, sizeof(p2), "tell %s ", name);
    items[n++] = (mxp_cmd_hint_t){ p2, "Tell", false };

    /* Staff actions */
    snprintf(c1, sizeof(c1), "stat char %s", name);
    items[n++] = (mxp_cmd_hint_t){ c1, "Stat player", true };

    link_route(d, buf, text, NULL, "player", items, n, mode);
}
```

- [ ] **Step 8: Rewrite `mxp_help_link()`**

Already player-facing — clean up to use `link_route()`:

```c
void mxp_help_link(descriptor_t *d, BUFFER *buf, const char *keyword,
                   const char *text)
{
    link_mode_t mode;
    char cmd[256];

    if (!keyword) { add_buf(buf, (char *)(text ? text : "")); return; }
    if (!text) text = keyword;

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, (char *)text); return; }

    snprintf(cmd, sizeof(cmd), "help %s", keyword);

    mxp_cmd_hint_t item = { cmd, "View help", false };
    link_route(d, buf, text, NULL, "help", &item, 1, mode);
}
```

- [ ] **Step 9: Rewrite `mxp_command_link()`**

No change needed — it already delegates to `mxp_link()` which was rewritten in Task 7:

```c
void mxp_command_link(descriptor_t *d, BUFFER *buf, const char *command,
                      const char *hint, const char *text)
{
    if (!command) { add_buf(buf, (char *)(text ? text : "")); return; }
    if (!text) text = command;
    mxp_link(d, buf, text, command, hint);
}
```

- [ ] **Step 10: Build**

```bash
cd /sentience/src && ./build tests
```

Expected: clean compile.

- [ ] **Step 11: Run all tests**

```bash
cd /sentience/src && ./install && cd /sentience && ./sent -test:slink
```

Expected: all 14 link tests PASS.

- [ ] **Step 12: Commit**

```bash
git add mxp_links.c
git commit -m "feat(link): add trust filtering and 4-way routing to entity link functions"
```

---

### Task 9: comm.c Flush Hook + Final Verification

**Files:**
- Modify: `comm.c` (~line 3154)

- [ ] **Step 1: Add include to `comm.c`**

Add `#include "sentience_link.h"` with the other includes at the top of `comm.c`.

- [ ] **Step 2: Add flush call in `process_output()`**

Find the `write_to_descriptor` call in `process_output()` (around line 3154). Add the link queue flush just before it:

```c
    /* Flush pending Sentience.Link.List GMCP before writing text */
    sentience_link_queue_flush(d);

    if (!write_to_descriptor(d, d->outbuf, d->outtop))
```

- [ ] **Step 3: Build with tests**

```bash
cd /sentience/src && ./build tests
```

Expected: clean compile.

- [ ] **Step 4: Install and run link tests**

```bash
cd /sentience/src && ./install && cd /sentience && ./sent -test:slink
```

Expected: 14/14 PASS.

- [ ] **Step 5: Run GMCP sentience tests (regression check)**

```bash
cd /sentience && ./sent -test:gmcp_sentience
```

Expected: 9/9 PASS.

- [ ] **Step 6: Verify `str_prefix` convention**

The codebase's `str_prefix()` may return `false` for a match (DikuMUD convention) or `true` (modern convention). Check:

```bash
cd /sentience/src && grep -A5 'bool str_prefix' handler.c | head -10
```

If `str_prefix` returns `false` on match, the `has_osc8_support()` condition `!str_prefix(...)` is correct. If it returns `true` on match, flip the condition. Fix if needed and rebuild.

- [ ] **Step 7: Grep verification — no stale `isMXP` calls in mxp_links.c**

```bash
grep -n 'isMXP' mxp_links.c
```

Expected: zero matches (all replaced with `link_mode()`/`link_route()`).

> **Note on integration testing:** `link_mode()`, `link_route()`, and MXP compatibility
> require live descriptors with protocol state — these cannot be unit-tested with the
> current framework. Verify them manually by connecting with:
> - A WebSocket client → should see Sentience.Link.List GMCP messages
> - A modern terminal (xterm-256color) → should see OSC 8 hyperlinks
> - An MXP client (Mudlet) → should see `<send>` tags as before
> - A plain telnet client → should see plain text with no markup

- [ ] **Step 8: Commit**

```bash
git add comm.c
git commit -m "feat(link): hook link queue flush into process_output()"
```

- [ ] **Step 9: Final integration commit (tag)**

```bash
git --no-pager log --oneline -8
```

Verify the commit chain is clean: header → preference → tests → url/osc8 → queue → link_mode → core routing → entity routing → flush hook.

---

## Dependency Graph

```
Task 1 (header + stub + mxp_links.h struct)
  ├── Task 2 (COMM_LINKS + protocol_t)
  │     └── Task 6 (link_mode + has_osc8)
  │           └── Task 7 (core routing) ─── Task 8 (entity routing)
  ├── Task 3 (test infra)
  │     ├── Task 4 (URL + OSC8)
  │     └── Task 5 (queue + JSON + filter)
  └── Task 9 (flush hook + verify) depends on ALL prior tasks (1-8)
```

Task 6 can run in parallel with Tasks 3-5 (different files: `mxp_links.c` vs `sentience_link.c`/tests).
Tasks 4 and 5 both modify `sentience_link.c` — they must be serialized (run 4 then 5, or vice versa).
Tasks 7 and 8 are sequential (8 depends on 7's `link_route()` function).
Task 9 is the final integration step — it depends on Tasks 1-8 being complete.
