# Sentience.Room.Map — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `Sentience.Room.Map` GMCP package that sends pre-rendered ASCII maps (area minimap and wilderness grid) as structured JSON, with inline map suppression when GMCP is active.

**Architecture:** Add a builder function to `gmcp_sentience.c` (same pattern as Room.Info), create `render_*_to_buffer()` extraction functions for both map systems, wire the send into `sentience_gmcp_update()` on room change, and suppress inline display when GMCP map is active.

**Tech Stack:** C, Jansson (JSON), existing test framework (JSON test data + C handler)

**Spec:** `docs/superpowers/specs/2026-03-25-gmcp-room-map-design.md`

**Build/Test:**
```bash
cd /sentience/src && ./build tests        # Build with test support
cd /sentience && ./sent -test:gmcp        # Run GMCP tests
cd /sentience && ./sent -test             # Run all tests
```

---

## File Map

| File | Action | Purpose |
|------|--------|---------|
| `gmcp_sentience.h` | Modify | Add `sentience_room_map_input_t` struct, builder declaration |
| `gmcp_sentience.c` | Modify | Add builder function + Room.Map send in update loop |
| `act_info.c` | Modify | Add `render_area_map_to_buffer()`, add suppression checks in `show_room()` |
| `wilds.c` | Modify | Add `render_wilds_map_to_buffer()` |
| `wilds.h` | Modify | Declare `render_wilds_map_to_buffer()` |
| `merc.h` | Modify | Declare `render_area_map_to_buffer()` |
| `tests/unit/gmcp_sentience_tests.c` | Modify | Add `run_gmcp_room_map_scenario()` + dispatcher entry |
| `tests/data/unit/gmcp_sentience_unit_tests.json` | Modify | Add 3 Room.Map test cases |

No new files. No build system changes (no new .c files).

---

### Task 1: Header — Input Struct + Builder Declaration

Add the Room.Map type definitions to `gmcp_sentience.h`. Pure header work.

**Files:**
- Modify: `gmcp_sentience.h`

- [ ] **Step 1: Add input struct after `sentience_room_input_t` (after line 148)**

```c
/* Room.Map input — pre-rendered map for GMCP delivery */
typedef struct {
    const char *type;       /* "area" or "wilds" */
    const char *map_text;   /* Pre-rendered map string with MUD color codes */
    int width;              /* Character columns */
    int height;             /* Character rows */
} sentience_room_map_input_t;
```

- [ ] **Step 2: Add builder declaration after `sentience_build_room_json` (after line 150)**

```c
json_t *sentience_build_room_map(const sentience_room_map_input_t *input);
```

**Verification:** Build compiles with no errors or warnings from `gmcp_sentience.h`.

---

### Task 2: Builder Function

Implement `sentience_build_room_map()` in `gmcp_sentience.c`. Simple field-to-JSON mapping.

**Files:**
- Modify: `gmcp_sentience.c`

- [ ] **Step 1: Add builder function after `sentience_build_room_json()` (after line 176)**

```c
json_t *sentience_build_room_map(const sentience_room_map_input_t *input)
{
    json_t *obj;

    if (!input || !input->type || !input->map_text)
        return NULL;

    obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "_v",       json_integer(1));
    json_object_set_new(obj, "type",     json_string(input->type));
    json_object_set_new(obj, "map_text", json_string(input->map_text));
    json_object_set_new(obj, "width",    json_integer(input->width));
    json_object_set_new(obj, "height",   json_integer(input->height));

    return obj;
}
```

**Verification:** Build compiles cleanly.

---

### Task 3: Unit Tests for Builder

Add test handler and JSON test data for the Room.Map builder.

**Files:**
- Modify: `tests/unit/gmcp_sentience_tests.c`
- Modify: `tests/data/unit/gmcp_sentience_unit_tests.json`

- [ ] **Step 1: Add test scenario handler in `gmcp_sentience_tests.c`**

Add `run_gmcp_room_map_scenario()` function (before the dispatcher function). Follow the pattern of `run_gmcp_room_scenario()` (lines 206-261):

