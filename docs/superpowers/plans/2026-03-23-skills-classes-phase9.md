# Skills/Classes Phase 9: Legacy Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove all legacy skill/class globals, static tables, and compatibility fields to complete the skills/classes system migration. Phases 0-8 are done — Phase 9 is the final cleanup.

**Architecture:** Phase 1 introduced `SKILL_DATA` as the dynamic backend for skills, bootstrapped from the legacy `skill_table[]`. Phases 2-8 migrated all runtime operations to use `SKILL_DATA` pointers, `CLASS_DATA`, `SONG_DATA`, and list-based storage. Phase 9 removes the legacy scaffolding: 356 `skill_table[]` accesses across 35 files, dead class/music/group tables, PC_DATA legacy arrays, and AFFECT_DATA compatibility fields.

**Tech Stack:** C, CMake/Make dual build system

**Critical constraint:** DO NOT modify anything in `src/tests/` — another agent session is working there.

**Reference docs:**
- Phase 9 checklist: `src/docs/PLAN_backport_skills_classes.md` (Phase 9 section)
- Recent work: `src/docs/WORKLOG_skills_classes.md` (Sessions 16-18)
- SKILL_DATA API: `src/skill_data.h`
- SKILL_DATA struct: `src/merc.h` (search `struct skill_data`)

---

## Migration Pattern Reference

All `skill_table[]` migrations follow these patterns. **Reference this section for every task.**

### Pattern A: Direct field access (most common — ~80% of refs)

```c
// BEFORE:
if (skill_table[sn].spell_fun != spell_null)
    (*skill_table[sn].spell_fun)(sn, level, ch, vo, target);

// AFTER:
SKILL_DATA *skill = skill_find_uid(sn);
if (skill && skill->spell_fun != spell_null)
    (*skill->spell_fun)(sn, level, ch, vo, target);
```

### Pattern B: Name display with null safety

```c
// BEFORE:
send_to_char(ch, "Spell: %s\n", skill_table[sn].name);

// AFTER:
SKILL_DATA *skill = skill_find_uid(sn);
send_to_char(ch, "Spell: %s\n", skill ? skill->name : "unknown");
```

### Pattern C: Reuse existing SKILL_DATA pointer (preferred)

```c
// Many functions already have a SKILL_DATA* from Phases 3-5.
// CHECK the function scope before adding a new skill_find_uid() call.

// BEFORE:
if (skill_table[sn].target == TAR_CHAR_OFFENSIVE)

// AFTER (using existing `skill` pointer in scope):
if (skill->target == TAR_CHAR_OFFENSIVE)
```

### Pattern D: Iteration over all skills

```c
// BEFORE:
for (sn = 0; sn < MAX_SKILL; sn++) {
    if (skill_table[sn].name == NULL) break;
    // ... use skill_table[sn] ...
}

// AFTER: Check skill_data.h for the iteration API.
// SKILL_DATA has a `next` field — there is likely a global linked list.
// Look for: skill_list, skill_first(), SKILL_DATA iteration macros.
// The subagent MUST check skill_data.h to find the correct pattern.
```

### Rules

1. **Always null-check** `skill_find_uid()` return value before dereferencing
2. **Reuse existing SKILL_DATA pointers** — don't add redundant lookups. Search the function for existing `SKILL_DATA *skill` variables.
3. **Add `#include "skill_data.h"`** if not already present in the file
4. **Field names are identical** between `struct skill_type` and `SKILL_DATA` (see mapping below)
5. **The `.race` field type changed**: `int` in skill_type → `RACE_DATA *` in SKILL_DATA. If comparing by race ID, use `skill->race` pointer or `skill->race_name` string instead.

### Field Mapping (struct skill_type → SKILL_DATA)

| skill_type field | SKILL_DATA field | Notes |
|---|---|---|
| `.name` | `.name` | Identical |
| `.spell_fun` | `.spell_fun` | Identical |
| `.target` | `.target` | Identical |
| `.skill_level[MAX_CLASS]` | `.skill_level[MAX_CLASS]` | Identical |
| `.rating[MAX_CLASS]` | `.rating[MAX_CLASS]` | Identical |
| `.min_mana` | `.min_mana` | Identical |
| `.beats` | `.beats` | Identical |
| `.noun_damage` | `.noun_damage` | Identical |
| `.msg_off` | `.msg_off` | Identical |
| `.msg_obj` | `.msg_obj` | Identical |
| `.msg_disp` | `.msg_disp` | Identical |
| `.inks[3][2]` | `.inks[3][2]` | Identical |
| `.race` (int) | `.race` (RACE_DATA*) | **Type changed** — see Rule 5 |
| `.minimum_position` | `.minimum_position` | Identical |

