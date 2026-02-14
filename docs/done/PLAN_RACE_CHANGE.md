# Plan: Scriptable Race Change System

## Status: Phase 1 Complete

Phase 1 (core infrastructure) is implemented and building. Phase 2 (remort
refactor) is planned but not yet started.

## Problem Statement

Changing a character's race at runtime (via scripts or the deprecated remort system)
currently only swaps the `ch->race` pointer — it does **not** recalculate the
derived properties that depend on race:

- Permanent affects (`affected_by_perm[]`)
- Immunities/resistances/vulnerabilities (`imm_flags_perm`, `res_flags_perm`, `vuln_flags_perm`)
- Physical form and body parts (`form`, `parts`)
- Racial skills
- Stat caps (`max_stats[]`)
- Size
- Traits (race is the bottom layer of the trait stack)

Additionally, there is no mechanism for tracking a character's **original race**
to support transformative race changes like "Elf → Lich" where the new race
overlays changes onto the original rather than fully replacing it.

## Current State

### What exists

| Component | Status |
|-----------|--------|
| `ch->race` | RACE_DATA pointer, saved as string ID in character JSON |
| `ch->orace` | Declared in merc.h but **completely unused** — never saved, loaded, or referenced |
| Script `altermob race` | Only does `mob->race = new_race` — no property recalc |
| `remort_player()` | Manually resets perm flags, form, parts, skills — hardcoded in skills.c |
| `fix_character()` | Called on character load — syncs perm flags from race, clamps stats |
| `affect_fix_char()` | Rebuilds active flags from `_perm` values + spell affects + equipment |
| Trait system | Three layers: personal > class > race — queries `ch->race` at runtime |
| Token variables | Key-value storage on tokens attached to chars — persists across reboots |
| Preferences | Key-value store on PC_DATA — currently used for settings, could extend |

### Script `altermob race` today (script_mpcmds.c ~L4531)

```c
RACE_DATA *new_race = race_lookup(arg->d.str);
if (new_race && op == OPR_ASSIGN) {
    mob->race = new_race;
}
```

No recalculation, no backup, no notification. Same pattern in `script_opcmds.c`,
`script_rpcmds.c`, and `script_tpcmds.c`.

## Design

### Part 1: `char_set_race()` — Central Race Change Function

Create a single authoritative function that all race-change paths use:

```c
/**
 * char_set_race - Change a character's race with full property recalculation
 *
 * @param ch         Character to modify
 * @param new_race   Target RACE_DATA
 * @param flags      RACE_CHANGE_* flags controlling behavior
 */
void char_set_race(CHAR_DATA *ch, RACE_DATA *new_race, int flags);
```

**Flags:**

| Flag | Effect |
|------|--------|
| `RACE_CHANGE_SAVE_ORIGINAL` | Store current race as original before changing |
| `RACE_CHANGE_KEEP_SKILLS` | Don't remove old racial skills |
| `RACE_CHANGE_KEEP_STATS` | Don't clamp stats to new race caps |
| `RACE_CHANGE_OVERLAY` | Merge new race properties with original (for Elf→Lich) |
| `RACE_CHANGE_SILENT` | Don't echo messages to the character |

**What it does:**

1. Optionally save `ch->race` as the original race (via `ch->orace`)
2. Swap `ch->race = new_race`
3. Recalculate permanent flags from race:
   - `affected_by_perm[0..1]` = `new_race->aff[0..1]`
   - `imm_flags_perm` = `new_race->imm`
   - `res_flags_perm` = `new_race->res`
   - `vuln_flags_perm` = `new_race->vuln`
4. If `RACE_CHANGE_OVERLAY` and `ch->orace` exists, OR the original race's flags in first
5. Update physical properties: `form`, `parts`, `size`
6. Add new racial skills; optionally remove old ones
7. Clamp stats to new `max_stats[]` (unless `RACE_CHANGE_KEEP_STATS`)
8. Call `affect_fix_char(ch)` to rebuild active flags from perm + affects + equipment
9. Save the character

### Part 2: Activate `orace` — Original Race Tracking

The `orace` field already exists in CHAR_DATA. We need to:

1. **Save/load it** in `json_char.c`:
   - Save: `json_object_set_new(basic, "original_race", json_string(ch->orace->id))`
   - Load: `ch->orace = race_lookup(str)`
2. **Expose to scripts** as `mob.originalrace` (read) and via `altermob originalrace` (write)
3. **Use in trait queries** — when `ch->orace` is set, the trait system can layer:
   personal > class > current race > original race

### Part 3: Race Overlay Semantics (Elf → Lich)

When `RACE_CHANGE_OVERLAY` is used, the new race's properties are merged with
the original race rather than fully replacing:

| Property | Overlay Behavior |
|----------|-----------------|
| `aff[]` | OR — union of original and new affects |
| `imm` | OR — gain new immunities, keep original |
| `res` | New race wins — lich resistances override elf |
| `vuln` | New race wins — lich vulnerabilities override elf |
| `form` | New race wins |
| `parts` | New race wins |
| `skills` | Union — keep original racial skills, add new |
| `traits` | New race > original race (both below class/personal) |
| `max_stats` | Per-stat maximum of both races |
| `size` | New race wins |

For traits, the query priority becomes:
**personal > class > current race > original race > defaults**

### Part 4: Update Script Handlers

Replace the bare pointer swap in all four `AlterMob` handlers with a call to
`char_set_race()`. The script command becomes:

```
altermob $mob race "lich"              → RACE_CHANGE_SAVE_ORIGINAL
altermob $mob race "lich" overlay      → RACE_CHANGE_SAVE_ORIGINAL | RACE_CHANGE_OVERLAY
altermob $mob race "elf" revert        → Restore from orace, clear orace
```

New script read properties:
```
$mob.originalrace     → string (orace name, or "none")
$mob.originalracedata → RACE_DATA pointer
```

### Part 5: Deprecate Remort Race Handling

The current `remort_player()` function manually does everything `char_set_race()`
would do. Refactor it to call `char_set_race()` instead:

```c
// In remort_player():
char_set_race(ch, get_remort_race(ch),
    RACE_CHANGE_SAVE_ORIGINAL | RACE_CHANGE_KEEP_SKILLS);
```

This keeps the existing remort behavior but routes through the central function.

## Implementation Order

### Phase 1 — Core Infrastructure (DONE)

1. **`char_set_race()` function** — handler.c (end of file)
   - Full recalculation of perm affects, imm, res, vuln, form, parts, size
   - Overlay mode: OR affects/immunities with original race, new race wins for rest
   - Revert mode: restore orace as current race
   - Stat capping with per-stat max in overlay mode
   - Racial skill addition (union in overlay mode)
   - `affect_fix_char()` called to rebuild active flags

2. **`orace` persistence** — io/json/json_char.c
   - Save as `"original_race"` string ID
   - Load via `race_lookup()`

3. **Trait system extension** — traits.c
   - All three `ch_get_trait_*` functions now query four layers:
     personal > class > race > original race (orace)

4. **Script integration** — script_mpcmds.c, script_opcmds.c, script_rpcmds.c, script_tpcmds.c
   - `altermob $mob race = "lich"` — full race change with SAVE_ORIGINAL
   - `altermob $mob raceoverlay = "lich"` — overlay on original race
   - `altermob $mob racerevert = 1` — restore from orace
   - All use `char_set_race()` instead of bare pointer swap
   - Added `$mob.originalrace` (string) and `$mob.originalracedata` (RACE_DATA) read properties

5. **Constants** — merc.h
   - `RACE_CHANGE_SAVE_ORIGINAL`, `RACE_CHANGE_OVERLAY`, `RACE_CHANGE_KEEP_SKILLS`,
     `RACE_CHANGE_KEEP_STATS`, `RACE_CHANGE_SILENT`, `RACE_CHANGE_REVERT`

### Phase 2 — Remort Refactor (DONE)

6. **Remort as "paths"** — skills.c
   - Refactored `remort_player()` to use `char_set_race()` with
     `RACE_CHANGE_SAVE_ORIGINAL | RACE_CHANGE_OVERLAY | RACE_CHANGE_SILENT`
   - Removed ~20 lines of manual flag/skill reset, replaced with single call
   - Removed redundant `affect_fix_char()` (already called by `char_set_race()`)
   - Remort races (lich, vampire, slayer) now overlay on the character's
     base race, preserving it as `orace`

### Phase 3 — Testing (TODO)

7. **Testing** — unit tests for flag recalculation, overlay math, trait layering

## Open Questions

1. Should `orace` support stacking (e.g., Elf → Vampire → Lich), or is one
   level of "original" sufficient? **Recommend: one level only.** Stacking
   adds complexity with questionable benefit; scripts can manage deeper chains
   via token variables if needed.

2. Should overlay mode OR or MAX the `res`/`vuln` flags? OR merges both sets
   of resistances/vulnerabilities; the current design says new-race-wins for
   these since transformation should fundamentally change weaknesses.

3. Should the "revert" operation also strip skills gained from the transformed
   race? **Recommend: yes**, unless `RACE_CHANGE_KEEP_SKILLS` is specified.

4. Should `fix_character()` (called on login) also use `char_set_race()` for
   its race property sync? This would centralize all race→character property
   derivation but needs careful ordering during boot.
