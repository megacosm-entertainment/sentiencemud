# GMCP Phase 1 — Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the Sentience.* GMCP package foundation — new package negotiation, six character/room packages with Jansson-built JSON, dirty-flag caching, game loop integration, and WebSocket path replacement — plus rename `Core.Resume` → `Sentience.Auth.Resume`.

**Architecture:** New `gmcp_sentience.c/.h` module alongside existing GMCP infrastructure. Serializer functions take explicit parameters (testable without game state) and build JSON via Jansson. A per-descriptor cache struct tracks previous values; dirty bitmask triggers selective sends. Legacy `Char.*`/`Room.Info` packages remain for existing telnet clients; new `Sentience.*` packages sent to clients that negotiate `Sentience 1` or connect via WebSocket. The `Core.Resume` rename is a standalone string replacement across `comm.c` and `nanny.c`.

**Tech Stack:** C11, Jansson (JSON), existing test framework (JSON-driven test cases + C handlers)

**Build/Test commands:**
```bash
cd /sentience/src && ./build tests          # Build with test support
cd /sentience && ./sent -test               # Run all tests
cd /sentience && ./sent -test:gmcp          # Run GMCP-specific tests
cd /sentience/src && ./build                # Build without tests (normal)
```

**Baseline:** 432 tests, 418 pass, 1 known failure, 13 skipped. No regressions allowed.

---

## File Structure

### New Files
| File | Responsibility |
|------|----------------|
| `gmcp_sentience.h` | Dirty-flag bitmask enum, cache struct, builder function declarations, update entry point |
| `gmcp_sentience.c` | Package serialization (Jansson JSON builders), dirty-flag comparison, `sentience_gmcp_update()` |
| `tests/unit/gmcp_sentience_tests.c` | Unit test handler for GMCP serialization functions |
| `tests/data/unit/gmcp_sentience_unit_tests.json` | JSON test cases for each package serializer |

### Modified Files
| File | Changes |
|------|---------|
| `protocol.h` | Add `GMCP_SUPPORT_SENTIENCE` to enum, add `sentience_gmcp_cache_t` and `sentience_dirty` to `protocol_t` |
| `protocol.c` | Update `bGMCPSupportTable`, `ProtocolCreate()` init, `ParseGMCP()` Sentience negotiation |
| `protocol_websocket.c` | Auto-enable Sentience support, stop sending `Char.Status` |
| `update.c` | Call `sentience_gmcp_update()` from `gmcp_update()` |
| `comm.c` | Rename `Core.Resume` → `Sentience.Auth.Resume` (4 locations) |
| `nanny.c` | Rename `Core.Resume` → `Sentience.Auth.Resume` (1 location) |
| `tests/framework/test_dispatcher.c` | Register `gmcp_` test handler |
| `tests/framework/test_modules.h` | Declare `run_gmcp_sentience_test_case()` |
| `tests/data/test_config.json` | Add `gmcp_sentience_unit_tests` suite |
| `CMakeLists.txt` | Add `gmcp_sentience.c` and test file |
| `Makefile` | Add `gmcp_sentience.c` and test file |

---

## Task 1: Rename Core.Resume → Sentience.Auth.Resume

**Files:**
- Modify: `comm.c:634,645,856,908`
- Modify: `nanny.c:82`

This is a standalone string replacement — no behavioral changes.

- [ ] **Step 1: Replace all Core.Resume strings in comm.c**

In `comm.c`, change all four occurrences of `"Core.Resume"` to `"Sentience.Auth.Resume"`:

```c
// Line 634 — ws_resume_apply_failure()
write_to_buffer(d, "Sentience.Auth.Resume {\"event\":\"fail\",\"reason\":\"invalid_or_expired\"}\n\r", 0);

// Line 645 — ws_resume_attach_character()
write_to_buffer(d, "Sentience.Auth.Resume {\"event\":\"ok\"}\n\r", 0);

// Line 856 — websocket_resume_issue()
"Sentience.Auth.Resume {\"event\":\"token\",\"token\":\"%s\",\"ttl\":%d}\n\r",

// Line 908 — websocket_resume_try()
write_to_buffer(d, "Sentience.Auth.Resume {\"event\":\"pending\"}\n\r", 0);
```

- [ ] **Step 2: Replace Core.Resume in nanny.c**

In `nanny.c`, line 82:
```c
write_to_buffer(d, "Sentience.Auth.Resume {\"event\":\"fail\",\"reason\":\"invalid_or_expired\"}\n\r", 0);
```

- [ ] **Step 3: Verify no remaining Core.Resume references in C code**

Run: `grep -rn "Core\.Resume" --include="*.c" --include="*.h" /sentience/src/`
Expected: No matches.

- [ ] **Step 4: Build and verify**

Run: `cd /sentience/src && ./build`
Expected: Clean build, no errors.

- [ ] **Step 5: Commit**

```bash
git add comm.c nanny.c
git commit -m "refactor: rename Core.Resume → Sentience.Auth.Resume

Move resume GMCP package from Core.* namespace (reserved for standard
GMCP negotiation) to Sentience.Auth.* namespace alongside Auth.QRCode.
No behavioral change — only the package name string changes.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 2: Create gmcp_sentience.h — Definitions and Declarations

**Files:**
- Create: `gmcp_sentience.h`

This header defines the dirty-flag bitmask, the value cache struct, and all public function declarations.

- [ ] **Step 1: Create gmcp_sentience.h**

```c
/*
 * gmcp_sentience.h — Sentience-native GMCP packages
 *
 * Defines Sentience.Char.* and Sentience.Room.* packages that replace
 * the legacy Char.*/Room.Info packages for clients that negotiate
 * "Sentience 1" support or connect via WebSocket.
 *
 * See docs/PLAN_GMCP_REWORK.md for the full package specification.
 */

#ifndef GMCP_SENTIENCE_H
#define GMCP_SENTIENCE_H

#include <jansson.h>
#include <stdbool.h>

/* Forward declarations — full types live in merc.h / protocol.h */
typedef struct descriptor_data descriptor_t;

/*
 * Dirty-flag bitmask — one bit per Sentience.* package.
 * Set when underlying data changes; cleared after send.
 */
typedef enum {
    SENTIENCE_DIRTY_IDENTITY  = (1 << 0),
    SENTIENCE_DIRTY_VITALS    = (1 << 1),
    SENTIENCE_DIRTY_STATS     = (1 << 2),
    SENTIENCE_DIRTY_COMBAT    = (1 << 3),
    SENTIENCE_DIRTY_WORTH     = (1 << 4),
    SENTIENCE_DIRTY_ROOM      = (1 << 5),
    SENTIENCE_DIRTY_AFFECTS   = (1 << 6),
    SENTIENCE_DIRTY_ENEMIES   = (1 << 7),
} sentience_dirty_t;

#define SENTIENCE_PACKAGE_VERSION 1

/* Maximum classes a character can have simultaneously */
#define SENTIENCE_MAX_CLASSES 8

/*
 * Cache of previously-sent values. Stored in protocol_t.
 * Compared each update cycle to detect changes (dirty tracking).
 */
typedef struct {
    /* Vitals */
    long hp, max_hp;
    long mana, max_mana;
    long move, max_move;

    /* Stats */
    int str, int_, wis, dex, con;
    int str_perm, int_perm, wis_perm, dex_perm, con_perm;
    int hitroll, damroll, wimpy;

    /* Combat (AC) */
    int ac_pierce, ac_bash, ac_slash, ac_exotic;

    /* Worth */
    int alignment;
    long xp, xp_tnl;
    int practices;
    long gold;

    /* Identity */
    int level, tot_level;
    char name[64];
    char race_wnum[128];
    char gender[16];
    char title[256];
    int num_classes;
    struct {
        char id[128];
        char name[64];
        int level;
        bool is_primary;
    } classes[SENTIENCE_MAX_CLASSES];

    /* Room */
    long room_id0, room_id1;

    /* Tracks which packages have been sent at least once */
    bool initialized;
} sentience_gmcp_cache_t;

/*
 * JSON builder functions — pure functions that take explicit parameters.
 * Testable without game state. Caller must json_decref() the result.
 */
json_t *sentience_build_vitals_json(long hp, long max_hp,
                                     long mana, long max_mana,
                                     long move, long max_move);