```c
static test_result_t run_gmcp_room_map_scenario(json_t *test_case)
{
    json_t *params = json_object_get(test_case, "params");
    json_t *expected = json_object_get(test_case, "expected");

    sentience_room_map_input_t input = {
        .type     = json_string_value(json_object_get(params, "type")),
        .map_text = json_string_value(json_object_get(params, "map_text")),
        .width    = json_integer_value(json_object_get(params, "width")),
        .height   = json_integer_value(json_object_get(params, "height")),
    };

    json_t *result = sentience_build_room_map(&input);
    if (!result) {
        /* If expected is null/missing, NULL result is correct */
        if (!expected || json_is_null(expected))
            return TEST_SUCCESS;
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "sentience_build_room_map returned NULL");
        return TEST_FAIL;
    }

    /* Verify fields match expected */
    test_result_t tr = TEST_SUCCESS;

    if (json_object_get(expected, "_v")) {
        int exp_v = json_integer_value(json_object_get(expected, "_v"));
        int got_v = json_integer_value(json_object_get(result, "_v"));
        if (exp_v != got_v) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "_v: expected %d, got %d", exp_v, got_v);
            tr = TEST_FAIL;
        }
    }

    if (json_object_get(expected, "type")) {
        const char *exp = json_string_value(json_object_get(expected, "type"));
        const char *got = json_string_value(json_object_get(result, "type"));
        if (!exp || !got || strcmp(exp, got) != 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "type: expected '%s', got '%s'", exp ? exp : "NULL", got ? got : "NULL");
            tr = TEST_FAIL;
        }
    }

    if (json_object_get(expected, "width")) {
        int exp_w = json_integer_value(json_object_get(expected, "width"));
        int got_w = json_integer_value(json_object_get(result, "width"));
        if (exp_w != got_w) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "width: expected %d, got %d", exp_w, got_w);
            tr = TEST_FAIL;
        }
    }

    if (json_object_get(expected, "height")) {
        int exp_h = json_integer_value(json_object_get(expected, "height"));
        int got_h = json_integer_value(json_object_get(result, "height"));
        if (exp_h != got_h) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "height: expected %d, got %d", exp_h, got_h);
            tr = TEST_FAIL;
        }
    }

    if (json_object_get(expected, "map_text")) {
        const char *exp = json_string_value(json_object_get(expected, "map_text"));
        const char *got = json_string_value(json_object_get(result, "map_text"));
        if (!exp || !got || strcmp(exp, got) != 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "map_text mismatch");
            tr = TEST_FAIL;
        }
    }

    json_decref(result);
    return tr;
}
```

- [ ] **Step 2: Add dispatcher entry (after line 316, before the `else` block)**

```c
} else if (strcmp(func_name, "build_room_map") == 0) {
    result = run_gmcp_room_map_scenario(tc);
```

- [ ] **Step 3: Add JSON test cases to `gmcp_sentience_unit_tests.json`**

Add 3 test entries after the last test (after line 254, before the final `]`):

```json
,
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_room_map_area",
    "description": "Builds Sentience.Room.Map JSON for an area minimap",
    "input": {
        "function": "build_room_map",
        "test_cases": [
            {
                "scenario": "area_minimap",
                "params": {
                    "type": "area",
                    "map_text": "{b|{GO{B-{GO{B|{WS{B|   {b|",
                    "width": 21,
                    "height": 7
                },
                "expected": {
                    "_v": 1,
                    "type": "area",
                    "width": 21,
                    "height": 7,
                    "map_text": "{b|{GO{B-{GO{B|{WS{B|   {b|"
                }
            }
        ]
    },
    "expected_result": "pass"
},
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_room_map_wilds",
    "description": "Builds Sentience.Room.Map JSON for a wilderness map",
    "input": {
        "function": "build_room_map",
        "test_cases": [
            {
                "scenario": "wilderness_map",
                "params": {
                    "type": "wilds",
                    "map_text": "{G.{G.{Y~{G.{G.\n\r{G.{M@{G.{G.{G.\n\r{G.{G.{G.{B~{G.",
                    "width": 33,
                    "height": 11
                },
                "expected": {
                    "_v": 1,
                    "type": "wilds",
                    "width": 33,
                    "height": 11
                }
            }
        ]
    },
    "expected_result": "pass"
},
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_room_map_empty",
    "description": "Builds Sentience.Room.Map JSON with empty map text",
    "input": {
        "function": "build_room_map",
        "test_cases": [
            {
                "scenario": "empty_map",
                "params": {
                    "type": "area",
                    "map_text": "",
                    "width": 0,
                    "height": 0
                },
                "expected": {
                    "_v": 1,
                    "type": "area",
                    "width": 0,
                    "height": 0,
                    "map_text": ""
                }
            }
        ]
    },
    "expected_result": "pass"
}
```

