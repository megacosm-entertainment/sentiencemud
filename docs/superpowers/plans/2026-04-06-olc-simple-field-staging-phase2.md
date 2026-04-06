# OLC Simple Field Staging (Phase 2) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Convert ~48 remaining simple/scalar OLC editor commands to use staged changesets, adding new infrastructure helpers (`olc_cmd_long`, `olc_cmd_bitvector`, `olc_cmd_dice`) and per-editor apply functions.

**Architecture:** Each editor command is converted from direct field manipulation to staged recording via `olc_cmd_*` helpers. New infrastructure handles long integers, multi-bank bitvectors, and dice formulae. Custom apply functions handle side effects at commit time. Every converted field gets a handler table entry mapping label → apply function.

**Tech Stack:** C23, Jansson (JSON), custom OLC framework, Ninja/CMake build

**Design Spec:** `docs/specs/2026-04-06-olc-simple-field-staging-design.md`

**Build/Test Commands:**
```bash
cd /sentience/src && ./build tests        # Build with test support
./install debug                           # Create symlink
cd /sentience && ./sent -test             # Run all tests (baseline: 601 total, 585 pass, 2 known failures, 14 skipped)
./sent -test:olccs_                       # Run OLC changeset tests
```

---

## File Map

### Infrastructure (Tasks 1–3)

| File | Action | Purpose |
|------|--------|---------|
| `editors/common/olc_changeset.h` | Modify | Add `OLC_FIELD_LONG` to `olc_field_type_t` enum |
| `editors/common/olc_field_handlers.h` | Modify | Add `olc_apply_generic_long`, `olc_apply_generic_dice`, `OLC_FIELD_APPLY_LONG`, `OLC_FIELD_APPLY_DICE` |
| `editors/common/olc_field_handlers.c` | Modify | Implement `olc_apply_generic_long`, `olc_apply_generic_dice` |
| `editors/common/olc_commands.h` | Modify | Declare `olc_cmd_long`, `olc_cmd_bitvector`, `olc_cmd_dice` |
| `editors/common/olc_commands.c` | Modify | Implement `olc_cmd_long`, `olc_cmd_bitvector`, `olc_cmd_dice` |
| `editors/common/olc_staged.h` | Modify | Declare `olc_staged_long` |
| `editors/common/olc_staged.c` | Modify | Implement `olc_staged_long` |

### Tests (Task 4)

| File | Action | Purpose |
|------|--------|---------|
| `tests/unit/olc_changeset_tests.c` | Modify | Add tests for long apply, dice apply, bitvector staging |
| `tests/data/unit/olc_changeset_unit_tests.json` | Modify | Add new test case entries |

### Editor Conversions (Tasks 5–9)

| File | Action | Functions |
|------|--------|-----------|
| `editors/objects/oedit.c` | Modify | 10 functions: cost, level, persist, sign, parent, fragility, extra, wear, varset, varclear |
| `editors/mobiles/medit.c` | Modify | 19 functions split across Tasks 6–7 |
| `editors/areas/aedit.c` | Modify | 11 functions: wilds, areawho, placetype, file, recall, airshipland, postoffice, vnum, levels, varset, varclear |
| `editors/rooms/redit.c` | Modify | 8 functions: locale, persist, recall, sector, region, parent, varset, varclear |

---

## Critical Field Type Reference

These field types were verified against `merc.h` and differ from what naive inspection might suggest:

| Field | Actual Type | Location in merc.h |
|-------|-------------|-------------------|
| `OBJ_INDEX_DATA.cost` | `long` | line 6099 |
| `MOB_INDEX_DATA.wealth` | `long` | line 4303 |
| `MOB_INDEX_DATA.move` | `long` | line 4309 |
| `MOB_INDEX_DATA.ac[4]` | `int16_t[4]` | line 4292 |
| `MOB_INDEX_DATA.start_pos` | `int16_t` | line 4298 |
| `MOB_INDEX_DATA.default_pos` | `int16_t` | line 4299 |
| `MOB_INDEX_DATA.level` | `int16_t` | line 4288 |
| `AREA_DATA.min_vnum` | `long` | line 6798 |
| `AREA_DATA.max_vnum` | `long` | line 6799 |
| `AREA_DATA.min_level` | `int16_t` | line 6796 |
| `AREA_DATA.max_level` | `int16_t` | line 6797 |
| `AREA_DATA.place_flags` | `long` | line 6831 |
| `AREA_DATA.area_who` | `int` | line 6828 |
| `AREA_DATA.wilds_uid` | `long` | line 6807 |
| `ROOM_INDEX_DATA.locale` | `long` | line 7701 |
| `DICE_DATA` | `{int number, int size, int bonus, long last_roll}` | line 511 |

---

## Conversion Pattern Quick-Reference

Every conversion follows one of these patterns. The subagent should match each function to its pattern.

### Pattern A — Direct `olc_cmd_*` Replacement
The function body reduces to a single `olc_cmd_*` call. Example:
```c
OEDIT(oedit_cost) {
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    return olc_cmd_long(ch, argument, "Cost", NULL,
        &pObj->cost, 0, LONG_MAX, NULL, NULL);
}
```
Handler: `OLC_FIELD_APPLY_LONG(oedit_apply_cost, OBJ_INDEX_DATA, cost)` + table entry.

### Pattern B — Pre-Validation Then `olc_cmd_*`
Validate input first, then delegate to helper. Example:
```c
OEDIT(oedit_wear) {
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    // ... existing validation (slot conflicts, type checks) ...
    return olc_cmd_flag_toggle(ch, argument, "Wear", NULL,
        &pObj->wear_flags, wear_flags, NULL, NULL);
}
```

### Pattern C — Side-Effect Apply
Stage via `olc_cmd_bool` or `olc_cmd_number`. Custom apply function runs side effects at commit time:
```c
static bool oedit_apply_persist(void *entity, olc_pending_change_t *change) {
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    if (!olc_apply_generic_bool(&pObj->persist, change))
        return false;
    if (pObj->persist)
        use_imp_sig(NULL, pObj);
    return true;
}
```

### Pattern D — Level Cascade
Stage level via helper. Apply function recalculates derived fields:
```c
static bool oedit_apply_level(void *entity, olc_pending_change_t *change) {
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    if (!olc_apply_generic_int16(&pObj->level, change))
        return false;
    pObj->points = object_point_value(pObj);
    // weapon dice / armor recalculation as needed
    return true;
}
```

### Pattern E — Multi-Field Command
Command calls `olc_cmd_*` twice (once per sub-field). Each sub-field gets its own handler entry:
```c
MEDIT(medit_position) {
    // parse "start <pos>" vs "default <pos>"
    // call olc_cmd_type_set_i16 for appropriate field(s)
}
```

### Pattern F — WNUM Reference
Validate widevnum, stage raw string. Custom apply resolves reference at commit:
```c
// Stage: olc_changeset_add_change(cs, "Parent", OLC_FIELD_STRING, old_json, new_json)
// Apply: parse widevnum, resolve entity pointer, set parent_load + parent_wnum + parent
```

### Pattern G — Variable Set/Clear
Stage with field_path `"var/<keyname>"`. Custom apply calls `olc_varset()`/`olc_varclear()`:
```c
// Stage: olc_changeset_add_change(cs, "var/myvar", OLC_FIELD_STRING, old_json, new_json)
// Apply: olc_varset(&pEntity->index_vars, ...) at commit time
```

### Pattern H — Sign (Creator Signature)
Permission check in command, stage via `olc_cmd_string`:
```c
OEDIT(oedit_sign) {
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    if (ch->tot_level < MAX_LEVEL) { /* reject */ return false; }
    return olc_cmd_string(ch, ch->name, "Signature", NULL,
        &pObj->imp_sig, OLC_STR_DEFAULT, NULL, NULL);
}
```

---

## Task 1: Infrastructure — `olc_cmd_long` + `OLC_FIELD_LONG`

**Files:**
- Modify: `editors/common/olc_changeset.h`
- Modify: `editors/common/olc_field_handlers.h`
- Modify: `editors/common/olc_field_handlers.c`
- Modify: `editors/common/olc_commands.h`
- Modify: `editors/common/olc_commands.c`
- Modify: `editors/common/olc_staged.h`
- Modify: `editors/common/olc_staged.c`

### Steps

- [ ] **Step 1: Add `OLC_FIELD_LONG` to the field type enum**

In `editors/common/olc_changeset.h`, add `OLC_FIELD_LONG` after `OLC_FIELD_TYPE_DATA`:

```c
typedef enum {
    OLC_FIELD_STRING,
    OLC_FIELD_INT,
    OLC_FIELD_INT16,
    OLC_FIELD_BOOL,
    OLC_FIELD_FLAGS,
    OLC_FIELD_MULTIFLAGS,
    OLC_FIELD_WIDEVNUM,
    OLC_FIELD_EXIT,
    OLC_FIELD_EMBEDDED,
    OLC_FIELD_LIST_ADD,
    OLC_FIELD_LIST_REMOVE,
    OLC_FIELD_LIST_UPDATE,
    OLC_FIELD_MULTILINE,
    OLC_FIELD_TYPE_DATA,
    OLC_FIELD_LONG          /* <-- NEW */
} olc_field_type_t;
```

- [ ] **Step 2: Add generic apply function and macro to field handlers**

In `editors/common/olc_field_handlers.h`, after the existing declarations, add:

```c
/* Generic apply for long fields */
bool olc_apply_generic_long(long *field_ptr, olc_pending_change_t *change);

/* Macro for generating long field apply functions */
#define OLC_FIELD_APPLY_LONG(func_name, entity_type, member) \
    static bool func_name(void *entity, olc_pending_change_t *change) { \
        return olc_apply_generic_long(&((entity_type *)entity)->member, change); \
    }
```