json_t *sentience_build_stats_json(int str, int int_, int wis, int dex, int con,
                                    int str_perm, int int_perm, int wis_perm,
                                    int dex_perm, int con_perm,
                                    int hitroll, int damroll, int wimpy);

json_t *sentience_build_combat_json(int ac_pierce, int ac_bash,
                                     int ac_slash, int ac_exotic);

json_t *sentience_build_worth_json(int alignment, long xp, long xp_tnl,
                                    int practices, long gold);

/*
 * Identity builder input — avoids a massive parameter list.
 */
typedef struct {
    const char *name;
    const char *race_wnum;
    const char *race_name;
    const char *gender;
    int level;
    int tot_level;
    const char *title;
    int num_classes;
    struct {
        const char *id;
        const char *name;
        int level;
        bool is_primary;
    } classes[SENTIENCE_MAX_CLASSES];
} sentience_identity_input_t;

json_t *sentience_build_identity_json(const sentience_identity_input_t *data);

/*
 * Room builder input.
 */
typedef struct {
    const char *wnum;
    const char *name;
    const char *area_name;
    const char *area_wnum;
    const char *sector;
    bool is_wilds;
    int wilds_uid;
    int wilds_x;
    int wilds_y;
    int num_exits;
    struct {
        const char *dir;
        const char *wnum;
        const char *name;
        bool is_door;
        bool is_closed;
        bool is_locked;
    } exits[10]; /* N,E,S,W,U,D + diagonals */
} sentience_room_input_t;

json_t *sentience_build_room_json(const sentience_room_input_t *data);

/*
 * Game-loop entry point. Called from gmcp_update() for each descriptor.
 * Compares current character state with cache, sends dirty packages.
 */
void sentience_gmcp_update(descriptor_t *d);

/*
 * Reset cache to force full resend (e.g., on login or reconnect).
 */
void sentience_gmcp_cache_reset(sentience_gmcp_cache_t *cache);

#endif /* GMCP_SENTIENCE_H */
```

- [ ] **Step 2: Verify header compiles**

Run: `echo '#include "gmcp_sentience.h"' | gcc -fsyntax-only -I/sentience/src -x c - 2>&1 || echo "Check includes"`

This is a quick syntax check. Full compilation comes after the .c file is created.

- [ ] **Step 3: Commit**

```bash
git add gmcp_sentience.h
git commit -m "feat(gmcp): add gmcp_sentience.h with package definitions

Define dirty-flag bitmask, value cache struct, and JSON builder
function signatures for all Sentience.* GMCP packages (Phase 1).

Builder functions take explicit parameters for testability.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 3: Add GMCP_SUPPORT_SENTIENCE to Protocol Layer

**Files:**
- Modify: `protocol.h:314-321` (GMCP_SUPPORT enum)
- Modify: `protocol.h:~489-495` (protocol_t struct)
- Modify: `protocol.c:3361-3367` (bGMCPSupportTable)
- Modify: `protocol.c:396-498` (ProtocolCreate — init cache)
- Modify: `protocol_websocket.c:~40-50` (auto-enable Sentience)
- Modify: `protocol_websocket.c:~124-163` (process_gmcp_input — handle Sentience negotiation)

- [ ] **Step 1: Add GMCP_SUPPORT_SENTIENCE to protocol.h enum**

In `protocol.h`, add `GMCP_SUPPORT_SENTIENCE` before `GMCP_SUPPORT_MAX`:

```c
typedef enum {
   GMCP_SUPPORT_NONE = -1,
   GMCP_SUPPORT_CHAR,
   GMCP_SUPPORT_ROOM,
   GMCP_SUPPORT_SENTIENCE,   /* Sentience.* native packages */
   GMCP_SUPPORT_MAX
} GMCP_SUPPORT;
```

- [ ] **Step 2: Add Sentience cache fields to protocol_t in protocol.h**

Add `#include "gmcp_sentience.h"` near the top of protocol.h (with other includes), then add to the GMCP section of `protocol_t`:

```c
   /*************** START GMCP ***************/
   bool   bGMCP;
   bool   bSGA;
   bool   bGMCPSupport[GMCP_SUPPORT_MAX];
   bool   bGMCPUpdatePackage[GMCP_PACKAGE_MAX];
   char  *GMCPVariable[GMCP_MAX];
   uint32_t              sentience_dirty;     /* Sentience.* dirty bitmask */
   sentience_gmcp_cache_t sentience_cache;    /* Cached values for dirty tracking */
   /*************** END GMCP ***************/
```

- [ ] **Step 3: Add Sentience to bGMCPSupportTable in protocol.c**

```c
const struct gmcp_support_struct bGMCPSupportTable[GMCP_SUPPORT_MAX+1] = {
   { GMCP_SUPPORT_CHAR,      "Char" },
   { GMCP_SUPPORT_ROOM,      "Room" },
   { GMCP_SUPPORT_SENTIENCE, "Sentience" },
   { GMCP_SUPPORT_MAX,       NULL   }
};
```

- [ ] **Step 4: Initialize Sentience cache in ProtocolCreate()**

In `protocol.c` `ProtocolCreate()`, after existing GMCP initialization, add:

```c
   /* Sentience GMCP cache */
   pProtocol->sentience_dirty = 0;
   sentience_gmcp_cache_reset(&pProtocol->sentience_cache);
```

- [ ] **Step 5: Auto-enable Sentience support for WebSocket connections**

In `protocol_websocket.c`, in the WebSocket protocol creation function (or `websocket_negotiate`), set:

```c
   /* WebSocket clients always get Sentience.* packages */
   if (proto->descriptor && proto->descriptor->pProtocol) {
       proto->descriptor->pProtocol->bGMCPSupport[GMCP_SUPPORT_SENTIENCE] = true;
   }
```

- [ ] **Step 6: Handle Sentience negotiation in WebSocket GMCP input**

In `protocol_websocket.c` `process_gmcp_input()`, add Sentience support detection alongside existing Char/Room:

```c
   if (strstr(input, "Sentience")) {
       ws_proto->supports_char = true;
       ws_proto->supports_room = true;
       if (proto->descriptor && proto->descriptor->pProtocol) {
           proto->descriptor->pProtocol->bGMCPSupport[GMCP_SUPPORT_SENTIENCE] = true;
       }
       log_string("WebSocket client supports Sentience.* GMCP packages");
   }
```

Also add Sentience negotiation to `ParseGMCP()` in `protocol.c` — when a telnet client sends `Core.Supports.Set ["Sentience 1"]`, the existing loop through `bGMCPSupportTable` will match "Sentience" and set the flag automatically (no code change needed — the table update in Step 3 handles it).

- [ ] **Step 7: Build and verify**

Run: `cd /sentience/src && ./build`
Expected: Clean build. The new support module is registered but no packages are sent yet.

- [ ] **Step 8: Commit**

```bash
git add protocol.h protocol.c protocol_websocket.c
git commit -m "feat(gmcp): add GMCP_SUPPORT_SENTIENCE negotiation

Add Sentience support module to GMCP negotiation. Telnet clients
opt-in via Core.Supports.Set [\"Sentience 1\"]. WebSocket clients
auto-enable Sentience support on connect.

Adds sentience_dirty bitmask and sentience_gmcp_cache_t to protocol_t.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 4: Create Test Infrastructure for GMCP Tests

**Files:**
- Create: `tests/unit/gmcp_sentience_tests.c`
- Create: `tests/data/unit/gmcp_sentience_unit_tests.json`
- Modify: `tests/framework/test_dispatcher.c:57-114`
- Modify: `tests/framework/test_modules.h`
- Modify: `tests/data/test_config.json`
- Modify: `CMakeLists.txt:314-358`
- Modify: `Makefile:296-338`

- [ ] **Step 1: Create the test handler skeleton**

Create `tests/unit/gmcp_sentience_tests.c`:

```c
#ifdef BUILD_TESTS

#include <string.h>
#include <jansson.h>

#include "../framework/test_framework.h"
#include "../../log.h"
#include "../../gmcp_sentience.h"

/*
 * JSON test cases drive this handler. Each test case has:
 *   "scenario": name of what to test
 *   "input": parameters for the builder function
 *   "expected": expected JSON output fields
 */