### SKILL_DATA API Quick Reference

```c
SKILL_DATA *skill_find(const char *name);       // Exact name match (case-insensitive)
SKILL_DATA *skill_search(const char *prefix);   // Prefix match (like old skill_lookup)
SKILL_DATA *skill_find_uid(int16_t uid);        // By UID — replaces skill_table[sn]
int16_t     skill_sn(SKILL_DATA *skill);        // Get UID from pointer
```

---

## Standard Workflow (all tasks)

**Build and test after every task:**
```bash
cd /sentience/src && ./build tests && cd /sentience && ./sent -test
```
Expected: Clean build, all tests pass (298+ pass, 0 fail).

**Commit format:**
```bash
git add -A && git commit -m "refactor(phase9): <description>

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Dependency Graph

```
Task 1 (music/group tables) ──────┐
Task 2 (class/subclass tables) ───┤
Task 3 (skill core) ─────────────┤
Task 4 (spell system) ───────────┤
Task 5 (combat system) ──────────┼── Task 9 (remove skill_table) ──┐
Task 6 (player commands) ────────┤                                  │
Task 7 (handlers/persistence) ───┤                                  │
Task 8 (OLC/script/misc) ────────┘                                  │
                                                                     ├── Task 12
Task 10 (paf->type migration) ─────────────────────────────────────┤
Task 11 (LEVEL_HERO/IMMORTAL) ────────────────────────────────────┘
```

**Tasks 1-8, 10-11** can run in any order (no dependencies between them).
**Task 9** depends on Tasks 3-8 (all external skill_table refs migrated).
**Task 12** depends on Tasks 9-11 (all legacy structures removed first).

---

## File Structure

**Files being removed/emptied:**
- `const.c`: `skill_table[MAX_SKILL]` (~4800 lines), `music_table[]`, `group_table[]`, `class_table[]`, `sub_class_table[]`

**Files with major modifications:**
- `merc.h`: Remove `struct skill_type`, legacy externs, PC_DATA legacy fields, AFFECT_DATA `.type`, SKILL_ENTRY `.sn`
- `skills.c` (41 refs), `magic.c` (34 refs), `fight.c` (31 refs), `act_info.c` (28 refs), `act_obj.c` (27 refs), `db.c` (23 refs)

**Files with minor modifications (1-11 refs each):**
- `fight2.c`, `shoot.c`, `act_obj2.c`, `act_move.c`, `act_wiz.c`, `act_class.c`, `nanny.c`
- `handler.c`, `update.c`, `save.c`, `db2.c`
- `olc_act.c`, `olc_save.c`
- `script_ifc.c`, `script_commands.c`, `script_expand.c`, `script_mpcmds.c`, `script_opcmds.c`, `script_rpcmds.c`, `scripts.c`, `script_tpcmds.c`
- `music.c`, `bit.c`, `special.c`
- `io/json/json_area.c`

---

### Task 1: Remove music_table[] and group_table[]

**Scope:** ~12 active refs, ~200 lines of table definitions

**Files:**
- Modify: `const.c` — remove `music_table[]` definition, `group_table[MAX_GROUP]` definition
- Modify: `save.c` — 4 active `music_table` refs (lines ~975, 977, 4052, 4054), 3 `group_table` refs
- Modify: `act_class.c` — 1 active `group_table` ref (line ~340)
- Modify: `db.c` — remove `music_table`/`group_table` boot references
- Modify: `db2.c` — 3 `group_table` refs
- Check: `song_data.c` — 3 `music_table` refs (likely bootstrap-only)
- Modify: `merc.h` — remove extern declarations, `struct music_type`, `struct group_type`, `MAX_GROUP` if unused

**Context:** Phase 8 completed `SONG_DATA` migration. `music_table[]` and `group_table[]` are legacy static arrays superseded by dynamic backends. Active refs in `save.c` likely handle legacy compatibility reads.

- [ ] **Step 1: Audit all music_table references**

```bash
cd /sentience/src && grep -rn '\bmusic_table\b' --include='*.c' --include='*.h' . | grep -v 'tests/'
```

Classify each as: definition, extern decl, active use, comment, or bootstrap.

- [ ] **Step 2: Audit all group_table references**

```bash
cd /sentience/src && grep -rn '\bgroup_table\b' --include='*.c' --include='*.h' . | grep -v 'tests/'
```

Classify each similarly.

- [ ] **Step 3: Migrate active music_table refs in save.c**

Replace with `SONG_DATA` API calls. Check `song_data.h` for the available API.

- [ ] **Step 4: Migrate active group_table refs in save.c, act_class.c, db2.c**

Replace with new group system API. Check how `LLIST *group_known` works in the new class system.

- [ ] **Step 5: Remove table definitions from const.c**

Delete `music_table[]` and `group_table[MAX_GROUP]` arrays entirely.

- [ ] **Step 6: Remove struct definitions and extern declarations from merc.h**

Remove `struct music_type`, `struct group_type`, their extern declarations, and `MAX_GROUP` if no other users.

- [ ] **Step 7: Remove boot references from db.c and song_data.c**

Remove or convert any `music_table`/`group_table` initialization code. If `song_data.c` bootstraps from `music_table`, convert to JSON loading.

- [ ] **Step 8: Build and test**

- [ ] **Step 9: Commit**

```
refactor(phase9): remove music_table[] and group_table[]