In `editors/common/olc_field_handlers.c`, implement the generic apply. Follow the pattern of `olc_apply_generic_int` but use `long` and `json_integer_value` (which returns `json_int_t` = `long long`):

```c
bool olc_apply_generic_long(long *field_ptr, olc_pending_change_t *change) {
    if (!field_ptr || !change || !change->new_value) return false;
    if (!json_is_integer(change->new_value)) return false;
    *field_ptr = (long)json_integer_value(change->new_value);
    return true;
}
```

- [ ] **Step 3: Add `olc_staged_long` preview function**

In `editors/common/olc_staged.h`, add after `olc_staged_bool`:

```c
/** Returns staged long if pending, otherwise live value. */
long olc_staged_long(olc_changeset_t *cs, const char *field, long live);
```

In `editors/common/olc_staged.c`, implement following the pattern of `olc_staged_int`:

```c
long olc_staged_long(olc_changeset_t *cs, const char *field, long live) {
    if (!cs) return live;
    olc_pending_change_t *ch = olc_changeset_find_change(cs, field);
    if (!ch || !ch->new_value || !json_is_integer(ch->new_value))
        return live;
    return (long)json_integer_value(ch->new_value);
}
```

- [ ] **Step 4: Implement `olc_cmd_long` in commands**

In `editors/common/olc_commands.h`, add the declaration after `olc_cmd_number_i16`:

```c
/**
 * Set a long integer field with range validation.
 * Same as olc_cmd_number() but for long fields.
 */
bool olc_cmd_long(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, long *field_ptr, long min_val, long max_val,
    void *ctx, olc_cmd_record_fn record_fn);
```

In `editors/common/olc_commands.c`, implement by copying `olc_cmd_number` and adapting:
- Use `atol()` instead of `atoi()`
- Use `long` for value variable
- Use `OLC_FIELD_LONG` instead of `OLC_FIELD_INT`
- Use `"long"` for GMCP type string
- Use `%ld` format specifier
- Range check against `long min_val, long max_val`

The staged mode block pattern is identical to `olc_cmd_number` — find editor def, get changeset, check limits, create JSON integers, call `olc_changeset_add_change`, send `[STAGED]` feedback, call `notify_field_change`.

- [ ] **Step 5: Build and verify compilation**

```bash
cd /sentience/src && ./build tests
```

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "feat(olc): add olc_cmd_long infrastructure for long integer fields

Add OLC_FIELD_LONG enum value, olc_apply_generic_long(), OLC_FIELD_APPLY_LONG
macro, olc_staged_long() preview function, and olc_cmd_long() command helper.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 2: Infrastructure — `olc_cmd_bitvector`

**Files:**
- Modify: `editors/common/olc_commands.h`
- Modify: `editors/common/olc_commands.c`

**Context:** `bitvector_lookup()` is in `bit.c` with signature:
```c
bool bitvector_lookup(char *argument, int nbanks, long *banks, ...)
```
The variadic args are `const struct flag_type *` tables (one per bank). It writes toggle bits into the `banks` array.

The existing `oedit_apply_extra` and `oedit_serialize_extra` already handle multi-bank flag apply/serialize for oedit extra flags. We need similar apply functions for medit act/affect. Those are editor-specific and will be written in Tasks 6–7. This task only adds the shared staging helper.

### Steps

- [ ] **Step 1: Design the `olc_cmd_bitvector` interface**

The helper needs to handle variable numbers of banks and flag tables. Since C variadic functions can't be forwarded easily, the approach is:

1. The **command function** (e.g., `oedit_extra`, `medit_act`) calls `bitvector_lookup()` itself to get the toggle bits
2. Then calls a new helper `olc_stage_bitvector()` to stage the toggled result
3. In non-staged mode, the command toggles directly (existing behavior)

This avoids wrapping variadic args. The helper signature:

```c
/**
 * Stage a multi-bank bitvector toggle.
 *
 * Caller has already called bitvector_lookup() to get toggle bits.
 * This function reads current staged values, XORs toggle bits in,
 * and stages the result as a JSON integer array.
 *
 * @param ch        Character making the change
 * @param label     Field name for changeset/GMCP
 * @param banks     Live entity bank values (array of nbanks longs)
 * @param toggle    Toggle bits from bitvector_lookup (array of nbanks longs)
 * @param nbanks    Number of banks
 * @return true if change was staged (false = no-op or error)
 */
bool olc_stage_bitvector(CHAR_DATA *ch, const char *label,
    long *banks, long *toggle, int nbanks);
```

- [ ] **Step 2: Declare in header**

In `editors/common/olc_commands.h`, add the declaration.

- [ ] **Step 3: Implement `olc_stage_bitvector`**

In `editors/common/olc_commands.c`:

```c
bool olc_stage_bitvector(CHAR_DATA *ch, const char *label,
    long *banks, long *toggle, int nbanks)
{
    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    if (!edef || edef->change_mode != OLC_CHANGE_STAGED)
        return false;

    olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
    if (!cs) return false;
    if (!olc_check_staging_limits(ch, cs)) return false;

    /* Build old value: current live banks */
    json_t *old_val = json_array();
    for (int i = 0; i < nbanks; i++)
        json_array_append_new(old_val, json_integer(banks[i]));

    /* Compute new value: staged-current XOR toggle */
    json_t *new_val = json_array();
    for (int i = 0; i < nbanks; i++) {
        /* Read current effective value (may already have pending change) */
        json_t *pending = olc_staged_json(cs, label);
        long current;
        if (pending && json_is_array(pending) && (int)json_array_size(pending) > i)
            current = (long)json_integer_value(json_array_get(pending, i));
        else
            current = banks[i];
        json_array_append_new(new_val, json_integer(current ^ toggle[i]));
    }

    olc_pending_change_t *result = olc_changeset_add_change(
        cs, label, OLC_FIELD_MULTIFLAGS, old_val, new_val);
    json_decref(old_val);
    json_decref(new_val);

    if (result)
        printf_to_char(ch, "{G[STAGED]{x %s flags toggled.\n\r", label);
    else
        printf_to_char(ch, "%s reverted to original value.\n\r", label);

    notify_field_change(cs, ch, label,
        result ? result->new_value : json_null(), "bitvector", result != NULL);
    return result != NULL;
}
```

**Important:** The `notify_field_change` call requires access to the static function. Since it's `static` in `olc_commands.c`, implementing `olc_stage_bitvector` in the same file is correct.

- [ ] **Step 4: Build and verify**

```bash
cd /sentience/src && ./build tests
```

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat(olc): add olc_stage_bitvector for multi-bank flag staging

Staging helper for bitvector fields (extra flags, act flags, affect flags).
Computes toggled values against current staged state and records as JSON
integer array. Each editor provides its own apply function.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 3: Infrastructure — `olc_cmd_dice`

**Files:**
- Modify: `editors/common/olc_field_handlers.h`
- Modify: `editors/common/olc_field_handlers.c`
- Modify: `editors/common/olc_commands.h`
- Modify: `editors/common/olc_commands.c`

**Context:** `DICE_DATA` is defined in merc.h as:
```c
struct dice_data {
    int number;    /* Number of dice */
    int size;      /* Size of each die */
    int bonus;     /* Bonus modifier */
    long last_roll;
};
typedef struct dice_data DICE_DATA;
```

Dice commands parse `XdY+Z` format (e.g., "10d8+200"). The existing medit_hitdice (line 2996) shows the parsing pattern.

### Steps

- [ ] **Step 1: Add generic apply and macro for dice**

In `editors/common/olc_field_handlers.h`, add after `OLC_FIELD_APPLY_FLAGS`:

```c
/* Generic apply for dice fields (DICE_DATA: number, size, bonus) */
bool olc_apply_generic_dice(DICE_DATA *field_ptr, olc_pending_change_t *change);

#define OLC_FIELD_APPLY_DICE(func_name, entity_type, member) \
    static bool func_name(void *entity, olc_pending_change_t *change) { \
        return olc_apply_generic_dice(&((entity_type *)entity)->member, change); \
    }
```

In `editors/common/olc_field_handlers.c`, implement:

```c
bool olc_apply_generic_dice(DICE_DATA *field_ptr, olc_pending_change_t *change) {
    if (!field_ptr || !change || !change->new_value) return false;
    if (!json_is_object(change->new_value)) return false;

    json_t *jnum = json_object_get(change->new_value, "number");
    json_t *jsiz = json_object_get(change->new_value, "size");
    json_t *jbon = json_object_get(change->new_value, "bonus");

    if (!json_is_integer(jnum) || !json_is_integer(jsiz) || !json_is_integer(jbon))
        return false;

    field_ptr->number = (int)json_integer_value(jnum);
    field_ptr->size   = (int)json_integer_value(jsiz);
    field_ptr->bonus  = (int)json_integer_value(jbon);
    return true;
}
```

- [ ] **Step 2: Implement `olc_cmd_dice` helper**

In `editors/common/olc_commands.h`, add:

```c
/**
 * Set a dice field (DICE_DATA) from "XdY+Z" format.
 *
 * Parses number, size, bonus. Records as JSON object with keys
 * "number", "size", "bonus". The field_ptr is used to read
 * old values for the changeset.
 */
bool olc_cmd_dice(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, DICE_DATA *field_ptr,
    void *ctx, olc_cmd_record_fn record_fn);
```

In `editors/common/olc_commands.c`, implement. The parsing logic should follow the existing `medit_hitdice` pattern:

```c
bool olc_cmd_dice(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, DICE_DATA *field_ptr,
    void *ctx, olc_cmd_record_fn record_fn)
{
    char *num_str, *size_str, *bonus_str;
    int number, size, bonus;

    if (IS_NULLSTR(argument)) {
        if (syntax)
            send_to_char(syntax, ch);
        else
            send_to_char(formatf("Syntax: %s <number>d<size>+<bonus>\n\r", label), ch);
        return false;
    }

    /* Parse XdY+Z */
    num_str = argument;
    size_str = strchr(argument, 'd');
    if (!size_str) { /* show syntax */ return false; }
    *size_str++ = '\0';
    bonus_str = strchr(size_str, '+');
    if (!bonus_str) { /* show syntax */ return false; }
    *bonus_str++ = '\0';

    if (!is_number(num_str) || !is_number(size_str) || !is_number(bonus_str)) {
        send_to_char(formatf("Syntax: %s <number>d<size>+<bonus>\n\r", label), ch);
        return false;
    }

    number = atoi(num_str);
    size   = atoi(size_str);
    bonus  = atoi(bonus_str);

    /* Staged mode */
    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
        olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
        if (cs) {
            if (!olc_check_staging_limits(ch, cs)) return false;

            json_t *old_val = json_pack("{s:i, s:i, s:i}",
                "number", field_ptr->number,
                "size",   field_ptr->size,
                "bonus",  field_ptr->bonus);
            json_t *new_val = json_pack("{s:i, s:i, s:i}",
                "number", number, "size", size, "bonus", bonus);

            olc_pending_change_t *result = olc_changeset_add_change(
                cs, label, OLC_FIELD_EMBEDDED, old_val, new_val);
            json_decref(old_val);
            json_decref(new_val);

            if (result)
                printf_to_char(ch, "{G[STAGED]{x %s set to %dd%d+%d.\n\r",
                    label, number, size, bonus);
            else
                printf_to_char(ch, "%s reverted to original value.\n\r", label);

            notify_field_change(cs, ch, label,
                result ? result->new_value : json_null(), "dice", result != NULL);
            return result != NULL;
        }
    }

    /* Non-staged: apply directly */
    char old_str[64];
    snprintf(old_str, sizeof(old_str), "%dd%d+%d",
        field_ptr->number, field_ptr->size, field_ptr->bonus);

    field_ptr->number = number;
    field_ptr->size   = size;
    field_ptr->bonus  = bonus;

    cmd_record(record_fn, ctx, ch, label, old_str,
        formatf("%dd%d+%d", number, size, bonus));
    printf_to_char(ch, "%s set to %dd%d+%d.\n\r", label, number, size, bonus);
    return true;
}
```

**Note:** The `notify_field_change` function is static in `olc_commands.c`. Since `olc_cmd_dice` is in the same file, it has access.

**Note:** The dice parsing modifies the argument string in place (inserting NUL terminators at 'd' and '+'). This matches how existing medit_hitdice works. The argument buffer is scratch space.

- [ ] **Step 3: Build and verify**

```bash
cd /sentience/src && ./build tests
```

- [ ] **Step 4: Commit**

```bash
git add -A && git commit -m "feat(olc): add olc_cmd_dice for dice field staging

Add olc_apply_generic_dice(), OLC_FIELD_APPLY_DICE macro, and olc_cmd_dice()
helper for XdY+Z dice fields. Stores as JSON object {number, size, bonus}.
Uses OLC_FIELD_EMBEDDED type for changeset tracking.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 4: Unit Tests for New Infrastructure

**Files:**
- Modify: `tests/unit/olc_changeset_tests.c`
- Modify: `tests/data/unit/olc_changeset_unit_tests.json`

**Context:** Existing tests use `olccs_` prefix for dispatcher matching (see `tests/framework/test_dispatcher.c` line 113: `MATCH_SUBSTR`). New tests must also use this prefix. Test functions return `test_result_t` with `TEST_SUCCESS`/`TEST_FAILURE`. Use `TEST_ASSERT_*` macros.

### Steps

- [ ] **Step 1: Add test for `olc_apply_generic_long`**

In `tests/unit/olc_changeset_tests.c`, add a new test function:

```c
static test_result_t test_olccs_apply_long(test_case_t *test) {
    long field = 500;
    olc_pending_change_t change = {
        .field_path = "Cost",
        .field_type = OLC_FIELD_LONG,
        .old_value = json_integer(500),
        .new_value = json_integer(99999)
    };
    TEST_ASSERT_TRUE(olc_apply_generic_long(&field, &change), "apply should succeed");
    TEST_ASSERT_TRUE(field == 99999, "field should be 99999");
    json_decref(change.old_value);
    json_decref(change.new_value);
    return TEST_SUCCESS;
}
```

- [ ] **Step 2: Add test for `olc_apply_generic_dice`**

```c
static test_result_t test_olccs_apply_dice(test_case_t *test) {
    DICE_DATA dice = { .number = 1, .size = 6, .bonus = 0 };
    json_t *new_val = json_pack("{s:i, s:i, s:i}", "number", 10, "size", 8, "bonus", 200);
    olc_pending_change_t change = {
        .field_path = "Hit Dice",
        .field_type = OLC_FIELD_EMBEDDED,
        .old_value = json_null(),
        .new_value = new_val
    };
    TEST_ASSERT_TRUE(olc_apply_generic_dice(&dice, &change), "apply should succeed");
    TEST_ASSERT_INT_EQ(dice.number, 10);
    TEST_ASSERT_INT_EQ(dice.size, 8);
    TEST_ASSERT_INT_EQ(dice.bonus, 200);
    json_decref(new_val);
    return TEST_SUCCESS;
}
```

- [ ] **Step 3: Add test for long field in changeset round-trip**

Test that adding a long change to a changeset preserves the value through serialize/deserialize:

```c
static test_result_t test_olccs_long_roundtrip(test_case_t *test) {
    WNUM_LOAD wl = { .auid = 1, .vnum = 100 };
    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT, wl, "Test Obj", "tester");
    json_t *old_v = json_integer(0);
    json_t *new_v = json_integer(2000000000L);
    olc_changeset_add_change(cs, "Cost", OLC_FIELD_LONG, old_v, new_v);
    json_decref(old_v);
    json_decref(new_v);

    json_t *j = olc_changeset_serialize(cs);
    olc_changeset_t *cs2 = olc_changeset_deserialize(j);
    json_decref(j);

    olc_pending_change_t *ch = olc_changeset_find_change(cs2, "Cost");
    TEST_ASSERT_NOT_NULL(ch, "change should exist after roundtrip");
    TEST_ASSERT_TRUE(json_integer_value(ch->new_value) == 2000000000L,
        "long value should survive roundtrip");

    olc_changeset_destroy(cs);
    olc_changeset_destroy(cs2);
    return TEST_SUCCESS;
}
```

- [ ] **Step 4: Register tests in dispatcher**

Add entries to the test function dispatch in `olc_changeset_tests.c` (the `run_olc_changeset_test_case` function's switch/if chain). The test_type strings should match entries in the JSON file.

- [ ] **Step 5: Add JSON test entries**

In `tests/data/unit/olc_changeset_unit_tests.json`, add entries:

```json
{ "test_type": "olccs_apply_long", "description": "Apply generic long field" },
{ "test_type": "olccs_apply_dice", "description": "Apply generic dice field" },
{ "test_type": "olccs_long_roundtrip", "description": "Long field changeset roundtrip" }
```

- [ ] **Step 6: Build and run tests**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test:olccs_apply_long && ./sent -test:olccs_apply_dice && ./sent -test:olccs_long_roundtrip
```

All 3 should pass. Run full suite to verify no regressions:
```bash
./sent -test
```

- [ ] **Step 7: Commit**

```bash
cd /sentience/src && git add -A && git commit -m "test(olc): add unit tests for long and dice infrastructure

Tests: olccs_apply_long, olccs_apply_dice, olccs_long_roundtrip.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 5: OEdit Field Conversions (10 functions)

**Files:**
- Modify: `editors/objects/oedit.c`

**Context:** The oedit handler table is at line 259. Existing apply macros are at lines 241–254. New apply macros/functions should be added in the same region. The EDIT_OBJ macro extracts `OBJ_INDEX_DATA *pObj`.

### Function List

| # | Function | Line | Pattern | Field(s) | Type |
|---|----------|------|---------|----------|------|
| 1 | `oedit_cost` | 2320 | A | `cost` | `long` |
| 2 | `oedit_level` | 2993 | D | `level` + cascade | `int16_t` |
| 3 | `oedit_persist` | 1839 | C | `persist` | `bool` |
| 4 | `oedit_sign` | 2029 | H | `imp_sig` | `char *` |
| 5 | `oedit_parent` | 2080 | F | `parent_load` etc. | `WNUM_LOAD` |
| 6 | `oedit_fragility` | 3054 | B+C | `fragility` | `int16_t` |
| 7 | `oedit_extra` | 2642 | bitvector | `extra[0..3]` | `long[4]` |
| 8 | `oedit_wear` | 2783 | B | `wear_flags` | `long` |
| 9 | `oedit_varset` | 2134 | G | `index_vars` | variable |
| 10 | `oedit_varclear` | 2143 | G | `index_vars` | variable |

### Steps

- [ ] **Step 1: Add apply functions**

Add near the existing apply macros (after line ~254):

```c
/* Simple field apply macros */
OLC_FIELD_APPLY_LONG(oedit_apply_cost, OBJ_INDEX_DATA, cost)

