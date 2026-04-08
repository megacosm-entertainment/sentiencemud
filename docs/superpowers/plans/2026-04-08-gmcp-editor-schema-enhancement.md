# GMCP Editor Schema Enhancement — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make all editor schemas fully self-describing so the web client can render proper forms for type-specific data, list-based content, and provide dropdown selectors for enum fields.

**Architecture:** The type tab's capture path will dispatch to per-type schema functions (paralleling apply_fn/serialize_fn). List tabs gain capture-mode paths that emit `"type":"list"` descriptors with item_schema. A new `Sentience.Editor.Schema.Update` message enables targeted tab refreshes after structural changes. Non-flag enum fields gain options via `olc_display_type()` conversions.

**Tech Stack:** C23, Jansson (JSON), custom OLC framework, GMCP protocol

**Spec:** `docs/superpowers/specs/2026-04-07-gmcp-editor-schema-enhancement-design.md`

---

## File Structure

| File | Responsibility |
|------|----------------|
| `editors/objects/oedit_types.c` | Add 32 `schema_fn` functions + extend dispatch table. New public `oedit_type_schema()`. |
| `editors/objects/oedit.c` | Type tab capture path calls `oedit_type_schema()`. Affects tab gains list capture paths. addtype/removetype trigger Schema.Update. |
| `editors/objects/oedit.c:49` | Extern declaration for `oedit_type_schema()` (alongside existing extern for `oedit_show_type_data`) |
| `gmcp_editor.c` | `gmcp_editor_build_schema_update()` + `gmcp_editor_send_schema_update()` |
| `gmcp_editor.h` | Declare Schema.Update builder + sender |
| `editors/common/olc_display.c` | `olc_display_scripts()` + `olc_display_vars()` gain capture-mode paths |
| `editors/rooms/redit.c` | Sector → `olc_display_type(sector_flags)` |
| `docs/GMCP_WEB_CLIENT_REFERENCE.md` | Schema.Update message, list type, type data examples |
| `tests/unit/olc_changeset_tests.c` | Schema function tests, Schema.Update builder test |
| `tests/data/unit/olc_changeset/` | Test JSON definitions |
| `tests/data/test_config.json` | Register new test suites |

---

### Task 1: Schema.Update GMCP Message Infrastructure

**Goal:** Add the `Sentience.Editor.Schema.Update` builder and sender to `gmcp_editor.c/.h`.

**Files:**
- Modify: `gmcp_editor.h:81-87` (add declarations after send_commit_result)
- Modify: `gmcp_editor.c:340-375` (add builder + sender after send_close)

**Context for implementer:**
- Existing pattern in `gmcp_editor.c`: each message has a `build_*` function returning `json_t*` and a `send_*` function that calls `can_send_gmcp()`, builds, and sends via `sentience_send_package()`.
- `sentience_send_package(d, "Sentience.Editor.Schema.Update", msg)` is the send call.
- `can_send_gmcp(d)` (line 311) gates all sends.
- The message format from spec:
```json
{
  "entity_id": "obj:5#3010",
  "updates": [
    { "tab": "Type", "action": "replace", "fields": [...] }
  ],
  "_v": 1
}
```

- [ ] **Step 1: Add declarations to `gmcp_editor.h`**

After the `gmcp_editor_send_commit_result` declaration (around line 82), add:

```c
/* Schema.Update — targeted tab schema refresh */
json_t *gmcp_editor_build_schema_update(const char *entity_id,
    const char *tab_name, json_t *fields);

void gmcp_editor_send_schema_update(descriptor_t *d,
    const char *entity_id, const char *tab_name,
    json_t *fields);
```

- [ ] **Step 2: Implement builder in `gmcp_editor.c`**

After `gmcp_editor_send_close` (around line 340), add:

```c
json_t *gmcp_editor_build_schema_update(const char *entity_id,
    const char *tab_name, json_t *fields)
{
    json_t *update = json_pack("{s:s, s:s, s:o}",
        "tab", tab_name,
        "action", "replace",
        "fields", fields ? fields : json_array());
    json_t *updates = json_array();
    json_array_append_new(updates, update);

    return json_pack("{s:s, s:o, s:i}",
        "entity_id", entity_id,
        "updates", updates,
        "_v", 1);
}

void gmcp_editor_send_schema_update(descriptor_t *d,
    const char *entity_id, const char *tab_name,
    json_t *fields)
{
    if (!can_send_gmcp(d) || !entity_id || !tab_name) return;

    json_t *msg = gmcp_editor_build_schema_update(entity_id, tab_name, fields);
    if (msg)
        sentience_send_package(d, "Sentience.Editor.Schema.Update", msg);
}
```

Note: `fields` ownership transfers to the builder (caller should NOT decref).

