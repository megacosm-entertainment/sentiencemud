# Design Spec: OLC Simple Field Staging (Phase 2)

## Problem

After Phase 1 (commit history + initial field staging), many editor command functions
still modify entity data directly, bypassing the staged changeset system. This means
changes via those commands are not tracked, not reviewable via `pending`, not revertable
via `history revert`, and invisible to the GMCP web client.

**Current state:** 68 staged functions, 140 unstaged functions across 5 editor files.

**Goal:** Convert all simple/scalar field commands to use staged changesets, bringing the
total staged count to ~115+ and reducing unstaged to only complex structure operations.

## Scope

### In Scope (This Phase)

All unstaged commands that modify **scalar fields** (single values, flags, dice, variables):

| Editor | File | Unstaged → Convert | Remain Unstaged |
|--------|------|--------------------|-----------------|
| oedit  | oedit.c | ~12 functions | ~28 (complex) |
| medit  | medit.c | ~19 functions | ~8 (complex) |
| aedit  | aedit.c | ~11 functions | ~2 (complex) |
| redit  | redit.c | ~8 functions | ~20 (complex) |
| **Total** | | **~51 functions** | **~57 (deferred)** |

### Out of Scope (Deferred to Phase 3: Complex Structures)

- Exit management (10 direction commands + dislink)
- Reset management (mreset, oreset)
- List add/remove operations (affects, spells, skills, catalysts, quests, reputation)
- Extra descriptions (ed, addcdesc, editcdesc, delcdesc)
- Lock structures, waypoints
- Type management (addtype, removetype, type)
- All 32 oedit_types.c functions (type-specific value editors)
- Shop, trainer, questor, crew configuration
- medit_race (OR-merges ~9 flag fields from race data — complex multi-field revert)
- Region management
- Coordinate management (coords — multi-field + wilderness link)
- Builder list management (aedit_builder — string list add/remove)
- oedit_value0-7 (type-dependent — delegate to oedit_values helper)

## New Infrastructure

### 1. `olc_cmd_long` — Long Integer Helper

```c
bool olc_cmd_long(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, long *field_ptr, long min_val, long max_val,
    void *ctx, olc_cmd_record_fn record_fn);
```

Mirrors `olc_cmd_number` but for `long` fields. Needed for:
- `oedit_cost` → `pObj->cost` (long)
- `aedit_wilds` → `pArea->wilds_uid` (long)
- `redit_locale` → `pRoom->locale` (long)

**Field type:** `OLC_FIELD_LONG` (new enum value in `olc_changeset.h`)

**Generic apply:**
```c
bool olc_apply_generic_long(long *field_ptr, olc_pending_change_t *change);
```

**Apply macro:**
```c
#define OLC_FIELD_APPLY_LONG(func_name, entity_type, member) \
    static bool func_name(void *entity, olc_pending_change_t *change) { \
        return olc_apply_generic_long(&((entity_type *)entity)->member, change); \
    }
```

### 2. `olc_cmd_bitvector` — Multi-Bank Bitvector Helper

```c
bool olc_cmd_bitvector(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, long *banks, int nbanks, ...);
```

Wraps `bitvector_lookup()` and records the change as a JSON object with per-bank
old/new values. Uses the existing `OLC_FIELD_MULTIFLAGS` type.

Needed for:
- `oedit_extra` → `pObj->extra[0..3]` (4 banks, 4 flag tables)
- `medit_act` → `pMob->act[0..1]` (2 banks, 2 flag tables)
- `medit_affect` → `pMob->affected_by[0..1]` (2 banks, 2 flag tables)

**Note:** `oedit_extra2`, `oedit_extra3`, `oedit_extra4` are already commented out in
the source — `oedit_extra` handles all 4 banks via `bitvector_lookup()`. No conversion
needed for extra2/3/4.

The existing `oedit_apply_extra` and `oedit_serialize_extra` handler functions already
handle the multi-bank apply for oedit. Equivalent apply functions needed for medit_act
and medit_affect.

### 3. `olc_cmd_dice` — Dice Formula Helper

```c
bool olc_cmd_dice(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, DICE_DATA *dice_ptr,
    void *ctx, olc_cmd_record_fn record_fn);
```

Parses `XdY+Z` format, validates, records as JSON `{"number":X,"size":Y,"bonus":Z}`.

Needed for:
- `medit_hitdice` → `pMob->hit` (DICE_DATA)
- `medit_manadice` → `pMob->mana` (DICE_DATA)
- `medit_damdice` → `pMob->damage` (DICE_DATA)

**Note:** `medit_movedice` modifies `pMob->move` which is a plain `int`, not DICE_DATA.

**Field type:** Reuse `OLC_FIELD_EMBEDDED` (JSON object with named sub-fields).

