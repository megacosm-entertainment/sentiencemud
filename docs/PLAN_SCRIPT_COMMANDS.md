# Plan: Script Commands & Ifchecks for New Systems

> Superseded by `docs/PLAN_SCRIPT_AUDIT_CONSOLIDATED.md` for consolidated
> prioritization and sequencing.

## Overview

Wire up the scripting engine to support race, class, skill, song, and trait
systems. This covers new ifchecks, new script commands, modernizing legacy
commands, and filling in existing stubs.

## Current State

### Ifchecks (script_ifc.c / script_const.c)

| Ifcheck       | Status            | Issues                                  |
|---------------|-------------------|-----------------------------------------|
| `class`       | Stub (returns false) | Needs implementation or removal         |
| `race`        | Works             | Uses old `race_lookup()` pointer compare|
| `skill`       | Works (legacy)    | Uses `skill_lookup()` → sn              |
| `testskill`   | Works (legacy)    | Same as skill + random check            |
| `skilllookup`  | Works             | Returns sn, could return uid instead    |

### Script Commands (script_commands.c)

| Command        | Status     | Issues                                  |
|----------------|------------|-----------------------------------------|
| `grantskill`   | Works      | Uses sn, skill_table[], learned[]       |
| `revokeskill`  | Works      | Uses sn, skill_entry_findsn()           |
| `setclass`     | Empty stub | Registered in mpcmds only?              |
| `setrace`      | Empty stub | Same                                    |
| `setsubclass`  | Empty stub | Subclass concept deprecated             |

### SKILL_ENTRY (merc.h)

Our struct has fields 2.0 removed (practice/improve bools, sn int16_t) and
fields 2.0 never had (SKILL_SOURCE, cross_class_scope, source_class).

Our `skill_entry_addskill()` / `skill_entry_addspell()` take `int sn`;
2.0's equivalents take `SKILL_DATA *`.

## Phase A: New Ifchecks

Add to `script_ifc.c` and register in `script_const.c`:

### `hasclass` — Does player have a class unlocked?
```
if hasclass($n, 'warrior')        → boolean
```
Implementation: `class_find(name)` → `has_class_level(mob, clazz)`.

### `isclass` — Is the player's current class X?
```
if isclass($n, 'warrior')         → boolean
```
Implementation: `class_find(name)` → compare with `get_current_class(mob)`.

### `classlevel` — Player's level in a specific class
```
if classlevel($n, 'warrior') >= 10  → number
```
Implementation: `class_find(name)` → `get_class_level(mob, clazz)` → `level->level`.

### `classcount` — Number of classes the player has
```
if classcount($n) >= 3            → number
```
Implementation: Count entries in `pcdata->classes` list.

### `hastrait` — Does character have a boolean trait?
```
if hastrait($n, 'can_fly')        → boolean
```
Implementation: `ch_has_trait(mob, id)`.

### `traitint` — Integer trait value on a character
```
if traitint($n, 'max_breath') >= 5  → number
```
Implementation: `ch_get_trait_int(mob, id)`.

### `traitstring` — String trait value (for equality checks)
```
if traitstring($n, 'home_plane') == 'astral'  → string? or boolean
```
Implementation: `ch_get_trait_string(mob, id)`. Compare in script.

### `hasskill` — Boolean: player has this skill at all?
```
if hasskill($n, 'sword')          → boolean
```
Implementation: `skill_lookup(name)` → `skill_entry_findsn(sorted_skills, sn)`.

### `hassong` — Boolean: player knows this song?
```
if hassong($n, 'lullaby')         → boolean
```
Implementation: `song_lookup(name)` → `skill_entry_findsong(sorted_songs, song)`.

### Fix `class` — Implement or deprecate
The `class` ifcheck is a stub. Either:
- (a) Implement as alias for `isclass`, or
- (b) Keep stub and add deprecation comment.
Recommend (a).

### Fix `race` — Use name comparison
Currently compares RACE_DATA pointers which works fine.
No change needed.

## Phase B: Fill Existing Stubs

### `setclass` — Switch active class
```
setclass $PLAYER 'warrior'
```
Implementation:
1. Parse: mob entity + string class name
2. `class_find(name)` → CLASS_DATA*
3. Verify `has_class_level(mob, clazz)` — must be unlocked
4. Call `set_current_class(mob, clazz)` or equivalent
5. Set `lastreturn = 1` on success

### `setrace` — Change character's race
```
setrace $PLAYER 'elf'
```
Implementation:
1. Parse: mob entity + string race name
2. `race_lookup(name)` → RACE_DATA*
3. Set `mob->race = race`
4. Apply racial stat/flag changes
5. Set `lastreturn = 1` on success