- [ ] **Step 3: Write test for Schema.Update builder**

In `tests/unit/olc_changeset_tests.c`, add test `olccs_schema_update_build`:

```c
static test_result_t test_schema_update_build(test_case_t *test)
{
    json_t *fields = json_array();
    json_t *f = json_pack("{s:s, s:s, s:s, s:s}",
        "label", "Class:", "command", "typedata/weapon/class",
        "type", "enum", "value", "sword");
    json_array_append_new(fields, f);

    json_t *msg = gmcp_editor_build_schema_update("obj:5#3010", "Type", fields);
    TEST_ASSERT(msg != NULL, "build returned NULL");
    TEST_ASSERT_STR_EQUALS(json_string_value(json_object_get(msg, "entity_id")), "obj:5#3010");

    json_t *updates = json_object_get(msg, "updates");
    TEST_ASSERT(json_is_array(updates) && json_array_size(updates) == 1, "updates array size");

    json_t *upd = json_array_get(updates, 0);
    TEST_ASSERT_STR_EQUALS(json_string_value(json_object_get(upd, "tab")), "Type");
    TEST_ASSERT_STR_EQUALS(json_string_value(json_object_get(upd, "action")), "replace");

    json_t *flds = json_object_get(upd, "fields");
    TEST_ASSERT(json_is_array(flds) && json_array_size(flds) == 1, "fields array size");

    json_decref(msg);
    return TEST_SUCCESS;
}
```

Register as `olccs_schema_update_build` in dispatcher + test_modules.h. Add JSON test definition.

- [ ] **Step 4: Build and run test**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test:olccs_schema_update
```

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add gmcp_editor.c gmcp_editor.h tests/
git commit -m "feat: add Sentience.Editor.Schema.Update GMCP message

Builder and sender for targeted tab schema refreshes.
Includes unit test for message structure.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 2: Type Schema Functions — Simple Types (16 types)

**Goal:** Add `schema_fn` to 16 simple types that have only scalar/flag fields (no widevnum, no dice, no complex lookups).

**Files:**
- Modify: `editors/objects/oedit_types.c` — add 16 schema functions, extend dispatch struct + table

**Types:** bodypart, book, cart, compass, container, food, furniture, ink, jewelry, light, map, mist, money, page, sextant, tattoo

**Context for implementer:**
- The dispatch table struct at line 51-55 currently has `type_name`, `apply_fn`, `serialize_fn`. Add `schema_fn`:
```c
typedef json_t *(*oedit_type_schema_fn)(const OBJ_INDEX_DATA *pObj);

typedef struct {
    const char             *type_name;
    oedit_type_apply_fn     apply_fn;
    oedit_type_serialize_fn serialize_fn;
    oedit_type_schema_fn    schema_fn;
} oedit_type_dispatch_t;
```
- Each schema function returns a `json_t *` array of field descriptors.
- A field descriptor is: `{"label": "...", "command": "typedata/typename/field", "type": "int"|"enum"|"flags"|"bool", "value": ..., "options": [...]}`.
- Use `olc_flag_options_json(table)` for flag/enum options — already available (declared in `olc_display.h`, line 57).
- Use `flag_string(table, value)` for current flag/enum values.
- The `command` path must match what `oedit_apply_typedata()` dispatches (e.g., `"typedata/container/weight"` for container max_weight).
- Guard pattern: `if (!IS_TYPE(pObj)) return json_null();`
- Include `olc_display.h` if not already included (it is: `../common.h` pulls it in).

**Reference: existing display for each type** (from `oedit_show_type_data()` starting line 4851):
- bodypart: parts (flags, part_flags), race (string — race lookup, no flag_type table)
- book: flags (flags, container_flags)
- cart: capacity (int), delay (int), strength (int), items (int), weightmult (int), flags (flags, cart_flags), vanish (int)
- compass: accuracy (int)
- container: weight (int), flags (flags, container_flags), items (int), weightmult (int)
- food: hunger (int), full (int), poison (bool), timer (int)
- furniture: people (int), weight (int), flags (flags, furniture_flags), heal (int), mana (int), move (int)
- ink: colour (string — built from R/G/B, not a flag_type)
- jewelry: flags (flags, jewelry_flags)
- light: duration (int)
- map: wilds (int — wildsUID), x (int), y (int), range (int), flags (flags, map_flags)
- mist: flags (flags, mist_flags)
- money: silver (int), gold (int)
- page: flags (flags, container_flags)
- sextant: (no fields — empty type)
- tattoo: flags (flags, tattoo_flags)

**Important caveats:**
- `bodypart/race` is a race_uid lookup, NOT a flag_type — emit as `"type": "string"` with the race name.
- `ink/colour` is composite RGB — emit as `"type": "string"` with the colour string.
- `map/wilds` is a wilderness UID — emit as `"type": "int"`.
- `sextant` has zero fields — return `json_array()` (empty array).

- [ ] **Step 1: Add schema_fn typedef and extend dispatch struct**

At line 51 in `oedit_types.c`, change the typedef:
```c
typedef json_t *(*oedit_type_schema_fn)(const OBJ_INDEX_DATA *pObj);