Remove legacy static arrays superseded by SONG_DATA and group list backends.
Migrate active refs in save.c, act_class.c, db2.c to dynamic APIs.
```

---

### Task 2: Remove class_table[] and sub_class_table[]

**Scope:** ~2 active refs, ~100 lines of table definitions

**Files:**
- Modify: `const.c` — remove `class_table[]` and `sub_class_table[]` definitions
- Modify: `act_wiz.c` — 1 active `sub_class_table` ref (line ~10718)
- Modify: `merc.h` — remove extern declarations, `struct class_type`/`sub_class_type` definitions, `CLASS_MAGE`/`CLASS_CLERIC`/`CLASS_THIEF`/`CLASS_WARRIOR` index constants
- Clean: Comments in `db.c`, `save.c`, `class_data.c` referencing these tables

**Context:** `CLASS_DATA` backend fully replaces `class_table[]`. Only 1 active `sub_class_table` ref remains in `act_wiz.c`.

- [ ] **Step 1: Audit all class_table and sub_class_table references**

```bash
cd /sentience/src && grep -rn '\bclass_table\b\|\bsub_class_table\b' --include='*.c' --include='*.h' . | grep -v 'tests/'
```

- [ ] **Step 2: Migrate active sub_class_table ref in act_wiz.c**

Replace with `CLASS_DATA` API call. Check `class_data.h` for available functions.

- [ ] **Step 3: Remove table definitions from const.c**

- [ ] **Step 4: Remove extern declarations, struct definitions, and CLASS_* constants from merc.h**

Remove: `struct class_type`, `struct sub_class_type`, `extern class_table[]`, `extern sub_class_table[]`, `CLASS_MAGE`, `CLASS_CLERIC`, `CLASS_THIEF`, `CLASS_WARRIOR`.
Keep: `CLASS_TYPE_*` constants (used by new system).

- [ ] **Step 5: Clean up stale comments**

Remove or update comments in `db.c`, `save.c`, `class_data.c` that reference removed tables.

- [ ] **Step 6: Build and test**

- [ ] **Step 7: Commit**

```
refactor(phase9): remove class_table[] and sub_class_table[]

Remove legacy static class arrays superseded by CLASS_DATA backend.
Remove CLASS_MAGE/CLERIC/THIEF/WARRIOR index constants (CLASS_TYPE_* retained).
```

---

### Task 3: skill_table migration — Skill system core

**Scope:** ~50 refs across 3 files

**Files:**
- Modify: `skills.c` (41 refs — highest count)
- Modify: `act_class.c` (skill/class lookup refs)
- Modify: `nanny.c` (character creation skill refs)

**Pattern:** Apply Patterns A-D from Migration Pattern Reference. Many call sites in `skills.c` likely already have `SKILL_DATA*` pointers — use Pattern C where possible.

- [ ] **Step 1: Audit skill_table refs in skills.c**

```bash
cd /sentience/src && grep -n '\bskill_table\b' skills.c
```

For each ref: identify which field is accessed, check if a `SKILL_DATA*` already exists in scope.

- [ ] **Step 2: Convert skill_table refs in skills.c**

Apply migration patterns. For iteration loops (`for sn = 0; sn < MAX_SKILL`), check `skill_data.h` for the correct iteration API first.

- [ ] **Step 3: Audit and convert in act_class.c and nanny.c**

```bash
cd /sentience/src && grep -n '\bskill_table\b' act_class.c nanny.c
```

- [ ] **Step 4: Build and test**

- [ ] **Step 5: Commit**

```
refactor(phase9): migrate skill_table refs in skill system core