### `setsubclass` — Remove or repurpose
Subclass concept is deprecated. Remove stub or redirect to setclass.

## Phase C: New Script Commands

### `grantclass` — Unlock a class for a player
```
grantclass $PLAYER 'warrior'[ level]
```
Implementation:
1. Parse: mob entity + class name string + optional level number
2. `class_find(name)` → CLASS_DATA*
3. If already has class: set `lastreturn = 0`, return
4. `add_class_level(mob, clazz, level)` (default level = 1)
5. Set `lastreturn = 1`

### `revokeclass` — Remove a class from a player
```
revokeclass $PLAYER 'warrior'
```
Implementation:
1. Parse: mob entity + class name string
2. `class_find(name)` → CLASS_DATA*
3. If it's current class: switch to another or clear
4. Remove class level from pcdata->classes
5. Remove skills granted by this class (via SKILL_SOURCE tracking)
6. Set `lastreturn = 1`

### `grantsong` — Grant a song to a player
```
grantsong $PLAYER 'lullaby'[ rating]
```
Implementation:
1. Parse: mob entity + song name string + optional rating
2. `song_lookup(name)` → SONG_DATA*
3. `skill_entry_addsong(mob, song, NULL, SKILLSRC_SCRIPT, flags)`
4. Set rating if provided
5. Set `lastreturn = 1`

### `revokesong` — Remove a song from a player
```
revokesong $PLAYER 'lullaby'
```
Implementation:
1. Parse: mob entity + song name string
2. `song_lookup(name)` → SONG_DATA*
3. `skill_entry_findsong(sorted_songs, song)` → entry
4. `skill_entry_removeentry(&sorted_songs, entry)`
5. Set `lastreturn = 1`

### `settrait` — Set a personal trait on a character
```
settrait $PLAYER 'can_fly' true|42|'string'
```
Implementation:
1. Parse: mob entity + trait id string + value (bool/int/string)
2. `trait_def_lookup(id)` → TRAIT_DEF*
3. Allocate pcdata->trait_values if NULL
4. Set value based on trait type
5. Set `lastreturn = 1`

## Phase D: Modernize grantskill / revokeskill

### `grantskill` changes
- Replace `skill_lookup()` → `int sn` with `skill_data_lookup()` → `SKILL_DATA*`
- Remove `mob->pcdata->learned[sn] = rating` (rating is on the SKILL_ENTRY)
- Replace `skill_table[sn].spell_fun == spell_null` with `!skill->is_spell` or equivalent
- Use `skill_entry_findskill()` instead of `skill_entry_findsn()`

### `revokeskill` changes
- Replace `skill_lookup()` with `skill_data_lookup()` → `SKILL_DATA*`
- Replace `skill_entry_findsn()` with `skill_entry_findskill()`

### Currently needed skill_entry API updates
- `skill_entry_addskill(ch, SKILL_DATA*, token, source, flags)` — change param from sn
- `skill_entry_addspell(ch, SKILL_DATA*, token, source, flags)` — same
- `skill_entry_findskill(list, SKILL_DATA*)` — add this function (we have findsn)

## Phase E: Registration

All new commands need registration in all 4 prog command tables:
- `script_mpcmds.c` — mob progs
- `script_opcmds.c` — object progs
- `script_rpcmds.c` — room progs
- `script_tpcmds.c` — token progs

Commands to register: `grantclass`, `revokeclass`, `grantsong`, `revokesong`, `settrait`.

Stubs `setclass`, `setrace`, `setsubclass` — check if already registered in all 4.

## Implementation Order

1. Write PLAN doc (this file) ✅
2. Implement ifchecks (hasclass, isclass, classlevel, classcount, hastrait,
   traitint, traitstring, hasskill, hassong) + fix class ifcheck
3. Fill setclass/setrace stubs
4. Implement new commands (grantclass, revokeclass, grantsong, revokesong, settrait)
5. Modernize grantskill/revokeskill
6. Register everything in all 4 prog types
7. Build and test

## Dependencies

- `class_data.h`: class_find(), has_class_level(), get_class_level(),
  get_current_class(), add_class_level(), class_flags
- `traits.h`: ch_has_trait(), ch_get_trait_bool/int/string(),
  trait_def_lookup(), trait_values_alloc()
- `song_data.h`: song_lookup(), song_flags
- `skill_data.h`: skill_find_uid(), SKILL_DATA
- `merc.h`: SKILL_ENTRY, skill_entry_* functions
