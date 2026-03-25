# Object Multityping Phase 5: Verification & Closure Plan

> **STATUS: COMPLETE (2026-03-24)** — All 3 tasks executed. 9 macros converted, Phase 5 closed as MOSTLY COMPLETE.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Verify that Phase 5 (game logic conversion from `obj->value[]` to type accessor macros) is complete, fix any remaining gaps, and close Phase 5.

**Architecture:** Phases 1-4 built the multitype data structures, versioning, serialization, and deprecated writes. Phase 5 converted game logic from `obj->value[N]` to type accessor macros (`WEAPON(obj)->damage.number`, `PORTAL(obj)->destination`, etc.). Initial audit found **zero** direct `->value[]` access in game code and **300+ accessor macro calls** already in use. This plan performs comprehensive verification, checks for hidden macro-based `value[]` access, and closes Phase 5.

**Tech Stack:** C, grep/ripgrep for audit, CMake/Make build system

**Critical constraint:** DO NOT modify anything in `src/tests/` — another agent session is working there.

**Reference docs:**
- Type structs and accessor macros: `src/item_types.h`
- Phase 5 plan: `src/docs/PLAN_backport_object_multityping.md`

---

## Standard Workflow (all tasks)

After making changes:
```bash
cd /sentience/src && ./build tests && cd /sentience && ./sent -test
```
Expected: Clean build, all tests pass (298+ pass, 0 fail).

---

### Task 1: Comprehensive value[] Audit

**Files:** All `.c` and `.h` files in `src/` and subdirectories (excluding `tests/`)

- [x] **Step 1: Search all C source files for direct value[] access**

```bash
cd /sentience/src
grep -rn '->value\[' --include='*.c' . | grep -v 'tests/' | grep -v '//'
```

Expected: Zero results in non-test, non-comment code.

- [x] **Step 2: Search all header files for macros that hide value[] access**

```bash
cd /sentience/src
grep -rn 'value\[' --include='*.h' . | grep -v 'tests/'
```

**Known hidden macros using value[] in merc.h:**
- `IS_WEAPON_STAT(obj,stat)` — uses `(obj)->value[4]`
- `WEIGHT_MULT(obj)` — uses `(obj)->value[0]`
- `CORPSE_TYPE`, `CORPSE_RESURRECT`, `CORPSE_ANIMATE`, `CORPSE_PARTS`, `CORPSE_FLAGS`, `CORPSE_MOBILE`, `CORPSE_MOBILE_AUID` — all use `(obj)->value[N]`

Document any additional macros found beyond these.

**Note:** `token->value[]` (`TOKEN_DATA`) is a completely different system — ignore those results.

- [x] **Step 3: Check if value[]-based macros have active callers**

```bash
cd /sentience/src
grep -rn 'IS_WEAPON_STAT\|WEIGHT_MULT\|CORPSE_TYPE\|CORPSE_RESURRECT\|CORPSE_ANIMATE\|CORPSE_PARTS\|CORPSE_FLAGS\|CORPSE_MOBILE\b' --include='*.c' . | grep -v 'tests/'
```

If callers exist, they indirectly use `value[]` and need conversion in Task 2.

- [x] **Step 4: Check scripting layer specifically**

```bash
cd /sentience/src
grep -rn '->value\[' script_*.c | grep -v '//' | grep -v 'token'
grep -rn 'IS_WEAPON_STAT\|WEIGHT_MULT\|CORPSE_' script_*.c | grep -v '//'
```

Expected: Zero direct `value[]` access (scripting was partially converted in earlier phases).

- [x] **Step 5: Record findings**

Document what was found (or confirm zero remaining access).

---

### Task 2: Fix Any Remaining value[] Access

**Files:** As identified in Task 1. Known: 9+ macros in `merc.h` using `value[]`.

**Skip this task entirely if Task 1 found zero issues.**

- [x] **Step 1: Convert IS_WEAPON_STAT macro**

```c
// BEFORE (merc.h):
#define IS_WEAPON_STAT(obj,stat)  (IS_SET((obj)->value[4],(stat)))

// AFTER (verify correct field name in item_types.h — may be ->flags, ->stats, or similar):
#define IS_WEAPON_STAT(obj,stat)  (IS_WEAPON(obj) && IS_SET(WEAPON(obj)->flags,(stat)))
```

**Important:** Check `item_types.h` for the actual weapon flags/stats field name before implementing.
The IS_WEAPON guard is a behavioral change (more type-safe) — note this in the commit.

- [x] **Step 2: Convert CORPSE_* macros**

Convert `CORPSE_TYPE`, `CORPSE_RESURRECT`, `CORPSE_ANIMATE`, `CORPSE_PARTS`, `CORPSE_FLAGS`,
`CORPSE_MOBILE`, `CORPSE_MOBILE_AUID` to use `CORPSE(obj)->field` accessors.
Check `item_types.h` for `CORPSE_DATA` struct fields.

- [x] **Step 3: Convert WEIGHT_MULT and any other macros found in Task 1**

Apply the established pattern: `obj->value[N]` → `TYPE(obj)->field`
Use `item_types.h` as the reference for field mappings.

- [x] **Step 3: Build and test**

```bash
cd /sentience/src && ./build tests && cd /sentience && ./sent -test
```

- [x] **Step 4: Commit**

```bash
git add -A && git commit -m "fix: convert remaining value[] access to type accessors (Phase 5 cleanup)

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 3: Documentation Update

- [x] **Step 1: Update PLAN_backport_object_multityping.md**

Mark Phase 5 as **COMPLETE** with verification note. Add a brief entry noting the audit results.

- [x] **Step 2: Commit documentation**

```bash
git add -A && git commit -m "docs: mark Object Multityping Phase 5 verified complete

Comprehensive audit: zero direct ->value[] in game code. 300+ accessor macro calls in use.
Converted N hidden value[]-based macros in merc.h to type accessor pattern.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

Note: Update commit message with actual audit findings (number of macros converted, etc.).

---

## Summary

Phase 5 is **substantially complete**. This plan performs:
1. Comprehensive audit confirming zero `->value[]` direct access in game code
2. Fixes for ~9+ hidden macro-based `value[]` accesses in `merc.h` (IS_WEAPON_STAT, CORPSE_*, WEIGHT_MULT)
3. Documentation closure

**Estimated scope:** 3 tasks, ~1 hour total.