typedef struct {
    const char             *type_name;
    oedit_type_apply_fn     apply_fn;
    oedit_type_serialize_fn serialize_fn;
    oedit_type_schema_fn    schema_fn;
} oedit_type_dispatch_t;
```

- [ ] **Step 2: Implement 16 schema functions**

Example pattern (for `container_schema`):
```c
static json_t *container_schema(const OBJ_INDEX_DATA *pObj)
{
    if (!IS_CONTAINER(pObj)) return json_null();
    json_t *fields = json_array();
    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Weight:", "command", "typedata/container/weight",
        "type", "int", "value", CONTAINER(pObj)->max_weight));
    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:s, s:o}",
        "label", "Flags:", "command", "typedata/container/flags",
        "type", "flags", "value", flag_string(container_flags, CONTAINER(pObj)->flags),
        "options", olc_flag_options_json(container_flags)));
    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Items:", "command", "typedata/container/items",
        "type", "int", "value", CONTAINER(pObj)->max_items));
    json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:i}",
        "label", "Weightmult:", "command", "typedata/container/weightmult",
        "type", "int", "value", CONTAINER(pObj)->weight_multiplier));
    return fields;
}
```

Each function goes near its corresponding serialize_fn. The field names in "command" must EXACTLY match the field names used in the corresponding `*_apply_field()` function (e.g., what's passed as `field_name` and matched via `strcmp`).

- [ ] **Step 3: Update dispatch table entries for these 16 types**

Change each entry from 3-member to 4-member. Types not yet done get `NULL`:
```c
{ "bodypart",    bodypart_apply_field,    bodypart_serialize,    bodypart_schema },
{ "book",        book_apply_field,        book_serialize,        book_schema },
...
{ "weapon",      weapon_apply_field,      weapon_serialize,      NULL },  /* Task 3 */
```

- [ ] **Step 4: Build to verify compilation**

```bash
cd /sentience/src && ./build tests
```

Expected: Clean build, no errors.

- [ ] **Step 5: Commit**

```bash
git add editors/objects/oedit_types.c
git commit -m "feat: add schema functions for 16 simple object types

bodypart, book, cart, compass, container, food, furniture, ink,
jewelry, light, map, mist, money, page, sextant, tattoo.

Each emits field descriptors with type, value, command path, and
options for enum/flag fields.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 3: Type Schema Functions — Complex Types (16 types)

**Goal:** Add `schema_fn` for 16 remaining types with complex fields (widevnum, dice, enum lookups, multi-field structs).

**Files:**
- Modify: `editors/objects/oedit_types.c` — add 16 schema functions, fill in dispatch table NULLs

**Types:** armor, corpse, drink, herb, instrument, portal, scroll, seed, ship, shipmodule, telescope, tool, trade, wand, weapon, weaponcon

**Context for implementer:**

These types need special handling for certain field types:

**Weapon dice** — emit as `"type": "dice"`:
```c
json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:s}",
    "label", "Dice:", "command", "typedata/weapon/dice",
    "type", "dice", "value", formatf("%dd%d+%d",
        WEAPON(pObj)->dice_number, WEAPON(pObj)->dice_size, WEAPON(pObj)->dice_bonus)));
```

**Enum fields (weapon class, armor type, etc.)** — emit as `"type": "enum"`:
```c
json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:s, s:o}",
    "label", "Class:", "command", "typedata/weapon/class",
    "type", "enum", "value", flag_string(weapon_class, WEAPON(pObj)->weapon_type),
    "options", olc_flag_options_json(weapon_class)));
```

**Widevnum fields (corpse mobile, portal destination, shipmodule ammo)** — emit as `"type": "widevnum"`:
```c
json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:s}",
    "label", "Mobile:", "command", "typedata/corpse/mobile",
    "type", "widevnum", "value",
    widevnum_string(
        CORPSE(pObj)->mobile_area_uid > 0 ? get_area_index(CORPSE(pObj)->mobile_area_uid) : NULL,
        CORPSE(pObj)->mobile_vnum, pObj->area)));
```

**Spell fields (scroll, wand, instrument)** — emit as `"type": "string"` with the spell name:
```c
SKILL_DATA *sk = SCROLL(pObj)->spell1 > 0 ? skill_find_uid(SCROLL(pObj)->spell1) : NULL;
json_array_append_new(fields, json_pack("{s:s, s:s, s:s, s:s}",
    "label", "Spell 1:", "command", "typedata/scroll/spell1",
    "type", "string", "value", sk ? sk->name : "none"));
```