**Verification:** `./build tests && cd /sentience && ./sent -test:gmcp` — all GMCP tests pass including the 3 new ones.

---

### Task 4: Area Map Render-to-Buffer

Extract the area minimap rendering into a reusable `render_area_map_to_buffer()` function in `act_info.c`.

**Files:**
- Modify: `act_info.c`
- Modify: `merc.h`

- [ ] **Step 1: Add `render_area_map_to_buffer()` in `act_info.c` (near `show_map()` at line ~7667)**

This function calls `create_map()` to build the 10×7 grid, then renders it to a BUFFER using `show_map()` logic (convert_map_char per cell, border rows). Returns the rendered text, width, and height.

```c
/*
 * Render the area minimap to a buffer string for GMCP delivery.
 * Returns true if map was generated, false if map cannot be shown.
 * Caller must free_buf() the output buffer.
 */
bool render_area_map_to_buffer(CHAR_DATA *ch, ROOM_INDEX_DATA *room,
                               BUFFER **out_buf, int *out_width, int *out_height)
{
    char map[100];
    char cell[4];
    int line, count;
    BUFFER *buf;

    if (!ch || !room || !out_buf)
        return false;

    if (IS_SET(ch->comm, COMM_NOMAP) ||
        IS_SET(room->room_flag[0], ROOM_NOMAP) ||
        IS_SET(room->area->area_flags, AREA_NOMAP))
        return false;

    /* Wilderness rooms don't get area minimaps */
    if (room->wilds)
        return false;

    create_map(ch, room, map);

    buf = new_buf();

    for (line = 1; line <= 7; line++) {
        if (line == 1 || line == 7) {
            add_buf(buf, "{B+{b----------{B+{x");
        } else {
            add_buf(buf, "{b|");
            for (count = ((line - 1) * 10); count < (line * 10); count++) {
                convert_map_char(cell, map[count]);
                cell[3] = '\0';
                add_buf(buf, cell);
            }
            add_buf(buf, "{b|{x");
        }
        if (line < 7)
            add_buf(buf, "\n\r");
    }

    *out_buf = buf;
    if (out_width)  *out_width = 21;   /* 10 map chars * ~2 color + borders */
    if (out_height) *out_height = 7;
    return true;
}
```

- [ ] **Step 2: Declare in `merc.h`**

Add declaration near other act_info.c declarations (search for `show_map` or `create_map` declarations):

```c
bool render_area_map_to_buffer(CHAR_DATA *ch, ROOM_INDEX_DATA *room,
                               BUFFER **out_buf, int *out_width, int *out_height);
```

**Verification:** Build compiles cleanly. No functional changes yet — just adding a new function.

---

### Task 5: Wilderness Map Render-to-Buffer

Create `render_wilds_map_to_buffer()` in `wilds.c` — a variant of `show_map_to_char_wyx()` that writes to a BUFFER instead of calling `send_to_char()`.

**Files:**
- Modify: `wilds.c`
- Modify: `wilds.h`

- [ ] **Step 1: Add `render_wilds_map_to_buffer()` in `wilds.c` (after `show_map_to_char_wyx()` at line ~2860)**