test_result_t run_gmcp_sentience_test_case(test_case_t *test)
{
    json_t *input;
    json_t *test_cases;
    const char *func_name;
    size_t index;
    json_t *tc;

    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "GMCP sentience test missing configuration");
        return TEST_ERROR;
    }

    input = json_object_get(test->config, "input");
    if (!input) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "GMCP sentience test missing input");
        return TEST_ERROR;
    }

    func_name = test_json_get_string(input, "function");
    test_cases = json_object_get(input, "test_cases");
    if (!test_cases || !json_is_array(test_cases)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "GMCP sentience test requires test_cases array");
        return TEST_ERROR;
    }

    json_array_foreach(test_cases, index, tc) {
        const char *scenario = test_json_get_string(tc, "scenario");
        test_result_t result;

        if (!scenario) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "GMCP sentience test case missing scenario");
            return TEST_ERROR;
        }

        if (strcmp(func_name, "build_vitals") == 0) {
            result = run_gmcp_vitals_scenario(tc);
        } else if (strcmp(func_name, "build_stats") == 0) {
            result = run_gmcp_stats_scenario(tc);
        } else if (strcmp(func_name, "build_combat") == 0) {
            result = run_gmcp_combat_scenario(tc);
        } else if (strcmp(func_name, "build_worth") == 0) {
            result = run_gmcp_worth_scenario(tc);
        } else if (strcmp(func_name, "build_identity") == 0) {
            result = run_gmcp_identity_scenario(tc);
        } else if (strcmp(func_name, "build_room") == 0) {
            result = run_gmcp_room_scenario(tc);
        } else {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Unknown GMCP function: %s", func_name);
            return TEST_ERROR;
        }

        if (result != TEST_SUCCESS)
            return result;
    }

    return TEST_SUCCESS;
}

/* --- Vitals scenario --- */

static test_result_t run_gmcp_vitals_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    long hp      = json_integer_value(json_object_get(params, "hp"));
    long max_hp  = json_integer_value(json_object_get(params, "max_hp"));
    long mana    = json_integer_value(json_object_get(params, "mana"));
    long max_mana = json_integer_value(json_object_get(params, "max_mana"));
    long move    = json_integer_value(json_object_get(params, "move"));
    long max_move = json_integer_value(json_object_get(params, "max_move"));

    json_t *result = sentience_build_vitals_json(hp, max_hp, mana, max_mana, move, max_move);
    TEST_ASSERT_NOT_NULL(result);

    /* Verify each expected field */
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "hp")),
                       json_integer_value(json_object_get(result, "hp")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "hp_max")),
                       json_integer_value(json_object_get(result, "hp_max")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "mana")),
                       json_integer_value(json_object_get(result, "mana")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "mana_max")),
                       json_integer_value(json_object_get(result, "mana_max")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "move")),
                       json_integer_value(json_object_get(result, "move")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "move_max")),
                       json_integer_value(json_object_get(result, "move_max")));

    /* Verify version field */
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    json_decref(result);
    return TEST_SUCCESS;
}

/* --- Stats scenario --- */

static test_result_t run_gmcp_stats_scenario(json_t *tc)
{
    json_t *p = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!p || !expected) return TEST_ERROR;

    json_t *result = sentience_build_stats_json(
        (int)json_integer_value(json_object_get(p, "str")),
        (int)json_integer_value(json_object_get(p, "int")),
        (int)json_integer_value(json_object_get(p, "wis")),
        (int)json_integer_value(json_object_get(p, "dex")),
        (int)json_integer_value(json_object_get(p, "con")),
        (int)json_integer_value(json_object_get(p, "str_perm")),
        (int)json_integer_value(json_object_get(p, "int_perm")),
        (int)json_integer_value(json_object_get(p, "wis_perm")),
        (int)json_integer_value(json_object_get(p, "dex_perm")),
        (int)json_integer_value(json_object_get(p, "con_perm")),
        (int)json_integer_value(json_object_get(p, "hitroll")),
        (int)json_integer_value(json_object_get(p, "damroll")),
        (int)json_integer_value(json_object_get(p, "wimpy"))
    );
    TEST_ASSERT_NOT_NULL(result);

    /* Spot-check key fields */
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "str")),
                       json_integer_value(json_object_get(result, "str")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "str_base")),
                       json_integer_value(json_object_get(result, "str_base")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "hitroll")),
                       json_integer_value(json_object_get(result, "hitroll")));
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    json_decref(result);
    return TEST_SUCCESS;
}

/* --- Combat scenario --- */

static test_result_t run_gmcp_combat_scenario(json_t *tc)
{
    json_t *p = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!p || !expected) return TEST_ERROR;

    json_t *result = sentience_build_combat_json(
        (int)json_integer_value(json_object_get(p, "ac_pierce")),
        (int)json_integer_value(json_object_get(p, "ac_bash")),
        (int)json_integer_value(json_object_get(p, "ac_slash")),
        (int)json_integer_value(json_object_get(p, "ac_exotic"))
    );
    TEST_ASSERT_NOT_NULL(result);

    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "ac_pierce")),
                       json_integer_value(json_object_get(result, "ac_pierce")));
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    json_decref(result);
    return TEST_SUCCESS;
}

/* --- Worth scenario --- */

static test_result_t run_gmcp_worth_scenario(json_t *tc)
{
    json_t *p = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!p || !expected) return TEST_ERROR;

    json_t *result = sentience_build_worth_json(
        (int)json_integer_value(json_object_get(p, "alignment")),
        json_integer_value(json_object_get(p, "xp")),
        json_integer_value(json_object_get(p, "xp_tnl")),
        (int)json_integer_value(json_object_get(p, "practices")),
        json_integer_value(json_object_get(p, "gold"))
    );
    TEST_ASSERT_NOT_NULL(result);

    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "alignment")),
                       json_integer_value(json_object_get(result, "alignment")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "xp")),
                       json_integer_value(json_object_get(result, "xp")));
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    json_decref(result);
    return TEST_SUCCESS;
}

/* --- Identity scenario --- */

static test_result_t run_gmcp_identity_scenario(json_t *tc)
{
    json_t *p = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!p || !expected) return TEST_ERROR;

    sentience_identity_input_t data = {0};
    data.name      = json_string_value(json_object_get(p, "name"));
    data.race_wnum = json_string_value(json_object_get(p, "race_wnum"));
    data.race_name = json_string_value(json_object_get(p, "race_name"));
    data.gender    = json_string_value(json_object_get(p, "gender"));
    data.level     = (int)json_integer_value(json_object_get(p, "level"));
    data.tot_level = (int)json_integer_value(json_object_get(p, "tot_level"));
    data.title     = json_string_value(json_object_get(p, "title"));

    json_t *classes = json_object_get(p, "classes");
    if (classes && json_is_array(classes)) {
        data.num_classes = (int)json_array_size(classes);
        if (data.num_classes > SENTIENCE_MAX_CLASSES)
            data.num_classes = SENTIENCE_MAX_CLASSES;
        for (int i = 0; i < data.num_classes; i++) {
            json_t *cls = json_array_get(classes, i);
            data.classes[i].id         = json_string_value(json_object_get(cls, "id"));
            data.classes[i].name       = json_string_value(json_object_get(cls, "name"));
            data.classes[i].level      = (int)json_integer_value(json_object_get(cls, "level"));
            data.classes[i].is_primary = json_is_true(json_object_get(cls, "is_primary"));
        }
    }

    json_t *result = sentience_build_identity_json(&data);
    TEST_ASSERT_NOT_NULL(result);

    /* Verify scalar fields */
    TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "name")),
                       json_string_value(json_object_get(result, "name")));
    TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "race")),
                       json_string_value(json_object_get(result, "race")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "level")),
                       json_integer_value(json_object_get(result, "level")));

    /* Verify classes array */
    json_t *result_classes = json_object_get(result, "classes");
    TEST_ASSERT_NOT_NULL(result_classes);
    TEST_ASSERT_TRUE(json_is_array(result_classes));

    json_t *expected_classes = json_object_get(expected, "classes");
    if (expected_classes) {
        TEST_ASSERT_INT_EQ((long)json_array_size(expected_classes),
                           (long)json_array_size(result_classes));
    }

    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    json_decref(result);
    return TEST_SUCCESS;
}