**Reference: existing display fields** (from `oedit_show_type_data()`):
- armor: type (enum, armor_types), strength (enum, armour_strength_table — NOT flag_type, use string), pierce/bash/slash/exotic (int)
- corpse: type (enum, corpse_types), resurrection (int), animation (int), parts (flags, part_flags), mobile (widevnum)
- drink: capacity (int), amount (int), liquid (string — liquid_name), poison (bool), refill (int)
- herb: food (int), immunity (flags), resistance (flags), vulnerability (flags)
- instrument: type (enum, instrument_types), mana_cost (int), beats (int), level (int)
- portal: flags (flags, gate_flags), exit_flags (flags, exit_flags), destination params (complex — param0..param4)
- scroll: level (int), spell1..spell4 (spell names)
- seed: plant (widevnum), growth (int), decay (int), type (enum, seed_types)
- ship: type (enum, ship_type), class (enum, ship_class), crew (int), cargo (int), hull (int), rooms (int), speed (int), flags (flags, ship_flags)
- shipmodule: type (enum, shipmod_type), many ints, ammo (widevnum)
- telescope: range (int), magnification (int)
- tool: type (enum, tool_types), uses (int)
- trade: type (enum, trade_types)
- wand: level (int), charges (int), spell (spell name)
- weapon: class (enum, weapon_class), dice (dice), flags (flags, weapon_type2)
- weaponcon: ammo_type (enum, weapon_class), dice (dice), range (int)

**Note:** armour_strength_table is NOT a flag_type. Emit strength as `"type": "string"` with the table name value. Same for liquid_name lookups.

**Portal is the most complex.** It has destination params that vary by flag state. Emit the individual param fields as they're staged: param0 (int), param1 (int), param2 (int), param3 (int), param4 (int). Also emit flags and exit_flags.

- [ ] **Step 1: Implement 16 schema functions**

Each near its serialize/apply functions. Follow the patterns described above.

- [ ] **Step 2: Fill remaining NULLs in dispatch table**

All 32 entries should now have non-NULL schema_fn.

- [ ] **Step 3: Build to verify**

```bash
cd /sentience/src && ./build tests
```

- [ ] **Step 4: Commit**

```bash
git add editors/objects/oedit_types.c
git commit -m "feat: add schema functions for 16 complex object types

armor, corpse, drink, herb, instrument, portal, scroll, seed,
ship, shipmodule, telescope, tool, trade, wand, weapon, weaponcon.

Handles widevnum, dice, spell lookup, and complex enum fields.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 4: Type Tab Capture Path + oedit_type_schema() Dispatcher

**Goal:** Wire the type schema functions into oedit's Type tab capture path, so `Editor.Open` includes type field descriptors.

**Files:**
- Modify: `editors/objects/oedit_types.c` — add public `oedit_type_schema()` dispatcher
- Modify: `editors/objects/oedit.c:1220-1225` — update `oedit_show_type_tab` to call schema in capture mode
- Modify: `editors/objects/oedit.c:49` — update extern declaration

**Context for implementer:**
- `oedit_show_type_tab` (line 1220) currently just calls `oedit_show_type_data(pObj, ctx->buffer)`.
- In capture mode (`ctx->capture_mode == true`), it should instead call `oedit_type_schema(pObj)` and append results to `ctx->captured_fields`.
- The schema function iterates ALL active types on the object and groups results with section markers.

- [ ] **Step 1: Add `oedit_type_schema()` public dispatcher**

In `oedit_types.c`, after `oedit_serialize_typedata()` (around line 850):

```c
json_t *oedit_type_schema(const OBJ_INDEX_DATA *pObj)
{
    json_t *fields = json_array();
    if (!pObj) return fields;

    for (int i = 0; type_dispatch[i].type_name; i++) {
        if (!type_dispatch[i].schema_fn) continue;
        json_t *type_fields = type_dispatch[i].schema_fn(pObj);
        if (!type_fields || json_is_null(type_fields)) {
            json_decref(type_fields);
            continue;
        }
        /* Add section marker before this type's fields */
        json_t *section = json_pack("{s:s, s:s}",
            "type", "section",
            "label", type_dispatch[i].type_name);
        json_array_append_new(fields, section);
        /* Append all field descriptors from this type */
        for (size_t j = 0; j < json_array_size(type_fields); j++) {
            json_array_append(fields, json_array_get(type_fields, j));
        }
        json_decref(type_fields);
    }

    return fields;
}
```

- [ ] **Step 2: Update extern declaration in oedit.c**

At line 49 of oedit.c, add or update:
```c
extern json_t *oedit_type_schema(const OBJ_INDEX_DATA *pObj);
```

- [ ] **Step 3: Update `oedit_show_type_tab` for capture mode**

Replace the function at line 1220:
```c
static void oedit_show_type_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)pEdit;

    if (ctx && ctx->capture_mode && ctx->captured_fields) {
        json_t *type_fields = oedit_type_schema(pObj);
        if (type_fields && !json_is_null(type_fields)) {
            for (size_t i = 0; i < json_array_size(type_fields); i++) {
                json_array_append(ctx->captured_fields, json_array_get(type_fields, i));
            }
            json_decref(type_fields);
        }
        return;
    }

    oedit_show_type_data(pObj, ctx->buffer);
}
```

- [ ] **Step 4: Write test for type schema dispatch**

In `tests/unit/olc_changeset_tests.c`, add test `olccs_type_schema_dispatch`:
- Create a mock OBJ_INDEX_DATA with weapon type allocated
- Call `oedit_type_schema(pObj)`
- Verify returned array contains section marker + weapon fields (class, flags, dice)
- Verify field command paths start with `typedata/weapon/`

- [ ] **Step 5: Build and run tests**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test:olccs_
```