/* Custom apply: level cascade */
static bool oedit_apply_level(void *entity, olc_pending_change_t *change) {
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    if (!olc_apply_generic_int16(&pObj->level, change)) return false;
    pObj->points = object_point_value(pObj);
    /* Recalculate type-dependent values */
    if (pObj->item_type == ITEM_WEAPON)
        set_weapon_dice(pObj);
    if (IS_ARMOR(pObj))
        calc_obj_armour(pObj);
    return true;
}

/* Custom apply: persist with imp_sig consumption */
static bool oedit_apply_persist(void *entity, olc_pending_change_t *change) {
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    if (!olc_apply_generic_bool(&pObj->persist, change)) return false;
    if (pObj->persist) use_imp_sig(NULL, pObj);
    return true;
}

OLC_FIELD_APPLY_STRING(oedit_apply_sign, OBJ_INDEX_DATA, imp_sig)

/* Custom apply: parent WNUM reference */
static bool oedit_apply_parent(void *entity, olc_pending_change_t *change) {
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    const char *val = json_string_value(change->new_value);
    if (IS_NULLSTR(val) || !str_cmp(val, "none") || !str_cmp(val, "0")) {
        pObj->parent_load = WNUM_LOAD_EMPTY;
        pObj->parent_wnum = WNUM_EMPTY;
        pObj->parent = NULL;
        pObj->parent_inherited = false;
        return true;
    }
    WNUM_LOAD wl;
    if (!parse_widevnum(val, pObj->area->uid, &wl)) return false;
    OBJ_INDEX_DATA *parent = get_obj_index(wl.auid, wl.vnum);
    if (!parent) return false;
    pObj->parent_load = wl;
    pObj->parent_wnum = (WNUM){ .pArea = parent->area, .vnum = parent->vnum };
    pObj->parent = parent;
    pObj->parent_inherited = false;
    return true;
}

/* Custom apply: fragility with imp_sig for solid */
static bool oedit_apply_fragility(void *entity, olc_pending_change_t *change) {
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    if (!olc_apply_generic_int16(&pObj->fragility, change)) return false;
    if (pObj->fragility == OBJ_FRAGILE_SOLID) use_imp_sig(NULL, pObj);
    return true;
}

OLC_FIELD_APPLY_FLAGS(oedit_apply_wear, OBJ_INDEX_DATA, wear_flags)
```

**Important:** Verify function names like `set_weapon_dice`, `calc_obj_armour`, `object_point_value`, `IS_ARMOR`, `parse_widevnum`, `get_obj_index` exist and have the right signatures by checking the existing `oedit_level` and `oedit_parent` implementations. Copy the exact logic.

- [ ] **Step 2: Add handler table entries**

Add to `oedit_field_handlers[]` before the `{ NULL }` terminator:

```c
    { "Cost",             OLC_FIELD_LONG,       NULL, oedit_apply_cost,       NULL },
    { "Level",            OLC_FIELD_INT16,      NULL, oedit_apply_level,      NULL },
    { "Persist",          OLC_FIELD_BOOL,       NULL, oedit_apply_persist,    NULL },
    { "Signature",        OLC_FIELD_STRING,     NULL, oedit_apply_sign,       NULL },
    { "Parent",           OLC_FIELD_STRING,     NULL, oedit_apply_parent,     NULL },
    { "Fragility",        OLC_FIELD_INT16,      NULL, oedit_apply_fragility,  NULL },
    { "Wear",             OLC_FIELD_FLAGS,      NULL, oedit_apply_wear,       NULL },
```

**Note:** `oedit_extra` already has a handler entry. `varset`/`varclear` use dynamic field paths (`"var/<key>"`) — see Step 4.

- [ ] **Step 3: Convert simple command functions**

For each function, preserve existing validation logic but replace direct field assignment with `olc_cmd_*` calls.

**oedit_cost** — Replace body with:
```c
OEDIT(oedit_cost) {
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    return olc_cmd_long(ch, argument, "Cost", NULL,
        &pObj->cost, 0, LONG_MAX, NULL, NULL);
}
```

**oedit_persist** — Keep permission check, replace toggle logic with `olc_cmd_bool`:
```c
OEDIT(oedit_persist) {
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    // Permission check: enabling requires IMP sig or MAX_LEVEL
    // ... (keep existing validation) ...
    return olc_cmd_bool(ch, argument, "Persist", NULL,
        &pObj->persist, NULL, NULL);
}
```

**oedit_sign** — Keep permission check, use `olc_cmd_string`:
```c
OEDIT(oedit_sign) {
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    if (ch->tot_level < MAX_LEVEL) {
        send_to_char("You must be level " STRINGIFY(MAX_LEVEL) " to sign objects.\n\r", ch);
        return false;
    }
    return olc_cmd_string(ch, ch->name, "Signature", NULL,
        &pObj->imp_sig, OLC_STR_DEFAULT, NULL, NULL);
}
```

**oedit_level** — Use `olc_cmd_number_i16`. The cascade recalculation is in the apply function:
```c
OEDIT(oedit_level) {
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    return olc_cmd_number_i16(ch, argument, "Level", NULL,
        &pObj->level, 0, MAX_LEVEL, NULL, NULL);
}
```

**oedit_fragility** — Keep validation (IMP check for solid), use `olc_cmd_number_i16`:
```c
OEDIT(oedit_fragility) {
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    // Keep: argument parsing (solid/strong/normal/weak → int value)
    // Keep: IMP signature check for solid
    // Use olc_cmd_number_i16 for staging after validation
    // OR stage directly with olc_changeset_add_change if the value
    //    is already parsed (since fragility uses named constants, not numbers)
    ...
}
```

**Note:** `oedit_fragility` accepts named constants ("solid", "strong", etc.), not raw numbers. The command should parse the name to a value, validate permissions, then stage the integer value directly. Use the existing command's parsing logic.

**oedit_wear** — Keep existing single-slot validation logic, delegate toggle to `olc_cmd_flag_toggle`:
```c
OEDIT(oedit_wear) {
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    // Keep: existing validation (slot conflict checks, type restrictions)
    // Then: return olc_cmd_flag_toggle(ch, argument, "Wear", NULL,
    //            &pObj->wear_flags, wear_flags, NULL, NULL);
}
```

**oedit_extra** — Replace direct toggle with `olc_stage_bitvector`:
```c
OEDIT(oedit_extra) {
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);
    if (IS_NULLSTR(argument)) { /* show syntax */ return false; }

    long toggle[4] = {0};
    if (!bitvector_lookup(argument, 4, toggle,
            extra_flags, extra2_flags, extra3_flags, extra4_flags))
    {
        send_to_char("No such extra flag.\n\r", ch);
        return false;
    }

    /* Staged mode */
    if (olc_stage_bitvector(ch, "extra", pObj->extra, toggle, 4))
        return true;

    /* Non-staged: toggle directly (existing behavior) */
    for (int i = 0; i < 4; i++)
        pObj->extra[i] ^= toggle[i];
    send_to_char("Extra flags toggled.\n\r", ch);
    return true;
}
```

- [ ] **Step 4: Convert parent (Pattern F)**

**oedit_parent** — Keep validation, stage WNUM as string:
```c
OEDIT(oedit_parent) {
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);

    if (IS_NULLSTR(argument)) { /* show syntax */ return false; }

    /* Validate: "none"/"clear"/"0" to clear, otherwise parse widevnum */
    const char *stage_val;
    if (!str_cmp(argument, "none") || !str_cmp(argument, "clear") || !str_cmp(argument, "0")) {
        stage_val = "none";
    } else {
        /* Parse and validate widevnum */
        WNUM_LOAD wl;
        if (!parse_widevnum(argument, pObj->area->uid, &wl)) {
            send_to_char("Invalid vnum format.\n\r", ch);
            return false;
        }
        OBJ_INDEX_DATA *parent = get_obj_index(wl.auid, wl.vnum);
        if (!parent) {
            send_to_char("Object not found.\n\r", ch);
            return false;
        }
        if (parent == pObj) {
            send_to_char("Object cannot be its own parent.\n\r", ch);
            return false;
        }
        stage_val = argument;
    }

    /* Stage or apply */
    return olc_cmd_string(ch, (char *)stage_val, "Parent", NULL,
        /* need a scratch string, not the entity field directly */
        ...);
}
```

**Important:** `oedit_parent` sets 4 fields (`parent_load`, `parent_wnum`, `parent`, `parent_inherited`). We stage a single string and the custom apply resolves all 4. Since the "field" being staged isn't a simple `char *` member, stage directly via `olc_changeset_add_change` instead of `olc_cmd_string`:

```c
    /* In staged mode: */
    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
        olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
        if (cs) {
            if (!olc_check_staging_limits(ch, cs)) return false;
            /* Build old value from current parent */
            json_t *old_val = /* format current parent as string or "none" */;
            json_t *new_val = json_string(stage_val);
            olc_pending_change_t *result = olc_changeset_add_change(
                cs, "Parent", OLC_FIELD_STRING, old_val, new_val);
            json_decref(old_val);
            json_decref(new_val);
            if (result)
                printf_to_char(ch, "{G[STAGED]{x Parent set to %s.\n\r", stage_val);
            else
                printf_to_char(ch, "Parent reverted to original value.\n\r");
            return result != NULL;
        }
    }
    /* Non-staged: apply directly (existing code) */