/* --- Room scenario --- */

static test_result_t run_gmcp_room_scenario(json_t *tc)
{
    json_t *p = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!p || !expected) return TEST_ERROR;

    sentience_room_input_t data = {0};
    data.wnum      = json_string_value(json_object_get(p, "wnum"));
    data.name      = json_string_value(json_object_get(p, "name"));
    data.area_name = json_string_value(json_object_get(p, "area_name"));
    data.area_wnum = json_string_value(json_object_get(p, "area_wnum"));
    data.sector    = json_string_value(json_object_get(p, "sector"));
    data.is_wilds  = json_is_true(json_object_get(p, "is_wilds"));
    data.wilds_uid = (int)json_integer_value(json_object_get(p, "wilds_uid"));
    data.wilds_x   = (int)json_integer_value(json_object_get(p, "wilds_x"));
    data.wilds_y   = (int)json_integer_value(json_object_get(p, "wilds_y"));

    json_t *exits = json_object_get(p, "exits");
    if (exits && json_is_array(exits)) {
        data.num_exits = (int)json_array_size(exits);
        if (data.num_exits > 10) data.num_exits = 10;
        for (int i = 0; i < data.num_exits; i++) {
            json_t *ex = json_array_get(exits, i);
            data.exits[i].dir       = json_string_value(json_object_get(ex, "dir"));
            data.exits[i].wnum      = json_string_value(json_object_get(ex, "wnum"));
            data.exits[i].name      = json_string_value(json_object_get(ex, "name"));
            data.exits[i].is_door   = json_is_true(json_object_get(ex, "is_door"));
            data.exits[i].is_closed = json_is_true(json_object_get(ex, "is_closed"));
            data.exits[i].is_locked = json_is_true(json_object_get(ex, "is_locked"));
        }
    }

    json_t *result = sentience_build_room_json(&data);
    TEST_ASSERT_NOT_NULL(result);

    TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "wnum")),
                       json_string_value(json_object_get(result, "wnum")));
    TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "name")),
                       json_string_value(json_object_get(result, "name")));

    /* Verify exits object exists */
    json_t *result_exits = json_object_get(result, "exits");
    TEST_ASSERT_NOT_NULL(result_exits);
    TEST_ASSERT_TRUE(json_is_object(result_exits));

    /* Verify wilds fields present when is_wilds is true */
    if (data.is_wilds) {
        TEST_ASSERT_TRUE(json_is_true(json_object_get(result, "is_wilds")));
        TEST_ASSERT_INT_EQ(data.wilds_x,
                           (int)json_integer_value(json_object_get(result, "wilds_x")));
    }

    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    json_decref(result);
    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
```

- [ ] **Step 2: Create JSON test data file**

Create `tests/data/unit/gmcp_sentience_unit_tests.json`:

```json
{
  "type": "test_suite",
  "test_suite": "gmcp_sentience_unit_tests",
  "description": "Unit tests for Sentience.* GMCP package JSON builders",
  "version": "1.0",
  "requires_mud_environment": false,
  "test_level": "unit",
  "tests": [
    {
      "name": "gmcp_vitals_basic",
      "description": "Verify Sentience.Char.Vitals JSON structure",
      "test_type": "gmcp_sentience_test",
      "input": {
        "function": "build_vitals",
        "test_cases": [
          {
            "scenario": "normal_values",
            "params": { "hp": 85, "max_hp": 120, "mana": 110, "max_mana": 130, "move": 40, "max_move": 100 },
            "expected": { "hp": 85, "hp_max": 120, "mana": 110, "mana_max": 130, "move": 40, "move_max": 100 }
          },
          {
            "scenario": "zero_values",
            "params": { "hp": 0, "max_hp": 100, "mana": 0, "max_mana": 50, "move": 0, "max_move": 75 },
            "expected": { "hp": 0, "hp_max": 100, "mana": 0, "mana_max": 50, "move": 0, "move_max": 75 }
          },
          {
            "scenario": "negative_hp",
            "params": { "hp": -10, "max_hp": 100, "mana": 50, "max_mana": 50, "move": 25, "max_move": 75 },
            "expected": { "hp": -10, "hp_max": 100, "mana": 50, "mana_max": 50, "move": 25, "move_max": 75 }
          }
        ]
      },
      "expected_output": { "all_cases_pass": true }
    },
    {
      "name": "gmcp_stats_basic",
      "description": "Verify Sentience.Char.Stats JSON structure",
      "test_type": "gmcp_sentience_test",
      "input": {
        "function": "build_stats",
        "test_cases": [
          {
            "scenario": "normal_stats",
            "params": {
              "str": 18, "int": 12, "wis": 14, "dex": 16, "con": 15,
              "str_perm": 18, "int_perm": 12, "wis_perm": 14, "dex_perm": 16, "con_perm": 15,
              "hitroll": 5, "damroll": 3, "wimpy": 20
            },
            "expected": { "str": 18, "str_base": 18, "hitroll": 5, "damroll": 3, "wimpy": 20 }
          },
          {
            "scenario": "buffed_stats",
            "params": {
              "str": 22, "int": 15, "wis": 14, "dex": 20, "con": 18,
              "str_perm": 18, "int_perm": 12, "wis_perm": 14, "dex_perm": 16, "con_perm": 15,
              "hitroll": 10, "damroll": 8, "wimpy": 0
            },
            "expected": { "str": 22, "str_base": 18, "hitroll": 10, "damroll": 8, "wimpy": 0 }
          }
        ]
      },
      "expected_output": { "all_cases_pass": true }
    },
    {
      "name": "gmcp_combat_basic",
      "description": "Verify Sentience.Char.Combat JSON structure",
      "test_type": "gmcp_sentience_test",
      "input": {
        "function": "build_combat",
        "test_cases": [
          {
            "scenario": "normal_ac",
            "params": { "ac_pierce": -80, "ac_bash": -60, "ac_slash": -70, "ac_exotic": -50 },
            "expected": { "ac_pierce": -80, "ac_bash": -60, "ac_slash": -70, "ac_exotic": -50 }
          }
        ]
      },
      "expected_output": { "all_cases_pass": true }
    },
    {
      "name": "gmcp_worth_basic",
      "description": "Verify Sentience.Char.Worth JSON structure",
      "test_type": "gmcp_sentience_test",
      "input": {
        "function": "build_worth",
        "test_cases": [
          {
            "scenario": "normal_worth",
            "params": { "alignment": 750, "xp": 150000, "xp_tnl": 50000, "practices": 12, "gold": 5000 },
            "expected": { "alignment": 750, "xp": 150000, "xp_tnl": 50000, "practices": 12, "gold": 5000 }
          }
        ]
      },
      "expected_output": { "all_cases_pass": true }
    },
    {
      "name": "gmcp_identity_single_class",
      "description": "Verify Sentience.Char.Identity with one class",
      "test_type": "gmcp_sentience_test",
      "input": {
        "function": "build_identity",
        "test_cases": [
          {
            "scenario": "single_class_warrior",
            "params": {
              "name": "Aeryn",
              "race_wnum": "races:human",
              "race_name": "Human",
              "gender": "female",
              "level": 30,
              "tot_level": 30,
              "title": "the Brave",
              "classes": [
                { "id": "classes:warrior", "name": "Warrior", "level": 30, "is_primary": true }
              ]
            },
            "expected": {
              "name": "Aeryn",
              "race": "races:human",
              "race_name": "Human",
              "gender": "female",
              "level": 30,
              "tot_level": 30,
              "classes": [
                { "id": "classes:warrior", "name": "Warrior", "level": 30, "is_primary": true }
              ]
            }
          }
        ]
      },
      "expected_output": { "all_cases_pass": true }
    },
    {
      "name": "gmcp_identity_multi_class",
      "description": "Verify Sentience.Char.Identity with multiple classes",
      "test_type": "gmcp_sentience_test",
      "input": {
        "function": "build_identity",
        "test_cases": [
          {
            "scenario": "dual_class",
            "params": {
              "name": "Brek",
              "race_wnum": "races:elf",
              "race_name": "Elf",
              "gender": "male",
              "level": 25,
              "tot_level": 55,
              "title": "the Arcane Blade",
              "classes": [
                { "id": "classes:mage", "name": "Mage", "level": 30, "is_primary": true },
                { "id": "classes:warrior", "name": "Warrior", "level": 25, "is_primary": false }
              ]
            },
            "expected": {
              "name": "Brek",
              "race": "races:elf",
              "level": 25,
              "tot_level": 55,
              "classes": [
                { "id": "classes:mage" },
                { "id": "classes:warrior" }
              ]
            }
          }
        ]
      },
      "expected_output": { "all_cases_pass": true }
    },
    {
      "name": "gmcp_room_basic",
      "description": "Verify Sentience.Room.Info JSON for a normal room",
      "test_type": "gmcp_sentience_test",
      "input": {
        "function": "build_room",
        "test_cases": [
          {
            "scenario": "normal_room",
            "params": {
              "wnum": "midgaard:3001",
              "name": "The Temple of Mota",
              "area_name": "Midgaard",
              "area_wnum": "midgaard",
              "sector": "inside",
              "is_wilds": false,
              "exits": [
                { "dir": "n", "wnum": "midgaard:3002", "name": "The Temple Courtyard", "is_door": false, "is_closed": false, "is_locked": false },
                { "dir": "s", "wnum": "midgaard:3000", "name": "Market Square", "is_door": false, "is_closed": false, "is_locked": false }
              ]
            },
            "expected": {
              "wnum": "midgaard:3001",
              "name": "The Temple of Mota",
              "area_name": "Midgaard"
            }
          }
        ]
      },
      "expected_output": { "all_cases_pass": true }
    },
    {
      "name": "gmcp_room_wilderness",
      "description": "Verify Sentience.Room.Info JSON for wilderness room",
      "test_type": "gmcp_sentience_test",
      "input": {
        "function": "build_room",
        "test_cases": [
          {
            "scenario": "wilderness_room",
            "params": {
              "wnum": "wilderness:0",
              "name": "Grassy Plains",
              "area_name": "Wilderness",
              "area_wnum": "wilderness",
              "sector": "field",
              "is_wilds": true,
              "wilds_uid": 1,
              "wilds_x": 150,
              "wilds_y": 200,
              "exits": [
                { "dir": "n", "wnum": "wilderness:0", "name": "", "is_door": false, "is_closed": false, "is_locked": false }
              ]
            },
            "expected": {
              "wnum": "wilderness:0",
              "name": "Grassy Plains",
              "is_wilds": true
            }
          }
        ]
      },
      "expected_output": { "all_cases_pass": true }
    },
    {
      "name": "gmcp_room_doors",
      "description": "Verify Sentience.Room.Info exit flags for doors",
      "test_type": "gmcp_sentience_test",
      "input": {
        "function": "build_room",
        "test_cases": [
          {
            "scenario": "room_with_locked_door",
            "params": {
              "wnum": "castle:100",
              "name": "Castle Gate",
              "area_name": "Castle",
              "area_wnum": "castle",
              "sector": "inside",
              "is_wilds": false,
              "exits": [
                { "dir": "n", "wnum": "castle:101", "name": "iron gate", "is_door": true, "is_closed": true, "is_locked": true },
                { "dir": "s", "wnum": "castle:99", "name": "", "is_door": false, "is_closed": false, "is_locked": false }
              ]
            },
            "expected": {
              "wnum": "castle:100",
              "name": "Castle Gate"
            }
          }
        ]
      },
      "expected_output": { "all_cases_pass": true }
    }
  ]
}
```

- [ ] **Step 3: Register handler in test_dispatcher.c**

Add to `handler_table[]` in `tests/framework/test_dispatcher.c`, in the unit test handlers section (after the `strdict_` entry):

```c
    { "gmcp_sentience_test",            run_gmcp_sentience_test_case,      MATCH_EXACT  },