Expected: All existing OLC tests pass + new test passes.

- [ ] **Step 6: Commit**

```bash
git add editors/objects/oedit_types.c editors/objects/oedit.c tests/
git commit -m "feat: wire type schema into oedit Type tab capture path

oedit_type_schema() dispatches to per-type schema_fn and groups
results with section markers. Type tab now emits field descriptors
in capture mode for Editor.Open.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 5: Oedit Affects Tab — List Capture Paths

**Goal:** Add capture-mode paths to oedit's Affects tab so affects, IRV, spells, catalysts, and waypoints emit `"type":"list"` descriptors.

**Files:**
- Modify: `editors/objects/oedit.c:1035-1177` — update `oedit_show_affects_tab`

**Context for implementer:**
- The affects tab currently renders raw text via `add_buf(ctx->buffer, ...)` for all content.
- In capture mode, it should emit structured list descriptors instead.
- Each list section needs: `type`, `label`, `command`, `item_type`, `items` array, `add_command`, `del_command`, `item_schema` array.
- **Note on other editors:** medit "affects" are flag bitvectors (`affect_flags`, `affect2_flags`) rendered via `olc_display_flags()` which already captures correctly with options. Only oedit has linked-list affects that need list capture. Scripts/vars in all editors are covered by Task 6.
- The key tables for options:
  - Affect where: `apply_types` table has TO_OBJECT entries
  - Affect location: `apply_flags` table
  - IRV types: `imm_flags` table
  - Catalyst types: `catalyst_types` table
  - Spell names come from `skill_find_uid()`

**Item schemas (from spec):**
```json
// TO_OBJECT affects
{ "item_schema": [
    {"field": "where", "type": "enum", "options": [...]},
    {"field": "location", "type": "enum", "options": [...]},
    {"field": "modifier", "type": "int"},
    {"field": "random", "type": "int", "min": 0, "max": 100}
]}

// IRV affects
{ "item_schema": [
    {"field": "where", "type": "enum", "options": ["immune","resist","vuln"]},
    {"field": "flags", "type": "flags", "options": [...]},
    {"field": "random", "type": "int", "min": 0, "max": 100}
]}

// Spells
{ "item_schema": [
    {"field": "spell", "type": "string"},
    {"field": "level", "type": "int"},
    {"field": "random", "type": "int", "min": 0, "max": 100}
]}

// Catalysts
{ "item_schema": [
    {"field": "type", "type": "enum", "options": [...]},
    {"field": "level", "type": "int"},
    {"field": "modifier", "type": "int"},
    {"field": "random", "type": "int", "min": 0, "max": 100},
    {"field": "name", "type": "string"}
]}
```

- [ ] **Step 1: Add capture-mode guard at top of affects tab**

At the top of `oedit_show_affects_tab`, after getting `pObj`:
```c
if (ctx && ctx->capture_mode && ctx->captured_fields) {
    /* Build list descriptors for all affect sections */
    oedit_capture_affects(ctx, pObj);
    return;
}
```

- [ ] **Step 2: Implement `oedit_capture_affects()` helper**

Static function above `oedit_show_affects_tab`. Builds 5 list descriptors:

1. **TO_OBJECT affects** — iterate `pObj->affected` where `paf->where == TO_OBJECT`
2. **IRV affects** — iterate `pObj->affected` where `paf->where` is TO_IMMUNE/TO_RESIST/TO_VULN
3. **Spells** — iterate `pObj->spells`
4. **Catalysts** — iterate `pObj->catalyst`
5. **Waypoints** — iterate `pObj->waypoints` LLIST

Each builds a JSON object:
```c
json_t *list = json_pack("{s:s, s:s, s:s, s:s, s:o, s:s, s:s, s:o}",
    "label", "Affects",
    "command", "affects",
    "type", "list",
    "item_type", "affect",
    "items", items_array,
    "add_command", "addaffect",
    "del_command", "delaffect",
    "item_schema", schema_array);