```

- [ ] **Step 5: Convert varset/varclear (Pattern G)**

These require staged-mode awareness. The approach:

```c
OEDIT(oedit_varset) {
    OBJ_INDEX_DATA *pObj;
    EDIT_OBJ(ch, pObj);

    /* Check if staged mode */
    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
        olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
        if (cs) {
            /* Parse variable name from argument */
            char varname[MAX_INPUT_LENGTH];
            char *rest = one_argument(argument, varname);
            if (IS_NULLSTR(varname)) {
                send_to_char("Syntax: varset <name> <type> <value>\n\r", ch);
                return false;
            }
            char field_path[MAX_INPUT_LENGTH];
            snprintf(field_path, sizeof(field_path), "var/%s", varname);

            /* Stage the full argument for replay at apply time */
            json_t *old_val = json_null(); /* variable state is complex */
            json_t *new_val = json_string(argument);
            if (!olc_check_staging_limits(ch, cs)) return false;
            olc_pending_change_t *result = olc_changeset_add_change(
                cs, field_path, OLC_FIELD_STRING, old_val, new_val);
            json_decref(old_val);
            json_decref(new_val);

            if (result)
                printf_to_char(ch, "{G[STAGED]{x Variable %s staged.\n\r", varname);
            else
                printf_to_char(ch, "Variable %s reverted.\n\r", varname);
            return result != NULL;
        }
    }

    /* Non-staged: existing behavior */
    return olc_varset(&pObj->index_vars, ch, argument, false);
}
```

**oedit_varclear** follows the same pattern but stages with `"var/<name>"` field path and `json_null()` as new_value (the apply function distinguishes set vs clear by checking for null).

For the varset/varclear **apply functions**, add a wildcard handler entry:

```c
{ "var/*", OLC_FIELD_STRING, NULL, oedit_apply_var, NULL },
```

The apply function replays the stored argument:

```c
static bool oedit_apply_var(void *entity, olc_pending_change_t *change) {
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    const char *arg = json_string_value(change->new_value);
    if (!arg) return false;

    if (str_prefix("var/", change->field_path))
        return false;

    /* Determine if varset or varclear based on null new_value convention */
    if (json_is_null(change->new_value)) {
        /* varclear: field_path is "var/<name>" */
        const char *varname = change->field_path + 4;
        char buf[MAX_INPUT_LENGTH];
        strlcpy(buf, varname, sizeof(buf));
        return olc_varclear(&pObj->index_vars, NULL, buf, true);
    } else {
        /* varset: new_value is the full argument string */
        char buf[MAX_INPUT_LENGTH];
        strlcpy(buf, arg, sizeof(buf));
        return olc_varset(&pObj->index_vars, NULL, buf, true);
    }
}
```

- [ ] **Step 6: Build and test**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test
```

- [ ] **Step 7: Commit**

```bash
cd /sentience/src && git add -A && git commit -m "feat(olc): convert oedit simple fields to staged mode

Convert 10 oedit commands to staged changeset system:
- cost (long), level (cascade), persist (imp_sig), sign, parent (WNUM),
  fragility (imp_sig), extra (bitvector), wear (flags), varset, varclear.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 6: MEdit Simple Field Conversions (9 functions)

**Files:**
- Modify: `editors/mobiles/medit.c`

**Context:** Handler table at line 214, apply macros at lines 145–173. The EDIT_MOB macro extracts `MOB_INDEX_DATA *pMob`.

### Function List

| # | Function | Line | Pattern | Field | Type |
|---|----------|------|---------|-------|------|
| 1 | `medit_persist` | 1038 | C | `persist` | `bool` |
| 2 | `medit_boss` | 1065 | C | `boss` | `bool` |
| 3 | `medit_sign` | 1438 | H | `sig` | `char *` |
| 4 | `medit_gold` | 3270 | A+C | `wealth` | `long` |
| 5 | `medit_movedice` | 3251 | A | `move` | `long` |
| 6 | `medit_corpsevnum` | 1591 | F | `corpse_load` | `WNUM_LOAD` |
| 7 | `medit_zombievnum` | 1627 | F | `zombie_load` | `WNUM_LOAD` |
| 8 | `medit_varset` | 1564 | G | `index_vars` | variable |
| 9 | `medit_varclear` | 1573 | G | `index_vars` | variable |

### Steps

- [ ] **Step 1: Add apply functions**

```c
/* Simple applies */
static bool medit_apply_persist(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    if (!olc_apply_generic_bool(&pMob->persist, change)) return false;
    if (pMob->persist) use_imp_sig(pMob, NULL);
    return true;
}

static bool medit_apply_boss(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    if (!olc_apply_generic_bool(&pMob->boss, change)) return false;
    if (pMob->boss) use_imp_sig(pMob, NULL);
    return true;
}

OLC_FIELD_APPLY_STRING(medit_apply_sign, MOB_INDEX_DATA, sig)

static bool medit_apply_gold(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    if (!olc_apply_generic_long(&pMob->wealth, change)) return false;
    if (pMob->wealth > 1000) use_imp_sig(pMob, NULL);
    return true;
}

OLC_FIELD_APPLY_LONG(medit_apply_move, MOB_INDEX_DATA, move)

/* WNUM apply for corpsevnum */
static bool medit_apply_corpsevnum(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    const char *val = json_string_value(change->new_value);
    if (IS_NULLSTR(val) || !str_cmp(val, "0")) {
        pMob->corpse_load = WNUM_LOAD_EMPTY;
        return true;
    }
    WNUM_LOAD wl;
    if (!parse_widevnum(val, pMob->area->uid, &wl)) return false;
    if (!get_obj_index(wl.auid, wl.vnum)) return false;
    pMob->corpse_load = wl;
    return true;
}

/* Similar for zombievnum */
static bool medit_apply_zombievnum(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    const char *val = json_string_value(change->new_value);
    if (IS_NULLSTR(val) || !str_cmp(val, "0")) {
        pMob->zombie_load = WNUM_LOAD_EMPTY;
        return true;
    }
    WNUM_LOAD wl;
    if (!parse_widevnum(val, pMob->area->uid, &wl)) return false;
    if (!get_obj_index(wl.auid, wl.vnum)) return false;
    pMob->zombie_load = wl;
    return true;
}
```

Variable apply: same pattern as oedit (Task 5 Step 5) but with `MOB_INDEX_DATA`.

- [ ] **Step 2: Add handler table entries**

Add to `medit_field_handlers[]` before the `{ NULL }` terminator:

```c
    { "Persist",          OLC_FIELD_BOOL,    NULL, medit_apply_persist,     NULL },
    { "Boss",             OLC_FIELD_BOOL,    NULL, medit_apply_boss,        NULL },
    { "Signature",        OLC_FIELD_STRING,  NULL, medit_apply_sign,        NULL },
    { "Gold",             OLC_FIELD_LONG,    NULL, medit_apply_gold,        NULL },
    { "Movement",         OLC_FIELD_LONG,    NULL, medit_apply_move,        NULL },
    { "Corpse Vnum",      OLC_FIELD_STRING,  NULL, medit_apply_corpsevnum,  NULL },
    { "Zombie Vnum",      OLC_FIELD_STRING,  NULL, medit_apply_zombievnum,  NULL },
    { "var/*",            OLC_FIELD_STRING,  NULL, medit_apply_var,         NULL },
```

- [ ] **Step 3: Convert command functions**

**medit_persist** — Keep IMP sig check, use `olc_cmd_bool`:
```c
MEDIT(medit_persist) {
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    // Keep: permission check (has_imp_sig or MAX_LEVEL)
    return olc_cmd_bool(ch, argument, "Persist", NULL,
        &pMob->persist, NULL, NULL);
}
```

**medit_boss** — Same pattern as persist.

**medit_sign** — Keep level check, use `olc_cmd_string`.

**medit_gold** — Keep IMP sig check for values > 1000, use `olc_cmd_long`:
```c
MEDIT(medit_gold) {
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    // Validate
    long value = atol(argument);
    if (value > 1000 && !has_imp_sig(pMob, NULL)) {
        send_to_char("This mobile has not been signed.\n\r", ch);
        return false;
    }
    return olc_cmd_long(ch, argument, "Gold", NULL,
        &pMob->wealth, 0, LONG_MAX, NULL, NULL);
}
```

**medit_movedice** — Direct replacement:
```c
MEDIT(medit_movedice) {
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_long(ch, argument, "Movement", NULL,
        &pMob->move, 0, LONG_MAX, NULL, NULL);
}
```

**medit_corpsevnum/zombievnum** — Validate widevnum, stage string (same pattern as oedit_parent).

**medit_varset/varclear** — Same pattern as oedit (Task 5 Step 5).

- [ ] **Step 4: Build and test**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test
```

- [ ] **Step 5: Commit**

```bash
cd /sentience/src && git add -A && git commit -m "feat(olc): convert medit simple fields to staged mode

Convert 9 medit commands: persist, boss, sign, gold, movedice,
corpsevnum, zombievnum, varset, varclear.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 7: MEdit Combat Field Conversions (10 functions)

**Files:**
- Modify: `editors/mobiles/medit.c`

### Function List

| # | Function | Line | Pattern | Field(s) | Type |
|---|----------|------|---------|----------|------|
| 1 | `medit_level` | 1303 | D | `level` + 4 dice cascade | `int16_t` |
| 2 | `medit_spec` | 1241 | B+custom | `spec_fun` | `SPEC_FUN *` |
| 3 | `medit_position` | 3203 | E | `start_pos`, `default_pos` | `int16_t` |
| 4 | `medit_act` | 2808 | bitvector | `act[0..1]` | `long[2]` |
| 5 | `medit_affect` | 2834 | bitvector | `affected_by[0..1]` | `long[2]` |
| 6 | `medit_ac` | 2859 | custom | `ac[0..3]` | `int16_t[4]` |
| 7 | `medit_hitdice` | 2996 | A | `hit` | `DICE_DATA` |
| 8 | `medit_manadice` | 3048 | A | `mana` | `DICE_DATA` |
| 9 | `medit_damdice` | 3100 | A | `damage` | `DICE_DATA` |
| 10 | `medit_parent` | 1509 | F | `parent_load` etc. | `WNUM_LOAD` |

### Steps

- [ ] **Step 1: Add apply functions for bitvectors**

```c
/* Apply for act flags (2 banks, force ACT_IS_NPC) */
static bool medit_apply_act(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    json_t *arr = change->new_value;
    if (!json_is_array(arr)) return false;
    for (size_t i = 0; i < json_array_size(arr) && i < 2; i++)
        pMob->act[i] = (long)json_integer_value(json_array_get(arr, i));
    SET_BIT(pMob->act[0], ACT_IS_NPC);
    return true;
}