Convert skills.c, act_class.c, nanny.c from skill_table[sn] to SKILL_DATA API.
```

---

### Task 4: skill_table migration — Spell system

**Scope:** ~34 refs in 1 file

**Files:**
- Modify: `magic.c` (34 refs)

**Pattern:** Most spell functions already have `SKILL_DATA*` from Phase 3 signature migration. Heavily use Pattern C.

- [ ] **Step 1: Audit**

```bash
cd /sentience/src && grep -n '\bskill_table\b' magic.c
```

- [ ] **Step 2: Convert all refs**

Most should use existing `skill` pointer (Pattern C). For any without, use `skill_find_uid(sn)`.

- [ ] **Step 3: Build and test**

- [ ] **Step 4: Commit**

```
refactor(phase9): migrate skill_table refs in magic.c
```

---

### Task 5: skill_table migration — Combat system

**Scope:** ~45 refs across 3 files

**Files:**
- Modify: `fight.c` (31 refs)
- Modify: `fight2.c` (~5-10 refs)
- Modify: `shoot.c` (~5-10 refs)

- [ ] **Step 1: Audit**

```bash
cd /sentience/src && grep -n '\bskill_table\b' fight.c fight2.c shoot.c
```

- [ ] **Step 2: Convert all refs**

- [ ] **Step 3: Build and test**

- [ ] **Step 4: Commit**

```
refactor(phase9): migrate skill_table refs in combat system

Convert fight.c, fight2.c, shoot.c to SKILL_DATA API.
```

---

### Task 6: skill_table migration — Player commands

**Scope:** ~70 refs across 5 files

**Files:**
- Modify: `act_info.c` (28 refs)
- Modify: `act_obj.c` (27 refs)
- Modify: `act_obj2.c` (~5 refs)
- Modify: `act_move.c` (~5 refs)
- Modify: `act_wiz.c` (11 refs)

- [ ] **Step 1: Audit**

```bash
cd /sentience/src && grep -n '\bskill_table\b' act_info.c act_obj.c act_obj2.c act_move.c act_wiz.c
```

- [ ] **Step 2: Convert all refs**

- [ ] **Step 3: Build and test**

- [ ] **Step 4: Commit**

```
refactor(phase9): migrate skill_table refs in player commands

Convert act_info.c, act_obj.c, act_obj2.c, act_move.c, act_wiz.c to SKILL_DATA API.
```

---

### Task 7: skill_table migration — Core handlers and persistence

**Scope:** ~55 refs across 5 files

**Files:**
- Modify: `handler.c` (~10 refs)
- Modify: `update.c` (~5 refs)
- Modify: `save.c` (~10 refs)
- Modify: `db.c` (23 refs — **NOTE:** includes skill_data bootstrap code)
- Modify: `db2.c` (~5 refs)

**Special note for db.c:** This file likely contains the `skill_data` bootstrap that reads `skill_table[]` to populate `SKILL_DATA` structs on boot. **DO NOT remove the bootstrap code** — that is Task 9's job. Only migrate non-bootstrap refs (display, lookup, validation code).

- [ ] **Step 1: Audit**

```bash
cd /sentience/src && grep -n '\bskill_table\b' handler.c update.c save.c db.c db2.c
```

- [ ] **Step 2: In db.c, classify each ref as bootstrap vs. active usage**

Bootstrap refs: any code that populates `SKILL_DATA` from `skill_table[]` — leave these alone.
Active usage refs: everything else — convert these.

- [ ] **Step 3: Convert non-bootstrap refs in all 5 files**

- [ ] **Step 4: Build and test**

- [ ] **Step 5: Commit**

```
refactor(phase9): migrate skill_table refs in handlers and persistence