json_array_append_new(ctx->captured_fields, list);
```

The `items` array is populated by iterating the linked list and packing each item's data.
The `item_schema` is a static descriptor of what fields each item has.

- [ ] **Step 3: Build and test**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test:olccs_
```

- [ ] **Step 4: Commit**

```bash
git add editors/objects/oedit.c
git commit -m "feat: oedit affects tab emits list descriptors in capture mode

TO_OBJECT affects, IRV, spells, catalysts, and waypoints all
produce structured list descriptors with item_schema for the
web client form builder.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 6: Scripts Tab Capture Paths (olc_display_scripts + olc_display_vars)

**Goal:** Add capture-mode paths to `olc_display_scripts()` and `olc_display_vars()` so all editors' Scripts tabs emit structured data.

**Files:**
- Modify: `editors/common/olc_display.c:859-905` — add capture-mode paths
- Modify: `editors/objects/oedit.c:1179-1218` — add capture path for quest v2 list in scripts tab

**Context for implementer:**
- `olc_display_scripts()` (line 859) currently checks `!ctx->buffer` and returns, but does NOT check `capture_mode`. It writes raw text.
- `olc_display_vars()` (line 886) same — writes raw text, no capture path.
- Both need: at top, if `ctx->capture_mode && ctx->captured_fields`, build list descriptor and return.
- These functions are called from ALL editors' scripts tabs (oedit, medit, redit, aedit), so this fix benefits all editors.

**Scripts list descriptor:**
```json
{
    "label": "ObjProg Vnum",
    "command": "oprogs",
    "type": "list",
    "item_type": "script",
    "items": [{"index": 0, "vnum": "5#1000"}],
    "add_command": "addoprog",
    "del_command": "deloprog",
    "item_schema": [
        {"field": "vnum", "type": "widevnum"}
    ]
}
```

**Variables list descriptor:**
```json
{
    "label": "Variables",
    "command": "vars",
    "type": "list",
    "item_type": "variable",
    "items": [...],
    "add_command": "varset",
    "del_command": "varclear",
    "item_schema": [
        {"field": "name", "type": "string"},
        {"field": "value", "type": "string"}
    ]
}
```

**Quest v2 in oedit scripts tab:**
The quests section at oedit.c:1189-1217 also needs a capture path. It uses `add_buf` directly.

- [ ] **Step 1: Add capture-mode path to `olc_display_scripts()`**

At the start of the function (after the NULL checks), add:
```c
if (ctx && ctx->capture_mode && ctx->captured_fields) {
    /* Build script/prog list descriptor */
    json_t *items = json_array();
    if (progs && *progs) {
        /* iterate progs list, build items array */
    }
    json_t *schema = json_array();
    json_array_append_new(schema, json_pack("{s:s, s:s}", "field", "vnum", "type", "widevnum"));
    json_t *list = json_pack("{s:s, s:s, s:s, s:s, s:o, s:s, s:s, s:o}",
        "label", title, "command", add_cmd ? add_cmd : "scripts",
        "type", "list", "item_type", "script",
        "items", items, "add_command", add_cmd ? add_cmd : "",
        "del_command", del_cmd ? del_cmd : "", "item_schema", schema);
    json_array_append_new(ctx->captured_fields, list);
    return;
}
```

- [ ] **Step 2: Add capture-mode path to `olc_display_vars()`**

Same pattern — build variable list descriptor.

- [ ] **Step 3: Add capture path for quest v2 in oedit scripts tab**

In `oedit_show_scripts_tab`, before the quest v2 rendering block, add a capture-mode guard.

- [ ] **Step 4: Build and test**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test:olccs_
```

- [ ] **Step 5: Commit**