```

- [ ] **Step 4: Declare handler in test_modules.h**

Add before `#endif /* BUILD_TESTS */`:

```c
test_result_t run_gmcp_sentience_test_case(test_case_t *test);
```

- [ ] **Step 5: Add suite to test_config.json**

Add `"gmcp_sentience_unit_tests"` to the `"default"`, `"full"`, and `"unit_only"` suite lists in `tests/data/test_config.json`.

- [ ] **Step 6: Add test source files to CMakeLists.txt and Makefile**

In `CMakeLists.txt`, inside the `if(BUILD_TESTS)` block, add:
```cmake
        tests/unit/gmcp_sentience_tests.c
```

In `Makefile`, inside the `ifdef BUILD_TESTS` block, add:
```makefile
               tests/unit/gmcp_sentience_tests.c \
```

- [ ] **Step 7: Build tests and verify they fail**

Run: `cd /sentience/src && ./build tests`
Expected: Build fails — `sentience_build_vitals_json` and other builder functions are not yet defined. This confirms the tests are wired up and calling the right functions.

- [ ] **Step 8: Commit test infrastructure (red phase)**

```bash
git add tests/unit/gmcp_sentience_tests.c tests/data/unit/gmcp_sentience_unit_tests.json \
        tests/framework/test_dispatcher.c tests/framework/test_modules.h \
        tests/data/test_config.json CMakeLists.txt Makefile
git commit -m "test(gmcp): add test infrastructure for Sentience.* GMCP builders

JSON-driven unit tests for all six package builders:
Vitals, Stats, Combat, Worth, Identity, Room.

Tests verify JSON structure, field values, version field, and
complex types (classes array, exits object, wilderness fields).

Build intentionally fails — builder functions not yet implemented.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 5: Implement gmcp_sentience.c — All JSON Builders

**Files:**
- Create: `gmcp_sentience.c`
- Modify: `CMakeLists.txt` (add `gmcp_sentience.c` to main sources)
- Modify: `Makefile` (add `gmcp_sentience.c` to main sources)

- [ ] **Step 1: Add gmcp_sentience.c to build files (non-test section)**

In `CMakeLists.txt`, add `gmcp_sentience.c` to the main `SOURCE_FILES` list (not inside `if(BUILD_TESTS)`) — alphabetically near other gmcp/protocol files.

In `Makefile`, add `gmcp_sentience.c` to the main `C_FILES` list.

- [ ] **Step 2: Create gmcp_sentience.c with all builders**

Create `gmcp_sentience.c`:

```c
/*
 * gmcp_sentience.c — Sentience-native GMCP package implementation
 *
 * JSON builder functions for Sentience.Char.* and Sentience.Room.*
 * packages. Each builder is a pure function that takes explicit
 * parameters and returns a json_t* (caller must json_decref).
 *
 * The sentience_gmcp_update() function compares current character
 * state with cached values, sets dirty flags, and sends changed
 * packages via SendGMCPRaw().
 */

#include "merc.h"
#include "gmcp_sentience.h"
#include "protocol.h"
#include "log.h"

#include <jansson.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Cache management
 * ---------------------------------------------------------------- */

void sentience_gmcp_cache_reset(sentience_gmcp_cache_t *cache)
{
    if (!cache)
        return;
    memset(cache, 0, sizeof(*cache));
    cache->initialized = false;
    cache->room_id0 = -1;
    cache->room_id1 = -1;
}

/* ----------------------------------------------------------------
 * JSON builder: Sentience.Char.Vitals
 * ---------------------------------------------------------------- */

json_t *sentience_build_vitals_json(long hp, long max_hp,
                                     long mana, long max_mana,
                                     long move, long max_move)
{
    json_t *obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "_v",       json_integer(SENTIENCE_PACKAGE_VERSION));
    json_object_set_new(obj, "hp",       json_integer(hp));
    json_object_set_new(obj, "hp_max",   json_integer(max_hp));
    json_object_set_new(obj, "mana",     json_integer(mana));
    json_object_set_new(obj, "mana_max", json_integer(max_mana));
    json_object_set_new(obj, "move",     json_integer(move));
    json_object_set_new(obj, "move_max", json_integer(max_move));

    return obj;
}

/* ----------------------------------------------------------------
 * JSON builder: Sentience.Char.Stats
 * ---------------------------------------------------------------- */

