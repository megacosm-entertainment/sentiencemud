# Backport Plan: Character Aura System

**Status:** In Progress / Planned (verified 2026-02-26 docs audit)


This document defines how to backport the **generic character aura subsystem** from `src_20_dev` into `src`.

> Scope clarification: this is **not** about existing spell/status effects that use the word “aura” (such as `healing_aura`, `spell_deflection`, or immortal `holyaura`).
>
> This backport targets the reusable, script-driven aura list attached to each character.

---

## 1. What the `src_20_dev` Aura System Is

In `src_20_dev`, aura support is a small runtime subsystem with these properties:

- Characters hold a list of custom aura entries (`LLIST *auras` on `CHAR_DATA`).
- Each aura entry stores:
  - `name` (unique key per character)
  - `long_descr` (formatted room-visible line, `%s` substitution for character name)
- Script commands can add/update/remove aura entries at runtime:
  - `addaura $MOBILE $NAME $%DESCRIPTION...`
  - `remaura $MOBILE $NAME`
- Aura entries are persisted on player save/load (`Aura <name>~ <long_descr>~`).
- Auras are displayed in look/room output and constrained by `MAX_AURAS_SHOWN`.

This enables lightweight, script-controlled visual state without introducing new affect bits for every unique effect.

---

## 2. Why Backport It

Mainline `src` currently lacks the generic aura container/API/script commands.

Backporting gives:

- Flexible scripted visuals for quest/event/state systems.
- Reduced pressure to add one-off affect flags.
- Persistence for long-running player states.
- Better parity with content patterns that were authored against `src_20_dev` behavior.

---

## 3. Non-Goals

- Do **not** replace or rewrite existing affect-based mechanics (`AFF2_HEALING_AURA`, `AFF2_SPELL_DEFLECTION`, `PLR_HOLYAURA`, etc.).
- Do **not** alter combat formulas or spell behavior as part of this backport.
- Do **not** expand beyond character-level auras (no object/room aura framework in this phase).

---

## 4. Backport Strategy (Phased)

### Stage A — Data Model + Runtime API

**Goal:** Introduce core data structures and lifecycle management.

Tasks:

- [ ] Add `AURA_DATA` definition and `MAX_AURAS_SHOWN` constant to `merc.h`.
- [ ] Add `LLIST *auras` to `CHAR_DATA` in `merc.h`.
- [ ] Add function prototypes in `merc.h`:
  - `find_aura_char(CHAR_DATA *ch, char *name)`
  - `add_aura_to_char(CHAR_DATA *ch, char *name, char *long_descr)`
  - `remove_aura_from_char(CHAR_DATA *ch, char *name)`
- [ ] Add allocator signatures to `recycle.h`:
  - `new_aura_data()` / `free_aura_data()`
- [ ] Implement allocator/free-list support in `mem.c`.
- [ ] Initialize `ch->auras` in character creation path (`new_char`).
- [ ] Ensure aura list memory is cleaned when character memory is released.
- [ ] Implement runtime helpers in `handler.c` (find/add/remove by unique aura name).

Acceptance criteria:

- Code builds cleanly.
- Creating and freeing many temporary chars does not leak/crash due to aura list handling.

---

### Stage B — Save/Load Persistence

**Goal:** Make aura state survive reconnect/reboot for PCs.

Tasks:

- [ ] Add player save writer support in `save.c`:
  - Emit `Aura %s~ %s~` records for each `AURA_DATA` entry.
- [ ] Add player load parser support in `save.c`:
  - Parse `Aura` records and call `add_aura_to_char(...)`.
- [ ] Confirm load-order safety (auras should load after `CHAR_DATA` list initialization).

Acceptance criteria:

- Aura entries persist through logout/login and reboot.
- Duplicate aura names update existing entries (replace behavior), not duplicate list growth.

---

### Stage C — Player Visibility Integration

**Goal:** Surface generic aura lines in look/room output.

Tasks:

- [ ] Port aura display block into `act_info.c` character formatting path.
- [ ] Respect `MAX_AURAS_SHOWN` cap and existing formatting conventions.
- [ ] Preserve immortal debug behavior only if desired (show aura internal name under holylight); otherwise gate behind staff visibility rules.
- [ ] Ensure formatting is robust when aura text lacks trailing newline.

Acceptance criteria:

- Auras appear in target look output.
- Rendering remains stable with 0, 1, and >cap aura entries.

---

### Stage D — Script Command Integration

**Goal:** Let scripts control aura state at runtime.

Tasks:

- [ ] Port `scriptcmd_addaura` and `scriptcmd_remaura` implementations to `script_commands.c`.
- [ ] Register command entries in script command tables (`script_commands.c` command arrays) for all relevant contexts.
- [ ] Validate parser behavior for long description string (`$%DESCRIPTION...`) and `%s` substitution expectations.
- [ ] Add guardrails:
  - Null/empty name rejected.
  - Null/empty description rejected for add/update.

Acceptance criteria:

- Script can add, replace, and remove aura entries during runtime.
- No crashes with malformed script arguments.

---

### Stage E — Hardening + Backward Compatibility

**Goal:** Prevent regressions and avoid collisions with existing aura-named mechanics.

Tasks:

- [ ] Verify existing mechanics still behave identically:
  - `healing_aura`
  - `spell_deflection` crimson aura messaging
  - `holyaura` immortal toggle
- [ ] Verify there is no naming/semantic confusion in logs and player messaging.
- [ ] Consider optional sanitization for `long_descr` format string safety (ensure exactly one `%s` or use safer templating helper).
- [ ] Confirm no changes required to `CMakeLists.txt`/`Makefile` (unless files are added).

Acceptance criteria:

- Existing gameplay behavior unchanged unless aura script commands are used.
- No new warnings or runtime format-string hazards introduced.

---

## 5. Implementation Notes

### 5.1 Name Uniqueness Rule

`add_aura_to_char` should act as upsert:

- If aura name exists: update `long_descr`.
- If not: create new entry and append.

This preserves script simplicity and avoids duplicate visual spam.

### 5.2 Display Contract

`long_descr` should be authored as a full sentence intended for room display, e.g.:

- `"{M%s is wreathed in fractal violet flames.{x\n\r"`

Keep output compatible with existing `act`/`pers` formatting style.

### 5.3 Persistence Scope

Primary target is player save/load persistence.

NPC auras are expected to be script/runtime driven and ephemeral unless specifically persisted elsewhere.

---

## 6. Risk Register

- **Format string misuse:** `long_descr` is formatted with a player name; malformed templates can crash or print garbage.
- **Memory lifecycle mismatch:** missing cleanup in char free path can leak aura entries.
- **Output bloat:** too many aura lines can clutter look output; enforce cap.
- **Semantic overlap:** “aura” term already appears in other systems; maintain clear separation in code comments/doc text.

---

## 7. Validation Plan

### Build

- `cd /sentience/src && ./build`
- `cd /sentience/src && ./build tests`

### Functional checks

1. Add temporary script to apply aura with `addaura`.
2. Observe aura line in look output.
3. Replace same-name aura and verify update (not duplicate).
4. Remove aura via `remaura` and verify disappearance.
5. Save player, reconnect, verify aura persistence.
6. Ensure healing aura / spell deflection behavior still matches baseline.

### Regression checks

- Run unit/integration tests after build-tests configuration.
- Spot-check staff commands and combat update paths for aura-named legacy features.

---

## 8. Suggested Execution Order

1. Stage A (data/runtime)
2. Stage B (save/load)
3. Stage C (display)
4. Stage D (scripts)
5. Stage E (hardening)

This order minimizes risk by bringing up low-level primitives first, then surfacing behavior.

---

## 9. Follow-up Docs to Add After Implementation

- `WORKLOG_aura_system_backport.md` (what was actually implemented)
- Optional `TODO_aura_system_backport.md` (remaining polish items, if any)
- Changelog entry in developer/admin docs once merged