Convert handler.c, update.c, save.c, db.c, db2.c to SKILL_DATA API.
Preserves skill_data bootstrap in db.c for Task 9.
```

---

### Task 8: skill_table migration — OLC, scripting, and remaining files

**Scope:** ~85 refs across ~15 files

**Files:**
- Modify: `olc_act.c` (~10 refs)
- Modify: `olc_save.c` (~5 refs)
- Modify: `script_ifc.c`, `script_commands.c`, `script_expand.c`, `script_mpcmds.c`, `script_opcmds.c`, `script_rpcmds.c`, `scripts.c`, `script_tpcmds.c` (combined ~30 refs)
- Modify: `music.c` (10 refs)
- Modify: `bit.c` (~5 refs)
- Modify: `special.c` (~5 refs)
- Modify: `io/json/json_area.c` (11 refs)
- Check for any other files not covered by Tasks 3-7

- [ ] **Step 1: Find ALL remaining skill_table refs (excluding const.c and tests/)**

```bash
cd /sentience/src && grep -rn '\bskill_table\b' --include='*.c' . | grep -v 'const.c' | grep -v 'tests/'
```

Any file not already handled by Tasks 3-7 gets handled here.

- [ ] **Step 2: Convert all refs in each file**

Apply migration patterns. For scripting files, be especially careful about the scripting API — scripts may pass `sn` values that need to work with `skill_find_uid()`.

- [ ] **Step 3: Verify no remaining external refs**

```bash
cd /sentience/src && grep -rn '\bskill_table\b' --include='*.c' . | grep -v 'const.c' | grep -v 'tests/'
```

Expected: Zero results.

- [ ] **Step 4: Build and test**

- [ ] **Step 5: Commit**

```
refactor(phase9): migrate skill_table refs in OLC, scripting, and remaining files

Final batch: olc_act.c, olc_save.c, script_*.c, music.c, bit.c,
special.c, io/json/json_area.c.
```

---

### Task 9: Remove skill_table[] definition

**Depends on:** Tasks 3-8 complete (all external refs migrated)

**Scope:** ~4800 lines of table data + struct definition

**Files:**
- Modify: `const.c` — remove `skill_table[MAX_SKILL]` array (~4800 lines)
- Modify: `merc.h` — remove `struct skill_type` definition, `extern skill_table[]` declaration
- Modify: `db.c` or `skill_data.c` — convert bootstrap mechanism

**Critical decision:** How does `skill_data.c` bootstrap without `skill_table`?
- **Option A (simpler):** Move `skill_table[]` into `skill_data.c` as `static const` — still used for bootstrap, but no longer extern.
- **Option B (cleaner):** Remove `skill_table[]` entirely — skills load from JSON on boot (requires JSON skill definitions to exist).
- Check `skill_data.c` to determine which option is feasible.

- [ ] **Step 1: Investigate bootstrap mechanism**

```bash
cd /sentience/src
grep -n 'skill_table\|bootstrap\|skill_init\|skill_boot\|skill_load' skill_data.c db.c
```

Determine: Does `skill_data.c` already have JSON loading? Is `skill_table[]` the sole bootstrap source?

- [ ] **Step 2: Convert or preserve bootstrap**

If JSON loading exists → Option B (remove `skill_table[]` entirely).
If `skill_table[]` is the only source → Option A (make it private to `skill_data.c`).

- [ ] **Step 3: Remove extern declarations from merc.h**

Remove: `struct skill_type` definition, `extern const struct skill_type skill_table[MAX_SKILL]`.
Evaluate: Can `MAX_SKILL` be removed or moved to `skill_data.h`? Check all users.

- [ ] **Step 4: Verify no remaining refs**

```bash
cd /sentience/src && grep -rn '\bskill_table\b' --include='*.c' --include='*.h' . | grep -v 'tests/'
```

Expected: Zero (Option B) or only in `skill_data.c` (Option A).

- [ ] **Step 5: Build and test**

- [ ] **Step 6: Commit**

```
refactor(phase9): remove skill_table[] extern and struct skill_type