This is a copy of `show_map_to_char_wyx()` modified to:
- Accept a `BUFFER **out_buf` and `int *out_width, *out_height` output parameters
- Write to the BUFFER instead of calling `send_to_char()`/`page_to_char()`
- Skip OLC-only features (corner coordinates, OLC markers)
- Return true/false instead of void

Key differences from `show_map_to_char_wyx()`:
- Replace `BUFFER *output = new_buf()` → caller-provided buffer
- Replace `send_to_char(buf_string(output), to)` → assign to `*out_buf`
- Set `*out_width = (squares_to_show_x * 2 + 1)` and `*out_height = (squares_to_show_y * 2 + 1)` at the end
- Always pass `olc = false` internally (no OLC features needed for GMCP)

```c
bool render_wilds_map_to_buffer(WILDS_DATA *pWilds, int wx, int wy,
                                CHAR_DATA *ch, int bonus_view_x, int bonus_view_y,
                                BUFFER **out_buf, int *out_width, int *out_height)
```

**Implementation approach:** Copy `show_map_to_char_wyx()`, remove the OLC branches (corner coordinates, OLC padding), replace the final `send_to_char()` / `page_to_char()` with buffer assignment, and add width/height output. The terrain rendering loop (the core map generation) stays identical. Internally use `wx` for `vx` and `wy` for `vy` — GMCP always centers on the player position.

- [ ] **Step 2: Declare in `wilds.h` (after `show_map_to_char_wyx` declaration, line ~102)**

```c
bool render_wilds_map_to_buffer(WILDS_DATA *pWilds, int wx, int wy,
                                CHAR_DATA *ch, int bonus_view_x, int bonus_view_y,
                                BUFFER **out_buf, int *out_width, int *out_height);
```

**Verification:** Build compiles cleanly. No functional changes — new function only.

---

### Task 6: Wire Room.Map into sentience_gmcp_update()

Connect the render functions and builder into the game loop, sending `Sentience.Room.Map` on every room change.

**Files:**
- Modify: `gmcp_sentience.c`

- [ ] **Step 1: Add Room.Map send after Room.Info send (after line ~447)**

Inside the `SENTIENCE_DIRTY_ROOM` block, after the `sentience_send_package(d, "Sentience.Room.Info", ...)` call:

```c
/* Room.Map — send pre-rendered minimap */
{
    BUFFER *map_buf = NULL;
    int map_w = 0, map_h = 0;
    bool has_map = false;

    if (ch->in_room->wilds) {
        int vp_x = get_squares_to_show_x(ch->wildview_bonus_x);
        int vp_y = get_squares_to_show_y(ch->wildview_bonus_y);
        has_map = render_wilds_map_to_buffer(ch->in_room->wilds,
            ch->in_room->x, ch->in_room->y, ch, vp_x, vp_y,
            &map_buf, &map_w, &map_h);
    } else {
        has_map = render_area_map_to_buffer(ch, ch->in_room,
            &map_buf, &map_w, &map_h);
    }

    if (has_map && map_buf) {
        sentience_room_map_input_t map_input = {
            .type     = ch->in_room->wilds ? "wilds" : "area",
            .map_text = buf_string(map_buf),
            .width    = map_w,
            .height   = map_h,
        };
        sentience_send_package(d, "Sentience.Room.Map",
            sentience_build_room_map(&map_input));
        free_buf(map_buf);
    }
}
```

- [ ] **Step 2: Add necessary includes/declarations**

Ensure `gmcp_sentience.c` can see `render_area_map_to_buffer()` (from merc.h, already included) and `render_wilds_map_to_buffer()` (from wilds.h — add `#include "wilds.h"` if not present). Also ensure `get_squares_to_show_x/y` are declared.

**Verification:** Build compiles. Connect via WebSocket, walk between rooms — `Sentience.Room.Map` JSON should appear in GMCP output with correct type, map_text, and dimensions.

---

### Task 7: Inline Suppression

Suppress inline map display when GMCP Room.Map is being sent, to avoid showing the map twice.

**Files:**
- Modify: `act_info.c`

- [ ] **Step 1: Add suppression check for area minimap (line ~2601)**