/* Apply for affect flags (2 banks) */
static bool medit_apply_affect(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    json_t *arr = change->new_value;
    if (!json_is_array(arr)) return false;
    for (size_t i = 0; i < json_array_size(arr) && i < 2; i++)
        pMob->affected_by[i] = (long)json_integer_value(json_array_get(arr, i));
    return true;
}
```

Also need serialize functions for display in `pending`:

```c
static json_t *medit_serialize_act(void *entity, const char *field_path) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    json_t *arr = json_array();
    for (int i = 0; i < 2; i++)
        json_array_append_new(arr, json_integer(pMob->act[i]));
    return arr;
}
/* Similar for affect */
```

- [ ] **Step 2: Add apply functions for dice, AC, level, spec, parent, position**

```c
OLC_FIELD_APPLY_DICE(medit_apply_hitdice, MOB_INDEX_DATA, hit)
OLC_FIELD_APPLY_DICE(medit_apply_manadice, MOB_INDEX_DATA, mana)
OLC_FIELD_APPLY_DICE(medit_apply_damdice, MOB_INDEX_DATA, damage)

/* AC: 4 separate int16_t apply functions */
OLC_FIELD_APPLY_INT16(medit_apply_ac_pierce, MOB_INDEX_DATA, ac[AC_PIERCE])
OLC_FIELD_APPLY_INT16(medit_apply_ac_bash,   MOB_INDEX_DATA, ac[AC_BASH])
OLC_FIELD_APPLY_INT16(medit_apply_ac_slash,  MOB_INDEX_DATA, ac[AC_SLASH])
OLC_FIELD_APPLY_INT16(medit_apply_ac_exotic, MOB_INDEX_DATA, ac[AC_EXOTIC])

/* Level with dice cascade */
static bool medit_apply_level(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    if (!olc_apply_generic_int16(&pMob->level, change)) return false;
    set_mob_hitdice(pMob);
    set_mob_damdice(pMob);
    set_mob_movedice(pMob);
    if (IS_SET(pMob->off_flags, OFF_MAGIC))
        set_mob_manadice(pMob);
    return true;
}

/* Spec: resolve name to function pointer */
static bool medit_apply_spec(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    const char *name = json_string_value(change->new_value);
    if (IS_NULLSTR(name) || !str_cmp(name, "none")) {
        pMob->spec_fun = NULL;
        return true;
    }
    SPEC_FUN *fn = spec_lookup(name);
    if (!fn) return false;
    pMob->spec_fun = fn;
    return true;
}

/* Position: start_pos and default_pos apply individually */
OLC_FIELD_APPLY_INT16(medit_apply_start_pos,   MOB_INDEX_DATA, start_pos)
OLC_FIELD_APPLY_INT16(medit_apply_default_pos,  MOB_INDEX_DATA, default_pos)

/* Parent: same pattern as oedit_apply_parent */
static bool medit_apply_parent(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    const char *val = json_string_value(change->new_value);
    if (IS_NULLSTR(val) || !str_cmp(val, "none") || !str_cmp(val, "0")) {
        pMob->parent_load = WNUM_LOAD_EMPTY;
        pMob->parent_wnum = WNUM_EMPTY;
        pMob->parent = NULL;
        pMob->parent_inherited = false;
        return true;
    }
    WNUM_LOAD wl;
    if (!parse_widevnum(val, pMob->area->uid, &wl)) return false;
    MOB_INDEX_DATA *parent = get_mob_index(wl.auid, wl.vnum);
    if (!parent) return false;
    pMob->parent_load = wl;
    pMob->parent_wnum = (WNUM){ .pArea = parent->area, .vnum = parent->vnum };
    pMob->parent = parent;
    pMob->parent_inherited = false;
    return true;
}
```

- [ ] **Step 3: Add handler table entries**

```c
    { "Act",              OLC_FIELD_MULTIFLAGS, medit_serialize_act, medit_apply_act,      NULL },
    { "Affected By",      OLC_FIELD_MULTIFLAGS, medit_serialize_affect, medit_apply_affect, NULL },
    { "AC Pierce",        OLC_FIELD_INT16,      NULL, medit_apply_ac_pierce,  NULL },
    { "AC Bash",          OLC_FIELD_INT16,      NULL, medit_apply_ac_bash,    NULL },
    { "AC Slash",         OLC_FIELD_INT16,      NULL, medit_apply_ac_slash,   NULL },
    { "AC Exotic",        OLC_FIELD_INT16,      NULL, medit_apply_ac_exotic,  NULL },
    { "Hit Dice",         OLC_FIELD_EMBEDDED,   NULL, medit_apply_hitdice,    NULL },
    { "Mana Dice",        OLC_FIELD_EMBEDDED,   NULL, medit_apply_manadice,   NULL },
    { "Damage Dice",      OLC_FIELD_EMBEDDED,   NULL, medit_apply_damdice,    NULL },
    { "Level",            OLC_FIELD_INT16,      NULL, medit_apply_level,      NULL },
    { "Spec",             OLC_FIELD_STRING,     NULL, medit_apply_spec,       NULL },
    { "Start Position",   OLC_FIELD_INT16,      NULL, medit_apply_start_pos,  NULL },
    { "Default Position", OLC_FIELD_INT16,      NULL, medit_apply_default_pos, NULL },
    { "Parent",           OLC_FIELD_STRING,     NULL, medit_apply_parent,     NULL },
```

- [ ] **Step 4: Convert command functions**

**medit_level** — Use `olc_cmd_number_i16`, cascade in apply:
```c
MEDIT(medit_level) {
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_number_i16(ch, argument, "Level", NULL,
        &pMob->level, 1, MAX_MOB_SKILL_LEVEL, NULL, NULL);
}
```

**medit_spec** — Validate, then stage string:
```c
MEDIT(medit_spec) {
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    if (IS_NULLSTR(argument)) { /* syntax */ return false; }
    if (str_cmp(argument, "none") && !spec_lookup(argument)) {
        send_to_char("Invalid spec function.\n\r", ch);
        return false;
    }
    /* Stage the spec name as string; apply function resolves pointer */
    return olc_cmd_string(ch, argument, "Spec", NULL,
        /* Need temp storage - use direct staging instead */
        ...);
}
```

**Note:** `medit_spec` stores a function pointer, not a string. We stage the spec **name** as a string. The apply function resolves the name to a pointer at commit time. Stage directly via `olc_changeset_add_change`.

**medit_position** — Parse "start"/"default" sub-command, call `olc_cmd_type_set_i16` for appropriate field:
```c
MEDIT(medit_position) {
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    char arg[MAX_INPUT_LENGTH];
    argument = one_argument(argument, arg);

    if (!str_cmp(arg, "start"))
        return olc_cmd_type_set_i16(ch, argument, "Start Position", NULL,
            &pMob->start_pos, position_flags, NULL, NULL);
    else if (!str_cmp(arg, "default"))
        return olc_cmd_type_set_i16(ch, argument, "Default Position", NULL,
            &pMob->default_pos, position_flags, NULL, NULL);
    else {
        send_to_char("Syntax: position start|default <position>\n\r", ch);
        return false;
    }
}
```

**medit_act** — Parse via bitvector_lookup, use `olc_stage_bitvector`:
```c
MEDIT(medit_act) {
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    if (IS_NULLSTR(argument)) { /* syntax */ return false; }

    long toggle[2] = {0};
    if (!bitvector_lookup(argument, 2, toggle, act_flags, act2_flags)) {
        send_to_char("No such act flag.\n\r", ch);
        return false;
    }

    if (olc_stage_bitvector(ch, "Act", pMob->act, toggle, 2))
        return true;

    /* Non-staged fallback */
    for (int i = 0; i < 2; i++)
        pMob->act[i] ^= toggle[i];
    SET_BIT(pMob->act[0], ACT_IS_NPC);
    send_to_char("Act flags toggled.\n\r", ch);
    return true;
}
```

**medit_affect** — Same pattern as medit_act with `affected_by`, `aff_flags`/`aff2_flags`.

**medit_ac** — Parse 1–4 values, stage each as separate field:
```c
MEDIT(medit_ac) {
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    char arg[MAX_INPUT_LENGTH];
    // Parse up to 4 arguments
    // For each provided value, call olc_cmd_number_i16 with appropriate label:
    //   "AC Pierce", "AC Bash", "AC Slash", "AC Exotic"
}
```

**medit_hitdice/manadice/damdice** — Direct `olc_cmd_dice`:
```c
MEDIT(medit_hitdice) {
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    // Keep level 151+ check if it exists
    return olc_cmd_dice(ch, argument, "Hit Dice", NULL,
        &pMob->hit, NULL, NULL);
}
```

**medit_parent** — Same pattern as oedit_parent (Task 5 Step 4).

- [ ] **Step 5: Build and test**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test
```

- [ ] **Step 6: Commit**