[If Option A]: Move skill_table to skill_data.c as private bootstrap source.
[If Option B]: Remove skill_table entirely — skills now load from JSON.
Remove struct skill_type from merc.h (~4800 lines of static data removed).
```

---

### Task 10: AFFECT_DATA.type → .skill pointer migration

**Scope:** Up to 312 SET operations + 15 READ comparisons (many already converted in Phase 5)

**Files:**
- Audit all files with `paf->type` assignments and comparisons
- Primary: `handler.c`, `magic.c`, `magic_*.c`, `fight.c`, `update.c`, `save.c`, `skill_data.c`

**Context:** Phase 5 added `SKILL_DATA *skill` to `AFFECT_DATA` and converted 61+ SET sites. Session 18 added pointer-first `affect_matches_skill_sn()` helper in `handler.c`. Remaining work: convert any unconverted sites.

- [ ] **Step 1: Audit remaining paf->type SET operations**

```bash
cd /sentience/src && grep -rn 'paf->type\s*=' --include='*.c' . | grep -v 'tests/' | grep -v '//'
```

For each, check if `paf->skill` is also set nearby. If yes → already converted (dual-write). If no → needs conversion.

- [ ] **Step 2: Audit paf->type READ comparisons**

```bash
cd /sentience/src && grep -rn 'paf->type\b' --include='*.c' . | grep -v 'tests/' | grep -v '//' | grep -v 'paf->type\s*='
```

These comparison sites need migration to use `paf->skill` pointer or `affect_matches_skill_sn()`.

- [ ] **Step 3: Convert unconverted SET operations**

For each `paf->type = sn` without a corresponding `paf->skill = ...`:
```c
// ADD alongside existing paf->type = sn:
paf->skill = skill_find_uid(sn);
```

- [ ] **Step 4: Convert READ comparisons**

Replace `paf->type == sn` with pointer comparison or `affect_matches_skill_sn()`:
```c
// BEFORE:
if (paf->type == gsn_something)

// AFTER (using helper):
if (affect_matches_skill_sn(paf, sn))

// OR (direct pointer comparison):
SKILL_DATA *target_skill = skill_find_uid(sn);
if (paf->skill == target_skill)
```

- [ ] **Step 5: Build and test**

- [ ] **Step 6: Commit**

```
refactor(phase9): complete AFFECT_DATA.type → .skill migration

All affect SET and READ sites now use SKILL_DATA pointer.
Legacy .type field retained but no longer actively used (removed in Task 12).
```

---

### Task 11: LEVEL_HERO and LEVEL_IMMORTAL cleanup

**Scope:** ~60 refs across .c and .h files

**Files:**
- Audit all files using `LEVEL_HERO` or `LEVEL_IMMORTAL`
- Modify: `merc.h` (constant definitions)

**Context:** `IS_HERO()` and `IS_TRUSTED()` macros were already removed (zero refs). `get_trust()` was already removed/unused. `LEVEL_HERO`/`LEVEL_IMMORTAL` are used in ~60 places for both NPC level checks and player authority checks.

- [ ] **Step 1: Audit all usage**

```bash
cd /sentience/src && grep -rn 'LEVEL_HERO\|LEVEL_IMMORTAL' --include='*.c' --include='*.h' . | grep -v 'tests/'
```

Classify each usage into:
- **NPC level check** (e.g., `mob->level >= LEVEL_HERO`) — KEEP as NPC constant
- **Player authority check** (e.g., `ch->level >= LEVEL_IMMORTAL`) — REPLACE with `IS_STAFF()` or `IS_IMMORTAL()`
- **Display/threshold** — evaluate case by case

- [ ] **Step 2: Plan replacements based on classification**

- [ ] **Step 3: Implement replacements**

For player authority checks, verify `IS_STAFF()` or equivalent exists and has the correct semantics.

- [ ] **Step 4: Build and test**

- [ ] **Step 5: Commit**

```
refactor(phase9): clean up LEVEL_HERO/LEVEL_IMMORTAL usage