Wrap the existing `show_map_and_description()` call with a GMCP check. Before line 2601:

```c
/* Current code at line 2600-2610: */
#if 1
            if (!IS_SET(ch->comm, COMM_NOMAP) && /*!ON_SHIP(ch) &&*/
                !IS_SET(room->room_flag[0], ROOM_NOMAP) &&
                !IS_SET(room->area->area_flags, AREA_NOMAP))
                show_map_and_description(ch, room);
            else {
#endif
                send_to_char("  ", ch);
                show_room_description(ch, room);
#if 1
            }
#endif
```

Change to add GMCP suppression — if GMCP map is active, skip minimap and show description only:

```c
#if 1
            if (!IS_SET(ch->comm, COMM_NOMAP) &&
                !IS_SET(room->room_flag[0], ROOM_NOMAP) &&
                !IS_SET(room->area->area_flags, AREA_NOMAP) &&
                !(ch->desc && ch->desc->pProtocol &&
                  ch->desc->pProtocol->bGMCP &&
                  ch->desc->pProtocol->bGMCPSupport[GMCP_SUPPORT_SENTIENCE]))
                show_map_and_description(ch, room);
            else {
#endif
                send_to_char("  ", ch);
                show_room_description(ch, room);
#if 1
            }
#endif
```

- [ ] **Step 2: Add suppression check for wilderness map (line ~2621)**

Wrap the wilderness map display with the same GMCP check:

```c
/* Add to the existing condition at line 2621-2625: */
    if (room->wilds && ((automatic && ((!IS_NPC(ch) &&
        IS_SET(room->room_flag[1], ROOM_VIRTUAL_ROOM) &&
        !IS_SET(ch->comm, COMM_BRIEF)))) ||
        (!automatic && !IS_NPC(ch) &&
        IS_SET(room->room_flag[1], ROOM_VIRTUAL_ROOM))) &&
        !(ch->desc && ch->desc->pProtocol &&
          ch->desc->pProtocol->bGMCP &&
          ch->desc->pProtocol->bGMCPSupport[GMCP_SUPPORT_SENTIENCE])) {
```

**Verification:** Build compiles. Connect via WebSocket — area rooms should NOT show inline minimap (GMCP delivers it instead). Connect via telnet — inline minimap still appears normally.

**Note:** `ROOM_VIEWWILDS` rooms (line ~2633) display a distant wilderness view but are NOT area minimap rooms. Since Task 6 does not send GMCP maps for viewwilds rooms, no suppression is needed there — this is intentional, not an oversight.

---

### Task 8: Integration Testing + Cleanup

Final verification that everything works end-to-end.

- [ ] **Step 1: Run full test suite**

```bash
cd /sentience/src && ./build tests
cd /sentience && ./sent -test:gmcp
```

All GMCP tests (existing 11 + 3 new = 14) should pass.

- [ ] **Step 2: Manual WebSocket verification**

Connect via WebSocket client:
- Walk between area rooms → `Sentience.Room.Map` with `type: "area"` appears
- Walk into wilderness → `Sentience.Room.Map` with `type: "wilds"` appears
- Inline minimap is suppressed for WebSocket
- `map_text` contains valid MUD color codes
- `width` and `height` are reasonable values

- [ ] **Step 3: Manual telnet verification**

Connect via telnet:
- Walk between rooms → inline minimap still displays
- No GMCP Room.Map interference with inline display

- [ ] **Step 4: Commit**

Commit all changes with descriptive message. Push to `legacy_testport` branch (NOT main).

---

## Dependency Graph

```
Task 1 (header) ──► Task 2 (builder) ──► Task 3 (tests)
                                              │
Task 4 (area render) ──────────────────►  Task 6 (wire update)
                                              │
Task 5 (wilds render) ─────────────────►  Task 6
                                              │
                                          Task 7 (suppression)
                                              │
                                          Task 8 (integration)
```

Tasks 1→2→3 and 4, 5 can proceed in parallel. Task 6 needs 2, 4, and 5. Task 7 needs 6. Task 8 is final.