```bash
cd /sentience/src && git add -A && git commit -m "feat(olc): convert medit combat fields to staged mode

Convert 10 medit commands: level (cascade), spec (fn pointer), position
(multi-field), act/affect (bitvector), ac (4-array), hitdice/manadice/damdice
(dice), parent (WNUM).

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 8: AEdit Field Conversions (11 functions)

**Files:**
- Modify: `editors/areas/aedit.c`

**Context:** Handler table at line 135, apply macros at lines 118–133. The EDIT_AREA macro extracts `AREA_DATA *pArea`.

### Function List

| # | Function | Pattern | Field(s) | Type |
|---|----------|---------|----------|------|
| 1 | `aedit_wilds` | B+A | `wilds_uid` | `long` |
| 2 | `aedit_areawho` | B | `area_who` | `int` |
| 3 | `aedit_placetype` | B | `place_flags` | `long` |
| 4 | `aedit_file` | B+string | `file_name` | `char *` |
| 5 | `aedit_recall` | F | `recall` | `LOCATION` |
| 6 | `aedit_airshipland` | F | `airship_land_load` | `WNUM_LOAD` |
| 7 | `aedit_postoffice` | F | `post_office_load` | `WNUM_LOAD` |
| 8 | `aedit_vnum` | E | `min_vnum`, `max_vnum` | `long` |
| 9 | `aedit_levels` | E | `min_level`, `max_level` | `int16_t` |
| 10 | `aedit_varset` | G | `index_vars` | variable |
| 11 | `aedit_varclear` | G | `index_vars` | variable |

### Key Type Notes
- `min_vnum`/`max_vnum` are `long` (merc.h:6798-6799) → use `olc_cmd_long`
- `min_level`/`max_level` are `int16_t` (merc.h:6796-6797) → use `olc_cmd_number_i16`
- `place_flags` is `long` (merc.h:6831) → stage directly as long
- `area_who` is `int` (merc.h:6828) → use `olc_cmd_type_set`
- `recall` is `LOCATION` struct → stage as string, custom apply
- `airship_land_load` is `WNUM_LOAD` → stage as string, custom apply
- `post_office_load` is `WNUM_LOAD` → stage as string, custom apply

### Steps

- [ ] **Step 1: Add apply functions**

```c
OLC_FIELD_APPLY_LONG(aedit_apply_wilds, AREA_DATA, wilds_uid)
OLC_FIELD_APPLY_INT(aedit_apply_areawho, AREA_DATA, area_who)
OLC_FIELD_APPLY_LONG(aedit_apply_placetype, AREA_DATA, place_flags)
OLC_FIELD_APPLY_STRING(aedit_apply_file, AREA_DATA, file_name)
OLC_FIELD_APPLY_LONG(aedit_apply_min_vnum, AREA_DATA, min_vnum)
OLC_FIELD_APPLY_LONG(aedit_apply_max_vnum, AREA_DATA, max_vnum)
OLC_FIELD_APPLY_INT16(aedit_apply_min_level, AREA_DATA, min_level)
OLC_FIELD_APPLY_INT16(aedit_apply_max_level, AREA_DATA, max_level)

/* Custom: recall location */
static bool aedit_apply_recall(void *entity, olc_pending_change_t *change) {
    AREA_DATA *pArea = (AREA_DATA *)entity;
    const char *val = json_string_value(change->new_value);
    if (IS_NULLSTR(val) || !str_cmp(val, "none") || !str_cmp(val, "clear")) {
        location_clear(&pArea->recall);
        return true;
    }
    /* Parse widevnum or wuid+coords format — copy logic from existing aedit_recall */
    /* ... */
    return true;
}

/* Custom: airship_land WNUM */
static bool aedit_apply_airshipland(void *entity, olc_pending_change_t *change) {
    AREA_DATA *pArea = (AREA_DATA *)entity;
    const char *val = json_string_value(change->new_value);
    if (IS_NULLSTR(val) || !str_cmp(val, "0")) {
        pArea->airship_land_load = WNUM_LOAD_EMPTY;
        pArea->airship_land_wnum = WNUM_EMPTY;
        return true;
    }
    WNUM_LOAD wl;
    if (!parse_widevnum(val, pArea->uid, &wl)) return false;
    ROOM_INDEX_DATA *room = get_room_index(wl.auid, wl.vnum);
    if (!room) return false;
    pArea->airship_land_load = wl;
    pArea->airship_land_wnum = (WNUM){ .pArea = room->area, .vnum = room->vnum };
    return true;
}

/* Custom: post_office WNUM */
static bool aedit_apply_postoffice(void *entity, olc_pending_change_t *change) {
    AREA_DATA *pArea = (AREA_DATA *)entity;
    const char *val = json_string_value(change->new_value);
    if (IS_NULLSTR(val) || !str_cmp(val, "0")) {
        pArea->post_office_load = WNUM_LOAD_EMPTY;
        pArea->post_office_wnum = WNUM_EMPTY;
        return true;
    }
    WNUM_LOAD wl;
    if (!parse_widevnum(val, pArea->uid, &wl)) return false;
    ROOM_INDEX_DATA *room = get_room_index(wl.auid, wl.vnum);
    if (!room) return false;
    pArea->post_office_load = wl;
    pArea->post_office_wnum = (WNUM){ .pArea = room->area, .vnum = room->vnum };
    return true;
}
```

Variable apply: same pattern as oedit/medit. **Additional note for aedit**: the existing `aedit_varset`/`aedit_varclear` also sync to live `progs->vars`. In staged mode, this sync should happen in the apply function at commit time, not at staging time. This is a behavioral change — variables won't take effect on the live area until commit.

- [ ] **Step 2: Add handler table entries**

```c
    { "Wilderness",       OLC_FIELD_LONG,    NULL, aedit_apply_wilds,       NULL },
    { "Area Who",         OLC_FIELD_INT,     NULL, aedit_apply_areawho,     NULL },
    { "Place Type",       OLC_FIELD_LONG,    NULL, aedit_apply_placetype,   NULL },
    { "File Name",        OLC_FIELD_STRING,  NULL, aedit_apply_file,        NULL },
    { "Recall",           OLC_FIELD_STRING,  NULL, aedit_apply_recall,      NULL },
    { "Airship Land",     OLC_FIELD_STRING,  NULL, aedit_apply_airshipland, NULL },
    { "Post Office",      OLC_FIELD_STRING,  NULL, aedit_apply_postoffice,  NULL },
    { "Min Vnum",         OLC_FIELD_LONG,    NULL, aedit_apply_min_vnum,    NULL },
    { "Max Vnum",         OLC_FIELD_LONG,    NULL, aedit_apply_max_vnum,    NULL },
    { "Min Level",        OLC_FIELD_INT16,   NULL, aedit_apply_min_level,   NULL },
    { "Max Level",        OLC_FIELD_INT16,   NULL, aedit_apply_max_level,   NULL },
    { "var/*",            OLC_FIELD_STRING,  NULL, aedit_apply_var,         NULL },
```

- [ ] **Step 3: Convert command functions**

**aedit_wilds** — Validate UID, use `olc_cmd_long`:
```c
AEDIT(aedit_wilds) {
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    if (IS_NULLSTR(argument)) { /* syntax */ return false; }
    long uid = atol(argument);
    if (uid != 0 && !get_wilds_from_uid(uid)) {
        send_to_char("Invalid wilderness UID.\n\r", ch);
        return false;
    }
    return olc_cmd_long(ch, argument, "Wilderness", NULL,
        &pArea->wilds_uid, 0, LONG_MAX, NULL, NULL);
}
```

**aedit_areawho** — Use `olc_cmd_type_set`:
```c
AEDIT(aedit_areawho) {
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    // Keep: validation for AREA_INSTANCE/AREA_DUTY in non-blueprints
    return olc_cmd_type_set(ch, argument, "Area Who", NULL,
        &pArea->area_who, area_who_titles, NULL, NULL);
}
```

**aedit_placetype** — Validate, stage long directly:
```c
AEDIT(aedit_placetype) {
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    // Parse via flag_value, then stage directly since field is long
    // Use olc_cmd_flag_toggle for the long* field, or stage directly
}
```

**aedit_file** — Validate filename, use `olc_cmd_string`.

**aedit_vnum** — Validate range, stage two fields:
```c
AEDIT(aedit_vnum) {
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    // Parse two arguments (lower, upper)
    // Validate: lower < upper, no conflicts
    // Stage each:
    //   olc_cmd_long(ch, lower_str, "Min Vnum", ...);
    //   olc_cmd_long(ch, upper_str, "Max Vnum", ...);
}
```

**aedit_levels** — Same pattern but `olc_cmd_number_i16`.

**aedit_recall/airshipland/postoffice** — Validate, stage as string, custom apply resolves.

**aedit_varset/varclear** — Same pattern as oedit. Note: the live sync to `progs->vars` moves to the apply function.

- [ ] **Step 4: Build and test**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test
```

- [ ] **Step 5: Commit**