json_t *sentience_build_stats_json(int str, int int_, int wis, int dex, int con,
                                    int str_perm, int int_perm, int wis_perm,
                                    int dex_perm, int con_perm,
                                    int hitroll, int damroll, int wimpy)
{
    json_t *obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "_v",        json_integer(SENTIENCE_PACKAGE_VERSION));
    json_object_set_new(obj, "str",       json_integer(str));
    json_object_set_new(obj, "int",       json_integer(int_));
    json_object_set_new(obj, "wis",       json_integer(wis));
    json_object_set_new(obj, "dex",       json_integer(dex));
    json_object_set_new(obj, "con",       json_integer(con));
    json_object_set_new(obj, "str_base",  json_integer(str_perm));
    json_object_set_new(obj, "int_base",  json_integer(int_perm));
    json_object_set_new(obj, "wis_base",  json_integer(wis_perm));
    json_object_set_new(obj, "dex_base",  json_integer(dex_perm));
    json_object_set_new(obj, "con_base",  json_integer(con_perm));
    json_object_set_new(obj, "hitroll",   json_integer(hitroll));
    json_object_set_new(obj, "damroll",   json_integer(damroll));
    json_object_set_new(obj, "wimpy",     json_integer(wimpy));

    return obj;
}

/* ----------------------------------------------------------------
 * JSON builder: Sentience.Char.Combat
 * ---------------------------------------------------------------- */

json_t *sentience_build_combat_json(int ac_pierce, int ac_bash,
                                     int ac_slash, int ac_exotic)
{
    json_t *obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "_v",        json_integer(SENTIENCE_PACKAGE_VERSION));
    json_object_set_new(obj, "ac_pierce", json_integer(ac_pierce));
    json_object_set_new(obj, "ac_bash",   json_integer(ac_bash));
    json_object_set_new(obj, "ac_slash",  json_integer(ac_slash));
    json_object_set_new(obj, "ac_exotic", json_integer(ac_exotic));

    return obj;
}

/* ----------------------------------------------------------------
 * JSON builder: Sentience.Char.Worth
 * ---------------------------------------------------------------- */

json_t *sentience_build_worth_json(int alignment, long xp, long xp_tnl,
                                    int practices, long gold)
{
    json_t *obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "_v",         json_integer(SENTIENCE_PACKAGE_VERSION));
    json_object_set_new(obj, "alignment",  json_integer(alignment));
    json_object_set_new(obj, "xp",         json_integer(xp));
    json_object_set_new(obj, "xp_tnl",     json_integer(xp_tnl));
    json_object_set_new(obj, "practices",  json_integer(practices));
    json_object_set_new(obj, "gold",       json_integer(gold));

    return obj;
}

/* ----------------------------------------------------------------
 * JSON builder: Sentience.Char.Identity
 * ---------------------------------------------------------------- */

json_t *sentience_build_identity_json(const sentience_identity_input_t *data)
{
    if (!data)
        return NULL;

    json_t *obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "_v",        json_integer(SENTIENCE_PACKAGE_VERSION));
    json_object_set_new(obj, "name",      json_string(data->name ? data->name : ""));
    json_object_set_new(obj, "race",      json_string(data->race_wnum ? data->race_wnum : ""));
    json_object_set_new(obj, "race_name", json_string(data->race_name ? data->race_name : ""));
    json_object_set_new(obj, "gender",    json_string(data->gender ? data->gender : "neutral"));
    json_object_set_new(obj, "level",     json_integer(data->level));
    json_object_set_new(obj, "tot_level", json_integer(data->tot_level));
    json_object_set_new(obj, "title",     json_string(data->title ? data->title : ""));

    json_t *classes = json_array();
    for (int i = 0; i < data->num_classes && i < SENTIENCE_MAX_CLASSES; i++) {
        json_t *cls = json_object();
        json_object_set_new(cls, "id",         json_string(data->classes[i].id ? data->classes[i].id : ""));
        json_object_set_new(cls, "name",       json_string(data->classes[i].name ? data->classes[i].name : ""));
        json_object_set_new(cls, "level",      json_integer(data->classes[i].level));
        json_object_set_new(cls, "is_primary", json_boolean(data->classes[i].is_primary));
        json_array_append_new(classes, cls);
    }
    json_object_set_new(obj, "classes", classes);

    return obj;
}

/* ----------------------------------------------------------------
 * JSON builder: Sentience.Room.Info
 * ---------------------------------------------------------------- */

json_t *sentience_build_room_json(const sentience_room_input_t *data)
{
    if (!data)
        return NULL;

    json_t *obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "_v",        json_integer(SENTIENCE_PACKAGE_VERSION));
    json_object_set_new(obj, "wnum",      json_string(data->wnum ? data->wnum : ""));
    json_object_set_new(obj, "name",      json_string(data->name ? data->name : ""));
    json_object_set_new(obj, "area_name", json_string(data->area_name ? data->area_name : ""));
    json_object_set_new(obj, "area_wnum", json_string(data->area_wnum ? data->area_wnum : ""));
    json_object_set_new(obj, "sector",    json_string(data->sector ? data->sector : ""));
    json_object_set_new(obj, "is_wilds",  json_boolean(data->is_wilds));

    if (data->is_wilds) {
        json_object_set_new(obj, "wilds_uid", json_integer(data->wilds_uid));
        json_object_set_new(obj, "wilds_x",   json_integer(data->wilds_x));
        json_object_set_new(obj, "wilds_y",   json_integer(data->wilds_y));
    }

    json_t *exits = json_object();
    for (int i = 0; i < data->num_exits && i < 10; i++) {
        if (!data->exits[i].dir)
            continue;

        json_t *exit_obj = json_object();
        json_object_set_new(exit_obj, "wnum", json_string(data->exits[i].wnum ? data->exits[i].wnum : ""));

        if (data->exits[i].name && data->exits[i].name[0])
            json_object_set_new(exit_obj, "name", json_string(data->exits[i].name));

        if (data->exits[i].is_door) {
            json_t *flags = json_array();
            json_array_append_new(flags, json_string("door"));
            if (data->exits[i].is_closed)
                json_array_append_new(flags, json_string("closed"));
            if (data->exits[i].is_locked)
                json_array_append_new(flags, json_string("locked"));
            json_object_set_new(exit_obj, "flags", flags);
        }

        json_object_set_new(exits, data->exits[i].dir, exit_obj);
    }
    json_object_set_new(obj, "exits", exits);

    return obj;
}

/* ----------------------------------------------------------------
 * Helper: serialize json_t to string and send via SendGMCPRaw
 * ---------------------------------------------------------------- */

static void sentience_send_package(descriptor_t *d, const char *package, json_t *json)
{
    if (!d || !json)
        return;

    char *str = json_dumps(json, JSON_COMPACT);
    if (str) {
        SendGMCPRaw(d, package, str);
        free(str);
    }
    json_decref(json);
}

/* ----------------------------------------------------------------
 * Game-loop entry point
 *
 * Called from gmcp_update() for each playing descriptor. Compares
 * current character state with cache, sets dirty flags, and sends
 * changed packages via SendGMCPRaw().
 * ---------------------------------------------------------------- */