```bash
git add editors/common/olc_display.c editors/objects/oedit.c
git commit -m "feat: scripts and vars emit list descriptors in capture mode

olc_display_scripts() and olc_display_vars() now produce structured
list descriptors when in capture mode. Benefits all 4 editors.
Quest v2 list in oedit scripts tab also gains capture path.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 7: Schema.Update Triggers (commit/revert)

**Goal:** Wire Schema.Update sends into commit and revert handlers to keep the web client's schema current after structural changes.

**Files:**
- Modify: `gmcp_editor.c:589-633` — commit handler
- Modify: `gmcp_editor.c:640-669` — revert handler

**Context for implementer:**

Schema.Update is sent from the **commit** handler (not addtype/removetype staging), because structural changes only take effect on commit. During staging, types aren't allocated yet — the web client sees staged changes via `Editor.Field` messages.

**Approach:**
1. After **commit** succeeds: call `olc_schema_capture()` to regenerate all tabs, send `Schema.Update` for each tab. This is infrequent (commits don't happen continuously) so overhead is negligible.
2. After **revert** of a typedata prefix: send `Schema.Update` for the Type tab (type structure may have changed).
3. **addtype/removetype staging**: do NOT send Schema.Update (type isn't allocated yet).

- [ ] **Step 1: Add Schema.Update to commit handler**

In `handle_editor_commit` (gmcp_editor.c), after the successful commit section (after `gmcp_editor_send_commit_result`):

```c
/* Send schema update after structural commit */
if (applied > 0 && def->editor_type == ED_OBJECT) {
    json_t *tabs = olc_schema_capture(d->character, def, d->pEdit, cs);
    if (tabs) {
        for (size_t t = 0; t < json_array_size(tabs); t++) {
            json_t *tab = json_array_get(tabs, t);
            const char *tname = json_string_value(json_object_get(tab, "name"));
            json_t *flds = json_object_get(tab, "fields");
            if (tname && flds)
                gmcp_editor_send_schema_update(d, entity_id, tname, json_incref(flds));
        }
        json_decref(tabs);
    }
}
```

Note: `json_incref(flds)` because `send_schema_update` takes ownership and `tabs` decref would free it. Send one `Schema.Update` per tab. Start with oedit only (it has type data); extend to medit/redit/aedit later.

- [ ] **Step 2: Add Schema.Update to revert handler for typedata**

In `handle_editor_revert`, after reverting a typedata prefix, send Schema.Update for Type tab. Check if the reverted field starts with "typedata".

- [ ] **Step 3: Build and test**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test:olccs_
```

- [ ] **Step 4: Commit**

```bash
git add gmcp_editor.c
git commit -m "feat: send Schema.Update after commit and typedata revert

Object editor commits now resend full tab schemas via
Sentience.Editor.Schema.Update. Reverting typedata fields
refreshes the Type tab schema.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 8: Enum Options Enrichment

**Goal:** Convert remaining `olc_display_string()`/`olc_display_number()` calls to `olc_display_type()` where flag_type tables exist.

**Files:**
- Modify: `editors/rooms/redit.c:449` — Sector field

**Context for implementer:**

**Confirmed candidate** (verified against codebase):

1. **redit.c line 449** — `olc_display_string(ctx, theme, "Sector:", "sector", sector_name(room_rs_sector_type(pRoom)))`
   → Change to: `olc_display_type(ctx, theme, "Sector:", "sector", sector_flags, room_rs_sector_type(pRoom))`
   - `sector_flags` is a valid `const struct flag_type` table (declared in tables.c:1239)
   - The value is `room_rs_sector_type(pRoom)` which returns an int matching sector_flags bits

**NOT candidates** (verified — already correct or incompatible):
- medit Sex (line 601) — already uses `olc_display_type()` ✓
- medit Size (line 603) — already uses `olc_display_type()` ✓
- medit Dam Type (line 653) — `attack_table` is `struct attack_type`, NOT `flag_type`. Skip.
- oedit Type (line 944) — composite display ("weapon + armor"), not a single enum. Leave as-is.
- oedit Material — `material_type` is a struct, not flag_type. Skip.
- aedit — already uses `olc_display_type()` for all enum fields ✓
- redit.c line 798 — second Sector display with NULL command (display-only). Leave as-is.

- [ ] **Step 1: Convert redit Sector to olc_display_type**

In `redit.c`, change line 449 from:
```c
olc_display_string(ctx, theme, "Sector:", "sector",
    sector_name(room_rs_sector_type(pRoom)));
```
to:
```c
olc_display_type(ctx, theme, "Sector:", "sector",
    sector_flags, room_rs_sector_type(pRoom));
```

Ensure `sector_flags` is accessible (it's declared as `extern const struct flag_type sector_flags[]` in `tables.h`).

- [ ] **Step 2: Build and test**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test
```

Expected: Clean build, all tests pass. Verify no telnet display regression by checking that the sector name still renders correctly (flag_name on sector_flags should return the same string as sector_name).

- [ ] **Step 3: Commit**