**Generic apply:**
```c
bool olc_apply_generic_dice(DICE_DATA *ptr, olc_pending_change_t *change);
```

**Apply macro:**
```c
#define OLC_FIELD_APPLY_DICE(func_name, entity_type, member) \
    static bool func_name(void *entity, olc_pending_change_t *change) { \
        return olc_apply_generic_dice(&((entity_type *)entity)->member, change); \
    }
```

## Conversion Patterns

### Pattern A: Direct olc_cmd_* Replacement

For simple scalars that map directly to an existing or new helper:

```c
// BEFORE:
MEDIT(medit_gold) {
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    if (argument[0] == '\0') { /* syntax */ return false; }
    pMob->wealth = atoi(argument);
    send_to_char("Gold set.\n\r", ch);
    return true;
}

// AFTER:
MEDIT(medit_gold) {
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_number(ch, argument, "Gold", NULL,
        &pMob->wealth, 0, INT_MAX, NULL, NULL);
}
```

**Applicable to:** oedit_cost, oedit_update, medit_gold, medit_movedice,
aedit_areawho, aedit_placetype, redit_locale, redit_persist, redit_region.

### Pattern B: Pre-Validation Then olc_cmd_*

For fields where the command validates input before staging:

```c
// medit_spec: validate spec function exists, then stage
MEDIT(medit_spec) {
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    if (IS_NULLSTR(argument)) { /* syntax */ return false; }
    // Validation
    if (spec_lookup(argument) == NULL) {
        send_to_char("Invalid spec function.\n\r", ch);
        return false;
    }
    // Stage as string (spec name) — apply function resolves to pointer
    return olc_cmd_string(ch, argument, "Spec", NULL,
        ...);
}
```

**Applicable to:** medit_spec, medit_race, medit_position, aedit_file, oedit_fragility,
oedit_wear, redit_sector, aedit_wilds, aedit_recall, aedit_airshipland, aedit_postoffice.

### Pattern C: Side-Effect Fields (Custom Apply)

For fields where COMMIT needs additional logic beyond assignment. The command stages
normally via olc_cmd_*, but the apply function includes side effects:

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

**Applicable to:** oedit_persist, oedit_fragility (use_imp_sig for "solid"),
medit_persist, medit_boss.

### Pattern D: Level Cascades (Custom Apply)

Level commands trigger recalculation of derived fields. Side effects at commit time:

```c
static bool oedit_apply_level(void *entity, olc_pending_change_t *change) {
    OBJ_INDEX_DATA *pObj = (OBJ_INDEX_DATA *)entity;
    if (!olc_apply_generic_int(&pObj->level, change))
        return false;
    pObj->points = object_point_value(pObj);
    // Weapon dice / armor AC recalculation as needed
    return true;
}
```

**Applicable to:** oedit_level (points + weapon dice + armor), medit_level (4 dice).

### Pattern E: Multi-Field Commands

Commands setting multiple related fields from one argument:

- `medit_position` — sets `start_pos` and `default_pos`
- `aedit_vnum` — sets `min_vnum` and `max_vnum`
- `aedit_levels` — sets `min_level` and `max_level`

**Approach:** Stage each sub-field as a separate change with its own label/handler.
The command calls olc_cmd_* twice (once per field).

### Pattern F: Parent WNUM References

`oedit_parent`, `medit_parent`, `redit_parent` all: resolve `auid#vnum` to WNUM_LOAD,
validate target exists, set parent_load + parent_wnum + parent + parent_inherited.

**Approach:** Stage the raw WNUM string. Custom apply function resolves the reference
at commit time. Uses `OLC_FIELD_STRING` for the staged value, custom apply for resolution.

### Pattern G: Variable Set/Clear

`varset` and `varclear` operate on key-value dictionaries (`index_vars`).

**Approach:** Record with field_path `"var:<keyname>"`. Old value captures previous value
(or null), new value captures the new value (or null for clear). Custom apply function
calls `olc_varset()`/`olc_varclear()` at commit time.

This requires staging directly (not via olc_cmd_*) since varset/varclear don't map to
any standard helper pattern.

### Pattern H: Sign (Creator Signature)

`oedit_sign` and `medit_sign` manage creator signatures with permission validation.
Stage via `olc_cmd_string` — permission check stays in command, staging is standard.

## Per-Editor Conversion List

### oedit.c (~12 functions)

| Function | Field(s) | Pattern | Notes |
|----------|----------|---------|-------|
| oedit_cost | cost (long) | A (olc_cmd_long) | |
| oedit_level | level + derived | D (cascade apply) | Points, weapon dice, armor |
| oedit_persist | persist (bool) | C (use_imp_sig) | |
| oedit_sign | imp_sig (string) | H | Permission check in command |
| oedit_parent | parent_load etc. | F (WNUM) | |
| oedit_fragility | fragility (int) | C (use_imp_sig) | Enum-like values |
| oedit_update | update (int) | A (olc_cmd_number) | |
| oedit_extra | extra[0..3] | B (olc_cmd_bitvector) | Already has handler entry |
| oedit_wear | wear_flags (long) | B (olc_cmd_flag_toggle) | Single-slot validation before toggle |
| oedit_varset | index_vars | G | |
| oedit_varclear | index_vars | G | |