void sentience_gmcp_update(descriptor_t *d)
{
    if (!d || !d->pProtocol)
        return;

    if (!d->pProtocol->bGMCPSupport[GMCP_SUPPORT_SENTIENCE])
        return;

    if (!d->character || d->connected != CON_PLAYING || IS_NPC(d->character))
        return;

    CHAR_DATA *ch = d->character;
    ROOM_INDEX_DATA *room = ch->in_room;
    if (!room)
        return;

    sentience_gmcp_cache_t *cache = &d->pProtocol->sentience_cache;
    uint32_t dirty = 0;
    bool force_all = !cache->initialized;

    /* --- Vitals --- */
    {
        long hp = ch->hit, max_hp = ch->max_hit;
        long mana = ch->mana, max_mana = ch->max_mana;
        long move = ch->move, max_move = ch->max_move;

        if (force_all || hp != cache->hp || max_hp != cache->max_hp ||
            mana != cache->mana || max_mana != cache->max_mana ||
            move != cache->move || max_move != cache->max_move) {
            dirty |= SENTIENCE_DIRTY_VITALS;
            cache->hp = hp; cache->max_hp = max_hp;
            cache->mana = mana; cache->max_mana = max_mana;
            cache->move = move; cache->max_move = max_move;
        }
    }

    /* --- Stats --- */
    {
        int str = get_curr_stat(ch, STAT_STR);
        int int_ = get_curr_stat(ch, STAT_INT);
        int wis = get_curr_stat(ch, STAT_WIS);
        int dex = get_curr_stat(ch, STAT_DEX);
        int con = get_curr_stat(ch, STAT_CON);
        int str_p = ch->perm_stat[STAT_STR];
        int int_p = ch->perm_stat[STAT_INT];
        int wis_p = ch->perm_stat[STAT_WIS];
        int dex_p = ch->perm_stat[STAT_DEX];
        int con_p = ch->perm_stat[STAT_CON];
        int hitroll = GET_HITROLL(ch);
        int damroll = GET_DAMROLL(ch);
        int wimpy = ch->wimpy;

        if (force_all || str != cache->str || int_ != cache->int_ ||
            wis != cache->wis || dex != cache->dex || con != cache->con ||
            str_p != cache->str_perm || int_p != cache->int_perm ||
            wis_p != cache->wis_perm || dex_p != cache->dex_perm ||
            con_p != cache->con_perm ||
            hitroll != cache->hitroll || damroll != cache->damroll ||
            wimpy != cache->wimpy) {
            dirty |= SENTIENCE_DIRTY_STATS;
            cache->str = str; cache->int_ = int_; cache->wis = wis;
            cache->dex = dex; cache->con = con;
            cache->str_perm = str_p; cache->int_perm = int_p;
            cache->wis_perm = wis_p; cache->dex_perm = dex_p;
            cache->con_perm = con_p;
            cache->hitroll = hitroll; cache->damroll = damroll;
            cache->wimpy = wimpy;
        }
    }

    /* --- Combat (AC) --- */
    {
        int pierce = GET_AC(ch, AC_PIERCE);
        int bash   = GET_AC(ch, AC_BASH);
        int slash  = GET_AC(ch, AC_SLASH);
        int exotic = GET_AC(ch, AC_EXOTIC);

        if (force_all || pierce != cache->ac_pierce || bash != cache->ac_bash ||
            slash != cache->ac_slash || exotic != cache->ac_exotic) {
            dirty |= SENTIENCE_DIRTY_COMBAT;
            cache->ac_pierce = pierce; cache->ac_bash = bash;
            cache->ac_slash = slash; cache->ac_exotic = exotic;
        }
    }

    /* --- Worth --- */
    {
        int align = ch->alignment;
        long xp = ch->exp;
        long xp_tnl = ((ch->level + 1) * exp_per_level(ch, NULL, ch->pcdata->points) - ch->exp);
        int pracs = ch->practice;
        long gold = ch->gold;

        if (force_all || align != cache->alignment || xp != cache->xp ||
            xp_tnl != cache->xp_tnl || pracs != cache->practices ||
            gold != cache->gold) {
            dirty |= SENTIENCE_DIRTY_WORTH;
            cache->alignment = align; cache->xp = xp;
            cache->xp_tnl = xp_tnl; cache->practices = pracs;
            cache->gold = gold;
        }
    }

    /* --- Identity (changes rarely — level up, class change, title change) --- */
    {
        bool id_dirty = force_all;

        if (ch->level != cache->level || ch->tot_level != cache->tot_level)
            id_dirty = true;
        if (strcmp(ch->name ? ch->name : "", cache->name) != 0)
            id_dirty = true;

        if (id_dirty)
            dirty |= SENTIENCE_DIRTY_IDENTITY;
    }

    /* --- Room (only when room changes) --- */
    {
        if (force_all || room->id[0] != cache->room_id0 || room->id[1] != cache->room_id1) {
            dirty |= SENTIENCE_DIRTY_ROOM;
            cache->room_id0 = room->id[0];
            cache->room_id1 = room->id[1];
        }
    }

    /* --- Send dirty packages --- */

    if (dirty & SENTIENCE_DIRTY_VITALS) {
        sentience_send_package(d, "Sentience.Char.Vitals",
            sentience_build_vitals_json(cache->hp, cache->max_hp,
                                        cache->mana, cache->max_mana,
                                        cache->move, cache->max_move));
    }

    if (dirty & SENTIENCE_DIRTY_STATS) {
        sentience_send_package(d, "Sentience.Char.Stats",
            sentience_build_stats_json(cache->str, cache->int_, cache->wis,
                                       cache->dex, cache->con,
                                       cache->str_perm, cache->int_perm,
                                       cache->wis_perm, cache->dex_perm,
                                       cache->con_perm,
                                       cache->hitroll, cache->damroll,
                                       cache->wimpy));
    }

    if (dirty & SENTIENCE_DIRTY_COMBAT) {
        sentience_send_package(d, "Sentience.Char.Combat",
            sentience_build_combat_json(cache->ac_pierce, cache->ac_bash,
                                        cache->ac_slash, cache->ac_exotic));
    }

    if (dirty & SENTIENCE_DIRTY_WORTH) {
        sentience_send_package(d, "Sentience.Char.Worth",
            sentience_build_worth_json(cache->alignment, cache->xp,
                                       cache->xp_tnl, cache->practices,
                                       cache->gold));
    }

    if (dirty & SENTIENCE_DIRTY_IDENTITY) {
        sentience_identity_input_t id_data = {0};
        id_data.name = ch->name;
        id_data.gender = (ch->sex == 1) ? "male" : (ch->sex == 2) ? "female" : "neutral";
        id_data.level = ch->level;
        id_data.tot_level = ch->tot_level;
        id_data.title = ch->pcdata ? ch->pcdata->title : "";

        if (ch->race) {
            id_data.race_wnum = widevnum_string_race(ch->race, NULL);
            id_data.race_name = ch->race->name;
        } else {
            id_data.race_wnum = "";
            id_data.race_name = "unknown";
        }

        /* Populate multi-class array from character's class levels */
        if (ch->pcdata) {
            CLASS_LEVEL *cl;
            CLASS_DATA *primary = get_current_class(ch);
            int ci = 0;
            for (cl = ch->pcdata->class_levels; cl && ci < SENTIENCE_MAX_CLASSES; cl = cl->next, ci++) {
                if (cl->clazz) {
                    id_data.classes[ci].id = widevnum_string_class(cl->clazz, NULL);
                    id_data.classes[ci].name = class_display_ch(cl->clazz, ch);
                    id_data.classes[ci].level = cl->level;
                    id_data.classes[ci].is_primary = (cl->clazz == primary);
                }
            }
            id_data.num_classes = ci;
        }

        /* Update cache */
        snprintf(cache->name, sizeof(cache->name), "%s", ch->name ? ch->name : "");
        cache->level = ch->level;
        cache->tot_level = ch->tot_level;

        sentience_send_package(d, "Sentience.Char.Identity",
            sentience_build_identity_json(&id_data));
    }

    if (dirty & SENTIENCE_DIRTY_ROOM) {
        sentience_room_input_t room_data = {0};
        room_data.wnum = widevnum_string_room(room, NULL);
        room_data.name = room->name;
        room_data.area_name = room->area ? room->area->name : "";
        room_data.area_wnum = room->area ? widevnum_string_area(room->area) : "";
        room_data.sector = sector_flag_name(room->sector_type);
        room_data.is_wilds = (room->wilds != NULL);

        if (room->wilds) {
            room_data.wilds_uid = room->wilds->uid;
            room_data.wilds_x = room->x;
            room_data.wilds_y = room->y;
        }

        static const char *dir_names[] = { "n", "e", "s", "w", "u", "d" };
        int ei = 0;
        for (int i = DIR_NORTH; i <= DIR_DOWN && ei < 10; i++) {
            if (!room->exit[i] || !room->exit[i]->u1.to_room)
                continue;

            room_data.exits[ei].dir = dir_names[i];

            ROOM_INDEX_DATA *to_room = room->exit[i]->u1.to_room;
            room_data.exits[ei].wnum = widevnum_string_room(to_room, NULL);
            room_data.exits[ei].name = room->exit[i]->keyword ? room->exit[i]->keyword : "";

            room_data.exits[ei].is_door = IS_SET(room->exit[i]->exit_info, EX_ISDOOR);
            room_data.exits[ei].is_closed = IS_SET(room->exit[i]->exit_info, EX_CLOSED);
            room_data.exits[ei].is_locked = IS_SET(room->exit[i]->exit_info, EX_LOCKED);
            ei++;
        }
        room_data.num_exits = ei;

        sentience_send_package(d, "Sentience.Room.Info",
            sentience_build_room_json(&room_data));
    }

    cache->initialized = true;
    d->pProtocol->sentience_dirty = dirty;
}
```

**Note:** This file references game functions (`get_curr_stat`, `GET_HITROLL`, `GET_DAMROLL`, `GET_AC`, `exp_per_level`, `widevnum_string_room`, `widevnum_string_race`, `widevnum_string_class`, `widevnum_string_area`, `get_current_class`, `class_display_ch`, `sector_flag_name`). These are all existing functions — the implementation uses the current game API. If `widevnum_string_race`, `widevnum_string_class`, or `widevnum_string_area` don't exist yet, they may need to be added following the pattern of `widevnum_string_room()` in `handler.c:14117`. Similarly, `sector_flag_name()` may need to be checked — use `sector_bit_name()` or `flag_name(sector_flags, room->sector_type)` if the exact function name differs. Adapt to what the codebase provides.

- [ ] **Step 3: Build with tests and verify tests pass**

Run: `cd /sentience/src && ./build tests`
Expected: Clean build.

Run: `cd /sentience && ./sent -test:gmcp`
Expected: All GMCP sentience tests pass (9 test cases).

- [ ] **Step 4: Run full test suite to verify no regressions**

Run: `cd /sentience && ./sent -test`
Expected: Baseline maintained (418 pass + new GMCP tests passing).

- [ ] **Step 5: Commit**

```bash
git add gmcp_sentience.c CMakeLists.txt Makefile
git commit -m "feat(gmcp): implement Sentience.* JSON builders and game loop update