```bash
git add editors/rooms/redit.c
git commit -m "feat: redit Sector field emits enum with options in schema

Convert olc_display_string to olc_display_type for the Sector
field, providing dropdown options to the web client.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 9: Reference Doc Updates

**Goal:** Update `docs/GMCP_WEB_CLIENT_REFERENCE.md` with Schema.Update documentation, list type, and type data schema examples.

**Files:**
- Modify: `docs/GMCP_WEB_CLIENT_REFERENCE.md`

**Context for implementer:**
- The Editor section starts around line 1675.
- Existing messages documented: Editor.Open, State, Field, Close, Error, Set, Commit, Revert, Request, CommitResult, StringEdit.*, Draft.*
- Add in appropriate places:

1. **New Field Schema Type: "list"** — add to the Field Schema Types table:
   - Type: `list`
   - Description: Contains array of items with add/remove commands and item_schema
   - Fields: `item_type`, `items`, `add_command`, `del_command`, `item_schema`

2. **New Field Schema Type: "dice"** — if not already documented:
   - Type: `dice`
   - Description: Dice expression like "3d6+2"

3. **New Field Schema Type: "widevnum"** — if not already documented

4. **New Message: Sentience.Editor.Schema.Update** — full documentation:
   - Direction: Server → Client
   - Purpose: Targeted tab schema refresh after structural changes
   - Format with example JSON
   - Trigger conditions (commit, revert)

5. **Type data schema examples** — show how Type tab includes `typedata/` command paths with section markers

6. **Update Editor session lifecycle** — add Schema.Update after Commit in the flow

- [ ] **Step 1: Add list, dice, widevnum to Field Schema Types table**

Find the Field Schema Types table and add rows.

- [ ] **Step 2: Add Schema.Update message documentation**

Add a new section after CommitResult or in the Server→Client messages area.

- [ ] **Step 3: Add type data schema examples**

Show how Type tab fields look in the schema with section markers and typedata/ command paths.

- [ ] **Step 4: Update lifecycle diagram**

Add Schema.Update arrow after CommitResult in the session flow.

- [ ] **Step 5: Commit**

```bash
git add docs/GMCP_WEB_CLIENT_REFERENCE.md
git commit -m "docs: add Schema.Update, list type, and type data schema docs

Document Sentience.Editor.Schema.Update message, list/dice/widevnum
field types, type data schema examples, and updated lifecycle flow.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 10: Integration Test + Final Verification

**Goal:** Add integration test for type schema in Editor.Open, run full test suite, verify clean build.

**Files:**
- Modify: `tests/unit/olc_changeset_tests.c` — add integration test
- Modify: `tests/data/unit/olc_changeset/` — add test JSON
- Modify: `tests/data/test_config.json` — register suite

**Test: `olccs_type_schema_open`**
- Creates a test object with weapon + armor types
- Calls `oedit_type_schema(pObj)` 
- Verifies section markers present for both types
- Verifies weapon fields: class (enum with options), flags (flags with options), dice (dice)
- Verifies armor fields: type (enum), pierce/bash/slash/exotic (int), strength (string)
- Verifies field command paths match `typedata/{type}/{field}` format

- [ ] **Step 1: Write integration test**

- [ ] **Step 2: Build and run full test suite**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test
```

Expected: 613+ total tests, all OLC tests pass, no regressions.

- [ ] **Step 3: Run OLC-specific tests**

```bash
cd /sentience && ./sent -test:olccs_
```

Expected: 43+ tests, all pass.

- [ ] **Step 4: Clean rebuild**

```bash
cd /sentience/src && ./build clean tests
```

Expected: No warnings, no errors.

- [ ] **Step 5: Commit**

```bash
git add tests/
git commit -m "test: integration test for type schema in Editor.Open

Verifies weapon + armor schema output with section markers,
field types, command paths, and options arrays.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Notes

- **File size consideration:** `oedit_types.c` is already 5398 lines. Adding 32 schema functions (~10-30 lines each) will add ~500-800 lines. This is acceptable since the file is already organized by type and each function cluster (apply + serialize + schema) stays together.

- **Test baseline:** 613 total, 597 pass, 2 known failures, 14 skipped (43/43 OLC tests pass). Any new failures are regressions.

- **Build commands:**
  ```bash
  cd /sentience/src && ./build tests        # Build with test support
  ./install debug                           # Create symlink
  cd /sentience && ./sent -test             # Run all tests
  ./sent -test:olccs_                       # Run OLC changeset tests
  ```

- **Key header files to include:**
  - `<jansson.h>` — JSON manipulation
  - `editors/common/olc_display.h` — `olc_flag_options_json()`, `olc_display_type()`
  - `gmcp_editor.h` — Schema.Update sender
  - `tables.h` — flag tables (weapon_class, armor_types, etc.)

- **Flag table access pattern:**
  ```c
  extern const struct flag_type weapon_class[];  // declared in tables.h
  olc_flag_options_json(weapon_class);           // returns json_t* array of settable option names
  flag_string(weapon_class, value);              // returns current value as string
  ```