Restrict to NPC-only contexts. Replace player authority checks with IS_STAFF().
```

---

### Task 12: PC_DATA legacy field removal and final cleanup

**Depends on:** Tasks 9-11 complete

**Scope:** Remove ~20 fields from PC_DATA, 3 fields from other structs

**Files:**
- Modify: `merc.h` — remove fields (see list below)
- Modify: `save.c` — remove read/write code for removed fields
- Modify: Any files that reference removed fields

**Fields to remove from PC_DATA (merc.h lines ~5866-5893):**
- `int16_t learned[MAX_SKILL]` and `mod_learned[MAX_SKILL]` — replaced by `SKILL_ENTRY.rating/mod_rating`
- 16 legacy class fields: `class_mage`, `second_class_mage`, `sub_class_mage`, `second_sub_class_mage` (×4 types: mage, cleric, thief, warrior) — replaced by `LLIST *classes`
- `class_current` and `sub_class_current` — replaced by `CLASS_DATA *class_current`
- `bool group_known[MAX_GROUP]` — replaced by `LLIST *group_known`

**Fields to remove from other structs:**
- `SKILL_DATA.pgsn` — if present (may already be removed; verify)
- `SKILL_ENTRY.sn` (int16_t) — replaced by `SKILL_ENTRY.skill` (SKILL_DATA pointer)
- `AFFECT_DATA.type` (int16_t) — replaced by `AFFECT_DATA.skill` (SKILL_DATA pointer)

- [ ] **Step 1: Verify no remaining callers of PC_DATA fields being removed**

```bash
cd /sentience/src
grep -rn '->learned\[' --include='*.c' . | grep -v 'tests/'
grep -rn '->mod_learned\[' --include='*.c' . | grep -v 'tests/'
grep -rn '->class_mage\|->class_cleric\|->class_thief\|->class_warrior' --include='*.c' . | grep -v 'tests/'
grep -rn '->second_class_\|->sub_class_mage\|->sub_class_cleric\|->sub_class_thief\|->sub_class_warrior\|->second_sub_class_' --include='*.c' . | grep -v 'tests/'
grep -rn '->class_current\b' --include='*.c' . | grep -v 'tests/' | head -20
grep -rn '->sub_class_current\b' --include='*.c' . | grep -v 'tests/' | head -20
grep -rn '->group_known\[' --include='*.c' . | grep -v 'tests/'
```

If any active callers exist, migrate them first before removing the fields.

- [ ] **Step 2: Check SKILL_DATA.pgsn, SKILL_ENTRY.sn, AFFECT_DATA.type**

```bash
cd /sentience/src
grep -rn '->pgsn\b\|\.pgsn\b' --include='*.c' . | grep -v 'tests/'
grep -rn '->sn\b\|\.sn\b' --include='*.c' . | grep -v 'tests/' | grep -i 'skill_entry\|skent' | head -20
grep -rn 'paf->type\b\|af->type\b' --include='*.c' . | grep -v 'tests/' | head -20
```

These should have zero active users after Tasks 9-10. If any remain, migrate them.

- [ ] **Step 3: Remove fields from merc.h**

Remove all listed fields. Update any sizeof/initialization code that references them.

- [ ] **Step 4: Remove read/write code from save.c**

Remove any JSON read/write code for the removed fields. The new system fields (`SKILL_ENTRY.skill`, `LLIST *classes`, etc.) should already have their own serialization.

- [ ] **Step 5: Remove any other references to removed fields**

```bash
cd /sentience/src && grep -rn 'learned\[MAX_SKILL\]\|mod_learned\|class_mage\|class_cleric\|class_thief\|class_warrior\|group_known\[MAX' --include='*.c' --include='*.h' . | grep -v 'tests/'
```

- [ ] **Step 6: Build and test**

- [ ] **Step 7: Commit**

```
refactor(phase9): remove legacy PC_DATA fields and compatibility shims

Remove: learned[]/mod_learned[], 16 class fields, class_current,
sub_class_current, group_known[]. Remove pgsn from SKILL_DATA,
sn from SKILL_ENTRY, type from AFFECT_DATA.

Phase 9 COMPLETE — skills/classes system fully migrated to dynamic backends.
```

---

## Summary

| Task | Description | Refs | Dependencies |
|------|-------------|------|-------------|
| 1 | Remove music_table + group_table | ~12 | None |
| 2 | Remove class_table + sub_class_table | ~6 | None |
| 3 | skill_table migration: skill core | ~50 | None |
| 4 | skill_table migration: spell system | ~34 | None |
| 5 | skill_table migration: combat | ~45 | None |
| 6 | skill_table migration: player commands | ~70 | None |
| 7 | skill_table migration: handlers/persistence | ~55 | None |
| 8 | skill_table migration: OLC/script/misc | ~85 | None |
| 9 | Remove skill_table[] definition | ~4800 lines | Tasks 3-8 |
| 10 | AFFECT_DATA.type migration | ~327 | None |
| 11 | LEVEL_HERO/IMMORTAL cleanup | ~60 | None |
| 12 | PC_DATA field removal + final cleanup | ~20 fields | Tasks 9-11 |

**Total estimated scope:** ~750 code changes across ~35 files, ~5000+ lines removed.