### medit.c (~20 functions)

| Function | Field(s) | Pattern | Notes |
|----------|----------|---------|-------|
| medit_persist | persist (bool) | C (use_imp_sig) | |
| medit_boss | boss (bool) | C (use_imp_sig) | |
| medit_spec | spec_fun (ptr) | B + custom apply | |
| medit_level | level + dice cascade | D | Recalculates 4 dice |
| medit_sign | sig (string) | H | |
| medit_parent | parent_load etc. | F (WNUM) | |
| medit_corpsevnum | (via index) | A | |
| medit_zombievnum | (via index) | A | |
| medit_act | act[0..1] | B (olc_cmd_bitvector) | Force ACT_IS_NPC on |
| medit_affect | affected_by[0..1] | B (olc_cmd_bitvector) | |
| medit_ac | ac[4] (int16_t) | Custom | 4-value array |
| medit_hitdice | hit (DICE_DATA) | A (olc_cmd_dice) | |
| medit_manadice | mana (DICE_DATA) | A (olc_cmd_dice) | |
| medit_damdice | damage (DICE_DATA) | A (olc_cmd_dice) | |
| medit_movedice | move (long) | A (olc_cmd_long) | Plain long, not dice |
| medit_gold | wealth (long) | A (olc_cmd_long) | |
| medit_position | start_pos, default_pos (int16_t) | E (olc_cmd_type_set_i16) | 2 fields |
| medit_varset | index_vars | G | |
| medit_varclear | index_vars | G | |

### aedit.c (~11 functions)

| Function | Field(s) | Pattern | Notes |
|----------|----------|---------|-------|
| aedit_wilds | wilds_uid (long) | B (olc_cmd_long) | Validate UID exists first |
| aedit_areawho | area_who (int) | B (olc_cmd_type_set) | |
| aedit_placetype | place_flags (int) | B (olc_cmd_type_set) | |
| aedit_file | file_name (string) | B | Filename validation |
| aedit_recall | recall WNUM | B | Vnum lookup |
| aedit_airshipland | airship_land (WNUM) | B | Vnum lookup |
| aedit_postoffice | postoffice (WNUM) | B | Vnum lookup |
| aedit_vnum | min/max_vnum (2 ints) | E | Overlap validation |
| aedit_levels | min/max_level (2 ints) | E | Range validation |
| aedit_varset | index_vars | G | |
| aedit_varclear | index_vars | G | |

### redit.c (~7 functions)

| Function | Field(s) | Pattern | Notes |
|----------|----------|---------|-------|
| redit_locale | locale (long) | A (olc_cmd_long) | |
| redit_persist | persist (bool) | A (olc_cmd_bool) | |
| redit_recall | recall WNUM | B | Vnum lookup |
| redit_sector | sector (int) | B (olc_cmd_type_set) | sector_lookup |
| redit_region | region (int) | B | Region validation |
| redit_parent | parent_load etc. | F (WNUM) | |
| redit_varset | index_vars | G | |
| redit_varclear | index_vars | G | |

## Field Handler Table Updates

Every converted field needs:
1. An `OLC_FIELD_APPLY_*` macro invocation (or custom apply function)
2. An entry in the editor's `*_field_handlers[]` table
3. The field label in the handler must EXACTLY match the label passed to `olc_cmd_*`

The existing handler table limit is `OLC_MAX_FIELD_HANDLERS = 64`. Current + projected:
- oedit: 15 → ~27 entries (within limit)
- medit: 30 → ~50 entries (within limit)
- aedit: 16 → ~27 entries (within limit)
- redit: 8 → ~15 entries (within limit)

## Testing

- Existing OLC changeset tests (31) and history tests (6) verify the framework
- Each new `olc_cmd_*` helper (long, bitvector, dice) needs basic unit tests
- Verify: stage a change → pending shows it → commit → history records it
- Add tests to existing `olc_changeset_tests.c` / new test file as needed
- Run full suite to confirm no regressions (baseline: 601 tests, 585 pass, 2 known failures, 14 skipped)

## Implementation Order

1. Add new infrastructure (olc_cmd_long, olc_cmd_bitvector, olc_cmd_dice + field types + apply macros)
2. Add unit tests for new helpers
3. Convert oedit simple fields
4. Convert medit simple fields
5. Convert aedit simple fields
6. Convert redit simple fields
7. Final verification