Implement six GMCP package builders using Jansson:
- Sentience.Char.Vitals (hp/mana/move)
- Sentience.Char.Stats (attributes, combat bonuses)
- Sentience.Char.Combat (AC by damage type)
- Sentience.Char.Worth (alignment, xp, gold)
- Sentience.Char.Identity (multi-class array, race widevnum)
- Sentience.Room.Info (widevnum, exit flags, wilderness coords)

sentience_gmcp_update() compares current state with per-descriptor
cache, sends only dirty packages. All builders include _v field
for client version handling.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 6: Wire into Game Loop

**Files:**
- Modify: `update.c:4566-4779` (gmcp_update function)

- [ ] **Step 1: Add include and call sentience_gmcp_update()**

Add `#include "gmcp_sentience.h"` near the top of `update.c` (with other includes).

In `gmcp_update()`, add the Sentience update call **before** `SendUpdatedGMCP(d)` (around line 4775). The call should be inside the `for (d = descriptor_list; ...)` loop, at the same level as the existing `SendUpdatedGMCP(d)` call:

```c
        /* Send Sentience.* packages (for clients that negotiated Sentience support) */
        sentience_gmcp_update(d);

        SendUpdatedGMCP( d );
```

The existing legacy `UpdateGMCPString`/`UpdateGMCPNumber` calls remain — they continue to serve legacy clients. `sentience_gmcp_update()` handles Sentience clients independently.

- [ ] **Step 2: Build and verify**

Run: `cd /sentience/src && ./build`
Expected: Clean build.

- [ ] **Step 3: Run tests**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test`
Expected: All tests pass, baseline maintained.

- [ ] **Step 4: Commit**

```bash
git add update.c
git commit -m "feat(gmcp): wire sentience_gmcp_update() into game loop

Call sentience_gmcp_update() for each descriptor in gmcp_update().
Sentience-capable clients now receive Sentience.* packages alongside
(or instead of) legacy Char.*/Room.Info packages.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 7: WebSocket Sends Sentience.* Instead of Char.Status

**Files:**
- Modify: `protocol_websocket.c:286-307` (websocket_send_mxp_variable)

- [ ] **Step 1: Replace Char.Status with Sentience.* routing**

The current `websocket_send_mxp_variable()` sends everything to `"Char.Status"`. Since WebSocket clients now auto-enable Sentience support (Task 3 Step 5), and `sentience_gmcp_update()` sends proper packages via `SendGMCPRaw()`, the generic `Char.Status` path is no longer needed for Sentience-supported variables.

Modify `websocket_send_mxp_variable()` to no-op (or log a deprecation warning) since `sentience_gmcp_update()` now handles all package sending:

```c
static void websocket_send_mxp_variable(protocol_layer_t *proto,
                                       const char *variable, const char *value,
                                       bool is_number)
{
    /* Sentience.* packages are now sent by sentience_gmcp_update() in
     * gmcp_sentience.c via SendGMCPRaw(). This legacy per-variable path
     * is no longer needed for WebSocket clients. */
    (void)proto;
    (void)variable;
    (void)value;
    (void)is_number;
}
```

- [ ] **Step 2: Build and verify**

Run: `cd /sentience/src && ./build`
Expected: Clean build.

- [ ] **Step 3: Run tests**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test`
Expected: All tests pass.

- [ ] **Step 4: Commit**

```bash
git add protocol_websocket.c
git commit -m "feat(gmcp): remove legacy Char.Status from WebSocket path

WebSocket GMCP is now served by sentience_gmcp_update() which sends
proper Sentience.Char.* and Sentience.Room.* packages via SendGMCPRaw().
The generic Char.Status per-variable path is no longer needed.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 8: Final Integration Verification

- [ ] **Step 1: Full build (non-test)**

Run: `cd /sentience/src && ./build`
Expected: Clean build, no warnings related to new code.

- [ ] **Step 2: Full test suite**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test`
Expected: Baseline maintained + all new GMCP tests pass.

- [ ] **Step 3: Verify no stray Core.Resume in C code**

Run: `grep -rn "Core\.Resume" --include="*.c" --include="*.h" /sentience/src/`
Expected: No matches.

- [ ] **Step 4: Verify Sentience support module is registered**

Run: `grep -n "GMCP_SUPPORT_SENTIENCE" /sentience/src/protocol.h /sentience/src/protocol.c`
Expected: Enum value in protocol.h, table entry in protocol.c.

- [ ] **Step 5: Review git log**

Run: `git --no-pager log --oneline -8`
Expected: 7 clean commits matching the task progression.

---

## Notes

### What This Plan Does NOT Cover (Future Phases)
- **Sentience.Char.Affects / Sentience.Char.Enemies** — Phase 3 packages with wnum fields
- **Sentience.Link** — MXP abstraction / Track 2 (separate plan)
- **Sentience.Client.Ready / Sentience.Client.Layout** — Client capabilities (separate plan)
- **Sentience.Channel.Message** — Chat panel GMCP (separate plan)
- **Mudlet package** — Phase 4, depends on stable GMCP packages
- **Web client panels** — Depends on GMCP packages being live

### Assumptions
- Jansson is available and linked (it is — used by test framework and game code)
- `widevnum_string_room()`, `widevnum_string_race()`, `widevnum_string_class()`, `widevnum_string_area()` exist or can be trivially added following the existing pattern in `handler.c:14117`
- `sector_flag_name()` or equivalent exists for sector type to string conversion
- `ch->pcdata->class_levels` is the linked list of class levels for multi-class
- `ch->tot_level` field exists on CHAR_DATA
- `get_current_class()` returns the primary CLASS_DATA*

### Risk Areas
- **Identity multi-class iteration**: The `ch->pcdata->class_levels` linked list structure and `CLASS_LEVEL` fields need to be verified. The implementation assumes `cl->clazz` and `cl->level` fields.
- **Widevnum helper functions**: `widevnum_string_race()` and `widevnum_string_class()` may not exist yet. If they don't, they need to be created following `widevnum_string_room()` as a template (3-4 lines each).
- **WebSocket GMCP framing**: `SendGMCPRaw()` uses IAC SB/SE framing for telnet. Need to verify it works correctly for WebSocket connections too, or if WebSocket connections bypass the IAC framing in the `Write()` path. The protocol layer abstraction should handle this.