```bash
cd /sentience/src && git add -A && git commit -m "feat(olc): convert aedit simple fields to staged mode

Convert 11 aedit commands: wilds, areawho, placetype, file, recall,
airshipland, postoffice, vnum (2 fields), levels (2 fields), varset, varclear.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 9: REdit Field Conversions (8 functions)

**Files:**
- Modify: `editors/rooms/redit.c`

**Context:** Handler table at line 122, apply macros at lines 113–120. The EDIT_ROOM macro extracts `ROOM_INDEX_DATA *pRoom`.

### Function List

| # | Function | Pattern | Field(s) | Type |
|---|----------|---------|----------|------|
| 1 | `redit_locale` | A | `locale` | `long` |
| 2 | `redit_persist` | C | persistence system | function-based |
| 3 | `redit_recall` | F | `rs_recall` | `RS_LOCATION` |
| 4 | `redit_sector` | B | sector_type | `int` via setter |
| 5 | `redit_region` | B | `region` | `AREA_REGION *` |
| 6 | `redit_parent` | F | `parent_load` etc. | `WNUM_LOAD` |
| 7 | `redit_varset` | G | `index_vars` | variable |
| 8 | `redit_varclear` | G | `index_vars` | variable |

### Notes on Complex Fields

**redit_persist** — Calls `persist_addroom()`/`persist_removeroom()` which are system-level functions. The apply function must call these:
```c
static bool redit_apply_persist(void *entity, olc_pending_change_t *change) {
    ROOM_INDEX_DATA *pRoom = (ROOM_INDEX_DATA *)entity;
    bool new_val = json_is_true(change->new_value);
    if (new_val)
        persist_addroom(pRoom);
    else
        persist_removeroom(pRoom);
    return true;
}
```

**redit_sector** — Uses `room_set_rs_sector_type()` setter, not direct assignment. The apply function must call this setter.

**redit_region** — Resolves region by name/number. Stage the region identifier string, apply resolves.

**redit_recall** — Uses `RS_LOCATION` type and `rs_location_set()`/`rs_location_clear()`. Stage as string, apply resolves.

### Steps

- [ ] **Step 1: Add apply functions**

```c
OLC_FIELD_APPLY_LONG(redit_apply_locale, ROOM_INDEX_DATA, locale)

static bool redit_apply_persist(void *entity, olc_pending_change_t *change) {
    ROOM_INDEX_DATA *pRoom = (ROOM_INDEX_DATA *)entity;
    bool new_val = json_is_true(change->new_value);
    if (new_val) persist_addroom(pRoom); else persist_removeroom(pRoom);
    return true;
}

/* Custom: recall location */
static bool redit_apply_recall(void *entity, olc_pending_change_t *change) {
    ROOM_INDEX_DATA *pRoom = (ROOM_INDEX_DATA *)entity;
    const char *val = json_string_value(change->new_value);
    if (IS_NULLSTR(val) || !str_cmp(val, "none") || !str_cmp(val, "clear")) {
        rs_location_clear(&pRoom->rs_recall);
        return true;
    }
    /* Parse and resolve — copy logic from existing redit_recall */
    return true;
}

/* Custom: sector via setter */
static bool redit_apply_sector(void *entity, olc_pending_change_t *change) {
    ROOM_INDEX_DATA *pRoom = (ROOM_INDEX_DATA *)entity;
    int val = (int)json_integer_value(change->new_value);
    room_set_rs_sector_type(pRoom, val);
    return true;
}

/* Custom: region assignment */
static bool redit_apply_region(void *entity, olc_pending_change_t *change) {
    ROOM_INDEX_DATA *pRoom = (ROOM_INDEX_DATA *)entity;
    const char *val = json_string_value(change->new_value);
    /* Resolve region by name/uid and call area_region_add_room */
    /* Copy logic from existing redit_region */
    return true;
}

/* Custom: parent WNUM (same pattern as oedit/medit) */
static bool redit_apply_parent(void *entity, olc_pending_change_t *change) {
    ROOM_INDEX_DATA *pRoom = (ROOM_INDEX_DATA *)entity;
    const char *val = json_string_value(change->new_value);
    if (IS_NULLSTR(val) || !str_cmp(val, "none") || !str_cmp(val, "0")) {
        pRoom->parent_load = WNUM_LOAD_EMPTY;
        pRoom->parent_wnum = WNUM_EMPTY;
        pRoom->parent = NULL;
        pRoom->parent_inherited = false;
        return true;
    }
    WNUM_LOAD wl;
    if (!parse_widevnum(val, pRoom->area->uid, &wl)) return false;
    ROOM_INDEX_DATA *parent = get_room_index(wl.auid, wl.vnum);
    if (!parent) return false;
    pRoom->parent_load = wl;
    pRoom->parent_wnum = (WNUM){ .pArea = parent->area, .vnum = parent->vnum };
    pRoom->parent = parent;
    pRoom->parent_inherited = false;
    return true;
}
```

- [ ] **Step 2: Add handler table entries**

```c
    { "Locale",           OLC_FIELD_LONG,    NULL, redit_apply_locale,    NULL },
    { "Persist",          OLC_FIELD_BOOL,    NULL, redit_apply_persist,   NULL },
    { "Recall",           OLC_FIELD_STRING,  NULL, redit_apply_recall,    NULL },
    { "Sector",           OLC_FIELD_INT,     NULL, redit_apply_sector,    NULL },
    { "Region",           OLC_FIELD_STRING,  NULL, redit_apply_region,    NULL },
    { "Parent",           OLC_FIELD_STRING,  NULL, redit_apply_parent,    NULL },
    { "var/*",            OLC_FIELD_STRING,  NULL, redit_apply_var,       NULL },
```

- [ ] **Step 3: Convert command functions**

**redit_locale** — Direct replacement:
```c
REDIT(redit_locale) {
    ROOM_INDEX_DATA *pRoom;
    EDIT_ROOM(ch, pRoom);
    return olc_cmd_long(ch, argument, "Locale", NULL,
        &pRoom->locale, 0, LONG_MAX, NULL, NULL);
}
```

**redit_persist** — Keep security check, use `olc_cmd_bool`:
```c
REDIT(redit_persist) {
    ROOM_INDEX_DATA *pRoom;
    EDIT_ROOM(ch, pRoom);
    if (ch->tot_level < MAX_LEVEL - 1) {
        send_to_char("Insufficient security.\n\r", ch);
        return false;
    }
    return olc_cmd_bool(ch, argument, "Persist", NULL,
        /* need a temporary bool — persist is function-based */
        ...);
}
```

**Note:** `redit_persist` is function-based (no simple bool field). The command needs to stage a bool value and the apply function calls persist_addroom/removeroom. Stage directly via `olc_changeset_add_change`.

**redit_sector** — Validate via sector_lookup, stage int value:
```c
REDIT(redit_sector) {
    ROOM_INDEX_DATA *pRoom;
    EDIT_ROOM(ch, pRoom);
    // Validate sector name
    // Stage directly with OLC_FIELD_INT type
}
```

**redit_region** — Validate region exists, stage identifier string.

**redit_recall** — Validate widevnum/coordinates, stage string.

**redit_parent** — Same pattern as oedit/medit parent.

**redit_varset/varclear** — Same pattern as other editors. Like aedit, these also sync to live `progs->vars`; move the sync to the apply function.

- [ ] **Step 4: Build and test**

```bash
cd /sentience/src && ./build tests && ./install debug
cd /sentience && ./sent -test
```

- [ ] **Step 5: Commit**

```bash
cd /sentience/src && git add -A && git commit -m "feat(olc): convert redit simple fields to staged mode

Convert 8 redit commands: locale, persist, recall, sector, region,
parent, varset, varclear.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 10: Final Build + Test Verification

**Files:** None (verification only)

### Steps

- [ ] **Step 1: Clean rebuild**

```bash
cd /sentience/src && ./build clean tests
```

- [ ] **Step 2: Install**

```bash
./install debug
```

- [ ] **Step 3: Run full test suite**

```bash
cd /sentience && ./sent -test
```

Expected: ≥601 tests, ≥585 pass, ≤2 known failures, ≤14 skipped. New tests from Task 4 should add 3+ more.

- [ ] **Step 4: Run OLC-specific tests**

```bash
./sent -test:olccs_
./sent -test:olchist_
```

All should pass.

- [ ] **Step 5: Verify no compiler warnings in new code**

```bash
cd /sentience/src && cat .build/Debug/build.log 2>/dev/null | grep -i 'warning:' | grep -E 'olc_commands|olc_field_handlers|olc_staged|oedit|medit|aedit|redit' | head -20
```

- [ ] **Step 6: Summary commit (if needed)**

If any fixes were needed, commit them. Otherwise, all work is done.

---

## Dependency Graph

```
Task 1 (olc_cmd_long)
  └→ Task 2 (olc_stage_bitvector)
       └→ Task 3 (olc_cmd_dice)
            └→ Task 4 (unit tests)
                 ├→ Task 5 (oedit: 10 functions)
                 ├→ Task 6 (medit simple: 9 functions)
                 │    └→ Task 7 (medit combat: 10 functions)
                 ├→ Task 8 (aedit: 11 functions)
                 └→ Task 9 (redit: 8 functions)
                      └→ Task 10 (final verification)
```

Tasks 5, 6, 8 could theoretically run in parallel after Task 4 (different files), but sequential execution is safer since they share patterns that benefit from iteration.

---

## Notes

1. **`oedit_update` is commented out** in the source (lines 3315–3340 wrapped in `/* */`). It is excluded from this plan.

2. **Labels must match exactly** between `olc_cmd_*` calls and handler table entries. Case matters.

3. **IMP signature timing:** `has_imp_sig()` is checked at staging time (pre-validation). `use_imp_sig()` is called at apply time (in the custom apply function). Between staging and commit, the sig could theoretically be consumed by another operation. In practice this is a single-builder workflow and the risk is negligible.

4. **Variable sync behavioral change:** In aedit and redit, `varset`/`varclear` currently sync to live `progs->vars` immediately. After this conversion, the sync happens at commit time. This is consistent with the staging model.

5. **Bitvector no-op detection:** When `olc_stage_bitvector` computes toggled values identical to the original, `olc_changeset_add_change` returns NULL (no-op). The command should handle this gracefully.

6. **olc_changeset_add_change behavior reminder:** If old_value == new_value, the function removes any existing pending change for that field and returns NULL. If updating an existing change and new_value == original old_value, it also removes and returns NULL.
